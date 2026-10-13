/*  output.c - Common routines for raster/vector

    libzint - the open source barcode library
    Copyright (C) 2020-2024 Robin Stuart <rstuart114@gmail.com>

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
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h> /* mkdir(2) */
#endif
#include "common.h"
#include "output.h"

#define OUT_SSET_F  (IS_NUM_F | IS_UHX_F | IS_LHX_F) /* SSET "0123456789ABCDEFabcdef" */

/* Helper to check an individual colour option is good */
static int out_check_colour(struct zint_symbol *symbol, const char *colour, const char *name) {
    const char *comma1, *comma2, *comma3;
    int val;

    if ((comma1 = strchr(colour, ',')) == NULL) {
        const int len = (int) strlen(colour);
        if ((len != 6) && (len != 8)) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 880, "Malformed %s RGB colour (6 or 8 characters only)",
                            name);
        }
        if (not_sane(OUT_SSET_F, (unsigned char *) colour, len)) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 881,
                            "Malformed %1$s RGB colour '%2$s' (hexadecimal only)", name, colour);
        }

        return 0;
    }

    /* CMYK comma-separated percentages */
    if ((comma2 = strchr(comma1 + 1, ',')) == NULL || (comma3 = strchr(comma2 + 1, ',')) == NULL
            || strchr(comma3 + 1, ',') != NULL) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 882,
                        "Malformed %s CMYK colour (4 decimal numbers, comma-separated)", name);
    }
    if (comma1 - colour > 3 || comma2 - (comma1 + 1) > 3 || comma3 - (comma2 + 1) > 3 || strlen(comma3 + 1) > 3) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 883,
                        "Malformed %s CMYK colour (3 digit maximum per number)", name);
    }

    if ((val = to_int((const unsigned char *) colour, (int) (comma1 - colour))) == -1 || val > 100) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 884, "Malformed %s CMYK colour C (decimal 0 to 100 only)",
                        name);
    }
    if ((val = to_int((const unsigned char *) (comma1 + 1), (int) (comma2 - (comma1 + 1)))) == -1 || val > 100) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 885, "Malformed %s CMYK colour M (decimal 0 to 100 only)",
                        name);
    }
    if ((val = to_int((const unsigned char *) (comma2 + 1), (int) (comma3 - (comma2 + 1)))) == -1 || val > 100) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 886, "Malformed %s CMYK colour Y (decimal 0 to 100 only)",
                        name);
    }
    if ((val = to_int((const unsigned char *) (comma3 + 1), (int) strlen(comma3 + 1))) == -1 || val > 100) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 887, "Malformed %s CMYK colour K (decimal 0 to 100 only)",
                        name);
    }

    return 0;
}

/* Check colour options are good (`symbol->fgcolour`, `symbol->bgcolour`) */
INTERNAL int out_check_colour_options(struct zint_symbol *symbol) {

    if (out_check_colour(symbol, symbol->fgcolour, "foreground") != 0) {
        return ZINT_ERROR_INVALID_OPTION;
    }
    if (out_check_colour(symbol, symbol->bgcolour, "background") != 0) {
        return ZINT_ERROR_INVALID_OPTION;
    }

    return 0;
}

/* Return RGB(A) from (well-formed) colour string. Returns 0 if RGB or converted CMYK, 1 if RGBA */
INTERNAL int out_colour_get_rgb(const char *colour, unsigned char *red, unsigned char *green, unsigned char *blue,
                unsigned char *alpha) {
    const char *comma1, *comma2, *comma3;
    int black, val;

    if ((comma1 = strchr(colour, ',')) == NULL) {
        *red = 16 * ctoi(colour[0]) + ctoi(colour[1]);
        *green = 16 * ctoi(colour[2]) + ctoi(colour[3]);
        *blue = 16 * ctoi(colour[4]) + ctoi(colour[5]);
        if (alpha) {
            *alpha = colour[6] ? 16 * ctoi(colour[6]) + ctoi(colour[7]) : 0xFF;
            return colour[6] ? 1 : 0;
        }
        return 0;
    }
    comma2 = strchr(comma1 + 1, ',');
    comma3 = strchr(comma2 + 1, ',');

    black = 100 - to_int((const unsigned char *) (comma3 + 1), (int) strlen(comma3 + 1));

    val = 100 - to_int((const unsigned char *) colour, (int) (comma1 - colour)); /* Cyan */
    *red = (int) round((0xFF * val * black) / 10000.0);

    val = 100 - to_int((const unsigned char *) (comma1 + 1), (int) (comma2 - (comma1 + 1))); /* Magenta */
    *green = (int) round((0xFF * val * black) / 10000.0);

    val = 100 - to_int((const unsigned char *) (comma2 + 1), (int) (comma3 - (comma2 + 1))); /* Yellow */
    *blue = (int) round((0xFF * val * black) / 10000.0);

    if (alpha) {
        *alpha = 0xFF;
    }

    return 0;
}

