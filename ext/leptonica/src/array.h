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

#ifndef  LEPTONICA_ARRAY_H
#define  LEPTONICA_ARRAY_H

/*!
 * \file array.h
 *
 * <pre>
 *  This file has typedefs for the following array structs:
 *      struct Numa          array of floats
 *      struct Numaa
 *      struct L_Dna         array of doubles
 *      struct L_Dnaa
 *      struct L_Dnahash
 *      struct Sarray        array of C-strings
 *      struct L_Bytea       array of bytes
 *
 *  It contains definitions for:
 *      Numa interpolation flags
 *      Numa border flags
 *      Numa data type conversion to string
 *
 *  Here are the non-image-related arrays in leptonica:
 *  * Numa, L_Dna, L_Ptra, Sarray:
 *    These have most of the typical operations of vectors, such as add,
 *    insert, remove and replace.
 *  * Numaa, L_Dnaa, L_Ptraa:
 *    These are arrays of float, double and generic pointer arrays.
 *  * L_Bytea:
 *    This is an array of bytes, analogous to a C++ string.
 *  * L_Dnahash:
 *    This is a simple hashing for integers, used in the jbig2 classifier.
 * </pre>
 */


/*------------------------------------------------------------------------*
 *                             Array Structs                              *
 *------------------------------------------------------------------------*/
    /*! Number array: an array of floats */
typedef struct Numa  NUMA;

    /*! Array of number arrays */
typedef struct Numaa  NUMAA;

    /*! Double number array: an array of doubles */
typedef struct L_Dna  L_DNA;

    /*! Array of double number arrays */
typedef struct L_Dnaa  L_DNAA;

    /*! Array of double number arrays, used as a simple hash */
typedef struct L_DnaHash L_DNAHASH;

    /*! String array: an array of C strings */
typedef struct Sarray SARRAY;

    /*! Byte array (analogous to C++ "string") */
typedef struct L_Bytea L_BYTEA;


/*------------------------------------------------------------------------*
 *                              Array flags                               *
 *------------------------------------------------------------------------*/
/*! Numa Interpolation */
enum {
    L_LINEAR_INTERP = 1,        /*!< linear     */
    L_QUADRATIC_INTERP = 2      /*!< quadratic  */
};

/*! Numa Border Adding */
enum {
    L_CONTINUED_BORDER = 1,    /*!< extended with same value                  */
    L_SLOPE_BORDER = 2,        /*!< extended with constant normal derivative  */
    L_MIRRORED_BORDER = 3      /*!< mirrored                                  */
};

/*! Numa Data Conversion */
enum {
    L_INTEGER_VALUE = 1,        /*!< convert to integer  */
    L_FLOAT_VALUE = 2           /*!< convert to float    */
};

#endif  /* LEPTONICA_ARRAY_H */
