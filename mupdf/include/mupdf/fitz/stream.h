#ifndef MUPDF_FITZ_STREAM_H
#define MUPDF_FITZ_STREAM_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"

int fz_file_exists(fz_context *ctx, const char *path);

/*
	fz_stream is a buffered reader capable of seeking in both
	directions.

	Streams are reference counted, so references must be dropped
	by a call to fz_drop_stream.

	Only the data between rp and wp is valid.
*/
typedef struct fz_stream_s fz_stream;

fz_stream *fz_open_file(fz_context *ctx, const char *filename);

fz_stream *fz_open_file_w(fz_context *ctx, const wchar_t *filename);

fz_stream *fz_open_memory(fz_context *ctx, const unsigned char *data, size_t len);

fz_stream *fz_open_buffer(fz_context *ctx, fz_buffer *buf);

fz_stream *fz_open_leecher(fz_context *ctx, fz_stream *chain, fz_buffer *buf);

void fz_drop_stream(fz_context *ctx, fz_stream *stm);

int64_t fz_tell(fz_context *ctx, fz_stream *stm);

void fz_seek(fz_context *ctx, fz_stream *stm, int64_t offset, int whence);

size_t fz_read(fz_context *ctx, fz_stream *stm, unsigned char *data, size_t len);

size_t fz_skip(fz_context *ctx, fz_stream *stm, size_t len);

fz_buffer *fz_read_all(fz_context *ctx, fz_stream *stm, size_t initial);

fz_buffer *fz_read_file(fz_context *ctx, const char *filename);

uint16_t fz_read_uint16(fz_context *ctx, fz_stream *stm);
uint32_t fz_read_uint24(fz_context *ctx, fz_stream *stm);
uint32_t fz_read_uint32(fz_context *ctx, fz_stream *stm);
uint64_t fz_read_uint64(fz_context *ctx, fz_stream *stm);

uint16_t fz_read_uint16_le(fz_context *ctx, fz_stream *stm);
uint32_t fz_read_uint24_le(fz_context *ctx, fz_stream *stm);
uint32_t fz_read_uint32_le(fz_context *ctx, fz_stream *stm);
uint64_t fz_read_uint64_le(fz_context *ctx, fz_stream *stm);

int16_t fz_read_int16(fz_context *ctx, fz_stream *stm);
int32_t fz_read_int32(fz_context *ctx, fz_stream *stm);
int64_t fz_read_int64(fz_context *ctx, fz_stream *stm);

int16_t fz_read_int16_le(fz_context *ctx, fz_stream *stm);
int32_t fz_read_int32_le(fz_context *ctx, fz_stream *stm);
int64_t fz_read_int64_le(fz_context *ctx, fz_stream *stm);

float fz_read_float_le(fz_context *ctx, fz_stream *stm);
float fz_read_float(fz_context *ctx, fz_stream *stm);

void fz_read_string(fz_context *ctx, fz_stream *stm, char *buffer, int len);

/*
	A function type for use when implementing
	fz_streams. The supplied function of this type is called
	whenever data is required, and the current buffer is empty.

	stm: The stream to operate on.

	max: a hint as to the maximum number of bytes that the caller
	needs to be ready immediately. Can safely be ignored.

	Returns -1 if there is no more data in the stream. Otherwise,
	the function should find its internal state using stm->state,
	refill its buffer, update stm->rp and stm->wp to point to the
	start and end of the new data respectively, and then
	"return *stm->rp++".
*/
typedef int (fz_stream_next_fn)(fz_context *ctx, fz_stream *stm, size_t max);

/*
	A function type for use when implementing
	fz_streams. The supplied function of this type is called
	when the stream is dropped, to release the stream specific
	state information.

	state: The stream state to release.
*/
typedef void (fz_stream_drop_fn)(fz_context *ctx, void *state);

/*
	A function type for use when implementing
	fz_streams. The supplied function of this type is called when
	fz_seek is requested, and the arguments are as defined for
	fz_seek.

	The stream can find it's private state in stm->state.
*/
typedef void (fz_stream_seek_fn)(fz_context *ctx, fz_stream *stm, int64_t offset, int whence);

struct fz_stream_s
{
	int refs;
	int error;
	int eof;
	int progressive;
	int64_t pos;
	int avail;
	int bits;
	unsigned char *rp, *wp;
	void *state;
	fz_stream_next_fn *next;
	fz_stream_drop_fn *drop;
	fz_stream_seek_fn *seek;
};

fz_stream *fz_new_stream(fz_context *ctx, void *state, fz_stream_next_fn *next, fz_stream_drop_fn *drop);

fz_stream *fz_keep_stream(fz_context *ctx, fz_stream *stm);