/* Return CMYK from (well-formed) colour string. Returns 0 if CMYK, 1 if converted RBG, 2 if converted RGBA */
INTERNAL int out_colour_get_cmyk(const char *colour, int *cyan, int *magenta, int *yellow, int *black,
                unsigned char *rgb_alpha) {
    const char *comma1;
    unsigned char red, green, blue, alpha;
    int have_alpha, k;

    if ((comma1 = strchr(colour, ',')) != NULL) {
        const char *const comma2 = strchr(comma1 + 1, ',');
        const char *const comma3 = strchr(comma2 + 1, ',');
        *cyan = to_int((const unsigned char *) colour, (int) (comma1 - colour));
        *magenta = to_int((const unsigned char *) (comma1 + 1), (int) (comma2 - (comma1 + 1)));
        *yellow = to_int((const unsigned char *) (comma2 + 1), (int) (comma3 - (comma2 + 1)));
        *black = to_int((const unsigned char *) (comma3 + 1), (int) strlen(comma3 + 1));
        if (rgb_alpha) {
            *rgb_alpha = 0xFF;
        }
        return 0;
    }
    have_alpha = out_colour_get_rgb(colour, &red, &green, &blue, &alpha);

    k = red;
    if (green > k) {
        k = green;
    }
    if (blue > k) {
        k = blue;
    }
    if (k == 0) {
        *cyan = *magenta = *yellow = 0;
        *black = 100;
    } else {
        *cyan = (int) round((k - red) * 100.0 / k);
        *magenta = (int) round((k - green) * 100.0 / k);
        *yellow = (int) round((k - blue) * 100.0 / k);
        *black = (int) round(((0xFF - k) * 100.0) / 0xFF);
    }

    if (rgb_alpha) {
        *rgb_alpha = have_alpha ? alpha : 0xFF;
    }

    return 1 + have_alpha;
}

/* Convert internal colour chars "WCBMRYGK" to RGB */
INTERNAL int out_colour_char_to_rgb(const char ch, unsigned char *red, unsigned char *green, unsigned char *blue) {
    static const char chars[] = "WCBMRYGK";
    static const unsigned char colours[8][3] = {
        { 0xff, 0xff, 0xff, }, /* White */
        {    0, 0xff, 0xff, }, /* Cyan */
        {    0,    0, 0xff, }, /* Blue */
        { 0xff,    0, 0xff, }, /* Magenta */
        { 0xff,    0,    0, }, /* Red */
        { 0xff, 0xff,    0, }, /* Yellow */
        {    0, 0xff,    0, }, /* Green */
        {    0,    0,    0, }, /* Black */
    };
    int i = posn(chars, ch);
    int ret = i != -1;

    if (i == -1) {
        i = 7; /* Black (zeroize) */
    }
    if (red) {
        *red = colours[i][0];
    }
    if (green) {
        *green = colours[i][1];
    }
    if (blue) {
        *blue = colours[i][2];
    }

    return ret;
}

