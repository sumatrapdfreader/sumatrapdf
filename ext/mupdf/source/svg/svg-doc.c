// Copyright (C) 2004-2024 Artifex Software, Inc.
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
#include "svg-imp.h"

typedef struct
{
	fz_page super;
	svg_document *doc;
} svg_page;

static void
svg_drop_document(fz_context *ctx, fz_document *doc_)
{
	svg_document *doc = (svg_document*)doc_;
	svg_drop_class_map(ctx, doc);
	fz_drop_tree(ctx, doc->idmap, NULL);
	fz_drop_xml(ctx, doc->xml);
}

static int
svg_count_pages(fz_context *ctx, fz_document *doc_, int chapter)
{
	return 1;
}

static fz_rect
svg_bound_page(fz_context *ctx, fz_page *page_, fz_box_type box)
{
	svg_page *page = (svg_page*)page_;
	svg_document *doc = page->doc;

	svg_parse_document_bounds(ctx, doc, doc->root);

	return fz_make_rect(0, 0, doc->width, doc->height);
}

static void
svg_run_page(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	svg_page *page = (svg_page*)page_;
	svg_document *doc = page->doc;
	svg_run_document(ctx, doc, doc->root, dev, ctm);
}

static void
svg_drop_page(fz_context *ctx, fz_page *page_)
{
	/* nothing */
}

