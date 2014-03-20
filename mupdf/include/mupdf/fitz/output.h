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
	fz_new_output_to_filename: Open an output stream to a filename.
*/
fz_output *fz_new_output_to_filename(fz_context *, const char *filename);

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
	fz_putc: putc equivalent for output streams.
*/
void fz_putc(fz_output *out, char c);

/*
	fz_close_output: Close a previously opened fz_output stream.

	Note: whether or not this closes the underlying output method is
	method dependent. FILE * streams created by fz_new_output_with_file
	are NOT closed.
*/
void fz_close_output(fz_output *);

void fz_rebind_output(fz_output *, fz_context *ctx);

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

/*
	fz_vsnprintf: Our customised vsnprintf routine. Takes %c, %d, %o, %s, %x, as usual.
	Modifiers are not supported except for zero-padding ints (e.g. %02d, %03o, %04x, etc).
	%f and %g both output in "as short as possible hopefully lossless non-exponent" form,
	see fz_ftoa for specifics.
	%C outputs a utf8 encoded int.
	%M outputs a fz_matrix*. %R outputs a fz_rect*. %P outputs a fz_point*.
	%q and %( output escaped strings in C/PDF syntax.
*/
int fz_vsnprintf(char *buffer, int space, const char *fmt, va_list args);

/*
	fz_vfprintf: Our customised vfprintf routine. Same supported
	format specifiers as for fz_vsnprintf.
*/
int fz_vfprintf(fz_context *ctx, FILE *file, const char *fmt, va_list ap);

#endif