/* Return minimum quiet zones for each symbology */
static int out_quiet_zones(const struct zint_symbol *symbol, const int hide_text, const int comp_xoffset,
                            float *left, float *right, float *top, float *bottom) {
    int done = 0;

    *left = *right = *top = *bottom = 0.0f;

    /* These always have quiet zones set (previously used whitespace_width) */
    switch (symbol->symbology) {
        case BARCODE_CODE16K:
            /* BS EN 12323:2005 Section 4.5 (c) */
            if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                *left = 10.0f;
                *right = 1.0f;
            }
            done = 1;
            break;
        case BARCODE_CODE49:
            /* ANSI/AIM BC6-2000 Section 2.4 */
            if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                *left = 10.0f;
                *right = 1.0f;
            }
            done = 1;
            break;
        case BARCODE_CODABLOCKF:
        case BARCODE_HIBC_BLOCKF:
            /* AIM ISS-X-24 Section 4.6.1 */
            if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                *left = *right = 10.0f;
            }
            done = 1;
            break;
        case BARCODE_ITF14:
            /* GS1 General Specifications 21.0.1 Section 5.3.2.2 */
            if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                *left = *right = 10.0f;
            }
            done = 1;
            break;
        case BARCODE_EANX:
        case BARCODE_EANX_CHK:
        case BARCODE_EANX_CC:
        case BARCODE_ISBNX:
            /* GS1 General Specifications 21.0.1 Section 5.2.3.4 */
            switch (ustrlen(symbol->text)) {
                case 13: /* EAN-13/ISBN */
                    if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                        *left = comp_xoffset >= 10 ? 1.0f : 11.0f - comp_xoffset; /* Need at least 1X for CC-A/B */
                        *right = 7.0f - (comp_xoffset != 0);
                    } else if (!hide_text) {
                        *left = comp_xoffset >= 10 ? 1.0f : 11.0f - comp_xoffset; /* Need for outside left digit */
                    }
                    break;
                case 16: /* EAN-13/ISBN + 2 digit add-on */
                case 19: /* EAN-13/ISBN + 5 digit add-on */
                    if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                        *left = comp_xoffset >= 10 ? 1.0f : 11.0f - comp_xoffset; /* Need at least 1X for CC-A/B */
                        *right = 5.0f;
                    } else if (!hide_text) {
                        *left = comp_xoffset >= 10 ? 1.0f : 11.0f - comp_xoffset; /* Need for outside left digit */
                    }
                    break;
                case 5: /* EAN-5 add-on */
                case 2: /* EAN-2 add-on */
                    if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                        *right = 5.0f;
                    }
                    break;
                case 8: /* EAN-8 */
                    if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                        *left = comp_xoffset >= 6 ? 1.0f : 7.0f - comp_xoffset; /* Need at least 1X for CC-A/B */
                        *right = 7.0f - (comp_xoffset != 0);
                    }
                    break;
                default: /* EAN-8 + 2/5 digit add-on */
                    if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                        *left = comp_xoffset >= 6 ? 1.0f : 7.0f - comp_xoffset; /* Need at least 1X for CC-A/B */
                        *right = 5.0f;
                    }
                    break;
            }
            done = 1;
            break;
        case BARCODE_UPCA:
        case BARCODE_UPCA_CHK:
        case BARCODE_UPCA_CC:
            /* GS1 General Specifications 21.0.1 Section 5.2.3.4 */
            if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                *left = comp_xoffset >= 8 ? 1.0f : 9.0f - comp_xoffset; /* Need at least 1X for CC-A/B */
                if (ustrlen(symbol->text) > 12) { /* UPC-A + add-on */
                    *right = 5.0f;
                } else {
                    *right = 9.0f - (comp_xoffset != 0);
                }
            } else if (!hide_text) {
                *left = comp_xoffset >= 8 ? 1.0f : 9.0f - comp_xoffset; /* Need for outside left digit */
                if (ustrlen(symbol->text) <= 12) { /* No add-on */
                    *right = 9.0f - (comp_xoffset != 0); /* Need for outside right digit */
                }
            }
            done = 1;
            break;
        case BARCODE_UPCE:
        case BARCODE_UPCE_CHK:
        case BARCODE_UPCE_CC:
            /* GS1 General Specifications 21.0.1 Section 5.2.3.4 */
            if (!(symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
                *left = comp_xoffset >= 8 ? 1.0f : 9.0f - comp_xoffset;
                if (ustrlen(symbol->text) > 8) { /* UPC-E + add-on */
                    *right = 5.0f;
                } else {
                    *right = 7.0f - (comp_xoffset != 0);
                }
            } else if (!hide_text) {
                *left = comp_xoffset >= 8 ? 1.0f : 9.0f - comp_xoffset; /* Need for outside left digit */
                if (ustrlen(symbol->text) <= 8) { /* No add-on */
                    *right = 7.0f - (comp_xoffset != 0); /* Need for outside right digit */
                }
            }
            done = 1;
            break;
    }

    if (done) {
        return done;
    }

    /* Only do others if flag set */
    if (!(symbol->output_options & BARCODE_QUIET_ZONES) || (symbol->output_options & BARCODE_NO_QUIET_ZONES)) {
        return 0;
    }

    switch (symbol->symbology) {
        case BARCODE_CODE11:
            /* No known standard. Following ITF-14, set to 10X */
            *left = *right = 10.0f;
            done = 1;
            break;
        
        case BARCODE_DXFILMEDGE:
            /* No known standard. Add a little horizontal space to make the detection easier. Tested with Zxing-CPP */
            *left = *right = 1.8f;
            done = 1;
            break;

        case BARCODE_C25INTER:
            /* ISO/IEC 16390:2007 Section 4.4 10X */
            *left = *right = 10.0f;
            done = 1;
            break;
        case BARCODE_C25STANDARD:
        case BARCODE_C25IATA:
        case BARCODE_C25LOGIC:
        case BARCODE_C25IND:
            /* No known standards. Following C25INTER, set to 10X */
            *left = *right = 10.0f;
            done = 1;
            break;

        case BARCODE_CODE39:
        case BARCODE_EXCODE39:
        case BARCODE_LOGMARS:
        case BARCODE_PZN:
        case BARCODE_VIN:
        case BARCODE_HIBC_39:
        case BARCODE_CODE32:
            /* ISO/IEC 16388:2007 Section 4.4 (d) */
            *left = *right = 10.0f;
            done = 1;
            break;
        case BARCODE_GS1_128: /* GS1-128 */
        case BARCODE_EAN14:
            /* GS1 General Specifications 21.0.1 Section 5.4.4.2 */
            *left = *right = 10.0f;
            done = 1;
            break;
        case BARCODE_GS1_128_CC:
            /* GS1 General Specifications 21.0.1 Sections 5.11.2.1 (CC-A), 5.11.2.2 (CC-B) & 5.11.2.3 (CC-C) */
            {
                int comp_roffset = 0; /* Right offset of linear */
                int min_qz; /* Minimum quiet zone - 1X for CC-A/B, 2X for CC-C */
                int x;
                for (x = symbol->width - 1; x >= 0 && !module_is_set(symbol, symbol->rows - 1, x); x--) {
                    comp_roffset++;
                }
                /* Determine if CC-C by counting initial start pattern */
                for (x = 0; x < 8 && module_is_set(symbol, 0, x); x++);
                min_qz = x == 8 ? 2 : 1;
                *left = comp_xoffset >= 10 - min_qz ? min_qz : 10.0f - comp_xoffset;
                *right = comp_roffset >= 10 - min_qz ? min_qz : 10.0f - comp_roffset;
            }
            done = 1;
            break;
        case BARCODE_CODABAR:
            /* BS EN 798:1995 Section 4.4.1 (d) */
            *left = *right = 10.0f;
            done = 1;
            break;
        case BARCODE_CODE128:
        case BARCODE_CODE128AB:
        case BARCODE_HIBC_128:
        case BARCODE_NVE18:
            /* ISO/IEC 15417:2007 4.4.2 */
            *left = *right = 10.0f;
            done = 1;
            break;
        case BARCODE_DPLEIT:
        case BARCODE_DPIDENT:
            /* Using C25INTER values TODO: Find doc */
            *left = *right = 10.0f;
            done = 1;
            break;

        case BARCODE_CODE93:
            /* ANSI/AIM BC5-1995 Section 2.4 */
            *left = *right = 10.0f;
            done = 1;
            break;

        case BARCODE_FLAT:
            /* TODO: Find doc (application defined according to TEC-IT) */
            break;

        case BARCODE_DBAR_OMN: /* GS1 Databar Omnidirectional */
        case BARCODE_DBAR_LTD: /* GS1 Databar Limited */
        case BARCODE_DBAR_EXP: /* GS1 Databar Expanded */
        case BARCODE_DBAR_STK: /* GS1 DataBar Stacked */
        case BARCODE_DBAR_OMNSTK: /* GS1 DataBar Stacked Omnidirectional */
        case BARCODE_DBAR_EXPSTK: /* GS1 Databar Expanded Stacked */
            /* GS1 General Specifications 21.0.1 Section 5.5.1.1 - Quiet Zones: None required */
            done = 1;
            break;

            /* GS1 General Specifications 21.0.1 Sections 5.11.2.1 (CC-A) & 5.11.2.2 (CC-B) require 1X either side
               but this may be supplied by the positioning of the linear component */
        case BARCODE_DBAR_OMN_CC:
        case BARCODE_DBAR_LTD_CC:
            /* Always have at least 1X to right of CC-A/B */
            if (comp_xoffset > 1) { /* Exclude DBAR_LTD_CC with CC-A which always has 1X to left */
                *left = 1.0f;
            }
            done = 1;
            break;
        case BARCODE_DBAR_STK_CC:
        case BARCODE_DBAR_OMNSTK_CC:
            /* Always have at least 1X to left of CC-A/B */
            *right = 1.0f;
            done = 1;
            break;
        case BARCODE_DBAR_EXP_CC:
        case BARCODE_DBAR_EXPSTK_CC:
            /* Always have at least 1X to left and right of CC-A/B */
            done = 1;
            break;

        case BARCODE_TELEPEN:
        case BARCODE_TELEPEN_NUM:
            /* Appears to be ~10X from diagram in Telepen Barcode Symbology information and History */
            /* TODO: Find better doc */
            *left = *right = 10.0f;
            done = 1;
            break;

        case BARCODE_POSTNET:
        case BARCODE_PLANET:
            /* USPS DMM 300 2006 (2011) 5.7 Barcode in Address Block
               left/right 0.125" / 0.025" (X max) = 5, top/bottom 0.04" / 0.025" (X max) = 1.6 */
            *left = *right = 5.0f;
            *top = *bottom = 1.6f;
            done = 1;
            break;
        case BARCODE_CEPNET:
            /* CEPNet e Código Bidimensional Datamatrix 2D (26/05/2021) 3.8 Localização */
            *left = *right = 10.0f;
            *top = *bottom = 1.6f; /* As POSTNET (1.016mm == 0.025") */
            done = 1;
            break;

        case BARCODE_MSI_PLESSEY:
            /* TODO Find doc (TEC-IT says 12X so use that for the moment) */
            *left = *right = 12.0f;
            done = 1;
            break;

        case BARCODE_FIM:
            /* USPS DMM 300 2006 (2011) 708.9.3 (top/bottom zero)
               right 0.125" (min) / 0.03925" (X max) ~ 3.18, left 1.25" - 0.66725" (max width of barcode)
               - 0.375 (max right) = 0.20775" / 0.03925" (X max) ~ 5.29 */
            *right = 3.18471336f; /* 0.125 / 0.03925 */
            *left = 5.29299355f; /* 0.20775 / 0.03925 */
            done = 1;
            break;
        case BARCODE_PHARMA:
        case BARCODE_PHARMA_TWO:
            /* Laetus Pharmacode Guide 2.2 from 6mm depending on context, 6mm / 1mm (Pharma Two X) = 6 */
            *left = *right = 6.0f;
            done = 1;
            break;
        case BARCODE_PDF417:
        case BARCODE_PDF417COMP:
        case BARCODE_HIBC_PDF:
            /* ISO/IEC 15438:2015 Section 5.8.3 */
            *left = *right = *top = *bottom = 2.0f;
            done = 1;
            break;
        case BARCODE_MICROPDF417:
        case BARCODE_HIBC_MICPDF:
            /* ISO/IEC 24728:2006 Section 5.8.3 */
            *left = *right = *top = *bottom = 1.0f;
            done = 1;
            break;
        case BARCODE_MAXICODE:
            /* ISO/IEC 16023:2000 Section 4.11.5 */
            *left = *right = *top = *bottom = 1.0f;
            done = 1;
            break;
        case BARCODE_QRCODE:
        case BARCODE_UPNQR:
        case BARCODE_HIBC_QR:
            /* ISO/IEC 18004:2015 Section 9.1 */
            *left = *right = *top = *bottom = 4.0f;
            done = 1;
            break;
        case BARCODE_DPD:
            /* DPD Parcel Label Specification Version 2.4.1 Section 4.6.1.2, 5mm / 0.4mm (X max) = 12.5 */
            *left = *right = 12.5f;
            done = 1;
            break;
        case BARCODE_MICROQR:
            /* ISO/IEC 18004:2015 Section 9.1 */
            *left = *right = *top = *bottom = 2.0f;
            done = 1;
            break;
        case BARCODE_RMQR:
            /* ISO/IEC JTC1/SC31N000 Section 6.3.10 */
            *left = *right = *top = *bottom = 2.0f;
            done = 1;
            break;
        case BARCODE_AUSPOST:
        case BARCODE_AUSREPLY:
        case BARCODE_AUSROUTE:
        case BARCODE_AUSREDIRECT:
            /* Customer Barcode Technical Specifications (2012) left/right 6mm / 0.6mm = 10,
               top/bottom 2mm / 0.6mm ~ 3.33 (X max) */
            *left = *right = 10.0f;
            *top = *bottom = 3.33333325f; /* 2.0 / 0.6 */
            done = 1;
            break;
        case BARCODE_RM4SCC:
            /* Royal Mail Know How User's Manual Appendix C: using CBC, same as MAILMARK_4S, 2mm all round,
               use X max (25.4mm / 39) i.e. 20 bars per 25.4mm */
            *left = *right = *top = *bottom = 3.07086611f; /* (2.0 * 39.0) / 25.4 */
            done = 1;
            break;
        case BARCODE_DATAMATRIX:
        case BARCODE_HIBC_DM:
            /* ISO/IEC 16022:2006 Section 7.1 */
            *left = *right = *top = *bottom = 1.0f;
            done = 1;
            break;
        case BARCODE_JAPANPOST:
            /* Japan Post Zip/Barcode Manual p.13 2mm all round, X 0.6mm, 2mm / 0.6mm ~ 3.33 */
            *left = *right = *top = *bottom = 3.33333325f; /* 2.0 / 0.6 */
            done = 1;
            break;

        case BARCODE_KOREAPOST:
            /* TODO Find doc (TEC-IT uses 10X but says not exactly specified - do the same for the moment) */
            *left = *right = 10.0f;
            done = 1;
            break;

        case BARCODE_USPS_IMAIL:
            /* USPS-B-3200 (2015) Section 2.3.2 left/right 0.125", top/bottom 0.026", use X max (1 / 39)
               i.e. 20 bars per inch */
            *left = *right = 4.875f; /* 0.125 * 39.0 */
            *top = *bottom = 1.01400006f; /* 0.026 * 39.0 */
            done = 1;
            break;

        case BARCODE_PLESSEY:
            /* TODO Find doc (see MSI_PLESSEY) */
            *left = *right = 12.0f;
            done = 1;
            break;

        case BARCODE_KIX:
            /* Handleiding KIX code brochure - same as RM4SCC/MAILMARK_4S */
            *left = *right = *top = *bottom = 3.07086611f; /* (2.0 * 39.0) / 25.4 */
            done = 1;
            break;
        case BARCODE_AZTEC:
        case BARCODE_HIBC_AZTEC:
        case BARCODE_AZRUNE:
            /* ISO/IEC 24778:2008 Section 4.1 (c) & Annex A.1 (Rune) - no quiet zone required */
            done = 1;
            break;
        case BARCODE_DAFT:
            /* Generic so unlikely to be defined */
            done = 1;
            break;
        case BARCODE_DOTCODE:
            /* ISS DotCode Rev. 4.0 Section 4.1 (3) (c) */
            *left = *right = *top = *bottom = 3.0f;
            done = 1;
            break;
        case BARCODE_HANXIN:
            /* ISO/IEC DIS 20830:2019 Section 4.2.8 (also Section 6.2) */
            *left = *right = *top = *bottom = 3.0f;
            done = 1;
            break;
        case BARCODE_MAILMARK_4S:
            /* Royal Mail Mailmark Barcode Definition Document Section 3.5.2, 2mm all round, use X max (25.4mm / 39)
               i.e. 20 bars per 25.4mm */
            *left = *right = *top = *bottom = 3.07086611f; /* (2.0 * 39.0) / 25.4 */
            done = 1;
            break;
        case BARCODE_UPU_S10:
            /* Universal Postal Union S10 Section 8 */
            *left = *right = 10.0f;
            done = 1;
            break;
        case BARCODE_MAILMARK_2D:
            /* Royal Mail Mailmark Barcode Definition Document, Section 2.4 */
            *left = *right = *top = *bottom = 4.0f;
            done = 1;
            break;
        case BARCODE_CHANNEL:
            /* ANSI/AIM BC12-1998 Section 4.4 (c) */
            *left = 1.0f;
            *right = 2.0f;
            done = 1;
            break;

        case BARCODE_CODEONE:
            /* USS Code One AIM 1994 Section 2.2.4 No quiet zone required for Versions A to H */
            if (symbol->option_2 == 9 || symbol->option_2 == 10) { /* Section 2.3.2 Versions S & T */
                *left = *right = *bottom = 1.0f;
            }
            done = 1;
            break;

        case BARCODE_GRIDMATRIX:
            /* AIMD014 (v 1.63) Section 7.1 */
            *left = *right = *top = *bottom = 6.0f;
            done = 1;
            break;
        case BARCODE_ULTRA:
            /* AIMD/TSC15032-43 (v 0.99c) Section 9.2 */
            *left = *right = *top = *bottom = 1.0f;
            done = 1;
            break;

        case BARCODE_BC412:
            /* SEMI T1-95 Table 4 */
            *left = *right = 10.0f;
            done = 1;
            break;
    }

    return done; /* For self-checking */
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int out_quiet_zones_test(const struct zint_symbol *symbol, const int hide_text, const int comp_xoffset,
                            float *left, float *right, float *top, float *bottom) {
    return out_quiet_zones(symbol, hide_text, comp_xoffset, left, right, top, bottom);
}
#endif

