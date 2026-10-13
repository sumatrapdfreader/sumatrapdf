/* ps.c - Post Script output */
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
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "filemem.h"
#include "output.h"

/* Output Ultracode rectangle colour as PostScript setrgbcolor/setcmykcolor */
static void ps_put_colour(const int is_rgb, const int colour, struct filemem *const fmp) {
    const int idx = colour >= 1 && colour <= 8 ? colour - 1 : 6 /*black*/;
    if (is_rgb) {
        /* Use RGB colour space */
        static const char ps_rgbs[8][6] = {
            "0 1 1", /* 0: Cyan (1) */
            "0 0 1", /* 1: Blue (2) */
            "1 0 1", /* 2: Magenta (3) */
            "1 0 0", /* 3: Red (4) */
            "1 1 0", /* 4: Yellow (5) */
            "0 1 0", /* 5: Green (6) */
            "0 0 0", /* 6: Black (7) */
            "1 1 1", /* 7: White (8) */
        };
        fm_puts(ps_rgbs[idx], fmp);
        fm_puts(" setrgbcolor\n", fmp);
    } else {
        static const char ps_cmyks[8][8] = {
            "1 0 0 0", /* 0: Cyan (1) */
            "1 1 0 0", /* 1: Blue (2) */
            "0 1 0 0", /* 2: Magenta (3) */
            "0 1 1 0", /* 3: Red (4) */
            "0 0 1 0", /* 4: Yellow (5) */
            "1 0 1 0", /* 5: Green (6) */
            "0 0 0 1", /* 6: Black (7) */
            "0 0 0 0", /* 7: White (8) */
        };
        fm_puts(ps_cmyks[idx], fmp);
        fm_puts(" setcmykcolor\n", fmp);
    }
}

