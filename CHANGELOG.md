# 智能车机项目 (car_system) — 开发日志

## 2026-06-23 (续4) — Phase 5: 空调控制面板

### 空调控制 UI
- 创建 `car_ui_ac_panel.c/h` — 中间区域（440×460）空调控制面板
- 控件布局：
  - **温度**：大号数值显示（28px 白字）+ 滑块（16-32 °C），橙色风格
  - **风量**：数值显示（24px 白字）+ 滑块（1-7 档），橙色风格
  - **模式**：2×2 按钮网格（吹面/吹脚/吹面+吹脚/除霜），选中态橙色底 `#FF6D00`+白字
- UI 风格：`#1A1A2E` 深底 + `#FF6D00` 橙色 + 圆角 12px，与主界面一致

### 仪表盘 ↔ 空调切换
- `g_dashboard_cont` 暴露仪表盘容器句柄
- 点击侧边栏 ❄ → `car_ui_ac_panel_toggle()` 切换仪表盘/空调面板可见性
- 空调面板初始隐藏，启动时预创建；切换时两者互斥显示

### 空调数据模型（模拟）
- `g_ac_temperature` — 16-32 °C，默认 24
- `g_ac_fan_speed` — 1-7 档，默认 3
- `g_ac_mode` — 0=吹面 1=吹脚 2=吹面+吹脚 3=除霜

### 修改文件
- `car_ui_ac_panel.c` — **新建**：空调面板 UI + 滑块/按钮回调 + toggle 逻辑
- `car_ui_ac_panel.h` — **新建**：`create` + `toggle` 声明
- `car_ui_dashboard.c` — 容器 `g_dashboard_cont` 全局化
- `car_ui_dashboard.h` — 声明 `g_dashboard_cont`
- `car_ui_sidebar.c` — `sidebar_ac_cb` 调用 toggle
- `car_ui.c` — 启动时创建空调面板（初始隐藏）
- `app_config.h` — 新增 `g_ac_temperature/fan_speed/mode` 声明
- `main.c` — 定义 AC 全局变量

---

## 2026-06-23 (续3) — Phase 4: CAN 数据模拟

### 驾驶循环模拟
- 创建 `can_simulator.c/h` — 后台线程模拟 CAN 总线数据
- 驾驶状态机：IDLE(熄火) → ACCELERATING(加速) → CRUISING(巡航) → DECELERATING(减速)
  - 加速：每 tick +2~5 km/h，随机目标 80~120 km/h
  - 巡航：±1 km/h 波动，持续 3~8 秒
  - 减速：每 tick -2~5 km/h，至 0 转入熄火
  - 熄火：speed=0，持续 2~5 秒后重新起步
- 转速：`RPM = speed × 55 + rand(-100,+100)`，熄火时 RPM=0，高速 ~6600 RPM
- 油耗：仅行驶中消耗 `0.005 + (RPM/4000)×0.01` 每 tick（200ms），熄火不耗油；约 1.5%/min(低速)~6%/min(高速)，最低 5%
- 更新周期 200ms（5Hz），通过 `g_can_mutex` 保护共享数据
- 用自包含 LCG 伪随机（避免 `rand()` 的线程安全隐患）

### 仪表盘实时刷新
- `car_ui_dashboard.c` 新增 200ms LVGL 刷新定时器
- `dashboard_update_speed/fuel/rpm()` 安全更新标签（仅主线程操作 LVGL）
- 定时器回调加锁读取 CAN 全局变量后更新 UI

### 修改文件
- `can_simulator.c` — **新建**：驾驶循环线程 + 状态机
- `can_simulator.h` — **新建**：`can_simulator_start()` 声明
- `car_ui_dashboard.c` — 新增刷新定时器 + 3 个更新函数
- `car_ui_dashboard.h` — 声明 `dashboard_update_speed/fuel/rpm`
- `main.c` — 引入 can_simulator，启动模拟线程

---

## 2026-06-23 (续2) — 音乐/视频互斥播放

