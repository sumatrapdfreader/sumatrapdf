/* gif.c - Handles output to gif file */
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
#include <stdio.h>
#include "common.h"
#include "filemem.h"
#include "output.h"

/* Set LZW buffer paging size to this in expectation that compressed data will fit for typical scalings */
#define GIF_LZW_PAGE_SIZE   0x100000 /* Megabyte */

struct gif_state {
    struct filemem *fmp;
    unsigned char *pOut;
    const unsigned char *pIn;
    const unsigned char *pInEnd;
    size_t OutLength;
    size_t OutPosCur;
    size_t OutByteCountPos;
    unsigned short ClearCode;
    unsigned short FreeCode;
    char fByteCountByteSet;
    char fOutPaged;
    unsigned char OutBitsFree;
    unsigned short NodeAxon[4096];
    unsigned short NodeNext[4096];
    unsigned char NodePix[4096];
    unsigned char map[256];
};

static void gif_BufferNextByte(struct gif_state *pState) {
    (pState->OutPosCur)++;
    if (pState->fOutPaged && pState->OutPosCur + 2 >= pState->OutLength) {
        /* Keep last 256 bytes so `OutByteCountPos` within range */
        fm_write(pState->pOut, 1, pState->OutPosCur - 256, pState->fmp);
        memmove(pState->pOut, pState->pOut + pState->OutPosCur - 256, 256);
        pState->OutByteCountPos -= pState->OutPosCur - 256;
        pState->OutPosCur = 256;
    }
    /* Check if this position is a byte count position
     * `fByteCountByteSet` indicates, if byte count position bytes should be
     * inserted in general.
     * If this is true, and the distance to the last byte count position is 256
     * (e.g. 255 bytes in between), a byte count byte is inserted, and the value
     * of the last one is set to 255.
     * */
    if (pState->fByteCountByteSet && (pState->OutByteCountPos + 256 == pState->OutPosCur)) {
        (pState->pOut)[pState->OutByteCountPos] = 255;
        pState->OutByteCountPos = pState->OutPosCur;
        (pState->OutPosCur)++;
    }

    (pState->pOut)[pState->OutPosCur] = 0x00;
}

static void gif_AddCodeToBuffer(struct gif_state *pState, unsigned short CodeIn, unsigned char CodeBits) {
    /* Check, if we may fill up the current byte completely */
    if (CodeBits >= pState->OutBitsFree) {
        (pState->pOut)[pState->OutPosCur] |= (unsigned char) (CodeIn << (8 - pState->OutBitsFree));
        gif_BufferNextByte(pState);
        CodeIn = (unsigned short) (CodeIn >> pState->OutBitsFree);
        CodeBits -= pState->OutBitsFree;
        pState->OutBitsFree = 8;
        /* Write a full byte if there are at least 8 code bits left */
        if (CodeBits >= pState->OutBitsFree) {
            (pState->pOut)[pState->OutPosCur] = (unsigned char) CodeIn;
            gif_BufferNextByte(pState);
            CodeIn = (unsigned short) (CodeIn >> 8);
            CodeBits -= 8;
        }
    }
    /* The remaining bits of CodeIn fit in the current byte. */
    if (CodeBits > 0) {
        (pState->pOut)[pState->OutPosCur] |= (unsigned char) (CodeIn << (8 - pState->OutBitsFree));
        pState->OutBitsFree -= CodeBits;
    }
}

static void gif_FlushStringTable(struct gif_state *pState) {
    unsigned short Pos;
    for (Pos = 0; Pos < pState->ClearCode; Pos++) {
        (pState->NodeAxon)[Pos] = 0;
    }
}

static unsigned short gif_FindPixelOutlet(struct gif_state *pState, unsigned short HeadNode, unsigned char Byte) {
    unsigned short Outlet;

    Outlet = (pState->NodeAxon)[HeadNode];
    while (Outlet) {
        if ((pState->NodePix)[Outlet] == Byte)
            return Outlet;
        Outlet = (pState->NodeNext)[Outlet];
    }
    return 0;
}

