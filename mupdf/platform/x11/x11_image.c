/*
 * Blit RGBA images to X with X(Shm)Images
 */

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 1
#endif

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 1
#endif

#define noSHOWINFO

#include "mupdf/fitz.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

extern int ffs(int);

static int is_big_endian(void)
{
	static const int one = 1;
	return *(char*)&one == 0;
}

typedef void (*ximage_convert_func_t)
(
	const unsigned char *src,
	int srcstride,
	unsigned char *dst,
	int dststride,
	int w,
	int h
	);

#define POOLSIZE 4
#define WIDTH 256
#define HEIGHT 256

enum {
	ARGB8888,
	BGRA8888,
	RGBA8888,
	ABGR8888,
	RGB888,
	BGR888,
	RGB565,
	RGB565_BR,
	RGB555,
	RGB555_BR,
	BGR233,
	UNKNOWN
};

#ifdef SHOWINFO
static char *modename[] = {
	"ARGB8888",
	"BGRA8888",
	"RGBA8888",
	"ABGR8888",
	"RGB888",
	"BGR888",
	"RGB565",
	"RGB565_BR",
	"RGB555",
	"RGB555_BR",
	"BGR233",
	"UNKNOWN"
};
#endif

extern ximage_convert_func_t ximage_convert_funcs[];

static struct
{
	Display *display;
	int screen;
	XVisualInfo visual;
	Colormap colormap;

	int bitsperpixel;
	int mode;

	XColor rgbcube[256];

	ximage_convert_func_t convert_func;

	int useshm;
	int shmcode;
	XImage *pool[POOLSIZE];
	/* MUST exist during the lifetime of the shared ximage according to the
	xc/doc/hardcopy/Xext/mit-shm.PS.gz */
	XShmSegmentInfo shminfo[POOLSIZE];
	int lastused;
} info;

static XImage *
createximage(Display *dpy, Visual *vis, XShmSegmentInfo *xsi, int depth, int w, int h)
{
	XImage *img;
	Status status;

	if (!XShmQueryExtension(dpy))
		goto fallback;
	if (!info.useshm)
		goto fallback;

	img = XShmCreateImage(dpy, vis, depth, ZPixmap, NULL, xsi, w, h);
	if (!img)
	{
		fprintf(stderr, "warn: could not XShmCreateImage\n");
		goto fallback;
	}

	xsi->shmid = shmget(IPC_PRIVATE,
		img->bytes_per_line * img->height,
		IPC_CREAT | 0777);
	if (xsi->shmid < 0)
	{
		XDestroyImage(img);
		fprintf(stderr, "warn: could not shmget\n");
		goto fallback;
	}

	img->data = xsi->shmaddr = shmat(xsi->shmid, NULL, 0);
	if (img->data == (char*)-1)
	{
		XDestroyImage(img);
		fprintf(stderr, "warn: could not shmat\n");
		goto fallback;
	}

	xsi->readOnly = False;
	status = XShmAttach(dpy, xsi);
	if (!status)
	{
		shmdt(xsi->shmaddr);
		XDestroyImage(img);
		fprintf(stderr, "warn: could not XShmAttach\n");
		goto fallback;
	}

	XSync(dpy, False);

	shmctl(xsi->shmid, IPC_RMID, NULL);

	return img;

fallback:
	info.useshm = 0;

	img = XCreateImage(dpy, vis, depth, ZPixmap, 0, NULL, w, h, 32, 0);
	if (!img)
	{
		fprintf(stderr, "fail: could not XCreateImage");
		abort();
	}

	img->data = malloc(h * img->bytes_per_line);
	if (!img->data)
	{
		fprintf(stderr, "fail: could not malloc");
		abort();
	}

	return img;
}