### 同时播放问题
- 现象：音乐和视频各有一个独立的 mplayer 进程，无互斥机制。同时播放会争抢 `/dev/dsp` 音频设备（后启动者静音/报错），且两个 mplayer 同时运行导致 CPU 过载
- 修复：**视频优先策略**
  1. `playback_play_current()` 启动视频时自动 `music_stop_all()` 暂停音乐，设 `g_music_interrupted_by_video = true`
  2. `ui_player_close_cb()` 关闭视频时若标志为真则自动 `music_play_current()` 续播
  3. `music_play_current()` 启动音乐时检查 `play_flag`，若视频播放中则拒绝启动
  4. `playback_stop_all()` 新增 `play_flag = 0`（之前该标志从未被清除，导致状态检测失效）

### 修改文件
- `app_config.h` — 新增 `g_music_interrupted_by_video` 声明
- `main.c` — 定义 `g_music_interrupted_by_video = false`
- `vp_playback_controller.c` — 视频启动时停音乐；`playback_stop_all` 清除 `play_flag`
- `vp_player_ui.c` — 视频关闭时续播音乐
- `music_controller.c` — 视频播放中拒绝启动音乐

---

## 2026-06-23 (续) — 音乐播放器 Seek 竞态条件修复

### Seek 拖进度条偶发卡死
- 现象：拖动进度条 seek 后，音乐实际跳转了但进度条停在原位不再移动，音乐继续播放。再次拖进度条导致播放完全卡死但不崩溃
- 根因：**异步信号丢失竞态条件**。Seek 时 `pthread_kill(reader, SIGUSR1)` → handler 内 `pthread_kill(writer, SIGUSR2)`（异步）→ reader `cond_wait` → seek 完成 → `cond_signal` 唤醒 reader → reader handler `cond_signal(&wr_cond)` 唤醒 writer。但 Writer 的信号处理是异步的——如果 writer 尚未进入 `cond_wait`，`cond_signal` 成为 no-op，writer 随后进入 `cond_wait` 并**永久阻塞**，不再发送 `get_time_pos`/`get_percent_pos`，进度条卡死
- 修复：
  1. **Seek 改用绝对百分比** `seek <val> 1`（而非 delta），无需暂停读写线程，彻底消除竞态窗口
  2. **暂停机制重构**：用 `g_music_io_paused` 标志位 + 主线程控制替代信号链式触发
     - Reader：`sigaction`（无 SA_RESTART）注册最小 handler（仅设标志），主循环检查 `g_music_io_paused` 进入 `cond_wait`
     - Writer：移除信号 handler，直接检查 `g_music_paused` 标志（已有逻辑）
     - `toggle_pause`：主线程设置标志 → 中断 reader → 等待 → 停止 mplayer（全程可控）
  3. **移除 `g_music_wr_mutex`/`g_music_wr_cond`**：Writer 不再需要信号同步
  4. **视频播放器 seek 同步修复**：同样改用 `seek %d 1` 绝对百分比

### 修改文件
- `music_controller.c` — 信号同步→标志位同步，移除 Writer 信号 handler
- `music_controller.h` — 更新注释，移除 wr_mutex/wr_cond 声明
- `car_ui_music_bar.c` — `music_bar_seek_cb` 简化为一行 seek 命令
- `music_player_ui.c` — `on_seek` 简化为一行 seek 命令
- `vp_player_ui.c` — 视频 seek 同样简化

---

## 2026-06-23 — 代码审查 + 关键 Bug 修复 + SIGPIPE 崩溃

### 代码审查（5 严重 + 5 高危）
通过全量审查发现 10 个 bug，全部修复：

