#include "muxps-internal.h"

#define REL_START_PART \
	"http://schemas.microsoft.com/xps/2005/06/fixedrepresentation"
#define REL_DOC_STRUCTURE \
	"http://schemas.microsoft.com/xps/2005/06/documentstructure"
#define REL_REQUIRED_RESOURCE \
	"http://schemas.microsoft.com/xps/2005/06/required-resource"
#define REL_REQUIRED_RESOURCE_RECURSIVE \
	"http://schemas.microsoft.com/xps/2005/06/required-resource#recursive"

static void
xps_rels_for_part(char *buf, char *name, int buflen)
{
	char *p, *basename;
	p = strrchr(name, '/');
	basename = p ? p + 1 : name;
	fz_strlcpy(buf, name, buflen);
	p = strrchr(buf, '/');
	if (p) *p = 0;
	fz_strlcat(buf, "/_rels/", buflen);
	fz_strlcat(buf, basename, buflen);
	fz_strlcat(buf, ".rels", buflen);
}

/*
 * The FixedDocumentSequence and FixedDocument parts determine
 * which parts correspond to actual pages, and the page order.
 */

void
xps_print_page_list(xps_document *doc)
{
	xps_fixdoc *fixdoc = doc->first_fixdoc;
	xps_page *page = doc->first_page;

	if (doc->start_part)
		printf("start part %s\n", doc->start_part);

	while (fixdoc)
	{
		printf("fixdoc %s\n", fixdoc->name);
		fixdoc = fixdoc->next;
	}

	while (page)
	{
		printf("page[%d] %s w=%d h=%d\n", page->number, page->name, page->width, page->height);
		page = page->next;
	}
}

