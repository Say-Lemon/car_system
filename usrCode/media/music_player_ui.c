/**
 * @file music_player_ui.c
 * @brief 全屏音乐播放界面
 *
 * 布局（800×480）：左侧 2/3 播放控制 + 右侧 1/3 播放列表
 *   ┌────────────────────────────┬──────────┐
 *   │  ♪ 音乐播放器          ✕    │ 播放列表 │
 *   │                            │ song1    │
 *   │    白色粗体歌名（滚动）      │ song2    │
 *   │    浅灰歌手（滚动）          │ ...      │
 *   │  ════════════════════      │          │
 *   │   01:23 / 04:56            │          │
 *   │   ⏮     ▶     ⏭          │          │
 *   │  ──●────────── 音量        │          │
 *   └────────────────────────────┴──────────┘
 */
#include "music_player_ui.h"
#include "music_controller.h"
#include "music_catalog.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "car_ui_dashboard.h"
#include "lvgl/lvgl.h"

static lv_obj_t *panel;
static lv_obj_t *prog_slider;
static lv_obj_t *vol_slider;
static lv_obj_t *time_label;
static lv_obj_t *song_label;
static lv_obj_t *artist_label;
static lv_obj_t *play_btn_label;
static lv_timer_t *refresh_timer;

#define CTRL_W 520
#define LIST_W 250

static void refresh_ui_cb(lv_timer_t *t)
{
    (void)t;
    if (!g_music_playing) return;
    lv_slider_set_value(prog_slider, g_music_percent_pos, LV_ANIM_OFF);
    int pm = (int)g_music_time_pos / 60, ps = (int)g_music_time_pos % 60;
    int lm = (int)g_music_time_length / 60, ls = (int)g_music_time_length % 60;
    lv_label_set_text_fmt(time_label, "%02d:%02d / %02d:%02d", pm, ps, lm, ls);
    const char *name = strrchr(g_music_path[g_music_index], '/');
    name = name ? name + 1 : g_music_path[g_music_index];
    lv_label_set_text(song_label, name);
    /* 同步播放/暂停图标 */
    lv_label_set_text(play_btn_label,
        g_music_paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}

void music_player_ui_close(void)
{
    if (refresh_timer) { lv_timer_del(refresh_timer); refresh_timer = NULL; }
    if (panel) { lv_obj_del(panel); panel = NULL; }
}

/* ---- 回调 ---- */
static void on_play_pause(lv_event_t *e)
{
    (void)e;
    music_toggle_pause();
    lv_label_set_text(play_btn_label,
        g_music_paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}
static void on_prev(lv_event_t *e)  { (void)e; music_play_prev(); }
static void on_next(lv_event_t *e)  { (void)e; music_play_next(); }
static void on_close(lv_event_t *e) { (void)e; music_player_ui_close(); }
static void on_vol_changed(lv_event_t *e)
{
    (void)e;
    int v = (int)lv_slider_get_value(vol_slider);
    g_sys_volume = v;
    music_set_volume(v);
    car_ui_dashboard_update_volume(v);
}
static void on_seek(lv_event_t *e)
{
    (void)e;
    if (!g_music_playing || g_music_time_length <= 0) return;
    int pct = (int)lv_slider_get_value(prog_slider);
    /* seek <val> 1 = 绝对百分比，无需暂停读写线程（无竞态风险） */
    music_send_cmd("seek %d 1\n", pct);
}
static void on_playlist_click(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= g_music_count) return;
    g_music_index = idx;
    music_play_current();
    lv_label_set_text(play_btn_label, LV_SYMBOL_PAUSE);
}

/* ---- 创建小图标按钮 ---- */
static lv_obj_t *make_icon_btn(lv_obj_t *p, const char *s, int size, bool orange)
{
    lv_obj_t *b = lv_btn_create(p);
    lv_obj_set_size(b, size + 10, size + 10);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, s);
    lv_obj_center(l);
    lv_obj_set_style_text_color(l, orange ? lv_color_hex(0xFF6D00) : lv_color_white(), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_24, 0);
    return b;
}

