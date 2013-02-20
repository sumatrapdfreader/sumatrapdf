#include "pdfapp.h"
#include "mupdf.h"
#include "muxps.h"
#include "mucbz.h"

#include <ctype.h> /* for tolower() */

#define ZOOMSTEP 1.142857
#define BEYOND_THRESHHOLD 40
#ifndef PATH_MAX
#define PATH_MAX (1024)
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

enum panning
{
	DONT_PAN = 0,
	PAN_TO_TOP,
	PAN_TO_BOTTOM
};

static void pdfapp_showpage(pdfapp_t *app, int loadpage, int drawpage, int repaint, int transition);
static void pdfapp_updatepage(pdfapp_t *app);

static void pdfapp_warn(pdfapp_t *app, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[sizeof(buf)-1] = 0;
	winwarn(app, buf);
}

static void pdfapp_error(pdfapp_t *app, char *msg)
{
	winerror(app, msg);
}

char *pdfapp_version(pdfapp_t *app)
{
	return
		"MuPDF 1.2\n"
		"Copyright 2006-2013 Artifex Software, Inc.\n";
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
		"W\t\t-- zoom to fit window width\n"
		"H\t\t-- zoom to fit window height\n"
		"w\t\t-- shrinkwrap\n"
		"f\t\t-- fullscreen\n"
		"r\t\t-- reload file\n"
		". pgdn right spc\t-- next page\n"
		", pgup left b bkspc\t-- previous page\n"
		">\t\t-- next 10 pages\n"
		"<\t\t-- back 10 pages\n"
		"m\t\t-- mark page for snap back\n"
		"t\t\t-- pop back to latest mark\n"
		"1m\t\t-- mark page in register 1\n"
		"1t\t\t-- go to page in register 1\n"
		"G\t\t-- go to last page\n"
		"123g\t\t-- go to page 123\n"
		"/\t\t-- search forwards for text\n"
		"?\t\t-- search backwards for text\n"
		"n\t\t-- find next search result\n"
		"N\t\t-- find previous search result\n"
		"c\t\t-- toggle between color and grayscale\n"
		"i\t\t-- toggle inverted color mode\n"
		"q\t\t-- quit\n"
	;
}

void pdfapp_init(fz_context *ctx, pdfapp_t *app)
{
	memset(app, 0, sizeof(pdfapp_t));
	app->scrw = 640;
	app->scrh = 480;
	app->resolution = 72;
	app->ctx = ctx;
#ifdef _WIN32
	app->colorspace = fz_device_bgr;
#else
	app->colorspace = fz_device_rgb;
#endif
}

void pdfapp_invert(pdfapp_t *app, const fz_rect *rect)
{
	fz_irect b;
	fz_invert_pixmap_rect(app->image, fz_round_rect(&b, rect));
}

static void event_cb(fz_doc_event *event, void *data)
{
	pdfapp_t *app = (pdfapp_t *)data;

	switch (event->type)
	{
	case FZ_DOCUMENT_EVENT_ALERT:
		{
			fz_alert_event *alert = fz_access_alert_event(event);
			winalert(app, alert);
		}
		break;

	case FZ_DOCUMENT_EVENT_PRINT:
		winprint(app);
		break;

	case FZ_DOCUMENT_EVENT_EXEC_MENU_ITEM:
		{
			char *item = fz_access_exec_menu_item_event(event);

			if (!strcmp(item, "Print"))
				winprint(app);
			else
				pdfapp_warn(app, "The document attempted to execute menu item: %s. (Not supported)", item);
		}
		break;

	case FZ_DOCUMENT_EVENT_EXEC_DIALOG:
		pdfapp_warn(app, "The document attempted to open a dialog box. (Not supported)");
		break;

	case FZ_DOCUMENT_EVENT_LAUNCH_URL:
		{
			fz_launch_url_event *launch_url = fz_access_launch_url_event(event);

			pdfapp_warn(app, "The document attempted to open url: %s. (Not supported by app)", launch_url->url);
		}
		break;

	case FZ_DOCUMENT_EVENT_MAIL_DOC:
		{
			fz_mail_doc_event *mail_doc = fz_access_mail_doc_event(event);

			pdfapp_warn(app, "The document attmepted to mail the document%s%s%s%s%s%s%s%s (Not supported)",
				mail_doc->to[0]?", To: ":"", mail_doc->to,
				mail_doc->cc[0]?", Cc: ":"", mail_doc->cc,
				mail_doc->bcc[0]?", Bcc: ":"", mail_doc->bcc,
				mail_doc->subject[0]?", Subject: ":"", mail_doc->subject);
		}
		break;
	}
}

