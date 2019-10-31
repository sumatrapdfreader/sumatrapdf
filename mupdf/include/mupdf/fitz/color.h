#ifndef MUPDF_FITZ_COLOR_H
#define MUPDF_FITZ_COLOR_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/store.h"

typedef struct fz_colorspace_s fz_colorspace;
typedef struct fz_pixmap_s fz_pixmap;

#if FZ_ENABLE_ICC

typedef struct fz_icc_instance_s fz_icc_instance;
typedef struct fz_icc_profile_s fz_icc_profile;
typedef struct fz_icc_link_s fz_icc_link;

#endif

/* Color handling parameters: rendering intent, overprint, etc. */

enum
{
	/* Same order as needed by lcms */
	FZ_RI_PERCEPTUAL,
	FZ_RI_RELATIVE_COLORIMETRIC,
	FZ_RI_SATURATION,
	FZ_RI_ABSOLUTE_COLORIMETRIC,
};

typedef struct fz_color_params_s fz_color_params;
struct fz_color_params_s
{
	uint8_t ri;	/* rendering intent */
	uint8_t bp;	/* black point compensation */
	uint8_t op;	/* overprinting */
	uint8_t opm;	/* overprint mode */
};

extern const fz_color_params fz_default_color_params;

int fz_lookup_rendering_intent(const char *name);
const char *fz_rendering_intent_name(int ri);

enum { FZ_MAX_COLORS = 32 };

enum fz_colorspace_type
{
	FZ_COLORSPACE_NONE,
	FZ_COLORSPACE_GRAY,
	FZ_COLORSPACE_RGB,
	FZ_COLORSPACE_BGR,
	FZ_COLORSPACE_CMYK,
	FZ_COLORSPACE_LAB,
	FZ_COLORSPACE_INDEXED,
	FZ_COLORSPACE_SEPARATION,
};

enum
{
	FZ_COLORSPACE_IS_DEVICE = 1,
	FZ_COLORSPACE_IS_ICC = 2,
	FZ_COLORSPACE_HAS_CMYK = 4,
	FZ_COLORSPACE_HAS_SPOTS = 8,
	FZ_COLORSPACE_HAS_CMYK_AND_SPOTS = 4|8,
};

struct fz_colorspace_s
{
	fz_key_storable key_storable;
	enum fz_colorspace_type type;
	int flags;
	int n;
	char *name;
	union {
#if FZ_ENABLE_ICC
		struct {
			fz_buffer *buffer;
			unsigned char md5[16];
			fz_icc_profile *profile;
		} icc;
#endif
		struct {
			fz_colorspace *base;
			int high;
			unsigned char *lookup;
		} indexed;
		struct {
			fz_colorspace *base;
			void (*eval)(fz_context *ctx, void *tint, const float *s, int sn, float *d, int dn);
			void (*drop)(fz_context *ctx, void *tint);
			void *tint;
			char *colorant[FZ_MAX_COLORS];
		} separation;
	} u;
};

fz_colorspace *fz_new_colorspace(fz_context *ctx, enum fz_colorspace_type type, int flags, int n, const char *name);
fz_colorspace *fz_keep_colorspace(fz_context *ctx, fz_colorspace *colorspace);
void fz_drop_colorspace(fz_context *ctx, fz_colorspace *colorspace);
void fz_drop_colorspace_imp(fz_context *ctx, fz_storable *cs_);
void fz_drop_colorspace_store_key(fz_context *ctx, fz_colorspace *cs);
fz_colorspace *fz_keep_colorspace_store_key(fz_context *ctx, fz_colorspace *cs);

fz_colorspace *fz_new_indexed_colorspace(fz_context *ctx, fz_colorspace *base, int high, unsigned char *lookup);
fz_colorspace *fz_new_icc_colorspace(fz_context *ctx, enum fz_colorspace_type type, int flags, const char *name, fz_buffer *buf);
fz_colorspace *fz_new_cal_gray_colorspace(fz_context *ctx, float wp[3], float bp[3], float gamma);
fz_colorspace *fz_new_cal_rgb_colorspace(fz_context *ctx, float wp[3], float bp[3], float gamma[3], float matrix[9]);

fz_buffer *fz_new_icc_data_from_cal(fz_context *ctx, float wp[3], float bp[3], float gamma[3], float matrix[9], int n);