/* Set left (x), top (y), right and bottom offsets for whitespace, also right quiet zone */
INTERNAL void out_set_whitespace_offsets(const struct zint_symbol *symbol, const int hide_text,
                const int comp_xoffset, float *p_xoffset, float *p_yoffset, float *p_roffset, float *p_boffset,
                float *p_qz_right, const float scaler, int *p_xoffset_si, int *p_yoffset_si, int *p_roffset_si,
                int *p_boffset_si, int *p_qz_right_si) {
    float qz_left, qz_right, qz_top, qz_bottom;

    out_quiet_zones(symbol, hide_text, comp_xoffset, &qz_left, &qz_right, &qz_top, &qz_bottom);

    *p_xoffset = symbol->whitespace_width + qz_left;
    *p_roffset = symbol->whitespace_width + qz_right;
    if (symbol->output_options & BARCODE_BOX) {
        *p_xoffset += symbol->border_width;
        *p_roffset += symbol->border_width;
    }

    *p_yoffset = symbol->whitespace_height + qz_top;
    *p_boffset = symbol->whitespace_height + qz_bottom;
    if (symbol->output_options & (BARCODE_BOX | BARCODE_BIND | BARCODE_BIND_TOP)) {
        *p_yoffset += symbol->border_width;
        if (!(symbol->output_options & BARCODE_BIND_TOP)) { /* Trumps BARCODE_BOX & BARCODE_BIND */
            *p_boffset += symbol->border_width;
        }
    }

    if (p_qz_right) {
        *p_qz_right = qz_right;
    }

    if (scaler) {
        if (p_xoffset_si) {
            *p_xoffset_si = (int) (*p_xoffset * scaler);
        }
        if (p_yoffset_si) {
            *p_yoffset_si = (int) (*p_yoffset * scaler);
        }
        if (p_roffset_si) {
            *p_roffset_si = (int) (*p_roffset * scaler);
        }
        if (p_boffset_si) {
            *p_boffset_si = (int) (*p_boffset * scaler);
        }
        if (p_qz_right_si) {
            *p_qz_right_si = (int) (qz_right * scaler);
        }
    }
}

