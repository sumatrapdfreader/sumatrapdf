/*  library.c - external functions of libzint */
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
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "eci.h"
#include "gs1.h"
#include "output.h"
#include "zfiletypes.h"

/* It's assumed that int is at least 32 bits, the following will compile-time fail if not
 * https://stackoverflow.com/a/1980056 */
typedef char static_assert_int_at_least_32bits[sizeof(int) * CHAR_BIT < 32 ? -1 : 1];

typedef char static_assert_uint16_is_16bits[sizeof(uint16_t) * CHAR_BIT != 16 ? -1 : 1];
typedef char static_assert_int32_is_32bits[sizeof(int32_t) * CHAR_BIT != 32 ? -1 : 1];
typedef char static_assert_uint32_is_32bits[sizeof(uint32_t) * CHAR_BIT != 32 ? -1 : 1];
typedef char static_assert_uint64_at_least_64bits[sizeof(uint64_t) * CHAR_BIT < 64 ? -1 : 1];

/* Set `symbol` to defaults (does not zeroize) */
static void set_symbol_defaults(struct zint_symbol *symbol) {

    symbol->symbology = BARCODE_CODE128;
    symbol->scale = 1.0f;
    strcpy(symbol->fgcolour, "000000");
    symbol->fgcolor = &symbol->fgcolour[0];
    strcpy(symbol->bgcolour, "ffffff");
    symbol->bgcolor = &symbol->bgcolour[0];
#ifdef ZINT_NO_PNG
    strcpy(symbol->outfile, "out.gif");
#else
    strcpy(symbol->outfile, "out.png");
#endif
    symbol->option_1 = -1;
    symbol->show_hrt = 1; /* Show human readable text */
    symbol->input_mode = DATA_MODE;
    symbol->eci = 0; /* Default 0 uses ECI 3 */
    symbol->dot_size = 0.8f; /* 0.4 / 0.5 */
    symbol->text_gap = 1.0f;
    symbol->guard_descent = 5.0f;
    symbol->warn_level = WARN_DEFAULT;
    symbol->bitmap = NULL;
    symbol->alphamap = NULL;
    symbol->vector = NULL;
    symbol->memfile = NULL;
}

/* Create and initialize a symbol structure */
struct zint_symbol *ZBarcode_Create(void) {
    struct zint_symbol *symbol;

    symbol = (struct zint_symbol *) calloc(1, sizeof(*symbol));
    if (!symbol) return NULL;

    set_symbol_defaults(symbol);

    return symbol;
}

INTERNAL void vector_free(struct zint_symbol *symbol); /* Free vector structures */

/* Free any output buffers that may have been created and initialize output fields */
void ZBarcode_Clear(struct zint_symbol *symbol) {
    int i;

    if (!symbol) return;

    for (i = 0; i < symbol->rows; i++) {
        memset(symbol->encoded_data[i], 0, sizeof(symbol->encoded_data[0]));
    }
    symbol->rows = 0;
    symbol->width = 0;
    memset(symbol->row_height, 0, sizeof(symbol->row_height));
    memset(symbol->text, 0, sizeof(symbol->text));
    symbol->errtxt[0] = '\0';
    if (symbol->bitmap != NULL) {
        free(symbol->bitmap);
        symbol->bitmap = NULL;
    }
    if (symbol->alphamap != NULL) {
        free(symbol->alphamap);
        symbol->alphamap = NULL;
    }
    symbol->bitmap_width = 0;
    symbol->bitmap_height = 0;
    if (symbol->memfile != NULL) {
        free(symbol->memfile);
        symbol->memfile = NULL;
    }
    symbol->memfile_size = 0;

    /* If there is a rendered version, ensure its memory is released */
    vector_free(symbol);
}

/* Free any output buffers that may have been created and reset all fields to defaults */
void ZBarcode_Reset(struct zint_symbol *symbol) {
    if (!symbol) return;

    if (symbol->bitmap != NULL)
        free(symbol->bitmap);
    if (symbol->alphamap != NULL)
        free(symbol->alphamap);
    if (symbol->memfile != NULL)
        free(symbol->memfile);

    vector_free(symbol);

    memset(symbol, 0, sizeof(*symbol));
    set_symbol_defaults(symbol);
}

/* Free a symbol structure, including any output buffers */
void ZBarcode_Delete(struct zint_symbol *symbol) {
    if (!symbol) return;

    if (symbol->bitmap != NULL)
        free(symbol->bitmap);
    if (symbol->alphamap != NULL)
        free(symbol->alphamap);
    if (symbol->memfile != NULL)
        free(symbol->memfile);

    vector_free(symbol);

    free(symbol);
}

