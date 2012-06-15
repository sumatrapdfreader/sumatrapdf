/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/


/* Huffman table decoding procedures
    -- See Annex B of the JBIG2 specification */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stdlib.h>
#include <string.h>

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
  Jbig2HuffmanState *result = NULL;

  result = jbig2_new(ctx, Jbig2HuffmanState, 1);

  if (result != NULL) {
      result->offset = 0;
      result->offset_bits = 0;
      result->this_word = ws->get_next_word (ws, 0);
      result->next_word = ws->get_next_word (ws, 4);
      result->ws = ws;
  } else {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
          "failed to allocate new huffman coding state");
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

/** print huffman table */
void jbig2_dump_huffman_table(const Jbig2HuffmanTable *table)
{
    int i;
    int table_size = (1 << table->log_table_size);
    fprintf(stderr, "huffman table %p (log_table_size=%d, %d entries, entryies=%p):\n", table, table->log_table_size, table_size, table->entries);
    for (i = 0; i < table_size; i++) {
        fprintf(stderr, "%6d: PREFLEN=%d, RANGELEN=%d, ", i, table->entries[i].PREFLEN, table->entries[i].RANGELEN);
        if ( table->entries[i].flags & JBIG2_HUFFMAN_FLAGS_ISEXT ) {
            fprintf(stderr, "ext=%p", table->entries[i].u.ext_table);
        } else {
            fprintf(stderr, "RANGELOW=%d", table->entries[i].u.RANGELOW);
        }
        if ( table->entries[i].flags ) {
            int need_comma = 0;
            fprintf(stderr, ", flags=0x%x(", table->entries[i].flags);
            if ( table->entries[i].flags & JBIG2_HUFFMAN_FLAGS_ISOOB ) {
                fprintf(stderr, "OOB");
                need_comma = 1;
            }
            if ( table->entries[i].flags & JBIG2_HUFFMAN_FLAGS_ISLOW ) {
                if ( need_comma )
                    fprintf(stderr, ",");
                fprintf(stderr, "LOW");
                need_comma = 1;
            }
            if ( table->entries[i].flags & JBIG2_HUFFMAN_FLAGS_ISEXT ) {
                if ( need_comma )
                    fprintf(stderr, ",");
                fprintf(stderr, "EXT");
            }
            fprintf(stderr, ")");
        }
        fprintf(stderr, "\n");
    }
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
      if ((flags == (byte)-1) && (PREFLEN == (byte)-1) && (entry->u.RANGELOW == -1))
      {
          if (oob)
              *oob = -1;
          return -1;
      }

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
  int *LENCOUNT;
  int LENMAX = -1;
  const int lencountcount = 256;
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

  LENCOUNT = jbig2_new(ctx, int, lencountcount);
  if (LENCOUNT == NULL) {
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
      "couldn't allocate storage for huffman histogram");
    return NULL;
  }
  memset(LENCOUNT, 0, sizeof(int) * lencountcount);

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

  result = jbig2_new(ctx, Jbig2HuffmanTable, 1);
  if (result == NULL)
  {
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
        "couldn't allocate result storage in jbig2_build_huffman_table");
    return NULL;
  }
  result->log_table_size = log_table_size;
  entries = jbig2_new(ctx, Jbig2HuffmanEntry, max_j);
  if (entries == NULL)
  {
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
        "couldn't allocate entries storage in jbig2_build_huffman_table");
    return NULL;
  }
  /* fill now to catch missing JBIG2Globals later */
  memset(entries, 0xFF, sizeof(Jbig2HuffmanEntry)*max_j);
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
		jbig2_free(ctx->allocator, LENCOUNT);
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

  jbig2_free(ctx->allocator, LENCOUNT);

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

/* Routines to handle "code table segment (53)" */

/* return 'bitlen' bits from 'bitoffset' of 'data' */
static uint32_t
jbig2_table_read_bits(const byte *data, size_t *bitoffset, const int bitlen)
{
    uint32_t result = 0;
    uint32_t byte_offset = *bitoffset / 8;
    const int endbit = (*bitoffset & 7) + bitlen;
    const int n_proc_bytes = (endbit + 7) / 8;
    const int rshift = n_proc_bytes * 8 - endbit;
    int i;
    for (i = n_proc_bytes - 1; i >= 0; i--) {
        uint32_t d = data[byte_offset++];
        const int nshift = i * 8 - rshift;
        if (nshift > 0)
            d <<= nshift;
        else if (nshift < 0)
            d >>= -nshift;
        result |= d;
    }
    result &= ~(-1 << bitlen);
    *bitoffset += bitlen;
    return result;
}

/* Parse a code table segment, store Jbig2HuffmanParams in segment->result */
int
jbig2_table(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    Jbig2HuffmanParams *params = NULL;
    Jbig2HuffmanLine *line = NULL;

    segment->result = NULL;
    if (segment->data_length < 10)
        goto too_short;

    {
        /* B.2 1) (B.2.1) Code table flags */
        const int code_table_flags = segment_data[0];
        const int HTOOB = code_table_flags & 0x01; /* Bit 0: HTOOB */
        /* Bits 1-3: Number of bits used in code table line prefix size fields */
        const int HTPS  = (code_table_flags >> 1 & 0x07) + 1;
        /* Bits 4-6: Number of bits used in code table line range size fields */
        const int HTRS  = (code_table_flags >> 4 & 0x07) + 1;
        /* B.2 2) (B.2.2) The lower bound of the first table line in the encoded table */
        const int32_t HTLOW  = jbig2_get_int32(segment_data + 1);
        /* B.2 3) (B.2.3) One larger than the upeer bound of
           the last normal table line in the encoded table */
        const int32_t HTHIGH = jbig2_get_int32(segment_data + 5);
        /* estimated number of lines int this table, used for alloacting memory for lines */
        const size_t lines_max = (segment->data_length * 8 - HTPS * (HTOOB ? 3 : 2)) /
                                                        (HTPS + HTRS) + (HTOOB ? 3 : 2);
        /* points to a first table line data */
        const byte *lines_data = segment_data + 9;
        const size_t lines_data_bitlen = (segment->data_length - 9) * 8;    /* length in bit */
        /* bit offset: controls bit reading */
        size_t boffset = 0;
        /* B.2 4) */
        int32_t CURRANGELOW = HTLOW;
        int NTEMP = 0;

#ifdef JBIG2_DEBUG
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, 
            "DECODING USER TABLE... Flags: %d, HTOOB: %d, HTPS: %d, HTRS: %d, HTLOW: %d, HTHIGH: %d", 
            code_table_flags, HTOOB, HTPS, HTRS, HTLOW, HTHIGH);
#endif

        /* allocate HuffmanParams & HuffmanLine */
        params = jbig2_new(ctx, Jbig2HuffmanParams, 1);
        if (params == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                            "Could not allocate Huffman Table Parameter");
            goto error_exit;
        }
        line = jbig2_new(ctx, Jbig2HuffmanLine, lines_max);
        if (line == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                            "Could not allocate Huffman Table Lines");
            goto error_exit;
        }
        /* B.2 5) */
        while (CURRANGELOW < HTHIGH) {
            /* B.2 5) a) */
            if (boffset + HTPS >= lines_data_bitlen)
                goto too_short;
            line[NTEMP].PREFLEN  = jbig2_table_read_bits(lines_data, &boffset, HTPS);
            /* B.2 5) b) */
            if (boffset + HTRS >= lines_data_bitlen)
                goto too_short;
            line[NTEMP].RANGELEN = jbig2_table_read_bits(lines_data, &boffset, HTRS);
            /* B.2 5) c) */
            line[NTEMP].RANGELOW = CURRANGELOW;
            CURRANGELOW += (1 << line[NTEMP].RANGELEN);
            NTEMP++;
        }
        /* B.2 6), B.2 7) lower range table line */
        if (boffset + HTPS >= lines_data_bitlen)
            goto too_short;
        line[NTEMP].PREFLEN  = jbig2_table_read_bits(lines_data, &boffset, HTPS);
        line[NTEMP].RANGELEN = 32;
        line[NTEMP].RANGELOW = HTLOW - 1;
        NTEMP++;
        /* B.2 8), B.2 9) upper range table line */
        if (boffset + HTPS >= lines_data_bitlen)
            goto too_short;
        line[NTEMP].PREFLEN  = jbig2_table_read_bits(lines_data, &boffset, HTPS);
        line[NTEMP].RANGELEN = 32;
        line[NTEMP].RANGELOW = HTHIGH;
        NTEMP++;
        /* B.2 10) */
        if (HTOOB) {
            /* B.2 10) a), B.2 10) b) out-of-bound table line */
            if (boffset + HTPS >= lines_data_bitlen)
                goto too_short;
            line[NTEMP].PREFLEN  = jbig2_table_read_bits(lines_data, &boffset, HTPS);
            line[NTEMP].RANGELEN = 0;
            line[NTEMP].RANGELOW = 0;
            NTEMP++;
        }
        if (NTEMP != lines_max) {
            Jbig2HuffmanLine *new_line = jbig2_renew(ctx, line,
                Jbig2HuffmanLine, NTEMP);
            if ( new_line == NULL ) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                                "Could not reallocate Huffman Table Lines");
                goto error_exit;
            }
            line = new_line;
        }
        params->HTOOB   = HTOOB;
        params->n_lines = NTEMP;
        params->lines   = line;
        segment->result = params;

#ifdef JBIG2_DEBUG
        {
            int i;
            for (i = 0; i < NTEMP; i++) {
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, 
                    "Line: %d, PREFLEN: %d, RANGELEN: %d, RANGELOW: %d", 
                    i, params->lines[i].PREFLEN, params->lines[i].RANGELEN, params->lines[i].RANGELOW);
            }
        }
#endif
    }
    return 0;

too_short:
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "Segment too short");
error_exit:
    if (line != NULL) {
        jbig2_free(ctx->allocator, line);
    }
    if (params != NULL) {
        jbig2_free(ctx->allocator, params);
    }
    return -1;
}

/* free Jbig2HuffmanParams allocated by jbig2_huffman_table() */
void
jbig2_table_free(Jbig2Ctx *ctx, Jbig2HuffmanParams *params)
{
    if (params != NULL) {
        if (params->lines != NULL)
            jbig2_free(ctx->allocator, (void *)params->lines);
        jbig2_free(ctx->allocator, params);
    }
}

