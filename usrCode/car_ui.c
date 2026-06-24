/**
 * @file car_ui.c
 * @brief 主仪表盘编排器 — 在活跃屏幕上创建 4 区域无边框平铺布局
 *
 * 调用顺序：
 *   car_ui_create_dashboard()
 *     ├── 设置屏幕背景色（0x0A0A1A）
 *     ├── car_ui_sidebar_create(scr)        → 左侧栏 70×460
 *     ├── car_ui_dashboard_create(scr)      → 中间仪表盘 440×460
 *     ├── car_ui_ac_panel_create(scr)       → 空调面板（初始隐藏）
 *     ├── app_menu_ui_create(scr)           → 应用菜单（初始隐藏）
 *     ├── car_ui_status_create(scr)         → 右上状态 250×140
 *     └── car_ui_music_bar_create(scr)      → 右下音乐栏 250×310
 *
 * 各子模块自行决定内部容器位置（绝对坐标），编排器只负责提供父容器。
 */

#include "car_ui.h"
#include "car_ui_sidebar.h"
#include "car_ui_dashboard.h"
#include "car_ui_status.h"
#include "car_ui_music_bar.h"
#include "car_ui_ac_panel.h"
#include "app_menu_ui.h"
#include "app_config.h"
#include "lvgl/lvgl.h"

/* ========== 创建仪表盘 ========== */
void car_ui_create_dashboard(void)
{
    /* 获取当前活跃屏幕 */
    lv_obj_t *scr = lv_scr_act();

    /* 设置屏幕背景色（全黑基底） */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A1A), 0);

    /* 创建各子区域（它们自身会设置位置和尺寸） */
    car_ui_sidebar_create(scr);
    car_ui_dashboard_create(scr);
    car_ui_ac_panel_create(scr);   /* 空调面板——初始隐藏 */
    app_menu_ui_create(scr);       /* 应用菜单——初始隐藏 */
    car_ui_status_create(scr);
    car_ui_music_bar_create(scr);
}

