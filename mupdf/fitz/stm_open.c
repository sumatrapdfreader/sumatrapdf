/*
 * Creation and destruction.
 */

#include "fitz.h"

static fz_stream *
newstm(int kind)
{
	fz_stream *stm;

	stm = fz_malloc(sizeof(fz_stream));

	stm->refs = 1;
	stm->kind = kind;
	stm->dead = 0;
	stm->error = fz_okay;
	stm->buffer = nil;

	stm->chain = nil;
	stm->filter = nil;
	stm->file = -1;

	return stm;
}

fz_stream *
fz_keepstream(fz_stream *stm)
{
	stm->refs ++;
	return stm;
}

void
fz_dropstream(fz_stream *stm)
{
	stm->refs --;
	if (stm->refs == 0)
	{
		if (stm->error)
		{
			fz_catch(stm->error, "dropped unhandled ioerror");
			stm->error = fz_okay;
		}

		switch (stm->kind)
		{
		case FZ_SFILE:
			close(stm->file);
			break;
		case FZ_SFILTER:
			fz_dropfilter(stm->filter);
			fz_dropstream(stm->chain);
			break;
		case FZ_SBUFFER:
			break;
		}

		fz_dropbuffer(stm->buffer);
		fz_free(stm);
	}
}

fz_stream *
fz_openfile(int fd)
{
	fz_stream *stm;

	stm = newstm(FZ_SFILE);
	stm->buffer = fz_newbuffer(FZ_BUFSIZE);
	stm->file = fd;

	return stm;
}

fz_stream *
fz_openfilter(fz_filter *flt, fz_stream *src)
{
	fz_stream *stm;

	stm = newstm(FZ_SFILTER);
	stm->buffer = fz_newbuffer(FZ_BUFSIZE);
	stm->chain = fz_keepstream(src);
	stm->filter = fz_keepfilter(flt);

	return stm;
}

fz_stream *
fz_openbuffer(fz_buffer *buf)
{
	fz_stream *stm;

	stm = newstm(FZ_SBUFFER);
	stm->buffer = fz_keepbuffer(buf);
	stm->buffer->eof = 1;

	return stm;
}

fz_stream *
fz_openmemory(unsigned char *mem, int len)
{
	fz_buffer *buf;
	fz_stream *stm;

	buf = fz_newbufferwithmemory(mem, len);
	stm = fz_openbuffer(buf);
	fz_dropbuffer(buf);

	return stm;
}
