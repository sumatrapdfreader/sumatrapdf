/*
 * Miscellaneous I/O functions
 */

#include "fitz-base.h"
#include "fitz-stream.h"

int
fz_tell(fz_stream *stm)
{
	if (stm->mode == FZ_SREAD)
		return fz_rtell(stm);
	return fz_wtell(stm);
}

fz_error *
fz_seek(fz_stream *stm, int offset, int whence)
{
	if (stm->mode == FZ_SREAD)
		return fz_rseek(stm, offset, whence);
	return fz_wseek(stm, offset, whence);
}

/*
 * Read a line terminated by LF or CR or CRLF.
 */

fz_error *
fz_readline(fz_stream *stm, char *mem, int n)
{
	fz_error *error;

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

/*
 * Utility function to consume all the contents of an input stream into
 * a freshly allocated buffer.
 */

fz_error *
fz_readall(fz_buffer **bufp, fz_stream *stm, int sizehint)
{
	fz_error *error;
	fz_buffer *buf;
	fz_buffer *tmpbuf;
	int bytesread;
	int leftinbuf;

	if (sizehint == 0)
	    sizehint = 4 * 1024;

	error = fz_newbuffer(&tmpbuf, sizehint);
	if (error)
		return fz_rethrow(error, "cannot create scratch buffer");

	buf = stm->buffer;
	while (!buf->eof)
	{
		error = fz_readimp(stm);
		if (error)
		{
			error = fz_rethrow(error, "cannot read data");
			goto cleanup;
		}
		bytesread = buf->wp - buf->rp;
		if (0 == bytesread)
		{
			if (buf->eof)
				break;
			assert(0);
		}
		leftinbuf = tmpbuf->ep - tmpbuf->wp;
		while (leftinbuf < bytesread)
		{
			error = fz_growbuffer(tmpbuf);
			if (error)
			{
				error = fz_rethrow(error, "cannot resize scratch buffer");
				goto cleanup;
			}
			leftinbuf = tmpbuf->ep - tmpbuf->wp;
		}
		memcpy(tmpbuf->wp, buf->rp, bytesread);
		buf->rp += bytesread;
		tmpbuf->wp += bytesread;
	}

	*bufp = tmpbuf;
	return fz_okay;
cleanup:
	fz_dropbuffer(tmpbuf);
	return error;
}

