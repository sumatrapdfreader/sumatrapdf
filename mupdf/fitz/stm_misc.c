/*
 * Miscellaneous I/O functions
 */

#include "fitz_base.h"
#include "fitz_stream.h"

/*
 * Read a line terminated by LF or CR or CRLF.
 */

fz_error
fz_readline(fz_stream *stm, char *mem, int n)
{
	fz_error error;

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
				c = fz_readbyte(stm);
			break;
		}
		if (c == '\n')
			break;
		*s++ = c;
		n--;
	}
	if (n)
		*s = '\0';

	error = fz_readerror(stm);
	if (error)
		return fz_rethrow(error, "cannot read line");
	return fz_okay;
}

static inline int fz_fillbuf(fz_stream *stm, fz_buffer *buf)
{
	int remaining = buf->ep - buf->wp;
	int available = stm->buffer->wp - stm->buffer->rp;

	if (available == 0 && remaining > 0)
	{
		int c = fz_readbytex(stm);
		if (c == EOF)
			return EOF;

		*buf->wp++ = c;

		remaining = buf->ep - buf->wp;
		available = stm->buffer->wp - stm->buffer->rp;
	}

	memmove(buf->wp, stm->buffer->rp, MIN(remaining, available));
	buf->wp += MIN(remaining, available);
	stm->buffer->rp += MIN(remaining, available);

	if (stm->buffer->rp == stm->buffer->wp && stm->buffer->eof)
		return EOF;
	return 0;
}

/*
 * Utility function to consume all the contents of an input stream into
 * a freshly allocated buffer.
 */

fz_buffer *
fz_readall(fz_stream *stm, int sizehint)
{
	fz_buffer *buf;

	if (sizehint == 0)
		sizehint = 4 * 1024;

	buf = fz_newbuffer(sizehint);

	while (fz_fillbuf(stm, buf) != EOF)
	{
		if (buf->wp == buf->ep)
			fz_growbuffer(buf);
	}

	return buf;
}
