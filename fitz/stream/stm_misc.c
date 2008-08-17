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
	int c;

	if (sizehint == 0)
	    sizehint = 4 * 1024;

	error = fz_newbuffer(&buf, sizehint);
	if (error)
	    return fz_rethrow(error, "cannot create scratch buffer");

	for (c = fz_readbyte(stm); c != EOF; c = fz_readbyte(stm))
	{
		if (buf->wp == buf->ep)
		{
		    error = fz_growbuffer(buf);
		    if (error)
		    {
			fz_dropbuffer(buf);
			return fz_rethrow(error, "cannot resize scratch buffer");
		    }
		}

		*buf->wp++ = c;
	}

	*bufp = buf;
	return fz_okay;
}

