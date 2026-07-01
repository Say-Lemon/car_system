/**
 * @file vp_player_ui.c
 * @brief 播放器 LVGL 界面：主菜单、播放控制条、列表、FIFO 命令下发
 *
 * 界面层次：
 *   video_player_launch()       → 扫描目录 + 创建播放界面
 *   ui_create_player_screen()   → 800x480 播放页
 *   ui_create_close_button()    → 右上角关闭钮
 *
 * 与系统编程相关：
 *   fork+pipe 接管 mplayer stdin/stdout — slave 命令与应答
 *   pthread_kill + kill(pid, SIGSTOP/SIGCONT) — seek/暂停时冻结线程与进程
 *   pthread_cond_signal(&cond)    — 与 playback_io_sync 读线程配合
 *
 * 线程：本文件回调均在 LVGL 主线程执行；勿长时间阻塞。
 */

#include "vp_player_ui.h"
#include "vp_media_catalog.h"
#include "vp_playback_controller.h"
#include "vp_playback_monitor.h"
#include "vp_playback_io_sync.h"
#include "app_config.h"
#include "music_controller.h"
#include "font_zh_cn.h"

static void ui_transport_btn_cb(lv_event_t *e);

/* ---------- 跨模块共享的 LVGL 对象 ---------- */
lv_obj_t *t0 = NULL;
lv_obj_t *volume_slider = NULL;
lv_obj_t *bright_slider = NULL;
lv_obj_t *play_slider = NULL;
lv_obj_t *time_pos_label = NULL;
lv_obj_t *time_length_label = NULL;

/* ---------- 仅本模块使用的控件 ---------- */
static lv_obj_t *cont = NULL;
static lv_obj_t *video_list = NULL;
static lv_obj_t *volume_label = NULL;
static lv_obj_t *bright_label = NULL;

/** 1=列表可见，0=隐藏；由 "List" 按钮切换 */
static bool list_flag = true;

/** 播放/暂停 toggle 按钮控件指针 */
static lv_obj_t *btn_play_pause = NULL;

/** 当前是否已暂停（用于 toggle 图标和状态判断） */
static bool is_paused = false;


