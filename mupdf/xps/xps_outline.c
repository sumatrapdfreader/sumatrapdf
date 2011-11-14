#include "fitz.h"
#include "muxps.h"

/*
 * Parse the document structure / outline parts referenced from fixdoc relationships.
 */

static fz_outline *
xps_find_last_outline_at_level(fz_outline *node, int level, int target_level)
{
	while (node->next)
		node = node->next;
	if (level == target_level || !node->down)
		return node;
	return xps_find_last_outline_at_level(node->down, level + 1, target_level);
}

/* SumatraPDF: extended outline actions */
static char *
xps_to_absolute_path(char *base, char *path)
{
	char tgtbuf[1024];
	char *base_parent = fz_strdup(base);
	char *last_part = strrchr(base_parent, '/');
	if (!last_part)
		last_part = base_parent;
	*last_part = '\0';
	xps_absolute_path(tgtbuf, base_parent, path, sizeof(tgtbuf));
	fz_free(base_parent);
	return fz_strdup(tgtbuf);
}

static fz_outline *
xps_parse_document_outline(xps_context *ctx, xml_element *root, xps_document *fixdoc)
{
	xml_element *node;
	fz_outline *head = NULL, *entry, *tail;
	int last_level = 1, this_level;
	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "OutlineEntry"))
		{
			char *level = xml_att(node, "OutlineLevel");
			char *target = xml_att(node, "OutlineTarget");
			char *description = xml_att(node, "Description");
			if (!target || !description)
				continue;

			entry = fz_malloc(sizeof *entry);
			entry->title = fz_strdup(description);
			/* SumatraPDF: extended outline actions */
			entry->data = xps_to_absolute_path(fixdoc->outline, target);
			entry->free_data = fz_free;
			entry->page = xps_find_link_target(ctx, entry->data);
			entry->down = NULL;
			entry->next = NULL;

			this_level = level ? atoi(level) : 1;
			entry->is_open = this_level == 1; /* SumatraPDF: support expansion states */

			if (!head)
			{
				head = entry;
			}
			else
			{
				tail = xps_find_last_outline_at_level(head, 1, this_level);
				if (this_level > last_level)
					tail->down = entry;
				else
					tail->next = entry;
			}

			last_level = this_level;
		}
	}
	return head;
}

static fz_outline *
xps_parse_document_structure(xps_context *ctx, xml_element *root, xps_document *fixdoc)
{
	xml_element *node;
	if (!strcmp(xml_tag(root), "DocumentStructure"))
	{
		node = xml_down(root);
		if (!strcmp(xml_tag(node), "DocumentStructure.Outline"))
		{
			node = xml_down(node);
			if (!strcmp(xml_tag(node), "DocumentOutline"))
				return xps_parse_document_outline(ctx, node, fixdoc);
		}
	}
	return NULL;
}

static fz_outline *
xps_load_document_structure(xps_context *ctx, xps_document *fixdoc)
{
	xps_part *part;
	xml_element *root;
	fz_outline *outline;

	part = xps_read_part(ctx, fixdoc->outline);
	if (!part)
		return NULL;

	root = xml_parse_document(part->data, part->size);
	if (!root) {
		fz_catch(-1, "cannot parse document structure part '%s'", part->name);
		xps_free_part(ctx, part);
		return NULL;
	}

	outline = xps_parse_document_structure(ctx, root, fixdoc);

	xml_free_element(root);
	xps_free_part(ctx, part);

	return outline;

}

fz_outline *
xps_load_outline(xps_context *ctx)
{
	xps_document *fixdoc;
	fz_outline *head = NULL, *tail, *outline;

	for (fixdoc = ctx->first_fixdoc; fixdoc; fixdoc = fixdoc->next) {
		if (fixdoc->outline) {
			outline = xps_load_document_structure(ctx, fixdoc);
			if (outline) {
				if (!head)
					head = outline;
				else
					tail->next = outline;
				tail = outline;
			}
		}
	}
	return head;
}

/* SumatraPDF: extended link support */

static xps_anchor *
xps_new_anchor(char *target, fz_rect rect, int is_dest)
{
	xps_anchor *link = fz_malloc(sizeof(xps_anchor));

	link->target = fz_strdup(target);
	link->rect = rect;
	link->is_dest = is_dest;
	link->next = NULL;

	return link;
}

void
xps_free_anchor(xps_anchor *link)
{
	while (link)
	{
		xps_anchor *next = link->next;
		fz_free(link->target);
		fz_free(link);
		link = next;
	}
}