enum fz_colorspace_type fz_colorspace_type(fz_context *ctx, fz_colorspace *cs);
const char *fz_colorspace_name(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_n(fz_context *ctx, fz_colorspace *cs);

int fz_colorspace_is_subtractive(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_device_n_has_only_cmyk(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_device_n_has_cmyk(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_gray(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_rgb(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_cmyk(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_lab(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_indexed(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_device_n(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_device(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_device_gray(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_device_cmyk(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_lab_icc(fz_context *ctx, fz_colorspace *cs);
int fz_is_valid_blend_colorspace(fz_context *ctx, fz_colorspace *cs);

fz_colorspace *fz_device_gray(fz_context *ctx);
fz_colorspace *fz_device_rgb(fz_context *ctx);
fz_colorspace *fz_device_bgr(fz_context *ctx);
fz_colorspace *fz_device_cmyk(fz_context *ctx);
fz_colorspace *fz_device_lab(fz_context *ctx);

void fz_colorspace_name_process_colorants(fz_context *ctx, fz_colorspace *cs);
void fz_colorspace_name_colorant(fz_context *ctx, fz_colorspace *cs, int n, const char *name);
const char *fz_colorspace_colorant(fz_context *ctx, fz_colorspace *cs, int n);

/* Color conversion */

typedef struct fz_color_converter_s fz_color_converter;
typedef void (fz_color_convert_fn)(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst);
struct fz_color_converter_s
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

fz_color_convert_fn *fz_lookup_fast_color_converter(fz_context *ctx, fz_colorspace *ss, fz_colorspace *ds);
void fz_find_color_converter(fz_context *ctx, fz_color_converter *cc, fz_colorspace *ss, fz_colorspace *ds, fz_colorspace *is, fz_color_params params);
void fz_drop_color_converter(fz_context *ctx, fz_color_converter *cc);
void fz_init_cached_color_converter(fz_context *ctx, fz_color_converter *cc, fz_colorspace *ss, fz_colorspace *ds, fz_colorspace *is, fz_color_params params);
void fz_fin_cached_color_converter(fz_context *ctx, fz_color_converter *cc);

void fz_clamp_color(fz_context *ctx, fz_colorspace *cs, const float *in, float *out);
void fz_convert_color(fz_context *ctx, fz_colorspace *ss, const float *sv, fz_colorspace *ds, float *dv, fz_colorspace *is, fz_color_params params);

/* Default (fallback) colorspace handling */

typedef struct fz_default_colorspaces_s fz_default_colorspaces;
struct fz_default_colorspaces_s
{
	int refs;
	fz_colorspace *gray;
	fz_colorspace *rgb;
	fz_colorspace *cmyk;
	fz_colorspace *oi;
};

fz_default_colorspaces *fz_new_default_colorspaces(fz_context *ctx);
fz_default_colorspaces* fz_keep_default_colorspaces(fz_context *ctx, fz_default_colorspaces *default_cs);
void fz_drop_default_colorspaces(fz_context *ctx, fz_default_colorspaces *default_cs);
fz_default_colorspaces *fz_clone_default_colorspaces(fz_context *ctx, fz_default_colorspaces *base);

fz_colorspace *fz_default_gray(fz_context *ctx, const fz_default_colorspaces *default_cs);
fz_colorspace *fz_default_rgb(fz_context *ctx, const fz_default_colorspaces *default_cs);
fz_colorspace *fz_default_cmyk(fz_context *ctx, const fz_default_colorspaces *default_cs);
fz_colorspace *fz_default_output_intent(fz_context *ctx, const fz_default_colorspaces *default_cs);

void fz_set_default_gray(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs);
void fz_set_default_rgb(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs);
void fz_set_default_cmyk(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs);
void fz_set_default_output_intent(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs);

/*
	Color convert a pixmap. The passing of default_cs is needed due to the base cs of the image possibly
	needing to be treated as being in one of the page default color spaces.
*/
void fz_convert_pixmap_samples(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, fz_colorspace *prf, const fz_default_colorspaces *default_cs, fz_color_params color_params, int copy_spots);
void fz_fast_any_to_alpha(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots);
void fz_convert_fast_pixmap_samples(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots);
void fz_convert_slow_pixmap_samples(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, fz_colorspace *prf, fz_color_params params, int copy_spots);

/* Color management engine */

#if FZ_ENABLE_ICC

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
void fz_icc_transform_pixmap(fz_context *ctx, fz_icc_link *link, fz_pixmap *src, fz_pixmap *dst, int copy_spots);

#endif

struct fz_colorspace_context_s
{
	int ctx_refs;
	fz_colorspace *gray, *rgb, *bgr, *cmyk, *lab;
#if FZ_ENABLE_ICC
	void *icc_instance;
#endif
};

#endif