/* find a user supplied table used by 'segment' and by 'index' */
const Jbig2HuffmanParams *
jbig2_find_table(Jbig2Ctx *ctx, Jbig2Segment *segment, int index)
{
    int i, table_index = 0;

    for (i = 0; i < segment->referred_to_segment_count; i++) {
        const Jbig2Segment * const rsegment =
                jbig2_find_segment(ctx, segment->referred_to_segments[i]);
        if (rsegment && (rsegment->flags & 63) == 53) {
            if (table_index == index)
                return (const Jbig2HuffmanParams *)rsegment->result;
            ++table_index;
        }
    }
    return NULL;
}


#ifdef TEST
#include <stdio.h>

/* cc -g -o jbig2_huffman.test1 -DTEST jbig2_huffman.c .libs/libjbig2dec.a */

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

#ifdef TEST2
#include <stdio.h>

/* cc -g -o jbig2_huffman.test2 -DTEST2 jbig2_huffman.c .libs/libjbig2dec.a */

/* a decoding test of each line from each standard table */

/* test code for Table B.1 - Standard Huffman table A */
const int32_t test_output_A[] = {
    /* line 0, PREFLEN=1, RANGELEN=4, VAL=0..15, 0+VAL */
    0,      /* 0 0000 */
    1,      /* 0 0001 */
    14,     /* 0 1110 */
    15,     /* 0 1111 */
    /* line 1, PREFLEN=2, RANGELEN=8, VAL=16..271, 10+(VAL-16) */
    16,     /* 10 00000000 */
    17,     /* 10 00000001 */
    270,    /* 10 11111110 */
    271,    /* 10 11111111 */
    /* line 2, PREFLEN=3, RANGELEN=16, VAL=272..65807, 110+(VAL-272) */
    272,    /* 110 00000000 00000000 */
    273,    /* 110 00000000 00000001 */
    65806,  /* 110 11111111 11111110 */
    65807,  /* 110 11111111 11111111 */
    /* line 3, PREFLEN=3, RANGELEN=32, VAL=65808..INF, 111+(VAL-65808) */
    65808,  /* 111 00000000 00000000 00000000 00000000 */
    65809,  /* 111 00000000 00000000 00000000 00000001 */
};
const byte test_input_A[] = {
    /* 0000 0000 0101 1100 1111 1000 0000 0010 */
       0x00,     0x5c,     0xf8,     0x02,
    /* 0000 0001 1011 1111 1010 1111 1111 1100 */
       0x01,     0xbf,     0xaf,     0xfc,
    /* 0000 0000 0000 0001 1000 0000 0000 0000 */
       0x00,     0x01,     0x80,     0x00,
    /* 0111 0111 1111 1111 1111 0110 1111 1111 */
       0x77,     0xff,     0xf6,     0xff,
    /* 1111 1111 1110 0000 0000 0000 0000 0000 */
       0xff,     0xe0,     0x00,     0x00,
    /* 0000 0000 0001 1100 0000 0000 0000 0000 */
       0x00,     0x1c,     0x00,     0x00,
    /* 0000 0000 0000 01 */
       0x00,     0x04, 
};


/* test code for Table B.2 - Standard Huffman table B */
const int32_t test_output_B[] = {
    /* line 0, PREFLEN=1, RANGELEN=0, VAL=0, 0 */
    0,      /* 0 */
    /* line 1, PREFLEN=2, RANGELEN=0, VAL=1, 10 */
    1,      /* 10 */
    /* line 2, PREFLEN=3, RANGELEN=0, VAL=2, 110 */
    2,      /* 110 */
    /* line 3, PREFLEN=4, RANGELEN=3, VAL=3..10, 1110+(VAL-3) */
    3,      /* 1110 000 */
    4,      /* 1110 001 */
    9,      /* 1110 110 */
    10,     /* 1110 111 */
    /* line 4, PREFLEN=5, RANGELEN=6, VAL=11..74, 11110+(VAL-11) */
    11,     /* 11110 000000 */
    12,     /* 11110 000001 */
    73,     /* 11110 111110 */
    74,     /* 11110 111111 */
    /* line 5, PREFLEN=6, RANGELEN=32, VAL=75..INF, 111110+(VAL-75) */
    75,     /* 111110 00000000 00000000 00000000 00000000 */
    76,     /* 111110 00000000 00000000 00000000 00000001 */
    /* line 6, PREFLEN=6, VAL=OOB, 111111 */
    /*OOB*/ /* 111111 */
};
const byte test_input_B[] = {
    /* 0101 1011 1000 0111 0001 1110 1101 1101 */
       0x5b,     0x87,     0x1e,     0xdd,
    /* 1111 1100 0000 0111 1000 0001 1111 0111 */
       0xfc,     0x07,     0x81,     0xf7,
    /* 1101 1110 1111 1111 1110 0000 0000 0000 */
       0xde,     0xff,     0xe0,     0x00,
    /* 0000 0000 0000 0000 0000 1111 1000 0000 */
       0x00,     0x00,     0x0f,     0x80,
    /* 0000 0000 0000 0000 0000 0000 0111 1111 */
       0x00,     0x00,     0x00,     0x7f,
};

/* test code for Table B.3 - Standard Huffman table C */
const int32_t test_output_C[] = {
    /* line 0, PREFLEN=8, RANGELEN=8, VAL=-256..-1, 11111110+(VAL+256) */
    -256,   /* 11111110 00000000 */
    -255,   /* 11111110 00000001 */
    -2,     /* 11111110 11111110 */
    -1,     /* 11111110 11111111 */
    /* line 1, PREFLEN=1, RANGELEN=0, VAL=0, 0 */
    0,      /* 0 */
    /* line 2, PREFLEN=2, RANGELEN=0, VAL=1, 10 */
    1,      /* 10 */
    /* line 3, PREFLEN=3, RANGELEN=0, VAL=2, 110 */
    2,      /* 110 */
    /* line 4, PREFLEN=4, RANGELEN=3, VAL=3..10, 1110+(VAL-3) */
    3,      /* 1110 000 */
    4,      /* 1110 001 */
    9,      /* 1110 110 */
    10,     /* 1110 111 */
    /* line 5, PREFLEN=5, RANGELEN=6, VAL=11..74, 11110+(VAL-11) */
    11,     /* 11110 000000 */
    12,     /* 11110 000001 */
    73,     /* 11110 111110 */
    74,     /* 11110 111111 */
    /* line 6, PREFLEN=8, RANGELEN=32, VAL=-INF..-257, 11111111+(-257-VAL) */
    -257,   /* 11111111 00000000 00000000 00000000 00000000 */
    -258,   /* 11111111 00000000 00000000 00000000 00000001 */
    /* line 7, PREFLEN=7, RANGELEN=32, VAL=75..INF, 1111110+(VAL-75) */
    75,     /* 1111110 00000000 00000000 00000000 00000000 */
    76,     /* 1111110 00000000 00000000 00000000 00000001 */
    /* line 8, PREFLEN=6, VAL=OOB, 111110 */
    /*OOB*/ /* 111110 */
};
const byte test_input_C[] = {
    /* 1111 1110 0000 0000 1111 1110 0000 0001 */
       0xfe,     0x00,     0xfe,     0x01,
    /* 1111 1110 1111 1110 1111 1110 1111 1111 */
       0xfe,     0xfe,     0xfe,     0xff,
    /* 0101 1011 1000 0111 0001 1110 1101 1101 */
       0x5b,     0x87,     0x1e,     0xdd,
    /* 1111 1100 0000 0111 1000 0001 1111 0111 */
       0xfc,     0x07,     0x81,     0xf7,
    /* 1101 1110 1111 1111 1111 1100 0000 0000 */
       0xde,     0xff,     0xfc,     0x00,
    /* 0000 0000 0000 0000 0000 0011 1111 1100 */
       0x00,     0x00,     0x03,     0xfc,
    /* 0000 0000 0000 0000 0000 0000 0000 0111 */
       0x00,     0x00,     0x00,     0x07,
    /* 1111 0000 0000 0000 0000 0000 0000 0000 */
       0xf0,     0x00,     0x00,     0x00,
    /* 0000 0111 1110 0000 0000 0000 0000 0000 */
       0x07,     0xe0,     0x00,     0x00,
    /* 0000 0000 0001 1111 10 */
       0x00,     0x1f,     0x80,
};

/* test code for Table B.4 - Standard Huffman table D */
const int32_t test_output_D[] = {
    /* line 0, PREFLEN=1, RANGELEN=0, VAL=1, 0 */
    1,      /* 0 */
    /* line 1, PREFLEN=2, RANGELEN=0, VAL=2, 10 */
    2,      /* 10 */
    /* line 2, PREFLEN=3, RANGELEN=0, VAL=3, 110 */
    3,      /* 110 */
    /* line 3, PREFLEN=4, RANGELEN=3, VAL=4..11, 1110+(VAL-4) */
    4,      /* 1110 000 */
    5,      /* 1110 001 */
    10,     /* 1110 110 */
    11,     /* 1110 111 */
    /* line 4, PREFLEN=5, RANGELEN=6, VAL=12..75, 11110+(VAL-12) */
    12,     /* 11110 000000 */
    13,     /* 11110 000001 */
    74,     /* 11110 111110 */
    75,     /* 11110 111111 */
    /* line 5, PREFLEN=5, RANGELEN=32, VAL=76..INF, 11111+(VAL-76) */
    76,     /* 11111 00000000 00000000 00000000 00000000 */
    77,     /* 11111 00000000 00000000 00000000 00000001 */
};
const byte test_input_D[] = {
    /* 0101 1011 1000 0111 0001 1110 1101 1101 */
       0x5b,     0x87,     0x1e,     0xdd,
    /* 1111 1100 0000 0111 1000 0001 1111 0111 */
       0xfc,     0x07,     0x81,     0xf7,
    /* 1101 1110 1111 1111 1110 0000 0000 0000 */
       0xde,     0xff,     0xe0,     0x00,
    /* 0000 0000 0000 0000 0001 1111 0000 0000 */
       0x00,     0x00,     0x1f,     0x00,
    /* 0000 0000 0000 0000 0000 0001 */
       0x00,     0x00,     0x01,
};

