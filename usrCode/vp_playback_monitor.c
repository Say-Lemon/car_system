/**
 * @file playback_monitor.c
 * @brief 解析 MPlayer slave 模式标准输出，刷新进度条与时间标签
 *
 * 与 playback_io_query_writer_thread 配合：写线程发 get_*，本线程读 ANS_*。
 * 播放到 percent_pos >= 99 时自动 playback_play_current() 切下一首。
 */

#include "vp_playback_monitor.h"
#include "lvgl/lvgl.h"
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

int percent_pos = 0;
float time_length = 0;
float time_pos = 0;
volatile bool g_video_eof = false;

/** 缓存上一帧显示的时间字符串，避免每秒重复 set_text */
static char show_time_buf[100] = {0};

extern lv_obj_t *play_slider;
extern lv_obj_t *time_pos_label;
extern lv_obj_t *time_length_label;
extern pthread_mutex_t mutex_lv;
extern FILE *fp_mplayer;
extern void playback_io_pause_reader_on_sig(int sig);

void *playback_status_reader_thread(void *arg)
{
    (void)arg;

    signal(10, playback_io_pause_reader_on_sig);

    char line[1024];
    char *p;

    while (1) {
        /* 切歌时旧 popen 可能已关闭，新 playback_launch_thread 尚未赋值 fp_mplayer */
        while (fp_mplayer == NULL)
            usleep(100000);

        memset(line, 0, sizeof(line));
        if (fgets(line, sizeof(line), fp_mplayer) == NULL) {
            if (fp_mplayer != NULL)
                g_video_eof = true;
            usleep(200000);
            continue;
        }

        /* ----- 当前播放位置（秒） ----- */
        if (strstr(line, "ANS_TIME_POSITION")) {
            p = strrchr(line, '=');
            if (p) {
                p++;
                sscanf(p, "%f", &time_pos);
                /* printf("[Video] 时间 %.2f\n", time_pos); */

                char tmp[100] = {0};
                int tmp_time = (int)time_pos;
                sprintf(tmp, "%02d:%02d", tmp_time / 60, tmp_time % 60);

                if (strcmp(show_time_buf, tmp) != 0) {
                    strcpy(show_time_buf, tmp);
                    pthread_mutex_lock(&mutex_lv);
                    lv_label_set_text(time_pos_label, show_time_buf);
                    pthread_mutex_unlock(&mutex_lv);
                }
            }
        }

        /* ----- 播放百分比，驱动进度条 ----- */
        if (strstr(line, "ANS_PERCENT_POSITION")) {
            p = strrchr(line, '=');
            if (p) {
                int percent = 0;
                sscanf(++p, "%d", &percent);
                /* printf("[Video] 百分比 %d\n", percent); */

                if (percent != percent_pos) {
                    percent_pos = percent;
                    pthread_mutex_lock(&mutex_lv);
                    lv_slider_set_value(play_slider, percent_pos, LV_ANIM_OFF);
                    pthread_mutex_unlock(&mutex_lv);
                }

                /* 接近结尾 → 设标志，主线程处理切歌 */
                if (percent_pos >= 98) {
                    g_video_eof = true;
                }
            }
        }

        /* ----- 媒体总时长 ----- */
        if (strstr(line, "ANS_LENGTH")) {
            p = strrchr(line, '=');
            if (p) {
                p++;
                sscanf(p, "%f", &time_length);
                /* printf("[Video] 总时长 %.2f\n", time_length); */

                char time_buf[100] = {0};
                int length = (int)time_length;
                sprintf(time_buf, "%02d:%02d", length / 60, length % 60);

                pthread_mutex_lock(&mutex_lv);
                lv_label_set_text(time_length_label, time_buf);
                pthread_mutex_unlock(&mutex_lv);
            }
        }
    }

    return NULL;
}
