/*  zint.h - definitions for libzint */
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

/*
 * Version: 2.13.0.9 (dev) (see "zintconfig.h")
 *
 * For documentation, see "../docs/manual.txt" or "../docs/manual.html" or online at
 * https://zint.org.uk/manual/
 */

#ifndef ZINT_H
#define ZINT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /* Vector elements - see vector header `zint_vector` below */
    struct zint_vector_rect {
        float x, y;         /* Top left */
        float height, width;
        int colour;         /* -1 for foreground, 1-8 for Cyan, Blue, Magenta, Red, Yellow, Green, Black, White */
        struct zint_vector_rect *next; /* Pointer to next rectangle */
    };

    struct zint_vector_hexagon {
        float x, y;         /* Centre */
        float diameter;     /* Short (minimal) diameter (i.e. diameter of inscribed circle) */
        int rotation;       /* 0, 90, 180, 270 degrees, where 0 has apex at top, i.e. short diameter is horizontal */
        struct zint_vector_hexagon *next; /* Pointer to next hexagon */
    };

    struct zint_vector_string {
        float x, y;         /* x is relative to halign (i.e. centre, left, right), y is relative to baseline */
        float fsize;        /* Font size */
        float width;        /* Rendered width estimate */
        int length;         /* Number of characters (bytes) */
        int rotation;       /* 0, 90, 180, 270 degrees */
        int halign;         /* Horizontal alignment: 0 for centre, 1 for left, 2 for right (end) */
        unsigned char *text; /* UTF-8, NUL-terminated */
        struct zint_vector_string *next; /* Pointer to next string */
    };

    struct zint_vector_circle {
        float x, y;         /* Centre */
        float diameter;     /* Circle diameter. Does not include width (if any) */
        float width;        /* Width of circle perimeter (circumference). 0 for fill (disc) */
        int colour;         /* Zero for draw with foreground colour (else draw with background colour (legacy)) */
        struct zint_vector_circle *next; /* Pointer to next circle */
    };

    /* Vector header */
    struct zint_vector {
        float width, height; /* Width, height of barcode image (including text, whitespace) */
        struct zint_vector_rect *rectangles; /* Pointer to first rectangle */
        struct zint_vector_hexagon *hexagons; /* Pointer to first hexagon */
        struct zint_vector_string *strings; /* Pointer to first string */
        struct zint_vector_circle *circles; /* Pointer to first circle */
    };

    /* Structured Append info (see `symbol->structapp` below) - ignored unless `zint_structapp.count` is non-zero */
    struct zint_structapp {
        int index;          /* Position in Structured Append sequence, 1-based. Must be <= `count` */
        int count;          /* Number of symbols in Structured Append sequence. Set >= 2 to add SA Info */
        char id[32];        /* Optional ID to distinguish sequence, ASCII, NUL-terminated unless max 32 long */
    };

    /* Main symbol structure */
    struct zint_symbol {
        int symbology;      /* Symbol to use (see BARCODE_XXX below) */
        float height;       /* Barcode height in X-dimensions (ignored for fixed-width barcodes) */
        float scale;        /* Scale factor when printing barcode, i.e. adjusts X-dimension. Default 1 */
        int whitespace_width; /* Width in X-dimensions of whitespace to left & right of barcode */
        int whitespace_height; /* Height in X-dimensions of whitespace above & below the barcode */
        int border_width;   /* Size of border in X-dimensions */
        int output_options; /* Various output parameters (bind, box etc, see below) */
        char fgcolour[16];  /* Foreground as hexadecimal RGB/RGBA or decimal "C,M,Y,K" string, NUL-terminated */
        char bgcolour[16];  /* Background as hexadecimal RGB/RGBA or decimal "C,M,Y,K" string, NUL-terminated */
        char *fgcolor;      /* Pointer to fgcolour (alternate spelling) */
        char *bgcolor;      /* Pointer to bgcolour (alternate spelling) */
        char outfile[256];  /* Name of file to output to, NUL-terminated. Default "out.png" ("out.gif" if no PNG) */
        char primary[128];  /* Primary message data (MaxiCode, Composite), NUL-terminated */
        int option_1;       /* Symbol-specific options (see "../docs/manual.txt") */
        int option_2;       /* Symbol-specific options */
        int option_3;       /* Symbol-specific options */
        int show_hrt;       /* Show (1) or hide (0) Human Readable Text (HRT). Default 1 */
        int input_mode;     /* Encoding of input data (see DATA_MODE etc below). Default DATA_MODE */
        int eci;            /* Extended Channel Interpretation. Default 0 (none) */
        float dpmm;         /* Resolution of output in dots per mm (BMP/EMF/PCX/PNG/TIF only). Default 0 (none) */
        float dot_size;     /* Size of dots used in BARCODE_DOTTY_MODE. Default 0.8 */
        float text_gap;     /* Gap between barcode and text (HRT) in X-dimensions. Default 1 */
        float guard_descent; /* Height in X-dimensions that EAN/UPC guard bars descend. Default 5 */
        struct zint_structapp structapp; /* Structured Append info. Default structapp.count 0 (none) */
        int warn_level;     /* Affects error/warning value returned by Zint API (see WARN_XXX below) */
        int debug;          /* Debugging flags */
        unsigned char text[256]; /* Human Readable Text (HRT) (if any), UTF-8, NUL-terminated (output only) */
        int rows;           /* Number of rows used by the symbol (output only) */
        int width;          /* Width of the generated symbol (output only) */
        unsigned char encoded_data[200][144]; /* Encoded data (output only). Allows for rows of 1152 modules */
        float row_height[200]; /* Heights of rows (output only). Allows for 200 row DotCode */
        char errtxt[100];   /* Error message if an error or warning occurs, NUL-terminated (output only) */
        unsigned char *bitmap; /* Stored bitmap image (raster output only) */
        int bitmap_width;   /* Width of bitmap image (raster output only) */
        int bitmap_height;  /* Height of bitmap image (raster output only) */
        unsigned char *alphamap; /* Array of alpha values used (raster output only) */
        struct zint_vector *vector; /* Pointer to vector header (vector output only) */
        unsigned char *memfile; /* Pointer to in-memory file buffer if BARCODE_MEMORY_FILE (output only) */
        int memfile_size;   /* Length of in-memory file buffer (output only) */
    };

    /* Segment for use with `ZBarcode_Encode_Segs()` below */
    struct zint_seg {
        unsigned char *source; /* Data to encode */
        int length;         /* Length of `source`. If 0, `source` must be NUL-terminated */
        int eci;            /* Extended Channel Interpretation */
    };

