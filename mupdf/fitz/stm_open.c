/*
 * Creation and destruction.
 */

#include "fitz_base.h"
#include "fitz_stream.h"

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

#ifdef WIN32
fz_error fz_openrfilew(fz_stream **stmp, wchar_t *path)
{
	fz_stream *stm;

	stm = newstm(FZ_SFILE);

	stm->buffer = fz_newbuffer(FZ_BUFSIZE);

	stm->file = _wopen(path, O_BINARY | O_RDONLY, 0666);
	if (stm->file < 0)
	{
		fz_dropbuffer(stm->buffer);
		fz_free(stm);
		return fz_throw("syserr: open '%s': %s", path, strerror(errno));
	}

	*stmp = stm;
	return fz_okay;
}
#endif

fz_error fz_openrfile(fz_stream **stmp, char *path)
{
	fz_stream *stm;

	stm = newstm(FZ_SFILE);

	stm->buffer = fz_newbuffer(FZ_BUFSIZE);

	stm->file = open(path, O_BINARY | O_RDONLY, 0666);
	if (stm->file < 0)
	{
		fz_dropbuffer(stm->buffer);
		fz_free(stm);
		return fz_throw("syserr: open '%s': %s", path, strerror(errno));
	}

	*stmp = stm;
	return fz_okay;
}

fz_stream * fz_openrfilter(fz_filter *flt, fz_stream *src)
{
	fz_stream *stm;

	stm = newstm(FZ_SFILTER);
	stm->buffer = fz_newbuffer(FZ_BUFSIZE);
	stm->chain = fz_keepstream(src);
	stm->filter = fz_keepfilter(flt);

	return stm;
}

fz_stream * fz_openrbuffer(fz_buffer *buf)
{
	fz_stream *stm;

	stm = newstm(FZ_SBUFFER);
	stm->buffer = fz_keepbuffer(buf);
	stm->buffer->eof = 1;

	return stm;
}

fz_stream * fz_openrmemory(unsigned char *mem, int len)
{
	fz_buffer *buf;
	fz_stream *stm;

	buf = fz_newbufferwithmemory(mem, len);
	stm = fz_openrbuffer(buf);
	fz_dropbuffer(buf);

	return stm;
}
