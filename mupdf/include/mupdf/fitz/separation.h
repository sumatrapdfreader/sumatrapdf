#ifndef MUPDF_FITZ_SEPARATION_H
#define MUPDF_FITZ_SEPARATION_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/*
	A fz_separation structure holds details of a set of separations
	(such as might be used on within a page of the document).

	The app might control the separations by enabling/disabling them,
	and subsequent renders would take this into account.
*/

enum
{
	FZ_MAX_SEPARATIONS = 64
};

typedef struct fz_separations_s fz_separations;

typedef enum
{
	/* "Composite" separations are rendered using process
	 * colors using the equivalent colors */
	FZ_SEPARATION_COMPOSITE = 0,
	/* Spot colors are rendered into their own spot plane. */
	FZ_SEPARATION_SPOT = 1,
	/* Disabled colors are not rendered at all in the final
	 * output. */
	FZ_SEPARATION_DISABLED = 2
} fz_separation_behavior;

fz_separations *fz_new_separations(fz_context *ctx, int controllable);

fz_separations *fz_keep_separations(fz_context *ctx, fz_separations *sep);
void fz_drop_separations(fz_context *ctx, fz_separations *sep);

void fz_add_separation(fz_context *ctx, fz_separations *sep, const char *name, fz_colorspace *cs, int cs_channel);

void fz_add_separation_equivalents(fz_context *ctx, fz_separations *sep, uint32_t rgba, uint32_t cmyk, const char *name);

void fz_set_separation_behavior(fz_context *ctx, fz_separations *sep, int separation, fz_separation_behavior behavior);

fz_separation_behavior fz_separation_current_behavior(fz_context *ctx, const fz_separations *sep, int separation);

const char *fz_separation_name(fz_context *ctx, const fz_separations *sep, int separation);
int fz_count_separations(fz_context *ctx, const fz_separations *sep);

int fz_count_active_separations(fz_context *ctx, const fz_separations *seps);

fz_separations *fz_clone_separations_for_overprint(fz_context *ctx, fz_separations *seps);

void fz_convert_separation_colors(fz_context *ctx, fz_colorspace *src_cs, const float *src_color, fz_separations *dst_seps, fz_colorspace *dst_cs, float *dst_color, fz_color_params color_params);

void fz_separation_equivalent(fz_context *ctx, const fz_separations *seps, int idx, fz_colorspace *dst_cs, float *dst_color, fz_colorspace *prf, fz_color_params color_params);

#endif
