/* tif.c - Aldus Tagged Image File Format support */
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

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "filemem.h"
#include "output.h"
#include "tif.h"
#include "tif_lzw.h"

/* PhotometricInterpretation */
#define TIF_PMI_WHITEISZERO     0
#define TIF_PMI_BLACKISZERO     1
#define TIF_PMI_RGB             2
#define TIF_PMI_PALETTE_COLOR   3
#define TIF_PMI_SEPARATED       5 /* CMYK */

/* Compression */
#define TIF_NO_COMPRESSION      1
#define TIF_LZW                 5

static void to_color_map(const unsigned char rgb[4], tiff_color_t *color_map_entry) {
    color_map_entry->red = (rgb[0] << 8) | rgb[0];
    color_map_entry->green = (rgb[1] << 8) | rgb[1];
    color_map_entry->blue = (rgb[2] << 8) | rgb[2];
}

static void to_cmyk(const char *colour, unsigned char *cmyk) {
    int cyan, magenta, yellow, black;
    unsigned char alpha;

    (void) out_colour_get_cmyk(colour, &cyan, &magenta, &yellow, &black, &alpha);
    cmyk[0] = (unsigned char) roundf(cyan * 0xFF / 100.0f);
    cmyk[1] = (unsigned char) roundf(magenta * 0xFF / 100.0f);
    cmyk[2] = (unsigned char) roundf(yellow * 0xFF / 100.0f);
    cmyk[3] = (unsigned char) roundf(black * 0xFF / 100.0f);
    cmyk[4] = alpha;
}

