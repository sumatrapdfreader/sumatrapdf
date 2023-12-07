// Copyright (C) 2004-2022 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include <assert.h>

/* TODO: error checking */

#define LZW_CLEAR(lzw)	(1 << ((lzw)->min_bits - 1))
#define LZW_EOD(lzw)	(LZW_CLEAR(lzw) + 1)
#define LZW_FIRST(lzw)	(LZW_CLEAR(lzw) + 2)

enum
{
	MAX_BITS = 12,
	NUM_CODES = (1 << MAX_BITS),
	MAX_LENGTH = 4097
};

typedef struct
{
	int prev;			/* prev code (in string) */
	unsigned short length;		/* string len, including this token */
	unsigned char value;		/* data value */
	unsigned char first_char;	/* first token of string */
} lzw_code;

typedef struct
{
	fz_stream *chain;
	int eod;

	int early_change;

	int reverse_bits;
	int old_tiff;
	int min_bits;			/* minimum num bits/code */
	int code_bits;			/* num bits/code */
	int code;			/* current code */
	int old_code;			/* previously recognized code */
	int next_code;			/* next free entry */

	lzw_code table[NUM_CODES];

	unsigned char bp[MAX_LENGTH];
	unsigned char *rp, *wp;

	unsigned char buffer[4096];
} fz_lzwd;

static int
next_lzwd(fz_context *ctx, fz_stream *stm, size_t len)
{
	fz_lzwd *lzw = stm->state;
	lzw_code *table = lzw->table;
	unsigned char *buf = lzw->buffer;
	unsigned char *p = buf;
	unsigned char *ep;
	unsigned char *s;
	int codelen;

	int code_bits = lzw->code_bits;
	int code = lzw->code;
	int old_code = lzw->old_code;
	int next_code = lzw->next_code;

	if (len > sizeof(lzw->buffer))
		len = sizeof(lzw->buffer);
	ep = buf + len;

	while (lzw->rp < lzw->wp && p < ep)
		*p++ = *lzw->rp++;

	while (p < ep)
	{
		if (lzw->eod)
			return EOF;

		if (fz_is_eof_bits(ctx, lzw->chain))
		{
			fz_warn(ctx, "premature end in lzw decode");
			lzw->eod = 1;
			break;
		}

		if (lzw->reverse_bits)
			code = fz_read_rbits(ctx, lzw->chain, code_bits);
		else
			code = fz_read_bits(ctx, lzw->chain, code_bits);

		if (code == LZW_EOD(lzw))
		{
			lzw->eod = 1;
			break;
		}

		/* Old Tiffs are allowed to NOT send the clear code, and to
		 * overrun at the end. */
		if (!lzw->old_tiff && next_code > NUM_CODES && code != LZW_CLEAR(lzw))
		{
			fz_warn(ctx, "missing clear code in lzw decode");
			code = LZW_CLEAR(lzw);
		}

		if (code == LZW_CLEAR(lzw))
		{
			code_bits = lzw->min_bits;
			next_code = LZW_FIRST(lzw);
			old_code = -1;
			continue;
		}

		/* if stream starts without a clear code, old_code is undefined... */
		if (old_code == -1)
		{
			old_code = code;
		}
		else if (!lzw->old_tiff && next_code == NUM_CODES)
		{
			/* TODO: Ghostscript checks for a following clear code before tolerating */
			fz_warn(ctx, "tolerating a single out of range code in lzw decode");
			next_code++;
		}
		else if (code > next_code || (!lzw->old_tiff && next_code >= NUM_CODES))
		{
			fz_throw(ctx, FZ_ERROR_FORMAT, "out of range code encountered in lzw decode");
		}
		else if (next_code < NUM_CODES)
		{
			/* add new entry to the code table */
			table[next_code].prev = old_code;
			table[next_code].first_char = table[old_code].first_char;
			table[next_code].length = table[old_code].length + 1;
			if (code < next_code)
				table[next_code].value = table[code].first_char;
			else if (code == next_code)
				table[next_code].value = table[next_code].first_char;
			else
				fz_throw(ctx, FZ_ERROR_FORMAT, "out of range code encountered in lzw decode");

			next_code ++;

			if (next_code > (1 << code_bits) - lzw->early_change - 1)
			{
				code_bits ++;
				if (code_bits > MAX_BITS)
					code_bits = MAX_BITS;
			}

			old_code = code;
		}

		/* code maps to a string, copy to output (in reverse...) */
		if (code >= LZW_CLEAR(lzw))
		{
			codelen = table[code].length;
			lzw->rp = lzw->bp;
			lzw->wp = lzw->bp + codelen;

			assert(codelen < MAX_LENGTH);

			s = lzw->wp;
			do {
				*(--s) = table[code].value;
				code = table[code].prev;
			} while (code >= 0 && s > lzw->bp);
		}

		/* ... or just a single character */
		else
		{
			lzw->bp[0] = code;
			lzw->rp = lzw->bp;
			lzw->wp = lzw->bp + 1;
		}

		/* copy to output */
		while (lzw->rp < lzw->wp && p < ep)
			*p++ = *lzw->rp++;
	}

	lzw->code_bits = code_bits;
	lzw->code = code;
	lzw->old_code = old_code;
	lzw->next_code = next_code;

	stm->rp = buf;
	stm->wp = p;
	if (buf == p)
		return EOF;
	stm->pos += p - buf;

	return *stm->rp++;
}

static void
close_lzwd(fz_context *ctx, void *state_)
{
	fz_lzwd *lzw = (fz_lzwd *)state_;
	fz_sync_bits(ctx, lzw->chain);
	fz_drop_stream(ctx, lzw->chain);
	fz_free(ctx, lzw);
}

fz_stream *
fz_open_lzwd(fz_context *ctx, fz_stream *chain, int early_change, int min_bits, int reverse_bits, int old_tiff)
{
	fz_lzwd *lzw;
	int i;

	if (min_bits > MAX_BITS)
	{
		fz_warn(ctx, "out of range initial lzw code size");
		min_bits = MAX_BITS;
	}

	lzw = fz_malloc_struct(ctx, fz_lzwd);
	lzw->eod = 0;
	lzw->early_change = early_change;
	lzw->reverse_bits = reverse_bits;
	lzw->old_tiff = old_tiff;
	lzw->min_bits = min_bits;
	lzw->code_bits = lzw->min_bits;
	lzw->code = -1;
	lzw->next_code = LZW_FIRST(lzw);
	lzw->old_code = -1;
	lzw->rp = lzw->bp;
	lzw->wp = lzw->bp;

	for (i = 0; i < LZW_CLEAR(lzw); i++)
	{
		lzw->table[i].value = i;
		lzw->table[i].first_char = i;
		lzw->table[i].length = 1;
		lzw->table[i].prev = -1;
	}

	for (i = LZW_CLEAR(lzw); i < NUM_CODES; i++)
	{
		lzw->table[i].value = 0;
		lzw->table[i].first_char = 0;
		lzw->table[i].length = 0;
		lzw->table[i].prev = -1;
	}

	lzw->chain = fz_keep_stream(ctx, chain);

	return fz_new_stream(ctx, lzw, next_lzwd, close_lzwd);
}
