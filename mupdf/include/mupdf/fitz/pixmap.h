#ifndef MUPDF_FITZ_PIXMAP_H
#define MUPDF_FITZ_PIXMAP_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/separation.h"

/*
	Pixmaps represent a set of pixels for a 2 dimensional region of a
	plane. Each pixel has n components per pixel, the last of which is
	always alpha. The data is in premultiplied alpha when rendering, but
	non-premultiplied for colorspace conversions and rescaling.
*/

typedef struct fz_overprint_s fz_overprint;

fz_irect fz_pixmap_bbox(fz_context *ctx, const fz_pixmap *pix);

int fz_pixmap_width(fz_context *ctx, fz_pixmap *pix);

int fz_pixmap_height(fz_context *ctx, fz_pixmap *pix);

int fz_pixmap_x(fz_context *ctx, fz_pixmap *pix);

int fz_pixmap_y(fz_context *ctx, fz_pixmap *pix);

fz_pixmap *fz_new_pixmap(fz_context *ctx, fz_colorspace *cs, int w, int h, fz_separations *seps, int alpha);

fz_pixmap *fz_new_pixmap_with_bbox(fz_context *ctx, fz_colorspace *colorspace, fz_irect bbox, fz_separations *seps, int alpha);

fz_pixmap *fz_new_pixmap_with_data(fz_context *ctx, fz_colorspace *colorspace, int w, int h, fz_separations *seps, int alpha, int stride, unsigned char *samples);

fz_pixmap *fz_new_pixmap_with_bbox_and_data(fz_context *ctx, fz_colorspace *colorspace, fz_irect rect, fz_separations *seps, int alpha, unsigned char *samples);

fz_pixmap *fz_new_pixmap_from_pixmap(fz_context *ctx, fz_pixmap *pixmap, const fz_irect *rect);
fz_pixmap *fz_clone_pixmap(fz_context *ctx, fz_pixmap *old);

fz_pixmap *fz_keep_pixmap(fz_context *ctx, fz_pixmap *pix);
void fz_drop_pixmap(fz_context *ctx, fz_pixmap *pix);

fz_colorspace *fz_pixmap_colorspace(fz_context *ctx, fz_pixmap *pix);

int fz_pixmap_components(fz_context *ctx, fz_pixmap *pix);

int fz_pixmap_colorants(fz_context *ctx, fz_pixmap *pix);

int fz_pixmap_spots(fz_context *ctx, fz_pixmap *pix);

int fz_pixmap_alpha(fz_context *ctx, fz_pixmap *pix);

unsigned char *fz_pixmap_samples(fz_context *ctx, fz_pixmap *pix);

int fz_pixmap_stride(fz_context *ctx, fz_pixmap *pix);

void fz_set_pixmap_resolution(fz_context *ctx, fz_pixmap *pix, int xres, int yres);

void fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value);

void fz_fill_pixmap_with_color(fz_context *ctx, fz_pixmap *pix, fz_colorspace *colorspace, float *color, fz_color_params color_params);

void fz_clear_pixmap_rect_with_value(fz_context *ctx, fz_pixmap *pix, int value, fz_irect r);

void fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix);

void fz_invert_pixmap_luminance(fz_context *ctx, fz_pixmap *pix);
void fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix);

void fz_tint_pixmap(fz_context *ctx, fz_pixmap *pix, int black, int white);

void fz_invert_pixmap_rect(fz_context *ctx, fz_pixmap *image, fz_irect rect);

void fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma);

fz_pixmap *fz_convert_pixmap(fz_context *ctx, fz_pixmap *pix, fz_colorspace *cs_des, fz_colorspace *prf, fz_default_colorspaces *default_cs, fz_color_params color_params, int keep_alpha);

