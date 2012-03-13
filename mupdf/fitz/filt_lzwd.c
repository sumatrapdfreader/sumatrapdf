#include "fitz-internal.h"

/* TODO: error checking */

enum
{
	MIN_BITS = 9,
	MAX_BITS = 12,
	NUM_CODES = (1 << MAX_BITS),
	LZW_CLEAR = 256,
	LZW_EOD = 257,
	LZW_FIRST = 258,
	MAX_LENGTH = 4097
};

typedef struct lzw_code_s lzw_code;

struct lzw_code_s
{
	int prev;			/* prev code (in string) */
	unsigned short length;		/* string len, including this token */
	unsigned char value;		/* data value */
	unsigned char first_char;	/* first token of string */
};

typedef struct fz_lzwd_s fz_lzwd;

struct fz_lzwd_s
{
	fz_stream *chain;
	int eod;

	int early_change;

	int code_bits;			/* num bits/code */
	int code;			/* current code */
	int old_code;			/* previously recognized code */
	int next_code;			/* next free entry */

	lzw_code table[NUM_CODES];

	unsigned char bp[MAX_LENGTH];
	unsigned char *rp, *wp;
};

static int
read_lzwd(fz_stream *stm, unsigned char *buf, int len)
{
	fz_lzwd *lzw = stm->state;
	lzw_code *table = lzw->table;
	unsigned char *p = buf;
	unsigned char *ep = buf + len;
	unsigned char *s;
	int codelen;

	int code_bits = lzw->code_bits;
	int code = lzw->code;
	int old_code = lzw->old_code;
	int next_code = lzw->next_code;

	while (lzw->rp < lzw->wp && p < ep)
		*p++ = *lzw->rp++;

	while (p < ep)
	{
		if (lzw->eod)
			return 0;

		code = fz_read_bits(lzw->chain, code_bits);

		if (fz_is_eof_bits(lzw->chain))
		{
			lzw->eod = 1;
			break;
		}

		if (code == LZW_EOD)
		{
			lzw->eod = 1;
			break;
		}

		if (code == LZW_CLEAR)
		{
			code_bits = MIN_BITS;
			next_code = LZW_FIRST;
			old_code = -1;
			continue;
		}

		/* if stream starts without a clear code, old_code is undefined... */
		if (old_code == -1)
		{
			old_code = code;
		}
		else
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
				fz_warn(stm->ctx, "out of range code encountered in lzw decode");

			next_code ++;

			if (next_code > (1 << code_bits) - lzw->early_change - 1)
			{
				code_bits ++;
				if (code_bits > MAX_BITS)
					code_bits = MAX_BITS;	/* FIXME */
			}

			old_code = code;
		}

		/* code maps to a string, copy to output (in reverse...) */
		if (code > 255)
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

	return p - buf;
}

static void
close_lzwd(fz_context *ctx, void *state_)
{
	fz_lzwd *lzw = (fz_lzwd *)state_;
	fz_close(lzw->chain);
	fz_free(ctx, lzw);
}

/* Default: early_change = 1 */
fz_stream *
fz_open_lzwd(fz_stream *chain, int early_change)
{
	fz_context *ctx = chain->ctx;
	fz_lzwd *lzw = NULL;
	int i;

	fz_var(lzw);

	fz_try(ctx)
	{
		lzw = fz_malloc_struct(ctx, fz_lzwd);
		lzw->chain = chain;
		lzw->eod = 0;
		lzw->early_change = early_change;

		for (i = 0; i < 256; i++)
		{
			lzw->table[i].value = i;
			lzw->table[i].first_char = i;
			lzw->table[i].length = 1;
			lzw->table[i].prev = -1;
		}

		for (i = 256; i < NUM_CODES; i++)
		{
			lzw->table[i].value = 0;
			lzw->table[i].first_char = 0;
			lzw->table[i].length = 0;
			lzw->table[i].prev = -1;
		}

		lzw->code_bits = MIN_BITS;
		lzw->code = -1;
		lzw->next_code = LZW_FIRST;
		lzw->old_code = -1;
		lzw->rp = lzw->bp;
		lzw->wp = lzw->bp;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, lzw);
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, lzw, read_lzwd, close_lzwd);
}
