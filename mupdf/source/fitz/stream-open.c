#include "mupdf/fitz.h"

void fz_rebind_stream(fz_stream *stm, fz_context *ctx)
{
	if (stm == NULL || stm->ctx == ctx)
		return;
	do {
		stm->ctx = ctx;
		stm = (stm->rebind == NULL ? NULL : stm->rebind(stm));
	} while (stm != NULL);
}

fz_stream *
fz_new_stream(fz_context *ctx, void *state,
	fz_stream_next_fn *next,
	fz_stream_close_fn *close,
	fz_stream_rebind_fn *rebind)
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

	stm->rp = NULL;
	stm->wp = NULL;

	stm->state = state;
	stm->next = next;
	stm->close = close;
	stm->seek = NULL;
	stm->rebind = rebind;
	stm->reopen = NULL;
	stm->ctx = ctx;

	return stm;
}

fz_stream *
fz_keep_stream(fz_stream *stm)
{
	if (stm)
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
		fz_free(stm->ctx, stm);
	}
}

/* SumatraPDF: allow to clone a stream */
fz_stream *
fz_clone_stream(fz_context *ctx, fz_stream *stm)
{
	fz_stream *clone;
	if (!stm->reopen)
		fz_throw(ctx, FZ_ERROR_GENERIC, "can't clone stream without reopening");
	clone = stm->reopen(ctx, stm);
	fz_seek(clone, fz_tell(stm), 0);
	return clone;
}

/* File stream */

typedef struct fz_file_stream_s
{
	int file;
	unsigned char buffer[4096];
} fz_file_stream;

static int next_file(fz_stream *stm, int n)
{
	fz_file_stream *state = stm->state;

	/* n is only a hint, that we can safely ignore */
	n = read(state->file, state->buffer, sizeof(state->buffer));
	if (n < 0)
		fz_throw(stm->ctx, FZ_ERROR_GENERIC, "read error: %s", strerror(errno));
	stm->rp = state->buffer;
	stm->wp = state->buffer + n;
	stm->pos += n;

	if (n == 0)
		return EOF;
	return *stm->rp++;
}

static void seek_file(fz_stream *stm, int offset, int whence)
{
	fz_file_stream *state = stm->state;
	int n = lseek(state->file, offset, whence);
	if (n < 0)
		fz_throw(stm->ctx, FZ_ERROR_GENERIC, "cannot lseek: %s", strerror(errno));
	stm->pos = n;
	stm->rp = state->buffer;
	stm->wp = state->buffer;
}

static void close_file(fz_context *ctx, void *state_)
{
	fz_file_stream *state = state_;
	int n = close(state->file);
	if (n < 0)
		fz_warn(ctx, "close error: %s", strerror(errno));
	fz_free(ctx, state);
}

fz_stream *
fz_open_fd(fz_context *ctx, int fd)
{
	fz_stream *stm;
	fz_file_stream *state = fz_malloc_struct(ctx, fz_file_stream);
	state->file = fd;

	fz_try(ctx)
	{
		stm = fz_new_stream(ctx, state, next_file, close_file, NULL);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}
	stm->seek = seek_file;
	/* SumatraPDF: TODO: can't reliably clone a file descriptor */

	return stm;
}

fz_stream *
fz_open_file(fz_context *ctx, const char *name)
{
#ifdef _WIN32
	char *s = (char*)name;
	wchar_t *wname, *d;
	int c, fd;
	/* SumatraPDF: prefer ANSI to UTF-8 for consistency with remaining API */
	fd = open(name, O_BINARY | O_RDONLY, 0);
	if (fd == -1)
	{

	d = wname = fz_malloc(ctx, (strlen(name)+1) * sizeof(wchar_t));
	while (*s) {
		s += fz_chartorune(&c, s);
		*d++ = c;
	}
	*d = 0;
	fd = _wopen(wname, O_BINARY | O_RDONLY, 0);
	fz_free(ctx, wname);

	}
#else
	int fd = open(name, O_BINARY | O_RDONLY, 0);
#endif
	if (fd == -1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open %s", name);
	return fz_open_fd(ctx, fd);
}

#ifdef _WIN32
fz_stream *
fz_open_file_w(fz_context *ctx, const wchar_t *name)
{
	int fd = _wopen(name, O_BINARY | O_RDONLY, 0);
	if (fd == -1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file %ls", name);
	return fz_open_fd(ctx, fd);
}
#endif

/* Memory stream */

static int next_buffer(fz_stream *stm, int max)
{
	return EOF;
}

static void seek_buffer(fz_stream *stm, int offset, int whence)
{
	int pos = stm->pos - (stm->wp - stm->rp);
	/* Convert to absolute pos */
	if (whence == 1)
	{
		offset += pos; /* Was relative to current pos */
	}
	else if (whence == 2)
	{
		offset += stm->pos; /* Was relative to end */
	}

	if (offset < 0)
		offset = 0;
	if (offset > stm->pos)
		offset = stm->pos;
	stm->rp += offset - pos;
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

	fz_buffer *orig = stm->state;
	fz_buffer *buf = fz_new_buffer_from_data(ctx, orig->data, orig->len);
	clone = fz_open_buffer(ctx, buf);
	fz_drop_buffer(ctx, buf);

	return clone;
}

fz_stream *
fz_open_buffer(fz_context *ctx, fz_buffer *buf)
{
	fz_stream *stm;

	fz_keep_buffer(ctx, buf);
	stm = fz_new_stream(ctx, buf, next_buffer, close_buffer, NULL);
	stm->seek = seek_buffer;
	stm->reopen = reopen_buffer;

	stm->rp = buf->data;
	stm->wp = buf->data + buf->len;

	stm->pos = buf->len;

	return stm;
}

fz_stream *
fz_open_memory(fz_context *ctx, unsigned char *data, int len)
{
	fz_stream *stm;

	stm = fz_new_stream(ctx, NULL, next_buffer, close_buffer, NULL);
	stm->seek = seek_buffer;
	stm->reopen = reopen_buffer;

	stm->rp = data;
	stm->wp = data + len;

	stm->pos = len;

	return stm;
}
