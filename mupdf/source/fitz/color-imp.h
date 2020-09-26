#ifndef FITZ_COLOR_IMP_H
#define FITZ_COLOR_IMP_H

#include "mupdf/fitz.h"

typedef struct fz_color_converter fz_color_converter;

/* Color management engine */

#if FZ_ENABLE_ICC

/*
	Create ICC profile from PDF calGray and calRGB definitions
*/
fz_buffer *fz_new_icc_data_from_cal(fz_context *ctx, float wp[3], float bp[3], float gamma[3], float matrix[9], int n);

/*
	Opaque type for a link (transform) generated between ICC
	profiles.
*/
typedef struct fz_icc_link fz_icc_link;

void fz_new_icc_context(fz_context *ctx);
void fz_drop_icc_context(fz_context *ctx);
fz_icc_profile *fz_new_icc_profile(fz_context *ctx, unsigned char *data, size_t size);
void fz_drop_icc_profile(fz_context *ctx, fz_icc_profile *profile);
void fz_icc_profile_name(fz_context *ctx, fz_icc_profile *profile, char *name, size_t size);
int fz_icc_profile_components(fz_context *ctx, fz_icc_profile *profile);
int fz_icc_profile_is_lab(fz_context *ctx, fz_icc_profile *profile);
fz_icc_link *fz_new_icc_link(fz_context *ctx,
	fz_colorspace *src, int src_extras,
	fz_colorspace *dst, int dst_extras,
	fz_colorspace *prf,
	fz_color_params color_params,
	int format,
	int copy_spots);
void fz_drop_icc_link_imp(fz_context *ctx, fz_storable *link);
void fz_drop_icc_link(fz_context *ctx, fz_icc_link *link);
fz_icc_link *fz_find_icc_link(fz_context *ctx,
	fz_colorspace *src, int src_extras,
	fz_colorspace *dst, int dst_extras,
	fz_colorspace *prf,
	fz_color_params color_params,
	int format,
	int copy_spots);
void fz_icc_transform_color(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst);
void fz_icc_transform_pixmap(fz_context *ctx, fz_icc_link *link, const fz_pixmap *src, fz_pixmap *dst, int copy_spots);

#endif

typedef void (fz_color_convert_fn)(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst);

struct fz_color_converter
{
	fz_color_convert_fn *convert;
	fz_color_convert_fn *convert_via;
	fz_colorspace *ds;
	fz_colorspace *ss;
	fz_colorspace *ss_via;
	void *opaque;
#if FZ_ENABLE_ICC
	fz_icc_link *link;
#endif
};

struct fz_colorspace_context
{
	int ctx_refs;
	fz_colorspace *gray, *rgb, *bgr, *cmyk, *lab;
#if FZ_ENABLE_ICC
	void *icc_instance;
#endif
};

void fz_drop_colorspace_store_key(fz_context *ctx, fz_colorspace *cs);
fz_colorspace *fz_keep_colorspace_store_key(fz_context *ctx, fz_colorspace *cs);

void fz_init_cached_color_converter(fz_context *ctx, fz_color_converter *cc, fz_colorspace *ss, fz_colorspace *ds, fz_colorspace *is, fz_color_params params);
void fz_fin_cached_color_converter(fz_context *ctx, fz_color_converter *cc);
fz_color_convert_fn *fz_lookup_fast_color_converter(fz_context *ctx, fz_colorspace *ss, fz_colorspace *ds);
void fz_find_color_converter(fz_context *ctx, fz_color_converter *cc, fz_colorspace *ss, fz_colorspace *ds, fz_colorspace *is, fz_color_params params);
void fz_drop_color_converter(fz_context *ctx, fz_color_converter *cc);

/*
	Color convert a pixmap. The passing of default_cs is needed due
	to the base cs of the image possibly needing to be treated as
	being in one of the page default color spaces.
*/
void fz_convert_pixmap_samples(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *prf, const fz_default_colorspaces *default_cs, fz_color_params color_params, int copy_spots);
void fz_fast_any_to_alpha(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, int copy_spots);
void fz_convert_fast_pixmap_samples(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, int copy_spots);
void fz_convert_slow_pixmap_samples(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *prf, fz_color_params params, int copy_spots);



#endif
