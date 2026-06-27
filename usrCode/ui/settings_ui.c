/**
 * @file settings_ui.c
 * @brief 系统设置面板 — 音量/亮度滑块 + 自动熄屏 + 蓝牙名称 + 保存
 *
 * 布局（440×460，覆盖中间仪表盘区域）：
 *   ┌──────────────────────────────┐
 *   │ 系统设置                     │
 *   │                              │
 *   │  音量         ──●── XX       │
 *   │  屏幕亮度     ──●── XX       │
 *   │  自动熄屏  [永不][30秒]      │
 *   │           [1分钟][5分钟]     │
 *   │  蓝牙名称:  GEC6818-CAR      │
 *   │                              │
 *   │         [保存设置]           │
 *   │        重启后生效            │
 *   └──────────────────────────────┘
 */

#include "settings_ui.h"
#include "settings_config.h"
#include "car_ui_dashboard.h"
#include "app_menu_ui.h"
#include "music_controller.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "lvgl/lvgl.h"
#include <fcntl.h>

#define BACKLIGHT_PATH "/sys/class/backlight/pwm-backlight"

/* ---- 控件句柄 ---- */
static lv_obj_t *settings_cont;
static lv_obj_t *vol_slider;
static lv_obj_t *vol_label;
static lv_obj_t *bri_slider;
static lv_obj_t *bri_label;
static lv_obj_t *off_btns[4];
static lv_obj_t *bt_label;
static lv_obj_t *save_hint_label;
static const char *off_labels[] = {"永不", "30秒", "1分钟", "5分钟"};
static const int    off_values[] = {0, 30, 60, 300};
static int off_sel = 0;

/* ---- 前向声明 ---- */
static void on_save_cb(lv_event_t *e);
static void on_vol_changed_cb(lv_event_t *e);
static void on_bri_changed_cb(lv_event_t *e);
static void on_off_clicked_cb(lv_event_t *e);
static void update_off_btn_styles(void);

/* ---- 辅助：创建标签 ---- */
static lv_obj_t *make_row_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0);
    APPLY_ZH_18(lbl);
    return lbl;
}

