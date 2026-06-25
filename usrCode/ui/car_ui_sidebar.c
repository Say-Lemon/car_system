/**
 * @file car_ui_sidebar.c
 * @brief 左侧图标栏 — 5 个圆形触控按钮
 *
 * 图标来源：Segoe MDL2 / Segoe UI Symbol / FontAwesome (simsun)
 *   ⏻  U+E7E8  — 电源开关 (Segoe MDL2, 20px)
 *   🔊 U+E995  — 音量加   (Segoe MDL2, 20px)
 *   🔈 U+E993  — 音量减   (Segoe MDL2, 20px)
 *   ❄  U+2744  — 空调/雪花 (Segoe UI Symbol, 26px)
 *   ☰  U+F00B  — 菜单列表 (FontAwesome via simsun, 16px)
 *
 * 样式：
 *   - 56×56px 完美圆形（radius: 50%）
 *   - 前 4 个：深色底 #2A2A3E + 白色图标
 *   - 菜单键：亮橙色底 #FF6D00 + 白色图标 + 外发光
 *   - 按压态：底色轻微变亮 + 阴影加深
 */

#include "car_ui_sidebar.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "font_icons_20.h"
#include "music_controller.h"
#include "app_menu_ui.h"
#include "car_ui_dashboard.h"
#include "car_ui_ac_panel.h"
#include "settings_ui.h"
#include "vp_playback_controller.h"
#include "lvgl/lvgl.h"

/* 雪花图标使用 26px 独立字体（更大更清晰） */
LV_FONT_DECLARE(snowflake_26);

/* 菜单图标使用 simsun 内置 16px FontAwesome（用户认可的效果） */
LV_FONT_DECLARE(lv_font_simsun_16_cjk);

/* ========== 图标 Unicode 码点（UTF-8 编码） ========== */
#define ICON_POWER     "\xEE\x9F\xA8"   /* U+E7E8  电源开关   (20px Segoe MDL2) */
#define ICON_VOL_UP    "\xEE\xA6\x95"   /* U+E995  喇叭+声波  (20px Segoe MDL2) */
#define ICON_VOL_DOWN  "\xEE\xA6\x93"   /* U+E993  喇叭+少声波(20px Segoe MDL2) */
#define ICON_AC        "\xE2\x9D\x84"   /* U+2744  ❄ 雪花     (24px Segoe UI Symbol) */
#define ICON_MENU      "\xEF\x80\x8B"   /* U+F00B  三条线列表 (16px FontAwesome via simsun) */

/* ========== 圆形按钮参数 ========== */
#define CIRCLE_DIAMETER  56
#define CIRCLE_RADIUS    LV_PCT(50)

/* ========== 颜色常量 ========== */
#define COLOR_DARK_BG    lv_color_hex(0x2A2A3E)   /* 深色圆形底 */
#define COLOR_DARK_HOVER lv_color_hex(0x3D3D56)   /* 按压变亮 */
#define COLOR_ORANGE_BG  lv_color_hex(0xFF6D00)   /* 菜单亮橙底 */
#define COLOR_ORANGE_HOVER lv_color_hex(0xFF8F33) /* 菜单按压 */
#define COLOR_WHITE      lv_color_hex(0xFFFFFF)

/* ========== 全局按钮句柄 ========== */
lv_obj_t *g_btn_power;
lv_obj_t *g_btn_vol_up;
lv_obj_t *g_btn_vol_down;
lv_obj_t *g_btn_ac;
lv_obj_t *g_btn_app_menu;

/* ========== 回调原型 ========== */
static void sidebar_power_cb(lv_event_t *e);
static void sidebar_vol_up_cb(lv_event_t *e);
static void sidebar_vol_down_cb(lv_event_t *e);
static void sidebar_ac_cb(lv_event_t *e);
static void sidebar_app_menu_cb(lv_event_t *e);

/* ========== 内部辅助：创建圆形图标按钮 ========== */
static lv_obj_t *create_circle_btn(lv_obj_t *parent,
                                   const char *icon_text,
                                   lv_color_t  bg_color,
                                   lv_color_t  hover_color,
                                   bool        has_glow)
{
    /* 圆形按钮主体 */
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, CIRCLE_DIAMETER, CIRCLE_DIAMETER);
    lv_obj_set_style_radius(btn, CIRCLE_RADIUS, 0);

    /* 背景色 */
    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_set_style_bg_color(btn, hover_color, LV_STATE_PRESSED);

    /* 边框：微妙的灰边让圆形在深色侧栏中清晰可见 */
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x4A4A5E), 0);

    /* 阴影 */
    lv_obj_set_style_shadow_width(btn, 4, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    lv_obj_set_style_shadow_width(btn, 6, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_50, LV_STATE_PRESSED);

    /* 外发光（仅菜单键） */
    if (has_glow) {
        lv_obj_set_style_shadow_width(btn, 8, 0);
        lv_obj_set_style_shadow_color(btn, COLOR_ORANGE_BG, 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_60, 0);
        lv_obj_set_style_shadow_width(btn, 12, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_80, LV_STATE_PRESSED);
    }

    /* 图标文字（居中，使用 20px 图标字体更清晰） */
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, icon_text);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, COLOR_WHITE, 0);
    APPLY_ICON_FONT(label);  /* 20px 图标字体，比中文 16px 更大更统一 */

    return btn;
}