void pdfapp_open(pdfapp_t *app, char *filename, int reload)
{
	fz_context *ctx = app->ctx;
	char *password = "";

	fz_try(ctx)
	{
		fz_interactive *idoc;

		app->doc = fz_open_document(ctx, filename);

		idoc = fz_interact(app->doc);

		if (idoc)
			fz_set_doc_event_callback(idoc, event_cb, app);

		if (fz_needs_password(app->doc))
		{
			int okay = fz_authenticate_password(app->doc, password);
			while (!okay)
			{
				password = winpassword(app, filename);
				if (!password)
					fz_throw(ctx, "Needs a password");
				okay = fz_authenticate_password(app->doc, password);
				if (!okay)
					pdfapp_warn(app, "Invalid password.");
			}
		}

		app->docpath = fz_strdup(ctx, filename);
		app->doctitle = filename;
		if (strrchr(app->doctitle, '\\'))
			app->doctitle = strrchr(app->doctitle, '\\') + 1;
		if (strrchr(app->doctitle, '/'))
			app->doctitle = strrchr(app->doctitle, '/') + 1;
		app->doctitle = fz_strdup(ctx, app->doctitle);

		app->pagecount = fz_count_pages(app->doc);
		app->outline = fz_load_outline(app->doc);
	}
	fz_catch(ctx)
	{
		pdfapp_error(app, "cannot open document");
	}

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

	pdfapp_showpage(app, 1, 1, 1, 0);
}

void pdfapp_close(pdfapp_t *app)
{
	if (app->page_list)
		fz_free_display_list(app->ctx, app->page_list);
	app->page_list = NULL;

	if (app->annotations_list)
		fz_free_display_list(app->ctx, app->annotations_list);
	app->annotations_list = NULL;

	if (app->page_text)
		fz_free_text_page(app->ctx, app->page_text);
	app->page_text = NULL;

	if (app->page_sheet)
		fz_free_text_sheet(app->ctx, app->page_sheet);
	app->page_sheet = NULL;

	if (app->page_links)
		fz_drop_link(app->ctx, app->page_links);
	app->page_links = NULL;

	if (app->doctitle)
		fz_free(app->ctx, app->doctitle);
	app->doctitle = NULL;

	if (app->docpath)
		fz_free(app->ctx, app->docpath);
	app->docpath = NULL;

	if (app->image)
		fz_drop_pixmap(app->ctx, app->image);
	app->image = NULL;

	if (app->new_image)
		fz_drop_pixmap(app->ctx, app->new_image);
	app->new_image = NULL;

	if (app->old_image)
		fz_drop_pixmap(app->ctx, app->old_image);
	app->old_image = NULL;

	if (app->outline)
		fz_free_outline(app->ctx, app->outline);
	app->outline = NULL;

	if (app->page)
		fz_free_page(app->doc, app->page);
	app->page = NULL;

	if (app->doc)
	{
		fz_close_document(app->doc);
		app->doc = NULL;
	}

	fz_flush_warnings(app->ctx);
}

static int gen_tmp_file(char *buf, int len)
{
	int i;
	char *name = strrchr(buf, '/');

	if (name == NULL)
		name = strrchr(buf, '\\');

	if (name != NULL)
		name++;
	else
		name = buf;

	for (i = 0; i < 10000; i++)
	{
		FILE *f;
		snprintf(name, buf+len-name, "tmp%04d", i);
		f = fopen(buf, "r");
		if (f == NULL)
			return 1;
		fclose(f);
	}

	return 0;
}

