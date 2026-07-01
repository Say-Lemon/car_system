/**
 * @file music_controller.c
 * @brief mplayer 音乐播放核心 — fork+pipe + 读写线程 + 标志位同步
 *
 * 线程同步说明：
 *   - Seek（拖进度条）不再暂停读写线程，直接发 "seek <val> 1"（绝对百分比）
 *   - Pause 用 g_music_io_paused 标志位 + SIGUSR1 中断 fgets，主线程控制全流程
 *   - Writer 直接检查 g_music_paused 标志，无需信号处理
 *   - 竞态条件修复：旧版使用异步 pthread_kill 链式触发信号处理→cond_wait→
 *     cond_signal，存在信号丢失窗口（Writer 未进入 cond_wait 时信号已发出）
 */
#include "music_controller.h"
#include "music_catalog.h"
#include "app_config.h"
#include <stdarg.h>
#include <sys/wait.h>
#include <errno.h>

/* ---- 全局变量 ---- */
FILE *g_fp_music_mplayer  = NULL;
int   g_fd_music_pipe     = -1;
int   g_music_index       = 0;
int   g_music_percent_pos = 0;
float g_music_time_pos    = 0.0f;
float g_music_time_length = 0.0f;
bool  g_music_playing     = false;
bool  g_music_paused      = false;

/* ---- 同步对象 ---- */
pthread_mutex_t g_music_lv_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_music_rd_mutex = PTHREAD_MUTEX_INITIALIZER;
/* g_music_wr_mutex / g_music_wr_cond 已移除：Writer 直接检查 g_music_paused，无需信号同步 */
pthread_cond_t  g_music_rd_cond  = PTHREAD_COND_INITIALIZER;
pthread_t       g_tid_music_read;
pthread_t       g_tid_music_write;

static bool threads_created = false;
volatile bool g_music_eof = false;
pid_t music_mplayer_pid = -1;

/* ---- 暂停标志位：Reader 在 main loop 中检查，Writer 直接读 g_music_paused ---- */
static volatile sig_atomic_t g_music_io_paused  = 0;
static volatile sig_atomic_t g_reader_sig_flag  = 0;

/* Reader 信号处理：只设标志位（async-signal-safe），主循环负责后续逻辑 */
static void music_reader_sig_handler(int sig)
{
    (void)sig;
    g_reader_sig_flag = 1;
}

/* ---- 音量曲线：UI值→mplayer值 (低音量段更细腻) ---- */
void music_set_volume(int ui_vol)
{
    if (ui_vol < 0) ui_vol = 0;
    if (ui_vol > 100) ui_vol = 100;
    /* 线性缩放至 20%: 5→1, 10→2, 20→4, 50→10, 100→20 */
    int mp_vol = ui_vol / 5;
    music_send_cmd("volume %d 1\n", mp_vol);
}

/* ---- 发送管道命令 ---- */
void music_send_cmd(const char *fmt, ...)
{
    if (g_fd_music_pipe < 0) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write(g_fd_music_pipe, buf, strlen(buf));
}

/* ---- 停止 mplayer ---- */
void music_stop_all(void)
{
    if (music_mplayer_pid > 0) { kill(music_mplayer_pid, SIGKILL); }
    music_mplayer_pid = -1;
    g_music_playing = false;
    g_music_paused  = false;
    g_fd_music_pipe = -1;
    g_fp_music_mplayer = NULL;
}

/* ---- 播放当前歌曲 ---- */
void music_play_current(void)
{
    if (g_music_count == 0) {
        printf("[MusicCtrl] 无歌曲\n");
        return;
    }

    /* ---- 互斥：若视频正在播放则不启动音乐（视频优先占用音频设备） ---- */
    extern bool play_flag;
    if (play_flag) {
        printf("[MusicCtrl] 视频播放中，拒绝启动音乐\n");
        return;
    }

    if (g_music_playing) music_stop_all();
    usleep(200000);

    g_music_playing     = true;
    g_music_paused      = false;
    g_music_io_paused   = 0;   /* 清除暂停标志 */
    g_music_percent_pos = 0;
    g_music_time_pos    = 0;
    g_music_time_length = 0;

    pthread_t tid_launch;
    pthread_create(&tid_launch, NULL, music_launch_thread, NULL);
    pthread_detach(tid_launch);

    if (!threads_created) {
        sleep(1); /* 等待 mplayer 启动 */
        pthread_create(&g_tid_music_read, NULL, music_reader_thread, NULL);
        pthread_detach(g_tid_music_read);
        pthread_create(&g_tid_music_write, NULL, music_writer_thread, NULL);
        pthread_detach(g_tid_music_write);
        threads_created = true;
    }

    /* 恢复读写线程 */
    pthread_mutex_lock(&g_music_rd_mutex);
    pthread_cond_signal(&g_music_rd_cond);
    pthread_mutex_unlock(&g_music_rd_mutex);

    /* 初始音量 */
    music_set_volume(g_sys_volume);
}

void music_play_next(void)
{
    if (g_music_count == 0) return;
    if (++g_music_index >= g_music_count) g_music_index = 0;
    music_play_current();
}

