#include "fitz.h"

int
fz_read(fz_stream *stm, unsigned char *buf, int len)
{
	int count, n;

	count = MIN(len, stm->wp - stm->rp);
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
		if (n < 0)
		{
			stm->error = 1;
			return fz_rethrow(n, "read error");
		}
		else if (n == 0)
		{
			stm->eof = 1;
		}
		else if (n > 0)
		{
			stm->rp = stm->bp;
			stm->wp = stm->bp + n;
			stm->pos += n;
		}

		n = MIN(len - count, stm->wp - stm->rp);
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
		if (n < 0)
		{
			stm->error = 1;
			return fz_rethrow(n, "read error");
		}
		else if (n == 0)
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

	n = stm->read(stm, stm->bp, stm->ep - stm->bp);
	if (n < 0)
	{
		stm->error = 1;
		fz_catch(n, "read error; treating as end of file");
	}
	else if (n == 0)
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

fz_error
fz_read_all(fz_buffer **bufp, fz_stream *stm, int initial)
{
	fz_buffer *buf;
	int n;

	if (initial < 1024)
		initial = 1024;

	buf = fz_new_buffer(initial);

	while (1)
	{
		if (buf->len == buf->cap)
			fz_grow_buffer(buf);

		if (buf->len / 200 > initial)
		{
			fz_drop_buffer(buf);
			return fz_throw("compression bomb detected");
		}

		n = fz_read(stm, buf->data + buf->len, buf->cap - buf->len);
		if (n < 0)
		{
			fz_drop_buffer(buf);
			return fz_rethrow(n, "read error");
		}
		if (n == 0)
			break;

		buf->len += n;
	}

	*bufp = buf;
	return fz_okay;
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
	if (stm->seek)
	{
		if (whence == 1)
		{
			offset = fz_tell(stm) + offset;
			whence = 0;
		}
		if (whence == 0)
		{
			unsigned char *p = stm->wp - (stm->pos - offset);
			if (p >= stm->bp && p <= stm->wp)
			{
				stm->rp = p;
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
			fz_warn("cannot seek backwards");
		/* dog slow, but rare enough */
		while (offset-- > 0)
			fz_read_byte(stm);
	}
	else
		fz_warn("cannot seek");
}