/* Set composite offset and main width excluding add-on (for start of add-on calc) and add-on text, returning
   EAN/UPC type */
INTERNAL int out_process_upcean(const struct zint_symbol *symbol, const int comp_xoffset, int *p_main_width,
                unsigned char addon[6], int *p_addon_len, int *p_addon_gap) {
    int main_width; /* Width of main linear symbol, excluding add-on */
    int upceanflag; /* EAN/UPC type flag */
    int i, j, latch;
    const int text_length = (int) ustrlen(symbol->text);

    latch = 0;
    j = 0;
    /* Isolate add-on text */
    for (i = 6; i < text_length && j < 5; i++) {
        if (latch == 1) {
            /* Use dummy space-filled add-on if no hrt */
            addon[j] = symbol->show_hrt ? symbol->text[i] : ' ';
            j++;
        } else if (symbol->text[i] == '+') {
            latch = 1;
        }
    }
    addon[j] = '\0';
    if (latch) {
        *p_addon_len = (int) ustrlen(addon);
        if (symbol->symbology == BARCODE_UPCA || symbol->symbology == BARCODE_UPCA_CHK
                || symbol->symbology == BARCODE_UPCA_CC) {
            *p_addon_gap = symbol->option_2 >= 9 && symbol->option_2 <= 12 ? symbol->option_2 : 9;
        } else {
            *p_addon_gap = symbol->option_2 >= 7 && symbol->option_2 <= 12 ? symbol->option_2 : 7;
        }
    }

    upceanflag = 0;
    main_width = symbol->width;
    if ((symbol->symbology == BARCODE_EANX) || (symbol->symbology == BARCODE_EANX_CHK)
            || (symbol->symbology == BARCODE_EANX_CC) || (symbol->symbology == BARCODE_ISBNX)) {
        switch (text_length) {
            case 13: /* EAN-13 */
            case 16: /* EAN-13 + EAN-2 */
            case 19: /* EAN-13 + EAN-5 */
                main_width = 95 + comp_xoffset; /* EAN-13 main symbol 95 modules wide */
                upceanflag = 13;
                break;
            case 2:
                /* EAN-2 can't have add-on or be composite */
                upceanflag = 2;
                break;
            case 5:
                /* EAN-5 can't have add-on or be composite */
                upceanflag = 5;
                break;
            default:
                main_width = 68 + comp_xoffset; /* EAN-8 main symbol 68 modules wide */
                upceanflag = 8;
                break;
        }
    } else if ((symbol->symbology == BARCODE_UPCA) || (symbol->symbology == BARCODE_UPCA_CHK)
            || (symbol->symbology == BARCODE_UPCA_CC)) {
        main_width = 95 + comp_xoffset; /* UPC-A main symbol 95 modules wide */
        upceanflag = 12;
    } else if ((symbol->symbology == BARCODE_UPCE) || (symbol->symbology == BARCODE_UPCE_CHK)
            || (symbol->symbology == BARCODE_UPCE_CC)) {
        main_width = 51 + comp_xoffset; /* UPC-E main symbol 51 modules wide */
        upceanflag = 6;
    }

    *p_main_width = main_width;

    return upceanflag;
}

