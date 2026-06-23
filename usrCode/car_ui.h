/**
 * @file car_ui.h
 * @brief 主仪表盘界面 — 4 区域布局编排器
 *
 * 职责：
 *   - 在 lv_scr_act() 上创建无边框根容器 (800×480)
 *   - 调用各子模块创建函数组装左侧栏、中间仪表盘、右上状态、右下音乐栏
 *   - 暴露各区域容器指针供后续阶段在外部附着功能
 */

#ifndef CAR_UI_H
#define CAR_UI_H

#include "lvgl/lvgl.h"

/** 创建完整仪表盘（在活跃屏幕上直接构建） */
void car_ui_create_dashboard(void);

/** 返回四个区域容器的句柄（供未来模块或 app_manager 使用） */
lv_obj_t *car_ui_get_sidebar_cont(void);
lv_obj_t *car_ui_get_center_cont(void);
lv_obj_t *car_ui_get_status_cont(void);
lv_obj_t *car_ui_get_music_bar_cont(void);

#endif /* CAR_UI_H */
