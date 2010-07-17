#include "fitz.h"

/* PDF 1.4 blend modes. These are slow.  */

typedef unsigned char byte;

const char *fz_blendnames[] =
{
	"Normal",
	"Multiply",
	"Screen",
	"Overlay",
	"Darken",
	"Lighten",
	"ColorDodge",
	"ColorBurn",
	"HardLight",
	"SoftLight",
	"Difference",
	"Exclusion",
	"Hue",
	"Saturation",
	"Color",
	"Luminosity",
	nil
};

/* Separable blend modes */

static inline int
fz_screen_byte(int b, int s)
{
	return b + s - fz_mul255(b, s);
}

static inline int
fz_hardlight_byte(int b, int s)
{
	int s2 = s << 1;
	if (s <= 127)
		return fz_mul255(b, s2);
	else
		return fz_screen_byte(b, s2 - 255);
}

static inline int
fz_overlay_byte(int b, int s)
{
	return fz_hardlight_byte(s, b); /* note swapped order */
}

static inline int
fz_darken_byte(int b, int s)
{
	return MIN(b, s);
}

static inline int
fz_lighten_byte(int b, int s)
{
	return MAX(b, s);
}

static inline int
fz_colordodge_byte(int b, int s)
{
	s = 255 - s;
	if (b == 0)
		return 0;
	else if (b >= s)
		return 255;
	else
		return (0x1fe * b + s) / (s << 1);
}

static inline int
fz_colorburn_byte(int b, int s)
{
	b = 255 - b;
	if (b == 0)
		return 255;
	else if (b >= s)
		return 0;
	else
		return 0xff - (0x1fe * b + s) / (s << 1);
}

static inline int
fz_softlight_byte(int b, int s)
{
	/* review this */
	if (s < 128) {
		return b - fz_mul255(fz_mul255((255 - (s<<1)), b), 255 - b);
	}
	else {
		int dbd;
		if (b < 64)
			dbd = fz_mul255(fz_mul255((b << 4) - 12, b) + 4, b);
		else
			dbd = (int)sqrtf(255.0f * b);
		return b + fz_mul255(((s<<1) - 255), (dbd - b));
	}
}

static inline int
fz_difference_byte(int b, int s)
{
	return ABS(b - s);
}

static inline int
fz_exclusion_byte(int b, int s)
{
	return b + s - (fz_mul255(b, s)<<1);
}

/* Non-separable blend modes */

static inline void
fz_luminosity_rgb(int *rd, int *gd, int *bd, int rb, int gb, int bb, int rs, int gs, int bs)
{
	int delta, scale;
	int r, g, b, y;

	/* 0.3, 0.59, 0.11 in fixed point */
	delta = ((rs - rb) * 77 + (gs - gb) * 151 + (bs - bb) * 28 + 0x80) >> 8;
	r = rb + delta;
	g = gb + delta;
	b = bb + delta;

	if ((r | g | b) & 0x100)
	{
		y = (rs * 77 + gs * 151 + bs * 28 + 0x80) >> 8;
		if (delta > 0) {
			int max;
			max = r > g ? r : g;
			max = b > max ? b : max;
			scale = ((255 - y) << 16) / (max - y);
		} else {
			int min;
			min = r < g ? r : g;
			min = b < min ? b : min;
			scale = (y << 16) / (y - min);
		}
		r = y + (((r - y) * scale + 0x8000) >> 16);
		g = y + (((g - y) * scale + 0x8000) >> 16);
		b = y + (((b - y) * scale + 0x8000) >> 16);
	}

	*rd = r;
	*gd = g;
	*bd = b;
}

static void
fz_saturation_rgb(int *rd, int *gd, int *bd, int rb, int gb, int bb, int rs, int gs, int bs)
{
	int minb, maxb;
	int mins, maxs;
	int y;
	int scale;
	int r, g, b;

	minb = rb < gb ? rb : gb;
	minb = minb < bb ? minb : bb;
	maxb = rb > gb ? rb : gb;
	maxb = maxb > bb ? maxb : bb;
	if (minb == maxb) {
		/* backdrop has zero saturation, avoid divide by 0 */
		*rd = gb;
		*gd = gb;
		*bd = gb;
		return;
	}

	mins = rs < gs ? rs : gs;
	mins = mins < bs ? mins : bs;
	maxs = rs > gs ? rs : gs;
	maxs = maxs > bs ? maxs : bs;

	scale = ((maxs - mins) << 16) / (maxb - minb);
	y = (rb * 77 + gb * 151 + bb * 28 + 0x80) >> 8;
	r = y + ((((rb - y) * scale) + 0x8000) >> 16);
	g = y + ((((gb - y) * scale) + 0x8000) >> 16);
	b = y + ((((bb - y) * scale) + 0x8000) >> 16);

	if ((r | g | b) & 0x100) {
		int scalemin, scalemax;
		int min, max;

		min = r < g ? r : g;
		min = min < b ? min : b;
		max = r > g ? r : g;
		max = max > b ? max : b;

		if (min < 0)
			scalemin = (y << 16) / (y - min);
		else
			scalemin = 0x10000;

		if (max > 255)
			scalemax = ((255 - y) << 16) / (max - y);
		else
			scalemax = 0x10000;

		scale = scalemin < scalemax ? scalemin : scalemax;
		r = y + (((r - y) * scale + 0x8000) >> 16);
		g = y + (((g - y) * scale + 0x8000) >> 16);
		b = y + (((b - y) * scale + 0x8000) >> 16);
	}

	*rd = r;
	*gd = g;
	*bd = b;
}

