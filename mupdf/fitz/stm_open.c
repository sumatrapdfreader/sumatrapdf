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
	if (!stm)
		return nil;

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

fz_error fz_openrfile(fz_stream **stmp, char *path)
{
	fz_error error;
	fz_stream *stm;

	stm = newstm(FZ_SFILE);
	if (!stm)
		return fz_rethrow(-1, "out of memory: stream struct");

	error = fz_newbuffer(&stm->buffer, FZ_BUFSIZE);
	if (error)
	{
		fz_free(stm);
		return fz_rethrow(error, "cannot create buffer");
	}

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

fz_error fz_openrfilter(fz_stream **stmp, fz_filter *flt, fz_stream *src)
{
	fz_error error;
	fz_stream *stm;

	stm = newstm(FZ_SFILTER);
	if (!stm)
		return fz_rethrow(-1, "out of memory: stream struct");

	error = fz_newbuffer(&stm->buffer, FZ_BUFSIZE);
	if (error)
	{
		fz_free(stm);
		return fz_rethrow(error, "cannot create buffer");
	}

	stm->chain = fz_keepstream(src);
	stm->filter = fz_keepfilter(flt);

	*stmp = stm;
	return fz_okay;
}

fz_error fz_openrbuffer(fz_stream **stmp, fz_buffer *buf)
{
	fz_stream *stm;

	stm = newstm(FZ_SBUFFER);
	if (!stm)
		return fz_rethrow(-1, "out of memory: stream struct");

	stm->buffer = fz_keepbuffer(buf);

	stm->buffer->eof = 1;

	*stmp = stm;
	return fz_okay;
}

fz_error fz_openrmemory(fz_stream **stmp, unsigned char *mem, int len)
{
	fz_error error;
	fz_buffer *buf;

	error = fz_newbufferwithmemory(&buf, mem, len);
	if (error)
		return fz_rethrow(error, "cannot create memory buffer");

	error = fz_openrbuffer(stmp, buf);
	if (error)
	{
		fz_dropbuffer(buf);
		return fz_rethrow(error, "cannot open memory buffer stream");
	}

	fz_dropbuffer(buf);

	return fz_okay;
}