static fz_page *
svg_load_page(fz_context *ctx, fz_document *doc_, int chapter, int number)
{
	svg_document *doc = (svg_document*)doc_;
	svg_page *page;

	if (number != 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot find page %d", number);

	page = fz_new_derived_page(ctx, svg_page, doc_);
	page->super.bound_page = svg_bound_page;
	page->super.run_page_contents = svg_run_page;
	page->super.drop_page = svg_drop_page;
	page->doc = doc;

	return (fz_page*)page;
}

static void
svg_build_id_map(fz_context *ctx, svg_document *doc, fz_xml *root)
{
	fz_xml *node;

	char *id_att = fz_xml_att(root, "id");
	if (id_att)
		doc->idmap = fz_tree_insert(ctx, doc->idmap, id_att, root);

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
		svg_build_id_map(ctx, doc, node);
}

/* SumatraPDF: minimal CSS class support (#2155) */
static int
svg_is_class_char(int c)
{
	return c == '-' || c == '_' ||
		(c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static const char *
svg_skip_css_space(const char *p, const char *end)
{
	while (p < end)
	{
		if (p + 1 < end && p[0] == '/' && p[1] == '*')
		{
			p += 2;
			while (p + 1 < end && !(p[0] == '*' && p[1] == '/'))
				++p;
			p = p + 1 < end ? p + 2 : end;
		}
		else if (svg_is_whitespace(*p))
			++p;
		else
			break;
	}
	return p;
}

static char *
svg_dup_range(fz_context *ctx, const char *s, const char *end)
{
	size_t n = end - s;
	char *p = fz_malloc(ctx, n + 1);
	memcpy(p, s, n);
	p[n] = 0;
	return p;
}

static void
svg_add_class(fz_context *ctx, svg_document *doc, const char *name, const char *name_end,
	const char *decl, const char *decl_end)
{
	svg_class *cls;
	const char *p;

	for (p = name; p < name_end; ++p)
		if (!svg_is_class_char(*p))
			return; /* not a plain class selector: pseudo class, descendant, ... */

	cls = fz_malloc_struct(ctx, svg_class);
	fz_try(ctx)
	{
		cls->name = svg_dup_range(ctx, name, name_end);
		cls->decl = svg_dup_range(ctx, decl, decl_end);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, cls->name);
		fz_free(ctx, cls);
		fz_rethrow(ctx);
	}

	cls->next = doc->classes;
	doc->classes = cls;
}

/* Split a selector list (".a, .b") and keep the class selectors in it. */
static void
svg_add_rule(fz_context *ctx, svg_document *doc, const char *sel, const char *sel_end,
	const char *decl, const char *decl_end)
{
	while (sel < sel_end)
	{
		const char *name, *end;

		name = svg_skip_css_space(sel, sel_end);
		sel = name;
		while (sel < sel_end && *sel != ',')
			++sel;
		end = sel;
		if (sel < sel_end)
			++sel;
		while (end > name && svg_is_whitespace(end[-1]))
			--end;

		if (end - name > 1 && name[0] == '.')
			svg_add_class(ctx, doc, name + 1, end, decl, decl_end);
	}
}

/* A deliberately small CSS subset: only ".name { ... }" rules are collected, so
 * that class="name" can be resolved like an inline style attribute. Everything
 * else (at-rules, element and id selectors, combinators) is skipped. */
static void
svg_parse_style_sheet(fz_context *ctx, svg_document *doc, const char *p)
{
	const char *end = p + strlen(p);

	while (p < end)
	{
		const char *sel, *sel_end, *decl;

		p = svg_skip_css_space(p, end);
		if (p == end)
			break;

		if (*p == '@')
		{
			int depth = 0;
			while (p < end)
			{
				if (*p == '{')
					++depth;
				else if (*p == '}' && --depth == 0)
				{
					++p;
					break;
				}
				else if (*p == ';' && depth == 0)
				{
					++p;
					break;
				}
				++p;
			}
			continue;
		}

		sel = p;
		while (p < end && *p != '{' && *p != '}')
			++p;
		if (p == end || *p != '{')
			break;
		sel_end = p++;

		decl = p;
		while (p < end && *p != '}')
			++p;
		svg_add_rule(ctx, doc, sel, sel_end, decl, p);
		if (p < end)
			++p;
	}
}

void
svg_build_class_map(fz_context *ctx, svg_document *doc, fz_xml *root)
{
	fz_xml *node;

	if (fz_xml_is_tag(root, "style"))
	{
		for (node = fz_xml_down(root); node; node = fz_xml_next(node))
		{
			char *text = fz_xml_text(node);
			if (text)
				svg_parse_style_sheet(ctx, doc, text);
		}
		return;
	}

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
		svg_build_class_map(ctx, doc, node);
}

void
svg_drop_class_map(fz_context *ctx, svg_document *doc)
{
	svg_class *cls = doc->classes;
	while (cls)
	{
		svg_class *next = cls->next;
		fz_free(ctx, cls->name);
		fz_free(ctx, cls->decl);
		fz_free(ctx, cls);
		cls = next;
	}
	doc->classes = NULL;
}

static int
svg_lookup_metadata(fz_context *ctx, fz_document *doc_, const char *key, char *buf, size_t size)
{
	if (!strcmp(key, FZ_META_FORMAT))
		return 1 + (int)fz_strlcpy(buf, "SVG", size);
	return -1;
}

static fz_document *
svg_open_document_with_xml(fz_context *ctx, fz_xml_doc *xmldoc, fz_xml *xml, const char *base_uri, fz_archive *zip)
{
	svg_document *doc;

	doc = fz_new_derived_document(ctx, svg_document);
	doc->super.drop_document = svg_drop_document;
	doc->super.count_pages = svg_count_pages;
	doc->super.load_page = svg_load_page;
	doc->super.lookup_metadata = svg_lookup_metadata;

	doc->idmap = NULL;
	doc->classes = NULL;
	if (base_uri)
		fz_strlcpy(doc->base_uri, base_uri, sizeof doc->base_uri);
	doc->xml = NULL;
	doc->root = xml;
	doc->zip = zip;

	fz_try(ctx)
	{
		if (xmldoc)
		{
			svg_build_id_map(ctx, doc, fz_xml_root(xmldoc));
			svg_build_class_map(ctx, doc, fz_xml_root(xmldoc));
		}
		else
		{
			svg_build_id_map(ctx, doc, doc->root);
			svg_build_class_map(ctx, doc, doc->root);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_document(ctx, &doc->super);
		fz_rethrow(ctx);
	}

	return (fz_document*)doc;
}

static fz_document *
svg_open_document_with_buffer(fz_context *ctx, fz_buffer *buf, const char *base_uri, fz_archive *zip)
{
	svg_document *doc;

	doc = fz_new_derived_document(ctx, svg_document);
	doc->super.drop_document = svg_drop_document;
	doc->super.count_pages = svg_count_pages;
	doc->super.load_page = svg_load_page;
	doc->super.lookup_metadata = svg_lookup_metadata;

	doc->idmap = NULL;
	doc->classes = NULL;
	if (base_uri)
		fz_strlcpy(doc->base_uri, base_uri, sizeof doc->base_uri);
	doc->zip = zip;

	fz_try(ctx)
	{
		doc->xml = fz_parse_xml(ctx, buf, 0);
		doc->root = fz_xml_root(doc->xml);
		svg_build_id_map(ctx, doc, doc->root);
		svg_build_class_map(ctx, doc, doc->root);
	}
	fz_catch(ctx)
	{
		fz_drop_document(ctx, &doc->super);
		fz_rethrow(ctx);
	}

	return (fz_document*)doc;
}

static fz_document *
svg_open_document(fz_context *ctx, const fz_document_handler *handler, fz_stream *file, fz_stream *accel, fz_archive *zip, void *state)
{
	fz_buffer *buf = fz_read_all(ctx, file, 0);
	fz_document *doc = NULL;

	fz_try(ctx)
		doc = svg_open_document_with_buffer(ctx, buf, NULL, NULL);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return doc;
}

fz_display_list *
fz_new_display_list_from_svg(fz_context *ctx, fz_buffer *buf, const char *base_uri, fz_archive *zip, float *w, float *h)
{
	fz_document *doc;
	fz_display_list *list = NULL;

	doc = svg_open_document_with_buffer(ctx, buf, base_uri, zip);
	fz_try(ctx)
	{
		list = fz_new_display_list_from_page_number(ctx, doc, 0);
		*w = ((svg_document*)doc)->width;
		*h = ((svg_document*)doc)->height;
	}
	fz_always(ctx)
		fz_drop_document(ctx, doc);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return list;
}

fz_display_list *
fz_new_display_list_from_svg_xml(fz_context *ctx, fz_xml_doc *xmldoc, fz_xml *xml, const char *base_uri, fz_archive *zip, float *w, float *h)
{
	fz_document *doc;
	fz_display_list *list = NULL;

	doc = svg_open_document_with_xml(ctx, xmldoc, xml, base_uri, zip);
	fz_try(ctx)
	{
		list = fz_new_display_list_from_page_number(ctx, doc, 0);
		*w = ((svg_document*)doc)->width;
		*h = ((svg_document*)doc)->height;
	}
	fz_always(ctx)
		fz_drop_document(ctx, doc);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return list;
}

fz_image *
fz_new_image_from_svg(fz_context *ctx, fz_buffer *buf, const char *base_uri, fz_archive *zip)
{
	fz_display_list *list;
	fz_image *image = NULL;
	float w, h;

	list = fz_new_display_list_from_svg(ctx, buf, base_uri, zip, &w, &h);
	fz_try(ctx)
		image = fz_new_image_from_display_list(ctx, w, h, list);
	fz_always(ctx)
		fz_drop_display_list(ctx, list);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return image;
}

fz_image *
fz_new_image_from_svg_xml(fz_context *ctx, fz_xml_doc *xmldoc, fz_xml *xml, const char *base_uri, fz_archive *zip)
{
	fz_display_list *list;
	fz_image *image = NULL;
	float w, h;

	list = fz_new_display_list_from_svg_xml(ctx, xmldoc, xml, base_uri, zip, &w, &h);
	fz_try(ctx)
		image = fz_new_image_from_display_list(ctx, w, h, list);
	fz_always(ctx)
		fz_drop_display_list(ctx, list);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return image;
}

static const char *svg_extensions[] =
{
	"svg",
	NULL
};

static const char *svg_mimetypes[] =
{
	"image/svg+xml",
	NULL
};

static int
svg_recognize_doc_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stm, fz_archive *dir, void **state, fz_document_recognize_state_free_fn **free_state)
{
	// A standalone SVG document is an XML document with an <svg> root element.
	//
	// Assume the document is ASCII or UTF-8.
	//
	// Parse the start of the file using a simplified XML parser, skipping
	// processing instructions and comments, and stopping at the first
	// element.
	//
	// Return failure on anything unexpected, or if the first element is not SVG.

	int c;

	if (state)
		*state = NULL;
	if (free_state)
		*free_state = NULL;

	if (stm == NULL)
		return 0;

parse_text:
	// Skip whitespace until "<"
	c = fz_read_byte(ctx, stm);
	while (c == ' ' || c == '\r' || c == '\n' || c == '\t')
		c = fz_read_byte(ctx, stm);
	if (c == '<')
		goto parse_element;
	return 0;

parse_element:
	// Either "<?...>" or "<!...>" or "<svg" or not an SVG document.
	c = fz_read_byte(ctx, stm);
	if (c == '!' || c == '?')
		goto parse_comment;
	if (c != 's')
		return 0;
	c = fz_read_byte(ctx, stm);
	if (c != 'v')
		return 0;
	c = fz_read_byte(ctx, stm);
	if (c != 'g')
		return 0;
	return 100;

parse_comment:
	// Skip everything after "<?" or "<!" until ">"
	c = fz_read_byte(ctx, stm);
	while (c != EOF && c != '>')
		c = fz_read_byte(ctx, stm);
	if (c == '>')
		goto parse_text;
	return 0;
}

fz_document_handler svg_document_handler =
{
	NULL,
	svg_open_document,
	svg_extensions,
	svg_mimetypes,
	svg_recognize_doc_content
};
