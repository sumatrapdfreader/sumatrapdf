#ifndef MUPDF_FITZ_PATH_H
#define MUPDF_FITZ_PATH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"

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

typedef enum fz_path_command_e
{
	FZ_MOVETO = 'M',
	FZ_LINETO = 'L',
	FZ_CURVETO = 'C',
	FZ_CLOSE_PATH = 'Z',
} fz_path_item_kind;

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

struct fz_path_s
{
	int cmd_len, cmd_cap;
	unsigned char *cmds;
	int coord_len, coord_cap;
	float *coords;
	fz_point current;
	fz_point begin;
	int last_cmd;
};

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

fz_path *fz_new_path(fz_context *ctx);
fz_point fz_currentpoint(fz_context *ctx, fz_path *path);
void fz_moveto(fz_context*, fz_path*, float x, float y);
void fz_lineto(fz_context*, fz_path*, float x, float y);
void fz_curveto(fz_context*,fz_path*, float, float, float, float, float, float);
void fz_curvetov(fz_context*,fz_path*, float, float, float, float);
void fz_curvetoy(fz_context*,fz_path*, float, float, float, float);
void fz_closepath(fz_context*,fz_path*);
void fz_free_path(fz_context *ctx, fz_path *path);

void fz_transform_path(fz_context *ctx, fz_path *path, const fz_matrix *transform);

fz_path *fz_clone_path(fz_context *ctx, fz_path *old);

fz_rect *fz_bound_path(fz_context *ctx, fz_path *path, const fz_stroke_state *stroke, const fz_matrix *ctm, fz_rect *r);
fz_rect *fz_adjust_rect_for_stroke(fz_rect *r, const fz_stroke_state *stroke, const fz_matrix *ctm);

extern const fz_stroke_state fz_default_stroke_state;

fz_stroke_state *fz_new_stroke_state(fz_context *ctx);
fz_stroke_state *fz_new_stroke_state_with_dash_len(fz_context *ctx, int len);
fz_stroke_state *fz_keep_stroke_state(fz_context *ctx, fz_stroke_state *stroke);
void fz_drop_stroke_state(fz_context *ctx, fz_stroke_state *stroke);
fz_stroke_state *fz_unshare_stroke_state(fz_context *ctx, fz_stroke_state *shared);
fz_stroke_state *fz_unshare_stroke_state_with_dash_len(fz_context *ctx, fz_stroke_state *shared, int len);
fz_stroke_state *fz_clone_stroke_state(fz_context *ctx, fz_stroke_state *stroke);

#ifndef NDEBUG
void fz_print_path(fz_context *ctx, FILE *out, fz_path *, int indent);
#endif

#endif
