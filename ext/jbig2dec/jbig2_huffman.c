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

    $Id: jbig2_huffman.c 467 2008-05-17 00:08:26Z giles $
*/

/* Huffman table decoding procedures
    -- See Annex B of the JBIG2 specification */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stdlib.h>

#ifdef JBIG2_DEBUG
#include <stdio.h>
#endif

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_huffman.h"
#include "jbig2_hufftab.h"

#define JBIG2_HUFFMAN_FLAGS_ISOOB 1
#define JBIG2_HUFFMAN_FLAGS_ISLOW 2
#define JBIG2_HUFFMAN_FLAGS_ISEXT 4



struct _Jbig2HuffmanState {
  /* The current bit offset is equal to (offset * 8) + offset_bits.
     The MSB of this_word is the current bit offset. The MSB of next_word
     is (offset + 4) * 8. */
  uint32_t this_word;
  uint32_t next_word;
  int offset_bits;
  int offset;

  Jbig2WordStream *ws;
};


/** Allocate and initialize a new huffman coding state
 *  the returned pointer can simply be freed; this does
 *  not affect the associated Jbig2WordStream.
 */
Jbig2HuffmanState *
jbig2_huffman_new (Jbig2Ctx *ctx, Jbig2WordStream *ws)
{
  Jbig2HuffmanState *result;

  result = (Jbig2HuffmanState *)jbig2_alloc(ctx->allocator,
	sizeof(Jbig2HuffmanState));

  if (result != NULL) {
      result->offset = 0;
      result->offset_bits = 0;
      result->this_word = ws->get_next_word (ws, 0);
      result->next_word = ws->get_next_word (ws, 4);

      result->ws = ws;
  }

  return result;
}

/** Free an allocated huffman coding state.
 *  This just calls jbig2_free() if the pointer is not NULL
 */
void
jbig2_huffman_free (Jbig2Ctx *ctx, Jbig2HuffmanState *hs)
{
  if (hs != NULL) free(hs);
  return;
}

/** debug routines **/
#ifdef JBIG2_DEBUG

/** print current huffman state */
void jbig2_dump_huffman_state(Jbig2HuffmanState *hs) {
  fprintf(stderr, "huffman state %08x %08x offset %d.%d\n",
	hs->this_word, hs->next_word, hs->offset, hs->offset_bits);
}

/** print the binary string we're reading from */
void jbig2_dump_huffman_binary(Jbig2HuffmanState *hs)
{
  const uint32_t word = hs->this_word;
  int i;

  fprintf(stderr, "huffman binary ");
  for (i = 31; i >= 0; i--)
    fprintf(stderr, ((word >> i) & 1) ? "1" : "0");
  fprintf(stderr, "\n");
}

#endif /* JBIG2_DEBUG */

/** Skip bits up to the next byte boundary
 */
void
jbig2_huffman_skip(Jbig2HuffmanState *hs)
{
  int bits = hs->offset_bits & 7;

  if (bits) {
    bits = 8 - bits;
    hs->offset_bits += bits;
    hs->this_word = (hs->this_word << bits) |
	(hs->next_word >> (32 - hs->offset_bits));
  }

  if (hs->offset_bits >= 32) {
    Jbig2WordStream *ws = hs->ws;
    hs->this_word = hs->next_word;
    hs->offset += 4;
    hs->next_word = ws->get_next_word (ws, hs->offset + 4);
    hs->offset_bits -= 32;
    if (hs->offset_bits) {
      hs->this_word = (hs->this_word << hs->offset_bits) |
	(hs->next_word >> (32 - hs->offset_bits));
    }
  }
}

/* skip ahead a specified number of bytes in the word stream
 */
void jbig2_huffman_advance(Jbig2HuffmanState *hs, int offset)
{
  Jbig2WordStream *ws = hs->ws;

  hs->offset += offset & ~3;
  hs->offset_bits += (offset & 3) << 3;
  if (hs->offset_bits >= 32) {
    hs->offset += 4;
    hs->offset_bits -= 32;
  }
  hs->this_word = ws->get_next_word (ws, hs->offset);
  hs->next_word = ws->get_next_word (ws, hs->offset + 4);
  if (hs->offset_bits > 0)
    hs->this_word = (hs->this_word << hs->offset_bits) |
	(hs->next_word >> (32 - hs->offset_bits));
}

/* return the offset of the huffman decode pointer (in bytes)
 * from the beginning of the WordStream
 */
int
jbig2_huffman_offset(Jbig2HuffmanState *hs)
{
  return hs->offset + (hs->offset_bits >> 3);
}

/* read a number of bits directly from the huffman state
 * without decoding against a table
 */
