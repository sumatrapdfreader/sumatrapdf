/* pcx.c - Handles output to ZSoft PCX file */
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
#include "pcx.h"        /* PCX header structure */

/* ZSoft PCX File Format Technical Reference Manual http://bespin.org/~qz/pc-gpe/pcx.txt */
INTERNAL int pcx_pixel_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf) {
    unsigned char fgred, fggrn, fgblu, fgalpha, bgred, bggrn, bgblu, bgalpha;
    int row, column, i, colour;
    int run_count;
    struct filemem fm;
    struct filemem *const fmp = &fm;
    pcx_header_t header;
    unsigned char previous;
    const unsigned char *pb;
    const int bytes_per_line = symbol->bitmap_width + (symbol->bitmap_width & 1); /* Must be even */
    unsigned char *rle_row = (unsigned char *) z_alloca(bytes_per_line);

    rle_row[bytes_per_line - 1] = 0; /* Will remain zero if bitmap_width odd */

    (void) out_colour_get_rgb(symbol->fgcolour, &fgred, &fggrn, &fgblu, &fgalpha);
    (void) out_colour_get_rgb(symbol->bgcolour, &bgred, &bggrn, &bgblu, &bgalpha);

    header.manufacturer = 10; /* ZSoft */
    header.version = 5; /* Version 3.0 */
    header.encoding = 1; /* Run length encoding */
    header.bits_per_pixel = 8; /* TODO: 1-bit monochrome black/white */
    out_le_u16(header.window_xmin, 0);
    out_le_u16(header.window_ymin, 0);
    out_le_u16(header.window_xmax, symbol->bitmap_width - 1);
    out_le_u16(header.window_ymax, symbol->bitmap_height - 1);
    out_le_u16(header.horiz_dpi, symbol->dpmm ? roundf(stripf(symbol->dpmm * 25.4f)) : 300);
    header.vert_dpi = header.horiz_dpi;

    for (i = 0; i < 48; i++) {
        header.colourmap[i] = 0x00;
    }

    header.reserved = 0;
    header.number_of_planes = 3 + (fgalpha != 0xFF || bgalpha != 0xFF); /* TODO: 1-bit monochrome black/white */

    out_le_u16(header.bytes_per_line, bytes_per_line);

    out_le_u16(header.palette_info, 1); /* Colour */
    out_le_u16(header.horiz_screen_size, 0);
    out_le_u16(header.vert_screen_size, 0);

    for (i = 0; i < 54; i++) {
        header.filler[i] = 0x00;
    }

    /* Open output file in binary mode */
    if (!fm_open(fmp, symbol, "wb")) {
        return errtxtf(ZINT_ERROR_FILE_ACCESS, symbol, 621, "Could not open PCX output file (%1$d: %2$s)", fmp->err,
                        strerror(fmp->err));
    }

    fm_write(&header, sizeof(pcx_header_t), 1, fmp);

    for (row = 0, pb = pixelbuf; row < symbol->bitmap_height; row++, pb += symbol->bitmap_width) {
        for (colour = 0; colour < header.number_of_planes; colour++) {
            for (column = 0; column < symbol->bitmap_width; column++) {
                const unsigned char ch = pb[column];
                switch (colour) {
                    case 0:
                        if (ch == '0' || ch == '1') {
                            rle_row[column] = ch != '0' ? fgred : bgred;
                        } else {
                            out_colour_char_to_rgb(ch, &rle_row[column], NULL, NULL);
                        }
                        break;
                    case 1:
                        if (ch == '0' || ch == '1') {
                            rle_row[column] = ch != '0' ? fggrn : bggrn;
                        } else {
                            out_colour_char_to_rgb(ch, NULL, &rle_row[column], NULL);
                        }
                        break;
                    case 2:
                        if (ch == '0' || ch == '1') {
                            rle_row[column] = ch != '0' ? fgblu : bgblu;
                        } else {
                            out_colour_char_to_rgb(ch, NULL, NULL, &rle_row[column]);
                        }
                        break;
                    case 3:
                        rle_row[column] = ch != '0' ? fgalpha : bgalpha;
                        break;
                }
            }

            /* Based on ImageMagick/coders/pcx.c PCXWritePixels()
             * Copyright 1999-2020 ImageMagick Studio LLC */
            previous = rle_row[0];
            run_count = 1;
            for (column = 1; column < bytes_per_line; column++) { /* Note going up to bytes_per_line */
                if ((previous == rle_row[column]) && (run_count < 63)) {
                    run_count++;
                } else {
                    if (run_count > 1 || (previous & 0xc0) == 0xc0) {
                        run_count += 0xc0;
                        fm_putc(run_count, fmp);
                    }
                    fm_putc(previous, fmp);
                    previous = rle_row[column];
                    run_count = 1;
                }
            }

            if (run_count > 1 || (previous & 0xc0) == 0xc0) {
                run_count += 0xc0;
                fm_putc(run_count, fmp);
            }
            fm_putc(previous, fmp);
        }
    }

    if (fm_error(fmp)) {
        errtxtf(0, symbol, 622, "Incomplete write of PCX output (%1$d: %2$s)", fmp->err, strerror(fmp->err));
        (void) fm_close(fmp, symbol);
        return ZINT_ERROR_FILE_WRITE;
    }

    if (!fm_close(fmp, symbol)) {
        return errtxtf(ZINT_ERROR_FILE_WRITE, symbol, 624, "Failure on closing PCX output file (%1$d: %2$s)",
                        fmp->err, strerror(fmp->err));
    }

    return 0;
}

/* vim: set ts=4 sw=4 et : */
