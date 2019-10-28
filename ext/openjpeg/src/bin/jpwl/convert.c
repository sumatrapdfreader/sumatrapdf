/*
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2006-2007, Parvatha Elangovan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "opj_apps_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef OPJ_HAVE_LIBTIFF
#include <tiffio.h>
#endif /* OPJ_HAVE_LIBTIFF */

#ifdef OPJ_HAVE_LIBPNG
#include <zlib.h>
#include <png.h>
#endif /* OPJ_HAVE_LIBPNG */

#include "openjpeg.h"
#include "convert.h"

/*
 * Get logarithm of an integer and round downwards.
 *
 * log2(a)
 */
static int int_floorlog2(int a)
{
    int l;
    for (l = 0; a > 1; l++) {
        a >>= 1;
    }
    return l;
}

/* -->> -->> -->> -->>

  TGA IMAGE FORMAT

 <<-- <<-- <<-- <<-- */

#ifdef INFORMATION_ONLY
/* TGA header definition. */
struct tga_header {
    unsigned char   id_length;              /* Image id field length    */
    unsigned char   colour_map_type;        /* Colour map type          */
    unsigned char   image_type;             /* Image type               */
    /*
    ** Colour map specification
    */
    unsigned short  colour_map_index;       /* First entry index        */
    unsigned short  colour_map_length;      /* Colour map length        */
    unsigned char   colour_map_entry_size;  /* Colour map entry size    */
    /*
    ** Image specification
    */
    unsigned short  x_origin;               /* x origin of image        */
    unsigned short  y_origin;               /* u origin of image        */
    unsigned short  image_width;            /* Image width              */
    unsigned short  image_height;           /* Image height             */
    unsigned char   pixel_depth;            /* Pixel depth              */
    unsigned char   image_desc;             /* Image descriptor         */
};
#endif /* INFORMATION_ONLY */

static unsigned short get_ushort(unsigned short val)
{

#ifdef OPJ_BIG_ENDIAN
    return (((val & 0xff) << 8) + (val >> 8));
#else
    return (val);
#endif

}

#define TGA_HEADER_SIZE 18

static int tga_readheader(FILE *fp, unsigned int *bits_per_pixel,
                          unsigned int *width, unsigned int *height, int *flip_image)
{
    int palette_size;
    unsigned char *tga ;
    unsigned char id_len, cmap_type, image_type;
    unsigned char pixel_depth, image_desc;
    unsigned short cmap_index, cmap_len, cmap_entry_size;
    unsigned short x_origin, y_origin, image_w, image_h;

    if (!bits_per_pixel || !width || !height || !flip_image) {
        return 0;
    }
    tga = (unsigned char*)malloc(18);

    if (fread(tga, TGA_HEADER_SIZE, 1, fp) != 1) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        free(tga);
        return 0 ;
    }
    id_len = (unsigned char)tga[0];
    cmap_type = (unsigned char)tga[1];
    image_type = (unsigned char)tga[2];
    cmap_index = get_ushort(*(unsigned short*)(&tga[3]));
    cmap_len = get_ushort(*(unsigned short*)(&tga[5]));
    cmap_entry_size = (unsigned char)tga[7];


    x_origin = get_ushort(*(unsigned short*)(&tga[8]));
    y_origin = get_ushort(*(unsigned short*)(&tga[10]));
    image_w = get_ushort(*(unsigned short*)(&tga[12]));
    image_h = get_ushort(*(unsigned short*)(&tga[14]));
    pixel_depth = (unsigned char)tga[16];
    image_desc  = (unsigned char)tga[17];

    free(tga);

    *bits_per_pixel = (unsigned int)pixel_depth;
    *width  = (unsigned int)image_w;
    *height = (unsigned int)image_h;

    /* Ignore tga identifier, if present ... */
    if (id_len) {
        unsigned char *id = (unsigned char *) malloc(id_len);
        if (!fread(id, id_len, 1, fp)) {
            fprintf(stderr,
                    "\nError: fread return a number of element different from the expected.\n");
            free(id);
            return 0 ;
        }
        free(id);
    }

    /* Test for compressed formats ... not yet supported ...
    // Note :-  9 - RLE encoded palettized.
    //         10 - RLE encoded RGB. */
    if (image_type > 8) {
        fprintf(stderr, "Sorry, compressed tga files are not currently supported.\n");
        return 0 ;
    }

    *flip_image = !(image_desc & 32);

    /* Palettized formats are not yet supported, skip over the palette, if present ... */
    palette_size = cmap_len * (cmap_entry_size / 8);

    if (palette_size > 0) {
        fprintf(stderr, "File contains a palette - not yet supported.");
        fseek(fp, palette_size, SEEK_CUR);
    }
    return 1;
}

#ifdef OPJ_BIG_ENDIAN

static inline uint16_t swap16(uint16_t x)
{
    return (((x & 0x00ffU) << 8) | ((x & 0xff00U) >> 8));
}

#endif

static int tga_writeheader(FILE *fp, int bits_per_pixel, int width, int height,
                           opj_bool flip_image)
{
    unsigned short image_w, image_h, us0;
    unsigned char uc0, image_type;
    unsigned char pixel_depth, image_desc;

    if (!bits_per_pixel || !width || !height) {
        return 0;
    }

    pixel_depth = 0;

    if (bits_per_pixel < 256) {
        pixel_depth = (unsigned char)bits_per_pixel;
    } else {
        fprintf(stderr, "ERROR: Wrong bits per pixel inside tga_header");
        return 0;
    }
    uc0 = 0;

    if (fwrite(&uc0, 1, 1, fp) != 1) {
        goto fails;    /* id_length */
    }
    if (fwrite(&uc0, 1, 1, fp) != 1) {
        goto fails;    /* colour_map_type */
    }

    image_type = 2; /* Uncompressed. */
    if (fwrite(&image_type, 1, 1, fp) != 1) {
        goto fails;
    }

    us0 = 0;
    if (fwrite(&us0, 2, 1, fp) != 1) {
        goto fails;    /* colour_map_index */
    }
    if (fwrite(&us0, 2, 1, fp) != 1) {
        goto fails;    /* colour_map_length */
    }
    if (fwrite(&uc0, 1, 1, fp) != 1) {
        goto fails;    /* colour_map_entry_size */
    }

    if (fwrite(&us0, 2, 1, fp) != 1) {
        goto fails;    /* x_origin */
    }
    if (fwrite(&us0, 2, 1, fp) != 1) {
        goto fails;    /* y_origin */
    }

    image_w = (unsigned short)width;
    image_h = (unsigned short) height;

#ifndef OPJ_BIG_ENDIAN
    if (fwrite(&image_w, 2, 1, fp) != 1) {
        goto fails;
    }
    if (fwrite(&image_h, 2, 1, fp) != 1) {
        goto fails;
    }
#else
    image_w = swap16(image_w);
    image_h = swap16(image_h);
    if (fwrite(&image_w, 2, 1, fp) != 1) {
        goto fails;
    }
    if (fwrite(&image_h, 2, 1, fp) != 1) {
        goto fails;
    }
#endif

    if (fwrite(&pixel_depth, 1, 1, fp) != 1) {
        goto fails;
    }

    image_desc = 8; /* 8 bits per component. */

    if (flip_image) {
        image_desc |= 32;
    }
    if (fwrite(&image_desc, 1, 1, fp) != 1) {
        goto fails;
    }

    return 1;

fails:
    fputs("\nwrite_tgaheader: write ERROR\n", stderr);
    return 0;
}

