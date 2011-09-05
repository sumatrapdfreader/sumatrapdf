#include "fitz.h"
#include "mupdf.h"
#include "muxps.h"
#include "pdfapp.h"

#include <ctype.h> /* for tolower() */

#define ZOOMSTEP 1.142857
#define BEYOND_THRESHHOLD 40

enum panning
{
	DONT_PAN = 0,
	PAN_TO_TOP,
	PAN_TO_BOTTOM
};

static void pdfapp_showpage(pdfapp_t *app, int loadpage, int drawpage, int repaint);

static void pdfapp_warn(pdfapp_t *app, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	winwarn(app, buf);
}

static void pdfapp_error(pdfapp_t *app, fz_error error)
{
	winerror(app, error);
}

char *pdfapp_version(pdfapp_t *app)
{
	return
		"MuPDF 0.9\n"
		"Copyright 2006-2011 Artifex Sofware, Inc.\n";
}

char *pdfapp_usage(pdfapp_t *app)
{
	return
		"L\t\t-- rotate left\n"
		"R\t\t-- rotate right\n"
		"h\t\t-- scroll left\n"
		"j down\t\t-- scroll down\n"
		"k up\t\t-- scroll up\n"
		"l\t\t-- scroll right\n"
		"+\t\t-- zoom in\n"
		"-\t\t-- zoom out\n"
		"w\t\t-- shrinkwrap\n"
		"r\t\t-- reload file\n"
		". pgdn right space\t-- next page\n"
		", pgup left b\t-- previous page\n"
		">\t\t-- next 10 pages\n"
		"<\t\t-- back 10 pages\n"
		"m\t\t-- mark page for snap back\n"
		"t\t\t-- pop back to latest mark\n"
		"1m\t\t-- mark page in register 1\n"
		"1t\t\t-- go to page in register 1\n"
		"123g\t\t-- go to page 123\n"
		"/\t\t-- search for text\n"
		"n\t\t-- find next search result\n"
		"N\t\t-- find previous search result\n"
		"c\t\t-- toggle between color and grayscale\n"
	;
}

void pdfapp_init(pdfapp_t *app)
{
	memset(app, 0, sizeof(pdfapp_t));
	app->scrw = 640;
	app->scrh = 480;
	app->resolution = 72;
}

void pdfapp_invert(pdfapp_t *app, fz_bbox rect)
{
	unsigned char *p;
	int x, y, n;

	int x0 = CLAMP(rect.x0 - app->image->x, 0, app->image->w - 1);
	int x1 = CLAMP(rect.x1 - app->image->x, 0, app->image->w - 1);
	int y0 = CLAMP(rect.y0 - app->image->y, 0, app->image->h - 1);
	int y1 = CLAMP(rect.y1 - app->image->y, 0, app->image->h - 1);

	for (y = y0; y < y1; y++)
	{
		p = app->image->samples + (y * app->image->w + x0) * app->image->n;
		for (x = x0; x < x1; x++)
		{
			for (n = app->image->n; n > 0; n--, p++)
				*p = 255 - *p;
		}
	}
}

static void pdfapp_open_pdf(pdfapp_t *app, char *filename, int fd)
{
	fz_error error;
	fz_stream *file;
	char *password = "";
	fz_obj *obj;
	fz_obj *info;

	/*
	 * Open PDF and load xref table
	 */

	file = fz_open_fd(fd);
	error = pdf_open_xref_with_stream(&app->xref, file, NULL);
	if (error)
		pdfapp_error(app, fz_rethrow(error, "cannot open document '%s'", filename));
	fz_close(file);

	/*
	 * Handle encrypted PDF files
	 */

	if (pdf_needs_password(app->xref))
	{
		int okay = pdf_authenticate_password(app->xref, password);
		while (!okay)
		{
			password = winpassword(app, filename);
			if (!password)
				exit(1);
			okay = pdf_authenticate_password(app->xref, password);
			if (!okay)
				pdfapp_warn(app, "Invalid password.");
		}
	}

	/*
	 * Load meta information
	 */

	app->outline = pdf_load_outline(app->xref);

	app->doctitle = filename;
	if (strrchr(app->doctitle, '\\'))
		app->doctitle = strrchr(app->doctitle, '\\') + 1;
	if (strrchr(app->doctitle, '/'))
		app->doctitle = strrchr(app->doctitle, '/') + 1;
	info = fz_dict_gets(app->xref->trailer, "Info");
	if (info)
	{
		obj = fz_dict_gets(info, "Title");
		if (obj)
			app->doctitle = pdf_to_utf8(obj);
	}

	/*
	 * Start at first page
	 */

	error = pdf_load_page_tree(app->xref);
	if (error)
		pdfapp_error(app, fz_rethrow(error, "cannot load page tree"));

	app->pagecount = pdf_count_pages(app->xref);
}

