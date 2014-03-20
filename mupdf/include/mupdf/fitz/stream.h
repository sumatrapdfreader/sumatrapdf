#ifndef MUPDF_FITZ_STREAM_H
#define MUPDF_FITZ_STREAM_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"

/*
	fz_stream is a buffered reader capable of seeking in both
	directions.

	Streams are reference counted, so references must be dropped
	by a call to fz_close.

	Only the data between rp and wp is valid.
*/
typedef struct fz_stream_s fz_stream;

/*
	fz_open_file: Open the named file and wrap it in a stream.

	filename: Path to a file. On non-Windows machines the filename should
	be exactly as it would be passed to open(2). On Windows machines, the
	path should be UTF-8 encoded so that non-ASCII characters can be
	represented. Other platforms do the encoding as standard anyway (and
	in most cases, particularly for MacOS and Linux, the encoding they
	use is UTF-8 anyway).
*/
fz_stream *fz_open_file(fz_context *ctx, const char *filename);

fz_stream *fz_open_fd_progressive(fz_context *ctx, int fd, int bps);
fz_stream *fz_open_file_progressive(fz_context *ctx, const char *filename, int bps);

/*
	fz_open_file_w: Open the named file and wrap it in a stream.

	This function is only available when compiling for Win32.

	filename: Wide character path to the file as it would be given
	to _wopen().
*/
fz_stream *fz_open_file_w(fz_context *ctx, const wchar_t *filename);

/*
	fz_open_fd: Wrap an open file descriptor in a stream.

	file: An open file descriptor supporting bidirectional
	seeking. The stream will take ownership of the file
	descriptor, so it may not be modified or closed after the call
	to fz_open_fd. When the stream is closed it will also close
	the file descriptor.
*/
fz_stream *fz_open_fd(fz_context *ctx, int file);

/*
	fz_open_memory: Open a block of memory as a stream.

	data: Pointer to start of data block. Ownership of the data block is
	NOT passed in.

	len: Number of bytes in data block.

	Returns pointer to newly created stream. May throw exceptions on
	failure to allocate.
*/
fz_stream *fz_open_memory(fz_context *ctx, unsigned char *data, int len);

/*
	fz_open_buffer: Open a buffer as a stream.

	buf: The buffer to open. Ownership of the buffer is NOT passed in
	(this function takes it's own reference).

	Returns pointer to newly created stream. May throw exceptions on
	failure to allocate.
*/
fz_stream *fz_open_buffer(fz_context *ctx, fz_buffer *buf);

/* SumatraPDF: allow to clone a stream */
fz_stream *fz_clone_stream(fz_context *ctx, fz_stream *stm);

/*
	fz_open_leecher: Attach a filter to a stream that will store any
	characters read from the stream into the supplied buffer.

	chain: The underlying stream to leech from.

	buf: The buffer into which the read data should be appended.
	The buffer will be resized as required.

	Returns pointer to newly created stream. May throw exceptions on
	failure to allocate.
*/
fz_stream *fz_open_leecher(fz_stream *chain, fz_buffer *buf);

/*
	fz_close: Close an open stream.

	Drops a reference for the stream. Once no references remain
	the stream will be closed, as will any file descriptor the
	stream is using.

	Does not throw exceptions.
*/
void fz_close(fz_stream *stm);

/*
	fz_tell: return the current reading position within a stream
*/
int fz_tell(fz_stream *stm);

/*
	fz_seek: Seek within a stream.

	stm: The stream to seek within.

	offset: The offset to seek to.

	whence: From where the offset is measured (see fseek).
*/
void fz_seek(fz_stream *stm, int offset, int whence);

/*
	fz_read: Read from a stream into a given data block.

	stm: The stream to read from.

	data: The data block to read into.

	len: The length of the data block (in bytes).

	Returns the number of bytes read. May throw exceptions.
*/
int fz_read(fz_stream *stm, unsigned char *data, int len);

/*
	fz_read_all: Read all of a stream into a buffer.

	stm: The stream to read from

	initial: Suggested initial size for the buffer.

	Returns a buffer created from reading from the stream. May throw
	exceptions on failure to allocate.
*/
fz_buffer *fz_read_all(fz_stream *stm, int initial);

enum
{
	FZ_STREAM_META_PROGRESSIVE = 1,
	FZ_STREAM_META_LENGTH = 2
};

int fz_stream_meta(fz_stream *stm, int key, int size, void *ptr);

void fz_rebind_stream(fz_stream *stm, fz_context *ctx);