#### 严重 (CRITICAL)
1. **cond_wait 未加锁** — `vp_playback_io_sync.c` 中 `pthread_cond_wait` 前缺少 `pthread_mutex_lock`，POSIX 未定义行为，seek 时可能死锁
2. **fdopen FILE* 泄漏** — stop 时只设 NULL 不 fclose，每次切歌泄漏 pipe read-end fd 和 FILE*
3. **killall 误杀** — `killall -9/-19/-18 mplayer` 无差别杀所有 mplayer，视频切歌会杀音乐进程。改为保存子进程 PID，`kill(pid, signal)` 精确控制
4. **auto-next 跨线程调 LVGL** — reader 线程直接调 `playback_play_current()` 写 LVGL 控件无锁，改为设 `g_video_eof` 标志，主线程定时器安全处理
5. **stop_all 中 fclose 崩溃** — reader 线程检测 EOF 后调用 stop_all → fclose 关闭自己正在 fgets 的 FILE*。改为只设 NULL 不 close/fclose

#### 高危 (HIGH)
- seek 信号/condvar 丢唤醒风险（加超时保护）
- 音乐全局变量无同步（`g_music_lv_mutex` 被定义但从未使用）
- 视频 slider 读写不对称加锁
- 迷你栏定时器句柄未保存无法清理
- `threads_created` 标志无同步

### SIGPIPE 崩溃（音乐 EOF 后进程退出）
- 现象：歌曲播放结束→reader 检测 EOF→设标志→进程直接退出（无 crash 日志）
- 诊断过程：加 PID/EOF/stop/auto-next 日志逐点追踪，发现 EOF 标志正确设置，但定时器来不及处理进程就已退出
- 根因：**SIGPIPE**。mplayer 退出后 stdin pipe 写端仍打开，写线程 `write()` → 无读者 → 内核发送 SIGPIPE，默认行为是终止进程
- 修复：`main()` 中 `signal(SIGPIPE, SIG_IGN)` 忽略 SIGPIPE，`write()` 改为返回 -1 而非杀进程

### 架构决策
- stop_all 不再 close/fclose pipe fd/FILE*（避免 TOCTOU 竞态），改为只设 -1/NULL 让线程检查跳过。代价：每次切歌泄漏一对 fd（嵌入式场景可接受）
- auto-next 从 reader 线程移到主线程定时器：reader 设 eof 标志 → 主线程 500ms 定时器检测 → 安全切歌

---

## 2026-06-22 (续5) — 退出视频播放器崩溃修复 + 音量标签同步(续)

### 退出视频播放器导致程序崩溃
- 现象：视频播放中按右上角关闭键，程序直接退出
- 根因：关闭时 `system("killall -9 mplayer")` 杀死 mplayer，但 reader 线程仍在访问旧的 LVGL 控件指针。随后 `lv_obj_del(cont)` 删除控件，reader 线程持有的 `play_slider`、`time_pos_label` 等变为野指针，随后访问导致 segfault
- 修复：关闭回调中设置 `fp_mplayer = NULL` + `fd_mplayer = -1` + `start = 0`，reader 线程检测到 `fp_mplayer == NULL` 后自旋等待不再 fgets，确保不会访问已删除的 LVGL 控件

### 音量标签不同步（第二次修复）
- 现象：主界面改音量→开视频播放器，滑块同步但数值标签(如"100")不变；第一次修复后仍无效
- 根因：`ui_playback_slider_cb` 顶部 `if (!start) return;` 拦截——未播视频时 start=0，即使 `lv_event_send(LV_EVENT_VALUE_CHANGED)` 触发回调也被直接返回
- 修复：将音量处理移到 `start` 检查之前，让音量始终可同步

### mp3 解码器缺失
- 现象：终端提示 "请求的音频编解码器族 [mp3] (afm=mp3lib) 不可用"
- 原因：开发板的 mplayer 编译时未启用 mp3lib
- 影响：部分 MP3 音轨的视频无声音，画面正常；不影响 AAC/PCM 音轨视频
- 解决：需重新交叉编译 mplayer 启用 mp3lib（非代码修改）

---

## 2026-06-22 (续4) — 视频切歌崩溃修复 + 音量标签同步

