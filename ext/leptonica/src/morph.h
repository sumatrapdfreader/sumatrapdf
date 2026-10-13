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

#ifndef  LEPTONICA_MORPH_H
#define  LEPTONICA_MORPH_H

/*!
 * \file morph.h
 *
 * <pre>
 *  Contains the following structs:
 *      struct Sel
 *      struct Sela
 *      struct Kernel
 *
 *  Contains definitions for:
 *      morphological b.c. flags
 *      structuring element types
 *      runlength flags for granulometry
 *      direction flags for grayscale morphology
 *      morphological operation flags
 *      standard border size
 *      grayscale intensity scaling flags
 *      morphological tophat flags
 *      arithmetic and logical operator flags
 *      grayscale morphology selection flags
 *      distance function b.c. flags
 *      image comparison flags
 * </pre>
 */

/*-------------------------------------------------------------------------*
 *                             Sel and Sel array                           *
 *-------------------------------------------------------------------------*/
#define  SEL_VERSION_NUMBER    1

/*! Selection */
struct Sel
{
    l_int32       sy;        /*!< sel height                               */
    l_int32       sx;        /*!< sel width                                */
    l_int32       cy;        /*!< y location of sel origin                 */
    l_int32       cx;        /*!< x location of sel origin                 */
    l_int32     **data;      /*!< {0,1,2}; data[i][j] in [row][col] order  */
    char         *name;      /*!< used to find sel by name                 */
};
typedef struct Sel SEL;

/*! Array of Sel */
struct Sela
{
    l_int32          n;       /*!< number of sel actually stored           */
    l_int32          nalloc;  /*!< size of allocated ptr array             */
    struct Sel     **sel;     /*!< sel ptr array                           */
};
typedef struct Sela SELA;


/*-------------------------------------------------------------------------*
 *                                 Kernel                                  *
 *-------------------------------------------------------------------------*/
#define  KERNEL_VERSION_NUMBER    2

/*! Kernel */
struct L_Kernel
{
    l_int32       sy;        /*!< kernel height                            */
    l_int32       sx;        /*!< kernel width                             */
    l_int32       cy;        /*!< y location of kernel origin              */
    l_int32       cx;        /*!< x location of kernel origin              */
    l_float32   **data;      /*!< data[i][j] in [row][col] order           */
};
typedef struct L_Kernel  L_KERNEL;


/*-------------------------------------------------------------------------*
 *                 Morphological boundary condition flags                  *
 *                                                                         *
 *  Two types of boundary condition for erosion.                           *
 *  The global variable MORPH_BC takes on one of these two values.         *
 *  See notes in morph.c for usage.                                        *
 *-------------------------------------------------------------------------*/
/*! Morph Boundary */
enum {
    SYMMETRIC_MORPH_BC = 0,
    ASYMMETRIC_MORPH_BC = 1
};

/*-------------------------------------------------------------------------*
 *                        Structuring element vals                         *
 *-------------------------------------------------------------------------*/
/*! SEL Vals */
enum {
    SEL_DONT_CARE  = 0,
    SEL_HIT        = 1,
    SEL_MISS       = 2
};

/*-------------------------------------------------------------------------*
 *                  Runlength flags for granulometry                       *
 *-------------------------------------------------------------------------*/
/*! Runlength Polarity */
enum {
    L_RUN_OFF = 0,
    L_RUN_ON  = 1
};

/*-------------------------------------------------------------------------*
 *         Direction flags for grayscale morphology, granulometry,         *
 *                 composable Sels, convolution, etc.                      *
 *-------------------------------------------------------------------------*/
/*! Direction Flags */
enum {
    L_HORIZ            = 1,
    L_VERT             = 2,
    L_BOTH_DIRECTIONS  = 3
};

/*-------------------------------------------------------------------------*
 *                   Morphological operation flags                         *
 *-------------------------------------------------------------------------*/
/*! Morph Operator */
enum {
    L_MORPH_DILATE    = 1,
    L_MORPH_ERODE     = 2,
    L_MORPH_OPEN      = 3,
    L_MORPH_CLOSE     = 4,
    L_MORPH_HMT       = 5
};

/*-------------------------------------------------------------------------*
 *                    Grayscale intensity scaling flags                    *
 *-------------------------------------------------------------------------*/
/*! Pixel Value Scaling */
enum {
    L_LINEAR_SCALE  = 1,
    L_LOG_SCALE     = 2
};

/*-------------------------------------------------------------------------*
 *                      Morphological tophat flags                         *
 *-------------------------------------------------------------------------*/
/*! Morph Tophat */
enum {
    L_TOPHAT_WHITE = 0,
    L_TOPHAT_BLACK = 1
};

/*-------------------------------------------------------------------------*
 *                Arithmetic and logical operator flags                    *
 *                 (use on grayscale images and Numas)                     *
 *-------------------------------------------------------------------------*/
/*! ArithLogical Ops */
enum {
    L_ARITH_ADD       = 1,
    L_ARITH_SUBTRACT  = 2,
    L_ARITH_MULTIPLY  = 3,   /* on numas only */
    L_ARITH_DIVIDE    = 4,   /* on numas only */
    L_UNION           = 5,   /* on numas only */
    L_INTERSECTION    = 6,   /* on numas only */
    L_SUBTRACTION     = 7,   /* on numas only */
    L_EXCLUSIVE_OR    = 8    /* on numas only */
};

/*-------------------------------------------------------------------------*
 *                        Min/max selection flags                          *
 *-------------------------------------------------------------------------*/
/*! MinMax Selection */
enum {
    L_CHOOSE_MIN = 1,         /* useful in a downscaling "erosion"       */
    L_CHOOSE_MAX = 2,         /* useful in a downscaling "dilation"      */
    L_CHOOSE_MAXDIFF = 3,     /* useful in a downscaling contrast        */
    L_CHOOSE_MIN_BOOST = 4,   /* use a modification of the min value     */
    L_CHOOSE_MAX_BOOST = 5    /* use a modification of the max value     */
};

/*-------------------------------------------------------------------------*
 *            Exterior value b.c. for distance function flags              *
 *-------------------------------------------------------------------------*/
/*! Exterior Value */
enum {
    L_BOUNDARY_BG = 1,  /* assume bg outside image */
    L_BOUNDARY_FG = 2   /* assume fg outside image */
};

/*-------------------------------------------------------------------------*
 *                         Image comparison flags                          *
 *-------------------------------------------------------------------------*/
/*! Image Comparison */
enum {
    L_COMPARE_XOR = 1,
    L_COMPARE_SUBTRACT = 2,
    L_COMPARE_ABS_DIFF = 3
};

/*-------------------------------------------------------------------------*
 *    Standard size of border added around images for special processing   *
 *-------------------------------------------------------------------------*/
static const l_int32  ADDED_BORDER = 32;   /*!< pixels, not bits */


#endif  /* LEPTONICA_MORPH_H */