static int pdfapp_save(pdfapp_t *app)
{
	char buf[PATH_MAX];

	if (wingetsavepath(app, buf, PATH_MAX))
	{
		fz_write_options opts;

		opts.do_ascii = 1;
		opts.do_expand = 0;
		opts.do_garbage = 1;
		opts.do_linear = 0;

		if (strcmp(buf, app->docpath) == 0)
		{
			if (gen_tmp_file(buf, PATH_MAX))
			{
				int written;

				fz_var(written);
				fz_try(app->ctx)
				{
					fz_write_document(app->doc, buf, &opts);
					written = 1;
				}
				fz_catch(app->ctx)
				{
					written = 0;
				}

				if (written)
				{
					char buf2[PATH_MAX];
					fz_strlcpy(buf2, app->docpath, PATH_MAX);
					pdfapp_close(app);
					winreplacefile(buf, buf2);
					pdfapp_open(app, buf2, 1);
				}

				return written;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			fz_write_document(app->doc, buf, &opts);
			return 1;
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

int pdfapp_preclose(pdfapp_t *app)
{
	fz_interactive *idoc = fz_interact(app->doc);

	if (idoc && fz_has_unsaved_changes(idoc))
	{
		switch (winsavequery(app))
		{
		case DISCARD:
			return 1;

		case CANCEL:
			return 0;

		case SAVE:
			return pdfapp_save(app);
		}
	}

	return 1;
}

static void pdfapp_viewctm(fz_matrix *mat, pdfapp_t *app)
{
	fz_pre_rotate(fz_scale(mat, app->resolution/72.0f, app->resolution/72.0f), app->rotate);
}

static void pdfapp_panview(pdfapp_t *app, int newx, int newy)
{
	int image_w = fz_pixmap_width(app->ctx, app->image);
	int image_h = fz_pixmap_height(app->ctx, app->image);

	if (newx > 0)
		newx = 0;
	if (newy > 0)
		newy = 0;

	if (newx + image_w < app->winw)
		newx = app->winw - image_w;
	if (newy + image_h < app->winh)
		newy = app->winh - image_h;

	if (app->winw >= image_w)
		newx = (app->winw - image_w) / 2;
	if (app->winh >= image_h)
		newy = (app->winh - image_h) / 2;

	if (newx != app->panx || newy != app->pany)
		winrepaint(app);

	app->panx = newx;
	app->pany = newy;
}

static void pdfapp_loadpage(pdfapp_t *app)
{
	fz_device *mdev = NULL;
	int errored = 0;
	fz_cookie cookie = { 0 };

	fz_var(mdev);

	if (app->page_list)
		fz_free_display_list(app->ctx, app->page_list);
	if (app->annotations_list)
		fz_free_display_list(app->ctx, app->annotations_list);
	if (app->page_text)
		fz_free_text_page(app->ctx, app->page_text);
	if (app->page_sheet)
		fz_free_text_sheet(app->ctx, app->page_sheet);
	if (app->page_links)
		fz_drop_link(app->ctx, app->page_links);
	if (app->page)
		fz_free_page(app->doc, app->page);

	app->page_list = NULL;
	app->annotations_list = NULL;
	app->page_text = NULL;
	app->page_sheet = NULL;
	app->page_links = NULL;
	app->page = NULL;
	app->page_bbox.x0 = 0;
	app->page_bbox.y0 = 0;
	app->page_bbox.x1 = 100;
	app->page_bbox.y1 = 100;

	fz_try(app->ctx)
	{
		app->page = fz_load_page(app->doc, app->pageno - 1);

		fz_bound_page(app->doc, app->page, &app->page_bbox);
	}
	fz_catch(app->ctx)
	{
		pdfapp_warn(app, "Cannot load page");
		return;
	}

	fz_try(app->ctx)
	{
		fz_annot *annot;
		/* Create display lists */
		app->page_list = fz_new_display_list(app->ctx);
		mdev = fz_new_list_device(app->ctx, app->page_list);
		fz_run_page_contents(app->doc, app->page, mdev, &fz_identity, &cookie);
		fz_free_device(mdev);
		mdev = NULL;
		app->annotations_list = fz_new_display_list(app->ctx);
		mdev = fz_new_list_device(app->ctx, app->annotations_list);
		for (annot = fz_first_annot(app->doc, app->page); annot; annot = fz_next_annot(app->doc, annot))
			fz_run_annot(app->doc, app->page, annot, mdev, &fz_identity, &cookie);
		if (cookie.errors)
		{
			pdfapp_warn(app, "Errors found on page");
			errored = 1;
		}
	}
	fz_always(app->ctx)
	{
		fz_free_device(mdev);
	}
	fz_catch(app->ctx)
	{
		pdfapp_warn(app, "Cannot load page");
		errored = 1;
	}

	fz_try(app->ctx)
	{
		app->page_links = fz_load_links(app->doc, app->page);
	}
	fz_catch(app->ctx)
	{
		if (!errored)
			pdfapp_warn(app, "Cannot load page");
	}

	app->errored = errored;
}

static void pdfapp_recreate_annotationslist(pdfapp_t *app)
{
	fz_device *mdev = NULL;
	int errored = 0;
	fz_cookie cookie = { 0 };

	fz_var(mdev);

	if (app->annotations_list)
	{
		fz_free_display_list(app->ctx, app->annotations_list);
		app->annotations_list = NULL;
	}

	fz_try(app->ctx)
	{
		fz_annot *annot;
		/* Create display list */
		app->annotations_list = fz_new_display_list(app->ctx);
		mdev = fz_new_list_device(app->ctx, app->annotations_list);
		for (annot = fz_first_annot(app->doc, app->page); annot; annot = fz_next_annot(app->doc, annot))
			fz_run_annot(app->doc, app->page, annot, mdev, &fz_identity, &cookie);
		if (cookie.errors)
		{
			pdfapp_warn(app, "Errors found on page");
			errored = 1;
		}
	}
	fz_always(app->ctx)
	{
		fz_free_device(mdev);
	}
	fz_catch(app->ctx)
	{
		pdfapp_warn(app, "Cannot load page");
		errored = 1;
	}

	app->errored = errored;
}

#define MAX_TITLE 256

static void pdfapp_updatepage(pdfapp_t *app)
{
	fz_interactive *idoc = fz_interact(app->doc);
	fz_device *idev;
	fz_matrix ctm;
	fz_annot *annot;

	pdfapp_viewctm(&ctm, app);
	fz_update_page(idoc, app->page);
	pdfapp_recreate_annotationslist(app);

	while ((annot = fz_poll_changed_annot(idoc, app->page)) != NULL)
	{
		fz_rect bounds;
		fz_irect ibounds;
		fz_transform_rect(fz_bound_annot(app->doc, annot, &bounds), &ctm);
		fz_rect_from_irect(&bounds, fz_round_rect(&ibounds, &bounds));
		fz_clear_pixmap_rect_with_value(app->ctx, app->image, 255, &ibounds);
		idev = fz_new_draw_device_with_bbox(app->ctx, app->image, &ibounds);

		if (app->page_list)
			fz_run_display_list(app->page_list, idev, &ctm, &bounds, NULL);
		if (app->annotations_list)
			fz_run_display_list(app->annotations_list, idev, &ctm, &bounds, NULL);

		fz_free_device(idev);
	}

	pdfapp_showpage(app, 0, 0, 1, 0);
}

static void pdfapp_showpage(pdfapp_t *app, int loadpage, int drawpage, int repaint, int transition)
{
	char buf[MAX_TITLE];
	fz_device *idev;
	fz_device *tdev;
	fz_colorspace *colorspace;
	fz_matrix ctm;
	fz_rect bounds;
	fz_irect ibounds;
	fz_cookie cookie = { 0 };

	if (!app->nowaitcursor)
		wincursor(app, WAIT);

	if (!app->transitions_enabled || !app->presentation_mode)
		transition = 0;

	if (transition)
	{
		app->old_image = app->image;
		app->image = NULL;
	}

	if (loadpage)
	{
		pdfapp_loadpage(app);

		/* Zero search hit position */
		app->hit = -1;
		app->hitlen = 0;

		/* Extract text */
		app->page_sheet = fz_new_text_sheet(app->ctx);
		app->page_text = fz_new_text_page(app->ctx, &app->page_bbox);

		if (app->page_list || app->annotations_list)
		{
			tdev = fz_new_text_device(app->ctx, app->page_sheet, app->page_text);
			if (app->page_list)
				fz_run_display_list(app->page_list, tdev, &fz_identity, &fz_infinite_rect, &cookie);
			if (app->annotations_list)
				fz_run_display_list(app->annotations_list, tdev, &fz_identity, &fz_infinite_rect, &cookie);
			fz_free_device(tdev);
		}
	}

	if (drawpage)
	{
		char buf2[64];
		int len;

		sprintf(buf2, " - %d/%d (%d dpi)",
				app->pageno, app->pagecount, app->resolution);
		len = MAX_TITLE-strlen(buf2);
		if ((int)strlen(app->doctitle) > len)
		{
			snprintf(buf, len-3, "%s", app->doctitle);
			strcat(buf, "...");
			strcat(buf, buf2);
		}
		else
			sprintf(buf, "%s%s", app->doctitle, buf2);
		wintitle(app, buf);

		pdfapp_viewctm(&ctm, app);
		bounds = app->page_bbox;
		fz_round_rect(&ibounds, fz_transform_rect(&bounds, &ctm));
		fz_rect_from_irect(&bounds, &ibounds);

		/* Draw */
		if (app->image)
			fz_drop_pixmap(app->ctx, app->image);
		if (app->grayscale)
			colorspace = fz_device_gray;
		else
			colorspace = app->colorspace;
		app->image = NULL;
		app->image = fz_new_pixmap_with_bbox(app->ctx, colorspace, &ibounds);
		fz_clear_pixmap_with_value(app->ctx, app->image, 255);
		if (app->page_list || app->annotations_list)
		{
			idev = fz_new_draw_device(app->ctx, app->image);
			if (app->page_list)
				fz_run_display_list(app->page_list, idev, &ctm, &bounds, &cookie);
			if (app->annotations_list)
				fz_run_display_list(app->annotations_list, idev, &ctm, &bounds, &cookie);
			fz_free_device(idev);
		}
		if (app->invert)
			fz_invert_pixmap(app->ctx, app->image);
	}

	if (transition)
	{
		fz_transition *new_trans;
		app->new_image = app->image;
		app->image = NULL;
		app->image = fz_new_pixmap_with_bbox(app->ctx, colorspace, &ibounds);
		app->duration = 0;
		new_trans = fz_page_presentation(app->doc, app->page, &app->duration);
		if (new_trans)
			app->transition = *new_trans;
		else
		{
			/* If no transition specified, use a default one */
			memset(&app->transition, 0, sizeof(*new_trans));
			app->transition.duration = 1.0;
			app->transition.type = FZ_TRANSITION_WIPE;
			app->transition.vertical = 0;
			app->transition.direction = 0;
		}
		if (app->duration == 0)
			app->duration = 5;
		app->in_transit = fz_generate_transition(app->image, app->old_image, app->new_image, 0, &app->transition);
		if (!app->in_transit)
		{
			if (app->duration != 0)
				winadvancetimer(app, app->duration);
		}
		app->start_time = clock();
	}

	if (repaint)
	{
		pdfapp_panview(app, app->panx, app->pany);

		if (app->shrinkwrap)
		{
			int w = fz_pixmap_width(app->ctx, app->image);
			int h = fz_pixmap_height(app->ctx, app->image);
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

	if (cookie.errors && app->errored == 0)
	{
		app->errored = 1;
		pdfapp_warn(app, "Errors found on page. Page rendering may be incomplete.");
	}

	fz_flush_warnings(app->ctx);
}

static void pdfapp_gotouri(pdfapp_t *app, char *uri)
{
	winopenuri(app, uri);
}

void pdfapp_gotopage(pdfapp_t *app, int number)
{
	app->isediting = 0;
	winrepaint(app);

	if (app->histlen + 1 == 256)
	{
		memmove(app->hist, app->hist + 1, sizeof(int) * 255);
		app->histlen --;
	}
	app->hist[app->histlen++] = app->pageno;
	app->pageno = number + 1;
	pdfapp_showpage(app, 1, 1, 1, 0);
}

static fz_text_char textcharat(fz_text_page *page, int idx)
{
	static fz_text_char emptychar = { {0,0,0,0}, ' ' };
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int ofs = 0;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				if (idx < ofs + span->len)
					return span->text[idx - ofs];
				/* pseudo-newline */
				if (span + 1 == line->spans + line->len)
				{
					if (idx == ofs + span->len)
						return emptychar;
					ofs++;
				}
				ofs += span->len;
			}
		}
	}
	return emptychar;
}

static int textlen(fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int len = 0;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
				len += span->len;
			len++; /* pseudo-newline */
		}
	}
	return len;
}

static inline int charat(fz_text_page *page, int idx)
{
	return textcharat(page, idx).c;
}

static inline fz_rect bboxcharat(fz_text_page *page, int idx)
{
	return textcharat(page, idx).bbox;
}

void pdfapp_inverthit(pdfapp_t *app)
{
	fz_rect hitbox, bbox;
	fz_matrix ctm;
	int i;

	if (app->hit < 0)
		return;

	hitbox = fz_empty_rect;
	pdfapp_viewctm(&ctm, app);

	for (i = app->hit; i < app->hit + app->hitlen; i++)
	{
		bbox = bboxcharat(app->page_text, i);
		if (fz_is_empty_rect(&bbox))
		{
			if (!fz_is_empty_rect(&hitbox))
				pdfapp_invert(app, fz_transform_rect(&hitbox, &ctm));
			hitbox = fz_empty_rect;
		}
		else
		{
			fz_union_rect(&hitbox, &bbox);
		}
	}

	if (!fz_is_empty_rect(&hitbox))
		pdfapp_invert(app, fz_transform_rect(&hitbox, &ctm));
}

static int match(char *s, fz_text_page *page, int n)
{
	int orig = n;
	int c;
	while ((c = *s++))
	{
		if (c == ' ' && charat(page, n) == ' ')
		{
			while (charat(page, n) == ' ')
				n++;
		}
		else
		{
			if (tolower(c) != tolower(charat(page, n)))
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

		pdfapp_showpage(app, 1, 0, 0, 0);
		*panto = PAN_TO_TOP;

	} while (app->pageno != startpage);

	if (app->pageno == startpage)
		pdfapp_warn(app, "String '%s' not found.", app->search);
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

		pdfapp_showpage(app, 1, 0, 0, 0);
		*panto = PAN_TO_BOTTOM;

	} while (app->pageno != startpage);

	if (app->pageno == startpage)
		pdfapp_warn(app, "String '%s' not found.", app->search);

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

					if (app->searchdir < 0)
					{
						if (app->pageno == 1)
							app->pageno = app->pagecount;
						else
							app->pageno--;
						pdfapp_showpage(app, 1, 1, 0, 0);
					}

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
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;
	case '-':
		app->resolution /= ZOOMSTEP;
		if (app->resolution < MINRES)
			app->resolution = MINRES;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;

	case 'W':
		app->resolution *= (double) app->winw / (double) fz_pixmap_width(app->ctx, app->image);
		if (app->resolution > MAXRES)
			app->resolution = MAXRES;
		else if (app->resolution < MINRES)
			app->resolution = MINRES;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;
	case 'H':
		app->resolution *= (double) app->winh / (double) fz_pixmap_height(app->ctx, app->image);
		if (app->resolution > MAXRES)
			app->resolution = MAXRES;
		else if (app->resolution < MINRES)
			app->resolution = MINRES;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;

	case 'L':
		app->rotate -= 90;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;
	case 'R':
		app->rotate += 90;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;

	case 'c':
		app->grayscale ^= 1;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;

	case 'i':
		app->invert ^= 1;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;

#ifndef NDEBUG
	case 'a':
		app->rotate -= 15;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;
	case 's':
		app->rotate += 15;
		pdfapp_showpage(app, 0, 1, 1, 0);
		break;
#endif

	/*
	 * Pan view, but don't need to repaint image
	 */

	case 'f':
		app->shrinkwrap = 0;
		winfullscreen(app, !app->fullscreen);
		app->fullscreen = !app->fullscreen;
		break;

	case 'w':
		if (app->fullscreen)
		{
			winfullscreen(app, 0);
			app->fullscreen = 0;
		}
		app->shrinkwrap = 1;
		app->panx = app->pany = 0;
		pdfapp_showpage(app, 0, 0, 1, 0);
		break;

	case 'h':
		app->panx += fz_pixmap_width(app->ctx, app->image) / 10;
		pdfapp_showpage(app, 0, 0, 1, 0);
		break;

	case 'j':
		app->pany -= fz_pixmap_height(app->ctx, app->image) / 10;
		pdfapp_showpage(app, 0, 0, 1, 0);
		break;

	case 'k':
		app->pany += fz_pixmap_height(app->ctx, app->image) / 10;
		pdfapp_showpage(app, 0, 0, 1, 0);
		break;

	case 'l':
		app->panx -= fz_pixmap_width(app->ctx, app->image) / 10;
		pdfapp_showpage(app, 0, 0, 1, 0);
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

	case 'p':
		app->presentation_mode = !app->presentation_mode;
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

	case '\b':
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
	 * Saving the file
	 */
	case 'S':
		pdfapp_save(app);
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

	case '?':
		app->isediting = 1;
		app->searchdir = -1;
		app->search[0] = 0;
		app->hit = -1;
		app->hitlen = 0;
		winrepaintsearch(app);
		break;

	case '/':
		app->isediting = 1;
		app->searchdir = 1;
		app->search[0] = 0;
		app->hit = -1;
		app->hitlen = 0;
		winrepaintsearch(app);
		break;

	case 'n':
		if (app->searchdir > 0)
			pdfapp_searchforward(app, &panto);
		else
			pdfapp_searchbackward(app, &panto);
		loadpage = 0;
		break;

	case 'N':
		if (app->searchdir > 0)
			pdfapp_searchbackward(app, &panto);
		else
			pdfapp_searchforward(app, &panto);
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
		pdfapp_showpage(app, loadpage, 1, 1, 1);
	}
}

