# 第 1 天：项目骨架 — 启动流程与 UI 架构

**目标：画出一张完整的程序启动→界面显示的流程图**

## 1. 你已有的基础知识

| 已学课程 | 今天会用到的知识点 |
|---------|-------------------|
| 开发板配置 | `/dev/fb0` 帧缓冲设备、`/dev/input/event0` 触摸设备 |
| 文件 IO | `open()`, `read()`, `fopen()`, `opendir()` |
| 系统编程 | `fork()`, `pipe()`, `pthread_create()`, `signal()` |
| LVGL | `lv_obj_create()`, `lv_obj_set_size()`, 容器-控件层级 |

---

## 2. 先看整体：main() 只有一个函数

打开 `usrCode/core/main.c`，从上到下共 151 行。只看 `main()` 函数（第 84-151 行），约 67 行有效代码，分三个阶段：

```
【阶段1: 初始化 — 建立基础设施】     第87-120行
  信号处理 → LVGL内核 → 文件系统 → 显示驱动 → 触摸驱动 → 缓冲区 → 注册设备

【阶段2: 启动业务 — 开始干活】       第122-141行
  扫描媒体 → 加载配置 → 注册触摸事件 → 创建UI → 启动线程

【阶段3: 无限循环 — 持续运转】       第143-148行
  while(1) { lv_timer_handler(); lv_tick_inc(17); usleep(17000); }
```

---

## 3. 逐行详解：阶段 0 — 头文件与全局变量（第 1-82 行）

在进入 `main()` 之前，先看文件顶部。这三部分很重要：

### 3.0.1 #include 头文件（第 20-33 行）

```c
#include "lvgl/lvgl.h"              // LVGL 核心 API（所有 lv_xxx 函数）
#include "lv_drivers/display/fbdev.h" // 帧缓冲显示驱动
#include "lv_drivers/indev/evdev.h"   // 触摸输入驱动
#include "lvgl/src/libs/fsdrv/lv_fsdrv.h" // 文件系统驱动
#include "car_ui.h"                 // UI 编排器
#include "car_ui_sidebar.h"         // 侧边栏（待机/唤醒用）
#include "music_catalog.h"          // 音乐目录扫描
#include "can_simulator.h"          // CAN 数据模拟
#include "network_client.h"         // 网络客户端
#include "settings_config.h"        // 配置读写
#include "settings_ui.h"            // 背光控制接口
#include "app_config.h"             // 公共配置（布局宏、全局变量声明）
```

每个 `#include` 对应一个功能模块——这就是 C 语言的模块化方式。

### 3.0.2 全局变量定义（第 38-73 行）

**对照系统编程课**：这些是"全局共享状态"，所有线程都能访问。项目中的线程同步就靠保护这些变量。

```c
// ---- 互斥锁（保护共享数据） ----
pthread_mutex_t g_lvgl_mutex = PTHREAD_MUTEX_INITIALIZER;  // LVGL控件操作锁
pthread_mutex_t g_can_mutex  = PTHREAD_MUTEX_INITIALIZER;  // CAN数据锁
pthread_mutex_t g_net_mutex  = PTHREAD_MUTEX_INITIALIZER;  // 网络数据锁

// ---- 运行标志 ----
volatile bool g_app_running = false;    // 应用运行标志（目前未用）
bool g_video_overlay_active  = false;   // 视频播放时跳过UI刷新
time_t g_last_input_time     = 0;       // 最后一次触摸时间（自动熄屏用）

// ---- CAN 数据（can_simulator线程更新，仪表盘定时器读取） ----
int g_speed_kmh    = 0;       // 车速
int g_fuel_percent = 80;      // 油量百分比
int g_engine_rpm   = 800;     // 发动机转速

// ---- 网络数据（network_client线程更新，状态栏读取） ----
char g_weather_str[32]  = "";  // 天气描述（如"晴"）
int  g_temp_celsius     = 25;  // 室外温度
char g_location_str[64] = "";  // 城市名
char g_nav_hint[32]     = "";  // 导航提示（未使用）

// ---- 系统设置（各处读写，settings_config持久化） ----
int g_sys_volume          = 10;     // 音量 0-100
int g_sys_brightness      = 70;     // 亮度 0-100
int g_auto_screen_off_sec = 30;     // 自动熄屏秒数
char g_bluetooth_name[32] = "GEC6818-CAR";  // 蓝牙名称

// ---- 其他 ----
bool g_music_interrupted_by_video = false;  // 视频→音乐互斥标志
int g_ac_temperature = 24;   // 空调温度 16-32
int g_ac_fan_speed   = 3;    // 空调风量 1-7
int g_ac_mode        = 0;    // 空调模式 0-3
```

