#include "mupdf/fitz.h"
#include "draw-imp.h"

/* PDF 1.4 blend modes. These are slow. */

typedef unsigned char byte;

static const char *fz_blendmode_names[] =
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
};

int fz_lookup_blendmode(char *name)
{
	int i;
	for (i = 0; i < nelem(fz_blendmode_names); i++)
		if (!strcmp(name, fz_blendmode_names[i]))
			return i;
	return FZ_BLEND_NORMAL;
}

char *fz_blendmode_name(int blendmode)
{
	if (blendmode >= 0 && blendmode < nelem(fz_blendmode_names))
		return (char*)fz_blendmode_names[blendmode];
	return "Normal";
}

/* Separable blend modes */

static inline int fz_screen_byte(int b, int s)
{
	return b + s - fz_mul255(b, s);
}

static inline int fz_hard_light_byte(int b, int s)
{
	int s2 = s << 1;
	if (s <= 127)
		return fz_mul255(b, s2);
	else
		return fz_screen_byte(b, s2 - 255);
}

static inline int fz_overlay_byte(int b, int s)
{
	return fz_hard_light_byte(s, b); /* note swapped order */
}

static inline int fz_darken_byte(int b, int s)
{
	return fz_mini(b, s);
}

static inline int fz_lighten_byte(int b, int s)
{
	return fz_maxi(b, s);
}

static inline int fz_color_dodge_byte(int b, int s)
{
	s = 255 - s;
	if (b == 0)
		return 0;
	else if (b >= s)
		return 255;
	else
		return (0x1fe * b + s) / (s << 1);
}

static inline int fz_color_burn_byte(int b, int s)
{
	b = 255 - b;
	if (b == 0)
		return 255;
	else if (b >= s)
		return 0;
	else
		return 0xff - (0x1fe * b + s) / (s << 1);
}

static inline int fz_soft_light_byte(int b, int s)
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

static inline int fz_difference_byte(int b, int s)
{
	return fz_absi(b - s);
}

static inline int fz_exclusion_byte(int b, int s)
{
	return b + s - (fz_mul255(b, s)<<1);
}

/* Non-separable blend modes */

static void
fz_luminosity_rgb(unsigned char *rd, unsigned char *gd, unsigned char *bd, int rb, int gb, int bb, int rs, int gs, int bs)
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
		if (delta > 0)
		{
			int max;
			max = fz_maxi(r, fz_maxi(g, b));
			scale = (max == y ? 0 : ((255 - y) << 16) / (max - y));
		}
		else
		{
			int min;
			min = fz_mini(r, fz_mini(g, b));
			scale = (y == min ? 0 : (y << 16) / (y - min));
		}
		r = y + (((r - y) * scale + 0x8000) >> 16);
		g = y + (((g - y) * scale + 0x8000) >> 16);
		b = y + (((b - y) * scale + 0x8000) >> 16);
	}

	*rd = fz_clampi(r, 0, 255);
	*gd = fz_clampi(g, 0, 255);
	*bd = fz_clampi(b, 0, 255);
}

static void
fz_saturation_rgb(unsigned char *rd, unsigned char *gd, unsigned char *bd, int rb, int gb, int bb, int rs, int gs, int bs)
{
	int minb, maxb;
	int mins, maxs;
	int y;
	int scale;
	int r, g, b;

	minb = fz_mini(rb, fz_mini(gb, bb));
	maxb = fz_maxi(rb, fz_maxi(gb, bb));
	if (minb == maxb)
	{
		/* backdrop has zero saturation, avoid divide by 0 */
		gb = fz_clampi(gb, 0, 255);
		*rd = gb;
		*gd = gb;
		*bd = gb;
		return;
	}

	mins = fz_mini(rs, fz_mini(gs, bs));
	maxs = fz_maxi(rs, fz_maxi(gs, bs));

	scale = ((maxs - mins) << 16) / (maxb - minb);
	y = (rb * 77 + gb * 151 + bb * 28 + 0x80) >> 8;
	r = y + ((((rb - y) * scale) + 0x8000) >> 16);
	g = y + ((((gb - y) * scale) + 0x8000) >> 16);
	b = y + ((((bb - y) * scale) + 0x8000) >> 16);

	if ((r | g | b) & 0x100)
	{
		int scalemin, scalemax;
		int min, max;

		min = fz_mini(r, fz_mini(g, b));
		max = fz_maxi(r, fz_maxi(g, b));

		if (min < 0)
			scalemin = (y << 16) / (y - min);
		else
			scalemin = 0x10000;

		if (max > 255)
			scalemax = ((255 - y) << 16) / (max - y);
		else
			scalemax = 0x10000;

		scale = fz_mini(scalemin, scalemax);
		r = y + (((r - y) * scale + 0x8000) >> 16);
		g = y + (((g - y) * scale + 0x8000) >> 16);
		b = y + (((b - y) * scale + 0x8000) >> 16);
	}

	*rd = fz_clampi(r, 0, 255);
	*gd = fz_clampi(g, 0, 255);
	*bd = fz_clampi(b, 0, 255);
}

