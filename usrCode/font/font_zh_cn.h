/**
 * @file font_zh_cn.h
 * @brief 多尺寸中文 + 图标字体（14/16/18px）
 *
 *   zh_cn_14  — 14px  小字中文（歌手名等）
 *   zh_cn_font — 16px  默认中文
 *   zh_cn_18  — 18px  大号中文（歌名等）
 */

#ifndef FONT_ZH_CN_H
#define FONT_ZH_CN_H

#include "lvgl/lvgl.h"

#define HAS_ZH_CN_FONT

/* 14px */
LV_FONT_DECLARE(zh_cn_14);
#define APPLY_ZH_14(obj)  lv_obj_set_style_text_font((obj), &zh_cn_14, 0)

/* 16px（默认） */
LV_FONT_DECLARE(zh_cn_font);
#define APPLY_ZH_FONT(obj)  lv_obj_set_style_text_font((obj), &zh_cn_font, 0)

/* 18px */
LV_FONT_DECLARE(font_zh_cn_18);
#define APPLY_ZH_18(obj)  lv_obj_set_style_text_font((obj), &font_zh_cn_18, 0)

#endif /* FONT_ZH_CN_H */
