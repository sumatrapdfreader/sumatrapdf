#ifndef MUPDF_FITZ_SHADE_H
#define MUPDF_FITZ_SHADE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/colorspace.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/compressed-buffer.h"

/*
 * The shading code uses gouraud shaded triangle meshes.
 */

enum
{
	FZ_FUNCTION_BASED = 1,
	FZ_LINEAR = 2,
	FZ_RADIAL = 3,
	FZ_MESH_TYPE4 = 4,
	FZ_MESH_TYPE5 = 5,
	FZ_MESH_TYPE6 = 6,
	FZ_MESH_TYPE7 = 7
};

typedef struct fz_shade_s fz_shade;

struct fz_shade_s
{
	fz_storable storable;

	fz_rect bbox;		/* can be fz_infinite_rect */
	fz_colorspace *colorspace;

	fz_matrix matrix;	/* matrix from pattern dict */
	int use_background;	/* background color for fills but not 'sh' */
	float background[FZ_MAX_COLORS];

	int use_function;
	float function[256][FZ_MAX_COLORS + 1];

	int type; /* function, linear, radial, mesh */
	union
	{
		struct
		{
			int extend[2];
			float coords[2][3]; /* (x,y,r) twice */
		} l_or_r;
		struct
		{
			int vprow;
			int bpflag;
			int bpcoord;
			int bpcomp;
			float x0, x1;
			float y0, y1;
			float c0[FZ_MAX_COLORS];
			float c1[FZ_MAX_COLORS];
		} m;
		struct
		{
			fz_matrix matrix;
			int xdivs;
			int ydivs;
			float domain[2][2];
			float *fn_vals;
		} f;
	} u;

	fz_compressed_buffer *buffer;
};

fz_shade *fz_keep_shade(fz_context *ctx, fz_shade *shade);
void fz_drop_shade(fz_context *ctx, fz_shade *shade);
void fz_free_shade_imp(fz_context *ctx, fz_storable *shade);

fz_rect *fz_bound_shade(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_rect *r);
void fz_paint_shade(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_pixmap *dest, const fz_irect *bbox);

/*
 *	Handy routine for processing mesh based shades
 */
typedef struct fz_vertex_s fz_vertex;

struct fz_vertex_s
{
	fz_point p;
	float c[FZ_MAX_COLORS];
};

typedef struct fz_mesh_processor_s fz_mesh_processor;

typedef void (fz_mesh_process_fn)(void *arg, fz_vertex *av, fz_vertex *bv, fz_vertex *cv);

struct fz_mesh_processor_s {
	fz_context *ctx;
	fz_shade *shade;
	fz_mesh_process_fn *process;
	void *process_arg;
	int ncomp;
};

void fz_process_mesh(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm,
			fz_mesh_process_fn *process, void *process_arg);

#ifndef NDEBUG
void fz_print_shade(fz_context *ctx, FILE *out, fz_shade *shade);
#endif

#endif