static void
fz_color_rgb(int *rr, int *rg, int *rb, int br, int bg, int bb, int sr, int sg, int sb)
{
	fz_luminosity_rgb(rr, rg, rb, sr, sg, sb, br, bg, bb);
}

static void
fz_hue_rgb(int *rr, int *rg, int *rb, int br, int bg, int bb, int sr, int sg, int sb)
{
	int tr, tg, tb;
	fz_luminosity_rgb(&tr, &tg, &tb, sr, sg, sb, br, bg, bb);
	fz_saturation_rgb(rr, rg, rb, tr, tg, tb, br, bg, bb);
}

/* Blending functions */

static void
fz_blendseparable(byte * restrict sp, byte * restrict bp, int n, int w, fz_blendmode blendmode)
{
	int k;
	int n1 = n - 1;
	while (w--)
	{
		int sa = sp[n1];
		int ba = bp[n1];
		int saba = fz_mul255(sa, ba);

		for (k = 0; k < n1; k++)
		{
			int sc = sp[k];
			int bc = bp[k];
			int rc;

			/* ugh, division to get non-premul components */
			if (sa) sc = sc * 255 / sa;
			if (ba) bc = bc * 255 / ba;

			switch (blendmode)
			{
			default:
			case FZ_BNORMAL: rc = sc; break;
			case FZ_BMULTIPLY: rc = fz_mul255(bc, sc); break;
			case FZ_BSCREEN: rc = fz_screen_byte(bc, sc); break;
			case FZ_BOVERLAY: rc = fz_overlay_byte(bc, sc); break;
			case FZ_BDARKEN: rc = fz_darken_byte(bc, sc); break;
			case FZ_BLIGHTEN: rc = fz_lighten_byte(bc, sc); break;
			case FZ_BCOLORDODGE: rc = fz_colordodge_byte(bc, sc); break;
			case FZ_BCOLORBURN: rc = fz_colorburn_byte(bc, sc); break;
			case FZ_BHARDLIGHT: rc = fz_hardlight_byte(bc, sc); break;
			case FZ_BSOFTLIGHT: rc = fz_softlight_byte(bc, sc); break;
			case FZ_BDIFFERENCE: rc = fz_difference_byte(bc, sc); break;
			case FZ_BEXCLUSION: rc = fz_exclusion_byte(bc, sc); break;
			}

			bp[k] = fz_mul255(255 - sa, bp[k]) + fz_mul255(255 - ba, sp[k]) + fz_mul255(saba, rc);
		}

		bp[k] = ba + sa - saba;

		sp += n;
		bp += n;
	}
}

static void
fz_blendnonseparable(byte * restrict sp, byte * restrict bp, int w, fz_blendmode blendmode)
{
	while (w--)
	{
		int rr, rg, rb, saba;

		int sr = sp[0];
		int sg = sp[1];
		int sb = sp[2];
		int sa = sp[3];

		int br = bp[0];
		int bg = bp[1];
		int bb = bp[2];
		int ba = bp[3];

		saba = fz_mul255(sa, ba);

		/* ugh, division to get non-premul components */
		if (sa) {
			sr = sr * 255 / sa;
			sg = sg * 255 / sa;
			sb = sb * 255 / sa;
		}
		if (ba) {
			br = br * 255 / ba;
			bg = bg * 255 / ba;
			bb = bb * 255 / ba;
		}

		switch (blendmode)
		{
		default:
		case FZ_BHUE:
			fz_hue_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
			break;
		case FZ_BSATURATION:
			fz_saturation_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
			break;
		case FZ_BCOLOR:
			fz_color_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
			break;
		case FZ_BLUMINOSITY:
			fz_luminosity_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
			break;
		}

		bp[0] = fz_mul255(255 - sa, bp[0]) + fz_mul255(255 - ba, sp[0]) + fz_mul255(saba, rr);
		bp[1] = fz_mul255(255 - sa, bp[1]) + fz_mul255(255 - ba, sp[1]) + fz_mul255(saba, rg);
		bp[2] = fz_mul255(255 - sa, bp[2]) + fz_mul255(255 - ba, sp[2]) + fz_mul255(saba, rb);
		bp[3] = ba + sa - saba;

		sp += 4;
		bp += 4;
	}
}

void
fz_blendpixmaps(fz_pixmap *src, fz_pixmap *dst, fz_blendmode blendmode)
{
	unsigned char *sp, *dp;
	fz_bbox sr, dr;
	int x, y, w, h, n;

	assert(src->n == dst->n);

	sr.x0 = src->x;
	sr.y0 = src->y;
	sr.x1 = src->x + src->w;
	sr.y1 = src->y + src->h;

	dr.x0 = dst->x;
	dr.y0 = dst->y;
	dr.x1 = dst->x + dst->w;
	dr.y1 = dst->y + dst->h;

	dr = fz_intersectbbox(sr, dr);
	x = dr.x0;
	y = dr.y0;
	w = dr.x1 - dr.x0;
	h = dr.y1 - dr.y0;

	n = src->n;
	sp = src->samples + ((y - src->y) * src->w + (x - src->x)) * n;
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * n;

	while (h--)
	{
		if (n == 4 && blendmode >= FZ_BHUE)
			fz_blendnonseparable(sp, dp, w, blendmode);
		else
			fz_blendseparable(sp, dp, n, w, blendmode);
		sp += src->w * n;
		dp += dst->w * n;
	}
}
