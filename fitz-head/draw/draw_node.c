#include "fitz-base.h"
#include "fitz-world.h"
#include "fitz-draw.h"

int gpixcnt = 1;

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

/*
 * Generic node.
 * Switch on type, set up scratch buffers and invoke
 * sub-rendering function.
 */

fz_error *
fz_drawnode(fz_graphics *gc, fz_node *node)
{
printf("drawnode cs=%s\n", gc->state.pcm ? gc->state.pcm->name : "none");
	switch (node->kind)
	{
	case FZ_NLINK:
		return fz_drawnode(gc, ((fz_linknode*)node)->tree->root);
	case FZ_NMETA:
		return fz_drawnode(gc, node->first);
	case FZ_NTRANSFORM:
		return fz_drawtransform(gc, (fz_transformnode*)node);
	case FZ_NBLEND:
		return fz_drawblend(gc, (fz_blendnode*)node);
	case FZ_NMASK:
		return fz_drawmask(gc, (fz_masknode*)node);
	case FZ_NOVER:
		return fz_drawover(gc, (fz_overnode*)node);
	case FZ_NCOLOR:
		return fz_drawsolid(gc, (fz_solidnode*)node);
	case FZ_NPATH:
		return fz_drawpath(gc, (fz_pathnode*)node);
	case FZ_NTEXT:
		return fz_drawtext(gc, (fz_textnode*)node);
	case FZ_NIMAGE:
		return fz_drawimage(gc, (fz_imagenode*)node);
	case FZ_NSHADE:
		return fz_drawshade(gc, (fz_shadenode*)node);
	}

	return fz_throw("unhandled node type in graphics: %d", node->kind);
}


/*
 * Transform.
 * Inherit new CTM for child.
 */

fz_error *
fz_drawtransform(fz_graphics *gc, fz_transformnode *node)
{
	fz_gstate savestate;
	fz_error *error;

	DEBUG("transform [%g %g %g %g %g %g]\n",
		node->m.a, node->m.b,
		node->m.c, node->m.d,
		node->m.e, node->m.f);
	DEBUG("{\n");

	savestate = gc->state;

	gc->state.ctm = fz_concat(node->m, gc->state.ctm);

	error = fz_drawnode(gc, node->super.first);
	if (error)
		return error;

	gc->state = savestate;

	DEBUG("}\n");

	return nil;
}


/*
 * Blend.
 * Inherit PCM.
 * Inherit blend mode.
 * Set up isolated/knockout buffers.
 */

fz_error *
fz_drawblend(fz_graphics *gc, fz_blendnode *node)
{
	fz_error *error;

	DEBUG("blend\n{\n");

	// TODO: change pcm, set up new backdrop, etc...

	error = fz_drawnode(gc, node->super.first);
	if (error)
		return error;

	// TODO: color convert result, etc...

	DEBUG("}\n");

	return nil;
}


/*
 * Mask.
 * If shape=path/text/imagemask and color=solid
 *   Render optimised
 * Else
 *   Render shape child isolated.
 *   Inherit clip mask.
 *   Render color child.
 */

