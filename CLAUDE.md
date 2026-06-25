# CLAUDE.md — 智能车机项目 (car_system)

## 项目概况

GEC6818 ARM Cortex-A53 开发板，800×480 LVGL v9.0-dev，arm-linux-gcc 交叉编译。
用户代码在 `usrCode/`，Makefile 自动扫描该目录所有 `.c`。

编译/部署在 WSL2 中：`make clean && make && make send`

## 开发阶段

| Phase | 内容 | 状态 |
|-------|------|------|
| 1 | 基础 UI 框架 + 仪表盘 | ✅ |
| 2 | 音乐播放 (mplayer fork+pipe) | ✅ |
| 3 | 视频播放 (复用 Project 2) | ✅ |
| 4 | CAN 数据模拟 | ✅ |
| 5 | 空调控制面板 + 应用菜单重构 | ✅ |
| 6 | 网络模块 | ✅ |
| 7 | 系统设置 | ⬜ |
| 8 | 综合联调 | ⬜ |

## 架构关键点

### 线程模型
- **主线程**: LVGL UI + 定时器回调（所有 lv_obj 操作必须在此线程）
- **音乐播放器**: Launch(fork+pipe, 每首歌一个) + Reader(fgets 解析 ANS_*) + Writer(周期性 get_time_pos/percent_pos)
- **视频播放器**: 同上，独立 mplayer 进程和 pipe
- **CAN 模拟器**: 200ms 周期线程，用 `g_can_mutex` 保护共享数据

### 中间区域互斥（Phase 5）
三个视图共享中间 440×460 区域：`g_dashboard_cont`(仪表盘)、`ac_cont`(空调面板)、`menu_cont`(应用菜单)。
通过 `car_ui_ac_panel_toggle()` / `app_menu_ui_toggle()` 切换，各自有 `hide()` 函数用于互斥。

### 音乐/视频互斥
视频优先：`playback_play_current()` 自动停音乐并设 `g_music_interrupted_by_video=true`，
`ui_player_close_cb()` 检查标志自动续播音乐。`music_play_current()` 检查 `play_flag` 拒绝在视频播放中启动。

## 已解决的问题（避免踩坑）

### 1. SIGPIPE 崩溃
**现象**: 音乐播完进程直接退出（无 core dump）
**根因**: mplayer 退出后 pipe 写端关闭，writer 线程 write() 触发 SIGPIPE
**修复**: `main()` 中 `signal(SIGPIPE, SIG_IGN);`

### 2. Seek 竞态条件（进度条卡死）
**现象**: 拖进度条后音乐跳转但进度条不动，再次拖动音乐卡死
**根因**: `pthread_kill(reader, SIGUSR1)` → handler 内 `pthread_kill(writer, SIGUSR2)`（异步）
→ reader `cond_signal(&wr_cond)` 时 writer 尚未进入 `cond_wait` → 信号丢失 → writer 永久阻塞
**修复**: 
- Seek 用 `seek <val> 1`（绝对百分比），不暂停任何线程
- Pause 改用标志位 `g_music_io_paused` + `sigaction`(无 SA_RESTART)，主线程统一控制

### 3. 中文字体
**字体**: `lv_font_conv --font NotoSansSC-VF.ttf --bpp 4 --format lvgl`
- `font_zh_cn.c` → `zh_cn_font` (16px)，宏 `APPLY_ZH_FONT`
- `font_zh_cn_14.c` → `zh_cn_14` (14px)，宏 `APPLY_ZH_14`
- `font_zh_cn_16.c` → `font_zh_cn_16` (16px，未使用)
- `font_zh_cn_18.c` → `font_zh_cn_18` (18px)，宏 `APPLY_ZH_18`

**关键坑**:
- `lv_conf.h` 必须 `LV_USE_FONT_COMPRESSED = 1`（字体是压缩格式 bitmap_format=1）
- 字体 .c 变量名和 .h 声明名必须一致（曾出现 `zh_cn_18` vs `font_zh_cn_18` 不匹配）
- 新增汉字时需用 lv_font_conv 重新生成，把新字符加到 `--symbols` 参数
- 字体 cmap 使用 RCP（Relative Code Point = Unicode - range_start），不是绝对码点
- 16px 字体缺失：风、档、吹、脚、除、霜、面 → 这些只在 18px 字体中

### 4. 视频与 LVGL 刷新冲突
**现象**: 播放视频时仪表盘/状态栏闪烁
**根因**: mplayer 直接写 /dev/fb0 与 LVGL 的 fbdev_flush 冲突
**修复**: 定时器回调中检查 `g_video_overlay_active`，为 true 时跳过 LVGL UI 刷新

### 5. 滑块触摸不灵敏
**现象**: 空调面板滑块难拖动
**根因**: 滑块对象高度仅 4px
**修复**: `lv_obj_set_height(slider, 30)` 提供触摸区，`lv_obj_set_style_height(slider, 4, LV_PART_MAIN)` 保持视觉细条

### 6. close/fclose 竞态
**现象**: stop_all 中 close/fclose 后 reader 线程崩溃
**根因**: 线程正在使用 fd/FILE*，主线程关闭导致 TOCTOU
**修复**: 只设 NULL/-1，不调用 close/fclose（嵌入式场景接受 fd 泄漏）

### 7. killall 误杀
**现象**: 视频切歌时音乐进程也被杀死
**根因**: `killall -9 mplayer` 无差别杀所有 mplayer
**修复**: 保存子进程 PID → `kill(pid, SIGKILL)` 精确控制

### 8. 爆音问题
**结论**: GEC6818 硬件限制，mplayer 音频初始化时必然短暂输出默认音量
**尝试过的无效方案**: -volume、-af volume=0.0、amixer、OSS ioctl、FIFO 预写、pause/unpause 循环

### 9. 字体重新生成（新增汉字时必读）
**场景**: 新增 UI 文字包含字体未覆盖的汉字，终端出现 `glyph dsc. not found for U+XXXX` 警告
**正确方法**:
1. **必须用原始字体** `NotoSansSC-VF.ttf`（换用 simhei 会丢失 ♫♪❄ 等特殊符号）
2. **从 git 提取原始符号集**：`git show HEAD:usrCode/font_zh_cn_18.c | head -4 | grep 'Opts:' | sed 's/.*--symbols //'`
3. **追加新字符后重新生成**：`lv_font_conv --no-compress --font NotoSansSC-VF.ttf --bpp 4 --format lvgl --size 18 --symbols "${ORIG}新字" -o font_zh_cn_18.c`
4. 只重新生成有缺字的那个尺寸的字体（14/16/18px），不影响其他尺寸
**字体文件对应**: font_zh_cn_14.c (14px/歌手名), font_zh_cn.c (16px/默认), font_zh_cn_18.c (18px/歌名天气)

## 常见错误速查

| 错误日志 | 原因 | 解决 |
|---------|------|------|
| `undefined reference to 'zh_cn_18'` | 字体变量名 头文件/.c 不匹配 | 对齐 font_zh_cn.h 声明与 .c 定义 |
| `Compressed fonts is used but LV_USE_FONT_COMPRESSED is not enabled` | lv_conf.h 未启用压缩字体 | `#define LV_USE_FONT_COMPRESSED 1` |
| `glyph dsc. not found for U+XXXX` | 字符不在字体 --symbols 中 | 见 §9：用 NotoSansSC-VF.ttf + 原始符号集追加新字 |
| 视频播放器退出后音乐不续播 | `play_flag` 未在 stop_all 中清零 | `playback_stop_all()` 中加 `play_flag = 0` |
