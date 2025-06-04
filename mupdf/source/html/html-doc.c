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
#include "html-imp.h"

#include <string.h>
#include <math.h>

enum { T, R, B, L };

typedef struct
{
	fz_document super;
	fz_archive *zip;
	fz_html_font_set *set;
	fz_html *html;
	fz_outline *outline;
	const fz_htdoc_format_t *format;
} html_document;

typedef struct
{
	fz_page super;
	html_document *doc;
	int number;
} html_page;

static void
htdoc_drop_document(fz_context *ctx, fz_document *doc_)
{
	html_document *doc = (html_document*)doc_;
	fz_drop_archive(ctx, doc->zip);
	fz_drop_html(ctx, doc->html);
	fz_drop_html_font_set(ctx, doc->set);
	fz_drop_outline(ctx, doc->outline);
}

static fz_link_dest
htdoc_resolve_link(fz_context *ctx, fz_document *doc_, const char *dest)
{
	html_document *doc = (html_document*)doc_;
	const char *s = strchr(dest, '#');
	if (s && s[1] != 0)
	{
		float y = fz_find_html_target(ctx, doc->html, s+1);
		if (y >= 0)
		{
			int page = y / doc->html->page_h;
			return fz_make_link_dest_xyz(0, page, 0, y - page * doc->html->page_h, 0);
		}
	}

	return fz_make_link_dest_none();
}

static int
htdoc_count_pages(fz_context *ctx, fz_document *doc_, int chapter)
{
	html_document *doc = (html_document*)doc_;
	if (doc->html->tree.root->s.layout.b > 0)
		return ceilf(doc->html->tree.root->s.layout.b / doc->html->page_h);
	return 1;
}

static void
htdoc_update_outline(fz_context *ctx, fz_document *doc, fz_outline *node)
{
	while (node)
	{
		fz_link_dest dest = htdoc_resolve_link(ctx, doc, node->uri);
		node->page = dest.loc;
		node->x = dest.x;
		node->y = dest.y;
		htdoc_update_outline(ctx, doc, node->down);
		node = node->next;
	}
}

static void
htdoc_layout(fz_context *ctx, fz_document *doc_, float w, float h, float em)
{
	html_document *doc = (html_document*)doc_;

	fz_layout_html(ctx, doc->html, w, h, em);

	htdoc_update_outline(ctx, doc_, doc->outline);
}

static void
htdoc_drop_page(fz_context *ctx, fz_page *page_)
{
}

static fz_rect
htdoc_bound_page(fz_context *ctx, fz_page *page_, fz_box_type box)
{
	html_page *page = (html_page*)page_;
	html_document *doc = page->doc;
	fz_rect bbox;
	bbox.x0 = 0;
	bbox.y0 = 0;
	bbox.x1 = doc->html->page_w + doc->html->page_margin[L] + doc->html->page_margin[R];
	bbox.y1 = doc->html->page_h + doc->html->page_margin[T] + doc->html->page_margin[B];
	return bbox;
}

static void
htdoc_run_page(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	html_page *page = (html_page*)page_;
	html_document *doc = page->doc;
	fz_draw_html(ctx, dev, ctm, doc->html, page->number);
}

static fz_link *
htdoc_load_links(fz_context *ctx, fz_page *page_)
{
	html_page *page = (html_page*)page_;
	html_document *doc = page->doc;
	return fz_load_html_links(ctx, doc->html, page->number, "");
}

static fz_bookmark
htdoc_make_bookmark(fz_context *ctx, fz_document *doc_, fz_location loc)
{
	html_document *doc = (html_document*)doc_;
	return fz_make_html_bookmark(ctx, doc->html, loc.page);
}

static fz_location
htdoc_lookup_bookmark(fz_context *ctx, fz_document *doc_, fz_bookmark mark)
{
	html_document *doc = (html_document*)doc_;
	return fz_make_location(0, fz_lookup_html_bookmark(ctx, doc->html, mark));
}

static fz_page *
htdoc_load_page(fz_context *ctx, fz_document *doc_, int chapter, int number)
{
	html_document *doc = (html_document*)doc_;
	html_page *page = fz_new_derived_page(ctx, html_page, doc_);
	page->super.bound_page = htdoc_bound_page;
	page->super.run_page_contents = htdoc_run_page;
	page->super.load_links = htdoc_load_links;
	page->super.drop_page = htdoc_drop_page;
	page->doc = doc;
	page->number = number;
	return (fz_page*)page;
}

static fz_outline *
htdoc_load_outline(fz_context *ctx, fz_document *doc_)
{
	html_document *doc = (html_document*)doc_;
	return fz_keep_outline(ctx, doc->outline);
}