/* Symbologies (`symbol->symbology`) */
    /* Tbarcode 7 codes */
#define BARCODE_CODE11          1   /* Code 11 */
#define BARCODE_C25STANDARD     2   /* 2 of 5 Standard (Matrix) */
#define BARCODE_C25MATRIX       2   /* Legacy */
#define BARCODE_C25INTER        3   /* 2 of 5 Interleaved */
#define BARCODE_C25IATA         4   /* 2 of 5 IATA */
#define BARCODE_C25LOGIC        6   /* 2 of 5 Data Logic */
#define BARCODE_C25IND          7   /* 2 of 5 Industrial */
#define BARCODE_CODE39          8   /* Code 39 */
#define BARCODE_EXCODE39        9   /* Extended Code 39 */
#define BARCODE_EANX            13  /* EAN (European Article Number) */
#define BARCODE_EANX_CHK        14  /* EAN + Check Digit */
#define BARCODE_GS1_128         16  /* GS1-128 */
#define BARCODE_EAN128          16  /* Legacy */
#define BARCODE_CODABAR         18  /* Codabar */
#define BARCODE_CODE128         20  /* Code 128 */
#define BARCODE_DPLEIT          21  /* Deutsche Post Leitcode */
#define BARCODE_DPIDENT         22  /* Deutsche Post Identcode */
#define BARCODE_CODE16K         23  /* Code 16k */
#define BARCODE_CODE49          24  /* Code 49 */
#define BARCODE_CODE93          25  /* Code 93 */
#define BARCODE_FLAT            28  /* Flattermarken */
#define BARCODE_DBAR_OMN        29  /* GS1 DataBar Omnidirectional */
#define BARCODE_RSS14           29  /* Legacy */
#define BARCODE_DBAR_LTD        30  /* GS1 DataBar Limited */
#define BARCODE_RSS_LTD         30  /* Legacy */
#define BARCODE_DBAR_EXP        31  /* GS1 DataBar Expanded */
#define BARCODE_RSS_EXP         31  /* Legacy */
#define BARCODE_TELEPEN         32  /* Telepen Alpha */
#define BARCODE_UPCA            34  /* UPC-A */
#define BARCODE_UPCA_CHK        35  /* UPC-A + Check Digit */
#define BARCODE_UPCE            37  /* UPC-E */
#define BARCODE_UPCE_CHK        38  /* UPC-E + Check Digit */
#define BARCODE_POSTNET         40  /* USPS (U.S. Postal Service) POSTNET */
#define BARCODE_MSI_PLESSEY     47  /* MSI Plessey */
#define BARCODE_FIM             49  /* Facing Identification Mark */
#define BARCODE_LOGMARS         50  /* LOGMARS */
#define BARCODE_PHARMA          51  /* Pharmacode One-Track */
#define BARCODE_PZN             52  /* Pharmazentralnummer */
#define BARCODE_PHARMA_TWO      53  /* Pharmacode Two-Track */
#define BARCODE_CEPNET          54  /* Brazilian CEPNet Postal Code */
#define BARCODE_PDF417          55  /* PDF417 */
#define BARCODE_PDF417COMP      56  /* Compact PDF417 (Truncated PDF417) */
#define BARCODE_PDF417TRUNC     56  /* Legacy */
#define BARCODE_MAXICODE        57  /* MaxiCode */
#define BARCODE_QRCODE          58  /* QR Code */
#define BARCODE_CODE128AB       60  /* Code 128 (Suppress Code Set C) */
#define BARCODE_CODE128B        60  /* Legacy */
#define BARCODE_AUSPOST         63  /* Australia Post Standard Customer */
#define BARCODE_AUSREPLY        66  /* Australia Post Reply Paid */
#define BARCODE_AUSROUTE        67  /* Australia Post Routing */
#define BARCODE_AUSREDIRECT     68  /* Australia Post Redirection */
#define BARCODE_ISBNX           69  /* ISBN */
#define BARCODE_RM4SCC          70  /* Royal Mail 4-State Customer Code */
#define BARCODE_DATAMATRIX      71  /* Data Matrix (ECC200) */
#define BARCODE_EAN14           72  /* EAN-14 */
#define BARCODE_VIN             73  /* Vehicle Identification Number */
#define BARCODE_CODABLOCKF      74  /* Codablock-F */
#define BARCODE_NVE18           75  /* NVE-18 (SSCC-18) */
#define BARCODE_JAPANPOST       76  /* Japanese Postal Code */
#define BARCODE_KOREAPOST       77  /* Korea Post */
#define BARCODE_DBAR_STK        79  /* GS1 DataBar Stacked */
#define BARCODE_RSS14STACK      79  /* Legacy */
#define BARCODE_DBAR_OMNSTK     80  /* GS1 DataBar Stacked Omnidirectional */
#define BARCODE_RSS14STACK_OMNI 80  /* Legacy */
#define BARCODE_DBAR_EXPSTK     81  /* GS1 DataBar Expanded Stacked */
#define BARCODE_RSS_EXPSTACK    81  /* Legacy */
#define BARCODE_PLANET          82  /* USPS PLANET */
#define BARCODE_MICROPDF417     84  /* MicroPDF417 */
#define BARCODE_USPS_IMAIL      85  /* USPS Intelligent Mail (OneCode) */
#define BARCODE_ONECODE         85  /* Legacy */
#define BARCODE_PLESSEY         86  /* UK Plessey */

    /* Tbarcode 8 codes */