void
xps_extract_anchor_info(xps_context *ctx, xml_element *node, fz_rect rect, char *base_uri)
{
	xps_anchor *link = NULL;
	char *value;

	if (!ctx->link_root)
		return;

	if ((value = xml_att(node, "FixedPage.NavigateUri")) && !strchr(value, ':'))
	{
		char tgtbuf[1024];
		xps_absolute_path(tgtbuf, base_uri, value, sizeof(tgtbuf));
		link = xps_new_anchor(tgtbuf, rect, 0);
	}
	else if (value) // link with a protocol (e.g. http://...)
		link = xps_new_anchor(value, rect, 0);
	else if ((value = xml_att(node, "Name")))
		link = xps_new_anchor(value, rect, 1);

	// insert the links in top-to-bottom order (first one is to be preferred)
	if (link)
	{
		link->next = ctx->link_root->next;
		ctx->link_root->next = link;
	}
}

/* SumatraPDF: extract document properties (hacky) */

static int
xps_open_and_parse(xps_context *ctx, char *path, xml_element **rootp)
{
	xps_part *part = xps_read_part(ctx, path);
	if (!part)
		return fz_rethrow(-1, "cannot read zip part '%s'", path);

	*rootp = xml_parse_document(part->data, part->size);
	xps_free_part(ctx, part);

	if (!*rootp)
		return fz_rethrow(-1, "cannot parse metadata for part '%s'", path);
	return fz_okay;
}

static inline int iswhite(c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// <cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:dcmitype="http://purl.org/dc/dcmitype/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><dc:title>XML Paper Specification</dc:title><dc:subject>XPS Specification and Reference Guide</dc:subject><dc:creator>Jesse McGatha</dc:creator><dcterms:created xsi:type="dcterms:W3CDTF">2006-10-19T01:21:08Z</dcterms:created><dcterms:modified xsi:type="dcterms:W3CDTF">2006-10-19T01:21:08Z</dcterms:modified></cp:coreProperties>

static void
xps_hacky_get_prop(char *data, fz_obj *dict, char *name, char *tag_name)
{
	char *start, *end;
	fz_obj *value;

	start = strstr(data, tag_name);
	if (!start || start == data || start[-1] != '<')
		return;
	end = strstr(start + 1, tag_name);
	start = strchr(start, '>');
	if (!start || !end || start >= end || end[-2] != '<' || end[-1] != '/')
		return;

	for (start++; iswhite(*start); start++);
	for (end -= 3; iswhite(*end) && end > start; end--);

	value = fz_new_string(start, end - start + 1);
	fz_dict_puts(dict, name, value);
	fz_drop_obj(value);
}

#define CONTENT_TYPE_CORE_PROPS \
	"application/vnd.openxmlformats-package.core-properties+xml"

static int
xps_find_doc_props_path(xps_context *ctx, char path[1024])
{
	xml_element *root;

	int code = xps_open_and_parse(ctx, "/[Content_Types].xml", &root);
	if (code != fz_okay)
		return code;

	*path = '\0';
	if (root && !strcmp(xml_tag(root), "Types"))
	{
		xml_element *item;
		for (item = xml_down(root); item; item = xml_next(item))
		{
			if (!strcmp(xml_tag(item), "Override") && xml_att(item, "ContentType") &&
				!strcmp(xml_att(item, "ContentType"), CONTENT_TYPE_CORE_PROPS) &&
				xml_att(item, "PartName"))
			{
				fz_strlcpy(path, xml_att(item, "PartName"), 1024);
			}
		}
	}
	else
		code = fz_throw("couldn't parse part '[Content_Types].xml'");

	xml_free_element(root);

	return code;
}

fz_obj *xps_extract_doc_props(xps_context *ctx)
{
	char path[1024];
	xps_part *part;
	fz_obj *dict;

	if (xps_find_doc_props_path(ctx, path) != fz_okay)
	{
		fz_catch(-1, "couldn't find the exact part name for /docProps/core.xml");
		fz_strlcpy(path, "/docProps/core.xml", sizeof(path));
	}
	if (!*path)
		return NULL;

	part = xps_read_part(ctx, path);
	if (!part)
	{
		fz_catch(-1, "cannot read zip part '%s'", path);
		return NULL;
	}

	dict = fz_new_dict(8);
	xps_hacky_get_prop(part->data, dict, "Title", "dc:title");
	xps_hacky_get_prop(part->data, dict, "Subject", "dc:subject");
	xps_hacky_get_prop(part->data, dict, "Author", "dc:creator");
	xps_hacky_get_prop(part->data, dict, "CreationDate", "dcterms:created");
	xps_hacky_get_prop(part->data, dict, "ModDate", "dcterms:modified");

	xps_free_part(ctx, part);

	return dict;
}