/* Symbology handlers */
INTERNAL int eanx(struct zint_symbol *symbol, unsigned char source[], int length); /* EAN system barcodes */
INTERNAL int code39(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 3 from 9 (or Code 39) */
INTERNAL int pzn(struct zint_symbol *symbol, unsigned char source[], int length); /* Pharmazentral Nummer (PZN) */
/* Extended Code 3 from 9 (or Code 39+) */
INTERNAL int excode39(struct zint_symbol *symbol, unsigned char source[], int length);
/* Codabar - a simple substitution cipher */
INTERNAL int codabar(struct zint_symbol *symbol, unsigned char source[], int length);
/* Code 2 of 5 Standard (& Matrix) */
INTERNAL int c25standard(struct zint_symbol *symbol, unsigned char source[], int length);
INTERNAL int c25ind(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 Industrial */
INTERNAL int c25iata(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 IATA */
INTERNAL int c25inter(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 Interleaved */
INTERNAL int c25logic(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 Data Logic */
INTERNAL int itf14(struct zint_symbol *symbol, unsigned char source[], int length); /* ITF-14 */
INTERNAL int dpleit(struct zint_symbol *symbol, unsigned char source[], int length); /* Deutsche Post Leitcode */
INTERNAL int dpident(struct zint_symbol *symbol, unsigned char source[], int length); /* Deutsche Post Identcode */
/* Code 93 - a re-working of Code 39+, generates 2 check digits */
INTERNAL int code93(struct zint_symbol *symbol, unsigned char source[], int length);
INTERNAL int code128(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 128 and NVE-18 */
INTERNAL int gs1_128(struct zint_symbol *symbol, unsigned char source[], int length); /* EAN-128 (GS1-128) */
INTERNAL int code11(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 11 */
INTERNAL int msi_plessey(struct zint_symbol *symbol, unsigned char source[], int length); /* MSI Plessey */
INTERNAL int telepen(struct zint_symbol *symbol, unsigned char source[], int length); /* Telepen ASCII */
INTERNAL int telepen_num(struct zint_symbol *symbol, unsigned char source[], int length); /* Telepen Numeric */
INTERNAL int plessey(struct zint_symbol *symbol, unsigned char source[], int length); /* Plessey Code */
INTERNAL int pharma(struct zint_symbol *symbol, unsigned char source[], int length); /* Pharmacode One Track */
INTERNAL int flat(struct zint_symbol *symbol, unsigned char source[], int length); /* Flattermarken */
INTERNAL int fim(struct zint_symbol *symbol, unsigned char source[], int length); /* Facing Identification Mark */
INTERNAL int pharma_two(struct zint_symbol *symbol, unsigned char source[], int length); /* Pharmacode Two Track */
INTERNAL int postnet(struct zint_symbol *symbol, unsigned char source[], int length); /* Postnet */
INTERNAL int planet(struct zint_symbol *symbol, unsigned char source[], int length); /* PLANET */
/* Intelligent Mail (aka USPS OneCode) */
INTERNAL int usps_imail(struct zint_symbol *symbol, unsigned char source[], int length);
INTERNAL int rm4scc(struct zint_symbol *symbol, unsigned char source[], int length); /* RM4SCC */
INTERNAL int auspost(struct zint_symbol *symbol, unsigned char source[], int length); /* Australia Post 4-state */
INTERNAL int code16k(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 16k */
INTERNAL int pdf417(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* PDF417 */
INTERNAL int micropdf417(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* Micro PDF417 */
INTERNAL int maxicode(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* Maxicode */
INTERNAL int dbar_omn(struct zint_symbol *symbol, unsigned char source[], int length); /* DataBar Omnidirectional */
INTERNAL int dbar_ltd(struct zint_symbol *symbol, unsigned char source[], int length); /* DataBar Limited */
INTERNAL int dbar_exp(struct zint_symbol *symbol, unsigned char source[], int length); /* DataBar Expanded */
INTERNAL int composite(struct zint_symbol *symbol, unsigned char source[], int length); /* Composite Symbology */
INTERNAL int kix(struct zint_symbol *symbol, unsigned char source[], int length); /* TNT KIX Code */
INTERNAL int aztec(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* Aztec Code */
INTERNAL int code32(struct zint_symbol *symbol, unsigned char source[], int length); /* Italian Pharmacode */
INTERNAL int daft(struct zint_symbol *symbol, unsigned char source[], int length); /* DAFT Code */
INTERNAL int ean14(struct zint_symbol *symbol, unsigned char source[], int length); /* EAN-14 */
INTERNAL int nve18(struct zint_symbol *symbol, unsigned char source[], int length); /* NVE-18 */
INTERNAL int microqr(struct zint_symbol *symbol, unsigned char source[], int length); /* Micro QR Code */
INTERNAL int azrune(struct zint_symbol *symbol, unsigned char source[], int length); /* Aztec Runes */
INTERNAL int koreapost(struct zint_symbol *symbol, unsigned char source[], int length); /* Korea Post */
INTERNAL int japanpost(struct zint_symbol *symbol, unsigned char source[], int length); /* Japanese Post */
INTERNAL int code49(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 49 */
INTERNAL int channel(struct zint_symbol *symbol, unsigned char source[], int length); /* Channel Code */
INTERNAL int codeone(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* Code One */
INTERNAL int gridmatrix(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* Grid Matrix */
INTERNAL int hanxin(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* Han Xin */
INTERNAL int dotcode(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* DotCode */
INTERNAL int codablockf(struct zint_symbol *symbol, unsigned char source[], int length); /* Codablock */
INTERNAL int upnqr(struct zint_symbol *symbol, unsigned char source[], int length); /* UPNQR */
INTERNAL int qrcode(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* QR Code */
/* Data Matrix (IEC16022) */
INTERNAL int datamatrix(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count);
/* VIN Code (Vehicle Identification Number) */
INTERNAL int vin(struct zint_symbol *symbol, unsigned char source[], int length);
/* Royal Mail 2D Mailmark */
INTERNAL int mailmark_2d(struct zint_symbol *symbol, unsigned char source[], int length);
/* Royal Mail 4-state Mailmark */
INTERNAL int mailmark_4s(struct zint_symbol *symbol, unsigned char source[], int length);
INTERNAL int upu_s10(struct zint_symbol *symbol, unsigned char source[], int length); /* Universal Postal Union S10 */
INTERNAL int ultra(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* Ultracode */
INTERNAL int rmqr(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count); /* rMQR */
INTERNAL int dpd(struct zint_symbol *symbol, unsigned char source[], int length); /* DPD Code */
INTERNAL int bc412(struct zint_symbol *symbol, unsigned char source[], int length); /* BC412 */
INTERNAL int dxfilmedge(struct zint_symbol *symbol, unsigned char source[], int length); /* DX Film Edge Barcode */

/* Output handlers */
/* Plot to BMP/GIF/PCX/PNG/TIF */
INTERNAL int plot_raster(struct zint_symbol *symbol, int rotate_angle, int file_type);
/* Plot to EMF/EPS/SVG */
INTERNAL int plot_vector(struct zint_symbol *symbol, int rotate_angle, int file_type);

/* Prefix error message with Error/Warning */
static int error_tag(int error_number, struct zint_symbol *symbol, const int err_id, const char *error_string) {

    if (error_number != 0) {
        if (error_string) {
            errtxt(0, symbol, err_id, error_string);
        }
        if (error_number < ZINT_ERROR && symbol->warn_level == WARN_FAIL_ALL) {
            /* Convert to error equivalent */
            if (error_number == ZINT_WARN_NONCOMPLIANT) {
                error_number = ZINT_ERROR_NONCOMPLIANT;
            } else if (error_number == ZINT_WARN_USES_ECI) {
                error_number = ZINT_ERROR_USES_ECI;
            } else if (error_number == ZINT_WARN_INVALID_OPTION) {
                error_number = ZINT_ERROR_INVALID_OPTION;
            } else if (error_number == ZINT_WARN_HRT_TRUNCATED) {
                error_number = ZINT_ERROR_HRT_TRUNCATED;
            } else { /* Shouldn't happen */
                assert(0); /* Not reached */
                error_number = ZINT_ERROR_ENCODING_PROBLEM;
            }
        }
        if (error_number >= ZINT_ERROR) {
            errtxt_adj(0, symbol, "Error %s", NULL);
        } else {
            errtxt_adj(0, symbol, "Warning %s", NULL);
        }
    }

    return error_number;
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int error_tag_test(int error_number, struct zint_symbol *symbol, const int err_id,
                const char *error_string) {
    return error_tag(error_number, symbol, err_id, error_string);
}
#endif

/* Output a hexadecimal representation of the rendered symbol */
static int dump_plot(struct zint_symbol *symbol) {
    FILE *f;
    int i, r;
    static const char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    int space = 0;
    const int output_to_stdout = symbol->output_options & BARCODE_STDOUT;

    if (output_to_stdout) {
        f = stdout;
    } else {
#ifdef _WIN32
        f = out_win_fopen(symbol->outfile, "w");
#else
        f = fopen(symbol->outfile, "w");
#endif
        if (!f) {
            return errtxt(ZINT_ERROR_FILE_ACCESS, symbol, 201, "Could not open output file");
        }
    }

    for (r = 0; r < symbol->rows; r++) {
        int byt = 0;
        for (i = 0; i < symbol->width; i++) {
            byt = byt << 1;
            if (symbol->symbology == BARCODE_ULTRA) {
                if (module_colour_is_set(symbol, r, i)) {
                    byt += 1;
                }
            } else {
                if (module_is_set(symbol, r, i)) {
                    byt += 1;
                }
            }
            if (((i + 1) % 4) == 0) {
                fputc(hex[byt], f);
                space++;
                byt = 0;
            }
            if (space == 2 && i + 1 < symbol->width) {
                fputc(' ', f);
                space = 0;
            }
        }

        if ((symbol->width % 4) != 0) {
            byt = byt << (4 - (symbol->width % 4));
            fputc(hex[byt], f);
        }
        fputc('\n', f);
        space = 0;
    }

    if (ferror(f)) {
        errtxtf(0, symbol, 795, "Incomplete write to output (%1$d: %2$s)", errno, strerror(errno));
        if (!output_to_stdout) {
            (void) fclose(f);
        }
        return ZINT_ERROR_FILE_WRITE;
    }

    if (output_to_stdout) {
        if (fflush(f) != 0) {
            return errtxtf(ZINT_ERROR_FILE_WRITE, symbol, 796, "Incomplete flush to output (%1$d: %2$s)",
                            errno, strerror(errno));
        }
    } else {
        if (fclose(f) != 0) {
            return errtxtf(ZINT_ERROR_FILE_WRITE, symbol, 792, "Failure on closing output file (%1$d: %2$s)",
                            errno, strerror(errno));
        }
    }

    return 0;
}

/* Permitted HIBC characters */
static const char TECHNETIUM[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%"; /* Same as SILVER (CODE39) */

/* Process health industry bar code data */
static int hibc(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    unsigned char *source = segs[0].source;
    int length = segs[0].length;

    int i;
    int counter, error_number = 0;
    char to_process[110 + 2 + 1];
    int posns[110];

    /* without "+" and check: max 110 characters in HIBC 2.6 */
    if (length > 110) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 202, "Input length %d too long for HIBC LIC (maximum 110)",
                        length);
    }
    to_upper(source, length);
    if ((i = not_sane_lookup(TECHNETIUM, sizeof(TECHNETIUM) - 1, source, length, posns))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 203,
                        "Invalid character at position %d in input (alphanumerics, space and \"-.$/+%%\" only)", i);
    }

    counter = 41;
    for (i = 0; i < length; i++) {
        counter += posns[i];
    }
    counter = counter % 43;

    to_process[0] = '+';
    memcpy(to_process + 1, source, length);
    to_process[++length] = TECHNETIUM[counter];
    to_process[++length] = '\0';

    segs[0].source = (unsigned char *) to_process;
    segs[0].length = length;

    if (symbol->debug & ZINT_DEBUG_PRINT) printf("HIBC processed source: %s\n", to_process);

    switch (symbol->symbology) {
        case BARCODE_HIBC_128:
            error_number = code128(symbol, segs[0].source, segs[0].length);
            ustrcpy(symbol->text, "*");
            ustrcat(symbol->text, to_process);
            ustrcat(symbol->text, "*");
            break;
        case BARCODE_HIBC_39:
            symbol->option_2 = 0;
            error_number = code39(symbol, segs[0].source, segs[0].length);
            ustrcpy(symbol->text, "*");
            ustrcat(symbol->text, to_process);
            ustrcat(symbol->text, "*");
            break;
        case BARCODE_HIBC_DM:
            error_number = datamatrix(symbol, segs, seg_count);
            break;
        case BARCODE_HIBC_QR:
            error_number = qrcode(symbol, segs, seg_count);
            break;
        case BARCODE_HIBC_PDF:
            error_number = pdf417(symbol, segs, seg_count);
            break;
        case BARCODE_HIBC_MICPDF:
            error_number = micropdf417(symbol, segs, seg_count);
            break;
        case BARCODE_HIBC_AZTEC:
            error_number = aztec(symbol, segs, seg_count);
            break;
        case BARCODE_HIBC_BLOCKF:
            error_number = codablockf(symbol, segs[0].source, segs[0].length);
            break;
    }

    return error_number;
}

/* Returns 1 if symbology MUST have GS1 data */
static int check_force_gs1(const int symbology) {

    switch (symbology) {
        case BARCODE_GS1_128:
        case BARCODE_EAN14:
        case BARCODE_NVE18:
        case BARCODE_DBAR_EXP:
        case BARCODE_DBAR_EXPSTK:
            return 1;
            break;
    }

    return is_composite(symbology);
}

/* Returns 1 if symbology supports GS1 data */
static int gs1_compliant(const int symbology) {

    switch (symbology) {
        case BARCODE_CODE16K:
        case BARCODE_AZTEC:
        case BARCODE_DATAMATRIX:
        case BARCODE_CODE49:
        case BARCODE_QRCODE:
        case BARCODE_DOTCODE:
        case BARCODE_CODEONE:
        case BARCODE_ULTRA:
        case BARCODE_RMQR:
        /* TODO: case BARCODE_CODABLOCKF: */
        /* TODO: case BARCODE_HANXIN: */
        /* TODO: case BARCODE_GRIDMATRIX: */
            return 1;
            break;
    }

    return check_force_gs1(symbology);
}

/* Returns 1 if symbology can encode the ECI character */
static int supports_eci(const int symbology) {

    switch (symbology) {
        case BARCODE_AZTEC:
        case BARCODE_DATAMATRIX:
        case BARCODE_MAXICODE:
        case BARCODE_MICROPDF417:
        case BARCODE_PDF417:
        case BARCODE_PDF417COMP:
        case BARCODE_QRCODE:
        case BARCODE_DOTCODE:
        case BARCODE_CODEONE:
        case BARCODE_GRIDMATRIX:
        case BARCODE_HANXIN:
        case BARCODE_ULTRA:
        case BARCODE_RMQR:
            return 1;
            break;
    }

    return 0;
}

/* Returns 1 if symbology supports HRT */
static int has_hrt(const int symbology) {

    if (is_fixed_ratio(symbology)) {
        return 0;
    }

    switch (symbology) { /* These don't support HRT */
        case BARCODE_CODE16K:
        case BARCODE_CODE49:
        case BARCODE_FLAT:
        case BARCODE_POSTNET:
        case BARCODE_FIM:
        case BARCODE_PHARMA:
        case BARCODE_PHARMA_TWO:
        case BARCODE_DXFILMEDGE:
        case BARCODE_CEPNET:
        case BARCODE_PDF417:
        case BARCODE_PDF417COMP:
        case BARCODE_AUSPOST:
        case BARCODE_AUSREPLY:
        case BARCODE_AUSROUTE:
        case BARCODE_AUSREDIRECT:
        case BARCODE_RM4SCC:
        case BARCODE_CODABLOCKF:
        case BARCODE_JAPANPOST:
        case BARCODE_DBAR_STK:
        case BARCODE_DBAR_OMNSTK:
        case BARCODE_DBAR_EXPSTK:
        case BARCODE_PLANET:
        case BARCODE_MICROPDF417:
        case BARCODE_USPS_IMAIL:
        case BARCODE_KIX:
        case BARCODE_DAFT:
        case BARCODE_HIBC_PDF:
        case BARCODE_HIBC_MICPDF:
        case BARCODE_HIBC_BLOCKF:
        case BARCODE_MAILMARK_2D:
        case BARCODE_MAILMARK_4S:
        case BARCODE_DBAR_STK_CC:
        case BARCODE_DBAR_OMNSTK_CC:
        case BARCODE_DBAR_EXPSTK_CC:
            return 0;
            break;
    }

    return 1;
}

typedef int (*barcode_src_func_t)(struct zint_symbol *, unsigned char[], int);
typedef int (*barcode_seg_func_t)(struct zint_symbol *, struct zint_seg[], const int);

/* Used for dispatching `barcode_src_func_t` barcodes */
/* Also used, with `barcode_seg_funcs` below, for testing whether symbol id valid in `ZBarcode_ValidID()` */
static const barcode_src_func_t barcode_src_funcs[BARCODE_LAST + 1] = {
          NULL,      code11, c25standard,    c25inter,     c25iata, /*0-4*/
          NULL,    c25logic,      c25ind,      code39,    excode39, /*5-9*/
          NULL,        NULL,        NULL,        eanx,        eanx, /*10-14*/
          NULL,     gs1_128,        NULL,     codabar,        NULL, /*15-19*/
       code128,      dpleit,     dpident,     code16k,      code49, /*20-24*/
        code93,        NULL,        NULL,        flat,    dbar_omn, /*25-29*/
      dbar_ltd,    dbar_exp,     telepen,        NULL,        eanx, /*30-34*/
          eanx,        NULL,        eanx,        eanx,        NULL, /*35-39*/
       postnet,        NULL,        NULL,        NULL,        NULL, /*40-44*/
          NULL,        NULL, msi_plessey,        NULL,         fim, /*45-49*/
        code39,      pharma,         pzn,  pharma_two,     postnet, /*50-54*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*55-59*/
       code128,        NULL,        NULL,     auspost,        NULL, /*60-64*/
          NULL,     auspost,     auspost,     auspost,        eanx, /*65-69*/
        rm4scc,        NULL,       ean14,         vin,  codablockf, /*70-74*/
         nve18,   japanpost,   koreapost,        NULL,    dbar_omn, /*75-79*/
      dbar_omn,    dbar_exp,      planet,        NULL,        NULL, /*80-84*/
    usps_imail,     plessey, telepen_num,        NULL,       itf14, /*85-89*/
           kix,        NULL,        NULL,        daft,        NULL, /*90-94*/
          NULL,         dpd,     microqr,        NULL,        NULL, /*95-99*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*100-104*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*105-109*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*110-114*/
          NULL,        NULL,        NULL,        NULL, mailmark_2d, /*115-119*/
       upu_s10, mailmark_4s,        NULL,        NULL,        NULL, /*120-124*/
          NULL,        NULL,        NULL,      azrune,      code32, /*125-129*/
     composite,   composite,   composite,   composite,   composite, /*130-134*/
     composite,   composite,   composite,   composite,   composite, /*135-139*/
       channel,        NULL,        NULL,       upnqr,        NULL, /*140-144*/
          NULL,       bc412,  dxfilmedge,                           /*145-147*/
};

#define LIB_SEG_FUNCS_START 55

/* Used for dispatching `barcode_seg_func_t` barcodes */
static const barcode_seg_func_t barcode_seg_funcs[BARCODE_LAST + 1 - LIB_SEG_FUNCS_START] = {
        pdf417,      pdf417,    maxicode,      qrcode,        NULL, /*55-59*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*60-64*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*65-69*/
          NULL,  datamatrix,        NULL,        NULL,        NULL, /*70-74*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*75-79*/
          NULL,        NULL,        NULL,        NULL, micropdf417, /*80-84*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*85-89*/
          NULL,        NULL,       aztec,        NULL,        NULL, /*90-94*/
          NULL,        NULL,        NULL,        hibc,        hibc, /*95-99*/
          NULL,        NULL,        hibc,        NULL,        hibc, /*100-104*/
          NULL,        hibc,        NULL,        hibc,        NULL, /*105-109*/
          hibc,        NULL,        hibc,        NULL,        NULL, /*110-114*/
       dotcode,      hanxin,        NULL,        NULL,        NULL, /*115-119*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*120-124*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*125-129*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*130-134*/
          NULL,        NULL,        NULL,        NULL,        NULL, /*135-139*/
          NULL,     codeone,  gridmatrix,        NULL,       ultra, /*140-144*/
          rmqr,        NULL,                                        /*145-146*/
};

static int reduced_charset(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count);

/* Main dispatch, checking for barcodes which handle ECIs/character sets themselves, otherwise calling
   `reduced_charset()` */
static int extended_or_reduced_charset(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int error_number = 0;

    switch (symbol->symbology) {
        /* These are the "elite" standards which have support for specific character sets + ECI */
        case BARCODE_QRCODE:
        case BARCODE_GRIDMATRIX:
        case BARCODE_HANXIN:
        case BARCODE_RMQR:
            error_number = barcode_seg_funcs[symbol->symbology - LIB_SEG_FUNCS_START](symbol, segs, seg_count);
            break;
        /* These are the standards which have support for specific character sets but not ECI */
        case BARCODE_MICROQR:
        case BARCODE_UPNQR:
            error_number = barcode_src_funcs[symbol->symbology](symbol, segs[0].source, segs[0].length);
            break;
        default: error_number = reduced_charset(symbol, segs, seg_count);
            break;
    }

    return error_number;
}

/* These are the "norm" standards which only support Latin-1 at most, though a few support ECI */
static int reduced_charset(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int error_number = 0;
    int i;
    struct zint_seg *local_segs = (struct zint_seg *) z_alloca(sizeof(struct zint_seg) * seg_count);
    int *convertible = (int *) z_alloca(sizeof(int) * seg_count);

    if ((symbol->input_mode & 0x07) == UNICODE_MODE && is_eci_convertible_segs(segs, seg_count, convertible)) {
        unsigned char *preprocessed;
        const int eci_length_segs = get_eci_length_segs(segs, seg_count);
        unsigned char *preprocessed_buf = (unsigned char *) z_alloca(eci_length_segs + seg_count);

        /* Prior check ensures ECI only set for those that support it */
        segs_cpy(symbol, segs, seg_count, local_segs); /* Shallow copy (needed to set default ECIs) */
        preprocessed = preprocessed_buf;
        for (i = 0; i < seg_count; i++) {
            if (convertible[i]) {
                error_number = utf8_to_eci(local_segs[i].eci, local_segs[i].source, preprocessed,
                                            &local_segs[i].length);
                if (error_number != 0) {
                    if (local_segs[i].eci) {
                        return errtxtf(error_number, symbol, 244, "Invalid character in input for ECI '%d'",
                                        local_segs[i].eci);
                    }
                    return errtxt(error_number, symbol, 204, "Invalid character in input (ISO/IEC 8859-1 only)");
                }
                local_segs[i].source = preprocessed;
                preprocessed += local_segs[i].length + 1;
            }
        }
        if (barcode_src_funcs[symbol->symbology]) {
            error_number = barcode_src_funcs[symbol->symbology](symbol, local_segs[0].source, local_segs[0].length);
        } else {
            assert(symbol->symbology >= LIB_SEG_FUNCS_START); /* Suppress clang-tidy-20 warning */
            assert(barcode_seg_funcs[symbol->symbology - LIB_SEG_FUNCS_START]); /* Suppress clang-tidy-20 warning */
            error_number = barcode_seg_funcs[symbol->symbology - LIB_SEG_FUNCS_START](symbol, local_segs, seg_count);
        }
    } else {
        if (barcode_src_funcs[symbol->symbology]) {
            error_number = barcode_src_funcs[symbol->symbology](symbol, segs[0].source, segs[0].length);
        } else {
            assert(symbol->symbology >= LIB_SEG_FUNCS_START); /* Suppress clang-tidy-19 warning */
            assert(barcode_seg_funcs[symbol->symbology - LIB_SEG_FUNCS_START]); /* Suppress clang-tidy-19 warning */
            segs_cpy(symbol, segs, seg_count, local_segs); /* Shallow copy (needed to set default ECIs) */
            error_number = barcode_seg_funcs[symbol->symbology - LIB_SEG_FUNCS_START](symbol, local_segs, seg_count);
        }
    }

    return error_number;
}

/* Remove Unicode BOM at start of data */
static void strip_bom(unsigned char *source, int *input_length) {
    int i;

    /* Note if BOM is only data then not stripped */
    if (*input_length > 3 && (source[0] == 0xef) && (source[1] == 0xbb) && (source[2] == 0xbf)) {
        /* BOM at start of input data, strip in accordance with RFC 3629 */
        for (i = 3; i <= *input_length; i++) { /* Include terminating NUL */
            source[i - 3] = source[i];
        }
        *input_length -= 3;
    }
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL void strip_bom_test(unsigned char *source, int *input_length) {
    strip_bom(source, input_length);
}
#endif

/* Helper to convert base octal, decimal, hexadecimal escape sequence */
static int esc_base(struct zint_symbol *symbol, const unsigned char *input_string, const int length,
            const int in_posn, const unsigned char base) {
    int c1, c2, c3;
    int min_len = base == 'x' ? 2 : 3;
    int val = -1;

    if (in_posn + min_len > length) {
        return errtxtf(-1, symbol, 232, "Incomplete '\\%c' escape sequence in input", base);
    }
    c1 = ctoi(input_string[in_posn]);
    c2 = ctoi(input_string[in_posn + 1]);
    if (base == 'd') {
        c3 = ctoi(input_string[in_posn + 2]);
        if ((c1 >= 0 && c1 <= 9) && (c2 >= 0 && c2 <= 9) && (c3 >= 0 && c3 <= 9)) {
            val = c1 * 100 + c2 * 10 + c3;
        }
    } else if (base == 'o') {
        c3 = ctoi(input_string[in_posn + 2]);
        if ((c1 >= 0 && c1 <= 7) && (c2 >= 0 && c2 <= 7) && (c3 >= 0 && c3 <= 7)) {
            val = (c1 << 6) | (c2 << 3) | c3;
        }
    } else {
        if ((c1 >= 0) && (c2 >= 0)) {
            val = (c1 << 4) | c2;
        }
    }

    if (val == -1) {
        return errtxtf(-1, symbol, 238, "Invalid character in escape sequence '%2$.*1$s' in input (%3$s only)",
                        base == 'x' ? 4 : 5, input_string + in_posn - 2,
                        base == 'd' ? "decimal" : base == 'o' ? "octal" : "hexadecimal");
    }
    if (val > 255) {
        assert(base != 'x');
        return errtxtf(-1, symbol, 237, "Value of escape sequence '%1$.5s' in input out of range (000 to %2$s)",
                        input_string + in_posn - 2, base == 'd' ? "255" : "377");
    }

    return val;
}

/* Helper to parse escape sequences. If `escaped_string` NULL, calculates length only */
static int escape_char_process(struct zint_symbol *symbol, const unsigned char *input_string, int *p_length,
            unsigned char *escaped_string) {
                               /* NUL   EOT   BEL   BS    HT    LF    VT    FF    CR    ESC   GS    RS   \ */
    static const char escs[] = {  '0',  'E',  'a',  'b',  't',  'n',  'v',  'f',  'r',  'e',  'G',  'R', '\\', '\0' };
    static const char vals[] = { 0x00, 0x04, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x1B, 0x1D, 0x1E, 0x5C };
    const int length = *p_length;
    int in_posn = 0, out_posn = 0;
    unsigned char ch;
    int val;
    int i;
    unsigned int unicode;
    const int extra_escape_mode = (symbol->input_mode & EXTRA_ESCAPE_MODE) && symbol->symbology == BARCODE_CODE128;

    do {
        if (input_string[in_posn] == '\\') {
            if (in_posn + 1 >= length) {
                return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 236, "Incomplete escape character in input");
            }
            ch = input_string[in_posn + 1];
            /* NOTE: if add escape character, must also update regex in "frontend_qt/datawindow.php" */
            switch (ch) {
                case '0':
                case 'E':
                case 'a':
                case 'b':
                case 't':
                case 'n':
                case 'v':
                case 'f':
                case 'r':
                case 'e':
                case 'G':
                case 'R':
                case '\\':
                    if (escaped_string) escaped_string[out_posn] = vals[posn(escs, ch)];
                    in_posn += 2;
                    break;
                case '^': /* CODE128 specific */
                    if (!extra_escape_mode) {
                        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 798,
                                        "Escape '\\^' only valid for Code 128 in extra escape mode");
                    }
                    /* Pass thru unaltered */
                    if (escaped_string) {
                        escaped_string[out_posn++] = '\\';
                        escaped_string[out_posn] = '^';
                    } else {
                        out_posn++;
                    }
                    in_posn += 2;
                    if (in_posn < length) { /* Note allowing '\\^' on its own at end */
                        if (escaped_string) {
                            escaped_string[++out_posn] = input_string[in_posn++];
                        } else {
                            ++out_posn;
                            in_posn++;
                        }
                    }
                    break;
                case 'd':
                case 'o':
                case 'x':
                    if ((val = esc_base(symbol, input_string, length, in_posn + 2, ch)) == -1) {
                        return ZINT_ERROR_INVALID_DATA;
                    }
                    if (escaped_string) escaped_string[out_posn] = val;
                    in_posn += 4 + (ch != 'x');
                    break;
                case 'u':
                case 'U':
                    if (in_posn + 6 > length || (ch == 'U' && in_posn + 8 > length)) {
                        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 209,
                                        "Incomplete '\\%c' escape sequence in input", ch);
                    }
                    unicode = 0;
                    for (i = 0; i < 6; i++) {
                        if ((val = ctoi(input_string[in_posn + i + 2])) == -1) {
                            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 211,
                                    "Invalid character for '\\%c' escape sequence in input (hexadecimal only)",
                                    ch);
                        }
                        unicode = (unicode << 4) | val;
                        if (i == 3 && ch == 'u') {
                            break;
                        }
                    }
                    /* Exclude reversed BOM and surrogates and out-of-range */
                    if (unicode == 0xfffe || (unicode >= 0xd800 && unicode < 0xe000) || unicode > 0x10ffff) {
                        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 246,
                                        "Value of escape sequence '%.*s' in input out of range",
                                        ch == 'u' ? 6 : 8, input_string + in_posn);
                    }
                    if (unicode < 0x80) {
                        if (escaped_string) escaped_string[out_posn] = (unsigned char) unicode;
                    } else if (unicode < 0x800) {
                        if (escaped_string) {
                            escaped_string[out_posn++] = (unsigned char) (0xC0 | (unicode >> 6));
                            escaped_string[out_posn] = (unsigned char) (0x80 | (unicode & 0x3F));
                        } else {
                            out_posn++;
                        }
                    } else if (unicode < 0x10000) {
                        if (escaped_string) {
                            escaped_string[out_posn++] = (unsigned char) (0xE0 | (unicode >> 12));
                            escaped_string[out_posn++] = (unsigned char) (0x80 | ((unicode >> 6) & 0x3F));
                            escaped_string[out_posn] = (unsigned char) (0x80 | (unicode & 0x3F));
                        } else {
                            out_posn += 2;
                        }
                    } else {
                        if (escaped_string) {
                            escaped_string[out_posn++] = (unsigned char) (0xF0 | (unicode >> 18));
                            escaped_string[out_posn++] = (unsigned char) (0x80 | ((unicode >> 12) & 0x3F));
                            escaped_string[out_posn++] = (unsigned char) (0x80 | ((unicode >> 6) & 0x3F));
                            escaped_string[out_posn] = (unsigned char) (0x80 | (unicode & 0x3F));
                        } else {
                            out_posn += 3;
                        }
                    }
                    in_posn += 6 + (ch == 'U') * 2;
                    break;
                default:
                    return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 234,
                                    "Unrecognised escape character '\\%c' in input", ch);
                    break;
            }
        } else {
            if (escaped_string) escaped_string[out_posn] = input_string[in_posn];
            in_posn++;
        }
        out_posn++;
    } while (in_posn < length);

    if (escaped_string) {
        escaped_string[out_posn] = '\0';
    }
    *p_length = out_posn;

    return 0;
}

#ifdef ZINT_TEST /* Wrapper for direct testing (also used by `testUtilZXingCPPCmp()` in "tests/testcommon.c") */
INTERNAL int escape_char_process_test(struct zint_symbol *symbol, const unsigned char *input_string, int *p_length,
                unsigned char *escaped_string) {
    return escape_char_process(symbol, input_string, p_length, escaped_string);
}
#endif

/* For backward-compatibility, map certain invalid symbol ids to zint equivalents, some silently, some with warning */
static int map_invalid_symbology(struct zint_symbol *symbol) {

    /* Symbol ids 1 to 126 are defined by tbarcode */
    /* 26 allowed: UPC-A up to tbarcode 9, ISSN for tbarcode 10+, mapped to UPC-A */
    /* 27 error: UPCD1 up to tbarcode 9, ISSN + 2 digit add-on for tbarcode 10+ */
    /* 91 warning: BC412 up to tbarcode 9, Code 32 for tbarcode 10+, mapped to Code 128 */
    /* Note: non-zero table entries map silently, i.e. do not produce a warning */
    #define LIB_ID_MAP_LAST 111
    static const unsigned char id_map[LIB_ID_MAP_LAST + 1] = {
                          0,                   0,                0,                0,                   0, /*0-4*/
        BARCODE_C25STANDARD,                   0,                0,                0,                   0, /*5-9*/
               BARCODE_EANX,        BARCODE_EANX,     BARCODE_EANX,                0,                   0, /*10-14*/
               BARCODE_EANX,                   0,     BARCODE_UPCA,                0,     BARCODE_CODABAR, /*15-19*/
                          0,                   0,                0,                0,                   0, /*20-24*/
                          0,        BARCODE_UPCA,                0,                0,                   0, /*25-29*/
                          0,                   0,                0,  BARCODE_GS1_128,                   0, /*30-34*/
                          0,        BARCODE_UPCA,                0,                0,        BARCODE_UPCE, /*35-39*/
                          0,     BARCODE_POSTNET,  BARCODE_POSTNET,  BARCODE_POSTNET,     BARCODE_POSTNET, /*40-44*/
            BARCODE_POSTNET,     BARCODE_PLESSEY,                0,    BARCODE_NVE18,                   0, /*45-49*/
                          0,                   0,                0,                0,                   0, /*50-54*/
                          0,                   0,                0,                0,     BARCODE_CODE128, /*55-59*/
                          0,     BARCODE_CODE128,   BARCODE_CODE93,                0,     BARCODE_AUSPOST, /*60-64*/
            BARCODE_AUSPOST,                   0,                0,                0,                   0, /*65-69*/
                          0,                   0,                0,                0,                   0, /*70-74*/
                          0,                   0,                0, BARCODE_DBAR_OMN,                   0, /*75-79*/
                          0,                   0,                0,   BARCODE_PLANET,                   0, /*80-84*/
                          0,                   0,                0,  BARCODE_GS1_128,                   0, /*85-89*/
                          0,                   0,                0,                0,                   0, /*90-94*/
                          0,                   0,                0,                0,                   0, /*95-99*/
           BARCODE_HIBC_128,     BARCODE_HIBC_39,                0,  BARCODE_HIBC_DM,                   0, /*100-104*/
            BARCODE_HIBC_QR,                   0, BARCODE_HIBC_PDF,                0, BARCODE_HIBC_MICPDF, /*105-109*/
                          0, BARCODE_HIBC_BLOCKF,                                                          /*110-111*/
    };
    const int orig_symbology = symbol->symbology; /* For self-check */
    int warn_number = 0;

    if (symbol->symbology == 19) {
        /* Has specific error message */
        warn_number = error_tag(ZINT_WARN_INVALID_OPTION, symbol, 207, "Codabar 18 not supported");
        if (warn_number >= ZINT_ERROR) {
            return warn_number;
        }
        symbol->symbology = BARCODE_CODABAR;
    } else if (symbol->symbology == 27) {
        /* Not mapped */
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 208, "UPCD1 not supported");

    } else if (symbol->symbology <= 0 || symbol->symbology > LIB_ID_MAP_LAST || id_map[symbol->symbology] == 0) {
        warn_number = error_tag(ZINT_WARN_INVALID_OPTION, symbol, 206, "Symbology out of range");
        if (warn_number >= ZINT_ERROR) {
            return warn_number;
        }
        symbol->symbology = BARCODE_CODE128;
    } else {
        symbol->symbology = id_map[symbol->symbology];
    }

    if (symbol->symbology == orig_symbology) { /* Should never happen */
        assert(0); /* Not reached */
        return error_tag(ZINT_ERROR_ENCODING_PROBLEM, symbol, 0, "Internal error");
    }

    return warn_number;
}

/* Encode a barcode. If `length` is 0, `source` must be NUL-terminated */
int ZBarcode_Encode(struct zint_symbol *symbol, const unsigned char *source, int length) {
    struct zint_seg segs[1];

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    segs[0].eci = symbol->eci;
    segs[0].source = (unsigned char *) source;
    segs[0].length = length;

    return ZBarcode_Encode_Segs(symbol, segs, 1);
}

/* Encode a barcode with multiple ECI segments. */
int ZBarcode_Encode_Segs(struct zint_symbol *symbol, const struct zint_seg segs[], const int seg_count) {
    int error_number, warn_number = 0;
    int total_len = 0;
    int have_zero_eci = 0;
    int escape_mode;
    int i;
    unsigned char *local_source;
    struct zint_seg *local_segs;
    unsigned char *local_sources;

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    if (segs == NULL) {
        return error_tag(ZINT_ERROR_INVALID_DATA, symbol, 200, "Input segments NULL");
    }
    /* `seg_count` zero dealt with via `total_len` zero below */
    if (seg_count > ZINT_MAX_SEG_COUNT) {
        return error_tag(ZINT_ERROR_INVALID_DATA, symbol, 771, "Too many input segments (maximum 256)");
    }

    if ((symbol->input_mode & 0x07) > 2) {
        symbol->input_mode = DATA_MODE; /* Reset completely */
        warn_number = error_tag(ZINT_WARN_INVALID_OPTION, symbol, 212, "Invalid input mode - reset to DATA_MODE");
        if (warn_number >= ZINT_ERROR) {
            return warn_number;
        }
    }

    /* Check the symbology field */
    if (!ZBarcode_ValidID(symbol->symbology)) {
        warn_number = map_invalid_symbology(symbol);
        if (warn_number >= ZINT_ERROR) {
            return warn_number;
        }
    }

    escape_mode = (symbol->input_mode & ESCAPE_MODE)
                    || ((symbol->input_mode & EXTRA_ESCAPE_MODE) && symbol->symbology == BARCODE_CODE128);

    local_segs = (struct zint_seg *) z_alloca(sizeof(struct zint_seg) * (seg_count > 0 ? seg_count : 1));

    /* Check segment lengths */
    for (i = 0; i < seg_count; i++) {
        local_segs[i] = segs[i];
        if (local_segs[i].source == NULL) {
            errtxtf(0, symbol, 772, "Input segment %d source NULL", i);
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, -1, NULL);
        }
        if (local_segs[i].length <= 0) {
            local_segs[i].length = (int) ustrlen(local_segs[i].source);
        }
        if (local_segs[i].length <= 0) {
            if (i == 0) {
                if (is_composite(symbol->symbology)
                        && ((symbol->input_mode & 0x07) == GS1_MODE || check_force_gs1(symbol->symbology))) {
                    errtxt(0, symbol, 779, "No composite data (2D component)");
                } else if (supports_eci(symbol->symbology)) {
                    errtxt(0, symbol, 228, "No input data (segment 0 empty)");
                } else {
                    errtxt(0, symbol, 778, "No input data");
                }
            } else {
                errtxtf(0, symbol, 773, "Input segment %d empty", i);
            }
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, -1, NULL);
        }
        /* Calculate de-escaped length for check against ZINT_MAX_DATA_LEN */
        if (escape_mode) {
            int escaped_len = local_segs[i].length;
            error_number = escape_char_process(symbol, local_segs[i].source, &escaped_len, NULL /*escaped_string*/);
            if (error_number != 0) { /* Only returns errors, not warnings */
                return error_tag(error_number, symbol, -1, NULL);
            }
            if (escaped_len > ZINT_MAX_DATA_LEN) {
                return error_tag(ZINT_ERROR_TOO_LONG, symbol, 797, "Input too long");
            }
            total_len += escaped_len;
        } else {
            if (local_segs[i].length > ZINT_MAX_DATA_LEN) {
                return error_tag(ZINT_ERROR_TOO_LONG, symbol, 777, "Input too long");
            }
            total_len += local_segs[i].length;
        }
    }

    if (total_len == 0) {
        return error_tag(ZINT_ERROR_INVALID_DATA, symbol, 205, "No input data");
    }

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        const int len = local_segs[0].length;
        const int primary_len = symbol->primary[0] ? (int) strlen(symbol->primary) : 0;
        char name[32];
        char source[151], primary[151]; /* 30*5 + 1 = 151 */
        (void) ZBarcode_BarcodeName(symbol->symbology, name);
        debug_print_escape(local_segs[0].source, len > 30 ? 30 : len, source);
        debug_print_escape((const unsigned char *) symbol->primary, primary_len > 30 ? 30 : primary_len, primary);
        printf("\nZBarcode_Encode_Segs: %s (%d), input_mode: 0x%X, ECI: %d, option_1/2/3: (%d, %d, %d)\n"
                "                      scale: %g, output_options: 0x%X, fg: %s, bg: %s, seg_count: %d,\n"
                "                      %ssource%s (%d): \"%s\",\n"
                "                      %sprimary (%d): \"%s\"\n",
                name, symbol->symbology, symbol->input_mode, symbol->eci, symbol->option_1, symbol->option_2,
                symbol->option_3, symbol->scale, symbol->output_options, symbol->fgcolour, symbol->bgcolour,
                seg_count, len > 30 ? "first 30 " : "", seg_count > 1 ? "[0]" : "", len, source,
                primary_len > 30 ? "first 30 " : "", primary_len, primary);
        fflush(stdout);
    }

    if (total_len > ZINT_MAX_DATA_LEN) {
        return error_tag(ZINT_ERROR_TOO_LONG, symbol, 243, "Input too long");
    }

    /* Reconcile symbol ECI and first segment ECI if both set */
    if (symbol->eci != local_segs[0].eci) {
        if (symbol->eci && local_segs[0].eci) {
            errtxtf(0, symbol, 774, "Symbol ECI '%1$d' must match segment zero ECI '%2$d'",
                    symbol->eci, local_segs[0].eci);
            return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, -1, NULL);
        }
        if (symbol->eci) {
            local_segs[0].eci = symbol->eci;
        } else {
            symbol->eci = local_segs[0].eci;
        }
    }

    if (seg_count > 1 && !supports_eci(symbol->symbology)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 775, "Symbology does not support multiple segments");
    }

    /* Check ECI(s) */
    for (i = 0; i < seg_count; i++) {
        if (local_segs[i].eci) {
            if (!supports_eci(symbol->symbology)) {
                return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 217, "Symbology does not support ECI switching");
            }
            if (local_segs[i].eci < 0 || local_segs[i].eci == 1 || local_segs[i].eci == 2 || local_segs[i].eci == 14
                    || local_segs[i].eci == 19 || local_segs[i].eci > 999999) {
                errtxtf(0, symbol, 218, "ECI code '%d' out of range (0 to 999999, excluding 1, 2, 14 and 19)",
                        local_segs[i].eci);
                return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, -1, NULL);
            }
        } else {
            have_zero_eci = 1;
        }
    }

    /* Check other symbol fields */
    if ((symbol->scale < 0.01f) || (symbol->scale > 200.0f)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 227, "Scale out of range (0.01 to 200)");
    }
    if ((symbol->dot_size < 0.01f) || (symbol->dot_size > 20.0f)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 221, "Dot size out of range (0.01 to 20)");
    }

    if ((symbol->height < 0.0f) || (symbol->height > 2000.0f)) { /* Allow for 44 row CODABLOCKF at 45X each */
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 765, "Height out of range (0 to 2000)");
    }
    if ((symbol->guard_descent < 0.0f) || (symbol->guard_descent > 50.0f)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 769, "Guard bar descent out of range (0 to 50)");
    }
    if ((symbol->text_gap < -5.0f) || (symbol->text_gap > 10.0f)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 219, "Text gap out of range (-5 to 10)");
    }
    if ((symbol->whitespace_width < 0) || (symbol->whitespace_width > 100)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 766, "Whitespace width out of range (0 to 100)");
    }
    if ((symbol->whitespace_height < 0) || (symbol->whitespace_height > 100)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 767, "Whitespace height out of range (0 to 100)");
    }
    if ((symbol->border_width < 0) || (symbol->border_width > 100)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 768, "Border width out of range (0 to 100)");
    }

    if (symbol->rows >= 200) { /* Check for stacking too many symbols */
        return error_tag(ZINT_ERROR_TOO_LONG, symbol, 770, "Too many stacked symbols");
    }
    if (symbol->rows < 0) { /* Silently defend against out-of-bounds access */
        symbol->rows = 0;
    }

    if ((symbol->input_mode & 0x07) == GS1_MODE && !gs1_compliant(symbol->symbology)) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 220, "Selected symbology does not support GS1 mode");
    }
    if (seg_count > 1) {
        /* Note: GS1_MODE not currently supported when using multiple segments */
        if ((symbol->input_mode & 0x07) == GS1_MODE) {
            return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 776, "GS1 mode not supported for multiple segments");
        }
    }

    local_sources = (unsigned char *) z_alloca(total_len + seg_count);

    /* Copy input, de-escaping if required */
    for (i = 0, local_source = local_sources; i < seg_count; i++) {
        local_segs[i].source = local_source;
        if (escape_mode) {
            /* Checked already */
            (void) escape_char_process(symbol, segs[i].source, &local_segs[i].length, local_segs[i].source);
        } else {
            memcpy(local_segs[i].source, segs[i].source, local_segs[i].length);
            local_segs[i].source[local_segs[i].length] = '\0';
        }
        local_source += local_segs[i].length + 1;
    }

    if (escape_mode && symbol->primary[0] && strchr(symbol->primary, '\\') != NULL) {
        char primary[sizeof(symbol->primary)];
        int primary_len = (int) strlen(symbol->primary);
        if (primary_len >= (int) sizeof(symbol->primary)) {
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, 799, "Invalid primary string");
        }
        ustrcpy(primary, symbol->primary);
        error_number = escape_char_process(symbol, (unsigned char *) primary, &primary_len,
                                            (unsigned char *) symbol->primary);
        if (error_number != 0) { /* Only returns errors, not warnings */
            return error_tag(error_number, symbol, -1, NULL);
        }
    }

    if ((symbol->input_mode & 0x07) == UNICODE_MODE) {
        for (i = 0; i < seg_count; i++) {
            if (!is_valid_utf8(local_segs[i].source, local_segs[i].length)) {
                return error_tag(ZINT_ERROR_INVALID_DATA, symbol, 245, "Invalid UTF-8 in input");
            }
        }
        /* Only strip BOM on first segment */
        strip_bom(local_segs[0].source, &local_segs[0].length);
    }

    if (((symbol->input_mode & 0x07) == GS1_MODE) || (check_force_gs1(symbol->symbology))) {
        if (gs1_compliant(symbol->symbology)) {
            /* Reduce input for composite and non-forced symbologies, others (EAN128 and RSS_EXP based) will
               handle it themselves */
            if (is_composite(symbol->symbology) || !check_force_gs1(symbol->symbology)) {
                unsigned char *reduced = (unsigned char *) z_alloca(local_segs[0].length + 1);
                error_number = gs1_verify(symbol, local_segs[0].source, local_segs[0].length, reduced);
                if (error_number) {
                    if (is_composite(symbol->symbology)) {
                        errtxt_adj(0, symbol, "%1$s%2$s", " (2D component)");
                    }
                    error_number = error_tag(error_number, symbol, -1, NULL);
                    if (error_number >= ZINT_ERROR) {
                        return error_number;
                    }
                    warn_number = error_number; /* Override any previous warning (errtxt has been overwritten) */
                }
                ustrcpy(local_segs[0].source, reduced); /* Cannot contain NUL char */
                local_segs[0].length = (int) ustrlen(reduced);
            }
        } else {
            return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 210, "Selected symbology does not support GS1 mode");
        }
    }

    error_number = extended_or_reduced_charset(symbol, local_segs, seg_count);

    if ((error_number == ZINT_ERROR_INVALID_DATA) && have_zero_eci && supports_eci(symbol->symbology)
            && (symbol->input_mode & 0x07) == UNICODE_MODE) {
        /* Try another ECI mode */
        const int first_eci_set = get_best_eci_segs(symbol, local_segs, seg_count);
        error_number = extended_or_reduced_charset(symbol, local_segs, seg_count);
        /* Inclusion of ECI more noteworthy than other warnings, so overwrite (if any) */
        if (error_number < ZINT_ERROR) {
            error_number = ZINT_WARN_USES_ECI;
            if (!(symbol->debug & ZINT_DEBUG_TEST)) {
                errtxtf(0, symbol, 222, "Encoded data includes ECI %d", first_eci_set);
            }
            if (symbol->debug & ZINT_DEBUG_PRINT) printf("Added ECI %d\n", first_eci_set);
        }
    }

    if (error_number == 0) {
        error_number = warn_number; /* Already tagged */
    } else {
        error_number = error_tag(error_number, symbol, -1, NULL);
    }

    if (error_number < ZINT_ERROR) {
        if (symbol->height < 0.5f) { /* Absolute minimum */
            (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
        }
    }

    return error_number;
}