#define BARCODE_TELEPEN_NUM     87  /* Telepen Numeric */
#define BARCODE_ITF14           89  /* ITF-14 */
#define BARCODE_KIX             90  /* Dutch Post KIX Code */
#define BARCODE_AZTEC           92  /* Aztec Code */
#define BARCODE_DAFT            93  /* DAFT Code */
#define BARCODE_DPD             96  /* DPD Code */
#define BARCODE_MICROQR         97  /* Micro QR Code */

    /* Tbarcode 9 codes */
#define BARCODE_HIBC_128        98  /* HIBC (Health Industry Barcode) Code 128 */
#define BARCODE_HIBC_39         99  /* HIBC Code 39 */
#define BARCODE_HIBC_DM         102 /* HIBC Data Matrix */
#define BARCODE_HIBC_QR         104 /* HIBC QR Code */
#define BARCODE_HIBC_PDF        106 /* HIBC PDF417 */
#define BARCODE_HIBC_MICPDF     108 /* HIBC MicroPDF417 */
#define BARCODE_HIBC_BLOCKF     110 /* HIBC Codablock-F */
#define BARCODE_HIBC_AZTEC      112 /* HIBC Aztec Code */

    /* Tbarcode 10 codes */
#define BARCODE_DOTCODE         115 /* DotCode */
#define BARCODE_HANXIN          116 /* Han Xin (Chinese Sensible) Code */

    /* Tbarcode 11 codes */
