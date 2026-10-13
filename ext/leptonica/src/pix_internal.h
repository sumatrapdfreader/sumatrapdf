/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

#ifndef  LEPTONICA_PIX_INTERNAL_H
#define  LEPTONICA_PIX_INTERNAL_H

/*!
 * \file pix_internal.h
 *
 * <pre>
 *  Valid image types in leptonica:
 *       Pix: 1 bpp, with and without colormap
 *       Pix: 2 bpp, with and without colormap
 *       Pix: 4 bpp, with and without colormap
 *       Pix: 8 bpp, with and without colormap
 *       Pix: 16 bpp (1 spp)
 *       Pix: 32 bpp (rgb, 3 spp)
 *       Pix: 32 bpp (rgba, 4 spp)
 *       FPix: 32 bpp float
 *       DPix: 64 bpp double
 *       Notes:
 *          (1) The only valid Pix image type with alpha is rgba.
 *              In particular, the alpha component is not used in
 *              cmapped images.
 *          (2) PixComp can hold any Pix with IFF_PNG encoding.
 *
 *  This file is internal: it is not part of the public interface.
 *  It contains the definitions of most of the image-related structs
 *  used in leptonica:
 *       struct Pix
 *       struct PixColormap
 *       struct RGBA_Quad
 *       struct Pixa
 *       struct Pixaa
 *       struct Box
 *       struct Boxa
 *       struct Boxaa
 *       struct Pta
 *       struct Ptaa
 *       struct Pixacc
 *       struct PixTiling
 *       struct FPix
 *       struct FPixa
 *       struct DPix
 *       struct PixComp
 *       struct PixaComp
 *
 *  This file can be #included after allheaders.h in source files that
 *  require direct access to the internal data fields in these structs.
 *
 *  Notes on the pixels in the raster image.  Most of this information
 *  can also be found in pix.h.
 *
 *       (1) The image data is stored in a single contiguous
 *           array of l_uint32, into which the pixels are packed.
 *           By "packed" we mean that there are no unused bits
 *           between pixels, except for end-of-line padding to
 *           satisfy item (2) below.
 *
 *       (2) Every image raster line begins on a 32-bit word
 *           boundary within this array.
 *
 *       (3) Pix image data is stored in 32-bit units, with the
 *           pixels ordered from left to right in the image being
 *           stored in order from the MSB to LSB within the word,
 *           for both big-endian and little-endian machines.
 *           This is the natural ordering for big-endian machines,
 *           as successive bytes are stored and fetched progressively
 *           to the right.  However, for little-endians, when storing
 *           we re-order the bytes from this byte stream order, and
 *           reshuffle again for byte access on 32-bit entities.
 *           So if the bytes come in sequence from left to right, we
 *           store them on little-endians in byte order:
 *                3 2 1 0 7 6 5 4 ...
 *           This MSB to LSB ordering allows left and right shift
 *           operations on 32 bit words to move the pixels properly.
 *
 *       (4) We use 32 bit pixels for both RGB and RGBA color images.
 *           The A (alpha) byte is ignored in most leptonica functions
 *           operating on color images.  Within each 4 byte pixel, the
 *           color samples are ordered from MSB to LSB, as follows:
 *
 *                |  MSB  |  2nd MSB  |  3rd MSB  |  LSB  |
 *                   red      green       blue      alpha
 *                    0         1           2         3   (big-endian)
 *                    3         2           1         0   (little-endian)
 *
 *           Because we use MSB to LSB ordering within the 32-bit word,
 *           the individual 8-bit samples can be accessed with
 *           GET_DATA_BYTE and SET_DATA_BYTE macros, using the
 *           (implicitly big-ending) ordering
 *                 red:    byte 0  (MSB)
 *                 green:  byte 1  (2nd MSB)
 *                 blue:   byte 2  (3rd MSB)
 *                 alpha:  byte 3  (LSB)
 *
 *           The specific color assignment is made in this file,
 *           through the definitions of COLOR_RED, etc.  Then the R, G
 *           B and A sample values can be retrieved using
 *                 redval = GET_DATA_BYTE(&pixel, COLOR_RED);
 *                 greenval = GET_DATA_BYTE(&pixel, COLOR_GREEN);
 *                 blueval = GET_DATA_BYTE(&pixel, COLOR_BLUE);
 *                 alphaval = GET_DATA_BYTE(&pixel, L_ALPHA_CHANNEL);
 *           and they can be set with
 *                 SET_DATA_BYTE(&pixel, COLOR_RED, redval);
 *                 SET_DATA_BYTE(&pixel, COLOR_GREEN, greenval);
 *                 SET_DATA_BYTE(&pixel, COLOR_BLUE, blueval);
 *                 SET_DATA_BYTE(&pixel, L_ALPHA_CHANNEL, alphaval);
 *
 *           More efficiently, these components can be extracted directly
 *           by shifting and masking, explicitly using the values in
 *           L_RED_SHIFT, etc.:
 *                 (pixel32 >> L_RED_SHIFT) & 0xff;         (red)
 *                 (pixel32 >> L_GREEN_SHIFT) & 0xff;       (green)
 *                 (pixel32 >> L_BLUE_SHIFT) & 0xff;        (blue)
 *                 (pixel32 >> L_ALPHA_SHIFT) & 0xff;       (alpha)
 *           The functions extractRGBValues() and extractRGBAValues() are
 *           provided to do this.  Likewise, the pixels can be set
 *           directly by shifting, using composeRGBPixel() and
 *           composeRGBAPixel().
 *
 *           All these operations work properly on both big- and little-endians.
 *
 *       (5) A reference count is held within each pix, giving the
 *           number of ptrs to the pix.  When a pixClone() call
 *           is made, the ref count is increased by 1, and
 *           when a pixDestroy() call is made, the reference count
 *           of the pix is decremented.  The pix is only destroyed
 *           when the reference count goes to zero.
 *
 *       (6) The version numbers (below) are used in the serialization
 *           of these data structures.  They are placed in the files,
 *           and rarely (if ever) change.
 *
 *       (7) The serialization dependencies are as follows:
 *               pixaa  :  pixa  :  boxa
 *               boxaa  :  boxa
 *           So, for example, pixaa and boxaa can be changed without
 *           forcing a change in pixa or boxa.  However, if pixa is
 *           changed, it forces a change in pixaa, and if boxa is
 *           changed, if forces a change in the other three.
 *           We define four version numbers:
 *               PIXAA_VERSION_NUMBER
 *               PIXA_VERSION_NUMBER
 *               BOXAA_VERSION_NUMBER
 *               BOXA_VERSION_NUMBER
 * </pre>
 *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*
 *                              Basic Pix                                  *
 *-------------------------------------------------------------------------*/
    /* The 'special' field is by default 0, but it can hold integers
     * that direct non-default actions, e.g., in png and jpeg I/O. */

