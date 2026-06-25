/**
 * @file main.c
 * @brief 智能车机项目入口 — LVGL / fbdev / evdev 初始化 + 主事件循环
 *
 * 初始化流程（复用 player_project 的已验证模式）：
 *   1. lv_init()             → LVGL 内核初始化
 *   2. lv_fs_stdio_init()    → 注册 STDIO 文件系统驱动
 *   3. fbdev_init()          → 打开 /dev/fb0，mmap 帧缓冲
 *   4. evdev_init()          → 打开 /dev/input/event0，注册触摸
 *   5. lv_disp_draw_buf_init → 单缓冲 800×480×2 字节
 *   6. lv_disp_drv_register  → 注册 fbdev_flush 显示刷新回调
 *   7. lv_indev_drv_register → 注册 evdev_read 触摸读取回调
 *   8. car_ui_create_dashboard() → 绘制 4 区域主界面
 *   9. while(1) { lv_timer_handler(); lv_tick_inc(5); usleep(5000); }
 *
 * LVGL 非线程安全：所有 UI 创建和事件处理均在主线程执行。
 * 后续阶段的工作线程通过 lv_async_call / lv_timer 安全跨线程更新。
 */

#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include "lvgl/src/libs/fsdrv/lv_fsdrv.h"
#include "car_ui.h"
#include "car_ui_sidebar.h"
#include "music_catalog.h"
#include "can_simulator.h"
#include "network_client.h"
#include "settings_config.h"
#include "settings_ui.h"
#include "app_config.h"
#include <unistd.h>
#include <pthread.h>

/* ========== 显示缓冲像素数量（RGB565 16-bit 色深：每像素 2 字节） ========== */
#define DISP_BUF_SIZE (DISP_HOR_RES * DISP_VER_RES * 2)

/* ========== 全局状态变量定义 ========== */

/* 互斥锁 */
pthread_mutex_t g_lvgl_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_can_mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_net_mutex  = PTHREAD_MUTEX_INITIALIZER;

/* 运行标志 */
volatile bool g_app_running = false;
bool g_video_overlay_active  = false;
time_t g_last_input_time     = 0;

/* CAN 数据 */
int g_speed_kmh    = 0;
int g_fuel_percent = 80;
int g_engine_rpm   = 800;

/* 网络数据 */
char g_weather_str[32]  = "";
int  g_temp_celsius     = 25;
char g_location_str[64] = "";
char g_nav_hint[32]     = "";

/* 系统设置 */
int g_sys_volume          = 10;
int g_sys_brightness      = 70;
int g_auto_screen_off_sec = 30;
char g_bluetooth_name[32] = "GEC6818-CAR";

/* 视频/音乐互斥标志：视频打开时若音乐正在播放则暂停，视频关闭后自动续播 */
bool g_music_interrupted_by_video = false;

/* 空调控制状态（Phase 5 模拟数据） */
int g_ac_temperature = 24;   /* 16-32 °C */
int g_ac_fan_speed   = 3;    /* 1-7 档 */
int g_ac_mode         = 0;   /* 0=吹面 1=吹脚 2=吹面+吹脚 3=除霜 */

/* 全局触摸回调：记录空闲时间 + 熄屏唤醒 */
static void on_global_input(lv_event_t *e)
{
    (void)e;
    if (car_ui_is_standby()) car_ui_standby_deactivate();
    g_last_input_time = time(NULL);
}

/* ========== 程序入口 ========== */
int main(void)
{
    /* 忽略 SIGPIPE：pipe 写端无读者时 write() 返回 -1 而非杀进程 */
    signal(SIGPIPE, SIG_IGN);

    /* ---- 1. LVGL 内核初始化 ---- */
    lv_init();

    /* ---- 2. 注册 STDIO 文件系统驱动 ---- */
    lv_fs_stdio_init();

    /* ---- 3. Linux framebuffer 显示驱动 ---- */
    fbdev_init();

    /* ---- 4. evdev 触摸输入驱动 ---- */
    evdev_init();

    /* ---- 5. 单缓冲绘制 ---- */
    static lv_color_t buf[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

    /* ---- 6. 注册显示设备 ---- */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb  = fbdev_flush;
    disp_drv.hor_res   = DISP_HOR_RES;
    disp_drv.ver_res   = DISP_VER_RES;
    lv_disp_drv_register(&disp_drv);

    /* ---- 7. 注册触摸输入设备 ---- */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = evdev_read;
    lv_indev_drv_register(&indev_drv);

    /* ---- 8. 扫描媒体目录 ---- */
    music_catalog_scan();

    /* ---- 8b. 加载用户配置（必须在 UI 创建前，确保初始值正确） ---- */
    settings_config_load();
    settings_backlight_apply();  /* 应用保存的亮度到硬件 */

    /* ---- 8c. 注册全局触摸事件（自动熄屏空闲检测） ---- */
    lv_obj_add_event_cb(lv_scr_act(), on_global_input, LV_EVENT_PRESSED, NULL);

    /* ---- 9. 创建车机主界面 ---- */
    car_ui_create_dashboard();

    /* ---- 10. 启动 CAN 数据模拟线程 ---- */
    can_simulator_start();

    /* ---- 11. 启动网络数据接收线程 ---- */
    network_client_start();

    printf("[Main] 智能车机系统启动完成\n");

    /* ---- 12. 主事件循环（约 60Hz） ---- */
    while (1) {
        lv_timer_handler();   /* 处理 LVGL 定时器/动画/事件 */
        lv_tick_inc(17);       /* 手动推进时间基准 */
        usleep(17000);         /* ~60Hz 间隔 */
    }

    return 0;
}