**关键特征**：所有全局变量以 `g_` 开头，这是项目的命名规范，一眼就能认出是全局变量。

### 3.0.3 触摸回调函数（第 75-81 行）

```c
static void on_global_input_cb(lv_event_t *e)
{
    (void)e;                                       // 未使用参数，消除编译警告
    if (car_ui_is_standby()) car_ui_standby_deactivate(); // 待机中→唤醒
    g_last_input_time = time(NULL);                 // 记录最后触摸时间
}
```

**两个作用**：
1. **自动熄屏**：每次触摸更新时间戳，仪表盘定时器检测超时
2. **待机唤醒**：如果在待机状态，触摸即唤醒

这个函数在第 130 行被注册到屏幕根对象上，所有触摸事件都会触发它。

---

## 4. 逐行详解：阶段 1 — 初始化（第 84-120 行）

### 4.1 `signal(SIGPIPE, SIG_IGN)` — 第 87 行

**回忆系统编程课**：管道写入规则——如果读端已关闭，写端 `write()` 会触发 SIGPIPE 信号，默认行为是杀死进程。

**在项目中**：音乐/视频播放器用 `fork()` + `pipe()` 通信。mplayer 进程退出后，管道读端关闭。Writer 线程还在 `write()` → SIGPIPE → 默认杀死进程。

```c
// 忽略 SIGPIPE，让 write() 返回 -1（errno=EPIPE）而非杀进程
signal(SIGPIPE, SIG_IGN);
```

### 4.2 `lv_init()` — 第 90 行

LVGL 内核初始化：分配内部内存、初始化样式系统、创建默认屏幕对象。必须在任何 LVGL 函数之前调用。**无参数，无返回值，一行搞定。**

### 4.3 `lv_fs_stdio_init()` — 第 93 行

**对照文件 IO 课**：注册文件系统驱动。内部做的事—把 LVGL 的文件操作（`lv_fs_open/read/close`）映射到 C 标准库（`fopen/fread/fclose`）。没有这行，`lv_img_set_src(img, "S:car_bg.bmp")` 找不到图片文件。

### 4.4 `fbdev_init()` — 第 96 行

**对照开发板配置课**：打开 `/dev/fb0`（Linux framebuffer）。帧缓冲是 Linux 在内存中给显示器预留的一段连续区域，写入像素数据 → 直接显示到屏幕。相当于 `open("/dev/fb0")` + `mmap()` 映射显存。

### 4.5 `evdev_init()` — 第 99 行

打开 `/dev/input/event0` 触摸输入设备。手指点击→内核生成 evdev 事件→LVGL 接收坐标→触发控件回调。

### 4.6 显示缓冲 — 第 101-120 行

```c
// ==== 5. 单缓冲绘制（第 101-104 行）====
static lv_color_t buf[DISP_BUF_SIZE];
// DISP_BUF_SIZE = 800 × 480 × 2 = 768,000 像素（单缓冲，buf2=NULL）
static lv_disp_draw_buf_t disp_buf;
lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

// ==== 6. 注册显示设备（第 106-113 行）====
static lv_disp_drv_t disp_drv;
lv_disp_drv_init(&disp_drv);
disp_drv.draw_buf   = &disp_buf;       // 绑定缓冲区
disp_drv.flush_cb    = fbdev_flush;     // 刷新回调 → 真正写 /dev/fb0
disp_drv.hor_res     = DISP_HOR_RES;    // 800 像素宽
disp_drv.ver_res     = DISP_VER_RES;    // 480 像素高
lv_disp_drv_register(&disp_drv);       // 注册到 LVGL

// ==== 7. 注册触摸输入设备（第 115-120 行）====
static lv_indev_drv_t indev_drv;
lv_indev_drv_init(&indev_drv);
indev_drv.type    = LV_INDEV_TYPE_POINTER;  // 指针类设备（触摸屏）
indev_drv.read_cb = evdev_read;             // 读取回调 → 从 /dev/input/event0
lv_indev_drv_register(&indev_drv);          // 注册到 LVGL
```

