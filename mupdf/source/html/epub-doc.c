// Copyright (C) 2004-2023 Artifex Software, Inc.
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
#include "html-imp.h"

#include <string.h>
#include <math.h>

#include <zlib.h> /* for crc32 */

enum { T, R, B, L };

typedef struct epub_chapter epub_chapter;
typedef struct epub_page epub_page;

typedef struct
{
	int max_chapters;
	int num_chapters;
	float layout_w;
	float layout_h;
	float layout_em;
	uint32_t css_sum;
	int use_doc_css;
	int *pages_in_chapter;
} epub_accelerator;

typedef struct
{
	fz_document super;
	fz_archive *zip;
	fz_html_font_set *set;
	int count;
	epub_chapter *spine;
	fz_outline *outline;
	char *dc_title, *dc_creator;
	float layout_w, layout_h, layout_em;
	epub_accelerator *accel;
	uint32_t css_sum;

	/* A common pattern of use is for us to open a document,
	 * load a page, draw it, drop it, load the next page,
	 * draw it, drop it etc. This means that the HTML for
	 * a chapter might get thrown away between the drop and
	 * the the next load (if the chapter is large, and the
	 * store size is low). Accordingly, we store a handle
	 * to the most recently used html block here, thus
	 * ensuring that the stored copy won't be evicted. */
	fz_html *most_recent_html;
} epub_document;

struct epub_chapter
{
	epub_document *doc;
	char *path;
	int number;
	epub_chapter *next;
};

struct epub_page
{
	fz_page super;
	epub_chapter *ch;
	int number;
	fz_html *html;
};

static uint32_t
user_css_sum(fz_context *ctx)
{
	uint32_t sum = 0;
	const char *css = fz_user_css(ctx);
	sum = crc32(0, NULL, 0);
	if (css)
		sum = crc32(sum, (Byte*)css, (int)strlen(css));
	return sum;
}

static int dummy = 1;

struct encrypted {
	fz_archive super;
	fz_archive *chain;
	fz_tree *info;
};

static int has_encrypted_entry(fz_context *ctx, fz_archive *arch_, const char *name)
{
	struct encrypted *arch = (struct encrypted *)arch_;
	return fz_has_archive_entry(ctx, arch->chain, name);
}

static fz_stream *open_encrypted_entry(fz_context *ctx, fz_archive *arch_, const char *name)
{
	struct encrypted *arch = (struct encrypted *)arch_;
	if (fz_tree_lookup(ctx, arch->info, name))
		return NULL;
	return fz_open_archive_entry(ctx, arch->chain, name);
}

static fz_buffer *read_encrypted_entry(fz_context *ctx, fz_archive *arch_, const char *name)
{
	struct encrypted *arch = (struct encrypted *)arch_;
	if (fz_tree_lookup(ctx, arch->info, name))
		return NULL;
	return fz_read_archive_entry(ctx, arch->chain, name);
}

static void drop_encrypted_archive(fz_context *ctx, fz_archive *arch_)
{
	struct encrypted *arch = (struct encrypted *)arch_;
	fz_drop_tree(ctx, arch->info, NULL);
	fz_drop_archive(ctx, arch->chain);
}

static fz_archive *new_encrypted_archive(fz_context *ctx, fz_archive *chain, fz_tree *info)
{
	struct encrypted *arch;

	arch = fz_new_derived_archive(ctx, NULL, struct encrypted);
	arch->super.format = "encrypted";
	arch->super.has_entry = has_encrypted_entry;
	arch->super.read_entry = read_encrypted_entry;
	arch->super.open_entry = open_encrypted_entry;
	arch->super.drop_archive = drop_encrypted_archive;
	arch->chain = chain;
	arch->info = info;

	return &arch->super;
}

static void
epub_parse_encryption(fz_context *ctx, epub_document *doc, fz_xml *root)
{
	fz_tree *info = NULL;
	fz_xml *edata;

	for (edata = fz_xml_find_down(root, "EncryptedData"); edata; edata = fz_xml_find_next(edata, "EncryptedData"))
	{
		fz_xml *cdata = fz_xml_find_down(edata, "CipherData");
		fz_xml *cref = fz_xml_find_down(cdata, "CipherReference");
		char *uri = fz_xml_att(cref, "URI");
		if (uri)
		{
			// TODO: Support reading EncryptedKey and EncryptionMethod to decrypt content.
			info = fz_tree_insert(ctx, info, uri, &dummy);
		}
	}

	if (info)
	{
		doc->zip = new_encrypted_archive(ctx, doc->zip, info);
	}
}

