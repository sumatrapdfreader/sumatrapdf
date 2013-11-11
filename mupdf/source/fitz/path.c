#include <assert.h>
#include "mupdf/fitz.h"

fz_path *
fz_new_path(fz_context *ctx)
{
	fz_path *path;

	path = fz_malloc_struct(ctx, fz_path);
	path->last_cmd = 0;
	path->current.x = 0;
	path->current.y = 0;
	path->begin.x = 0;
	path->begin.y = 0;

	return path;
}

fz_path *
fz_clone_path(fz_context *ctx, fz_path *old)
{
	fz_path *path;

	assert(old);
	path = fz_malloc_struct(ctx, fz_path);
	fz_try(ctx)
	{
		path->cmd_len = old->cmd_len;
		path->cmd_cap = old->cmd_len;
		path->cmds = fz_malloc_array(ctx, path->cmd_cap, sizeof(unsigned char));
		memcpy(path->cmds, old->cmds, sizeof(unsigned char) * path->cmd_len);

		path->coord_len = old->coord_len;
		path->coord_cap = old->coord_len;
		path->coords = fz_malloc_array(ctx, path->coord_cap, sizeof(float));
		memcpy(path->coords, old->coords, sizeof(float) * path->coord_len);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, path->cmds);
		fz_free(ctx, path->coords);
		fz_free(ctx, path);
		fz_rethrow(ctx);
	}

	return path;
}

void
fz_free_path(fz_context *ctx, fz_path *path)
{
	if (path == NULL)
		return;
	fz_free(ctx, path->cmds);
	fz_free(ctx, path->coords);
	fz_free(ctx, path);
}

static void
push_cmd(fz_context *ctx, fz_path *path, int cmd)
{
	if (path->cmd_len + 1 >= path->cmd_cap)
	{
		int new_cmd_cap = fz_maxi(16, path->cmd_cap * 2);
		path->cmds = fz_resize_array(ctx, path->cmds, new_cmd_cap, sizeof(unsigned char));
		path->cmd_cap = new_cmd_cap;
	}

	path->cmds[path->cmd_len++] = cmd;
	path->last_cmd = cmd;
}

static void
push_coord(fz_context *ctx, fz_path *path, float x, float y)
{
	if (path->coord_len + 2 >= path->coord_cap)
	{
		int new_coord_cap = fz_maxi(32, path->coord_cap * 2);
		path->coords = fz_resize_array(ctx, path->coords, new_coord_cap, sizeof(float));
		path->coord_cap = new_coord_cap;
	}

	path->coords[path->coord_len++] = x;
	path->coords[path->coord_len++] = y;

	path->current.x = x;
	path->current.y = y;
}

fz_point
fz_currentpoint(fz_context *ctx, fz_path *path)
{
	return path->current;
}

void
fz_moveto(fz_context *ctx, fz_path *path, float x, float y)
{
	if (path->cmd_len > 0 && path->last_cmd == FZ_MOVETO)
	{
		/* Collapse moveto followed by moveto. */
		path->coords[path->coord_len-2] = x;
		path->coords[path->coord_len-1] = y;
		path->current.x = x;
		path->current.y = y;
		path->begin = path->current;
		return;
	}

	push_cmd(ctx, path, FZ_MOVETO);
	push_coord(ctx, path, x, y);

	path->begin = path->current;
}

void
fz_lineto(fz_context *ctx, fz_path *path, float x, float y)
{
	float x0 = path->current.x;
	float y0 = path->current.y;

	if (path->cmd_len == 0)
	{
		fz_warn(ctx, "lineto with no current point");
		return;
	}

	/* Anything other than MoveTo followed by LineTo the same place is a nop */
	if (path->last_cmd != FZ_MOVETO && x0 == x && y0 == y)
		return;

	push_cmd(ctx, path, FZ_LINETO);
	push_coord(ctx, path, x, y);
}

void
fz_curveto(fz_context *ctx, fz_path *path,
	float x1, float y1,
	float x2, float y2,
	float x3, float y3)
{
	float x0 = path->current.x;
	float y0 = path->current.y;

	if (path->cmd_len == 0)
	{
		fz_warn(ctx, "curveto with no current point");
		return;
	}

	/* Check for degenerate cases: */
	if (x0 == x1 && y0 == y1)
	{
		if (x2 == x3 && y2 == y3)
		{
			/* If (x1,y1)==(x2,y2) and prev wasn't a moveto, then skip */
			if (x1 == x2 && y1 == y2 && path->last_cmd != FZ_MOVETO)
				return;
			/* Otherwise a line will suffice */
			fz_lineto(ctx, path, x3, y3);
			return;
		}
		if (x1 == x2 && y1 == y2)
		{
			/* A line will suffice */
			fz_lineto(ctx, path, x3, y3);
			return;
		}
	}
	else if (x1 == x2 && y1 == y2 && x2 == x3 && y2 == y3)
	{
		/* A line will suffice */
		fz_lineto(ctx, path, x3, y3);
		return;
	}

	push_cmd(ctx, path, FZ_CURVETO);
	push_coord(ctx, path, x1, y1);
	push_coord(ctx, path, x2, y2);
	push_coord(ctx, path, x3, y3);
}

