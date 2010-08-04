#include "fitz.h"

fz_stream *
fz_newstream(void *state,
	int(*read)(fz_stream *stm, unsigned char *buf, int len),
	void(*close)(fz_stream *stm))
{
	fz_stream *stm;

	stm = fz_malloc(sizeof(fz_stream));

	stm->refs = 1;
	stm->dead = 0;
	stm->pos = 0;

	stm->bp = stm->buf;
	stm->rp = stm->bp;
	stm->wp = stm->bp;
	stm->ep = stm->buf + sizeof stm->buf;

	stm->state = state;
	stm->read = read;
	stm->close = close;
	stm->seek = nil;

	return stm;
}

fz_stream *
fz_keepstream(fz_stream *stm)
{
	stm->refs ++;
	return stm;
}

void
fz_close(fz_stream *stm)
{
	stm->refs --;
	if (stm->refs == 0)
	{
		if (stm->close)
			stm->close(stm);
		fz_free(stm);
	}
}

/* File stream */

static int readfile(fz_stream *stm, unsigned char *buf, int len)
{
	int n = read(*(int*)stm->state, buf, len);
	if (n < 0)
		return fz_throw("read error: %s", strerror(errno));
	return n;
}

static void seekfile(fz_stream *stm, int offset, int whence)
{
	int n = lseek(*(int*)stm->state, offset, whence);
	if (n < 0)
		fz_warn("cannot lseek: %s", strerror(errno));
	stm->pos = n;
	stm->rp = stm->bp;
	stm->wp = stm->bp;
}

static void closefile(fz_stream *stm)
{
	int n = close(*(int*)stm->state);
	if (n < 0)
		fz_warn("close error: %s", strerror(errno));
	fz_free(stm->state);
}

fz_stream *
fz_openfile(int fd)
{
	fz_stream *stm;
	int *state;

	state = fz_malloc(sizeof(int));
	*state = fd;

	stm = fz_newstream(state, readfile, closefile);
	stm->seek = seekfile;

	return stm;
}

/* Memory stream */

static int readbuffer(fz_stream *stm, unsigned char *buf, int len)
{
	return 0;
}

static void seekbuffer(fz_stream *stm, int offset, int whence)
{
	if (whence == 0)
		stm->rp = stm->bp + offset;
	if (whence == 1)
		stm->rp += offset;
	if (whence == 2)
		stm->rp = stm->wp - offset;
	stm->rp = CLAMP(stm->rp, stm->bp, stm->wp);
}

static void closebuffer(fz_stream *stm)
{
	fz_dropbuffer(stm->state);
}

fz_stream *
fz_openbuffer(fz_buffer *buf)
{
	fz_stream *stm;

	stm = fz_newstream(fz_keepbuffer(buf), readbuffer, closebuffer);
	stm->seek = seekbuffer;

	stm->bp = buf->data;
	stm->rp = buf->data;
	stm->wp = buf->data + buf->len;
	stm->ep = buf->data + buf->len;

	stm->pos = buf->len;

	return stm;
}