### 视频播放器切歌崩溃
- 现象：播放视频时点击列表切歌，程序直接退出
- 根因：fork+pipe 架构下，旧 mplayer 进程被 kill 后，父进程仍持有 pipe 写端 `fd_mplayer`
- reader 线程的 `fgets` 因 pipe 写端未关闭而阻塞（而非返回 EOF），导致死锁
- 修复：`playback_stop_all()` 中 `close(fd_mplayer)` 关闭旧 pipe 写端，强制 reader 检测 EOF
- 音乐播放器同步修复

### 视频播放器音量标签不同步
- 现象：主界面改音量后，视频播放器滑块同步但数值标签(如"100")不变
- 修复：`playback_play_current()` 中 `lv_slider_set_value` 后追加 `lv_event_send(LV_EVENT_VALUE_CHANGED)` 触发回调更新标签

---

## 2026-06-22 (续3) — 视频播放器完整重构

### 架构升级：popen+FIFO → fork+pipe
- 视频和音乐播放器均从 popen+FIFO 迁移到 fork+pipe 架构
- 不再依赖 `/tmp/music_control` 和 `/tmp/video_control` 命名管道
- mplayer 的 stdin/stdout 直接通过 pipe 与父进程通信
- 消除 FIFO 时序问题和 O_RDWR 缓冲不确定性