/* Helper for output routines to check `rotate_angle` and dottiness */
static int check_output_args(struct zint_symbol *symbol, int rotate_angle) {

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    switch (rotate_angle) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        default:
            return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 223, "Invalid rotation angle");
            break;
    }

    if ((symbol->output_options & BARCODE_DOTTY_MODE) && !(is_dotty(symbol->symbology))) {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 224, "Selected symbology cannot be rendered as dots");
    }

    return 0;
}

static const struct { const char extension[4]; int is_raster; int filetype; } filetypes[] = {
    { "BMP", 1, OUT_BMP_FILE }, { "EMF", 0, OUT_EMF_FILE }, { "EPS", 0, OUT_EPS_FILE },
    { "GIF", 1, OUT_GIF_FILE }, { "PCX", 1, OUT_PCX_FILE }, { "PNG", 1, OUT_PNG_FILE },
    { "SVG", 0, OUT_SVG_FILE }, { "TIF", 1, OUT_TIF_FILE }, { "TXT", 0, 0 }
};

/* Return index of `extension` in `filetypes`, or -1 if not found */
static int filetype_idx(const char *extension) {
    char uc_extension[4] = {0};
    int i;

    if (strlen(extension) != 3) {
        return -1;
    }
    memcpy(uc_extension, extension, 3);
    to_upper((unsigned char *) uc_extension, 3);

    for (i = 0; i < ARRAY_SIZE(filetypes); i++) {
        if (strcmp(uc_extension, filetypes[i].extension) == 0) {
            break;
        }
    }

    return i == ARRAY_SIZE(filetypes) ? -1 : i;
}

