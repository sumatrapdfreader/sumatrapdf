#include "mupdf/fitz.h"

#define MIN_BOMB (100 << 20)

int
fz_read(fz_stream *stm, unsigned char *buf, int len)
{
	int count, n;

	count = fz_mini(len, stm->wp - stm->rp);
	if (count)
	{
		memcpy(buf, stm->rp, count);
		stm->rp += count;
	}

	if (count == len || stm->error || stm->eof)
		return count;

	assert(stm->rp == stm->wp);

	if (len - count < stm->ep - stm->bp)
	{
		n = stm->read(stm, stm->bp, stm->ep - stm->bp);
		if (n == 0)
		{
			stm->eof = 1;
		}
		else if (n > 0)
		{
			stm->rp = stm->bp;
			stm->wp = stm->bp + n;
			stm->pos += n;
		}

		n = fz_mini(len - count, stm->wp - stm->rp);
		if (n)
		{
			memcpy(buf + count, stm->rp, n);
			stm->rp += n;
			count += n;
		}
	}
	else
	{
		n = stm->read(stm, buf + count, len - count);
		if (n == 0)
		{
			stm->eof = 1;
		}
		else if (n > 0)
		{
			stm->pos += n;
			count += n;
		}
	}

	return count;
}

void
fz_fill_buffer(fz_stream *stm)
{
	int n;

	assert(stm->rp == stm->wp);

	if (stm->error || stm->eof)
		return;

	fz_try(stm->ctx)
	{
		n = stm->read(stm, stm->bp, stm->ep - stm->bp);
		if (n == 0)
		{
			stm->eof = 1;
		}
		else if (n > 0)
		{
			stm->rp = stm->bp;
			stm->wp = stm->bp + n;
			stm->pos += n;
		}
	}
	fz_catch(stm->ctx)
	{
		fz_rethrow_if(stm->ctx, FZ_ERROR_TRYLATER);
		fz_warn(stm->ctx, "read error; treating as end of file");
		stm->error = 1;
	}
}

fz_buffer *
fz_read_all(fz_stream *stm, int initial)
{
	return fz_read_best(stm, initial, NULL);
}

fz_buffer *
fz_read_best(fz_stream *stm, int initial, int *truncated)
{
	fz_buffer *buf = NULL;
	int n;
	fz_context *ctx = stm->ctx;

	fz_var(buf);

	if (truncated)
		*truncated = 0;

	fz_try(ctx)
	{
		if (initial < 1024)
			initial = 1024;

		buf = fz_new_buffer(ctx, initial+1);

		while (1)
		{
			if (buf->len == buf->cap)
				fz_grow_buffer(ctx, buf);

			if (buf->len >= MIN_BOMB && buf->len / 200 > initial)
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "compression bomb detected");
			}

			n = fz_read(stm, buf->data + buf->len, buf->cap - buf->len);
			if (n == 0)
				break;

			buf->len += n;
		}
	}
	fz_catch(ctx)
	{
		if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
		{
			fz_drop_buffer(ctx, buf);
			fz_rethrow(ctx);
		}
		if (truncated)
		{
			*truncated = 1;
		}
		else
		{
			fz_drop_buffer(ctx, buf);
			fz_rethrow(ctx);
		}
	}
	fz_trim_buffer(ctx, buf);

	return buf;
}

void
fz_read_line(fz_stream *stm, char *mem, int n)
{
	char *s = mem;
	int c = EOF;
	while (n > 1)
	{
		c = fz_read_byte(stm);
		if (c == EOF)
			break;
		if (c == '\r') {
			c = fz_peek_byte(stm);
			if (c == '\n')
				fz_read_byte(stm);
			break;
		}
		if (c == '\n')
			break;
		*s++ = c;
		n--;
	}
	if (n)
		*s = '\0';
}

int
fz_tell(fz_stream *stm)
{
	return stm->pos - (stm->wp - stm->rp);
}

void
fz_seek(fz_stream *stm, int offset, int whence)
{
	stm->avail = 0; /* Reset bit reading */
	if (stm->seek)
	{
		if (whence == 1)
		{
			offset = fz_tell(stm) + offset;
			whence = 0;
		}
		if (whence == 0)
		{
			int dist = stm->pos - offset;
			if (dist >= 0 && dist <= stm->wp - stm->bp)
			{
				stm->rp = stm->wp - dist;
				stm->eof = 0;
				return;
			}
		}
		stm->seek(stm, offset, whence);
		stm->eof = 0;
	}
	else if (whence != 2)
	{
		if (whence == 0)
			offset -= fz_tell(stm);
		if (offset < 0)
			fz_warn(stm->ctx, "cannot seek backwards");
		/* dog slow, but rare enough */
		while (offset-- > 0)
			fz_read_byte(stm);
	}
	else
		fz_warn(stm->ctx, "cannot seek");
}

int fz_stream_meta(fz_stream *stm, int key, int size, void *ptr)
{
	if (!stm || !stm->meta)
		return -1;
	return stm->meta(stm, key, size, ptr);
}
