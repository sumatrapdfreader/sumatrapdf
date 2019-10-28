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
#ifndef __J2K_CONVERT_H
#define __J2K_CONVERT_H

/**@name RAW component encoding parameters */
/*@{*/
typedef struct raw_comp_cparameters {
    /** subsampling in X direction */
    int dx;
    /** subsampling in Y direction */
    int dy;
    /*@}*/
} raw_comp_cparameters_t;

/**@name RAW image encoding parameters */
/*@{*/
typedef struct raw_cparameters {
    /** width of the raw image */
    int rawWidth;
    /** height of the raw image */
    int rawHeight;
    /** number of components of the raw image */
    int rawComp;
    /** bit depth of the raw image */
    int rawBitDepth;
    /** signed/unsigned raw image */
    OPJ_BOOL rawSigned;
    /** raw components parameters */
    raw_comp_cparameters_t *rawComps;
    /*@}*/
} raw_cparameters_t;

/* Component precision clipping */
void clip_component(opj_image_comp_t* component, OPJ_UINT32 precision);
/* Component precision scaling */
void scale_component(opj_image_comp_t* component, OPJ_UINT32 precision);

/* planar / interleaved conversions */
typedef void (* convert_32s_CXPX)(const OPJ_INT32* pSrc, OPJ_INT32* const* pDst,
                                  OPJ_SIZE_T length);
extern const convert_32s_CXPX convert_32s_CXPX_LUT[5];
typedef void (* convert_32s_PXCX)(OPJ_INT32 const* const* pSrc, OPJ_INT32* pDst,
                                  OPJ_SIZE_T length, OPJ_INT32 adjust);
extern const convert_32s_PXCX convert_32s_PXCX_LUT[5];
/* bit depth conversions */
typedef void (* convert_XXx32s_C1R)(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                                    OPJ_SIZE_T length);
extern const convert_XXx32s_C1R convert_XXu32s_C1R_LUT[9]; /* up to 8bpp */
typedef void (* convert_32sXXx_C1R)(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                                    OPJ_SIZE_T length);
extern const convert_32sXXx_C1R convert_32sXXu_C1R_LUT[9]; /* up to 8bpp */


/* TGA conversion */
opj_image_t* tgatoimage(const char *filename, opj_cparameters_t *parameters);
int imagetotga(opj_image_t * image, const char *outfile);

/* BMP conversion */
opj_image_t* bmptoimage(const char *filename, opj_cparameters_t *parameters);
int imagetobmp(opj_image_t *image, const char *outfile);

/* TIFF conversion*/
opj_image_t* tiftoimage(const char *filename, opj_cparameters_t *parameters);
int imagetotif(opj_image_t *image, const char *outfile);
/**
Load a single image component encoded in PGX file format
@param filename Name of the PGX file to load
@param parameters *List ?*
@return Returns a greyscale image if successful, returns NULL otherwise
*/
opj_image_t* pgxtoimage(const char *filename, opj_cparameters_t *parameters);
int imagetopgx(opj_image_t *image, const char *outfile);

opj_image_t* pnmtoimage(const char *filename, opj_cparameters_t *parameters);
int imagetopnm(opj_image_t *image, const char *outfile, int force_split);

/* RAW conversion */
int imagetoraw(opj_image_t * image, const char *outfile);
int imagetorawl(opj_image_t * image, const char *outfile);
opj_image_t* rawtoimage(const char *filename, opj_cparameters_t *parameters,
                        raw_cparameters_t *raw_cp);
opj_image_t* rawltoimage(const char *filename, opj_cparameters_t *parameters,
                         raw_cparameters_t *raw_cp);

/* PNG conversion*/
extern int imagetopng(opj_image_t *image, const char *write_idf);
extern opj_image_t* pngtoimage(const char *filename,
                               opj_cparameters_t *parameters);

#endif /* __J2K_CONVERT_H */

