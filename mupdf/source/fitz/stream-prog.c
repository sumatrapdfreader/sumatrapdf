#include "mupdf/fitz/stream.h"
#include "mupdf/fitz/string.h"

#if defined(_WIN32) && !defined(NDEBUG)
#include "windows.h"

static void
show_progress(int av, int pos)
{
	char text[80];
	sprintf(text, "Have %d, Want %d\n", av, pos);
	OutputDebugStringA(text);
}
#else
#define show_progress(A,B) do {} while (0)
#endif

/* File stream - progressive reading to simulate http download */

typedef struct prog_state
{
	int fd;
	int length;
	int available;
	int bps;
	clock_t start_time;
} prog_state;

static int read_prog(fz_stream *stm, unsigned char *buf, int len)
{
	prog_state *ps = (prog_state *)stm->state;
	int n;

	/* Simulate more data having arrived */
	if (ps->available < ps->length)
	{
		int av = (int)((float)(clock() - ps->start_time) * ps->bps / (CLOCKS_PER_SEC*8));
		if (av > ps->length)
			av = ps->length;
		ps->available = av;
		/* Limit any fetches to be within the data we have */
		if (av < ps->length && len + stm->pos > av)
		{
			len = av - stm->pos;
			if (len <= 0)
			{
				show_progress(av, stm->pos);
				fz_throw(stm->ctx, FZ_ERROR_TRYLATER, "Not enough data yet");
			}
		}
	}

	n = (len > 0 ? read(ps->fd, buf, len) : 0);
	if (n < 0)
		fz_throw(stm->ctx, FZ_ERROR_GENERIC, "read error: %s", strerror(errno));
	return n;
}

static void seek_prog(fz_stream *stm, int offset, int whence)
{
	prog_state *ps = (prog_state *)stm->state;
	int n;

	/* Simulate more data having arrived */
	if (ps->available < ps->length)
	{
		int av = (int)((float)(clock() - ps->start_time) * ps->bps / (CLOCKS_PER_SEC*8));
		if (av > ps->length)
			av = ps->length;
		ps->available = av;
	}
	if (ps->available < ps->length)
	{
		if (whence == SEEK_END)
		{
			show_progress(ps->available, ps->length);
			fz_throw(stm->ctx, FZ_ERROR_TRYLATER, "Not enough data to seek to end yet");
		}
	}
	if (whence == SEEK_CUR)
	{
		whence = SEEK_SET;
		offset += stm->pos;
		if (offset > ps->available)
		{
			show_progress(ps->available, offset);
			fz_throw(stm->ctx, FZ_ERROR_TRYLATER, "Not enough data to seek (relatively) to offset yet");
		}
	}
	if (whence == SEEK_SET)
	{
		if (offset > ps->available)
		{
			show_progress(ps->available, offset);
			fz_throw(stm->ctx, FZ_ERROR_TRYLATER, "Not enough data to seek to offset yet");
		}
	}

	n = lseek(ps->fd, offset, whence);
	if (n < 0)
		fz_throw(stm->ctx, FZ_ERROR_GENERIC, "cannot lseek: %s", strerror(errno));
	stm->pos = n;
	stm->rp = stm->bp;
	stm->wp = stm->bp;
}

static void close_prog(fz_context *ctx, void *state)
{
	prog_state *ps = (prog_state *)state;
	int n = close(ps->fd);
	if (n < 0)
		fz_warn(ctx, "close error: %s", strerror(errno));
	fz_free(ctx, state);
}

static int meta_prog(fz_stream *stm, int key, int size, void *ptr)
{
	prog_state *ps = (prog_state *)stm->state;
	switch(key)
	{
	case FZ_STREAM_META_PROGRESSIVE:
		return 1;
		break;
	case FZ_STREAM_META_LENGTH:
		return ps->length;
	}
	return -1;
}

fz_stream *
fz_open_fd_progressive(fz_context *ctx, int fd, int bps)
{
	fz_stream *stm;
	prog_state *state;

	state = fz_malloc_struct(ctx, prog_state);
	state->fd = fd;
	state->bps = bps;
	state->start_time = clock();
	state->available = 0;

	state->length = lseek(state->fd, 0, SEEK_END);
	lseek(state->fd, 0, SEEK_SET);

	fz_try(ctx)
	{
		stm = fz_new_stream(ctx, state, read_prog, close_prog);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}
	stm->seek = seek_prog;
	stm->meta = meta_prog;

	return stm;
}

fz_stream *
fz_open_file_progressive(fz_context *ctx, const char *name, int bps)
{
#ifdef _WIN32
	char *s = (char*)name;
	wchar_t *wname, *d;
	int c, fd;
	d = wname = fz_malloc(ctx, (strlen(name)+1) * sizeof(wchar_t));
	while (*s) {
		s += fz_chartorune(&c, s);
		*d++ = c;
	}
	*d = 0;
	fd = _wopen(wname, O_BINARY | O_RDONLY, 0);
	fz_free(ctx, wname);
#else
	int fd = open(name, O_BINARY | O_RDONLY, 0);
#endif
	if (fd == -1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open %s", name);
	return fz_open_fd_progressive(ctx, fd, bps);
}