int32_t
jbig2_huffman_get_bits (Jbig2HuffmanState *hs, const int bits)
{
  uint32_t this_word = hs->this_word;
  int32_t result;

  result = this_word >> (32 - bits);
  hs->offset_bits += bits;
  if (hs->offset_bits >= 32) {
    hs->offset += 4;
    hs->offset_bits -= 32;
    hs->this_word = hs->next_word;
    hs->next_word = hs->ws->get_next_word(hs->ws, hs->offset + 4);
    if (hs->offset_bits) {
      hs->this_word = (hs->this_word << hs->offset_bits) |
	(hs->next_word >> (32 - hs->offset_bits));
    } else {
      hs->this_word = (hs->this_word << hs->offset_bits);
    }
  } else {
    hs->this_word = (this_word << bits) |
	(hs->next_word >> (32 - hs->offset_bits));
  }

  return result;
}

int32_t
jbig2_huffman_get (Jbig2HuffmanState *hs,
		   const Jbig2HuffmanTable *table, bool *oob)
{
  Jbig2HuffmanEntry *entry;
  byte flags;
  int offset_bits = hs->offset_bits;
  uint32_t this_word = hs->this_word;
  uint32_t next_word;
  int RANGELEN;
  int32_t result;

  for (;;)
    {
      int log_table_size = table->log_table_size;
      int PREFLEN;

      entry = &table->entries[this_word >> (32 - log_table_size)];
      flags = entry->flags;
      PREFLEN = entry->PREFLEN;

      next_word = hs->next_word;
      offset_bits += PREFLEN;
      if (offset_bits >= 32)
	{
	  Jbig2WordStream *ws = hs->ws;
	  this_word = next_word;
	  hs->offset += 4;
	  next_word = ws->get_next_word (ws, hs->offset + 4);
	  offset_bits -= 32;
	  hs->next_word = next_word;
	  PREFLEN = offset_bits;
	}
      if (PREFLEN)
	this_word = (this_word << PREFLEN) |
	  (next_word >> (32 - offset_bits));
      if (flags & JBIG2_HUFFMAN_FLAGS_ISEXT)
	{
	  table = entry->u.ext_table;
	}
      else
	break;
    }
  result = entry->u.RANGELOW;
  RANGELEN = entry->RANGELEN;
  if (RANGELEN > 0)
    {
      int32_t HTOFFSET;

      HTOFFSET = this_word >> (32 - RANGELEN);
      if (flags & JBIG2_HUFFMAN_FLAGS_ISLOW)
	result -= HTOFFSET;
      else
	result += HTOFFSET;

      offset_bits += RANGELEN;
      if (offset_bits >= 32)
	{
	  Jbig2WordStream *ws = hs->ws;
	  this_word = next_word;
	  hs->offset += 4;
	  next_word = ws->get_next_word (ws, hs->offset + 4);
	  offset_bits -= 32;
	  hs->next_word = next_word;
	  RANGELEN = offset_bits;
	}
if (RANGELEN)
      this_word = (this_word << RANGELEN) |
	(next_word >> (32 - offset_bits));
    }

  hs->this_word = this_word;
  hs->offset_bits = offset_bits;

  if (oob != NULL)
    *oob = (flags & JBIG2_HUFFMAN_FLAGS_ISOOB);

  return result;
}

/* TODO: more than 8 bits here is wasteful of memory. We have support
   for sub-trees in jbig2_huffman_get() above, but don't use it here.
   We should, and then revert to 8 bits */
#define LOG_TABLE_SIZE_MAX 16

/** Build an in-memory representation of a Huffman table from the
 *  set of template params provided by the spec or a table segment
 */