static int gif_NextCode(struct gif_state *pState, unsigned char *pPixelValueCur, unsigned char CodeBits) {
    unsigned short UpNode;
    unsigned short DownNode;
    /* start with the root node for last pixel chain */
    UpNode = *pPixelValueCur;
    if (pState->pIn == pState->pInEnd) {
        gif_AddCodeToBuffer(pState, UpNode, CodeBits);
        return 0;
    }
    *pPixelValueCur = pState->map[*pState->pIn++];
    /* Follow the string table and the data stream to the end of the longest string that has a code */
    while (0 != (DownNode = gif_FindPixelOutlet(pState, UpNode, *pPixelValueCur))) {
        UpNode = DownNode;
        if (pState->pIn == pState->pInEnd) {
            gif_AddCodeToBuffer(pState, UpNode, CodeBits);
            return 0;
        }
        *pPixelValueCur = pState->map[*pState->pIn++];
    }
    /* Submit 'UpNode' which is the code of the longest string */
    gif_AddCodeToBuffer(pState, UpNode, CodeBits);
    /* ... and extend the string by appending 'PixelValueCur' */
    /* Create a successor node for 'PixelValueCur' whose code is 'freecode' */
    (pState->NodePix)[pState->FreeCode] = *pPixelValueCur;
    (pState->NodeAxon)[pState->FreeCode] = (pState->NodeNext)[pState->FreeCode] = 0;
    /* ...and link it to the end of the chain emanating from fg_axon[UpNode]. */
    DownNode = (pState->NodeAxon)[UpNode];
    if (!DownNode) {
        (pState->NodeAxon)[UpNode] = pState->FreeCode;
    } else {
        while ((pState->NodeNext)[DownNode]) {
            DownNode = (pState->NodeNext)[DownNode];
        }
        (pState->NodeNext)[DownNode] = pState->FreeCode;
    }
    return 1;
}

static int gif_lzw(struct gif_state *pState, int paletteBitSize) {
    unsigned char PixelValueCur;
    unsigned char CodeBits;
    unsigned short Pos;

    /* > Get first data byte */
    if (pState->pIn == pState->pInEnd)
        return 0;
    PixelValueCur = pState->map[*pState->pIn++];
    /* Number of bits per data item (=pixel)
     * We need at least a value of 2, otherwise the cc and eoi code consumes
     * the whole string table
     */
    if (paletteBitSize == 1)
        paletteBitSize = 2;

    /* initial size of compression codes */
    CodeBits = paletteBitSize + 1;
    pState->ClearCode = (1 << paletteBitSize);
    pState->FreeCode = pState->ClearCode + 2;
    pState->OutBitsFree = 8;
    pState->OutPosCur = 0;
    pState->fByteCountByteSet = 0;

    for (Pos = 0; Pos < pState->ClearCode; Pos++)
        (pState->NodePix)[Pos] = (unsigned char) Pos;

    gif_FlushStringTable(pState);

    /* Write what the GIF specification calls the "code size". */
    (pState->pOut)[pState->OutPosCur] = paletteBitSize;
    /* Reserve first bytecount byte */
    gif_BufferNextByte(pState);
    pState->OutByteCountPos = pState->OutPosCur;
    gif_BufferNextByte(pState);
    pState->fByteCountByteSet = 1;
    /* Submit one 'ClearCode' as the first code */
    gif_AddCodeToBuffer(pState, pState->ClearCode, CodeBits);

    for (;;) {
        /* generate and save the next code, which may consist of multiple input pixels. */
        if (!gif_NextCode(pState, &PixelValueCur, CodeBits)) { /* Check for end of data stream */
            /* submit 'eoi' as the last item of the code stream */
            gif_AddCodeToBuffer(pState, (unsigned short) (pState->ClearCode + 1), CodeBits);
            pState->fByteCountByteSet = 0;
            if (pState->OutBitsFree < 8) {
                gif_BufferNextByte(pState);
            }
            /* > Update last bytecount byte; */
            if (pState->OutByteCountPos < pState->OutPosCur) {
                (pState->pOut)[pState->OutByteCountPos]
                    = (unsigned char) (pState->OutPosCur - pState->OutByteCountPos - 1);
            }
            pState->OutPosCur++;
            return 1;
        }
        /* Check for currently last code */
        if (pState->FreeCode == (1U << CodeBits))
            CodeBits++;
        pState->FreeCode++;
        /* Check for full stringtable - for widest compatibility with gif decoders, empty when 0xfff, not 0x1000 */
        if (pState->FreeCode == 0xfff) {
            gif_FlushStringTable(pState);
            gif_AddCodeToBuffer(pState, pState->ClearCode, CodeBits);

            CodeBits = (unsigned char) (1 + paletteBitSize);
            pState->FreeCode = (unsigned short) (pState->ClearCode + 2);
        }
    }
}

/*
 * Called function to save in gif format
 */