static void
fz_color_rgb(unsigned char *rr, unsigned char *rg, unsigned char *rb, int br, int bg, int bb, int sr, int sg, int sb)
{
	fz_luminosity_rgb(rr, rg, rb, sr, sg, sb, br, bg, bb);
}

static void
fz_hue_rgb(unsigned char *rr, unsigned char *rg, unsigned char *rb, int br, int bg, int bb, int sr, int sg, int sb)
{
	unsigned char tr, tg, tb;
	fz_luminosity_rgb(&tr, &tg, &tb, sr, sg, sb, br, bg, bb);
	fz_saturation_rgb(rr, rg, rb, tr, tg, tb, br, bg, bb);
}

void
fz_blend_pixel(unsigned char dp[3], unsigned char bp[3], unsigned char sp[3], int blendmode)
{
	int k;
	/* non-separable blend modes */
	switch (blendmode)
	{
	case FZ_BLEND_HUE: fz_hue_rgb(&dp[0], &dp[1], &dp[2], bp[0], bp[1], bp[2], sp[0], sp[1], sp[2]); return;
	case FZ_BLEND_SATURATION: fz_saturation_rgb(&dp[0], &dp[1], &dp[2], bp[0], bp[1], bp[2], sp[0], sp[1], sp[2]); return;
	case FZ_BLEND_COLOR: fz_color_rgb(&dp[0], &dp[1], &dp[2], bp[0], bp[1], bp[2], sp[0], sp[1], sp[2]); return;
	case FZ_BLEND_LUMINOSITY: fz_luminosity_rgb(&dp[0], &dp[1], &dp[2], bp[0], bp[1], bp[2], sp[0], sp[1], sp[2]); return;
	}
	/* separable blend modes */
	for (k = 0; k < 3; k++)
	{
		switch (blendmode)
		{
		default:
		case FZ_BLEND_NORMAL: dp[k] = sp[k]; break;
		case FZ_BLEND_MULTIPLY: dp[k] = fz_mul255(bp[k], sp[k]); break;
		case FZ_BLEND_SCREEN: dp[k] = fz_screen_byte(bp[k], sp[k]); break;
		case FZ_BLEND_OVERLAY: dp[k] = fz_overlay_byte(bp[k], sp[k]); break;
		case FZ_BLEND_DARKEN: dp[k] = fz_darken_byte(bp[k], sp[k]); break;
		case FZ_BLEND_LIGHTEN: dp[k] = fz_lighten_byte(bp[k], sp[k]); break;
		case FZ_BLEND_COLOR_DODGE: dp[k] = fz_color_dodge_byte(bp[k], sp[k]); break;
		case FZ_BLEND_COLOR_BURN: dp[k] = fz_color_burn_byte(bp[k], sp[k]); break;
		case FZ_BLEND_HARD_LIGHT: dp[k] = fz_hard_light_byte(bp[k], sp[k]); break;
		case FZ_BLEND_SOFT_LIGHT: dp[k] = fz_soft_light_byte(bp[k], sp[k]); break;
		case FZ_BLEND_DIFFERENCE: dp[k] = fz_difference_byte(bp[k], sp[k]); break;
		case FZ_BLEND_EXCLUSION: dp[k] = fz_exclusion_byte(bp[k], sp[k]); break;
		}
	}
}

/* Blending loops */