/* Calculate large bar height i.e. linear bars with zero row height that respond to the symbol height.
   If scaler `si` non-zero (raster), then large_bar_height if non-zero or else row heights will be rounded
   to nearest pixel and symbol height adjusted */
INTERNAL float out_large_bar_height(struct zint_symbol *symbol, const int si, int *row_heights_si,
                int *symbol_height_si) {
    float fixed_height = 0.0f;
    int zero_count = 0;
    int round_rows = 0;
    int i;
    float large_bar_height = 0.0f; /* Not used if zero_count zero */

    if (si) {
        for (i = 0; i < symbol->rows; i++) {
            if (symbol->row_height[i]) {
                fixed_height += symbol->row_height[i];
                if (!round_rows && !isfintf(symbol->row_height[i] * si)) {
                    round_rows = 1;
                }
            } else {
                zero_count++;
            }
        }

        if (zero_count) {
            large_bar_height = stripf((symbol->height - fixed_height) / zero_count);
            assert(large_bar_height >= 0.5f); /* Min row height as set by `set_height()` */
            if (!isfintf(large_bar_height * si)) {
                large_bar_height = stripf(roundf(large_bar_height * si) / si);
            }
            symbol->height = stripf(large_bar_height * zero_count + fixed_height);
            /* Note should never happen that have both zero_count and round_rows */
        } else {
            if (round_rows) {
                float total_height = 0.0f;
                for (i = 0; i < symbol->rows; i++) {
                    if (!isfintf(symbol->row_height[i] * si)) {
                        symbol->row_height[i] = roundf(symbol->row_height[i] * si) / si;
                    }
                    total_height += symbol->row_height[i];
                }
                symbol->height = stripf(total_height);
            }
        }

        if (row_heights_si) {
            assert(symbol_height_si);
            *symbol_height_si = 0;
            for (i = 0; i < symbol->rows; i++) {
                if (symbol->row_height[i]) {
                    row_heights_si[i] = (int) roundf(symbol->row_height[i] * si);
                } else {
                    row_heights_si[i] = (int) roundf(large_bar_height * si);
                }
                *symbol_height_si += row_heights_si[i];
            }
        }
    } else {
        for (i = 0; i < symbol->rows; i++) {
            if (symbol->row_height[i]) {
                fixed_height += symbol->row_height[i];
            } else {
                zero_count++;
            }
        }
        if (zero_count) {
            large_bar_height = stripf((symbol->height - fixed_height) / zero_count);
            assert(large_bar_height >= 0.5f); /* Min row height as set by `set_height()` */
            symbol->height = stripf(large_bar_height * zero_count + fixed_height);
        }
    }

    return large_bar_height;
}