fz_buffer *fz_read_best(fz_context *ctx, fz_stream *stm, size_t initial, int *truncated);

char *fz_read_line(fz_context *ctx, fz_stream *stm, char *buf, size_t max);

/*
	Ask how many bytes are available immediately from
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
static inline size_t fz_available(fz_context *ctx, fz_stream *stm, size_t max)
{
	size_t len = stm->wp - stm->rp;
	int c = EOF;

	if (len)
		return len;
	if (stm->eof)
		return 0;

	fz_try(ctx)
		c = stm->next(ctx, stm, max);
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "read error; treating as end of file");
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

/*
	Read the next byte from a stream.

	stm: The stream t read from.

	Returns -1 for end of stream, or the next byte. May
	throw exceptions.
*/
static inline int fz_read_byte(fz_context *ctx, fz_stream *stm)
{
	int c = EOF;

	if (stm->rp != stm->wp)
		return *stm->rp++;
	if (stm->eof)
		return EOF;
	fz_try(ctx)
		c = stm->next(ctx, stm, 1);
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "read error; treating as end of file");
		stm->error = 1;
		c = EOF;
	}
	if (c == EOF)
		stm->eof = 1;
	return c;
}

/*
	Peek at the next byte in a stream.

	stm: The stream to peek at.

	Returns -1 for EOF, or the next byte that will be read.
*/
static inline int fz_peek_byte(fz_context *ctx, fz_stream *stm)
{
	int c = EOF;

	if (stm->rp != stm->wp)
		return *stm->rp;
	if (stm->eof)
		return EOF;

	fz_try(ctx)
	{
		c = stm->next(ctx, stm, 1);
		if (c != EOF)
			stm->rp--;
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "read error; treating as end of file");
		stm->error = 1;
		c = EOF;
	}
	if (c == EOF)
		stm->eof = 1;
	return c;
}

/*
	Unread the single last byte successfully
	read from a stream. Do not call this without having
	successfully read a byte.

	stm: The stream to operate upon.
*/
static inline void fz_unread_byte(fz_context *ctx FZ_UNUSED, fz_stream *stm)
{
	stm->rp--;
}

static inline int fz_is_eof(fz_context *ctx, fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		if (stm->eof)
			return 1;
		return fz_peek_byte(ctx, stm) == EOF;
	}
	return 0;
}

/*
	Read the next n bits from a stream (assumed to
	be packed most significant bit first).

	stm: The stream to read from.

	n: The number of bits to read, between 1 and 8*sizeof(int)
	inclusive.

	Returns -1 for EOF, or the required number of bits.
*/
static inline unsigned int fz_read_bits(fz_context *ctx, fz_stream *stm, int n)
{
	int x;

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
			x = (x << 8) | fz_read_byte(ctx, stm);
			n -= 8;
		}

		if (n > 0)
		{
			stm->bits = fz_read_byte(ctx, stm);
			stm->avail = 8 - n;
			x = (x << n) | (stm->bits >> stm->avail);
		}
	}

	return x;
}

/*
	Read the next n bits from a stream (assumed to
	be packed least significant bit first).

	stm: The stream to read from.

	n: The number of bits to read, between 1 and 8*sizeof(int)
	inclusive.

	Returns (unsigned int)-1 for EOF, or the required number of bits.
*/
static inline unsigned int fz_read_rbits(fz_context *ctx, fz_stream *stm, int n)
{
	int x;

	if (n <= stm->avail)
	{
		x = stm->bits & ((1 << n) - 1);
		stm->avail -= n;
		stm->bits = stm->bits >> n;
	}
	else
	{
		unsigned int used = 0;

		x = stm->bits & ((1 << stm->avail) - 1);
		n -= stm->avail;
		used = stm->avail;
		stm->avail = 0;

		while (n > 8)
		{
			x = (fz_read_byte(ctx, stm) << used) | x;
			n -= 8;
			used += 8;
		}

		if (n > 0)
		{
			stm->bits = fz_read_byte(ctx, stm);
			x = ((stm->bits & ((1 << n) - 1)) << used) | x;
			stm->avail = 8 - n;
			stm->bits = stm->bits >> n;
		}
	}

	return x;
}

/*
	Called after reading bits to tell the stream
	that we are about to return to reading bytewise. Resyncs
	the stream to whole byte boundaries.
*/
static inline void fz_sync_bits(fz_context *ctx FZ_UNUSED, fz_stream *stm)
{
	stm->avail = 0;
}

static inline int fz_is_eof_bits(fz_context *ctx, fz_stream *stm)
{
	return fz_is_eof(ctx, stm) && (stm->avail == 0 || stm->bits == EOF);
}

#endif
