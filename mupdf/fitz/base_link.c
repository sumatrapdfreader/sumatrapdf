#include "fitz.h"

static void
free_link_dest(fz_context *ctx, fz_link_kind kind, fz_link_dest *dest)
{
	switch(kind)
	{
	case FZ_LINK_GOTO:
		break;
	case FZ_LINK_URI:
		fz_free(ctx, dest->uri.uri);
		break;
	case FZ_LINK_LAUNCH:
		fz_free(ctx, dest->launch.file_spec);
		break;
	case FZ_LINK_NAMED:
		fz_free(ctx, dest->named.named);
		break;
	case FZ_LINK_GOTOR:
		fz_free(ctx, dest->gotor.file_spec);
		break;
	}
}

fz_link *
fz_new_link(fz_context *ctx, fz_link_kind kind, fz_rect bbox, fz_link_dest dest, fz_obj *extra)
{
	fz_link *link;

	fz_try(ctx)
	{
		link = fz_malloc_struct(ctx, fz_link);
	}
	fz_catch(ctx)
	{
		free_link_dest(ctx, kind, &dest);
		fz_rethrow(ctx);
	}
	link->kind = kind;
	link->rect = bbox;
	link->dest = dest;
	link->next = NULL;
	/* SumatraPDF: provide access to the link object */
	link->extra = extra ? fz_keep_obj(extra) : NULL;
	return link;
}

void
fz_free_link(fz_context *ctx, fz_link *link)
{
	fz_link *next;

	while (link)
	{
		next = link->next;
		free_link_dest(ctx, link->kind, &link->dest);
		/* SumatraPDF: provide access to the link object */
		fz_drop_obj(link->extra);
		fz_free(ctx, link);
		link = next;
	}
}