/* Output a previously encoded symbol to file `symbol->outfile` */
int ZBarcode_Print(struct zint_symbol *symbol, int rotate_angle) {
    int error_number;
    int len;

    if ((error_number = check_output_args(symbol, rotate_angle))) { /* >= ZINT_ERROR only */
        return error_number; /* Already tagged */
    }

    len = (int) strlen(symbol->outfile);
    if (len > 3) {
        int i = filetype_idx(symbol->outfile + len - 3);
        if (i >= 0) {
            if (filetypes[i].filetype) {
                if (filetypes[i].is_raster) {
                    error_number = plot_raster(symbol, rotate_angle, filetypes[i].filetype);
                } else {
                    error_number = plot_vector(symbol, rotate_angle, filetypes[i].filetype);
                }
            } else {
                error_number = dump_plot(symbol);
            }
        } else {
            return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 225, "Unknown output format");
        }
    } else {
        return error_tag(ZINT_ERROR_INVALID_OPTION, symbol, 226, "Unknown output format");
    }

    return error_tag(error_number, symbol, -1, NULL);
}

/* Output a previously encoded symbol to memory as raster (`symbol->bitmap`) */
int ZBarcode_Buffer(struct zint_symbol *symbol, int rotate_angle) {
    int error_number;

    if ((error_number = check_output_args(symbol, rotate_angle))) { /* >= ZINT_ERROR only */
        return error_number; /* Already tagged */
    }

    error_number = plot_raster(symbol, rotate_angle, OUT_BUFFER);
    return error_tag(error_number, symbol, -1, NULL);
}

