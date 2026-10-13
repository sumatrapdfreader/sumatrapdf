/* raster.c - Handles output to raster files */
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

#include <assert.h>
#include <math.h>

#ifdef _MSC_VER
#include <fcntl.h>
#include <io.h>
#endif /* _MSC_VER */

#include "common.h"
#include "output.h"
#include "zfiletypes.h"

#include "raster_font.h" /* Font for human readable text */

#define DEFAULT_INK     '1' /* Black */
#define DEFAULT_PAPER   '0' /* White */

/* Flags for `draw_string()`/`draw_letter()` */
#define ZFONT_HALIGN_CENTRE 0
#define ZFONT_HALIGN_LEFT   1
#define ZFONT_HALIGN_RIGHT  2
#define ZFONT_UPCEAN_TEXT   4   /* Helper flag to indicate dealing with EAN/UPC */

#ifndef ZINT_NO_PNG
INTERNAL int png_pixel_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf);
#endif /* ZINT_NO_PNG */
INTERNAL int bmp_pixel_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf);
INTERNAL int pcx_pixel_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf);
INTERNAL int gif_pixel_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf);
INTERNAL int tif_pixel_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf);

static const char ultra_colour[] = "0CBMRYGKW";

/* Wrapper to pre-check `size` on `malloc()` isn't too big (`prev_size` given if doing 2nd `malloc()` in a row) */
static void *raster_malloc(size_t size, size_t prev_size) {
    /* Check for large image `malloc`s, which produce very large files most systems can't handle anyway */
    /* Also `malloc()` on Linux will (usually) succeed regardless of request, and then get untrappably killed on
       access by OOM killer if too much, so this is a crude mitigation */
    if (size + prev_size < size /*Overflow check*/ || size + prev_size > 0x40000000 /*1GB*/) {
        return NULL;
    }
    return malloc(size);
}

