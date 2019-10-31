#ifndef MUPDF_FITZ_LINK_H
#define MUPDF_FITZ_LINK_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"

typedef struct fz_link_s fz_link;

/*
	fz_link is a list of interactive links on a page.

	There is no relation between the order of the links in the
	list and the order they appear on the page. The list of links
	for a given page can be obtained from fz_load_links.

	A link is reference counted. Dropping a reference to a link is
	done by calling fz_drop_link.

	rect: The hot zone. The area that can be clicked in
	untransformed coordinates.

	uri: Link destinations come in two forms: internal and external.
	Internal links refer to other pages in the same document.
	External links are URLs to other documents.

	next: A pointer to the next link on the same page.
*/
struct fz_link_s
{
	int refs;
	fz_link *next;
	fz_rect rect;
	void *doc;
	char *uri;
};

fz_link *fz_new_link(fz_context *ctx, fz_rect bbox, void *doc, const char *uri);
fz_link *fz_keep_link(fz_context *ctx, fz_link *link);

int fz_is_external_link(fz_context *ctx, const char *uri);

void fz_drop_link(fz_context *ctx, fz_link *link);

#endif