/* test code for Table B.5 - Standard Huffman table E */
const int32_t test_output_E[] = {
    /* line 0, PREFLEN=7, RANGELEN=8, VAL=-255..0, 1111110+(VAL+255) */
    -255,   /* 1111110 00000000 */
    -254,   /* 1111110 00000001 */
    -1,     /* 1111110 11111110 */
    0,      /* 1111110 11111111 */
    /* line 1, PREFLEN=1, RANGELEN=0, VAL=1, 0 */
    1,      /* 0 */
    /* line 2, PREFLEN=2, RANGELEN=0, VAL=2, 10 */
    2,      /* 10 */
    /* line 3, PREFLEN=3, RANGELEN=0, VAL=3, 110 */
    3,      /* 110 */
    /* line 4, PREFLEN=4, RANGELEN=3, VAL=4..11, 1110+(VAL-4) */
    4,      /* 1110 000 */
    5,      /* 1110 001 */
    10,     /* 1110 110 */
    11,     /* 1110 111 */
    /* line 5, PREFLEN=5, RANGELEN=6, VAL=12..75, 11110+(VAL-12) */
    12,     /* 11110 000000 */
    13,     /* 11110 000001 */
    74,     /* 11110 111110 */
    75,     /* 11110 111111 */
    /* line 6, PREFLEN=7, RANGELEN=32, VAL=-INF..-256, 1111111+(-256-VAL) */
    -256,   /* 1111111 00000000 00000000 00000000 00000000 */
    -257,   /* 1111111 00000000 00000000 00000000 00000001 */
    /* line 6, PREFLEN=6, RANGELEN=32, VAL=76..INF, 111110+(VAL-76) */
    76,     /* 111110 00000000 00000000 00000000 00000000 */
    77,     /* 111110 00000000 00000000 00000000 00000001 */
};
const byte test_input_E[] = {
    /* 1111 1100 0000 0001 1111 1000 0000 0111 */
       0xfc,     0x01,     0xf8,     0x07,
    /* 1111 0111 1111 0111 1110 1111 1111 0101 */
       0xf7,     0xf7,     0xef,     0xf5,
    /* 1011 1000 0111 0001 1110 1101 1101 1111 */
       0xb8,     0x71,     0xed,     0xdf,
    /* 1100 0000 0111 1000 0001 1111 0111 1101 */
       0xc0,     0x78,     0x1f,     0x7d,
    /* 1110 1111 1111 1111 1000 0000 0000 0000 */
       0xef,     0xff,     0x80,     0x00,
    /* 0000 0000 0000 0000 0111 1111 0000 0000 */
       0x00,     0x00,     0x7f,     0x00,
    /* 0000 0000 0000 0000 0000 0001 1111 1000 */
       0x00,     0x00,     0x01,     0xf8,
    /* 0000 0000 0000 0000 0000 0000 0000 0011 */
       0x00,     0x00,     0x00,     0x03,
    /* 1110 0000 0000 0000 0000 0000 0000 0000 */
       0xe0,     0x00,     0x00,     0x00,
    /* 0001 */
       0x10,
};

/* test code for Table B.6 - Standard Huffman table F */
const int32_t test_output_F[] = {
    /* line 0, PREFLEN=5, RANGELEN=10, VAL=-2048..-1025, 11100+(VAL+2048) */
    -2048,  /* 11100 00000000 00 */
    -2047,  /* 11100 00000000 01 */
    -1026,  /* 11100 11111111 10 */
    -1025,  /* 11100 11111111 11 */
    /* line 1, PREFLEN=4, RANGELEN=9, VAL=-1024..-513, 1000+(VAL+1024) */
    -1024,  /* 1000 00000000 0 */
    -1023,  /* 1000 00000000 1 */
    -514,   /* 1000 11111111 0 */
    -513,   /* 1000 11111111 1 */
    /* line 2, PREFLEN=4, RANGELEN=8, VAL=-512..-257, 1001+(VAL+512) */
    -512,   /* 1001 00000000 */
    -511,   /* 1001 00000001 */
    -258,   /* 1001 11111110 */
    -257,   /* 1001 11111111 */
    /* line 3, PREFLEN=4, RANGELEN=7, VAL=-256..-129, 1010+(VAL+256) */
    -256,   /* 1010 0000000 */
    -255,   /* 1010 0000001 */
    -130,   /* 1010 1111110 */
    -129,   /* 1010 1111111 */
    /* line 4, PREFLEN=5, RANGELEN=6, VAL=-128..-65, 11101+(VAL+128) */
    -128,   /* 11101 000000 */
    -127,   /* 11101 000001 */
    -66,    /* 11101 111110 */
    -65,    /* 11101 111111 */
    /* line 5, PREFLEN=5, RANGELEN=5, VAL=-64..-33, 11110+(VAL+64) */
    -64,    /* 11110 00000 */
    -63,    /* 11110 00001 */
    -34,    /* 11110 11110 */
    -33,    /* 11110 11111 */
    /* line 6, PREFLEN=4, RANGELEN=5, VAL=-32..-1, 1011+(VAL+32) */
    -32,    /* 1011 00000 */
    -31,    /* 1011 00001 */
    -2,     /* 1011 11110 */
    -1,     /* 1011 11111 */
    /* line 7, PREFLEN=2, RANGELEN=7, VAL=0..127, 00+VAL */
    0,      /* 00 0000000 */
    1,      /* 00 0000001 */
    126,    /* 00 1111110 */
    127,    /* 00 1111111 */
    /* line 8, PREFLEN=3, RANGELEN=7, VAL=128..255, 010+(VAL-128) */
    128,    /* 010 0000000 */
    129,    /* 010 0000001 */
    254,    /* 010 1111110 */
    255,    /* 010 1111111 */
    /* line 9, PREFLEN=3, RANGELEN=8, VAL=256..511, 011+(VAL-256) */
    256,    /* 011 00000000 */
    257,    /* 011 00000001 */
    510,    /* 011 11111110 */
    511,    /* 011 11111111 */
    /* line 10, PREFLEN=4, RANGELEN=9, VAL=512..1023, 1100+(VAL-512) */
    512,    /* 1100 00000000 0 */
    513,    /* 1100 00000000 1 */
    1022,   /* 1100 11111111 0 */
    1023,   /* 1100 11111111 1 */
    /* line 11, PREFLEN=4, RANGELEN=10, VAL=1024..2047, 1101+(VAL-1024) */
    1024,   /* 1101 00000000 00 */
    1025,   /* 1101 00000000 01 */
    2046,   /* 1101 11111111 10 */
    2047,   /* 1101 11111111 11 */
    /* line 12, PREFLEN=6, RANGELEN=32, VAL=-INF..-2049, 111110+(-2049-VAL) */
    -2049,  /* 111110 00000000 00000000 00000000 00000000 */
    -2050,  /* 111110 00000000 00000000 00000000 00000001 */
    /* line 13, PREFLEN=6, RANGELEN=32, VAL=2048..INF, 111111+(VAL-2048) */
    2048,   /* 111111 00000000 00000000 00000000 00000000 */
    2049,   /* 111111 00000000 00000000 00000000 00000001 */
};
const byte test_input_F[] = {
    /* 1110 0000 0000 0001 1100 0000 0000 0111 */
       0xe0,     0x01,     0xc0,     0x07,
    /* 1001 1111 1111 0111 0011 1111 1111 1000 */
       0x9f,     0xf7,     0x3f,     0xf8,
    /* 0000 0000 0100 0000 0000 0110 0011 1111 */
       0x00,     0x40,     0x06,     0x3f,
    /* 1101 0001 1111 1111 1001 0000 0000 1001 */
       0xd1,     0xff,     0x90,     0x09,
    /* 0000 0001 1001 1111 1110 1001 1111 1111 */
       0x01,     0x9f,     0xe9,     0xff,
    /* 1010 0000 0001 0100 0000 0110 1011 1111 */
       0xa0,     0x14,     0x06,     0xbf,
    /* 0101 0111 1111 1110 1000 0001 1101 0000 */
       0x57,     0xfe,     0x81,     0xd0,
    /* 0111 1011 1111 0111 0111 1111 1111 0000 */
       0x7b,     0xf7,     0x7f,     0xf0,
    /* 0011 1100 0001 1111 0111 1011 1101 1111 */
       0x3c,     0x1f,     0x7b,     0xdf,
    /* 1011 0000 0101 1000 0110 1111 1101 0111 */
       0xb0,     0x58,     0x6f,     0xd7,
    /* 1111 0000 0000 0000 0000 0100 1111 1100 */
       0xf0,     0x00,     0x04,     0xfc,
    /* 0111 1111 0100 0000 0001 0000 0001 0101 */
       0x7f,     0x40,     0x10,     0x15,
    /* 1111 1001 0111 1111 0110 0000 0000 1100 */
       0xf9,     0x7f,     0x60,     0x0c,
    /* 0000 0101 1111 1111 0011 1111 1111 1100 */
       0x05,     0xff,     0x3f,     0xfc,
    /* 0000 0000 0110 0000 0000 0111 0011 1111 */
       0x00,     0x60,     0x07,     0x3f,
    /* 1101 1001 1111 1111 1101 0000 0000 0011 */
       0xd9,     0xff,     0xd0,     0x03,
    /* 0100 0000 0001 1101 1111 1111 1011 0111 */
       0x40,     0x1d,     0xff,     0xb7,
    /* 1111 1111 1111 1000 0000 0000 0000 0000 */
       0xff,     0xf8,     0x00,     0x00,
    /* 0000 0000 0000 0011 1110 0000 0000 0000 */
       0x00,     0x03,     0xe0,     0x00,
    /* 0000 0000 0000 0000 0001 1111 1100 0000 */
       0x00,     0x00,     0x1f,     0xc0,
    /* 0000 0000 0000 0000 0000 0000 0011 1111 */
       0x00,     0x00,     0x00,     0x3f,
    /* 0000 0000 0000 0000 0000 0000 0000 0001 */
       0x00,     0x00,     0x00,     0x01,
};

