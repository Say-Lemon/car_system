/**
 * @file car_ui_sidebar.h
 * @brief 左侧图标栏 — 5 个圆形触控按钮（白色线性图标 + 黑色圆形底）
 *
 *   ⏻ U+E7E8 — 电源开关 (Segoe MDL2, 20px icons_20)
 *   🔊 U+E995 — 音量加   (Segoe MDL2, 20px icons_20)
 *   🔈 U+E993 — 音量减   (Segoe MDL2, 20px icons_20)
 *   ❄ U+2744 — 空调/雪花 (Segoe UI Symbol, 26px snowflake_26)
 *   ☰ U+F00B — 菜单列表 (FontAwesome via simsun, 16px)
 */

#ifndef CAR_UI_SIDEBAR_H
#define CAR_UI_SIDEBAR_H

#include "lvgl/lvgl.h"

/** 在 parent 内创建左侧图标栏 */
void car_ui_sidebar_create(lv_obj_t *parent);

/** 圆形按钮句柄（供外部绑定附加事件） */
extern lv_obj_t *g_btn_power;
extern lv_obj_t *g_btn_vol_up;
extern lv_obj_t *g_btn_vol_down;
extern lv_obj_t *g_btn_ac;
extern lv_obj_t *g_btn_app_menu;

#endif /* CAR_UI_SIDEBAR_H */
