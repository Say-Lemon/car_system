/*******************************************************************************
 * Size: 26 px
 * Bpp: 4
 * Opts: --size 26 --bpp 4 --format lvgl --font C:/Windows/Fonts/seguisym.ttf -r 0x2744 -o usrCode/font_snowflake_26.c --lv-font-name snowflake_26 --no-compress --no-prefilter
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef SNOWFLAKE_26
#define SNOWFLAKE_26 1
#endif

#if SNOWFLAKE_26

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+2744 "❄" */
    0x0, 0x0, 0x0, 0x0, 0xb, 0x10, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x7, 0x60, 0xf1, 0x58,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3e, 0x5f,
    0x6f, 0x40, 0x0, 0x0, 0x0, 0x0, 0x79, 0x0,
    0x3e, 0xff, 0x40, 0x8, 0x80, 0x0, 0x3, 0x13,
    0xd0, 0x0, 0x3f, 0x40, 0x0, 0xc4, 0x3, 0x0,
    0x8e, 0x7f, 0x10, 0x0, 0xf1, 0x0, 0xf, 0x7e,
    0xa0, 0x0, 0x3d, 0xf7, 0x0, 0xf, 0x0, 0x5,
    0xfd, 0x40, 0x0, 0x9d, 0xfb, 0xea, 0x10, 0xf0,
    0x19, 0xeb, 0xfd, 0x91, 0x8, 0x40, 0x0, 0x8e,
    0x7f, 0x6e, 0x81, 0x0, 0x38, 0x10, 0x0, 0x0,
    0x0, 0x1c, 0xfd, 0x20, 0x0, 0x0, 0x0, 0x2,
    0x0, 0x0, 0x2b, 0xdf, 0xcc, 0x30, 0x0, 0x2,
    0x1, 0xee, 0xa6, 0x9e, 0x60, 0xf0, 0x6e, 0xa5,
    0x9e, 0xe2, 0x0, 0x3d, 0xfb, 0x10, 0xf, 0x0,
    0x1a, 0xfd, 0x30, 0x0, 0x4d, 0xcf, 0x30, 0x0,
    0xf1, 0x0, 0x2f, 0xce, 0x50, 0x7, 0x62, 0xf0,
    0x0, 0xf, 0x20, 0x0, 0xd3, 0x58, 0x0, 0x0,
    0x6b, 0x0, 0xb, 0xfc, 0x10, 0x9, 0x70, 0x0,
    0x0, 0x2, 0x30, 0xb, 0xaf, 0xac, 0x10, 0x33,
    0x0, 0x0, 0x0, 0x0, 0x8, 0xb0, 0xf1, 0x9a,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf,
    0x10, 0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x10, 0x0, 0x0, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 369, .box_w = 19, .box_h = 20, .ofs_x = 2, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 10052, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t snowflake_26 = {
#else
lv_font_t snowflake_26 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 20,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 2,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if SNOWFLAKE_26*/