lv_obj_t *music_player_ui_create(void)
{
    panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(panel, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0A0A1A), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);

    /* ==== 左侧：播放控制卡片 ==== */
    lv_obj_t *card = lv_obj_create(panel);
    lv_obj_set_size(card, CTRL_W, 460);
    lv_obj_align(card, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 20, 0);

    /* 标题栏 */
    lv_obj_t *hdr = lv_label_create(card);
    lv_label_set_text(hdr, "♫ 音乐播放器");
    lv_obj_set_style_text_color(hdr, lv_color_hex(0xFF6D00), 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);
    APPLY_ZH_18(hdr);

    lv_obj_t *cb = lv_btn_create(card);
    lv_obj_set_size(cb, 36, 36);
    lv_obj_align(cb, LV_ALIGN_TOP_RIGHT, 0, 13);
    lv_obj_set_style_bg_color(cb, lv_color_hex(0xFF6D00), 0);
    lv_obj_set_style_radius(cb, 6, 0);
    lv_obj_set_style_shadow_width(cb, 0, 0);
    lv_obj_t *cl = lv_label_create(cb);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE);
    lv_obj_center(cl);
    lv_obj_set_style_text_color(cl, lv_color_white(), 0);
    lv_obj_add_event_cb(cb, on_close, LV_EVENT_CLICKED, NULL);

    /* 歌名 */
    song_label = lv_label_create(card);
    lv_label_set_text(song_label, "暂无歌曲");
    lv_obj_set_style_text_color(song_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(song_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(song_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(song_label, CTRL_W - 50);
    lv_obj_align(song_label, LV_ALIGN_TOP_MID, 0, 110);
    APPLY_ZH_18(song_label);

    /* 歌手 */
    artist_label = lv_label_create(card);
    lv_label_set_text(artist_label, "未知歌手");
    lv_obj_set_style_text_color(artist_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(artist_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(artist_label, CTRL_W - 50);
    lv_obj_align(artist_label, LV_ALIGN_TOP_MID, 0, 140);
    APPLY_ZH_14(artist_label);

    /* 进度条 */
    prog_slider = lv_slider_create(card);
    lv_obj_set_width(prog_slider, CTRL_W - 50);
    lv_obj_set_height(prog_slider, 5);
    lv_slider_set_range(prog_slider, 0, 100);
    lv_slider_set_value(prog_slider, 0, LV_ANIM_OFF);
    lv_obj_align(prog_slider, LV_ALIGN_TOP_MID, 0, 275);
    lv_obj_set_style_bg_color(prog_slider, lv_color_hex(0x3A3A4E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(prog_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(prog_slider, lv_color_hex(0xFF6D00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(prog_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(prog_slider, lv_color_hex(0xFF6D00), LV_PART_KNOB);
    lv_obj_set_style_width(prog_slider, 10, LV_PART_KNOB);
    lv_obj_set_style_height(prog_slider, 10, LV_PART_KNOB);
    lv_obj_add_event_cb(prog_slider, on_seek, LV_EVENT_RELEASED, NULL);

    /* 时间 */
    time_label = lv_label_create(card);
    lv_label_set_text(time_label, "00:00 / 00:00");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 292);

    /* 运输按钮 */
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_set_size(row, 280, 70);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 310);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *bp = make_icon_btn(row, LV_SYMBOL_PREV, 48, false);
    lv_obj_add_event_cb(bp, on_prev, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bk = make_icon_btn(row, LV_SYMBOL_PLAY, 56, true);
    play_btn_label = lv_obj_get_child(bk, 0);
    lv_obj_add_event_cb(bk, on_play_pause, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bn = make_icon_btn(row, LV_SYMBOL_NEXT, 48, false);
    lv_obj_add_event_cb(bn, on_next, LV_EVENT_CLICKED, NULL);

    /* 音量 */
    vol_slider = lv_slider_create(card);
    lv_obj_set_width(vol_slider, 250);
    lv_obj_set_height(vol_slider, 4);
    lv_slider_set_range(vol_slider, 0, 100);
    lv_slider_set_value(vol_slider, g_sys_volume, LV_ANIM_OFF);
    lv_obj_align(vol_slider, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_add_event_cb(vol_slider, on_vol_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *vl = lv_label_create(card);
    lv_label_set_text(vl, "音量");
    lv_obj_set_style_text_color(vl, lv_color_hex(0x888888), 0);
    lv_obj_align(vl, LV_ALIGN_BOTTOM_MID, 0, -5);
    APPLY_ZH_14(vl);

    /* ==== 右侧：播放列表 ==== */
    lv_obj_t *list_card = lv_obj_create(panel);
    lv_obj_set_size(list_card, LIST_W, 460);
    lv_obj_align(list_card, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(list_card, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(list_card, 0, 0);
    lv_obj_set_style_radius(list_card, 12, 0);
    lv_obj_set_style_pad_all(list_card, 8, 0);

    lv_obj_t *lt = lv_label_create(list_card);
    lv_label_set_text(lt, "播放列表");
    lv_obj_set_style_text_color(lt, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lt, LV_ALIGN_TOP_MID, 0, 4);
    APPLY_ZH_18(lt);

    lv_obj_t *list = lv_list_create(list_card);
    lv_obj_set_size(list, LIST_W - 20, 410);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 28);

    for (int i = 0; i < g_music_count; i++) {
        const char *full = g_music_path[i];
        const char *name = strrchr(full, '/');
        name = name ? name + 1 : full;
        lv_obj_t *btn = lv_list_add_btn(list, NULL, name);
        lv_obj_add_event_cb(btn, on_playlist_click, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        APPLY_ZH_FONT(btn);
    }

    refresh_timer = lv_timer_create(refresh_ui_cb, 500, NULL);
    return panel;
}
