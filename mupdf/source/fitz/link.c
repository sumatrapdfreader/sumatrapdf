#include "mupdf/fitz.h"
#include "fitz-imp.h"

fz_link *
fz_new_link(fz_context *ctx, fz_rect bbox, void *doc, const char *uri)
{
	fz_link *link;

	link = fz_malloc_struct(ctx, fz_link);
	link->refs = 1;
	link->rect = bbox;
	link->next = NULL;
	link->doc = doc; /* don't take reference */
	link->uri = NULL;

	fz_try(ctx)
		link->uri = fz_strdup(ctx, uri);
	fz_catch(ctx)
	{
		fz_drop_link(ctx, link);
		fz_rethrow(ctx);
	}

	return link;
}

fz_link *
fz_keep_link(fz_context *ctx, fz_link *link)
{
	return fz_keep_imp(ctx, link, &link->refs);
}

void
fz_drop_link(fz_context *ctx, fz_link *link)
{
	while (fz_drop_imp(ctx, link, &link->refs))
	{
		fz_link *next = link->next;
		fz_free(ctx, link->uri);
		fz_free(ctx, link);
		link = next;
	}
}

int
fz_is_external_link(fz_context *ctx, const char *uri)
{
	while (*uri >= 'a' && *uri <= 'z')
		++uri;
	return uri[0] == ':';
}