#define BARCODE_MAILMARK_2D     119 /* Royal Mail 2D Mailmark (CMDM) (Data Matrix) */
#define BARCODE_UPU_S10         120 /* Universal Postal Union S10 */
#define BARCODE_MAILMARK_4S     121 /* Royal Mail 4-State Mailmark */
#define BARCODE_MAILMARK        121 /* Legacy */

    /* Zint specific */
#define BARCODE_AZRUNE          128 /* Aztec Runes */
#define BARCODE_CODE32          129 /* Code 32 */
#define BARCODE_EANX_CC         130 /* EAN Composite */
#define BARCODE_GS1_128_CC      131 /* GS1-128 Composite */
#define BARCODE_EAN128_CC       131 /* Legacy */
#define BARCODE_DBAR_OMN_CC     132 /* GS1 DataBar Omnidirectional Composite */
#define BARCODE_RSS14_CC        132 /* Legacy */
#define BARCODE_DBAR_LTD_CC     133 /* GS1 DataBar Limited Composite */
#define BARCODE_RSS_LTD_CC      133 /* Legacy */
#define BARCODE_DBAR_EXP_CC     134 /* GS1 DataBar Expanded Composite */
#define BARCODE_RSS_EXP_CC      134 /* Legacy */
#define BARCODE_UPCA_CC         135 /* UPC-A Composite */
#define BARCODE_UPCE_CC         136 /* UPC-E Composite */
#define BARCODE_DBAR_STK_CC     137 /* GS1 DataBar Stacked Composite */
#define BARCODE_RSS14STACK_CC   137 /* Legacy */
#define BARCODE_DBAR_OMNSTK_CC  138 /* GS1 DataBar Stacked Omnidirectional Composite */
#define BARCODE_RSS14_OMNI_CC   138 /* Legacy */
#define BARCODE_DBAR_EXPSTK_CC  139 /* GS1 DataBar Expanded Stacked Composite */
#define BARCODE_RSS_EXPSTACK_CC 139 /* Legacy */
#define BARCODE_CHANNEL         140 /* Channel Code */
#define BARCODE_CODEONE         141 /* Code One */
#define BARCODE_GRIDMATRIX      142 /* Grid Matrix */
#define BARCODE_UPNQR           143 /* UPNQR (Univerzalnega Plačilnega Naloga QR) */
#define BARCODE_ULTRA           144 /* Ultracode */
#define BARCODE_RMQR            145 /* Rectangular Micro QR Code (rMQR) */
#define BARCODE_BC412           146 /* IBM BC412 (SEMI T1-95) */
#define BARCODE_DXFILMEDGE      147 /* DX Film Edge Barcode on 35mm and APS films */
#define BARCODE_LAST            147 /* Max barcode number marker, not barcode */

/* Output options (`symbol->output_options`) */
#define BARCODE_BIND_TOP        0x00001 /* Boundary bar above the symbol only (not below), does not affect stacking */
                                        /* Note: value was once used by the legacy (never-used) BARCODE_NO_ASCII */
#define BARCODE_BIND            0x00002 /* Boundary bars above & below the symbol and between stacked symbols */
#define BARCODE_BOX             0x00004 /* Box around symbol */
#define BARCODE_STDOUT          0x00008 /* Output to stdout */
#define READER_INIT             0x00010 /* Reader Initialisation (Programming) */
#define SMALL_TEXT              0x00020 /* Use smaller font */
#define BOLD_TEXT               0x00040 /* Use bold font */
#define CMYK_COLOUR             0x00080 /* CMYK colour space (Encapsulated PostScript and TIF) */
#define BARCODE_DOTTY_MODE      0x00100 /* Plot a matrix symbol using dots rather than squares */
#define GS1_GS_SEPARATOR        0x00200 /* Use GS instead of FNC1 as GS1 separator (Data Matrix) */
#define OUT_BUFFER_INTERMEDIATE 0x00400 /* Return ASCII values in bitmap buffer (OUT_BUFFER only) */
#define BARCODE_QUIET_ZONES     0x00800 /* Add compliant quiet zones (additional to any specified whitespace) */
                                        /* Note: CODE16K, CODE49, CODABLOCKF, ITF14, EAN/UPC have default quiet zones
                                         */
