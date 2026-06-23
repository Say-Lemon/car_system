/**
 * @file playback_monitor.h
 * @brief 解析 MPlayer slave 应答并更新播放状态
 *
 * 读线程从 fp_mplayer（popen 管道）按行读取，识别例如：
 *   ANS_TIME_POSITION=12.3
 *   ANS_PERCENT_POSITION=45
 *   ANS_LENGTH=120.0
 */

#ifndef PLAYBACK_MONITOR_H
#define PLAYBACK_MONITOR_H

#include <stdio.h>
#include <stdbool.h>

/** 当前播放进度百分比 0~100 */
extern int percent_pos;

/** 媒体总时长、当前播放位置（秒） */
extern float time_length;
extern float time_pos;

/**
 * @brief 读线程入口：循环 fgets 解析 ANS_*，更新全局状态与 LVGL 控件
 *
 * 更新 UI 时必须 pthread_mutex_lock(&mutex_lv)。
 */
void *playback_status_reader_thread(void *arg);
void video_reset_volume_sync(void);
extern volatile bool g_video_eof;

#endif /* PLAYBACK_MONITOR_H */
