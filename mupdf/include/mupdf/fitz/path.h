#ifndef MUPDF_FITZ_PATH_H
#define MUPDF_FITZ_PATH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"

/*
 * Vector path buffer.
 * It can be stroked and dashed, or be filled.
 * It has a fill rule (nonzero or even_odd).
 *
 * When rendering, they are flattened, stroked and dashed straight
 * into the Global Edge List.
 */

typedef struct fz_path_s fz_path;
typedef struct fz_stroke_state_s fz_stroke_state;

typedef enum fz_linecap_e
{
	FZ_LINECAP_BUTT = 0,
	FZ_LINECAP_ROUND = 1,
	FZ_LINECAP_SQUARE = 2,
	FZ_LINECAP_TRIANGLE = 3
} fz_linecap;

typedef enum fz_linejoin_e
{
	FZ_LINEJOIN_MITER = 0,
	FZ_LINEJOIN_ROUND = 1,
	FZ_LINEJOIN_BEVEL = 2,
	FZ_LINEJOIN_MITER_XPS = 3
} fz_linejoin;

struct fz_stroke_state_s
{
	int refs;
	fz_linecap start_cap, dash_cap, end_cap;
	fz_linejoin linejoin;
	float linewidth;
	float miterlimit;
	float dash_phase;
	int dash_len;
	float dash_list[32];
};

typedef struct
{
	/* Compulsory ones */
	void (*moveto)(fz_context *ctx, void *arg, float x, float y);
	void (*lineto)(fz_context *ctx, void *arg, float x, float y);
	void (*curveto)(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3);
	void (*closepath)(fz_context *ctx, void *arg);
	/* Optional ones */
	void (*quadto)(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2);
	void (*curvetov)(fz_context *ctx, void *arg, float x2, float y2, float x3, float y3);
	void (*curvetoy)(fz_context *ctx, void *arg, float x1, float y1, float x3, float y3);
	void (*rectto)(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2);
} fz_path_walker;

void fz_walk_path(fz_context *ctx, const fz_path *path, const fz_path_walker *walker, void *arg);

fz_path *fz_new_path(fz_context *ctx);

fz_path *fz_keep_path(fz_context *ctx, const fz_path *path);
void fz_drop_path(fz_context *ctx, const fz_path *path);

void fz_trim_path(fz_context *ctx, fz_path *path);

int fz_packed_path_size(const fz_path *path);

size_t fz_pack_path(fz_context *ctx, uint8_t *pack, size_t max, const fz_path *path);

fz_path *fz_clone_path(fz_context *ctx, fz_path *path);

fz_point fz_currentpoint(fz_context *ctx, fz_path *path);

void fz_moveto(fz_context *ctx, fz_path *path, float x, float y);

void fz_lineto(fz_context *ctx, fz_path *path, float x, float y);

void fz_rectto(fz_context *ctx, fz_path *path, float x0, float y0, float x1, float y1);

void fz_quadto(fz_context *ctx, fz_path *path, float x0, float y0, float x1, float y1);

void fz_curveto(fz_context *ctx, fz_path *path, float x0, float y0, float x1, float y1, float x2, float y2);

void fz_curvetov(fz_context *ctx, fz_path *path, float x1, float y1, float x2, float y2);

void fz_curvetoy(fz_context *ctx, fz_path *path, float x0, float y0, float x2, float y2);

void fz_closepath(fz_context *ctx, fz_path *path);

void fz_transform_path(fz_context *ctx, fz_path *path, fz_matrix transform);

fz_rect fz_bound_path(fz_context *ctx, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm);
fz_rect fz_adjust_rect_for_stroke(fz_context *ctx, fz_rect rect, const fz_stroke_state *stroke, fz_matrix ctm);

extern const fz_stroke_state fz_default_stroke_state;

fz_stroke_state *fz_new_stroke_state(fz_context *ctx);

fz_stroke_state *fz_new_stroke_state_with_dash_len(fz_context *ctx, int len);

fz_stroke_state *fz_keep_stroke_state(fz_context *ctx, const fz_stroke_state *stroke);

void fz_drop_stroke_state(fz_context *ctx, const fz_stroke_state *stroke);

fz_stroke_state *fz_unshare_stroke_state(fz_context *ctx, fz_stroke_state *shared);

fz_stroke_state *fz_unshare_stroke_state_with_dash_len(fz_context *ctx, fz_stroke_state *shared, int len);

fz_stroke_state *fz_clone_stroke_state(fz_context *ctx, fz_stroke_state *stroke);

#endif
