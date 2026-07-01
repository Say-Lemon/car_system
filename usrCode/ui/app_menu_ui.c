/**
 * @file app_menu_ui.c
 * @brief 应用菜单 — 中间区域（440×460）网格选择应用
 *
 * 与空调面板同模式：按侧边栏菜单键切换仪表盘↔菜单，不弹窗。
 */

#include "app_menu_ui.h"
#include "car_ui_dashboard.h"
#include "car_ui_ac_panel.h"
#include "settings_ui.h"
#include "camera_ui.h"
#include "music_player_ui.h"
#include "vp_player_ui.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "lvgl/lvgl.h"

static lv_obj_t *menu_cont;

/* ---- 创建应用卡片 ---- */
static lv_obj_t *create_app_card(lv_obj_t *parent, const char *icon,
                                  const char *label, lv_event_cb_t cb)
{
    lv_obj_t *card = lv_btn_create(parent);
    lv_obj_set_size(card, 160, 140);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A3E), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_shadow_width(card, 3, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);

    lv_obj_t *icon_lbl = lv_label_create(card);
    lv_label_set_text(icon_lbl, icon);
    lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -12);
    lv_obj_set_style_text_color(icon_lbl, lv_color_hex(0xFF6D00), 0);
    APPLY_ZH_FONT(icon_lbl);

    lv_obj_t *text_lbl = lv_label_create(card);
    lv_label_set_text(text_lbl, label);
    lv_obj_align(text_lbl, LV_ALIGN_CENTER, 0, 36);
    lv_obj_set_style_text_color(text_lbl, lv_color_white(), 0);
    APPLY_ZH_18(text_lbl);

    lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, NULL);
    return card;
}

/* ---- 回调 ---- */
static void on_music(lv_event_t *e)
{
    (void)e;
    app_menu_ui_toggle();       /* 先切回仪表盘 */
    music_player_ui_create();   /* 再打开音乐播放器 */
}

static void on_video(lv_event_t *e)
{
    (void)e;
    app_menu_ui_toggle();       /* 先切回仪表盘 */
    video_player_launch();      /* 再打开视频播放器 */
}

static void on_settings(lv_event_t *e)
{
    (void)e;
    app_menu_ui_hide();         /* 隐藏菜单 */
    settings_ui_open();         /* 打开设置面板 */
}

static void on_reverse(lv_event_t *e)
{
    (void)e;
    app_menu_ui_toggle();       /* 切回仪表盘 */
    camera_ui_open();           /* 打开倒车影像 */
}

/* ================================================================
 *  创建菜单面板
 * ================================================================ */
void app_menu_ui_create(lv_obj_t *parent)
{
    /* ---- 容器：与仪表盘同位置同尺寸 ---- */
    menu_cont = lv_obj_create(parent);
    lv_obj_set_size(menu_cont, CENTER_W, ZONE_H);
    lv_obj_set_pos(menu_cont, MARGIN + SIDEBAR_W + GAP, MARGIN);
    lv_obj_set_style_bg_color(menu_cont, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(menu_cont, 0, 0);
    lv_obj_set_style_radius(menu_cont, 12, 0);
    lv_obj_set_style_pad_all(menu_cont, 20, 0);

    /* ---- 标题 ---- */
    lv_obj_t *title = lv_label_create(menu_cont);
    lv_label_set_text(title, "应用菜单");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF6D00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    APPLY_ZH_18(title);

    /* ---- 应用网格 ---- */
    lv_obj_t *grid = lv_obj_create(menu_cont);
    lv_obj_set_size(grid, CENTER_W - 40, 320);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER);

    create_app_card(grid, "乐",  "音乐播放器", on_music);
    create_app_card(grid, "视",  "视频播放器", on_video);
    create_app_card(grid, "设",  "系统设置",   on_settings);
    create_app_card(grid, "倒",  "倒车影像",   on_reverse);

    /* ---- 初始隐藏 ---- */
    lv_obj_add_flag(menu_cont, LV_OBJ_FLAG_HIDDEN);
}

/* ================================================================
 *  切换
 * ================================================================ */

void app_menu_ui_toggle(void)
{
    if (!menu_cont || !g_dashboard_cont) return;

    bool menu_visible = !lv_obj_has_flag(menu_cont, LV_OBJ_FLAG_HIDDEN);

    if (menu_visible) {
        /* 当前显示菜单 → 隐藏菜单，显示仪表盘 */
        lv_obj_add_flag(menu_cont, LV_OBJ_FLAG_HIDDEN);
        settings_ui_hide();
        lv_obj_clear_flag(g_dashboard_cont, LV_OBJ_FLAG_HIDDEN);
        printf("[Menu] 切换到仪表盘\n");
    } else {
        /* 显示菜单，隐藏仪表盘、空调面板和设置 */
        lv_obj_add_flag(g_dashboard_cont, LV_OBJ_FLAG_HIDDEN);
        car_ui_ac_panel_hide();
        settings_ui_hide();
        lv_obj_clear_flag(menu_cont, LV_OBJ_FLAG_HIDDEN);
        printf("[Menu] 切换到应用菜单\n");
    }
}

void app_menu_ui_hide(void)
{
    if (menu_cont) {
        lv_obj_add_flag(menu_cont, LV_OBJ_FLAG_HIDDEN);
    }
}
