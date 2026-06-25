// Copyright (C) 2004-2026 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "xfa-imp.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
	PDF_XFA_SOM_DOT = 0,
	PDF_XFA_SOM_DOTDOT,
} pdf_xfa_som_op;

typedef struct
{
	char name[128];
	pdf_xfa_som_op op;
	int index; /* -1 = all (*), else 0-based */
} pdf_xfa_som_step;

#define PDF_XFA_SOM_MAX 64

static int
pdf_xfa_som_is_name_char(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') || c == '_' || c == '-';
}

static int
pdf_xfa_som_is_transparent(pdf_xfa_object *node)
{
	return !pdf_xfa_object_get_attr(NULL, node, "name");
}

static pdf_xfa_object *
pdf_xfa_som_shortcut(pdf_xfa_object *root, pdf_xfa_object *container, const char *name)
{
	pdf_xfa_object *datasets, *data;

	if (strcmp(name, "$data") == 0 || strcmp(name, "$record") == 0)
	{
		datasets = pdf_xfa_object_find_child(root, "datasets");
		data = datasets ? pdf_xfa_object_find_child(datasets, "data") : NULL;
		if (!data)
			return NULL;
		if (strcmp(name, "$record") == 0)
			return data->first_child ? data->first_child : data;
		return data;
	}
	if (strcmp(name, "$template") == 0)
		return pdf_xfa_object_find_child(root, "template");
	if (strcmp(name, "$") == 0)
		return container;
	if (strcmp(name, "$xfa") == 0 || strcmp(name, "xfa") == 0)
		return root;
	if (strcmp(name, "!") == 0)
		return pdf_xfa_object_find_child(root, "datasets");
	return NULL;
}

static void
pdf_xfa_som_collect_by_name(pdf_xfa_object *node, const char *name, int all_transparent,
		pdf_xfa_object **out, int *n, int max)
{
	pdf_xfa_object *child;
	char *named;

	if (!node || *n >= max)
		return;

	named = pdf_xfa_object_get_attr(NULL, node, "name");
	if ((node->name && strcmp(node->name, name) == 0) ||
		(named && strcmp(named, name) == 0))
		out[(*n)++] = node;

	for (child = node->first_child; child && *n < max; child = child->next_sibling)
	{
		named = pdf_xfa_object_get_attr(NULL, child, "name");
		if ((child->name && strcmp(child->name, name) == 0) ||
			(named && strcmp(named, name) == 0))
			out[(*n)++] = child;
		else if (all_transparent || pdf_xfa_som_is_transparent(child))
			pdf_xfa_som_collect_by_name(child, name, all_transparent, out, n, max);
	}
}

static int
pdf_xfa_som_parse(const char *expr, int dot_dot_allowed, pdf_xfa_som_step *steps, int max_steps)
{
	int n = 0;
	const char *p = expr;

	while (*p && n < max_steps)
	{
		const char *start;
		int len;

		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;

		if (*p == '[')
		{
			p++;
			if (n == 0)
				return 0;
			if (*p == '*')
			{
				steps[n-1].index = -1;
				p++;
			}
			else
			{
				steps[n-1].index = atoi(p);
				while (*p && *p != ']')
					p++;
			}
			if (*p == ']')
				p++;
			continue;
		}

		start = p;
		while (*p && pdf_xfa_som_is_name_char(*p))
			p++;
		len = (int)(p - start);
		if (len == 0)
			return 0;
		if (len >= (int)sizeof(steps[0].name))
			len = (int)sizeof(steps[0].name) - 1;
		memcpy(steps[n].name, start, (size_t)len);
		steps[n].name[len] = 0;
		steps[n].op = PDF_XFA_SOM_DOT;
		steps[n].index = 0;

		if (p[0] == '.' && p[1] == '.')
		{
			if (!dot_dot_allowed)
				return 0;
			steps[n].op = PDF_XFA_SOM_DOTDOT;
			p += 2;
		}
		else if (p[0] == '.')
			p++;

		n++;
	}

	return n;
}

int
pdf_xfa_som_search(fz_context *ctx, pdf_xfa_object *root, pdf_xfa_object *container,
		const char *expr, int dot_dot_allowed, pdf_xfa_object **out, int out_max)
{
	pdf_xfa_som_step steps[PDF_XFA_SOM_MAX];
	pdf_xfa_object *cands[PDF_XFA_SOM_MAX];
	pdf_xfa_object *matches[PDF_XFA_SOM_MAX];
	int nsteps, ncands, i, j, nmatches, nout = 0;
	pdf_xfa_object *shortcut;
	int start = 0;

	(void)ctx;
	if (!expr || !out || out_max <= 0)
		return 0;

	nsteps = pdf_xfa_som_parse(expr, dot_dot_allowed, steps, PDF_XFA_SOM_MAX);
	if (nsteps <= 0)
		return 0;

	ncands = 0;
	shortcut = pdf_xfa_som_shortcut(root, container, steps[0].name);
	if (shortcut)
	{
		cands[ncands++] = shortcut;
		start = 1;
	}
	else
	{
		if (container)
			cands[ncands++] = container;
		else if (root)
			cands[ncands++] = root;
	}

	for (i = start; i < nsteps; i++)
	{
		pdf_xfa_object *next[PDF_XFA_SOM_MAX];
		int nnext = 0;

		for (j = 0; j < ncands; j++)
		{
			nmatches = 0;
			pdf_xfa_som_collect_by_name(cands[j], steps[i].name,
				steps[i].op == PDF_XFA_SOM_DOTDOT, matches, &nmatches, PDF_XFA_SOM_MAX);

			if (steps[i].index < 0)
			{
				int k;
				for (k = 0; k < nmatches && nnext < PDF_XFA_SOM_MAX; k++)
					next[nnext++] = matches[k];
			}
			else if (steps[i].index < nmatches && nnext < PDF_XFA_SOM_MAX)
				next[nnext++] = matches[steps[i].index];
		}

		if (nnext == 0)
			return 0;
		ncands = nnext;
		memcpy(cands, next, sizeof(pdf_xfa_object *) * ncands);
	}

	for (i = 0; i < ncands && nout < out_max; i++)
		out[nout++] = cands[i];
	return nout;
}