#include <fitz.h>
#include <mupdf.h>
#include "pdfapp.h"

#include <ctype.h> /* for tolower() */

#define ZOOMSTEP 1.142857

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
		"MuPDF 0.8\n"
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

void pdfapp_open(pdfapp_t *app, char *filename, int fd)
{
	fz_error error;
	fz_obj *obj;
	fz_obj *info;
	char *password = "";
	fz_stream *file;

	app->cache = fz_newglyphcache();

	/*
	 * Open PDF and load xref table
	 */

	file = fz_openfile(fd);
	error = pdf_openxrefwithstream(&app->xref, file, nil);
	if (error)
		pdfapp_error(app, fz_rethrow(error, "cannot open document '%s'", filename));
	fz_close(file);

	/*
	 * Handle encrypted PDF files
	 */

	if (pdf_needspassword(app->xref))
	{
		int okay = pdf_authenticatepassword(app->xref, password);
		while (!okay)
		{
			password = winpassword(app, filename);
			if (!password)
				exit(1);
			okay = pdf_authenticatepassword(app->xref, password);
			if (!okay)
				pdfapp_warn(app, "Invalid password.");
		}
	}

	/*
	 * Load meta information
	 */

	app->outline = pdf_loadoutline(app->xref);

	app->doctitle = filename;
	if (strrchr(app->doctitle, '\\'))
		app->doctitle = strrchr(app->doctitle, '\\') + 1;
	if (strrchr(app->doctitle, '/'))
		app->doctitle = strrchr(app->doctitle, '/') + 1;
	info = fz_dictgets(app->xref->trailer, "Info");
	if (info)
	{
		obj = fz_dictgets(info, "Title");
		if (obj)
			app->doctitle = pdf_toutf8(obj);
	}

	/*
	 * Start at first page
	 */

	error = pdf_loadpagetree(app->xref);
	if (error)
		pdfapp_error(app, fz_rethrow(error, "cannot load page tree"));

	app->pagecount = pdf_getpagecount(app->xref);

	app->shrinkwrap = 1;
	if (app->pageno < 1)
		app->pageno = 1;
	if (app->pageno > app->pagecount)
		app->pageno = app->pagecount;
	if (app->resolution < MINRES)
		app->resolution = MINRES;
	if (app->resolution > MAXRES)
		app->resolution = MAXRES;
	app->rotate = 0;
	app->panx = 0;
	app->pany = 0;

	pdfapp_showpage(app, 1, 1, 1);
}

void pdfapp_close(pdfapp_t *app)
{
	if (app->cache)
		fz_freeglyphcache(app->cache);
	app->cache = nil;

	if (app->page)
		pdf_freepage(app->page);
	app->page = nil;

	if (app->image)
		fz_droppixmap(app->image);
	app->image = nil;

	if (app->outline)
		pdf_freeoutline(app->outline);
	app->outline = nil;

	if (app->xref)
	{
		if (app->xref->store)
			pdf_freestore(app->xref->store);
		app->xref->store = nil;

		pdf_freexref(app->xref);
		app->xref = nil;
	}

	fz_flushwarnings();
}

