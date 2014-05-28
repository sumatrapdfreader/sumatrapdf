#ifndef MUPDF_FITZ_IMAGE_H
#define MUPDF_FITZ_IMAGE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/colorspace.h"
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

/*
	fz_new_pixmap_from_image: Called to get a handle to a pixmap from an image.

	image: The image to retrieve a pixmap from.

	w: The desired width (in pixels). This may be completely ignored, but
	may serve as an indication of a suitable subsample factor to use for
	image types that support this.

	h: The desired height (in pixels). This may be completely ignored, but
	may serve as an indication of a suitable subsample factor to use for
	image types that support this.

	Returns a non NULL pixmap pointer. May throw exceptions.
*/
fz_pixmap *fz_new_pixmap_from_image(fz_context *ctx, fz_image *image, int w, int h);

/*
	fz_drop_image: Drop a reference to an image.

	image: The image to drop a reference to.
*/
void fz_drop_image(fz_context *ctx, fz_image *image);

/*
	fz_keep_image: Increment the reference count of an image.

	image: The image to take a reference to.

	Returns a pointer to the image.
*/
fz_image *fz_keep_image(fz_context *ctx, fz_image *image);

fz_image *fz_new_image(fz_context *ctx, int w, int h, int bpc, fz_colorspace *colorspace, int xres, int yres, int interpolate, int imagemask, float *decode, int *colorkey, fz_compressed_buffer *buffer, fz_image *mask);
fz_image *fz_new_image_from_pixmap(fz_context *ctx, fz_pixmap *pixmap, fz_image *mask);
fz_image *fz_new_image_from_data(fz_context *ctx, unsigned char *data, int len);
fz_image *fz_new_image_from_buffer(fz_context *ctx, fz_buffer *buffer);
fz_pixmap *fz_image_get_pixmap(fz_context *ctx, fz_image *image, int w, int h);
void fz_free_image(fz_context *ctx, fz_storable *image);
fz_pixmap *fz_decomp_image_from_stream(fz_context *ctx, fz_stream *stm, fz_image *image, int indexed, int l2factor, int native_l2factor);
fz_pixmap *fz_expand_indexed_pixmap(fz_context *ctx, fz_pixmap *src);

struct fz_image_s
{
	fz_storable storable;
	int w, h, n, bpc;
	fz_image *mask;
	fz_colorspace *colorspace;
	fz_pixmap *(*get_pixmap)(fz_context *, fz_image *, int w, int h);
	fz_compressed_buffer *buffer;
	int colorkey[FZ_MAX_COLORS * 2];
	float decode[FZ_MAX_COLORS * 2];
	int imagemask;
	int interpolate;
	int usecolorkey;
	fz_pixmap *tile; /* Private to the implementation */
	int xres; /* As given in the image, not necessarily as rendered */
	int yres; /* As given in the image, not necessarily as rendered */
	int invert_cmyk_jpeg;
};

fz_pixmap *fz_load_jpx(fz_context *ctx, unsigned char *data, int size, fz_colorspace *cs, int indexed);
fz_pixmap *fz_load_png(fz_context *ctx, unsigned char *data, int size);
fz_pixmap *fz_load_tiff(fz_context *ctx, unsigned char *data, int size);
fz_pixmap *fz_load_jxr(fz_context *ctx, unsigned char *data, int size);

void fz_load_jpeg_info(fz_context *ctx, unsigned char *data, int size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_png_info(fz_context *ctx, unsigned char *data, int size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_tiff_info(fz_context *ctx, unsigned char *data, int size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);
void fz_load_jxr_info(fz_context *ctx, unsigned char *data, int size, int *w, int *h, int *xres, int *yres, fz_colorspace **cspace);

int fz_load_tiff_subimage_count(fz_context *ctx, unsigned char *buf, int len);
fz_pixmap *fz_load_tiff_subimage(fz_context *ctx, unsigned char *buf, int len, int subimage);

#endif
