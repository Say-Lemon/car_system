/**
 * @file playback_io_sync.h
 * @brief 播放 I/O 后台线程、互斥锁与条件变量声明
 *
 * 与 playback_controller / playback_monitor / player_ui 协作：
 *   - playback_io_query_writer_thread：周期性向 FIFO 发送 get_time_pos / get_percent_pos
 *   - playback_status_reader_thread：从 popen 管道读 ANS_* 应答
 *   - playback_io_pause_*：在 seek、拖进度条时暂停读/写，避免状态竞争
 *
 * 读/写线程在 playback_play_current() 首次播放时创建。
 */

#ifndef PLAYBACK_IO_SYNC_H
#define PLAYBACK_IO_SYNC_H

#include <pthread.h>

/** 保护 LVGL 控件更新（读线程改 label/slider 时必须持有） */
extern pthread_mutex_t mutex_lv;

/** 配合 cond，供 playback_io_pause_reader_on_sig 中 pthread_cond_wait 使用 */
extern pthread_mutex_t mutex;

/** 配合 cond1，供 playback_io_pause_writer_on_sig 中 pthread_cond_wait 使用 */
extern pthread_mutex_t mutex1;

/** UI seek 结束后 pthread_cond_signal(&cond) 唤醒读线程 */
extern pthread_cond_t cond;

/** 读线程暂停写线程后，用于写线程侧的等待/唤醒 */
extern pthread_cond_t cond1;

extern pthread_t tid_read;   /* playback_status_reader_thread */
extern pthread_t tid_write;  /* playback_io_query_writer_thread */

void playback_io_pause_reader_on_sig(int sig);
void playback_io_pause_writer_on_sig(int sig);
void *playback_io_query_writer_thread(void *arg);

#endif /* PLAYBACK_IO_SYNC_H */