static void pdfapp_open_xps(pdfapp_t *app, char *filename, int fd)
{
	fz_error error;
	fz_stream *file;

	file = fz_open_fd(fd);
	error = xps_open_stream(&app->xps, file);
	if (error)
		pdfapp_error(app, fz_rethrow(error, "cannot open document '%s'", filename));
	fz_close(file);

	app->doctitle = filename;

	app->pagecount = xps_count_pages(app->xps);
}

void pdfapp_open(pdfapp_t *app, char *filename, int fd, int reload)
{
	if (strstr(filename, ".xps") || strstr(filename, ".XPS") || strstr(filename, ".rels"))
		pdfapp_open_xps(app, filename, fd);
	else
		pdfapp_open_pdf(app, filename, fd);

	app->cache = fz_new_glyph_cache();

	if (app->pageno < 1)
		app->pageno = 1;
	if (app->pageno > app->pagecount)
		app->pageno = app->pagecount;
	if (app->resolution < MINRES)
		app->resolution = MINRES;
	if (app->resolution > MAXRES)
		app->resolution = MAXRES;

	if (!reload)
	{
		app->shrinkwrap = 1;
		app->rotate = 0;
		app->panx = 0;
		app->pany = 0;
	}

	pdfapp_showpage(app, 1, 1, 1);
}

void pdfapp_close(pdfapp_t *app)
{
	if (app->cache)
		fz_free_glyph_cache(app->cache);
	app->cache = NULL;

	if (app->image)
		fz_drop_pixmap(app->image);
	app->image = NULL;

	if (app->outline)
		pdf_free_outline(app->outline);
	app->outline = NULL;

	if (app->xref)
	{
		if (app->xref->store)
			pdf_free_store(app->xref->store);
		app->xref->store = NULL;

		pdf_free_xref(app->xref);
		app->xref = NULL;
	}

	if (app->xps)
	{
		xps_free_context(app->xps);
		app->xps = NULL;
	}

	fz_flush_warnings();
}

static fz_matrix pdfapp_viewctm(pdfapp_t *app)
{
	fz_matrix ctm;
	ctm = fz_identity;
	ctm = fz_concat(ctm, fz_translate(0, -app->page_bbox.y1));
	if (app->xref)
		ctm = fz_concat(ctm, fz_scale(app->resolution/72.0f, -app->resolution/72.0f));
	else
		ctm = fz_concat(ctm, fz_scale(app->resolution/96.0f, app->resolution/96.0f));
	ctm = fz_concat(ctm, fz_rotate(app->rotate + app->page_rotate));
	return ctm;
}

static void pdfapp_panview(pdfapp_t *app, int newx, int newy)
{
	if (newx > 0)
		newx = 0;
	if (newy > 0)
		newy = 0;

	if (newx + app->image->w < app->winw)
		newx = app->winw - app->image->w;
	if (newy + app->image->h < app->winh)
		newy = app->winh - app->image->h;

	if (app->winw >= app->image->w)
		newx = (app->winw - app->image->w) / 2;
	if (app->winh >= app->image->h)
		newy = (app->winh - app->image->h) / 2;

	if (newx != app->panx || newy != app->pany)
		winrepaint(app);

	app->panx = newx;
	app->pany = newy;
}

static void pdfapp_loadpage_pdf(pdfapp_t *app)
{
	pdf_page *page;
	fz_error error;
	fz_device *mdev;

	error = pdf_load_page(&page, app->xref, app->pageno - 1);
	if (error)
		pdfapp_error(app, error);

	app->page_bbox = page->mediabox;
	app->page_rotate = page->rotate;
	app->page_links = page->links;
	page->links = NULL;

	/* Create display list */
	app->page_list = fz_new_display_list();
	mdev = fz_new_list_device(app->page_list);
	error = pdf_run_page(app->xref, page, mdev, fz_identity);
	if (error)
	{
		error = fz_rethrow(error, "cannot draw page %d in '%s'", app->pageno, app->doctitle);
		pdfapp_error(app, error);
	}
	fz_free_device(mdev);

	pdf_free_page(page);

	pdf_age_store(app->xref->store, 3);
}

