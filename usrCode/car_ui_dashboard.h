/**
 * @file car_ui_dashboard.h
 * @brief 中间仪表盘 — 汽车图片区域 + 车速/油量/转速标签
 *
 * Phase 1: 静态占位显示
 * Phase 4: 由 CAN 模拟线程动态更新数值
 */

#ifndef CAR_UI_DASHBOARD_H
#define CAR_UI_DASHBOARD_H

#include "lvgl/lvgl.h"

/** 在 parent 内创建中间仪表盘区域（包含 CAN 数据刷新定时器） */
void car_ui_dashboard_create(lv_obj_t *parent);

/** 数据标签句柄（Phase 4 外部更新） */
extern lv_obj_t *g_label_speed;
extern lv_obj_t *g_label_speed_unit;  /* KM/H 单位标签 */
extern lv_obj_t *g_label_fuel;
extern lv_obj_t *g_label_volume;
extern lv_obj_t *g_label_rpm;

/** 更新音量显示（侧边栏 Vol+/Vol- 调用） */
void car_ui_dashboard_update_volume(int vol);

/** Phase 4：仪表盘数据实时更新（由 CAN 刷新定时器调用） */
void dashboard_update_speed(int kmh);
void dashboard_update_fuel(int pct);
void dashboard_update_rpm(int rpm);

#endif /* CAR_UI_DASHBOARD_H */