#ifdef _WIN32
/* Convert UTF-8 to Windows wide chars. Ticket #288, props Marcel */
#define utf8_to_wide(u, w, r) \
    { \
        int lenW; /* Includes NUL terminator */ \
        if ((lenW = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u, -1, NULL, 0)) == 0) return r; \
        w = (wchar_t *) z_alloca(sizeof(wchar_t) * lenW); \
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u, -1, w, lenW) == 0) return r; \
    }

/* Do `fopen()` on Windows, assuming `filename` is UTF-8 encoded. Ticket #288, props Marcel */
INTERNAL FILE *out_win_fopen(const char *filename, const char *mode) {
    wchar_t *filenameW, *modeW;

    utf8_to_wide(filename, filenameW, NULL);
    utf8_to_wide(mode, modeW, NULL);

    return _wfopen(filenameW, modeW);
}
#endif

/* Make a directory; already existing dir okay */
/* Adapted from https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950 and
   https://nachtimwald.com/2019/07/10/recursive-create-directory-in-c-revisited/ */
static int out_maybe_mkdir(const char *path) {
#ifdef _WIN32
    DWORD dwAttrib;
    wchar_t *pathW;

    /* Assumes `path` is UTF-8 encoded */
    utf8_to_wide(path, pathW, 0);

    /* Try to make the directory */
    if (CreateDirectoryW(pathW, NULL) != 0) { /* Non-zero on success */
        return 0;
    }
    /* If it fails for any reason but already exists, fail */
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return -1;
    }
    /* Check if the existing path is a directory */
    if ((dwAttrib = GetFileAttributesW(pathW)) == (DWORD) -1 || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return -1;
    }