static void pdfapp_loadpage_xps(pdfapp_t *app)
{
	xps_page *page;
	fz_device *mdev;
	fz_error error;

	error = xps_load_page(&page, app->xps, app->pageno - 1);
	if (error)
		pdfapp_error(app, fz_rethrow(error, "cannot load page %d in file '%s'", app->pageno, app->doctitle));

	app->page_bbox.x0 = 0;
	app->page_bbox.y0 = 0;
	app->page_bbox.x1 = page->width;
	app->page_bbox.y1 = page->height;
	app->page_rotate = 0;
	app->page_links = NULL;

	/* Create display list */
	app->page_list = fz_new_display_list();
	mdev = fz_new_list_device(app->page_list);
	app->xps->dev = mdev;
	xps_parse_fixed_page(app->xps, fz_identity, page);
	app->xps->dev = NULL;
	fz_free_device(mdev);

	xps_free_page(app->xps, page);
}

static void pdfapp_showpage(pdfapp_t *app, int loadpage, int drawpage, int repaint)
{
	char buf[256];
	fz_device *idev;
	fz_device *tdev;
	fz_colorspace *colorspace;
	fz_matrix ctm;
	fz_bbox bbox;

	wincursor(app, WAIT);

	if (loadpage)
	{
		if (app->page_list)
			fz_free_display_list(app->page_list);
		if (app->page_text)
			fz_free_text_span(app->page_text);
		if (app->page_links)
			pdf_free_link(app->page_links);

		if (app->xref)
			pdfapp_loadpage_pdf(app);
		if (app->xps)
			pdfapp_loadpage_xps(app);

		/* Zero search hit position */
		app->hit = -1;
		app->hitlen = 0;

		/* Extract text */
		app->page_text = fz_new_text_span();
		tdev = fz_new_text_device(app->page_text);
		fz_execute_display_list(app->page_list, tdev, fz_identity, fz_infinite_bbox);
		fz_free_device(tdev);
	}

	if (drawpage)
	{
		sprintf(buf, "%s - %d/%d (%d dpi)", app->doctitle,
				app->pageno, app->pagecount, app->resolution);
		wintitle(app, buf);

		ctm = pdfapp_viewctm(app);
		bbox = fz_round_rect(fz_transform_rect(ctm, app->page_bbox));

		/* Draw */
		if (app->image)
			fz_drop_pixmap(app->image);
		if (app->grayscale)
			colorspace = fz_device_gray;
		else
#ifdef _WIN32
			colorspace = fz_device_bgr;
#else
			colorspace = fz_device_rgb;
#endif
		app->image = fz_new_pixmap_with_rect(colorspace, bbox);
		fz_clear_pixmap_with_color(app->image, 255);
		idev = fz_new_draw_device(app->cache, app->image);
		fz_execute_display_list(app->page_list, idev, ctm, bbox);
		fz_free_device(idev);
	}

	if (repaint)
	{
		pdfapp_panview(app, app->panx, app->pany);

		if (app->shrinkwrap)
		{
			int w = app->image->w;
			int h = app->image->h;
			if (app->winw == w)
				app->panx = 0;
			if (app->winh == h)
				app->pany = 0;
			if (w > app->scrw * 90 / 100)
				w = app->scrw * 90 / 100;
			if (h > app->scrh * 90 / 100)
				h = app->scrh * 90 / 100;
			if (w != app->winw || h != app->winh)
				winresize(app, w, h);
		}

		winrepaint(app);

		wincursor(app, ARROW);
	}

	fz_flush_warnings();
}

static void pdfapp_gotouri(pdfapp_t *app, fz_obj *uri)
{
	char *buf;
	buf = fz_malloc(fz_to_str_len(uri) + 1);
	memcpy(buf, fz_to_str_buf(uri), fz_to_str_len(uri));
	buf[fz_to_str_len(uri)] = 0;
	winopenuri(app, buf);
	fz_free(buf);
}

static void pdfapp_gotopage(pdfapp_t *app, fz_obj *obj)
{
	int number;

	number = pdf_find_page_number(app->xref, obj);
	if (number < 0)
		return;

	if (app->histlen + 1 == 256)
	{
		memmove(app->hist, app->hist + 1, sizeof(int) * 255);
		app->histlen --;
	}
	app->hist[app->histlen++] = app->pageno;
	app->pageno = number + 1;
	pdfapp_showpage(app, 1, 1, 1);
}