static int buffer_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf) {
    /* Place pixelbuffer into symbol */
    unsigned char alpha[2];
    unsigned char map[91][3] = {
        {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, /* 0x00-0F */
        {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, /* 0x10-1F */
        {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, /* 0x20-2F */
        {0} /*bg*/, {0} /*fg*/, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, /* 0-9 */
        {0}, {0}, {0}, {0}, {0}, {0}, {0}, /* :;<=>?@ */
        {0}, { 0, 0, 0xff } /*Blue*/, { 0, 0xff, 0xff } /*Cyan*/, {0}, {0}, {0}, { 0, 0xff, 0 } /*Green*/, /* A-G */
        {0}, {0}, {0}, { 0, 0, 0 } /*blacK*/, {0}, { 0xff, 0, 0xff } /*Magenta*/, {0}, /* H-N */
        {0}, {0}, {0}, { 0xff, 0, 0 } /*Red*/, {0}, {0}, {0}, {0}, /* O-V */
        { 0xff, 0xff, 0xff } /*White*/, {0}, { 0xff, 0xff, 0 } /*Yellow*/, {0} /* W-Z */
    };
    int row;
    int plot_alpha = 0;
    const size_t bm_bitmap_width = (size_t) symbol->bitmap_width * 3;
    const size_t bm_bitmap_size = bm_bitmap_width * symbol->bitmap_height;

    if (out_colour_get_rgb(symbol->fgcolour, &map[DEFAULT_INK][0], &map[DEFAULT_INK][1], &map[DEFAULT_INK][2],
            &alpha[0])) {
        plot_alpha = 1;
    }
    if (out_colour_get_rgb(symbol->bgcolour, &map[DEFAULT_PAPER][0], &map[DEFAULT_PAPER][1], &map[DEFAULT_PAPER][2],
            &alpha[1])) {
        plot_alpha = 1;
    }

    /* Free any previous bitmap */
    if (symbol->bitmap != NULL) {
        free(symbol->bitmap);
        symbol->bitmap = NULL;
    }
    if (symbol->alphamap != NULL) {
        free(symbol->alphamap);
        symbol->alphamap = NULL;
    }

    if (!(symbol->bitmap = (unsigned char *) raster_malloc(bm_bitmap_size, 0 /*prev_size*/))) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 661, "Insufficient memory for bitmap buffer");
    }

    if (plot_alpha) {
        const size_t alpha_size = (size_t) symbol->bitmap_width * symbol->bitmap_height;
        if (!(symbol->alphamap = (unsigned char *) raster_malloc(alpha_size, bm_bitmap_size))) {
            return errtxt(ZINT_ERROR_MEMORY, symbol, 662, "Insufficient memory for alphamap buffer");
        }
        for (row = 0; row < symbol->bitmap_height; row++) {
            size_t p = (size_t) symbol->bitmap_width * row;
            const unsigned char *pb = pixelbuf + p;
            unsigned char *bitmap = symbol->bitmap + p * 3;
            if (row && memcmp(pb, pb - symbol->bitmap_width, symbol->bitmap_width) == 0) {
                memcpy(bitmap, bitmap - bm_bitmap_width, bm_bitmap_width);
                memcpy(symbol->alphamap + p, symbol->alphamap + p - symbol->bitmap_width, symbol->bitmap_width);
            } else {
                const size_t pe = p + symbol->bitmap_width;
                for (; p < pe; p++, bitmap += 3) {
                    memcpy(bitmap, map[pixelbuf[p]], 3);
                    symbol->alphamap[p] = alpha[pixelbuf[p] == DEFAULT_PAPER];
                }
            }
        }
    } else {
        for (row = 0; row < symbol->bitmap_height; row++) {
            const size_t r = (size_t) symbol->bitmap_width * row;
            const unsigned char *pb = pixelbuf + r;
            unsigned char *bitmap = symbol->bitmap + r * 3;
            if (row && memcmp(pb, pb - symbol->bitmap_width, symbol->bitmap_width) == 0) {
                memcpy(bitmap, bitmap - bm_bitmap_width, bm_bitmap_width);
            } else {
                const unsigned char *const pbe = pb + symbol->bitmap_width;
                for (; pb < pbe; pb++, bitmap += 3) {
                    memcpy(bitmap, map[*pb], 3);
                }
            }
        }
    }

    return 0;
}

static int save_raster_image_to_file(struct zint_symbol *symbol, const int image_height, const int image_width,
            unsigned char *pixelbuf, int rotate_angle, const int file_type) {
    int error_number;
    int row, column;

    unsigned char *rotated_pixbuf = pixelbuf;

    /* Suppress clang-analyzer-core.UndefinedBinaryOperatorResult warning */
    assert(rotate_angle == 0 || rotate_angle == 90 || rotate_angle == 180 || rotate_angle == 270);
    switch (rotate_angle) {
        case 0:
        case 180:
            symbol->bitmap_width = image_width;
            symbol->bitmap_height = image_height;
            break;
        case 90:
        case 270:
            symbol->bitmap_width = image_height;
            symbol->bitmap_height = image_width;
            break;
    }

    if (rotate_angle) {
        if (!(rotated_pixbuf = (unsigned char *) raster_malloc((size_t) image_width * image_height,
                                                                0 /*prev_size*/))) {
            return errtxt(ZINT_ERROR_MEMORY, symbol, 650, "Insufficient memory for pixel buffer");
        }
    }

    /* Rotate image before plotting */
    switch (rotate_angle) {
        case 0: /* Plot the right way up */
            /* Nothing to do */
            break;
        case 90: /* Plot 90 degrees clockwise */
            for (row = 0; row < image_width; row++) {
                const size_t h_offset = (size_t) image_height * row;
                for (column = 0; column < image_height; column++) {
                    const size_t w_offset = (size_t) image_width * (image_height - column - 1);
                    rotated_pixbuf[h_offset + column] = *(pixelbuf + w_offset + row);
                }
            }
            break;
        case 180: /* Plot upside down */
            for (row = 0; row < image_height; row++) {
                const size_t w_offset = (size_t) image_width * row;
                const size_t wh_offset = (size_t) image_width * (image_height - row - 1);
                for (column = 0; column < image_width; column++) {
                    rotated_pixbuf[w_offset + column] = *(pixelbuf + wh_offset + (image_width - column - 1));
                }
            }
            break;
        case 270: /* Plot 90 degrees anti-clockwise */
            for (row = 0; row < image_width; row++) {
                const size_t h_offset = (size_t) image_height * row;
                for (column = 0; column < image_height; column++) {
                    const size_t w_offset = (size_t) image_width * column;
                    rotated_pixbuf[h_offset + column] = *(pixelbuf + w_offset + (image_width - row - 1));
                }
            }
            break;
    }

    switch (file_type) {
        case OUT_BUFFER:
            if (symbol->output_options & OUT_BUFFER_INTERMEDIATE) {
                if (symbol->bitmap != NULL) {
                    free(symbol->bitmap);
                    symbol->bitmap = NULL;
                }
                if (symbol->alphamap != NULL) {
                    free(symbol->alphamap);
                    symbol->alphamap = NULL;
                }
                symbol->bitmap = rotated_pixbuf;
                rotate_angle = 0; /* Suppress freeing buffer if rotated */
                error_number = 0;
            } else {
                error_number = buffer_plot(symbol, rotated_pixbuf);
            }
            break;
        case OUT_PNG_FILE:
#ifndef ZINT_NO_PNG
            error_number = png_pixel_plot(symbol, rotated_pixbuf);
#else
            error_number = ZINT_ERROR_INVALID_OPTION;
#endif
            break;
#if defined(__GNUC__) && !defined(__clang__) && defined(NDEBUG) && defined(ZINT_NO_PNG)
/* Suppress gcc warning ‘<unknown>’ may be used uninitialized - only when Release and ZINT_NO_PNG */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
        case OUT_PCX_FILE:
            error_number = pcx_pixel_plot(symbol, rotated_pixbuf);
            break;
        case OUT_GIF_FILE:
            error_number = gif_pixel_plot(symbol, rotated_pixbuf);
            break;
        case OUT_TIF_FILE:
            error_number = tif_pixel_plot(symbol, rotated_pixbuf);
            break;
        default:
            error_number = bmp_pixel_plot(symbol, rotated_pixbuf);
            break;
#if defined(__GNUC__) && !defined(__clang__) && defined(NDEBUG) && defined(ZINT_NO_PNG)
#pragma GCC diagnostic pop
#endif
    }

    if (rotate_angle) {
        free(rotated_pixbuf);
    }
    return error_number;
}

/* Helper to check point within bounds before setting */
static void draw_pt(unsigned char *buf, const int buf_width, const int buf_height,
            const int x, const int y, const int fill) {
    if (x >= 0 && x < buf_width && y >= 0 && y < buf_height) {
        buf[y * buf_width + x] = fill;
    }
}

/* Draw the first line of a bar, to be completed by `copy_bar_line()`; more performant than multiple `draw_bar()`s */
static void draw_bar_line(unsigned char *pixelbuf, const int xpos, const int xlen, const int ypos,
            const int image_width, const int fill) {
    unsigned char *pb = pixelbuf + ((size_t) image_width * ypos) + xpos;

    memset(pb, fill, xlen);
}

/* Fill out a bar code row by copying the first line (called after multiple `draw_bar_line()`s) */
static void copy_bar_line(unsigned char *pixelbuf, const int xpos, const int xlen, const int ypos, const int ylen,
            const int image_width, const int image_height) {
    int y;
    const int ye = ypos + ylen > image_height ? image_height : ypos + ylen; /* Defensive, should never happen */
    unsigned char *pb = pixelbuf + ((size_t) image_width * ypos) + xpos;

    assert(ypos + ylen <= image_height); /* Trigger assert if "should never happen" happens */

    for (y = ypos + 1; y < ye; y++) {
        memcpy(pixelbuf + ((size_t) image_width * y) + xpos, pb, xlen);
    }
}

/* Draw a rectangle */
static void draw_bar(unsigned char *pixelbuf, const int xpos, const int xlen, const int ypos, const int ylen,
            const int image_width, const int image_height, const int fill) {
    int y;
    const int ye = ypos + ylen > image_height ? image_height : ypos + ylen; /* Defensive, should never happen */
    unsigned char *pb = pixelbuf + ((size_t) image_width * ypos) + xpos;

    assert(ypos + ylen <= image_height); /* Trigger assert if "should never happen" happens */

    for (y = ypos; y < ye; y++, pb += image_width) {
        memset(pb, fill, xlen);
    }
}

/* Put a letter into a position */
static void draw_letter(unsigned char *pixelbuf, const unsigned char letter, int xposn, const int yposn,
            const int textflags, const int image_width, const int image_height, const int si) {
    int glyph_no;
    int x, y;
    int max_x, max_y;
    const raster_font_item *font_table;
    int bold = 0;
    unsigned glyph_mask;
    int font_y;
    int half_si;
    int odd_si;
    unsigned char *linePtr;
    int x_start = 0;

    if (letter < 33) {
        return;
    }

    if ((letter >= 127) && (letter < 161)) {
        return;
    }

    if (yposn < 0) { /* Allow xposn < 0, dealt with below */
        return;
    }

    half_si = si / 2;
    odd_si = si & 1;

    if (letter > 127) {
        glyph_no = letter - 67; /* 161 - (127 - 33) */
    } else {
        glyph_no = letter - 33;
    }

    if (textflags & ZFONT_UPCEAN_TEXT) { /* Needs to be before SMALL_TEXT check */
        /* No bold for UPCEAN */
        if (textflags & SMALL_TEXT) {
            font_table = upcean_small_font;
            max_x = UPCEAN_SMALL_FONT_WIDTH;
            max_y = UPCEAN_SMALL_FONT_HEIGHT;
        } else {
            font_table = upcean_font;
            max_x = UPCEAN_FONT_WIDTH;
            max_y = UPCEAN_FONT_HEIGHT;
        }
        glyph_no = letter - '0';
    } else if (textflags & SMALL_TEXT) { /* small font 5x9 */
        /* No bold for small */
        max_x = SMALL_FONT_WIDTH;
        max_y = SMALL_FONT_HEIGHT;
        font_table = small_font;
    } else if (textflags & BOLD_TEXT) { /* bold font -> regular font + 1 */
        max_x = NORMAL_FONT_WIDTH + 1;
        max_y = NORMAL_FONT_HEIGHT;
        font_table = ascii_font;
        bold = 1;
    } else { /* regular font 7x14 */
        max_x = NORMAL_FONT_WIDTH;
        max_y = NORMAL_FONT_HEIGHT;
        font_table = ascii_font;
    }
    glyph_mask = ((unsigned) 1) << (max_x - 1);
    font_y = glyph_no * max_y;

    if (xposn < 0) {
        x_start = -xposn;
        xposn = 0;
    }

    if (yposn + max_y > image_height) {
        max_y = image_height - yposn;
    }

    linePtr = pixelbuf + ((size_t) yposn * image_width) + xposn;
    for (y = 0; y < max_y; y++) {
        int x_si, y_si;
        unsigned char *pixelPtr = linePtr; /* Avoid warning */
        for (y_si = 0; y_si < half_si; y_si++) {
            int extra_dot = 0;
            unsigned char *const maxPtr = linePtr + image_width - xposn;
            pixelPtr = linePtr;
            for (x = x_start; x < max_x && pixelPtr < maxPtr; x++) {
                unsigned set = font_table[font_y + y] & (glyph_mask >> x);
                for (x_si = 0; x_si < half_si && pixelPtr < maxPtr; x_si++) {
                    if (set) {
                        *pixelPtr = DEFAULT_INK;
                        extra_dot = bold;
                    } else if (extra_dot) {
                        *pixelPtr = DEFAULT_INK;
                        extra_dot = 0;
                    }
                    pixelPtr++;
                }
                if (pixelPtr < maxPtr && odd_si && (x & 1)) {
                    if (set) {
                        *pixelPtr = DEFAULT_INK;
                    }
                    pixelPtr++;
                }
            }
            if (pixelPtr < maxPtr && extra_dot) {
                *pixelPtr++ = DEFAULT_INK;
            }
            linePtr += image_width;
        }
        if (odd_si && (y & 1)) {
            memcpy(linePtr, linePtr - image_width, pixelPtr - (linePtr - image_width));
            linePtr += image_width;
        }
    }
}

/* Plot a string into the pixel buffer */
static void draw_string(unsigned char *pixelbuf, const unsigned char input_string[], int length, const int xposn,
            const int yposn, const int textflags, const int image_width, const int image_height, const int si) {
    int i, string_left_hand, letter_width, letter_gap;
    const int half_si = si / 2, odd_si = si & 1;
    int x_incr;

    if (textflags & ZFONT_UPCEAN_TEXT) { /* Needs to be before SMALL_TEXT check */
        /* No bold for UPCEAN */
        letter_width = textflags & SMALL_TEXT ? UPCEAN_SMALL_FONT_WIDTH : UPCEAN_FONT_WIDTH;
        letter_gap = 4;
    } else if (textflags & SMALL_TEXT) { /* small font 5x9 */
        /* No bold for small */
        letter_width = SMALL_FONT_WIDTH;
        letter_gap = 0;
    } else if (textflags & BOLD_TEXT) { /* bold font -> width of the regular font + 1 extra dot + 1 extra space */
        letter_width = NORMAL_FONT_WIDTH + 1;
        letter_gap = 1;
    } else { /* regular font 7x15 */
        letter_width = NORMAL_FONT_WIDTH;
        letter_gap = 0;
    }
    letter_width += letter_gap;

    if (length == -1) {
        length = (int) ustrlen(input_string);
    }

    if (textflags & ZFONT_HALIGN_LEFT) {
        string_left_hand = xposn;
    } else if (textflags & ZFONT_HALIGN_RIGHT) {
        string_left_hand = xposn - ((letter_width * length - letter_gap) * half_si);
    } else {
        string_left_hand = xposn - (int) roundf(((letter_width * length - letter_gap) * half_si) / 2.0f);
    }
    if (odd_si) {
        string_left_hand -= (letter_width * length - letter_gap) / 4;
    }
    for (i = 0; i < length; i++) {
        x_incr = i * letter_width * half_si;
        if (odd_si) {
            x_incr += i * letter_width / 2;
        }
        draw_letter(pixelbuf, input_string[i], string_left_hand + x_incr, yposn, textflags, image_width, image_height,
                    si);
    }
}

/* Draw disc using x² + y² <= r² */
static void draw_circle(unsigned char *pixelbuf, const int image_width, const int image_height,
            const int x0, const int y0, const float radius, const char fill) {
    int x, y;
    const int radius_i = (int) radius;
    const int radius_squared = radius_i * radius_i;

    for (y = -radius_i; y <= radius_i; y++) {
        const int y_squared = y * y;
        for (x = -radius_i; x <= radius_i; x++) {
            if ((x * x) + y_squared <= radius_squared) {
                draw_pt(pixelbuf, image_width, image_height, x0 + x, y0 + y, fill);
            }
        }
    }
}

/* Helper for `draw_mp_circle()` to draw horizontal filler lines within disc */
static void draw_mp_circle_lines(unsigned char *pixelbuf, const int image_width, const int image_height,
            const int x0, const int y0, const int x, const int y, const int fill) {
    int i;
    for (i = x0 - x; i <= x0 + x; i++) {
        draw_pt(pixelbuf, image_width, image_height, i, y0 + y, fill); /* (-x, y) to (x, y) */
        draw_pt(pixelbuf, image_width, image_height, i, y0 - y, fill); /* (-x, -y) to (x, -y) */
    }
    for (i = x0 - y; i <= x0 + y; i++) {
        draw_pt(pixelbuf, image_width, image_height, i, y0 + x, fill); /* (-y, x) to (y, x) */
        draw_pt(pixelbuf, image_width, image_height, i, y0 - x, fill); /* (-y, -x) to (y, -x) */
    }
}

/* Draw disc using Midpoint Circle Algorithm. Using this for MaxiCode rather than `draw_circle()` because it gives a
 * flatter circumference with no single pixel peaks, similar to Figures J3 and J6 in ISO/IEC 16023:2000.
 * Taken from https://rosettacode.org/wiki/Bitmap/Midpoint_circle_algorithm#C
 * "Content is available under GNU Free Documentation License 1.2 unless otherwise noted."
 * https://www.gnu.org/licenses/old-licenses/fdl-1.2.html */
static void draw_mp_circle(unsigned char *pixelbuf, const int image_width, const int image_height,
            const int x0, const int y0, const int r, const int fill) {
    /* Using top RHS octant from (0, r) going clockwise, so fast direction is x (i.e. always incremented) */
    int f = 1 - r;
    int ddF_x = 0;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    draw_mp_circle_lines(pixelbuf, image_width, image_height, x0, y0, x, y, fill);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x + 1;
        draw_mp_circle_lines(pixelbuf, image_width, image_height, x0, y0, x, y, fill);
    }
}

