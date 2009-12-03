/*
 * Input streams.
 */

#include "fitz_base.h"
#include "fitz_stream.h"

fz_error
fz_readimp(fz_stream *stm)
{
	fz_buffer *buf = stm->buffer;
	fz_error error;
	fz_error reason;
	int produced;
	int n;

	if (stm->dead)
		return fz_throw("assert: read from dead stream");

	if (buf->eof)
		return fz_okay;

	fz_rewindbuffer(buf);
	if (buf->ep - buf->wp == 0)
		fz_growbuffer(buf);

	switch (stm->kind)
	{

	case FZ_SFILE:
		n = read(stm->file, buf->wp, buf->ep - buf->wp);
		if (n == -1)
		{
			stm->dead = 1;
			return fz_throw("syserr: read: %s", strerror(errno));
		}

		if (n == 0)
			buf->eof = 1;
		buf->wp += n;

		return fz_okay;

	case FZ_SFILTER:
		produced = 0;

		while (1)
		{
			reason = fz_process(stm->filter, stm->chain->buffer, buf);

			if (stm->filter->produced)
				produced = 1;

			if (reason == fz_ioneedin)
			{
				error = fz_readimp(stm->chain);
				if (error)
				{
					stm->dead = 1;
					return fz_rethrow(error, "cannot read from input stream");
				}
			}

			else if (reason == fz_ioneedout)
			{
				if (produced)
					return 0;

				if (buf->rp > buf->bp)
					fz_rewindbuffer(buf);
				else
					fz_growbuffer(buf);
			}

			else if (reason == fz_iodone)
			{
				return fz_okay;
			}

			else
			{
				stm->dead = 1;
				return fz_rethrow(reason, "cannot process filter");
			}
		}

	case FZ_SBUFFER:
		return fz_okay;

	default:
		return fz_throw("assert: unknown stream type");
	}
}

int
fz_tell(fz_stream *stm)
{
	fz_buffer *buf = stm->buffer;
	int t;

	if (stm->dead)
		return EOF;

	switch (stm->kind)
	{
	case FZ_SFILE:
		t = lseek(stm->file, 0, 1);
		if (t < 0)
		{
			fz_warn("syserr: lseek: %s", strerror(errno));
			stm->dead = 1;
			return EOF;
		}
		return t - (buf->wp - buf->rp);

	case FZ_SFILTER:
		return stm->filter->count - (buf->wp - buf->rp);

	case FZ_SBUFFER:
		return buf->rp - buf->bp;

	default:
		return EOF;
	}
}

fz_error
fz_seek(fz_stream *stm, int offset, int whence)
{
	fz_error error;
	fz_buffer *buf = stm->buffer;
	int t, c;

	if (stm->dead)
		return fz_throw("assert: seek in dead stream");

	if (whence == 1)
	{
		int cur = fz_tell(stm);
		if (cur < 0)
			return fz_throw("cannot tell current position");
		offset = cur + offset;
		whence = 0;
	}

	buf->eof = 0;

	switch (stm->kind)
	{
	case FZ_SFILE:
		t = lseek(stm->file, offset, whence);
		if (t < 0)
		{
			stm->dead = 1;
			return fz_throw("syserr: lseek: %s", strerror(errno));
		}

		buf->rp = buf->bp;
		buf->wp = buf->bp;

		return fz_okay;

	case FZ_SFILTER:
		if (whence == 0)
		{
			if (offset < fz_tell(stm))
			{
				stm->dead = 1;
				return fz_throw("assert: seek backwards in filter");
			}
			while (fz_tell(stm) < offset)
			{
				c = fz_readbyte(stm);
				if (c == EOF)
				{
					error = fz_readerror(stm);
					if (error)
						return fz_rethrow(error, "cannot seek forward in filter");
					break;
				}
			}
			return fz_okay;
		}

		stm->dead = 1;
		return fz_throw("assert: relative seek in filter");

	case FZ_SBUFFER:
		if (whence == 0)
			buf->rp = CLAMP(buf->bp + offset, buf->bp, buf->ep);
		else
			buf->rp = CLAMP(buf->ep + offset, buf->bp, buf->ep);
		return fz_okay;

	default:
		return fz_throw("unknown stream type");
	}
}

fz_error
fz_read(int *np, fz_stream *stm, unsigned char *mem, int n)
{
	fz_error error;
	fz_buffer *buf = stm->buffer;
	int i = 0;

	while (i < n)
	{
		while (buf->rp < buf->wp && i < n)
			mem[i++] = *buf->rp++;

		if (buf->rp == buf->wp)
		{
			if (buf->eof)
			{
				*np = i;
				return fz_okay;
			}

			error = fz_readimp(stm);
			if (error)
				return fz_rethrow(error, "cannot produce data");
		}
	}

	*np = i;
	return fz_okay;
}

fz_error
fz_readerror(fz_stream *stm)
{
	fz_error error;
	if (stm->error)
	{
		error = stm->error;
		stm->error = fz_okay;
		return fz_rethrow(error, "delayed read error");
	}
	return fz_okay;
}

int
fz_readbytex(fz_stream *stm)
{
	fz_buffer *buf = stm->buffer;

	if (buf->rp == buf->wp)
	{
		if (!buf->eof && !stm->error)
		{
			fz_error error = fz_readimp(stm);
			if (error)
				stm->error = fz_rethrow(error, "cannot read data");
		}
	}

	return buf->rp < buf->wp ? *buf->rp++ : EOF ;
}

int
fz_peekbytex(fz_stream *stm)
{
	fz_buffer *buf = stm->buffer;

	if (buf->rp == buf->wp)
	{
		if (!buf->eof && !stm->error)
		{
			fz_error error = fz_readimp(stm);
			if (error)
				stm->error = fz_rethrow(error, "cannot read data");
		}
	}

	return buf->rp < buf->wp ? *buf->rp : EOF ;
}

