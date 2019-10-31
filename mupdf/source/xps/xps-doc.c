#include "mupdf/fitz.h"
#include "xps-imp.h"

#include <string.h>
#include <stdlib.h>

#define REL_START_PART \
	"http://schemas.microsoft.com/xps/2005/06/fixedrepresentation"
#define REL_DOC_STRUCTURE \
	"http://schemas.microsoft.com/xps/2005/06/documentstructure"
#define REL_REQUIRED_RESOURCE \
	"http://schemas.microsoft.com/xps/2005/06/required-resource"
#define REL_REQUIRED_RESOURCE_RECURSIVE \
	"http://schemas.microsoft.com/xps/2005/06/required-resource#recursive"

#define REL_START_PART_OXPS \
	"http://schemas.openxps.org/oxps/v1.0/fixedrepresentation"
#define REL_DOC_STRUCTURE_OXPS \
	"http://schemas.openxps.org/oxps/v1.0/documentstructure"

static void
xps_rels_for_part(fz_context *ctx, xps_document *doc, char *buf, char *name, int buflen)
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

static void
xps_add_fixed_document(fz_context *ctx, xps_document *doc, char *name)
{
	xps_fixdoc *fixdoc;

	/* Check for duplicates first */
	for (fixdoc = doc->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
		if (!strcmp(fixdoc->name, name))
			return;

	fixdoc = fz_malloc_struct(ctx, xps_fixdoc);
	fz_try(ctx)
	{
		fixdoc->name = fz_strdup(ctx, name);
		fixdoc->outline = NULL;
		fixdoc->next = NULL;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, fixdoc);
		fz_rethrow(ctx);
	}

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

static void
xps_add_fixed_page(fz_context *ctx, xps_document *doc, char *name, int width, int height)
{
	xps_fixpage *page;

	/* Check for duplicates first */
	for (page = doc->first_page; page; page = page->next)
		if (!strcmp(page->name, name))
			return;

	page = fz_malloc_struct(ctx, xps_fixpage);
	page->name = NULL;

	fz_try(ctx)
	{
		page->name = fz_strdup(ctx, name);
		page->number = doc->page_count++;
		page->width = width;
		page->height = height;
		page->next = NULL;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, page->name);
		fz_free(ctx, page);
		fz_rethrow(ctx);
	}

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
xps_add_link_target(fz_context *ctx, xps_document *doc, char *name)
{
	xps_fixpage *page = doc->last_page;
	xps_target *target = fz_malloc_struct(ctx, xps_target);

	fz_try(ctx)
	{
		target->name = fz_strdup(ctx, name);
		target->page = page->number;
		target->next = doc->target;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, target);
		fz_rethrow(ctx);
	}

	doc->target = target;
}

fz_location
xps_lookup_link_target(fz_context *ctx, fz_document *doc_, const char *target_uri, float *xp, float *yp)
{
	xps_document *doc = (xps_document*)doc_;
	xps_target *target;
	const char *needle = strrchr(target_uri, '#');
	needle = needle ? needle + 1 : target_uri;
	for (target = doc->target; target; target = target->next)
		if (!strcmp(target->name, needle))
			return fz_make_location(0, target->page);
	return fz_make_location(-1, -1);
}

static void
xps_drop_link_targets(fz_context *ctx, xps_document *doc)
{
	xps_target *target = doc->target, *next;
	while (target)
	{
		next = target->next;
		fz_free(ctx, target->name);
		fz_free(ctx, target);
		target = next;
	}
}

static void
xps_drop_fixed_pages(fz_context *ctx, xps_document *doc)
{
	xps_fixpage *page = doc->first_page;
	while (page)
	{
		xps_fixpage *next = page->next;
		fz_free(ctx, page->name);
		fz_free(ctx, page);
		page = next;
	}
	doc->first_page = NULL;
	doc->last_page = NULL;
}

static void
xps_drop_fixed_documents(fz_context *ctx, xps_document *doc)
{
	xps_fixdoc *fixdoc = doc->first_fixdoc;
	while (fixdoc)
	{
		xps_fixdoc *next = fixdoc->next;
		fz_free(ctx, fixdoc->name);
		fz_free(ctx, fixdoc->outline);
		fz_free(ctx, fixdoc);
		fixdoc = next;
	}
	doc->first_fixdoc = NULL;
	doc->last_fixdoc = NULL;
}

