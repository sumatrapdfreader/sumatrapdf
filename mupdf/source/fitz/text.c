#include "mupdf/fitz.h"

fz_text *
fz_new_text(fz_context *ctx, fz_font *font, const fz_matrix *trm, int wmode)
{
	fz_text *text;

	text = fz_malloc_struct(ctx, fz_text);
	text->font = fz_keep_font(ctx, font);
	text->trm = *trm;
	text->wmode = wmode;
	text->len = 0;
	text->cap = 0;
	text->items = NULL;

	return text;
}

void
fz_free_text(fz_context *ctx, fz_text *text)
{
	if (text != NULL)
	{
		fz_drop_font(ctx, text->font);
		fz_free(ctx, text->items);
	}
	fz_free(ctx, text);
}

fz_text *
fz_clone_text(fz_context *ctx, fz_text *old)
{
	fz_text *text;

	text = fz_malloc_struct(ctx, fz_text);
	text->len = old->len;
	fz_try(ctx)
	{
		text->items = fz_malloc_array(ctx, text->len, sizeof(fz_text_item));
	}
	fz_catch(ctx)
	{
		fz_free(ctx, text);
		fz_rethrow(ctx);
	}
	memcpy(text->items, old->items, text->len * sizeof(fz_text_item));
	text->font = fz_keep_font(ctx, old->font);
	text->trm = old->trm;
	text->wmode = old->wmode;
	text->cap = text->len;

	return text;
}

fz_rect *
fz_bound_text(fz_context *ctx, fz_text *text, const fz_stroke_state *stroke, const fz_matrix *ctm, fz_rect *bbox)
{
	fz_matrix tm, trm;
	fz_rect gbox;
	int i;

	if (text->len == 0)
	{
		*bbox = fz_empty_rect;
		return bbox;
	}

	// TODO: stroke state

	tm = text->trm;

	tm.e = text->items[0].x;
	tm.f = text->items[0].y;
	fz_concat(&trm, &tm, ctm);
	fz_bound_glyph(ctx, text->font, text->items[0].gid, &trm, bbox);

	for (i = 1; i < text->len; i++)
	{
		if (text->items[i].gid >= 0)
		{
			tm.e = text->items[i].x;
			tm.f = text->items[i].y;
			fz_concat(&trm, &tm, ctm);
			fz_bound_glyph(ctx, text->font, text->items[i].gid, &trm, &gbox);

			bbox->x0 = fz_min(bbox->x0, gbox.x0);
			bbox->y0 = fz_min(bbox->y0, gbox.y0);
			bbox->x1 = fz_max(bbox->x1, gbox.x1);
			bbox->y1 = fz_max(bbox->y1, gbox.y1);
		}
	}

	if (stroke)
		fz_adjust_rect_for_stroke(bbox, stroke, ctm);

	/* Compensate for the glyph cache limited positioning precision */
	bbox->x0 -= 1;
	bbox->y0 -= 1;
	bbox->x1 += 1;
	bbox->y1 += 1;

	return bbox;
}

static void
fz_grow_text(fz_context *ctx, fz_text *text, int n)
{
	int new_cap = text->cap;
	if (text->len + n < new_cap)
		return;
	while (text->len + n > new_cap)
		new_cap = new_cap + 36;
	text->items = fz_resize_array(ctx, text->items, new_cap, sizeof(fz_text_item));
	text->cap = new_cap;
}

void
fz_add_text(fz_context *ctx, fz_text *text, int gid, int ucs, float x, float y)
{
	fz_grow_text(ctx, text, 1);
	text->items[text->len].ucs = ucs;
	text->items[text->len].gid = gid;
	text->items[text->len].x = x;
	text->items[text->len].y = y;
	text->len++;
}

static int
isxmlmeta(int c)
{
	return c < 32 || c >= 128 || c == '&' || c == '<' || c == '>' || c == '\'' || c == '"';
}

static void
do_print_text(FILE *out, fz_text *text, int indent)
{
	int i, n;
	for (i = 0; i < text->len; i++)
	{
		for (n = 0; n < indent; n++)
			fputc(' ', out);
		if (!isxmlmeta(text->items[i].ucs))
			fprintf(out, "<g ucs=\"%c\" gid=\"%d\" x=\"%g\" y=\"%g\" />\n",
				text->items[i].ucs, text->items[i].gid, text->items[i].x, text->items[i].y);
		else
			fprintf(out, "<g ucs=\"U+%04X\" gid=\"%d\" x=\"%g\" y=\"%g\" />\n",
				text->items[i].ucs, text->items[i].gid, text->items[i].x, text->items[i].y);
	}
}

void fz_print_text(fz_context *ctx, FILE *out, fz_text *text)
{
	do_print_text(out, text, 0);
}
