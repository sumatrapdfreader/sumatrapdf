#ifndef PDFAPP_H
#define PDFAPP_H

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <time.h>

/*
 * Utility object for handling a pdf application / view
 * Takes care of PDF loading and displaying and navigation,
 * uses a number of callbacks to the GUI app.
 */

/* 25% .. 1600% */
#define MINRES 18
#define MAXRES 1152

typedef struct pdfapp_s pdfapp_t;

enum { ARROW, HAND, WAIT, CARET };

enum { DISCARD, SAVE, CANCEL };

enum { QUERY_NO, QUERY_YES };

extern void winwarn(pdfapp_t*, char *s);
extern void winerror(pdfapp_t*, char *s);
extern void wintitle(pdfapp_t*, char *title);
extern void winresize(pdfapp_t*, int w, int h);
extern void winrepaint(pdfapp_t*);
extern void winrepaintsearch(pdfapp_t*);
extern char *winpassword(pdfapp_t*, char *filename);
extern char *wintextinput(pdfapp_t*, char *inittext, int retry);
extern int winchoiceinput(pdfapp_t*, int nopts, const char *opts[], int *nvals, const char *vals[]);
extern void winopenuri(pdfapp_t*, char *s);
extern void wincursor(pdfapp_t*, int curs);
extern void windocopy(pdfapp_t*);
extern void windrawstring(pdfapp_t*, int x, int y, char *s);
extern void winclose(pdfapp_t*);
extern void winhelp(pdfapp_t*);
extern void winfullscreen(pdfapp_t*, int state);
extern int winsavequery(pdfapp_t*);
extern int winquery(pdfapp_t*, const char*);
extern int wingetcertpath(pdfapp_t *, char *buf, int len);
extern int wingetsavepath(pdfapp_t*, char *buf, int len);
extern void winalert(pdfapp_t *, pdf_alert_event *alert);
extern void winprint(pdfapp_t *);
extern void winadvancetimer(pdfapp_t *, float duration);
extern void winreplacefile(pdfapp_t *, char *source, char *target);
extern void wincopyfile(pdfapp_t *, char *source, char *target);
extern void winreloadpage(pdfapp_t *);

struct pdfapp_s
{
	/* current document params */
	fz_document *doc;
	char *docpath;
	char *doctitle;
	fz_outline *outline;
	int outline_deferred;

	float layout_w;
	float layout_h;
	float layout_em;
	char *layout_css;
	int layout_use_doc_css;

	int pagecount;

	/* current view params */
	float default_resolution;
	float resolution;
	int rotate;
	fz_pixmap *image;
	int imgw, imgh;
	int grayscale;
	fz_colorspace *colorspace;
	int invert;
	int tint, tint_white;
	int useicc;
	int useseparations;
	int aalevel;

	/* presentation mode */
	int presentation_mode;
	int transitions_enabled;
	fz_pixmap *old_image;
	fz_pixmap *new_image;
	clock_t start_time;
	int in_transit;
	float duration;
	fz_transition transition;

	/* current page params */
	int pageno;
	fz_page *page;
	fz_rect page_bbox;
	fz_display_list *page_list;
	fz_display_list *annotations_list;
	fz_stext_page *page_text;
	fz_link *page_links;
	int errored;
	int incomplete;

	/* separations */
	fz_separations *seps;

	/* snapback history */
	int hist[256];
	int histlen;
	int marks[10];

	/* window system sizes */
	int winw, winh;
	int scrw, scrh;
	int shrinkwrap;
	int fullscreen;

	/* event handling state */
	char number[256];
	int numberlen;

	int ispanning;
	int panx, pany;

	int iscopying;
	int selx, sely;
	/* TODO - While sely keeps track of the relative change in
	 * cursor position between two ticks/events, beyondy shall keep
	 * track of the relative change in cursor position from the
	 * point where the user hits a scrolling limit. This is ugly.
	 * Used in pdfapp.c:pdfapp_onmouse.
	 */
	int beyondy;
	fz_rect selr;

	int nowaitcursor;

	/* search state */
	int issearching;
	int searchdir;
	char search[512];
	int searchpage;
	fz_quad hit_bbox[512];
	int hit_count;

	/* client context storage */
	void *userdata;

	fz_context *ctx;
#ifdef HAVE_CURL
	fz_stream *stream;
#endif
};

void pdfapp_init(fz_context *ctx, pdfapp_t *app);
void pdfapp_setresolution(pdfapp_t *app, int res);
void pdfapp_open(pdfapp_t *app, char *filename, int reload);
void pdfapp_open_progressive(pdfapp_t *app, char *filename, int reload, int kbps);
void pdfapp_close(pdfapp_t *app);
int pdfapp_preclose(pdfapp_t *app);
void pdfapp_reloadfile(pdfapp_t *app);

char *pdfapp_version(pdfapp_t *app);
char *pdfapp_usage(pdfapp_t *app);

void pdfapp_onkey(pdfapp_t *app, int c, int modifiers);
void pdfapp_onmouse(pdfapp_t *app, int x, int y, int btn, int modifiers, int state);
void pdfapp_oncopy(pdfapp_t *app, unsigned short *ucsbuf, int ucslen);
void pdfapp_onresize(pdfapp_t *app, int w, int h);
void pdfapp_gotopage(pdfapp_t *app, int number);
void pdfapp_reloadpage(pdfapp_t *app);
void pdfapp_autozoom_horizontal(pdfapp_t *app);
void pdfapp_autozoom_vertical(pdfapp_t *app);
void pdfapp_autozoom(pdfapp_t *app);

void pdfapp_invert(pdfapp_t *app, fz_rect rect);
void pdfapp_inverthit(pdfapp_t *app);

void pdfapp_postblit(pdfapp_t *app);

void pdfapp_warn(pdfapp_t *app, const char *fmt, ...);
void pdfapp_error(pdfapp_t *app, char *msg);

#endif
