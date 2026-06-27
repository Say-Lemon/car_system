/**
 * @file car_ui_ac_panel.c
 * @brief 空调控制面板 — 温度/风量滑块 + 吹风模式选择
 *
 * 布局（440×460，覆盖中间仪表盘区域）：
 *   ┌──────────────────────────────┐
 *   │  ❄ 空调控制                   │  标题
 *   │                              │
 *   │       温度  24 °C             │  大号温度值
 *   │  16 °C ──●────────── 32 °C  │  温度滑块
 *   │                              │
 *   │       风量  3 档              │  风量值
 *   │   1 ──●─────────── 7        │  风量滑块
 *   │                              │
 *   │  模式                        │
 *   │  [吹面] [吹脚]               │  2×2 按钮
 *   │  [吹面+吹脚] [除霜]           │
 *   └──────────────────────────────┘
 */

#include "car_ui_ac_panel.h"
#include "car_ui_dashboard.h"
#include "app_menu_ui.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "lvgl/lvgl.h"

/* ---- 控件句柄 ---- */
static lv_obj_t *ac_cont;
static lv_obj_t *temp_label;      /* 温度数值标签 */
static lv_obj_t *temp_slider;
static lv_obj_t *fan_label;       /* 风量数值标签 */
static lv_obj_t *fan_slider;
static lv_obj_t *mode_btns[4];    /* 4 个模式按钮 */

/* 模式名称 */
static const char *mode_names[] = {
    "吹面", "吹脚", "吹面+吹脚", "除霜"
};

/* ---- 前向声明 ---- */
static void update_mode_btn_styles(void);
static void on_temp_changed_cb(lv_event_t *e);
static void on_fan_changed_cb(lv_event_t *e);
static void on_mode_clicked_cb(lv_event_t *e);

/* ---- 辅助：创建样式滑块 ---- */
static void style_ac_slider(lv_obj_t *slider)
{
    lv_obj_set_height(slider, 30);                           /* 触摸区高度 */
    lv_obj_set_style_height(slider, 4, LV_PART_MAIN);        /* 轨道视觉细条 */
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x3A3A4E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFF6D00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFF6D00), LV_PART_KNOB);
    lv_obj_set_style_width(slider, 8, LV_PART_KNOB);
    lv_obj_set_style_height(slider, 8, LV_PART_KNOB);
}

/* ---- 辅助：创建模式按钮 ---- */
static lv_obj_t *create_mode_btn(lv_obj_t *parent, const char *text)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 110, 38);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A3E), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    APPLY_ZH_18(label);

    return btn;
}

/* ================================================================
 *  创建空调面板
 * ================================================================ */