static inline fz_bbox bboxcharat(fz_text_span *span, int idx)
{
	int ofs = 0;
	while (span)
	{
		if (idx < ofs + span->len)
			return span->text[idx - ofs].bbox;
		if (span->eol)
		{
			if (idx == ofs + span->len)
				return fz_empty_bbox;
			ofs ++;
		}
		ofs += span->len;
		span = span->next;
	}
	return fz_empty_bbox;
}

void pdfapp_inverthit(pdfapp_t *app)
{
	fz_bbox hitbox, bbox;
	fz_matrix ctm;
	int i;

	if (app->hit < 0)
		return;

	hitbox = fz_empty_bbox;
	ctm = pdfapp_viewctm(app);

	for (i = app->hit; i < app->hit + app->hitlen; i++)
	{
		bbox = bboxcharat(app->page_text, i);
		if (fz_is_empty_rect(bbox))
		{
			if (!fz_is_empty_rect(hitbox))
				pdfapp_invert(app, fz_transform_bbox(ctm, hitbox));
			hitbox = fz_empty_bbox;
		}
		else
		{
			hitbox = fz_union_bbox(hitbox, bbox);
		}
	}

	if (!fz_is_empty_rect(hitbox))
		pdfapp_invert(app, fz_transform_bbox(ctm, hitbox));
}

static inline int charat(fz_text_span *span, int idx)
{
	int ofs = 0;
	while (span)
	{
		if (idx < ofs + span->len)
			return span->text[idx - ofs].c;
		if (span->eol)
		{
			if (idx == ofs + span->len)
				return ' ';
			ofs ++;
		}
		ofs += span->len;
		span = span->next;
	}
	return 0;
}

static int textlen(fz_text_span *span)
{
	int len = 0;
	while (span)
	{
		len += span->len;
		if (span->eol)
			len ++;
		span = span->next;
	}
	return len;
}

static int match(char *s, fz_text_span *span, int n)
{
	int orig = n;
	int c;
	while ((c = *s++))
	{
		if (c == ' ' && charat(span, n) == ' ')
		{
			while (charat(span, n) == ' ')
				n++;
		}
		else
		{
			if (tolower(c) != tolower(charat(span, n)))
				return 0;
			n++;
		}
	}
	return n - orig;
}

static void pdfapp_searchforward(pdfapp_t *app, enum panning *panto)
{
	int matchlen;
	int test;
	int len;
	int startpage;

	wincursor(app, WAIT);

	startpage = app->pageno;

	do
	{
		len = textlen(app->page_text);

		if (app->hit >= 0)
			test = app->hit + strlen(app->search);
		else
			test = 0;

		while (test < len)
		{
			matchlen = match(app->search, app->page_text, test);
			if (matchlen)
			{
				app->hit = test;
				app->hitlen = matchlen;
				wincursor(app, HAND);
				winrepaint(app);
				return;
			}
			test++;
		}

		app->pageno++;
		if (app->pageno > app->pagecount)
			app->pageno = 1;

		pdfapp_showpage(app, 1, 0, 0);
		*panto = PAN_TO_TOP;

	} while (app->pageno != startpage);

	if (app->pageno == startpage)
	{
		pdfapp_warn(app, "String '%s' not found.", app->search);
		winrepaintsearch(app);
	}
	else
		winrepaint(app);

	wincursor(app, HAND);
}

static void pdfapp_searchbackward(pdfapp_t *app, enum panning *panto)
{
	int matchlen;
	int test;
	int len;
	int startpage;

	wincursor(app, WAIT);

	startpage = app->pageno;

	do
	{
		len = textlen(app->page_text);

		if (app->hit >= 0)
			test = app->hit - 1;
		else
			test = len;

		while (test >= 0)
		{
			matchlen = match(app->search, app->page_text, test);
			if (matchlen)
			{
				app->hit = test;
				app->hitlen = matchlen;
				wincursor(app, HAND);
				winrepaint(app);
				return;
			}
			test--;
		}

		app->pageno--;
		if (app->pageno < 1)
			app->pageno = app->pagecount;

		pdfapp_showpage(app, 1, 0, 0);
		*panto = PAN_TO_BOTTOM;

	} while (app->pageno != startpage);

	if (app->pageno == startpage)
	{
		pdfapp_warn(app, "String '%s' not found.", app->search);
		winrepaintsearch(app);
	}
	else
		winrepaint(app);

	wincursor(app, HAND);
}