void
fz_curvetov(fz_context *ctx, fz_path *path, float x2, float y2, float x3, float y3)
{
	float x1 = path->current.x;
	float y1 = path->current.y;

	if (path->cmd_len == 0)
	{
		fz_warn(ctx, "curvetov with no current point");
		return;
	}

	fz_curveto(ctx, path, x1, y1, x2, y2, x3, y3);
}

void
fz_curvetoy(fz_context *ctx, fz_path *path, float x1, float y1, float x3, float y3)
{
	fz_curveto(ctx, path, x1, y1, x3, y3, x3, y3);
}

void
fz_closepath(fz_context *ctx, fz_path *path)
{
	if (path->cmd_len == 0)
	{
		fz_warn(ctx, "closepath with no current point");
		return;
	}

	/* CLOSE following a CLOSE is a NOP */
	if (path->last_cmd == FZ_CLOSE_PATH)
		return;

	push_cmd(ctx, path, FZ_CLOSE_PATH);

	path->current = path->begin;
}

static inline fz_rect *bound_expand(fz_rect *r, const fz_point *p)
{
	if (p->x < r->x0) r->x0 = p->x;
	if (p->y < r->y0) r->y0 = p->y;
	if (p->x > r->x1) r->x1 = p->x;
	if (p->y > r->y1) r->y1 = p->y;
	return r;
}

fz_rect *
fz_bound_path(fz_context *ctx, fz_path *path, const fz_stroke_state *stroke, const fz_matrix *ctm, fz_rect *r)
{
	fz_point p;
	int i = 0, k = 0;

	/* If the path is empty, return the empty rectangle here - don't wait
	 * for it to be expanded in the stroked case below.
	 * A path must start with a moveto - and if that's all there is
	 * then the path is empty. */
	if (path->cmd_len == 0 || path->cmd_len == 1)
	{
		*r = fz_empty_rect;
		return r;
	}

	/* Initial moveto point */
	p.x = path->coords[0];
	p.y = path->coords[1];
	fz_transform_point(&p, ctm);
	r->x0 = r->x1 = p.x;
	r->y0 = r->y1 = p.y;

	while (i < path->cmd_len)
	{
		switch (path->cmds[i++])
		{
		case FZ_CURVETO:
			p.x = path->coords[k++];
			p.y = path->coords[k++];
			bound_expand(r, fz_transform_point(&p, ctm));
			p.x = path->coords[k++];
			p.y = path->coords[k++];
			bound_expand(r, fz_transform_point(&p, ctm));
			p.x = path->coords[k++];
			p.y = path->coords[k++];
			bound_expand(r, fz_transform_point(&p, ctm));
			break;
		case FZ_MOVETO:
			if (k + 2 == path->coord_len)
			{
				/* Trailing Moveto - cannot affect bbox */
				k += 2;
				break;
			}
			/* fallthrough */
		case FZ_LINETO:
			p.x = path->coords[k++];
			p.y = path->coords[k++];
			bound_expand(r, fz_transform_point(&p, ctm));
			break;
		case FZ_CLOSE_PATH:
			break;
		}
	}

	if (stroke)
	{
		fz_adjust_rect_for_stroke(r, stroke, ctm);
	}

	return r;
}

fz_rect *
fz_adjust_rect_for_stroke(fz_rect *r, const fz_stroke_state *stroke, const fz_matrix *ctm)
{
	float expand;

	if (!stroke)
		return r;

	expand = stroke->linewidth;
	expand *= 0.5; /* SumatraPDF: expansion happens from the middle of the line */
	if (expand == 0)
		expand = 1.0f;
	expand *= fz_matrix_max_expansion(ctm);
	if ((stroke->linejoin == FZ_LINEJOIN_MITER || stroke->linejoin == FZ_LINEJOIN_MITER_XPS) && stroke->miterlimit > 1)
		expand *= stroke->miterlimit;

	r->x0 -= expand;
	r->y0 -= expand;
	r->x1 += expand;
	r->y1 += expand;
	return r;
}

void
fz_transform_path(fz_context *ctx, fz_path *path, const fz_matrix *ctm)
{
	int i;
	for (i = 0; i < path->coord_len; i += 2)
		fz_transform_point((fz_point *)&path->coords[i], ctm);
}

