#include "fitz.h"

/* SumatraPDF: fix memory leak */
void
fz_free_link_dest(fz_context *ctx, fz_link_dest *dest)
{
	switch(dest->kind)
	{
	case FZ_LINK_NONE:
	case FZ_LINK_GOTO:
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
		break;
	}

	/* SumatraPDF: support extended link actions */
	if (dest->kind != FZ_LINK_NONE)
		fz_drop_obj(dest->extra);
}

fz_link *
fz_new_link(fz_context *ctx, fz_rect bbox, fz_link_dest dest)
{
	fz_link *link;

	fz_try(ctx)
	{
		link = fz_malloc_struct(ctx, fz_link);
	}
	fz_catch(ctx)
	{
		fz_free_link_dest(ctx, &dest);
		fz_rethrow(ctx);
	}
	link->dest = dest;
	link->rect = bbox;
	link->next = NULL;
	return link;
}

void
fz_free_link(fz_context *ctx, fz_link *link)
{
	fz_link *next;

	while (link)
	{
		next = link->next;
		fz_free_link_dest(ctx, &link->dest);
		fz_free(ctx, link);
		link = next;
	}
}
