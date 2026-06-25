/**
 * @file vp_playback_controller.c
 * @brief 播放引擎：MPlayer 子进程管理与播放控制逻辑
 *
 * 数据通路：
 *   控制：write(fd_mplayer) → stdin pipe → mplayer (slave 模式读 stdin)
 *   状态：stdout pipe ← mplayer stdout（ANS_* 行，由 playback_monitor 解析）
 *
 * 线程：
 *   playback_launch_thread — 每次切歌时 pthread_create，fork+pipe 启动 mplayer
 *   读/写线程 — 仅在首次 playback_play_current 时各创建一次，之后复用
 */

#include "vp_playback_controller.h"
#include "vp_media_catalog.h"
#include "vp_playback_io_sync.h"
#include "vp_playback_monitor.h"
#include "vp_config.h"
#include "app_config.h"
#include "music_controller.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

FILE *fp_mplayer = NULL;
int fd_mplayer = 0;
int video_index = -1;
bool play_flag = 0;
bool start = 0;
pid_t mplayer_pid = -1;

/**
 * @brief 强制结束所有名为 mplayer 的进程
 *
 * 保存子进程 PID，kill(pid, SIGKILL) 精确控制（避免误杀音乐 mplayer）。
 */
void playback_stop_all(void)
{
    if (mplayer_pid > 0) { kill(mplayer_pid, SIGKILL); mplayer_pid = -1; }
    /* 不 close/fclose：线程可能正在使用 fd/FILE*，只设 -1/NULL */
    fd_mplayer = -1;
    fp_mplayer = NULL;
    play_flag = 0;  /* 清除播放标志，允许音乐等模块检测视频已停止 */
}

/**
 * @brief 播放线程：fork+pipe 启动 mplayer，并查询总时长与当前位置
 *
 * -slave：文本命令/应答模式
 * -input file=FIFO_PATH：从命名管道读命令
 * -x/-y/-zoom：视频窗口大小（叠在 LVGL 界面中央）
 * 全局 fp_mplayer 供 playback_status_reader_thread 阻塞读取。
 */
void *playback_launch_thread(void *arg)
{
    (void)arg;
    printf("---- %ld 播放线程 ------------------\n", (long)pthread_self());

    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) || pipe(stdout_pipe)) { perror("pipe"); return NULL; }

    pid_t pid = fork();
    if (pid == 0) {
        close(stdin_pipe[1]);  close(stdout_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);  close(stdout_pipe[1]);
        execlp("mplayer", "mplayer", "-quiet", "-slave", "-zoom",
               "-x", "680", "-y", "362", video_path[video_index], NULL);
        _exit(1);
    }

    close(stdin_pipe[0]);  close(stdout_pipe[1]);
    mplayer_pid = pid;
    fd_mplayer = stdin_pipe[1];
    fp_mplayer = fdopen(stdout_pipe[0], "r");

    write(fd_mplayer, "get_time_length\n", strlen("get_time_length\n"));
    write(fd_mplayer, "get_time_pos\n", strlen("get_time_pos\n"));

    usleep(300000);
    { int v = g_sys_volume / 5; char c[32]; snprintf(c, sizeof(c), "volume %d 1\n", v);
      write(fd_mplayer, c, strlen(c)); }
    return NULL;
}

/**
 * @brief 播放当前 video_index 指向的视频（切歌入口）
 *
 * 1. 若已在播则 playback_stop_all 并短暂等待
 * 2. 新建 detached 线程执行 fork+pipe
 * 3. 首次调用时 sleep(1) 后创建读/写线程（等待 mplayer 与 FIFO 就绪）
 * 4. pthread_cond_signal 唤醒可能阻塞在 cond 上的读线程
 * 5. 按 UI 滑条同步音量、亮度
 */
void playback_play_current(void)
{
    /* ---- 视频优先：若音乐正在播放则暂停之，关闭视频后自动续播 ---- */
    extern bool g_music_interrupted_by_video;
    if (g_music_playing) {
        music_stop_all();
        g_music_interrupted_by_video = true;
        printf("[Video] 视频启动，暂停背景音乐\n");
    }

    if (play_flag != 0) {
        playback_stop_all();
        usleep(200000);
    }

    play_flag = 1;
    printf("视频索引 %d\n", video_index);

    pthread_t tid;
    if (pthread_create(&tid, NULL, playback_launch_thread, NULL) != 0) {
        perror("创建播放线程失败");
        return;
    }
    pthread_detach(tid);

    if (!start) {
        sleep(1);  /* 等待 mplayer 进程启动并打开 FIFO 读端 */

        pthread_create(&tid_read, NULL, playback_status_reader_thread, NULL);
        pthread_detach(tid_read);

        pthread_create(&tid_write, NULL, playback_io_query_writer_thread, NULL);
        pthread_detach(tid_write);

        printf("tid_read %ld  tid_write %ld\n", (long)tid_read, (long)tid_write);
        start = 1;
    }

    pthread_cond_signal(&cond);

    if (volume_slider) {
        pthread_mutex_lock(&mutex_lv);
        lv_slider_set_value(volume_slider, g_sys_volume, LV_ANIM_OFF);
        lv_event_send(volume_slider, LV_EVENT_VALUE_CHANGED, NULL);
        pthread_mutex_unlock(&mutex_lv);
    }

    if (bright_slider) {
        int bright = (int)lv_slider_get_value(bright_slider);
        char cmd[64];
        sprintf(cmd, "brightness %d 1\n", bright);
        write(fd_mplayer, cmd, strlen(cmd));
    }
}