static void
make_colormap(void)
{
	if (info.visual.class == PseudoColor && info.visual.depth == 8)
	{
		int i, r, g, b;
		i = 0;
		for (b = 0; b < 4; b++) {
			for (g = 0; g < 8; g++) {
				for (r = 0; r < 8; r++) {
					info.rgbcube[i].pixel = i;
					info.rgbcube[i].red = (r * 36) << 8;
					info.rgbcube[i].green = (g * 36) << 8;
					info.rgbcube[i].blue = (b * 85) << 8;
					info.rgbcube[i].flags =
					DoRed | DoGreen | DoBlue;
					i++;
				}
			}
		}
		info.colormap = XCreateColormap(info.display,
			RootWindow(info.display, info.screen),
			info.visual.visual,
			AllocAll);
		XStoreColors(info.display, info.colormap, info.rgbcube, 256);
		return;
	}
	else if (info.visual.class == TrueColor)
	{
		info.colormap = 0;
		return;
	}
	fprintf(stderr, "Cannot handle visual class %d with depth: %d\n",
		info.visual.class, info.visual.depth);
	return;
}

static void
select_mode(void)
{
	int byteorder;
	int byterev;
	unsigned long rm, gm, bm;
	unsigned long rs, gs, bs;

	byteorder = ImageByteOrder(info.display);
	if (is_big_endian())
		byterev = byteorder != MSBFirst;
	else
		byterev = byteorder != LSBFirst;

	rm = info.visual.red_mask;
	gm = info.visual.green_mask;
	bm = info.visual.blue_mask;

	rs = ffs(rm) - 1;
	gs = ffs(gm) - 1;
	bs = ffs(bm) - 1;

#ifdef SHOWINFO
	printf("ximage: mode %d/%d %08lx %08lx %08lx (%ld,%ld,%ld) %s%s\n",
		info.visual.depth,
		info.bitsperpixel,
		rm, gm, bm, rs, gs, bs,
		byteorder == MSBFirst ? "msb" : "lsb",
		byterev ? " <swap>":"");
#endif

	info.mode = UNKNOWN;
	if (info.bitsperpixel == 8) {
		/* Either PseudoColor with BGR233 colormap, or TrueColor */
		info.mode = BGR233;
	}
	else if (info.bitsperpixel == 16) {
		if (rm == 0xF800 && gm == 0x07E0 && bm == 0x001F)
			info.mode = !byterev ? RGB565 : RGB565_BR;
		if (rm == 0x7C00 && gm == 0x03E0 && bm == 0x001F)
			info.mode = !byterev ? RGB555 : RGB555_BR;
	}
	else if (info.bitsperpixel == 24) {
		if (rs == 0 && gs == 8 && bs == 16)
			info.mode = byteorder == MSBFirst ? RGB888 : BGR888;
		if (rs == 16 && gs == 8 && bs == 0)
			info.mode = byteorder == MSBFirst ? BGR888 : RGB888;
	}
	else if (info.bitsperpixel == 32) {
		if (rs == 0 && gs == 8 && bs == 16)
			info.mode = byteorder == MSBFirst ? ABGR8888 : RGBA8888;
		if (rs == 8 && gs == 16 && bs == 24)
			info.mode = byteorder == MSBFirst ? BGRA8888 : ARGB8888;
		if (rs == 16 && gs == 8 && bs == 0)
			info.mode = byteorder == MSBFirst ? ARGB8888 : BGRA8888;
		if (rs == 24 && gs == 16 && bs == 8)
			info.mode = byteorder == MSBFirst ? RGBA8888 : ABGR8888;
	}

#ifdef SHOWINFO
	printf("ximage: RGBA8888 to %s\n", modename[info.mode]);
#endif

	/* select conversion function */
	info.convert_func = ximage_convert_funcs[info.mode];
}

static int
create_pool(void)
{
	int i;

	info.lastused = 0;

	for (i = 0; i < POOLSIZE; i++) {
		info.pool[i] = NULL;
	}

	for (i = 0; i < POOLSIZE; i++) {
		info.pool[i] = createximage(info.display,
			info.visual.visual, &info.shminfo[i], info.visual.depth,
			WIDTH, HEIGHT);
		if (!info.pool[i]) {
			return 0;
		}
	}

	return 1;
}

