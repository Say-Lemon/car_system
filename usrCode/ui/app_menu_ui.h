/**
 * @file app_menu_ui.h
 * @brief 应用菜单 — 中间区域切换显示
 *
 * Phase 5: 改为与空调面板相同的 toggle 模式。
 *   按下侧边栏菜单键 → 中间区域切换为菜单 / 按下 → 回到仪表盘。
 */

#ifndef APP_MENU_UI_H
#define APP_MENU_UI_H

#include "lvgl/lvgl.h"

/** 创建菜单面板（初始隐藏），挂载到 parent */
void app_menu_ui_create(lv_obj_t *parent);

/** 切换仪表盘 ↔ 应用菜单可见性 */
void app_menu_ui_toggle(void);

/** 隐藏菜单（供 AC panel toggle 互斥调用） */
void app_menu_ui_hide(void);

#endif /* APP_MENU_UI_H */
