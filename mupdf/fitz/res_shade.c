#include "fitz.h"

fz_shade *
fz_keep_shade(fz_shade *shade)
{
	shade->refs ++;
	return shade;
}

void
fz_drop_shade(fz_shade *shade)
{
	if (shade && --shade->refs == 0)
	{
		if (shade->colorspace)
			fz_drop_colorspace(shade->colorspace);
		fz_free(shade->mesh);
		fz_free(shade);
	}
}

fz_rect
fz_bound_shade(fz_shade *shade, fz_matrix ctm)
{
	float *v;
	fz_rect r;
	fz_point p;
	int i, ncomp, nvert;

	ctm = fz_concat(shade->matrix, ctm);
	ncomp = shade->use_function ? 3 : 2 + shade->colorspace->n;
	nvert = shade->mesh_len / ncomp;
	v = shade->mesh;

	if (shade->type == FZ_LINEAR)
		return fz_infinite_rect;
	if (shade->type == FZ_RADIAL)
		return fz_infinite_rect;

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

	return r;
}

void
fz_debug_shade(fz_shade *shade)
{
	int i, j, n;
	float *vertex;
	int triangle;

	printf("shading {\n");

	switch (shade->type)
	{
	case FZ_LINEAR: printf("\ttype linear\n"); break;
	case FZ_RADIAL: printf("\ttype radial\n"); break;
	case FZ_MESH: printf("\ttype mesh\n"); break;
	}

	printf("\tbbox [%g %g %g %g]\n",
		shade->bbox.x0, shade->bbox.y0,
		shade->bbox.x1, shade->bbox.y1);

	printf("\tcolorspace %s\n", shade->colorspace->name);

	printf("\tmatrix [%g %g %g %g %g %g]\n",
			shade->matrix.a, shade->matrix.b, shade->matrix.c,
			shade->matrix.d, shade->matrix.e, shade->matrix.f);

	if (shade->use_background)
	{
		printf("\tbackground [");
		for (i = 0; i < shade->colorspace->n; i++)
			printf("%s%g", i == 0 ? "" : " ", shade->background[i]);
		printf("]\n");
	}

	if (shade->use_function)
	{
		printf("\tfunction\n");
		n = 3;
	}
	else
		n = 2 + shade->colorspace->n;

	printf("\tvertices: %d\n", shade->mesh_len);

	vertex = shade->mesh;
	triangle = 0;
	i = 0;
	while (i < shade->mesh_len)
	{
		printf("\t%d:(%g, %g): ", triangle, vertex[0], vertex[1]);

		for (j = 2; j < n; j++)
			printf("%s%g", j == 2 ? "" : " ", vertex[j]);
		printf("\n");

		vertex += n;
		i++;
		if (i % 3 == 0)
			triangle++;
	}

	printf("}\n");
}
