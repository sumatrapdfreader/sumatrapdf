/* pdf417.h - PDF417 tables and coefficients declarations */
/*
    libzint - the open source barcode library
    Copyright (C) 2008-2024 Robin Stuart <rstuart114@gmail.com>
    Portions Copyright (C) 2004 Grandzebu

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

/* See "pdf417_tabs.h" for table definitions */

#ifndef Z_PDF417_H
#define Z_PDF417_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* PDF417 error correction coefficients from Grand Zebu */
INTERNAL_DATA_EXTERN const unsigned short pdf_coefrs[1022];

INTERNAL_DATA_EXTERN const unsigned short pdf_bitpattern[2787];

/* MicroPDF417 coefficients from ISO/IEC 24728:2006 Annex F */
INTERNAL_DATA_EXTERN const unsigned short pdf_Microcoeffs[344];

/* rows, columns, error codewords, k-offset of valid MicroPDF417 sizes from ISO/IEC 24728:2006 */
INTERNAL_DATA_EXTERN const unsigned short pdf_MicroVariants[136];

/* following is Left RAP, Centre RAP, Right RAP and Start Cluster from ISO/IEC 24728:2006 tables 10, 11 and 12 */
INTERNAL_DATA_EXTERN const char pdf_RAPTable[136];

/* Left and Right Row Address Pattern from Table 2 */
INTERNAL_DATA_EXTERN const unsigned short pdf_rap_side[52];

/* Centre Row Address Pattern from Table 2 */
INTERNAL_DATA_EXTERN const unsigned short pdf_rap_centre[52];

INTERNAL void pdf_byteprocess(short *chainemc, int *p_mclength, const unsigned char chaine[], int start,
                const int length, const int lastmode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* Z_PDF417_H */