/* Draw central bullseye finder in Maxicode symbols */
static void draw_bullseye(unsigned char *pixelbuf, const int image_width, const int image_height,
            const int hex_width, const int hex_height, const int hx_start, const int hx_end,
            const int hex_image_height, const int xoffset_si, const int yoffset_si) {

    /* ISO/IEC 16023:2000 4.11.4 and 4.2.1.1 */

    /* 14W right from leftmost centre = 14.5X */
    const int x = (int) (14.5f * hex_width - hx_start + xoffset_si);
    /* 16Y above bottom-most centre = halfway */
    const int y = (int) ceilf(hex_image_height / 2 + yoffset_si);

    const int r1 = (int) ceilf(hex_height / 2.0f); /* Inner diameter is hex_height (V) */
    /* Total finder diameter is 9X, so radial increment for 5 radii r2 to r6 is ((9X - r1) / 5) / 2 */
    int r_incr = ((hex_width * 9 - r1) / 5) / 2;
    /* Fudge increment to lessen overlapping of finder with top/bottom of hexagons due to rounding errors */
    if (r_incr > hx_end) {
        r_incr -= hx_end;
    }

    draw_mp_circle(pixelbuf, image_width, image_height, x, y, r1 + r_incr * 5, DEFAULT_INK);
    draw_mp_circle(pixelbuf, image_width, image_height, x, y, r1 + r_incr * 4, DEFAULT_PAPER);
    draw_mp_circle(pixelbuf, image_width, image_height, x, y, r1 + r_incr * 3, DEFAULT_INK);
    draw_mp_circle(pixelbuf, image_width, image_height, x, y, r1 + r_incr * 2, DEFAULT_PAPER);
    draw_mp_circle(pixelbuf, image_width, image_height, x, y, r1 + r_incr, DEFAULT_INK);
    draw_mp_circle(pixelbuf, image_width, image_height, x, y, r1, DEFAULT_PAPER);
}

/* Put a hexagon into the pixel buffer */
static void draw_hexagon(unsigned char *pixelbuf, const int image_width, const int image_height,
            const unsigned char *scaled_hexagon, const int hex_width, const int hex_height,
            const int xposn, const int yposn) {
    int i, j;

    for (i = 0; i < hex_height; i++) {
        for (j = 0; j < hex_width; j++) {
            if (scaled_hexagon[(i * hex_width) + j] == DEFAULT_INK) {
                draw_pt(pixelbuf, image_width, image_height, xposn + j, yposn + i, DEFAULT_INK);
            }
        }
    }
}

/* Bresenham's line algorithm https://en.wikipedia.org/wiki/Bresenham's_line_algorithm
 * Creative Commons Attribution-ShareAlike License
 * https://en.wikipedia.org/wiki/Wikipedia:Text_of_Creative_Commons_Attribution-ShareAlike_3.0_Unported_License */
