/**
 * @file app_config.h
 * @brief 智能车机项目 — 公共宏定义与全局状态声明
 *
 * 所有业务模块通过包含本头文件获得统一的 POSIX 接口、布局参数、
 * FIFO 路径、网络配置，以及各阶段共享的全局变量与互斥锁声明。
 *
 * 各阶段按需使用对应区块的声明；未使用的声明在编译期无害。
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ========== 标准库 ========== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>          /* open, O_RDWR */
#include <sys/stat.h>       /* mkfifo */
#include <dirent.h>         /* opendir, readdir */
#include <time.h>
#include <math.h>

/* ========== 屏幕尺寸 ========== */
#define DISP_HOR_RES  800
#define DISP_VER_RES  480

/* ========== 布局参数（10px 边距 + 10px 区间距） ========== */
#define MARGIN        10     /* 四周边距 */
#define GAP           10     /* 区域间距 */
#define SIDEBAR_W     70     /* 左侧功能栏宽度 */
#define CENTER_W      440    /* 中间仪表盘宽度 */
#define RIGHT_W       250    /* 右侧两栏宽度 */
#define STATUS_H      140    /* 右上状态栏高度 */
#define MUSIC_BAR_H   310    /* 右下音乐栏高度 */
#define ZONE_H        460    /* 左侧/中间区域总高度 (=480-2*10) */

/* ========== FIFO 路径（Phase 2+ 音乐/视频播放） ========== */
#define MUSIC_FIFO_PATH  "/tmp/music_control"
#define VIDEO_FIFO_PATH  "/tmp/video_control"

/* ========== 媒体文件扫描路径 ========== */
#define MUSIC_DIR        "/mmcblk/music"
#define VIDEO_DIR        "/mmcblk/video"

/* ========== 网络配置（Phase 6） ========== */
#define SERVER_IP        "192.168.137.1"
#define SERVER_PORT      8888

/* ========== 设置文件路径（Phase 7） ========== */
#define CONFIG_FILE      "./config.json"

/* ================================================
 *  全局互斥锁与运行标志
 * ================================================ */

/** 保护 LVGL 控件操作（非主线程读写 UI 相关全局变量时使用） */
extern pthread_mutex_t g_lvgl_mutex;

/** 应用运行标志：置 false 后所有工作线程应在下一次循环检查时安全退出 */
extern volatile bool g_app_running;

/** 视频播放器覆盖标志：为 true 时仪表盘刷新暂停，避免 fbdev_flush 覆盖 mplayer 画面 */
extern bool g_video_overlay_active;

/* ================================================
 *  音乐播放控制全局状态（Phase 2）
 * ================================================ */
extern char g_music_path[200][512];
extern int  g_music_count;
extern int  g_music_index;
extern int  g_music_percent_pos;
extern float g_music_time_pos;
extern float g_music_time_length;
extern bool g_music_playing;
extern bool g_music_paused;

/** 视频播放时自动暂停音乐的恢复标志：视频关闭后自动续播 */
extern bool g_music_interrupted_by_video;

/* ================================================
 *  空调控制状态（Phase 5）—— 模拟数据
 * ================================================ */
extern int g_ac_temperature;    /* 温度 16-32 °C，默认 24 */
extern int g_ac_fan_speed;      /* 风量 1-7 档，默认 3 */
extern int g_ac_mode;           /* 模式 0=吹面 1=吹脚 2=吹面+吹脚 3=除霜 */

/* ================================================
 *  CAN 模拟数据（Phase 4）—— 由 can_simulator 线程更新
 * ================================================ */
extern pthread_mutex_t g_can_mutex;
extern int  g_speed_kmh;        /* 车速 0-240 km/h */
extern int  g_fuel_percent;     /* 油量 0-100 % */
extern int  g_engine_rpm;       /* 转速 0-8000 rpm */

/* ================================================
 *  网络数据（Phase 6）—— 由 network_receiver 线程更新
 * ================================================ */
extern pthread_mutex_t g_net_mutex;
extern char g_weather_str[32];  /* 天气描述，如 "晴" */
extern int  g_temp_celsius;     /* 室外温度 ℃ */
extern char g_location_str[64]; /* 定位城市 */
extern char g_nav_hint[32];     /* 导航提示文字 */

/* ================================================
 *  系统设置（Phase 7）
 * ================================================ */
extern int g_sys_volume;           /* 默认音量 0-100 */
extern int g_sys_brightness;       /* 屏幕亮度 0-100 */
extern int g_auto_screen_off_sec;  /* 自动熄屏倒计时秒数，0 = 永不息屏 */
extern char g_bluetooth_name[32];  /* 蓝牙设备名称 */

#endif /* APP_CONFIG_H */
