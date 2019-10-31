#include "mupdf/fitz.h"
#include "draw-imp.h"

#include <string.h>
#include <math.h>
#include <assert.h>

/* PDF 1.4 blend modes. These are slow. */

/* Define PARANOID_PREMULTIPLY to check premultiplied values are
 * properly in range. */
#undef PARANOID_PREMULTIPLY

/*

Some notes on the transparency maths:

Compositing equation:
=====================

In section 7.2.2 (page 517) of pdf_reference17.pdf, it says:

 Cr = (1 - As/Ar) * Cb  + As/Ar * [ (1-Ab) * Cs + Ab * B(Cb,Cs) ]

It says that this is a simplified version of the more general form.

This equation is then restated in section 7.2.2 and it says:

The formula shown above is a simplification of the following formula:

 Ar * Cr = [(1-As)*Ab*Cb] + [(1-Ab)*As*Cs] + [Ab*As*B(Cb, Cs)]

At first glance this always appears to be a mistake to me, as it looks
like they have make a mistake in the division.

However, if we consider the result alpha equation:

 Ar = Union(Ab, As) = Ab + As - Ab * As

we can rearrange that to give:

 Ar - As = (1 - As) * Ab

 1 - As/Ar = (1 - As) * Ab / Ar

So substituting into the first equation above, we get:

 Cr = ((1 - As) * Ab/Ar) * Cb + As/Ar * [ (1-Ab) * Cs + Ab * B(Cb,Cs) ]

And thus:

 Ar * Cr = (1 - As) * Ab * Cb + As * [ (1-Ab)*Cs + Ab * B(Cb,Cs) ]

as required.

Alpha blending on top of compositing:
=====================================

Suppose we have a group to blend using blend mode B, and we want
to apply alpha too. Let's apply the blending first to get an
intermediate result (Ir), then apply the alpha to that to get the
result (Cr):

 Ir	= (1 - As/Ar) * Cb  + As/Ar * [ (1-Ab) * Cs + Ab * B(Cb,Cs) ]

 Cr	= (1-alpha) * Cb + alpha * Ir
	= Cb - alpha * Cb + alpha * Cb - alpha * Cb * As / Ar + alpha * As / Ar * [ (1 - Ab) * Cs + Ab * B(Cb, Cs) ]
	= Cb                           - alpha * Cb * As / Ar + alpha * As / Ar * [ (1 - Ab) * Cs + Ab * B(Cb, Cs) ]
	= Cb * (1 - alpha * As / Ar)                          + alpha * As / Ar * [ (1 - Ab) * Cs + Ab * B(Cb, Cs) ]

We want premultiplied results, so:

 Ar*Cr	= Cb * (Ar - alpha * As) + alpha * As * (1 - Ab) * Cs + alpha * As * Ab * B(Cb, Cs) ]

In the same way, for the alpha values:

 Ia	= Union(Ab, As) = Ab + As - As*Ab
 Ar	= (1-alpha) * Ab + alpha * Ia
	= Ab - alpha * Ab + alpha * Ab + alpha * As - alpha * As * Ab
	= Ab + alpha * As - alpha * As * Ab
	= Union(Ab, alpha * As)

*/

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

int fz_lookup_blendmode(const char *name)
{
	int i;
	for (i = 0; i < (int)nelem(fz_blendmode_names); i++)
		if (!strcmp(name, fz_blendmode_names[i]))
			return i;
	return FZ_BLEND_NORMAL;
}

char *fz_blendmode_name(int blendmode)
{
	if (blendmode >= 0 && blendmode < (int)nelem(fz_blendmode_names))
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
	if (b <= 0)
		return 0;
	else if (b >= s)
		return 255;
	else
		return (0x1fe * b + s) / (s << 1);
}

static inline int fz_color_burn_byte(int b, int s)
{
	b = 255 - b;
	if (b <= 0)
		return 255;
	else if (b >= s)
		return 0;
	else
		return 0xff - (0x1fe * b + s) / (s << 1);
}

