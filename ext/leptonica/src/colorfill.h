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

#ifndef  LEPTONICA_COLORFILL_H
#define  LEPTONICA_COLORFILL_H

/*!
 * \file colorfill.h
 *
 * <pre>
 *  Contains the following struct
 *      struct L_Colorfill
 *
 *  This accumulates color information, linked to location, within a
 *  set of tiles that (mostly) covers an input RGB image.
 * </pre>
 */


/*------------------------------------------------------------------------*
 *                            Colorfill data                              *
 *------------------------------------------------------------------------*/
/*! Colorfill data */
struct L_Colorfill
{
    struct Pix     *pixs;        /*!< clone of source pix                    */
    struct Pix     *pixst;       /*!< source pix, after optional transform   */
    l_int32         nx;          /*!< number of tiles in each tile row       */
    l_int32         ny;          /*!< number of tiles in each tile column    */
    l_int32         tw;          /*!< width of each tile                     */
    l_int32         th;          /*!< height of each tile                    */
    l_int32         minarea;     /*!< min number of pixels in a color region */
    struct Boxa    *boxas;       /*!< tile locations                         */
    struct Pixa    *pixas;       /*!< tiles from source pix                  */
    struct Pixa    *pixam;       /*!< mask tiles with components covering    */
                                 /*!< regions with similar color             */
    struct Numaa   *naa;         /*!< sizes of color regions (in pixels)     */
    struct L_Dnaa  *dnaa;        /*!< average color in each region           */
    struct Pixa    *pixadb;      /*!< debug reconstruction from segmentation */
};
typedef struct L_Colorfill  L_COLORFILL;


#endif  /* LEPTONICA_COLORFILL_H */
