// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "xps-imp.h"

#include <string.h>

static fz_xml *
xps_lookup_resource(fz_context *ctx, xps_document *doc, xps_resource *dict, char *name, char **urip)
{
	xps_resource *head, *node;
	for (head = dict; head; head = head->parent)
	{
		for (node = head; node; node = node->next)
		{
			if (!strcmp(node->name, name))
			{
				if (urip && head->base_uri)
					*urip = head->base_uri;
				return node->data;
			}
		}
	}
	return NULL;
}

static fz_xml *
xps_parse_resource_reference(fz_context *ctx, xps_document *doc, xps_resource *dict, char *att, char **urip)
{
	char name[1024];
	char *s;

	if (strstr(att, "{StaticResource ") != att)
		return NULL;

	fz_strlcpy(name, att + 16, sizeof name);
	s = strrchr(name, '}');
	if (s)
		*s = 0;

	return xps_lookup_resource(ctx, doc, dict, name, urip);
}

void
xps_resolve_resource_reference(fz_context *ctx, xps_document *doc, xps_resource *dict,
		char **attp, fz_xml **tagp, char **urip)
{
	if (*attp)
	{
		fz_xml *rsrc = xps_parse_resource_reference(ctx, doc, dict, *attp, urip);
		if (rsrc)
		{
			*attp = NULL;
			*tagp = rsrc;
		}
	}
}

static xps_resource *
xps_parse_remote_resource_dictionary(fz_context *ctx, xps_document *doc, char *base_uri, char *source_att)
{
	char part_name[1024];
	char part_uri[1024];
	xps_part *part;
	xps_resource *dict = NULL;
	fz_xml_doc *xml = NULL;
	char *s;

	fz_var(xml);

	/* External resource dictionaries MUST NOT reference other resource dictionaries */
	xps_resolve_url(ctx, doc, part_name, base_uri, source_att, sizeof part_name);

	part = xps_read_part(ctx, doc, part_name);
	fz_try(ctx)
	{
		xml = fz_parse_xml(ctx, part->data, 0);
		if (!fz_xml_is_tag(fz_xml_root(xml), "ResourceDictionary"))
			fz_throw(ctx, FZ_ERROR_FORMAT, "expected ResourceDictionary element");

		fz_strlcpy(part_uri, part_name, sizeof part_uri);
		s = strrchr(part_uri, '/');
		if (s)
			s[1] = 0;

		dict = xps_parse_resource_dictionary(ctx, doc, part_uri, fz_xml_root(xml));
		if (dict)
		{
			dict->base_xml = xml; /* pass on ownership */
			xml = NULL;
		}
	}
	fz_always(ctx)
	{
		xps_drop_part(ctx, doc, part);
		fz_drop_xml(ctx, xml);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return dict;
}

xps_resource *
xps_parse_resource_dictionary(fz_context *ctx, xps_document *doc, char *base_uri, fz_xml *root)
{
	xps_resource *head;
	xps_resource *entry;
	fz_xml *node;
	char *source;
	char *key;

	source = fz_xml_att(root, "Source");
	if (source)
		return xps_parse_remote_resource_dictionary(ctx, doc, base_uri, source);

	head = NULL;

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		key = fz_xml_att(node, "x:Key");
		if (key)
		{
			entry = fz_malloc_struct(ctx, xps_resource);
			entry->name = key;
			entry->base_uri = NULL;
			entry->base_xml = NULL;
			entry->data = node;
			entry->next = head;
			entry->parent = NULL;
			head = entry;
		}
	}

	if (head)
	{
		fz_try(ctx)
			head->base_uri = fz_strdup(ctx, base_uri);
		fz_catch(ctx)
		{
			fz_free(ctx, entry);
			fz_rethrow(ctx);
		}
	}

	return head;
}

void
xps_drop_resource_dictionary(fz_context *ctx, xps_document *doc, xps_resource *dict)
{
	xps_resource *next;
	while (dict)
	{
		next = dict->next;
		fz_drop_xml(ctx, dict->base_xml);
		fz_free(ctx, dict->base_uri);
		fz_free(ctx, dict);
		dict = next;
	}
}