#define BARCODE_NO_QUIET_ZONES  0x01000 /* Disable quiet zones, notably those with defaults as listed above */
#define COMPLIANT_HEIGHT        0x02000 /* Warn if height not compliant, or use standard height (if any) as default */
#define EANUPC_GUARD_WHITESPACE 0x04000 /* Add quiet zone indicators ("<"/">") to HRT whitespace (EAN/UPC) */
#define EMBED_VECTOR_FONT       0x08000 /* Embed font in vector output - currently only for SVG output */
#define BARCODE_MEMORY_FILE     0x10000 /* Write output to in-memory buffer `memfile` instead of to `outfile` */

/* Input data types (`symbol->input_mode`) */
#define DATA_MODE               0       /* Binary */
#define UNICODE_MODE            1       /* UTF-8 */
#define GS1_MODE                2       /* GS1 */
/* The following may be OR-ed with above */
#define ESCAPE_MODE             0x0008  /* Process escape sequences */
#define GS1PARENS_MODE          0x0010  /* Process parentheses as GS1 AI delimiters (instead of square brackets) */
#define GS1NOCHECK_MODE         0x0020  /* Do not check validity of GS1 data (except that printable ASCII only) */
#define HEIGHTPERROW_MODE       0x0040  /* Interpret `height` as per-row rather than as overall height */
#define FAST_MODE               0x0080  /* Use faster if less optimal encodation or other shortcuts if available */
                                        /* Note: affects DATAMATRIX, MICROPDF417, PDF417, QRCODE & UPNQR only */
#define EXTRA_ESCAPE_MODE       0x0100  /* Process special symbology-specific escape sequences as well as others */
                                        /* Note: currently Code 128 only */

/* Data Matrix specific options (`symbol->option_3`) */
#define DM_SQUARE               100     /* Only consider square versions on automatic symbol size selection */
#define DM_DMRE                 101     /* Consider DMRE versions on automatic symbol size selection */
#define DM_ISO_144              128     /* Use ISO instead of "de facto" format for 144x144 (i.e. don't skew ECC) */

/* QR, Han Xin, Grid Matrix specific options (`symbol->option_3`) */
#define ZINT_FULL_MULTIBYTE     200     /* Enable Kanji/Hanzi compression for Latin-1 & binary data */

/* Ultracode specific option (`symbol->option_3`) */
#define ULTRA_COMPRESSION       128     /* Enable Ultracode compression (experimental) */

/* Warning and error conditions (API return values) */
#define ZINT_WARN_HRT_TRUNCATED     1   /* Human Readable Text was truncated (max 199 bytes) */
#define ZINT_WARN_INVALID_OPTION    2   /* Invalid option given but overridden by Zint */
#define ZINT_WARN_USES_ECI          3   /* Automatic ECI inserted by Zint */
#define ZINT_WARN_NONCOMPLIANT      4   /* Symbol created not compliant with standards */
#define ZINT_ERROR                  5   /* Warn/error marker, not returned */
#define ZINT_ERROR_TOO_LONG         5   /* Input data wrong length */
#define ZINT_ERROR_INVALID_DATA     6   /* Input data incorrect */
#define ZINT_ERROR_INVALID_CHECK    7   /* Input check digit incorrect */
#define ZINT_ERROR_INVALID_OPTION   8   /* Incorrect option given */
#define ZINT_ERROR_ENCODING_PROBLEM 9   /* Internal error (should not happen) */
#define ZINT_ERROR_FILE_ACCESS      10  /* Error opening output file */
#define ZINT_ERROR_MEMORY           11  /* Memory allocation (malloc) failure */
#define ZINT_ERROR_FILE_WRITE       12  /* Error writing to output file */
#define ZINT_ERROR_USES_ECI         13  /* Error counterpart of warning if WARN_FAIL_ALL set (see below) */
#define ZINT_ERROR_NONCOMPLIANT     14  /* Error counterpart of warning if WARN_FAIL_ALL set */
#define ZINT_ERROR_HRT_TRUNCATED    15  /* Error counterpart of warning if WARN_FAIL_ALL set */