#ifndef NDEBUG
void
fz_print_path(fz_context *ctx, FILE *out, fz_path *path, int indent)
{
	float x, y;
	int i = 0, k = 0;
	int n;
	while (i < path->cmd_len)
	{
		for (n = 0; n < indent; n++)
			fputc(' ', out);
		switch (path->cmds[i++])
		{
		case FZ_MOVETO:
			x = path->coords[k++];
			y = path->coords[k++];
			fprintf(out, "%g %g m\n", x, y);
			break;
		case FZ_LINETO:
			x = path->coords[k++];
			y = path->coords[k++];
			fprintf(out, "%g %g l\n", x, y);
			break;
		case FZ_CURVETO:
			x = path->coords[k++];
			y = path->coords[k++];
			fprintf(out, "%g %g ", x, y);
			x = path->coords[k++];
			y = path->coords[k++];
			fprintf(out, "%g %g ", x, y);
			x = path->coords[k++];
			y = path->coords[k++];
			fprintf(out, "%g %g c\n", x, y);
			break;
		case FZ_CLOSE_PATH:
			fprintf(out, "h\n");
			break;
		}
	}
}
#endif

const fz_stroke_state fz_default_stroke_state = {
	-2, /* -2 is the magic number we use when we have stroke states stored on the stack */
	FZ_LINECAP_BUTT, FZ_LINECAP_BUTT, FZ_LINECAP_BUTT,
	FZ_LINEJOIN_MITER,
	1, 10,
	0, 0, { 0 }
};

fz_stroke_state *
fz_keep_stroke_state(fz_context *ctx, fz_stroke_state *stroke)
{
	if (!stroke)
		return NULL;

	/* -2 is the magic number we use when we have stroke states stored on the stack */
	if (stroke->refs == -2)
		return fz_clone_stroke_state(ctx, stroke);

	fz_lock(ctx, FZ_LOCK_ALLOC);
	if (stroke->refs > 0)
		stroke->refs++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	return stroke;
}

void
fz_drop_stroke_state(fz_context *ctx, fz_stroke_state *stroke)
{
	int drop;

	if (!stroke)
		return;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	drop = (stroke->refs > 0 ? --stroke->refs == 0 : 0);
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (drop)
		fz_free(ctx, stroke);
}

fz_stroke_state *
fz_new_stroke_state_with_dash_len(fz_context *ctx, int len)
{
	fz_stroke_state *state;

	len -= nelem(state->dash_list);
	if (len < 0)
		len = 0;

	state = Memento_label(fz_malloc(ctx, sizeof(*state) + sizeof(state->dash_list[0]) * len), "fz_stroke_state");
	state->refs = 1;
	state->start_cap = FZ_LINECAP_BUTT;
	state->dash_cap = FZ_LINECAP_BUTT;
	state->end_cap = FZ_LINECAP_BUTT;
	state->linejoin = FZ_LINEJOIN_MITER;
	state->linewidth = 1;
	state->miterlimit = 10;
	state->dash_phase = 0;
	state->dash_len = 0;
	memset(state->dash_list, 0, sizeof(state->dash_list[0]) * (len + nelem(state->dash_list)));

	return state;
}

fz_stroke_state *
fz_new_stroke_state(fz_context *ctx)
{
	return fz_new_stroke_state_with_dash_len(ctx, 0);
}

fz_stroke_state *
fz_clone_stroke_state(fz_context *ctx, fz_stroke_state *stroke)
{
	fz_stroke_state *clone = fz_new_stroke_state_with_dash_len(ctx, stroke->dash_len);
	int extra = stroke->dash_len - nelem(stroke->dash_list);
	int size = sizeof(*stroke) + sizeof(stroke->dash_list[0]) * extra;
	memcpy(clone, stroke, size);
	clone->refs = 1;
	return clone;
}

fz_stroke_state *
fz_unshare_stroke_state_with_dash_len(fz_context *ctx, fz_stroke_state *shared, int len)
{
	int single, unsize, shsize, shlen, drop;
	fz_stroke_state *unshared;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	single = (shared->refs == 1);
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	shlen = shared->dash_len - nelem(shared->dash_list);
	if (shlen < 0)
		shlen = 0;
	shsize = sizeof(*shared) + sizeof(shared->dash_list[0]) * shlen;
	len -= nelem(shared->dash_list);
	if (len < 0)
		len = 0;
	if (single && shlen >= len)
		return shared;
	unsize = sizeof(*unshared) + sizeof(unshared->dash_list[0]) * len;
	unshared = Memento_label(fz_malloc(ctx, unsize), "fz_stroke_state");
	memcpy(unshared, shared, (shsize > unsize ? unsize : shsize));
	unshared->refs = 1;
	fz_lock(ctx, FZ_LOCK_ALLOC);
	drop = (shared->refs > 0 ? --shared->refs == 0 : 0);
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (drop)
		fz_free(ctx, shared);
	return unshared;
}

fz_stroke_state *
fz_unshare_stroke_state(fz_context *ctx, fz_stroke_state *shared)
{
	return fz_unshare_stroke_state_with_dash_len(ctx, shared, shared->dash_len);
}
