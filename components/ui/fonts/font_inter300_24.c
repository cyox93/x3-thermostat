/*******************************************************************************
 * Size: 24 px
 * Bpp: 4
 * Opts: --font src/InterTight-Light.ttf --size 24 --bpp 4 --format lvgl --no-compress --symbols % --lv-font-name font_inter300_24 -o ../../components/ui/fonts/font_inter300_24.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef FONT_INTER300_24
#define FONT_INTER300_24 1
#endif

#if FONT_INTER300_24

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0025 "%" */
    0x2, 0xbf, 0xe9, 0x0, 0x0, 0x0, 0x1e, 0x80,
    0xe, 0xb3, 0x4e, 0x90, 0x0, 0x0, 0xad, 0x0,
    0x4f, 0x10, 0x6, 0xf0, 0x0, 0x6, 0xf2, 0x0,
    0x6f, 0x0, 0x5, 0xf0, 0x0, 0x2f, 0x60, 0x0,
    0x5f, 0x0, 0x6, 0xf0, 0x0, 0xcb, 0x0, 0x0,
    0x1f, 0x80, 0x1d, 0xa0, 0x7, 0xe1, 0x0, 0x0,
    0x4, 0xef, 0xfb, 0x10, 0x3f, 0x50, 0x0, 0x0,
    0x0, 0x2, 0x10, 0x0, 0xd9, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x9, 0xd0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x5f, 0x30, 0x3c, 0xfd, 0x50,
    0x0, 0x0, 0x1, 0xe7, 0x1, 0xe9, 0x36, 0xf4,
    0x0, 0x0, 0xb, 0xc0, 0x7, 0xe0, 0x0, 0xba,
    0x0, 0x0, 0x6f, 0x20, 0x9, 0xc0, 0x0, 0x8d,
    0x0, 0x2, 0xf6, 0x0, 0x9, 0xc0, 0x0, 0x8d,
    0x0, 0xc, 0xa0, 0x0, 0x7, 0xe0, 0x0, 0xab,
    0x0, 0x8e, 0x10, 0x0, 0x1, 0xf9, 0x26, 0xf4,
    0x4, 0xf4, 0x0, 0x0, 0x0, 0x3c, 0xfe, 0x60
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 295, .box_w = 16, .box_h = 17, .ofs_x = 1, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 37, .range_length = 1, .glyph_id_start = 1,
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
const lv_font_t font_inter300_24 = {
#else
lv_font_t font_inter300_24 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 29,          /*The maximum line height required by the font*/
    .base_line = 6,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -4,
    .underline_thickness = 2,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FONT_INTER300_24*/

