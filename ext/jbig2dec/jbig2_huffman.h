/*
    jbig2dec
    
    Copyright (C) 2001-2005 Artifex Software, Inc.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id: jbig2_huffman.h 461 2008-05-07 21:37:02Z giles $
*/

#ifndef JBIG2_HUFFMAN_H
#define JBIG2_HUFFMAN_H

/* Huffman coder interface */

typedef struct _Jbig2HuffmanEntry Jbig2HuffmanEntry;
typedef struct _Jbig2HuffmanState Jbig2HuffmanState;
typedef struct _Jbig2HuffmanTable Jbig2HuffmanTable;
typedef struct _Jbig2HuffmanParams Jbig2HuffmanParams;

struct _Jbig2HuffmanEntry {
  union {
    int32_t RANGELOW;
    Jbig2HuffmanTable *ext_table;
  } u;
  byte PREFLEN;
  byte RANGELEN;
  byte flags;
};

struct _Jbig2HuffmanTable {
  int log_table_size;
  Jbig2HuffmanEntry *entries;
};

typedef struct _Jbig2HuffmanLine Jbig2HuffmanLine;

struct _Jbig2HuffmanLine {
  int PREFLEN;
  int RANGELEN;
  int RANGELOW;
};

struct _Jbig2HuffmanParams {
  bool HTOOB;
  int n_lines;
  const Jbig2HuffmanLine *lines;
};

Jbig2HuffmanState *
jbig2_huffman_new (Jbig2Ctx *ctx, Jbig2WordStream *ws);

void
jbig2_huffman_free (Jbig2Ctx *ctx, Jbig2HuffmanState *hs);

void
jbig2_huffman_skip(Jbig2HuffmanState *hs);

void jbig2_huffman_advance(Jbig2HuffmanState *hs, int offset);

int 
jbig2_huffman_offset(Jbig2HuffmanState *hs);

int32_t
jbig2_huffman_get (Jbig2HuffmanState *hs,
		   const Jbig2HuffmanTable *table, bool *oob);

int32_t
jbig2_huffman_get_bits (Jbig2HuffmanState *hs, const int bits);

#ifdef JBIG2_DEBUG
void jbig2_dump_huffman_state(Jbig2HuffmanState *hs);
void jbig2_dump_huffman_binary(Jbig2HuffmanState *hs);
#endif

Jbig2HuffmanTable *
jbig2_build_huffman_table (Jbig2Ctx *ctx, const Jbig2HuffmanParams *params);

void
jbig2_release_huffman_table (Jbig2Ctx *ctx, Jbig2HuffmanTable *table);

/* standard Huffman templates defined by the specification */
extern const Jbig2HuffmanParams jbig2_huffman_params_A; /* Table B.1  */
extern const Jbig2HuffmanParams jbig2_huffman_params_B; /* Table B.2  */
extern const Jbig2HuffmanParams jbig2_huffman_params_C; /* Table B.3  */
extern const Jbig2HuffmanParams jbig2_huffman_params_D; /* Table B.4  */
extern const Jbig2HuffmanParams jbig2_huffman_params_E; /* Table B.5  */
extern const Jbig2HuffmanParams jbig2_huffman_params_F; /* Table B.6  */
extern const Jbig2HuffmanParams jbig2_huffman_params_G; /* Table B.7  */
extern const Jbig2HuffmanParams jbig2_huffman_params_H; /* Table B.8  */
extern const Jbig2HuffmanParams jbig2_huffman_params_I; /* Table B.9  */
extern const Jbig2HuffmanParams jbig2_huffman_params_J; /* Table B.10 */
extern const Jbig2HuffmanParams jbig2_huffman_params_K; /* Table B.11 */
extern const Jbig2HuffmanParams jbig2_huffman_params_L; /* Table B.12 */
extern const Jbig2HuffmanParams jbig2_huffman_params_M; /* Table B.13 */
extern const Jbig2HuffmanParams jbig2_huffman_params_N; /* Table B.14 */
extern const Jbig2HuffmanParams jbig2_huffman_params_O; /* Table B.15 */


#endif /* JBIG2_HUFFMAN_H */