static fz_html *epub_get_laid_out_html(fz_context *ctx, epub_document *doc, epub_chapter *ch);

static int count_laid_out_pages(fz_html *html)
{
	if (html->tree.root->s.layout.b > 0)
		return ceilf(html->tree.root->s.layout.b / html->page_h);
	return 1;
}

static void
invalidate_accelerator(fz_context *ctx, epub_accelerator *acc)
{
	int i;

	for (i = 0; i < acc->max_chapters; i++)
		acc->pages_in_chapter[i] = -1;
}

static int count_chapter_pages(fz_context *ctx, epub_document *doc, epub_chapter *ch)
{
	epub_accelerator *acc = doc->accel;
	int use_doc_css = fz_use_document_css(ctx);

	if (use_doc_css != acc->use_doc_css || doc->css_sum != acc->css_sum)
	{
		acc->use_doc_css = use_doc_css;
		acc->css_sum = doc->css_sum;
		invalidate_accelerator(ctx, acc);
	}

	if (ch->number < acc->num_chapters && acc->pages_in_chapter[ch->number] != -1)
		return acc->pages_in_chapter[ch->number];

	fz_drop_html(ctx, epub_get_laid_out_html(ctx, doc, ch));
	return acc->pages_in_chapter[ch->number];
}

static fz_link_dest
epub_resolve_link(fz_context *ctx, fz_document *doc_, const char *dest)
{
	epub_document *doc = (epub_document*)doc_;
	epub_chapter *ch;
	int i;

	const char *s = strchr(dest, '#');
	size_t n = s ? (size_t)(s - dest) : strlen(dest);
	if (s && s[1] == 0)
		s = NULL;

	for (i = 0, ch = doc->spine; ch; ++i, ch = ch->next)
	{
		if (!strncmp(ch->path, dest, n) && ch->path[n] == 0)
		{
			if (s)
			{
				float y;
				fz_html *html = epub_get_laid_out_html(ctx, doc, ch);
				int ph = html->page_h;

				/* Search for a matching fragment */
				y = fz_find_html_target(ctx, html, s+1);
				fz_drop_html(ctx, html);
				if (y >= 0)
				{
					int page = y / ph;
					return fz_make_link_dest_xyz(i, page, 0, y - page * ph, 0);
				}
				return fz_make_link_dest_none();
			}
			return fz_make_link_dest_xyz(i, 0, 0, 0, 0);
		}
	}

	return fz_make_link_dest_none();
}

static void
epub_layout(fz_context *ctx, fz_document *doc_, float w, float h, float em)
{
	epub_document *doc = (epub_document*)doc_;
	uint32_t css_sum = user_css_sum(ctx);
	int use_doc_css = fz_use_document_css(ctx);

	if (doc->layout_w == w && doc->layout_h == h && doc->layout_em == em && doc->css_sum == css_sum)
		return;
	doc->layout_w = w;
	doc->layout_h = h;
	doc->layout_em = em;

	if (doc->accel == NULL)
		return;

	/* When we load the saved accelerator, doc->accel
	 * can be populated with different values than doc.
	 * This is really useful as doc starts out with the
	 * values being 0. If we've got the right values
	 * already, then don't bin the data! */
	if (doc->accel->layout_w == w &&
		doc->accel->layout_h == h &&
		doc->accel->layout_em == em &&
		doc->accel->use_doc_css == use_doc_css &&
		doc->accel->css_sum == css_sum)
		return;

	doc->accel->layout_w = w;
	doc->accel->layout_h = h;
	doc->accel->layout_em = em;
	doc->accel->use_doc_css = use_doc_css;
	doc->accel->css_sum = css_sum;
	invalidate_accelerator(ctx, doc->accel);
}

static int
epub_count_chapters(fz_context *ctx, fz_document *doc_)
{
	epub_document *doc = (epub_document*)doc_;
	epub_chapter *ch;
	int count = 0;
	for (ch = doc->spine; ch; ch = ch->next)
		++count;
	return count;
}