static XImage *
next_pool_image(void)
{
	if (info.lastused + 1 >= POOLSIZE) {
		if (info.useshm)
			XSync(info.display, False);
		else
			XFlush(info.display);
		info.lastused = 0;
	}
	return info.pool[info.lastused ++];
}

static int
ximage_error_handler(Display *display, XErrorEvent *event)
{
	/* Turn off shared memory images if we get an error from the MIT-SHM extension */
	if (event->request_code == info.shmcode)
	{
		char buf[80];
		XGetErrorText(display, event->error_code, buf, sizeof buf);
		fprintf(stderr, "ximage: disabling shared memory extension: %s\n", buf);
		info.useshm = 0;
		return 0;
	}

	XSetErrorHandler(NULL);
	return (XSetErrorHandler(ximage_error_handler))(display, event);
}

int
ximage_init(Display *display, int screen, Visual *visual)
{
	XVisualInfo template;
	XVisualInfo *visuals;
	int nvisuals;
	XPixmapFormatValues *formats;
	int nformats;
	int ok;
	int i;
	int major;
	int event;
	int error;

	info.display = display;
	info.screen = screen;
	info.colormap = 0;

	/* Get XVisualInfo for this visual */
	template.visualid = XVisualIDFromVisual(visual);
	visuals = XGetVisualInfo(display, VisualIDMask, &template, &nvisuals);
	if (nvisuals != 1) {
		fprintf(stderr, "Visual not found!\n");
		XFree(visuals);
		return 0;
	}
	memcpy(&info.visual, visuals, sizeof (XVisualInfo));
	XFree(visuals);

	/* Get appropriate PixmapFormat for this visual */
	formats = XListPixmapFormats(info.display, &nformats);
	for (i = 0; i < nformats; i++) {
		if (formats[i].depth == info.visual.depth) {
			info.bitsperpixel = formats[i].bits_per_pixel;
			break;
		}
	}
	XFree(formats);
	if (i == nformats) {
		fprintf(stderr, "PixmapFormat not found!\n");
		return 0;
	}

	/* extract mode */
	select_mode();

	/* prepare colormap */
	make_colormap();

	/* identify code for MIT-SHM extension */
	if (XQueryExtension(display, "MIT-SHM", &major, &event, &error) &&
		XShmQueryExtension(display))
		info.shmcode = major;

	/* intercept errors looking for SHM code */
	XSetErrorHandler(ximage_error_handler);

	/* prepare pool of XImages */
	info.useshm = 1;
	ok = create_pool();
	if (!ok)
		return 0;

#ifdef SHOWINFO
	printf("ximage: %sPutImage\n", info.useshm ? "XShm" : "X");
#endif

	return 1;
}

int
ximage_get_depth(void)
{
	return info.visual.depth;
}

Visual *
ximage_get_visual(void)
{
	return info.visual.visual;
}

Colormap
ximage_get_colormap(void)
{
	return info.colormap;
}

void
ximage_blit(Drawable d, GC gc,
	int dstx, int dsty,
	unsigned char *srcdata,
	int srcx, int srcy,
	int srcw, int srch,
	int srcstride)
{
	XImage *image;
	int ax, ay;
	int w, h;
	unsigned char *srcptr;

	for (ay = 0; ay < srch; ay += HEIGHT)
	{
		h = fz_mini(srch - ay, HEIGHT);
		for (ax = 0; ax < srcw; ax += WIDTH)
		{
			w = fz_mini(srcw - ax, WIDTH);

			image = next_pool_image();

			srcptr = srcdata +
			(ay + srcy) * srcstride +
			(ax + srcx) * 4;

			info.convert_func(srcptr, srcstride,
				(unsigned char *) image->data,
				image->bytes_per_line, w, h);

			if (info.useshm)
			{
				XShmPutImage(info.display, d, gc, image,
					0, 0, dstx + ax, dsty + ay,
					w, h, False);
			}
			else
			{
				XPutImage(info.display, d, gc, image,
					0, 0,
					dstx + ax,
					dsty + ay,
					w, h);
			}
		}
	}
}