/*! Basic Pix */
struct Pix
{
    l_uint32             w;         /*!< width in pixels                   */
    l_uint32             h;         /*!< height in pixels                  */
    l_uint32             d;         /*!< depth in bits (bpp)               */
    l_uint32             spp;       /*!< number of samples per pixel       */
    l_uint32             wpl;       /*!< 32-bit words/line                 */
    l_atomic             refcount;  /*!< reference count (1 if no clones)  */
    l_int32              xres;      /*!< image res (ppi) in x direction    */
                                    /*!< (use 0 if unknown)                */
    l_int32              yres;      /*!< image res (ppi) in y direction    */
                                    /*!< (use 0 if unknown)                */
    l_int32              informat;  /*!< input file format, IFF_*          */
    l_int32              special;   /*!< special instructions for I/O, etc */
    char                *text;      /*!< text string associated with pix   */
    struct PixColormap  *colormap;  /*!< colormap (may be null)            */
    l_uint32            *data;      /*!< the image data                    */
};

/*! Colormap of a Pix */
struct PixColormap
{
    void            *array;   /*!< colormap table (array of RGBA_QUAD)     */
    l_int32          depth;   /*!< of pix (1, 2, 4 or 8 bpp)               */
    l_int32          nalloc;  /*!< number of color entries allocated       */
    l_int32          n;       /*!< number of color entries used            */
};


    /*! Colormap table entry (after the BMP version).
     * Note that the BMP format stores the colormap table exactly
     * as it appears here, with color samples being stored sequentially,
     * in the order (b,g,r,a). */