static void
xps_add_fixed_document(xps_document *doc, char *name)
{
	xps_fixdoc *fixdoc;

	/* Check for duplicates first */
	for (fixdoc = doc->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
		if (!strcmp(fixdoc->name, name))
			return;

	fixdoc = fz_malloc_struct(doc->ctx, xps_fixdoc);
	fixdoc->name = fz_strdup(doc->ctx, name);
	fixdoc->outline = NULL;
	fixdoc->next = NULL;

	if (!doc->first_fixdoc)
	{
		doc->first_fixdoc = fixdoc;
		doc->last_fixdoc = fixdoc;
	}
	else
	{
		doc->last_fixdoc->next = fixdoc;
		doc->last_fixdoc = fixdoc;
	}
}

void
xps_add_link(xps_document *doc, fz_rect area, char *base_uri, char *target_uri)
{
	int len;
	char *buffer = NULL;
	char *uri;
	xps_target *target;
	fz_link_dest dest;
	fz_link *link;
	fz_context *ctx = doc->ctx;

	fz_var(buffer);

	if (doc->current_page == NULL || doc->current_page->links_resolved)
		return;

	fz_try(ctx)
	{
		len = 2 + (base_uri ? strlen(base_uri) : 0) +
			(target_uri ? strlen(target_uri) : 0);
		buffer = fz_malloc(doc->ctx, len);
		xps_resolve_url(buffer, base_uri, target_uri, len);
		if (xps_url_is_remote(buffer))
		{
			dest.kind = FZ_LINK_URI;
			dest.ld.uri.is_map = 0;
			dest.ld.uri.uri = buffer;
			buffer = NULL;
		}
		else
		{
			uri = buffer;

			/* FIXME: This won't work for remote docs */
			/* Skip until we find the fragment marker */
			while (*uri && *uri != '#')
				uri++;
			if (*uri == '#')
				uri++;

			for (target = doc->target; target; target = target->next)
				if (!strcmp(target->name, uri))
					break;

			if (target == NULL)
				break;

			dest.kind = FZ_LINK_GOTO;
			dest.ld.gotor.flags = 0;
			dest.ld.gotor.lt.x = 0;
			dest.ld.gotor.lt.y = 0;
			dest.ld.gotor.rb.x = 0;
			dest.ld.gotor.rb.y = 0;
			dest.ld.gotor.page = target->page;
			dest.ld.gotor.file_spec = NULL;
			dest.ld.gotor.new_window = 0;
		}

		link = fz_new_link(doc->ctx, area, dest);
		link->next = doc->current_page->links;
		doc->current_page->links = link;
	}
	fz_always(ctx)
	{
		fz_free(doc->ctx, buffer);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

fz_link *
xps_load_links(xps_document *doc, xps_page *page)
{
	if (!page->links_resolved)
		fz_warn(doc->ctx, "xps_load_links before page has been executed!");
	return fz_keep_link(doc->ctx, page->links);
}

static void
xps_add_fixed_page(xps_document *doc, char *name, int width, int height)
{
	xps_page *page;

	/* Check for duplicates first */
	for (page = doc->first_page; page; page = page->next)
		if (!strcmp(page->name, name))
			return;

	page = fz_malloc_struct(doc->ctx, xps_page);
	page->name = fz_strdup(doc->ctx, name);
	page->number = doc->page_count++;
	page->width = width;
	page->height = height;
	page->links = NULL;
	page->links_resolved = 0;
	page->root = NULL;
	page->next = NULL;

	if (!doc->first_page)
	{
		doc->first_page = page;
		doc->last_page = page;
	}
	else
	{
		doc->last_page->next = page;
		doc->last_page = page;
	}
}

static void
xps_add_link_target(xps_document *doc, char *name)
{
	xps_page *page = doc->last_page;
	xps_target *target = fz_malloc_struct(doc->ctx, xps_target);
	target->name = fz_strdup(doc->ctx, name);
	target->page = page->number;
	target->rect = fz_empty_rect; /* SumatraPDF: extended link support */
	target->next = doc->target;
	doc->target = target;
}

/* SumatraPDF: extended link support */
xps_target *
xps_lookup_link_target_obj(xps_document *doc, char *target_uri)
{
	xps_target *target;
	char *needle = strrchr(target_uri, '#');
	if (!needle)
		return NULL;
	for (target = doc->target; target; target = target->next)
		if (!strcmp(target->name, needle + 1))
			return target;
	return NULL;
}

int
xps_lookup_link_target(xps_document *doc, char *target_uri)
{
	xps_target *target = xps_lookup_link_target_obj(doc, target_uri);
	return target ? target->page : -1;
}

static void
xps_free_link_targets(xps_document *doc)
{
	xps_target *target = doc->target, *next;
	while (target)
	{
		next = target->next;
		fz_free(doc->ctx, target->name);
		fz_free(doc->ctx, target);
		target = next;
	}
}

static void
xps_free_fixed_pages(xps_document *doc)
{
	xps_page *page = doc->first_page;
	while (page)
	{
		xps_page *next = page->next;
		xps_free_page(doc, page);
		fz_drop_link(doc->ctx, page->links);
		fz_free(doc->ctx, page->name);
		fz_free(doc->ctx, page);
		page = next;
	}
	doc->first_page = NULL;
	doc->last_page = NULL;
}

static void
xps_free_fixed_documents(xps_document *doc)
{
	xps_fixdoc *fixdoc = doc->first_fixdoc;
	while (fixdoc)
	{
		xps_fixdoc *next = fixdoc->next;
		fz_free(doc->ctx, fixdoc->name);
		fz_free(doc->ctx, fixdoc->outline);
		fz_free(doc->ctx, fixdoc);
		fixdoc = next;
	}
	doc->first_fixdoc = NULL;
	doc->last_fixdoc = NULL;
}

void
xps_free_page_list(xps_document *doc)
{
	xps_free_fixed_documents(doc);
	xps_free_fixed_pages(doc);
	xps_free_link_targets(doc);
}

/*
 * Parse the fixed document sequence structure and _rels/.rels to find the start part.
 */

static void
xps_parse_metadata_imp(xps_document *doc, xml_element *item, xps_fixdoc *fixdoc)
{
	while (item)
	{
		if (!strcmp(xml_tag(item), "Relationship"))
		{
			char *target = xml_att(item, "Target");
			char *type = xml_att(item, "Type");
			if (target && type)
			{
				char tgtbuf[1024];
				xps_resolve_url(tgtbuf, doc->base_uri, target, sizeof tgtbuf);
				if (!strcmp(type, REL_START_PART))
					doc->start_part = fz_strdup(doc->ctx, tgtbuf);
				if (!strcmp(type, REL_DOC_STRUCTURE) && fixdoc)
					fixdoc->outline = fz_strdup(doc->ctx, tgtbuf);
				if (!xml_att(item, "Id"))
					fz_warn(doc->ctx, "missing relationship id for %s", target);
			}
		}

		if (!strcmp(xml_tag(item), "DocumentReference"))
		{
			char *source = xml_att(item, "Source");
			if (source)
			{
				char srcbuf[1024];
				xps_resolve_url(srcbuf, doc->base_uri, source, sizeof srcbuf);
				xps_add_fixed_document(doc, srcbuf);
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
				xps_resolve_url(srcbuf, doc->base_uri, source, sizeof srcbuf);
				xps_add_fixed_page(doc, srcbuf, width, height);
			}
		}

		if (!strcmp(xml_tag(item), "LinkTarget"))
		{
			char *name = xml_att(item, "Name");
			if (name)
				xps_add_link_target(doc, name);
		}

		xps_parse_metadata_imp(doc, xml_down(item), fixdoc);

		item = xml_next(item);
	}
}

static void
xps_parse_metadata(xps_document *doc, xps_part *part, xps_fixdoc *fixdoc)
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

	doc->base_uri = buf;
	doc->part_uri = part->name;

	root = xml_parse_document(doc->ctx, part->data, part->size);
	xps_parse_metadata_imp(doc, root, fixdoc);
	xml_free_element(doc->ctx, root);

	doc->base_uri = NULL;
	doc->part_uri = NULL;
}

static void
xps_read_and_process_metadata_part(xps_document *doc, char *name, xps_fixdoc *fixdoc)
{
	if (xps_has_part(doc, name))
	{
		xps_part *part = xps_read_part(doc, name);
		/* SumatraPDF: fix memory leak */
		fz_try(doc->ctx)
		{
		xps_parse_metadata(doc, part, fixdoc);
		}
		fz_always(doc->ctx)
		{
		xps_free_part(doc, part);
		}
		fz_catch(doc->ctx)
		{
			fz_rethrow(doc->ctx);
		}
	}
}

void
xps_read_page_list(xps_document *doc)
{
	xps_fixdoc *fixdoc;

	xps_read_and_process_metadata_part(doc, "/_rels/.rels", NULL);

	if (!doc->start_part)
		fz_throw(doc->ctx, "cannot find fixed document sequence start part");

	xps_read_and_process_metadata_part(doc, doc->start_part, NULL);

	for (fixdoc = doc->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
	{
		char relbuf[1024];
		fz_try(doc->ctx)
		{
			xps_rels_for_part(relbuf, fixdoc->name, sizeof relbuf);
			xps_read_and_process_metadata_part(doc, relbuf, fixdoc);
		}
		fz_catch(doc->ctx)
		{
			fz_warn(doc->ctx, "cannot process FixedDocument rels part");
		}
		xps_read_and_process_metadata_part(doc, fixdoc->name, fixdoc);
	}
}

int
xps_count_pages(xps_document *doc)
{
	return doc->page_count;
}

static void
xps_load_fixed_page(xps_document *doc, xps_page *page)
{
	xps_part *part;
	xml_element *root;
	char *width_att;
	char *height_att;

	part = xps_read_part(doc, page->name);
	/* SumatraPDF: fix memory leak */
	fz_var(part);
	fz_try(doc->ctx)
	{
	root = xml_parse_document(doc->ctx, part->data, part->size);
	}
	fz_catch(doc->ctx)
	{
		root = NULL;
	}
	xps_free_part(doc, part);
	if (!root)
		fz_throw(doc->ctx, "FixedPage missing root element");

	if (!strcmp(xml_tag(root), "mc:AlternateContent"))
	{
		xml_element *node = xps_lookup_alternate_content(root);
		if (!node)
		{
			xml_free_element(doc->ctx, root);
			fz_throw(doc->ctx, "FixedPage missing alternate root element");
		}
		xml_detach(node);
		xml_free_element(doc->ctx, root);
		root = node;
	}

	if (strcmp(xml_tag(root), "FixedPage"))
	{
		xml_free_element(doc->ctx, root);
		fz_throw(doc->ctx, "expected FixedPage element");
	}

	width_att = xml_att(root, "Width");
	if (!width_att)
	{
		xml_free_element(doc->ctx, root);
		fz_throw(doc->ctx, "FixedPage missing required attribute: Width");
	}

	height_att = xml_att(root, "Height");
	if (!height_att)
	{
		xml_free_element(doc->ctx, root);
		fz_throw(doc->ctx, "FixedPage missing required attribute: Height");
	}

	page->width = atoi(width_att);
	page->height = atoi(height_att);
	page->root = root;
}

xps_page *
xps_load_page(xps_document *doc, int number)
{
	xps_page *page;
	int n = 0;

	for (page = doc->first_page; page; page = page->next)
	{
		if (n == number)
		{
			doc->current_page = page;
			if (!page->root)
				xps_load_fixed_page(doc, page);
			return page;
		}
		n ++;
	}

	fz_throw(doc->ctx, "cannot find page %d", number + 1);
	return NULL;
}

fz_rect
xps_bound_page(xps_document *doc, xps_page *page)
{
	fz_rect bounds;
	bounds.x0 = bounds.y0 = 0;
	bounds.x1 = page->width * 72.0f / 96.0f;
	bounds.y1 = page->height * 72.0f / 96.0f;
	return bounds;
}

void
xps_free_page(xps_document *doc, xps_page *page)
{
	/* only free the XML contents */
	if (page->root)
		xml_free_element(doc->ctx, page->root);
	page->root = NULL;
}

/* SumatraPDF: extract page bounds without parsing the entire page content */
static xps_page *
xps_get_page(xps_document *doc, int number)
{
	int n;
	xps_page *page = doc->first_page;
	for (n = 0; n < number && page; n++)
		page = page->next;
	return page;
}

fz_rect
xps_bound_page_quick_and_dirty(xps_document *doc, int number)
{
	fz_rect bounds = fz_empty_rect;
	byte *end = NULL;
	xml_element *root;

	xps_page *page = xps_get_page(doc, number);
	xps_part *part = page ? xps_read_part(doc, page->name) : NULL;
	if (!part)
		return fz_empty_rect;

	if (part->data[0] == 0xFF && part->data[1] == 0xFE)
	{
		wchar_t *start = wcsstr((wchar_t *)part->data, L"<FixedPage");
		if (start)
			end = (byte *)wcschr(start, '>');
		if (end)
			end += 2;
	}
	else
	{
		const char *start = strstr((char *)part->data, "<FixedPage");
		if (start)
			end = (byte *)strchr(start, '>');
		if (end)
			// xml_parse_document ignores the length argument for UTF-8 data
			*(++end) = '\0';
	}
	if (!end)
	{
		xps_free_part(doc, part);
		return fz_empty_rect;
	}

	// we depend on the parser not validating its input (else we'd
	// have to append a closing "</FixedPage>" to the byte data)
	fz_try(doc->ctx)
	{
		root = xml_parse_document(doc->ctx, part->data, (int)(end - part->data));
	}
	fz_always(doc->ctx)
	{
		xps_free_part(doc, part);
	}
	fz_catch(doc->ctx)
	{
		fz_rethrow(doc->ctx);
	}

	if (!strcmp(xml_tag(root), "FixedPage"))
	{
		char *value = xml_att(root, "Width");
		if (value)
			bounds.x1 = fz_atof(value) * 72.0f / 96.0f;
		value = xml_att(root, "Height");
		if (value)
			bounds.y1 = fz_atof(value) * 72.0f / 96.0f;
	}
	xml_free_element(doc->ctx, root);

	return bounds;
}