/* test code for Table B.7 - Standard Huffman table G */
const int32_t test_output_G[] = {
    /* line 0, PREFLEN=4, RANGELEN=9, VAL=-1024..-513, 1000+(VAL+1024) */
    -1024,  /* 1000 00000000 0 */
    -1023,  /* 1000 00000000 1 */
    -514,   /* 1000 11111111 0 */
    -513,   /* 1000 11111111 1 */
    /* line 1, PREFLEN=3, RANGELEN=8, VAL=-512..-257, 000+(VAL+512) */
    -512,   /* 000 00000000 */
    -511,   /* 000 00000001 */
    -258,   /* 000 11111110 */
    -257,   /* 000 11111111 */
    /* line 2, PREFLEN=4, RANGELEN=7, VAL=-256..-129, 1001+(VAL+256) */
    -256,   /* 1001 0000000 */
    -255,   /* 1001 0000001 */
    -130,   /* 1001 1111110 */
    -129,   /* 1001 1111111 */
    /* line 3, PREFLEN=5, RANGELEN=6, VAL=-128..-65, 11010+(VAL+128) */
    -128,   /* 11010 000000 */
    -127,   /* 11010 000001 */
    -66,    /* 11010 111110 */
    -65,    /* 11010 111111 */
    /* line 4, PREFLEN=5, RANGELEN=5, VAL=-64..-33, 11011+(VAL+64) */
    -64,    /* 11011 00000 */
    -63,    /* 11011 00001 */
    -34,    /* 11011 11110 */
    -33,    /* 11011 11111 */
    /* line 5, PREFLEN=4, RANGELEN=5, VAL=-32..-1, 1010+(VAL+32) */
    -32,    /* 1010 00000 */
    -31,    /* 1010 00001 */
    -2,     /* 1010 11110 */
    -1,     /* 1010 11111 */
    /* line 6, PREFLEN=4, RANGELEN=5, VAL=0..31, 1011+VAL */
    0,      /* 1011 00000 */
    1,      /* 1011 00001 */
    30,     /* 1011 11110 */
    31,     /* 1011 11111 */
    /* line 7, PREFLEN=5, RANGELEN=5, VAL=32..63, 11100+(VAL-32) */
    32,     /* 11100 00000 */
    33,     /* 11100 00001 */
    62,     /* 11100 11110 */
    63,     /* 11100 11111 */
    /* line 8, PREFLEN=5, RANGELEN=6, VAL=64..127, 11101+(VAL-64) */
    64,     /* 11101 000000 */
    65,     /* 11101 000001 */
    126,    /* 11101 111110 */
    127,    /* 11101 111111 */
    /* line 9, PREFLEN=4, RANGELEN=7, VAL=128..255, 1100+(VAL-128) */
    128,    /* 1100 0000000 */
    129,    /* 1100 0000001 */
    254,    /* 1100 1111110 */
    255,    /* 1100 1111111 */
    /* line 10, PREFLEN=3, RANGELEN=8, VAL=256..511, 001+(VAL-256) */
    256,    /* 001 00000000 */
    257,    /* 001 00000001 */
    510,    /* 001 11111110 */
    511,    /* 001 11111111 */
    /* line 11, PREFLEN=3, RANGELEN=9, VAL=512..1023, 010+(VAL-512) */
    512,    /* 010 00000000 0 */
    513,    /* 010 00000000 1 */
    1022,   /* 010 11111111 0 */
    1023,   /* 010 11111111 1 */
    /* line 12, PREFLEN=3, RANGELEN=10, VAL=1024..2047, 011+(VAL-1024) */
    1024,   /* 011 00000000 00 */
    1025,   /* 011 00000000 01 */
    2046,   /* 011 11111111 10 */
    2047,   /* 011 11111111 11 */
    /* line 13, PREFLEN=5, RANGELEN=32, VAL=-INF..-1025, 11110+(-1025-VAL) */
    -1025,  /* 11110 00000000 00000000 00000000 00000000 */
    -1026,  /* 11110 00000000 00000000 00000000 00000001 */
    /* line 14, PREFLEN=5, RANGELEN=32, VAL=2048..INF, 11111+(VAL-2048) */
    2048,   /* 11111 00000000 00000000 00000000 00000000 */
    2049,   /* 11111 00000000 00000000 00000000 00000001 */
};
const byte test_input_G[] = {
    /* 1000 0000 0000 0100 0000 0000 0110 0011 */
       0x80,     0x04,     0x00,     0x63,
    /* 1111 1101 0001 1111 1111 0000 0000 0000 */
       0xfd,     0x1f,     0xf0,     0x00,
    /* 0000 0000 0100 0111 1111 0000 1111 1111 */
       0x00,     0x47,     0xf0,     0xff,
    /* 1001 0000 0001 0010 0000 0110 0111 1111 */
       0x90,     0x12,     0x06,     0x7f,
    /* 0100 1111 1111 1101 0000 0001 1010 0000 */
       0x4f,     0xfd,     0x01,     0xa0,
    /* 0111 0101 1111 0110 1011 1111 1101 1000 */
       0x75,     0xf6,     0xbf,     0xd8,
    /* 0011 0110 0001 1101 1111 1011 0111 1111 */
       0x36,     0x1d,     0xfb,     0x7f,
    /* 1010 0000 0101 0000 0110 1011 1101 0101 */
       0xa0,     0x50,     0x6b,     0xd5,
    /* 1111 1011 0000 0101 1000 0110 1111 1101 */
       0xfb,     0x05,     0x86,     0xfd,
    /* 0111 1111 1110 0000 0011 1000 0001 1110 */
       0x7f,     0xe0,     0x38,     0x1e,
    /* 0111 1011 1001 1111 1110 1000 0001 1101 */
       0x7b,     0x9f,     0xe8,     0x1d,
    /* 0000 0111 1011 1111 0111 0111 1111 1100 */
       0x07,     0xbf,     0x77,     0xfc,
    /* 0000 0001 1000 0000 0111 0011 1111 0110 */
       0x01,     0x80,     0x73,     0xf6,
    /* 0111 1111 0010 0000 0000 0100 0000 0100 */
       0x7f,     0x20,     0x04,     0x04,
    /* 1111 1111 0001 1111 1111 0100 0000 0000 */
       0xff,     0x1f,     0xf4,     0x00,
    /* 0100 0000 0001 0101 1111 1110 0101 1111 */
       0x40,     0x15,     0xfe,     0x5f,
    /* 1111 0110 0000 0000 0011 0000 0000 0101 */
       0xf6,     0x00,     0x30,     0x05,
    /* 1111 1111 1100 1111 1111 1111 1111 0000 */
       0xff,     0xcf,     0xff,     0xf0,
    /* 0000 0000 0000 0000 0000 0000 0000 0111 */
       0x00,     0x00,     0x00,     0x07,
    /* 1000 0000 0000 0000 0000 0000 0000 0000 */
       0x80,     0x00,     0x00,     0x00,
    /* 0111 1110 0000 0000 0000 0000 0000 0000 */
       0x7e,     0x00,     0x00,     0x00,
    /* 0000 0001 1111 0000 0000 0000 0000 0000 */
       0x01,     0xf0,     0x00,     0x00,
    /* 0000 0000 0001 */
       0x00,     0x10,
};

/* test code for Table B.8 - Standard Huffman table H */
const int32_t test_output_H[] = {
    /* line 0, PREFLEN=8, RANGELEN=3, VAL=-15..-8, 11111100+(VAL+15) */
    -15,    /* 11111100 000 */
    -14,    /* 11111100 001 */
    -9,     /* 11111100 110 */
    -8,     /* 11111100 111 */
    /* line 1, PREFLEN=9, RANGELEN=1, VAL=-7..-6, 111111100+(VAL+7) */
    -7,     /* 111111100 0 */
    -6,     /* 111111100 1 */
    /* line 2, PREFLEN=8, RANGELEN=1, VAL=-5..-4, 11111101+(VAL+5) */
    -5,     /* 11111101 0 */
    -4,     /* 11111101 1 */
    /* line 3, PREFLEN=9, RANGELEN=0, VAL=-3, 111111101 */
    -3,     /* 111111101 */
    /* line 4, PREFLEN=7, RANGELEN=0, VAL=-2, 1111100 */
    -2,     /* 1111100 */
    /* line 5, PREFLEN=4, RANGELEN=0, VAL=-1, 1010 */
    -1,     /* 1010 */
    /* line 6, PREFLEN=2, RANGELEN=1, VAL=0..1, 00+VAL */
    0,      /* 00 0 */
    1,      /* 00 1 */
    /* line 7, PREFLEN=5, RANGELEN=0, VAL=2, 11010 */
    2,      /* 11010 */
    /* line 8, PREFLEN=6, RANGELEN=0, VAL=3, 111010 */
    3,      /* 111010 */
    /* line 9, PREFLEN=3, RANGELEN=4, VAL=4..19, 100+(VAL-4) */
    4,      /* 100 0000 */
    5,      /* 100 0001 */
    18,     /* 100 1110 */
    19,     /* 100 1111 */
    /* line 10, PREFLEN=6, RANGELEN=1, VAL=20..21, 111011+(VAL-20) */
    20,     /* 111011 0 */
    21,     /* 111011 1 */
    /* line 11, PREFLEN=4, RANGELEN=4, VAL=22..37, 1011+(VAL-22) */
    22,     /* 1011 0000 */
    23,     /* 1011 0001 */
    36,     /* 1011 1110 */
    37,     /* 1011 1111 */
    /* line 12, PREFLEN=4, RANGELEN=5, VAL=38..69, 1100+(VAL-38) */
    38,     /* 1100 00000 */
    39,     /* 1100 00001 */
    68,     /* 1100 11110 */
    69,     /* 1100 11111 */
    /* line 13, PREFLEN=5, RANGELEN=6, VAL=70..133, 11011+(VAL-70) */
    70,     /* 11011 000000 */
    71,     /* 11011 000001 */
    132,    /* 11011 111110 */
    133,    /* 11011 111111 */
    /* line 14, PREFLEN=5, RANGELEN=7, VAL=134..261, 11100+(VAL-134) */
    134,    /* 11100 0000000 */
    135,    /* 11100 0000001 */
    260,    /* 11100 1111110 */
    261,    /* 11100 1111111 */
    /* line 15, PREFLEN=6, RANGELEN=7, VAL=262..389, 111100+(VAL-262) */
    262,    /* 111100 0000000 */
    263,    /* 111100 0000001 */
    388,    /* 111100 1111110 */
    389,    /* 111100 1111111 */
    /* line 16, PREFLEN=7, RANGELEN=8, VAL=390..645, 1111101+(VAL-390) */
    390,    /* 1111101 00000000 */
    391,    /* 1111101 00000001 */
    644,    /* 1111101 11111110 */
    645,    /* 1111101 11111111 */
    /* line 17, PREFLEN=6, RANGELEN=10, VAL=646..1669, 111101+(VAL-646) */
    646,    /* 111101 00000000 00 */
    647,    /* 111101 00000000 01 */
    1668,   /* 111101 11111111 10 */
    1669,   /* 111101 11111111 11 */
    /* line 18, PREFLEN=9, RANGELEN=32, VAL=-INF..-16, 111111110+(-16-VAL) */
    -16,    /* 111111110 00000000 00000000 00000000 00000000 */
    -17,    /* 111111110 00000000 00000000 00000000 00000001 */
    /* line 19, PREFLEN=9, RANGELEN=32, VAL=1670..INF, 111111111+(VAL-1670) */
    1670,   /* 111111111 00000000 00000000 00000000 00000000 */
    1671,   /* 111111111 00000000 00000000 00000000 00000001 */
    /* line 20, PREFLEN=2, VAL=OOB, 01 */
    /*OOB*/ /* 01 */
};
const byte test_input_H[] = {
    /* 1111 1100  0001 1111 1000 0111 1111 0011 */
       0xfc,     0x1f,     0x87,     0xf3,
    /* 0111 1110  0111 1111 1110 0011 1111 1001 */
       0x7e,     0x7f,     0xe3,     0xf9,
    /* 1111 1101  0111 1110 1111 1111 1011 1111 */
       0xfd,     0x7e,     0xff,     0xbf,
    /* 0010 1000  0001 1101 0111 0101 0000 0010 */
       0x28,     0x1d,     0x75,     0x02,
    /* 0000 1100  1110 1001 1111 1101 1011 1011 */
       0x0c,     0xe9,     0xfd,     0xbb,
    /* 1101 1000  0101 1000 1101 1111 0101 1111 */
       0xd8,     0x58,     0xdf,     0x5f,
    /* 1110 0000  0011 0000 0011 1001 1110 1100 */
       0xe0,     0x30,     0x39,     0xec,
    /* 1111 1110  1100 0000 1101 1000 0011 1011 */
       0xfe,     0xc0,     0xd8,     0x3b,
    /* 1111 1011  0111 1111 1111 0000 0000 0111 */
       0xfb,     0x7f,     0xf0,     0x07,
    /* 0000 0000  1111 0011 1111 0111 0011 1111 */
       0x00,     0xf3,     0xf7,     0x3f,
    /* 1111 1000  0000 0011 1100 0000 0011 1110 */
       0xf8,     0x03,     0xc0,     0x3e,
    /* 0111 1110  1111 0011 1111 1111 1101 0000 */
       0x7e,     0xf3,     0xff,     0xd0,
    /* 0000 1111  1010 0000 0011 1111 0111 1111 */
       0x0f,     0xa0,     0x3f,     0x7f,
    /* 1011 1110  1111 1111 1111 1010 0000 0000 */
       0xbe,     0xff,     0xfa,     0x00,
    /* 0111 1010  0000 0000 1111 1011 1111 1111 */
       0x7a,     0x00,     0xfb,     0xff,
    /* 0111 1011  1111 1111 1111 1111 1000 0000 */
       0x7b,     0xff,     0xff,     0x80,
    /* 0000 0000  0000 0000 0000 0000 0011 1111 */
       0x00,     0x00,     0x00,     0x3f,
    /* 1100 0000  0000 0000 0000 0000 0000 0000 */
       0xc0,     0x00,     0x00,     0x00,
    /* 0011 1111  1111 0000 0000 0000 0000 0000 */
       0x3f,     0xf0,     0x00,     0x00,
    /* 0000 0000  0000 1111 1111 1000 0000 0000 */
       0x00,     0x0f,     0xf8,     0x00,
    /* 0000 0000  0000 0000 0000 101 */
       0x00,     0x00,     0x0a,
};