void pdfapp_onresize(pdfapp_t *app, int w, int h)
{
	if (app->winw != w || app->winh != h)
	{
		app->winw = w;
		app->winh = h;
		pdfapp_panview(app, app->panx, app->pany);
		winrepaint(app);
	}
}

void pdfapp_onkey(pdfapp_t *app, int c)
{
	int oldpage = app->pageno;
	enum panning panto = PAN_TO_TOP;
	int loadpage = 1;

	if (app->isediting)
	{
		int n = strlen(app->search);
		if (c < ' ')
		{
			if (c == '\b' && n > 0)
			{
				app->search[n - 1] = 0;
				winrepaintsearch(app);
			}
			if (c == '\n' || c == '\r')
			{
				app->isediting = 0;
				if (n > 0)
				{
					winrepaintsearch(app);
					pdfapp_onkey(app, 'n');
				}
				else
					winrepaint(app);
			}
			if (c == '\033')
			{
				app->isediting = 0;
				winrepaint(app);
			}
		}
		else
		{
			if (n + 2 < sizeof app->search)
			{
				app->search[n] = c;
				app->search[n + 1] = 0;
				winrepaintsearch(app);
			}
		}
		return;
	}

	/*
	 * Save numbers typed for later
	 */

	if (c >= '0' && c <= '9')
	{
		app->number[app->numberlen++] = c;
		app->number[app->numberlen] = '\0';
	}

	switch (c)
	{

	case '?':
		winhelp(app);
		break;

	case 'q':
		winclose(app);
		break;

	/*
	 * Zoom and rotate
	 */

	case '+':
	case '=':
		app->resolution *= ZOOMSTEP;
		if (app->resolution > MAXRES)
			app->resolution = MAXRES;
		pdfapp_showpage(app, 0, 1, 1);
		break;
	case '-':
		app->resolution /= ZOOMSTEP;
		if (app->resolution < MINRES)
			app->resolution = MINRES;
		pdfapp_showpage(app, 0, 1, 1);
		break;

	case 'L':
		app->rotate -= 90;
		pdfapp_showpage(app, 0, 1, 1);
		break;
	case 'R':
		app->rotate += 90;
		pdfapp_showpage(app, 0, 1, 1);
		break;

	case 'c':
		app->grayscale ^= 1;
		pdfapp_showpage(app, 0, 1, 1);
		break;

#ifndef NDEBUG
	case 'a':
		app->rotate -= 15;
		pdfapp_showpage(app, 0, 1, 1);
		break;
	case 's':
		app->rotate += 15;
		pdfapp_showpage(app, 0, 1, 1);
		break;
#endif

	/*
	 * Pan view, but dont need to repaint image
	 */

	case 'w':
		app->shrinkwrap = 1;
		app->panx = app->pany = 0;
		pdfapp_showpage(app, 0, 0, 1);
		break;

	case 'h':
		app->panx += app->image->w / 10;
		pdfapp_showpage(app, 0, 0, 1);
		break;

	case 'j':
		app->pany -= app->image->h / 10;
		pdfapp_showpage(app, 0, 0, 1);
		break;

	case 'k':
		app->pany += app->image->h / 10;
		pdfapp_showpage(app, 0, 0, 1);
		break;

	case 'l':
		app->panx -= app->image->w / 10;
		pdfapp_showpage(app, 0, 0, 1);
		break;

	/*
	 * Page navigation
	 */

	case 'g':
	case '\n':
	case '\r':
		if (app->numberlen > 0)
			app->pageno = atoi(app->number);
		else
			app->pageno = 1;
		break;

	case 'G':
		app->pageno = app->pagecount;
		break;

	case 'm':
		if (app->numberlen > 0)
		{
			int idx = atoi(app->number);

			if (idx >= 0 && idx < nelem(app->marks))
				app->marks[idx] = app->pageno;
		}
		else
		{
			if (app->histlen + 1 == 256)
			{
				memmove(app->hist, app->hist + 1, sizeof(int) * 255);
				app->histlen --;
			}
			app->hist[app->histlen++] = app->pageno;
		}
		break;

	case 't':
		if (app->numberlen > 0)
		{
			int idx = atoi(app->number);

			if (idx >= 0 && idx < nelem(app->marks))
				if (app->marks[idx] > 0)
					app->pageno = app->marks[idx];
		}
		else if (app->histlen > 0)
			app->pageno = app->hist[--app->histlen];
		break;

	/*
	 * Back and forth ...
	 */

	case ',':
		panto = PAN_TO_BOTTOM;
		if (app->numberlen > 0)
			app->pageno -= atoi(app->number);
		else
			app->pageno--;
		break;

	case '.':
		panto = PAN_TO_TOP;
		if (app->numberlen > 0)
			app->pageno += atoi(app->number);
		else
			app->pageno++;
		break;

	case 'b':
		panto = DONT_PAN;
		if (app->numberlen > 0)
			app->pageno -= atoi(app->number);
		else
			app->pageno--;
		break;

	case ' ':
		panto = DONT_PAN;
		if (app->numberlen > 0)
			app->pageno += atoi(app->number);
		else
			app->pageno++;
		break;

	case '<':
		panto = PAN_TO_TOP;
		app->pageno -= 10;
		break;
	case '>':
		panto = PAN_TO_TOP;
		app->pageno += 10;
		break;

	/*
	 * Reloading the file...
	 */

	case 'r':
		panto = DONT_PAN;
		oldpage = -1;
		winreloadfile(app);
		break;

	/*
	 * Searching
	 */

	case '/':
		app->isediting = 1;
		app->search[0] = 0;
		app->hit = -1;
		app->hitlen = 0;
		winrepaintsearch(app);
		break;

	case 'n':
		pdfapp_searchforward(app, &panto);
		loadpage = 0;
		break;

	case 'N':
		pdfapp_searchbackward(app, &panto);
		loadpage = 0;
		break;

	}

	if (c < '0' || c > '9')
		app->numberlen = 0;

	if (app->pageno < 1)
		app->pageno = 1;
	if (app->pageno > app->pagecount)
		app->pageno = app->pagecount;

	if (app->pageno != oldpage)
	{
		switch (panto)
		{
		case PAN_TO_TOP:
			app->pany = 0;
			break;
		case PAN_TO_BOTTOM:
			app->pany = -2000;
			break;
		case DONT_PAN:
			break;
		}
		pdfapp_showpage(app, loadpage, 1, 1);
	}
}