void pdfapp_onmouse(pdfapp_t *app, int x, int y, int btn, int modifiers, int state)
{
	fz_context *ctx = app->ctx;
	fz_irect rect;
	fz_link *link;
	fz_matrix ctm;
	fz_point p;
	int processed = 0;

	fz_pixmap_bbox(app->ctx, app->image, &rect);
	p.x = x - app->panx + rect.x0;
	p.y = y - app->pany + rect.y0;

	pdfapp_viewctm(&ctm, app);
	fz_invert_matrix(&ctm, &ctm);

	fz_transform_point(&p, &ctm);

	if (btn == 1 && (state == 1 || state == -1))
	{
		fz_ui_event event;
		fz_interactive *idoc = fz_interact(app->doc);

		event.etype = FZ_EVENT_TYPE_POINTER;
		event.event.pointer.pt = p;
		if (state == 1)
			event.event.pointer.ptype = FZ_POINTER_DOWN;
		else /* state == -1 */
			event.event.pointer.ptype = FZ_POINTER_UP;

		if (idoc && fz_pass_event(idoc, app->page, &event))
		{
			fz_widget *widget;

			widget = fz_focused_widget(idoc);

			app->nowaitcursor = 1;
			pdfapp_updatepage(app);

			if (widget)
			{
				switch (fz_widget_get_type(widget))
				{
				case FZ_WIDGET_TYPE_TEXT:
					{
						char *text = fz_text_widget_text(idoc, widget);
						char *current_text = text;
						int retry = 0;

						do
						{
							current_text = wintextinput(app, current_text, retry);
							retry = 1;
						}
						while (current_text && !fz_text_widget_set_text(idoc, widget, current_text));

						fz_free(app->ctx, text);
						pdfapp_updatepage(app);
					}
					break;

				case FZ_WIDGET_TYPE_LISTBOX:
				case FZ_WIDGET_TYPE_COMBOBOX:
					{
						int nopts;
						int nvals;
						char **opts = NULL;
						char **vals = NULL;

						fz_var(opts);
						fz_var(vals);

						fz_try(ctx)
						{
							nopts = fz_choice_widget_options(idoc, widget, NULL);
							opts = fz_malloc(ctx, nopts * sizeof(*opts));
							(void)fz_choice_widget_options(idoc, widget, opts);

							nvals = fz_choice_widget_value(idoc, widget, NULL);
							vals = fz_malloc(ctx, MAX(nvals,nopts) * sizeof(*vals));
							(void)fz_choice_widget_value(idoc, widget, vals);

							if (winchoiceinput(app, nopts, opts, &nvals, vals))
							{
								fz_choice_widget_set_value(idoc, widget, nvals, vals);
								pdfapp_updatepage(app);
							}
						}
						fz_always(ctx)
						{
							fz_free(ctx, opts);
							fz_free(ctx, vals);
						}
						fz_catch(ctx)
						{
							pdfapp_warn(app, "setting of choice failed");
						}
					}
					break;
				}
			}

			app->nowaitcursor = 0;
			processed = 1;
		}
	}

	for (link = app->page_links; link; link = link->next)
	{
		if (p.x >= link->rect.x0 && p.x <= link->rect.x1)
			if (p.y >= link->rect.y0 && p.y <= link->rect.y1)
				break;
	}

	if (link)
	{
		wincursor(app, HAND);
		if (btn == 1 && state == 1 && !processed)
		{
			if (link->dest.kind == FZ_LINK_URI)
				pdfapp_gotouri(app, link->dest.ld.uri.uri);
			else if (link->dest.kind == FZ_LINK_GOTO)
				pdfapp_gotopage(app, link->dest.ld.gotor.page);
			return;
		}
	}
	else
	{
		fz_annot *annot;
		for (annot = fz_first_annot(app->doc, app->page); annot; annot = fz_next_annot(app->doc, annot))
		{
			fz_rect rect;
			fz_bound_annot(app->doc, annot, &rect);
			if (x >= rect.x0 && x < rect.x1)
				if (y >= rect.y0 && y < rect.y1)
					break;
		}
		if (annot)
			wincursor(app, CARET);
		else
			wincursor(app, ARROW);
	}

	if (state == 1 && !processed)
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
				pdfapp_showpage(app, 0, 1, 1, 0);
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
			app->selr.x0 = fz_mini(app->selx, x) - app->panx + rect.x0;
			app->selr.x1 = fz_maxi(app->selx, x) - app->panx + rect.x0;
			app->selr.y0 = fz_mini(app->sely, y) - app->pany + rect.y0;
			app->selr.y1 = fz_maxi(app->sely, y) - app->pany + rect.y0;
			winrepaint(app);
			if (app->selr.x0 < app->selr.x1 && app->selr.y0 < app->selr.y1)
				windocopy(app);
		}
		app->ispanning = 0;
	}

	else if (app->ispanning)
	{
		int newx = app->panx + x - app->selx;
		int newy = app->pany + y - app->sely;
		/* Scrolling beyond limits implies flipping pages */
		/* Are we requested to scroll beyond limits? */
		if (newy + fz_pixmap_height(app->ctx, app->image) < app->winh || newy > 0)
		{
			/* Yes. We can assume that deltay != 0 */
			int deltay = y - app->sely;
			/* Check whether the panning has occurred in the
			 * direction that we are already crossing the
			 * limit it. If not, we can conclude that we
			 * have switched ends of the page and will thus
			 * start over counting.
			 */
			if( app->beyondy == 0 || (app->beyondy ^ deltay) >= 0 )
			{
				/* Updating how far we are beyond and
				 * flipping pages if beyond threshold
				 */
				app->beyondy += deltay;
				if (app->beyondy > BEYOND_THRESHHOLD)
				{
					if( app->pageno > 1 )
					{
						app->pageno--;
						pdfapp_showpage(app, 1, 1, 1, 0);
						newy = -fz_pixmap_height(app->ctx, app->image);
					}
					app->beyondy = 0;
				}
				else if (app->beyondy < -BEYOND_THRESHHOLD)
				{
					if( app->pageno < app->pagecount )
					{
						app->pageno++;
						pdfapp_showpage(app, 1, 1, 1, 0);
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
		app->selr.x0 = fz_mini(app->selx, x) - app->panx + rect.x0;
		app->selr.x1 = fz_maxi(app->selx, x) - app->panx + rect.x0;
		app->selr.y0 = fz_mini(app->sely, y) - app->pany + rect.y0;
		app->selr.y1 = fz_maxi(app->sely, y) - app->pany + rect.y0;
		winrepaint(app);
	}

}

void pdfapp_oncopy(pdfapp_t *app, unsigned short *ucsbuf, int ucslen)
{
	fz_rect hitbox;
	fz_matrix ctm;
	fz_text_page *page = app->page_text;
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int c, i, p;
	int seen = 0;

	int x0 = app->selr.x0;
	int x1 = app->selr.x1;
	int y0 = app->selr.y0;
	int y1 = app->selr.y1;

	pdfapp_viewctm(&ctm, app);

	p = 0;

	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				if (seen)
				{
#ifdef _WIN32
					if (p < ucslen - 1)
						ucsbuf[p++] = '\r';
#endif
					if (p < ucslen - 1)
						ucsbuf[p++] = '\n';
				}

				seen = 0;

				for (i = 0; i < span->len; i++)
				{
					hitbox = span->text[i].bbox;
					fz_transform_rect(&hitbox, &ctm);
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

				seen = (seen && span + 1 == line->spans + line->len);
			}
		}
	}

	ucsbuf[p] = 0;
}

void pdfapp_postblit(pdfapp_t *app)
{
	clock_t time;
	float seconds;
	int llama;

	app->transitions_enabled = 1;
	if (!app->in_transit)
		return;
	time = clock();
	seconds = (float)(time - app->start_time) / CLOCKS_PER_SEC;
	llama = seconds * 256 / app->transition.duration;
	if (llama >= 256)
	{
		/* Completed. */
		app->in_transit = 0;
		fz_drop_pixmap(app->ctx, app->image);
		app->image = app->new_image;
		app->new_image = NULL;
		fz_drop_pixmap(app->ctx, app->old_image);
		app->old_image = NULL;
		if (app->duration != 0)
			winadvancetimer(app, app->duration);
	}
	else
		fz_generate_transition(app->image, app->old_image, app->new_image, llama, &app->transition);
	winrepaint(app);
}
