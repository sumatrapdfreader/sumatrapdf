#include "mupdf/fitz.h"
#include "xps-imp.h"

#include <stdlib.h>
#include <math.h>

/*
 * Parse the document structure / outline parts referenced from fixdoc relationships.
 */

static fz_outline *
xps_lookup_last_outline_at_level(fz_context *ctx, xps_document *doc, fz_outline *node, int level, int target_level)
{
	while (node->next)
		node = node->next;
	if (level == target_level || !node->down)
		return node;
	return xps_lookup_last_outline_at_level(ctx, doc, node->down, level + 1, target_level);
}

static fz_outline *
xps_parse_document_outline(fz_context *ctx, xps_document *doc, fz_xml *root)
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
			if (!target || !description)
				continue;

			entry = fz_new_outline(ctx);
			entry->title = Memento_label(fz_strdup(ctx, description), "outline_title");
			entry->uri = Memento_label(fz_strdup(ctx, target), "outline_uri");
			entry->page = xps_lookup_link_target(ctx, (fz_document*)doc, target, NULL, NULL).page;
			entry->down = NULL;
			entry->next = NULL;

			this_level = level ? atoi(level) : 1;

			if (!head)
			{
				head = entry;
			}
			else
			{
				tail = xps_lookup_last_outline_at_level(ctx, doc, head, 1, this_level);
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
xps_parse_document_structure(fz_context *ctx, xps_document *doc, fz_xml *root)
{
	fz_xml *node;
	if (fz_xml_is_tag(root, "DocumentStructure"))
	{
		node = fz_xml_down(root);
		if (node && fz_xml_is_tag(node, "DocumentStructure.Outline"))
		{
			node = fz_xml_down(node);
			if (node && fz_xml_is_tag(node, "DocumentOutline"))
				return xps_parse_document_outline(ctx, doc, node);
		}
	}
	return NULL;
}

static fz_outline *
xps_load_document_structure(fz_context *ctx, xps_document *doc, xps_fixdoc *fixdoc)
{
	xps_part *part;
	fz_xml_doc *xml = NULL;
	fz_outline *outline = NULL;

	fz_var(xml);

	part = xps_read_part(ctx, doc, fixdoc->outline);
	fz_try(ctx)
	{
		xml = fz_parse_xml(ctx, part->data, 0, 0);
		outline = xps_parse_document_structure(ctx, doc, fz_xml_root(xml));
	}
	fz_always(ctx)
	{
		fz_drop_xml(ctx, xml);
		xps_drop_part(ctx, doc, part);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return outline;
}

fz_outline *
xps_load_outline(fz_context *ctx, fz_document *doc_)
{
	xps_document *doc = (xps_document*)doc_;
	xps_fixdoc *fixdoc;
	fz_outline *head = NULL, *tail, *outline = NULL;

	for (fixdoc = doc->first_fixdoc; fixdoc; fixdoc = fixdoc->next)
	{
		if (fixdoc->outline)
		{
			fz_try(ctx)
			{
				outline = xps_load_document_structure(ctx, doc, fixdoc);
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
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
