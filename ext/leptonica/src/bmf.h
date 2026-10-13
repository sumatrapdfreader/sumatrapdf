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

#ifndef  LEPTONICA_BMF_H
#define  LEPTONICA_BMF_H

/*!
 * \file bmf.h
 *
 *     Simple data structure to hold bitmap fonts and related data
 */

    /*! Constants for deciding when text block is divided into paragraphs */
/*! Split Text */
enum {
    SPLIT_ON_LEADING_WHITE = 1,    /*!< tab or space at beginning of line   */
    SPLIT_ON_BLANK_LINE    = 2,    /*!< newline with optional white space   */
    SPLIT_ON_BOTH          = 3     /*!< leading white space or newline      */
};


/*! Data structure to hold bitmap fonts and related data */
struct L_Bmf
{
    struct Pixa  *pixa;        /*!< pixa of bitmaps for 93 characters        */
    l_int32       size;        /*!< font size (in points at 300 ppi)         */
    char         *directory;   /*!< directory containing font bitmaps        */
    l_int32       baseline1;   /*!< baseline offset for ascii 33 - 57        */
    l_int32       baseline2;   /*!< baseline offset for ascii 58 - 91        */
    l_int32       baseline3;   /*!< baseline offset for ascii 93 - 126       */
    l_int32       lineheight;  /*!< max height of line of chars              */
    l_int32       kernwidth;   /*!< pixel dist between char bitmaps          */
    l_int32       spacewidth;  /*!< pixel dist between word bitmaps          */
    l_int32       vertlinesep; /*!< extra vertical space between text lines  */
    l_int32      *fonttab;     /*!< table mapping ascii --> font index       */
    l_int32      *baselinetab; /*!< table mapping ascii --> baseline offset  */
};
typedef struct L_Bmf L_BMF;

#endif  /* LEPTONICA_BMF_H */