/* test code for Table B.9 - Standard Huffman table I */
const int32_t test_output_I[] = {
    /* line 0, PREFLEN=8, RANGELEN=4, VAL=-31..-16, 11111100+(VAL+31) */
    -31,    /* 11111100 0000 */
    -30,    /* 11111100 0001 */
    -17,    /* 11111100 1110 */
    -16,    /* 11111100 1111 */
    /* line 1, PREFLEN=9, RANGELEN=2, VAL=-15..-12, 111111100+(VAL+15) */
    -15,    /* 111111100 00 */
    -14,    /* 111111100 01 */
    -13,    /* 111111100 10 */
    -12,    /* 111111100 11 */
    /* line 2, PREFLEN=8, RANGELEN=2, VAL=-11..-8, 11111101+(VAL+11) */
    -11,    /* 11111101 00 */
    -10,    /* 11111101 01 */
    -9,     /* 11111101 10 */
    -8,     /* 11111101 11 */
    /* line 3, PREFLEN=9, RANGELEN=1, VAL=-7..-6, 111111101+(VAL+7) */
    -7,     /* 111111101 0 */
    -6,     /* 111111101 1 */
    /* line 4, PREFLEN=7, RANGELEN=1, VAL=-5..-4, 1111100+(VAL+5) */
    -5,     /* 1111100 0 */
    -4,     /* 1111100 1 */
    /* line 5, PREFLEN=4, RANGELEN=1, VAL=-3..-2, 1010+(VAL+3) */
    -3,     /* 1010 0 */
    -2,     /* 1010 1 */
    /* line 6, PREFLEN=3, RANGELEN=1, VAL=-1..0, 010+(VAL+1) */
    -1,     /* 010 0 */
    0,      /* 010 1 */
    /* line 7, PREFLEN=3, RANGELEN=1, VAL=1..2, 011+(VAL-1) */
    1,      /* 011 0 */
    2,      /* 011 1 */
    /* line 8, PREFLEN=5, RANGELEN=1, VAL=3..4, 11010+(VAL-3) */
    3,      /* 11010 0 */
    4,      /* 11010 1 */
    /* line 9, PREFLEN=6, RANGELEN=1, VAL=5..6, 111010+(VAL-5) */
    5,      /* 111010 0 */
    6,      /* 111010 1 */
    /* line 10, PREFLEN=3, RANGELEN=5, VAL=7..38, 100+(VAL-7) */
    7,      /* 100 00000 */
    8,      /* 100 00001 */
    37,     /* 100 11110 */
    38,     /* 100 11111 */
    /* line 11, PREFLEN=6, RANGELEN=2, VAL=39..42, 111011+(VAL-39) */
    39,     /* 111011 00 */
    40,     /* 111011 01 */
    41,     /* 111011 10 */
    42,     /* 111011 11 */
    /* line 12, PREFLEN=4, RANGELEN=5, VAL=43..74, 1011+(VAL-43) */
    43,     /* 1011 00000 */
    44,     /* 1011 00001 */
    73,     /* 1011 11110 */
    74,     /* 1011 11111 */
    /* line 13, PREFLEN=4, RANGELEN=6, VAL=75..138, 1100+(VAL-75) */
    75,     /* 1100 000000 */
    76,     /* 1100 000001 */
    137,    /* 1100 111110 */
    138,    /* 1100 111111 */
    /* line 14, PREFLEN=5, RANGELEN=7, VAL=139..266, 11011+(VAL-139) */
    139,    /* 11011 0000000 */
    140,    /* 11011 0000001 */
    265,    /* 11011 1111110 */
    266,    /* 11011 1111111 */
    /* line 15, PREFLEN=5, RANGELEN=8, VAL=267..522, 11100+(VAL-267) */
    267,    /* 11100 00000000 */
    268,    /* 11100 00000001 */
    521,    /* 11100 11111110 */
    522,    /* 11100 11111111 */
    /* line 16, PREFLEN=6, RANGELEN=8, VAL=523..778, 111100+(VAL-523) */
    523,    /* 111100 00000000 */
    524,    /* 111100 00000001 */
    777,    /* 111100 11111110 */
    778,    /* 111100 11111111 */
    /* line 17, PREFLEN=7, RANGELEN=9, VAL=779..1290, 1111101+(VAL-779) */
    779,    /* 1111101 00000000 0 */
    780,    /* 1111101 00000000 1 */
    1289,   /* 1111101 11111111 0 */
    1290,   /* 1111101 11111111 1 */
    /* line 18, PREFLEN=6, RANGELEN=11, VAL=1291..3338, 111101+(VAL-1291) */
    1291,   /* 111101 00000000 000 */
    1292,   /* 111101 00000000 001 */
    3337,   /* 111101 11111111 110 */
    3338,   /* 111101 11111111 111 */
    /* line 19, PREFLEN=9, RANGELEN=32, VAL=-INF..-32, 111111110+(-32-VAL) */
    -32,    /* 111111110 00000000 00000000 00000000 00000000 */
    -33,    /* 111111110 00000000 00000000 00000000 00000001 */
    /* line 20, PREFLEN=9, RANGELEN=32, VAL=3339..INF, 111111111+(VAL-3339) */
    3339,   /* 111111111 00000000 00000000 00000000 00000000 */
    3340,   /* 111111111 00000000 00000000 00000000 00000001 */
    /* line 21, PREFLEN=2, VAL=OOB, 00 */
    /*OOB*/ /* 00 */
};
const byte test_input_I[] = {
    /* 1111 1100 0000 1111 1100 0001 1111 1100 */
       0xfc,     0x0f,     0xc1,     0xfc,
    /* 1110 1111 1100 1111 1111 1110 0001 1111 */
       0xef,     0xcf,     0xfe,     0x1f,
    /* 1100 0111 1111 1001 0111 1111 0011 1111 */
       0xc7,     0xf9,     0x7f,     0x3f,
    /* 1101 0011 1111 0101 1111 1101 1011 1111 */
       0xd3,     0xf5,     0xfd,     0xbf,
    /* 0111 1111 1110 1011 1111 1011 1111 1000 */
       0x7f,     0xeb,     0xfb,     0xf8,
    /* 1111 1001 1010 0101 0101 0001 0101 1001 */
       0xf9,     0xa5,     0x51,     0x59,
    /* 1111 0100 1101 0111 1010 0111 0101 1000 */
       0xf4,     0xd7,     0xa7,     0x58,
    /* 0000 1000 0001 1001 1110 1001 1111 1110 */
       0x08,     0x19,     0xe9,     0xfe,
    /* 1100 1110 1101 1110 1110 1110 1111 1011 */
       0xce,     0xde,     0xee,     0xfb,
    /* 0000 0101 1000 0110 1111 1101 0111 1111 */
       0x05,     0x86,     0xfd,     0x7f,
    /* 1100 0000 0011 0000 0001 1100 1111 1011 */
       0xc0,     0x30,     0x1c,     0xfb,
    /* 0011 1111 1101 1000 0000 1101 1000 0001 */
       0x3f,     0xd8,     0x0d,     0x81,
    /* 1101 1111 1110 1101 1111 1111 1110 0000 */
       0xdf,     0xed,     0xff,     0xe0,
    /* 0000 0111 0000 0000 0111 1001 1111 1101 */
       0x07,     0x00,     0x79,     0xfd,
    /* 1100 1111 1111 1111 0000 0000 0011 1100 */
       0xcf,     0xff,     0x00,     0x3c,
    /* 0000 0001 1111 0011 1111 1011 1100 1111 */
       0x01,     0xf3,     0xfb,     0xcf,
    /* 1111 1111 1010 0000 0000 1111 1010 0000 */
       0xff,     0xa0,     0x0f,     0xa0,
    /* 0001 1111 1011 1111 1110 1111 1011 1111 */
       0x1f,     0xbf,     0xef,     0xbf,
    /* 1111 1111 0100 0000 0000 0111 1010 0000 */
       0xff,     0x40,     0x07,     0xa0,
    /* 0000 0111 1101 1111 1111 1101 1110 1111 */
       0x07,     0xdf,     0xfd,     0xef,
    /* 1111 1111 1111 1111 0000 0000 0000 0000 */
       0xff,     0xff,     0x00,     0x00,
    /* 0000 0000 0000 0000 0111 1111 1000 0000 */
       0x00,     0x00,     0x7f,     0x80,
    /* 0000 0000 0000 0000 0000 0000 0111 1111 */
       0x00,     0x00,     0x00,     0x7f,
    /* 1110 0000 0000 0000 0000 0000 0000 0000 */
       0xe0,     0x00,     0x00,     0x00,
    /* 0001 1111 1111 0000 0000 0000 0000 0000 */
       0x1f,     0xf0,     0x00,     0x00,
    /* 0000 0000 0001 00 */
       0x00,     0x10,
};