void
xps_drop_page_list(fz_context *ctx, xps_document *doc)
{
	xps_drop_fixed_documents(ctx, doc);
	xps_drop_fixed_pages(ctx, doc);
	xps_drop_link_targets(ctx, doc);
}

/*
 * Parse the fixed document sequence structure and _rels/.rels to find the start part.
 */

static void
xps_parse_metadata_imp(fz_context *ctx, xps_document *doc, fz_xml *item, xps_fixdoc *fixdoc)
{
	while (item)
	{
		if (fz_xml_is_tag(item, "Relationship"))
		{
			char *target = fz_xml_att(item, "Target");
			char *type = fz_xml_att(item, "Type");
			if (target && type)
			{
				char tgtbuf[1024];
				xps_resolve_url(ctx, doc, tgtbuf, doc->base_uri, target, sizeof tgtbuf);
				if (!strcmp(type, REL_START_PART) || !strcmp(type, REL_START_PART_OXPS))
				{
					fz_free(ctx, doc->start_part);
					doc->start_part = fz_strdup(ctx, tgtbuf);
				}
				if ((!strcmp(type, REL_DOC_STRUCTURE) || !strcmp(type, REL_DOC_STRUCTURE_OXPS)) && fixdoc)
					fixdoc->outline = fz_strdup(ctx, tgtbuf);
				if (!fz_xml_att(item, "Id"))
					fz_warn(ctx, "missing relationship id for %s", target);
			}
		}

		if (fz_xml_is_tag(item, "DocumentReference"))
		{
			char *source = fz_xml_att(item, "Source");
			if (source)
			{
				char srcbuf[1024];
				xps_resolve_url(ctx, doc, srcbuf, doc->base_uri, source, sizeof srcbuf);
				xps_add_fixed_document(ctx, doc, srcbuf);
			}
		}

		if (fz_xml_is_tag(item, "PageContent"))
		{
			char *source = fz_xml_att(item, "Source");
			char *width_att = fz_xml_att(item, "Width");
			char *height_att = fz_xml_att(item, "Height");
			int width = width_att ? atoi(width_att) : 0;
			int height = height_att ? atoi(height_att) : 0;
			if (source)
			{
				char srcbuf[1024];
				xps_resolve_url(ctx, doc, srcbuf, doc->base_uri, source, sizeof srcbuf);
				xps_add_fixed_page(ctx, doc, srcbuf, width, height);
			}
		}

		if (fz_xml_is_tag(item, "LinkTarget"))
		{
			char *name = fz_xml_att(item, "Name");
			if (name)
				xps_add_link_target(ctx, doc, name);
		}

		xps_parse_metadata_imp(ctx, doc, fz_xml_down(item), fixdoc);

		item = fz_xml_next(item);
	}
}