#else
    struct stat st;

    /* Try to make the directory */
    if (mkdir(path, 0777) == 0) {
        return 0;
    }
    /* If it fails for any reason but already exists, fail */
    if (errno != EEXIST) {
        return -1;
    }
    /* Check if the existing path is a directory */
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return -1;
    }
#endif

    return 0;
}

/* Create output file, creating sub-directories if necessary. Returns `fopen()` FILE pointer */
INTERNAL FILE *out_fopen(const char filename[256], const char *mode) {
    FILE *outfile;

#ifdef _WIN32
    if (!(outfile = out_win_fopen(filename, mode))) {
#else
    if (!(outfile = fopen(filename, mode))) {
#endif
        char dirname[256];
        char *d;
#ifdef _WIN32
        char *dirend = strrchr(filename, '\\');
        if (!dirend) {
            dirend = strrchr(filename, '/');
        }
#else
        char *dirend = strrchr(filename, '/');
#endif
        if (!dirend) {
            return outfile;
        }

        /* Adapted from https://gist.github.com/JonathonReinhart/8c0d90191c38af2dcadb102c4e202950 */
        /* Remove filename, leaving directories */
        memcpy(dirname, filename, dirend - filename);
        dirname[dirend - filename] = '/';
        dirname[dirend - filename + 1] = '\0';
#ifdef _WIN32
        for (d = dirname; *d; d++) { /* Convert to Unix separators */
            if (*d == '\\') {
                *d = '/';
            }
        }
#endif
        for (d = dirname + 1; *d; d++) { /* Ignore slash at start if any */
            if (*d == '/' && *(d - 1) != '/') { /* Ignore double-slashes */
                *d = '\0'; /* Temporarily truncate */
                if (out_maybe_mkdir(dirname) != 0) {
                    return NULL;
                }
                *d = '/'; /* Restore */
            }
        }
#ifdef _WIN32
        outfile = out_win_fopen(filename, mode);
#else
        outfile = fopen(filename, mode);
#endif
    }

    return outfile;
}

/* vim: set ts=4 sw=4 et : */
