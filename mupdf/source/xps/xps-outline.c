#include "mupdf/xps.h"

/*
 * Parse the document structure / outline parts referenced from fixdoc relationships.
 */

static fz_outline *
xps_lookup_last_outline_at_level(fz_outline *node, int level, int target_level)
{
	while (node->next)
		node = node->next;
	if (level == target_level || !node->down)
		return node;
	return xps_lookup_last_outline_at_level(node->down, level + 1, target_level);
}

static fz_outline *
xps_parse_document_outline(xps_document *doc, fz_xml *root)
{
	fz_xml *node;
	fz_outline *head = NULL, *entry, *tail;
	int last_level = 1, this_level;
	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "OutlineEntry"))
		{
			char *level = fz_xml_att(node, "OutlineLevel");
			char *target = fz_xml_att(node, "OutlineTarget");
			char *description = fz_xml_att(node, "Description");
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
				entry->dest.ld.gotor.dest = fz_strdup(doc->ctx, target);
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

			if (!head)
			{
				head = entry;
			}
			else
			{
				tail = xps_lookup_last_outline_at_level(head, 1, this_level);
				if (this_level > last_level)
					tail->down = entry;
				else
					tail->next = entry;
			}

			last_level = this_level;
		}
	}
	/* SumatraPDF: support expansion states */
	if (head && (!head->next || !head->next->next))
	{
		head->is_open = 1;
		if (head->next)
			head->next->is_open = 1;
	}
	return head;
}

static fz_outline *
xps_parse_document_structure(xps_document *doc, fz_xml *root)
{
	fz_xml *node;
	if (fz_xml_is_tag(root, "DocumentStructure"))
	{
		node = fz_xml_down(root);
		if (node && fz_xml_is_tag(node, "DocumentStructure.Outline"))
		{
			node = fz_xml_down(node);
			if (node && fz_xml_is_tag(node, "DocumentOutline"))
				return xps_parse_document_outline(doc, node);
		}
	}
	return NULL;
}

static fz_outline *
xps_load_document_structure(xps_document *doc, xps_fixdoc *fixdoc)
{
	xps_part *part;
	fz_xml *root;
	fz_outline *outline;

	part = xps_read_part(doc, fixdoc->outline);
	fz_try(doc->ctx)
	{
		root = fz_parse_xml(doc->ctx, part->data, part->size, 0);
	}
	fz_always(doc->ctx)
	{
		xps_free_part(doc, part);
	}
	fz_catch(doc->ctx)
	{
		fz_rethrow(doc->ctx);
	}
	if (!root)
		return NULL;

	fz_try(doc->ctx)
	{
		outline = xps_parse_document_structure(doc, root);
	}
	fz_always(doc->ctx)
	{
		fz_free_xml(doc->ctx, root);
	}
	fz_catch(doc->ctx)
	{
		fz_rethrow(doc->ctx);
	}

	return outline;
}

fz_outline *
xps_load_outline(xps_document *doc)
{
	xps_fixdoc *fixdoc;
	fz_outline *head = NULL, *tail, *outline;

	for (fixdoc = doc->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
	{
		if (fixdoc->outline)
		{
			fz_try(doc->ctx)
			{
				outline = xps_load_document_structure(doc, fixdoc);
			}
			fz_catch(doc->ctx)
			{
				fz_rethrow_if(doc->ctx, FZ_ERROR_TRYLATER);
				outline = NULL;
			}
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
xps_extract_anchor_info(xps_document *doc, const fz_rect *rect, char *target_uri, char *anchor_name, int step)
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
			ld.ld.gotor.dest = fz_strdup(doc->ctx, target_uri);
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
		if (!fz_is_empty_rect(&doc->_clinks[doc->_clinks_len].rect))
			rect = &doc->_clinks[doc->_clinks_len].rect;
		if (doc->_clinks[doc->_clinks_len].link)
			doc->_clinks[doc->_clinks_len].link->rect = *rect;
	}
	if (step != 1 && doc->_clinks_len > 0 && doc->_clinks_len <= nelem(doc->_clinks))
		fz_union_rect(&doc->_clinks[doc->_clinks_len-1].rect, rect);

	if (anchor_name)
	{
		xps_target *target;
		char *value_id = fz_malloc(doc->ctx, strlen(anchor_name) + 2);
		sprintf(value_id, "#%s", anchor_name);
		target = xps_lookup_link_target_obj(doc, value_id);
		if (target)
			target->rect = *rect;
		fz_free(doc->ctx, value_id);
	}
}