/* ---- 辅助：样式化滑块 ---- */
static void style_slider(lv_obj_t *s)
{
    lv_obj_set_height(s, 30);
    lv_obj_set_style_height(s, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, lv_color_hex(0x3A3A4E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, lv_color_hex(0xFF6D00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, lv_color_hex(0xFF6D00), LV_PART_KNOB);
    lv_obj_set_style_width(s, 8, LV_PART_KNOB);
    lv_obj_set_style_height(s, 8, LV_PART_KNOB);
}

/* ================================================================
 *  创建设置面板
 * ================================================================ */
void settings_ui_create(lv_obj_t *parent)
{
    /* ---- 容器：与仪表盘同位置同尺寸 ---- */
    settings_cont = lv_obj_create(parent);
    lv_obj_set_size(settings_cont, CENTER_W, ZONE_H);
    lv_obj_set_pos(settings_cont, MARGIN + SIDEBAR_W + GAP, MARGIN);
    lv_obj_set_style_bg_color(settings_cont, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(settings_cont, 0, 0);
    lv_obj_set_style_radius(settings_cont, 12, 0);
    lv_obj_set_style_pad_all(settings_cont, 20, 0);

    /* ---- 标题 + 返回按钮 ---- */
    lv_obj_t *title = lv_label_create(settings_cont);
    lv_label_set_text(title, "系统设置");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF6D00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    APPLY_ZH_18(title);

    /* 返回：通过侧边栏菜单键切换 */

    /* ======== 音量 ======== */
    lv_obj_t *row_vol = lv_obj_create(settings_cont);
    lv_obj_set_size(row_vol, CENTER_W - 40, 80);
    lv_obj_align(row_vol, LV_ALIGN_TOP_MID, 0, 65);
    lv_obj_set_style_bg_opa(row_vol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_vol, 0, 0);
    lv_obj_set_style_pad_all(row_vol, 0, 0);

    make_row_label(row_vol, "默认音量");
    vol_slider = lv_slider_create(row_vol);
    lv_obj_set_width(vol_slider, CENTER_W - 100);
    lv_slider_set_range(vol_slider, 0, 100);
    lv_slider_set_value(vol_slider, g_sys_volume, LV_ANIM_OFF);
    lv_obj_align(vol_slider, LV_ALIGN_BOTTOM_MID, 0, -40);
    style_slider(vol_slider);
    lv_obj_add_event_cb(vol_slider, on_vol_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    vol_label = lv_label_create(row_vol);
    lv_label_set_text_fmt(vol_label, "%d", g_sys_volume);
    lv_obj_set_style_text_color(vol_label, lv_color_white(), 0);
    lv_obj_align_to(vol_label, vol_slider, LV_ALIGN_OUT_TOP_RIGHT, 8, -5);
    APPLY_ZH_18(vol_label);

    /* ======== 屏幕亮度 ======== */
    lv_obj_t *row_bri = lv_obj_create(settings_cont);
    lv_obj_set_size(row_bri, CENTER_W - 40, 80);
    lv_obj_align(row_bri, LV_ALIGN_TOP_MID, 0, 135);
    lv_obj_set_style_bg_opa(row_bri, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_bri, 0, 0);
    lv_obj_set_style_pad_all(row_bri, 0, 0);

    make_row_label(row_bri, "默认屏幕亮度");
    bri_slider = lv_slider_create(row_bri);
    lv_obj_set_width(bri_slider, CENTER_W - 100);
    lv_slider_set_range(bri_slider, 0, 100);
    lv_slider_set_value(bri_slider, g_sys_brightness, LV_ANIM_OFF);
    lv_obj_align(bri_slider, LV_ALIGN_BOTTOM_MID, 0, -40);
    style_slider(bri_slider);
    lv_obj_add_event_cb(bri_slider, on_bri_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    bri_label = lv_label_create(row_bri);
    lv_label_set_text_fmt(bri_label, "%d", g_sys_brightness);
    lv_obj_set_style_text_color(bri_label, lv_color_white(), 0);
    lv_obj_align_to(bri_label, bri_slider, LV_ALIGN_OUT_TOP_RIGHT, 8, -5);
    APPLY_ZH_18(bri_label);

    /* ======== 自动熄屏 ======== */
    lv_obj_t *row_off = lv_obj_create(settings_cont);
    lv_obj_set_size(row_off, CENTER_W - 40, 65);
    lv_obj_align(row_off, LV_ALIGN_TOP_MID, 0, 200);
    lv_obj_set_style_bg_opa(row_off, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_off, 0, 0);
    lv_obj_set_style_pad_all(row_off, 0, 0);

    make_row_label(row_off, "自动熄屏");

    /* 1×4 按钮行 */
    lv_obj_t *grid = lv_obj_create(row_off);
    lv_obj_set_size(grid, CENTER_W - 40, 40);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(grid,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 4; i++) {
        off_btns[i] = lv_btn_create(grid);
        lv_obj_set_size(off_btns[i], 80, 32);
        lv_obj_set_style_bg_color(off_btns[i], lv_color_hex(0x2A2A3E), 0);
        lv_obj_set_style_radius(off_btns[i], 8, 0);
        lv_obj_set_style_border_width(off_btns[i], 0, 0);
        lv_obj_set_style_shadow_width(off_btns[i], 0, 0);
        lv_obj_t *lbl = lv_label_create(off_btns[i]);
        lv_label_set_text(lbl, off_labels[i]);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        APPLY_ZH_18(lbl);
        lv_obj_add_event_cb(off_btns[i], on_off_clicked_cb,
                            LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* 初始选中 */
    for (int i = 0; i < 4; i++) {
        if (off_values[i] == g_auto_screen_off_sec) { off_sel = i; break; }
    }
    update_off_btn_styles();

    /* ======== 蓝牙名称 ======== */
    lv_obj_t *row_bt = lv_obj_create(settings_cont);
    lv_obj_set_size(row_bt, CENTER_W - 40, 40);
    lv_obj_align(row_bt, LV_ALIGN_TOP_MID, 0, 270);
    lv_obj_set_style_bg_opa(row_bt, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_bt, 0, 0);
    lv_obj_set_style_pad_all(row_bt, 0, 0);

    lv_obj_t *bt_title = lv_label_create(row_bt);
    lv_label_set_text(bt_title, "蓝牙名称");
    lv_obj_set_style_text_color(bt_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(bt_title, LV_ALIGN_LEFT_MID, 0, 0);
    APPLY_ZH_18(bt_title);

    bt_label = lv_label_create(row_bt);
    lv_label_set_text(bt_label, g_bluetooth_name);
    lv_obj_set_style_text_color(bt_label, lv_color_white(), 0);
    lv_obj_align(bt_label, LV_ALIGN_RIGHT_MID, 0, 0);
    APPLY_ZH_18(bt_label);

    /* ======== 保存按钮 ======== */
    lv_obj_t *btn_save = lv_btn_create(settings_cont);
    lv_obj_set_size(btn_save, 160, 44);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0xFF6D00), 0);
    lv_obj_set_style_radius(btn_save, 8, 0);
    lv_obj_set_style_shadow_width(btn_save, 0, 0);
    lv_obj_t *sl = lv_label_create(btn_save);
    lv_label_set_text(sl, "保存设置");
    lv_obj_center(sl);
    lv_obj_set_style_text_color(sl, lv_color_white(), 0);
    APPLY_ZH_18(sl);
    lv_obj_add_event_cb(btn_save, on_save_cb, LV_EVENT_CLICKED, NULL);

    /* 保存提示 */
    save_hint_label = lv_label_create(settings_cont);
    lv_label_set_text(save_hint_label, "重启后生效");
    lv_obj_set_style_text_color(save_hint_label, lv_color_hex(0x666666), 0);
    lv_obj_align(save_hint_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    APPLY_ZH_FONT(save_hint_label);

    /* ---- 初始隐藏 ---- */
    lv_obj_add_flag(settings_cont, LV_OBJ_FLAG_HIDDEN);
}

/* ================================================================
 *  回调
 * ================================================================ */

static void on_vol_changed_cb(lv_event_t *e)
{
    (void)e;
    g_sys_volume = (int)lv_slider_get_value(vol_slider);
    lv_label_set_text_fmt(vol_label, "%d", g_sys_volume);
    music_set_volume(g_sys_volume);
    car_ui_dashboard_update_volume(g_sys_volume);
}

void settings_backlight_off(void)
{
    int fd = open(BACKLIGHT_PATH "/brightness", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
    }
}

void settings_backlight_apply(void)
{
    static int max_bri = -1;
    if (max_bri < 0) {
        char buf[16];
        int fd = open(BACKLIGHT_PATH "/max_brightness", O_RDONLY);
        if (fd < 0) return;
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n <= 0) return;
        buf[n] = '\0';
        max_bri = atoi(buf);
    }
    if (max_bri <= 0) return;

    int val = g_sys_brightness * max_bri / 100;
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%d", val);
    int fd = open(BACKLIGHT_PATH "/brightness", O_WRONLY);
    if (fd >= 0) {
        write(fd, buf, len);
        close(fd);
    }
}

static void on_bri_changed_cb(lv_event_t *e)
{
    (void)e;
    g_sys_brightness = (int)lv_slider_get_value(bri_slider);
    lv_label_set_text_fmt(bri_label, "%d", g_sys_brightness);
    settings_backlight_apply();
}

static void on_off_clicked_cb(lv_event_t *e)
{
    off_sel = (int)(intptr_t)lv_event_get_user_data(e);
    g_auto_screen_off_sec = off_values[off_sel];
    update_off_btn_styles();
}

static void update_off_btn_styles(void)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = off_btns[i];
        lv_obj_t *lbl = lv_obj_get_child(btn, 0);
        if (i == off_sel) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF6D00), 0);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A3E), 0);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        }
    }
}

static void on_save_cb(lv_event_t *e)
{
    (void)e;
    settings_config_save();
    lv_label_set_text(save_hint_label, "已保存，重启后生效");
}

/* ================================================================
 *  显示/隐藏
 * ================================================================ */

void settings_ui_open(void)
{
    if (!settings_cont) return;
    lv_obj_clear_flag(settings_cont, LV_OBJ_FLAG_HIDDEN);
}

void settings_ui_close(void)
{
    settings_ui_hide();
    /* 显示应用菜单（当前为隐藏状态，toggle 会 show） */
    app_menu_ui_toggle();
}

void settings_ui_hide(void)
{
    if (settings_cont)
        lv_obj_add_flag(settings_cont, LV_OBJ_FLAG_HIDDEN);
}
