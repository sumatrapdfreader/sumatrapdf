#ifndef MUPDF_FITZ_OUTPUT_PNG_H
#define MUPDF_FITZ_OUTPUT_PNG_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/bitmap.h"

#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/image.h"

/*
	fz_write_png: Save a pixmap as a png

	filename: The filename to save as (including extension).
*/
void fz_write_png(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha);

/*
	Output a pixmap to an output stream as a png.
*/
void fz_output_png(fz_output *out, const fz_pixmap *pixmap, int savealpha);

/*
	Get an image as a png in a buffer.
*/
fz_buffer *fz_new_png_from_image(fz_context *ctx, fz_image *image, int w, int h);

fz_buffer *fz_new_png_from_pixmap(fz_context *ctx, fz_pixmap *pixmap);

typedef struct fz_png_output_context_s fz_png_output_context;

fz_png_output_context *fz_output_png_header(fz_output *out, int w, int h, int n, int savealpha);

void fz_output_png_band(fz_output *out, int w, int h, int n, int band, int bandheight, unsigned char *samples, int savealpha, fz_png_output_context *poc);

void fz_output_png_trailer(fz_output *out, fz_png_output_context *poc);

#endif