static inline int fz_soft_light_byte(int b, int s)
{
	if (s < 128) {
		return b - fz_mul255(fz_mul255((255 - (s<<1)), b), 255 - b);
	}
	else {
		int dbd;
		if (b < 64)
			dbd = fz_mul255(fz_mul255((b << 4) - 3060, b) + 1020, b);
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

	/* 0.3f, 0.59f, 0.11f in fixed point */
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

/* Blending loops */

static inline void
fz_blend_separable(byte * FZ_RESTRICT bp, int bal, const byte * FZ_RESTRICT sp, int sal, int n1, int w, int blendmode, int complement, int first_spot)
{
	int k;
	do
	{
		int sa = (sal ? sp[n1] : 255);

		if (sa != 0)
		{
			int ba = (bal ? bp[n1] : 255);
			if (ba == 0)
			{
				memcpy(bp, sp, n1 + (sal && bal));
				if (bal && !sal)
					bp[n1+1] = 255;
			}
			else
			{
				int saba = fz_mul255(sa, ba);

				/* ugh, division to get non-premul components */
				int invsa = sa ? 255 * 256 / sa : 0;
				int invba = ba ? 255 * 256 / ba : 0;

				/* Process colorants */
				for (k = 0; k < first_spot; k++)
				{
					int sc = (sp[k] * invsa) >> 8;
					int bc = (bp[k] * invba) >> 8;
					int rc;

					if (complement)
					{
						sc = 255 - sc;
						bc = 255 - bc;
					}

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

					if (complement)
					{
						rc = 255 - rc;
					}

					bp[k] = fz_mul255(255 - sa, bp[k]) + fz_mul255(255 - ba, sp[k]) + fz_mul255(saba, rc);
				}

				/* spots */
				for (; k < n1; k++)
				{
					int sc = 255 - ((sp[k] * invsa) >> 8);
					int bc = 255 - ((bp[k] * invba) >> 8);
					int rc;

					switch (blendmode)
					{
					default:
					case FZ_BLEND_NORMAL:
					case FZ_BLEND_DIFFERENCE:
					case FZ_BLEND_EXCLUSION:
						rc = sc; break;
					case FZ_BLEND_MULTIPLY: rc = fz_mul255(bc, sc); break;
					case FZ_BLEND_SCREEN: rc = fz_screen_byte(bc, sc); break;
					case FZ_BLEND_OVERLAY: rc = fz_overlay_byte(bc, sc); break;
					case FZ_BLEND_DARKEN: rc = fz_darken_byte(bc, sc); break;
					case FZ_BLEND_LIGHTEN: rc = fz_lighten_byte(bc, sc); break;
					case FZ_BLEND_COLOR_DODGE: rc = fz_color_dodge_byte(bc, sc); break;
					case FZ_BLEND_COLOR_BURN: rc = fz_color_burn_byte(bc, sc); break;
					case FZ_BLEND_HARD_LIGHT: rc = fz_hard_light_byte(bc, sc); break;
					case FZ_BLEND_SOFT_LIGHT: rc = fz_soft_light_byte(bc, sc); break;
					}
					bp[k] = fz_mul255(255 - sa, bp[k]) + fz_mul255(255 - ba, sp[k]) + fz_mul255(saba, 255 - rc);
				}

				if (bal)
					bp[k] = ba + sa - saba;
			}
		}
		sp += n1 + sal;
		bp += n1 + bal;
	}
	while (--w);
}

static inline void
fz_blend_nonseparable_gray(byte * FZ_RESTRICT bp, int bal, const byte * FZ_RESTRICT sp, int sal, int n, int w, int blendmode, int first_spot)
{
	do
	{
		int sa = (sal ? sp[n] : 255);

		if (sa != 0)
		{
			int ba = (bal ? bp[n] : 255);
			if (ba == 0)
			{
				memcpy(bp, sp, n + (sal && bal));
				if (bal && !sal)
					bp [n + 1] = 255;
			}
			else
			{
				int saba = fz_mul255(sa, ba);

				/* ugh, division to get non-premul components */
				int invsa = sa ? 255 * 256 / sa : 0;
				int invba = ba ? 255 * 256 / ba : 0;
				int k;
				int sg = (sp[0] * invsa) >> 8;
				int bg = (bp[0] * invba) >> 8;

				switch (blendmode)
				{
				default:
				case FZ_BLEND_HUE:
				case FZ_BLEND_SATURATION:
				case FZ_BLEND_COLOR:
					bp[0] = fz_mul255(bp[n], bg);
					break;
				case FZ_BLEND_LUMINOSITY:
					bp[0] = fz_mul255(bp[n], sg);
					break;
				}

				/* Normal blend for spots */
				for (k = first_spot; k < n; k++)
				{
					int sc = (sp[k] * invsa) >> 8;
					bp[k] = fz_mul255(255 - sa, bp[k]) + fz_mul255(255 - ba, sp[k]) + fz_mul255(saba, sc);
				}
				if (bal)
					bp[n] = ba + sa - saba;
			}
		}
		sp += n + sal;
		bp += n + bal;
	} while (--w);
}

static inline void
fz_blend_nonseparable(byte * FZ_RESTRICT bp, int bal, const byte * FZ_RESTRICT sp, int sal, int n, int w, int blendmode, int complement, int first_spot)
{
	do
	{
		unsigned char rr, rg, rb;

		int sa = (sal ? sp[n] : 255);

		if (sa != 0)
		{
			int ba = (bal ? bp[n] : 255);
			if (ba == 0)
			{
				memcpy(bp, sp, n + (sal && bal));
				if (bal && !sal)
					bp [n + 1] = 255;
			}
			else
			{
				int k;
				int saba = fz_mul255(sa, ba);

				/* ugh, division to get non-premul components */
				int invsa = sa ? 255 * 256 / sa : 0;
				int invba = 255 * 256 / ba;

				int sr = (sp[0] * invsa) >> 8;
				int sg = (sp[1] * invsa) >> 8;
				int sb = (sp[2] * invsa) >> 8;

				int br = (bp[0] * invba) >> 8;
				int bg = (bp[1] * invba) >> 8;
				int bb = (bp[2] * invba) >> 8;

				/* CMYK */
				if (complement)
				{
					sr = 255 - sr;
					sg = 255 - sg;
					sb = 255 - sb;
					br = 255 - br;
					bg = 255 - bg;
					bb = 255 - bb;
				}

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

				/* CMYK */
				if (complement)
				{
					int sk = (sp[3] * invsa) >> 8;
					int bk = (bp[3] * invba) >> 8;

					rr = 255 - rr;
					rg = 255 - rg;
					rb = 255 - rb;
					bp[0] = fz_mul255(255 - sa, 255 - bp[0]) + fz_mul255(255 - ba, sp[0]) + fz_mul255(saba, rr);
					bp[1] = fz_mul255(255 - sa, 255 - bp[1]) + fz_mul255(255 - ba, sp[1]) + fz_mul255(saba, rg);
					bp[2] = fz_mul255(255 - sa, 255 - bp[2]) + fz_mul255(255 - ba, sp[2]) + fz_mul255(saba, rb);

					switch (blendmode)
					{
					default:
					case FZ_BLEND_HUE:
					case FZ_BLEND_SATURATION:
					case FZ_BLEND_COLOR:
						bp[3] = fz_mul255(bp[n], bk);
						break;
					case FZ_BLEND_LUMINOSITY:
						bp[3] = fz_mul255(bp[n], sk);
						break;
					}
				}
				else
				{
					bp[0] = fz_mul255(255 - sa, bp[0]) + fz_mul255(255 - ba, sp[0]) + fz_mul255(saba, rr);
					bp[1] = fz_mul255(255 - sa, bp[1]) + fz_mul255(255 - ba, sp[1]) + fz_mul255(saba, rg);
					bp[2] = fz_mul255(255 - sa, bp[2]) + fz_mul255(255 - ba, sp[2]) + fz_mul255(saba, rb);
				}

				if (bal)
					bp[n] = ba + sa - saba;

				/* Normal blend for spots */
				for (k = first_spot; k < n; k++)
				{
					int sc = (sp[k] * invsa) >> 8;
					bp[k] = fz_mul255(255 - sa, bp[k]) + fz_mul255(255 - ba, sp[k]) + fz_mul255(saba, sc);
				}
			}
		}
		sp += n + sal;
		bp += n + bal;
	}
	while (--w);
}

static inline void
fz_blend_separable_nonisolated(byte * FZ_RESTRICT bp, int bal, const byte * FZ_RESTRICT sp, int sal, int n1, int w, int blendmode, int complement, const byte * FZ_RESTRICT hp, int alpha, int first_spot)
{
	int k;

	if (sal == 0 && alpha == 255 && blendmode == 0)
	{
		/* In this case, the uncompositing and the recompositing
		 * cancel one another out, and it's just a simple copy. */
		/* FIXME: Maybe we can avoid using the shape plane entirely
		 * and just copy? */
		do
		{
			int ha = fz_mul255(*hp++, alpha); /* ha = shape_alpha */
			/* If ha == 0 then leave everything unchanged */
			if (ha != 0)
			{
				for (k = 0; k < n1; k++)
					bp[k] = sp[k];
				if (bal)
					bp[k] = 255;
			}

			sp += n1;
			bp += n1 + bal;
		}
		while (--w);
		return;
	}
	do
	{
		int ha = *hp++;
		int haa = fz_mul255(ha, alpha); /* ha = shape_alpha */
		/* If haa == 0 then leave everything unchanged */
		while (haa != 0) /* Use while, so we can break out */
		{
			int sa, ba, bahaa, ra, ra0, invsa, invba, scale;
			sa = (sal ? sp[n1] : 255);
			if (sa == 0)
				break; /* No change! */
			invsa = 255 * 256 / sa;
			ba = (bal ? bp[n1] : 255);
			if (ba == 0)
			{
				/* Just copy pixels (allowing for change in
				 * premultiplied alphas) */
				for (k = 0; k < n1; k++)
					bp[k] = fz_mul255((sp[k] * invsa) >> 8, haa);
				if (bal)
					bp[n1] = haa;
				break;
			}
			invba = 255 * 256 / ba;

			/* Because we are in a non-isolated group, we need to
			 * do some 'uncomposition' magic before we blend.
			 * My attempts to understand what is going on here have
			 * utterly failed, so I've resorted (after much patient
			 * help from Michael) to copying what the gs code does.
			 * This seems to be an implementation of the equations
			 * given on page 236 (section 7.3.3) of pdf_reference17.
			 * My understanding is that this is "composition" when
			 * we actually want to do "decomposition", hence my
			 * confusion. It appears to work though.
			 */
			scale = (512 * ba + ha) / (ha*2) - FZ_EXPAND(ba);

			sa = haa;

			/* Calculate result_alpha - a combination of the
			 * background alpha, and 'shape' */
			bahaa = fz_mul255(ba, haa);
			ra0 = ba - bahaa;
			ra = ra0 + haa;
			if (bal)
				bp[n1] = ra;

			if (ra == 0)
				break;

			/* Process colorants */
			for (k = 0; k < first_spot; k++)
			{
				/* Read pixels (and convert to non-premultiplied form) */
				int sc = (sp[k] * invsa) >> 8;
				int bc = (bp[k] * invba) >> 8;
				int rc;

				if (complement)
				{
					sc = 255 - sc;
					bc = 255 - bc;
				}

				/* Uncomposite (see above) */
				sc = sc + (((sc-bc) * scale)>>8);
				sc = fz_clampi(sc, 0, 255);

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

				/* From the notes at the top:
				 *
				 *  Ar * Cr = Cb * (Ar - alpha * As) + alpha * As * (1 - Ab) * Cs + alpha * As * Ab * B(Cb, Cs) ]
				 *
				 * And:
				 *
				 *  Ar = ba + haa - bahaa
				 *
				 * In our 0..255 world, with our current variables:
				 *
				 * ra.rc = bc * (ra - haa) + haa * (255 - ba) * sc + bahaa * B(Cb, Cs)
				 *       = bc * ra0        + haa * (255 - ba) * sc + bahaa * B(Cb, Cs)
				 */

				if (bahaa != 255)
					rc = fz_mul255(bahaa, rc);
				if (ba != 255)
				{
					int t = fz_mul255(255 - ba, haa);
					rc += fz_mul255(t, sc);
				}
				if (ra0 != 0)
					rc += fz_mul255(ra0, bc);

				if (complement)
					rc = ra - rc;

				bp[k] = fz_clampi(rc, 0, ra);
			}

			/* Spots */
			for (; k < n1; k++)
			{
				int sc = 255 - ((sp[k] * invsa + 128) >> 8);
				int bc = 255 - ((bp[k] * invba + 128) >> 8);
				int rc;

				sc = sc + (((sc-bc) * scale)>>8);

				/* Non-white preserving use Normal */
				switch (blendmode)
				{
				default:
				case FZ_BLEND_NORMAL:
				case FZ_BLEND_DIFFERENCE:
				case FZ_BLEND_EXCLUSION:
					rc = sc; break;
				case FZ_BLEND_MULTIPLY: rc = fz_mul255(bc, sc); break;
				case FZ_BLEND_SCREEN: rc = fz_screen_byte(bc, sc); break;
				case FZ_BLEND_OVERLAY: rc = fz_overlay_byte(bc, sc); break;
				case FZ_BLEND_DARKEN: rc = fz_darken_byte(bc, sc); break;
				case FZ_BLEND_LIGHTEN: rc = fz_lighten_byte(bc, sc); break;
				case FZ_BLEND_COLOR_DODGE: rc = fz_color_dodge_byte(bc, sc); break;
				case FZ_BLEND_COLOR_BURN: rc = fz_color_burn_byte(bc, sc); break;
				case FZ_BLEND_HARD_LIGHT: rc = fz_hard_light_byte(bc, sc); break;
				case FZ_BLEND_SOFT_LIGHT: rc = fz_soft_light_byte(bc, sc); break;
				}

				if (bahaa != 255)
					rc = fz_mul255(bahaa, rc);
				if (ba != 255)
				{
					int t = fz_mul255(255 - ba, haa);
					rc += fz_mul255(t, sc);
				}
				if (ra0 != 0)
					rc += fz_mul255(ra0, bc);

				bp[k] = ra - rc;
			}
			break;
		}

		sp += n1 + sal;
		bp += n1 + bal;
	}
	while (--w);
}

static inline void
fz_blend_nonseparable_nonisolated_gray(byte * FZ_RESTRICT bp, int bal, const byte * FZ_RESTRICT sp, int sal, int n, int w, int blendmode, const byte * FZ_RESTRICT hp, int alpha, int first_spot)
{
	do
	{
		int ha = *hp++;
		int haa = fz_mul255(ha, alpha);
		if (haa != 0)
		{
			int ba = (bal ? bp[n] : 255);

			if (ba == 0 && alpha == 255)
			{
				memcpy(bp, sp, n + (sal && bal));
				if (bal && !sal)
					bp[n+1] = 255;
			}
			else
			{
				int sa = (sal ? sp[n] : 255);
				int bahaa = fz_mul255(ba, haa);
				int k;

				/* Calculate result_alpha */
				int ra = ba - bahaa + haa;
				if (bal)
					bp[n] = ra;
				if (ra != 0)
				{
					int invha = ha ? 255 * 256 / ha : 0;

					/* ugh, division to get non-premul components */
					int invsa = sa ? 255 * 256 / sa : 0;
					int invba = ba ? 255 * 256 / ba : 0;

					int sg = (sp[0] * invsa) >> 8;
					int bg = (bp[0] * invba) >> 8;

					/* Uncomposite */
					sg = (((sg - bg)*invha) >> 8) + bg;
					sg = fz_clampi(sg, 0, 255);

					switch (blendmode)
					{
					default:
					case FZ_BLEND_HUE:
					case FZ_BLEND_SATURATION:
					case FZ_BLEND_COLOR:
						bp[0] = fz_mul255(ra, bg);
						break;
					case FZ_BLEND_LUMINOSITY:
						bp[0] = fz_mul255(ra, sg);
						break;
					}

					/* Normal blend for spots */
					for (k = first_spot; k < n; k++)
					{
						int sc = (sp[k] * invsa + 128) >> 8;
						int bc = (bp[k] * invba + 128) >> 8;
						int rc;

						sc = (((sc - bc) * invha + 128) >> 8) + bc;
						sc = fz_clampi(sc, 0, 255);
						rc = bc + fz_mul255(sa, fz_mul255(255 - ba, sc) + fz_mul255(ba, sc) - bc);
						rc = fz_clampi(rc, 0, 255);
						bp[k] = fz_mul255(rc, ra);
					}
				}
			}
		}
		sp += n + sal;
		bp += n + bal;
	} while (--w);
}

static inline void
fz_blend_nonseparable_nonisolated(byte * FZ_RESTRICT bp, int bal, const byte * FZ_RESTRICT sp, int sal, int n, int w, int blendmode, int complement, const byte * FZ_RESTRICT hp, int alpha, int first_spot)
{
	do
	{
		int ha = *hp++;
		int haa = fz_mul255(ha, alpha);
		if (haa != 0)
		{
			int sa = (sal ? sp[n] : 255);
			int ba = (bal ? bp[n] : 255);

			if (ba == 0 && alpha == 255)
			{
				memcpy(bp, sp, n + (sal && bal));
				if (bal && !sal)
					bp[n] = 255;
			}
			else
			{
				int bahaa = fz_mul255(ba, haa);

				/* Calculate result_alpha */
				int ra0 = ba - bahaa;
				int ra = ra0 + haa;

				if (bal)
					bp[n] = ra;

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
					int k;
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

					if (complement)
					{
						sr = 255 - sr;
						sg = 255 - sg;
						sb = 255 - sb;
						br = 255 - br;
						bg = 255 - bg;
						bb = 255 - bb;
					}

					/* Uncomposite */
					sr = (((sr - br)*invha) >> 8) + br;
					sr = fz_clampi(sr, 0, 255);
					sg = (((sg - bg)*invha) >> 8) + bg;
					sg = fz_clampi(sg, 0, 255);
					sb = (((sb - bb)*invha) >> 8) + bb;
					sb = fz_clampi(sb, 0, 255);

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

					/* From the notes at the top:
					 *
					 *  Ar * Cr = Cb * (Ar - alpha * As) + alpha * As * (1 - Ab) * Cs + alpha * As * Ab * B(Cb, Cs) ]
					 *
					 * And:
					 *
					 *  Ar = ba + haa - bahaa
					 *
					 * In our 0..255 world, with our current variables:
					 *
					 * ra.rc = bc * (ra - haa) + haa * (255 - ba) * sc + bahaa * B(Cb, Cs)
					 *       = bc * ra0        + haa * (255 - ba) * sc + bahaa * B(Cb, Cs)
					 */

					if (bahaa != 255)
					{
						rr = fz_mul255(bahaa, rr);
						rg = fz_mul255(bahaa, rg);
						rb = fz_mul255(bahaa, rb);
					}
					if (ba != 255)
					{
						int t = fz_mul255(255 - ba, haa);
						rr += fz_mul255(t, sr);
						rg += fz_mul255(t, sg);
						rb += fz_mul255(t, sb);
					}
					if (ra0 != 0)
					{
						rr += fz_mul255(ra0, br);
						rg += fz_mul255(ra0, bg);
						rb += fz_mul255(ra0, bb);
					}

					/* CMYK */
					if (complement)
					{
						int sk, bk, rk;

						/* Care must be taking when inverting here, as r = alpha * col.
						 * We want to store alpha * (255 - col) = alpha * 255 - alpha * col
						 */
						rr = ra - rr;
						rg = ra - rg;
						rb = ra - rb;

						sk = sa ? (sp[3] * invsa) >> 8 : 255;
						bk = ba ? (bp[3] * invba) >> 8 : 255;

						bk = fz_clampi(bk, 0, 255);
						sk = fz_clampi(sk, 0, 255);

						if (blendmode == FZ_BLEND_LUMINOSITY)
							rk = sk;
						else
							rk = bk;

						if (bahaa != 255)
							rk = fz_mul255(bahaa, rk);

						if (ba != 255)
						{
							int t = fz_mul255(255 - ba, haa);
							rk += fz_mul255(t, sk);
						}

						if (ra0 != 0)
							rk += fz_mul255(ra0, bk);

						bp[3] = rk;
					}

					bp[0] = rr;
					bp[1] = rg;
					bp[2] = rb;

					/* Normal blend for spots */
					for (k = first_spot; k < n; k++)
					{
						int sc = (sp[k] * invsa + 128) >> 8;
						int bc = (bp[k] * invba + 128) >> 8;
						int rc;

						sc = (((sc - bc) * invha + 128) >> 8) + bc;
						sc = fz_clampi(sc, 0, 255);
						rc = bc + fz_mul255(ha, fz_mul255(255 - ba, sc) + fz_mul255(ba, sc) - bc);
						rc = fz_clampi(rc, 0, 255);
						bp[k] = fz_mul255(rc, ra);
					}
				}
			}
		}
		sp += n + sal;
		bp += n + bal;
	}
	while (--w);
}

#ifdef PARANOID_PREMULTIPLY
static void
verify_premultiply(fz_context *ctx, const fz_pixmap * FZ_RESTRICT dst)
{
	unsigned char *dp = dst->samples;
	int w = dst->w;
	int h = dst->h;
	int n = dst->n;
	int x, y, i;
	int s = dst->stride - n * w;

	for (y = h; y > 0; y--)
	{
		for (x = w; x > 0; x--)
		{
			int a = dp[n-1];
			for (i = n-1; i > 0; i--)
				if (*dp++ > a)
					abort();
			dp++;
		}
		dp += s;
	}
}
#endif

void
fz_blend_pixmap(fz_context *ctx, fz_pixmap * FZ_RESTRICT dst, fz_pixmap * FZ_RESTRICT src, int alpha, int blendmode, int isolated, const fz_pixmap * FZ_RESTRICT shape)
{
	unsigned char *sp;
	unsigned char *dp;
	fz_irect bbox;
	int x, y, w, h, n;
	int da, sa;
	int complement;

	/* TODO: fix this hack! */
	if (isolated && alpha < 255)
	{
		unsigned char *sp2;
		int nn;
		h = src->h;
		sp2 = src->samples;
		nn = src->w * src->n;
		while (h--)
		{
			n = nn;
			while (n--)
			{
				*sp2 = fz_mul255(*sp2, alpha);
				sp2++;
			}
			sp2 += src->stride - nn;
		}
	}

	bbox = fz_intersect_irect(fz_pixmap_bbox(ctx, src), fz_pixmap_bbox(ctx, dst));

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	if (w == 0 || h == 0)
		return;

	complement = fz_colorspace_is_subtractive(ctx, src->colorspace);
	n = src->n;
	sp = src->samples + (unsigned int)((y - src->y) * src->stride + (x - src->x) * src->n);
	sa = src->alpha;
	dp = dst->samples + (unsigned int)((y - dst->y) * dst->stride + (x - dst->x) * dst->n);
	da = dst->alpha;

#ifdef PARANOID_PREMULTIPLY
	if (sa)
		verify_premultiply(ctx, src);
	if (da)
		verify_premultiply(ctx, dst);
#endif

	n -= sa;
	assert(n == dst->n - da);

	if (!isolated)
	{
		const unsigned char *hp = shape->samples + (unsigned int)((y - shape->y) * shape->stride + (x - shape->x));

		while (h--)
		{
			if (blendmode >= FZ_BLEND_HUE)
			{
				if (complement || src->s > 0)
					if ((n - src->s) == 1)
						fz_blend_nonseparable_nonisolated_gray(dp, da, sp, sa, n, w, blendmode, hp, alpha, 1);
					else
						fz_blend_nonseparable_nonisolated(dp, da, sp, sa, n, w, blendmode, complement, hp, alpha, n - src->s);
				else
					if (da)
						if (sa)
							if (n == 1)
								fz_blend_nonseparable_nonisolated_gray(dp, 1, sp, 1, 1, w, blendmode, hp, alpha, 1);
							else
								fz_blend_nonseparable_nonisolated(dp, 1, sp, 1, n, w, blendmode, complement, hp, alpha, n);
						else
							if (n == 1)
								fz_blend_nonseparable_nonisolated_gray(dp, 1, sp, 0, 1, w, blendmode, hp, alpha, 1);
							else
								fz_blend_nonseparable_nonisolated(dp, 1, sp, 0, n, w, blendmode, complement, hp, alpha, n);
					else
						if (sa)
							if (n == 1)
								fz_blend_nonseparable_nonisolated_gray(dp, 0, sp, 1, 1, w, blendmode, hp, alpha, 1);
							else
								fz_blend_nonseparable_nonisolated(dp, 0, sp, 1, n, w, blendmode, complement, hp, alpha, n);
						else
							if (n == 1)
								fz_blend_nonseparable_nonisolated_gray(dp, 0, sp, 0, 1, w, blendmode, hp, alpha, 1);
							else
								fz_blend_nonseparable_nonisolated(dp, 0, sp, 0, n, w, blendmode, complement, hp, alpha, n);
			}
			else
			{
				if (complement || src->s > 0)
					fz_blend_separable_nonisolated(dp, da, sp, sa, n, w, blendmode, complement, hp, alpha, n - src->s);
				else
					if (da)
						if (sa)
							fz_blend_separable_nonisolated(dp, 1, sp, 1, n, w, blendmode, 0, hp, alpha, n);
						else
							fz_blend_separable_nonisolated(dp, 1, sp, 0, n, w, blendmode, 0, hp, alpha, n);
					else
						if (sa)
							fz_blend_separable_nonisolated(dp, 0, sp, 1, n, w, blendmode, 0, hp, alpha, n);
						else
							fz_blend_separable_nonisolated(dp, 0, sp, 0, n, w, blendmode, 0, hp, alpha, n);
			}
			sp += src->stride;
			dp += dst->stride;
			hp += shape->stride;
		}
	}
	else
	{
		while (h--)
		{
			if (blendmode >= FZ_BLEND_HUE)
			{
				if (complement || src->s > 0)
					if ((n - src->s) == 1)
						fz_blend_nonseparable_gray(dp, da, sp, sa, n, w, blendmode, 1);
					else
						fz_blend_nonseparable(dp, da, sp, sa, n, w, blendmode, complement, n - src->s);
				else
					if (da)
						if (sa)
							if (n == 1)
								fz_blend_nonseparable_gray(dp, 1, sp, 1, 1, w, blendmode, 1);
							else
								fz_blend_nonseparable(dp, 1, sp, 1, n, w, blendmode, complement, n);
						else
							if (n == 1)
								fz_blend_nonseparable_gray(dp, 1, sp, 0, 1,  w, blendmode, 1);
							else
								fz_blend_nonseparable(dp, 1, sp, 0, n, w, blendmode, complement, n);
					else
						if (sa)
							if (n == 1)
								fz_blend_nonseparable_gray(dp, 0, sp, 1, 1, w, blendmode, 1);
							else
								fz_blend_nonseparable(dp, 0, sp, 1, n, w, blendmode, complement, n);
						else
							if (n == 1)
								fz_blend_nonseparable_gray(dp, 0, sp, 0, 1,  w, blendmode, 1);
							else
								fz_blend_nonseparable(dp, 0, sp, 0, n, w, blendmode, complement, n);
			}
			else
			{
				if (complement || src->s > 0)
					fz_blend_separable(dp, da, sp, sa, n, w, blendmode, complement, n - src->s);
				else
					if (da)
						if (sa)
							fz_blend_separable(dp, 1, sp, 1, n, w, blendmode, 0, n);
						else
							fz_blend_separable(dp, 1, sp, 0, n, w, blendmode, 0, n);
					else
						if (sa)
							fz_blend_separable(dp, 0, sp, 1, n, w, blendmode, 0, n);
						else
							fz_blend_separable(dp, 0, sp, 0, n, w, blendmode, 0, n);
			}
			sp += src->stride;
			dp += dst->stride;
		}
	}

#ifdef PARANOID_PREMULTIPLY
	if (da)
		verify_premultiply(ctx, dst);
#endif
}

static inline void
fz_blend_knockout(byte * FZ_RESTRICT bp, int bal, const byte * FZ_RESTRICT sp, int sal, int n1, int w, const byte * FZ_RESTRICT hp)
{
	int k;
	do
	{
		int ha = *hp++;

		if (ha != 0)
		{
			int sa = (sal ? sp[n1] : 255);
			int ba = (bal ? bp[n1] : 255);
			if (ba == 0 && ha == 0xFF)
			{
				memcpy(bp, sp, n1);
				if (bal)
					bp[n1] = sa;
			}
			else
			{
				int hasa = fz_mul255(ha, sa);
				/* ugh, division to get non-premul components */
				int invsa = sa ? 255 * 256 / sa : 0;
				int invba = ba ? 255 * 256 / ba : 0;
				int ra = hasa + fz_mul255(255-ha, ba);

				/* Process colorants + spots */
				for (k = 0; k < n1; k++)
				{
					int sc = (sp[k] * invsa) >> 8;
					int bc = (bp[k] * invba) >> 8;
					int rc = fz_mul255(255 - ha, bc) + fz_mul255(ha, sc);

					bp[k] = fz_mul255(ra, rc);
				}

				if (bal)
					bp[k] = ra;
			}
		}
		sp += n1 + sal;
		bp += n1 + bal;
	}
	while (--w);
}

void
fz_blend_pixmap_knockout(fz_context *ctx, fz_pixmap * FZ_RESTRICT dst, fz_pixmap * FZ_RESTRICT src, const fz_pixmap * FZ_RESTRICT shape)
{
	unsigned char *sp;
	unsigned char *dp;
	fz_irect sbox, dbox, bbox;
	int x, y, w, h, n;
	int da, sa;
	const unsigned char *hp;

	dbox = fz_pixmap_bbox_no_ctx(dst);
	sbox = fz_pixmap_bbox_no_ctx(src);
	bbox = fz_intersect_irect(dbox, sbox);

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	if (w == 0 || h == 0)
		return;

	n = src->n;
	sp = src->samples + (unsigned int)((y - src->y) * src->stride + (x - src->x) * src->n);
	sa = src->alpha;
	dp = dst->samples + (unsigned int)((y - dst->y) * dst->stride + (x - dst->x) * dst->n);
	da = dst->alpha;
	hp = shape->samples + (unsigned int)((y - shape->y) * shape->stride + (x - shape->x));

#ifdef PARANOID_PREMULTIPLY
	if (sa)
		verify_premultiply(ctx, src);
	if (da)
		verify_premultiply(ctx, dst);
#endif

	n -= sa;
	assert(n == dst->n - da);

	while (h--)
	{
		fz_blend_knockout(dp, da, sp, sa, n, w, hp);
		sp += src->stride;
		dp += dst->stride;
		hp += shape->stride;
	}

#ifdef PARANOID_PREMULTIPLY
	if (da)
		verify_premultiply(ctx, dst);
#endif
}