/* Warning level (`symbol->warn_level`) */
#define WARN_DEFAULT            0  /* Default behaviour */
#define WARN_FAIL_ALL           2  /* Treat warning as error */

/* Capability flags (ZBarcode_Cap() `cap_flag`) */
#define ZINT_CAP_HRT                0x0001  /* Prints Human Readable Text? */
#define ZINT_CAP_STACKABLE          0x0002  /* Is stackable? */
#define ZINT_CAP_EANUPC             0x0004  /* Is EAN/UPC? */
#define ZINT_CAP_EXTENDABLE         0x0004  /* Legacy */
#define ZINT_CAP_COMPOSITE          0x0008  /* Can have composite data? */
#define ZINT_CAP_ECI                0x0010  /* Supports Extended Channel Interpretations? */
#define ZINT_CAP_GS1                0x0020  /* Supports GS1 data? */
#define ZINT_CAP_DOTTY              0x0040  /* Can be output as dots? */
#define ZINT_CAP_QUIET_ZONES        0x0080  /* Has default quiet zones? */
#define ZINT_CAP_FIXED_RATIO        0x0100  /* Has fixed width-to-height (aspect) ratio? */
#define ZINT_CAP_READER_INIT        0x0200  /* Supports Reader Initialisation? */
#define ZINT_CAP_FULL_MULTIBYTE     0x0400  /* Supports full-multibyte option? */
#define ZINT_CAP_MASK               0x0800  /* Is mask selectable? */
#define ZINT_CAP_STRUCTAPP          0x1000  /* Supports Structured Append? */
#define ZINT_CAP_COMPLIANT_HEIGHT   0x2000  /* Has compliant height? */

/* The largest amount of data that can be encoded is 4350 4-byte UTF-8 chars in Han Xin Code */
#define ZINT_MAX_DATA_LEN       17400
/* Maximum number of segments allowed for (`seg_count`) */
#define ZINT_MAX_SEG_COUNT      256

/* Debug flags (`symbol->debug`) */
#define ZINT_DEBUG_PRINT        0x0001  /* Print debug info (if any) to stdout */
#define ZINT_DEBUG_TEST         0x0002  /* For internal test use only */