/* test code for Table B.10 - Standard Huffman table J */
const int32_t test_output_J[] = {
    /* line 0, PREFLEN=7, RANGELEN=4, VAL=-21..-6, 1111010+(VAL+21) */
    -21,    /* 1111010 0000 */
    -20,    /* 1111010 0001 */
    -7,     /* 1111010 1110 */
    -6,     /* 1111010 1111 */
    /* line 1, PREFLEN=8, RANGELEN=0, VAL=-5, 11111100 */
    -5,     /* 11111100 */
    /* line 2, PREFLEN=7, RANGELEN=0, VAL=-5, 1111011 */
    -4,     /* 1111011 */
    /* line 3, PREFLEN=5, RANGELEN=0, VAL=-3, 11000 */
    -3,     /* 11000 */
    /* line 4, PREFLEN=2, RANGELEN=2, VAL=-2..1, 00+(VAL+2) */
    -2,     /* 00 00 */
    -1,     /* 00 01 */
    0,      /* 00 10 */
    1,      /* 00 11 */
    /* line 5, PREFLEN=5, RANGELEN=0, VAL=2, 11001 */
    2,      /* 11001 */
    /* line 6, PREFLEN=6, RANGELEN=0, VAL=3, 110110 */
    3,      /* 110110 */
    /* line 7, PREFLEN=7, RANGELEN=0, VAL=4, 1111100 */
    4,      /* 1111100 */
    /* line 8, PREFLEN=8, RANGELEN=0, VAL=5, 11111101 */
    5,      /* 11111101 */
    /* line 9, PREFLEN=2, RANGELEN=6, VAL=6..69, 01+(VAL-6) */
    6,      /* 01 000000 */
    7,      /* 01 000001 */
    68,     /* 01 111110 */
    69,     /* 01 111111 */
    /* line 8, PREFLEN=5, RANGELEN=5, VAL=70..101, 11010+(VAL-70) */
    70,     /* 11010 00000 */
    71,     /* 11010 00001 */
    100,    /* 11010 11110 */
    101,    /* 11010 11111 */
    /* line 9, PREFLEN=6, RANGELEN=5, VAL=102..133, 110111+(VAL-102) */
    102,    /* 110111 00000 */
    103,    /* 110111 00001 */
    132,    /* 110111 11110 */
    133,    /* 110111 11111 */
    /* line 10, PREFLEN=6, RANGELEN=6, VAL=134..197, 111000+(VAL-134) */
    134,    /* 111000 000000 */
    135,    /* 111000 000001 */
    196,    /* 111000 111110 */
    197,    /* 111000 111111 */
    /* line 11, PREFLEN=6, RANGELEN=7, VAL=198..325, 111001+(VAL-198) */
    198,    /* 111001 0000000 */
    199,    /* 111001 0000001 */
    324,    /* 111001 1111110 */
    325,    /* 111001 1111111 */
    /* line 12, PREFLEN=6, RANGELEN=8, VAL=326..581, 111010+(VAL-326) */
    326,    /* 111010 00000000 */
    327,    /* 111010 00000001 */
    580,    /* 111010 11111110 */
    581,    /* 111010 11111111 */
    /* line 13, PREFLEN=6, RANGELEN=9, VAL=582..1093, 111011+(VAL-582) */
    582,    /* 111011 00000000 0 */
    583,    /* 111011 00000000 1 */
    1092,   /* 111011 11111111 0 */
    1093,   /* 111011 11111111 1 */
    /* line 14, PREFLEN=6, RANGELEN=10, VAL=1094..2117, 111100+(VAL-1094) */
    1094,   /* 111100 00000000 00 */
    1095,   /* 111100 00000000 01 */
    2116,   /* 111100 11111111 10 */
    2117,   /* 111100 11111111 11 */
    /* line 15, PREFLEN=7, RANGELEN=11, VAL=2118..4165, 1111101+(VAL-2118) */
    2118,   /* 1111101 00000000 000 */
    2119,   /* 1111101 00000000 001 */
    4164,   /* 1111101 11111111 110 */
    4165,   /* 1111101 11111111 111 */
    /* line 16, PREFLEN=8, RANGELEN=32, VAL=-INF..-22, 11111110+(-22-VAL) */
    -22,    /* 11111110 00000000 00000000 00000000 00000000 */
    -23,    /* 11111110 00000000 00000000 00000000 00000001 */
    /* line 17, PREFLEN=8, RANGELEN=32, VAL=4166..INF, 11111111+(VAL-4166) */
    4166,   /* 11111111 00000000 00000000 00000000 00000000 */
    4167,   /* 11111111 00000000 00000000 00000000 00000001 */
    /* line 8, PREFLEN=2, VAL=OOB, 10 */
    /*OOB*/ /* 10 */
};
const byte test_input_J[] = {
    /* 1111 0100 0001 1110 1000 0111 1101 0111 */
       0xf4,     0x1e,     0x87,     0xd7,
    /* 0111 1010 1111 1111 1100 1111 0111 1000 */
       0x7a,     0xff,     0xcf,     0x78,
    /* 0000 0001 0010 0011 1100 1110 1101 1111 */
       0x01,     0x23,     0xce,     0xdf,
    /* 0011 1111 0101 0000 0001 0000 0101 1111 */
       0x3f,     0x50,     0x10,     0x5f,
    /* 1001 1111 1111 0100 0000 1101 0000 0111 */
       0x9f,     0xf4,     0x0d,     0x07,
    /* 0101 1110 1101 0111 1111 0111 0000 0110 */
       0x5e,     0xd7,     0xf7,     0x06,
    /* 1110 0001 1101 1111 1101 1011 1111 1111 */
       0xe1,     0xdf,     0xdb,     0xff,
    /* 1000 0000 0011 1000 0000 0111 1000 1111 */
       0x80,     0x38,     0x07,     0x8f,
    /* 1011 1000 1111 1111 1001 0000 0001 1100 */
       0xb8,     0xff,     0x90,     0x1c,
    /* 1000 0001 1110 0111 1111 0111 0011 1111 */
       0x81,     0xe7,     0xf7,     0x3f,
    /* 1111 1010 0000 0000 1110 1000 0000 0111 */
       0xfa,     0x00,     0xe8,     0x07,
    /* 1010 1111 1110 1110 1011 1111 1111 1011 */
       0xaf,     0xee,     0xbf,     0xfb,
    /* 0000 0000 0111 0110 0000 0001 1110 1111 */
       0x00,     0x76,     0x01,     0xef,
    /* 1111 1101 1101 1111 1111 1111 1100 0000 */
       0xfd,     0xdf,     0xff,     0xc0,
    /* 0000 0011 1100 0000 0000 0111 1100 1111 */
       0x03,     0xc0,     0x07,     0xcf,
    /* 1111 1011 1100 1111 1111 1111 1110 1000 */
       0xfb,     0xcf,     0xff,     0xe8,
    /* 0000 0000 1111 1010 0000 0000 0111 1110 */
       0x00,     0xfa,     0x00,     0x7e,
    /* 1111 1111 1110 1111 1011 1111 1111 1111 */
       0xff,     0xef,     0xbf,     0xff,
    /* 1111 1000 0000 0000 0000 0000 0000 0000 */
       0xf8,     0x00,     0x00,     0x00,
    /* 0000 0011 1111 1000 0000 0000 0000 0000 */
       0x03,     0xf8,     0x00,     0x00,
    /* 0000 0000 0000 0111 1111 1100 0000 0000 */
       0x00,     0x07,     0xfc,     0x00,
    /* 0000 0000 0000 0000 0000 0011 1111 1100 */
       0x00,     0x00,     0x03,     0xfc,
    /* 0000 0000 0000 0000 0000 0000 0000 0110 */
       0x00,     0x00,     0x00,     0x06,
};

