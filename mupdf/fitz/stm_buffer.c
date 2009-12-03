#include "fitz_base.h"
#include "fitz_stream.h"

fz_buffer *
fz_newbuffer(int size)
{
	fz_buffer *b;

	b = fz_malloc(sizeof(fz_buffer));
	b->refs = 1;
	b->ownsdata = 1;
	b->bp = fz_malloc(size);
	b->rp = b->bp;
	b->wp = b->bp;
	b->ep = b->bp + size;
	b->eof = 0;

	return b;
}

fz_buffer *
fz_newbufferwithmemory(unsigned char *data, int size)
{
	fz_buffer *b;

	b = fz_malloc(sizeof(fz_buffer));
	b->refs = 1;
	b->ownsdata = 0;
	b->bp = data;
	b->rp = b->bp;
	b->wp = b->bp + size;
	b->ep = b->bp + size;
	b->eof = 0;

	return b;
}

fz_buffer *
fz_keepbuffer(fz_buffer *buf)
{
	buf->refs ++;
	return buf;
}

void
fz_dropbuffer(fz_buffer *buf)
{
	if (--buf->refs == 0)
	{
		if (buf->ownsdata)
			fz_free(buf->bp);
		fz_free(buf);
	}
}

void
fz_growbuffer(fz_buffer *buf)
{
	int rp = buf->rp - buf->bp;
	int wp = buf->wp - buf->bp;
	int ep = buf->ep - buf->bp;

	if (!buf->ownsdata)
	{
		fz_warn("assert: grow borrowed memory");
		return;
	}

	buf->bp = fz_realloc(buf->bp, (ep * 3) / 2);
	buf->rp = buf->bp + rp;
	buf->wp = buf->bp + wp;
	buf->ep = buf->bp + (ep * 3) / 2;
}

void
fz_rewindbuffer(fz_buffer *buf)
{
	if (!buf->ownsdata)
	{
		fz_warn("assert: rewind borrowed memory");
		return;
	}

	memmove(buf->bp, buf->rp, buf->wp - buf->rp);
	buf->wp = buf->bp + (buf->wp - buf->rp);
	buf->rp = buf->bp;
}

