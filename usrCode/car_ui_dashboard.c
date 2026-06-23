/**
 * @file car_ui_dashboard.c
 * @brief 中间仪表盘 — car.png 背景（嵌入）+ 车速/油量/转速数据叠加
 *
 * 车速：大号数字 + 下方小号 KM/H，中上区域
 * 油量：左下角，绿色文字
 * 转速：右下角，蓝色文字
 */
#include "car_ui_dashboard.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "lvgl/lvgl.h"

/* 嵌入的 car.png 图片数据 */
extern const lv_img_dsc_t car_img;

lv_obj_t *g_label_speed;
lv_obj_t *g_label_speed_unit;
lv_obj_t *g_label_fuel;
lv_obj_t *g_label_volume;
lv_obj_t *g_label_rpm;

/* 前向声明 */
static void dashboard_refresh_timer_cb(lv_timer_t *t);

void car_ui_dashboard_create(lv_obj_t *parent)
{
    /* ---- 外层容器 ---- */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, CENTER_W, ZONE_H);
    lv_obj_set_pos(cont, MARGIN + SIDEBAR_W + GAP, MARGIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x0A0A1A), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 12, 0);
    lv_obj_set_style_clip_corner(cont, true, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);

    /* ---- 嵌入图片 ---- */
    lv_obj_t *img = lv_img_create(cont);
    lv_img_set_src(img, &car_img);
    lv_obj_set_size(img, CENTER_W, ZONE_H);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    /* ---- 车速：大号数字 + 单位（无底色） ---- */
    g_label_speed = lv_label_create(cont);
    lv_label_set_text(g_label_speed, "0");
    lv_obj_align(g_label_speed, LV_ALIGN_CENTER, 0, -160);
    lv_obj_set_style_text_color(g_label_speed, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_label_speed, &lv_font_montserrat_48, 0);

    g_label_speed_unit = lv_label_create(cont);
    lv_label_set_text(g_label_speed_unit, "KM/H");
    lv_obj_align(g_label_speed_unit, LV_ALIGN_CENTER, 0, -120);
    lv_obj_set_style_text_color(g_label_speed_unit, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(g_label_speed_unit, &lv_font_montserrat_16, 0);
    /* ---- 油量：左下角（无底色，稍大字体） ---- */
    g_label_fuel = lv_label_create(cont);
    lv_label_set_text(g_label_fuel, "油量 80%");
    lv_obj_align(g_label_fuel, LV_ALIGN_BOTTOM_LEFT, 15, -15);
    lv_obj_set_style_text_color(g_label_fuel, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_text_font(g_label_fuel, &lv_font_montserrat_18, 0);
    APPLY_ZH_18(g_label_fuel);

    /* ---- 音量：底部居中 ---- */
    g_label_volume = lv_label_create(cont);
    lv_label_set_text(g_label_volume, "音量 10%");
    lv_obj_align(g_label_volume, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_text_color(g_label_volume, lv_color_hex(0xFFC107), 0);
    lv_obj_set_style_text_font(g_label_volume, &lv_font_montserrat_18, 0);
    APPLY_ZH_18(g_label_volume);

    /* ---- 转速：右下角（无底色，稍大字体） ---- */
    g_label_rpm = lv_label_create(cont);
    lv_label_set_text(g_label_rpm, "转速 800 RPM");
    lv_obj_align(g_label_rpm, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
    lv_obj_set_style_text_color(g_label_rpm, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_text_font(g_label_rpm, &lv_font_montserrat_18, 0);
    APPLY_ZH_18(g_label_rpm);

    /* ---- CAN 数据刷新定时器（200ms） ---- */
    lv_timer_create(dashboard_refresh_timer_cb, 200, NULL);
}

/* ========== 仪表盘数据更新函数 ========== */

void dashboard_update_speed(int kmh)
{
    if (!g_label_speed) return;
    lv_label_set_text_fmt(g_label_speed, "%d", kmh);
}

void dashboard_update_fuel(int pct)
{
    if (!g_label_fuel) return;
    lv_label_set_text_fmt(g_label_fuel, "油量 %d%%", pct);
}

void dashboard_update_rpm(int rpm)
{
    if (!g_label_rpm) return;
    lv_label_set_text_fmt(g_label_rpm, "转速 %d RPM", rpm);
}

void car_ui_dashboard_update_volume(int vol)
{
    if (g_label_volume)
        lv_label_set_text_fmt(g_label_volume, "音量 %d%%", vol);
}

/* ========== CAN 数据刷新定时器回调 ========== */

static void dashboard_refresh_timer_cb(lv_timer_t *t)
{
    (void)t;
    int speed, fuel, rpm;
    pthread_mutex_lock(&g_can_mutex);
    speed = g_speed_kmh;
    fuel  = g_fuel_percent;
    rpm   = g_engine_rpm;
    pthread_mutex_unlock(&g_can_mutex);

    dashboard_update_speed(speed);
    dashboard_update_fuel(fuel);
    dashboard_update_rpm(rpm);
}
