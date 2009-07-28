#include "fitz.h"
#include "mupdf.h"
#include "pdfapp.h"

#include "gs_l.xbm"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef timeradd
#define timeradd(a, b, result) \
    do { \
	(result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
	(result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
	if ((result)->tv_usec >= 1000000) \
	{ \
	    ++(result)->tv_sec; \
	    (result)->tv_usec -= 1000000; \
	} \
    } while (0)
#endif

#ifndef timersub
#define timersub(a, b, result) \
    do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if ((result)->tv_usec < 0) { \
	    --(result)->tv_sec; \
	    (result)->tv_usec += 1000000; \
	} \
    } while (0)
#endif

extern int ximage_init(Display *display, int screen, Visual *visual);
extern int ximage_get_depth(void);
extern Visual *ximage_get_visual(void);
extern Colormap ximage_get_colormap(void);
extern void ximage_blit(Drawable d, GC gc, int dstx, int dsty,
	unsigned char *srcdata,
	int srcx, int srcy, int srcw, int srch, int srcstride);

static Display *xdpy;
static Atom XA_TARGETS;
static Atom XA_TIMESTAMP;
static Atom XA_UTF8_STRING;
static int x11fd;
static int xscr;
static Window xwin;
static GC xgc;
static XEvent xevt;
static int mapped = 0;
static Cursor xcarrow, xchand, xcwait;
static int justcopied = 0;
static int isshowingpage = 0;
static int dirty = 0;
static char *password = "";
static XColor xbgcolor;
static XColor xshcolor;
static int reqw = 0;
static int reqh = 0;
static char copylatin1[1024 * 16] = "";
static char copyutf8[1024 * 48] = "";
static Time copytime;

static pdfapp_t gapp;

/*
 * Dialog boxes
 */

void winwarn(pdfapp_t *app, char *msg)
{
	fprintf(stderr, "mupdf: %s\n", msg);
}

void winerror(pdfapp_t *app, fz_error error)
{
	fz_catch(error, "aborting");
	exit(1);
}

char *winpassword(pdfapp_t *app, char *filename)
{
	char *r = password;
	password = NULL;
	return r;
}

/*
 * X11 magic
 */

static void winopen(void)
{
	XWMHints *wmhints;
	XClassHint *classhint;

	xdpy = XOpenDisplay(nil);
	if (!xdpy)
		winerror(&gapp, fz_throw("could not open display"));

	XA_TARGETS = XInternAtom(xdpy, "TARGETS", False);
	XA_TIMESTAMP = XInternAtom(xdpy, "TIMESTAMP", False);
	XA_UTF8_STRING = XInternAtom(xdpy, "UTF8_STRING", False);

	xscr = DefaultScreen(xdpy);

	ximage_init(xdpy, xscr, DefaultVisual(xdpy, xscr));

	xcarrow = XCreateFontCursor(xdpy, XC_left_ptr);
	xchand = XCreateFontCursor(xdpy, XC_hand2);
	xcwait = XCreateFontCursor(xdpy, XC_watch);

	xbgcolor.red = 0x7000;
	xbgcolor.green = 0x7000;
	xbgcolor.blue = 0x7000;

	xshcolor.red = 0x4000;
	xshcolor.green = 0x4000;
	xshcolor.blue = 0x4000;

	XAllocColor(xdpy, DefaultColormap(xdpy, xscr), &xbgcolor);
	XAllocColor(xdpy, DefaultColormap(xdpy, xscr), &xshcolor);

	xwin = XCreateWindow(xdpy, DefaultRootWindow(xdpy),
			10, 10, 200, 100, 1,
			ximage_get_depth(),
			InputOutput,
			ximage_get_visual(),
			0,
			nil);

	XSetWindowColormap(xdpy, xwin, ximage_get_colormap());
	XSelectInput(xdpy, xwin,
			StructureNotifyMask | ExposureMask | KeyPressMask |
			PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

	mapped = 0;

	xgc = XCreateGC(xdpy, xwin, 0, nil);

	XDefineCursor(xdpy, xwin, xcarrow);

	wmhints = XAllocWMHints();
	if (wmhints)
	{
		wmhints->flags = IconPixmapHint;
		wmhints->icon_pixmap = XCreateBitmapFromData(xdpy, xwin,
				(char *) gs_l_xbm_bits, gs_l_xbm_width, gs_l_xbm_height);
		if (wmhints->icon_pixmap)
		{
			XSetWMHints(xdpy, xwin, wmhints);
		}
		XFree(wmhints);
	}

	classhint = XAllocClassHint();
	if (classhint)
	{
		classhint->res_name = "mupdf";
		classhint->res_class = "MuPDF";
		XSetClassHint(xdpy, xwin, classhint);
		XFree(classhint);
	}

	x11fd = ConnectionNumber(xdpy);
}

void wincursor(pdfapp_t *app, int curs)
{
	if (curs == ARROW)
		XDefineCursor(xdpy, xwin, xcarrow);
	if (curs == HAND)
		XDefineCursor(xdpy, xwin, xchand);
	if (curs == WAIT)
		XDefineCursor(xdpy, xwin, xcwait);
	XFlush(xdpy);
}

void wintitle(pdfapp_t *app, char *s)
{
#ifdef X_HAVE_UTF8_STRING
	Xutf8SetWMProperties(xdpy, xwin, s, s, nil, 0, nil, nil, nil);
#else
	XmbSetWMProperties(xdpy, xwin, s, s, nil, 0, nil, nil, nil);
#endif
}

void winconvert(pdfapp_t *app, fz_pixmap *image)
{
	/* never mind */
}

void winresize(pdfapp_t *app, int w, int h)
{
	XWindowChanges values;
	int mask;

	mask = CWWidth | CWHeight;
	values.width = w;
	values.height = h;
	XConfigureWindow(xdpy, xwin, mask, &values);

	reqw = w;
	reqh = h;

	if (!mapped)
	{
		gapp.winw = w;
		gapp.winh = h;

		XMapWindow(xdpy, xwin);
		XFlush(xdpy);

		while (1)
		{
			XNextEvent(xdpy, &xevt);
			if (xevt.type == MapNotify)
				break;
		}

		XSetForeground(xdpy, xgc, WhitePixel(xdpy, xscr));
		XFillRectangle(xdpy, xwin, xgc, 0, 0, gapp.image->w, gapp.image->h);
		XFlush(xdpy);

		mapped = 1;
	}
}

static void fillrect(int x, int y, int w, int h)
{
	if (w > 0 && h > 0)
		XFillRectangle(xdpy, xwin, xgc, x, y, w, h);
}

static void invertcopyrect()
{
	unsigned *p;
	int x, y;

	int x0 = gapp.selr.x0 - gapp.panx;
	int x1 = gapp.selr.x1 - gapp.panx;
	int y0 = gapp.selr.y0 - gapp.pany;
	int y1 = gapp.selr.y1 - gapp.pany;

	x0 = CLAMP(x0, 0, gapp.image->w - 1);
	x1 = CLAMP(x1, 0, gapp.image->w - 1);
	y0 = CLAMP(y0, 0, gapp.image->h - 1);
	y1 = CLAMP(y1, 0, gapp.image->h - 1);

	for (y = y0; y < y1; y++)
	{
		p = (unsigned *)(gapp.image->samples + (y * gapp.image->w + x0) * 4);
		for (x = x0; x < x1; x++)
		{
			*p = ~0 - *p;
			p ++;
		}
	}

	justcopied = 1;
}

static void winblit(pdfapp_t *app)
{
	int x0 = gapp.panx;
	int y0 = gapp.pany;
	int x1 = gapp.panx + gapp.image->w;
	int y1 = gapp.pany + gapp.image->h;

	XSetForeground(xdpy, xgc, xbgcolor.pixel);
	fillrect(0, 0, x0, gapp.winh);
	fillrect(x1, 0, gapp.winw - x1, gapp.winh);
	fillrect(0, 0, gapp.winw, y0);
	fillrect(0, y1, gapp.winw, gapp.winh - y1);

	XSetForeground(xdpy, xgc, xshcolor.pixel);
	fillrect(x0+2, y1, gapp.image->w, 2);
	fillrect(x1, y0+2, 2, gapp.image->h);

	if (gapp.iscopying || justcopied)
		invertcopyrect();

	ximage_blit(xwin, xgc,
			x0, y0,
			gapp.image->samples,
			0, 0,
			gapp.image->w,
			gapp.image->h,
			gapp.image->w * gapp.image->n);

	if (gapp.iscopying || justcopied)
		invertcopyrect();
}

void winrepaint(pdfapp_t *app)
{
	dirty = 1;
}

static void windrawstring(pdfapp_t *app, char *s, int x, int y)
{
	int prevfunction;
	XGCValues xgcv;

	XGetGCValues(xdpy, xgc, GCFunction, &xgcv);
	prevfunction = xgcv.function;
	xgcv.function = GXxor;
	XChangeGC(xdpy, xgc, GCFunction, &xgcv);

	XSetForeground(xdpy, xgc, WhitePixel(xdpy, DefaultScreen(xdpy)));

	XDrawString(xdpy, xwin, xgc, x, y, s, strlen(s));
	XFlush(xdpy);

	XGetGCValues(xdpy, xgc, GCFunction, &xgcv);
	xgcv.function = prevfunction;
	XChangeGC(xdpy, xgc, GCFunction, &xgcv);
}

static void windrawpageno(pdfapp_t *app)
{
	char s[100];

	int ret = snprintf(s, 100, "Page %d/%d", gapp.pageno, gapp.pagecount);
	if (ret >= 0)
	{
		isshowingpage = 1;
		windrawstring(&gapp, s, 10, 20);
	}
}

void windocopy(pdfapp_t *app)
{
	unsigned short copyucs2[16 * 1024];
	char *latin1 = copylatin1;
	char *utf8 = copyutf8;
	unsigned short *ucs2;
	int ucs;

	pdfapp_oncopy(&gapp, copyucs2, 16 * 1024);

	for (ucs2 = copyucs2; ucs2[0] != 0; ucs2++)
	{
		ucs = ucs2[0];

		utf8 += runetochar(utf8, &ucs);

		if (ucs < 256)
			*latin1++ = ucs;
		else
			*latin1++ = '?';
	}

	*utf8 = 0;
	*latin1 = 0;

printf("oncopy utf8=%zd latin1=%zd\n", strlen(copyutf8), strlen(copylatin1));

	XSetSelectionOwner(xdpy, XA_PRIMARY, xwin, copytime);

	justcopied = 1;
}

void onselreq(Window requestor, Atom selection, Atom target, Atom property, Time time)
{
	XEvent nevt;

printf("onselreq\n");

	if (property == None)
		property = target;

	nevt.xselection.type = SelectionNotify;
	nevt.xselection.send_event = True;
	nevt.xselection.display = xdpy;
	nevt.xselection.requestor = requestor;
	nevt.xselection.selection = selection;
	nevt.xselection.target = target;
	nevt.xselection.property = property;
	nevt.xselection.time = time;

	if (target == XA_TARGETS)
	{
		Atom atomlist[4];
		atomlist[0] = XA_TARGETS;
		atomlist[1] = XA_TIMESTAMP;
		atomlist[2] = XA_STRING;
		atomlist[3] = XA_UTF8_STRING;
printf(" -> targets\n");
		XChangeProperty(xdpy, requestor, property, target,
				32, PropModeReplace,
				(unsigned char *)atomlist, sizeof(atomlist)/sizeof(Atom));
	}

	else if (target == XA_STRING)
	{
printf(" -> string %zd\n", strlen(copylatin1));
		XChangeProperty(xdpy, requestor, property, target,
				8, PropModeReplace,
				(unsigned char *)copylatin1, strlen(copylatin1));
	}

	else if (target == XA_UTF8_STRING)
	{
printf(" -> utf8string\n");
		XChangeProperty(xdpy, requestor, property, target,
				8, PropModeReplace,
				(unsigned char *)copyutf8, strlen(copyutf8));
	}

	else
	{
printf(" -> unknown\n");
		nevt.xselection.property = None;
	}

	XSendEvent(xdpy, requestor, False, SelectionNotify, &nevt);
}

void winopenuri(pdfapp_t *app, char *buf)
{
	char cmd[2048];
	if (getenv("BROWSER"))
		sprintf(cmd, "$BROWSER %s &", buf);
	else
		sprintf(cmd, "open %s", buf);
	system(cmd);
}

static void onkey(int c)
{
	if (justcopied)
	{
		justcopied = 0;
		winrepaint(&gapp);
	}

	if (c == 'P')
		windrawpageno(&gapp);
	else if (c == 'q')
		exit(0);
	else
		pdfapp_onkey(&gapp, c);
}

static void onmouse(int x, int y, int btn, int modifiers, int state)
{
	if (state != 0 && justcopied)
	{
		justcopied = 0;
		winrepaint(&gapp);
	}

	pdfapp_onmouse(&gapp, x, y, btn, modifiers, state);
}

static void usage(void)
{
	fprintf(stderr, "usage: mupdf [-d password] [-z zoom%%] [-p pagenumber] file.pdf\n");
	exit(1);
}

static void winawaitevent(struct timeval *tmo, struct timeval *tmo_at)
{
	if (tmo_at->tv_sec == 0 && tmo_at->tv_usec == 0 &&
		tmo->tv_sec == 0 && tmo->tv_usec == 0)
		XNextEvent(xdpy, &xevt);
	else
	{
		fd_set fds;
		struct timeval now;

		FD_ZERO(&fds);
		FD_SET(x11fd, &fds);

		if (select(x11fd + 1, &fds, nil, nil, tmo))
		{
			gettimeofday(&now, nil);
			timersub(tmo_at, &now, tmo);
			XNextEvent(xdpy, &xevt);
		}
	}
}

static void winsettmo(struct timeval *tmo, struct timeval *tmo_at)
{
	struct timeval now;

	tmo->tv_sec = 2;
	tmo->tv_usec = 0;

	gettimeofday(&now, nil);
	timeradd(&now, tmo, tmo_at);
}

static void winresettmo(struct timeval *tmo, struct timeval *tmo_at)
{
	tmo->tv_sec = 0;
	tmo->tv_usec = 0;

	tmo_at->tv_sec = 0;
	tmo_at->tv_usec = 0;
}

int main(int argc, char **argv)
{
	char *filename;
	int c;
	int len;
	char buf[128];
	KeySym keysym;
	int oldx = 0;
	int oldy = 0;
	int zoom = 100;
	int pageno = 1;
	int wasshowingpage;
	struct timeval tmo, tmo_at;

	while ((c = fz_getopt(argc, argv, "d:z:p:")) != -1)
	{
		switch (c)
		{
		case 'd': password = fz_optarg; break;
		case 'z': zoom = atoi(fz_optarg); break;
		case 'p': pageno = atoi(fz_optarg); break;
		default: usage();
		}
	}

	if (zoom < 100)
		zoom = 100;
	if (zoom > 300)
		zoom = 300;

	if (argc - fz_optind == 0)
		usage();

	filename = argv[fz_optind++];

	fz_cpudetect();
	fz_accelerate();

	winopen();

	pdfapp_init(&gapp);
	gapp.scrw = DisplayWidth(xdpy, xscr);
	gapp.scrh = DisplayHeight(xdpy, xscr);
	gapp.zoom = zoom / 100.0;
	gapp.pageno = pageno;

	pdfapp_open(&gapp, filename);

	winresettmo(&tmo, &tmo_at);

	while (1)
	{
		do
		{
			winawaitevent(&tmo, &tmo_at);

			if (tmo_at.tv_sec != 0 && tmo_at.tv_usec != 0 &&
				tmo.tv_sec == 0 && tmo.tv_usec == 0)
			{
				/* redraw page */
				winblit(&gapp);
				isshowingpage = 0;
				winresettmo(&tmo, &tmo_at);
				continue;
			}

			switch (xevt.type)
			{
			case Expose:
				dirty = 1;
				break;

			case ConfigureNotify:
				if (gapp.image)
				{
					if (xevt.xconfigure.width != reqw ||
						xevt.xconfigure.height != reqh)
						gapp.shrinkwrap = 0;
				}
				pdfapp_onresize(&gapp,
						xevt.xconfigure.width,
						xevt.xconfigure.height);
				break;

			case KeyPress:
				wasshowingpage = isshowingpage;

				len = XLookupString(&xevt.xkey, buf, sizeof buf, &keysym, nil);
				if (len)
					onkey(buf[0]);
				onmouse(oldx, oldy, 0, 0, 0);

				if (dirty)
				{
					winblit(&gapp);
					dirty = 0;
					if (isshowingpage)
					{
						isshowingpage = 0;
						winresettmo(&tmo, &tmo_at);
					}
				}

				if (!wasshowingpage && isshowingpage)
					winsettmo(&tmo, &tmo_at);

				break;

			case MotionNotify:
				oldx = xevt.xbutton.x;
				oldy = xevt.xbutton.y;
				onmouse(xevt.xbutton.x, xevt.xbutton.y, xevt.xbutton.button, xevt.xbutton.state, 0);
				break;

			case ButtonPress:
				onmouse(xevt.xbutton.x, xevt.xbutton.y, xevt.xbutton.button, xevt.xbutton.state, 1);
				break;

			case ButtonRelease:
				copytime = xevt.xbutton.time;
				onmouse(xevt.xbutton.x, xevt.xbutton.y, xevt.xbutton.button, xevt.xbutton.state, -1);
				break;

			case SelectionRequest:
				onselreq(xevt.xselectionrequest.requestor,
						xevt.xselectionrequest.selection,
						xevt.xselectionrequest.target,
						xevt.xselectionrequest.property,
						xevt.xselectionrequest.time);
				break;
			}
		}
		while (XPending(xdpy));

		if (dirty)
		{
			winblit(&gapp);
			dirty = 0;
			if (isshowingpage)
			{
				isshowingpage = 0;
				winresettmo(&tmo, &tmo_at);
			}
		}
	}

	pdfapp_close(&gapp);

	return 0;
}

