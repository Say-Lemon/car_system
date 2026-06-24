/**
 * @file car_ui.h
 * @brief 主仪表盘界面 — 4 区域布局编排器
 *
 * 职责：
 *   - 在 lv_scr_act() 上设置背景色并调用各子模块创建函数
 *   - 组装侧边栏、中间区域（仪表盘/空调/菜单三视图切换）、右侧状态+音乐栏
 *   - 各子模块自行管理容器位置和可见性，通过各自头文件交互
 */

#ifndef CAR_UI_H
#define CAR_UI_H

#include "lvgl/lvgl.h"

/** 创建完整仪表盘（在活跃屏幕上直接构建） */
void car_ui_create_dashboard(void);


#endif /* CAR_UI_H */