void
fz_blend_separable(byte * restrict bp, byte * restrict sp, int n, int w, int blendmode)
{
	int k;
	int n1 = n - 1;
	while (w--)
	{
		int sa = sp[n1];
		int ba = bp[n1];
		int saba = fz_mul255(sa, ba);

		/* ugh, division to get non-premul components */
		int invsa = sa ? 255 * 256 / sa : 0;
		int invba = ba ? 255 * 256 / ba : 0;

		for (k = 0; k < n1; k++)
		{
			int sc = (sp[k] * invsa) >> 8;
			int bc = (bp[k] * invba) >> 8;
			int rc;

			switch (blendmode)
			{
			default:
			case FZ_BLEND_NORMAL: rc = sc; break;
			case FZ_BLEND_MULTIPLY: rc = fz_mul255(bc, sc); break;
			case FZ_BLEND_SCREEN: rc = fz_screen_byte(bc, sc); break;
			case FZ_BLEND_OVERLAY: rc = fz_overlay_byte(bc, sc); break;
			case FZ_BLEND_DARKEN: rc = fz_darken_byte(bc, sc); break;
			case FZ_BLEND_LIGHTEN: rc = fz_lighten_byte(bc, sc); break;
			case FZ_BLEND_COLOR_DODGE: rc = fz_color_dodge_byte(bc, sc); break;
			case FZ_BLEND_COLOR_BURN: rc = fz_color_burn_byte(bc, sc); break;
			case FZ_BLEND_HARD_LIGHT: rc = fz_hard_light_byte(bc, sc); break;
			case FZ_BLEND_SOFT_LIGHT: rc = fz_soft_light_byte(bc, sc); break;
			case FZ_BLEND_DIFFERENCE: rc = fz_difference_byte(bc, sc); break;
			case FZ_BLEND_EXCLUSION: rc = fz_exclusion_byte(bc, sc); break;
			}

			bp[k] = fz_mul255(255 - sa, bp[k]) + fz_mul255(255 - ba, sp[k]) + fz_mul255(saba, rc);
		}

		bp[k] = ba + sa - saba;

		sp += n;
		bp += n;
	}
}