static int
epub_count_pages(fz_context *ctx, fz_document *doc_, int chapter)
{
	epub_document *doc = (epub_document*)doc_;
	epub_chapter *ch;
	int i;
	for (i = 0, ch = doc->spine; ch; ++i, ch = ch->next)
	{
		if (i == chapter)
		{
			return count_chapter_pages(ctx, doc, ch);
		}
	}
	return 0;
}

#define MAGIC_ACCELERATOR 0xacce1e7a
#define MAGIC_ACCEL_EPUB  0x62755065
#define ACCEL_VERSION     0x00010001

static void epub_load_accelerator(fz_context *ctx, epub_document *doc, fz_stream *accel)
{
	int v;
	float w, h, em;
	int num_chapters;
	epub_accelerator *acc = NULL;
	uint32_t css_sum;
	int use_doc_css;
	int make_new = (accel == NULL);

	fz_var(acc);

	if (accel)
	{
		/* Try to read the accelerator data. If we fail silently give up. */
		fz_try(ctx)
		{
			v = fz_read_int32_le(ctx, accel);
			if (v != (int32_t)MAGIC_ACCELERATOR)
			{
				make_new = 1;
				break;
			}

			v = fz_read_int32_le(ctx, accel);
			if (v != MAGIC_ACCEL_EPUB)
			{
				make_new = 1;
				break;
			}

			v = fz_read_int32_le(ctx, accel);
			if (v != ACCEL_VERSION)
			{
				make_new = 1;
				break;
			}

			w = fz_read_float_le(ctx, accel);
			h = fz_read_float_le(ctx, accel);
			em = fz_read_float_le(ctx, accel);
			css_sum = fz_read_uint32_le(ctx, accel);
			use_doc_css = fz_read_int32_le(ctx, accel);

			num_chapters = fz_read_int32_le(ctx, accel);
			if (num_chapters <= 0)
			{
				make_new = 1;
				break;
			}

			acc = fz_malloc_struct(ctx, epub_accelerator);
			acc->pages_in_chapter = Memento_label(fz_malloc_array(ctx, num_chapters, int), "accel_pages_in_chapter");
			acc->max_chapters = acc->num_chapters = num_chapters;
			acc->layout_w = w;
			acc->layout_h = h;
			acc->layout_em = em;
			acc->css_sum = css_sum;
			acc->use_doc_css = use_doc_css;

			for (v = 0; v < num_chapters; v++)
				acc->pages_in_chapter[v] = fz_read_int32_le(ctx, accel);
		}
		fz_catch(ctx)
		{
			if (acc)
				fz_free(ctx, acc->pages_in_chapter);
			fz_free(ctx, acc);
			/* Swallow the error and run unaccelerated */
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
			make_new = 1;
		}
	}

	/* If we aren't given an accelerator to load (or the one we're given
	 * is bad) create a blank stub and we can fill it out as we go. */
	if (make_new)
	{
		acc = fz_malloc_struct(ctx, epub_accelerator);
		acc->css_sum = doc->css_sum;
		acc->use_doc_css = fz_use_document_css(ctx);
	}

	doc->accel = acc;
}

static void
accelerate_chapter(fz_context *ctx, epub_document *doc, epub_chapter *ch, fz_html *html)
{
	epub_accelerator *acc = doc->accel;
	int p = count_laid_out_pages(html);

	if (ch->number < acc->num_chapters)
	{
		if (acc->pages_in_chapter[ch->number] != p && acc->pages_in_chapter[ch->number] != -1)
		{
			fz_warn(ctx, "Invalidating stale accelerator data.");
			invalidate_accelerator(ctx, doc->accel);
		}
		acc->pages_in_chapter[ch->number] = p;
		return;
	}

	if (ch->number >= acc->max_chapters)
	{
		int n = acc->max_chapters;
		int i;
		if (n == 0)
			n = 4;
		while (n <= ch->number)
			n *= 2;

		acc->pages_in_chapter = fz_realloc_array(ctx, acc->pages_in_chapter, n, int);
		for (i = acc->max_chapters; i < n; i++)
			acc->pages_in_chapter[i] = -1;
		acc->max_chapters = n;
	}
	acc->pages_in_chapter[ch->number] = p;
	if (acc->num_chapters < ch->number+1)
		acc->num_chapters = ch->number+1;
}

