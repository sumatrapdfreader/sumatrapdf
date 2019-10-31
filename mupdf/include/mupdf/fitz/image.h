#ifndef MUPDF_FITZ_IMAGE_H
#define MUPDF_FITZ_IMAGE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/pixmap.h"

#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"
#include "mupdf/fitz/compressed-buffer.h"

/*
	Images are storable objects from which we can obtain fz_pixmaps.
	These may be implemented as simple wrappers around a pixmap, or as
	more complex things that decode at different subsample settings on
	demand.
*/
typedef struct fz_image_s fz_image;
typedef struct fz_compressed_image_s fz_compressed_image;
typedef struct fz_pixmap_image_s fz_pixmap_image;

fz_pixmap *fz_get_pixmap_from_image(fz_context *ctx, fz_image *image, const fz_irect *subarea, fz_matrix *ctm, int *w, int *h);

void fz_drop_image(fz_context *ctx, fz_image *image);
fz_image *fz_keep_image(fz_context *ctx, fz_image *image);

fz_image *fz_keep_image_store_key(fz_context *ctx, fz_image *image);
void fz_drop_image_store_key(fz_context *ctx, fz_image *image);

/*
	Function type to destroy an images data
	when it's reference count reaches zero.
*/
typedef void (fz_drop_image_fn)(fz_context *ctx, fz_image *image);

/*
	Function type to get a decoded pixmap
	for an image.

	im: The image to decode.

	subarea: NULL, or the subarea of the image required. Expressed
	in terms of a rectangle in the original width/height of the
	image. If non NULL, this should be updated by the function to
	the actual subarea decoded - which must include the requested
	area!

	w, h: The actual width and height that the whole image would
	need to be decoded to.

	l2factor: On entry, the log 2 subsample factor required. If
	possible the decode process can take care of (all or some) of
	this subsampling, and must then update the value so the caller
	knows what remains to be done.

	Returns a reference to a decoded pixmap that satisfies the
	requirements of the request.
*/
typedef fz_pixmap *(fz_image_get_pixmap_fn)(fz_context *ctx, fz_image *im, fz_irect *subarea, int w, int h, int *l2factor);

/*
	Function type to get the given storage
	size for an image.

	Returns the size in bytes used for a given image.
*/
typedef size_t (fz_image_get_size_fn)(fz_context *, fz_image *);

fz_image *fz_new_image_of_size(fz_context *ctx,
		int w,
		int h,
		int bpc,
		fz_colorspace *colorspace,
		int xres,
		int yres,
		int interpolate,
		int imagemask,
		float *decode,
		int *colorkey,
		fz_image *mask,
		size_t size,
		fz_image_get_pixmap_fn *get_pixmap,
		fz_image_get_size_fn *get_size,
		fz_drop_image_fn *drop);

#define fz_new_derived_image(CTX,W,H,B,CS,X,Y,I,IM,D,C,M,T,G,S,Z) \
	((T*)Memento_label(fz_new_image_of_size(CTX,W,H,B,CS,X,Y,I,IM,D,C,M,sizeof(T),G,S,Z),#T))

fz_image *fz_new_image_from_compressed_buffer(fz_context *ctx, int w, int h, int bpc, fz_colorspace *colorspace, int xres, int yres, int interpolate, int imagemask, float *decode, int *colorkey, fz_compressed_buffer *buffer, fz_image *mask);

fz_image *fz_new_image_from_pixmap(fz_context *ctx, fz_pixmap *pixmap, fz_image *mask);

fz_image *fz_new_image_from_buffer(fz_context *ctx, fz_buffer *buffer);

fz_image *fz_new_image_from_file(fz_context *ctx, const char *path);

void fz_drop_image_imp(fz_context *ctx, fz_storable *image);
void fz_drop_image_base(fz_context *ctx, fz_image *image);
fz_pixmap *fz_decomp_image_from_stream(fz_context *ctx, fz_stream *stm, fz_compressed_image *image, fz_irect *subarea, int indexed, int l2factor);
fz_pixmap *fz_convert_indexed_pixmap_to_base(fz_context *ctx, const fz_pixmap *src);
fz_pixmap *fz_convert_separation_pixmap_to_base(fz_context *ctx, const fz_pixmap *src);
size_t fz_image_size(fz_context *ctx, fz_image *im);

/*
	Structure is public to allow other structures to
	be derived from it. Do not access members directly.
*/
struct fz_image_s
{
	fz_key_storable key_storable;
	int w, h;
	uint8_t n;
	uint8_t bpc;
	unsigned int imagemask:1;
	unsigned int interpolate:1;
	unsigned int use_colorkey:1;
	unsigned int use_decode:1;
	unsigned int invert_cmyk_jpeg:1;
	unsigned int decoded:1;
	unsigned int scalable:1;
	fz_image *mask;
	int xres; /* As given in the image, not necessarily as rendered */
	int yres; /* As given in the image, not necessarily as rendered */
	fz_colorspace *colorspace;
	fz_drop_image_fn *drop_image;
	fz_image_get_pixmap_fn *get_pixmap;
	fz_image_get_size_fn *get_size;
	int colorkey[FZ_MAX_COLORS * 2];
	float decode[FZ_MAX_COLORS * 2];
};

fz_pixmap *fz_load_jpeg(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_jpx(fz_context *ctx, const unsigned char *data, size_t size, fz_colorspace *cs);
fz_pixmap *fz_load_png(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_tiff(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_jxr(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_gif(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_bmp(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_pnm(fz_context *ctx, const unsigned char *data, size_t size);
fz_pixmap *fz_load_jbig2(fz_context *ctx, const unsigned char *data, size_t size);

void fz_load_jpeg_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_jpx_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_png_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_tiff_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_jxr_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_gif_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_bmp_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_pnm_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_jbig2_info(fz_context *ctx, const unsigned char *data, size_t size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);

int fz_load_tiff_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len);
fz_pixmap *fz_load_tiff_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage);
int fz_load_pnm_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len);
fz_pixmap *fz_load_pnm_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage);
int fz_load_jbig2_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len);
fz_pixmap *fz_load_jbig2_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage);

void fz_image_resolution(fz_image *image, int *xres, int *yres);

fz_pixmap *fz_compressed_image_tile(fz_context *ctx, fz_compressed_image *cimg);
void fz_set_compressed_image_tile(fz_context *ctx, fz_compressed_image *cimg, fz_pixmap *pix);

fz_compressed_buffer *fz_compressed_image_buffer(fz_context *ctx, fz_image *image);
void fz_set_compressed_image_buffer(fz_context *ctx, fz_compressed_image *cimg, fz_compressed_buffer *buf);

fz_pixmap *fz_pixmap_image_tile(fz_context *ctx, fz_pixmap_image *cimg);
void fz_set_pixmap_image_tile(fz_context *ctx, fz_pixmap_image *cimg, fz_pixmap *pix);

#endif
