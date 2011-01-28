#include "fitz.h"

fz_buffer *
fz_newbuffer(int size)
{
	fz_buffer *b;

	size = size > 1 ? size : 16;

	b = fz_malloc(sizeof(fz_buffer));
	b->refs = 1;
	b->data = fz_malloc(size);
	b->cap = size;
	b->len = 0;

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
		fz_free(buf->data);
		fz_free(buf);
	}
}

void
fz_resizebuffer(fz_buffer *buf, int size)
{
	buf->data = fz_realloc(buf->data, size, 1);
	buf->cap = size;
	if (buf->len > buf->cap)
		buf->len = buf->cap;
}

void
fz_growbuffer(fz_buffer *buf)
{
	fz_resizebuffer(buf, (buf->cap * 3) / 2);
}