struct RGBA_Quad
{
    l_uint8     blue;         /*!< blue value */
    l_uint8     green;        /*!< green value */
    l_uint8     red;          /*!< red value */
    l_uint8     alpha;        /*!< alpha value */
};


/*-------------------------------------------------------------------------*
 *                              Array of pix                               *
 *-------------------------------------------------------------------------*/
    /*  Serialization for primary data structures */
#define  PIXAA_VERSION_NUMBER      2  /*!< Version for Pixaa serialization */
#define  PIXA_VERSION_NUMBER       2  /*!< Version for Pixa serialization  */
#define  BOXA_VERSION_NUMBER       2  /*!< Version for Boxa serialization  */
#define  BOXAA_VERSION_NUMBER      3  /*!< Version for Boxaa serialization */

/*! Array of pix */
struct Pixa
{
    l_int32             n;          /*!< number of Pix in ptr array        */
    l_int32             nalloc;     /*!< number of Pix ptrs allocated      */
    l_atomic            refcount;   /*!< reference count (1 if no clones)  */
    struct Pix        **pix;        /*!< the array of ptrs to pix          */
    struct Boxa        *boxa;       /*!< array of boxes                    */
};

/*! Array of arrays of pix */
struct Pixaa
{
    l_int32             n;          /*!< number of Pixa in ptr array       */
    l_int32             nalloc;     /*!< number of Pixa ptrs allocated     */
    struct Pixa       **pixa;       /*!< array of ptrs to pixa             */
    struct Boxa        *boxa;       /*!< array of boxes                    */
};


/*-------------------------------------------------------------------------*
 *                    Basic rectangle and rectangle arrays                 *
 *-------------------------------------------------------------------------*/
/*! Basic rectangle */
struct Box
{
    l_int32            x;           /*!< left coordinate                   */
    l_int32            y;           /*!< top coordinate                    */
    l_int32            w;           /*!< box width                         */
    l_int32            h;           /*!< box height                        */
    l_atomic           refcount;    /*!< reference count (1 if no clones)  */
};

/*! Array of Box */
struct Boxa
{
    l_int32            n;           /*!< number of box in ptr array        */
    l_int32            nalloc;      /*!< number of box ptrs allocated      */
    l_atomic           refcount;    /*!< reference count (1 if no clones)  */
    struct Box       **box;         /*!< box ptr array                     */
};

/*! Array of Boxa */
struct Boxaa
{
    l_int32            n;           /*!< number of boxa in ptr array       */
    l_int32            nalloc;      /*!< number of boxa ptrs allocated     */
    struct Boxa      **boxa;        /*!< boxa ptr array                    */
};


/*-------------------------------------------------------------------------*
 *                               Array of points                           *
 *-------------------------------------------------------------------------*/
#define  PTA_VERSION_NUMBER      1  /*!< Version for Pta serialization     */

/*! Array of points */
struct Pta
{
    l_int32            n;           /*!< actual number of pts              */
    l_int32            nalloc;      /*!< size of allocated arrays          */
    l_atomic           refcount;    /*!< reference count (1 if no clones)  */
    l_float32         *x, *y;       /*!< arrays of floats                  */
};


/*-------------------------------------------------------------------------*
 *                              Array of Pta                               *
 *-------------------------------------------------------------------------*/
/*! Array of Pta */
struct Ptaa
{
    l_int32              n;         /*!< number of pta in ptr array        */
    l_int32              nalloc;    /*!< number of pta ptrs allocated      */
    struct Pta         **pta;       /*!< pta ptr array                     */
};


/*-------------------------------------------------------------------------*
 *                       Pix accumulator container                         *
 *-------------------------------------------------------------------------*/
/*! Pix accumulator container */
struct Pixacc
{
    l_int32             w;          /*!< array width                       */
    l_int32             h;          /*!< array height                      */
    l_int32             offset;     /*!< used to allow negative            */
                                    /*!< intermediate results              */
    struct Pix         *pix;        /*!< the 32 bit accumulator pix        */
};


/*-------------------------------------------------------------------------*
 *                              Pix tiling                                 *
 *-------------------------------------------------------------------------*/
