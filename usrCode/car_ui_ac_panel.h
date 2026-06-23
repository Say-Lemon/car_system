/**
 * @file car_ui_ac_panel.h
 * @brief 空调控制面板 — 温度/风量滑块 + 模式选择
 *
 * Phase 5: 占据中间仪表盘区域（440×460），通过侧边栏 ❄ 按钮切换显示。
 */

#ifndef CAR_UI_AC_PANEL_H
#define CAR_UI_AC_PANEL_H

#include "lvgl/lvgl.h"

/** 创建空调面板（初始隐藏），挂载到 parent */
void car_ui_ac_panel_create(lv_obj_t *parent);

/** 切换仪表盘 ↔ 空调面板可见性 */
void car_ui_ac_panel_toggle(void);

/** 隐藏空调面板（供 menu toggle 互斥调用） */
void car_ui_ac_panel_hide(void);

#endif /* CAR_UI_AC_PANEL_H */
