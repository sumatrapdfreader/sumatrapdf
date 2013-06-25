#ifndef MUPDF_FITZ_OUTPUT_H
#define MUPDF_FITZ_OUTPUT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"

/*
	Generic output streams - generalise between outputting to a file,
	a buffer, etc.
*/
typedef struct fz_output_s fz_output;

struct fz_output_s
{
	fz_context *ctx;
	void *opaque;
	int (*printf)(fz_output *, const char *, va_list ap);
	int (*write)(fz_output *, const void *, int n);
	void (*close)(fz_output *);
};

/*
	fz_new_output_with_file: Open an output stream onto a FILE *.

	The stream does NOT take ownership of the FILE *.
*/
fz_output *fz_new_output_with_file(fz_context *, FILE *);

/*
	fz_new_output_with_buffer: Open an output stream onto a buffer.

	The stream does NOT take ownership of the buffer.
*/
fz_output *fz_new_output_with_buffer(fz_context *, fz_buffer *);

/*
	fz_printf: fprintf equivalent for output streams.
*/
int fz_printf(fz_output *, const char *, ...);

/*
	fz_puts: fputs equivalent for output streams.
*/
int fz_puts(fz_output *, const char *);

/*
	fz_write: fwrite equivalent for output streams.
*/
int fz_write(fz_output *out, const void *data, int len);

/*
	fz_close_output: Close a previously opened fz_output stream.

	Note: whether or not this closes the underlying output method is
	method dependent. FILE * streams created by fz_new_output_with_file
	are NOT closed.
*/
void fz_close_output(fz_output *);

static inline int fz_write_int32be(fz_output *out, int x)
{
	char data[4];

	data[0] = x>>24;
	data[1] = x>>16;
	data[2] = x>>8;
	data[3] = x;

	return fz_write(out, data, 4);
}

static inline void
fz_write_byte(fz_output *out, int x)
{
	char data = x;

	fz_write(out, &data, 1);
}

#endif
