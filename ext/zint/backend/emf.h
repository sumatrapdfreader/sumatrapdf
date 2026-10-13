/*  emf.h - header structure for Microsoft EMF */
/*
    libzint - the open source barcode library
    Copyright (C) 2016-2024 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef Z_EMF_H
#define Z_EMF_H

#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef OUT_USE_PRAGMA_PACK
#pragma pack(1)
#endif

    typedef struct rect_l {
        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;
    } OUT_PACK rect_l_t;

    typedef struct size_l {
        uint32_t cx;
        uint32_t cy;
    } OUT_PACK size_l_t;

    typedef struct point_l {
        int32_t x;
        int32_t y;
    } OUT_PACK point_l_t;

    typedef struct color_ref {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t reserved;
    } OUT_PACK color_ref_t;

    typedef struct log_brush_ex {
        uint32_t brush_style;
        color_ref_t color;
        uint32_t brush_hatch;
    } OUT_PACK log_brush_ex_t;

    typedef struct log_pen {
        uint32_t pen_style;
        point_l_t width;
        color_ref_t color_ref;
    } OUT_PACK log_pen_t;

    typedef struct log_font {
        int32_t height;
        int32_t width;
        int32_t escapement;
        int32_t orientation;
        int32_t weight;
        uint8_t italic;
        uint8_t underline;
        uint8_t strike_out;
        uint8_t char_set;
        uint8_t out_precision;
        uint8_t clip_precision;
        uint8_t quality;
        uint8_t pitch_and_family;
        unsigned char facename[64];
    } OUT_PACK log_font_t;

    typedef struct emr_text {
        point_l_t reference;
        uint32_t chars;
        uint32_t off_string;
        uint32_t options;
        rect_l_t rectangle;
        uint32_t off_dx;
    } OUT_PACK emr_text_t;

    typedef struct emf_header {
        rect_l_t bounds;
        rect_l_t frame;
        uint32_t record_signature;
        uint32_t version;
        uint32_t bytes;
        uint32_t records;
        uint16_t handles;
        uint16_t reserved;
        uint32_t n_description;
        uint32_t off_description;
        uint32_t n_pal_entries;
        size_l_t device;
        size_l_t millimeters;
        /* HeaderExtension1 Object */
        uint32_t cb_pixel_format;
        uint32_t off_pixel_format;
        uint32_t b_open_gl;
        /* HeaderExtension2 Object */
        size_l_t micrometers;
    } OUT_PACK emf_header_t;

    typedef struct emr_header {
        uint32_t type;
        uint32_t size;
        emf_header_t emf_header;
    } OUT_PACK emr_header_t;

    typedef struct emr_mapmode {
        uint32_t type;
        uint32_t size;
        uint32_t mapmode;
    } OUT_PACK emr_mapmode_t;

    typedef struct emr_setworldtransform {
        uint32_t type;
        uint32_t size;
        float m11;
        float m12;
        float m21;
        float m22;
        float dx;
        float dy;
    } OUT_PACK emr_setworldtransform_t;

    typedef struct emr_createbrushindirect {
        uint32_t type;
        uint32_t size;
        uint32_t ih_brush;
        log_brush_ex_t log_brush;
    } OUT_PACK emr_createbrushindirect_t;

    typedef struct emr_createpen {
        uint32_t type;
        uint32_t size;
        uint32_t ih_pen;
        log_pen_t log_pen;
    } OUT_PACK emr_createpen_t;

    typedef struct emr_selectobject {
        uint32_t type;
        uint32_t size;
        uint32_t ih_object;
    } OUT_PACK emr_selectobject_t;

    typedef struct emr_rectangle {
        uint32_t type;
        uint32_t size;
        rect_l_t box;
    } OUT_PACK emr_rectangle_t;

    typedef struct emr_ellipse {
        uint32_t type;
        uint32_t size;
        rect_l_t box;
    } OUT_PACK emr_ellipse_t;

    typedef struct emr_polygon {
        uint32_t type;
        uint32_t size;
        rect_l_t bounds;
        uint32_t count;
        point_l_t a_points_a;
        point_l_t a_points_b;
        point_l_t a_points_c;
        point_l_t a_points_d;
        point_l_t a_points_e;
        point_l_t a_points_f;
    } OUT_PACK emr_polygon_t;

    typedef struct emr_extcreatefontindirectw {
        uint32_t type;
        uint32_t size;
        uint32_t ih_fonts;
        log_font_t elw;
    } OUT_PACK emr_extcreatefontindirectw_t;

    typedef struct emr_settextalign {
        uint32_t type;
        uint32_t size;
        uint32_t text_alignment_mode;
    } OUT_PACK emr_settextalign_t;

    typedef struct emr_settextcolor {
        uint32_t type;
        uint32_t size;
        color_ref_t color;
    } OUT_PACK emr_settextcolor_t;

    typedef struct emr_exttextoutw {
        uint32_t type;
        uint32_t size;
        rect_l_t bounds;
        uint32_t i_graphics_mode;
        float ex_scale;
        float ey_scale;
        emr_text_t w_emr_text;
    } OUT_PACK emr_exttextoutw_t;

    typedef struct emr_eof {
        uint32_t type;
        uint32_t size;
        uint32_t n_pal_entries;
        uint32_t off_pal_entries;
        uint32_t size_last;
    } OUT_PACK emr_eof_t;

    typedef struct box {
        emr_rectangle_t top;
        emr_rectangle_t bottom;
        emr_rectangle_t left;
        emr_rectangle_t right;
    } OUT_PACK box_t;

#ifdef OUT_USE_PRAGMA_PACK
#pragma pack()
#endif

#ifdef  __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* Z_EMF_H */