/* Output a previously encoded symbol to memory as vector (`symbol->vector`) */
int ZBarcode_Buffer_Vector(struct zint_symbol *symbol, int rotate_angle) {
    int error_number;

    if ((error_number = check_output_args(symbol, rotate_angle))) { /* >= ZINT_ERROR only */
        return error_number; /* Already tagged */
    }

    error_number = plot_vector(symbol, rotate_angle, OUT_BUFFER);
    return error_tag(error_number, symbol, -1, NULL);
}

/* Encode and output a symbol to file `symbol->outfile` */
int ZBarcode_Encode_and_Print(struct zint_symbol *symbol, const unsigned char *source, int length, int rotate_angle) {
    struct zint_seg segs[1];

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    segs[0].eci = symbol->eci;
    segs[0].source = (unsigned char *) source;
    segs[0].length = length;

    return ZBarcode_Encode_Segs_and_Print(symbol, segs, 1, rotate_angle);
}

/* Encode a symbol with multiple ECI segments and output to file `symbol->outfile` */
int ZBarcode_Encode_Segs_and_Print(struct zint_symbol *symbol, const struct zint_seg segs[], const int seg_count,
            int rotate_angle) {
    int error_number;
    int warn_number;

    warn_number = ZBarcode_Encode_Segs(symbol, segs, seg_count);
    if (warn_number >= ZINT_ERROR) {
        return warn_number;
    }

    error_number = ZBarcode_Print(symbol, rotate_angle);

    return error_number ? error_number : warn_number;
}