static void plot_hexline(unsigned char *scaled_hexagon, const int hex_width, const int hex_height,
            int start_x, int start_y, const int end_x, const int end_y) {
    const int dx = abs(end_x - start_x);
    const int sx = start_x < end_x ? 1 : -1;
    const int dy = -abs(end_y - start_y);
    const int sy = start_y < end_y ? 1 : -1;
    int e_xy = dx + dy;

    for (;;) {
        int e2;
        draw_pt(scaled_hexagon, hex_width, hex_height, start_x, start_y, DEFAULT_INK);
        if (start_x == end_x && start_y == end_y) {
            break;
        }
        e2 = 2 * e_xy;
        if (e2 >= dy) { /* e_xy+e_x > 0 */
            e_xy += dy;
            start_x += sx;
        }
        if (e2 <= dx) { /* e_xy+e_y < 0 */
            e_xy += dx;
            start_y += sy;
        }
    }
}

/* Create a hexagon shape and fill it */
static void plot_hexagon(unsigned char *scaled_hexagon, const int hex_width, const int hex_height,
            const int hx_start, const int hy_start, const int hx_end, const int hy_end) {
    int line, i;
    int not_top;

    const int hx_width = hex_width - hx_start - hx_end;
    const int hy_height = hex_height - hx_start - hx_end;

    const int hx_width_odd = hx_width & 1;
    const int hy_height_odd = hy_height & 1;

    const int hx_radius = hx_width / 2;
    const int hy_radius = hy_height / 2;

    /* To ensure symmetry, draw top left quadrant first, then copy/flip to other quadrants */

    int start_y = hy_start + (hy_radius + 1) / 2;
    int end_x = hx_start + hx_radius;
    if (hx_radius > 2 && (hx_radius < 10 || !hx_width_odd)) {
        /* Line drawing matches examples in ISO/IEC 16023:2000 Annexe J if point just to the left of end midpoint */
        end_x--;
    }

    /* Plot line of top left quadrant */
    plot_hexline(scaled_hexagon, hex_width, hex_height, hx_start, start_y, end_x, hy_start);

    /* Fill to right */
    not_top = 0;
    for (line = hy_start; line < hy_start + hy_radius + hy_height_odd; line++) {
        int first = -1;
        for (i = hx_start; i < hx_start + hx_radius + hx_width_odd; i++) {
            if (first != -1) {
                scaled_hexagon[(hex_width * line) + i] = DEFAULT_INK;
                not_top = 1;
            } else if (scaled_hexagon[(hex_width * line) + i] == DEFAULT_INK) {
                first = i + 1;
            }
        }
        if (not_top && first == -1) { /* Fill empty lines at bottom */
            for (i = hx_start; i < hx_start + hx_radius + hx_width_odd; i++) {
                scaled_hexagon[(hex_width * line) + i] = DEFAULT_INK;
            }
        }
    }

    /* Copy left quadrant to right, flipping horizontally */
    for (line = hy_start; line < hy_start + hy_radius + hy_height_odd; line++) {
        for (i = hx_start; i < hx_start + hx_radius + hx_width_odd; i++) {
            if (scaled_hexagon[(hex_width * line) + i] == DEFAULT_INK) {
                scaled_hexagon[(hex_width * line) + hex_width - hx_end - (i - hx_start + 1)] = DEFAULT_INK;
            }
        }
    }

    /* Copy top to bottom, flipping vertically */
    for (line = hy_start; line < hy_start + hy_radius + hy_height_odd; line++) {
        for (i = hx_start; i < hex_width; i++) {
            if (scaled_hexagon[(hex_width * line) + i] == DEFAULT_INK) {
                scaled_hexagon[(hex_width * (hex_height - hy_end - (line - hy_start + 1))) + i] = DEFAULT_INK;
            }
        }
    }
}

/* Draw binding or box */
static void draw_bind_box(const struct zint_symbol *symbol, unsigned char *pixelbuf,
            const int xoffset_si, const int yoffset_si, const int symbol_height_si, const int dot_overspill_si,
            const int upceanflag, const int textoffset_si, const int image_width, const int image_height,
            const int si) {
    if (symbol->border_width > 0 && (symbol->output_options & (BARCODE_BOX | BARCODE_BIND | BARCODE_BIND_TOP))) {
        const int no_extend = symbol->symbology == BARCODE_CODABLOCKF || symbol->symbology == BARCODE_HIBC_BLOCKF
                                || symbol->symbology == BARCODE_DPD;
        const int horz_outside = is_fixed_ratio(symbol->symbology);
        const int bwidth_si = symbol->border_width * si;
        int ybind_top = yoffset_si - bwidth_si;
        int ybind_bot = yoffset_si + symbol_height_si + dot_overspill_si;
        if (horz_outside) {
            ybind_top = 0;
            ybind_bot = image_height - bwidth_si;
        } else if (upceanflag == 2 || upceanflag == 5) {
            ybind_top += textoffset_si;
            ybind_bot += textoffset_si;
        }
        /* Horizontal boundary bars */
        if ((symbol->output_options & BARCODE_BOX) || !no_extend) {
            /* Box or not CodaBlockF/DPD */
            draw_bar(pixelbuf, 0, image_width, ybind_top, bwidth_si, image_width, image_height, DEFAULT_INK);
            if (!(symbol->output_options & BARCODE_BIND_TOP)) { /* Trumps BARCODE_BOX & BARCODE_BIND */
                draw_bar(pixelbuf, 0, image_width, ybind_bot, bwidth_si, image_width, image_height, DEFAULT_INK);
            }
        } else {
            /* CodaBlockF/DPD bind - does not extend over horizontal whitespace */
            const int width_si = symbol->width * si;
            draw_bar(pixelbuf, xoffset_si, width_si, ybind_top, bwidth_si, image_width, image_height, DEFAULT_INK);
            if (!(symbol->output_options & BARCODE_BIND_TOP)) { /* Trumps BARCODE_BOX & BARCODE_BIND */
                draw_bar(pixelbuf, xoffset_si, width_si, ybind_bot, bwidth_si, image_width, image_height,
                            DEFAULT_INK);
            }
        }
        if (symbol->output_options & BARCODE_BOX) {
            /* Vertical side bars */
            const int xbox_right = image_width - bwidth_si;
            int box_top = yoffset_si;
            int box_height = symbol_height_si + dot_overspill_si;
            if (horz_outside) {
                box_top = bwidth_si;
                box_height = image_height - bwidth_si * 2;
            } else if (upceanflag == 2 || upceanflag == 5) {
                box_top += textoffset_si;
            }
            draw_bar(pixelbuf, 0, bwidth_si, box_top, box_height, image_width, image_height, DEFAULT_INK);
            draw_bar(pixelbuf, xbox_right, bwidth_si, box_top, box_height, image_width, image_height, DEFAULT_INK);
        }
    }
}

