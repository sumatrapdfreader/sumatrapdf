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

#ifndef  LEPTONICA_WATERSHED_H
#define  LEPTONICA_WATERSHED_H

/*!
 * \file watershed.h
 *
 *     Simple data structure to hold watershed data.
 *     All data here is owned by the L_WShed and must be freed.
 */

/*! Simple data structure to hold watershed data. */
struct L_WShed
{
    struct Pix    *pixs;      /*!< clone of input 8 bpp pixs                */
    struct Pix    *pixm;      /*!< clone of input 1 bpp seed (marker) pixm  */
    l_int32        mindepth;  /*!< minimum depth allowed for a watershed    */
    struct Pix    *pixlab;    /*!< 16 bpp label pix                         */
    struct Pix    *pixt;      /*!< scratch pix for computing wshed regions  */
    void         **lines8;    /*!< line ptrs for pixs                       */
    void         **linem1;    /*!< line ptrs for pixm                       */
    void         **linelab32; /*!< line ptrs for pixlab                     */
    void         **linet1;    /*!< line ptrs for pixt                       */
    struct Pixa   *pixad;     /*!< result: 1 bpp pixa of watersheds         */
    struct Pta    *ptas;      /*!< pta of initial seed pixels               */
    struct Numa   *nasi;      /*!< numa of seed indicators; 0 if completed  */
    struct Numa   *nash;      /*!< numa of initial seed heights             */
    struct Numa   *namh;      /*!< numa of initial minima heights           */
    struct Numa   *nalevels;  /*!< result: numa of watershed levels         */
    l_int32        nseeds;    /*!< number of seeds (markers)                */
    l_int32        nother;    /*!< number of minima different from seeds    */
    l_int32       *lut;       /*!< lut for pixel indices                    */
    struct Numa  **links;     /*!< back-links into lut, for updates         */
    l_int32        arraysize; /*!< size of links array                      */
    l_int32        debug;     /*!< set to 1 for debug output                */
};
typedef struct L_WShed L_WSHED;

#endif  /* LEPTONICA_WATERSHED_H */