static void
epub_drop_page(fz_context *ctx, fz_page *page_)
{
	epub_page *page = (epub_page *)page_;
	fz_drop_html(ctx, page->html);
}

static epub_chapter *
epub_load_chapter(fz_context *ctx, epub_document *doc, const char *path, int i)
{
	epub_chapter *ch;

	ch = fz_malloc_struct(ctx, epub_chapter);
	fz_try(ctx)
	{
		ch->path = Memento_label(fz_strdup(ctx, path), "chapter_path");
		ch->number = i;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, ch);
		fz_rethrow(ctx);
	}

	return ch;
}

static fz_html *
epub_parse_chapter(fz_context *ctx, epub_document *doc, epub_chapter *ch)
{
	fz_archive *zip = doc->zip;
	fz_buffer *buf;
	char base_uri[2048];
	fz_html *html;

	/* Look for one we made earlier */
	html = fz_find_html(ctx, doc, ch->number);
	if (html)
		return html;

	fz_dirname(base_uri, ch->path, sizeof base_uri);

	buf = fz_read_archive_entry(ctx, zip, ch->path);
	fz_try(ctx)
		html = fz_parse_xhtml(ctx, doc->set, zip, base_uri, buf, fz_user_css(ctx));
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return fz_store_html(ctx, html, doc, ch->number);
}

static fz_html *
epub_get_laid_out_html(fz_context *ctx, epub_document *doc, epub_chapter *ch)
{
	fz_html *html = epub_parse_chapter(ctx, doc, ch);
	fz_try(ctx)
	{
		fz_layout_html(ctx, html, doc->layout_w, doc->layout_h, doc->layout_em);
		accelerate_chapter(ctx, doc, ch, html);
	}
	fz_catch(ctx)
	{
		fz_drop_html(ctx, html);
		fz_rethrow(ctx);
	}

	fz_drop_html(ctx, doc->most_recent_html);
	doc->most_recent_html = fz_keep_html(ctx, html);

	return html;
}

static fz_rect
epub_bound_page(fz_context *ctx, fz_page *page_, fz_box_type box)
{
	epub_document *doc = (epub_document*)page_->doc;
	epub_page *page = (epub_page*)page_;
	epub_chapter *ch = page->ch;
	fz_rect bbox;
	fz_html *html = epub_get_laid_out_html(ctx, doc, ch);

	bbox.x0 = 0;
	bbox.y0 = 0;
	bbox.x1 = html->page_w + html->page_margin[L] + html->page_margin[R];
	bbox.y1 = html->page_h + html->page_margin[T] + html->page_margin[B];
	fz_drop_html(ctx, html);
	return bbox;
}

static void
epub_run_page(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	epub_page *page = (epub_page*)page_;

	fz_draw_html(ctx, dev, ctm, page->html, page->number);
}

static fz_link *
epub_load_links(fz_context *ctx, fz_page *page_)
{
	epub_page *page = (epub_page*)page_;
	epub_chapter *ch = page->ch;

	return fz_load_html_links(ctx, page->html, page->number, ch->path);
}

static fz_bookmark
epub_make_bookmark(fz_context *ctx, fz_document *doc_, fz_location loc)
{
	epub_document *doc = (epub_document*)doc_;
	epub_chapter *ch;
	int i;

	for (i = 0, ch = doc->spine; ch; ++i, ch = ch->next)
	{
		if (i == loc.chapter)
		{
			fz_html *html = epub_get_laid_out_html(ctx, doc, ch);
			fz_bookmark mark = fz_make_html_bookmark(ctx, html, loc.page);
			fz_drop_html(ctx, html);
			return mark;
		}
	}

	return 0;
}

static fz_location
epub_lookup_bookmark(fz_context *ctx, fz_document *doc_, fz_bookmark mark)
{
	epub_document *doc = (epub_document*)doc_;
	epub_chapter *ch;
	int i;

	for (i = 0, ch = doc->spine; ch; ++i, ch = ch->next)
	{
		fz_html *html = epub_get_laid_out_html(ctx, doc, ch);
		int p = fz_lookup_html_bookmark(ctx, html, mark);
		fz_drop_html(ctx, html);
		if (p != -1)
			return fz_make_location(i, p);
	}
	return fz_make_location(-1, -1);
}