Jbig2HuffmanTable *
jbig2_build_huffman_table (Jbig2Ctx *ctx, const Jbig2HuffmanParams *params)
{
  int LENCOUNT[1 << LOG_TABLE_SIZE_MAX];
  int LENMAX = -1;
  const Jbig2HuffmanLine *lines = params->lines;
  int n_lines = params->n_lines;
  int i, j;
  int max_j;
  int log_table_size = 0;
  Jbig2HuffmanTable *result;
  Jbig2HuffmanEntry *entries;
  int CURLEN;
  int firstcode = 0;
  int CURCODE;
  int CURTEMP;

  /* B.3, 1. */
  for (i = 0; i < params->n_lines; i++)
    {
      int PREFLEN = lines[i].PREFLEN;
      int lts;

      if (PREFLEN > LENMAX)
		{
			for (j = LENMAX + 1; j < PREFLEN + 1; j++)
				LENCOUNT[j] = 0;
			LENMAX = PREFLEN;
		}
      LENCOUNT[PREFLEN]++;

      lts = PREFLEN + lines[i].RANGELEN;
      if (lts > LOG_TABLE_SIZE_MAX)
		lts = PREFLEN;
      if (lts <= LOG_TABLE_SIZE_MAX && log_table_size < lts)
		log_table_size = lts;
    }
  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1,
	"constructing huffman table log size %d", log_table_size);
  max_j = 1 << log_table_size;

  result = (Jbig2HuffmanTable *)jbig2_alloc(ctx->allocator, sizeof(Jbig2HuffmanTable));
  result->log_table_size = log_table_size;
  entries = (Jbig2HuffmanEntry *)jbig2_alloc(ctx->allocator, max_j * sizeof(Jbig2HuffmanEntry));
  result->entries = entries;

  LENCOUNT[0] = 0;

  for (CURLEN = 1; CURLEN <= LENMAX; CURLEN++)
    {
      int shift = log_table_size - CURLEN;

      /* B.3 3.(a) */
      firstcode = (firstcode + LENCOUNT[CURLEN - 1]) << 1;
      CURCODE = firstcode;
      /* B.3 3.(b) */
      for (CURTEMP = 0; CURTEMP < n_lines; CURTEMP++)
	{
	  int PREFLEN = lines[CURTEMP].PREFLEN;
	  if (PREFLEN == CURLEN)
	    {
	      int RANGELEN = lines[CURTEMP].RANGELEN;
	      int start_j = CURCODE << shift;
	      int end_j = (CURCODE + 1) << shift;
	      byte eflags = 0;

	      if (end_j > max_j) {
		jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
		  "ran off the end of the entries table! (%d >= %d)",
		  end_j, max_j);
		jbig2_free(ctx->allocator, result->entries);
		jbig2_free(ctx->allocator, result);
		return NULL;
	      }
	      /* todo: build extension tables */
	      if (params->HTOOB && CURTEMP == n_lines - 1)
		eflags |= JBIG2_HUFFMAN_FLAGS_ISOOB;
	      if (CURTEMP == n_lines - (params->HTOOB ? 3 : 2))
		eflags |= JBIG2_HUFFMAN_FLAGS_ISLOW;
	      if (PREFLEN + RANGELEN > LOG_TABLE_SIZE_MAX) {
		  for (j = start_j; j < end_j; j++) {
		      entries[j].u.RANGELOW = lines[CURTEMP].RANGELOW;
		      entries[j].PREFLEN = PREFLEN;
		      entries[j].RANGELEN = RANGELEN;
		      entries[j].flags = eflags;
		    }
	      } else {
		  for (j = start_j; j < end_j; j++) {
		      int32_t HTOFFSET = (j >> (shift - RANGELEN)) &
			((1 << RANGELEN) - 1);
		      if (eflags & JBIG2_HUFFMAN_FLAGS_ISLOW)
			entries[j].u.RANGELOW = lines[CURTEMP].RANGELOW -
			  HTOFFSET;
		      else
			entries[j].u.RANGELOW = lines[CURTEMP].RANGELOW +
			  HTOFFSET;
		      entries[j].PREFLEN = PREFLEN + RANGELEN;
		      entries[j].RANGELEN = 0;
		      entries[j].flags = eflags;
		    }
		}
	      CURCODE++;
	    }
	}
    }

  return result;
}

/** Free the memory associated with the representation of table */
void
jbig2_release_huffman_table (Jbig2Ctx *ctx, Jbig2HuffmanTable *table)
{
  if (table != NULL) {
      jbig2_free(ctx->allocator, table->entries);
      jbig2_free(ctx->allocator, table);
  }
  return;
}

#ifdef TEST
#include <stdio.h>

/* a test bitstream, and a list of the table indicies
   to use in decoding it. 1 = table B.1 (A), 2 = table B.2 (B), and so on */
/* this test stream should decode to { 8, 5, oob, 8 } */

const byte	test_stream[] = { 0xe9, 0xcb, 0xf4, 0x00 };
const byte	test_tabindex[] = { 4, 2, 2, 1 };

static uint32_t
test_get_word (Jbig2WordStream *self, int offset)
{
	/* assume test_stream[] is at least 4 bytes */
	if (offset+3 > sizeof(test_stream))
		return 0;
	else
		return ( (test_stream[offset] << 24) |
				 (test_stream[offset+1] << 16) |
				 (test_stream[offset+2] << 8) |
				 (test_stream[offset+3]) );
}

int
main (int argc, char **argv)
{
  Jbig2Ctx *ctx;
  Jbig2HuffmanTable *tables[5];
  Jbig2HuffmanState *hs;
  Jbig2WordStream ws;
  bool oob;
  int32_t code;

  ctx = jbig2_ctx_new(NULL, 0, NULL, NULL, NULL);

  tables[0] = NULL;
  tables[1] = jbig2_build_huffman_table (ctx, &jbig2_huffman_params_A);
  tables[2] = jbig2_build_huffman_table (ctx, &jbig2_huffman_params_B);
  tables[3] = NULL;
  tables[4] = jbig2_build_huffman_table (ctx, &jbig2_huffman_params_D);
  ws.get_next_word = test_get_word;
  hs = jbig2_huffman_new (ctx, &ws);

  printf("testing jbig2 huffmann decoding...");
  printf("\t(should be 8 5 (oob) 8)\n");

  {
	int i;
	int sequence_length = sizeof(test_tabindex);

	for (i = 0; i < sequence_length; i++) {
		code = jbig2_huffman_get (hs, tables[test_tabindex[i]], &oob);
		if (oob) printf("(oob) ");
		else printf("%d ", code);
	}
  }

  printf("\n");

  jbig2_ctx_free(ctx);

  return 0;
}
#endif
