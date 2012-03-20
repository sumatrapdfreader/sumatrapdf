#include "muxps-internal.h"

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

static fz_outline *
xps_parse_document_outline(xps_document *doc, xml_element *root)
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
			/* SumatraPDF: allow target-less outline entries */
			if (!description)
				continue;

			entry = fz_malloc_struct(doc->ctx, fz_outline);
			entry->title = fz_strdup(doc->ctx, description);
			/* SumatraPDF: extended outline actions */
			if (!target)
				entry->dest.kind = FZ_LINK_NONE;
			else if (!xps_url_is_remote(target))
			{
				entry->dest.kind = FZ_LINK_GOTO;
				entry->dest.ld.gotor.page = xps_lookup_link_target(doc, target);
				/* for retrieving updated target rectangles */
				entry->dest.ld.gotor.rname = fz_strdup(doc->ctx, target);
			}
			else
			{
				entry->dest.kind = FZ_LINK_URI;
				entry->dest.ld.uri.uri = fz_strdup(doc->ctx, target);
				entry->dest.ld.uri.is_map = 0;
			}
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
xps_parse_document_structure(xps_document *doc, xml_element *root)
{
	xml_element *node;
	if (!strcmp(xml_tag(root), "DocumentStructure"))
	{
		node = xml_down(root);
		if (!strcmp(xml_tag(node), "DocumentStructure.Outline"))
		{
			node = xml_down(node);
			if (!strcmp(xml_tag(node), "DocumentOutline"))
				return xps_parse_document_outline(doc, node);
		}
	}
	return NULL;
}

static fz_outline *
xps_load_document_structure(xps_document *doc, xps_fixdoc *fixdoc)
{
	xps_part *part;
	xml_element *root;
	fz_outline *outline;

	part = xps_read_part(doc, fixdoc->outline);
	fz_try(doc->ctx)
	{
		root = xml_parse_document(doc->ctx, part->data, part->size);
	}
	fz_catch(doc->ctx)
	{
		xps_free_part(doc, part);
		fz_rethrow(doc->ctx);
	}
	xps_free_part(doc, part);
	if (!root)
		return NULL;

	fz_try(doc->ctx)
	{
		outline = xps_parse_document_structure(doc, root);
	}
	fz_catch(doc->ctx)
	{
		xml_free_element(doc->ctx, root);
		fz_rethrow(doc->ctx);
	}
	xml_free_element(doc->ctx, root);

	return outline;
}

fz_outline *
xps_load_outline(xps_document *doc)
{
	xps_fixdoc *fixdoc;
	fz_outline *head = NULL, *tail, *outline;

	for (fixdoc = doc->first_fixdoc; fixdoc; fixdoc = fixdoc->next) {
		if (fixdoc->outline) {
			outline = xps_load_document_structure(doc, fixdoc);
			if (!outline)
				continue;
			if (!head)
				head = outline;
			else
			{
				while (tail->next)
					tail = tail->next;
				tail->next = outline;
			}
			tail = outline;
		}
	}
	return head;
}

/* SumatraPDF: extended link support */

void
xps_extract_anchor_info(xps_document *doc, fz_rect rect, char *target_uri, char *anchor_name, int step)
{
	fz_link *new_link = NULL;

	if (!doc->current_page || doc->current_page->links_resolved)
		return;
	assert((step != 2 || !target_uri) && (step != 1 || !anchor_name));

	if (target_uri)
	{
		fz_link_dest ld = { 0 };
		if (!xps_url_is_remote(target_uri))
		{
			ld.kind = FZ_LINK_GOTO;
			ld.ld.gotor.page = xps_lookup_link_target(doc, target_uri);
			/* for retrieving updated target rectangles */
			ld.ld.gotor.rname = fz_strdup(doc->ctx, target_uri);
		}
		else
		{
			ld.kind = FZ_LINK_URI;
			ld.ld.uri.uri = fz_strdup(doc->ctx, target_uri);
			ld.ld.uri.is_map = 0;
		}
		new_link = fz_new_link(doc->ctx, rect, ld);
		new_link->next = doc->current_page->links;
		doc->current_page->links = new_link;
	}

	/* canvas bounds estimates for link and target positioning */
	if (step == 1 && ++doc->_clinks_len <= nelem(doc->_clinks)) // canvas begin
	{
		doc->_clinks[doc->_clinks_len-1].rect = fz_empty_rect;
		doc->_clinks[doc->_clinks_len-1].link = new_link;
	}
	if (step == 2 && doc->_clinks_len-- <= nelem(doc->_clinks)) // canvas end
	{
		if (!fz_is_empty_rect(doc->_clinks[doc->_clinks_len].rect))
			rect = doc->_clinks[doc->_clinks_len].rect;
		if (doc->_clinks[doc->_clinks_len].link)
			doc->_clinks[doc->_clinks_len].link->rect = rect;
	}
	if (step != 1 && doc->_clinks_len > 0 && doc->_clinks_len <= nelem(doc->_clinks))
		doc->_clinks[doc->_clinks_len-1].rect = fz_union_rect(doc->_clinks[doc->_clinks_len-1].rect, rect);

	if (anchor_name)
	{
		xps_target *target;
		char *value_id = fz_malloc(doc->ctx, strlen(anchor_name) + 2);
		sprintf(value_id, "#%s", anchor_name);
		target = xps_lookup_link_target_obj(doc, value_id);
		if (target)
			target->rect = rect;
		fz_free(doc->ctx, value_id);
	}
}

