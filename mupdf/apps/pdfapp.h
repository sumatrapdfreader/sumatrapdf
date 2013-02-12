#ifndef PDFAPP_H
#define PDFAPP_H

#include "fitz.h"

/*
 * Utility object for handling a pdf application / view
 * Takes care of PDF loading and displaying and navigation,
 * uses a number of callbacks to the GUI app.
 */

#define MINRES 54
#define MAXRES 300

typedef struct pdfapp_s pdfapp_t;

enum { ARROW, HAND, WAIT, CARET };

enum { DISCARD, SAVE, CANCEL };

extern void winwarn(pdfapp_t*, char *s);
extern void winerror(pdfapp_t*, char *s);
extern void wintitle(pdfapp_t*, char *title);
extern void winresize(pdfapp_t*, int w, int h);
extern void winrepaint(pdfapp_t*);
extern void winrepaintsearch(pdfapp_t*);
extern char *winpassword(pdfapp_t*, char *filename);
extern char *wintextinput(pdfapp_t*, char *inittext, int retry);
extern int winchoiceinput(pdfapp_t*, int nopts, char *opts[], int *nvals, char *vals[]);
extern void winopenuri(pdfapp_t*, char *s);
extern void wincursor(pdfapp_t*, int curs);
extern void windocopy(pdfapp_t*);
extern void winreloadfile(pdfapp_t*);
extern void windrawstring(pdfapp_t*, int x, int y, char *s);
extern void winclose(pdfapp_t*);
extern void winhelp(pdfapp_t*);
extern void winfullscreen(pdfapp_t*, int state);
extern int winsavequery(pdfapp_t*);
extern int wingetsavepath(pdfapp_t*, char *buf, int len);
extern void winalert(pdfapp_t *, fz_alert_event *alert);
extern void winprint(pdfapp_t *);
extern void winadvancetimer(pdfapp_t *, float duration);
extern void winreplacefile(char *source, char *target);

struct pdfapp_s
{
	/* current document params */
	fz_document *doc;
	char *docpath;
	char *doctitle;
	fz_outline *outline;

	int pagecount;

	/* current view params */
	int resolution;
	int rotate;
	fz_pixmap *image;
	int grayscale;
	fz_colorspace *colorspace;
	int invert;

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
	fz_text_page *page_text;
	fz_text_sheet *page_sheet;
	fz_link *page_links;
	int errored;

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
	int isediting;
	int searchdir;
	char search[512];
	int hit;
	int hitlen;

	/* client context storage */
	void *userdata;

	fz_context *ctx;
};

void pdfapp_init(fz_context *ctx, pdfapp_t *app);
void pdfapp_open(pdfapp_t *app, char *filename, int reload);
void pdfapp_close(pdfapp_t *app);
int pdfapp_preclose(pdfapp_t *app);

char *pdfapp_version(pdfapp_t *app);
char *pdfapp_usage(pdfapp_t *app);

void pdfapp_onkey(pdfapp_t *app, int c);
void pdfapp_onmouse(pdfapp_t *app, int x, int y, int btn, int modifiers, int state);
void pdfapp_oncopy(pdfapp_t *app, unsigned short *ucsbuf, int ucslen);
void pdfapp_onresize(pdfapp_t *app, int w, int h);
void pdfapp_gotopage(pdfapp_t *app, int number);

void pdfapp_invert(pdfapp_t *app, const fz_rect *rect);
void pdfapp_inverthit(pdfapp_t *app);

void pdfapp_postblit(pdfapp_t *app);

#endif