void pdfapp_onmouse(pdfapp_t *app, int x, int y, int btn, int modifiers, int state)
{
	pdf_link *link;
	fz_matrix ctm;
	fz_point p;

	p.x = x - app->panx + app->image->x;
	p.y = y - app->pany + app->image->y;

	ctm = pdfapp_viewctm(app);
	ctm = fz_invert_matrix(ctm);

	p = fz_transform_point(ctm, p);

	for (link = app->page_links; link; link = link->next)
	{
		if (p.x >= link->rect.x0 && p.x <= link->rect.x1)
			if (p.y >= link->rect.y0 && p.y <= link->rect.y1)
				break;
	}

	if (link)
	{
		wincursor(app, HAND);
		if (btn == 1 && state == 1)
		{
			if (link->kind == PDF_LINK_URI)
				pdfapp_gotouri(app, link->dest);
			else if (link->kind == PDF_LINK_GOTO)
				pdfapp_gotopage(app, fz_array_get(link->dest, 0)); /* [ pageobj ... ] */
			return;
		}
	}
	else
	{
		wincursor(app, ARROW);
	}

	if (state == 1)
	{
		if (btn == 1 && !app->iscopying)
		{
			app->ispanning = 1;
			app->selx = x;
			app->sely = y;
			app->beyondy = 0;
		}
		if (btn == 3 && !app->ispanning)
		{
			app->iscopying = 1;
			app->selx = x;
			app->sely = y;
			app->selr.x0 = x;
			app->selr.x1 = x;
			app->selr.y0 = y;
			app->selr.y1 = y;
		}
		if (btn == 4 || btn == 5) /* scroll wheel */
		{
			int dir = btn == 4 ? 1 : -1;
			app->ispanning = app->iscopying = 0;
			if (modifiers & (1<<2))
			{
				/* zoom in/out if ctrl is pressed */
				if (dir > 0)
					app->resolution *= ZOOMSTEP;
				else
					app->resolution /= ZOOMSTEP;
				if (app->resolution > MAXRES)
					app->resolution = MAXRES;
				if (app->resolution < MINRES)
					app->resolution = MINRES;
				pdfapp_showpage(app, 0, 1, 1);
			}
			else
			{
				/* scroll up/down, or left/right if
				shift is pressed */
				int isx = (modifiers & (1<<0));
				int xstep = isx ? 20 * dir : 0;
				int ystep = !isx ? 20 * dir : 0;
				pdfapp_panview(app, app->panx + xstep, app->pany + ystep);
			}
		}
	}

	else if (state == -1)
	{
		if (app->iscopying)
		{
			app->iscopying = 0;
			app->selr.x0 = MIN(app->selx, x) - app->panx + app->image->x;
			app->selr.x1 = MAX(app->selx, x) - app->panx + app->image->x;
			app->selr.y0 = MIN(app->sely, y) - app->pany + app->image->y;
			app->selr.y1 = MAX(app->sely, y) - app->pany + app->image->y;
			winrepaint(app);
			if (app->selr.x0 < app->selr.x1 && app->selr.y0 < app->selr.y1)
				windocopy(app);
		}
		if (app->ispanning)
			app->ispanning = 0;
	}

	else if (app->ispanning)
	{
		int newx = app->panx + x - app->selx;
		int newy = app->pany + y - app->sely;
		/* Scrolling beyond limits implies flipping pages */
		/* Are we requested to scroll beyond limits? */
		if (newy + app->image->h < app->winh || newy > 0)
		{
			/* Yes. We can assume that deltay != 0 */
			int deltay = y - app->sely;
			/* Check whether the panning has occured in the
			 * direction that we are already crossing the
			 * limit it. If not, we can conclude that we
			 * have switched ends of the page and will thus
			 * start over counting.
			 */
			if( app->beyondy == 0 || (app->beyondy ^ deltay) >= 0 )
			{
				/* Updating how far we are beyond and
				 * flipping pages if beyond threshhold
				 */
				app->beyondy += deltay;
				if (app->beyondy > BEYOND_THRESHHOLD)
				{
					if( app->pageno > 1 )
					{
						app->pageno--;
						pdfapp_showpage(app, 1, 1, 1);
						newy = -app->image->h;
					}
					app->beyondy = 0;
				}
				else if (app->beyondy < -BEYOND_THRESHHOLD)
				{
					if( app->pageno < app->pagecount )
					{
						app->pageno++;
						pdfapp_showpage(app, 1, 1, 1);
						newy = 0;
					}
					app->beyondy = 0;
				}
			}
			else
				app->beyondy = 0;
		}
		/* Although at this point we've already determined that
		 * or that no scrolling will be performed in
		 * y-direction, the x-direction has not yet been taken
		 * care off. Therefore
		 */
		pdfapp_panview(app, newx, newy);

		app->selx = x;
		app->sely = y;
	}

	else if (app->iscopying)
	{
		app->selr.x0 = MIN(app->selx, x) - app->panx + app->image->x;
		app->selr.x1 = MAX(app->selx, x) - app->panx + app->image->x;
		app->selr.y0 = MIN(app->sely, y) - app->pany + app->image->y;
		app->selr.y1 = MAX(app->sely, y) - app->pany + app->image->y;
		winrepaint(app);
	}

}

