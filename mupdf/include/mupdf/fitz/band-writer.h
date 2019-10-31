#ifndef MUPDF_FITZ_BAND_WRITER_H
#define MUPDF_FITZ_BAND_WRITER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"

/*
	fz_band_writer
*/
typedef struct fz_band_writer_s fz_band_writer;

typedef void (fz_write_header_fn)(fz_context *ctx, fz_band_writer *writer, fz_colorspace *cs);
typedef void (fz_write_band_fn)(fz_context *ctx, fz_band_writer *writer, int stride, int band_start, int band_height, const unsigned char *samples);
typedef void (fz_write_trailer_fn)(fz_context *ctx, fz_band_writer *writer);
typedef void (fz_drop_band_writer_fn)(fz_context *ctx, fz_band_writer *writer);

struct fz_band_writer_s
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

void fz_write_header(fz_context *ctx, fz_band_writer *writer, int w, int h, int n, int alpha, int xres, int yres, int pagenum, fz_colorspace *cs, fz_separations *seps);

void fz_write_band(fz_context *ctx, fz_band_writer *writer, int stride, int band_height, const unsigned char *samples);

void fz_drop_band_writer(fz_context *ctx, fz_band_writer *writer);

#endif
