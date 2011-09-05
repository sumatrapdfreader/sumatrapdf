/*
 * Utility object for handling a pdf application / view
 * Takes care of PDF loading and displaying and navigation,
 * uses a number of callbacks to the GUI app.
 */

#define MINRES 54
#define MAXRES 300

typedef struct pdfapp_s pdfapp_t;

enum { ARROW, HAND, WAIT };

extern void winwarn(pdfapp_t*, char *s);
extern void winerror(pdfapp_t*, fz_error error);
extern void wintitle(pdfapp_t*, char *title);
extern void winresize(pdfapp_t*, int w, int h);
extern void winrepaint(pdfapp_t*);
extern void winrepaintsearch(pdfapp_t*);
extern char *winpassword(pdfapp_t*, char *filename);
extern void winopenuri(pdfapp_t*, char *s);
extern void wincursor(pdfapp_t*, int curs);
extern void windocopy(pdfapp_t*);
extern void winreloadfile(pdfapp_t*);
extern void windrawstring(pdfapp_t*, int x, int y, char *s);
extern void winclose(pdfapp_t*);
extern void winhelp(pdfapp_t*);

struct pdfapp_s
{
	/* current document params */
	char *doctitle;
	pdf_xref *xref;
	pdf_outline *outline;
	xps_context *xps;

	int pagecount;
	fz_glyph_cache *cache;

	/* current view params */
	int resolution;
	int rotate;
	fz_pixmap *image;
	int grayscale;

	/* current page params */
	int pageno;
	fz_rect page_bbox;
	float page_rotate;
	fz_display_list *page_list;
	fz_text_span *page_text;
	pdf_link *page_links;

	/* snapback history */
	int hist[256];
	int histlen;
	int marks[10];

	/* window system sizes */
	int winw, winh;
	int scrw, scrh;
	int shrinkwrap;

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
	fz_bbox selr;

	/* search state */
	int isediting;
	char search[512];
	int hit;
	int hitlen;

	/* client context storage */
	void *userdata;
};

void pdfapp_init(pdfapp_t *app);
void pdfapp_open(pdfapp_t *app, char *filename, int fd, int reload);
void pdfapp_close(pdfapp_t *app);

char *pdfapp_version(pdfapp_t *app);
char *pdfapp_usage(pdfapp_t *app);

void pdfapp_onkey(pdfapp_t *app, int c);
void pdfapp_onmouse(pdfapp_t *app, int x, int y, int btn, int modifiers, int state);
void pdfapp_oncopy(pdfapp_t *app, unsigned short *ucsbuf, int ucslen);
void pdfapp_onresize(pdfapp_t *app, int w, int h);

void pdfapp_invert(pdfapp_t *app, fz_bbox rect);
void pdfapp_inverthit(pdfapp_t *app);
