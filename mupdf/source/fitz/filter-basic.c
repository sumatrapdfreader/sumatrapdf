#include "mupdf/fitz.h"

/* Pretend we have a filter that just copies data forever */

fz_stream *
fz_open_copy(fz_stream *chain)
{
	return fz_keep_stream(chain);
}

/* Null filter copies a specified amount of data */

struct null_filter
{
	fz_stream *chain;
	int remain;
	int offset;
	unsigned char buffer[4096];
};

static int
next_null(fz_stream *stm, int max)
{
	struct null_filter *state = stm->state;
	int n;

	if (state->remain == 0)
		return EOF;
	fz_seek(state->chain, state->offset, 0);
	n = fz_available(state->chain, max);
	if (n > state->remain)
		n = state->remain;
	if (n > sizeof(state->buffer))
		n = sizeof(state->buffer);
	memcpy(state->buffer, state->chain->rp, n);
	stm->rp = state->buffer;
	stm->wp = stm->rp + n;
	if (n == 0)
		return EOF;
	state->chain->rp += n;
	state->remain -= n;
	state->offset += n;
	stm->pos += n;
	return *stm->rp++;
}

static void
close_null(fz_context *ctx, void *state_)
{
	struct null_filter *state = (struct null_filter *)state_;
	fz_stream *chain = state->chain;
	fz_free(ctx, state);
	fz_close(chain);
}

static fz_stream *
rebind_null(fz_stream *s)
{
	struct null_filter *state = s->state;
	return state->chain;
}

fz_stream *
fz_open_null(fz_stream *chain, int len, int offset)
{
	struct null_filter *state;
	fz_context *ctx = chain->ctx;

	if (len < 0)
		len = 0;
	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, struct null_filter);
		state->chain = chain;
		state->remain = len;
		state->offset = offset;
	}
	fz_catch(ctx)
	{
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_null, close_null, rebind_null);
}

/* Concat filter concatenates several streams into one */

struct concat_filter
{
	int max;
	int count;
	int current;
	int pad; /* 1 if we should add whitespace padding between streams */
	unsigned char ws_buf;
	fz_stream *chain[1];
};

static int
next_concat(fz_stream *stm, int max)
{
	struct concat_filter *state = (struct concat_filter *)stm->state;
	int n;

	while (state->current < state->count)
	{
		/* Read the next block of underlying data. */
		if (stm->wp == state->chain[state->current]->wp)
			state->chain[state->current]->rp = stm->wp;
		n = fz_available(state->chain[state->current], max);
		if (n)
		{
			stm->rp = state->chain[state->current]->rp;
			stm->wp = state->chain[state->current]->wp;
			stm->pos += n;
			return *stm->rp++;
		}
		else
		{
			if (state->chain[state->current]->error)
			{
				/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1239 */
				fz_warn(stm->ctx, "skipping to next stream part on error");
			}
			state->current++;
			fz_close(state->chain[state->current-1]);
			if (state->pad)
			{
				/* SumatraPDF: force-close strings at PDF stream boundaries */
				if (state->pad == 5)
				{
					stm->rp = (unsigned char *)"\n%)>\n";
					stm->wp = stm->rp + 5;
					stm->pos += 5;
					return *stm->rp++;
				}
				stm->rp = (&state->ws_buf)+1;
				stm->wp = stm->rp + 1;
				stm->pos++;
				return 32;
			}
		}
	}

	stm->rp = stm->wp;

	return EOF;
}

static void
close_concat(fz_context *ctx, void *state_)
{
	struct concat_filter *state = (struct concat_filter *)state_;
	int i;

	for (i = state->current; i < state->count; i++)
	{
		fz_close(state->chain[i]);
	}
	fz_free(ctx, state);
}

static fz_stream *
rebind_concat(fz_stream *s)
{
	struct concat_filter *state = s->state;
	int i;

	if (state->current >= state->count)
		return NULL;
	for (i = state->current; i < state->count-1; i++)
	{
		fz_rebind_stream(state->chain[i], s->ctx);
	}
	return state->chain[i];
}