/** @brief 播放控制按钮统一事件处理（暂停/继续/seek/切歌/列表显隐） */
static void ui_transport_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    char *msg = lv_event_get_user_data(e);

    if (code != LV_EVENT_CLICKED)
        return;

    if (strcmp(msg, "play_pause") == 0) {
        if (!start)
            return;

        if (!is_paused) {
            pthread_kill(tid_read, 10);
            kill(mplayer_pid, SIGSTOP);
            lv_label_set_text(lv_obj_get_child(btn_play_pause, 0), LV_SYMBOL_PLAY);
            is_paused = true;
        } else {
            kill(mplayer_pid, SIGCONT);
            pthread_cond_signal(&cond);
            usleep(1000); pthread_cond_signal(&cond);
            lv_label_set_text(lv_obj_get_child(btn_play_pause, 0), LV_SYMBOL_PAUSE);
            is_paused = false;
        }
    }

    if (strcmp(msg, "forward") == 0) {
        if (!start)
            return;
        usleep(1000);
        write(fd_mplayer, "seek +10\n", strlen("seek +10\n"));
        write(fd_mplayer, "get_percent_pos\n", strlen("get_percent_pos\n"));
    }

    if (strcmp(msg, "back") == 0) {
        if (!start)
            return;
        usleep(10000);
        write(fd_mplayer, "seek -10\n", strlen("seek -10\n"));
        write(fd_mplayer, "get_percent_pos\n", strlen("get_percent_pos\n"));
    }

    if (strcmp(msg, "next_music") == 0) {
        usleep(1000);
        if (++video_index >= video_num)
            video_index = 0;
        playback_play_current();
    }

    if (strcmp(msg, "prev_music") == 0) {
        usleep(1000);
        if (--video_index < 0)
            video_index = video_num - 1;
        playback_play_current();
    }

    if (strcmp(msg, "music_show_list") == 0) {
        list_flag = !list_flag;
        if (list_flag)
            lv_obj_clear_flag(video_list, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(video_list, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 创建底部播放控制按钮（播放/暂停/seek/上下曲/列表显隐）
 * @note user_data 传入字符串常量，在 ui_transport_btn_cb 中 strcmp 区分功能
 */
static lv_obj_t *make_vbtn(lv_obj_t *p, const char *icon, int s, bool orange)
{
    lv_obj_t *b = lv_btn_create(p);
    lv_obj_set_size(b, s + 10, s + 10);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, icon);
    lv_obj_center(l);
    lv_obj_set_style_text_color(l, orange ? lv_color_hex(0xFF6D00) : lv_color_white(), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_24, 0);
    return b;
}

static void ui_create_transport_controls(void)
{
    lv_obj_t *btn_prev   = make_vbtn(cont, LV_SYMBOL_PREV, 50, false);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_MID, -200, 10);
    lv_obj_add_event_cb(btn_prev, ui_transport_btn_cb, LV_EVENT_CLICKED, "prev_music");

    lv_obj_t *btn_back   = make_vbtn(cont, LV_SYMBOL_LEFT LV_SYMBOL_LEFT, 42, false);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, -100, 5);
    lv_obj_add_event_cb(btn_back, ui_transport_btn_cb, LV_EVENT_CLICKED, "back");

    btn_play_pause = make_vbtn(cont, LV_SYMBOL_PAUSE, 58, true);
    lv_obj_align(btn_play_pause, LV_ALIGN_BOTTOM_MID, 0, 12);
    lv_obj_add_event_cb(btn_play_pause, ui_transport_btn_cb, LV_EVENT_CLICKED, "play_pause");

    lv_obj_t *btn_forward = make_vbtn(cont, LV_SYMBOL_RIGHT LV_SYMBOL_RIGHT, 42, false);
    lv_obj_align(btn_forward, LV_ALIGN_BOTTOM_MID, 100, 5);
    lv_obj_add_event_cb(btn_forward, ui_transport_btn_cb, LV_EVENT_CLICKED, "forward");

    lv_obj_t *btn_next   = make_vbtn(cont, LV_SYMBOL_NEXT, 50, false);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_MID, 200, 10);
    lv_obj_add_event_cb(btn_next, ui_transport_btn_cb, LV_EVENT_CLICKED, "next_music");

    lv_obj_t *btn_list = lv_btn_create(cont);
    lv_obj_set_size(btn_list, 56, 34);
    lv_obj_set_style_bg_color(btn_list, lv_color_hex(0xFF6D00), 0);
    lv_obj_set_style_radius(btn_list, 6, 0);
    lv_obj_align(btn_list, LV_ALIGN_BOTTOM_RIGHT, 12, -45);
    lv_obj_t *label = lv_label_create(btn_list);
    lv_label_set_text(label, "List");
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_add_event_cb(btn_list, ui_transport_btn_cb, LV_EVENT_CLICKED, "music_show_list");
}

/**
 * @brief 滑条事件：音量/亮度实时写 FIFO；进度条在松开时 seek
 *
 * seek 使用绝对百分比 "seek <val> 1"，无需暂停读写线程（无竞态风险）。
 */
static void ui_playback_slider_cb(lv_event_t *e)
{
    char *msg = lv_event_get_user_data(e);

    /* 音量不受 start 限制：打开播放器即可同步 */
    if (strcmp(msg, "volume") == 0) {
        lv_obj_t *slider = lv_event_get_target(e);
        int volume = (int)lv_slider_get_value(slider);
        lv_label_set_text_fmt(volume_label, "%d", volume);
        lv_obj_align_to(volume_label, slider, LV_ALIGN_OUT_TOP_RIGHT, 8, -5);
        g_sys_volume = volume;
        if (start) {
            int mp_vol = volume / 5;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "volume %d 1\n", mp_vol);
            write(fd_mplayer, cmd, strlen(cmd));
        }
        return;
    }

    if (!start)
        return;

    if (strcmp(msg, "brightness") == 0) {
        lv_obj_t *slider1 = lv_event_get_target(e);
        char buf[8];
        int brightness = (int)lv_slider_get_value(slider1);
        lv_snprintf(buf, sizeof(buf), "%d", brightness);
        lv_label_set_text(bright_label, buf);
        lv_obj_align_to(bright_label, slider1, LV_ALIGN_OUT_TOP_RIGHT, 8, -5);
        usleep(100);

        char cmd[1024] = {0};
        sprintf(cmd, "brightness %d 1\n", brightness);
        write(fd_mplayer, cmd, strlen(cmd));
    }

    if (strcmp(msg, "play") == 0) {
        /* 使用绝对百分比 seek（seek <val> 1），无需暂停读写线程 */
        int rate = (int)lv_slider_get_value(play_slider);

        char cmd[1024] = {0};
        sprintf(cmd, "seek %d 1\n", rate);
        write(fd_mplayer, cmd, strlen(cmd));
    }
}