/*! Pix tiling */
struct PixTiling
{
    struct Pix          *pix;       /*!< input pix (a clone)               */
    l_int32              nx;        /*!< number of tiles horizontally      */
    l_int32              ny;        /*!< number of tiles vertically        */
    l_int32              w;         /*!< tile width                        */
    l_int32              h;         /*!< tile height                       */
    l_int32              xoverlap;  /*!< overlap on left and right         */
    l_int32              yoverlap;  /*!< overlap on top and bottom         */
    l_int32              strip;     /*!< strip for paint; default is TRUE  */
};


/*-------------------------------------------------------------------------*
 *                       FPix: pix with float array                        *
 *-------------------------------------------------------------------------*/
#define  FPIX_VERSION_NUMBER      2 /*!< Version for FPix serialization    */

/*! Pix with float array */
struct FPix
{
    l_int32              w;         /*!< width in pixels                   */
    l_int32              h;         /*!< height in pixels                  */
    l_int32              wpl;       /*!< 32-bit words/line                 */
    l_atomic             refcount;  /*!< reference count (1 if no clones)  */
    l_int32              xres;      /*!< image res (ppi) in x direction    */
                                    /*!< (use 0 if unknown)                */
    l_int32              yres;      /*!< image res (ppi) in y direction    */
                                    /*!< (use 0 if unknown)                */
    l_float32           *data;      /*!< the float image data              */
};

/*! Array of FPix */
struct FPixa
{
    l_int32             n;          /*!< number of fpix in ptr array       */
    l_int32             nalloc;     /*!< number of fpix ptrs allocated     */
    l_atomic            refcount;   /*!< reference count (1 if no clones)  */
    struct FPix       **fpix;       /*!< the array of ptrs to fpix         */
};


/*-------------------------------------------------------------------------*
 *                       DPix: pix with double array                       *
 *-------------------------------------------------------------------------*/
#define  DPIX_VERSION_NUMBER      2 /*!< Version for DPix serialization    */

/*! Pix with double array */
struct DPix
{
    l_int32              w;         /*!< width in pixels                   */
    l_int32              h;         /*!< height in pixels                  */
    l_int32              wpl;       /*!< 32-bit words/line                 */
    l_atomic             refcount;  /*!< reference count (1 if no clones)  */
    l_int32              xres;      /*!< image res (ppi) in x direction    */
                                    /*!< (use 0 if unknown)                */
    l_int32              yres;      /*!< image res (ppi) in y direction    */
                                    /*!< (use 0 if unknown)                */
    l_float64           *data;      /*!< the double image data             */
};


/*-------------------------------------------------------------------------*
 *                        PixComp: compressed pix                          *
 *-------------------------------------------------------------------------*/
/*! Compressed Pix */
struct PixComp
{
    l_int32              w;         /*!< width in pixels                   */
    l_int32              h;         /*!< height in pixels                  */
    l_int32              d;         /*!< depth in bits                     */
    l_int32              xres;      /*!< image res (ppi) in x direction    */
                                    /*!<   (use 0 if unknown)              */
    l_int32              yres;      /*!< image res (ppi) in y direction    */
                                    /*!<   (use 0 if unknown)              */
    l_int32              comptype;  /*!< compressed format (IFF_TIFF_G4,   */
                                    /*!<   IFF_PNG, IFF_JFIF_JPEG)         */
    char                *text;      /*!< text string associated with pix   */
    l_int32              cmapflag;  /*!< flag (1 for cmap, 0 otherwise)    */
    l_uint8             *data;      /*!< the compressed image data         */
    size_t               size;      /*!< size of the data array            */
};


/*-------------------------------------------------------------------------*
 *                     PixaComp: array of compressed pix                   *
 *-------------------------------------------------------------------------*/
#define  PIXACOMP_VERSION_NUMBER 2  /*!< Version for PixaComp serialization */

/*! Array of compressed pix */
struct PixaComp
{
    l_int32              n;         /*!< number of PixComp in ptr array    */
    l_int32              nalloc;    /*!< number of PixComp ptrs allocated  */
    l_int32              offset;    /*!< indexing offset into ptr array    */
    struct PixComp     **pixc;      /*!< the array of ptrs to PixComp      */
    struct Boxa         *boxa;      /*!< array of boxes                    */
};

#endif  /* LEPTONICA_PIX_INTERNAL_H */
