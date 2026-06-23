/**
 * @file font_icons_20.h
 * @brief 侧边栏 20px 图标字体（Segoe MDL2 + Segoe UI Symbol）
 *
 * 比 zh_cn_font (16px) 更大，专用于侧边栏圆形按钮图标，
 * 使 5 个图标在 56px 圆形区域内视觉大小统一。
 */

#ifndef FONT_ICONS_20_H
#define FONT_ICONS_20_H

#include "lvgl/lvgl.h"

/** 声明 20px 图标字体 */
LV_FONT_DECLARE(icons_20);

/** 为控件设置 20px 图标字体 */
#define APPLY_ICON_FONT(obj) \
    lv_obj_set_style_text_font((obj), &icons_20, 0)

#endif /* FONT_ICONS_20_H */