void pdfapp_oncopy(pdfapp_t *app, unsigned short *ucsbuf, int ucslen)
{
	fz_bbox hitbox;
	fz_matrix ctm;
	fz_text_span *span;
	int c, i, p;
	int seen;

	int x0 = app->selr.x0;
	int x1 = app->selr.x1;
	int y0 = app->selr.y0;
	int y1 = app->selr.y1;

	ctm = pdfapp_viewctm(app);

	p = 0;
	for (span = app->page_text; span; span = span->next)
	{
		seen = 0;

		for (i = 0; i < span->len; i++)
		{
			hitbox = fz_transform_bbox(ctm, span->text[i].bbox);
			c = span->text[i].c;
			if (c < 32)
				c = '?';
			if (hitbox.x1 >= x0 && hitbox.x0 <= x1 && hitbox.y1 >= y0 && hitbox.y0 <= y1)
			{
				if (p < ucslen - 1)
					ucsbuf[p++] = c;
				seen = 1;
			}
		}

		if (seen && span->eol)
		{
#ifdef _WIN32
			if (p < ucslen - 1)
				ucsbuf[p++] = '\r';
#endif
			if (p < ucslen - 1)
				ucsbuf[p++] = '\n';
		}
	}

	ucsbuf[p] = 0;
}