/*
 * Primitive conversion functions
 */

#ifndef restrict
#ifndef _C99
#ifdef __GNUC__
#define restrict __restrict__
#else
#define restrict
#endif
#endif
#endif

#define PARAMS \
	const unsigned char * restrict src, \
	int srcstride, \
	unsigned char * restrict dst, \
	int dststride, \
	int w, \
	int h

/*
 * Convert byte:RGBA8888 to various formats
 */

static void
ximage_convert_argb8888(PARAMS)
{
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x ++) {
			dst[x * 4 + 0] = src[x * 4 + 3]; /* a */
			dst[x * 4 + 1] = src[x * 4 + 0]; /* r */
			dst[x * 4 + 2] = src[x * 4 + 1]; /* g */
			dst[x * 4 + 3] = src[x * 4 + 2]; /* b */
		}
		dst += dststride;
		src += srcstride;
	}
}

static void
ximage_convert_bgra8888(PARAMS)
{
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			dst[x * 4 + 0] = src[x * 4 + 2];
			dst[x * 4 + 1] = src[x * 4 + 1];
			dst[x * 4 + 2] = src[x * 4 + 0];
			dst[x * 4 + 3] = src[x * 4 + 3];
		}
		dst += dststride;
		src += srcstride;
	}
}

static void
ximage_convert_abgr8888(PARAMS)
{
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			dst[x * 4 + 0] = src[x * 4 + 3];
			dst[x * 4 + 1] = src[x * 4 + 2];
			dst[x * 4 + 2] = src[x * 4 + 1];
			dst[x * 4 + 3] = src[x * 4 + 0];
		}
		dst += dststride;
		src += srcstride;
	}
}

static void
ximage_convert_rgba8888(PARAMS)
{
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			((unsigned *)dst)[x] = ((unsigned *)src)[x];
		}
		dst += dststride;
		src += srcstride;
	}
}

static void
ximage_convert_bgr888(PARAMS)
{
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			dst[3*x + 0] = src[4*x + 2];
			dst[3*x + 1] = src[4*x + 1];
			dst[3*x + 2] = src[4*x + 0];
		}
		src += srcstride;
		dst += dststride;
	}
}

static void
ximage_convert_rgb888(PARAMS)
{
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			dst[3*x + 0] = src[4*x + 0];
			dst[3*x + 1] = src[4*x + 1];
			dst[3*x + 2] = src[4*x + 2];
		}
		src += srcstride;
		dst += dststride;
	}
}

static void
ximage_convert_rgb565(PARAMS)
{
	unsigned char r, g, b;
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			r = src[4*x + 0];
			g = src[4*x + 1];
			b = src[4*x + 2];
			((unsigned short *)dst)[x] =
			((r & 0xF8) << 8) |
			((g & 0xFC) << 3) |
			(b >> 3);
		}
		src += srcstride;
		dst += dststride;
	}
}

static void
ximage_convert_rgb565_br(PARAMS)
{
	unsigned char r, g, b;
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			r = src[4*x + 0];
			g = src[4*x + 1];
			b = src[4*x + 2];
			/* final word is:
			g4 g3 g2 b7 b6 b5 b4 b3 : r7 r6 r5 r4 r3 g7 g6 g5
			*/
			((unsigned short *)dst)[x] =
			(r & 0xF8) |
			((g & 0xE0) >> 5) |
			((g & 0x1C) << 11) |
			((b & 0xF8) << 5);
		}
		src += srcstride;
		dst += dststride;
	}
}

static void
ximage_convert_rgb555(PARAMS)
{
	unsigned char r, g, b;
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			r = src[4*x + 0];
			g = src[4*x + 1];
			b = src[4*x + 2];
			((unsigned short *)dst)[x] =
			((r & 0xF8) << 7) |
			((g & 0xF8) << 2) |
			(b >> 3);
		}
		src += srcstride;
		dst += dststride;
	}
}