void car_ui_ac_panel_create(lv_obj_t *parent)
{
    /* ---- 容器：与仪表盘同位置同尺寸 ---- */
    ac_cont = lv_obj_create(parent);
    lv_obj_set_size(ac_cont, CENTER_W, ZONE_H);
    lv_obj_set_pos(ac_cont, MARGIN + SIDEBAR_W + GAP, MARGIN);
    lv_obj_set_style_bg_color(ac_cont, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(ac_cont, 0, 0);
    lv_obj_set_style_radius(ac_cont, 12, 0);
    lv_obj_set_style_pad_all(ac_cont, 20, 0);

    /* ---- 标题 ---- */
    lv_obj_t *title = lv_label_create(ac_cont);
    lv_label_set_text(title, "空调控制");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF6D00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    APPLY_ZH_18(title);

    /* ======== 温度区域 ======== */
    lv_obj_t *temp_section = lv_obj_create(ac_cont);
    lv_obj_set_size(temp_section, CENTER_W - 40, 110);
    lv_obj_align(temp_section, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_opa(temp_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(temp_section, 0, 0);
    lv_obj_set_style_pad_all(temp_section, 0, 0);

    /* 温度数值（居中大字） */
    temp_label = lv_label_create(temp_section);
    lv_label_set_text_fmt(temp_label, "温度  %d °C", g_ac_temperature);
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_text_color(temp_label, lv_color_white(), 0);
    APPLY_ZH_18(temp_label);

    /* 温度滑块 */
    temp_slider = lv_slider_create(temp_section);
    lv_obj_set_width(temp_slider, CENTER_W - 60);
    lv_slider_set_range(temp_slider, 16, 32);
    lv_slider_set_value(temp_slider, g_ac_temperature, LV_ANIM_OFF);
    lv_obj_align(temp_slider, LV_ALIGN_BOTTOM_MID, 0, 0);
    style_ac_slider(temp_slider);
    lv_obj_add_event_cb(temp_slider, on_temp_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 温度范围标签 */
    lv_obj_t *t_lo = lv_label_create(temp_section);
    lv_label_set_text(t_lo, "16 °C");
    lv_obj_align(t_lo, LV_ALIGN_BOTTOM_LEFT, 2, -12);
    lv_obj_set_style_text_color(t_lo, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(t_lo, &lv_font_montserrat_12, 0);

    lv_obj_t *t_hi = lv_label_create(temp_section);
    lv_label_set_text(t_hi, "32 °C");
    lv_obj_align(t_hi, LV_ALIGN_BOTTOM_RIGHT, -2, -12);
    lv_obj_set_style_text_color(t_hi, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(t_hi, &lv_font_montserrat_12, 0);

    /* ======== 风量区域 ======== */
    lv_obj_t *fan_section = lv_obj_create(ac_cont);
    lv_obj_set_size(fan_section, CENTER_W - 40, 110);
    lv_obj_align(fan_section, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_set_style_bg_opa(fan_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fan_section, 0, 0);
    lv_obj_set_style_pad_all(fan_section, 0, 0);

    /* 风量数值 */
    fan_label = lv_label_create(fan_section);
    lv_label_set_text_fmt(fan_label, "风量  %d 档", g_ac_fan_speed);
    lv_obj_align(fan_label, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_text_color(fan_label, lv_color_white(), 0);
    APPLY_ZH_18(fan_label);

    /* 风量滑块 */
    fan_slider = lv_slider_create(fan_section);
    lv_obj_set_width(fan_slider, CENTER_W - 60);
    lv_slider_set_range(fan_slider, 1, 7);
    lv_slider_set_value(fan_slider, g_ac_fan_speed, LV_ANIM_OFF);
    lv_obj_align(fan_slider, LV_ALIGN_BOTTOM_MID, 0, 0);
    style_ac_slider(fan_slider);
    lv_obj_add_event_cb(fan_slider, on_fan_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 风量范围标签 */
    lv_obj_t *f_lo = lv_label_create(fan_section);
    lv_label_set_text(f_lo, "1");
    lv_obj_align(f_lo, LV_ALIGN_BOTTOM_LEFT, 2, -12);
    lv_obj_set_style_text_color(f_lo, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(f_lo, &lv_font_montserrat_12, 0);

    lv_obj_t *f_hi = lv_label_create(fan_section);
    lv_label_set_text(f_hi, "7");
    lv_obj_align(f_hi, LV_ALIGN_BOTTOM_RIGHT, -2, -12);
    lv_obj_set_style_text_color(f_hi, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(f_hi, &lv_font_montserrat_12, 0);

    /* ======== 模式区域 ======== */
    lv_obj_t *mode_section = lv_obj_create(ac_cont);
    lv_obj_set_size(mode_section, CENTER_W - 40, 120);
    lv_obj_align(mode_section, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_opa(mode_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mode_section, 0, 0);
    lv_obj_set_style_pad_all(mode_section, 0, 0);

    /* 模式标题 */
    lv_obj_t *mode_title = lv_label_create(mode_section);
    lv_label_set_text(mode_title, "模式");
    lv_obj_align(mode_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(mode_title, lv_color_hex(0xAAAAAA), 0);
    APPLY_ZH_18(mode_title);

    /* 2×2 按钮网格：限定宽度确保每行 2 个 */
    lv_obj_t *grid = lv_obj_create(mode_section);
    lv_obj_set_size(grid, 250, 90);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 4; i++) {
        mode_btns[i] = create_mode_btn(grid, mode_names[i]);
        lv_obj_add_event_cb(mode_btns[i], on_mode_clicked_cb,
                            LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* 初始选中态 */
    update_mode_btn_styles();

    /* ---- 初始隐藏 ---- */
    lv_obj_add_flag(ac_cont, LV_OBJ_FLAG_HIDDEN);
}

/* ================================================================
 *  回调
 * ================================================================ */

static void on_temp_changed_cb(lv_event_t *e)
{
    (void)e;
    g_ac_temperature = (int)lv_slider_get_value(temp_slider);
    lv_label_set_text_fmt(temp_label, "温度  %d °C", g_ac_temperature);
}

static void on_fan_changed_cb(lv_event_t *e)
{
    (void)e;
    g_ac_fan_speed = (int)lv_slider_get_value(fan_slider);
    lv_label_set_text_fmt(fan_label, "风量  %d 档", g_ac_fan_speed);
}

static void on_mode_clicked_cb(lv_event_t *e)
{
    g_ac_mode = (int)(intptr_t)lv_event_get_user_data(e);
    update_mode_btn_styles();
}

static void update_mode_btn_styles(void)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = mode_btns[i];
        lv_obj_t *lbl = lv_obj_get_child(btn, 0);
        if (i == g_ac_mode) {
            /* 选中：橙色底 + 白色字 */
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF6D00), 0);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        } else {
            /* 未选中：深灰底 + 白色字 */
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A3E), 0);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        }
    }
}

/* ================================================================
 *  切换
 * ================================================================ */

void car_ui_ac_panel_toggle(void)
{
    if (!ac_cont || !g_dashboard_cont) return;

    bool ac_visible = !lv_obj_has_flag(ac_cont, LV_OBJ_FLAG_HIDDEN);

    if (ac_visible) {
        /* 当前显示空调 → 隐藏空调，显示仪表盘 */
        lv_obj_add_flag(ac_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_dashboard_cont, LV_OBJ_FLAG_HIDDEN);
        printf("[AC] 切换到仪表盘\n");
    } else {
        /* 显示空调，隐藏仪表盘和应用菜单 */
        lv_obj_add_flag(g_dashboard_cont, LV_OBJ_FLAG_HIDDEN);
        app_menu_ui_hide();
        lv_obj_clear_flag(ac_cont, LV_OBJ_FLAG_HIDDEN);
        printf("[AC] 切换到空调面板\n");
    }
}

void car_ui_ac_panel_hide(void)
{
    if (ac_cont) {
        lv_obj_add_flag(ac_cont, LV_OBJ_FLAG_HIDDEN);
    }
}