static void
xps_parse_metadata(fz_context *ctx, xps_document *doc, xps_part *part, xps_fixdoc *fixdoc)
{
	fz_xml_doc *xml;
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

	xml = fz_parse_xml(ctx, part->data, 0, 0);
	fz_try(ctx)
	{
		xps_parse_metadata_imp(ctx, doc, fz_xml_root(xml), fixdoc);
	}
	fz_always(ctx)
	{
		fz_drop_xml(ctx, xml);
		doc->base_uri = NULL;
		doc->part_uri = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
xps_read_and_process_metadata_part(fz_context *ctx, xps_document *doc, char *name, xps_fixdoc *fixdoc)
{
	xps_part *part;

	if (!xps_has_part(ctx, doc, name))
		return;

	part = xps_read_part(ctx, doc, name);
	fz_try(ctx)
	{
		xps_parse_metadata(ctx, doc, part, fixdoc);
	}
	fz_always(ctx)
	{
		xps_drop_part(ctx, doc, part);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
xps_read_page_list(fz_context *ctx, xps_document *doc)
{
	xps_fixdoc *fixdoc;

	xps_read_and_process_metadata_part(ctx, doc, "/_rels/.rels", NULL);

	if (!doc->start_part)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find fixed document sequence start part");

	xps_read_and_process_metadata_part(ctx, doc, doc->start_part, NULL);

	for (fixdoc = doc->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
	{
		char relbuf[1024];
		fz_try(ctx)
		{
			xps_rels_for_part(ctx, doc, relbuf, fixdoc->name, sizeof relbuf);
			xps_read_and_process_metadata_part(ctx, doc, relbuf, fixdoc);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_warn(ctx, "cannot process FixedDocument rels part");
		}
		xps_read_and_process_metadata_part(ctx, doc, fixdoc->name, fixdoc);
	}
}

int
xps_count_pages(fz_context *ctx, fz_document *doc_, int chapter)
{
	xps_document *doc = (xps_document*)doc_;
	return doc->page_count;
}

static fz_xml_doc *
xps_load_fixed_page(fz_context *ctx, xps_document *doc, xps_fixpage *page)
{
	xps_part *part;
	fz_xml_doc *xml = NULL;
	fz_xml *root;
	char *width_att;
	char *height_att;

	part = xps_read_part(ctx, doc, page->name);
	fz_try(ctx)
	{
		xml = fz_parse_xml(ctx, part->data, 0, 0);

		root = fz_xml_root(xml);
		if (!root)
			fz_throw(ctx, FZ_ERROR_GENERIC, "FixedPage missing root element");

		if (fz_xml_is_tag(root, "AlternateContent"))
		{
			fz_xml *node = xps_lookup_alternate_content(ctx, doc, root);
			if (!node)
				fz_throw(ctx, FZ_ERROR_GENERIC, "FixedPage missing alternate root element");
			fz_detach_xml(ctx, xml, node);
			root = node;
		}

		if (!fz_xml_is_tag(root, "FixedPage"))
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected FixedPage element");
		width_att = fz_xml_att(root, "Width");
		if (!width_att)
			fz_throw(ctx, FZ_ERROR_GENERIC, "FixedPage missing required attribute: Width");
		height_att = fz_xml_att(root, "Height");
		if (!height_att)
			fz_throw(ctx, FZ_ERROR_GENERIC, "FixedPage missing required attribute: Height");

		page->width = atoi(width_att);
		page->height = atoi(height_att);
	}
	fz_always(ctx)
	{
		xps_drop_part(ctx, doc, part);
	}
	fz_catch(ctx)
	{
		fz_drop_xml(ctx, xml);
		fz_rethrow(ctx);
	}

	return xml;
}

static fz_rect
xps_bound_page(fz_context *ctx, fz_page *page_)
{
	xps_page *page = (xps_page*)page_;
	fz_rect bounds;
	bounds.x0 = bounds.y0 = 0;
	bounds.x1 = page->fix->width * 72.0f / 96.0f;
	bounds.y1 = page->fix->height * 72.0f / 96.0f;
	return bounds;
}

static void
xps_drop_page_imp(fz_context *ctx, fz_page *page_)
{
	xps_page *page = (xps_page*)page_;
	fz_drop_document(ctx, &page->doc->super);
	fz_drop_xml(ctx, page->xml);
}

fz_page *
xps_load_page(fz_context *ctx, fz_document *doc_, int chapter, int number)
{
	xps_document *doc = (xps_document*)doc_;
	xps_page *page = NULL;
	xps_fixpage *fix;
	fz_xml_doc *xml;
	int n = 0;

	fz_var(page);

	for (fix = doc->first_page; fix; fix = fix->next)
	{
		if (n == number)
		{
			xml = xps_load_fixed_page(ctx, doc, fix);
			fz_try(ctx)
			{
				page = fz_new_derived_page(ctx, xps_page);
				page->super.load_links = xps_load_links;
				page->super.bound_page = xps_bound_page;
				page->super.run_page_contents = xps_run_page;
				page->super.drop_page = xps_drop_page_imp;

				page->doc = (xps_document*) fz_keep_document(ctx, (fz_document*)doc);
				page->fix = fix;
				page->xml = xml;
			}
			fz_catch(ctx)
			{
				fz_drop_xml(ctx, xml);
				fz_rethrow(ctx);
			}
			return (fz_page*)page;
		}
		n ++;
	}

	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find page %d", number + 1);
}

static int
xps_recognize(fz_context *ctx, const char *magic)
{
	if (strstr(magic, "/_rels/.rels") || strstr(magic, "\\_rels\\.rels"))
		return 100;
	return 0;
}

static const char *xps_extensions[] =
{
	"oxps",
	"xps",
	NULL
};

static const char *xps_mimetypes[] =
{
	"application/oxps",
	"application/vnd.ms-xpsdocument",
	"application/xps",
	NULL
};

fz_document_handler xps_document_handler =
{
	xps_recognize,
	xps_open_document,
	xps_open_document_with_stream,
	xps_extensions,
	xps_mimetypes,
	NULL,
	NULL
};
