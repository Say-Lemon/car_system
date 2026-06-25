/**
 * @file car_ui_music_bar.c
 * @brief 右下音乐播放控制卡 — 歌名/歌手/进度条/播放控制
 *
 * 布局（250 × 310）：
 *   ┌─────────────────────────┐
 *   │                         │
 *   │     白色粗体歌名         │  中部歌名（18px 粗体）
 *   │     浅灰小字歌手         │  歌手名（14px）
 *   │  ═══════════════════    │  进度条（浅灰底，橙色填充）
 *   │     00:00 / 00:00       │  时间
 *   │     ⏮    ▶    ⏭       │  控制按钮（透明背板，大号图标）
 *   │                         │
 *   └─────────────────────────┘
 */

#include "car_ui_music_bar.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "music_controller.h"
#include "music_catalog.h"
#include "lvgl/lvgl.h"

lv_obj_t *g_music_bar_label_song;
lv_obj_t *g_music_bar_label_artist;
lv_obj_t *g_music_bar_label_time;
lv_obj_t *g_music_bar_progress;
lv_obj_t *g_music_bar_btn_play;
lv_obj_t *g_music_bar_btn_prev;
lv_obj_t *g_music_bar_btn_next;
lv_obj_t *g_music_bar_play_label;

static void music_bar_play_cb(lv_event_t *e);
static void music_bar_prev_cb(lv_event_t *e);
static void music_bar_next_cb(lv_event_t *e);

/* 前向声明 */
static void music_bar_refresh_cb(lv_timer_t *t);
static void music_bar_seek_cb(lv_event_t *e);

/* 创建透明背板图标按钮 */
static lv_obj_t *create_icon_btn(lv_obj_t *parent, const char *symbol,
                                  lv_color_t color, const lv_font_t *font,
                                  int icon_size)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, icon_size + 16, icon_size + 16);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);

    return btn;
}