/* Encode and output a symbol to memory as raster (`symbol->bitmap`) */
int ZBarcode_Encode_and_Buffer(struct zint_symbol *symbol, const unsigned char *source, int length,
            int rotate_angle) {
    struct zint_seg segs[1];

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    segs[0].eci = symbol->eci;
    segs[0].source = (unsigned char *) source;
    segs[0].length = length;

    return ZBarcode_Encode_Segs_and_Buffer(symbol, segs, 1, rotate_angle);
}

/* Encode a symbol with multiple ECI segments and output to memory as raster (`symbol->bitmap`) */
int ZBarcode_Encode_Segs_and_Buffer(struct zint_symbol *symbol, const struct zint_seg segs[],
            const int seg_count, int rotate_angle) {
    int error_number;
    int warn_number;

    warn_number = ZBarcode_Encode_Segs(symbol, segs, seg_count);
    if (warn_number >= ZINT_ERROR) {
        return warn_number;
    }

    error_number = ZBarcode_Buffer(symbol, rotate_angle);

    return error_number ? error_number : warn_number;
}

/* Encode and output a symbol to memory as vector (`symbol->vector`) */
int ZBarcode_Encode_and_Buffer_Vector(struct zint_symbol *symbol, const unsigned char *source, int length,
            int rotate_angle) {
    struct zint_seg segs[1];

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    segs[0].eci = symbol->eci;
    segs[0].source = (unsigned char *) source;
    segs[0].length = length;

    return ZBarcode_Encode_Segs_and_Buffer_Vector(symbol, segs, 1, rotate_angle);
}

/* Encode a symbol with multiple ECI segments and output to memory as vector (`symbol->vector`) */
int ZBarcode_Encode_Segs_and_Buffer_Vector(struct zint_symbol *symbol, const struct zint_seg segs[],
            const int seg_count, int rotate_angle) {
    int error_number;
    int warn_number;

    warn_number = ZBarcode_Encode_Segs(symbol, segs, seg_count);
    if (warn_number >= ZINT_ERROR) {
        return warn_number;
    }

    error_number = ZBarcode_Buffer_Vector(symbol, rotate_angle);

    return error_number ? error_number : warn_number;
}

/* Encode a barcode using input data from file `filename` */
int ZBarcode_Encode_File(struct zint_symbol *symbol, const char *filename) {
    FILE *file;
    int file_opened = 0;
    unsigned char *buffer;
    long fileLen;
    size_t n;
    size_t nRead = 0;
    int ret;

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    if (!filename) {
        return error_tag(ZINT_ERROR_INVALID_DATA, symbol, 239, "Filename NULL");
    }

    if (strcmp(filename, "-") == 0) {
        file = stdin;
        fileLen = ZINT_MAX_DATA_LEN;
    } else {
#ifdef _WIN32
        file = out_win_fopen(filename, "rb");
#else
        file = fopen(filename, "rb");
#endif
        if (!file) {
            errtxtf(0, symbol, 229, "Unable to read input file (%1$d: %2$s)", errno, strerror(errno));
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, -1, NULL);
        }
        file_opened = 1;

        /* Get file length */
        if (fseek(file, 0, SEEK_END) != 0) {
            errtxtf(0, symbol, 797, "Unable to seek input file (%1$d: %2$s)", errno, strerror(errno));
            (void) fclose(file);
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, -1, NULL);
        }

        fileLen = ftell(file);

        /* On many Linux distros `ftell()` returns LONG_MAX not -1 on error */
        if (fileLen <= 0 || fileLen == LONG_MAX) {
            (void) fclose(file);
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, 235, "Input file empty or unseekable");
        }
        if (fileLen > ZINT_MAX_DATA_LEN) {
            (void) fclose(file);
            return error_tag(ZINT_ERROR_TOO_LONG, symbol, 230, "Input file too long");
        }

        if (fseek(file, 0, SEEK_SET) != 0) {
            errtxtf(0, symbol, 793, "Unable to seek input file (%1$d: %2$s)", errno, strerror(errno));
            (void) fclose(file);
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, -1, NULL);
        }
    }

    /* Allocate memory */
    buffer = (unsigned char *) malloc(fileLen);
    if (!buffer) {
        if (file_opened) {
            (void) fclose(file);
        }
        return error_tag(ZINT_ERROR_MEMORY, symbol, 231, "Insufficient memory for file read buffer");
    }

    /* Read file contents into buffer */

    do {
        n = fread(buffer + nRead, 1, fileLen - nRead, file);
        if (ferror(file)) {
            errtxtf(0, symbol, 241, "Input file read error (%1$d: %2$s)", errno, strerror(errno));
            free(buffer);
            if (file_opened) {
                (void) fclose(file);
            }
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, -1, NULL);
        }
        nRead += n;
    } while (!feof(file) && (0 < n) && ((long) nRead < fileLen));

    if (file_opened) {
        if (fclose(file) != 0) {
            errtxtf(0, symbol, 794, "Failure on closing input file (%1$d: %2$s)", errno, strerror(errno));
            free(buffer);
            return error_tag(ZINT_ERROR_INVALID_DATA, symbol, -1, NULL);
        }
    }
    ret = ZBarcode_Encode(symbol, buffer, (int) nRead);
    free(buffer);
    return ret;
}

/* Encode a symbol using input data from file `filename` and output to file `symbol->outfile` */
int ZBarcode_Encode_File_and_Print(struct zint_symbol *symbol, const char *filename, int rotate_angle) {
    int error_number;
    int warn_number;

    warn_number = ZBarcode_Encode_File(symbol, filename);
    if (warn_number >= ZINT_ERROR) {
        return warn_number;
    }

    error_number = ZBarcode_Print(symbol, rotate_angle);

    return error_number ? error_number : warn_number;
}

/* Encode a symbol using input data from file `filename` and output to memory as raster (`symbol->bitmap`) */
int ZBarcode_Encode_File_and_Buffer(struct zint_symbol *symbol, char const *filename, int rotate_angle) {
    int error_number;
    int warn_number;

    warn_number = ZBarcode_Encode_File(symbol, filename);
    if (warn_number >= ZINT_ERROR) {
        return warn_number;
    }

    error_number = ZBarcode_Buffer(symbol, rotate_angle);

    return error_number ? error_number : warn_number;
}

/* Encode a symbol using input data from file `filename` and output to memory as vector (`symbol->vector`) */
int ZBarcode_Encode_File_and_Buffer_Vector(struct zint_symbol *symbol, const char *filename, int rotate_angle) {
    int error_number;
    int warn_number;

    warn_number = ZBarcode_Encode_File(symbol, filename);
    if (warn_number >= ZINT_ERROR) {
        return warn_number;
    }

    error_number = ZBarcode_Buffer_Vector(symbol, rotate_angle);

    return error_number ? error_number : warn_number;
}

