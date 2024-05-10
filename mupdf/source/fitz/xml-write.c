// Copyright (C) 2024 Artifex Software, Inc.
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

#include "xml-imp.h"

void fz_save_xml(fz_context *ctx, fz_xml *root, const char *path, int indented)
{
	fz_output *out = fz_new_output_with_path(ctx, path, 0);

	fz_try(ctx)
	{
		fz_write_xml(ctx, root, out, indented);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
xml_escape_tag(fz_context *ctx, fz_output *out, const char *s)
{
	while (1)
	{
		int c;
		size_t len = fz_chartorune(&c, s);
		size_t i;
		if (c == 0)
			break;
		if (c == '<')
			fz_write_string(ctx, out, "&lt;");
		else if (c == '>')
			fz_write_string(ctx, out, "&gt;");
		else if (c == '&')
			fz_write_string(ctx, out, "&amp;");
		else
			for (i = 0; i < len; i++)
			{
				char d = s[i];
				if (d < 32 || d >= 127)
				{
					fz_write_string(ctx, out, "&#x");
					fz_write_byte(ctx, out, "0123456789abcdef"[(d>>4)&15]);
					fz_write_byte(ctx, out, "0123456789abcdef"[d&15]);
					fz_write_byte(ctx, out, ';');
				}
				else
					fz_write_byte(ctx, out, d);
			}
		s += len;
	}
}

static void
xml_escape_string(fz_context *ctx, fz_output *out, const char *s)
{
	while (1)
	{
		int c;
		size_t len = fz_chartorune(&c, s);
		size_t i;
		if (c == 0)
			break;
		if (c == '<')
			fz_write_string(ctx, out, "&lt;");
		else if (c == '>')
			fz_write_string(ctx, out, "&gt;");
		else if (c == '&')
			fz_write_string(ctx, out, "&amp;");
		else if (c == '\"')
		{
			fz_write_string(ctx, out, "&quot;");
		}
		else
			for (i = 0; i < len; i++)
			{
				char d = s[i];
				if (d < 32 || d >= 127)
				{
					fz_write_string(ctx, out, "&#x");
					fz_write_byte(ctx, out, "0123456789abcdef"[(d>>4)&15]);
					fz_write_byte(ctx, out, "0123456789abcdef"[d&15]);
					fz_write_byte(ctx, out, ';');
				}
				else
					fz_write_byte(ctx, out, d);
			}
		s += len;
	}
}

static void
indent(fz_context *ctx, fz_output *out, int depth)
{
	fz_write_byte(ctx, out, '\n');
	while (depth-- > 0)
	{
		fz_write_byte(ctx, out, ' ');
	}
}

static int
do_write(fz_context *ctx, fz_xml *node, fz_output *out, int depth)
{
	const char *tag;
	fz_xml *down;
	int last_was_text = 0;

	for (; node != NULL; node = fz_xml_next(node))
	{
		struct attribute *att;

		tag = fz_xml_tag(node);
		if (!tag)
		{
			/* Text node. */
			char *text = fz_xml_text(node);
			if (text)
				xml_escape_tag(ctx, out, text);
			last_was_text = 1;
			continue;
		}

		last_was_text = 0;
		if (depth >= 0)
			indent(ctx, out, depth);
		fz_write_byte(ctx, out, '<');
		xml_escape_tag(ctx, out, tag);

		for (att = node->u.node.u.d.atts; att; att = att->next)
		{
			fz_write_byte(ctx, out, ' ');
			xml_escape_tag(ctx, out, att->name);
			fz_write_string(ctx, out, "=\"");
			xml_escape_string(ctx, out, att->value);
			fz_write_byte(ctx, out, '\"');
		}

		down = fz_xml_down(node);
		if (down)
		{
			fz_write_byte(ctx, out, '>');
			if (!do_write(ctx, down, out, depth >= 0 ? depth+1 : -1))
				indent(ctx, out, depth);
			fz_write_string(ctx, out, "</");
			xml_escape_tag(ctx, out, tag);
			fz_write_byte(ctx, out, '>');
		}
		else
		{
			fz_write_string(ctx, out, "/>");
		}
	}
	return depth >= 0 ? last_was_text : 1;
}

void
fz_write_xml(fz_context *ctx, fz_xml *root, fz_output *out, int indented)
{
	if (root == NULL)
		return;

	fz_write_string(ctx, out, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>");

	/* Skip over the document object, if we're handed that. */
	if (root->up == NULL)
		root = root->down;

	if (!do_write(ctx, root, out, indented ? 0 : -1))
		fz_write_byte(ctx, out, '\n');
}