/* ========== 创建侧边栏 ========== */
void car_ui_sidebar_create(lv_obj_t *parent)
{
    /* 侧边栏容器：深色背景 + 弹性列布局 + 均匀分布 */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, SIDEBAR_W, ZONE_H);
    lv_obj_set_pos(cont, MARGIN, MARGIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 7, 0);   /* 7px 内边距 */
    lv_obj_set_style_radius(cont, 12, 0);    /* 侧栏圆角 */
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    /* ---- 电源按钮 ---- */
    g_btn_power = create_circle_btn(cont, ICON_POWER,
                                    COLOR_DARK_BG, COLOR_DARK_HOVER, false);
    lv_obj_add_event_cb(g_btn_power, sidebar_power_cb,
                        LV_EVENT_CLICKED, NULL);

    /* ---- 音量+ 按钮 ---- */
    g_btn_vol_up = create_circle_btn(cont, ICON_VOL_UP,
                                     COLOR_DARK_BG, COLOR_DARK_HOVER, false);
    lv_obj_add_event_cb(g_btn_vol_up, sidebar_vol_up_cb,
                        LV_EVENT_CLICKED, NULL);

    /* ---- 音量- 按钮 ---- */
    g_btn_vol_down = create_circle_btn(cont, ICON_VOL_DOWN,
                                       COLOR_DARK_BG, COLOR_DARK_HOVER, false);
    lv_obj_add_event_cb(g_btn_vol_down, sidebar_vol_down_cb,
                        LV_EVENT_CLICKED, NULL);

    /* ---- 空调按钮（雪花使用 24px 独立字体，更大） ---- */
    g_btn_ac = create_circle_btn(cont, ICON_AC,
                                 COLOR_DARK_BG, COLOR_DARK_HOVER, false);
    {
        lv_obj_t *ac_label = lv_obj_get_child(g_btn_ac, 0);
        lv_obj_set_style_text_font(ac_label, &snowflake_26, 0);
    }
    lv_obj_add_event_cb(g_btn_ac, sidebar_ac_cb,
                        LV_EVENT_CLICKED, NULL);

    /* ---- 菜单按钮（亮橙色 + 外发光，使用 simsun 16px FontAwesome） ---- */
    g_btn_app_menu = create_circle_btn(cont, ICON_MENU,
                                       COLOR_ORANGE_BG, COLOR_ORANGE_HOVER,
                                       true);
    {
        lv_obj_t *menu_label = lv_obj_get_child(g_btn_app_menu, 0);
        lv_obj_set_style_text_font(menu_label, &lv_font_simsun_16_cjk, 0);
    }
    lv_obj_add_event_cb(g_btn_app_menu, sidebar_app_menu_cb,
                        LV_EVENT_CLICKED, NULL);
}

/* ================================================================
 *  按钮事件回调
 * ================================================================ */

static lv_obj_t *standby_overlay = NULL;

static void on_standby_click(lv_event_t *e)
{
    (void)e;
    car_ui_standby_deactivate();
}

void car_ui_standby_activate(void)
{
    if (standby_overlay) return;
    music_stop_all();
    playback_stop_all();

    standby_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(standby_overlay, DISP_HOR_RES, DISP_VER_RES);
    lv_obj_set_pos(standby_overlay, 0, 0);
    lv_obj_set_style_bg_color(standby_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(standby_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(standby_overlay, 0, 0);
    lv_obj_clear_flag(standby_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(standby_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(standby_overlay, on_standby_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hint = lv_label_create(standby_overlay);
    lv_label_set_text(hint, "待机中\n点击屏幕唤醒");
    lv_obj_center(hint);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    APPLY_ZH_18(hint);
    printf("[Power] 进入待机\n");
}

void car_ui_standby_deactivate(void)
{
    if (!standby_overlay) return;
    lv_obj_del(standby_overlay);
    standby_overlay = NULL;
    g_last_input_time = time(NULL);
    printf("[Power] 退出待机\n");
}

bool car_ui_is_standby(void)
{
    return standby_overlay != NULL;
}

static void sidebar_power_cb(lv_event_t *e)
{
    (void)e;
    if (!standby_overlay) car_ui_standby_activate();
    else                   car_ui_standby_deactivate();
}

static void sidebar_vol_up_cb(lv_event_t *e)
{
    (void)e;
    g_sys_volume += 5;
    if (g_sys_volume > 100) g_sys_volume = 100;
    music_set_volume(g_sys_volume);
    car_ui_dashboard_update_volume(g_sys_volume);
    printf("[Sidebar] 音量+ → %d\n", g_sys_volume);
}

static void sidebar_vol_down_cb(lv_event_t *e)
{
    (void)e;
    g_sys_volume -= 5;
    if (g_sys_volume < 0) g_sys_volume = 0;
    music_set_volume(g_sys_volume);
    car_ui_dashboard_update_volume(g_sys_volume);
    printf("[Sidebar] 音量- → %d\n", g_sys_volume);
}

static void sidebar_ac_cb(lv_event_t *e)
{
    (void)e;
    car_ui_ac_panel_toggle();
}

static void sidebar_app_menu_cb(lv_event_t *e)
{
    (void)e;
    app_menu_ui_toggle();
}
