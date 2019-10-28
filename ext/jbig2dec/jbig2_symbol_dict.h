/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifndef _JBIG2_SYMBOL_DICT_H
#define _JBIG2_SYMBOL_DICT_H

/* symbol dictionary header */

/* the results of decoding a symbol dictionary */
typedef struct {
    uint32_t n_symbols;
    Jbig2Image **glyphs;
} Jbig2SymbolDict;

/* decode a symbol dictionary segment and store the results */
int jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);

/* get a particular glyph by index */
Jbig2Image *jbig2_sd_glyph(Jbig2SymbolDict *dict, unsigned int id);

/* return a new empty symbol dict */
Jbig2SymbolDict *jbig2_sd_new(Jbig2Ctx *ctx, uint32_t n_symbols);

/* release the memory associated with a symbol dict */
void jbig2_sd_release(Jbig2Ctx *ctx, Jbig2SymbolDict *dict);

/* generate a new symbol dictionary by concatenating a list of
   existing dictionaries */
Jbig2SymbolDict *jbig2_sd_cat(Jbig2Ctx *ctx, uint32_t n_dicts, Jbig2SymbolDict **dicts);

/* count the number of dictionary segments referred
   to by the given segment */
uint32_t jbig2_sd_count_referred(Jbig2Ctx *ctx, Jbig2Segment *segment);

/* return an array of pointers to symbol dictionaries referred
   to by a segment */
Jbig2SymbolDict **jbig2_sd_list_referred(Jbig2Ctx *ctx, Jbig2Segment *segment);

#endif /* _JBIG2_SYMBOL_DICT_H */
