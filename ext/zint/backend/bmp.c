/* bmp.c - Handles output to Windows Bitmap file */
/*
    libzint - the open source barcode library
    Copyright (C) 2009-2024 Robin Stuart <rstuart114@gmail.com>

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

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "filemem.h"
#include "output.h"
#include "bmp.h"        /* Bitmap header structure */

INTERNAL int bmp_pixel_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf) {
    int i, row, column;
    int bits_per_pixel;
    int colour_count;
    int resolution;
    size_t row_size, data_offset, file_size;
    struct filemem fm;
    struct filemem *const fmp = &fm;
    bitmap_file_header_t file_header;
    bitmap_info_header_t info_header;
    color_ref_t bg;
    color_ref_t fg;
    color_ref_t palette[8];
    int ultra_fg_index = 9;
    unsigned char map[128];
    unsigned char *rowbuf;

    (void) out_colour_get_rgb(symbol->fgcolour, &fg.red, &fg.green, &fg.blue, NULL /*alpha*/);
    fg.reserved = 0x00;

    (void) out_colour_get_rgb(symbol->bgcolour, &bg.red, &bg.green, &bg.blue, NULL /*alpha*/);
    bg.reserved = 0x00;

    if (symbol->symbology == BARCODE_ULTRA) {
        static const unsigned char ultra_chars[8] = { 'C', 'B', 'M', 'R', 'Y', 'G', 'K', 'W' };
        for (i = 0; i < 8; i++) {
            out_colour_char_to_rgb(ultra_chars[i], &palette[i].red, &palette[i].green, &palette[i].blue);
            palette[i].reserved = 0x00;
            if (memcmp(&palette[i], &fg, sizeof(fg)) == 0) {
                ultra_fg_index = i + 1;
            }
            map[ultra_chars[i]] = i + 1;
        }
        bits_per_pixel = 4;
        colour_count = ultra_fg_index == 9 ? 10 : 9;
        map['0'] = 0;
        map['1'] = ultra_fg_index;
    } else {
        bits_per_pixel = 1;
        colour_count = 2;
        map['0'] = 0;
        map['1'] = 0x80;
    }
    row_size = 4 * (((size_t) symbol->bitmap_width * bits_per_pixel + 31) / 32);
    data_offset = sizeof(bitmap_file_header_t) + sizeof(bitmap_info_header_t);
    data_offset += sizeof(color_ref_t) * colour_count;
    file_size = data_offset + row_size * symbol->bitmap_height;

    /* Must fit in `uint32_t` field in header */
    if (file_size != (uint32_t) file_size) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 606, "Output size too large for file size field of BMP header");
    }

    if (!(rowbuf = (unsigned char *) malloc(row_size))) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 602, "Insufficient memory for BMP row buffer");
    }

    out_le_u16(file_header.header_field, 0x4d42); /* "BM" */
    out_le_u32(file_header.file_size, file_size);
    out_le_u32(file_header.reserved, 0);
    out_le_u32(file_header.data_offset, data_offset);

    out_le_u32(info_header.header_size, sizeof(bitmap_info_header_t));
    out_le_i32(info_header.width, symbol->bitmap_width);
    out_le_i32(info_header.height, symbol->bitmap_height);
    out_le_u16(info_header.colour_planes, 1);
    out_le_u16(info_header.bits_per_pixel, bits_per_pixel);
    out_le_u32(info_header.compression_method, 0); /* BI_RGB */
    out_le_u32(info_header.image_size, 0);
    resolution = symbol->dpmm ? (int) roundf(stripf(symbol->dpmm * 1000.0f)) : 0; /* pixels per metre */
    out_le_i32(info_header.horiz_res, resolution);
    out_le_i32(info_header.vert_res, resolution);
    out_le_u32(info_header.colours, colour_count);
    out_le_u32(info_header.important_colours, colour_count);

    /* Open output file in binary mode */
    if (!fm_open(fmp, symbol, "wb")) {
        errtxtf(0, symbol, 601, "Could not open BMP output file (%1$d: %2$s)", fmp->err, strerror(fmp->err));
        free(rowbuf);
        return ZINT_ERROR_FILE_ACCESS;
    }

    fm_write(&file_header, sizeof(bitmap_file_header_t), 1, fmp);
    fm_write(&info_header, sizeof(bitmap_info_header_t), 1, fmp);

    fm_write(&bg, sizeof(color_ref_t), 1, fmp);
    if (bits_per_pixel == 4) {
        for (i = 0; i < 8; i++) {
            fm_write(&palette[i], sizeof(color_ref_t), 1, fmp);
        }
        if (ultra_fg_index == 9) {
            fm_write(&fg, sizeof(color_ref_t), 1, fmp);
        }
    } else {
        fm_write(&fg, sizeof(color_ref_t), 1, fmp);
    }

    /* Pixel Plotting */
    if (bits_per_pixel == 4) {
        for (row = 0; row < symbol->bitmap_height; row++) {
            const unsigned char *pb = pixelbuf + ((size_t) symbol->bitmap_width * (symbol->bitmap_height - row - 1));
            memset(rowbuf, 0, row_size);
            for (column = 0; column < symbol->bitmap_width; column++) {
                rowbuf[column >> 1] |= map[pb[column]] << (!(column & 1) << 2);
            }
            fm_write(rowbuf, 1, row_size, fmp);
        }
    } else { /* bits_per_pixel == 1 */
        for (row = 0; row < symbol->bitmap_height; row++) {
            const unsigned char *pb = pixelbuf + ((size_t) symbol->bitmap_width * (symbol->bitmap_height - row - 1));
            memset(rowbuf, 0, row_size);
            for (column = 0; column < symbol->bitmap_width; column++) {
                rowbuf[column >> 3] |= map[pb[column]] >> (column & 7);
            }
            fm_write(rowbuf, 1, row_size, fmp);
        }
    }
    free(rowbuf);

    if (fm_error(fmp)) {
        errtxtf(0, symbol, 603, "Incomplete write of BMP output (%1$d: %2$s)", fmp->err, strerror(fmp->err));
        (void) fm_close(fmp, symbol);
        return ZINT_ERROR_FILE_WRITE;
    }

    if (!fm_close(fmp, symbol)) {
        return errtxtf(ZINT_ERROR_FILE_WRITE, symbol, 605, "Failure on closing BMP output file (%1$d: %2$s)",
                        fmp->err, strerror(fmp->err));
    }

    return 0;
}

/* vim: set ts=4 sw=4 et : */
