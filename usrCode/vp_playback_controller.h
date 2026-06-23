/**
 * @file playback_controller.h
 * @brief 播放引擎：MPlayer 进程启停、切歌与播放线程接口
 *
 * 全局状态说明：
 *   fp_mplayer — popen("mplayer ...", "r") 返回的 FILE*，读 slave 应答
 *   fd_mplayer — FIFO 文件描述符，写 slave 命令（在 ui_player_open_cb 中 open）
 *   video_index — 当前播放列表下标
 *   play_flag / start — 是否曾播放、读写线程是否已创建
 */

#ifndef PLAYBACK_CONTROLLER_H
#define PLAYBACK_CONTROLLER_H

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include "lvgl/lvgl.h"

extern FILE *fp_mplayer;
extern int fd_mplayer;
extern int video_index;
extern bool play_flag;
extern bool start;
extern pid_t mplayer_pid;

/* 定义在 vp_player_ui.c */
extern lv_obj_t *volume_slider;
extern lv_obj_t *bright_slider;
extern lv_obj_t *play_slider;

/** 强制结束所有 mplayer 进程（切歌、异常退出时使用） */
void playback_stop_all(void);

/** 播放 video_index 当前指向的视频（切歌统一入口） */
void playback_play_current(void);

/** 播放线程：popen 启动 mplayer 子进程 */
void *playback_launch_thread(void *arg);

#endif /* PLAYBACK_CONTROLLER_H */