**为什么用单缓冲（buf2=NULL）？** 双缓冲可防撕裂但需 2 倍显存。嵌入式场景内存有限，单缓冲+局部刷新更合适。

---

## 5. 逐行详解：阶段 2 — 启动业务（第 122-141 行）

### 5.1 `music_catalog_scan()` — 第 123 行

**对照文件 IO 课**：扫描 `/mmcblk/music` 目录。`opendir()`→`readdir()`→过滤 `.mp3` 后缀→`snprintf()` 拼接路径→存到 `g_music_path[]` 数组→`closedir()`。

### 5.2 `settings_config_load()` — 第 126 行

读取 `config.json`，把上次保存的音量、亮度等设置加载到全局变量。文件不存在时自动创建默认配置。

**为什么必须在 UI 创建之前？** UI 创建时（第 133 行）会读取全局变量作为滑块的初始值。如果配置在 UI 创建之后才加载，滑块显示的是默认值，不是已保存的值。这是我们之前修过的 bug。

### 5.3 `settings_backlight_apply()` — 第 127 行

把加载的亮度值写入硬件背光。虽然这块板子的背光不通，但代码逻辑是正确的——换块板子就行。

### 5.4 注册全局触摸事件 — 第 130 行

```c
lv_obj_add_event_cb(lv_scr_act(), on_global_input_cb, LV_EVENT_PRESSED, NULL);
```

`lv_scr_act()` 获取根屏幕对象，给它注册一个 `LV_EVENT_PRESSED` 回调。所有点击事件都会从控件冒泡到屏对象，触发 `on_global_input_cb`（第 76 行）→ 更新时间戳 + 唤醒待机。

### 5.5 `car_ui_create_dashboard()` — 第 133 行

**对照 LVGL 课**：整个 UI 的入口函数。打开 `core/car_ui.c`（只有 50 行）：

```c
void car_ui_create_dashboard(void) {
    lv_obj_t *scr = lv_scr_act();              // 获取默认屏幕
    car_ui_sidebar_create(scr);                 // 左侧栏 70×460
    car_ui_dashboard_create(scr);               // 中间仪表盘 440×460
    car_ui_ac_panel_create(scr);                // 空调面板（初始隐藏）
    app_menu_ui_create(scr);                    // 应用菜单（初始隐藏）
    settings_ui_create(scr);                    // 设置面板（初始隐藏）
    car_ui_status_create(scr);                  // 右上状态 250×140
    car_ui_music_bar_create(scr);               // 右下音乐栏 250×310
}
```

**"编排器模式"**：`car_ui.c` 不画任何控件，只负责按正确顺序调用子模块。就像导演——不演戏但决定上场顺序。

**各面板的父子关系（从代码中的 lv_obj_set_pos 提取）**：
```
lv_scr_act() (根容器 800×480, 背景 #0A0A1A)
├── 左侧栏     x=10,  y=10,  w=70,  h=460
├── 仪表盘     x=90,  y=10,  w=440, h=460  (默认可见)
├── 空调面板   x=90,  y=10,  w=440, h=460  (初始隐藏)
├── 应用菜单   x=90,  y=10,  w=440, h=460  (初始隐藏)
├── 设置面板   x=90,  y=10,  w=440, h=460  (初始隐藏)
├── 状态卡     x=540, y=10,  w=250, h=140
└── 音乐栏     x=540, y=160, w=250, h=310
```