#ifdef _WIN32
#  if defined(DLL_EXPORT) || defined(PIC) || defined(_USRDLL)
#    define ZINT_EXTERN __declspec(dllexport)
#  elif defined(ZINT_DLL)
#    define ZINT_EXTERN __declspec(dllimport)
#  else
#    define ZINT_EXTERN extern
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define ZINT_EXTERN extern __attribute__((__visibility__("default")))
#else
#  define ZINT_EXTERN extern
#endif

    /* Create and initialize a symbol structure */
    ZINT_EXTERN struct zint_symbol *ZBarcode_Create(void);

    /* Free any output buffers that may have been created and initialize output fields */
    ZINT_EXTERN void ZBarcode_Clear(struct zint_symbol *symbol);

    /* Free any output buffers that may have been created and reset all fields to defaults */
    ZINT_EXTERN void ZBarcode_Reset(struct zint_symbol *symbol);

    /* Free a symbol structure, including any output buffers */
    ZINT_EXTERN void ZBarcode_Delete(struct zint_symbol *symbol);


    /* Encode a barcode. If `length` is 0, `source` must be NUL-terminated */
    ZINT_EXTERN int ZBarcode_Encode(struct zint_symbol *symbol, const unsigned char *source, int length);

    /* Encode a barcode with multiple ECI segments */
    ZINT_EXTERN int ZBarcode_Encode_Segs(struct zint_symbol *symbol, const struct zint_seg segs[],
                        const int seg_count);

    /* Encode a barcode using input data from file `filename` */
    ZINT_EXTERN int ZBarcode_Encode_File(struct zint_symbol *symbol, const char *filename);

    /* Output a previously encoded symbol to file `symbol->outfile` */
    ZINT_EXTERN int ZBarcode_Print(struct zint_symbol *symbol, int rotate_angle);


    /* Encode and output a symbol to file `symbol->outfile` */
    ZINT_EXTERN int ZBarcode_Encode_and_Print(struct zint_symbol *symbol, const unsigned char *source, int length,
                        int rotate_angle);

    /* Encode a symbol with multiple ECI segments and output to file `symbol->outfile` */
    ZINT_EXTERN int ZBarcode_Encode_Segs_and_Print(struct zint_symbol *symbol, const struct zint_seg segs[],
                        const int seg_count, int rotate_angle);

    /* Encode a symbol using input data from file `filename` and output to file `symbol->outfile` */
    ZINT_EXTERN int ZBarcode_Encode_File_and_Print(struct zint_symbol *symbol, const char *filename,
                        int rotate_angle);


    /* Output a previously encoded symbol to memory as raster (`symbol->bitmap`) */
    ZINT_EXTERN int ZBarcode_Buffer(struct zint_symbol *symbol, int rotate_angle);

    /* Encode and output a symbol to memory as raster (`symbol->bitmap`) */
    ZINT_EXTERN int ZBarcode_Encode_and_Buffer(struct zint_symbol *symbol, const unsigned char *source, int length,
                        int rotate_angle);

    /* Encode a symbol with multiple ECI segments and output to memory as raster (`symbol->bitmap`) */
    ZINT_EXTERN int ZBarcode_Encode_Segs_and_Buffer(struct zint_symbol *symbol, const struct zint_seg segs[],
                        const int seg_count, int rotate_angle);

    /* Encode a symbol using input data from file `filename` and output to memory as raster (`symbol->bitmap`) */
    ZINT_EXTERN int ZBarcode_Encode_File_and_Buffer(struct zint_symbol *symbol, const char *filename,
                        int rotate_angle);


    /* Output a previously encoded symbol to memory as vector (`symbol->vector`) */
    ZINT_EXTERN int ZBarcode_Buffer_Vector(struct zint_symbol *symbol, int rotate_angle);

    /* Encode and output a symbol to memory as vector (`symbol->vector`) */
    ZINT_EXTERN int ZBarcode_Encode_and_Buffer_Vector(struct zint_symbol *symbol, const unsigned char *source,
                        int length, int rotate_angle);

    /* Encode a symbol with multiple ECI segments and output to memory as vector (`symbol->vector`) */
    ZINT_EXTERN int ZBarcode_Encode_Segs_and_Buffer_Vector(struct zint_symbol *symbol, const struct zint_seg segs[],
                        const int seg_count, int rotate_angle);

    /* Encode a symbol using input data from file `filename` and output to memory as vector (`symbol->vector`) */
    ZINT_EXTERN int ZBarcode_Encode_File_and_Buffer_Vector(struct zint_symbol *symbol, const char *filename,
                        int rotate_angle);


    /* Is `symbol_id` a recognized symbology? */
    ZINT_EXTERN int ZBarcode_ValidID(int symbol_id);

    /* Copy BARCODE_XXX name of `symbol_id` into `name` buffer, NUL-terminated.
       Returns 0 if valid, 1 if not valid */
    ZINT_EXTERN int ZBarcode_BarcodeName(int symbol_id, char name[32]);

    /* Return the capability flags for symbology `symbol_id` that match `cap_flag` */
    ZINT_EXTERN unsigned int ZBarcode_Cap(int symbol_id, unsigned int cap_flag);


    /* Return default X-dimension in mm for symbology `symbol_id`. Returns 0 on error (invalid `symbol_id`) */
    ZINT_EXTERN float ZBarcode_Default_Xdim(int symbol_id);

    /* Return the scale to use for `symbol_id` for non-zero X-dimension `x_dim_mm` at `dpmm` dots per mm for
       `filetype`. If `dpmm` zero defaults to 12. If `filetype` NULL/empty, defaults to "GIF". Returns 0 on error */
    ZINT_EXTERN float ZBarcode_Scale_From_XdimDp(int symbol_id, float x_dim_mm, float dpmm, const char *filetype);

    /* Reverse of `ZBarcode_Scale_From_XdimDp()` above to estimate the X-dimension or dpmm given non-zero `scale` and
       non-zero `x_dim_mm_or_dpmm`. Return value bound to dpmm max not X-dimension max. Returns 0 on error */
    ZINT_EXTERN float ZBarcode_XdimDp_From_Scale(int symbol_id, float scale, float x_dim_mm_or_dpmm,
                        const char *filetype);


    /* Whether Zint built without PNG support */
    ZINT_EXTERN int ZBarcode_NoPng(void);

    /* Return the version of Zint linked to */
    ZINT_EXTERN int ZBarcode_Version(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* ZINT_H */