void music_play_prev(void)
{
    if (g_music_count == 0) return;
    if (--g_music_index < 0) g_music_index = g_music_count - 1;
    music_play_current();
}

void music_toggle_pause(void)
{
    if (!g_music_playing) { music_play_current(); return; }
    if (g_music_paused) {
        /* ---- 恢复播放 ---- */
        g_music_io_paused = 0;
        g_music_paused    = false;
        kill(music_mplayer_pid, SIGCONT);
        /* 唤醒 Reader（若在 cond_wait 上等待） */
        pthread_mutex_lock(&g_music_rd_mutex);
        pthread_cond_signal(&g_music_rd_cond);
        pthread_mutex_unlock(&g_music_rd_mutex);
    } else {
        /* ---- 暂停 ---- */
        g_music_io_paused = 1;
        g_music_paused    = true;
        /* 发信号中断 Reader 的 fgets（需 sigaction 无 SA_RESTART），
           Reader 返回后在主循环顶检查 g_music_io_paused 并进入 cond_wait */
        pthread_kill(g_tid_music_read, SIGUSR1);
        /* 短暂等待 Reader 进入 cond_wait，再停止 mplayer */
        usleep(50000);
        kill(music_mplayer_pid, SIGSTOP);
    }
}

/* ---- 启动线程：fork+pipe ---- */
void *music_launch_thread(void *arg)
{
    (void)arg;
    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) || pipe(out_pipe)) { perror("pipe"); return NULL; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return NULL; }
    if (pid == 0) {
        close(in_pipe[1]);  close(out_pipe[0]);
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]);  close(out_pipe[1]);
        execlp("mplayer", "mplayer", "-quiet", "-slave", "-novideo",
               g_music_path[g_music_index], NULL);
        _exit(1);
    }

    close(in_pipe[0]);  close(out_pipe[1]);
    music_mplayer_pid = pid;
    g_fd_music_pipe = in_pipe[1];
    g_fp_music_mplayer = fdopen(out_pipe[0], "r");

    music_send_cmd("get_time_length\n");
    music_send_cmd("get_time_pos\n");
    music_send_cmd("get_percent_pos\n");

    usleep(300000);
    music_set_volume(g_sys_volume);
    return NULL;
}

/* ---- 读取线程：解析 ANS_* 响应 ---- */
void *music_reader_thread(void *arg)
{
    (void)arg;

    /* 用 sigaction（无 SA_RESTART）注册 SIGUSR1：
       fgets 被信号中断后返回 NULL + errno=EINTR，主循环检查标志进入暂停 */
    struct sigaction sa;
    sa.sa_handler = music_reader_sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    char line[256];
    /* 等待 writers 就绪 */
    if (!threads_created) usleep(5000);

    while (1) {
        while (g_fp_music_mplayer == NULL) usleep(200000);

        /* ---- 检查暂停标志（主循环安全点） ---- */
        if (g_music_io_paused) {
            pthread_mutex_lock(&g_music_rd_mutex);
            pthread_cond_wait(&g_music_rd_cond, &g_music_rd_mutex);
            pthread_mutex_unlock(&g_music_rd_mutex);
            g_reader_sig_flag = 0;
            continue;
        }

        if (fgets(line, sizeof(line), g_fp_music_mplayer) == NULL) {
            /* 信号中断 → 检查暂停标志 */
            if (g_reader_sig_flag) {
                g_reader_sig_flag = 0;
                continue;
            }
            /* 真实 EOF：mplayer 退出 */
            if (g_fp_music_mplayer != NULL && !g_music_eof) {
                g_music_eof = true;
            }
            usleep(500000);  /* EOF 后降低轮询频率 */
            continue;
        }

        if (strstr(line, "ANS_TIME_POSITION")) {
            char *p = strrchr(line, '=');
            if (p) {
                sscanf(p + 1, "%f", &g_music_time_pos);
            }
        }
        else if (strstr(line, "ANS_PERCENT_POSITION")) {
            char *p = strrchr(line, '=');
            if (p) {
                int pct = 0;
                sscanf(p + 1, "%d", &pct);
                if (pct != g_music_percent_pos) {
                    g_music_percent_pos = pct;
                }
                /* 接近结尾 → 设标志，主线程定时器处理切歌 */
                if (g_music_percent_pos >= 98) {
                    g_music_eof = true;
                }
            }
        }
        else if (strstr(line, "ANS_LENGTH")) {
            char *p = strrchr(line, '=');
            if (p) {
                sscanf(p + 1, "%f", &g_music_time_length);
            }
        }
    }
    return NULL;
}

/* ---- 写入线程：周期性查询 ---- */
void *music_writer_thread(void *arg)
{
    (void)arg;
    /* 无需信号处理 — 直接检查 g_music_paused 标志（与主线程共享 volatile） */
    while (1) {
        if (g_music_playing && !g_music_paused) {
            music_send_cmd("get_time_pos\n");
            usleep(400000);
            music_send_cmd("get_percent_pos\n");
            usleep(400000);
        } else {
            usleep(500000);
        }
    }
    return NULL;
}