void
fz_blend_nonseparable(byte * restrict bp, byte * restrict sp, int w, int blendmode)
{
	while (w--)
	{
		unsigned char rr, rg, rb;

		int sa = sp[3];
		int ba = bp[3];
		int saba = fz_mul255(sa, ba);

		/* ugh, division to get non-premul components */
		int invsa = sa ? 255 * 256 / sa : 0;
		int invba = ba ? 255 * 256 / ba : 0;

		int sr = (sp[0] * invsa) >> 8;
		int sg = (sp[1] * invsa) >> 8;
		int sb = (sp[2] * invsa) >> 8;

		int br = (bp[0] * invba) >> 8;
		int bg = (bp[1] * invba) >> 8;
		int bb = (bp[2] * invba) >> 8;

		switch (blendmode)
		{
		default:
		case FZ_BLEND_HUE:
			fz_hue_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
			break;
		case FZ_BLEND_SATURATION:
			fz_saturation_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
			break;
		case FZ_BLEND_COLOR:
			fz_color_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
			break;
		case FZ_BLEND_LUMINOSITY:
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

static void
fz_blend_separable_nonisolated(byte * restrict bp, byte * restrict sp, int n, int w, int blendmode, byte * restrict hp, int alpha)
{
	int k;
	int n1 = n - 1;

	if (alpha == 255 && blendmode == 0)
	{
		/* In this case, the uncompositing and the recompositing
		 * cancel one another out, and it's just a simple copy. */
		/* FIXME: Maybe we can avoid using the shape plane entirely
		 * and just copy? */
		while (w--)
		{
			int ha = fz_mul255(*hp++, alpha); /* ha = shape_alpha */
			/* If ha == 0 then leave everything unchanged */
			if (ha != 0)
			{
				for (k = 0; k < n; k++)
				{
					bp[k] = sp[k];
				}
			}

			sp += n;
			bp += n;
		}
		return;
	}
	while (w--)
	{
		int ha = *hp++;
		int haa = fz_mul255(ha, alpha); /* ha = shape_alpha */
		/* If haa == 0 then leave everything unchanged */
		while (haa != 0) /* Use while, so we can break out */
		{
			int sa, ba, bahaa, ra, invsa, invba, invha, invra;
			sa = sp[n1];
			if (sa == 0)
				break; /* No change! */
			invsa = sa ? 255 * 256 / sa : 0;
			ba = bp[n1];
			if (ba == 0)
			{
				/* Just copy pixels (allowing for change in
				 * premultiplied alphas) */
				for (k = 0; k < n1; k++)
				{
					bp[k] = fz_mul255((sp[k] * invsa) >> 8, haa);
				}
				bp[n1] = haa;
				break;
			}
			bahaa = fz_mul255(ba, haa);

			/* ugh, division to get non-premul components */
			invba = ba ? 255 * 256 / ba : 0;

			/* Calculate result_alpha - a combination of the
			 * background alpha, and 'shape' */
			ra = bp[n1] = ba - bahaa + haa;
			if (ra == 0)
				break;
			/* Because we are a non-isolated group, we need to
			 * 'uncomposite' before we blend (recomposite).
			 * We assume that normal blending has been done inside
			 * the group, so: rc = (1-ha).bc + ha.sc
			 * A bit of rearrangement, and that gives us that:
			 * sc = (rc - bc)/ha + bc
			 * Now, the result of the blend (rc) was stored in src, so
			 * we actually want to calculate:
			 * sc = (sc-bc)/ha + bc
			 */
			invha = ha ? 255 * 256 / ha : 0;
			invra = ra ? 255 * 256 / ra : 0;

			/* sa = the final alpha to blend with - this
			 * is calculated from the shape + alpha,
			 * divided by ra. */
			sa = (haa*invra + 128)>>8;
			if (sa < 0) sa = 0;
			if (sa > 255) sa = 255;

			for (k = 0; k < n1; k++)
			{
				/* Read pixels (and convert to non-premultiplied form) */
				int sc = (sp[k] * invsa + 128) >> 8;
				int bc = (bp[k] * invba + 128) >> 8;
				int rc;

				/* Uncomposite (see above) */
				sc = (((sc-bc) * invha + 128)>>8) + bc;
				if (sc < 0) sc = 0;
				if (sc > 255) sc = 255;

				switch (blendmode)
				{
				default:
				case FZ_BLEND_NORMAL: rc = sc; break;
				case FZ_BLEND_MULTIPLY: rc = fz_mul255(bc, sc); break;
				case FZ_BLEND_SCREEN: rc = fz_screen_byte(bc, sc); break;
				case FZ_BLEND_OVERLAY: rc = fz_overlay_byte(bc, sc); break;
				case FZ_BLEND_DARKEN: rc = fz_darken_byte(bc, sc); break;
				case FZ_BLEND_LIGHTEN: rc = fz_lighten_byte(bc, sc); break;
				case FZ_BLEND_COLOR_DODGE: rc = fz_color_dodge_byte(bc, sc); break;
				case FZ_BLEND_COLOR_BURN: rc = fz_color_burn_byte(bc, sc); break;
				case FZ_BLEND_HARD_LIGHT: rc = fz_hard_light_byte(bc, sc); break;
				case FZ_BLEND_SOFT_LIGHT: rc = fz_soft_light_byte(bc, sc); break;
				case FZ_BLEND_DIFFERENCE: rc = fz_difference_byte(bc, sc); break;
				case FZ_BLEND_EXCLUSION: rc = fz_exclusion_byte(bc, sc); break;
				}
				/* Composition formula, as given in pdf_reference17.pdf:
				 * rc = ( 1 - (ha/ra)) * bc + (ha/ra) * ((1-ba)*sc + ba * rc)
				 */
				rc = bc + fz_mul255(sa, fz_mul255(255 - ba, sc) + fz_mul255(ba, rc) - bc);
				if (rc < 0) rc = 0;
				if (rc > 255) rc = 255;
				bp[k] = fz_mul255(rc, ra);
			}
			break;
		}

		sp += n;
		bp += n;
	}
}

static void
fz_blend_nonseparable_nonisolated(byte * restrict bp, byte * restrict sp, int w, int blendmode, byte * restrict hp, int alpha)
{
	while (w--)
	{
		int ha = *hp++;
		int haa = fz_mul255(ha, alpha);
		if (haa != 0)
		{
			int sa = sp[3];
			int ba = bp[3];
			int baha = fz_mul255(ba, haa);

			/* Calculate result_alpha */
			int ra = bp[3] = ba - baha + haa;
			if (ra != 0)
			{
				/* Because we are a non-isolated group, we
				 * need to 'uncomposite' before we blend
				 * (recomposite). We assume that normal
				 * blending has been done inside the group,
				 * so: ra.rc = (1-ha).bc + ha.sc
				 * A bit of rearrangement, and that gives us
				 * that: sc = (ra.rc - bc)/ha + bc
				 * Now, the result of the blend was stored in
				 * src, so: */
				int invha = ha ? 255 * 256 / ha : 0;

				unsigned char rr, rg, rb;

				/* ugh, division to get non-premul components */
				int invsa = sa ? 255 * 256 / sa : 0;
				int invba = ba ? 255 * 256 / ba : 0;

				int sr = (sp[0] * invsa) >> 8;
				int sg = (sp[1] * invsa) >> 8;
				int sb = (sp[2] * invsa) >> 8;

				int br = (bp[0] * invba) >> 8;
				int bg = (bp[1] * invba) >> 8;
				int bb = (bp[2] * invba) >> 8;

				/* Uncomposite */
				sr = (((sr-br)*invha)>>8) + br;
				sg = (((sg-bg)*invha)>>8) + bg;
				sb = (((sb-bb)*invha)>>8) + bb;

				switch (blendmode)
				{
				default:
				case FZ_BLEND_HUE:
					fz_hue_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
					break;
				case FZ_BLEND_SATURATION:
					fz_saturation_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
					break;
				case FZ_BLEND_COLOR:
					fz_color_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
					break;
				case FZ_BLEND_LUMINOSITY:
					fz_luminosity_rgb(&rr, &rg, &rb, br, bg, bb, sr, sg, sb);
					break;
				}

				rr = fz_mul255(255 - haa, bp[0]) + fz_mul255(fz_mul255(255 - ba, sr), haa) + fz_mul255(baha, rr);
				rg = fz_mul255(255 - haa, bp[1]) + fz_mul255(fz_mul255(255 - ba, sg), haa) + fz_mul255(baha, rg);
				rb = fz_mul255(255 - haa, bp[2]) + fz_mul255(fz_mul255(255 - ba, sb), haa) + fz_mul255(baha, rb);
				bp[0] = fz_mul255(ra, rr);
				bp[1] = fz_mul255(ra, rg);
				bp[2] = fz_mul255(ra, rb);
			}
		}

		sp += 4;
		bp += 4;
	}
}

void
fz_blend_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha, int blendmode, int isolated, fz_pixmap *shape)
{
	unsigned char *sp, *dp;
	fz_irect bbox;
	fz_irect bbox2;
	int x, y, w, h, n;

	/* TODO: fix this hack! */
	if (isolated && alpha < 255)
	{
		sp = src->samples;
		n = src->w * src->h * src->n;
		while (n--)
		{
			*sp = fz_mul255(*sp, alpha);
			sp++;
		}
	}

	fz_pixmap_bbox_no_ctx(dst, &bbox);
	fz_pixmap_bbox_no_ctx(src, &bbox2);
	fz_intersect_irect(&bbox, &bbox2);

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	n = src->n;
	sp = src->samples + (unsigned int)(((y - src->y) * src->w + (x - src->x)) * n);
	dp = dst->samples + (unsigned int)(((y - dst->y) * dst->w + (x - dst->x)) * n);

	assert(src->n == dst->n);

	if (!isolated)
	{
		unsigned char *hp = shape->samples + (unsigned int)((y - shape->y) * shape->w + (x - shape->x));

		while (h--)
		{
			if (n == 4 && blendmode >= FZ_BLEND_HUE)
				fz_blend_nonseparable_nonisolated(dp, sp, w, blendmode, hp, alpha);
			else
				fz_blend_separable_nonisolated(dp, sp, n, w, blendmode, hp, alpha);
			sp += src->w * n;
			dp += dst->w * n;
			hp += shape->w;
		}
	}
	else
	{
		while (h--)
		{
			if (n == 4 && blendmode >= FZ_BLEND_HUE)
				fz_blend_nonseparable(dp, sp, w, blendmode);
			else
				fz_blend_separable(dp, sp, n, w, blendmode);
			sp += src->w * n;
			dp += dst->w * n;
		}
	}
}