/* Checks whether a symbology is supported */
int ZBarcode_ValidID(int symbol_id) {

    if (symbol_id <= 0 || symbol_id > BARCODE_LAST) {
        return 0;
    }

    return barcode_src_funcs[symbol_id] != NULL
            || (symbol_id >= LIB_SEG_FUNCS_START && barcode_seg_funcs[symbol_id - LIB_SEG_FUNCS_START] != NULL);
}

/* Copy BARCODE_XXX name of `symbol_id` into `name` buffer, NUL-terminated.
   Returns 0 if valid, 1 if not valid */
int ZBarcode_BarcodeName(int symbol_id, char name[32]) {
    static const char *const names[] = {
        "",            "CODE11",      "C25STANDARD", "C25INTER",       "C25IATA",        /*0-4*/
        "",            "C25LOGIC",    "C25IND",      "CODE39",         "EXCODE39",       /*5-9*/
        "",            "",            "",            "EANX",           "EANX_CHK",       /*10-14*/
        "",            "GS1_128",     "",            "CODABAR",        "",               /*15-19*/
        "CODE128",     "DPLEIT",      "DPIDENT",     "CODE16K",        "CODE49",         /*20-24*/
        "CODE93",      "",            "",            "FLAT",           "DBAR_OMN",       /*25-29*/
        "DBAR_LTD",    "DBAR_EXP",    "TELEPEN",     "",               "UPCA",           /*30-34*/
        "UPCA_CHK",    "",            "UPCE",        "UPCE_CHK",       "",               /*35-39*/
        "POSTNET",     "",            "",            "",               "",               /*40-44*/
        "",            "",            "MSI_PLESSEY", "",               "FIM",            /*45-49*/
        "LOGMARS",     "PHARMA",      "PZN",         "PHARMA_TWO",     "CEPNET",         /*50-54*/
        "PDF417",      "PDF417COMP",  "MAXICODE",    "QRCODE",         "",               /*55-59*/
        "CODE128AB",   "",            "",            "AUSPOST",        "",               /*60-64*/
        "",            "AUSREPLY",    "AUSROUTE",    "AUSREDIRECT",    "ISBNX",          /*65-69*/
        "RM4SCC",      "DATAMATRIX",  "EAN14",       "VIN",            "CODABLOCKF",     /*70-74*/
        "NVE18",       "JAPANPOST",   "KOREAPOST",   "",               "DBAR_STK",       /*75-79*/
        "DBAR_OMNSTK", "DBAR_EXPSTK", "PLANET",      "",               "MICROPDF417",    /*80-84*/
        "USPS_IMAIL",  "PLESSEY",     "TELEPEN_NUM", "",               "ITF14",          /*85-89*/
        "KIX",         "",            "AZTEC",       "DAFT",            "",              /*90-94*/
        "",            "DPD",         "MICROQR",     "HIBC_128",       "HIBC_39",        /*95-99*/
        "",            "",            "HIBC_DM",     "",               "HIBC_QR",        /*100-104*/
        "",            "HIBC_PDF",    "",            "HIBC_MICPDF",    "",               /*105-109*/
        "HIBC_BLOCKF", "",            "HIBC_AZTEC",  "",               "",               /*110-114*/
        "DOTCODE",     "HANXIN",      "",            "",               "MAILMARK_2D",    /*115-119*/
        "UPU_S10",     "MAILMARK_4S", "",            "",               "",               /*120-124*/
        "",            "",            "",            "AZRUNE",         "CODE32",         /*125-129*/
        "EANX_CC",     "GS1_128_CC",  "DBAR_OMN_CC", "DBAR_LTD_CC",    "DBAR_EXP_CC",    /*130-134*/
        "UPCA_CC",     "UPCE_CC",     "DBAR_STK_CC", "DBAR_OMNSTK_CC", "DBAR_EXPSTK_CC", /*135-139*/
        "CHANNEL",     "CODEONE",     "GRIDMATRIX",  "UPNQR",          "ULTRA",          /*140-144*/
        "RMQR",        "BC412",       "DXFILMEDGE",                                      /*145-147*/
    };

    name[0] = '\0';

    if (!ZBarcode_ValidID(symbol_id)) {
        return 1;
    }
    assert(symbol_id >= 0 && symbol_id < ARRAY_SIZE(names) && names[symbol_id][0]);

    memcpy(name, "BARCODE_", 8);
    strcpy(name + 8, names[symbol_id]);

    return 0;
}

/* Return the capability flags for symbology `symbol_id` that match `cap_flag` */
unsigned int ZBarcode_Cap(int symbol_id, unsigned int cap_flag) {
    unsigned int result = 0;

    if (!ZBarcode_ValidID(symbol_id)) {
        return 0;
    }

    if ((cap_flag & ZINT_CAP_HRT) && has_hrt(symbol_id)) {
        result |= ZINT_CAP_HRT;
    }
    if ((cap_flag & ZINT_CAP_STACKABLE) && is_stackable(symbol_id)) {
        result |= ZINT_CAP_STACKABLE;
    }
    if ((cap_flag & ZINT_CAP_EANUPC) && is_upcean(symbol_id)) {
        result |= ZINT_CAP_EANUPC;
    }
    if ((cap_flag & ZINT_CAP_COMPOSITE) && is_composite(symbol_id)) {
        result |= ZINT_CAP_COMPOSITE;
    }
    if ((cap_flag & ZINT_CAP_ECI) && supports_eci(symbol_id)) {
        result |= ZINT_CAP_ECI;
    }
    if ((cap_flag & ZINT_CAP_GS1) && gs1_compliant(symbol_id)) {
        result |= ZINT_CAP_GS1;
    }
    if ((cap_flag & ZINT_CAP_DOTTY) && is_dotty(symbol_id)) {
        result |= ZINT_CAP_DOTTY;
    }
    if (cap_flag & ZINT_CAP_QUIET_ZONES) {
        switch (symbol_id) { /* See `out_quiet_zones()` in "output.c" */
            case BARCODE_CODE16K:
            case BARCODE_CODE49:
            case BARCODE_CODABLOCKF:
            case BARCODE_HIBC_BLOCKF:
            case BARCODE_ITF14:
            case BARCODE_EANX:
            case BARCODE_EANX_CHK:
            case BARCODE_EANX_CC:
            case BARCODE_ISBNX:
            case BARCODE_UPCA:
            case BARCODE_UPCA_CHK:
            case BARCODE_UPCA_CC:
            case BARCODE_UPCE:
            case BARCODE_UPCE_CHK:
            case BARCODE_UPCE_CC:
                result |= ZINT_CAP_QUIET_ZONES;
                break;
        }
    }
    if ((cap_flag & ZINT_CAP_FIXED_RATIO) && is_fixed_ratio(symbol_id)) {
        result |= ZINT_CAP_FIXED_RATIO;
    }
    if (cap_flag & ZINT_CAP_READER_INIT) {
        /* Note does not include HIBC versions */
        switch (symbol_id) {
            case BARCODE_CODE128: /* Note does not include GS1_128 or NVE18 */
            case BARCODE_CODE128AB:
            case BARCODE_CODE16K:
            case BARCODE_CODABLOCKF:
            case BARCODE_PDF417:
            case BARCODE_PDF417COMP:
            case BARCODE_DATAMATRIX:
            case BARCODE_MICROPDF417:
            case BARCODE_AZTEC:
            case BARCODE_DOTCODE:
            case BARCODE_GRIDMATRIX:
            case BARCODE_ULTRA:
                result |= ZINT_CAP_READER_INIT;
                break;
        }
    }
    if (cap_flag & ZINT_CAP_FULL_MULTIBYTE) {
        switch (symbol_id) {
            case BARCODE_QRCODE:
            case BARCODE_MICROQR:
            /* case BARCODE_HIBC_QR: Note character set restricted to ASCII subset */
            /* case BARCODE_UPNQR: Note does not use Kanji mode */
            case BARCODE_RMQR:
            case BARCODE_HANXIN:
            case BARCODE_GRIDMATRIX:
                result |= ZINT_CAP_FULL_MULTIBYTE;
                break;
        }
    }
    if (cap_flag & ZINT_CAP_MASK) {
        switch (symbol_id) {
            case BARCODE_QRCODE:
            case BARCODE_MICROQR:
            case BARCODE_UPNQR:
            case BARCODE_HANXIN:
            case BARCODE_DOTCODE:
                result |= ZINT_CAP_MASK;
                break;
        }
    }
    if (cap_flag & ZINT_CAP_STRUCTAPP) {
        switch (symbol_id) {
            case BARCODE_PDF417:
            case BARCODE_PDF417COMP:
            case BARCODE_MAXICODE:
            case BARCODE_QRCODE: /* Note does not include MICROQR, UPNQR or rMQR */
            case BARCODE_DATAMATRIX:
            case BARCODE_MICROPDF417:
            case BARCODE_AZTEC:
            case BARCODE_HIBC_DM:
            case BARCODE_HIBC_QR:
            case BARCODE_HIBC_PDF:
            case BARCODE_HIBC_MICPDF:
            case BARCODE_HIBC_AZTEC:
            case BARCODE_DOTCODE:
            case BARCODE_CODEONE:
            case BARCODE_GRIDMATRIX:
            case BARCODE_ULTRA:
                result |= ZINT_CAP_STRUCTAPP;
                break;
        }
    }
    if ((cap_flag & ZINT_CAP_COMPLIANT_HEIGHT) && !is_fixed_ratio(symbol_id)) {
        switch (symbol_id) {
            /* These don't have a compliant height defined */
            case BARCODE_CODE11: /* TODO: Find doc */
            case BARCODE_C25STANDARD: /* For C25 only have doc for C25INTER */
            case BARCODE_C25IATA:
            case BARCODE_C25LOGIC:
            case BARCODE_C25IND:
            case BARCODE_CODE128: /* Left to application */
            case BARCODE_CODE128AB:
            case BARCODE_DPLEIT: /* TODO: Find doc */
            case BARCODE_DPIDENT: /* TODO: Find doc */
            case BARCODE_FLAT: /* TODO: Find doc */
            case BARCODE_MSI_PLESSEY: /* TODO: Find doc */
            case BARCODE_PDF417: /* Has compliant height but already warns & uses for default */
            case BARCODE_PDF417COMP:
            case BARCODE_VIN: /* Spec unlikely */
            case BARCODE_KOREAPOST: /* TODO: Find doc */
            case BARCODE_MICROPDF417: /* See PDF417 */
            case BARCODE_PLESSEY: /* TODO: Find doc */
            case BARCODE_DAFT: /* Generic */
            case BARCODE_HIBC_128: /* See CODE128 */
            case BARCODE_HIBC_PDF: /* See PDF417 */
            case BARCODE_HIBC_MICPDF: /* See PDF417 */
                break;
            default:
                result |= ZINT_CAP_COMPLIANT_HEIGHT;
                break;
        }
    }

    return result;
}

