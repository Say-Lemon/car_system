/**
 * @file car_ui_status.h
 * @brief 右上状态栏 — 日期/星期/时间/天气/定位
 *
 * 内含 1 秒 lv_timer 自动刷新时钟。
 * Phase 6: 天气与定位标签由网络线程异步更新。
 */

#ifndef CAR_UI_STATUS_H
#define CAR_UI_STATUS_H

#include "lvgl/lvgl.h"

/** 在 parent 内创建状态栏并启动时钟定时器 */
void car_ui_status_create(lv_obj_t *parent);

/** 各标签句柄 */
extern lv_obj_t *g_label_date;
extern lv_obj_t *g_label_weekday;
extern lv_obj_t *g_label_time;
extern lv_obj_t *g_label_weather;

/** lv_async_call 回调：刷新天气/定位标签（Phase 6 网络线程调用） */
void ui_refresh_status_labels(void *user_data);

/** 强制立即刷新时钟（时间同步后调用） */
void car_ui_status_clock_force_refresh(void *user_data);

#endif /* CAR_UI_STATUS_H */
