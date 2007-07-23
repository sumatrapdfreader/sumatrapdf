#include "fitz-base.h"
#include "fitz-world.h"
#include "fitz-draw.h"

#ifdef _MSC_VER
#undef DEBUG
#define DEBUG printf
#else
#define DEBUG(args...) printf(args)
#ifndef DEBUG
#define DEBUG(args...)
#endif
#endif

#define QUANT(x,a) (((int)((x) * (a))) / (a))
#define HSUBPIX 5.0
#define VSUBPIX 5.0

void fzd_ainboverc(fz_pixmap *color, fz_pixmap *shape, fz_pixmap *dest);
void fzd_maskover(fz_scanargs*, unsigned char*, int, int, int, int, int);
void fzd_maskoverwithsolid(fz_scanargs*, unsigned char*, int, int, int, int, int);
void fzd_imageover(fz_scanargs*, unsigned char*, int, int, int, int, int);
void fzd_imageoverwithsolid(fz_scanargs*, unsigned char*, int, int, int, int, int);
void fzd_imageadd(fz_scanargs*, unsigned char*, int, int, int, int, int);
void fzd_shadeover(fz_scanargs*, unsigned char*, int, int, int, int, int);
void fzd_solidover(unsigned char*, fz_pixmap*);

/*
 * Allocate and destroy graphics context.
 */

fz_error *
fz_newgraphics(fz_graphics **gcp, int gcmem)
{
	fz_error *error;
	fz_graphics *gc;

	gc = fz_malloc(sizeof(fz_graphics));
	if (!gc)
		return fz_outofmem;

	gc->cache = nil;
	gc->gel = nil;
	gc->ael = nil;

	error = fz_newglyphcache(&gc->cache, gcmem / 24, gcmem);
	if (error)
		goto cleanup;

	error = fz_newgel(&gc->gel);
	if (error)
		goto cleanup;

	error = fz_newael(&gc->ael);
	if (error)
		goto cleanup;

	gc->state.ctm = fz_identity();
	gc->state.pcm = nil;
	gc->state.blend = FZ_BNORMAL;
	gc->state.dest = nil;

	gc->funs.ainboverc = fzd_ainboverc;
	gc->funs.maskover = fzd_maskover;
	gc->funs.maskoverwithsolid = fzd_maskoverwithsolid;
	gc->funs.imageover = fzd_imageover;
	gc->funs.imageoverwithsolid = fzd_imageoverwithsolid;
	gc->funs.imageadd = fzd_imageadd;
	gc->funs.shadeover = fzd_shadeover;
	gc->funs.solidover = fzd_solidover;

	memset(gc->solid, sizeof gc->solid, 0);

	*gcp = gc;
	return nil;

cleanup:
	if (gc->cache) fz_dropglyphcache(gc->cache);
	if (gc->gel) fz_dropgel(gc->gel);
	if (gc->ael) fz_dropael(gc->ael);
	fz_free(gc);
	return error;
}

void
fz_dropgraphics(fz_graphics *gc)
{
	if (gc->cache) fz_dropglyphcache(gc->cache);
	if (gc->gel) fz_dropgel(gc->gel);
	if (gc->ael) fz_dropael(gc->ael);

	if (gc->state.pcm) fz_dropcolorspace(gc->state.pcm);
	if (gc->state.dest) fz_droppixmap(gc->state.dest);

	fz_free(gc);
}

/*
 * Entry point for rendering a tree.
 */

fz_error *
fz_drawtree(fz_pixmap **outp,
	fz_graphics *gc, fz_tree *tree, fz_matrix ctm, fz_colorspace *pcm,
	fz_irect bbox, int white)
{
	fz_error *error;

	gc->state.ctm = ctm;
	gc->state.pcm = fz_keepcolorspace(pcm);
	gc->state.blend = FZ_BNORMAL;
	gc->state.dest = nil;

	DEBUG("tree [%d %d %d %d]\n{\n",
			bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	if (gc->state.pcm)
		error = fz_newpixmapwithrect(&gc->state.dest, bbox, gc->state.pcm->n, 1);
	else
		error = fz_newpixmapwithrect(&gc->state.dest, bbox, 0, 1);
	if (error)
		return error;

	memset(gc->state.dest->p, white ? 0xFF : 0x00, gc->state.dest->h * gc->state.dest->s);

	error = fz_drawnode(gc, tree->root);
	if (error)
		return error;

	DEBUG("}\n");

	*outp = gc->state.dest;
	gc->state.dest = nil;

	return nil;
}

fz_error *
fz_drawtreeover(fz_pixmap *out,
	fz_graphics *gc, fz_tree *tree, fz_matrix ctm, fz_colorspace *pcm)
{
	fz_error *error;

	gc->state.ctm = ctm;
	gc->state.pcm = fz_keepcolorspace(pcm);
	gc->state.blend = FZ_BNORMAL;
	gc->state.dest = out;

	DEBUG("tree over\n{\n");

	error = fz_drawnode(gc, tree->root);
	if (error)
		return error;

	DEBUG("}\n");

	gc->state.dest = nil;

	return nil;
}

