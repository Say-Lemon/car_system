/**
 * @file car_ui_status.c
 * @brief 右上时间天气信息卡 — 半透明深灰圆角卡片
 *
 * 布局（250 × 140）：
 *   ┌─────────────────────────┐
 *   │    08:20  星期二         │  时钟行：时间 + 星期/日期紧挨居中
 *   │           2026/6/17      │
 *   │  番禺  26°  晴           │  天气行：定位 + 温度 + 天气
 *   └─────────────────────────┘
 */

#include "car_ui_status.h"
#include "app_config.h"
#include "font_zh_cn.h"
#include "lvgl/lvgl.h"

lv_obj_t *g_label_date;
lv_obj_t *g_label_weekday;
lv_obj_t *g_label_time;
lv_obj_t *g_label_weather;

static const char *weekday_cn[] = {
    "星期日", "星期一", "星期二", "星期三",
    "星期四", "星期五", "星期六"
};

static void status_clock_refresh_cb(lv_timer_t *t)
{
    (void)t;
    static int last_min = -1, last_wday = -1, last_mday = -1;

    /* 视频播放器覆盖时跳过：避免 fbdev_flush 覆盖 mplayer 画面导致闪烁 */
    if (g_video_overlay_active) return;
    time_t   now = time(NULL);
    struct tm *tm = localtime(&now);

    /* 每分钟刷新时间，日期和星期仅变化时刷新 */
    if (tm->tm_min != last_min) {
        lv_label_set_text_fmt(g_label_time, "%02d:%02d",
                              tm->tm_hour, tm->tm_min);
        last_min = tm->tm_min;
    }
    if (tm->tm_wday != last_wday) {
        lv_label_set_text(g_label_weekday, weekday_cn[tm->tm_wday]);
        last_wday = tm->tm_wday;
    }
    if (tm->tm_mday != last_mday) {
        lv_label_set_text_fmt(g_label_date, "%d/%d/%d",
                              tm->tm_year + 1900,
                              tm->tm_mon + 1,
                              tm->tm_mday);
        last_mday = tm->tm_mday;
    }
}

void car_ui_status_create(lv_obj_t *parent)
{
    /* ---- 半透明深灰圆角卡片 ---- */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, RIGHT_W, STATUS_H);
    lv_obj_set_pos(card,
                   MARGIN + SIDEBAR_W + GAP + CENTER_W + GAP,
                   MARGIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 8, 0);

    /* ==== 时钟行：时间 + 星期/日期（弹性水平布局，居中） ==== */
    lv_obj_t *clock_row = lv_obj_create(card);
    lv_obj_set_size(clock_row, RIGHT_W - 16, 50);
    lv_obj_align(clock_row, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_opa(clock_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_row, 0, 0);
    lv_obj_set_style_pad_all(clock_row, 0, 0);
    lv_obj_set_flex_flow(clock_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock_row,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    /* 大号时间 */
    g_label_time = lv_label_create(clock_row);
    lv_label_set_text(g_label_time, "00:00");
    lv_obj_set_style_text_color(g_label_time, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_label_time, &lv_font_montserrat_44, 0);

    /* 星期 + 日期纵向容器 */
    lv_obj_t *date_col = lv_obj_create(clock_row);
    lv_obj_set_size(date_col, 80, 42);
    lv_obj_set_style_bg_opa(date_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_col, 0, 0);
    lv_obj_set_style_pad_all(date_col, 0, 0);
    lv_obj_set_flex_flow(date_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(date_col,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    g_label_weekday = lv_label_create(date_col);
    lv_label_set_text(g_label_weekday, "星期二");
    lv_obj_set_style_text_color(g_label_weekday, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_label_weekday, &lv_font_montserrat_14, 0);
    APPLY_ZH_FONT(g_label_weekday);

    g_label_date = lv_label_create(date_col);
    lv_label_set_text(g_label_date, "2026/6/17");
    lv_obj_set_style_text_color(g_label_date, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(g_label_date, &lv_font_montserrat_14, 0);

    /* ==== 天气行：定位 + 温度 + 天气 ==== */
    g_label_weather = lv_label_create(card);
    lv_label_set_text(g_label_weather, "番禺  26°  晴");
    lv_obj_align(g_label_weather, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_text_color(g_label_weather, lv_color_white(), 0);
    APPLY_ZH_18(g_label_weather);

    /* ---- 1 秒时钟 ---- */
    lv_timer_create(status_clock_refresh_cb, 1000, NULL);
    status_clock_refresh_cb(NULL);
}

/* 强制刷新时钟（时间同步后立即调用，无需等待 1 秒定时器） */
void car_ui_status_clock_force_refresh(void *user_data)
{
    (void)user_data;
    if (g_video_overlay_active) return;
    time_t   now = time(NULL);
    struct tm *tm = localtime(&now);
    lv_label_set_text_fmt(g_label_time, "%02d:%02d",
                          tm->tm_hour, tm->tm_min);
    lv_label_set_text(g_label_weekday, weekday_cn[tm->tm_wday]);
    lv_label_set_text_fmt(g_label_date, "%d/%d/%d",
                          tm->tm_year + 1900,
                          tm->tm_mon + 1,
                          tm->tm_mday);
}

void ui_refresh_status_labels(void *user_data)
{
    (void)user_data;
    /* 视频播放时 mplayer 直接写 framebuffer，LVGL 刷新会导致闪烁 */
    if (g_video_overlay_active) return;

    pthread_mutex_lock(&g_net_mutex);
    lv_label_set_text_fmt(g_label_weather, "%s  %d°  %s",
                          g_location_str[0] ? g_location_str : "番禺",
                          g_temp_celsius,
                          g_weather_str[0] ? g_weather_str : "晴");
    pthread_mutex_unlock(&g_net_mutex);
}