static void style_slider(lv_obj_t *s) {
    lv_obj_set_height(s, 4);
    lv_obj_set_style_bg_color(s, lv_color_hex(0x3A3A4E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, lv_color_hex(0xFF6D00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, lv_color_hex(0xFF6D00), LV_PART_KNOB);
    lv_obj_set_style_width(s, 8, LV_PART_KNOB);
    lv_obj_set_style_height(s, 8, LV_PART_KNOB);
}

static void ui_create_playback_sliders(void)
{
    volume_slider = lv_slider_create(cont);
    lv_obj_align(volume_slider, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_width(volume_slider, 100);
    lv_obj_add_event_cb(volume_slider, ui_playback_slider_cb, LV_EVENT_VALUE_CHANGED, "volume");
    lv_slider_set_value(volume_slider, g_sys_volume, LV_ANIM_OFF);
    style_slider(volume_slider);

    volume_label = lv_label_create(cont);
    lv_label_set_text(volume_label, "100");
    lv_obj_align_to(volume_label, volume_slider, LV_ALIGN_OUT_TOP_RIGHT, 8, -10);

    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, "音量");
    lv_obj_align_to(label, volume_slider, LV_ALIGN_OUT_TOP_LEFT, -5, -10);
    APPLY_ZH_14(label);

    bright_slider = lv_slider_create(cont);
    lv_obj_align(bright_slider, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(bright_slider, ui_playback_slider_cb, LV_EVENT_VALUE_CHANGED, "brightness");
    lv_obj_set_width(bright_slider, 100);
    lv_slider_set_value(bright_slider, g_sys_brightness, LV_ANIM_OFF);
    style_slider(bright_slider);

    bright_label = lv_label_create(cont);
    lv_label_set_text(bright_label, "20");
    lv_obj_align_to(bright_label, bright_slider, LV_ALIGN_OUT_TOP_RIGHT, 8, -10);

    lv_obj_t *label_bright = lv_label_create(cont);
    lv_label_set_text(label_bright, "亮度");
    lv_obj_align_to(label_bright, bright_slider, LV_ALIGN_OUT_TOP_LEFT, -5, -10);
    APPLY_ZH_14(label_bright);

    play_slider = lv_slider_create(cont);
    lv_obj_align(play_slider, LV_ALIGN_CENTER, 0, 150);
    lv_obj_set_width(play_slider, 600);
    lv_obj_add_event_cb(play_slider, ui_playback_slider_cb, LV_EVENT_RELEASED, "play");
    lv_slider_set_value(play_slider, 0, LV_ANIM_OFF);
    lv_slider_set_range(play_slider, 0, 100);
    style_slider(play_slider);

    time_length_label = lv_label_create(cont);
    lv_label_set_text(time_length_label, "0:00");
    lv_obj_align_to(time_length_label, play_slider, LV_ALIGN_OUT_RIGHT_BOTTOM, -10, 20);

    time_pos_label = lv_label_create(cont);
    lv_label_set_text(time_pos_label, "0:00");
    lv_obj_align_to(time_pos_label, play_slider, LV_ALIGN_OUT_LEFT_BOTTOM, 0, 20);
}

/** @brief 列表项点击：设置 video_index 并调用 playback_play_current */
static void ui_playlist_item_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        printf("Clicked %s\n", lv_list_get_btn_text(video_list, obj));
        video_index = (int)(intptr_t)lv_event_get_user_data(e);
        playback_play_current();
    }
}

/**
 * @brief 根据 media_catalog_scan 结果创建右侧播放列表
 * @note strtok(tmp, ".") 会修改 tmp，仅用于去掉扩展名显示
 */
static void ui_create_playlist(void)
{
    video_list = lv_list_create(cont);
    lv_obj_set_size(video_list, 100, 260);
    lv_obj_align(video_list, LV_ALIGN_RIGHT_MID, 15, 0);
    lv_list_add_text(video_list, "list");

    for (int i = 0; i < video_num; i++) {
        char tmp[100] = {0};
        char *p = video_path[i];
        p = strrchr(p, '/');
        strcpy(tmp, ++p);

        lv_obj_t *btn = lv_list_add_btn(video_list, NULL, strtok(tmp, "."));
        lv_obj_add_event_cb(btn, ui_playlist_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

/**
 * @brief 关闭播放页：终止 mplayer 并删除播放容器
 * @note user_data 为关闭按钮自身，其父对象即 cont
 */
void ui_player_close_cb(lv_event_t *e)
{
    playback_stop_all();
    /* 停止 reader/writer 线程：置 NULL 后 reader 在 while(fp==NULL) 中自旋，不再 fgets */
    extern FILE *fp_mplayer;
    extern int fd_mplayer;
    extern bool start;
    start = 0;
    fp_mplayer = NULL;
    fd_mplayer = -1;
    usleep(100000);
    lv_obj_del(lv_obj_get_parent((lv_obj_t *)(e->user_data)));
    g_video_overlay_active = false;

    /* ---- 视频关闭后自动续播被中断的音乐 ---- */
    extern bool g_music_interrupted_by_video;
    if (g_music_interrupted_by_video) {
        g_music_interrupted_by_video = false;
        printf("[Video] 视频关闭，恢复音乐播放\n");
        music_play_current();
    }
}

/** @brief 创建播放页右上角关闭按钮 */
static void ui_create_close_button(void)
{
    lv_obj_t *btn_close = lv_btn_create(cont);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, 0, 30);
    lv_obj_set_size(btn_close, 48, 48);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0xFF6D00), 0);
    lv_obj_set_style_radius(btn_close, 6, 0);
    lv_obj_set_style_shadow_width(btn_close, 0, 0);
    lv_obj_t *label = lv_label_create(btn_close);
    lv_label_set_text(label, LV_SYMBOL_CLOSE);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_add_event_cb(btn_close, ui_player_close_cb, LV_EVENT_RELEASED, btn_close);
}

/* reader 线程设置 g_video_eof，主线程定时器安全切歌 */
static void video_auto_next_cb(lv_timer_t *t)
{
    (void)t;
    extern volatile bool g_video_eof;
    if (!g_video_eof) return;
    g_video_eof = false;
    extern int video_index, video_num;
    if (++video_index >= video_num) video_index = 0;
    playback_play_current();
}

/** @brief 组装播放页：关闭钮 + 控制钮 + 列表 + 滑条 */
static void ui_create_player_screen(void)
{
    cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 800, 480);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_close_button();
    ui_create_transport_controls();
    ui_create_playlist();
    ui_create_playback_sliders();
    lv_timer_create(video_auto_next_cb, 1000, NULL);
}