opj_image_t* tgatoimage(const char *filename, opj_cparameters_t *parameters)
{
    FILE *f;
    opj_image_t *image;
    unsigned int image_width, image_height, pixel_bit_depth;
    unsigned int x, y;
    int flip_image = 0;
    opj_image_cmptparm_t cmptparm[4];   /* maximum 4 components */
    int numcomps;
    OPJ_COLOR_SPACE color_space;
    opj_bool mono ;
    opj_bool save_alpha;
    int subsampling_dx, subsampling_dy;
    int i;

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for reading !!\n", filename);
        return 0;
    }

    if (!tga_readheader(f, &pixel_bit_depth, &image_width, &image_height,
                        &flip_image)) {
        return NULL;
    }

    /* We currently only support 24 & 32 bit tga's ... */
    if (!((pixel_bit_depth == 24) || (pixel_bit_depth == 32))) {
        return NULL;
    }

    /* initialize image components */
    memset(&cmptparm[0], 0, 4 * sizeof(opj_image_cmptparm_t));

    mono = (pixel_bit_depth == 8) ||
           (pixel_bit_depth == 16);  /* Mono with & without alpha. */
    save_alpha = (pixel_bit_depth == 16) ||
                 (pixel_bit_depth == 32); /* Mono with alpha, or RGB with alpha */

    if (mono) {
        color_space = CLRSPC_GRAY;
        numcomps = save_alpha ? 2 : 1;
    } else {
        numcomps = save_alpha ? 4 : 3;
        color_space = CLRSPC_SRGB;
    }

    subsampling_dx = parameters->subsampling_dx;
    subsampling_dy = parameters->subsampling_dy;

    for (i = 0; i < numcomps; i++) {
        cmptparm[i].prec = 8;
        cmptparm[i].bpp = 8;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = subsampling_dx;
        cmptparm[i].dy = subsampling_dy;
        cmptparm[i].w = image_width;
        cmptparm[i].h = image_height;
    }

    /* create the image */
    image = opj_image_create(numcomps, &cmptparm[0], color_space);

    if (!image) {
        return NULL;
    }

    /* set image offset and reference grid */
    image->x0 = parameters->image_offset_x0;
    image->y0 = parameters->image_offset_y0;
    image->x1 = !image->x0 ? (image_width - 1) * subsampling_dx + 1 : image->x0 +
                (image_width - 1) * subsampling_dx + 1;
    image->y1 = !image->y0 ? (image_height - 1) * subsampling_dy + 1 : image->y0 +
                (image_height - 1) * subsampling_dy + 1;

    /* set image data */
    for (y = 0; y < image_height; y++) {
        int index;

        if (flip_image) {
            index = (image_height - y - 1) * image_width;
        } else {
            index = y * image_width;
        }

        if (numcomps == 3) {
            for (x = 0; x < image_width; x++) {
                unsigned char r, g, b;

                if (!fread(&b, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    return NULL;
                }
                if (!fread(&g, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    return NULL;
                }
                if (!fread(&r, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    return NULL;
                }

                image->comps[0].data[index] = r;
                image->comps[1].data[index] = g;
                image->comps[2].data[index] = b;
                index++;
            }
        } else if (numcomps == 4) {
            for (x = 0; x < image_width; x++) {
                unsigned char r, g, b, a;
                if (!fread(&b, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    return NULL;
                }
                if (!fread(&g, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    return NULL;
                }
                if (!fread(&r, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    return NULL;
                }
                if (!fread(&a, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    return NULL;
                }

                image->comps[0].data[index] = r;
                image->comps[1].data[index] = g;
                image->comps[2].data[index] = b;
                image->comps[3].data[index] = a;
                index++;
            }
        } else {
            fprintf(stderr, "Currently unsupported bit depth : %s\n", filename);
        }
    }
    return image;
}

int imagetotga(opj_image_t * image, const char *outfile)
{
    int width, height, bpp, x, y;
    opj_bool write_alpha;
    int i, adjustR, adjustG, adjustB;
    unsigned int alpha_channel;
    float r, g, b, a;
    unsigned char value;
    float scale;
    FILE *fdest;
    size_t res;

    fdest = fopen(outfile, "wb");
    if (!fdest) {
        fprintf(stderr, "ERROR -> failed to open %s for writing\n", outfile);
        return 1;
    }

    for (i = 0; i < image->numcomps - 1; i++) {
        if ((image->comps[0].dx != image->comps[i + 1].dx)
                || (image->comps[0].dy != image->comps[i + 1].dy)
                || (image->comps[0].prec != image->comps[i + 1].prec)) {
            fprintf(stderr,
                    "Unable to create a tga file with such J2K image charateristics.");
            return 1;
        }
    }

    width = image->comps[0].w;
    height = image->comps[0].h;

    /* Mono with alpha, or RGB with alpha. */
    write_alpha = (image->numcomps == 2) || (image->numcomps == 4);

    /* Write TGA header  */
    bpp = write_alpha ? 32 : 24;
    if (!tga_writeheader(fdest, bpp, width, height, OPJ_TRUE)) {
        return 1;
    }

    alpha_channel = image->numcomps - 1;

    scale = 255.0f / (float)((1 << image->comps[0].prec) - 1);

    adjustR = (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
    adjustG = (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
    adjustB = (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);

    for (y = 0; y < height; y++) {
        unsigned int index = y * width;

        for (x = 0; x < width; x++, index++)  {
            r = (float)(image->comps[0].data[index] + adjustR);

            if (image->numcomps > 2) {
                g = (float)(image->comps[1].data[index] + adjustG);
                b = (float)(image->comps[2].data[index] + adjustB);
            } else  { /* Greyscale ... */
                g = r;
                b = r;
            }

            /* TGA format writes BGR ... */
            value = (unsigned char)(b * scale);
            res = fwrite(&value, 1, 1, fdest);
            if (res < 1) {
                fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                return 1;
            }

            value = (unsigned char)(g * scale);
            res = fwrite(&value, 1, 1, fdest);
            if (res < 1) {
                fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                return 1;
            }

            value = (unsigned char)(r * scale);
            res = fwrite(&value, 1, 1, fdest);
            if (res < 1) {
                fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                return 1;
            }

            if (write_alpha) {
                a = (float)(image->comps[alpha_channel].data[index]);
                value = (unsigned char)(a * scale);
                res = fwrite(&value, 1, 1, fdest);
                if (res < 1) {
                    fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                    return 1;
                }
            }
        }
    }

    return 0;
}

/* -->> -->> -->> -->>

  BMP IMAGE FORMAT

 <<-- <<-- <<-- <<-- */

/* WORD defines a two byte word */
typedef unsigned short int WORD;

/* DWORD defines a four byte word */
typedef unsigned int DWORD;

typedef struct {
    WORD bfType;          /* 'BM' for Bitmap (19776) */
    DWORD bfSize;         /* Size of the file        */
    WORD bfReserved1;     /* Reserved : 0            */
    WORD bfReserved2;     /* Reserved : 0            */
    DWORD bfOffBits;      /* Offset                  */
} BITMAPFILEHEADER_t;

typedef struct {
    DWORD biSize;         /* Size of the structure in bytes */
    DWORD biWidth;        /* Width of the image in pixels */
    DWORD biHeight;       /* Height of the image in pixels */
    WORD biPlanes;        /* 1 */
    WORD biBitCount;      /* Number of color bits by pixels */
    DWORD biCompression;      /* Type of encoding 0: none 1: RLE8 2: RLE4 */
    DWORD biSizeImage;        /* Size of the image in bytes */
    DWORD biXpelsPerMeter;    /* Horizontal (X) resolution in pixels/meter */
    DWORD biYpelsPerMeter;    /* Vertical (Y) resolution in pixels/meter */
    DWORD biClrUsed;      /* Number of color used in the image (0: ALL) */
    DWORD biClrImportant;     /* Number of important color (0: ALL) */
} BITMAPINFOHEADER_t;

opj_image_t* bmptoimage(const char *filename, opj_cparameters_t *parameters)
{
    int subsampling_dx = parameters->subsampling_dx;
    int subsampling_dy = parameters->subsampling_dy;

    int i, numcomps, w, h;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t cmptparm[3];   /* maximum of 3 components */
    opj_image_t * image = NULL;

    FILE *IN;
    BITMAPFILEHEADER_t File_h;
    BITMAPINFOHEADER_t Info_h;
    unsigned char *RGB;
    unsigned char *table_R, *table_G, *table_B;
    unsigned int j, PAD = 0;

    int x, y, index;
    int gray_scale = 1;
    int has_color;
    DWORD W, H;

    IN = fopen(filename, "rb");
    if (!IN) {
        fprintf(stderr, "Failed to open %s for reading !!\n", filename);
        return NULL;
    }

    File_h.bfType = getc(IN);
    File_h.bfType = (getc(IN) << 8) + File_h.bfType;

    if (File_h.bfType != 19778) {
        fprintf(stderr, "Error, not a BMP file!\n");
        fclose(IN);
        return NULL;
    }
    /* FILE HEADER */
    /* ------------- */
    File_h.bfSize = getc(IN);
    File_h.bfSize = (getc(IN) << 8) + File_h.bfSize;
    File_h.bfSize = (getc(IN) << 16) + File_h.bfSize;
    File_h.bfSize = (getc(IN) << 24) + File_h.bfSize;

    File_h.bfReserved1 = getc(IN);
    File_h.bfReserved1 = (getc(IN) << 8) + File_h.bfReserved1;

    File_h.bfReserved2 = getc(IN);
    File_h.bfReserved2 = (getc(IN) << 8) + File_h.bfReserved2;

    File_h.bfOffBits = getc(IN);
    File_h.bfOffBits = (getc(IN) << 8) + File_h.bfOffBits;
    File_h.bfOffBits = (getc(IN) << 16) + File_h.bfOffBits;
    File_h.bfOffBits = (getc(IN) << 24) + File_h.bfOffBits;

    /* INFO HEADER */
    /* ------------- */

    Info_h.biSize = getc(IN);
    Info_h.biSize = (getc(IN) << 8) + Info_h.biSize;
    Info_h.biSize = (getc(IN) << 16) + Info_h.biSize;
    Info_h.biSize = (getc(IN) << 24) + Info_h.biSize;

    if (Info_h.biSize != 40) {
        fprintf(stderr, "Error, unknown BMP header size %d\n", Info_h.biSize);
        fclose(IN);
        return NULL;
    }
    Info_h.biWidth = getc(IN);
    Info_h.biWidth = (getc(IN) << 8) + Info_h.biWidth;
    Info_h.biWidth = (getc(IN) << 16) + Info_h.biWidth;
    Info_h.biWidth = (getc(IN) << 24) + Info_h.biWidth;
    w = Info_h.biWidth;

    Info_h.biHeight = getc(IN);
    Info_h.biHeight = (getc(IN) << 8) + Info_h.biHeight;
    Info_h.biHeight = (getc(IN) << 16) + Info_h.biHeight;
    Info_h.biHeight = (getc(IN) << 24) + Info_h.biHeight;
    h = Info_h.biHeight;

    Info_h.biPlanes = getc(IN);
    Info_h.biPlanes = (getc(IN) << 8) + Info_h.biPlanes;

    Info_h.biBitCount = getc(IN);
    Info_h.biBitCount = (getc(IN) << 8) + Info_h.biBitCount;

    Info_h.biCompression = getc(IN);
    Info_h.biCompression = (getc(IN) << 8) + Info_h.biCompression;
    Info_h.biCompression = (getc(IN) << 16) + Info_h.biCompression;
    Info_h.biCompression = (getc(IN) << 24) + Info_h.biCompression;

    Info_h.biSizeImage = getc(IN);
    Info_h.biSizeImage = (getc(IN) << 8) + Info_h.biSizeImage;
    Info_h.biSizeImage = (getc(IN) << 16) + Info_h.biSizeImage;
    Info_h.biSizeImage = (getc(IN) << 24) + Info_h.biSizeImage;

    Info_h.biXpelsPerMeter = getc(IN);
    Info_h.biXpelsPerMeter = (getc(IN) << 8) + Info_h.biXpelsPerMeter;
    Info_h.biXpelsPerMeter = (getc(IN) << 16) + Info_h.biXpelsPerMeter;
    Info_h.biXpelsPerMeter = (getc(IN) << 24) + Info_h.biXpelsPerMeter;

    Info_h.biYpelsPerMeter = getc(IN);
    Info_h.biYpelsPerMeter = (getc(IN) << 8) + Info_h.biYpelsPerMeter;
    Info_h.biYpelsPerMeter = (getc(IN) << 16) + Info_h.biYpelsPerMeter;
    Info_h.biYpelsPerMeter = (getc(IN) << 24) + Info_h.biYpelsPerMeter;

    Info_h.biClrUsed = getc(IN);
    Info_h.biClrUsed = (getc(IN) << 8) + Info_h.biClrUsed;
    Info_h.biClrUsed = (getc(IN) << 16) + Info_h.biClrUsed;
    Info_h.biClrUsed = (getc(IN) << 24) + Info_h.biClrUsed;

    Info_h.biClrImportant = getc(IN);
    Info_h.biClrImportant = (getc(IN) << 8) + Info_h.biClrImportant;
    Info_h.biClrImportant = (getc(IN) << 16) + Info_h.biClrImportant;
    Info_h.biClrImportant = (getc(IN) << 24) + Info_h.biClrImportant;

    /* Read the data and store them in the OUT file */

    if (Info_h.biBitCount == 24) {
        numcomps = 3;
        color_space = CLRSPC_SRGB;
        /* initialize image components */
        memset(&cmptparm[0], 0, 3 * sizeof(opj_image_cmptparm_t));
        for (i = 0; i < numcomps; i++) {
            cmptparm[i].prec = 8;
            cmptparm[i].bpp = 8;
            cmptparm[i].sgnd = 0;
            cmptparm[i].dx = subsampling_dx;
            cmptparm[i].dy = subsampling_dy;
            cmptparm[i].w = w;
            cmptparm[i].h = h;
        }
        /* create the image */
        image = opj_image_create(numcomps, &cmptparm[0], color_space);
        if (!image) {
            fclose(IN);
            return NULL;
        }

        /* set image offset and reference grid */
        image->x0 = parameters->image_offset_x0;
        image->y0 = parameters->image_offset_y0;
        image->x1 = !image->x0 ? (w - 1) * subsampling_dx + 1 : image->x0 +
                    (w - 1) * subsampling_dx + 1;
        image->y1 = !image->y0 ? (h - 1) * subsampling_dy + 1 : image->y0 +
                    (h - 1) * subsampling_dy + 1;

        /* set image data */

        /* Place the cursor at the beginning of the image information */
        fseek(IN, 0, SEEK_SET);
        fseek(IN, File_h.bfOffBits, SEEK_SET);

        W = Info_h.biWidth;
        H = Info_h.biHeight;

        /* PAD = 4 - (3 * W) % 4; */
        /* PAD = (PAD == 4) ? 0 : PAD; */
        PAD = (3 * W) % 4 ? 4 - (3 * W) % 4 : 0;

        RGB = (unsigned char *)
              malloc((3 * W + PAD) * H * sizeof(unsigned char));

        if (fread(RGB, sizeof(unsigned char), (3 * W + PAD) * H,
                  IN) != (3 * W + PAD) * H) {
            free(RGB);
            opj_image_destroy(image);
            fprintf(stderr,
                    "\nError: fread return a number of element different from the expected.\n");
            return NULL;
        }

        index = 0;

        for (y = 0; y < (int)H; y++) {
            unsigned char *scanline = RGB + (3 * W + PAD) * (H - 1 - y);
            for (x = 0; x < (int)W; x++) {
                unsigned char *pixel = &scanline[3 * x];
                image->comps[0].data[index] = pixel[2]; /* R */
                image->comps[1].data[index] = pixel[1]; /* G */
                image->comps[2].data[index] = pixel[0]; /* B */
                index++;
            }
        }
        free(RGB);
    }/* if (Info_h.biBitCount == 24) */
    else if (Info_h.biBitCount == 8 && Info_h.biCompression == 0) { /*RGB */
        if (Info_h.biClrUsed == 0) {
            Info_h.biClrUsed = 256;
        } else if (Info_h.biClrUsed > 256) {
            Info_h.biClrUsed = 256;
        }

        table_R = (unsigned char *) malloc(256 * sizeof(unsigned char));
        table_G = (unsigned char *) malloc(256 * sizeof(unsigned char));
        table_B = (unsigned char *) malloc(256 * sizeof(unsigned char));

        has_color = 0;
        for (j = 0; j < Info_h.biClrUsed; j++) {
            table_B[j] = (unsigned char)getc(IN);
            table_G[j] = (unsigned char)getc(IN);
            table_R[j] = (unsigned char)getc(IN);
            getc(IN);
            has_color +=
                !(table_R[j] == table_G[j] && table_R[j] == table_B[j]);
        }
        if (has_color) {
            gray_scale = 0;
        }

        /* Place the cursor at the beginning of the image information */
        fseek(IN, 0, SEEK_SET);
        fseek(IN, File_h.bfOffBits, SEEK_SET);

        W = Info_h.biWidth;
        H = Info_h.biHeight;
        if (Info_h.biWidth % 2) {
            W++;
        }

        numcomps = gray_scale ? 1 : 3;
        color_space = gray_scale ? CLRSPC_GRAY : CLRSPC_SRGB;
        /* initialize image components */
        memset(&cmptparm[0], 0, 3 * sizeof(opj_image_cmptparm_t));
        for (i = 0; i < numcomps; i++) {
            cmptparm[i].prec = 8;
            cmptparm[i].bpp = 8;
            cmptparm[i].sgnd = 0;
            cmptparm[i].dx = subsampling_dx;
            cmptparm[i].dy = subsampling_dy;
            cmptparm[i].w = w;
            cmptparm[i].h = h;
        }
        /* create the image */
        image = opj_image_create(numcomps, &cmptparm[0], color_space);
        if (!image) {
            fclose(IN);
            free(table_R);
            free(table_G);
            free(table_B);
            return NULL;
        }

        /* set image offset and reference grid */
        image->x0 = parameters->image_offset_x0;
        image->y0 = parameters->image_offset_y0;
        image->x1 = !image->x0 ? (w - 1) * subsampling_dx + 1 : image->x0 +
                    (w - 1) * subsampling_dx + 1;
        image->y1 = !image->y0 ? (h - 1) * subsampling_dy + 1 : image->y0 +
                    (h - 1) * subsampling_dy + 1;

        /* set image data */

        RGB = (unsigned char *) malloc(W * H * sizeof(unsigned char));

        if (fread(RGB, sizeof(unsigned char), W * H, IN) != W * H) {
            free(table_R);
            free(table_G);
            free(table_B);
            free(RGB);
            opj_image_destroy(image);
            fprintf(stderr,
                    "\nError: fread return a number of element different from the expected.\n");
            return NULL;
        }
        if (gray_scale) {
            index = 0;
            for (j = 0; j < W * H; j++) {
                if ((j % W < W - 1 && Info_h.biWidth % 2) || !(Info_h.biWidth % 2)) {
                    image->comps[0].data[index] =
                        table_R[RGB[W * H - ((j) / (W) + 1) * W + (j) % (W)]];
                    index++;
                }
            }

        } else {
            index = 0;
            for (j = 0; j < W * H; j++) {
                if ((j % W < W - 1 && Info_h.biWidth % 2)
                        || !(Info_h.biWidth % 2)) {
                    unsigned char pixel_index =
                        RGB[W * H - ((j) / (W) + 1) * W + (j) % (W)];
                    image->comps[0].data[index] = table_R[pixel_index];
                    image->comps[1].data[index] = table_G[pixel_index];
                    image->comps[2].data[index] = table_B[pixel_index];
                    index++;
                }
            }
        }
        free(RGB);
        free(table_R);
        free(table_G);
        free(table_B);
    }/* RGB8 */
    else if (Info_h.biBitCount == 8 && Info_h.biCompression == 1) { /*RLE8*/
        unsigned char *pix, *beyond;
        int *gray, *red, *green, *blue;
        unsigned int x, y, max;
        int i, c, c1;
        unsigned char uc;

        if (Info_h.biClrUsed == 0) {
            Info_h.biClrUsed = 256;
        } else if (Info_h.biClrUsed > 256) {
            Info_h.biClrUsed = 256;
        }

        table_R = (unsigned char *) malloc(256 * sizeof(unsigned char));
        table_G = (unsigned char *) malloc(256 * sizeof(unsigned char));
        table_B = (unsigned char *) malloc(256 * sizeof(unsigned char));

        has_color = 0;
        for (j = 0; j < Info_h.biClrUsed; j++) {
            table_B[j] = (unsigned char)getc(IN);
            table_G[j] = (unsigned char)getc(IN);
            table_R[j] = (unsigned char)getc(IN);
            getc(IN);
            has_color += !(table_R[j] == table_G[j] && table_R[j] == table_B[j]);
        }

        if (has_color) {
            gray_scale = 0;
        }

        numcomps = gray_scale ? 1 : 3;
        color_space = gray_scale ? CLRSPC_GRAY : CLRSPC_SRGB;
        /* initialize image components */
        memset(&cmptparm[0], 0, 3 * sizeof(opj_image_cmptparm_t));
        for (i = 0; i < numcomps; i++) {
            cmptparm[i].prec = 8;
            cmptparm[i].bpp = 8;
            cmptparm[i].sgnd = 0;
            cmptparm[i].dx = subsampling_dx;
            cmptparm[i].dy = subsampling_dy;
            cmptparm[i].w = w;
            cmptparm[i].h = h;
        }
        /* create the image */
        image = opj_image_create(numcomps, &cmptparm[0], color_space);
        if (!image) {
            fclose(IN);
            free(table_R);
            free(table_G);
            free(table_B);
            return NULL;
        }

        /* set image offset and reference grid */
        image->x0 = parameters->image_offset_x0;
        image->y0 = parameters->image_offset_y0;
        image->x1 = !image->x0 ? (w - 1) * subsampling_dx + 1 : image->x0 + (w
                    - 1) * subsampling_dx + 1;
        image->y1 = !image->y0 ? (h - 1) * subsampling_dy + 1 : image->y0 + (h
                    - 1) * subsampling_dy + 1;

        /* set image data */

        /* Place the cursor at the beginning of the image information */
        fseek(IN, 0, SEEK_SET);
        fseek(IN, File_h.bfOffBits, SEEK_SET);

        W = Info_h.biWidth;
        H = Info_h.biHeight;
        RGB = (unsigned char *) calloc(1, W * H * sizeof(unsigned char));
        beyond = RGB + W * H;
        pix = beyond - W;
        x = y = 0;

        while (y < H) {
            c = getc(IN);

            if (c) {
                c1 = getc(IN);

                for (i = 0; i < c && x < W && pix < beyond; i++, x++, pix++) {
                    *pix = (unsigned char)c1;
                }
            } else {
                c = getc(IN);

                if (c == 0x00) { /* EOL */
                    x = 0;
                    ++y;
                    pix = RGB + x + (H - y - 1) * W;
                } else if (c == 0x01) { /* EOP */
                    break;
                } else if (c == 0x02) { /* MOVE by dxdy */
                    c = getc(IN);
                    x += c;
                    c = getc(IN);
                    y += c;
                    pix = RGB + (H - y - 1) * W + x;
                } else { /* 03 .. 255 */
                    i = 0;
                    for (; i < c && x < W && pix < beyond; i++, x++, pix++) {
                        c1 = getc(IN);
                        *pix = (unsigned char)c1;
                    }
                    if (c & 1) { /* skip padding byte */
                        getc(IN);
                    }
                }
            }
        }/* while() */

        if (gray_scale) {
            gray = image->comps[0].data;
            pix = RGB;
            max = W * H;

            while (max--) {
                uc = *pix++;

                *gray++ = table_R[uc];
            }
        } else {
            /*int *red, *green, *blue;*/

            red = image->comps[0].data;
            green = image->comps[1].data;
            blue = image->comps[2].data;
            pix = RGB;
            max = W * H;

            while (max--) {
                uc = *pix++;

                *red++ = table_R[uc];
                *green++ = table_G[uc];
                *blue++ = table_B[uc];
            }
        }
        free(RGB);
        free(table_R);
        free(table_G);
        free(table_B);
    }/* RLE8 */
    else {
        fprintf(stderr,
                "Other system than 24 bits/pixels or 8 bits (no RLE coding) "
                "is not yet implemented [%d]\n", Info_h.biBitCount);
    }
    fclose(IN);
    return image;
}

int imagetobmp(opj_image_t * image, const char *outfile)
{
    int w, h;
    int i, pad;
    FILE *fdest = NULL;
    int adjustR, adjustG, adjustB;

    if (image->comps[0].prec < 8) {
        fprintf(stderr, "Unsupported precision: %d\n", image->comps[0].prec);
        return 1;
    }
    if (image->numcomps >= 3 && image->comps[0].dx == image->comps[1].dx
            && image->comps[1].dx == image->comps[2].dx
            && image->comps[0].dy == image->comps[1].dy
            && image->comps[1].dy == image->comps[2].dy
            && image->comps[0].prec == image->comps[1].prec
            && image->comps[1].prec == image->comps[2].prec) {

        /* -->> -->> -->> -->>
        24 bits color
        <<-- <<-- <<-- <<-- */

        fdest = fopen(outfile, "wb");
        if (!fdest) {
            fprintf(stderr, "ERROR -> failed to open %s for writing\n", outfile);
            return 1;
        }

        w = image->comps[0].w;
        h = image->comps[0].h;

        fprintf(fdest, "BM");

        /* FILE HEADER */
        /* ------------- */
        fprintf(fdest, "%c%c%c%c",
                (unsigned char)(h * w * 3 + 3 * h * (w % 2) + 54) & 0xff,
                (unsigned char)((h * w * 3 + 3 * h * (w % 2) + 54) >> 8) & 0xff,
                (unsigned char)((h * w * 3 + 3 * h * (w % 2) + 54) >> 16) & 0xff,
                (unsigned char)((h * w * 3 + 3 * h * (w % 2) + 54) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (0) & 0xff, ((0) >> 8) & 0xff, ((0) >> 16) & 0xff,
                ((0) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (54) & 0xff, ((54) >> 8) & 0xff, ((54) >> 16) & 0xff,
                ((54) >> 24) & 0xff);

        /* INFO HEADER   */
        /* ------------- */
        fprintf(fdest, "%c%c%c%c", (40) & 0xff, ((40) >> 8) & 0xff, ((40) >> 16) & 0xff,
                ((40) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (unsigned char)((w) & 0xff),
                (unsigned char)((w) >> 8) & 0xff,
                (unsigned char)((w) >> 16) & 0xff,
                (unsigned char)((w) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (unsigned char)((h) & 0xff),
                (unsigned char)((h) >> 8) & 0xff,
                (unsigned char)((h) >> 16) & 0xff,
                (unsigned char)((h) >> 24) & 0xff);
        fprintf(fdest, "%c%c", (1) & 0xff, ((1) >> 8) & 0xff);
        fprintf(fdest, "%c%c", (24) & 0xff, ((24) >> 8) & 0xff);
        fprintf(fdest, "%c%c%c%c", (0) & 0xff, ((0) >> 8) & 0xff, ((0) >> 16) & 0xff,
                ((0) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (unsigned char)(3 * h * w + 3 * h * (w % 2)) & 0xff,
                (unsigned char)((h * w * 3 + 3 * h * (w % 2)) >> 8) & 0xff,
                (unsigned char)((h * w * 3 + 3 * h * (w % 2)) >> 16) & 0xff,
                (unsigned char)((h * w * 3 + 3 * h * (w % 2)) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (7834) & 0xff, ((7834) >> 8) & 0xff,
                ((7834) >> 16) & 0xff, ((7834) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (7834) & 0xff, ((7834) >> 8) & 0xff,
                ((7834) >> 16) & 0xff, ((7834) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (0) & 0xff, ((0) >> 8) & 0xff, ((0) >> 16) & 0xff,
                ((0) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (0) & 0xff, ((0) >> 8) & 0xff, ((0) >> 16) & 0xff,
                ((0) >> 24) & 0xff);

        if (image->comps[0].prec > 8) {
            adjustR = image->comps[0].prec - 8;
            printf("BMP CONVERSION: Truncating component 0 from %d bits to 8 bits\n",
                   image->comps[0].prec);
        } else {
            adjustR = 0;
        }
        if (image->comps[1].prec > 8) {
            adjustG = image->comps[1].prec - 8;
            printf("BMP CONVERSION: Truncating component 1 from %d bits to 8 bits\n",
                   image->comps[1].prec);
        } else {
            adjustG = 0;
        }
        if (image->comps[2].prec > 8) {
            adjustB = image->comps[2].prec - 8;
            printf("BMP CONVERSION: Truncating component 2 from %d bits to 8 bits\n",
                   image->comps[2].prec);
        } else {
            adjustB = 0;
        }

        for (i = 0; i < w * h; i++) {
            unsigned char rc, gc, bc;
            int r, g, b;

            r = image->comps[0].data[w * h - ((i) / (w) + 1) * w + (i) % (w)];
            r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
            r = ((r >> adjustR) + ((r >> (adjustR - 1)) % 2));
            if (r > 255) {
                r = 255;
            } else if (r < 0) {
                r = 0;
            }
            rc = (unsigned char)r;

            g = image->comps[1].data[w * h - ((i) / (w) + 1) * w + (i) % (w)];
            g += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
            g = ((g >> adjustG) + ((g >> (adjustG - 1)) % 2));
            if (g > 255) {
                g = 255;
            } else if (g < 0) {
                g = 0;
            }
            gc = (unsigned char)g;

            b = image->comps[2].data[w * h - ((i) / (w) + 1) * w + (i) % (w)];
            b += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
            b = ((b >> adjustB) + ((b >> (adjustB - 1)) % 2));
            if (b > 255) {
                b = 255;
            } else if (b < 0) {
                b = 0;
            }
            bc = (unsigned char)b;

            fprintf(fdest, "%c%c%c", bc, gc, rc);

            if ((i + 1) % w == 0) {
                for (pad = (3 * w) % 4 ? 4 - (3 * w) % 4 : 0; pad > 0; pad--) { /* ADD */
                    fprintf(fdest, "%c", 0);
                }
            }
        }
        fclose(fdest);
    } else {            /* Gray-scale */

        /* -->> -->> -->> -->>
        8 bits non code (Gray scale)
        <<-- <<-- <<-- <<-- */

        fdest = fopen(outfile, "wb");
        w = image->comps[0].w;
        h = image->comps[0].h;

        fprintf(fdest, "BM");

        /* FILE HEADER */
        /* ------------- */
        fprintf(fdest, "%c%c%c%c",
                (unsigned char)(h * w + 54 + 1024 + h * (w % 2)) & 0xff,
                (unsigned char)((h * w + 54 + 1024 + h * (w % 2)) >> 8) & 0xff,
                (unsigned char)((h * w + 54 + 1024 + h * (w % 2)) >> 16) & 0xff,
                (unsigned char)((h * w + 54 + 1024 + w * (w % 2)) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (0) & 0xff, ((0) >> 8) & 0xff, ((0) >> 16) & 0xff,
                ((0) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (54 + 1024) & 0xff, ((54 + 1024) >> 8) & 0xff,
                ((54 + 1024) >> 16) & 0xff,
                ((54 + 1024) >> 24) & 0xff);

        /* INFO HEADER */
        /* ------------- */
        fprintf(fdest, "%c%c%c%c", (40) & 0xff, ((40) >> 8) & 0xff, ((40) >> 16) & 0xff,
                ((40) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (unsigned char)((w) & 0xff),
                (unsigned char)((w) >> 8) & 0xff,
                (unsigned char)((w) >> 16) & 0xff,
                (unsigned char)((w) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (unsigned char)((h) & 0xff),
                (unsigned char)((h) >> 8) & 0xff,
                (unsigned char)((h) >> 16) & 0xff,
                (unsigned char)((h) >> 24) & 0xff);
        fprintf(fdest, "%c%c", (1) & 0xff, ((1) >> 8) & 0xff);
        fprintf(fdest, "%c%c", (8) & 0xff, ((8) >> 8) & 0xff);
        fprintf(fdest, "%c%c%c%c", (0) & 0xff, ((0) >> 8) & 0xff, ((0) >> 16) & 0xff,
                ((0) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (unsigned char)(h * w + h * (w % 2)) & 0xff,
                (unsigned char)((h * w + h * (w % 2)) >> 8) &  0xff,
                (unsigned char)((h * w + h * (w % 2)) >> 16) & 0xff,
                (unsigned char)((h * w + h * (w % 2)) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (7834) & 0xff, ((7834) >> 8) & 0xff,
                ((7834) >> 16) & 0xff, ((7834) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (7834) & 0xff, ((7834) >> 8) & 0xff,
                ((7834) >> 16) & 0xff, ((7834) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (256) & 0xff, ((256) >> 8) & 0xff,
                ((256) >> 16) & 0xff, ((256) >> 24) & 0xff);
        fprintf(fdest, "%c%c%c%c", (256) & 0xff, ((256) >> 8) & 0xff,
                ((256) >> 16) & 0xff, ((256) >> 24) & 0xff);

        if (image->comps[0].prec > 8) {
            adjustR = image->comps[0].prec - 8;
            printf("BMP CONVERSION: Truncating component 0 from %d bits to 8 bits\n",
                   image->comps[0].prec);
        } else {
            adjustR = 0;
        }

        for (i = 0; i < 256; i++) {
            fprintf(fdest, "%c%c%c%c", i, i, i, 0);
        }

        for (i = 0; i < w * h; i++) {
            int r;

            r = image->comps[0].data[w * h - ((i) / (w) + 1) * w + (i) % (w)];
            r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
            r = ((r >> adjustR) + ((r >> (adjustR - 1)) % 2));
            if (r > 255) {
                r = 255;
            } else if (r < 0) {
                r = 0;
            }

            fprintf(fdest, "%c", (unsigned char)r);

            if ((i + 1) % w == 0) {
                for (pad = w % 4 ? 4 - w % 4 : 0; pad > 0; pad--) { /* ADD */
                    fprintf(fdest, "%c", 0);
                }
            }
        }
        fclose(fdest);
    }

    return 0;
}

/* -->> -->> -->> -->>

PGX IMAGE FORMAT

<<-- <<-- <<-- <<-- */


static unsigned char readuchar(FILE * f)
{
    unsigned char c1;
    if (!fread(&c1, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    return c1;
}

static unsigned short readushort(FILE * f, int bigendian)
{
    unsigned char c1, c2;
    if (!fread(&c1, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (!fread(&c2, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (bigendian) {
        return (c1 << 8) + c2;
    } else {
        return (c2 << 8) + c1;
    }
}

static unsigned int readuint(FILE * f, int bigendian)
{
    unsigned char c1, c2, c3, c4;
    if (!fread(&c1, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (!fread(&c2, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (!fread(&c3, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (!fread(&c4, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (bigendian) {
        return (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;
    } else {
        return (c4 << 24) + (c3 << 16) + (c2 << 8) + c1;
    }
}

opj_image_t* pgxtoimage(const char *filename, opj_cparameters_t *parameters)
{
    FILE *f = NULL;
    int w, h, prec;
    int i, numcomps, max;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t cmptparm;  /* maximum of 1 component  */
    opj_image_t * image = NULL;
    int adjustS, ushift, dshift, force8;

    char endian1, endian2, sign;
    char signtmp[32];

    char temp[32];
    int bigendian;
    opj_image_comp_t *comp = NULL;

    numcomps = 1;
    color_space = CLRSPC_GRAY;

    memset(&cmptparm, 0, sizeof(opj_image_cmptparm_t));

    max = 0;

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for reading !\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_SET);
    if (fscanf(f, "PG%[ \t]%c%c%[ \t+-]%d%[ \t]%d%[ \t]%d", temp, &endian1,
               &endian2, signtmp, &prec, temp, &w, temp, &h) != 9) {
        fprintf(stderr,
                "ERROR: Failed to read the right number of element from the fscanf() function!\n");
        fclose(f);
        return NULL;
    }

    i = 0;
    sign = '+';
    while (signtmp[i] != '\0') {
        if (signtmp[i] == '-') {
            sign = '-';
        }
        i++;
    }

    fgetc(f);
    if (endian1 == 'M' && endian2 == 'L') {
        bigendian = 1;
    } else if (endian2 == 'M' && endian1 == 'L') {
        bigendian = 0;
    } else {
        fprintf(stderr, "Bad pgx header, please check input file\n");
        fclose(f);
        return NULL;
    }

    /* initialize image component */

    cmptparm.x0 = parameters->image_offset_x0;
    cmptparm.y0 = parameters->image_offset_y0;
    cmptparm.w = !cmptparm.x0 ? (w - 1) * parameters->subsampling_dx + 1 :
                 cmptparm.x0 + (w - 1) * parameters->subsampling_dx + 1;
    cmptparm.h = !cmptparm.y0 ? (h - 1) * parameters->subsampling_dy + 1 :
                 cmptparm.y0 + (h - 1) * parameters->subsampling_dy + 1;

    if (sign == '-') {
        cmptparm.sgnd = 1;
    } else {
        cmptparm.sgnd = 0;
    }
    if (prec < 8) {
        force8 = 1;
        ushift = 8 - prec;
        dshift = prec - ushift;
        if (cmptparm.sgnd) {
            adjustS = (1 << (prec - 1));
        } else {
            adjustS = 0;
        }
        cmptparm.sgnd = 0;
        prec = 8;
    } else {
        ushift = dshift = force8 = adjustS = 0;
    }

    cmptparm.prec = prec;
    cmptparm.bpp = prec;
    cmptparm.dx = parameters->subsampling_dx;
    cmptparm.dy = parameters->subsampling_dy;

    /* create the image */
    image = opj_image_create(numcomps, &cmptparm, color_space);
    if (!image) {
        fclose(f);
        return NULL;
    }
    /* set image offset and reference grid */
    image->x0 = cmptparm.x0;
    image->y0 = cmptparm.x0;
    image->x1 = cmptparm.w;
    image->y1 = cmptparm.h;

    /* set image data */

    comp = &image->comps[0];

    for (i = 0; i < w * h; i++) {
        int v;
        if (force8) {
            v = readuchar(f) + adjustS;
            v = (v << ushift) + (v >> dshift);
            comp->data[i] = (unsigned char)v;

            if (v > max) {
                max = v;
            }

            continue;
        }
        if (comp->prec == 8) {
            if (!comp->sgnd) {
                v = readuchar(f);
            } else {
                v = (char) readuchar(f);
            }
        } else if (comp->prec <= 16) {
            if (!comp->sgnd) {
                v = readushort(f, bigendian);
            } else {
                v = (short) readushort(f, bigendian);
            }
        } else {
            if (!comp->sgnd) {
                v = readuint(f, bigendian);
            } else {
                v = (int) readuint(f, bigendian);
            }
        }
        if (v > max) {
            max = v;
        }
        comp->data[i] = v;
    }
    fclose(f);
    comp->bpp = int_floorlog2(max) + 1;

    return image;
}

int imagetopgx(opj_image_t * image, const char *outfile)
{
    int w, h;
    int i, j, compno;
    FILE *fdest = NULL;

    for (compno = 0; compno < image->numcomps; compno++) {
        opj_image_comp_t *comp = &image->comps[compno];
        char bname[256]; /* buffer for name */
        char *name = bname; /* pointer */
        int nbytes = 0;
        size_t res;
        const size_t olen = strlen(outfile);
        const size_t dotpos = olen - 4;
        const size_t total = dotpos + 1 + 1 + 4; /* '-' + '[1-3]' + '.pgx' */
        if (outfile[dotpos] != '.') {
            /* `pgx` was recognized but there is no dot at expected position */
            fprintf(stderr, "ERROR -> Impossible happen.");
            return 1;
        }
        if (total > 256) {
            name = (char*)malloc(total + 1);
        }
        strncpy(name, outfile, dotpos);
        /*if (image->numcomps > 1) {*/
        sprintf(name + dotpos, "_%d.pgx", compno);
        /*} else {
            strcpy(name+dotpos, ".pgx");
        }*/
        fdest = fopen(name, "wb");
        if (!fdest) {
            fprintf(stderr, "ERROR -> failed to open %s for writing\n", name);
            return 1;
        }
        /* don't need name anymore */
        if (total > 256) {
            free(name);
        }

        w = image->comps[compno].w;
        h = image->comps[compno].h;

        fprintf(fdest, "PG ML %c %d %d %d\n", comp->sgnd ? '-' : '+', comp->prec, w, h);
        if (comp->prec <= 8) {
            nbytes = 1;
        } else if (comp->prec <= 16) {
            nbytes = 2;
        } else {
            nbytes = 4;
        }
        for (i = 0; i < w * h; i++) {
            int v = image->comps[compno].data[i];
            for (j = nbytes - 1; j >= 0; j--) {
                char byte = (char)(v >> (j * 8));
                res = fwrite(&byte, 1, 1, fdest);
                if (res < 1) {
                    fprintf(stderr, "failed to write 1 byte for %s\n", name);
                    fclose(fdest);
                    return 1;
                }
            }
        }
        fclose(fdest);
    }

    return 0;
}

/* -->> -->> -->> -->>

PNM IMAGE FORMAT

<<-- <<-- <<-- <<-- */

struct pnm_header {
    int width, height, maxval, depth, format;
    char rgb, rgba, gray, graya, bw;
    char ok;
};

static char *skip_white(char *s)
{
    while (*s) {
        if (*s == '\n' || *s == '\r') {
            return NULL;
        }
        if (isspace(*s)) {
            ++s;
            continue;
        }
        return s;
    }
    return NULL;
}

static char *skip_int(char *start, int *out_n)
{
    char *s;
    char c;

    *out_n = 0;
    s = start;

    s = skip_white(start);
    if (s == NULL) {
        return NULL;
    }
    start = s;

    while (*s) {
        if (!isdigit(*s)) {
            break;
        }
        ++s;
    }
    c = *s;
    *s = 0;
    *out_n = atoi(start);
    *s = c;
    return s;
}

static char *skip_idf(char *start, char out_idf[256])
{
    char *s;
    char c;

    s = skip_white(start);
    if (s == NULL) {
        return NULL;
    }
    start = s;

    while (*s) {
        if (isalpha(*s) || *s == '_') {
            ++s;
            continue;
        }
        break;
    }
    c = *s;
    *s = 0;
    strncpy(out_idf, start, 255);
    *s = c;
    return s;
}

static void read_pnm_header(FILE *reader, struct pnm_header *ph)
{
    char *s;
    int format, have_wh, end, ttype;
    char idf[256], type[256];
    char line[256];

    if (fgets(line, 250, reader) == NULL) {
        fprintf(stderr, "\nWARNING: fgets return a NULL value");
        return;
    }

    if (line[0] != 'P') {
        fprintf(stderr, "read_pnm_header:PNM:magic P missing\n");
        return;
    }
    format = atoi(line + 1);
    if (format < 1 || format > 7) {
        fprintf(stderr, "read_pnm_header:magic format %d invalid\n", format);
        return;
    }
    ph->format = format;
    ttype = end = have_wh = 0;

    while (fgets(line, 250, reader)) {
        if (*line == '#') {
            continue;
        }

        s = line;

        if (format == 7) {
            s = skip_idf(s, idf);

            if (s == NULL || *s == 0) {
                return;
            }

            if (strcmp(idf, "ENDHDR") == 0) {
                end = 1;
                break;
            }
            if (strcmp(idf, "WIDTH") == 0) {
                s = skip_int(s, &ph->width);
                if (s == NULL || *s == 0) {
                    return;
                }

                continue;
            }
            if (strcmp(idf, "HEIGHT") == 0) {
                s = skip_int(s, &ph->height);
                if (s == NULL || *s == 0) {
                    return;
                }

                continue;
            }
            if (strcmp(idf, "DEPTH") == 0) {
                s = skip_int(s, &ph->depth);
                if (s == NULL || *s == 0) {
                    return;
                }

                continue;
            }
            if (strcmp(idf, "MAXVAL") == 0) {
                s = skip_int(s, &ph->maxval);
                if (s == NULL || *s == 0) {
                    return;
                }

                continue;
            }
            if (strcmp(idf, "TUPLTYPE") == 0) {
                s = skip_idf(s, type);
                if (s == NULL || *s == 0) {
                    return;
                }

                if (strcmp(type, "BLACKANDWHITE") == 0) {
                    ph->bw = 1;
                    ttype = 1;
                    continue;
                }
                if (strcmp(type, "GRAYSCALE") == 0) {
                    ph->gray = 1;
                    ttype = 1;
                    continue;
                }
                if (strcmp(type, "GRAYSCALE_ALPHA") == 0) {
                    ph->graya = 1;
                    ttype = 1;
                    continue;
                }
                if (strcmp(type, "RGB") == 0) {
                    ph->rgb = 1;
                    ttype = 1;
                    continue;
                }
                if (strcmp(type, "RGB_ALPHA") == 0) {
                    ph->rgba = 1;
                    ttype = 1;
                    continue;
                }
                fprintf(stderr, "read_pnm_header:unknown P7 TUPLTYPE %s\n", type);
                return;
            }
            fprintf(stderr, "read_pnm_header:unknown P7 idf %s\n", idf);
            return;
        } /* if(format == 7) */

        if (!have_wh) {
            s = skip_int(s, &ph->width);

            s = skip_int(s, &ph->height);

            have_wh = 1;

            if (format == 1 || format == 4) {
                break;
            }

            continue;
        }
        if (format == 2 || format == 3 || format == 5 || format == 6) {
            /* P2, P3, P5, P6: */
            s = skip_int(s, &ph->maxval);

            if (ph->maxval > 65535) {
                return;
            }
        }
        break;
    }/* while(fgets( ) */
    if (format == 2 || format == 3 || format > 4) {
        if (ph->maxval < 1 || ph->maxval > 65535) {
            return;
        }
    }
    if (ph->width < 1 || ph->height < 1) {
        return;
    }

    if (format == 7) {
        if (!end) {
            fprintf(stderr, "read_pnm_header:P7 without ENDHDR\n");
            return;
        }
        if (ph->depth < 1 || ph->depth > 4) {
            return;
        }

        if (ph->width && ph->height && ph->depth & ph->maxval && ttype) {
            ph->ok = 1;
        }
    } else {
        if (format != 1 && format != 4) {
            if (ph->width && ph->height && ph->maxval) {
                ph->ok = 1;
            }
        } else {
            if (ph->width && ph->height) {
                ph->ok = 1;
            }
            ph->maxval = 255;
        }
    }
}

static int has_prec(int val)
{
    if (val < 2) {
        return 1;
    }
    if (val < 4) {
        return 2;
    }
    if (val < 8) {
        return 3;
    }
    if (val < 16) {
        return 4;
    }
    if (val < 32) {
        return 5;
    }
    if (val < 64) {
        return 6;
    }
    if (val < 128) {
        return 7;
    }
    if (val < 256) {
        return 8;
    }
    if (val < 512) {
        return 9;
    }
    if (val < 1024) {
        return 10;
    }
    if (val < 2048) {
        return 11;
    }
    if (val < 4096) {
        return 12;
    }
    if (val < 8192) {
        return 13;
    }
    if (val < 16384) {
        return 14;
    }
    if (val < 32768) {
        return 15;
    }
    return 16;
}

opj_image_t* pnmtoimage(const char *filename, opj_cparameters_t *parameters)
{
    int subsampling_dx = parameters->subsampling_dx;
    int subsampling_dy = parameters->subsampling_dy;

    FILE *fp = NULL;
    int i, compno, numcomps, w, h, prec, format;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t cmptparm[4]; /* RGBA: max. 4 components */
    opj_image_t * image = NULL;
    struct pnm_header header_info;

    if ((fp = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "pnmtoimage:Failed to open %s for reading!\n", filename);
        return NULL;
    }
    memset(&header_info, 0, sizeof(struct pnm_header));

    read_pnm_header(fp, &header_info);

    if (!header_info.ok) {
        fclose(fp);
        return NULL;
    }

    format = header_info.format;

    switch (format) {
    case 1: /* ascii bitmap */
    case 4: /* raw bitmap */
        numcomps = 1;
        break;

    case 2: /* ascii greymap */
    case 5: /* raw greymap */
        numcomps = 1;
        break;

    case 3: /* ascii pixmap */
    case 6: /* raw pixmap */
        numcomps = 3;
        break;

    case 7: /* arbitrary map */
        numcomps = header_info.depth;
        break;

    default:
        fclose(fp);
        return NULL;
    }
    if (numcomps < 3) {
        color_space = CLRSPC_GRAY;    /* GRAY, GRAYA */
    } else {
        color_space = CLRSPC_SRGB;    /* RGB, RGBA */
    }

    prec = has_prec(header_info.maxval);

    if (prec < 8) {
        prec = 8;
    }

    w = header_info.width;
    h = header_info.height;
    subsampling_dx = parameters->subsampling_dx;
    subsampling_dy = parameters->subsampling_dy;

    memset(&cmptparm[0], 0, numcomps * sizeof(opj_image_cmptparm_t));

    for (i = 0; i < numcomps; i++) {
        cmptparm[i].prec = prec;
        cmptparm[i].bpp = prec;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = subsampling_dx;
        cmptparm[i].dy = subsampling_dy;
        cmptparm[i].w = w;
        cmptparm[i].h = h;
    }
    image = opj_image_create(numcomps, &cmptparm[0], color_space);

    if (!image) {
        fclose(fp);
        return NULL;
    }

    /* set image offset and reference grid */
    image->x0 = parameters->image_offset_x0;
    image->y0 = parameters->image_offset_y0;
    image->x1 = parameters->image_offset_x0 + (w - 1) * subsampling_dx + 1;
    image->y1 = parameters->image_offset_y0 + (h - 1) * subsampling_dy + 1;

    if ((format == 2) || (format == 3)) { /* ascii pixmap */
        unsigned int index;

        for (i = 0; i < w * h; i++) {
            for (compno = 0; compno < numcomps; compno++) {
                index = 0;
                if (fscanf(fp, "%u", &index) != 1) {
                    fprintf(stderr,
                            "\nWARNING: fscanf return a number of element different from the expected.\n");
                }

                image->comps[compno].data[i] = (index * 255) / header_info.maxval;
            }
        }
    } else if ((format == 5)
               || (format == 6)
               || ((format == 7)
                   && (header_info.gray || header_info.graya
                       || header_info.rgb || header_info.rgba))) { /* binary pixmap */
        unsigned char c0, c1, one;

        one = (prec < 9);

        for (i = 0; i < w * h; i++) {
            for (compno = 0; compno < numcomps; compno++) {
                if (!fread(&c0, 1, 1, fp)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                }
                if (one) {
                    image->comps[compno].data[i] = c0;
                } else {
                    if (!fread(&c1, 1, 1, fp)) {
                        fprintf(stderr,
                                "\nError: fread return a number of element different from the expected.\n");
                    }
                    /* netpbm: */
                    image->comps[compno].data[i] = ((c0 << 8) | c1);
                }
            }
        }
    } else if (format == 1) { /* ascii bitmap */
        for (i = 0; i < w * h; i++) {
            unsigned int index;

            if (fscanf(fp, "%u", &index) != 1) {
                fprintf(stderr,
                        "\nWARNING: fscanf return a number of element different from the expected.\n");
            }

            image->comps[0].data[i] = (index ? 0 : 255);
        }
    } else if (format == 4) {
        int x, y, bit;
        unsigned char uc;

        i = 0;
        for (y = 0; y < h; ++y) {
            bit = -1;
            uc = 0;

            for (x = 0; x < w; ++x) {
                if (bit == -1) {
                    bit = 7;
                    uc = (unsigned char)getc(fp);
                }
                image->comps[0].data[i] = (((uc >> bit) & 1) ? 0 : 255);
                --bit;
                ++i;
            }
        }
    } else if ((format == 7 && header_info.bw)) { /*MONO*/
        unsigned char uc;

        for (i = 0; i < w * h; ++i) {
            if (!fread(&uc, 1, 1, fp)) {
                fprintf(stderr,
                        "\nError: fread return a number of element different from the expected.\n");
            }
            image->comps[0].data[i] = (uc & 1) ? 0 : 255;
        }
    }
    fclose(fp);

    return image;
}/* pnmtoimage() */

int imagetopnm(opj_image_t * image, const char *outfile)
{
    int *red, *green, *blue, *alpha;
    int wr, hr, max;
    int i, compno, ncomp;
    int adjustR, adjustG, adjustB, adjustA;
    int fails, two, want_gray, has_alpha, triple;
    int prec, v;
    FILE *fdest = NULL;
    const char *tmp = outfile;
    char *destname;
    alpha = NULL;
    if ((prec = image->comps[0].prec) > 16) {
        fprintf(stderr, "%s:%d:imagetopnm\n\tprecision %d is larger than 16"
                "\n\t: refused.\n", __FILE__, __LINE__, prec);
        return 1;
    }
    two = has_alpha = 0;
    fails = 1;
    ncomp = image->numcomps;

    while (*tmp) {
        ++tmp;
    }
    tmp -= 2;
    want_gray = (*tmp == 'g' || *tmp == 'G');
    ncomp = image->numcomps;

    if (want_gray) {
        ncomp = 1;
    }

    if (ncomp == 2 /* GRAYA */
            || (ncomp > 2 /* RGB, RGBA */
                && image->comps[0].dx == image->comps[1].dx
                && image->comps[1].dx == image->comps[2].dx
                && image->comps[0].dy == image->comps[1].dy
                && image->comps[1].dy == image->comps[2].dy
                && image->comps[0].prec == image->comps[1].prec
                && image->comps[1].prec == image->comps[2].prec
               )) {
        fdest = fopen(outfile, "wb");

        if (!fdest) {
            fprintf(stderr, "ERROR -> failed to open %s for writing\n", outfile);
            return fails;
        }
        two = (prec > 8);
        triple = (ncomp > 2);
        wr = image->comps[0].w;
        hr = image->comps[0].h;
        max = (1 << prec) - 1;
        has_alpha = (ncomp == 4 || ncomp == 2);

        red = image->comps[0].data;

        if (triple) {
            green = image->comps[1].data;
            blue = image->comps[2].data;
        } else {
            green = blue = NULL;
        }

        if (has_alpha) {
            const char *tt = (triple ? "RGB_ALPHA" : "GRAYSCALE_ALPHA");

            fprintf(fdest, "P7\n# OpenJPEG-%s\nWIDTH %d\nHEIGHT %d\nDEPTH %d\n"
                    "MAXVAL %d\nTUPLTYPE %s\nENDHDR\n", opj_version(),
                    wr, hr, ncomp, max, tt);
            alpha = image->comps[ncomp - 1].data;
            adjustA = (image->comps[ncomp - 1].sgnd ?
                       1 << (image->comps[ncomp - 1].prec - 1) : 0);
        } else {
            fprintf(fdest, "P6\n# OpenJPEG-%s\n%d %d\n%d\n",
                    opj_version(), wr, hr, max);
            adjustA = 0;
        }
        adjustR = (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

        if (triple) {
            adjustG = (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
            adjustB = (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
        } else {
            adjustG = adjustB = 0;
        }

        for (i = 0; i < wr * hr; ++i) {
            if (two) {
                v = *red + adjustR;
                ++red;
                /* netpbm: */
                fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);

                if (triple) {
                    v = *green + adjustG;
                    ++green;
                    /* netpbm: */
                    fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);

                    v =  *blue + adjustB;
                    ++blue;
                    /* netpbm: */
                    fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);

                }/* if(triple) */

                if (has_alpha) {
                    v = *alpha + adjustA;
                    ++alpha;
                    /* netpbm: */
                    fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);
                }
                continue;

            }  /* if(two) */

            /* prec <= 8: */

            fprintf(fdest, "%c", (unsigned char)*red++);
            if (triple) {
                fprintf(fdest, "%c%c", (unsigned char)*green++, (unsigned char)*blue++);
            }

            if (has_alpha) {
                fprintf(fdest, "%c", (unsigned char)*alpha++);
            }

        } /* for(i */

        fclose(fdest);
        return 0;
    }

    /* YUV or MONO: */

    if (image->numcomps > ncomp) {
        fprintf(stderr, "WARNING -> [PGM file] Only the first component\n");
        fprintf(stderr, "           is written to the file\n");
    }
    destname = (char*)malloc(strlen(outfile) + 8);

    for (compno = 0; compno < ncomp; compno++) {
        if (ncomp > 1) {
            sprintf(destname, "%d.%s", compno, outfile);
        } else {
            sprintf(destname, "%s", outfile);
        }

        fdest = fopen(destname, "wb");
        if (!fdest) {
            fprintf(stderr, "ERROR -> failed to open %s for writing\n", destname);
            free(destname);
            return 1;
        }
        wr = image->comps[compno].w;
        hr = image->comps[compno].h;
        prec = image->comps[compno].prec;
        max = (1 << prec) - 1;

        fprintf(fdest, "P5\n#OpenJPEG-%s\n%d %d\n%d\n",
                opj_version(), wr, hr, max);

        red = image->comps[compno].data;
        adjustR =
            (image->comps[compno].sgnd ? 1 << (image->comps[compno].prec - 1) : 0);

        if (prec > 8) {
            for (i = 0; i < wr * hr; i++) {
                v = *red + adjustR;
                ++red;
                /* netpbm: */
                fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);

                if (has_alpha) {
                    v = *alpha++;
                    /* netpbm: */
                    fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);
                }
            }/* for(i */
        } else { /* prec <= 8 */
            for (i = 0; i < wr * hr; ++i) {
                fprintf(fdest, "%c", (unsigned char)(*red + adjustR));
                ++red;
            }
        }
        fclose(fdest);
    } /* for (compno */
    free(destname);

    return 0;
}/* imagetopnm() */

#ifdef OPJ_HAVE_LIBTIFF
/* -->> -->> -->> -->>

    TIFF IMAGE FORMAT

 <<-- <<-- <<-- <<-- */

int imagetotif(opj_image_t * image, const char *outfile)
{
    int width, height, imgsize;
    int bps, index, adjust, sgnd;
    int ushift, dshift, has_alpha, force16;
    TIFF *tif;
    tdata_t buf;
    tstrip_t strip;
    tsize_t strip_size;

    ushift = dshift = force16 = has_alpha = 0;
    bps = image->comps[0].prec;

    if (bps > 8 && bps < 16) {
        ushift = 16 - bps;
        dshift = bps - ushift;
        bps = 16;
        force16 = 1;
    }

    if (bps != 8 && bps != 16) {
        fprintf(stderr, "imagetotif: Bits=%d, Only 8 and 16 bits implemented\n",
                bps);
        fprintf(stderr, "\tAborting\n");
        return 1;
    }
    tif = TIFFOpen(outfile, "wb");

    if (!tif) {
        fprintf(stderr, "imagetotif:failed to open %s for writing\n", outfile);
        return 1;
    }
    sgnd = image->comps[0].sgnd;
    adjust = sgnd ? 1 << (image->comps[0].prec - 1) : 0;

    if (image->numcomps >= 3
            && image->comps[0].dx == image->comps[1].dx
            && image->comps[1].dx == image->comps[2].dx
            && image->comps[0].dy == image->comps[1].dy
            && image->comps[1].dy == image->comps[2].dy
            && image->comps[0].prec == image->comps[1].prec
            && image->comps[1].prec == image->comps[2].prec) {
        has_alpha = (image->numcomps == 4);

        width   = image->comps[0].w;
        height  = image->comps[0].h;
        imgsize = width * height ;

        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3 + has_alpha);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bps);
        TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
        strip_size = TIFFStripSize(tif);
        buf = _TIFFmalloc(strip_size);
        index = 0;

        for (strip = 0; strip < TIFFNumberOfStrips(tif); strip++) {
            unsigned char *dat8;
            tsize_t i, ssize, last_i = 0;
            int step, restx;
            ssize = TIFFStripSize(tif);
            dat8 = (unsigned char*)buf;

            if (bps == 8) {
                step = 3 + has_alpha;
                restx = step - 1;

                for (i = 0; i < ssize - restx; i += step) {
                    int r, g, b, a = 0;

                    if (index < imgsize) {
                        r = image->comps[0].data[index];
                        g = image->comps[1].data[index];
                        b = image->comps[2].data[index];
                        if (has_alpha) {
                            a = image->comps[3].data[index];
                        }

                        if (sgnd) {
                            r += adjust;
                            g += adjust;
                            b += adjust;
                            if (has_alpha) {
                                a += adjust;
                            }
                        }
                        dat8[i + 0] = r ;
                        dat8[i + 1] = g ;
                        dat8[i + 2] = b ;
                        if (has_alpha) {
                            dat8[i + 3] = a;
                        }

                        index++;
                        last_i = i + step;
                    } else {
                        break;
                    }
                }/*for(i = 0;)*/

                if (last_i < ssize) {
                    for (i = last_i; i < ssize; i += step) {
                        int r, g, b, a = 0;

                        if (index < imgsize) {
                            r = image->comps[0].data[index];
                            g = image->comps[1].data[index];
                            b = image->comps[2].data[index];
                            if (has_alpha) {
                                a = image->comps[3].data[index];
                            }

                            if (sgnd) {
                                r += adjust;
                                g += adjust;
                                b += adjust;
                                if (has_alpha) {
                                    a += adjust;
                                }
                            }
                            dat8[i + 0] = r ;
                            if (i + 1 < ssize) {
                                dat8[i + 1] = g ;
                            }  else {
                                break;
                            }
                            if (i + 2 < ssize) {
                                dat8[i + 2] = b ;
                            }  else {
                                break;
                            }
                            if (has_alpha) {
                                if (i + 3 < ssize) {
                                    dat8[i + 3] = a ;
                                }  else {
                                    break;
                                }
                            }
                            index++;
                        } else {
                            break;
                        }
                    }/*for(i)*/
                }/*if(last_i < ssize)*/

            }  /*if(bps == 8)*/
            else if (bps == 16) {
                step = 6 + has_alpha + has_alpha;
                restx = step - 1;

                for (i = 0; i < ssize - restx ; i += step) {
                    int r, g, b, a = 0;

                    if (index < imgsize) {
                        r = image->comps[0].data[index];
                        g = image->comps[1].data[index];
                        b = image->comps[2].data[index];
                        if (has_alpha) {
                            a = image->comps[3].data[index];
                        }

                        if (sgnd) {
                            r += adjust;
                            g += adjust;
                            b += adjust;
                            if (has_alpha) {
                                a += adjust;
                            }
                        }
                        if (force16) {
                            r = (r << ushift) + (r >> dshift);
                            g = (g << ushift) + (g >> dshift);
                            b = (b << ushift) + (b >> dshift);
                            if (has_alpha) {
                                a = (a << ushift) + (a >> dshift);
                            }
                        }
                        dat8[i + 0] =  r; /*LSB*/
                        dat8[i + 1] = (r >> 8); /*MSB*/
                        dat8[i + 2] =  g;
                        dat8[i + 3] = (g >> 8);
                        dat8[i + 4] =  b;
                        dat8[i + 5] = (b >> 8);
                        if (has_alpha) {
                            dat8[i + 6] =  a;
                            dat8[i + 7] = (a >> 8);
                        }
                        index++;
                        last_i = i + step;
                    } else {
                        break;
                    }
                }/*for(i = 0;)*/

                if (last_i < ssize) {
                    for (i = last_i ; i < ssize ; i += step) {
                        int r, g, b, a = 0;

                        if (index < imgsize) {
                            r = image->comps[0].data[index];
                            g = image->comps[1].data[index];
                            b = image->comps[2].data[index];
                            if (has_alpha) {
                                a = image->comps[3].data[index];
                            }

                            if (sgnd) {
                                r += adjust;
                                g += adjust;
                                b += adjust;
                                if (has_alpha) {
                                    a += adjust;
                                }
                            }
                            if (force16) {
                                r = (r << ushift) + (r >> dshift);
                                g = (g << ushift) + (g >> dshift);
                                b = (b << ushift) + (b >> dshift);
                                if (has_alpha) {
                                    a = (a << ushift) + (a >> dshift);
                                }
                            }
                            dat8[i + 0] =  r; /*LSB*/
                            if (i + 1 < ssize) {
                                dat8[i + 1] = (r >> 8);
                            } else {
                                break;    /*MSB*/
                            }
                            if (i + 2 < ssize) {
                                dat8[i + 2] =  g;
                            }      else {
                                break;
                            }
                            if (i + 3 < ssize) {
                                dat8[i + 3] = (g >> 8);
                            } else {
                                break;
                            }
                            if (i + 4 < ssize) {
                                dat8[i + 4] =  b;
                            }      else {
                                break;
                            }
                            if (i + 5 < ssize) {
                                dat8[i + 5] = (b >> 8);
                            } else {
                                break;
                            }

                            if (has_alpha) {
                                if (i + 6 < ssize) {
                                    dat8[i + 6] = a;
                                } else {
                                    break;
                                }
                                if (i + 7 < ssize) {
                                    dat8[i + 7] = (a >> 8);
                                } else {
                                    break;
                                }
                            }
                            index++;
                        } else {
                            break;
                        }
                    }/*for(i)*/
                }/*if(last_i < ssize)*/

            }/*if(bps == 16)*/
            (void)TIFFWriteEncodedStrip(tif, strip, (void*)buf, strip_size);
        }/*for(strip = 0; )*/

        _TIFFfree((void*)buf);
        TIFFClose(tif);

        return 0;
    }/*RGB(A)*/

    if (image->numcomps == 1 /* GRAY */
            || (image->numcomps == 2    /* GRAY_ALPHA */
                && image->comps[0].dx == image->comps[1].dx
                && image->comps[0].dy == image->comps[1].dy
                && image->comps[0].prec == image->comps[1].prec)) {
        int step;

        has_alpha = (image->numcomps == 2);

        width   = image->comps[0].w;
        height  = image->comps[0].h;
        imgsize = width * height;

        /* Set tags */
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1 + has_alpha);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bps);
        TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);

        /* Get a buffer for the data */
        strip_size = TIFFStripSize(tif);
        buf = _TIFFmalloc(strip_size);
        index = 0;

        for (strip = 0; strip < TIFFNumberOfStrips(tif); strip++) {
            unsigned char *dat8;
            tsize_t i, ssize = TIFFStripSize(tif);
            dat8 = (unsigned char*)buf;

            if (bps == 8) {
                step = 1 + has_alpha;

                for (i = 0; i < ssize; i += step) {
                    if (index < imgsize) {
                        int r, a = 0;

                        r = image->comps[0].data[index];
                        if (has_alpha) {
                            a = image->comps[1].data[index];
                        }

                        if (sgnd) {
                            r += adjust;
                            if (has_alpha) {
                                a += adjust;
                            }
                        }
                        dat8[i + 0] = r;
                        if (has_alpha) {
                            dat8[i + 1] = a;
                        }
                        index++;
                    } else {
                        break;
                    }
                }/*for(i )*/
            }/*if(bps == 8*/
            else if (bps == 16) {
                step = 2 + has_alpha + has_alpha;

                for (i = 0; i < ssize; i += step) {
                    if (index < imgsize) {
                        int r, a = 0;

                        r = image->comps[0].data[index];
                        if (has_alpha) {
                            a = image->comps[1].data[index];
                        }

                        if (sgnd) {
                            r += adjust;
                            if (has_alpha) {
                                a += adjust;
                            }
                        }
                        if (force16) {
                            r = (r << ushift) + (r >> dshift);
                            if (has_alpha) {
                                a = (a << ushift) + (a >> dshift);
                            }
                        }
                        dat8[i + 0] = r; /*LSB*/
                        dat8[i + 1] = r >> 8; /*MSB*/
                        if (has_alpha) {
                            dat8[i + 2] = a;
                            dat8[i + 3] = a >> 8;
                        }
                        index++;
                    }/*if(index < imgsize)*/
                    else {
                        break;
                    }
                }/*for(i )*/
            }
            (void)TIFFWriteEncodedStrip(tif, strip, (void*)buf, strip_size);
        }/*for(strip*/

        _TIFFfree(buf);
        TIFFClose(tif);

        return 0;
    }

    TIFFClose(tif);

    fprintf(stderr, "imagetotif: Bad color format.\n"
            "\tOnly RGB(A) and GRAY(A) has been implemented\n");
    fprintf(stderr, "\tFOUND: numcomps(%d)\n\tAborting\n",
            image->numcomps);

    return 1;
}/* imagetotif() */

/*
 * libtiff/tif_getimage.c : 1,2,4,8,16 bitspersample accepted
 * CINEMA                 : 12 bit precision
*/
opj_image_t* tiftoimage(const char *filename, opj_cparameters_t *parameters)
{
    int subsampling_dx = parameters->subsampling_dx;
    int subsampling_dy = parameters->subsampling_dy;
    TIFF *tif;
    tdata_t buf;
    tstrip_t strip;
    tsize_t strip_size;
    int j, numcomps, w, h, index;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t cmptparm[4]; /* RGBA */
    opj_image_t *image = NULL;
    int imgsize = 0;
    int has_alpha = 0;
    unsigned short tiBps, tiPhoto, tiSf, tiSpp, tiPC;
    unsigned int tiWidth, tiHeight;

    tif = TIFFOpen(filename, "r");

    if (!tif) {
        fprintf(stderr, "tiftoimage:Failed to open %s for reading\n", filename);
        return 0;
    }
    tiBps = tiPhoto = tiSf = tiSpp = tiPC = 0;
    tiWidth = tiHeight = 0;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &tiWidth);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &tiHeight);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &tiBps);
    TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &tiSf);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &tiSpp);
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &tiPhoto);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &tiPC);
    w = tiWidth;
    h = tiHeight;

    {
        unsigned short b = tiBps, p = tiPhoto;

        if (tiBps != 8 && tiBps != 16 && tiBps != 12) {
            b = 0;
        }
        if (tiPhoto != 1 && tiPhoto != 2) {
            p = 0;
        }

        if (!b || !p) {
            if (!b)
                fprintf(stderr, "imagetotif: Bits=%d, Only 8 and 16 bits"
                        " implemented\n", tiBps);
            else if (!p)
                fprintf(stderr, "tiftoimage: Bad color format %d.\n\tOnly RGB(A)"
                        " and GRAY(A) has been implemented\n", (int) tiPhoto);

            fprintf(stderr, "\tAborting\n");
            TIFFClose(tif);

            return NULL;
        }
    }
    {/* From: tiff-4.0.x/libtiff/tif_getimage.c : */
        uint16* sampleinfo;
        uint16 extrasamples;

        TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,
                              &extrasamples, &sampleinfo);

        if (extrasamples >= 1) {
            switch (sampleinfo[0]) {
            case EXTRASAMPLE_UNSPECIFIED:
                /* Workaround for some images without correct info about alpha channel
                */
                if (tiSpp > 3) {
                    has_alpha = 1;
                }
                break;

            case EXTRASAMPLE_ASSOCALPHA: /* data pre-multiplied */
            case EXTRASAMPLE_UNASSALPHA: /* data not pre-multiplied */
                has_alpha = 1;
                break;
            }
        } else /* extrasamples == 0 */
            if (tiSpp == 4 || tiSpp == 2) {
                has_alpha = 1;
            }
    }

    /* initialize image components
    */
    memset(&cmptparm[0], 0, 4 * sizeof(opj_image_cmptparm_t));

    if (tiPhoto == PHOTOMETRIC_RGB) { /* RGB(A) */
        numcomps = 3 + has_alpha;
        color_space = CLRSPC_SRGB;

        for (j = 0; j < numcomps; j++) {
            if (parameters->cp_cinema) {
                cmptparm[j].prec = 12;
                cmptparm[j].bpp = 12;
            } else {
                cmptparm[j].prec = tiBps;
                cmptparm[j].bpp = tiBps;
            }
            cmptparm[j].dx = subsampling_dx;
            cmptparm[j].dy = subsampling_dy;
            cmptparm[j].w = w;
            cmptparm[j].h = h;
        }

        image = opj_image_create(numcomps, &cmptparm[0], color_space);

        if (!image) {
            TIFFClose(tif);
            return NULL;
        }
        /* set image offset and reference grid
        */
        image->x0 = parameters->image_offset_x0;
        image->y0 = parameters->image_offset_y0;
        image->x1 = !image->x0 ? (w - 1) * subsampling_dx + 1 :
                    image->x0 + (w - 1) * subsampling_dx + 1;
        image->y1 = !image->y0 ? (h - 1) * subsampling_dy + 1 :
                    image->y0 + (h - 1) * subsampling_dy + 1;

        buf = _TIFFmalloc(TIFFStripSize(tif));

        strip_size = TIFFStripSize(tif);
        index = 0;
        imgsize = image->comps[0].w * image->comps[0].h ;
        /* Read the Image components
        */
        for (strip = 0; strip < TIFFNumberOfStrips(tif); strip++) {
            unsigned char *dat8;
            int step;
            tsize_t i, ssize;
            ssize = TIFFReadEncodedStrip(tif, strip, buf, strip_size);
            dat8 = (unsigned char*)buf;

            if (tiBps == 16) {
                step = 6 + has_alpha + has_alpha;

                for (i = 0; i < ssize; i += step) {
                    if (index < imgsize) {
                        image->comps[0].data[index] = (dat8[i + 1] << 8) | dat8[i + 0]; /* R */
                        image->comps[1].data[index] = (dat8[i + 3] << 8) | dat8[i + 2]; /* G */
                        image->comps[2].data[index] = (dat8[i + 5] << 8) | dat8[i + 4]; /* B */
                        if (has_alpha) {
                            image->comps[3].data[index] = (dat8[i + 7] << 8) | dat8[i + 6];
                        }

                        if (parameters->cp_cinema) {
                            /* Rounding 16 to 12 bits
                            */
                            image->comps[0].data[index] =
                                (image->comps[0].data[index] + 0x08) >> 4 ;
                            image->comps[1].data[index] =
                                (image->comps[1].data[index] + 0x08) >> 4 ;
                            image->comps[2].data[index] =
                                (image->comps[2].data[index] + 0x08) >> 4 ;
                            if (has_alpha)
                                image->comps[3].data[index] =
                                    (image->comps[3].data[index] + 0x08) >> 4 ;
                        }
                        index++;
                    } else {
                        break;
                    }
                }/*for(i = 0)*/
            }/*if(tiBps == 16)*/
            else if (tiBps == 8) {
                step = 3 + has_alpha;

                for (i = 0; i < ssize; i += step) {
                    if (index < imgsize) {
                        image->comps[0].data[index] = dat8[i + 0]; /* R */
                        image->comps[1].data[index] = dat8[i + 1]; /* G */
                        image->comps[2].data[index] = dat8[i + 2]; /* B */
                        if (has_alpha) {
                            image->comps[3].data[index] = dat8[i + 3];
                        }

                        if (parameters->cp_cinema) {
                            /* Rounding 8 to 12 bits
                            */
                            image->comps[0].data[index] = image->comps[0].data[index] << 4 ;
                            image->comps[1].data[index] = image->comps[1].data[index] << 4 ;
                            image->comps[2].data[index] = image->comps[2].data[index] << 4 ;
                            if (has_alpha) {
                                image->comps[3].data[index] = image->comps[3].data[index] << 4 ;
                            }
                        }
                        index++;
                    }/*if(index*/
                    else {
                        break;
                    }
                }/*for(i )*/
            }/*if( tiBps == 8)*/
            else if (tiBps == 12) { /* CINEMA file */
                step = 9;

                for (i = 0; i < ssize; i += step) {
                    if ((index < imgsize) & (index + 1 < imgsize)) {
                        image->comps[0].data[index]   = (dat8[i + 0] << 4)        | (dat8[i + 1] >> 4);
                        image->comps[1].data[index]   = ((dat8[i + 1] & 0x0f) << 8) | dat8[i + 2];

                        image->comps[2].data[index]   = (dat8[i + 3] << 4)         | (dat8[i + 4] >> 4);
                        image->comps[0].data[index + 1] = ((dat8[i + 4] & 0x0f) << 8) | dat8[i + 5];

                        image->comps[1].data[index + 1] = (dat8[i + 6] << 4)        |
                                                          (dat8[i + 7] >> 4);
                        image->comps[2].data[index + 1] = ((dat8[i + 7] & 0x0f) << 8) | dat8[i + 8];

                        index += 2;
                    } else {
                        break;
                    }
                }/*for(i )*/
            }
        }/*for(strip = 0; )*/

        _TIFFfree(buf);
        TIFFClose(tif);

        return image;
    }/*RGB(A)*/

    if (tiPhoto == PHOTOMETRIC_MINISBLACK) { /* GRAY(A) */
        numcomps = 1 + has_alpha;
        color_space = CLRSPC_GRAY;

        for (j = 0; j < numcomps; ++j) {
            cmptparm[j].prec = tiBps;
            cmptparm[j].bpp = tiBps;
            cmptparm[j].dx = subsampling_dx;
            cmptparm[j].dy = subsampling_dy;
            cmptparm[j].w = w;
            cmptparm[j].h = h;
        }
        image = opj_image_create(numcomps, &cmptparm[0], color_space);

        if (!image) {
            TIFFClose(tif);
            return NULL;
        }
        /* set image offset and reference grid
        */
        image->x0 = parameters->image_offset_x0;
        image->y0 = parameters->image_offset_y0;
        image->x1 = !image->x0 ? (w - 1) * subsampling_dx + 1 :
                    image->x0 + (w - 1) * subsampling_dx + 1;
        image->y1 = !image->y0 ? (h - 1) * subsampling_dy + 1 :
                    image->y0 + (h - 1) * subsampling_dy + 1;

        buf = _TIFFmalloc(TIFFStripSize(tif));

        strip_size = TIFFStripSize(tif);
        index = 0;
        imgsize = image->comps[0].w * image->comps[0].h ;
        /* Read the Image components
        */
        for (strip = 0; strip < TIFFNumberOfStrips(tif); strip++) {
            unsigned char *dat8;
            tsize_t i, ssize;
            int step;

            ssize = TIFFReadEncodedStrip(tif, strip, buf, strip_size);
            dat8 = (unsigned char*)buf;

            if (tiBps == 16) {
                step = 2 + has_alpha + has_alpha;

                for (i = 0; i < ssize; i += step) {
                    if (index < imgsize) {
                        image->comps[0].data[index] = (dat8[i + 1] << 8) | dat8[i + 0];
                        if (has_alpha) {
                            image->comps[1].data[index] = (dat8[i + 3] << 8) | dat8[i + 2];
                        }
                        index++;
                    } else {
                        break;
                    }
                }/*for(i )*/
            } else if (tiBps == 8) {
                step = 1 + has_alpha;

                for (i = 0; i < ssize; i += step) {
                    if (index < imgsize) {
                        image->comps[0].data[index] = dat8[i + 0];
                        if (has_alpha) {
                            image->comps[1].data[index] = dat8[i + 1];
                        }
                        index++;
                    } else {
                        break;
                    }
                }/*for(i )*/
            }
        }/*for(strip = 0;*/

        _TIFFfree(buf);
        TIFFClose(tif);

    }/*GRAY(A)*/

    return image;

}/* tiftoimage() */

#endif /* OPJ_HAVE_LIBTIFF */

/* -->> -->> -->> -->>

    RAW IMAGE FORMAT

 <<-- <<-- <<-- <<-- */

opj_image_t* rawtoimage(const char *filename, opj_cparameters_t *parameters,
                        raw_cparameters_t *raw_cp)
{
    int subsampling_dx = parameters->subsampling_dx;
    int subsampling_dy = parameters->subsampling_dy;

    FILE *f = NULL;
    int i, compno, numcomps, w, h;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t *cmptparm;
    opj_image_t * image = NULL;
    unsigned short ch;

    if ((!(raw_cp->rawWidth & raw_cp->rawHeight & raw_cp->rawComp &
            raw_cp->rawBitDepth)) == 0) {
        fprintf(stderr, "\nError: invalid raw image parameters\n");
        fprintf(stderr, "Please use the Format option -F:\n");
        fprintf(stderr,
                "-F rawWidth,rawHeight,rawComp,rawBitDepth,s/u (Signed/Unsigned)\n");
        fprintf(stderr, "Example: -i lena.raw -o lena.j2k -F 512,512,3,8,u\n");
        fprintf(stderr, "Aborting\n");
        return NULL;
    }

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for reading !!\n", filename);
        fprintf(stderr, "Aborting\n");
        return NULL;
    }
    numcomps = raw_cp->rawComp;
    color_space = CLRSPC_SRGB;
    w = raw_cp->rawWidth;
    h = raw_cp->rawHeight;
    cmptparm = (opj_image_cmptparm_t*) malloc(numcomps * sizeof(
                   opj_image_cmptparm_t));

    /* initialize image components */
    memset(&cmptparm[0], 0, numcomps * sizeof(opj_image_cmptparm_t));
    for (i = 0; i < numcomps; i++) {
        cmptparm[i].prec = raw_cp->rawBitDepth;
        cmptparm[i].bpp = raw_cp->rawBitDepth;
        cmptparm[i].sgnd = raw_cp->rawSigned;
        cmptparm[i].dx = subsampling_dx;
        cmptparm[i].dy = subsampling_dy;
        cmptparm[i].w = w;
        cmptparm[i].h = h;
    }
    /* create the image */
    image = opj_image_create(numcomps, &cmptparm[0], color_space);
    if (!image) {
        fclose(f);
        return NULL;
    }
    /* set image offset and reference grid */
    image->x0 = parameters->image_offset_x0;
    image->y0 = parameters->image_offset_y0;
    image->x1 = parameters->image_offset_x0 + (w - 1) * subsampling_dx + 1;
    image->y1 = parameters->image_offset_y0 + (h - 1) * subsampling_dy + 1;

    if (raw_cp->rawBitDepth <= 8) {
        unsigned char value = 0;
        for (compno = 0; compno < numcomps; compno++) {
            for (i = 0; i < w * h; i++) {
                if (!fread(&value, 1, 1, f)) {
                    fprintf(stderr, "Error reading raw file. End of file probably reached.\n");
                    fclose(f);
                    return NULL;
                }
                image->comps[compno].data[i] = raw_cp->rawSigned ? (char)value : value;
            }
        }
    } else if (raw_cp->rawBitDepth <= 16) {
        unsigned short value;
        for (compno = 0; compno < numcomps; compno++) {
            for (i = 0; i < w * h; i++) {
                unsigned char temp;
                if (!fread(&temp, 1, 1, f)) {
                    fprintf(stderr, "Error reading raw file. End of file probably reached.\n");
                    fclose(f);
                    return NULL;
                }
                value = temp << 8;
                if (!fread(&temp, 1, 1, f)) {
                    fprintf(stderr, "Error reading raw file. End of file probably reached.\n");
                    fclose(f);
                    return NULL;
                }
                value += temp;
                image->comps[compno].data[i] = raw_cp->rawSigned ? (short)value : value;
            }
        }
    } else {
        fprintf(stderr,
                "OpenJPEG cannot encode raw components with bit depth higher than 16 bits.\n");
        fclose(f);
        return NULL;
    }

    if (fread(&ch, 1, 1, f)) {
        fprintf(stderr, "Warning. End of raw file not reached... processing anyway\n");
    }
    fclose(f);

    return image;
}

int imagetoraw(opj_image_t * image, const char *outfile)
{
    FILE *rawFile = NULL;
    size_t res;
    int compno;
    int w, h;
    int line, row;
    int *ptr;

    if ((image->numcomps * image->x1 * image->y1) == 0) {
        fprintf(stderr, "\nError: invalid raw image parameters\n");
        return 1;
    }

    rawFile = fopen(outfile, "wb");
    if (!rawFile) {
        fprintf(stderr, "Failed to open %s for writing !!\n", outfile);
        return 1;
    }

    fprintf(stdout, "Raw image characteristics: %d components\n", image->numcomps);

    for (compno = 0; compno < image->numcomps; compno++) {
        fprintf(stdout, "Component %d characteristics: %dx%dx%d %s\n", compno,
                image->comps[compno].w,
                image->comps[compno].h, image->comps[compno].prec,
                image->comps[compno].sgnd == 1 ? "signed" : "unsigned");

        w = image->comps[compno].w;
        h = image->comps[compno].h;

        if (image->comps[compno].prec <= 8) {
            if (image->comps[compno].sgnd == 1) {
                signed char curr;
                int mask = (1 << image->comps[compno].prec) - 1;
                ptr = image->comps[compno].data;
                for (line = 0; line < h; line++) {
                    for (row = 0; row < w; row++)    {
                        curr = (signed char)(*ptr & mask);
                        res = fwrite(&curr, sizeof(signed char), 1, rawFile);
                        if (res < 1) {
                            fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                            return 1;
                        }
                        ptr++;
                    }
                }
            } else if (image->comps[compno].sgnd == 0) {
                unsigned char curr;
                int mask = (1 << image->comps[compno].prec) - 1;
                ptr = image->comps[compno].data;
                for (line = 0; line < h; line++) {
                    for (row = 0; row < w; row++)    {
                        curr = (unsigned char)(*ptr & mask);
                        res = fwrite(&curr, sizeof(unsigned char), 1, rawFile);
                        if (res < 1) {
                            fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                            return 1;
                        }
                        ptr++;
                    }
                }
            }
        } else if (image->comps[compno].prec <= 16) {
            if (image->comps[compno].sgnd == 1) {
                signed short int curr;
                int mask = (1 << image->comps[compno].prec) - 1;
                ptr = image->comps[compno].data;
                for (line = 0; line < h; line++) {
                    for (row = 0; row < w; row++)    {
                        unsigned char temp;
                        curr = (signed short int)(*ptr & mask);
                        temp = (unsigned char)(curr >> 8);
                        res = fwrite(&temp, 1, 1, rawFile);
                        if (res < 1) {
                            fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                            return 1;
                        }
                        temp = (unsigned char) curr;
                        res = fwrite(&temp, 1, 1, rawFile);
                        if (res < 1) {
                            fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                            return 1;
                        }
                        ptr++;
                    }
                }
            } else if (image->comps[compno].sgnd == 0) {
                unsigned short int curr;
                int mask = (1 << image->comps[compno].prec) - 1;
                ptr = image->comps[compno].data;
                for (line = 0; line < h; line++) {
                    for (row = 0; row < w; row++)    {
                        unsigned char temp;
                        curr = (unsigned short int)(*ptr & mask);
                        temp = (unsigned char)(curr >> 8);
                        res = fwrite(&temp, 1, 1, rawFile);
                        if (res < 1) {
                            fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                            return 1;
                        }
                        temp = (unsigned char) curr;
                        res = fwrite(&temp, 1, 1, rawFile);
                        if (res < 1) {
                            fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                            return 1;
                        }
                        ptr++;
                    }
                }
            }
        } else if (image->comps[compno].prec <= 32) {
            fprintf(stderr, "More than 16 bits per component no handled yet\n");
            return 1;
        } else {
            fprintf(stderr, "Error: invalid precision: %d\n", image->comps[compno].prec);
            return 1;
        }
    }
    fclose(rawFile);
    return 0;
}

#ifdef OPJ_HAVE_LIBPNG

#define PNG_MAGIC "\x89PNG\x0d\x0a\x1a\x0a"
#define MAGIC_SIZE 8
/* PNG allows bits per sample: 1, 2, 4, 8, 16 */

opj_image_t *pngtoimage(const char *read_idf, opj_cparameters_t * params)
{
    png_structp  png;
    png_infop    info;
    double gamma, display_exponent;
    int bit_depth, interlace_type, compression_type, filter_type;
    int unit;
    png_uint_32 resx, resy;
    unsigned int i, j;
    png_uint_32  width, height;
    int color_type, has_alpha, is16;
    unsigned char *s;
    FILE *reader;
    unsigned char **rows;
    /* j2k: */
    opj_image_t *image;
    opj_image_cmptparm_t cmptparm[4];
    int sub_dx, sub_dy;
    unsigned int nr_comp;
    int *r, *g, *b, *a;
    unsigned char sigbuf[8];

    if ((reader = fopen(read_idf, "rb")) == NULL) {
        fprintf(stderr, "pngtoimage: can not open %s\n", read_idf);
        return NULL;
    }
    image = NULL;
    png = NULL;
    rows = NULL;

    if (fread(sigbuf, 1, MAGIC_SIZE, reader) != MAGIC_SIZE
            || memcmp(sigbuf, PNG_MAGIC, MAGIC_SIZE) != 0) {
        fprintf(stderr, "pngtoimage: %s is no valid PNG file\n", read_idf);
        goto fin;
    }
    /* libpng-VERSION/example.c:
     * PC : screen_gamma = 2.2;
     * Mac: screen_gamma = 1.7 or 1.0;
    */
    display_exponent = 2.2;

    if ((png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                      NULL, NULL, NULL)) == NULL) {
        goto fin;
    }
    if ((info = png_create_info_struct(png)) == NULL) {
        goto fin;
    }

    if (setjmp(png_jmpbuf(png))) {
        goto fin;
    }

    png_init_io(png, reader);
    png_set_sig_bytes(png, MAGIC_SIZE);

    png_read_info(png, info);

    if (png_get_IHDR(png, info, &width, &height,
                     &bit_depth, &color_type, &interlace_type,
                     &compression_type, &filter_type) == 0) {
        goto fin;
    }

    /* png_set_expand():
     * expand paletted images to RGB, expand grayscale images of
     * less than 8-bit depth to 8-bit depth, and expand tRNS chunks
     * to alpha channels.
    */
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_expand(png);
    } else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_expand(png);
    }

    is16 = (bit_depth == 16);

    /* GRAY => RGB; GRAY_ALPHA => RGBA
    */
    if (color_type == PNG_COLOR_TYPE_GRAY
            || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
        color_type =
            (color_type == PNG_COLOR_TYPE_GRAY ? PNG_COLOR_TYPE_RGB :
             PNG_COLOR_TYPE_RGB_ALPHA);
    }
    if (!png_get_gAMA(png, info, &gamma)) {
        gamma = 0.45455;
    }

    png_set_gamma(png, display_exponent, gamma);

    png_read_update_info(png, info);

    png_get_pHYs(png, info, &resx, &resy, &unit);

    color_type = png_get_color_type(png, info);

    has_alpha = (color_type == PNG_COLOR_TYPE_RGB_ALPHA);

    nr_comp = 3 + has_alpha;

    bit_depth = png_get_bit_depth(png, info);

    rows = (unsigned char**)calloc(height + 1, sizeof(unsigned char*));
    for (i = 0; i < height; ++i) {
        rows[i] = (unsigned char*)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, rows);

    memset(&cmptparm, 0, 4 * sizeof(opj_image_cmptparm_t));

    sub_dx = params->subsampling_dx;
    sub_dy = params->subsampling_dy;

    for (i = 0; i < nr_comp; ++i) {
        cmptparm[i].prec = bit_depth;
        /* bits_per_pixel: 8 or 16 */
        cmptparm[i].bpp = bit_depth;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = sub_dx;
        cmptparm[i].dy = sub_dy;
        cmptparm[i].w = width;
        cmptparm[i].h = height;
    }

    image = opj_image_create(nr_comp, &cmptparm[0], CLRSPC_SRGB);

    if (image == NULL) {
        goto fin;
    }

    image->x0 = params->image_offset_x0;
    image->y0 = params->image_offset_y0;
    image->x1 = image->x0 + (width  - 1) * sub_dx + 1 + image->x0;
    image->y1 = image->y0 + (height - 1) * sub_dy + 1 + image->y0;

    r = image->comps[0].data;
    g = image->comps[1].data;
    b = image->comps[2].data;
    a = image->comps[3].data;

    for (i = 0; i < height; ++i) {
        s = rows[i];

        for (j = 0; j < width; ++j) {
            if (is16) {
                *r++ = s[0] << 8 | s[1];
                s += 2;

                *g++ = s[0] << 8 | s[1];
                s += 2;

                *b++ = s[0] << 8 | s[1];
                s += 2;

                if (has_alpha) {
                    *a++ = s[0] << 8 | s[1];
                    s += 2;
                }

                continue;
            }
            *r++ = *s++;
            *g++ = *s++;
            *b++ = *s++;

            if (has_alpha) {
                *a++ = *s++;
            }
        }
    }
fin:
    if (rows) {
        for (i = 0; i < height; ++i) {
            free(rows[i]);
        }
        free(rows);
    }
    if (png) {
        png_destroy_read_struct(&png, &info, NULL);
    }

    fclose(reader);

    return image;

}/* pngtoimage() */

int imagetopng(opj_image_t * image, const char *write_idf)
{
    FILE *writer;
    png_structp png;
    png_infop info;
    int *red, *green, *blue, *alpha;
    unsigned char *row_buf, *d;
    int has_alpha, width, height, nr_comp, color_type;
    int adjustR, adjustG, adjustB, adjustA, x, y, fails;
    int prec, ushift, dshift, is16, force16, force8;
    unsigned short mask = 0xffff;
    png_color_8 sig_bit;

    is16 = force16 = force8 = ushift = dshift = 0;
    fails = 1;
    prec = image->comps[0].prec;
    nr_comp = image->numcomps;

    if (prec > 8 && prec < 16) {
        ushift = 16 - prec;
        dshift = prec - ushift;
        prec = 16;
        force16 = 1;
    } else if (prec < 8 && nr_comp > 1) { /* GRAY_ALPHA, RGB, RGB_ALPHA */
        ushift = 8 - prec;
        dshift = 8 - ushift;
        prec = 8;
        force8 = 1;
    }

    if (prec != 1 && prec != 2 && prec != 4 && prec != 8 && prec != 16) {
        fprintf(stderr, "imagetopng: can not create %s"
                "\n\twrong bit_depth %d\n", write_idf, prec);
        return fails;
    }
    writer = fopen(write_idf, "wb");

    if (writer == NULL) {
        return fails;
    }

    info = NULL;
    has_alpha = 0;

    /* Create and initialize the png_struct with the desired error handler
     * functions.  If you want to use the default stderr and longjump method,
     * you can supply NULL for the last three parameters.  We also check that
     * the library version is compatible with the one used at compile time,
     * in case we are using dynamically linked libraries.  REQUIRED.
    */
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                  NULL, NULL, NULL);
    /*png_voidp user_error_ptr, user_error_fn, user_warning_fn); */

    if (png == NULL) {
        goto fin;
    }

    /* Allocate/initialize the image information data.  REQUIRED
    */
    info = png_create_info_struct(png);

    if (info == NULL) {
        goto fin;
    }

    /* Set error handling.  REQUIRED if you are not supplying your own
     * error handling functions in the png_create_write_struct() call.
    */
    if (setjmp(png_jmpbuf(png))) {
        goto fin;
    }

    /* I/O initialization functions is REQUIRED
    */
    png_init_io(png, writer);

    /* Set the image information here.  Width and height are up to 2^31,
     * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
     * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
     * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
     * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
     * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
     * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE.
     * REQUIRED
     *
     * ERRORS:
     *
     * color_type == PNG_COLOR_TYPE_PALETTE && bit_depth > 8
     * color_type == PNG_COLOR_TYPE_RGB && bit_depth < 8
     * color_type == PNG_COLOR_TYPE_GRAY_ALPHA && bit_depth < 8
     * color_type == PNG_COLOR_TYPE_RGB_ALPHA) && bit_depth < 8
     *
    */
    png_set_compression_level(png, Z_BEST_COMPRESSION);

    if (prec == 16) {
        mask = 0xffff;
    } else if (prec == 8) {
        mask = 0x00ff;
    } else if (prec == 4) {
        mask = 0x000f;
    } else if (prec == 2) {
        mask = 0x0003;
    } else if (prec == 1) {
        mask = 0x0001;
    }

    if (nr_comp >= 3
            && image->comps[0].dx == image->comps[1].dx
            && image->comps[1].dx == image->comps[2].dx
            && image->comps[0].dy == image->comps[1].dy
            && image->comps[1].dy == image->comps[2].dy
            && image->comps[0].prec == image->comps[1].prec
            && image->comps[1].prec == image->comps[2].prec) {
        int v;

        has_alpha = (nr_comp > 3);

        is16 = (prec == 16);

        width = image->comps[0].w;
        height = image->comps[0].h;

        red = image->comps[0].data;
        green = image->comps[1].data;
        blue = image->comps[2].data;

        sig_bit.red = sig_bit.green = sig_bit.blue = prec;

        if (has_alpha) {
            sig_bit.alpha = prec;
            alpha = image->comps[3].data;
            color_type = PNG_COLOR_TYPE_RGB_ALPHA;
            adjustA = (image->comps[3].sgnd ? 1 << (image->comps[3].prec - 1) : 0);
        } else {
            sig_bit.alpha = 0;
            alpha = NULL;
            color_type = PNG_COLOR_TYPE_RGB;
            adjustA = 0;
        }
        png_set_sBIT(png, info, &sig_bit);

        png_set_IHDR(png, info, width, height, prec,
                     color_type,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE,  PNG_FILTER_TYPE_BASE);
        /*=============================*/
        png_write_info(png, info);
        /*=============================*/
        if (prec < 8) {
            png_set_packing(png);
        }
        adjustR = (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
        adjustG = (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
        adjustB = (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);

        row_buf = (unsigned char*)malloc(width * nr_comp * 2);

        for (y = 0; y < height; ++y) {
            d = row_buf;

            for (x = 0; x < width; ++x) {
                if (is16) {
                    v = *red + adjustR;
                    ++red;

                    if (force16) {
                        v = (v << ushift) + (v >> dshift);
                    }

                    *d++ = (unsigned char)(v >> 8);
                    *d++ = (unsigned char)v;

                    v = *green + adjustG;
                    ++green;

                    if (force16) {
                        v = (v << ushift) + (v >> dshift);
                    }

                    *d++ = (unsigned char)(v >> 8);
                    *d++ = (unsigned char)v;

                    v =  *blue + adjustB;
                    ++blue;

                    if (force16) {
                        v = (v << ushift) + (v >> dshift);
                    }

                    *d++ = (unsigned char)(v >> 8);
                    *d++ = (unsigned char)v;

                    if (has_alpha) {
                        v = *alpha + adjustA;
                        ++alpha;

                        if (force16) {
                            v = (v << ushift) + (v >> dshift);
                        }

                        *d++ = (unsigned char)(v >> 8);
                        *d++ = (unsigned char)v;
                    }
                    continue;
                }/* if(is16) */

                v = *red + adjustR;
                ++red;

                if (force8) {
                    v = (v << ushift) + (v >> dshift);
                }

                *d++ = (unsigned char)(v & mask);

                v = *green + adjustG;
                ++green;

                if (force8) {
                    v = (v << ushift) + (v >> dshift);
                }

                *d++ = (unsigned char)(v & mask);

                v = *blue + adjustB;
                ++blue;

                if (force8) {
                    v = (v << ushift) + (v >> dshift);
                }

                *d++ = (unsigned char)(v & mask);

                if (has_alpha) {
                    v = *alpha + adjustA;
                    ++alpha;

                    if (force8) {
                        v = (v << ushift) + (v >> dshift);
                    }

                    *d++ = (unsigned char)(v & mask);
                }
            }  /* for(x) */

            png_write_row(png, row_buf);

        } /* for(y) */
        free(row_buf);

    }/* nr_comp >= 3 */
    else if (nr_comp == 1 /* GRAY */
             || (nr_comp == 2    /* GRAY_ALPHA */
                 && image->comps[0].dx == image->comps[1].dx
                 && image->comps[0].dy == image->comps[1].dy
                 && image->comps[0].prec == image->comps[1].prec)) {
        int v;

        red = image->comps[0].data;

        sig_bit.gray = prec;
        sig_bit.red = sig_bit.green = sig_bit.blue = sig_bit.alpha = 0;
        alpha = NULL;
        adjustA = 0;
        color_type = PNG_COLOR_TYPE_GRAY;

        if (nr_comp == 2) {
            has_alpha = 1;
            sig_bit.alpha = prec;
            alpha = image->comps[1].data;
            color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
            adjustA = (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
        }
        width = image->comps[0].w;
        height = image->comps[0].h;

        png_set_IHDR(png, info, width, height, sig_bit.gray,
                     color_type,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE,  PNG_FILTER_TYPE_BASE);

        png_set_sBIT(png, info, &sig_bit);
        /*=============================*/
        png_write_info(png, info);
        /*=============================*/
        adjustR = (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

        if (prec < 8) {
            png_set_packing(png);
        }

        if (prec > 8) {
            row_buf = (unsigned char*)
                      malloc(width * nr_comp * sizeof(unsigned short));

            for (y = 0; y < height; ++y) {
                d = row_buf;

                for (x = 0; x < width; ++x) {
                    v = *red + adjustR;
                    ++red;

                    if (force16) {
                        v = (v << ushift) + (v >> dshift);
                    }

                    *d++ = (unsigned char)(v >> 8);
                    *d++ = (unsigned char)v;

                    if (has_alpha) {
                        v = *alpha++;

                        if (force16) {
                            v = (v << ushift) + (v >> dshift);
                        }

                        *d++ = (unsigned char)(v >> 8);
                        *d++ = (unsigned char)v;
                    }
                }/* for(x) */
                png_write_row(png, row_buf);

            }  /* for(y) */
            free(row_buf);
        } else { /* prec <= 8 */
            row_buf = (unsigned char*)calloc(width, nr_comp * 2);

            for (y = 0; y < height; ++y) {
                d = row_buf;

                for (x = 0; x < width; ++x) {
                    v = *red + adjustR;
                    ++red;

                    if (force8) {
                        v = (v << ushift) + (v >> dshift);
                    }

                    *d++ = (unsigned char)(v & mask);

                    if (has_alpha) {
                        v = *alpha + adjustA;
                        ++alpha;

                        if (force8) {
                            v = (v << ushift) + (v >> dshift);
                        }

                        *d++ = (unsigned char)(v & mask);
                    }
                }/* for(x) */

                png_write_row(png, row_buf);

            }  /* for(y) */
            free(row_buf);
        }
    } else {
        fprintf(stderr, "imagetopng: can not create %s\n", write_idf);
        goto fin;
    }
    png_write_end(png, info);

    fails = 0;

fin:

    if (png) {
        png_destroy_write_struct(&png, &info);
    }
    fclose(writer);

    if (fails) {
        remove(write_idf);
    }

    return fails;
}/* imagetopng() */
#endif /* OPJ_HAVE_LIBPNG */
