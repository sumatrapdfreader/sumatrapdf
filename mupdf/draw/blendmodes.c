#include "fitz.h"

typedef unsigned char byte;

/*
PDF 1.4 blend modes, except Normal and Multiply which are Over and In respectively.
Only the actual blend routines are here, not the node rendering logic which lives in render.c.
These are slow.
*/

/* These functions apply to a single component, 0-255 range typically */

static inline int
fz_screen_byte(int bd, int s)
{
	return bd + s - fz_mul255(bd, s);
}

static inline int
fz_hardlight_byte(int bd, int s)
{
	int s2 = s << 1;
	if (s <= 127)
		return fz_mul255(bd, s2);
	else
		return fz_screen_byte(bd, s2 - 1);
}

static inline int
fz_overlay_byte(int bd, int s)
{
	return fz_hardlight_byte(s, bd); // note swapped order
}

static inline int
fz_darken_byte(int bd, int s)
{
	return MIN(bd, s);
}

static inline int
fz_lighten_byte(int bd, int s)
{
	return MAX(bd, s);
}

static inline int
fz_colordodge_byte(int bd, int s)
{
	if (s < 255)
		return MIN(255, 255 * bd / (255 - s));
	else
		return 255;
}

static inline int
fz_colorburn_byte(int bd, int s)
{
	if (s > 0)
		return 255 - MIN(255, 255 * (255 - bd) / s);
	else
		return 0;
}

static inline int
fz_softlight_byte(int bd, int s)
{
	/* review this */
	if (s < 128) {
		return bd - fz_mul255(fz_mul255((255 - (s<<1)), bd), 255 - bd);
	}
	else {
		int dbd;
		if (bd < 64)
			dbd = fz_mul255(fz_mul255((bd << 4) - 12, bd) + 4, bd);
		else
			dbd = (int)sqrtf(255.0f * bd);
		return bd + fz_mul255(((s<<1) - 255), (dbd - bd));
	}
}

static inline int
fz_difference_byte(int bd, int s)
{
	return ABS(bd - s);
}

static inline int
fz_exclusion_byte(int bd, int s)
{
	return bd + s - (fz_mul255(bd, s)<<1);
}

/* Non-separable blend modes */

static inline int
lum(int r, int g, int b)
{
	/* 0.3, 0.59, 0.11 in 16.16 fixed point */
	return (19662 * r + 38666 * g + 7208 * b) >> 16;
}

static void
clipcolor(int r, int g, int b, int *dr, int *dg, int *db)
{
	int l = lum(r, g, b);
	int n = MIN(MIN(r, g), b);
	int x = MAX(MAX(r, g), b);
	if (n < 0) {
		*dr = l + 255 * (r - l) / (l - n);
		*dg = l + 255 * (g - l) / (l - n);
		*db = l + 255 * (b - l) / (l - n);
	}
	else {
		*dr = l + 255 * (255 - l) / (x - l);
		*dg = l + 255 * (255 - l) / (x - l);
		*db = l + 255 * (255 - l) / (x - l);
	}
}

static void
setlum(int r, int g, int b, int l, int *dr, int *dg, int *db)
{
	int d = 255 - lum(r, g, b);
	clipcolor(r + d, g + d, b + d, dr, dg, db);
}

static inline int
sat(int r, int g, int b)
{
	return MAX(MAX(r, g), b) - MIN(MIN(r, g), b);
}

static void
setsat(int r, int g, int b, int s, int *dr, int *dg, int *db)
{
	int *m[3]; /* min, med, max */
	int *t;
	m[0] = &r;
	m[1] = &g;
	m[2] = &b;
#define SWAP(a, b) (t = a, a = b, b = t)
	if (*m[0] > *m[1])
		SWAP(m[0], m[1]);
	if (*m[0] > *m[2])
		SWAP(m[0], m[2]);
	if (*m[1] > *m[2])
		SWAP(m[1], m[2]);

	if (*m[2] > *m[0]) {
		*m[1] = (*m[1] - *m[0]) * s / (*m[2] - *m[0]);
		*m[2] = s;
	}
	else {
		*m[1] = 0;
		*m[2] = 0;
	}
	*dr = r;
	*dg = g;
	*db = b;
}

