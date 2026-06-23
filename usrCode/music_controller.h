/**
 * @file music_controller.h
 * @brief 音乐播放控制器 — mplayer 进程管理 + FIFO + 读写线程
 *
 * 架构（继承自 player_project）：
 *   - music_play_current() → 启动 mplayer (fork+pipe), 初始查询
 *   - music_reader_thread() → 解析 ANS_* 响应, 更新进度/时长
 *   - music_writer_thread() → 周期性查询时间/百分比
 *   - music_send_cmd() → 写 pipe 命令
 *   - Seek：直接发 "seek <val> 1"（绝对百分比），无需暂停读写线程
 *   - Pause：标志位 g_music_io_paused + SIGUSR1 中断 fgets，主线程控制全流程
 */

#ifndef MUSIC_CONTROLLER_H
#define MUSIC_CONTROLLER_H

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

/* ---- 全局状态 ---- */
extern FILE *g_fp_music_mplayer;    /* popen 返回的 stdout 管道 */
extern int   g_fd_music_fifo;       /* FIFO 文件描述符 */
extern int   g_music_index;         /* 当前播放索引 */
extern int   g_music_percent_pos;   /* 0-100 */
extern float g_music_time_pos;      /* 秒 */
extern float g_music_time_length;   /* 秒 */
extern bool  g_music_playing;       /* 是否在播放 */
extern bool  g_music_paused;        /* 是否暂停 */
extern pid_t music_mplayer_pid;     /* mplayer 进程 PID */
extern volatile bool g_music_eof;   /* reader 线程设 true → 主线程切歌 */

/* ---- 核心 API ---- */
void music_play_current(void);      /* 播放 g_music_index 指向的歌曲 */
void music_stop_all(void);          /* 停止所有播放 */
void music_toggle_pause(void);      /* 暂停/继续切换 */
void music_play_next(void);         /* 下一首 */
void music_play_prev(void);         /* 上一首 */
void music_send_cmd(const char *fmt, ...);  /* 向 FIFO 发送命令 */
void music_set_volume(int ui_vol);  /* 设置音量（UI值→mplayer值曲线映射） */

/* ---- 线程入口 ---- */
void *music_launch_thread(void *arg);
void *music_reader_thread(void *arg);
void *music_writer_thread(void *arg);

/* ---- 同步对象（供 music_player_ui seek 使用） ---- */
extern pthread_mutex_t g_music_lv_mutex;
extern pthread_mutex_t g_music_rd_mutex;
/* g_music_wr_mutex / g_music_wr_cond 已移除：Writer 直接检查 g_music_paused 标志 */
extern pthread_cond_t  g_music_rd_cond;
extern pthread_t       g_tid_music_read;
extern pthread_t       g_tid_music_write;

#endif