static fz_matrix pdfapp_viewctm(pdfapp_t *app)
{
	fz_matrix ctm;
	ctm = fz_identity;
	ctm = fz_concat(ctm, fz_translate(0, -app->page->mediabox.y1));
	ctm = fz_concat(ctm, fz_scale(app->resolution/72.0f, -app->resolution/72.0f));
	ctm = fz_concat(ctm, fz_rotate(app->rotate + app->page->rotate));
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

static void pdfapp_showpage(pdfapp_t *app, int loadpage, int drawpage, int repaint)
{
	char buf[256];
	fz_error error;
	fz_device *idev, *tdev, *mdev;
	fz_colorspace *colorspace;
	fz_matrix ctm;
	fz_bbox bbox;
	fz_obj *obj;

	wincursor(app, WAIT);

	if (loadpage)
	{
		if (app->page)
			pdf_freepage(app->page);
		app->page = nil;

		obj = pdf_getpageobject(app->xref, app->pageno);
		error = pdf_loadpage(&app->page, app->xref, obj);
		if (error)
			pdfapp_error(app, error);

		/* Create display list */
		app->page->list = fz_newdisplaylist();
		mdev = fz_newlistdevice(app->page->list);
		error = pdf_runpage(app->xref, app->page, mdev, fz_identity);
		if (error)
		{
			error = fz_rethrow(error, "cannot draw page %d in '%s'", app->pageno, app->doctitle);
			pdfapp_error(app, error);
		}
		fz_freedevice(mdev);

		/* Zero search hit position */
		app->hit = -1;
		app->hitlen = 0;

		/* Extract text */
		app->page->text = fz_newtextspan();
		tdev = fz_newtextdevice(app->page->text);
		fz_executedisplaylist(app->page->list, tdev, fz_identity);
		fz_freedevice(tdev);

		pdf_agestore(app->xref->store, 3);
	}

	if (drawpage)
	{
		sprintf(buf, "%s - %d/%d (%d dpi)", app->doctitle,
				app->pageno, app->pagecount, app->resolution);
		wintitle(app, buf);

		ctm = pdfapp_viewctm(app);
		bbox = fz_roundrect(fz_transformrect(ctm, app->page->mediabox));

		/* Draw */
		if (app->image)
			fz_droppixmap(app->image);
		if (app->grayscale)
			colorspace = fz_devicegray;
		else
#ifdef _WIN32
			colorspace = fz_devicebgr;
#else
			colorspace = fz_devicergb;
#endif
		app->image = fz_newpixmapwithrect(colorspace, bbox);
		fz_clearpixmapwithcolor(app->image, 255);
		idev = fz_newdrawdevice(app->cache, app->image);
		fz_executedisplaylist(app->page->list, idev, ctm);
		fz_freedevice(idev);
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

	fz_flushwarnings();
}

static void pdfapp_gotouri(pdfapp_t *app, fz_obj *uri)
{
	char *buf;
	buf = fz_malloc(fz_tostrlen(uri) + 1);
	memcpy(buf, fz_tostrbuf(uri), fz_tostrlen(uri));
	buf[fz_tostrlen(uri)] = 0;
	winopenuri(app, buf);
	fz_free(buf);
}

static void pdfapp_gotopage(pdfapp_t *app, fz_obj *obj)
{
	int page;

	page = pdf_findpageobject(app->xref, obj);

	if (app->histlen + 1 == 256)
	{
		memmove(app->hist, app->hist + 1, sizeof(int) * 255);
		app->histlen --;
	}
	app->hist[app->histlen++] = app->pageno;
	app->pageno = page;
	pdfapp_showpage(app, 1, 1, 1);
}

static inline fz_bbox bboxcharat(fz_textspan *span, int idx)
{
	int ofs = 0;
	while (span)
	{
		if (idx < ofs + span->len)
			return span->text[idx - ofs].bbox;
		if (span->eol)
		{
			if (idx == ofs + span->len)
				return fz_emptybbox;
			ofs ++;
		}
		ofs += span->len;
		span = span->next;
	}
	return fz_emptybbox;
}

void pdfapp_inverthit(pdfapp_t *app)
{
	fz_bbox hitbox, bbox;
	int i;

	if (app->hit < 0)
		return;

	hitbox = fz_emptybbox;

	for (i = app->hit; i < app->hit + app->hitlen; i++)
	{
		bbox = bboxcharat(app->page->text, i);
		if (fz_isemptyrect(bbox))
		{
			if (!fz_isemptyrect(hitbox))
				pdfapp_invert(app, hitbox);
			hitbox = fz_emptybbox;
		}
		else
		{
			hitbox = fz_unionbbox(hitbox, bbox);
		}
	}

	if (!fz_isemptyrect(hitbox))
	{
		fz_matrix ctm;

		ctm = pdfapp_viewctm(app);
		hitbox = fz_transformbbox(ctm, hitbox);

		pdfapp_invert(app, hitbox);
	}
}

static inline int charat(fz_textspan *span, int idx)
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

static int textlen(fz_textspan *span)
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

static int match(char *s, fz_textspan *span, int n)
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

static void pdfapp_searchforward(pdfapp_t *app)
{
	int matchlen;
	int test;
	int len;
	int startpage;

	wincursor(app, WAIT);

	startpage = app->pageno;

	do
	{
		len = textlen(app->page->text);

		if (app->hit >= 0)
			test = app->hit + strlen(app->search);
		else
			test = 0;

		while (test < len)
		{
			matchlen = match(app->search, app->page->text, test);
			if (matchlen)
			{
				app->hit = test;
				app->hitlen = matchlen;
				wincursor(app, HAND);
				return;
			}
			test++;
		}

		app->pageno++;
		if (app->pageno > app->pagecount)
			app->pageno = 1;

		pdfapp_showpage(app, 1, 0, 0);
		app->pany = 0;

	} while (app->pageno != startpage);

	if (app->pageno == startpage)
		printf("hit not found\n");

	wincursor(app, HAND);
}

static void pdfapp_searchbackward(pdfapp_t *app)
{
	int matchlen;
	int test;
	int len;
	int startpage;

	wincursor(app, WAIT);

	startpage = app->pageno;

	do
	{
		len = textlen(app->page->text);

		if (app->hit >= 0)
			test = app->hit - 1;
		else
			test = len;

		while (test >= 0)
		{
			matchlen = match(app->search, app->page->text, test);
			if (matchlen)
			{
				app->hit = test;
				app->hitlen = matchlen;
				wincursor(app, HAND);
				return;
			}
			test--;
		}

		app->pageno--;
		if (app->pageno < 1)
			app->pageno = app->pagecount;

		pdfapp_showpage(app, 1, 0, 0);
		app->pany = -2000;

	} while (app->pageno != startpage);

	if (app->pageno == startpage)
		printf("hit not found\n");

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
				app->search[n - 1] = 0;
			if (c == '\n' || c == '\r')
			{
				app->isediting = 0;
				winrepaint(app);
				pdfapp_onkey(app, 'n');
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
		break;

	case 'n':
		pdfapp_searchforward(app);
		winrepaint(app);
		loadpage = 0;
		break;

	case 'N':
		pdfapp_searchbackward(app);
		winrepaint(app);
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
	ctm = fz_invertmatrix(ctm);

	p = fz_transformpoint(ctm, p);

	for (link = app->page->links; link; link = link->next)
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
			if (link->kind == PDF_LURI)
				pdfapp_gotouri(app, link->dest);
			else if (link->kind == PDF_LGOTO)
				pdfapp_gotopage(app, fz_arrayget(link->dest, 0)); /* [ pageobj ... ] */
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
	fz_textspan *span;
	int c, i, p;
	int seen;

	int x0 = app->selr.x0;
	int x1 = app->selr.x1;
	int y0 = app->selr.y0;
	int y1 = app->selr.y1;

	ctm = pdfapp_viewctm(app);

	p = 0;
	for (span = app->page->text; span; span = span->next)
	{
		seen = 0;

		for (i = 0; i < span->len; i++)
		{
			hitbox = fz_transformbbox(ctm, span->text[i].bbox);
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
