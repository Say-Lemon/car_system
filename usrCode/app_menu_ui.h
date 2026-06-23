/**
 * @file app_menu_ui.h
 * @brief 应用菜单选择界面
 */

#ifndef APP_MENU_UI_H
#define APP_MENU_UI_H

#include "lvgl/lvgl.h"

lv_obj_t *app_menu_ui_create(void);
void app_menu_ui_close(void);

#endif