typedef int (fz_stream_next_fn)(fz_stream *stm, int max);
typedef void (fz_stream_close_fn)(fz_context *ctx, void *state);
typedef void (fz_stream_seek_fn)(fz_stream *stm, int offset, int whence);
typedef int (fz_stream_meta_fn)(fz_stream *stm, int key, int size, void *ptr);
typedef fz_stream *(fz_stream_rebind_fn)(fz_stream *stm);
/* SumatraPDF: allow to clone a stream */
typedef fz_stream *(fz_stream_reopen_fn)(fz_context *ctx, fz_stream *stm);

struct fz_stream_s
{
	fz_context *ctx;
	int refs;
	int error;
	int eof;
	int pos;
	int avail;
	int bits;
	unsigned char *rp, *wp;
	void *state;
	fz_stream_next_fn *next;
	fz_stream_close_fn *close;
	fz_stream_seek_fn *seek;
	fz_stream_meta_fn *meta;
	fz_stream_rebind_fn *rebind;
	fz_stream_reopen_fn *reopen; /* SumatraPDF: allow to clone a stream */
};

fz_stream *fz_new_stream(fz_context *ctx,
			 void *state,
			 fz_stream_next_fn *next,
			 fz_stream_close_fn *close,
			 fz_stream_rebind_fn *rebind);
fz_stream *fz_keep_stream(fz_stream *stm);

/*
	fz_read_best: Attempt to read a stream into a buffer. If truncated
	is NULL behaves as fz_read_all, otherwise does not throw exceptions
	in the case of failure, but instead sets a truncated flag.

	stm: The stream to read from.

	initial: Suggested initial size for the buffer.

	truncated: Flag to store success/failure indication in.

	Returns a buffer created from reading from the stream.
*/
fz_buffer *fz_read_best(fz_stream *stm, int initial, int *truncated);

void fz_read_line(fz_stream *stm, char *buf, int max);

/*
	fz_available: Ask how many bytes are available immediately from
	a given stream.

	stm: The stream to read from.

	max: A hint for the underlying stream; the maximum number of
	bytes that we are sure we will want to read. If you do not know
	this number, give 1.

	Returns the number of bytes immediately available between the
	read and write pointers. This number is guaranteed only to be 0
	if we have hit EOF. The number of bytes returned here need have
	no relation to max (could be larger, could be smaller).
*/
static inline int fz_available(fz_stream *stm, int max)
{
	int len = stm->wp - stm->rp;
	int c;

	if (len)
		return len;
	fz_try(stm->ctx)
	{
		c = stm->next(stm, max);
	}
	fz_catch(stm->ctx)
	{
		fz_rethrow_if(stm->ctx, FZ_ERROR_TRYLATER);
		fz_warn(stm->ctx, "read error; treating as end of file");
		stm->error = 1;
		c = EOF;
	}
	if (c == EOF)
	{
		stm->eof = 1;
		return 0;
	}
	stm->rp--;
	return stm->wp - stm->rp;
}

static inline int fz_read_byte(fz_stream *stm)
{
	int c;

	if (stm->rp != stm->wp)
		return *stm->rp++;
	fz_try(stm->ctx)
	{
		c = stm->next(stm, 1);
	}
	fz_catch(stm->ctx)
	{
		fz_rethrow_if(stm->ctx, FZ_ERROR_TRYLATER);
		fz_warn(stm->ctx, "read error; treating as end of file");
		stm->error = 1;
		c = EOF;
	}
	if (c == EOF)
		stm->eof = 1;
	return c;
}

static inline int fz_peek_byte(fz_stream *stm)
{
	int c;

	if (stm->rp != stm->wp)
		return *stm->rp;

	c = stm->next(stm, 1);
	if (c != EOF)
		stm->rp--;
	return c;
}

static inline void fz_unread_byte(fz_stream *stm)
{
	stm->rp--;
}

static inline int fz_is_eof(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		if (stm->eof)
			return 1;
		return fz_peek_byte(stm) == EOF;
	}
	return 0;
}

static inline unsigned int fz_read_bits(fz_stream *stm, int n)
{
	unsigned int x;

	if (n <= stm->avail)
	{
		stm->avail -= n;
		x = (stm->bits >> stm->avail) & ((1 << n) - 1);
	}
	else
	{
		x = stm->bits & ((1 << stm->avail) - 1);
		n -= stm->avail;
		stm->avail = 0;

		while (n > 8)
		{
			x = (x << 8) | fz_read_byte(stm);
			n -= 8;
		}

		if (n > 0)
		{
			stm->bits = fz_read_byte(stm);
			stm->avail = 8 - n;
			x = (x << n) | (stm->bits >> stm->avail);
		}
	}

	return x;
}

static inline void fz_sync_bits(fz_stream *stm)
{
	stm->avail = 0;
}

static inline int fz_is_eof_bits(fz_stream *stm)
{
	return fz_is_eof(stm) && (stm->avail == 0 || stm->bits == EOF);
}

#endif