/* Escape special PostScript chars. Assumes valid UTF-8-encoded ISO/IEC 8859-1 */
static void ps_convert(const unsigned char *string, unsigned char *ps_string) {
    const unsigned char *s;
    unsigned char *p = ps_string;

    for (s = string; *s; s++) {
        switch (*s) {
            case '(':
            case ')':
            case '\\':
                *p++ = '\\';
                *p++ = *s;
                break;
            case 0xC2:
                *p++ = *++s; /* C2 80-BF -> 80-BF */
                break;
            case 0xC3:
                *p++ = *++s + 0x40; /* C3 80-BF -> C0-FF */
                break;
            default:
                if (*s < 0x80) { /* ASCII - all other Unicode points > U+00FF ignored */
                    *p++ = *s;
                }
                break;

        }
    }
    *p = '\0';
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL void ps_convert_test(const unsigned char *string, unsigned char *ps_string) {
    ps_convert(string, ps_string);
}
#endif

/* Helper to output RGB colour */
static void ps_put_rgbcolor(const float red, const float green, const float blue,
                struct filemem *const fmp) {
    fm_putsf("", 2, red, fmp);
    fm_putsf(" ", 2, green, fmp);
    fm_putsf(" ", 2, blue, fmp);
    fm_puts(" setrgbcolor\n", fmp);
}

/* Helper to output CMYK colour */
static void ps_put_cmykcolor(const float cyan, const float magenta, const float yellow, const float black,
                struct filemem *const fmp) {
    fm_putsf("", 2, cyan, fmp);
    fm_putsf(" ", 2, magenta, fmp);
    fm_putsf(" ", 2, yellow, fmp);
    fm_putsf(" ", 2, black, fmp);
    fm_puts(" setcmykcolor\n", fmp);
}

/* Helper to output rectangle */
static void ps_put_rect(const struct zint_symbol *symbol, const struct zint_vector_rect *rect, const int type,
                struct filemem *const fmp) {
    if (type == 0 || type == 1) {
        fm_putsf("", 2, rect->height, fmp);
        fm_putsf(" ", 2, (symbol->vector->height - rect->y) - rect->height, fmp);
    }
    fm_putsf(type == 0 ? " " : type == 1 ? " I " : type == 2 ? "I " : "", 2, rect->x, fmp);
    fm_putsf(" ", 2, rect->width, fmp);
    fm_puts(" R\n", fmp);
}

/* Helper to output circle/disc */
static void ps_put_circle(const struct zint_symbol *symbol, const struct zint_vector_circle *circle,
                const float radius, const int type, struct filemem *const fmp) {
    if (circle->width) {
        fm_putsf("", 2, circle->x, fmp);
        fm_putsf(" ", 2, symbol->vector->height - circle->y, fmp);
        fm_putsf(" ", 4, radius, fmp);
        fm_putsf(" ", 4, circle->width, fmp);
        fm_puts(" C\n", fmp);
    } else {
        if (type == 0 || type == 1) {
            fm_putsf("", 2, symbol->vector->height - circle->y, fmp);
            fm_putsf(" ", 4, radius, fmp);
        }
        fm_putsf(type == 0 ? " " : type == 1 ? " I " : type == 2 ? "I " : "", 2, circle->x, fmp);
        fm_puts(" D\n", fmp);
    }
}

/* Helper to count rectangles */
static int ps_count_rectangles(const struct zint_symbol *symbol) {
    int rectangles = 0;
    const struct zint_vector_rect *rect;

    for (rect = symbol->vector->rectangles; rect; rect = rect->next) {
        rectangles++;
    }

    return rectangles;
}

INTERNAL int ps_plot(struct zint_symbol *symbol) {
    struct filemem fm;
    struct filemem *const fmp = &fm;
    unsigned char fgred, fggrn, fgblu, bgred, bggrn, bgblu, bgalpha;
    int fgcyan, fgmagenta, fgyellow, fgblack, bgcyan, bgmagenta, bgyellow, bgblack;
    float red_ink = 0.0f, green_ink = 0.0f, blue_ink = 0.0f; /* Suppress `-Wmaybe-uninitialized` */
    float red_paper = 0.0f, green_paper = 0.0f, blue_paper = 0.0f;
    float cyan_ink = 0.0f, magenta_ink = 0.0f, yellow_ink = 0.0f, black_ink = 0.0f;
    float cyan_paper = 0.0f, magenta_paper = 0.0f, yellow_paper = 0.0f, black_paper = 0.0f;
    float previous_diameter;
    float radius;
    int colour_rect_flag;
    int type_latch;
    int draw_background = 1;
    struct zint_vector_rect *rect;
    struct zint_vector_hexagon *hex;
    struct zint_vector_circle *circle;
    struct zint_vector_string *string;
    int i, len;
    int ps_len = 0;
    int iso_latin1 = 0;
    int have_circles_with_width = 0, have_circles_without_width = 0;
    const int upcean = is_upcean(symbol->symbology);
    const int is_rgb = (symbol->output_options & CMYK_COLOUR) == 0;

    if (symbol->vector == NULL) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 646, "Vector header NULL");
    }
    if (!fm_open(fmp, symbol, "w")) {
        return errtxtf(ZINT_ERROR_FILE_ACCESS, symbol, 645, "Could not open EPS output file (%1$d: %2$s)", fmp->err,
                        strerror(fmp->err));
    }

    if (is_rgb) {
        (void) out_colour_get_rgb(symbol->fgcolour, &fgred, &fggrn, &fgblu, NULL /*alpha*/);
        red_ink = fgred / 255.0f;
        green_ink = fggrn / 255.0f;
        blue_ink = fgblu / 255.0f;

        (void) out_colour_get_rgb(symbol->bgcolour, &bgred, &bggrn, &bgblu, &bgalpha);
        red_paper = bgred / 255.0f;
        green_paper = bggrn / 255.0f;
        blue_paper = bgblu / 255.0f;
    } else {
        (void) out_colour_get_cmyk(symbol->fgcolour, &fgcyan, &fgmagenta, &fgyellow, &fgblack, NULL /*rgb_alpha*/);
        cyan_ink = fgcyan / 100.0f;
        magenta_ink = fgmagenta / 100.0f;
        yellow_ink = fgyellow / 100.0f;
        black_ink = fgblack / 100.0f;

        (void) out_colour_get_cmyk(symbol->bgcolour, &bgcyan, &bgmagenta, &bgyellow, &bgblack, &bgalpha);
        cyan_paper = bgcyan / 100.0f;
        magenta_paper = bgmagenta / 100.0f;
        yellow_paper = bgyellow / 100.0f;
        black_paper = bgblack / 100.0f;
    }
    if (bgalpha == 0) {
        draw_background = 0;
    }

    for (i = 0, len = (int) ustrlen(symbol->text); i < len; i++) {
        switch (symbol->text[i]) {
            case '(':
            case ')':
            case '\\':
                ps_len += 2;
                break;
            default:
                if (!iso_latin1 && symbol->text[i] >= 0x80) {
                    iso_latin1 = 1;
                }
                ps_len++; /* Will overcount 2 byte UTF-8 chars */
                break;
        }
    }

    /* Check for circle widths */
    for (circle = symbol->vector->circles; circle; circle = circle->next) {
        if (circle->width) {
            have_circles_with_width = 1;
            if (have_circles_without_width) {
                break;
            }
        } else {
            have_circles_without_width = 1;
            if (have_circles_with_width) {
                break;
            }
        }
    }

    /* Start writing the header */
    fm_puts("%!PS-Adobe-3.0 EPSF-3.0\n"
            "%%Creator: Zint ", fmp);