fz_error *
fz_drawmask(fz_graphics *gc, fz_masknode *node)
{
	fz_error *error;
	fz_gstate savestate;
	fz_node *shapenode;
	fz_node *colornode;
	fz_pixmap *shapepix;
	fz_pixmap *colorpix;
	fz_irect bbox;
	int a;

	char name[32];

	shapenode = node->super.first;
	colornode = shapenode->next;

	DEBUG("mask\n{\n");

	a = gc->state.dest->a;
	if (a == 0)
		a = 1;

printf("drawmask a=%d\n", a);

	bbox = fz_boundpixmap(gc->state.dest);
	bbox = fz_intersectirects(bbox, fz_roundrect(fz_boundnode(shapenode, gc->state.ctm)));
	bbox = fz_intersectirects(bbox, fz_roundrect(fz_boundnode(colornode, gc->state.ctm)));

printf("maskbbox [%d %d %d %d]\n", bbox);

#if 0
	if (fz_issolidnode(colornode))
	{
		fz_solidnode *solid = (fz_solidnode*)colornode;

		fz_convertcolor(solid->cs, solid->p, gc->state.pcm, rgb);
		gc->solid[0] = rgb[0] * 255;
		gc->solid[1] = rgb[1] * 255;
		gc->solid[2] = rgb[2] * 255;
		gc->flag |= FRGB;

		/* we know these can handle drawing with solid color */
		if (fz_ispathnode(shapenode))
			return fz_drawpath(gc, (fz_pathnode*)shapenode, ctm);
		if (fz_istextnode(shapenode))
			return fz_drawtext(gc, (fz_textnode*)shapenode, ctm);
		if (fz_isimagenode(shapenode))
			return fz_drawimage(gc, (fz_imagenode*)shapenode, ctm);
	}
#endif

	/* draw shape isolated */
	DEBUG("shape {\n");
	{
		savestate = gc->state;

		error = fz_newpixmapwithrect(&shapepix, bbox, 0, a);
		if (error)
			goto cleanup;

		fz_clearpixmap(shapepix);

		gc->state.dest = shapepix;
		gc->state.pcm = nil;
		gc->state.blend = FZ_BNORMAL;

		error = fz_drawnode(gc, shapenode);
		if (error)
			goto cleanupshape;

		gc->state = savestate;

		sprintf(name, "trace-%03d-shape.pnm", gpixcnt++);
		fz_debugpixmap(shapepix, name);
	}
	DEBUG("}\n");

	/* draw color isolated */
	DEBUG("color {\n");
	{
		savestate = gc->state;

		error = fz_newpixmapwithrect(&colorpix, bbox, gc->state.pcm->n, a);
		if (error)
			goto cleanupshape;

		fz_clearpixmap(colorpix);

		gc->state.dest = colorpix;

		error = fz_drawnode(gc, colornode);
		if (error)
			goto cleanupcolor;

		gc->state = savestate;

		sprintf(name, "trace-%03d-color.pnm", gpixcnt++);
		fz_debugpixmap(colorpix, name);
	}
	DEBUG("}\n");

	// mask colorpix and shapepix then combine result with over
	gc->funs.ainboverc(colorpix, shapepix, gc->state.dest);

	sprintf(name, "trace-%03d-dest.pnm", gpixcnt++);
	fz_debugpixmap(gc->state.dest, name);

	fz_droppixmap(colorpix);
	fz_droppixmap(shapepix);

	DEBUG("}\n");

	return nil;

cleanupcolor:
	fz_droppixmap(colorpix);
cleanupshape:
	fz_droppixmap(shapepix);
cleanup:
printf("!!! error: %s\n", error->msg);
	return error;
}


/*
 * Over.
 * Render children, combine with backdrop using current blend mode.
 */

fz_error *
fz_drawover(fz_graphics *gc, fz_overnode *node)
{
	fz_error *error;
	fz_node *child;

	DEBUG("over\n{\n");

	for (child = node->super.first; child != nil; child = child->next)
	{
		error = fz_drawnode(gc, child);
		if (error)
			return error;
	}

	DEBUG("}\n");

	return nil;
}


/*
 * Solid.
 * Transform color
 * Fill byte sample buffer
 * Invoke specified primitive
 */

fz_error *
fz_drawsolid(fz_graphics *gc, fz_solidnode *node)
{
	float fsolid[FZ_MAXCOLORS + 2];
	unsigned char isolid[FZ_MAXCOLORS + 2];
	fz_pixmap *dst = gc->state.dest;
	float f, q;
	int i;

	DEBUG("solid n=%d a=%d cs=%s ;\n",
		node->n, node->a, node->cs ? node->cs->name : "(none)");
	DEBUG("  dest cs=%s\n", gc->state.pcm->name);
	DEBUG("  dst n=%d a=%d\n", dst->n, dst->a);

	fz_convertcolor(node->cs, node->p, gc->state.pcm, fsolid);
	for (i = 0; i < dst->n; i++)
		isolid[i] = fsolid[i] * 0xFF;

	switch (node->a)
	{
	case 0: f = 1.0; q = 1.0; break;
	case 1: f = node->p[node->n]; q = 1.0; break;
	case 2: f = node->p[node->n]; q = node->p[node->n + 1]; break;
	}

	switch (dst->a)
	{
	case 0: break;
	case 1: isolid[dst->n] = f * q * 0xFF; break;
	case 2: isolid[dst->n] = f * 0xFF; isolid[dst->n + 1] = q * 0xFF; break;
	}

printf("solidover\n");
for (i = 0; i < dst->n + dst->a; i++) printf("  %d\n", isolid[i]);

	gc->funs.solidover(isolid, gc->state.dest);

	return nil;
}


/*
 * Path.
 * Fill edge list.
 * Call scan conversion with specified primitive.
 */