fz_stream *
fz_open_concat(fz_context *ctx, int len, int pad)
{
	struct concat_filter *state;

	state = fz_calloc(ctx, 1, sizeof(struct concat_filter) + (len-1)*sizeof(fz_stream *));
	state->max = len;
	state->count = 0;
	state->current = 0;
	state->pad = pad;
	state->ws_buf = 32;

	return fz_new_stream(ctx, state, next_concat, close_concat, rebind_concat);
}

void
fz_concat_push(fz_stream *concat, fz_stream *chain)
{
	struct concat_filter *state = (struct concat_filter *)concat->state;

	if (state->count == state->max)
		fz_throw(concat->ctx, FZ_ERROR_GENERIC, "Concat filter size exceeded");

	state->chain[state->count++] = chain;
}

/* ASCII Hex Decode */

typedef struct fz_ahxd_s fz_ahxd;

struct fz_ahxd_s
{
	fz_stream *chain;
	int eod;
	unsigned char buffer[256];
};

static inline int iswhite(int a)
{
	switch (a) {
	case '\n': case '\r': case '\t': case ' ':
	case '\0': case '\f': case '\b': case 0177:
		return 1;
	}
	return 0;
}

static inline int ishex(int a)
{
	return (a >= 'A' && a <= 'F') ||
		(a >= 'a' && a <= 'f') ||
		(a >= '0' && a <= '9');
}

static inline int unhex(int a)
{
	if (a >= 'A' && a <= 'F') return a - 'A' + 0xA;
	if (a >= 'a' && a <= 'f') return a - 'a' + 0xA;
	if (a >= '0' && a <= '9') return a - '0';
	return 0;
}

static int
next_ahxd(fz_stream *stm, int max)
{
	fz_ahxd *state = stm->state;
	unsigned char *p = state->buffer;
	unsigned char *ep;
	int a, b, c, odd;

	if (max > sizeof(state->buffer))
		max = sizeof(state->buffer);
	ep = p + max;

	odd = 0;

	while (p < ep)
	{
		if (state->eod)
			break;

		c = fz_read_byte(state->chain);
		if (c < 0)
			break;

		if (ishex(c))
		{
			if (!odd)
			{
				a = unhex(c);
				odd = 1;
			}
			else
			{
				b = unhex(c);
				*p++ = (a << 4) | b;
				odd = 0;
			}
		}
		else if (c == '>')
		{
			if (odd)
				*p++ = (a << 4);
			state->eod = 1;
			break;
		}
		else if (!iswhite(c))
		{
			fz_throw(stm->ctx, FZ_ERROR_GENERIC, "bad data in ahxd: '%c'", c);
		}
	}
	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if (stm->rp != p)
		return *stm->rp++;
	return EOF;
}

static void
close_ahxd(fz_context *ctx, void *state_)
{
	fz_ahxd *state = (fz_ahxd *)state_;
	fz_stream *chain = state->chain;
	fz_free(ctx, state);
	fz_close(chain);
}

static fz_stream *
rebind_ahxd(fz_stream *s)
{
	fz_ahxd *state = s->state;
	return state->chain;
}

fz_stream *
fz_open_ahxd(fz_stream *chain)
{
	fz_ahxd *state;
	fz_context *ctx = chain->ctx;

	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, fz_ahxd);
		state->chain = chain;
		state->eod = 0;
	}
	fz_catch(ctx)
	{
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_ahxd, close_ahxd, rebind_ahxd);
}

/* ASCII 85 Decode */

typedef struct fz_a85d_s fz_a85d;

struct fz_a85d_s
{
	fz_stream *chain;
	unsigned char buffer[256];
	int eod;
};

