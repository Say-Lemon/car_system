/**
 * @file car_ui.c
 * @brief 主仪表盘编排器 — 在活跃屏幕上创建 4 区域无边框平铺布局
 *
 * 调用顺序：
 *   car_ui_create_dashboard()
 *     ├── 创建 screen-size 透明根容器（800×480）
 *     ├── car_ui_sidebar_create(root)      → 左侧栏
 *     ├── car_ui_dashboard_create(root)    → 中间仪表盘
 *     ├── car_ui_status_create(root)       → 右上状态
 *     └── car_ui_music_bar_create(root)    → 右下音乐栏
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

/* ========== 内部持有的区域容器句柄 ========== */
static lv_obj_t *sidebar_cont;
static lv_obj_t *center_cont;
static lv_obj_t *status_cont;
static lv_obj_t *music_bar_cont;

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

/* ========== 区域容器查询 ========== */

lv_obj_t *car_ui_get_sidebar_cont(void)
{
    return sidebar_cont;
}

lv_obj_t *car_ui_get_center_cont(void)
{
    return center_cont;
}

lv_obj_t *car_ui_get_status_cont(void)
{
    return status_cont;
}

lv_obj_t *car_ui_get_music_bar_cont(void)
{
    return music_bar_cont;
}
