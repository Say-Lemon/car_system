/**
 * @file app_menu_ui.c
 * @brief 应用菜单 — 网格布局选择要启动的程序
 */
#include "app_menu_ui.h"
#include "music_player_ui.h"
#include "vp_player_ui.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "lvgl/lvgl.h"

static lv_obj_t *panel;

void app_menu_ui_close(void)
{
    if (panel) { lv_obj_del(panel); panel = NULL; }
}

/* ---- 创建应用卡片 ---- */
static lv_obj_t *create_app_card(lv_obj_t *parent, const char *icon,
                                  const char *label, lv_event_cb_t cb)
{
    lv_obj_t *card = lv_btn_create(parent);
    lv_obj_set_size(card, 120, 120);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A3E), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_shadow_width(card, 3, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);

    lv_obj_t *icon_lbl = lv_label_create(card);
    lv_label_set_text(icon_lbl, icon);
    lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_color(icon_lbl, lv_color_hex(0xFF6D00), 0);
    APPLY_ZH_FONT(icon_lbl);

    lv_obj_t *text_lbl = lv_label_create(card);
    lv_label_set_text(text_lbl, label);
    lv_obj_align(text_lbl, LV_ALIGN_CENTER, 0, 28);
    lv_obj_set_style_text_color(text_lbl, lv_color_white(), 0);
    APPLY_ZH_18(text_lbl);

    lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, NULL);
    return card;
}

/* ---- 回调 ---- */
static void on_music(lv_event_t *e) { (void)e; app_menu_ui_close(); music_player_ui_create(); }
static void on_video(lv_event_t *e) { (void)e; app_menu_ui_close(); video_player_launch(); }
static void on_close(lv_event_t *e) { (void)e; app_menu_ui_close(); }

lv_obj_t *app_menu_ui_create(void)
{
    panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(panel, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0A0A1A), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);

    /* 卡片 */
    lv_obj_t *card = lv_obj_create(panel);
    lv_obj_set_size(card, 440, 300);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 20, 0);

    /* 标题 */
    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "应用菜单");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF6D00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    APPLY_ZH_18(title);

    /* 关闭 */
    lv_obj_t *cb = lv_btn_create(card);
    lv_obj_set_size(cb, 36, 36);
    lv_obj_align(cb, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(cb, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cb, 0, 0);
    lv_obj_set_style_shadow_width(cb, 0, 0);
    lv_obj_t *cl = lv_label_create(cb);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE);
    lv_obj_center(cl);
    lv_obj_set_style_text_color(cl, lv_color_hex(0x888888), 0);
    lv_obj_add_event_cb(cb, on_close, LV_EVENT_CLICKED, NULL);

    /* 应用网格 */
    lv_obj_t *grid = lv_obj_create(card);
    lv_obj_set_size(grid, 380, 200);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_app_card(grid, "乐",  "音乐播放器", on_music);
    create_app_card(grid, "视",  "视频播放器", on_video);
    /* 未来: create_app_card(grid, "⚙", "设置", on_settings); */

    return panel;
}