static int
htdoc_lookup_metadata(fz_context *ctx, fz_document *doc_, const char *key, char *buf, size_t size)
{
	html_document *doc = (html_document *)doc_;
	if (!strcmp(key, FZ_META_FORMAT))
		return 1 + (int)fz_strlcpy(buf, doc->format->format_name, size);
	if (!strcmp(key, FZ_META_INFO_TITLE) && doc->html->title)
		return 1 + (int)fz_strlcpy(buf, doc->html->title, size);
	return -1;
}

static fz_html *
generic_parse(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_buffer *buffer_in, const char *user_css, const fz_htdoc_format_t *format)
{
	fz_buffer *buffer_html = NULL;
	fz_html *html = NULL;

	fz_try(ctx)
	{
		if (format->convert_to_html)
			buffer_html = format->convert_to_html(ctx, set, buffer_in, zip, user_css);
		else
			buffer_html = fz_keep_buffer(ctx, buffer_in);
		html = fz_parse_html(ctx, set, zip, base_uri, buffer_html, user_css, format->try_xml, format->try_html5, format->patch_mobi);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buffer_html);
	}
	fz_catch(ctx)
	{
		fz_drop_html(ctx, html);
		fz_rethrow(ctx);
	}
	return html;
}

fz_document *
fz_htdoc_open_document_with_buffer(fz_context *ctx, fz_archive *dir, fz_buffer *buf, const fz_htdoc_format_t *format)
{
	html_document *doc = NULL;

	fz_var(doc);
	fz_var(dir);

	fz_try(ctx)
	{
		doc = fz_new_derived_document(ctx, html_document);
		doc->super.drop_document = htdoc_drop_document;
		doc->super.layout = htdoc_layout;
		doc->super.load_outline = htdoc_load_outline;
		doc->super.resolve_link_dest = htdoc_resolve_link;
		doc->super.make_bookmark = htdoc_make_bookmark;
		doc->super.lookup_bookmark = htdoc_lookup_bookmark;
		doc->super.count_pages = htdoc_count_pages;
		doc->super.load_page = htdoc_load_page;
		doc->super.lookup_metadata = htdoc_lookup_metadata;
		doc->super.is_reflowable = 1;

		doc->zip = fz_keep_archive(ctx, dir);
		doc->format = format;
		doc->set = fz_new_html_font_set(ctx);
		doc->html = generic_parse(ctx, doc->set, doc->zip, ".", buf, fz_user_css(ctx), format);
		doc->outline = fz_load_html_outline(ctx, doc->html);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
	{
		fz_drop_document(ctx, &doc->super);
		fz_rethrow(ctx);
	}

	return (fz_document*)doc;
}

fz_document *
fz_htdoc_open_document_with_stream_and_dir(fz_context *ctx, fz_stream *stm, fz_archive *dir, const fz_htdoc_format_t *format)
{
	fz_buffer *buf = NULL;

	if (stm)
		buf = fz_read_all(ctx, stm, 0);

	return fz_htdoc_open_document_with_buffer(ctx, dir, buf, format);
}

/* Variant specific functions */

/* Generic HTML document handler */

static int isws(int c)
{
	return c == 32 || c == 9 || c == 10 || c == 13 || c == 12;
}

static int recognize_html_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stream, fz_archive *dir, void **hstate, fz_document_recognize_state_free_fn **free_state, int xhtml)
{
	uint8_t buffer[4096];
	size_t i, n, m;
	enum {
		state_top,
		state_open,
		state_pling,
		state_query,
		state_maybe_doctype,
		state_maybe_doctype_ws,
		state_maybe_doctype_html,
		state_maybe_doctype_html_xhtml,
		state_maybe_comment,
		state_maybe_html,
		state_maybe_html_xhtml,
		state_comment
	};
	int state = state_top;
	int type = 0;

	if (hstate)
		*hstate = NULL;
	if (free_state)
		*free_state = NULL;

	if (stream == NULL)
		return 0;

	/* Simple state machine. Search for "<!doctype html" or "<html" in the first
	 * 4K of the file, allowing for comments and whitespace and case insensitivity. */

	n = fz_read(ctx, stream, buffer, sizeof(buffer));
	fz_seek(ctx, stream, 0, SEEK_SET);
	if (n == 0)
		return 0;

	i = 0;
	if (n >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
	{
		/* UTF-8 encoded BOM. Just skip it. */
		i = 3;
	}
	else if (n >= 2 && buffer[0] == 0xFE && buffer[1] == 0xFF)
	{
		/* UTF-16, big endian. */
		type = 1;
		i = 2;
		n &= ~1;
	}
	else if (n >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE)
	{
		/* UTF-16, little endian. */
		i = 2;
		type = 2;
		n &= ~1;
	}

	while (i < n)
	{
		int c;

		switch (type)
		{
		case 0: /* UTF-8 */
			c = buffer[i++];
			break;
		case 1: /* UTF-16 - big endian */
			c = buffer[i++] << 8;
			c |= buffer[i++];
			break;
		case 2: /* UTF-16 - little endian */
			c = buffer[i++];
			c |= buffer[i++] << 8;
			break;
		}

		switch (state)
		{
		case state_top:
			if (isws(c))
				continue; /* whitespace */
			if (c == '<')
				state = state_open;
			else
				return 0; /* Non whitespace found at the top level prior to a known tag. Fail. */
			break;
		case state_open:
			if (isws(c))
				continue; /* whitespace */
			if (c == '!')
				state = state_pling;
			else if (c == '?')
				state = state_query;
			else if (c == 'h' || c == 'H')
				state = state_maybe_html;
			else
				return 0; /* Not an acceptable opening tag. */
			m = 0;
			break;
		case state_query:
			if (c == '>')
				state = state_top;
			break;
		case state_pling:
			if (isws(c))
				continue; /* whitespace */
			else if (c == '-')
				state = state_maybe_comment;
			else if (c == 'd' || c == 'D')
				state = state_maybe_doctype;
			else
				return 0; /* Not an acceptable opening tag. */
			break;
		case state_maybe_comment:
			if (c == '-')
				state = state_comment;
			else
				return 0; /* Not an acceptable opening tag. */
			break;
		case state_comment:
			if (c == '-')
			{
				m++;
			}
			else if (c == '>' && m >= 2)
			{
				state = state_top;
			}
			else
				m = 0;
			break;
		case state_maybe_doctype:
			if (c == "octype"[m] || c == "OCTYPE"[m])
			{
				m++;
				if (m == 6)
				{
					state = state_maybe_doctype_ws;
					m = 0;
				}
			}
			else
				return 0; /* Not an acceptable opening tag. */
			break;
		case state_maybe_doctype_ws:
			if (isws(c))
				m++;
			else if (m > 0 && (c == 'h' || c == 'H'))
			{
				state = state_maybe_doctype_html;
				m = 0;
			}
			else
				return 0; /* Not an acceptable opening tag. */
			break;
		case state_maybe_doctype_html:
			if (c == "tml"[m] || c == "TML"[m])
			{
				m++;
				if (m == 3)
				{
					state = state_maybe_doctype_html_xhtml;
					m = 0;
				}
			}
			else
				return 0; /* Not an acceptable opening tag. */
			break;
		case state_maybe_doctype_html_xhtml:
			if (c == '>')
			{
				/* Not xhtml - the xhtml agent can handle this at a pinch (so 25),
				 * but we'd rather the html one did (75). */
				return xhtml ? 25 : 75;
			}
			if (c >= 'A'  && c <= 'Z')
				c += 'a'-'A';
			if (c == "xhtml"[m])
			{
				m++;
				if (m == 5)
				{
					/* xhtml - the xhtml agent would be better (75) than the html
					 * agent (25). */
					return xhtml ? 75 : 25;
				}
			}
			else
				m = 0;
			break;
		case state_maybe_html:
			if (c == "tml"[m] || c == "TML"[m])
			{
				m++;
				if (m == 3)
				{
					state = state_maybe_html_xhtml;
					m = 0;
				}
			}
			else
				return 0; /* Not an acceptable opening tag. */
			break;
		case state_maybe_html_xhtml:
			if (c == '>')
			{
				/* Not xhtml - the xhtml agent can handle this at a pinch (so 25),
				 * but we'd rather the html one did (75). */
				return xhtml ? 25 : 75;
			}
			if (c >= 'A'  && c <= 'Z')
				c += 'a'-'A';
			if (c == "xhtml"[m])
			{
				m++;
				if (m == 5)
				{
					/* xhtml - the xhtml agent would be better (75) than the html
					 * agent (25). */
					return xhtml ? 75 : 25;
				}
			}
			else
				m = 0;
			break;
		}
	}

	return 0;
}

int htdoc_recognize_html_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stream, fz_archive *dir, void **hstate, fz_document_recognize_state_free_fn **free_state)
{
	return recognize_html_content(ctx, handler, stream, dir, hstate, free_state, 0);
}

