/**
 * @file vp_playback_io_sync.c
 * @brief 播放 I/O 轮询写线程与读/写协调（信号 + 条件变量）
 *
 * 设计原因：
 *   MPlayer slave 模式不会主动推送进度，需要周期性 write "get_time_pos\n" 等命令，
 *   再在 popen 的 stdout 上读 ANS_* 行（见 playback_monitor.c）。
 *   用户拖动进度条或 seek 时，若读写仍在进行，进度会乱跳，故用信号 10/12
 *   让读/写线程在 cond 上等待，UI 完成后再 signal 唤醒。
 */

#include "vp_playback_io_sync.h"
#include "vp_playback_controller.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

pthread_mutex_t mutex_lv  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex1    = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond      = PTHREAD_COND_INITIALIZER;
pthread_cond_t  cond1     = PTHREAD_COND_INITIALIZER;

pthread_t tid_read;
pthread_t tid_write;

/**
 * @brief 读线程收到信号 10 时的处理（由 ui_playback_slider_cb 等 pthread_kill 触发）
 *
 * 流程：先让写线程在信号 12 处理函数里阻塞 → 读线程在 cond 上等待 →
 *       UI 发完 seek 后 pthread_cond_signal(&cond) 继续读。
 */
void playback_io_pause_reader_on_sig(int sig)
{
    if (sig == 10) {
        printf("收到信号 %d：暂停读线程\n", sig);
        pthread_kill(tid_write, 12);
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&cond1);
        puts("继续工作：读 mplayer");
    }
}

void playback_io_pause_writer_on_sig(int sig)
{
    printf("收到信号 %d：暂停写线程\n", sig);
    pthread_mutex_lock(&mutex1);
    pthread_cond_wait(&cond1, &mutex1);
    pthread_mutex_unlock(&mutex1);
    puts("继续工作：写 mplayer");
}

/**
 * @brief 写线程：约每 800ms 向 FIFO 查询一次当前时间与播放百分比
 *
 * 须在 playback_play_current 首次播放且 fd_mplayer 已 open 后才会有效写入。
 * 与 playback_status_reader_thread 配合构成“问—答”循环。
 */
void *playback_io_query_writer_thread(void *arg)
{
    (void)arg;

    signal(12, playback_io_pause_writer_on_sig);

    const char *cmd_time    = "get_time_pos\n";
    const char *cmd_percent = "get_percent_pos\n";

    while (1) {
        if (fd_mplayer > 0) {
            write(fd_mplayer, cmd_time, strlen(cmd_time));
            usleep(400 * 1000);
            write(fd_mplayer, cmd_percent, strlen(cmd_percent));
            usleep(400 * 1000);
        } else {
            usleep(100000);  /* FIFO 尚未打开时空转等待 */
        }
    }
    return NULL;
}