/* test code for Table B.11 - Standard Huffman table K */
const int32_t test_output_K[] = {
    /* line 0, PREFLEN=1, RANGELEN=0, VAL=1, 0 */
    1,      /* 0 */
    /* line 1, PREFLEN=2, RANGELEN=1, VAL=2..3, 10+(VAL-2) */
    2,      /* 10 0 */
    3,      /* 10 1 */
    /* line 2, PREFLEN=4, RANGELEN=0, VAL=4, 1100 */
    4,      /* 1100 */
    /* line 3, PREFLEN=4, RANGELEN=1, VAL=5..6, 1101+(VAL-5) */
    5,      /* 1101 0 */
    6,      /* 1101 1 */
    /* line 4, PREFLEN=5, RANGELEN=1, VAL=7..8, 11100+(VAL-7) */
    7,      /* 11100 0 */
    8,      /* 11100 1 */
    /* line 5, PREFLEN=5, RANGELEN=2, VAL=9..12, 11101+(VAL-9) */
    9,      /* 11101 00 */
    10,     /* 11101 01 */
    11,     /* 11101 10 */
    12,     /* 11101 11 */
    /* line 6, PREFLEN=6, RANGELEN=2, VAL=13..16, 111100+(VAL-13) */
    13,     /* 111100 00 */
    14,     /* 111100 01 */
    15,     /* 111100 10 */
    16,     /* 111100 11 */
    /* line 7, PREFLEN=7, RANGELEN=2, VAL=17..20, 1111010+(VAL-17) */
    17,     /* 1111010 00 */
    18,     /* 1111010 01 */
    19,     /* 1111010 10 */
    20,     /* 1111010 11 */
    /* line 8, PREFLEN=7, RANGELEN=3, VAL=21..28, 1111011+(VAL-21) */
    21,     /* 1111011 000 */
    22,     /* 1111011 001 */
    27,     /* 1111011 110 */
    28,     /* 1111011 111 */
    /* line 9, PREFLEN=7, RANGELEN=4, VAL=29..44, 1111100+(VAL-29) */
    29,     /* 1111100 0000 */
    30,     /* 1111100 0001 */
    43,     /* 1111100 1110 */
    44,     /* 1111100 1111 */
    /* line 10, PREFLEN=7, RANGELEN=5, VAL=45..76, 1111101+(VAL-45) */
    45,     /* 1111101 00000 */
    46,     /* 1111101 00001 */
    75,     /* 1111101 11110 */
    76,     /* 1111101 11111 */
    /* line 11, PREFLEN=7, RANGELEN=6, VAL=77..140, 1111110+(VAL-77) */
    77,     /* 1111110 000000 */
    78,     /* 1111110 000001 */
    139,    /* 1111110 111110 */
    140,    /* 1111110 111111 */
    /* line 12, PREFLEN=7, RANGELEN=32, VAL=141..INF, 1111111+(VAL-141) */
    141,    /* 1111111 00000000 00000000 00000000 00000000 */
    142,    /* 1111111 00000000 00000000 00000000 00000001 */
};
const byte test_input_K[] = {
    /* 0100 1011 1001 1010 1101 1111 0001 1100 */
       0x4b,     0x9a,     0xdf,     0x1c,
    /* 1111 0100 1110 1011 1101 1011 1011 1111 */
       0xf4,     0xeb,     0xdb,     0xbf,
    /* 1000 0111 1000 1111 1001 0111 1001 1111 */
       0x87,     0x8f,     0x97,     0x9f,
    /* 1010 0011 1101 0011 1110 1010 1111 0101 */
       0xa3,     0xd3,     0xea,     0xf5,
    /* 1111 1011 0001 1110 1100 1111 1011 1101 */
       0xfb,     0x1e,     0xcf,     0xbd,
    /* 1110 1111 1111 1100 0000 1111 1000 0011 */
       0xef,     0xfc,     0x0f,     0x83,
    /* 1111 0011 1011 1110 0111 1111 1101 0000 */
       0xf3,     0xbe,     0x7f,     0xd0,
    /* 0111 1101 0000 1111 1101 1111 0111 1101 */
       0x7d,     0x0f,     0xdf,     0x7d,
    /* 1111 1111 1110 0000 0011 1111 0000 0011 */
       0xff,     0xe0,     0x3f,     0x03,
    /* 1111 1011 1110 1111 1101 1111 1111 1111 */
       0xfb,     0xef,     0xdf,     0xff,
    /* 0000 0000 0000 0000 0000 0000 0000 0000 */
       0x00,     0x00,     0x00,     0x00,
    /* 1111 1110 0000 0000 0000 0000 0000 0000 */
       0xfe,     0x00,     0x00,     0x00,
    /* 0000 001 */
       0x02,
};

/* test code for Table B.12 - Standard Huffman table L */
const int32_t test_output_L[] = {
    /* line 0, PREFLEN=1, RANGELEN=0, VAL=1, 0 */
    1,      /* 0 */
    /* line 1, PREFLEN=2, RANGELEN=0, VAL=2, 10 */
    2,      /* 10 */
    /* line 2, PREFLEN=3, RANGELEN=1, VAL=3..4, 110+(VAL-3) */
    3,      /* 110 0 */
    4,      /* 110 1 */
    /* line 3, PREFLEN=5, RANGELEN=0, VAL=5, 11100 */
    5,      /* 11100 */
    /* line 4, PREFLEN=5, RANGELEN=1, VAL=6..7, 11101+(VAL-7) */
    6,      /* 11101 0 */
    7,      /* 11101 1 */
    /* line 5, PREFLEN=6, RANGELEN=1, VAL=8..9, 111100+(VAL-8) */
    8,      /* 111100 0 */
    9,      /* 111100 1 */
    /* line 6, PREFLEN=7, RANGELEN=0, VAL=10, 1111010 */
    10,     /* 1111010 */
    /* line 7, PREFLEN=7, RANGELEN=1, VAL=11..12, 1111011+(VAL-11) */
    11,     /* 1111011 0 */
    12,     /* 1111011 1 */
    /* line 8, PREFLEN=7, RANGELEN=2, VAL=13..16, 1111100+(VAL-13) */
    13,     /* 1111100 00 */
    14,     /* 1111100 01 */
    15,     /* 1111100 10 */
    16,     /* 1111100 11 */
    /* line 9, PREFLEN=7, RANGELEN=3, VAL=17..24, 1111101+(VAL-17) */
    17,     /* 1111101 000 */
    18,     /* 1111101 001 */
    23,     /* 1111101 110 */
    24,     /* 1111101 111 */
    /* line 10, PREFLEN=7, RANGELEN=4, VAL=25..40, 1111110+(VAL-25) */
    25,     /* 1111110 0000 */
    26,     /* 1111110 0001 */
    39,     /* 1111110 1110 */
    40,     /* 1111110 1111 */
    /* line 11, PREFLEN=8, RANGELEN=5, VAL=41..72, 11111110+(VAL-41) */
    41,     /* 11111110 00000 */
    42,     /* 11111110 00001 */
    71,     /* 11111110 11110 */
    72,     /* 11111110 11111 */
    /* line 12, PREFLEN=8, RANGELEN=32, VAL=73..INF, 11111111+(VAL-73) */
    73,     /* 11111111 00000000 00000000 00000000 00000000 */
    74,     /* 11111111 00000000 00000000 00000000 00000001 */
};
const byte test_input_L[] = {
    /* 0101 1001 1011 1100 1110 1011 1011 1111 */
       0x59,     0xbc,     0xeb,     0xbf,
    /* 0001 1110 0111 1101 0111 1011 0111 1011 */
       0x1e,     0x7d,     0x7b,     0x7b,
    /* 1111 1100 0011 1110 0011 1111 0010 1111 */
       0xfc,     0x3e,     0x3f,     0x2f,
    /* 1001 1111 1101 0001 1111 0100 1111 1101 */
       0x9f,     0xd1,     0xf4,     0xfd,
    /* 1101 1111 0111 1111 1110 0000 1111 1100 */
       0xdf,     0x7f,     0xe0,     0xfc,
    /* 0011 1111 1011 1011 1111 0111 1111 1111 */
       0x3f,     0xbb,     0xf7,     0xff,
    /* 0000 0011 1111 1000 0011 1111 1101 1110 */
       0x03,     0xf8,     0x3f,     0xde,
    /* 1111 1110 1111 1111 1111 1000 0000 0000 */
       0xfe,     0xff,     0xf8,     0x00,
    /* 0000 0000 0000 0000 0000 0111 1111 1000 */
       0x00,     0x00,     0x07,     0xf8,
    /* 0000 0000 0000 0000 0000 0000 0000 1 */
       0x00,     0x00,     0x00,     0x08,
};

/* test code for Table B.13 - Standard Huffman table M */
const int32_t test_output_M[] = {
    /* line 0, PREFLEN=1, RANGELEN=0, VAL=1, 0 */
    1,      /* 0 */
    /* line 1, PREFLEN=3, RANGELEN=0, VAL=2, 100 */
    2,      /* 100 */
    /* line 2, PREFLEN=3, RANGELEN=0, VAL=3, 1100 */
    3,      /* 1100 */
    /* line 3, PREFLEN=5, RANGELEN=0, VAL=4, 11100 */
    4,      /* 11100 */
    /* line 4, PREFLEN=4, RANGELEN=1, VAL=5..6, 1101+(VAL-5) */
    5,      /* 1101 0 */
    6,      /* 1101 1 */
    /* line 5, PREFLEN=3, RANGELEN=3, VAL=7..14, 101+(VAL-7) */
    7,      /* 101 000 */
    8,      /* 101 001 */
    13,     /* 101 110 */
    14,     /* 101 111 */
    /* line 6, PREFLEN=6, RANGELEN=1, VAL=15..16, 111010+(VAL-15) */
    15,     /* 111010 0 */
    16,     /* 111010 1 */
    /* line 7, PREFLEN=6, RANGELEN=2, VAL=17..20, 111011+(VAL-17) */
    17,     /* 111011 00 */
    18,     /* 111011 01 */
    19,     /* 111011 10 */
    20,     /* 111011 11 */
    /* line 8, PREFLEN=6, RANGELEN=3, VAL=21..28, 111100+(VAL-21) */
    21,     /* 111100 000 */
    22,     /* 111100 001 */
    27,     /* 111100 110 */
    28,     /* 111100 111 */
    /* line 9, PREFLEN=6, RANGELEN=4, VAL=29..44, 111101+(VAL-29) */
    29,     /* 111101 0000 */
    30,     /* 111101 0001 */
    43,     /* 111101 1110 */
    44,     /* 111101 1111 */
    /* line 10, PREFLEN=6, RANGELEN=5, VAL=45..76, 111110+(VAL-45) */
    45,     /* 111110 00000 */
    46,     /* 111110 00001 */
    75,     /* 111110 11110 */
    76,     /* 111110 11111 */
    /* line 11, PREFLEN=7, RANGELEN=6, VAL=77..140, 1111110+(VAL-77) */
    77,     /* 1111110 000000 */
    78,     /* 1111110 000001 */
    139,    /* 1111110 111110 */
    140,    /* 1111110 111111 */
    /* line 12, PREFLEN=7, RANGELEN=32, VAL=141..INF, 1111111+(VAL-141) */
    141,    /* 1111111 00000000 00000000 00000000 00000000 */
    142,    /* 1111111 00000000 00000000 00000000 00000001 */
};
const byte test_input_M[] = {
    /* 0100 1100 1110 0110 1011 0111 0100 0101 */
       0x4c,     0xe6,     0xb7,     0x45,
    /* 0011 0111 0101 1111 1101 0011 1010 1111 */
       0x37,     0x5f,     0xd3,     0xaf,
    /* 0110 0111 0110 1111 0111 0111 0111 1111 */
       0x67,     0x6f,     0x77,     0x7f,
    /* 1000 0011 1100 0011 1110 0110 1111 0011 */
       0x83,     0xc3,     0xe6,     0xf3,
    /* 1111 1010 0001 1110 1000 1111 1011 1101 */
       0xfa,     0x1e,     0x8f,     0xbd,
    /* 1110 1111 1111 1100 0000 1111 1000 0011 */
       0xef,     0xfc,     0x0f,     0x83,
    /* 1111 0111 1011 1110 1111 1111 1110 0000 */
       0xf7,     0xbe,     0xff,     0xe0,
    /* 0011 1111 0000 0011 1111 1011 1110 1111 */
       0x3f,     0x03,     0xfb,     0xef,
    /* 1101 1111 1111 1111 0000 0000 0000 0000 */
       0xdf,     0xff,     0x00,     0x00,
    /* 0000 0000 0000 0000 1111 1110 0000 0000 */
       0x00,     0x00,     0xfe,     0x00,
    /* 0000 0000 0000 0000 0000 001 */
       0x00,     0x00,     0x02,
};