/* TIFF Revision 6.0 https://www.adobe.io/content/dam/udp/en/open/standards/tiff/TIFF6.pdf */
INTERNAL int tif_pixel_plot(struct zint_symbol *symbol, const unsigned char *pixelbuf) {
    unsigned char fg[4], bg[4];
    int i;
    int pmi; /* PhotometricInterpretation */
    int rows_per_strip, strip_count;
    int rows_last_strip;
    int bytes_per_strip;
    uint16_t bits_per_sample;
    int samples_per_pixel;
    int pixels_per_sample;
    unsigned char map[128];
    tiff_color_t color_map[256] = {{0}};
    unsigned char palette[32][5];
    int color_map_size = 0;
    int extra_samples = 0;
    size_t free_memory;
    int row, column, strip;
    int strip_row;
    unsigned int bytes_put;
    long total_bytes_put;
    struct filemem fm;
    struct filemem *const fmp = &fm;
    const unsigned char *pb;
    int compression = TIF_NO_COMPRESSION;
    tif_lzw_state lzw_state;
    long file_pos;
    const int output_to_stdout = symbol->output_options & BARCODE_STDOUT;
    uint32_t *strip_offset;
    uint32_t *strip_bytes;
    unsigned char *strip_buf;

    tiff_header_t header;
    uint16_t entries = 0;
    tiff_tag_t tags[20];
    uint32_t offset = 0;
    int update_offsets[20];
    int offsets = 0;
    int ifd_size;
    uint32_t temp32;
    uint16_t temp16;

    (void) out_colour_get_rgb(symbol->fgcolour, &fg[0], &fg[1], &fg[2], &fg[3]);
    (void) out_colour_get_rgb(symbol->bgcolour, &bg[0], &bg[1], &bg[2], &bg[3]);

    if (symbol->symbology == BARCODE_ULTRA) {
        static const unsigned char ultra_chars[8] = { 'W', 'C', 'B', 'M', 'R', 'Y', 'G', 'K' };

        if (symbol->output_options & CMYK_COLOUR) {
            static const unsigned char ultra_cmyks[8][4] = {
                {    0,    0,    0,    0 }, /* White */
                { 0xFF,    0,    0,    0 }, /* Cyan */
                { 0xFF, 0xFF,    0,    0 }, /* Blue */
                {    0, 0xFF,    0,    0 }, /* Magenta */
                {    0, 0xFF, 0xFF,    0 }, /* Red */
                {    0,    0, 0xFF,    0 }, /* Yellow */
                { 0xFF,    0, 0xFF,    0 }, /* Green */
                {    0,    0,    0, 0xFF }, /* Black */
            };
            for (i = 0; i < 8; i++) {
                map[ultra_chars[i]] = i;
                memcpy(palette[i], ultra_cmyks[i], 4);
                palette[i][4] = fg[3];
            }
            map['0'] = 8;
            to_cmyk(symbol->bgcolour, palette[8]);
            map['1'] = 9;
            to_cmyk(symbol->fgcolour, palette[9]);

            pmi = TIF_PMI_SEPARATED;
            bits_per_sample = 8;
            if (fg[3] == 0xff && bg[3] == 0xff) { /* If no alpha */
                samples_per_pixel = 4;
            } else {
                samples_per_pixel = 5;
                extra_samples = 1; /* Associated alpha */
            }
            pixels_per_sample = 1;
        } else {
            static const unsigned char ultra_rgbs[8][3] = {
                { 0xff, 0xff, 0xff, }, /* White */
                {    0, 0xff, 0xff, }, /* Cyan */
                {    0,    0, 0xff, }, /* Blue */
                { 0xff,    0, 0xff, }, /* Magenta */
                { 0xff,    0,    0, }, /* Red */
                { 0xff, 0xff,    0, }, /* Yellow */
                {    0, 0xff,    0, }, /* Green */
                {    0,    0,    0, }, /* Black */
            };
            for (i = 0; i < 8; i++) {
                map[ultra_chars[i]] = i;
                memcpy(palette[i], ultra_rgbs[i], 3);
                palette[i][3] = fg[3];
            }
            map['0'] = 8;
            memcpy(palette[8], bg, 4);
            map['1'] = 9;
            memcpy(palette[9], fg, 4);

            if (fg[3] == 0xff && bg[3] == 0xff) { /* If no alpha */
                pmi = TIF_PMI_PALETTE_COLOR;
                for (i = 0; i < 10; i++) {
                    to_color_map(palette[i], &color_map[i]);
                }
                bits_per_sample = 4;
                samples_per_pixel = 1;
                pixels_per_sample = 2;
                color_map_size = 16; /* 2**BitsPerSample */
            } else {
                pmi = TIF_PMI_RGB;
                bits_per_sample = 8;
                samples_per_pixel = 4;
                pixels_per_sample = 1;
                extra_samples = 1; /* Associated alpha */
            }
        }
    } else { /* fg/bg only */
        if (symbol->output_options & CMYK_COLOUR) {
            map['0'] = 0;
            to_cmyk(symbol->bgcolour, palette[0]);
            map['1'] = 1;
            to_cmyk(symbol->fgcolour, palette[1]);

            pmi = TIF_PMI_SEPARATED;
            bits_per_sample = 8;
            if (fg[3] == 0xff && bg[3] == 0xff) { /* If no alpha */
                samples_per_pixel = 4;
            } else {
                samples_per_pixel = 5;
                extra_samples = 1; /* Associated alpha */
            }
            pixels_per_sample = 1;
        } else if (bg[0] == 0xff && bg[1] == 0xff && bg[2] == 0xff && bg[3] == 0xff
                    && fg[0] == 0 && fg[1] == 0 && fg[2] == 0 && fg[3] == 0xff) {
            map['0'] = 0;
            map['1'] = 1;

            pmi = TIF_PMI_WHITEISZERO;
            bits_per_sample = 1;
            samples_per_pixel = 1;
            pixels_per_sample = 8;
        } else if (bg[0] == 0 && bg[1] == 0 && bg[2] == 0 && bg[3] == 0xff
                && fg[0] == 0xff && fg[1] == 0xff && fg[2] == 0xff && fg[3] == 0xff) {
            map['0'] = 0;
            map['1'] = 1;

            pmi = TIF_PMI_BLACKISZERO;
            bits_per_sample = 1;
            samples_per_pixel = 1;
            pixels_per_sample = 8;
        } else {
            map['0'] = 0;
            memcpy(palette[0], bg, 4);
            map['1'] = 1;
            memcpy(palette[1], fg, 4);

            pmi = TIF_PMI_PALETTE_COLOR;
            for (i = 0; i < 2; i++) {
                to_color_map(palette[i], &color_map[i]);
            }
            if (fg[3] == 0xff && bg[3] == 0xff) { /* If no alpha */
                bits_per_sample = 4;
                samples_per_pixel = 1;
                pixels_per_sample = 2;
                color_map_size = 16; /* 2**BitsPerSample */
            } else {
                bits_per_sample = 8;
                samples_per_pixel = 2;
                pixels_per_sample = 1;
                color_map_size = 256; /* 2**BitsPerSample */
                extra_samples = 1; /* Associated alpha */
            }
        }
    }

    /* TIFF Rev 6 Section 7 p.27 "Set RowsPerStrip such that the size of each strip is about 8K bytes...
     * Note that extremely wide high resolution images may have rows larger than 8K bytes; in this case,
     * RowsPerStrip should be 1, and the strip will be larger than 8K." */
    rows_per_strip = (8192 * pixels_per_sample) / (symbol->bitmap_width * samples_per_pixel);
    if (rows_per_strip == 0) {
        rows_per_strip = 1;
    }

    /* Suppresses clang-tidy clang-analyzer-core.VLASize warning */
    assert(symbol->bitmap_height > 0);

    if (rows_per_strip >= symbol->bitmap_height) {
        strip_count = 1;
        rows_per_strip = rows_last_strip = symbol->bitmap_height;
    } else {
        strip_count = symbol->bitmap_height / rows_per_strip;
        rows_last_strip = symbol->bitmap_height % rows_per_strip;
        if (rows_last_strip != 0) {
            strip_count++;
        }
        if (rows_per_strip > symbol->bitmap_height) {
            rows_per_strip = rows_last_strip = symbol->bitmap_height;
        }
    }
    assert(strip_count > 0); /* Suppress clang-analyzer-core.UndefinedBinaryOperatorResult */

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("TIFF (%dx%d) Strip Count %d, Rows Per Strip %d, Pixels Per Sample %d, Samples Per Pixel %d, PMI %d\n",
            symbol->bitmap_width, symbol->bitmap_height, strip_count, rows_per_strip, pixels_per_sample,
            samples_per_pixel, pmi);
    }

    bytes_per_strip = rows_per_strip * ((symbol->bitmap_width + pixels_per_sample - 1) / pixels_per_sample)
                        * samples_per_pixel;

    strip_offset = (uint32_t *) z_alloca(sizeof(uint32_t) * strip_count);
    strip_bytes = (uint32_t *) z_alloca(sizeof(uint32_t) * strip_count);
    strip_buf = (unsigned char *) z_alloca(bytes_per_strip + 1);

    free_memory = sizeof(tiff_header_t);

    for (i = 0; i < strip_count; i++) {
        strip_offset[i] = (uint32_t) free_memory;
        if (i != (strip_count - 1)) {
            strip_bytes[i] = bytes_per_strip;
        } else {
            if (rows_last_strip) {
                strip_bytes[i] = rows_last_strip
                                    * ((symbol->bitmap_width + pixels_per_sample - 1) / pixels_per_sample)
                                    * samples_per_pixel;
            } else {
                strip_bytes[i] = bytes_per_strip;
            }
        }
        free_memory += strip_bytes[i];
    }
    if (free_memory & 1) {
        free_memory++; /* IFD must be on word boundary */
    }

    if (free_memory > 0xffff0000) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 670, "TIF output file size too big");
    }

    /* Open output file in binary mode */
    if (!fm_open(fmp, symbol, "wb")) {
        return errtxtf(ZINT_ERROR_FILE_ACCESS, symbol, 672, "Could not open TIF output file (%1$d: %2$s)", fmp->err,
                        strerror(fmp->err));
    }
    if (!output_to_stdout) {
        compression = TIF_LZW;
        tif_lzw_init(&lzw_state);
    }

    /* Header */
    out_le_u16(header.byte_order, 0x4949); /* "II" little-endian */
    out_le_u16(header.identity, 42);
    out_le_u32(header.offset, free_memory);

    fm_write(&header, sizeof(tiff_header_t), 1, fmp);
    total_bytes_put = sizeof(tiff_header_t);

    /* Pixel data */
    pb = pixelbuf;
    strip = 0;
    strip_row = 0;
    bytes_put = 0;
    for (row = 0; row < symbol->bitmap_height; row++) {
        if (samples_per_pixel == 1) {
            if (bits_per_sample == 1) { /* WHITEISZERO or BLACKISZERO */
                for (column = 0; column < symbol->bitmap_width; column += 8) {
                    unsigned char byte = 0;
                    for (i = 0; i < 8 && column + i < symbol->bitmap_width; i++, pb++) {
                        byte |= map[*pb] << (7 - i);
                    }
                    strip_buf[bytes_put++] = byte;
                }
            } else { /* bits_per_sample == 4, PALETTE_COLOR with no alpha */
                for (column = 0; column < symbol->bitmap_width; column += 2) {
                    unsigned char byte = map[*pb++] << 4;
                    if (column + 1 < symbol->bitmap_width) {
                        byte |= map[*pb++];
                    }
                    strip_buf[bytes_put++] = byte;
                }
            }
        } else if (samples_per_pixel == 2) { /* PALETTE_COLOR with alpha */
            for (column = 0; column < symbol->bitmap_width; column++) {
                const int idx = map[*pb++];
                strip_buf[bytes_put++] = idx;
                strip_buf[bytes_put++] = palette[idx][3];
            }
        } else { /* samples_per_pixel >= 4, RGB with alpha (4) or CMYK with (5) or without (4) alpha */
            for (column = 0; column < symbol->bitmap_width; column++) {
                const int idx = map[*pb++];
                memcpy(&strip_buf[bytes_put], &palette[idx], samples_per_pixel);
                bytes_put += samples_per_pixel;
            }
        }

        strip_row++;

        if (strip_row == rows_per_strip || (strip == strip_count - 1 && strip_row == rows_last_strip)) {
            /* End of strip */
            if (compression == TIF_LZW) {
                file_pos = fm_tell(fmp);
                if (!tif_lzw_encode(&lzw_state, fmp, strip_buf, bytes_put)) { /* Only fails if can't malloc */
                    tif_lzw_cleanup(&lzw_state);
                    (void) fm_close(fmp, symbol);
                    return errtxt(ZINT_ERROR_MEMORY, symbol, 673, "Insufficient memory for TIF LZW hash table");
                }
                bytes_put = fm_tell(fmp) - file_pos;
                if (bytes_put != strip_bytes[strip]) {
                    const int diff = bytes_put - strip_bytes[strip];
                    strip_bytes[strip] = bytes_put;
                    for (i = strip + 1; i < strip_count; i++) {
                        strip_offset[i] += diff;
                    }
                }
            } else {
                fm_write(strip_buf, 1, bytes_put, fmp);
            }
            strip++;
            total_bytes_put += bytes_put;
            bytes_put = 0;
            strip_row = 0;
            /* Suppress clang-analyzer-core.UndefinedBinaryOperatorResult */
            assert(strip < strip_count || row + 1 == symbol->bitmap_height);
        }
    }

    if (total_bytes_put & 1) {
        fm_putc(0, fmp); /* IFD must be on word boundary */
        total_bytes_put++;
    }

    if (compression == TIF_LZW) {
        tif_lzw_cleanup(&lzw_state);

        file_pos = fm_tell(fmp);
        fm_seek(fmp, 4, SEEK_SET);
        free_memory = file_pos;
        temp32 = (uint32_t) free_memory;
        /* Shouldn't happen as `free_memory` checked above to be <= 0xffff0000 & should only decrease */
        if (free_memory != temp32 || (long) free_memory != file_pos) {
            (void) fm_close(fmp, symbol);
            return errtxt(ZINT_ERROR_MEMORY, symbol, 982, "TIF output file size too big");
        }
        out_le_u32(temp32, temp32);
        fm_write(&temp32, 4, 1, fmp);
        fm_seek(fmp, file_pos, SEEK_SET);
    }

    /* Image File Directory */
    out_le_u16(tags[entries].tag, 0x0100); /* ImageWidth */
    out_le_u16(tags[entries].type, 3); /* SHORT */
    out_le_u32(tags[entries].count, 1);
    out_le_u32(tags[entries++].offset, symbol->bitmap_width);

    out_le_u16(tags[entries].tag, 0x0101); /* ImageLength - number of rows */
    out_le_u16(tags[entries].type, 3); /* SHORT */
    out_le_u32(tags[entries].count, 1);
    out_le_u32(tags[entries++].offset, symbol->bitmap_height);

    if (samples_per_pixel != 1 || bits_per_sample != 1) {
        out_le_u16(tags[entries].tag, 0x0102); /* BitsPerSample */
        out_le_u16(tags[entries].type, 3); /* SHORT */
        out_le_u32(tags[entries].count, samples_per_pixel);
        if (samples_per_pixel == 1) {
            out_le_u32(tags[entries++].offset, bits_per_sample);
        } else if (samples_per_pixel == 2) { /* 2 SHORTS fit into LONG offset so packed into offset */
            out_le_u32(tags[entries++].offset, (bits_per_sample << 16) | bits_per_sample);
        } else {
            update_offsets[offsets++] = entries;
            tags[entries++].offset = (uint32_t) free_memory;
            free_memory += samples_per_pixel * 2;
        }
    }

    out_le_u16(tags[entries].tag, 0x0103); /* Compression */
    out_le_u16(tags[entries].type, 3); /* SHORT */
    out_le_u32(tags[entries].count, 1);
    out_le_u32(tags[entries++].offset, compression);

    out_le_u16(tags[entries].tag, 0x0106); /* PhotometricInterpretation */
    out_le_u16(tags[entries].type, 3); /* SHORT */
    out_le_u32(tags[entries].count, 1);
    out_le_u32(tags[entries++].offset, pmi);

    out_le_u16(tags[entries].tag, 0x0111); /* StripOffsets */
    out_le_u16(tags[entries].type, 4); /* LONG */
    out_le_u32(tags[entries].count, strip_count);
    if (strip_count == 1) {
        out_le_u32(tags[entries++].offset, strip_offset[0]);
    } else {
        update_offsets[offsets++] = entries;
        tags[entries++].offset = (uint32_t) free_memory;
        free_memory += strip_count * 4;
    }

    if (samples_per_pixel > 1) {
        out_le_u16(tags[entries].tag, 0x0115); /* SamplesPerPixel */
        out_le_u16(tags[entries].type, 3); /* SHORT */
        out_le_u32(tags[entries].count, 1);
        out_le_u32(tags[entries++].offset, samples_per_pixel);
    }

    out_le_u16(tags[entries].tag, 0x0116); /* RowsPerStrip */
    out_le_u16(tags[entries].type, 4); /* LONG */
    out_le_u32(tags[entries].count, 1);
    out_le_u32(tags[entries++].offset, rows_per_strip);

    out_le_u16(tags[entries].tag, 0x0117); /* StripByteCounts */
    out_le_u16(tags[entries].type, 4); /* LONG */
    out_le_u32(tags[entries].count, strip_count);
    if (strip_count == 1) {
        out_le_u32(tags[entries++].offset, strip_bytes[0]);
    } else {
        update_offsets[offsets++] = entries;
        tags[entries++].offset = (uint32_t) free_memory;
        free_memory += strip_count * 4;
    }

    out_le_u16(tags[entries].tag, 0x011a); /* XResolution */
    out_le_u16(tags[entries].type, 5); /* RATIONAL */
    out_le_u32(tags[entries].count, 1);
    update_offsets[offsets++] = entries;
    tags[entries++].offset = (uint32_t) free_memory;
    free_memory += 8;

    out_le_u16(tags[entries].tag, 0x011b); /* YResolution */
    out_le_u16(tags[entries].type, 5); /* RATIONAL */
    out_le_u32(tags[entries].count, 1);
    update_offsets[offsets++] = entries;
    tags[entries++].offset = (uint32_t) free_memory;
    free_memory += 8;

    out_le_u16(tags[entries].tag, 0x0128); /* ResolutionUnit */
    out_le_u16(tags[entries].type, 3); /* SHORT */
    out_le_u32(tags[entries].count, 1);
    if (symbol->dpmm) {
        out_le_u32(tags[entries++].offset, 3); /* Centimetres */
    } else {
        out_le_u32(tags[entries++].offset, 2); /* Inches */
    }

    if (color_map_size) {
        out_le_u16(tags[entries].tag, 0x0140); /* ColorMap */
        out_le_u16(tags[entries].type, 3); /* SHORT */
        out_le_u32(tags[entries].count, color_map_size * 3);
        update_offsets[offsets++] = entries;
        tags[entries++].offset = (uint32_t) free_memory;
        /* free_memory += color_map_size * 3 * 2; Unnecessary as long as last use */
    }

    if (extra_samples) {
        out_le_u16(tags[entries].tag, 0x0152); /* ExtraSamples */
        out_le_u16(tags[entries].type, 3); /* SHORT */
        out_le_u32(tags[entries].count, 1);
        out_le_u32(tags[entries++].offset, extra_samples);
    }

    ifd_size = sizeof(entries) + sizeof(tiff_tag_t) * entries + sizeof(offset);
    for (i = 0; i < offsets; i++) {
        out_le_u32(tags[update_offsets[i]].offset, tags[update_offsets[i]].offset + ifd_size);
    }

    out_le_u16(temp16, entries);
    fm_write(&temp16, sizeof(entries), 1, fmp);
    fm_write(&tags, sizeof(tiff_tag_t), entries, fmp);
    out_le_u32(offset, offset);
    fm_write(&offset, sizeof(offset), 1, fmp);
    total_bytes_put += ifd_size;

    if (samples_per_pixel > 2) {
        out_le_u16(bits_per_sample, bits_per_sample);
        for (i = 0; i < samples_per_pixel; i++) {
            fm_write(&bits_per_sample, sizeof(bits_per_sample), 1, fmp);
        }
        total_bytes_put += sizeof(bits_per_sample) * samples_per_pixel;
    }

    if (strip_count != 1) {
        /* Strip offsets */
        for (i = 0; i < strip_count; i++) {
            out_le_u32(temp32, strip_offset[i]);
            fm_write(&temp32, 4, 1, fmp);
        }

        /* Strip byte lengths */
        for (i = 0; i < strip_count; i++) {
            out_le_u32(temp32, strip_bytes[i]);
            fm_write(&temp32, 4, 1, fmp);
        }
        total_bytes_put += strip_count * 8;
    }

    /* XResolution */
    out_le_u32(temp32, symbol->dpmm ? symbol->dpmm : 72);
    fm_write(&temp32, 4, 1, fmp);
    out_le_u32(temp32, symbol->dpmm ? 10 /*cm*/ : 1);
    fm_write(&temp32, 4, 1, fmp);
    total_bytes_put += 8;

    /* YResolution */
    out_le_u32(temp32, symbol->dpmm ? symbol->dpmm : 72);
    fm_write(&temp32, 4, 1, fmp);
    out_le_u32(temp32, symbol->dpmm ? 10 /*cm*/ : 1);
    fm_write(&temp32, 4, 1, fmp);
    total_bytes_put += 8;

    if (color_map_size) {
        for (i = 0; i < color_map_size; i++) {
            fm_write(&color_map[i].red, 2, 1, fmp);
        }
        for (i = 0; i < color_map_size; i++) {
            fm_write(&color_map[i].green, 2, 1, fmp);
        }
        for (i = 0; i < color_map_size; i++) {
            fm_write(&color_map[i].blue, 2, 1, fmp);
        }
        total_bytes_put += 6 * color_map_size;
    }

    if (fm_error(fmp)) {
        errtxtf(0, symbol, 679, "Incomplete write of TIF output (%1$d: %2$s)", fmp->err, strerror(fmp->err));
        (void) fm_close(fmp, symbol);
        return ZINT_ERROR_FILE_WRITE;
    }

    if (!output_to_stdout) {
        if (fm_tell(fmp) != total_bytes_put) {
            (void) fm_close(fmp, symbol);
            return errtxt(ZINT_ERROR_FILE_WRITE, symbol, 674, "Failed to write all TIF output");
        }
    }
    if (!fm_close(fmp, symbol)) {
        return errtxtf(ZINT_ERROR_FILE_WRITE, symbol, 981, "Failure on closing TIF output file (%1$d: %2$s)",
                        fmp->err, strerror(fmp->err));
    }

    return 0;
}

/* vim: set ts=4 sw=4 et : */
