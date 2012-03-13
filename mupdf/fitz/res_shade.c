#include "fitz-internal.h"

fz_shade *
fz_keep_shade(fz_context *ctx, fz_shade *shade)
{
	return (fz_shade *)fz_keep_storable(ctx, &shade->storable);
}

void
fz_free_shade_imp(fz_context *ctx, fz_storable *shade_)
{
	fz_shade *shade = (fz_shade *)shade_;

	if (shade->colorspace)
		fz_drop_colorspace(ctx, shade->colorspace);
	fz_free(ctx, shade->mesh);
	fz_free(ctx, shade);
}

void
fz_drop_shade(fz_context *ctx, fz_shade *shade)
{
	fz_drop_storable(ctx, &shade->storable);
}

fz_rect
fz_bound_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm)
{
	float *v;
	fz_rect r, s;
	fz_point p;
	int i, ncomp, nvert;

	ctm = fz_concat(shade->matrix, ctm);
	ncomp = shade->use_function ? 3 : 2 + shade->colorspace->n;
	nvert = shade->mesh_len / ncomp;
	v = shade->mesh;

	s = fz_transform_rect(ctm, shade->bbox);
	if (shade->type == FZ_LINEAR)
		return fz_intersect_rect(s, fz_infinite_rect);
	if (shade->type == FZ_RADIAL)
		return fz_intersect_rect(s, fz_infinite_rect);

	if (nvert == 0)
		return fz_empty_rect;

	p.x = v[0];
	p.y = v[1];
	v += ncomp;
	p = fz_transform_point(ctm, p);
	r.x0 = r.x1 = p.x;
	r.y0 = r.y1 = p.y;

	for (i = 1; i < nvert; i++)
	{
		p.x = v[0];
		p.y = v[1];
		p = fz_transform_point(ctm, p);
		v += ncomp;
		if (p.x < r.x0) r.x0 = p.x;
		if (p.y < r.y0) r.y0 = p.y;
		if (p.x > r.x1) r.x1 = p.x;
		if (p.y > r.y1) r.y1 = p.y;
	}

	return fz_intersect_rect(s, r);
}

void
fz_print_shade(fz_context *ctx, FILE *out, fz_shade *shade)
{
	int i, j, n;
	float *vertex;
	int triangle;

	fprintf(out, "shading {\n");

	switch (shade->type)
	{
	case FZ_LINEAR: fprintf(out, "\ttype linear\n"); break;
	case FZ_RADIAL: fprintf(out, "\ttype radial\n"); break;
	case FZ_MESH: fprintf(out, "\ttype mesh\n"); break;
	}

	fprintf(out, "\tbbox [%g %g %g %g]\n",
		shade->bbox.x0, shade->bbox.y0,
		shade->bbox.x1, shade->bbox.y1);

	fprintf(out, "\tcolorspace %s\n", shade->colorspace->name);

	fprintf(out, "\tmatrix [%g %g %g %g %g %g]\n",
			shade->matrix.a, shade->matrix.b, shade->matrix.c,
			shade->matrix.d, shade->matrix.e, shade->matrix.f);

	if (shade->use_background)
	{
		fprintf(out, "\tbackground [");
		for (i = 0; i < shade->colorspace->n; i++)
			fprintf(out, "%s%g", i == 0 ? "" : " ", shade->background[i]);
		fprintf(out, "]\n");
	}

	if (shade->use_function)
	{
		fprintf(out, "\tfunction\n");
		n = 3;
	}
	else
		n = 2 + shade->colorspace->n;

	fprintf(out, "\tvertices: %d\n", shade->mesh_len);

	vertex = shade->mesh;
	triangle = 0;
	i = 0;
	while (i < shade->mesh_len)
	{
		fprintf(out, "\t%d:(%g, %g): ", triangle, vertex[0], vertex[1]);

		for (j = 2; j < n; j++)
			fprintf(out, "%s%g", j == 2 ? "" : " ", vertex[j]);
		fprintf(out, "\n");

		vertex += n;
		i++;
		if (i % 3 == 0)
			triangle++;
	}

	fprintf(out, "}\n");
}