/* Plot a MaxiCode symbol with hexagons and bullseye */
static int plot_raster_maxicode(struct zint_symbol *symbol, const int rotate_angle, const int file_type) {
    int row, column;
    int image_height, image_width;
    size_t image_size;
    unsigned char *pixelbuf;
    int error_number;
    float xoffset, yoffset, roffset, boffset;
    float scaler = symbol->scale;
    unsigned char *scaled_hexagon;
    int hex_width, hex_height;
    size_t hex_size;
    int hx_start, hy_start, hx_end, hy_end;
    int hex_image_width, hex_image_height;
    int yposn_offset;
    int xoffset_si, yoffset_si, roffset_si, boffset_si;

    const float two_div_sqrt3 = 1.1547f; /* 2 / √3 */
    const float sqrt3_div_two = 0.866f; /* √3 / 2 == 1.5 / √3 */

    if (scaler < 0.2f) {
        scaler = 0.2f;
    }
    scaler *= 10.0f;

    out_set_whitespace_offsets(symbol, 0 /*hide_text*/, 0 /*comp_xoffset*/, &xoffset, &yoffset, &roffset, &boffset,
        NULL /*qz_right*/, scaler, &xoffset_si, &yoffset_si, &roffset_si, &boffset_si, NULL /*qz_right_si*/);

    hex_width = (int) roundf(scaler); /* Short diameter, X in ISO/IEC 16023:2000 Figure 8 (same as W) */
    hex_height = (int) roundf(scaler * two_div_sqrt3); /* Long diameter, V in Figure 8 */

    /* Allow for whitespace around each hexagon (see ISO/IEC 16023:2000 Annexe J.4)
       TODO: replace following kludge with proper calc of whitespace as per J.4 Steps 8 to 11 */
    hx_start = (int) (scaler < 3.5f ? 0.0f : ceilf(hex_width * 0.05f));
    hy_start = (int) ceilf(hex_height * 0.05f);
    hx_end = (int) roundf((hex_width - hx_start) * 0.05f);
    hy_end = (int) roundf((hex_height - hy_start) * 0.05f);

    /* The hexagons will be drawn within box (hex_width - hx_start - hx_end) x (hex_height - hy_start - hy_end)
       and plotted starting at (-hx_start, -hy_start) */

    hex_image_width = 30 * hex_width - hx_start - hx_end;
    /* `yposn_offset` is vertical distance between rows, Y in Figure 8 */
    /* TODO: replace following kludge with proper calc of hex_width/hex_height/yposn_offset as per J.4 Steps 1 to 7 */
    yposn_offset = (int) (scaler > 10.0f ? (sqrt3_div_two * hex_width) : roundf(sqrt3_div_two * hex_width));
    /* 32 rows drawn yposn_offset apart + final hexagon */
    hex_image_height = 32 * yposn_offset + hex_height - hy_start - hy_end;

    image_width = (int) ceilf(hex_image_width + xoffset_si + roffset_si);
    image_height = (int) ceilf(hex_image_height + yoffset_si + boffset_si);
    assert(image_width && image_height);
    image_size = (size_t) image_width * image_height;

    if (!(pixelbuf = (unsigned char *) raster_malloc(image_size, 0 /*prev_size*/))) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 655, "Insufficient memory for pixel buffer");
    }
    memset(pixelbuf, DEFAULT_PAPER, image_size);

    hex_size = (size_t) hex_width * hex_height;
    if (!(scaled_hexagon = (unsigned char *) raster_malloc(hex_size, image_size))) {
        free(pixelbuf);
        return errtxt(ZINT_ERROR_MEMORY, symbol, 656, "Insufficient memory for pixel buffer");
    }
    memset(scaled_hexagon, DEFAULT_PAPER, hex_size);

    plot_hexagon(scaled_hexagon, hex_width, hex_height, hx_start, hy_start, hx_end, hy_end);

    for (row = 0; row < symbol->rows; row++) {
        const int odd_row = row & 1; /* Odd (reduced) row, even (full) row */
        const int yposn = row * yposn_offset + yoffset_si - hy_start;
        const int xposn_offset = (odd_row ? hex_width / 2 : 0) + xoffset_si - hx_start;
        for (column = 0; column < symbol->width - odd_row; column++) {
            const int xposn = column * hex_width + xposn_offset;
            if (module_is_set(symbol, row, column)) {
                draw_hexagon(pixelbuf, image_width, image_height, scaled_hexagon, hex_width, hex_height, xposn,
                            yposn);
            }
        }
    }

    draw_bullseye(pixelbuf, image_width, image_height, hex_width, hex_height, hx_start, hx_end, hex_image_height,
                xoffset_si, yoffset_si);

    draw_bind_box(symbol, pixelbuf, xoffset_si, yoffset_si, hex_image_height, 0 /*dot_overspill_si*/,
                0 /*upceanflag*/, 0 /*textoffset_si*/, image_width, image_height, (int) scaler);

    error_number = save_raster_image_to_file(symbol, image_height, image_width, pixelbuf, rotate_angle, file_type);
    free(scaled_hexagon);
    if (rotate_angle || file_type != OUT_BUFFER || !(symbol->output_options & OUT_BUFFER_INTERMEDIATE)) {
        free(pixelbuf);
    }
    if (error_number == 0) {
        /* Check whether size is compliant */
        const float min_ratio = 0.92993629f; /* 24.82 / 26.69 */
        const float max_ratio = 1.177984f; /* 27.93 / 23.71 */
        const float size_ratio = (float) hex_image_width / hex_image_height;
        if (size_ratio < min_ratio || size_ratio > max_ratio) {
            return errtxt(ZINT_WARN_NONCOMPLIANT, symbol, 663, "Size not within the minimum/maximum ranges");
        }
    }
    return error_number;
}

static int plot_raster_dotty(struct zint_symbol *symbol, const int rotate_angle, const int file_type) {
    float scaler = 2 * symbol->scale;
    unsigned char *scaled_pixelbuf;
    int r, i;
    int scale_width, scale_height;
    size_t scale_size;
    int error_number = 0;
    float xoffset, yoffset, roffset, boffset;
    float dot_offset_s;
    float dot_radius_s;
    int dot_radius_si;
    int dot_overspill_si;
    int xoffset_si, yoffset_si, roffset_si, boffset_si;
    int symbol_height_si;

    if (scaler < 2.0f) {
        scaler = 2.0f;
    }
    symbol_height_si = (int) ceilf(symbol->height * scaler);
    dot_radius_s = (symbol->dot_size * scaler) / 2.0f;
    dot_radius_si = (int) dot_radius_s;

    out_set_whitespace_offsets(symbol, 0 /*hide_text*/, 0 /*comp_xoffset*/, &xoffset, &yoffset, &roffset, &boffset,
        NULL /*qz_right*/, scaler, &xoffset_si, &yoffset_si, &roffset_si, &boffset_si, NULL /*qz_right_si*/);

    /* TODO: Revisit this overspill stuff, it's hacky */
    if (symbol->dot_size < 1.0f) {
        dot_overspill_si = 0;
        /* Offset (1 - dot_size) / 2 + dot_radius == (1 - dot_size + dot_size) / 2 == 1 / 2 */
        dot_offset_s = scaler / 2.0f;
    } else { /* Allow for exceeding 1X */
        dot_overspill_si = (int) ceilf((symbol->dot_size - 1.0f) * scaler);
        dot_offset_s = dot_radius_s;
    }
    if (dot_overspill_si == 0) {
        dot_overspill_si = 1;
    }

    scale_width = (int) (symbol->width * scaler + xoffset_si + roffset_si + dot_overspill_si);
    scale_height = (int) (symbol_height_si + yoffset_si + boffset_si + dot_overspill_si);
    scale_size = (size_t) scale_width * scale_height;

    /* Apply scale options by creating pixel buffer */
    if (!(scaled_pixelbuf = (unsigned char *) raster_malloc(scale_size, 0 /*prev_size*/))) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 657, "Insufficient memory for pixel buffer");
    }
    memset(scaled_pixelbuf, DEFAULT_PAPER, scale_size);

    /* Plot the body of the symbol to the pixel buffer */
    for (r = 0; r < symbol->rows; r++) {
        int row_si = (int) (r * scaler + yoffset_si + dot_offset_s);
        for (i = 0; i < symbol->width; i++) {
            if (module_is_set(symbol, r, i)) {
                draw_circle(scaled_pixelbuf, scale_width, scale_height,
                            (int) (i * scaler + xoffset_si + dot_offset_s),
                            row_si, dot_radius_si, DEFAULT_INK);
            }
        }
    }

    draw_bind_box(symbol, scaled_pixelbuf, xoffset_si, yoffset_si, symbol_height_si, dot_overspill_si,
                0 /*upceanflag*/, 0 /*textoffset_si*/, scale_width, scale_height, (int) scaler);

    error_number = save_raster_image_to_file(symbol, scale_height, scale_width, scaled_pixelbuf, rotate_angle,
                                            file_type);
    if (rotate_angle || file_type != OUT_BUFFER || !(symbol->output_options & OUT_BUFFER_INTERMEDIATE)) {
        free(scaled_pixelbuf);
    }

    return error_number;
}