#if ZINT_VERSION_BUILD
    fm_printf(fmp, "%d.%d.%d.%d\n", ZINT_VERSION_MAJOR, ZINT_VERSION_MINOR, ZINT_VERSION_RELEASE, ZINT_VERSION_BUILD);
#else
    fm_printf(fmp, "%d.%d.%d\n", ZINT_VERSION_MAJOR, ZINT_VERSION_MINOR, ZINT_VERSION_RELEASE);
#endif
    fm_puts("%%Title: Zint Generated Symbol\n"
            "%%Pages: 0\n"
            "%%BoundingBox: 0 0 ", fmp);
    fm_printf(fmp, "%d %d\n", (int) ceilf(symbol->vector->width), (int) ceilf(symbol->vector->height));
    fm_puts("%%EndComments\n", fmp);

    /* Definitions */
    if (have_circles_without_width) {
        /* Disc: y radius x D */
        fm_puts("/D { newpath 3 1 roll 0 360 arc fill } bind def\n", fmp);
    }
    if (have_circles_with_width) {
        /* Circle (ring): x y radius width C (adapted from BWIPP renmaxicode.ps) */
        fm_puts("/C { newpath 4 1 roll 3 copy 0 360 arc closepath 4 -1 roll add 360 0 arcn closepath fill }"
                " bind def\n", fmp);
    }
    if (symbol->vector->hexagons) {
        /* Hexagon: radius half_radius half_sqrt3_radius x y */
        if (symbol->vector->hexagons->rotation == 0 || symbol->vector->hexagons->rotation == 180) {
            fm_puts("/H { newpath moveto 2 copy exch neg exch rmoveto 2 index neg 0 exch rlineto 2 copy neg rlineto"
                    " 2 copy rlineto 3 -1 roll 0 exch rlineto exch neg exch rlineto closepath fill }"
                    " bind def\n", fmp);
        } else {
            fm_puts("/H { newpath moveto 2 copy neg exch neg rmoveto 2 index 0 rlineto 2 copy exch rlineto"
                    " 2 copy neg exch rlineto 3 -1 roll neg 0 rlineto neg exch neg rlineto closepath fill }"
                    " bind def\n", fmp);
        }
        /* Copy r hr hsr for repeat use without having to specify them subsequently */
        fm_puts("/J { 3 copy } bind def\n", fmp);
        /* TODO: Save repeating x also */
    }
    if (symbol->vector->rectangles || draw_background) {
        /* Rectangle: h y x w */
        fm_puts("/R { newpath 4 1 roll exch moveto 1 index 0 rlineto 0 exch rlineto neg 0 rlineto closepath fill }"
                " bind def\n", fmp);
    }
    if (symbol->vector->rectangles || have_circles_without_width) {
        /* Copy h y (rect) or y r (disc) for repeat use without having to specify them subsequently */
        fm_puts("/I { 2 copy } bind def\n", fmp);
    }

    /* Now the actual representation */

    /* Background */
    if (draw_background) {
        if (is_rgb) {
            ps_put_rgbcolor(red_paper, green_paper, blue_paper, fmp);
        } else {
            ps_put_cmykcolor(cyan_paper, magenta_paper, yellow_paper, black_paper, fmp);
        }

        fm_putsf("", 2, symbol->vector->height, fmp);
        fm_putsf(" 0 0 ", 2, symbol->vector->width, fmp); /* y x w */
        fm_puts(" R\n", fmp);
    }

    if (symbol->symbology != BARCODE_ULTRA) {
        if (is_rgb) {
            ps_put_rgbcolor(red_ink, green_ink, blue_ink, fmp);
        } else {
            ps_put_cmykcolor(cyan_ink, magenta_ink, yellow_ink, black_ink, fmp);
        }
    }

    /* Rectangles */
    if (symbol->symbology == BARCODE_ULTRA) {
        /* Group rectangles by colour */
        const int rect_cnt = ps_count_rectangles(symbol);
        struct zint_vector_rect **ultra_rects
            = (struct zint_vector_rect **) z_alloca(sizeof(struct zint_vector_rect *) * (rect_cnt ? rect_cnt : 1));
        int u_i = 0;
        for (i = 0; i <= 8; i++) {
            for (rect = symbol->vector->rectangles; rect; rect = rect->next) {
                if ((i == 0 && rect->colour == -1) || rect->colour == i) {
                    ultra_rects[u_i++] = rect;
                }
            }
        }
        assert(u_i == ps_count_rectangles(symbol));

        colour_rect_flag = 0;
        type_latch = 0;
        for (i = 0; i < u_i; i++) {
            rect = ultra_rects[i];
            if (i == 0 || rect->colour != ultra_rects[i - 1]->colour) {
                if (rect->colour == -1) {
                    if (colour_rect_flag == 0) {
                        /* Set foreground colour */
                        if (is_rgb) {
                            ps_put_rgbcolor(red_ink, green_ink, blue_ink, fmp);
                        } else {
                            ps_put_cmykcolor(cyan_ink, magenta_ink, yellow_ink, black_ink, fmp);
                        }
                        colour_rect_flag = 1;
                    }
                } else {
                    /* Set new colour */
                    ps_put_colour(is_rgb, rect->colour, fmp);
                }
            }
            if (i + 1 < u_i && rect->height == ultra_rects[i + 1]->height && rect->y == ultra_rects[i + 1]->y) {
                ps_put_rect(symbol, rect, type_latch ? 2 : 1, fmp);
                type_latch = 1;
            } else {
                ps_put_rect(symbol, rect, type_latch ? 3 : 0, fmp);
                type_latch = 0;
            }
        }
    } else {
        type_latch = 0;
        for (rect = symbol->vector->rectangles; rect; rect = rect->next) {
            if (rect->next && rect->height == rect->next->height && rect->y == rect->next->y) {
                ps_put_rect(symbol, rect, type_latch ? 2 : 1, fmp);
                type_latch = 1;
            } else {
                ps_put_rect(symbol, rect, type_latch ? 3 : 0, fmp);
                type_latch = 0;
            }
        }
    }

    /* Hexagons */
    previous_diameter = 0.0f;
    for (hex = symbol->vector->hexagons; hex; hex = hex->next) {
        float hy = symbol->vector->height - hex->y;
        if (previous_diameter != hex->diameter) {
            previous_diameter = hex->diameter;
            fm_putsf("", 4, 0.5f * previous_diameter /*radius*/, fmp);
            fm_putsf(" ", 4, 0.43301270189221932338f * previous_diameter /*half_sqrt3_radius*/, fmp);
            fm_putsf(" ", 4, 0.25f * previous_diameter /*half_radius*/, fmp);
            fm_putc('\n', fmp);
        }
        if (hex->next) {
            fm_putsf("J ", 2, hex->x, fmp);
        } else {
            fm_putsf("", 2, hex->x, fmp);
        }
        fm_putsf(" ", 2, hy, fmp);
        fm_puts(" H\n", fmp);
    }

    /* Circles */
    previous_diameter = radius = 0.0f;
    type_latch = 0;
    for (circle = symbol->vector->circles; circle; circle = circle->next) {
        if (previous_diameter != circle->diameter - circle->width) {
            previous_diameter = circle->diameter - circle->width;
            radius = 0.5f * previous_diameter;
        }
        if (circle->colour) { /* Legacy - no longer used */
            /* A 'white' circle */
            if (is_rgb) {
                ps_put_rgbcolor(red_paper, green_paper, blue_paper, fmp);
            } else {
                ps_put_cmykcolor(cyan_paper, magenta_paper, yellow_paper, black_paper, fmp);
            }
            ps_put_circle(symbol, circle, radius, 0 /*type*/, fmp);
            if (circle->next) {
                if (is_rgb) {
                    ps_put_rgbcolor(red_ink, green_ink, blue_ink, fmp);
                } else {
                    ps_put_cmykcolor(cyan_ink, magenta_ink, yellow_ink, black_ink, fmp);
                }
            }
        } else {
            /* A 'black' circle */
            if (circle->next && circle->y == circle->next->y && circle->diameter == circle->next->diameter) {
                ps_put_circle(symbol, circle, radius, type_latch ? 2 : 1, fmp);
                type_latch = 1;
            } else {
                ps_put_circle(symbol, circle, radius, type_latch ? 3 : 0, fmp);
                type_latch = 0;
            }
        }
    }

    /* Text */

    string = symbol->vector->strings;

    if (string) {
        float previous_fsize = 0.0f;
        const char *font;
        unsigned char *ps_string = (unsigned char *) z_alloca(ps_len + 1);

        if ((symbol->output_options & BOLD_TEXT) && !upcean) {
            font = "Helvetica-Bold";
        } else {
            font = "Helvetica";
        }
        if (iso_latin1) {
            /* Change encoding to ISO 8859-1, see Postscript Language Reference Manual 2nd Edition Example 5.6 */
            fm_printf(fmp, "/%s findfont\n", font);
            fm_puts("dup length dict begin\n"
                  "{1 index /FID ne {def} {pop pop} ifelse} forall\n"
                  "/Encoding ISOLatin1Encoding def\n"
                  "currentdict\n"
                  "end\n"
                  "/Helvetica-ISOLatin1 exch definefont pop\n", fmp);
            font = "Helvetica-ISOLatin1";
        }
        do {
            ps_convert(string->text, ps_string);
            if (string->fsize != previous_fsize) {
                fm_printf(fmp, "/%s findfont", font);
                /* Compensate for Helvetica being smaller than Zint's OCR-B */
                fm_putsf( " ", 2, upcean ? string->fsize * 1.07f : string->fsize, fmp);
                fm_puts(" scalefont setfont\n", fmp);
                previous_fsize = string->fsize;
            }
            /* Unhack the guard whitespace `gws_left_fudge`/`gws_right_fudge` hack */
            if (upcean && string->halign == 1 && string->text[0] == '<') {
                const float gws_left_fudge = symbol->scale < 0.1f ? 0.1f : symbol->scale; /* 0.5 * 2 * scale */
                fm_putsf(" ", 2, string->x + gws_left_fudge, fmp);
            } else if (upcean && string->halign == 2 && string->text[0] == '>') {
                const float gws_right_fudge = symbol->scale < 0.1f ? 0.1f : symbol->scale; /* 0.5 * 2 * scale */
                fm_putsf(" ", 2, string->x - gws_right_fudge, fmp);
            } else {
                fm_putsf(" ", 2, string->x, fmp);
            }
            fm_putsf(" ", 2, symbol->vector->height - string->y, fmp);
            fm_puts(" moveto\n", fmp);
            if (string->rotation != 0) {
                fm_puts(" gsave\n", fmp);
                fm_printf(fmp, " %d rotate\n", 360 - string->rotation);
            }
            if (string->halign == 0 || string->halign == 2) { /* Need width for middle or right align */
                fm_printf(fmp, " (%s) stringwidth pop" /* Returns "width height" - discard "height" */
                                " %s 0 rmoveto\n", ps_string, string->halign == 2 ? "neg" : "-2 div");
            }
            fm_printf(fmp, " (%s) show\n", ps_string);
            if (string->rotation != 0) {
                fm_puts(" grestore\n", fmp);
            }
            string = string->next;
        } while (string);
    }

    if (fm_error(fmp)) {
        errtxtf(0, symbol, 647, "Incomplete write of EPS output (%1$d: %2$s)", fmp->err, strerror(fmp->err));
        (void) fm_close(fmp, symbol);
        return ZINT_ERROR_FILE_WRITE;
    }

    if (!fm_close(fmp, symbol)) {
        return errtxtf(ZINT_ERROR_FILE_WRITE, symbol, 649, "Failure on closing EPS output file (%1$d: %2$s)",
                        fmp->err, strerror(fmp->err));
    }

    return 0;
}

/* vim: set ts=4 sw=4 et : */