static int
next_a85d(fz_stream *stm, int max)
{
	fz_a85d *state = stm->state;
	unsigned char *p = state->buffer;
	unsigned char *ep;
	int count = 0;
	int word = 0;
	int c;

	if (state->eod)
		return EOF;

	if (max > sizeof(state->buffer))
		max = sizeof(state->buffer);

	ep = p + max;
	while (p < ep)
	{
		c = fz_read_byte(state->chain);
		if (c < 0)
			break;

		if (c >= '!' && c <= 'u')
		{
			if (count == 4)
			{
				word = word * 85 + (c - '!');

				*p++ = (word >> 24) & 0xff;
				*p++ = (word >> 16) & 0xff;
				*p++ = (word >> 8) & 0xff;
				*p++ = (word) & 0xff;

				word = 0;
				count = 0;
			}
			else
			{
				word = word * 85 + (c - '!');
				count ++;
			}
		}

		else if (c == 'z' && count == 0)
		{
			*p++ = 0;
			*p++ = 0;
			*p++ = 0;
			*p++ = 0;
		}

		else if (c == '~')
		{
			c = fz_read_byte(state->chain);
			if (c != '>')
				fz_warn(stm->ctx, "bad eod marker in a85d");

			switch (count) {
			case 0:
				break;
			case 1:
				/* Specifically illegal in the spec, but adobe
				 * and gs both cope. See normal_87.pdf for a
				 * case where this matters. */
				fz_warn(stm->ctx, "partial final byte in a85d");
				break;
			case 2:
				word = word * (85 * 85 * 85) + 0xffffff;
				*p++ = word >> 24;
				break;
			case 3:
				word = word * (85 * 85) + 0xffff;
				*p++ = word >> 24;
				*p++ = word >> 16;
				break;
			case 4:
				word = word * 85 + 0xff;
				*p++ = word >> 24;
				*p++ = word >> 16;
				*p++ = word >> 8;
				break;
			}
			state->eod = 1;
			break;
		}

		else if (!iswhite(c))
		{
			fz_throw(stm->ctx, FZ_ERROR_GENERIC, "bad data in a85d: '%c'", c);
		}
	}

	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if (p == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void
close_a85d(fz_context *ctx, void *state_)
{
	fz_a85d *state = (fz_a85d *)state_;
	fz_stream *chain = state->chain;

	fz_free(ctx, state);
	fz_close(chain);
}

static fz_stream *
rebind_a85d(fz_stream *s)
{
	fz_a85d *state = s->state;
	return state->chain;
}

fz_stream *
fz_open_a85d(fz_stream *chain)
{
	fz_a85d *state;
	fz_context *ctx = chain->ctx;

	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, fz_a85d);
		state->chain = chain;
		state->eod = 0;
	}
	fz_catch(ctx)
	{
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_a85d, close_a85d, rebind_a85d);
}

/* Run Length Decode */

typedef struct fz_rld_s fz_rld;

struct fz_rld_s
{
	fz_stream *chain;
	int run, n, c;
	unsigned char buffer[256];
};

static int
next_rld(fz_stream *stm, int max)
{
	fz_rld *state = stm->state;
	unsigned char *p = state->buffer;
	unsigned char *ep;

	if (state->run == 128)
		return EOF;

	if (max > sizeof(state->buffer))
		max = sizeof(state->buffer);
	ep = p + max;

	while (p < ep)
	{
		if (state->run == 128)
			break;

		if (state->n == 0)
		{
			state->run = fz_read_byte(state->chain);
			if (state->run < 0)
			{
				state->run = 128;
				break;
			}
			if (state->run < 128)
				state->n = state->run + 1;
			if (state->run > 128)
			{
				state->n = 257 - state->run;
				state->c = fz_read_byte(state->chain);
				if (state->c < 0)
					fz_throw(stm->ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
			}
		}

		if (state->run < 128)
		{
			while (p < ep && state->n)
			{
				int c = fz_read_byte(state->chain);
				if (c < 0)
					fz_throw(stm->ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
				*p++ = c;
				state->n--;
			}
		}

		if (state->run > 128)
		{
			while (p < ep && state->n)
			{
				*p++ = state->c;
				state->n--;
			}
		}
	}

	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if (p == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void
close_rld(fz_context *ctx, void *state_)
{
	fz_rld *state = (fz_rld *)state_;
	fz_stream *chain = state->chain;

	fz_free(ctx, state);
	fz_close(chain);
}

static fz_stream *
rebind_rld(fz_stream *s)
{
	fz_rld *state = s->state;
	return state->chain;
}

fz_stream *
fz_open_rld(fz_stream *chain)
{
	fz_rld *state;
	fz_context *ctx = chain->ctx;

	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, fz_rld);
		state->chain = chain;
		state->run = 0;
		state->n = 0;
		state->c = 0;
	}
	fz_catch(ctx)
	{
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_rld, close_rld, rebind_rld);
}

/* RC4 Filter */

typedef struct fz_arc4c_s fz_arc4c;

struct fz_arc4c_s
{
	fz_stream *chain;
	fz_arc4 arc4;
	unsigned char buffer[256];
};

static int
next_arc4(fz_stream *stm, int max)
{
	fz_arc4c *state = stm->state;
	int n = fz_available(state->chain, max);

	if (n == 0)
		return EOF;
	if (n > sizeof(state->buffer))
		n = sizeof(state->buffer);

	stm->rp = state->buffer;
	stm->wp = state->buffer + n;
	fz_arc4_encrypt(&state->arc4, stm->rp, state->chain->rp, n);
	state->chain->rp += n;
	stm->pos += n;

	return *stm->rp++;
}

static void
close_arc4(fz_context *ctx, void *state_)
{
	fz_arc4c *state = (fz_arc4c *)state_;
	fz_stream *chain = state->chain;

	fz_free(ctx, state);
	fz_close(chain);
}

static fz_stream *
rebind_arc4c(fz_stream *s)
{
	fz_arc4c *state = s->state;
	return state->chain;
}

fz_stream *
fz_open_arc4(fz_stream *chain, unsigned char *key, unsigned keylen)
{
	fz_arc4c *state;
	fz_context *ctx = chain->ctx;

	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, fz_arc4c);
		state->chain = chain;
		fz_arc4_init(&state->arc4, key, keylen);
	}
	fz_catch(ctx)
	{
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_arc4, close_arc4, rebind_arc4c);
}

