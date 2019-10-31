#include "mupdf/fitz.h"
#include "fitz-imp.h"

fz_outline *
fz_new_outline(fz_context *ctx)
{
	fz_outline *outline = fz_malloc_struct(ctx, fz_outline);
	outline->refs = 1;
	return outline;
}

fz_outline *
fz_keep_outline(fz_context *ctx, fz_outline *outline)
{
	return fz_keep_imp(ctx, outline, &outline->refs);
}

void
fz_drop_outline(fz_context *ctx, fz_outline *outline)
{
	while (fz_drop_imp(ctx, outline, &outline->refs))
	{
		fz_outline *next = outline->next;
		fz_drop_outline(ctx, outline->down);
		fz_free(ctx, outline->title);
		fz_free(ctx, outline->uri);
		fz_free(ctx, outline);
		outline = next;
	}
}
