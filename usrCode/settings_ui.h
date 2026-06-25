/**
 * @file settings_ui.h
 * @brief 系统设置面板 — 音量/亮度/熄屏/蓝牙名称
 *
 * Phase 7: AC 面板 toggle 模式。从应用菜单进入，返回键回到菜单。
 */

#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include "lvgl/lvgl.h"

/** 创建设置面板（初始隐藏），挂载到 parent */
void settings_ui_create(lv_obj_t *parent);

/** 打开设置面板（从应用菜单进入） */
void settings_ui_open(void);

/** 关闭设置面板，返回上一级 */
void settings_ui_close(void);

/** 强制隐藏（供其他面板互斥调用） */
void settings_ui_hide(void);

/** 应用当前亮度设置到硬件背光 */
void settings_backlight_apply(void);

#endif