/* Return default X-dimension in mm for symbology `symbol_id`. Returns 0 on error (invalid `symbol_id`) */
float ZBarcode_Default_Xdim(int symbol_id) {
    float x_dim_mm;

    if (!ZBarcode_ValidID(symbol_id)) {
        return 0.0f;
    }
    switch (symbol_id) {
        /* Postal 2/4-track */
        case BARCODE_AUSPOST:
        case BARCODE_AUSREPLY:
        case BARCODE_AUSROUTE:
        case BARCODE_AUSREDIRECT:
            /* Australia Post Customer Barcoding Technical Specifications, average of 0.4 to 0.6 mm */
            x_dim_mm = 0.5f;
            break;
        case BARCODE_CEPNET:
        case BARCODE_POSTNET:
        case BARCODE_PLANET:
        case BARCODE_USPS_IMAIL:
            /* USPS-B-3200 Section 2.3.1, height 0.145" (average of 0.125, 0.165) / 6.235 (Zint height), same as
               USPS DMM 300 Section 708.4.2.5 using bar pitch (1" / 43) ~ 0.023" */
            x_dim_mm = 0.591f;
            break;
        case BARCODE_RM4SCC:
        case BARCODE_KIX:
        case BARCODE_MAILMARK_4S:
            /* Royal Mail Mailmark Barcode Definition Document, height 5.1mm / 8 (Zint height) == 0.6375 */
            x_dim_mm = 0.638f; /* Seems better fit to round up to 3 d.p. */
            break;
        case BARCODE_JAPANPOST:
            x_dim_mm = 0.6f; /* Japan Post Zip/Barcode Manual */
            break;

        /* GS1 (excluding GS1-128, ITF-14, GS1 QRCODE & GS1 DATAMATRIX - see default) */
        case BARCODE_EANX:
        case BARCODE_EANX_CHK:
        case BARCODE_EANX_CC:
        case BARCODE_ISBNX:
        case BARCODE_UPCA:
        case BARCODE_UPCA_CHK:
        case BARCODE_UPCA_CC:
        case BARCODE_UPCE:
        case BARCODE_UPCE_CHK:
        case BARCODE_UPCE_CC:
        case BARCODE_DBAR_OMN:
        case BARCODE_DBAR_OMN_CC:
        case BARCODE_DBAR_LTD:
        case BARCODE_DBAR_LTD_CC:
        case BARCODE_DBAR_EXP:
        case BARCODE_DBAR_EXP_CC:
        case BARCODE_DBAR_STK:
        case BARCODE_DBAR_STK_CC:
        case BARCODE_DBAR_OMNSTK:
        case BARCODE_DBAR_OMNSTK_CC:
        case BARCODE_DBAR_EXPSTK:
        case BARCODE_DBAR_EXPSTK_CC:
            x_dim_mm = 0.33f; /* GS1 General Standards 22.0 Section 5.12.3 Table 1 except DBAR_LTD Table 4 */
            break;
        case BARCODE_DXFILMEDGE:
            /* Measured on Kodak 35mm film, a DX Film Edge with frame number with 31 symbols is 12,51 mm long */
            x_dim_mm = 0.403548f;
            break;
        /* Specific */
        case BARCODE_BC412:
            x_dim_mm = 0.12f; /* SEMI T1-95 Table 1 */
            break;
        case BARCODE_CODABAR:
            x_dim_mm = 0.38f; /* EN 798:1996 Appendix D.1 (d), average of 0.33 to 0.43 mm */
            break;
        case BARCODE_CODE32:
            x_dim_mm = 0.25f; /* Allegato A Caratteristiche tecniche del bollino farmaceutico, 0.25mm */
            break;
        case BARCODE_DPD:
            x_dim_mm = 0.375f; /* DPD Parcel Label Specification Version 2.4.1 (19.01.2021) Section 4.6.1.2 */
            break;
        case BARCODE_FIM:
            /* USPS DMM 300 Section 708.9.3, 0.03125" */
            x_dim_mm = 0.79375f;
            break;
        case BARCODE_LOGMARS:
            x_dim_mm = 0.34925f; /* MIL-STD-1189 Rev. B Section 5.2, average of 0.0075" and 0.02" */
            break;
        case BARCODE_MAILMARK_2D:
            /* Royal Mail Mailmark Barcode Definition Document, Section 2.4 */
            x_dim_mm = 0.5f;
            break;
        case BARCODE_MAXICODE:
            /* ISO/IEC 16023:2000 Table 7, based on L = 25.5mm */
            x_dim_mm =  0.88f;
            break;
        case BARCODE_PHARMA:
            x_dim_mm = 0.5f; /* Laetus Pharmacode Guide Section 1.2, standard 0.5mm */
            break;
        case BARCODE_PHARMA_TWO:
            x_dim_mm = 1.0f; /* Laetus Pharmacode Guide Section 1.4, standard 1mm */
            break;
        case BARCODE_PZN:
            x_dim_mm = 0.25f; /* Technical Information regarding PZN, "normal" X 0.25mm */
            break;
        case BARCODE_TELEPEN:
        case BARCODE_TELEPEN_NUM:
            /* Telepen Barcode Symbology information and History, average of between 0.010" and 0.0125" */
            x_dim_mm = 0.28575f;
            break;
        case BARCODE_UPU_S10:
            x_dim_mm = 0.42f; /* Universal Postal Union S10 Section 8, average of 0.33mm & 0.51mm */
            break;

        /* Stacked (excluding GS1 DataBar) */
        case BARCODE_CODE16K: /* Application-defined */
        case BARCODE_CODE49: /* ANSI/AIM BC6-2000 Appendix D.2.4, C grade if > 0.25mm */
        case BARCODE_CODABLOCKF: /* Application-defined */
        case BARCODE_HIBC_BLOCKF:
        case BARCODE_PDF417: /* Maybe 0.27mm following ISO/IEC 15438:2015 Annex S.2.2 example? */
        case BARCODE_PDF417COMP:
        case BARCODE_HIBC_PDF:
        case BARCODE_MICROPDF417:
        case BARCODE_HIBC_MICPDF:
            /* Fairly arbitrarily using ISO/IEC 15416:2016 Section 5.3.1 Table 1, aperature diameters 0.125 & 0.250
              (also fits in 0.25 <= X < 0.5 range for aperature 0.2 from ISO/IEC 15415:2011 Annex D Table D.1) */
            x_dim_mm = 0.33f;
            break;

        /* Application defined (and hence pretty arbitrary) */
        default:
            if (is_fixed_ratio(symbol_id)) {
                /* GS1 General Standards 22.0 Section 5.12.3 Table 1 (general retail) */
                x_dim_mm = 0.625f;
            } else {
                /* GS1 General Standards 22.0 Section 5.12.3.4 GS1-128 Tables 2, 4, 5, 6, 8 */
                x_dim_mm = 0.495f;
            }
            break;
    }

    return x_dim_mm;
}

/* Return the scale to use for `symbol_id` for non-zero X-dimension `x_dim_mm` at `dpmm` dots per mm for
   `filetype`. If `dpmm` zero defaults to 12. If `filetype` NULL/empty, defaults to "GIF". Returns 0 on error */
float ZBarcode_Scale_From_XdimDp(int symbol_id, float x_dim_mm, float dpmm, const char *filetype) {
    int i;
    float scale;

    if (!ZBarcode_ValidID(symbol_id)) {
        return 0.0f;
    }
    if (x_dim_mm <= 0.0f || x_dim_mm > 10.0f) { /* 10mm == 0.39" */
        return 0.0f;
    }
    if (dpmm == 0.0f) {
        dpmm = 12.0f; /* ~300 dpi */
    } else if (dpmm < 0.0f || dpmm > 1000.0f) { /* 1000 dpmm == 25400 dpi */
        return 0.0f;
    }
    if (filetype && *filetype) {
        if ((i = filetype_idx(filetype)) < 0 || filetypes[i].filetype == 0) { /* Not found or TXT */
            return 0.0f;
        }
    } else {
        i = filetype_idx("GIF"); /* Default to raster */
    }

    scale = stripf(stripf(x_dim_mm) * stripf(dpmm));

    if (symbol_id == BARCODE_MAXICODE) {
        if (filetypes[i].is_raster) {
            scale /= 10.0f;
        } else if (filetypes[i].filetype == OUT_EMF_FILE) {
            scale /= 40.0f;
        } else {
            scale /= 2.0f;
        }
    } else {
        if (filetypes[i].is_raster) {
            scale = roundf(scale) / 2.0f; /* Half-integer increments */
        } else {
            scale /= 2.0f;
        }
    }
    scale = stripf(scale);

    if (scale > 200.0f) {
        scale = 200.0f;
    } else {
        if (filetypes[i].is_raster) {
            if (symbol_id == BARCODE_MAXICODE) {
                if (scale < 0.2f) {
                    scale = 0.2f;
                }
            } else if (scale < 0.5f) {
                scale = 0.5f; /* Note if dotty mode needs further bounding to 1.0 */
            }
        } else {
            if (scale < 0.1f) {
                scale = 0.1f;
            }
        }
    }

    return scale;
}

/* Reverse of `ZBarcode_Scale_From_XdimDp()` above to estimate the X-dimension or dpmm given non-zero `scale` and
   non-zero `x_dim_mm_or_dpmm`. Return value bound to dpmm max not X-dimension max. Returns 0 on error */
float ZBarcode_XdimDp_From_Scale(int symbol_id, float scale, float xdim_mm_or_dpmm, const char *filetype) {
    int i;

    if (!ZBarcode_ValidID(symbol_id)) {
        return 0.0f;
    }
    if (scale <= 0.0f || scale > 200.0f) {
        return 0.0f;
    }
    if (xdim_mm_or_dpmm <= 0.0f || xdim_mm_or_dpmm > 1000.0f) { /* 1000 dpmm == 25400 dpi */
        return 0.0f;
    }
    if (filetype && *filetype) {
        if ((i = filetype_idx(filetype)) < 0 || filetypes[i].filetype == 0) { /* Not found or TXT */
            return 0.0f;
        }
    } else {
        i = filetype_idx("GIF"); /* Default to raster */
    }

    if (symbol_id == BARCODE_MAXICODE) {
        if (filetypes[i].is_raster) {
            scale *= 10.0f;
        } else if (filetypes[i].filetype == OUT_EMF_FILE) {
            scale *= 40.0f;
        } else {
            scale *= 2.0f;
        }
    } else {
        scale *= 2.0f;
    }

    xdim_mm_or_dpmm = stripf(stripf(scale) / stripf(xdim_mm_or_dpmm));

    if (xdim_mm_or_dpmm > 1000.0f) { /* Note if X-dimension sought needs to be further bound to <= 10 on return */
        xdim_mm_or_dpmm = 1000.0f;
    }

    return xdim_mm_or_dpmm;
}

/* Whether Zint built without PNG support */
int ZBarcode_NoPng(void) {
#ifdef ZINT_NO_PNG
    return 1;
#else
    return 0;
#endif
}

/* Return the version of Zint linked to */
int ZBarcode_Version(void) {
#if ZINT_VERSION_BUILD
    return (ZINT_VERSION_MAJOR * 10000) + (ZINT_VERSION_MINOR * 100) + ZINT_VERSION_RELEASE * 10 + ZINT_VERSION_BUILD;
#else
    return (ZINT_VERSION_MAJOR * 10000) + (ZINT_VERSION_MINOR * 100) + ZINT_VERSION_RELEASE;
#endif
}

/* vim: set ts=4 sw=4 et : */