static const fz_htdoc_format_t fz_htdoc_html5 =
{
	"HTML5",
	NULL,
	0, 1, 0
};

static fz_document *
htdoc_open_document(fz_context *ctx, const fz_document_handler *handler, fz_stream *file, fz_stream *accel, fz_archive *dir, void *state)
{
	return fz_htdoc_open_document_with_stream_and_dir(ctx, file, dir, &fz_htdoc_html5);
}

static const char *htdoc_extensions[] =
{
	"htm",
	"html",
	NULL
};

static const char *htdoc_mimetypes[] =
{
	"text/html",
	NULL
};

fz_document_handler html_document_handler =
{
	NULL,
	htdoc_open_document,
	htdoc_extensions,
	htdoc_mimetypes,
	htdoc_recognize_html_content,
	1
};

/* XHTML document handler */

static const fz_htdoc_format_t fz_htdoc_xhtml =
{
	"XHTML",
	NULL,
	1, 1, 0
};

static fz_document *
xhtdoc_open_document(fz_context *ctx, const fz_document_handler *handler, fz_stream *file, fz_stream *accel, fz_archive *dir, void *state)
{
	return fz_htdoc_open_document_with_stream_and_dir(ctx, file, dir, &fz_htdoc_xhtml);
}

int xhtdoc_recognize_xhtml_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stream, fz_archive *dir, void **hstate, fz_document_recognize_state_free_fn **free_state)
{
	return recognize_html_content(ctx, handler, stream, dir, hstate, free_state, 1);
}