/*
	Pixmaps represent a set of pixels for a 2 dimensional region of a
	plane. Each pixel has n components per pixel, the last of which is
	always alpha. The data is in premultiplied alpha when rendering, but
	non-premultiplied for colorspace conversions and rescaling.

	x, y: The minimum x and y coord of the region in pixels.

	w, h: The width and height of the region in pixels.

	n: The number of color components in the image.
		n = num composite colors + num spots + num alphas

	s: The number of spot channels in the image.

	alpha: 0 for no alpha, 1 for alpha present.

	flags: flag bits.
		Bit 0: If set, draw the image with linear interpolation.
		Bit 1: If set, free the samples buffer when the pixmap
		is destroyed.

	stride: The byte offset from the data for any given pixel
	to the data for the same pixel on the row below.

	seps: NULL, or a pointer to a separations structure. If NULL,
	s should be 0.

	xres, yres: Image resolution in dpi. Default is 96 dpi.

	colorspace: Pointer to a colorspace object describing the colorspace
	the pixmap is in. If NULL, the image is a mask.

	samples: A simple block of memory w * h * n bytes of memory in which
	the components are stored. The first n bytes are components 0 to n-1
	for the pixel at (x,y). Each successive n bytes gives another pixel
	in scanline order. Subsequent scanlines follow on with no padding.
*/
struct fz_pixmap_s
{
	fz_storable storable;
	int x, y, w, h;
	unsigned char n;
	unsigned char s;
	unsigned char alpha;
	unsigned char flags;
	ptrdiff_t stride;
	fz_separations *seps;
	int xres, yres;
	fz_colorspace *colorspace;
	unsigned char *samples;
	fz_pixmap *underlying;
};

enum
{
	FZ_PIXMAP_FLAG_INTERPOLATE = 1,
	FZ_PIXMAP_FLAG_FREE_SAMPLES = 2
};

void fz_drop_pixmap_imp(fz_context *ctx, fz_storable *pix);

void fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, fz_irect r, const fz_default_colorspaces *default_cs);
void fz_premultiply_pixmap(fz_context *ctx, fz_pixmap *pix);
fz_pixmap *fz_alpha_from_gray(fz_context *ctx, fz_pixmap *gray);
size_t fz_pixmap_size(fz_context *ctx, fz_pixmap *pix);

fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip);

typedef struct fz_scale_cache_s fz_scale_cache;

fz_scale_cache *fz_new_scale_cache(fz_context *ctx);
void fz_drop_scale_cache(fz_context *ctx, fz_scale_cache *cache);
fz_pixmap *fz_scale_pixmap_cached(fz_context *ctx, const fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip, fz_scale_cache *cache_x, fz_scale_cache *cache_y);

void fz_subsample_pixmap(fz_context *ctx, fz_pixmap *tile, int factor);

fz_irect fz_pixmap_bbox_no_ctx(const fz_pixmap *src);

void fz_decode_tile(fz_context *ctx, fz_pixmap *pix, const float *decode);
void fz_decode_indexed_tile(fz_context *ctx, fz_pixmap *pix, const float *decode, int maxval);
void fz_unpack_tile(fz_context *ctx, fz_pixmap *dst, unsigned char *src, int n, int depth, size_t stride, int scale);

void fz_md5_pixmap(fz_context *ctx, fz_pixmap *pixmap, unsigned char digest[16]);

fz_pixmap *fz_new_pixmap_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);
fz_pixmap *fz_new_pixmap_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);

#ifdef HAVE_VALGRIND
int fz_valgrind_pixmap(const fz_pixmap *pix);
#else
#define fz_valgrind_pixmap(pix) do {} while (0)
#endif

fz_pixmap *fz_clone_pixmap_area_with_different_seps(fz_context *ctx, fz_pixmap *src, const fz_irect *bbox, fz_colorspace *dcs, fz_separations *seps, fz_color_params color_params, fz_default_colorspaces *default_cs);
fz_pixmap *fz_copy_pixmap_area_converting_seps(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, fz_colorspace *prf, fz_color_params color_params, fz_default_colorspaces *default_cs);

#endif
