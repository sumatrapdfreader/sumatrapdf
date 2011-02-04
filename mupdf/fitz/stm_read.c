#include "fitz.h"

int
fz_read(fz_stream *stm, unsigned char *buf, int len)
{
	int avail, count;

	avail = stm->wp - stm->rp;
	if (avail)
	{
		count = MIN(len, avail);
		memcpy(buf, stm->rp, count);
		stm->rp += count;
	}
	else
	{
		count = 0;
	}

	if (stm->dead)
		return count;

	while (len > count)
	{
		int n = stm->read(stm, buf + count, len - count);
		if (n < 0)
		{
			stm->dead = 1;
			return fz_rethrow(n, "read error");
		}
		if (n == 0)
			break;
		stm->pos += n;
		count += n;
	}

	return count;
}

void
fz_fillbuffer(fz_stream *stm)
{
	int n;

	assert(stm->rp == stm->wp);

	n = fz_read(stm, stm->bp, stm->ep - stm->bp);
	if (n < 0)
		fz_catch(n, "read error; treating as end of file");
	else if (n > 0)
	{
		stm->rp = stm->bp;
		stm->wp = stm->bp + n;
	}
}

fz_error
fz_readall(fz_buffer **bufp, fz_stream *stm, int initial)
{
	fz_buffer *buf;
	int n;

	if (initial < 1024)
		initial = 1024;

	buf = fz_newbuffer(initial);

	while (1)
	{
		if (buf->len == buf->cap)
			fz_growbuffer(buf);

		if (buf->len > initial * 200)
		{
			fz_dropbuffer(buf);
			return fz_throw("compression bomb detected");
		}

		n = fz_read(stm, buf->data + buf->len, buf->cap - buf->len);
		if (n < 0)
		{
			fz_dropbuffer(buf);
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
fz_readline(fz_stream *stm, char *mem, int n)
{
	char *s = mem;
	int c = EOF;
	while (n > 1)
	{
		c = fz_readbyte(stm);
		if (c == EOF)
			break;
		if (c == '\r') {
			c = fz_peekbyte(stm);
			if (c == '\n')
				fz_readbyte(stm);
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

		stm->seek(stm, offset, whence);
	}
	else if (whence != 2)
	{
		if (whence == 0)
			offset -= fz_tell(stm);
		if (offset < 0)
			fz_warn("cannot seek backwards");
		/* dog slow, but rare enough */
		while (offset-- > 0)
			fz_readbyte(stm);
	}
	else
		fz_warn("cannot seek");
}
