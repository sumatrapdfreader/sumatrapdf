#include "fitz-internal.h"

void
fz_free_outline(fz_context *ctx, fz_outline *outline)
{
	while (outline)
	{
		fz_outline *next = outline->next;
		fz_free_outline(ctx, outline->down);
		fz_free(ctx, outline->title);
		fz_free_link_dest(ctx, &outline->dest);
		fz_free(ctx, outline);
		outline = next;
	}
}

static void
do_debug_outline_xml(FILE *out, fz_outline *outline, int level)
{
	while (outline)
	{
		fprintf(out, "<outline title=\"%s\" page=\"%d\"", outline->title, outline->dest.kind == FZ_LINK_GOTO ? outline->dest.ld.gotor.page + 1 : 0);
		if (outline->down)
		{
			fprintf(out, ">\n");
			do_debug_outline_xml(out, outline->down, level + 1);
			fprintf(out, "</outline>\n");
		}
		else
		{
			fprintf(out, " />\n");
		}
		outline = outline->next;
	}
}

void
fz_print_outline_xml(fz_context *ctx, FILE *out, fz_outline *outline)
{
	do_debug_outline_xml(out, outline, 0);
}

static void
do_debug_outline(FILE *out, fz_outline *outline, int level)
{
	int i;
	while (outline)
	{
		for (i = 0; i < level; i++)
			fputc('\t', out);
		fprintf(out, "%s\t%d\n", outline->title, outline->dest.kind == FZ_LINK_GOTO ? outline->dest.ld.gotor.page + 1 : 0);
		if (outline->down)
			do_debug_outline(out, outline->down, level + 1);
		outline = outline->next;
	}
}

void
fz_print_outline(fz_context *ctx, FILE *out, fz_outline *outline)
{
	do_debug_outline(out, outline, 0);
}
