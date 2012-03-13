#include "fitz-internal.h"

fz_stream *
fz_new_stream(fz_context *ctx, void *state,
	int(*read)(fz_stream *stm, unsigned char *buf, int len),
	void(*close)(fz_context *ctx, void *state))
{
	fz_stream *stm;

	fz_try(ctx)
	{
		stm = fz_malloc_struct(ctx, fz_stream);
	}
	fz_catch(ctx)
	{
		close(ctx, state);
		fz_rethrow(ctx);
	}

	stm->refs = 1;
	stm->error = 0;
	stm->eof = 0;
	stm->pos = 0;

	stm->bits = 0;
	stm->avail = 0;
	stm->locked = 0;

	stm->bp = stm->buf;
	stm->rp = stm->bp;
	stm->wp = stm->bp;
	stm->ep = stm->buf + sizeof stm->buf;

	stm->state = state;
	stm->read = read;
	stm->close = close;
	stm->seek = NULL;
	stm->reopen = NULL;
	stm->ctx = ctx;

	return stm;
}

void
fz_lock_stream(fz_stream *stm)
{
	if (stm)
	{
		fz_lock(stm->ctx, FZ_LOCK_FILE);
		stm->locked = 1;
	}
}

fz_stream *
fz_keep_stream(fz_stream *stm)
{
	stm->refs ++;
	return stm;
}

void
fz_close(fz_stream *stm)
{
	if (!stm)
		return;
	stm->refs --;
	if (stm->refs == 0)
	{
		if (stm->close)
			stm->close(stm->ctx, stm->state);
		if (stm->locked)
			fz_unlock(stm->ctx, FZ_LOCK_FILE);
		fz_free(stm->ctx, stm);
	}
}

/* SumatraPDF: allow to clone a stream */
fz_stream *
fz_clone_stream(fz_context *ctx, fz_stream *stm)
{
	fz_stream *clone;
	if (!stm->reopen)
		fz_throw(ctx, "can't clone stream without reopening");
	clone = stm->reopen(ctx, stm);
	fz_seek(clone, fz_tell(stm), 0);
	return clone;
}

/* File stream */

static int read_file(fz_stream *stm, unsigned char *buf, int len)
{
	int n = read(*(int*)stm->state, buf, len);
	fz_assert_lock_held(stm->ctx, FZ_LOCK_FILE);
	if (n < 0)
		fz_throw(stm->ctx, "read error: %s", strerror(errno));
	return n;
}

static void seek_file(fz_stream *stm, int offset, int whence)
{
	int n = lseek(*(int*)stm->state, offset, whence);
	fz_assert_lock_held(stm->ctx, FZ_LOCK_FILE);
	if (n < 0)
		fz_throw(stm->ctx, "cannot lseek: %s", strerror(errno));
	stm->pos = n;
	stm->rp = stm->bp;
	stm->wp = stm->bp;
}

static void close_file(fz_context *ctx, void *state)
{
	int n = close(*(int*)state);
	if (n < 0)
		fz_warn(ctx, "close error: %s", strerror(errno));
	fz_free(ctx, state);
}

/* SumatraPDF: allow to clone a stream */
static fz_stream *reopen_file(fz_context *ctx, fz_stream *stm)
{
	return fz_open_fd(ctx, dup(*(int *)stm->state));
}

fz_stream *
fz_open_fd(fz_context *ctx, int fd)
{
	fz_stream *stm;
	int *state;

	state = fz_malloc_struct(ctx, int);
	*state = fd;

	fz_try(ctx)
	{
		stm = fz_new_stream(ctx, state, read_file, close_file);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}
	stm->seek = seek_file;
	stm->reopen = reopen_file;

	return stm;
}

fz_stream *
fz_open_file(fz_context *ctx, const char *name)
{
	int fd = open(name, O_BINARY | O_RDONLY, 0);
	if (fd == -1)
		fz_throw(ctx, "cannot open %s", name);
	return fz_open_fd(ctx, fd);
}

#ifdef _WIN32
fz_stream *
fz_open_file_w(fz_context *ctx, const wchar_t *name)
{
	int fd = _wopen(name, O_BINARY | O_RDONLY, 0);
	if (fd == -1)
		return NULL;
	return fz_open_fd(ctx, fd);
}
#endif

/* Memory stream */

static int read_buffer(fz_stream *stm, unsigned char *buf, int len)
{
	return 0;
}

static void seek_buffer(fz_stream *stm, int offset, int whence)
{
	if (whence == 0)
		stm->rp = stm->bp + offset;
	if (whence == 1)
		stm->rp += offset;
	if (whence == 2)
		stm->rp = stm->ep - offset;
	stm->rp = CLAMP(stm->rp, stm->bp, stm->ep);
	stm->wp = stm->ep;
}

static void close_buffer(fz_context *ctx, void *state_)
{
	fz_buffer *state = (fz_buffer *)state_;
	if (state)
		fz_drop_buffer(ctx, state);
}

/* SumatraPDF: allow to clone a stream */
static fz_stream *reopen_buffer(fz_context *ctx, fz_stream *stm)
{
	fz_stream *clone;

	fz_buffer *buf = fz_new_buffer(ctx, stm->ep - stm->bp);
	memcpy(buf->data, stm->bp, (buf->len = buf->cap));
	clone = fz_open_buffer(ctx, buf);
	fz_drop_buffer(ctx, buf);

	return clone;
}

fz_stream *
fz_open_buffer(fz_context *ctx, fz_buffer *buf)
{
	fz_stream *stm;

	fz_keep_buffer(ctx, buf);
	stm = fz_new_stream(ctx, buf, read_buffer, close_buffer);
	stm->seek = seek_buffer;
	stm->reopen = reopen_buffer;

	stm->bp = buf->data;
	stm->rp = buf->data;
	stm->wp = buf->data + buf->len;
	stm->ep = buf->data + buf->len;

	stm->pos = buf->len;

	return stm;
}

fz_stream *
fz_open_memory(fz_context *ctx, unsigned char *data, int len)
{
	fz_stream *stm;

	stm = fz_new_stream(ctx, NULL, read_buffer, close_buffer);
	stm->seek = seek_buffer;
	stm->reopen = reopen_buffer;

	stm->bp = data;
	stm->rp = data;
	stm->wp = data + len;
	stm->ep = data + len;

	stm->pos = len;

	return stm;
}