static fz_page *
epub_load_page(fz_context *ctx, fz_document *doc_, int chapter, int number)
{
	epub_document *doc = (epub_document*)doc_;
	epub_chapter *ch;
	int i;
	for (i = 0, ch = doc->spine; ch; ++i, ch = ch->next)
	{
		if (i == chapter)
		{
			epub_page *page = fz_new_derived_page(ctx, epub_page, doc_);
			page->super.bound_page = epub_bound_page;
			page->super.run_page_contents = epub_run_page;
			page->super.load_links = epub_load_links;
			page->super.drop_page = epub_drop_page;
			page->ch = ch;
			page->number = number;
			page->html = epub_get_laid_out_html(ctx, doc, ch);
			return (fz_page*)page;
		}
	}
	return NULL;
}

static void
epub_page_label(fz_context *ctx, fz_document *doc_, int chapter, int number, char *buf, size_t size)
{
	fz_snprintf(buf, size, "ch. %d, p. %d", chapter+1, number+1);
}

static void
epub_drop_accelerator(fz_context *ctx, epub_accelerator *acc)
{
	if (acc == NULL)
		return;

	fz_free(ctx, acc->pages_in_chapter);
	fz_free(ctx, acc);
}

static void
epub_drop_document(fz_context *ctx, fz_document *doc_)
{
	epub_document *doc = (epub_document*)doc_;
	epub_chapter *ch, *next;
	ch = doc->spine;
	while (ch)
	{
		next = ch->next;
		fz_free(ctx, ch->path);
		fz_free(ctx, ch);
		ch = next;
	}
	epub_drop_accelerator(ctx, doc->accel);
	fz_drop_archive(ctx, doc->zip);
	fz_drop_html_font_set(ctx, doc->set);
	fz_drop_outline(ctx, doc->outline);
	fz_free(ctx, doc->dc_title);
	fz_free(ctx, doc->dc_creator);
	fz_drop_html(ctx, doc->most_recent_html);
	fz_purge_stored_html(ctx, doc);
}

static const char *
rel_path_from_idref(fz_xml *manifest, const char *idref)
{
	fz_xml *item;
	if (!idref)
		return NULL;
	item = fz_xml_find_down(manifest, "item");
	while (item)
	{
		const char *id = fz_xml_att(item, "id");
		if (id && !strcmp(id, idref))
			return fz_xml_att(item, "href");
		item = fz_xml_find_next(item, "item");
	}
	return NULL;
}

static const char *
path_from_idref(char *path, fz_xml *manifest, const char *base_uri, const char *idref, int n)
{
	const char *rel_path = rel_path_from_idref(manifest, idref);
	if (!rel_path)
	{
		path[0] = 0;
		return NULL;
	}
	fz_strlcpy(path, base_uri, n);
	fz_strlcat(path, "/", n);
	fz_strlcat(path, rel_path, n);
	return fz_cleanname(fz_urldecode(path));
}

static fz_outline *
epub_parse_ncx_imp(fz_context *ctx, epub_document *doc, fz_xml *node, char *base_uri)
{
	char path[2048];
	fz_outline *outline, *head, **tailp;

	head = NULL;
	tailp = &head;

	node = fz_xml_find_down(node, "navPoint");
	while (node)
	{
		char *text = fz_xml_text(fz_xml_down(fz_xml_find_down(fz_xml_find_down(node, "navLabel"), "text")));
		char *content = fz_xml_att(fz_xml_find_down(node, "content"), "src");
		if (text && content)
		{
			fz_strlcpy(path, base_uri, sizeof path);
			fz_strlcat(path, "/", sizeof path);
			fz_strlcat(path, content, sizeof path);
			fz_urldecode(path);
			fz_cleanname(path);

			fz_try(ctx)
			{
				*tailp = outline = fz_new_outline(ctx);
				tailp = &(*tailp)->next;
				outline->title = Memento_label(fz_strdup(ctx, text), "outline_title");
				outline->uri = Memento_label(fz_strdup(ctx, path), "outline_uri");
				outline->page = fz_make_location(-1, -1);
				outline->down = epub_parse_ncx_imp(ctx, doc, node, base_uri);
				outline->is_open = 1;
			}
			fz_catch(ctx)
			{
				fz_drop_outline(ctx, head);
				fz_rethrow(ctx);
			}
		}
		node = fz_xml_find_next(node, "navPoint");
	}

	return head;
}