中间 440×460 区域被 4 个面板共享（仪表盘/空调/菜单/设置），同一时刻只有一个可见。

### 5.6 `can_simulator_start()` — 第 136 行

**对照系统编程课**：启动 detached 线程模拟 CAN 总线。`pthread_create()` 创建线程 → `pthread_detach()` 分离。线程函数里是 `while(1)` + `usleep(200000)` 200ms 一周期更新车速/油量/转速。

### 5.7 `network_client_start()` — 第 139 行

**对照网络编程课**：启动 TCP 客户端线程。`socket()`→`connect()`→`recv()` 循环接收 JSON 天气数据→通过 `lv_async_call` 安全更新 UI。

### 5.8 `printf("[Main] 智能车机系统启动完成\n")` — 第 141 行

启动流程完成的标志。看到这行说明所有初始化都成功了。

---

## 6. 逐行详解：阶段 3 — 主循环（第 143-148 行）

```c
/* ---- 12. 主事件循环（约 60Hz） ---- */
while (1) {
    lv_timer_handler();   /* 处理 LVGL 定时器/动画/事件 */
    lv_tick_inc(17);       /* 推进 LVGL 内部时钟 17ms */
    usleep(17000);         /* 休眠 17ms → 约 59 次/秒 */
}
```

**对照系统编程课**：`lv_timer_handler()` 内部遍历所有注册的定时器，检查是否到期。到期就调用回调函数（如仪表盘刷新、时钟更新）。`lv_tick_inc(17)` 告诉 LVGL "时间过去了 17ms"，LVGL 内部用这个值驱动动画和定时器。

**为什么是 60Hz 而不是 200Hz？** 人眼不需要 200 次/秒的刷新。最快的用户定时器是 200ms 间隔（仪表盘），17ms 精度完全够用。这比原来的 200Hz（5ms）节省了约 70% 的 CPU 空闲唤醒次数。

---

## 7. 全局变量与线程安全的完整视图

man.c 中定义的全局变量，最终被哪些线程读写：

| 全局变量 | 写线程 | 读线程 |
|---------|--------|--------|
| `g_speed_kmh/fuel/rpm` | CAN 模拟器线程 | 主线程(仪表盘定时器) |
| `g_weather_str/temp/location` | 网络客户端线程 | 主线程(状态栏刷新) |
| `g_sys_volume` | 主线程(侧边栏/滑块) | Launch 线程(发 mplayer 命令) |
| `g_video_overlay_active` | 主线程(视频播放器 UI) | 主线程(所有定时器) |
| `g_last_input_time` | 主线程(全局触摸回调) | 主线程(仪表盘定时器) |

**关键**：大多数变量都是主线程之内读写，不需要锁。跨线程共享的（CAN数据、网络数据）都有专用 mutex。

---

## 8. 动手任务

### 任务 1：画 UI 布局图
在一张纸上画出 800×480 矩形，标注每个面板的位置和尺寸。然后打开代码找到 `lv_obj_set_size` 和 `lv_obj_set_pos`，核实你的数字。

### 任务 2：追踪一条完整的代码路径
从 `sidebar_vol_up_cb()`（`ui/car_ui_sidebar.c` 第 230 行）开始，手写追踪：
```
用户点击 Vol+ 按钮
  → sidebar_vol_up_cb()     // 读取全局变量，+5，限幅 0-100
  → music_set_volume()      // 换算 mplayer 音量（÷5），发 FIFO 命令
  → car_ui_dashboard_update_volume()  // 更新仪表盘"音量 XX%"标签
```
找出每一步在哪个文件的第几行。

### 任务 3：改一个数字看效果
把 `main.c` 第 147 行的 `usleep(17000)` 改成 `usleep(50000)`，编译部署，观察 UI 响应速度变化。（改完改回来）

### 任务 4：验证全局变量初始化
在 `main()` 函数的 `return 0` 之前（第 150 行）加一行：
```c
printf("当前全局变量: speed=%d fuel=%d volume=%d\n", g_speed_kmh, g_fuel_percent, g_sys_volume);
```
编译部署，观察启动时终端输出的默认值。