INTERNAL int gif_pixel_plot(struct zint_symbol *symbol, unsigned char *pixelbuf) {
    struct filemem fm;
    unsigned char outbuf[10];
    unsigned char paletteRGB[10][3];
    int paletteCount, i;
    int paletteBitSize;
    int paletteSize;
    struct gif_state State;
    int transparent_index;
    int bgindex = -1, fgindex = -1;

    static const unsigned char RGBUnused[3] = {0,0,0};
    unsigned char RGBfg[3];
    unsigned char RGBbg[3];
    unsigned char fgalpha;
    unsigned char bgalpha;

    const size_t bitmapSize = (size_t) symbol->bitmap_height * symbol->bitmap_width;

    (void) out_colour_get_rgb(symbol->fgcolour, &RGBfg[0], &RGBfg[1], &RGBfg[2], &fgalpha);
    (void) out_colour_get_rgb(symbol->bgcolour, &RGBbg[0], &RGBbg[1], &RGBbg[2], &bgalpha);

    /* prepare state array */
    State.pIn = pixelbuf;
    State.pInEnd = pixelbuf + bitmapSize;
    /* Allow for overhead of 4 == code size + byte count + overflow byte + zero terminator */
    State.OutLength = bitmapSize + 4;
    State.fOutPaged = State.OutLength > GIF_LZW_PAGE_SIZE;
    if (State.fOutPaged) {
        State.OutLength = GIF_LZW_PAGE_SIZE;
    }
    if (!(State.pOut = (unsigned char *) malloc(State.OutLength))) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 614, "Insufficient memory for GIF LZW buffer");
    }

    State.fmp = &fm;

    /* Open output file in binary mode */
    if (!fm_open(State.fmp, symbol, "wb")) {
        errtxtf(0, symbol, 611, "Could not open GIF output file (%1$d: %2$s)", State.fmp->err,
                        strerror(State.fmp->err));
        free(State.pOut);
        return ZINT_ERROR_FILE_ACCESS;
    }

    /*
     * Build a table of the used palette items.
     * Currently, there are the following 10 colour codes:
     * '0': standard background
     * '1': standard foreground
     * 'W': white
     * 'C': cyan
     * 'B': blue
     * 'M': magenta
     * 'R': red
     * 'Y': yellow
     * 'G': green
     * 'K': black
     * '0' and '1' may be identical to one of the other values
     */
    memset(State.map, 0, sizeof(State.map));
    if (symbol->symbology == BARCODE_ULTRA) {
        static const unsigned char ultra_chars[8] = { 'W', 'C', 'B', 'M', 'R', 'Y', 'G', 'K' };
        for (i = 0; i < 8; i++) {
            State.map[ultra_chars[i]] = i;
            out_colour_char_to_rgb(ultra_chars[i], &paletteRGB[i][0], &paletteRGB[i][1], &paletteRGB[i][2]);
        }
        paletteCount = 8;
        paletteBitSize = 3;

        /* For Ultracode, have foreground only if have bind/box */
        if (symbol->border_width > 0 && (symbol->output_options & (BARCODE_BIND | BARCODE_BOX | BARCODE_BIND_TOP))) {
            /* Check whether can re-use black */
            if (RGBfg[0] == 0 && RGBfg[1] == 0 && RGBfg[2] == 0) {
                State.map['1'] = fgindex = 7; /* Re-use black */
            } else {
                State.map['1'] = fgindex = paletteCount;
                memcpy(paletteRGB[paletteCount++], RGBfg, 3);
                paletteBitSize = 4;
            }
        }

        /* For Ultracode, have background only if have whitespace/quiet zones */
        if (symbol->whitespace_width > 0 || symbol->whitespace_height > 0
                || ((symbol->output_options & BARCODE_QUIET_ZONES)
                    && !(symbol->output_options & BARCODE_NO_QUIET_ZONES))) {
            /* Check whether can re-use white */
            if (RGBbg[0] == 0xff && RGBbg[1] == 0xff && RGBbg[2] == 0xff && bgalpha == fgalpha) {
                State.map['0'] = bgindex = 0; /* Re-use white */
            } else {
                State.map['0'] = bgindex = paletteCount;
                memcpy(paletteRGB[paletteCount++], RGBbg, 3);
                paletteBitSize = 4;
            }
        }
    } else {
        State.map['0'] = bgindex = 0;
        memcpy(paletteRGB[bgindex], RGBbg, 3);
        State.map['1'] = fgindex = 1;
        memcpy(paletteRGB[fgindex], RGBfg, 3);
        paletteCount = 2;
        paletteBitSize = 1;
    }

    /* Set transparency */
    /* Note: does not allow both transparent foreground and background -
     * background takes priority */
    transparent_index = -1;
    if (bgalpha == 0 && bgindex != -1) {
        /* Transparent background */
        transparent_index = bgindex;
    } else if (fgalpha == 0 && fgindex != -1) {
        /* Transparent foreground */
        transparent_index = fgindex;
    }

    /* palette size 2 ^ bit size */
    paletteSize = 1 << paletteBitSize;

    /* GIF signature (6) */
    fm_write(transparent_index == -1 ? "GIF87a" : "GIF89a", 1, 6, State.fmp);
    /* Screen Descriptor (7) */
    /* Screen Width */
    outbuf[0] = (unsigned char) (0xff & symbol->bitmap_width);
    outbuf[1] = (unsigned char) (0xff & (symbol->bitmap_width >> 8));
    /* Screen Height */
    outbuf[2] = (unsigned char) (0xff & symbol->bitmap_height);
    outbuf[3] = (unsigned char) (0xff & (symbol->bitmap_height >> 8));
    /* write ImageBits-1 to the three least significant bits of byte 5  of
     * the Screen Descriptor
     * Bits 76543210
     *      1        : Global colour map
     *       111     : 8 bit colour depth of the palette
     *          0    : Not ordered in decreasing importance
     *           xxx : palette bit size - 1
     */
    outbuf[4] = (unsigned char) (0xf0 | (0x7 & (paletteBitSize - 1)));

    /*
     * Background colour index
     * Default to 0. If colour code 0 or K is present, it is used as index
     */
    outbuf[5] = bgindex == -1 ? 0 : bgindex;
    /* Byte 7 must be 0x00  */
    outbuf[6] = 0x00;
    fm_write(outbuf, 1, 7, State.fmp);
    /* Global Color Table (paletteSize*3) */
    fm_write(paletteRGB, 1, 3*paletteCount, State.fmp);
    /* add unused palette items to fill palette size */
    for (i = paletteCount; i < paletteSize; i++) {
        fm_write(RGBUnused, 1, 3, State.fmp);
    }

    /* Graphic control extension (8) */
    /* A graphic control extension block is used for overlay gifs.
     * This is necessary to define a transparent color.
     */
    if (transparent_index != -1) {
        /* Extension Introducer = '!' */
        outbuf[0] = '!';
        /* Graphic Control Label */
        outbuf[1] = 0xf9;
        /* Block Size */
        outbuf[2] = 4;
        /* Packet fields:
         * 3 Reserved
         * 3 Disposal Method: 0 No Action, 1 No Dispose, 2: Background, 3: Prev.
         * 1 User Input Flag: 0: no user input, 1: user input
         * 1 Transparent Color Flag: 0: No Transparency, 1: Transparency index
         */
        outbuf[3] = 1;
        /* Delay Time */
        outbuf[4] = 0;
        outbuf[5] = 0;
        /* Transparent Color Index */
        outbuf[6] = (unsigned char) transparent_index;
        /* Block Terminator */
        outbuf[7] = 0;
        fm_write(outbuf, 1, 8, State.fmp);
    }
    /* Image Descriptor */
    /* Image separator character = ',' */
    outbuf[0] = ',';
    /* "Image Left" */
    outbuf[1] = 0x00;
    outbuf[2] = 0x00;
    /* "Image Top" */
    outbuf[3] = 0x00;
    outbuf[4] = 0x00;
    /* Image Width (low byte first) */
    outbuf[5] = (unsigned char) (0xff & symbol->bitmap_width);
    outbuf[6] = (unsigned char) (0xff & (symbol->bitmap_width >> 8));
    /* Image Height */
    outbuf[7] = (unsigned char) (0xff & symbol->bitmap_height);
    outbuf[8] = (unsigned char) (0xff & (symbol->bitmap_height >> 8));

    /* Byte 10 contains the interlaced flag and
     * information on the local color table.
     * There is no local color table if its most significant bit is reset.
     */
    outbuf[9] = 0x00;
    fm_write(outbuf, 1, 10, State.fmp);

    /* call lzw encoding */
    if (!gif_lzw(&State, paletteBitSize)) {
        free(State.pOut);
        (void) fm_close(State.fmp, symbol);
        return errtxt(ZINT_ERROR_MEMORY, symbol, 613, "Insufficient memory for GIF LZW buffer");
    }
    fm_write(State.pOut, 1, State.OutPosCur, State.fmp);
    free(State.pOut);

    /* GIF terminator */
    fm_putc(';', State.fmp);

    if (fm_error(State.fmp)) {
        errtxtf(0, symbol, 615, "Incomplete write of GIF output (%1$d: %2$s)", State.fmp->err,
                        strerror(State.fmp->err));
        (void) fm_close(State.fmp, symbol);
        return ZINT_ERROR_FILE_WRITE;
    }

    if (!fm_close(State.fmp, symbol)) {
        return errtxtf(ZINT_ERROR_FILE_WRITE, symbol, 617, "Failure on closing GIF output file (%1$d: %2$s)",
                        State.fmp->err, strerror(State.fmp->err));
    }

    return 0;
}

/* vim: set ts=4 sw=4 et : */