void car_ui_music_bar_create(lv_obj_t *parent)
{
    /* ---- 外层容器 ---- */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, RIGHT_W, MUSIC_BAR_H);
    lv_obj_set_pos(cont,
                   MARGIN + SIDEBAR_W + GAP + CENTER_W + GAP,
                   MARGIN + STATUS_H + GAP);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 10, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);

    /* ---- "♫ 正在播放" 标题 ---- */
    lv_obj_t *header = lv_label_create(cont);
    lv_label_set_text(header, "♫ 正在播放");
    lv_obj_set_style_text_color(header, lv_color_hex(0xFF6D00), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);
    APPLY_ZH_18(header);

    /* ---- 歌名：白色粗体，居中（18px 中文） ---- */
    g_music_bar_label_song = lv_label_create(cont);
    lv_label_set_text(g_music_bar_label_song, "暂无歌曲");
    lv_obj_set_style_text_color(g_music_bar_label_song, lv_color_white(), 0);
    lv_obj_set_style_text_align(g_music_bar_label_song, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(g_music_bar_label_song, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(g_music_bar_label_song, RIGHT_W - 24);
    lv_obj_align(g_music_bar_label_song, LV_ALIGN_CENTER, 0, -40);
    APPLY_ZH_18(g_music_bar_label_song);

    /* ---- 歌手：浅灰小字（14px 中文） ---- */
    g_music_bar_label_artist = lv_label_create(cont);
    lv_label_set_text(g_music_bar_label_artist, "未知歌手");
    lv_obj_set_style_text_color(g_music_bar_label_artist, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(g_music_bar_label_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(g_music_bar_label_artist, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(g_music_bar_label_artist, RIGHT_W - 24);
    lv_obj_align(g_music_bar_label_artist, LV_ALIGN_CENTER, 0, -14);
    APPLY_ZH_14(g_music_bar_label_artist);

    /* ---- 进度条：细条，浅灰底，橙色填充 + 橙色小圆球 ---- */
    g_music_bar_progress = lv_slider_create(cont);
    lv_obj_set_width(g_music_bar_progress, RIGHT_W - 24);
    lv_obj_set_height(g_music_bar_progress, 4);                  /* 细条 */
    lv_slider_set_range(g_music_bar_progress, 0, 100);
    lv_slider_set_value(g_music_bar_progress, 0, LV_ANIM_OFF);
    lv_obj_align(g_music_bar_progress, LV_ALIGN_BOTTOM_MID, 0, -90);
    /* 底色浅灰 */
    lv_obj_set_style_bg_color(g_music_bar_progress, lv_color_hex(0x3A3A4E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_music_bar_progress, LV_OPA_COVER, LV_PART_MAIN);
    /* 已填充段橙色 */
    lv_obj_set_style_bg_color(g_music_bar_progress, lv_color_hex(0xFF6D00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_music_bar_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    /* 小圆球：橙色，缩小 */
    lv_obj_set_style_bg_color(g_music_bar_progress, lv_color_hex(0xFF6D00), LV_PART_KNOB);
    lv_obj_set_style_width(g_music_bar_progress, 8, LV_PART_KNOB);
    lv_obj_set_style_height(g_music_bar_progress, 8, LV_PART_KNOB);
    lv_obj_add_event_cb(g_music_bar_progress, music_bar_seek_cb, LV_EVENT_RELEASED, NULL);

    /* ---- 时间标签 ---- */
    g_music_bar_label_time = lv_label_create(cont);
    lv_label_set_text(g_music_bar_label_time, "00:00 / 00:00");
    lv_obj_set_style_text_color(g_music_bar_label_time, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(g_music_bar_label_time, &lv_font_montserrat_12, 0);
    lv_obj_align(g_music_bar_label_time, LV_ALIGN_BOTTOM_MID, 0, -72);

    /* ---- 播放控制按钮行（透明背板，等距排列） ---- */
    lv_obj_t *ctrl_row = lv_obj_create(cont);
    lv_obj_set_size(ctrl_row, RIGHT_W - 20, 60);
    lv_obj_align(ctrl_row, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(ctrl_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctrl_row, 0, 0);
    lv_obj_set_style_pad_all(ctrl_row, 0, 0);
    lv_obj_set_flex_flow(ctrl_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_row,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    /* 上一曲 */
    g_music_bar_btn_prev = create_icon_btn(ctrl_row, LV_SYMBOL_PREV,
                                           lv_color_white(),
                                           &lv_font_montserrat_24, 40);
    lv_obj_add_event_cb(g_music_bar_btn_prev, music_bar_prev_cb,
                        LV_EVENT_CLICKED, NULL);

    /* 播放（橙色实心三角） */
    g_music_bar_btn_play = create_icon_btn(ctrl_row, LV_SYMBOL_PLAY,
                                           lv_color_hex(0xFF6D00),
                                           &lv_font_montserrat_24, 40);
    g_music_bar_play_label = lv_obj_get_child(g_music_bar_btn_play, 0);
    lv_obj_add_event_cb(g_music_bar_btn_play, music_bar_play_cb,
                        LV_EVENT_CLICKED, NULL);

    /* 下一曲 */
    g_music_bar_btn_next = create_icon_btn(ctrl_row, LV_SYMBOL_NEXT,
                                           lv_color_white(),
                                           &lv_font_montserrat_24, 40);
    lv_obj_add_event_cb(g_music_bar_btn_next, music_bar_next_cb,
                        LV_EVENT_CLICKED, NULL);

    /* ---- 迷你栏刷新定时器（500ms） ---- */
    lv_timer_create(music_bar_refresh_cb, 500, NULL);
}

/* ---- 回调 ---- */
static void music_bar_play_cb(lv_event_t *e)
{
    (void)e;
    music_toggle_pause();
    lv_label_set_text(g_music_bar_play_label,
        g_music_paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}

static void music_bar_prev_cb(lv_event_t *e)
{
    (void)e;
    music_play_prev();
}

static void music_bar_next_cb(lv_event_t *e)
{
    (void)e;
    music_play_next();
}

static void music_bar_seek_cb(lv_event_t *e)
{
    (void)e;
    if (!g_music_playing || g_music_time_length <= 0) return;
    int target_pct = (int)lv_slider_get_value(g_music_bar_progress);
    /* seek <val> 1 = 绝对百分比，无需暂停读写线程（无竞态风险） */
    music_send_cmd("seek %d 1\n", target_pct);
}

/* ---- 迷你栏刷新定时器定义 ---- */
static void music_bar_refresh_cb(lv_timer_t *t)
{
    (void)t;
    /* 视频播放时跳过 LVGL 刷新，避免 fbdev_flush 冲突闪烁 */
    if (g_video_overlay_active) return;

    /* reader 线程检测到 EOF → 主线程安全切歌 */
    if (g_music_eof) {
        g_music_eof = false;
        music_play_next();
        return;
    }
    if (!g_music_playing) return;
    const char *name = strrchr(g_music_path[g_music_index], '/');
    name = name ? name + 1 : g_music_path[g_music_index];
    car_ui_music_bar_update_song(name);
    car_ui_music_bar_update_progress(g_music_percent_pos,
                                     g_music_time_pos, g_music_time_length);
    lv_label_set_text(g_music_bar_play_label,
        g_music_paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}

/* ---- 外部更新接口 ---- */
void car_ui_music_bar_update_song(const char *name)
{
    if (!g_music_bar_label_song) return;
    lv_label_set_text(g_music_bar_label_song, name ? name : "未知歌曲");
}

void car_ui_music_bar_update_progress(int percent, float pos_sec, float len_sec)
{
    if (g_music_bar_progress) {
        lv_slider_set_value(g_music_bar_progress, percent, LV_ANIM_OFF);
    }
    if (g_music_bar_label_time) {
        int pm = (int)pos_sec / 60, ps = (int)pos_sec % 60;
        int lm = (int)len_sec / 60, ls = (int)len_sec % 60;
        lv_label_set_text_fmt(g_music_bar_label_time,
                              "%02d:%02d / %02d:%02d", pm, ps, lm, ls);
    }
}