/* Convert UTF-8 to ISO/IEC 8859-1 for `draw_string()` human readable text */
static void to_iso8859_1(const unsigned char source[], unsigned char preprocessed[]) {
    int j, i, input_length;

    input_length = (int) ustrlen(source);

    j = 0;
    i = 0;
    while (i < input_length) {
        switch (source[i]) {
            case 0xC2:
                /* UTF-8 C2xxh */
                /* Character range: C280h (latin: 80h) to C2BFh (latin: BFh) */
                assert(i + 1 < input_length);
                i++;
                preprocessed[j] = source[i];
                j++;
                break;
            case 0xC3:
                /* UTF-8 C3xx */
                /* Character range: C380h (latin: C0h) to C3BFh (latin: FFh) */
                assert(i + 1 < input_length);
                i++;
                preprocessed[j] = source[i] + 64;
                j++;
                break;
            default:
                /* Process ASCII (< 80h), all other unicode points are ignored */
                if (source[i] < 128) {
                    preprocessed[j] = source[i];
                    j++;
                }
                break;
        }
        i++;
    }
    preprocessed[j] = '\0';
}

static int plot_raster_default(struct zint_symbol *symbol, const int rotate_angle, const int file_type) {
    int error_number;
    int main_width;
    int comp_xoffset = 0;
    unsigned char addon[6];
    int addon_len = 0;
    int addon_gap = 0;
    float addon_text_yposn = 0.0f;
    float xoffset, yoffset, roffset, boffset;
    float textoffset;
    int upceanflag = 0;
    int addon_latch = 0;
    int hide_text;
    int i, r;
    int block_width = 0;
    int font_height; /* Font pixel size (so whole integers) */
    float guard_descent;
    const int upcean_guard_whitespace = !(symbol->output_options & BARCODE_NO_QUIET_ZONES)
                                        && (symbol->output_options & EANUPC_GUARD_WHITESPACE);
    const int is_codablockf = symbol->symbology == BARCODE_CODABLOCKF || symbol->symbology == BARCODE_HIBC_BLOCKF;

    int textflags = 0;
    int xoffset_si, yoffset_si, roffset_si, boffset_si, qz_right_si;
    int xoffset_comp_si;
    int row_heights_si[200];
    int symbol_height_si;
    int image_width, image_height;
    size_t image_size;
    unsigned char *pixelbuf;
    float scaler = symbol->scale;
    int si;
    int half_int_scaling;
    int yposn_si;

    /* Ignore scaling < 0.5 for raster as would drop modules */
    if (scaler < 0.5f) {
        scaler = 0.5f;
    }
    /* If half-integer scaling, then set integer scaler `si` to avoid scaling at end */
    half_int_scaling = isfintf(scaler * 2.0f);
    if (half_int_scaling) {
        si = (int) (scaler * 2.0f);
    } else {
        si = 2;
    }

    (void) out_large_bar_height(symbol, si /*(scale and round)*/, row_heights_si, &symbol_height_si);

    main_width = symbol->width;

    if (is_composite(symbol->symbology)) {
        while (!module_is_set(symbol, symbol->rows - 1, comp_xoffset)) {
            comp_xoffset++;
        }
    }
    if (is_upcean(symbol->symbology)) {
        upceanflag = out_process_upcean(symbol, comp_xoffset, &main_width, addon, &addon_len, &addon_gap);
    }

    hide_text = ((!symbol->show_hrt) || (ustrlen(symbol->text) == 0) || scaler < 1.0f);

    out_set_whitespace_offsets(symbol, hide_text, comp_xoffset, &xoffset, &yoffset, &roffset, &boffset,
        NULL /*qz_right*/, si, &xoffset_si, &yoffset_si, &roffset_si, &boffset_si, &qz_right_si);

    xoffset_comp_si = xoffset_si + comp_xoffset * si;

    image_width = symbol->width * si + xoffset_si + roffset_si;

    /* Note font sizes halved as in pixels */
    if (upceanflag) {
        textflags = ZFONT_UPCEAN_TEXT | (symbol->output_options & SMALL_TEXT); /* Bold not available for EAN/UPC */
        font_height = (UPCEAN_FONT_HEIGHT + 1) / 2;
        /* Height of guard bar descent (none for EAN-2 and EAN-5) */
        guard_descent = upceanflag >= 6 ? symbol->guard_descent : 0.0f;
    } else {
        textflags = symbol->output_options & (SMALL_TEXT | BOLD_TEXT);
        font_height = textflags & SMALL_TEXT ? (SMALL_FONT_HEIGHT + 1) / 2 : (NORMAL_FONT_HEIGHT + 1) / 2;
        guard_descent = 0.0f;
    }

    if (hide_text) {
        textoffset = guard_descent;
    } else {
        if (upceanflag) {
            textoffset = font_height + symbol->text_gap;
            if (textoffset < guard_descent) {
                textoffset = guard_descent;
            }
        } else {
            textoffset = font_height + symbol->text_gap;
        }
    }

    image_height = symbol_height_si + (int) ceilf(textoffset * si) + yoffset_si + boffset_si;
    assert(image_width && image_height);
    image_size = (size_t) image_width * image_height;

    if (!(pixelbuf = (unsigned char *) raster_malloc(image_size, 0 /*prev_size*/))) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 658, "Insufficient memory for pixel buffer");
    }
    memset(pixelbuf, DEFAULT_PAPER, image_size);

    yposn_si = yoffset_si;

    /* Plot the body of the symbol to the pixel buffer */
    if (symbol->symbology == BARCODE_ULTRA) {
        for (r = 0; r < symbol->rows; r++) {
            const int row_height_si = row_heights_si[r];

            for (i = 0; i < symbol->width; i += block_width) {
                const int fill = module_colour_is_set(symbol, r, i);
                for (block_width = 1; (i + block_width < symbol->width)
                                        && module_colour_is_set(symbol, r, i + block_width) == fill; block_width++);
                if (fill) {
                    /* a colour block */
                    draw_bar_line(pixelbuf, i * si + xoffset_si, block_width * si, yposn_si, image_width,
                                ultra_colour[fill]);
                }
            }
            copy_bar_line(pixelbuf, xoffset_si, image_width - xoffset_si - roffset_si, yposn_si, row_height_si,
                        image_width, image_height);
            yposn_si += row_height_si;
        }

    } else if (upceanflag >= 6) { /* UPC-E, EAN-8, UPC-A, EAN-13 */
        for (r = 0; r < symbol->rows; r++) {
            int row_height_si = row_heights_si[r];

            for (i = 0; i < symbol->width; i += block_width) {
                const int fill = module_is_set(symbol, r, i);
                for (block_width = 1; (i + block_width < symbol->width)
                                        && module_is_set(symbol, r, i + block_width) == fill; block_width++);
                if ((r == (symbol->rows - 1)) && (i > main_width) && (addon_latch == 0)) {
                    int addon_row_height_si;
                    const int addon_row_adj_si = (int) ceilf((font_height + symbol->text_gap) * si);
                    copy_bar_line(pixelbuf, xoffset_si, main_width * si, yposn_si, row_height_si, image_width,
                                image_height);
                    addon_text_yposn = yposn_si;
                    yposn_si += addon_row_adj_si;
                    addon_row_height_si = row_height_si - addon_row_adj_si;
                    /* Following ISO/IEC 15420:2009 Figure 5 — UPC-A bar code symbol with 2-digit add-on (contrary to
                       GS1 General Specs v24.0 Figure 5.2.6.6-5) descends for all including UPC-A/E */
                    addon_row_height_si += guard_descent * si;
                    if (addon_row_height_si == 0) {
                        addon_row_height_si = 1;
                    }
                    row_height_si = addon_row_height_si;
                    addon_latch = 1;
                }
                if (fill) {
                    /* a bar */
                    draw_bar_line(pixelbuf, i * si + xoffset_si, block_width * si, yposn_si, image_width,
                                DEFAULT_INK);
                }
            }
            if (addon_latch) {
                copy_bar_line(pixelbuf, xoffset_si + main_width * si,
                            image_width - main_width * si - xoffset_si - roffset_si, yposn_si, row_height_si,
                            image_width, image_height);
            } else {
                copy_bar_line(pixelbuf, xoffset_si, image_width - xoffset_si - roffset_si, yposn_si, row_height_si,
                            image_width, image_height);
            }
            yposn_si += row_height_si;
        }

    } else {
        if (upceanflag && !hide_text) { /* EAN-2, EAN-5 (standalone add-ons) */
            yposn_si += (int) ceilf((font_height + symbol->text_gap) * si);
        }
        for (r = 0; r < symbol->rows; r++) {
            int row_height_si = row_heights_si[r];
            if (upceanflag && !hide_text) { /* EAN-2, EAN-5 (standalone add-ons) */
                row_height_si += textoffset * si - (yposn_si - yoffset_si);
            }

            for (i = 0; i < symbol->width; i += block_width) {
                const int fill = module_is_set(symbol, r, i);
                for (block_width = 1; (i + block_width < symbol->width)
                                        && module_is_set(symbol, r, i + block_width) == fill; block_width++);
                if (fill) {
                    /* a bar */
                    draw_bar_line(pixelbuf, i * si + xoffset_si, block_width * si, yposn_si, image_width,
                                DEFAULT_INK);
                }
            }
            copy_bar_line(pixelbuf, xoffset_si, image_width - xoffset_si - roffset_si, yposn_si, row_height_si,
                        image_width, image_height);
            yposn_si += row_height_si;
        }
    }

    if (guard_descent && upceanflag >= 6) { /* UPC-E, EAN-8, UPC-A, EAN-13 */
        /* Guard bar extension */
        const int guard_yoffset_si = yoffset_si + symbol_height_si;
        const int guard_descent_si = guard_descent * si;

        if (upceanflag == 6) { /* UPC-E */
            draw_bar_line(pixelbuf, 0 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 2 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 46 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 48 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 50 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);

        } else if (upceanflag == 8) { /* EAN-8 */
            draw_bar_line(pixelbuf, 0 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 2 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 32 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 34 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 64 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 66 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);

        } else if (upceanflag == 12) { /* UPC-A */
            for (i = 0 + comp_xoffset; i < 11 + comp_xoffset; i += block_width) {
                const int fill = module_is_set(symbol, symbol->rows - 1, i);
                for (block_width = 1; (i + block_width < symbol->width)
                                            && module_is_set(symbol, symbol->rows - 1, i + block_width) == fill;
                                        block_width++);
                if (fill) {
                    draw_bar_line(pixelbuf, i * si + xoffset_si, block_width * si, guard_yoffset_si, image_width,
                                DEFAULT_INK);
                }
            }
            draw_bar_line(pixelbuf, 46 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 48 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            for (i = 85 + comp_xoffset; i < 96 + comp_xoffset; i += block_width) {
                const int fill = module_is_set(symbol, symbol->rows - 1, i);
                for (block_width = 1; (i + block_width < symbol->width)
                                            && module_is_set(symbol, symbol->rows - 1, i + block_width) == fill;
                                        block_width++);
                if (fill) {
                    draw_bar_line(pixelbuf, i * si + xoffset_si, block_width * si, guard_yoffset_si, image_width,
                                DEFAULT_INK);
                }
            }

        } else { /* EAN-13 */
            draw_bar_line(pixelbuf, 0 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 2 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 46 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 48 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 92 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
            draw_bar_line(pixelbuf, 94 * si + xoffset_comp_si, 1 * si, guard_yoffset_si, image_width, DEFAULT_INK);
        }
        copy_bar_line(pixelbuf, xoffset_comp_si, image_width - xoffset_comp_si - roffset_si, guard_yoffset_si,
                    guard_descent_si, image_width, image_height);
    }

    /* Add the text */

    if (!hide_text) {

        if (upceanflag >= 6) { /* UPC-E, EAN-8, UPC-A, EAN-13 */

            /* Note font sizes halved as in pixels */
            const int upcea_height_adj = ((UPCEAN_FONT_HEIGHT - UPCEAN_SMALL_FONT_HEIGHT) * si + 1) / 2;

            int text_yposn = yoffset_si + symbol_height_si + (int) (symbol->text_gap * si);
            if (symbol->border_width > 0 && (symbol->output_options & (BARCODE_BOX | BARCODE_BIND))
                    && !(symbol->output_options & BARCODE_BIND_TOP)) { /* Trumps BARCODE_BOX & BARCODE_BIND */
                text_yposn += symbol->border_width * si;
            }

            if (upceanflag == 6) { /* UPC-E */
                int text_xposn = -5 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text, 1, text_xposn, text_yposn + upcea_height_adj,
                            textflags | SMALL_TEXT | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
                text_xposn = 24 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text + 1, 6, text_xposn, text_yposn, textflags, image_width,
                            image_height, si);
                /* TODO: GS1 General Specs v24.0 5.2.5 Human readable interpretation says 3X but this could cause
                   digit's righthand to touch any add-on, now that they descend, so use 2X, until clarified */
                text_xposn = (51 + 2) * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text + 7, 1, text_xposn, text_yposn + upcea_height_adj,
                            textflags | SMALL_TEXT | ZFONT_HALIGN_LEFT, image_width, image_height, si);
                if (addon_len) {
                    text_xposn = ((addon_len == 2 ? 61 : 75) + addon_gap) * si + xoffset_comp_si;
                    draw_string(pixelbuf, addon, addon_len, text_xposn, addon_text_yposn, textflags,
                                image_width, image_height, si);
                    if (upcean_guard_whitespace) {
                        text_xposn = symbol->width * si + qz_right_si + xoffset_si;
                        draw_string(pixelbuf, (const unsigned char *) ">", 1, text_xposn, addon_text_yposn,
                                    textflags | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
                    }
                }

            } else if (upceanflag == 8) { /* EAN-8 */
                int text_xposn;
                if (upcean_guard_whitespace) {
                    text_xposn = -7 * si + xoffset_comp_si;
                    draw_string(pixelbuf, (const unsigned char *) "<", 1, text_xposn, text_yposn,
                                textflags | ZFONT_HALIGN_LEFT, image_width, image_height, si);
                }
                text_xposn = 17 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text, 4, text_xposn, text_yposn, textflags, image_width, image_height,
                            si);
                text_xposn = 50 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text + 4, 4, text_xposn, text_yposn, textflags, image_width,
                            image_height, si);
                if (addon_len) {
                    text_xposn = ((addon_len == 2 ? 77 : 91) + addon_gap) * si + xoffset_comp_si;
                    draw_string(pixelbuf, addon, addon_len, text_xposn, addon_text_yposn, textflags,
                                image_width, image_height, si);
                    if (upcean_guard_whitespace) {
                        text_xposn = symbol->width * si + qz_right_si + xoffset_si;
                        draw_string(pixelbuf, (const unsigned char *) ">", 1, text_xposn, addon_text_yposn,
                                    textflags | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
                    }
                } else if (upcean_guard_whitespace) {
                    text_xposn = symbol->width * si + qz_right_si + xoffset_si;
                    draw_string(pixelbuf, (const unsigned char *) ">", 1, text_xposn, text_yposn,
                                textflags | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
                }

            } else if (upceanflag == 12) { /* UPC-A */
                int text_xposn = -5 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text, 1, text_xposn, text_yposn + upcea_height_adj,
                            textflags | SMALL_TEXT | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
                text_xposn = 28 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text + 1, 5, text_xposn, text_yposn, textflags, image_width,
                            image_height, si);
                text_xposn = 67 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text + 6, 5, text_xposn, text_yposn, textflags, image_width,
                            image_height, si);
                /* TODO: GS1 General Specs v24.0 5.2.5 Human readable interpretation says 5X but this could cause
                   digit's righthand to touch any add-on, now that they descend, so use 4X, until clarified */
                text_xposn = (95 + 4) * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text + 11, 1, text_xposn, text_yposn + upcea_height_adj,
                            textflags | SMALL_TEXT | ZFONT_HALIGN_LEFT, image_width, image_height, si);
                if (addon_len) {
                    text_xposn = ((addon_len == 2 ? 105 : 119) + addon_gap) * si + xoffset_comp_si;
                    draw_string(pixelbuf, addon, addon_len, text_xposn, addon_text_yposn, textflags,
                                image_width, image_height, si);
                    if (upcean_guard_whitespace) {
                        text_xposn = symbol->width * si + qz_right_si + xoffset_si;
                        draw_string(pixelbuf, (const unsigned char *) ">", 1, text_xposn, addon_text_yposn,
                                    textflags | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
                    }
                }

            } else { /* EAN-13 */
                int text_xposn = -5 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text, 1, text_xposn, text_yposn, textflags | ZFONT_HALIGN_RIGHT,
                            image_width, image_height, si);
                text_xposn = 24 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text + 1, 6, text_xposn, text_yposn, textflags, image_width,
                            image_height, si);
                text_xposn = 71 * si + xoffset_comp_si;
                draw_string(pixelbuf, symbol->text + 7, 6, text_xposn, text_yposn, textflags, image_width,
                            image_height, si);
                if (addon_len) {
                    text_xposn = ((addon_len == 2 ? 105 : 119) + addon_gap) * si + xoffset_comp_si;
                    draw_string(pixelbuf, addon, addon_len, text_xposn, addon_text_yposn, textflags,
                                image_width, image_height, si);
                    if (upcean_guard_whitespace) {
                        text_xposn = symbol->width * si + qz_right_si + xoffset_si;
                        draw_string(pixelbuf, (const unsigned char *) ">", 1, text_xposn, addon_text_yposn,
                                    textflags | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
                    }
                } else if (upcean_guard_whitespace) {
                    text_xposn = symbol->width * si + qz_right_si + xoffset_si;
                    draw_string(pixelbuf, (const unsigned char *) ">", 1, text_xposn, text_yposn,
                                textflags | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
                }
            }
        } else if (upceanflag) { /* EAN-2, EAN-5 (standalone add-ons) */
            int text_xposn = (int) ((main_width / 2.0f) * si) + xoffset_si;
            int text_yposn = yoffset_si;
            if (symbol->border_width > 0
                    && (symbol->output_options & (BARCODE_BOX | BARCODE_BIND | BARCODE_BIND_TOP))) {
                text_yposn -= symbol->border_width * si;
            }
            /* Put the human readable text at the top */
            draw_string(pixelbuf, symbol->text, -1, text_xposn, text_yposn, textflags, image_width, image_height, si);
            if (upcean_guard_whitespace) {
                text_xposn = symbol->width * si + qz_right_si + xoffset_comp_si;
                draw_string(pixelbuf, (const unsigned char *) ">", 1, text_xposn, text_yposn,
                            textflags | ZFONT_HALIGN_RIGHT, image_width, image_height, si);
            }
        } else {
            /* Suppress clang-analyzer-core.CallAndMessage warning */
            unsigned char local_text[sizeof(symbol->text)] = {0};
            int text_xposn = (int) ((main_width / 2.0f) * si) + xoffset_si;
            int text_yposn = yoffset_si + symbol_height_si + (int) (symbol->text_gap * si);
            if (symbol->border_width > 0 && (symbol->output_options & (BARCODE_BOX | BARCODE_BIND))
                    && !(symbol->output_options & BARCODE_BIND_TOP)) { /* Trumps BARCODE_BOX & BARCODE_BIND */
                text_yposn += symbol->border_width * si;
            }
            to_iso8859_1(symbol->text, local_text);
            /* Put the human readable text at the bottom */
            draw_string(pixelbuf, local_text, -1, text_xposn, text_yposn, textflags, image_width, image_height, si);
        }
    }

    /* Separator binding for stacked barcodes */
    if ((symbol->output_options & BARCODE_BIND) && (symbol->rows > 1) && is_stackable(symbol->symbology)) {
        int sep_xoffset_si = xoffset_si;
        int sep_width_si = symbol->width * si;
        int sep_height_si, sep_yoffset_si;
        float sep_height = 1.0f;
        if (symbol->option_3 > 0 && symbol->option_3 <= 4) {
            sep_height = symbol->option_3;
        }
        sep_height_si = (int) (sep_height * si);
        sep_yoffset_si = yoffset_si + row_heights_si[0] - sep_height_si / 2;
        if (is_codablockf) {
            /* Avoid 11-module start and 13-module stop chars */
            sep_xoffset_si += 11 * si;
            sep_width_si -= (11 + 13) * si;
        }
        for (r = 1; r < symbol->rows; r++) {
            draw_bar(pixelbuf, sep_xoffset_si, sep_width_si, sep_yoffset_si, sep_height_si, image_width, image_height,
                        DEFAULT_INK);
            sep_yoffset_si += row_heights_si[r];
        }
    }

    draw_bind_box(symbol, pixelbuf, xoffset_si, yoffset_si, symbol_height_si, 0 /*dot_overspill_si*/,
                upceanflag, (int) (textoffset * si), image_width, image_height, si);

    if (!half_int_scaling) {
        size_t prev_image_row;
        unsigned char *scaled_pixelbuf;
        const int scale_width = (int) stripf(image_width * scaler);
        const int scale_height = (int) stripf(image_height * scaler);

        /* Apply scale options by creating another pixel buffer */
        if (!(scaled_pixelbuf = (unsigned char *) raster_malloc((size_t) scale_width * scale_height, image_size))) {
            free(pixelbuf);
            return errtxt(ZINT_ERROR_MEMORY, symbol, 659, "Insufficient memory for scaled pixel buffer");
        }
        memset(scaled_pixelbuf, DEFAULT_PAPER, (size_t) scale_width * scale_height);

        /* Interpolate */
        for (r = 0; r < scale_height; r++) {
            size_t scaled_row = (size_t) scale_width * r;
            size_t image_row = (size_t) stripf(r / scaler) * image_width;
            if (r && (image_row == prev_image_row
                    || memcmp(pixelbuf + image_row, pixelbuf + prev_image_row, image_width) == 0)) {
                memcpy(scaled_pixelbuf + scaled_row, scaled_pixelbuf + scaled_row - scale_width, scale_width);
            } else {
                for (i = 0; i < scale_width; i++) {
                    *(scaled_pixelbuf + scaled_row + i) = *(pixelbuf + image_row + (int) stripf(i / scaler));
                }
            }
            prev_image_row = image_row;
        }

        error_number = save_raster_image_to_file(symbol, scale_height, scale_width, scaled_pixelbuf, rotate_angle,
                                                file_type);
        if (rotate_angle || file_type != OUT_BUFFER || !(symbol->output_options & OUT_BUFFER_INTERMEDIATE)) {
            free(scaled_pixelbuf);
        }
        free(pixelbuf);
    } else {
        error_number = save_raster_image_to_file(symbol, image_height, image_width, pixelbuf, rotate_angle,
                                                file_type);
        if (rotate_angle || file_type != OUT_BUFFER || !(symbol->output_options & OUT_BUFFER_INTERMEDIATE)) {
            free(pixelbuf);
        }
    }
    return error_number;
}

INTERNAL int plot_raster(struct zint_symbol *symbol, int rotate_angle, int file_type) {
    int error;

#ifdef ZINT_NO_PNG
    if (file_type == OUT_PNG_FILE) {
        return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 660, "PNG format disabled at compile time");
    }
#endif /* ZINT_NO_PNG */

    error = out_check_colour_options(symbol);
    if (error != 0) {
        return error;
    }
    if (symbol->rows <= 0) {
        return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 664, "No rows");
    }

    if (symbol->symbology == BARCODE_MAXICODE) {
        error = plot_raster_maxicode(symbol, rotate_angle, file_type);
    } else if (symbol->output_options & BARCODE_DOTTY_MODE) {
        error = plot_raster_dotty(symbol, rotate_angle, file_type);
    } else {
        error = plot_raster_default(symbol, rotate_angle, file_type);
    }

    return error;
}

/* vim: set ts=4 sw=4 et : */