static void
ximage_convert_rgb555_br(PARAMS)
{
	unsigned char r, g, b;
	int x, y;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			r = src[4*x + 0];
			g = src[4*x + 1];
			b = src[4*x + 2];
			/* final word is:
			g5 g4 g3 b7 b6 b5 b4 b3 : 0 r7 r6 r5 r4 r3 g7 g6
			*/
			((unsigned short *)dst)[x] =
			((r & 0xF8) >> 1) |
			((g & 0xC0) >> 6) |
			((g & 0x38) << 10) |
			((b & 0xF8) << 5);
		}
		src += srcstride;
		dst += dststride;
	}
}

static void
ximage_convert_bgr233(PARAMS)
{
	unsigned char r, g, b;
	int x,y;
	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++) {
			r = src[4*x + 0];
			g = src[4*x + 1];
			b = src[4*x + 2];
			/* format: b7 b6 g7 g6 g5 r7 r6 r5 */
			dst[x] = (b&0xC0) | ((g>>2)&0x38) | ((r>>5)&0x7);
		}
		src += srcstride;
		dst += dststride;
	}
}

static void
ximage_convert_generic(PARAMS)
{
	unsigned long rm, gm, bm, rs, gs, bs, rx, bx, gx;
	unsigned long pixel;
	unsigned long r, g, b;
	int x, y;

	rm = info.visual.red_mask;
	gm = info.visual.green_mask;
	bm = info.visual.blue_mask;

	rs = ffs(rm) - 1;
	gs = ffs(gm) - 1;
	bs = ffs(bm) - 1;

	rx = ffs(~(rm >> rs)) - 1;
	gx = ffs(~(gm >> gs)) - 1;
	bx = ffs(~(bm >> bs)) - 1;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			r = src[4*x + 0];
			g = src[4*x + 1];
			b = src[4*x + 2];

			/* adjust precision */
			if (rx < 8) r >>= (8 - rx); else if (rx > 8) r <<= (rx - 8);
			if (gx < 8) g >>= (8 - gx); else if (gx > 8) g <<= (gx - 8);
			if (bx < 8) b >>= (8 - bx); else if (bx > 8) b <<= (bx - 8);

			pixel = (r << rs) | (g << gs) | (b << bs);

			if (ImageByteOrder(info.display) == MSBFirst) {
				if (info.bitsperpixel > 16) {
					dst[4*x + 0] = (pixel >> 24) & 0xFF;
					dst[4*x + 1] = (pixel >> 16) & 0xFF;
					dst[4*x + 2] = (pixel >> 8) & 0xFF;
					dst[4*x + 3] = (pixel) & 0xFF;
				} else if (info.bitsperpixel > 8) {
					dst[2*x + 0] = (pixel >> 8) & 0xFF;
					dst[2*x + 1] = (pixel) & 0xFF;
				}
			} else {
				if (info.bitsperpixel > 16) {
					dst[4*x + 0] = (pixel) & 0xFF;
					dst[4*x + 1] = (pixel >> 8) & 0xFF;
					dst[4*x + 2] = (pixel >> 16) & 0xFF;
					dst[4*x + 3] = (pixel >> 24) & 0xFF;
				} else if (info.bitsperpixel > 8) {
					dst[2*x + 0] = (pixel) & 0xFF;
					dst[2*x + 1] = (pixel >> 8) & 0xFF;
				}
			}
		}
		src += srcstride;
		dst += dststride;
	}
}

ximage_convert_func_t ximage_convert_funcs[] = {
	ximage_convert_argb8888,
	ximage_convert_bgra8888,
	ximage_convert_rgba8888,
	ximage_convert_abgr8888,
	ximage_convert_rgb888,
	ximage_convert_bgr888,
	ximage_convert_rgb565,
	ximage_convert_rgb565_br,
	ximage_convert_rgb555,
	ximage_convert_rgb555_br,
	ximage_convert_bgr233,
	ximage_convert_generic
};
