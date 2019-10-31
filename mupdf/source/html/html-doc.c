#include "mupdf/fitz.h"
#include "html-imp.h"

#include <string.h>
#include <math.h>

enum { T, R, B, L };

typedef struct html_document_s html_document;
typedef struct html_page_s html_page;

struct html_document_s
{
	fz_document super;
	fz_archive *zip;
	fz_html_font_set *set;
	fz_html *html;
	fz_outline *outline;
};

struct html_page_s
{
	fz_page super;
	html_document *doc;
	int number;
};

static void
htdoc_drop_document(fz_context *ctx, fz_document *doc_)
{
	html_document *doc = (html_document*)doc_;
	fz_drop_archive(ctx, doc->zip);
	fz_drop_html(ctx, doc->html);
	fz_drop_html_font_set(ctx, doc->set);
	fz_drop_outline(ctx, doc->outline);
}

static fz_location
htdoc_resolve_link(fz_context *ctx, fz_document *doc_, const char *dest, float *xp, float *yp)
{
	html_document *doc = (html_document*)doc_;
	const char *s = strchr(dest, '#');
	if (s && s[1] != 0)
	{
		float y = fz_find_html_target(ctx, doc->html, s+1);
		if (y >= 0)
		{
			int page = y / doc->html->page_h;
			if (yp) *yp = y - page * doc->html->page_h;
			return fz_make_location(0, page);
		}
	}

	return fz_make_location(-1, -1);
}

static int
htdoc_count_pages(fz_context *ctx, fz_document *doc_, int chapter)
{
	html_document *doc = (html_document*)doc_;
	if (doc->html->root->b > 0)
		return ceilf(doc->html->root->b / doc->html->page_h);
	return 1;
}

static void
htdoc_update_outline(fz_context *ctx, fz_document *doc, fz_outline *node)
{
	while (node)
	{
		node->page = htdoc_resolve_link(ctx, doc, node->uri, &node->x, &node->y).page;
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
htdoc_bound_page(fz_context *ctx, fz_page *page_)
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
	return fz_load_html_links(ctx, doc->html, page->number, "", doc);
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
	html_page *page = fz_new_derived_page(ctx, html_page);
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
htdoc_lookup_metadata(fz_context *ctx, fz_document *doc_, const char *key, char *buf, int size)
{
	html_document *doc = (html_document*)doc_;
	if (!strcmp(key, FZ_META_FORMAT))
		return (int)fz_strlcpy(buf, "XHTML", size);
	if (!strcmp(key, FZ_META_INFO_TITLE) && doc->html->title)
		return (int)fz_strlcpy(buf, doc->html->title, size);
	return -1;
}

static fz_document *
htdoc_open_document_with_buffer(fz_context *ctx, const char *dirname, fz_buffer *buf)
{
	html_document *doc = fz_new_derived_document(ctx, html_document);
	doc->super.drop_document = htdoc_drop_document;
	doc->super.layout = htdoc_layout;
	doc->super.load_outline = htdoc_load_outline;
	doc->super.resolve_link = htdoc_resolve_link;
	doc->super.make_bookmark = htdoc_make_bookmark;
	doc->super.lookup_bookmark = htdoc_lookup_bookmark;
	doc->super.count_pages = htdoc_count_pages;
	doc->super.load_page = htdoc_load_page;
	doc->super.lookup_metadata = htdoc_lookup_metadata;
	doc->super.is_reflowable = 1;

	fz_try(ctx)
	{
		doc->zip = fz_open_directory(ctx, dirname);
		doc->set = fz_new_html_font_set(ctx);
		doc->html = fz_parse_html(ctx, doc->set, doc->zip, ".", buf, fz_user_css(ctx));
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

static fz_document *
htdoc_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	return htdoc_open_document_with_buffer(ctx, ".", fz_read_all(ctx, file, 0));
}

static fz_document *
htdoc_open_document(fz_context *ctx, const char *filename)
{
	char dirname[2048];
	fz_dirname(dirname, filename, sizeof dirname);
	return htdoc_open_document_with_buffer(ctx, dirname, fz_read_file(ctx, filename));
}

static const char *htdoc_extensions[] =
{
	"fb2",
	"htm",
	"html",
	"xhtml",
	"xml",
	NULL
};

static const char *htdoc_mimetypes[] =
{
	"application/html+xml",
	"application/x-fictionbook",
	"application/xml",
	"text/xml",
	NULL
};

fz_document_handler html_document_handler =
{
	NULL,
	htdoc_open_document,
	htdoc_open_document_with_stream,
	htdoc_extensions,
	htdoc_mimetypes
};