/* SumatraPDF: extract document properties (hacky) */

static xml_element *
xps_open_and_parse(xps_document *doc, char *path)
{
	xml_element *root = NULL;
	xps_part *part = xps_read_part(doc, path);
	/* "cannot read part '%s'", path */;
	fz_var(part);

	fz_try(doc->ctx)
	{
		root = xml_parse_document(doc->ctx, part->data, part->size);
	}
	fz_always(doc->ctx)
	{
		xps_free_part(doc, part);
	}
	fz_catch(doc->ctx)
	{
		fz_rethrow(doc->ctx);
	}

	return root;
}

static inline int iswhite(c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static xps_doc_prop *
xps_hacky_get_prop(fz_context *ctx, char *data, char *name, char *tag_name, xps_doc_prop *prev)
{
	char *start, *end;
	xps_doc_prop *prop;

	start = strstr(data, tag_name);
	if (!start || start == data || start[-1] != '<')
		return prev;
	end = strstr(start + 1, tag_name);
	start = strchr(start, '>');
	if (!start || !end || start >= end || end[-2] != '<' || end[-1] != '/')
		return prev;

	for (start++; iswhite(*start); start++);
	for (end -= 3; iswhite(*end) && end > start; end--);

	prop = fz_malloc_struct(ctx, xps_doc_prop);
	prop->name = fz_strdup(ctx, name);
	prop->value = fz_malloc(ctx, end - start + 2);
	fz_strlcpy(prop->value, start, end - start + 2);
	prop->next = prev;

	return prop;
}

#define REL_CORE_PROPERTIES \
	"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties"

static void
xps_find_doc_props_path(xps_document *doc, char path[1024])
{
	xml_element *root = xps_open_and_parse(doc, "/_rels/.rels");
	xml_element *item;

	if (!root || strcmp(xml_tag(root), "Relationships") != 0)
		fz_throw(doc->ctx, "couldn't parse part '/_rels/.rels'");

	*path = '\0';
	for (item = xml_down(root); item; item = xml_next(item))
	{
		if (!strcmp(xml_tag(item), "Relationship") && xml_att(item, "Type") &&
			!strcmp(xml_att(item, "Type"), REL_CORE_PROPERTIES) && xml_att(item, "Target"))
		{
			xps_resolve_url(path, "", xml_att(item, "Target"), 1024);
		}
	}

	xml_free_element(doc->ctx, root);
}

xps_doc_prop *xps_extract_doc_props(xps_document *doc)
{
	char path[1024];
	xps_part *part;
	xps_doc_prop *prop = NULL;
	fz_var(part);
	fz_var(prop);

	xps_find_doc_props_path(doc, path);
	if (!*path)
		return NULL;
	part = xps_read_part(doc, path);

	fz_try(doc->ctx)
	{
		prop = xps_hacky_get_prop(doc->ctx, part->data, "Title", "dc:title", prop);
		prop = xps_hacky_get_prop(doc->ctx, part->data, "Subject", "dc:subject", prop);
		prop = xps_hacky_get_prop(doc->ctx, part->data, "Author", "dc:creator", prop);
		prop = xps_hacky_get_prop(doc->ctx, part->data, "CreationDate", "dcterms:created", prop);
		prop = xps_hacky_get_prop(doc->ctx, part->data, "ModDate", "dcterms:modified", prop);
	}
	fz_always(doc->ctx)
	{
		xps_free_part(doc, part);
	}
	fz_catch(doc->ctx)
	{
		xps_free_doc_prop(doc->ctx, prop);
		fz_rethrow(doc->ctx);
	}

	return prop;
}

void
xps_free_doc_prop(fz_context *ctx, xps_doc_prop *prop)
{
	while (prop)
	{
		xps_doc_prop *next = prop->next;
		fz_free(ctx, prop->name);
		fz_free(ctx, prop->value);
		fz_free(ctx, prop);
		prop = next;
	}
}
