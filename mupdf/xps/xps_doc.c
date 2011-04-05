#include "fitz.h"
#include "muxps.h"

/*
 * The FixedDocumentSequence and FixedDocument parts determine
 * which parts correspond to actual pages, and the page order.
 */

void
xps_debug_page_list(xps_context *ctx)
{
	xps_document *fixdoc = ctx->first_fixdoc;
	xps_page *page = ctx->first_page;

	if (ctx->start_part)
		printf("start part %s\n", ctx->start_part);

	while (fixdoc)
	{
		printf("fixdoc %s\n", fixdoc->name);
		fixdoc = fixdoc->next;
	}

	while (page)
	{
		printf("page %s w=%d h=%d\n", page->name, page->width, page->height);
		page = page->next;
	}
}

static void
xps_add_fixed_document(xps_context *ctx, char *name)
{
	xps_document *fixdoc;

	/* Check for duplicates first */
	for (fixdoc = ctx->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
		if (!strcmp(fixdoc->name, name))
			return;

	fixdoc = fz_malloc(sizeof(xps_document));
	fixdoc->name = fz_strdup(name);
	fixdoc->next = NULL;

	if (!ctx->first_fixdoc)
	{
		ctx->first_fixdoc = fixdoc;
		ctx->last_fixdoc = fixdoc;
	}
	else
	{
		ctx->last_fixdoc->next = fixdoc;
		ctx->last_fixdoc = fixdoc;
	}
}

static void
xps_add_fixed_page(xps_context *ctx, char *name, int width, int height)
{
	xps_page *page;

	/* Check for duplicates first */
	for (page = ctx->first_page; page; page = page->next)
		if (!strcmp(page->name, name))
			return;

	page = fz_malloc(sizeof(xps_page));
	page->name = fz_strdup(name);
	page->width = width;
	page->height = height;
	page->root = NULL;
	page->next = NULL;

	if (!ctx->first_page)
	{
		ctx->first_page = page;
		ctx->last_page = page;
	}
	else
	{
		ctx->last_page->next = page;
		ctx->last_page = page;
	}
}

static void
xps_free_fixed_pages(xps_context *ctx)
{
	xps_page *page = ctx->first_page;
	while (page)
	{
		xps_page *next = page->next;
		xps_free_page(ctx, page);
		fz_free(page->name);
		fz_free(page);
		page = next;
	}
	ctx->first_page = NULL;
	ctx->last_page = NULL;
}

static void
xps_free_fixed_documents(xps_context *ctx)
{
	xps_document *doc = ctx->first_fixdoc;
	while (doc)
	{
		xps_document *next = doc->next;
		fz_free(doc->name);
		fz_free(doc);
		doc = next;
	}
	ctx->first_fixdoc = NULL;
	ctx->last_fixdoc = NULL;
}

void
xps_free_page_list(xps_context *ctx)
{
	xps_free_fixed_documents(ctx);
	xps_free_fixed_pages(ctx);
}

/*
 * Parse the fixed document sequence structure and _rels/.rels to find the start part.
 */

static void
xps_parse_metadata_imp(xps_context *ctx, xml_element *item)
{
	while (item)
	{
		xps_parse_metadata_imp(ctx, xml_down(item));

		if (!strcmp(xml_tag(item), "Relationship"))
		{
			char *target = xml_att(item, "Target");
			char *type = xml_att(item, "Type");
			if (target && type)
			{
				char tgtbuf[1024];
				xps_absolute_path(tgtbuf, ctx->base_uri, target, sizeof tgtbuf);
				if (!strcmp(type, REL_START_PART))
					ctx->start_part = fz_strdup(tgtbuf);
			}
		}

		if (!strcmp(xml_tag(item), "DocumentReference"))
		{
			char *source = xml_att(item, "Source");
			if (source)
			{
				char srcbuf[1024];
				xps_absolute_path(srcbuf, ctx->base_uri, source, sizeof srcbuf);
				xps_add_fixed_document(ctx, srcbuf);
			}
		}

		if (!strcmp(xml_tag(item), "PageContent"))
		{
			char *source = xml_att(item, "Source");
			char *width_att = xml_att(item, "Width");
			char *height_att = xml_att(item, "Height");
			int width = width_att ? atoi(width_att) : 0;
			int height = height_att ? atoi(height_att) : 0;
			if (source)
			{
				char srcbuf[1024];
				xps_absolute_path(srcbuf, ctx->base_uri, source, sizeof srcbuf);
				xps_add_fixed_page(ctx, srcbuf, width, height);
			}
		}

		item = xml_next(item);
	}
}

static int
xps_parse_metadata(xps_context *ctx, xps_part *part)
{
	xml_element *root;
	char buf[1024];
	char *s;

	/* Save directory name part */
	fz_strlcpy(buf, part->name, sizeof buf);
	s = strrchr(buf, '/');
	if (s)
		s[0] = 0;

	/* _rels parts are voodoo: their URI references are from
	 * the part they are associated with, not the actual _rels
	 * part being parsed.
	 */
	s = strstr(buf, "/_rels");
	if (s)
		*s = 0;

	ctx->base_uri = buf;
	ctx->part_uri = part->name;

	root = xml_parse_document(part->data, part->size);
	if (!root)
		return fz_rethrow(-1, "cannot parse metadata part '%s'", part->name);

	xps_parse_metadata_imp(ctx, root);

	xml_free_element(root);

	ctx->base_uri = NULL;
	ctx->part_uri = NULL;

	return fz_okay;
}

static int
xps_read_and_process_metadata_part(xps_context *ctx, char *name)
{
	xps_part *part;
	int code;

	part = xps_read_part(ctx, name);
	if (!part)
		return fz_rethrow(-1, "cannot read zip part '%s'", name);

	code = xps_parse_metadata(ctx, part);
	if (code)
		return fz_rethrow(code, "cannot process metadata part '%s'", name);

	xps_free_part(ctx, part);

	return fz_okay;
}

int
xps_read_page_list(xps_context *ctx)
{
	xps_document *doc;
	int code;

	code = xps_read_and_process_metadata_part(ctx, "/_rels/.rels");
	if (code)
		return fz_rethrow(code, "cannot process root relationship part");

	if (!ctx->start_part)
		return fz_throw("cannot find fixed document sequence start part");

	code = xps_read_and_process_metadata_part(ctx, ctx->start_part);
	if (code)
		return fz_rethrow(code, "cannot process FixedDocumentSequence part");

	for (doc = ctx->first_fixdoc; doc; doc = doc->next)
	{
		code = xps_read_and_process_metadata_part(ctx, doc->name);
		if (code)
			return fz_rethrow(code, "cannot process FixedDocument part");
	}

	return fz_okay;
}

int
xps_count_pages(xps_context *ctx)
{
	xps_page *page;
	int n = 0;
	for (page = ctx->first_page; page; page = page->next)
		n ++;
	return n;
}

static int
xps_load_fixed_page(xps_context *ctx, xps_page *page)
{
	xps_part *part;
	xml_element *root;
	char *width_att;
	char *height_att;

	part = xps_read_part(ctx, page->name);
	if (!part)
		return fz_rethrow(-1, "cannot read zip part '%s'", page->name);

	root = xml_parse_document(part->data, part->size);
	if (!root)
		return fz_rethrow(-1, "cannot parse xml part '%s'", page->name);

	xps_free_part(ctx, part);

	if (strcmp(xml_tag(root), "FixedPage"))
		return fz_throw("expected FixedPage element (found %s)", xml_tag(root));

	width_att = xml_att(root, "Width");
	if (!width_att)
		return fz_throw("FixedPage missing required attribute: Width");

	height_att = xml_att(root, "Height");
	if (!height_att)
		return fz_throw("FixedPage missing required attribute: Height");

	page->width = atoi(width_att);
	page->height = atoi(height_att);
	page->root = root;

	return 0;
}

int
xps_load_page(xps_page **pagep, xps_context *ctx, int number)
{
	xps_page *page;
	int code;
	int n = 0;

	for (page = ctx->first_page; page; page = page->next)
	{
		if (n == number)
		{
			if (!page->root)
			{
				code = xps_load_fixed_page(ctx, page);
				if (code)
					return fz_rethrow(code, "cannot load page %d", number + 1);
			}
			*pagep = page;
			return fz_okay;
		}
		n ++;
	}

	return fz_throw("cannot find page %d", number + 1);
}

void
xps_free_page(xps_context *ctx, xps_page *page)
{
	/* only free the XML contents */
	if (page->root)
		xml_free_element(page->root);
	page->root = NULL;
}
