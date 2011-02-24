#include "fitz.h"

/* TODO: error checking */

enum
{
	MINBITS = 9,
	MAXBITS = 12,
	NUMCODES = (1 << MAXBITS),
	LZW_CLEAR = 256,
	LZW_EOD = 257,
	LZW_FIRST = 258,
	MAXLENGTH = 4097
};

typedef struct lzw_code_s lzw_code;

struct lzw_code_s
{
	int prev;			/* prev code (in string) */
	unsigned short length;		/* string len, including this token */
	unsigned char value;		/* data value */
	unsigned char firstchar;	/* first token of string */
};

typedef struct fz_lzwd_s fz_lzwd;

struct fz_lzwd_s
{
	fz_stream *chain;
	int eod;

	int earlychange;

	int codebits;			/* num bits/code */
	int code;			/* current code */
	int oldcode;			/* previously recognized code */
	int nextcode;			/* next free entry */

	lzw_code table[NUMCODES];

	unsigned char bp[MAXLENGTH];
	unsigned char *rp, *wp;
};

static int
readlzwd(fz_stream *stm, unsigned char *buf, int len)
{
	fz_lzwd *lzw = stm->state;
	lzw_code *table = lzw->table;
	unsigned char *p = buf;
	unsigned char *ep = buf + len;
	unsigned char *s;
	int codelen;

	int codebits = lzw->codebits;
	int code = lzw->code;
	int oldcode = lzw->oldcode;
	int nextcode = lzw->nextcode;

	while (lzw->rp < lzw->wp && p < ep)
		*p++ = *lzw->rp++;

	while (p < ep)
	{
		if (lzw->eod)
			return 0;

		code = fz_readbits(lzw->chain, codebits);

		if (fz_peekbyte(lzw->chain) == EOF)
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
			codebits = MINBITS;
			nextcode = LZW_FIRST;
			oldcode = -1;
			continue;
		}

		/* if stream starts without a clear code, oldcode is undefined... */
		if (oldcode == -1)
		{
			oldcode = code;
		}
		else
		{
			/* add new entry to the code table */
			table[nextcode].prev = oldcode;
			table[nextcode].firstchar = table[oldcode].firstchar;
			table[nextcode].length = table[oldcode].length + 1;
			if (code < nextcode)
				table[nextcode].value = table[code].firstchar;
			else if (code == nextcode)
				table[nextcode].value = table[nextcode].firstchar;
			else
				fz_warn("out of range code encountered in lzw decode");

			nextcode ++;

			if (nextcode > (1 << codebits) - lzw->earlychange - 1)
			{
				codebits ++;
				if (codebits > MAXBITS)
					codebits = MAXBITS;	/* FIXME */
			}

			oldcode = code;
		}

		/* code maps to a string, copy to output (in reverse...) */
		if (code > 255)
		{
			codelen = table[code].length;
			lzw->rp = lzw->bp;
			lzw->wp = lzw->bp + codelen;

			assert(codelen < MAXLENGTH);

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

	lzw->codebits = codebits;
	lzw->code = code;
	lzw->oldcode = oldcode;
	lzw->nextcode = nextcode;

	return p - buf;
}

static void
closelzwd(fz_stream *stm)
{
	fz_lzwd *lzw = stm->state;
	fz_close(lzw->chain);
	fz_free(lzw);
}

fz_stream *
fz_openlzwd(fz_stream *chain, fz_obj *params)
{
	fz_lzwd *lzw;
	fz_obj *obj;
	int i;

	lzw = fz_malloc(sizeof(fz_lzwd));
	lzw->chain = chain;
	lzw->eod = 0;
	lzw->earlychange = 1;

	obj = fz_dictgets(params, "EarlyChange");
	if (obj)
		lzw->earlychange = !!fz_toint(obj);

	for (i = 0; i < 256; i++)
	{
		lzw->table[i].value = i;
		lzw->table[i].firstchar = i;
		lzw->table[i].length = 1;
		lzw->table[i].prev = -1;
	}

	for (i = 256; i < NUMCODES; i++)
	{
		lzw->table[i].value = 0;
		lzw->table[i].firstchar = 0;
		lzw->table[i].length = 0;
		lzw->table[i].prev = -1;
	}

	lzw->codebits = MINBITS;
	lzw->code = -1;
	lzw->nextcode = LZW_FIRST;
	lzw->oldcode = -1;
	lzw->rp = lzw->bp;
	lzw->wp = lzw->bp;

	return fz_newstream(lzw, readlzwd, closelzwd);
}