### 视频播放器 UI 优化（全部重新应用）
- 运输按钮：透明底 + 放大白色图标 + 橙色播放键
- 关闭键：橙色底 + LV_SYMBOL_CLOSE（替代 "X"）
- 播放列表 "List" 键：橙色底
- 进度条/音量/亮度滑块：细条(4px) + 橙色填充(#FF6D00) + 橙色小球(8px)
- 英文标签改为中文："volume"→"音量"、"bright"→"亮度"
- 播放列表下移（y: -25 → 30）
- 打开时不自动播放

### 音量同步修复
- 视频音量回调：更新 g_sys_volume 并通过 /5 曲线发送到 pipe
- 切歌时 launch 线程 300ms 延迟后重新发送音量（确保新 mplayer 就绪）
- playback_play_current 中同步 slider 显示值到 g_sys_volume

### 爆音问题（已接受为硬件限制）
- 尝试方案：-volume 参数、-af volume=0.0、amixer、OSS ioctl、FIFO 预写、循环发送、pause+unpause
- 全部失败，根因：GEC6818 的 mplayer 音频初始化时必然短暂输出默认音量
- 板子无 amixer，OSS 设备存在但 ioctl 对 mplayer 音频路径无效
- 最终状态：代码干净，无实验残留

### 代码审查与清理
- 移除 oss_volume.c/h
- 移除 vp_playback_monitor 中的 video_sync_volume 实验代码
- 恢复 vp_playback_controller.h 的 bright_slider 声明
- 所有 vp_* 文件 includes 正确使用 vp_ 前缀
- 验证全部历史功能：fork+pipe、UI 样式、音量曲线、覆盖标志、应用菜单、仪表盘音量标签

### 音量映射迭代
- 初版幂次曲线 (0.55) → 用户反馈仍太大声
- 改为线性 /2 → 太大声
- 改为线性 /5 → 100→20, 50→10, 最终确定
- 初始音量: 80 → 10 (实际值 2)
- `math.h` 依赖已移除 (不再需要 powf)

### 播放/暂停图标同步
- 迷你栏和全屏播放器的播放键图标不同步
- 在两个界面的 500ms 刷新定时器中加入:
  `lv_label_set_text(label, g_music_paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE)`
- 无论在哪边操作，另一边自动跟上

---

## 2026-06-22 (续) — 音乐播放完善

### 全屏播放器 UI 优化
- 布局: 左2/3(520px)控制 → 右1/3(250px)列表
- 播放键: 橙底圆→透明底橙色三角
- 按钮缩小下移, 进度条下移, 歌名/歌手居中
- "播放列表"和"音量"中文修复: 缺"列表"二字, 重建字体
- 关闭键补 border/shadow 清零

### Bug 修复
- `pclose` 在 reader 线程读取时调用导致卡死 → 只设 `g_fp_music_mplayer = NULL`
- 迷你栏播放键不切换图标 → 回调中补充 `lv_label_set_text`
- 音乐目录从 `/mnt/sdcard/music` 改为 `/mmcblk/music`

---

## 2026-06-16 — 项目启动 & Phase 1 基础界面框架

### 项目初始化
- 从 `player_project/` 复制 `lvgl/`, `lv_drivers/`, `lv_conf.h`, `lv_drv_conf.h`, `mouse_cursor_icon.c`
- 创建 `Makefile`（arm-linux-gcc, -lm -lpthread）
- 创建 `usrCode/` 目录

### 基础 UI 框架
- `main.c` — LVGL/fbdev/evdev 初始化 + 主循环
- `app_config.h` — 布局宏 (10px边距+间距), 全局状态声明
- `car_ui.c/h` — 4区域布局编排器
- `car_ui_sidebar.c/h` — 左侧圆形图标栏 (5键)
- `car_ui_dashboard.c/h` — 中间仪表盘 (car.png + 数据叠加)
- `car_ui_status.c/h` — 右上时间天气卡片
- `car_ui_music_bar.c/h` — 右下音乐控制卡片

### 中文字体
- 用 `lv_font_conv` + NotoSansSC.ttf 生成 `zh_cn_font` (16px 4bpp)
- 生成 `font_icons_20.c` (侧边栏图标 20px)
- 生成 `font_snowflake_26.c` (雪花 26px)
- 后来生成多尺寸: `zh_cn_14`, `zh_cn_18`
- 多次增量添加字符(番禺晴转多云手列表等), 最终 562 汉字

### 侧边栏按钮
- 圆形 56px 图标按钮, 弹性列布局
- 电源 (U+E7E8), Vol+ (U+E995), Vol- (U+E993), 空调 (U+2744), 菜单 (U+F00B)
- 前4个深色底, 菜单键亮橙底+外发光
- 空调雪花从 16px→20px→24px→26px 多次调整

### 中间仪表盘图片
- 尝试 BMP → 失败 (LVGL 仅支持 24-bit 无压缩)
- 尝试 PNG 文件系统加载 → 三处根因:
  1. `lv_fs_stdio_init()` 未调用
  2. `LV_FS_STDIO_PATH "/"` 导致路径指向根目录
  3. lodepng 与 LVGL 内置分配器不兼容 (realloc 失败)
- 最终方案: 嵌入式 C 数组 `car_img_data.c` (440×460 BGRA)
- `stride` 字段不存在 (v9.0-dev)
- 颜色偏蓝修复: ARGB→BGRA 字节序

### 右上时间天气卡片
- 半透明→不透明卡片 (0x1A1A2E)
- 时钟行: 时间+星期/日期弹性居中
- 天气行: 番禺+温度+天气
- 移除 📍 emoji (字体不支持)
- 移除 "定位中..." 标签

### 右下音乐控制卡片
- "♫ 正在播放" 橙色标题
- 歌名+歌手居中, 文字滚动
- 细进度条(4px), 橙色填充+橙色小球
- 透明底大图标控制按钮
- 时间标签

---

## 2026-06-17 — Phase 1 完善 & Phase 2 音乐播放

### Phase 1 收尾
- 布局调整: 中心区 410→440, 右侧 280→250
- 数据叠加透明化 (移除黑色底条)
- 车速: 大号"0" + 下方"KM/H", 72px→48px
- 油量/转速: 18px 中文, 无底色
- 代码审查: 移除死代码, 修正注释, 清理冗余include
- 更新 `DASHBOARD_DEBUG.md`

### Phase 2 音乐播放模块

#### 新增文件
- `music_catalog.c/h` — 扫描 `/mmcblk/music` (原 `/mnt/sdcard/music` 后改)
- `music_controller.c/h` — popen + FIFO + 读写线程 + seek同步
- `music_player_ui.c/h` — 全屏播放界面 (左2/3控制+右1/3列表)

#### 架构
- 4线程模型: Main(LVGL) + Launch(popen) + Reader(ANS_*解析) + Writer(周期性查询)
- IPC: `/tmp/music_control` FIFO + popen stdout 管道
- seek: SIGUSR1/2 + condvar 同步 + killall -19/-18 mplayer
- 自动下一首: percent_pos≥98% 触发 (加 cooldown 防重复)

#### 回调接线
- 侧边栏: Vol+/Vol- → FIFO volume 命令; Menu → 打开播放器
- 迷你栏: Play/Pause/Prev/Next → 播放控制; 500ms 定时器刷新进度
- 迷你栏进度条: 拖拽→seek

#### 播放器 UI 迭代
- 初版: 上下布局 (控制区+列表)
- 终版: 左2/3(520px)控制 + 右1/3(250px)列表
- 播放键: 橙底圆→透明底橙色三角
- 按钮缩小并下移, 进度条下移, 歌名/歌手居中

#### Bug 修复
- `music_controller.h` 缺 `#include <pthread.h>` → 类型未定义
- `music_bar_refresh_cb` 前向声明顺序错误
- 全局变量 main.c + music_controller.c 重复定义 → 从 main.c 移除
- `pclose` 在 reader 线程读取时调用导致卡死 → 只设 NULL
- 迷你栏播放键不切换 ⏸/▶ 图标 → 补充 `lv_label_set_text`

### 字体
- 生成 14/16/18px 三尺寸中文字体
- `APPLY_ZH_14/18` 宏按尺寸区分
- 全项目中文统一 18px, 歌手名 14px

---

## 2026-06-24 — Phase 6: 网络通信集成

### 网络客户端
- 创建 `network_client.c/h` — TCP 客户端线程，连接 `192.168.137.1:8888` 接收 JSON 天气数据
- 参照 `can_simulator.c` 模式: detached 线程 + mutex + lv_async_call
- 自动重连机制：连接失败/断开后每 3 秒重试，不影响其他模块
- cJSON 库集成：下载 v1.7.18 到 `usrCode/cJSON/`，Makefile 已有 include 路径

### JSON 协议
- 服务器每 5 秒发送一行: `{"weather":"晴","temp":28,"location":"番禺"}`
- 字段: weather(天气描述), temp(温度℃), location(城市名)
- cJSON 解析在锁外完成，仅 strncpy/int 赋值在 g_net_mutex 临界区内

### 天气服务器
- 创建 `scripts/weather_server.py` — 初版模拟 8 种天气场景循环
- 升级为真实天气：接入 wttr.in 免费 API，无需注册
- 英→中翻译表覆盖 20+ 天气描述（晴/多云/阴/小雨/阵雨/雷阵雨/暴雨/雾/雪等）
- **部署方式**：必须在 Windows 终端运行（`python scripts/weather_server.py`），开发板连 `192.168.137.1:8888`
- WSL2 网络隔离解决方案：端口转发（可用但不稳定）→ 最终改为 Windows 直连

### 时间同步
- JSON 新增 `"time"` 字段（Unix 时间戳），服务器每次发送
- 开发板首次收到数据时调用 `settimeofday()` 设置系统时钟
- `setenv("TZ", "CST-8") + tzset()` 配置中国标准时间（UTC+8）
- 解决开发板无 RTC 电池导致每次启动日期为 2015/1/1 的问题

### UI 刷新保护
- `ui_refresh_status_labels()` 首次被实际调用（此前已实现但从未使用）
- 新增 `g_video_overlay_active` 检查：视频播放时跳过天气刷新，避免 fbdev_flush 冲突闪烁

### 字体更新
- 天气数据引入新汉字（广州阴阵雷圳雾暴冻雹沙尘霾）→ `font_zh_cn_18.c` 两次重新生成，689→699 字符
- **教训**：必须用原始字体 `NotoSansSC-VF.ttf` + git 历史提取原始符号集 + 追加新字，换用 simhei 会丢失 ♫♪❄ 等图标

### 代码清理
- `car_ui.c/h` 删除 4 个未使用的静态变量和 getter 函数（死代码）
- `CHANGELOG.md` 和 `DASHBOARD_DEBUG.md` 文件清单更新为 Phase 5 完整状态

### 新增文件
- `usrCode/network_client.c/h` — TCP 网络客户端
- `usrCode/cJSON/cJSON.c/h` — JSON 解析库
- `scripts/weather_server.py` — 测试用天气服务器

### 修改文件
- `usrCode/main.c` — +2 行集成网络线程
- `usrCode/network_client.c` — 时间同步（settimeofday + TZ）
- `usrCode/car_ui_status.c` — g_video_overlay_active 保护
- `usrCode/car_ui.c/h` — 清理死代码
- `usrCode/font_zh_cn_18.c` — 三次重新生成，追加 13 个汉字
- `usrCode/font_symbols.txt` — 同步更新
- `scripts/weather_server.py` — 模拟→真实天气 API + 时间戳

---

## 文件清单

```
usrCode/
├── main.c                         # 入口 — LVGL/fbdev/evdev 初始化 + 主循环
├── app_config.h                   # 公共配置 — 布局宏、全局状态、FIFO 路径
│
├── car_ui.c/h                     # 布局编排器 — 调用各子模块组装主界面
├── car_ui_sidebar.c/h             # 侧边栏 — 5 圆形图标按钮 (70×460)
├── car_ui_dashboard.c/h           # 仪表盘 — 车速/油量/转速 + CAN 刷新 (440×460)
├── car_ui_ac_panel.c/h            # 空调面板 — 温度/风量滑块 + 吹风模式 (Phase 5)
├── app_menu_ui.c/h                # 应用菜单 — 2×2 应用网格 (Phase 5)
├── car_ui_status.c/h              # 右上状态 — 时间/日期/天气 (250×140)
├── car_ui_music_bar.c/h           # 右下音乐栏 — 歌名/进度/播放控制 (250×310)
│
├── music_catalog.c/h              # 音乐目录扫描 — /mmcblk/music → mp3 列表
├── music_controller.c/h           # 音乐播放核心 — fork+pipe + 读写线程
├── music_player_ui.c/h            # 全屏音乐播放器 — 左控制 + 右列表
│
├── vp_config.h                    # 视频播放器配置 — FIFO 路径、头文件
├── vp_media_catalog.c/h           # 视频目录扫描 — /mmcblk/video → 视频列表
├── vp_playback_controller.c/h     # 视频播放引擎 — fork+pipe + mplayer 管理
├── vp_playback_io_sync.c/h        # 视频 I/O 同步 — 读写线程 + 信号/cond 协调
├── vp_playback_monitor.c/h        # 视频状态解析 — ANS_* 应答 → 进度/时间
├── vp_player_ui.c/h               # 视频播放器 UI — 控制栏/进度条/列表
│
├── can_simulator.c/h              # CAN 数据模拟 — 驾驶循环状态机 (Phase 4)
├── network_client.c/h              # TCP 网络客户端 — JSON 天气数据接收 (Phase 6)
├── cJSON/cJSON.c/h                 # JSON 解析库 — v1.7.18 (Phase 6)
│
├── font_zh_cn.c/h                 # 中文 16px — zh_cn_font + APPLY_ZH_FONT 宏
├── font_zh_cn_14.c                # 中文 14px — zh_cn_14 + APPLY_ZH_14 宏
├── font_zh_cn_16.c                # 中文 16px — font_zh_cn_16（未使用）
├── font_zh_cn_18.c                # 中文 18px — font_zh_cn_18 + APPLY_ZH_18 宏
├── font_icons_20.c/h              # 图标 20px — Segoe MDL2 (电源/音量)
├── font_snowflake_26.c            # 雪花 26px — ❄ 空调图标
├── font_symbols.txt               # 字体字符清单 — 562 汉字 + 符号
│
└── car_img_data.c                 # 仪表盘背景图 — 440×460 BGRA 嵌入式数组
```