/* AES Filter */

typedef struct fz_aesd_s fz_aesd;

struct fz_aesd_s
{
	fz_stream *chain;
	fz_aes aes;
	unsigned char iv[16];
	int ivcount;
	unsigned char bp[16];
	unsigned char *rp, *wp;
	unsigned char buffer[256];
};

static int
next_aesd(fz_stream *stm, int max)
{
	fz_aesd *state = stm->state;
	unsigned char *p = state->buffer;
	unsigned char *ep;

	if (max > sizeof(state->buffer))
		max = sizeof(state->buffer);
	ep = p + max;

	while (state->ivcount < 16)
	{
		int c = fz_read_byte(state->chain);
		if (c < 0)
			fz_throw(stm->ctx, FZ_ERROR_GENERIC, "premature end in aes filter");
		state->iv[state->ivcount++] = c;
	}

	while (state->rp < state->wp && p < ep)
		*p++ = *state->rp++;

	while (p < ep)
	{
		int n = fz_read(state->chain, state->bp, 16);
		if (n == 0)
			break;
		else if (n < 16)
			fz_throw(stm->ctx, FZ_ERROR_GENERIC, "partial block in aes filter");

		aes_crypt_cbc(&state->aes, AES_DECRYPT, 16, state->iv, state->bp, state->bp);
		state->rp = state->bp;
		state->wp = state->bp + 16;

		/* strip padding at end of file */
		if (fz_is_eof(state->chain))
		{
			int pad = state->bp[15];
			if (pad < 1 || pad > 16)
				fz_throw(stm->ctx, FZ_ERROR_GENERIC, "aes padding out of range: %d", pad);
			state->wp -= pad;
		}

		while (state->rp < state->wp && p < ep)
			*p++ = *state->rp++;
	}

	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if (p == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void
close_aesd(fz_context *ctx, void *state_)
{
	fz_aesd *state = (fz_aesd *)state_;
	fz_stream *chain = state->chain;

	fz_free(ctx, state);
	fz_close(chain);
}

static fz_stream *
rebind_aesd(fz_stream *s)
{
	fz_aesd *state = s->state;
	return state->chain;
}

fz_stream *
fz_open_aesd(fz_stream *chain, unsigned char *key, unsigned keylen)
{
	fz_aesd *state = NULL;
	fz_context *ctx = chain->ctx;

	fz_var(state);

	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, fz_aesd);
		state->chain = chain;
		if (aes_setkey_dec(&state->aes, key, keylen * 8))
			fz_throw(ctx, FZ_ERROR_GENERIC, "AES key init failed (keylen=%d)", keylen * 8);
		state->ivcount = 0;
		state->rp = state->bp;
		state->wp = state->bp;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_aesd, close_aesd, rebind_aesd);
}
