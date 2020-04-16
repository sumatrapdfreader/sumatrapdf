#ifndef MUPDF_FITZ_BAND_WRITER_H
#define MUPDF_FITZ_BAND_WRITER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"

/*
	fz_band_writer
*/
typedef struct fz_band_writer fz_band_writer;

/*
	Cause a band writer to write the header for
	a banded image with the given properties/dimensions etc. This
	also configures the bandwriter for the format of the data to be
	passed in future calls.

	w, h: Width and Height of the entire page.

	n: Number of components (including spots and alphas).

	alpha: Number of alpha components.

	xres, yres: X and Y resolutions in dpi.

	cs: Colorspace (NULL for bitmaps)

	seps: Separation details (or NULL).
*/
void fz_write_header(fz_context *ctx, fz_band_writer *writer, int w, int h, int n, int alpha, int xres, int yres, int pagenum, fz_colorspace *cs, fz_separations *seps);

/*
	Cause a band writer to write the next band
	of data for an image.

	stride: The byte offset from the first byte of the data
	for a pixel to the first byte of the data for the same pixel
	on the row below.

	band_height: The number of lines in this band.

	samples: Pointer to first byte of the data.
*/
void fz_write_band(fz_context *ctx, fz_band_writer *writer, int stride, int band_height, const unsigned char *samples);

/*
	Drop the reference to the band writer, causing it to be
	destroyed.

	Never throws an exception.
*/
void fz_drop_band_writer(fz_context *ctx, fz_band_writer *writer);

/* Implementation details: subject to change. */

typedef void (fz_write_header_fn)(fz_context *ctx, fz_band_writer *writer, fz_colorspace *cs);
typedef void (fz_write_band_fn)(fz_context *ctx, fz_band_writer *writer, int stride, int band_start, int band_height, const unsigned char *samples);
typedef void (fz_write_trailer_fn)(fz_context *ctx, fz_band_writer *writer);
typedef void (fz_drop_band_writer_fn)(fz_context *ctx, fz_band_writer *writer);

struct fz_band_writer
{
	fz_drop_band_writer_fn *drop;
	fz_write_header_fn *header;
	fz_write_band_fn *band;
	fz_write_trailer_fn *trailer;
	fz_output *out;
	int w;
	int h;
	int n;
	int s;
	int alpha;
	int xres;
	int yres;
	int pagenum;
	int line;
	fz_separations *seps;
};

fz_band_writer *fz_new_band_writer_of_size(fz_context *ctx, size_t size, fz_output *out);
#define fz_new_band_writer(C,M,O) ((M *)Memento_label(fz_new_band_writer_of_size(ctx, sizeof(M), O), #M))


#endif
