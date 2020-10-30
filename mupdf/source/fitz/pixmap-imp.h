#ifndef FITZ_PIXMAP_IMP_H
#define FITZ_PIXMAP_IMP_H

void fz_drop_pixmap_imp(fz_context *ctx, fz_storable *pix);

void fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, fz_irect r, const fz_default_colorspaces *default_cs);
void fz_premultiply_pixmap(fz_context *ctx, fz_pixmap *pix);
size_t fz_pixmap_size(fz_context *ctx, fz_pixmap *pix);

fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip);

typedef struct fz_scale_cache fz_scale_cache;

fz_scale_cache *fz_new_scale_cache(fz_context *ctx);
void fz_drop_scale_cache(fz_context *ctx, fz_scale_cache *cache);
fz_pixmap *fz_scale_pixmap_cached(fz_context *ctx, const fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip, fz_scale_cache *cache_x, fz_scale_cache *cache_y);

void fz_subsample_pixmap(fz_context *ctx, fz_pixmap *tile, int factor);
void fz_subsample_pixblock(unsigned char *s, int w, int h, int n, int factor, ptrdiff_t stride);

fz_irect fz_pixmap_bbox_no_ctx(const fz_pixmap *src);

void fz_decode_indexed_tile(fz_context *ctx, fz_pixmap *pix, const float *decode, int maxval);
void fz_unpack_tile(fz_context *ctx, fz_pixmap *dst, unsigned char *src, int n, int depth, size_t stride, int scale);

fz_pixmap *fz_new_pixmap_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);
fz_pixmap *fz_new_pixmap_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);

#ifdef HAVE_VALGRIND
int fz_valgrind_pixmap(const fz_pixmap *pix);
#else
#define fz_valgrind_pixmap(pix) do {} while (0)
#endif

/*
	Convert between different separation results.
*/
fz_pixmap *fz_clone_pixmap_area_with_different_seps(fz_context *ctx, fz_pixmap *src, const fz_irect *bbox, fz_colorspace *dcs, fz_separations *seps, fz_color_params color_params, fz_default_colorspaces *default_cs);

/*
	Convert a region of the src pixmap into the dst pixmap
	via an optional proofing colorspace, prf.

	We assume that we never map from a DeviceN space to another
	DeviceN space here.
 */
fz_pixmap *fz_copy_pixmap_area_converting_seps(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, fz_colorspace *prf, fz_color_params color_params, fz_default_colorspaces *default_cs);


#endif
