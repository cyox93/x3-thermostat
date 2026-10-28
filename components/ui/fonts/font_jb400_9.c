/*******************************************************************************
 * Size: 9 px
 * Bpp: 4
 * Opts: --font src/JetBrainsMono-Regular.ttf --size 9 --bpp 4 --format lvgl --no-compress --symbols 0123456789/OFN --lv-font-name font_jb400_9 -o ../../components/ui/fonts/font_jb400_9.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef FONT_JB400_9
#define FONT_JB400_9 1
#endif

#if FONT_JB400_9

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+002F "/" */
    0x0, 0x1, 0x50, 0x0, 0x85, 0x0, 0xc, 0x0,
    0x4, 0x90, 0x0, 0xa3, 0x0, 0xc, 0x0, 0x6,
    0x70, 0x0, 0xc1, 0x0, 0x2b, 0x0, 0x0,

    /* U+0030 "0" */
    0x9, 0xbb, 0x23, 0x90, 0x39, 0x48, 0x1, 0xb4,
    0x8b, 0x4b, 0x48, 0x1, 0xb3, 0x90, 0x39, 0x9,
    0xbb, 0x20,

    /* U+0031 "1" */
    0x5, 0xe6, 0x2, 0x96, 0x60, 0x0, 0x66, 0x0,
    0x6, 0x60, 0x0, 0x66, 0x0, 0x6, 0x60, 0x2b,
    0xdd, 0xa0,

    /* U+0032 "2" */
    0x9, 0xbb, 0x24, 0x90, 0x48, 0x0, 0x4, 0x80,
    0x0, 0xc2, 0x0, 0xb5, 0x0, 0x97, 0x0, 0x2f,
    0xbb, 0x80,

    /* U+0033 "3" */
    0x1b, 0xbe, 0x60, 0x1, 0xc0, 0x0, 0xa3, 0x0,
    0xd, 0xa1, 0x0, 0x5, 0x83, 0x60, 0x48, 0xa,
    0xbb, 0x20,

    /* U+0034 "4" */
    0x0, 0xc, 0x0, 0x8, 0x50, 0x2, 0xc0, 0x0,
    0xb3, 0x36, 0x3a, 0x5, 0x83, 0xcb, 0xc8, 0x0,
    0x5, 0x80,

    /* U+0035 "5" */
    0x1e, 0xbb, 0x41, 0xb0, 0x0, 0x1d, 0xab, 0x10,
    0x80, 0x58, 0x0, 0x2, 0xa1, 0x70, 0x49, 0x9,
    0xbc, 0x20,

    /* U+0036 "6" */
    0x0, 0x77, 0x0, 0x1e, 0x0, 0xa, 0x70, 0x2,
    0xf9, 0xb2, 0x59, 0x3, 0xb4, 0x80, 0x2b, 0x9,
    0xbc, 0x30,

    /* U+0037 "7" */
    0x4e, 0xbb, 0xc3, 0x60, 0x48, 0x0, 0xa, 0x20,
    0x0, 0xc0, 0x0, 0x67, 0x0, 0xc, 0x10, 0x2,
    0xb0, 0x0,

    /* U+0038 "8" */
    0x9, 0xbc, 0x21, 0xb0, 0x58, 0xc, 0x29, 0x40,
    0x6e, 0xc0, 0x2b, 0x5, 0x83, 0x90, 0x3a, 0xa,
    0xbc, 0x30,

    /* U+0039 "9" */
    0x9, 0xbc, 0x23, 0xa0, 0x3a, 0x67, 0x0, 0xc3,
    0xb0, 0x48, 0x8, 0xad, 0x10, 0x4, 0x70, 0x1,
    0xb0, 0x0,

    /* U+0046 "F" */
    0x2e, 0xbb, 0x82, 0xa0, 0x0, 0x2a, 0x0, 0x2,
    0xeb, 0xb5, 0x2a, 0x0, 0x2, 0xa0, 0x0, 0x2a,
    0x0, 0x0,

    /* U+004E "N" */
    0x3f, 0x13, 0x93, 0xd6, 0x39, 0x39, 0xa3, 0x93,
    0x9a, 0x49, 0x39, 0x59, 0x93, 0x90, 0xd9, 0x39,
    0xa, 0x90,

    /* U+004F "O" */
    0x9, 0xbc, 0x22, 0xb0, 0x48, 0x39, 0x3, 0x93,
    0x90, 0x39, 0x39, 0x3, 0x92, 0xb0, 0x48, 0x9,
    0xcc, 0x20
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 86, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 23, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 41, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 59, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 95, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 113, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 167, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 185, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x8, 0x9
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 47, .range_length = 11, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 70, .range_length = 10, .glyph_id_start = 12,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 3, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
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
    .cmap_num = 2,
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
const lv_font_t font_jb400_9 = {
#else
lv_font_t font_jb400_9 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 12,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FONT_JB400_9*/