static void
epub_parse_ncx(fz_context *ctx, epub_document *doc, const char *path)
{
	fz_archive *zip = doc->zip;
	fz_buffer *buf = NULL;
	fz_xml_doc *ncx = NULL;
	char base_uri[2048];

	fz_var(buf);
	fz_var(ncx);

	fz_try(ctx)
	{
		fz_dirname(base_uri, path, sizeof base_uri);
		buf = fz_read_archive_entry(ctx, zip, path);
		ncx = fz_parse_xml(ctx, buf, 0);
		doc->outline = epub_parse_ncx_imp(ctx, doc, fz_xml_find_down(fz_xml_root(ncx), "navMap"), base_uri);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_drop_xml(ctx, ncx);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static char *
find_metadata(fz_context *ctx, fz_xml *metadata, char *key)
{
	char *text = fz_xml_text(fz_xml_down(fz_xml_find_down(metadata, key)));
	if (text)
		return fz_strdup(ctx, text);
	return NULL;
}

static fz_buffer *
read_container_and_prefix(fz_context *ctx, fz_archive *zip, char *prefix, size_t prefix_len)
{
	int n = fz_count_archive_entries(ctx, zip);
	int i;

	prefix[0] = 0;

	/* First off, look for the container.xml at the top level. */
	for (i = 0; i < n; i++)
	{
		const char *p = fz_list_archive_entry(ctx, zip, i);

		if (!strcmp(p, "META-INF/container.xml"))
			return fz_read_archive_entry(ctx, zip, "META-INF/container.xml");
	}

	/* If that failed, look for the first such file in a subdirectory. */
	for (i = 0; i < n; i++)
	{
		const char *p = fz_list_archive_entry(ctx, zip, i);
		size_t z = strlen(p);
		size_t z0 = sizeof("META-INF/container.xml")-1;

		if (z < z0)
			continue;
		if (!strcmp(p + z - z0, "META-INF/container.xml"))
		{
			if (z - z0 >= prefix_len)
			{
				fz_warn(ctx, "Ignoring %s as path too long.", p);
				continue;
			}
			memcpy(prefix, p, z-z0);
			prefix[z-z0] = 0;
			return fz_read_archive_entry(ctx, zip, p);
		}
	}

	return fz_read_archive_entry(ctx, zip, "META-INF/container.xml");
}

static void
epub_parse_header(fz_context *ctx, epub_document *doc)
{
	fz_archive *zip = doc->zip;
	fz_buffer *buf = NULL;
	fz_xml_doc *encryption_xml = NULL;
	fz_xml_doc *container_xml = NULL;
	fz_xml_doc *content_opf = NULL;
	fz_xml *container, *rootfiles, *rootfile;
	fz_xml *package, *manifest, *spine, *itemref, *metadata;
	char base_uri[2048];
	const char *full_path;
	const char *version;
	char ncx[2048], s[2048];
	char *prefixed_full_path = NULL;
	size_t prefix_len;
	epub_chapter **tailp;
	int i;

	fz_var(buf);
	fz_var(encryption_xml);
	fz_var(container_xml);
	fz_var(content_opf);
	fz_var(prefixed_full_path);

	fz_try(ctx)
	{
		/* parse META-INF/encryption.xml to figure out which entries are encrypted */

		/* parse META-INF/container.xml to find OPF */
		/* Reuse base_uri to read the prefix. */
		buf = read_container_and_prefix(ctx, zip, base_uri, sizeof(base_uri));
		container_xml = fz_parse_xml(ctx, buf, 0);
		fz_drop_buffer(ctx, buf);
		buf = NULL;

		/* Some epub files can be prefixed by a directory name. This (normally
		 * empty!) will be in base_uri. */
		prefix_len = strlen(base_uri);
		{
			/* Further abuse base_uri to hold a temporary name. */
			const size_t z0 = sizeof("META-INF/encryption.xml")-1;
			if (sizeof(base_uri) <= prefix_len + z0)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Prefix too long in epub");
			strcpy(base_uri + prefix_len, "META-INF/encryption.xml");
			if (fz_has_archive_entry(ctx, zip, base_uri))
			{
				fz_warn(ctx, "EPUB may be locked by DRM");

				buf = fz_read_archive_entry(ctx, zip, base_uri);
				encryption_xml = fz_parse_xml(ctx, buf, 0);
				fz_drop_buffer(ctx, buf);
				buf = NULL;

				epub_parse_encryption(ctx, doc, fz_xml_find(fz_xml_root(encryption_xml), "encryption"));
				zip = doc->zip;
			}
		}

		container = fz_xml_find(fz_xml_root(container_xml), "container");
		rootfiles = fz_xml_find_down(container, "rootfiles");
		rootfile = fz_xml_find_down(rootfiles, "rootfile");
		full_path = fz_xml_att(rootfile, "full-path");
		if (!full_path)
			fz_throw(ctx, FZ_ERROR_FORMAT, "cannot find root file in EPUB");

		fz_dirname(base_uri+prefix_len, full_path, sizeof(base_uri) - prefix_len);

		prefixed_full_path = fz_malloc(ctx, strlen(full_path) + prefix_len + 1);
		memcpy(prefixed_full_path, base_uri, prefix_len);
		strcpy(prefixed_full_path + prefix_len, full_path);

		/* parse OPF to find NCX and spine */

		buf = fz_read_archive_entry(ctx, zip, prefixed_full_path);
		content_opf = fz_parse_xml(ctx, buf, 0);
		fz_drop_buffer(ctx, buf);
		buf = NULL;

		package = fz_xml_find(fz_xml_root(content_opf), "package");
		version = fz_xml_att(package, "version");
		if (!version || strcmp(version, "2.0"))
			fz_warn(ctx, "unknown epub version: %s", version ? version : "<none>");

		metadata = fz_xml_find_down(package, "metadata");
		if (metadata)
		{
			doc->dc_title = Memento_label(find_metadata(ctx, metadata, "title"), "epub_title");
			doc->dc_creator = Memento_label(find_metadata(ctx, metadata, "creator"), "epub_creator");
		}

		manifest = fz_xml_find_down(package, "manifest");
		spine = fz_xml_find_down(package, "spine");

		if (path_from_idref(ncx, manifest, base_uri, fz_xml_att(spine, "toc"), sizeof ncx))
		{
			epub_parse_ncx(ctx, doc, ncx);
		}

		doc->spine = NULL;
		tailp = &doc->spine;
		itemref = fz_xml_find_down(spine, "itemref");
		i = 0;
		while (itemref)
		{
			if (path_from_idref(s, manifest, base_uri, fz_xml_att(itemref, "idref"), sizeof s))
			{
				fz_try(ctx)
				{
					*tailp = epub_load_chapter(ctx, doc, s, i);
					tailp = &(*tailp)->next;
					i++;
				}
				fz_catch(ctx)
				{
					fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
					fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
					fz_report_error(ctx);
					fz_warn(ctx, "ignoring chapter %s", s);
				}
			}
			itemref = fz_xml_find_next(itemref, "itemref");
		}
	}
	fz_always(ctx)
	{
		fz_drop_xml(ctx, content_opf);
		fz_drop_xml(ctx, container_xml);
		fz_drop_xml(ctx, encryption_xml);
		fz_drop_buffer(ctx, buf);
		fz_free(ctx, prefixed_full_path);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static fz_outline *
epub_load_outline(fz_context *ctx, fz_document *doc_)
{
	epub_document *doc = (epub_document*)doc_;
	return fz_keep_outline(ctx, doc->outline);
}

static int
epub_lookup_metadata(fz_context *ctx, fz_document *doc_, const char *key, char *buf, size_t size)
{
	epub_document *doc = (epub_document*)doc_;
	if (!strcmp(key, FZ_META_FORMAT))
		return 1 + (int)fz_strlcpy(buf, "EPUB", size);
	if (!strcmp(key, FZ_META_INFO_TITLE) && doc->dc_title)
		return 1 + (int)fz_strlcpy(buf, doc->dc_title, size);
	if (!strcmp(key, FZ_META_INFO_AUTHOR) && doc->dc_creator)
		return 1 + (int)fz_strlcpy(buf, doc->dc_creator, size);
	return -1;
}

static void
epub_output_accelerator(fz_context *ctx, fz_document *doc_, fz_output *out)
{
	epub_document *doc = (epub_document*)doc_;
	int i;

	fz_try(ctx)
	{
		if (doc->accel == NULL)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "No accelerator data to write");

		fz_write_int32_le(ctx, out, MAGIC_ACCELERATOR);
		fz_write_int32_le(ctx, out, MAGIC_ACCEL_EPUB);
		fz_write_int32_le(ctx, out, ACCEL_VERSION);
		fz_write_float_le(ctx, out, doc->accel->layout_w);
		fz_write_float_le(ctx, out, doc->accel->layout_h);
		fz_write_float_le(ctx, out, doc->accel->layout_em);
		fz_write_uint32_le(ctx, out, doc->accel->css_sum);
		fz_write_int32_le(ctx, out, doc->accel->use_doc_css);
		fz_write_int32_le(ctx, out, doc->accel->num_chapters);
		for (i = 0; i < doc->accel->num_chapters; i++)
			fz_write_int32_le(ctx, out, doc->accel->pages_in_chapter[i]);

		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static fz_document *
epub_init(fz_context *ctx, fz_archive *zip, fz_stream *accel)
{
	epub_document *doc = NULL;

	fz_var(doc);
	fz_var(zip);

	fz_try(ctx)
	{
		doc = fz_new_derived_document(ctx, epub_document);
		doc->zip = zip;
		zip = NULL;

		doc->super.drop_document = epub_drop_document;
		doc->super.layout = epub_layout;
		doc->super.load_outline = epub_load_outline;
		doc->super.resolve_link_dest = epub_resolve_link;
		doc->super.make_bookmark = epub_make_bookmark;
		doc->super.lookup_bookmark = epub_lookup_bookmark;
		doc->super.count_chapters = epub_count_chapters;
		doc->super.count_pages = epub_count_pages;
		doc->super.load_page = epub_load_page;
		doc->super.page_label = epub_page_label;
		doc->super.lookup_metadata = epub_lookup_metadata;
		doc->super.output_accelerator = epub_output_accelerator;
		doc->super.is_reflowable = 1;

		doc->set = fz_new_html_font_set(ctx);
		doc->css_sum = user_css_sum(ctx);
		epub_load_accelerator(ctx, doc, accel);
		epub_parse_header(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_drop_archive(ctx, zip);
		fz_drop_document(ctx, &doc->super);
		fz_rethrow(ctx);
	}

	return (fz_document*)doc;
}

static fz_document *
epub_open_accel_document_with_stream(fz_context *ctx, fz_stream *file, fz_stream *accel)
{
	return epub_init(ctx, fz_open_zip_archive_with_stream(ctx, file), accel);
}

static fz_document *
epub_open_accel_document(fz_context *ctx, const char *filename, const char *accel)
{
	fz_stream *afile = NULL;
	fz_document *doc;

	if (accel)
		afile = fz_open_file(ctx, accel);

	fz_try(ctx)
	{
		if (strstr(filename, "META-INF/container.xml") || strstr(filename, "META-INF\\container.xml"))
		{
			char dirname[2048], *p;
			fz_strlcpy(dirname, filename, sizeof dirname);
			p = strstr(dirname, "META-INF");
			*p = 0;
			if (!dirname[0])
				fz_strlcpy(dirname, ".", sizeof dirname);
			doc = epub_init(ctx, fz_open_directory(ctx, dirname), afile);
		}
		else
			doc = epub_init(ctx, fz_open_zip_archive(ctx, filename), afile);
	}
	fz_always(ctx)
		fz_drop_stream(ctx, afile);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return doc;
}

static fz_document *
epub_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	return epub_init(ctx, fz_open_zip_archive_with_stream(ctx, file), NULL);
}

static fz_document *
epub_open_document(fz_context *ctx, const char *filename)
{
	return epub_open_accel_document(ctx, filename, NULL);
}

static int
epub_recognize(fz_context *doc, const char *magic)
{
	if (strstr(magic, "META-INF/container.xml") || strstr(magic, "META-INF\\container.xml"))
		return 200;
	return 0;
}

static const char *epub_extensions[] =
{
	"epub",
	NULL
};

static const char *epub_mimetypes[] =
{
	"application/epub+zip",
	NULL
};

fz_document_handler epub_document_handler =
{
	epub_recognize,
	epub_open_document,
	epub_open_document_with_stream,
	epub_extensions,
	epub_mimetypes,
	epub_open_accel_document,
	epub_open_accel_document_with_stream
};