/* test code for Table B.14 - Standard Huffman table N */
const int32_t test_output_N[] = {
    /* line 0, PREFLEN=3, RANGELEN=0, VAL=-2, 100 */
    -2,     /* 100 */
    /* line 1, PREFLEN=3, RANGELEN=0, VAL=-1, 101 */
    -1,     /* 101 */
    /* line 2, PREFLEN=1, RANGELEN=0, VAL=1, 0 */
    0,      /* 0 */
    /* line 3, PREFLEN=3, RANGELEN=0, VAL=1, 110 */
    1,      /* 110 */
    /* line 4, PREFLEN=3, RANGELEN=0, VAL=2, 111 */
    2,      /* 111 */
};
const byte test_input_N[] = {
    /* 1001 0101 1011 1 */
       0x95,     0xb8,
};

/* test code for Table B.15 - Standard Huffman table O */
const int32_t test_output_O[] = {
    /* line 0, PREFLEN=7, RANGELEN=4, VAL=-24..-9, 1111100+(VAL+24) */
    -24,    /* 1111100 0000 */
    -23,    /* 1111100 0001 */
    -10,    /* 1111100 1110 */
    -9,     /* 1111100 1111 */
    /* line 1, PREFLEN=6, RANGELEN=2, VAL=-8..-5, 111100+(VAL+8) */
    -8,     /* 111100 00 */
    -7,     /* 111100 01 */
    -6,     /* 111100 10 */
    -5,     /* 111100 11 */
    /* line 2, PREFLEN=5, RANGELEN=1, VAL=-4..-3, 11100+(VAL+4) */
    -4,     /* 11100 0 */
    -3,     /* 11100 1 */
    /* line 3, PREFLEN=4, RANGELEN=0, VAL=-2, 1100 */
    -2,     /* 1100 */
    /* line 4, PREFLEN=3, RANGELEN=0, VAL=-1, 100 */
    -1,     /* 100 */
    /* line 5, PREFLEN=1, RANGELEN=0, VAL=1, 0 */
    0,      /* 0 */
    /* line 6, PREFLEN=3, RANGELEN=0, VAL=1, 101 */
    1,      /* 101 */
    /* line 7, PREFLEN=4, RANGELEN=0, VAL=2, 1101 */
    2,      /* 1101 */
    /* line 8, PREFLEN=5, RANGELEN=1, VAL=3..4, 11101+(VAL-3) */
    3,      /* 11101 0 */
    4,      /* 11101 1 */
    /* line 9, PREFLEN=6, RANGELEN=2, VAL=5..8, 111101+(VAL-5) */
    5,      /* 111101 00 */
    6,      /* 111101 01 */
    7,      /* 111101 10 */
    8,      /* 111101 11 */
    /* line 10, PREFLEN=7, RANGELEN=4, VAL=9..24, 1111101+(VAL-9) */
    9,      /* 1111101 0000 */
    10,     /* 1111101 0001 */
    23,     /* 1111101 1110 */
    24,     /* 1111101 1111 */
    /* line 11, PREFLEN=7, RANGELEN=32, VAL=-INF..-25, 1111110+(-25-VAL) */
    -25,    /* 1111110 00000000 00000000 00000000 00000000 */
    -26,    /* 1111110 00000000 00000000 00000000 00000001 */
    /* line 12, PREFLEN=7, RANGELEN=32, VAL=25..INF, 1111111+(VAL-25) */
    25,     /* 1111111 00000000 00000000 00000000 00000000 */
    26,     /* 1111111 00000000 00000000 00000000 00000001 */
};
const byte test_input_O[] = {
    /* 1111 1000 0001 1111 0000 0111 1110 0111 */
       0xf8,     0x1f,     0x07,     0xe7,
    /* 0111 1100 1111 1111 0000 1111 0001 1111 */
       0x7c,     0xff,     0x0f,     0x1f,
    /* 0010 1111 0011 1110 0011 1001 1100 1000 */
       0x2f,     0x3e,     0x39,     0xc8,
    /* 1011 1011 1101 0111 0111 1110 1001 1110 */
       0xbb,     0xd7,     0x7e,     0x9e,
    /* 1011 1110 1101 1110 1111 1111 0100 0011 */
       0xbe,     0xde,     0xff,     0x43,
    /* 1110 1000 1111 1101 1110 1111 1011 1111 */
       0xe8,     0xfd,     0xef,     0xbf,
    /* 1111 1000 0000 0000 0000 0000 0000 0000 */
       0xf8,     0x00,     0x00,     0x00,
    /* 0000 0011 1111 0000 0000 0000 0000 0000 */
       0x03,     0xf0,     0x00,     0x00,
    /* 0000 0000 0000 1111 1111 0000 0000 0000 */
       0x00,     0x0f,     0xf0,     0x00,
    /* 0000 0000 0000 0000 0000 1111 1110 0000 */
       0x00,     0x00,     0x0f,     0xe0,
    /* 0000 0000 0000 0000 0000 0000 001 */
       0x00,     0x00,     0x00,     0x20,
};

typedef struct test_huffmancodes {
    const char *name;
    const Jbig2HuffmanParams *params;
    const byte *input;
    const size_t input_len;
    const int32_t *output;
    const size_t output_len;
} test_huffmancodes_t;

#define countof(x) (sizeof((x)) / sizeof((x)[0]))

#define DEF_TEST_HUFFMANCODES(x) { \
    #x, \
    &jbig2_huffman_params_##x, \
    test_input_##x, countof(test_input_##x), \
    test_output_##x, countof(test_output_##x), \
}

test_huffmancodes_t   tests[] = {
    DEF_TEST_HUFFMANCODES(A),
    DEF_TEST_HUFFMANCODES(B),
    DEF_TEST_HUFFMANCODES(C),
    DEF_TEST_HUFFMANCODES(D),
    DEF_TEST_HUFFMANCODES(E),
    DEF_TEST_HUFFMANCODES(F),
    DEF_TEST_HUFFMANCODES(G),
    DEF_TEST_HUFFMANCODES(H),
    DEF_TEST_HUFFMANCODES(I),
    DEF_TEST_HUFFMANCODES(J),
    DEF_TEST_HUFFMANCODES(K),
    DEF_TEST_HUFFMANCODES(L),
    DEF_TEST_HUFFMANCODES(M),
    DEF_TEST_HUFFMANCODES(N),
    DEF_TEST_HUFFMANCODES(O),
};

typedef struct test_stream {
    Jbig2WordStream ws;
    test_huffmancodes_t *h;
} test_stream_t;

static uint32_t
test_get_word(Jbig2WordStream *self, int offset)
{
    uint32_t val = 0;
    test_stream_t *st = (test_stream_t *)self;
    if (st != NULL) {
        if (st->h != NULL) {
            if (offset   < st->h->input_len) {
                val |= (st->h->input[offset]   << 24);
            }
            if (offset+1 < st->h->input_len) {
                val |= (st->h->input[offset+1] << 16);
            }
            if (offset+2 < st->h->input_len) {
                val |= (st->h->input[offset+2] << 8);
            }
            if (offset+3 < st->h->input_len) {
                val |=  st->h->input[offset+3];
            }
        }
    }
    return val;
}

int
main (int argc, char **argv)
{
    Jbig2Ctx *ctx = jbig2_ctx_new(NULL, 0, NULL, NULL, NULL);
    int i;

    for (i = 0; i < countof(tests); i++) {
        Jbig2HuffmanTable *table;
        Jbig2HuffmanState *hs;
        test_stream_t st;
        int32_t code;
        bool oob;
        int j;

        st.ws.get_next_word = test_get_word;
        st.h = &tests[i];
        printf("testing Standard Huffman table %s: ", st.h->name);
        table = jbig2_build_huffman_table(ctx, st.h->params);
        if (table == NULL) {
            printf("jbig2_build_huffman_table() returned NULL!\n");
        } else {
            /* jbig2_dump_huffman_table(table); */
            hs = jbig2_huffman_new(ctx, &st.ws);
            if ( hs == NULL ) {
                printf("jbig2_huffman_new() returned NULL!\n");
            } else {
                for (j = 0; j < st.h->output_len; j++) {
                    printf("%d...", st.h->output[j]);
                    code = jbig2_huffman_get(hs, table, &oob);
                    if (code == st.h->output[j] && !oob) {
                        printf("ok, ");
                    } else {
                        int need_comma = 0;
                        printf("NG(");
                        if (code != st.h->output[j]) {
                            printf("%d", code);
                            need_comma = 1;
                        }
                        if (oob) {
                            if (need_comma)
                                printf(",");
                            printf("OOB");
                        }
                        printf("), ");
                    }
                }
                if (st.h->params->HTOOB) {
                    printf("OOB...");
                    code = jbig2_huffman_get(hs, table, &oob);
                    if (oob) {
                        printf("ok");
                    } else {
                        printf("NG(%d)", code);
                    }
                }
                printf("\n");
                jbig2_huffman_free(ctx, hs);
            }
            jbig2_release_huffman_table(ctx, table);
        }
    }
    jbig2_ctx_free(ctx);
    return 0;
}
#endif