fz_error *
fz_drawpath(fz_graphics *gc, fz_pathnode *node)
{
	fz_error *error;
	fz_pixmap *dst = gc->state.dest;
	float flatness;
	fz_scanargs args;

	DEBUG("path %s ;\n", node->paint == FZ_STROKE ? "stroke" : "fill");

	assert(gc->state.dest->n == 0 && gc->state.dest->a == 1 && gc->state.pcm == nil);

	flatness = 0.3 / fz_matrixexpansion(gc->state.ctm);
	if (flatness < 0.1)
		flatness = 0.1f;

printf("flatness = %g\n", flatness);

	fz_resetgel(gc->gel, fz_boundpixmap(dst));

	if (node->paint == FZ_STROKE)
	{
		if (node->dash)
			error = fz_dashpath(gc->gel, node, gc->state.ctm, flatness);
		else
			error = fz_strokepath(gc->gel, node, gc->state.ctm, flatness);
	}
	else
		error = fz_fillpath(gc->gel, node, gc->state.ctm, flatness);
	if (error)
		return error;

	fz_sortgel(gc->gel);

printf("sortgel okay\n");

	args.blit = gc->funs.maskover;
	args.dest = gc->state.dest;
	args.nt = 0;

	return fz_scanconvert(gc->gel, gc->ael, node->paint == FZ_EOFILL, &args);
}


/*
 * Text.
 * Apply transform matrices
 * Get bitmaps from glyph cache system
 * Invoke specified primitive for each glyph
 */

fz_error *
fz_drawtext(fz_graphics *gc, fz_textnode *node)
{
	fz_pixmap *dst = gc->state.dest;
	fz_error *error;
	fz_matrix tm, trm;
	fz_glyph glyph;
	int i, cid, x, y;
	fz_scanargs args;

	DEBUG("text %s n=%d [%g %g %g %g] ;\n",
			node->font->name, node->len,
			node->trm.a, node->trm.b, node->trm.c, node->trm.d);

	assert(dst && dst->n == 0 && dst->a == 1 && gc->state.pcm == nil);

	memset(dst->p, 00, dst->w * dst->h * (dst->n + dst->a));

	tm = node->trm;

printf("node->len = %d\n", node->len);

	for (i = 0; i < node->len; i++)
	{
		cid = node->els[i].cid;
		tm.e = node->els[i].x;
		tm.f = node->els[i].y;

		trm = fz_concat(tm, gc->state.ctm);
		x = fz_floor(trm.e);
		y = fz_floor(trm.f);
		trm.e = QUANT(trm.e - fz_floor(trm.e), HSUBPIX);
		trm.f = QUANT(trm.f - fz_floor(trm.f), VSUBPIX);

printf("glyf %d ", i);

		error = fz_renderglyph(gc->cache, &glyph, node->font, cid, trm);
		if (error)
			return error;

printf("%dx%d\n", glyph.w, glyph.h);
		/* todo: clip to dest before calling maskover */

		args.dest = gc->state.dest;
		args.nt = 0;
		{
			unsigned char *sp = glyph.p;

			int dx0 = args.dest->x;
			int dy0 = args.dest->y;
			int dx1 = dx0 + args.dest->w;
			int dy1 = dy0 + args.dest->h;

			int sx0 = x + glyph.x;
			int sy0 = y + glyph.y;
			int sx1 = sx0 + glyph.w;
			int sy1 = sy0 + glyph.h;

			int w, h;

			if (sx0 < dx0) { sp += dx0 - sx0; sx0 = dx0; }
			if (sy0 < dy0) { sp += (dy0 - sy0) * glyph.w; sy0 = dy0; }
			if (sx1 > dx1) sx1 = dx1;
			if (sy1 > dy1) sy1 = dy1;

			w = sx1 - sx0;
			h = sy1 - sy0;

			if (w > 0 && h > 0)
				gc->funs.maskover(&args, sp, sx0, sy0, w, h, glyph.w);
		}
	}

	return nil;
}


/*
 * Image.
 * Load image data into pixmap
 * Convert color space
 * Scale pixmap
 * Invoke specified primitive
 */

fz_error *
fz_drawimage(fz_graphics *gc, fz_imagenode *node)
{
	fz_image *image = node->image;
	DEBUG("image %dx%d n=%d a=%d cs=%s ;\n",
			image->w, image->h,
			image->n, image->a, image->cs ? image->cs->name : "(nil)");
	return nil;
}


/*
 * Shade.
 * Invoke specified primitive
 */

fz_error *
fz_drawshade(fz_graphics *gc, fz_shadenode *node)
{
	DEBUG("shade ;\n");
	return nil;
}