/**
 * @brief 进入播放器：绘制播放界面
 *
 * 注意：此时尚未自动播放，需用户在列表中点击某一集
 */
void ui_player_open_cb(lv_event_t *e)
{
    (void)e;
    ui_create_player_screen();
}

/**
 * @brief 应用启动后的主菜单
 *
 * 流程：创建 flex 容器 → media_catalog_scan() → "video" 按钮注册 ui_player_open_cb
 */
void ui_show_main_menu(void)
{
    t0 = lv_obj_create(lv_scr_act());
    lv_obj_set_size(t0, 800, 460);
    lv_obj_align(t0, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_pad_all(t0, 60, LV_PART_MAIN);
    lv_obj_set_style_pad_row(t0, 60, LV_PART_MAIN);
    lv_obj_set_style_pad_column(t0, 60, LV_PART_MAIN);
    lv_obj_clear_flag(t0, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_flex_flow(t0, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(t0, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_SPACE_EVENLY, 0);

    media_catalog_scan();

    lv_obj_t *btn01 = lv_btn_create(t0);
    lv_obj_set_size(btn01, 150, 150);
    lv_obj_t *label01 = lv_label_create(btn01);
    lv_label_set_text(label01, "video");
    lv_obj_center(label01);
    lv_obj_add_event_cb(btn01, ui_player_open_cb, LV_EVENT_RELEASED, (void *)t0);

    printf("扫描到视频数量: %d\n", video_num);
}

void video_player_launch(void)
{
    g_video_overlay_active = true;
    media_catalog_scan();
    ui_player_open_cb(NULL);
    if (volume_slider) {
        lv_slider_set_value(volume_slider, g_sys_volume, LV_ANIM_OFF);
        lv_event_send(volume_slider, LV_EVENT_VALUE_CHANGED, NULL);
    }
}
