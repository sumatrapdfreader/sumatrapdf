/*  eci.c - Extended Channel Interpretations to Unicode tables */
/*
    libzint - the open source barcode library
    Copyright (C) 2009-2024 Robin Stuart <rstuart114@gmail.com>

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

#ifndef Z_ECI_H
#define Z_ECI_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

INTERNAL int is_eci_convertible(const int eci);
INTERNAL int is_eci_convertible_segs(const struct zint_seg segs[], const int seg_count, int convertible[]);

INTERNAL int get_eci_length(const int eci, const unsigned char source[], int length);
INTERNAL int get_eci_length_segs(const struct zint_seg segs[], const int seg_count);

INTERNAL int utf8_to_eci(const int eci, const unsigned char source[], unsigned char dest[], int *p_length);

INTERNAL int get_best_eci(const unsigned char source[], int length);
INTERNAL int get_best_eci_segs(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count);

/* QRCODE Shift JIS helpers */
INTERNAL int sjis_utf8(struct zint_symbol *symbol, const unsigned char source[], int *p_length,
                unsigned int *ddata);
INTERNAL void sjis_cpy(const unsigned char source[], int *p_length, unsigned int *ddata, const int full_multibyte);
INTERNAL void sjis_cpy_segs(struct zint_seg segs[], const int seg_count, unsigned int *ddata,
                const int full_multibyte);
INTERNAL int sjis_utf8_to_eci(const int eci, const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte);

/* GRIDMATRIX GB 2312 helpers */
INTERNAL int gb2312_utf8(struct zint_symbol *symbol, const unsigned char source[], int *p_length,
                unsigned int *ddata);
INTERNAL void gb2312_cpy_segs(struct zint_seg segs[], const int seg_count, unsigned int *ddata,
                const int full_multibyte);
INTERNAL int gb2312_utf8_to_eci(const int eci, const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte);

/* HANXIN GB 18030 helpers */
INTERNAL int gb18030_utf8(struct zint_symbol *symbol, const unsigned char source[], int *p_length,
                unsigned int *ddata);
INTERNAL void gb18030_cpy_segs(struct zint_seg segs[], const int seg_count, unsigned int *ddata,
                const int full_multibyte);
INTERNAL int gb18030_utf8_to_eci(const int eci, const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* Z_ECI_H */
