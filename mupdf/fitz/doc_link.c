#include "fitz-internal.h"

void
fz_free_link_dest(fz_context *ctx, fz_link_dest *dest)
{
	switch(dest->kind)
	{
	case FZ_LINK_NONE:
		break;
	case FZ_LINK_GOTO:
		/* SumatraPDF: extended link support for MuXPS */
		fz_free(ctx, dest->ld.gotor.rname);
		break;
	case FZ_LINK_URI:
		fz_free(ctx, dest->ld.uri.uri);
		break;
	case FZ_LINK_LAUNCH:
		fz_free(ctx, dest->ld.launch.file_spec);
		break;
	case FZ_LINK_NAMED:
		fz_free(ctx, dest->ld.named.named);
		break;
	case FZ_LINK_GOTOR:
		fz_free(ctx, dest->ld.gotor.file_spec);
		/* SumatraPDF: allow to resolve against remote documents */
		fz_free(ctx, dest->ld.gotor.rname);
		break;
	}
}

fz_link *
fz_new_link(fz_context *ctx, const fz_rect *bbox, fz_link_dest dest)
{
	fz_link *link;

	fz_try(ctx)
	{
		link = fz_malloc_struct(ctx, fz_link);
		link->refs = 1;
	}
	fz_catch(ctx)
	{
		fz_free_link_dest(ctx, &dest);
		fz_rethrow(ctx);
	}
	link->dest = dest;
	link->rect = *bbox;
	link->next = NULL;
	return link;
}

fz_link *
fz_keep_link(fz_context *ctx, fz_link *link)
{
	if (link)
		link->refs++;
	return link;
}

void
fz_drop_link(fz_context *ctx, fz_link *link)
{
	while (link && --link->refs == 0)
	{
		fz_link *next = link->next;
		fz_free_link_dest(ctx, &link->dest);
		fz_free(ctx, link);
		link = next;
	}
}