static const char *xhtdoc_extensions[] =
{
	"xhtml",
	NULL
};

static const char *xhtdoc_mimetypes[] =
{
	"application/xhtml+xml",
	NULL
};

fz_document_handler xhtml_document_handler =
{
	NULL,
	xhtdoc_open_document,
	xhtdoc_extensions,
	xhtdoc_mimetypes,
	xhtdoc_recognize_xhtml_content,
	1
};

/* FB2 document handler */

static const fz_htdoc_format_t fz_htdoc_fb2 =
{
	"FictionBook2",
	NULL,
	1, 0, 0
};

static fz_document *
fb2doc_open_document(fz_context *ctx, const fz_document_handler *handler, fz_stream *file, fz_stream *accel, fz_archive *dir, void *state)
{
	return fz_htdoc_open_document_with_stream_and_dir(ctx, file, dir, &fz_htdoc_fb2);
}

static int
fb2doc_recognize_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stream, fz_archive *dir, void **state, fz_document_recognize_state_free_fn **free_state)
{
	const char *match = "<FictionBook";
	int pos = 0;
	int n = 4096;
	int c;

	if (state)
		*state = NULL;
	if (free_state)
		*free_state = NULL;

	if (stream == NULL)
		return 0;

	do
	{
		c = fz_read_byte(ctx, stream);
		if (c == EOF)
			return 0;
		if (c == match[pos])
		{
			pos++;
			if (pos == 12)
				return 100;
		}
		else
		{
			/* Restart matching, but recheck c against the start. */
			pos = (c == match[0]);
		}
	}
	while (--n > 0);

	return 0;
}

static const char *fb2doc_extensions[] =
{
	"fb2",
	"xml",
	NULL
};

static const char *fb2doc_mimetypes[] =
{
	"application/x-fictionbook",
	"application/xml",
	"text/xml",
	NULL
};

fz_document_handler fb2_document_handler =
{
	NULL,
	fb2doc_open_document,
	fb2doc_extensions,
	fb2doc_mimetypes,
	fb2doc_recognize_content
};

/* Mobi document handler */

static const fz_htdoc_format_t fz_htdoc_mobi =
{
	"MOBI",
	NULL,
	1, 1, 1
};

static fz_document *
mobi_open_document_with_buffer(fz_context *ctx, fz_buffer *mobi)
{
	fz_archive *dir = NULL;
	fz_buffer *html;
	fz_document *doc;
	fz_var(dir);
	fz_try(ctx)
	{
		dir = fz_extract_html_from_mobi(ctx, mobi);
		html = fz_read_archive_entry(ctx, dir, "index.html");
		doc = fz_htdoc_open_document_with_buffer(ctx, dir, html, &fz_htdoc_mobi);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, mobi);
		fz_drop_archive(ctx, dir);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
	return doc;
}

static int
mobi_recognize_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stream, fz_archive *dir, void **state, fz_document_recognize_state_free_fn **free_state)
{
	char text[8];

	if (state)
		*state = NULL;
	if (free_state)
		*free_state = NULL;

	if (stream == NULL)
		return 0;

	fz_seek(ctx, stream, 32 + 28, SEEK_SET);
	if (fz_read(ctx, stream, (unsigned char *)text, 8) != 8)
		return 0;
	if (memcmp(text, "BOOKMOBI", 8) == 0)
		return 100;
	if (memcmp(text, "TEXtREAd", 8) == 0)
		return 100;

	return 0;
}

static fz_document *
mobi_open_document(fz_context *ctx, const fz_document_handler *handler, fz_stream *file, fz_stream *accel, fz_archive *dir, void *state)
{
	return mobi_open_document_with_buffer(ctx, fz_read_all(ctx, file, 0));
}

static const char *mobi_extensions[] =
{
	"mobi",
	"prc",
	"pdb",
	NULL
};

static const char *mobi_mimetypes[] =
{
	"application/x-mobipocket-ebook",
	NULL
};

fz_document_handler mobi_document_handler =
{
	NULL,
	mobi_open_document,
	mobi_extensions,
	mobi_mimetypes,
	mobi_recognize_content
};