static void
fz_hue_rgb(int *bdr, int *bdg, int *bdb, int sr, int sg, int sb)
{
	int tr, tg, tb;
	setsat(sr, sg, sb, sat(*bdr, *bdg, *bdb), &tr, &tg, &tb);
	setlum(tr, tg, tb, lum(*bdr, *bdg, *bdb), bdr, bdg, bdb);
}

static void
fz_saturation_rgb(int *bdr, int *bdg, int *bdb, int sr, int sg, int sb)
{
	int tr, tg, tb;
	setsat(*bdr, *bdg, *bdb, sat(sr, sg, sb), &tr, &tg, &tb);
	setlum(tr, tg, tb, lum(*bdr, *bdg, *bdb), bdr, bdg, bdb);
}

static void
fz_color_rgb(int *bdr, int *bdg, int *bdb, int sr, int sg, int sb)
{
	setlum(sr, sg, sb, lum(*bdr, *bdg, *bdb), bdr, bdg, bdb);
}

static void
fz_luminosity_rgb(int *bdr, int *bdg, int *bdb, int sr, int sg, int sb)
{
	setlum(*bdr, *bdg, *bdb, lum(sr, sg, sb), bdr, bdg, bdb);
}

/*
 *
 */

void
fz_blend_nxn(byte * restrict sp, int sw, int sn,
	byte * restrict dp, int dw,
	int w0, int h, fz_blendmode blendmode)
{
	int k;

	sw -= w0*sn;
	dw -= w0*sn;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			int sa = sp[sn-1];
			int da = dp[sn-1];
			int ta = 255 - sa;
			int tb = 255 - da;
			int tc = fz_mul255(sa, da);
			for (k = 0; k < sn; k++)
			{
				int r, bd, s;
				if (da)
					bd = dp[k] * 255 / da;
				if (sa)
					s = sp[k] * 255 / sa;
				switch (blendmode)
				{
				default:
				case FZ_BMULTIPLY: r = fz_mul255(bd, s); break;
				case FZ_BSCREEN: r = fz_screen_byte(bd, s); break;
				case FZ_BOVERLAY: r = fz_overlay_byte(bd, s); break;
				case FZ_BDARKEN: r = fz_darken_byte(bd, s); break;
				case FZ_BLIGHTEN: r = fz_lighten_byte(bd, s); break;
				case FZ_BCOLORDODGE: r = fz_colordodge_byte(bd, s); break;
				case FZ_BCOLORBURN: r = fz_colorburn_byte(bd, s); break;
				case FZ_BHARDLIGHT: r = fz_hardlight_byte(bd, s); break;
				case FZ_BSOFTLIGHT: r = fz_softlight_byte(bd, s); break;
				case FZ_BDIFFERENCE: r = fz_difference_byte(bd, s); break;
				case FZ_BEXCLUSION: r = fz_exclusion_byte(bd, s); break;
				}
				dp[k] = fz_mul255(ta, dp[k]) + fz_mul255(tb, sp[k]) + fz_mul255(tc, r);
			}
			sp += sn;
			dp += sn;
		}
		sp += sw;
		dp += dw;
	}
}

void
fz_blendpixmaps(fz_pixmap *src, fz_pixmap *dst, fz_blendmode blendmode)
{
	unsigned char *sp, *dp;
	fz_bbox sr, dr;
	int x, y, w, h;

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

	sp = src->samples + ((y - src->y) * src->w + (x - src->x)) * src->n;
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;

	fz_blend_nxn(sp, src->w * src->n, src->n, dp, dst->w * dst->n, w, h, blendmode);
}
