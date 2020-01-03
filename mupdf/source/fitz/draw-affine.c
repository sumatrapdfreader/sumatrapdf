#include "mupdf/fitz.h"
#include "draw-imp.h"

#include <math.h>
#include <float.h>
#include <assert.h>

/* Number of fraction bits for fixed point math */
#define PREC 14
#define MASK ((1<<PREC)-1)
#define ONE (1<<PREC)
#define HALF (1<<(PREC-1))
#define LIMIT (1<<(31-PREC))

typedef unsigned char byte;

typedef void (paintfn_t)(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop);

static inline int lerp(int a, int b, int t)
{
	return a + (((b - a) * t) >> PREC);
}

static inline int bilerp(int a, int b, int c, int d, int u, int v)
{
	return lerp(lerp(a, b, u), lerp(c, d, u), v);
}

static inline const byte *sample_nearest(const byte *s, int w, int h, int str, int n, int u, int v)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= (w>>PREC)) u = (w>>PREC) - 1;
	if (v >= (h>>PREC)) v = (h>>PREC) - 1;
	return s + v * str + u * n;
}

/* Blend premultiplied source image in constant alpha over destination */

static inline void
template_affine_alpha_N_lerp(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int k;

	do
	{
		if (u + HALF >= 0 && u + ONE < sw && v + HALF >= 0 && v + ONE < sh)
		{
			int ui = u >> PREC;
			int vi = v >> PREC;
			int uf = u & MASK;
			int vf = v & MASK;
			const byte *a = sample_nearest(sp, sw, sh, ss, sn1+sa, ui, vi);
			const byte *b = sample_nearest(sp, sw, sh, ss, sn1+sa, ui+1, vi);
			const byte *c = sample_nearest(sp, sw, sh, ss, sn1+sa, ui, vi+1);
			const byte *d = sample_nearest(sp, sw, sh, ss, sn1+sa, ui+1, vi+1);
			int x = sa ? bilerp(a[sn1], b[sn1], c[sn1], d[sn1], uf, vf) : 255;
			int xa = sa ? fz_mul255(x, alpha) : alpha;
			if (xa != 0)
			{
				int t = 255 - xa;
				for (k = 0; k < sn1; k++)
				{
					int x = bilerp(a[k], b[k], c[k], d[k], uf, vf);
					dp[k] = fz_mul255(x, alpha) + fz_mul255(dp[k], t);
				}
				for (; k < dn1; k++)
					dp[k] = 0;
				if (da)
					dp[dn1] = xa + fz_mul255(dp[dn1], t);
				if (hp)
					hp[0] = x + fz_mul255(hp[0], 255 - x);
				if (gp)
					gp[0] = xa + fz_mul255(gp[0], t);
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_alpha_N_lerp_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	int k;

	do
	{
		if (u + HALF >= 0 && u + ONE < sw && v + HALF >= 0 && v + ONE < sh)
		{
			int ui = u >> PREC;
			int vi = v >> PREC;
			int uf = u & MASK;
			int vf = v & MASK;
			const byte *a = sample_nearest(sp, sw, sh, ss, sn1+sa, ui, vi);
			const byte *b = sample_nearest(sp, sw, sh, ss, sn1+sa, ui+1, vi);
			const byte *c = sample_nearest(sp, sw, sh, ss, sn1+sa, ui, vi+1);
			const byte *d = sample_nearest(sp, sw, sh, ss, sn1+sa, ui+1, vi+1);
			int x = sa ? bilerp(a[sn1], b[sn1], c[sn1], d[sn1], uf, vf) : 255;
			int xa = sa ? fz_mul255(x, alpha) : alpha;
			if (xa != 0)
			{
				int t = 255 - xa;
				for (k = 0; k < sn1; k++)
				{
					if (fz_overprint_component(eop, k))
					{
						int x = bilerp(a[k], b[k], c[k], d[k], uf, vf);
						dp[k] = fz_mul255(x, alpha) + fz_mul255(dp[k], t);
					}
				}
				for (; k < dn1; k++)
					if (fz_overprint_component(eop, k))
						dp[k] = 0;
				if (da)
					dp[dn1] = xa + fz_mul255(dp[dn1], t);
				if (hp)
					hp[0] = x + fz_mul255(hp[0], 255-x);
				if (gp)
					gp[0] = xa + fz_mul255(gp[0], t);
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

/* Special case code for gray -> rgb */
static inline void
template_affine_alpha_g2rgb_lerp(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	do
	{
		if (u + HALF >= 0 && u + ONE < sw && v + HALF >= 0 && v + ONE < sh)
		{
			int ui = u >> PREC;
			int vi = v >> PREC;
			int uf = u & MASK;
			int vf = v & MASK;
			const byte *a = sample_nearest(sp, sw, sh, ss, 1+sa, ui, vi);
			const byte *b = sample_nearest(sp, sw, sh, ss, 1+sa, ui+1, vi);
			const byte *c = sample_nearest(sp, sw, sh, ss, 1+sa, ui, vi+1);
			const byte *d = sample_nearest(sp, sw, sh, ss, 1+sa, ui+1, vi+1);
			int y = sa ? bilerp(a[1], b[1], c[1], d[1], uf, vf) : 255;
			int ya = (sa ? fz_mul255(y, alpha) : alpha);
			if (ya != 0)
			{
				int x = bilerp(a[0], b[0], c[0], d[0], uf, vf);
				int t = 255 - ya;
				x = fz_mul255(x, alpha);
				dp[0] = x + fz_mul255(dp[0], t);
				dp[1] = x + fz_mul255(dp[1], t);
				dp[2] = x + fz_mul255(dp[2], t);
				if (da)
					dp[3] = ya + fz_mul255(dp[3], t);
				if (hp)
					hp[0] = y + fz_mul255(hp[0], 255 - y);
				if (gp)
					gp[0] = ya + fz_mul255(gp[0], t);
			}
		}
		dp += 4;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_alpha_N_near_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int k;
	int ui = u >> PREC;
	TRACK_FN();
	if (ui < 0 || ui >= sw)
		return;
	sp += ui * (sn1+sa);
	do
	{
		int vi = v >> PREC;
		if (vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss);
			int a = sa ? sample[sn1] : 255;
			int aa = (sa ? fz_mul255(a, alpha) : alpha);
			if (aa != 0)
			{
				int t = 255 - aa;
				for (k = 0; k < sn1; k++)
					dp[k] = fz_mul255(sample[k], alpha) + fz_mul255(dp[k], t);
				for (; k < dn1; k++)
					dp[k] = 0;
				if (da)
					dp[dn1] = aa + fz_mul255(dp[dn1], t);
				if (hp)
					hp[0] = a + fz_mul255(hp[0], 255 - a);
				if (gp)
					gp[0] = aa + fz_mul255(gp[0], t);
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_alpha_N_near_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int k;
	int vi = v >> PREC;
	if (vi < 0 || vi >= sh)
		return;
	sp += vi * ss;
	do
	{
		int ui = u >> PREC;
		if (ui >= 0 && ui < sw)
		{
			const byte *sample = sp + (ui * (sn1+sa));
			int a = sa ? sample[sn1] : 255;
			int aa = (sa ? fz_mul255(a, alpha) : alpha);
			if (aa != 0)
			{
				int t = 255 - aa;
				for (k = 0; k < sn1; k++)
					dp[k] = fz_mul255(sample[k], alpha) + fz_mul255(dp[k], t);
				for (; k < dn1; k++)
					dp[k] = 0;
				if (da)
					dp[dn1] = aa + fz_mul255(dp[dn1], t);
				if (hp)
					hp[0] = a + fz_mul255(hp[0], 255 - a);
				if (gp)
					gp[0] = aa + fz_mul255(gp[0], t);
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
	}
	while (--w);
}

static inline void
template_affine_alpha_N_near(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int k;

	do
	{
		int ui = u >> PREC;
		int vi = v >> PREC;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss) + (ui * (sn1+sa));
			int a = sa ? sample[sn1] : 255;
			int aa = (sa ? fz_mul255(a, alpha) : alpha);
			if (aa != 0)
			{
				int t = 255 - aa;
				for (k = 0; k < sn1; k++)
					dp[k] = fz_mul255(sample[k], alpha) + fz_mul255(dp[k], t);
				for (; k < dn1; k++)
					dp[k] = 0;
				if (da)
					dp[dn1] = aa + fz_mul255(dp[dn1], t);
				if (hp)
					hp[0] = a + fz_mul255(hp[0], 255 - a);
				if (gp)
					gp[0] = aa + fz_mul255(gp[0], t);
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_alpha_N_near_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	int k;

	do
	{
		int ui = u >> PREC;
		int vi = v >> PREC;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss) + (ui * (sn1+sa));
			int a = sa ? sample[sn1] : 255;
			int aa = (sa ? fz_mul255(a, alpha) : alpha);
			if (aa != 0)
			{
				int t = 255 - aa;
				for (k = 0; k < sn1; k++)
					if (fz_overprint_component(eop, k))
						dp[k] = fz_mul255(sample[k], alpha) + fz_mul255(dp[k], t);
				for (; k < dn1; k++)
					if (fz_overprint_component(eop, k))
						dp[k] = 0;
				if (da)
					dp[dn1] = aa + fz_mul255(dp[dn1], t);
				if (hp)
					hp[0] = a + fz_mul255(hp[0], 255 - a);
				if (gp)
					gp[0] = aa + fz_mul255(gp[0], t);
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_alpha_g2rgb_near_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int ui = u >> PREC;
	if (ui < 0 || ui >= sw)
		return;
	sp += ui * (1+sa);
	do
	{
		int vi = v >> PREC;
		if (vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss);
			int x = fz_mul255(sample[0], alpha);
			int a = sa ? sample[1] : 255;
			int aa = (sa ? fz_mul255(a, alpha) : alpha);
			if (aa != 0)
			{
				int t = 255 - aa;
				dp[0] = x + fz_mul255(dp[0], t);
				dp[1] = x + fz_mul255(dp[1], t);
				dp[2] = x + fz_mul255(dp[2], t);
				if (da)
					dp[3] = aa + fz_mul255(dp[3], t);
				if (hp)
					hp[0] = a + fz_mul255(hp[0], 255 - a);
				if (gp)
					gp[0] = aa + fz_mul255(gp[0], t);
			}
		}
		dp += 3 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_alpha_g2rgb_near_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int vi = v >> PREC;
	if (vi < 0 || vi >= sh)
		return;
	sp += vi * ss;
	do
	{
		int ui = u >> PREC;
		if (ui >= 0 && ui < sw)
		{
			const byte *sample = sp + (ui * (1+sa));
			int x = fz_mul255(sample[0], alpha);
			int a = sa ? sample[1] : 255;
			int aa = (sa ? fz_mul255(a, alpha) : alpha);
			if (aa != 0)
			{
				int t = 255 - aa;
				dp[0] = x + fz_mul255(dp[0], t);
				dp[1] = x + fz_mul255(dp[1], t);
				dp[2] = x + fz_mul255(dp[2], t);
				if (da)
					dp[3] = aa + fz_mul255(dp[3], t);
				if (hp)
					hp[0] = a + fz_mul255(hp[0], 255 - a);
				if (gp)
					gp[0] = aa + fz_mul255(gp[0], t);
			}
		}
		dp += 3 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
	}
	while (--w);
}

static inline void
template_affine_alpha_g2rgb_near(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int alpha, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	do
	{
		int ui = u >> PREC;
		int vi = v >> PREC;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss) + (ui * (1+sa));
			int x = fz_mul255(sample[0], alpha);
			int a = sa ? sample[1] : 255;
			int aa = (sa ? fz_mul255(a, alpha): alpha);
			if (aa != 0)
			{
				int t = 255 - aa;
				dp[0] = x + fz_mul255(dp[0], t);
				dp[1] = x + fz_mul255(dp[1], t);
				dp[2] = x + fz_mul255(dp[2], t);
				if (da)
					dp[3] = aa + fz_mul255(dp[3], t);
				if (hp)
					hp[0] = a + fz_mul255(hp[0], 255 - a);
				if (gp)
					gp[0] = aa + fz_mul255(gp[0], t);
			}
		}
		dp += 3 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

/* Blend premultiplied source image over destination */
static inline void
template_affine_N_lerp(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int k;

	do
	{
		if (u + HALF >= 0 && u + ONE < sw && v + HALF >= 0 && v + ONE < sh)
		{
			int ui = u >> PREC;
			int vi = v >> PREC;
			int uf = u & MASK;
			int vf = v & MASK;
			const byte *a = sample_nearest(sp, sw, sh, ss, sn1+sa, ui, vi);
			const byte *b = sample_nearest(sp, sw, sh, ss, sn1+sa, ui+1, vi);
			const byte *c = sample_nearest(sp, sw, sh, ss, sn1+sa, ui, vi+1);
			const byte *d = sample_nearest(sp, sw, sh, ss, sn1+sa, ui+1, vi+1);
			int y = sa ? bilerp(a[sn1], b[sn1], c[sn1], d[sn1], uf, vf) : 255;
			if (y != 0)
			{
				int t = 255 - y;
				for (k = 0; k < sn1; k++)
				{
					int x = bilerp(a[k], b[k], c[k], d[k], uf, vf);
					dp[k] = x + fz_mul255(dp[k], t);
				}
				for (; k < dn1; k++)
					dp[k] = 0;
				if (da)
					dp[dn1] = y + fz_mul255(dp[dn1], t);
				if (hp)
					hp[0] = y + fz_mul255(hp[0], t);
				if (gp)
					gp[0] = y + fz_mul255(gp[0], t);
			}
		}
		dp += dn1 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_N_lerp_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	int k;

	do
	{
		if (u + HALF >= 0 && u + ONE < sw && v + HALF >= 0 && v + ONE < sh)
		{
			int ui = u >> PREC;
			int vi = v >> PREC;
			int uf = u & MASK;
			int vf = v & MASK;
			const byte *a = sample_nearest(sp, sw, sh, ss, sn1+sa, ui, vi);
			const byte *b = sample_nearest(sp, sw, sh, ss, sn1+sa, ui+1, vi);
			const byte *c = sample_nearest(sp, sw, sh, ss, sn1+sa, ui, vi+1);
			const byte *d = sample_nearest(sp, sw, sh, ss, sn1+sa, ui+1, vi+1);
			int y = sa ? bilerp(a[sn1], b[sn1], c[sn1], d[sn1], uf, vf) : 255;
			if (y != 0)
			{
				int t = 255 - y;
				for (k = 0; k < sn1; k++)
					if (fz_overprint_component(eop, k))
					{
						int x = bilerp(a[k], b[k], c[k], d[k], uf, vf);
						dp[k] = x + fz_mul255(dp[k], t);
					}
				for (; k < dn1; k++)
					if (fz_overprint_component(eop, k))
						dp[k] = 0;
				if (da)
					dp[dn1] = y + fz_mul255(dp[dn1], t);
				if (hp)
					hp[0] = y + fz_mul255(hp[0], t);
				if (gp)
					gp[0] = y + fz_mul255(gp[0], t);
			}
		}
		dp += dn1 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_solid_g2rgb_lerp(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	do
	{
		if (u + HALF >= 0 && u + ONE < sw && v + HALF >= 0 && v + ONE < sh)
		{
			int ui = u >> PREC;
			int vi = v >> PREC;
			int uf = u & MASK;
			int vf = v & MASK;
			const byte *a = sample_nearest(sp, sw, sh, ss, 1+sa, ui, vi);
			const byte *b = sample_nearest(sp, sw, sh, ss, 1+sa, ui+1, vi);
			const byte *c = sample_nearest(sp, sw, sh, ss, 1+sa, ui, vi+1);
			const byte *d = sample_nearest(sp, sw, sh, ss, 1+sa, ui+1, vi+1);
			int y = (sa ? bilerp(a[1], b[1], c[1], d[1], uf, vf) : 255);
			if (y != 0)
			{
				int t = 255 - y;
				int x = bilerp(a[0], b[0], c[0], d[0], uf, vf);
				dp[0] = x + fz_mul255(dp[0], t);
				dp[1] = x + fz_mul255(dp[1], t);
				dp[2] = x + fz_mul255(dp[2], t);
				if (da)
					dp[3] = y + fz_mul255(dp[3], t);
				if (hp)
					hp[0] = y + fz_mul255(hp[0], t);
				if (gp)
					gp[0] = y + fz_mul255(gp[0], t);
			}
		}
		dp += 3 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_N_near_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int k;
	int ui = u >> PREC;
	if (ui < 0 || ui >= sw)
		return;
	sp += ui*(sn1+sa);
	do
	{
		int vi = v >> PREC;
		if (vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss);
			int a = (sa ? sample[sn1] : 255);
			/* If a is 0, then sample[k] = 0 for all k, as premultiplied */
			if (a != 0)
			{
				int t = 255 - a;
				if (t == 0)
				{
					if (dn1+da == 4 && dn1+sa == 4)
					{
						*(int32_t *)dp = *(int32_t *)sample;
					}
					else
					{
						dp[0] = sample[0];
						if (sn1 > 1)
							dp[1] = sample[1];
						if (sn1 > 2)
							dp[2] = sample[2];
						for (k = 3; k < sn1; k++)
							dp[k] = sample[k];
						for (k = sn1; k < dn1; k++)
							dp[k] = 0;
						if (da)
							dp[dn1] = a;
					}
					if (hp)
						hp[0] = a;
					if (gp)
						gp[0] = a;
				}
				else
				{
					for (k = 0; k < sn1; k++)
						dp[k] = sample[k] + fz_mul255(dp[k], t);
					for (; k < dn1; k++)
						dp[k] = 0;
					if (da)
						dp[dn1] = a + fz_mul255(dp[dn1], t);
					if (hp)
						hp[0] = a + fz_mul255(hp[0], t);
					if (gp)
						gp[0] = a + fz_mul255(gp[0], t);
				}
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_N_near_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int k;
	int vi = v >> PREC;
	if (vi < 0 || vi >= sh)
		return;
	sp += vi * ss;
	do
	{
		int ui = u >> PREC;
		if (ui >= 0 && ui < sw)
		{
			const byte *sample = sp + (ui * (sn1+sa));
			int a = sa ? sample[sn1] : 255;
			/* If a is 0, then sample[k] = 0 for all k, as premultiplied */
			if (a != 0)
			{
				int t = 255 - a;
				if (t == 0)
				{
					if (dn1+da == 4 && sn1+sa == 4)
					{
						*(int32_t *)dp = *(int32_t *)sample;
					}
					else
					{
						dp[0] = sample[0];
						if (sn1 > 1)
							dp[1] = sample[1];
						if (sn1 > 2)
							dp[2] = sample[2];
						for (k = 3; k < sn1; k++)
							dp[k] = sample[k];
						for (k = sn1; k < dn1; k++)
							dp[k] = 0;
						if (da)
							dp[dn1] = a;
					}
					if (hp)
						hp[0] = a;
					if (gp)
						gp[0] = a;
				}
				else
				{
					for (k = 0; k < sn1; k++)
						dp[k] = sample[k] + fz_mul255(dp[k], t);
					for (; k < dn1; k++)
						dp[k] = 0;
					if (da)
						dp[dn1] = a + fz_mul255(dp[dn1], t);
					if (hp)
						hp[0] = a + fz_mul255(hp[0], t);
					if (gp)
						gp[0] = a + fz_mul255(gp[0], t);
				}
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
	}
	while (--w);
}

static inline void
template_affine_N_near(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int k;

	do
	{
		int ui = u >> PREC;
		int vi = v >> PREC;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss) + (ui * (sn1+sa));
			int a = sa ? sample[sn1] : 255;
			/* If a is 0, then sample[k] = 0 for all k, as premultiplied */
			if (a != 0)
			{
				int t = 255 - a;
				if (t == 0)
				{
					if (dn1+da == 4 && sn1+sa == 4)
					{
						*(int32_t *)dp = *(int32_t *)sample;
					}
					else
					{
						dp[0] = sample[0];
						if (sn1 > 1)
							dp[1] = sample[1];
						if (sn1 > 2)
							dp[2] = sample[2];
						for (k = 3; k < sn1; k++)
							dp[k] = sample[k];
						for (; k < dn1; k++)
							dp[k] = 0;
						if (da)
							dp[dn1] = a;
					}
					if (hp)
						hp[0] = a;
					if (gp)
						gp[0] = a;
				}
				else
				{
					for (k = 0; k < sn1; k++)
						dp[k] = sample[k] + fz_mul255(dp[k], t);
					for (; k < dn1; k++)
						dp[k] = 0;
					if (da)
						dp[dn1] = a + fz_mul255(dp[dn1], t);
					if (hp)
						hp[0] = a + fz_mul255(hp[0], t);
					if (gp)
						gp[0] = a + fz_mul255(gp[0], t);
				}
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_N_near_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	int k;

	do
	{
		int ui = u >> PREC;
		int vi = v >> PREC;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss) + (ui * (sn1+sa));
			int a = sa ? sample[sn1] : 255;
			/* If a is 0, then sample[k] = 0 for all k, as premultiplied */
			if (a != 0)
			{
				int t = 255 - a;
				if (t == 0)
				{
					if (fz_overprint_component(eop, 0))
						dp[0] = sample[0];
					if (sn1 > 1)
						if (fz_overprint_component(eop, 1))
							dp[1] = sample[1];
					if (sn1 > 2)
						if (fz_overprint_component(eop, 2))
							dp[2] = sample[2];
					for (k = 3; k < sn1; k++)
						if (fz_overprint_component(eop, k))
							dp[k] = sample[k];
					for (; k < dn1; k++)
						if (fz_overprint_component(eop, k))
							dp[k] = 0;
					if (da)
						dp[dn1] = a;
					if (hp)
						hp[0] = a;
					if (gp)
						gp[0] = a;
				}
				else
				{
					for (k = 0; k < sn1; k++)
						if (fz_overprint_component(eop, k))
							dp[k] = sample[k] + fz_mul255(dp[k], t);
					for (; k < dn1; k++)
						if (fz_overprint_component(eop, k))
							dp[k] = 0;
					if (da)
						dp[dn1] = a + fz_mul255(dp[dn1], t);
					if (hp)
						hp[0] = a + fz_mul255(hp[0], t);
					if (gp)
						gp[0] = a + fz_mul255(gp[0], t);
				}
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_solid_g2rgb_near_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int ui = u >> PREC;
	if (ui < 0 || ui >= sw)
		return;
	sp += ui * (1+sa);
	do
	{
		int vi = v >> PREC;
		if (vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss);
			int a = (sa ? sample[1] : 255);
			if (a != 0)
			{
				int x = sample[0];
				int t = 255 - a;
				if (t == 0)
				{
					dp[0] = x;
					dp[1] = x;
					dp[2] = x;
					if (da)
						dp[3] = a;
					if (hp)
						hp[0] = a;
					if (gp)
						gp[0] = a;
				}
				else
				{
					dp[0] = x + fz_mul255(dp[0], t);
					dp[1] = x + fz_mul255(dp[1], t);
					dp[2] = x + fz_mul255(dp[2], t);
					if (da)
						dp[3] = a + fz_mul255(dp[3], t);
					if (hp)
						hp[0] = a + fz_mul255(hp[0], t);
					if (gp)
						gp[0] = a + fz_mul255(gp[0], t);
				}
			}
		}
		dp += 3 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_solid_g2rgb_near_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int vi = v >> PREC;
	if (vi < 0 || vi >= sh)
		return;
	sp += vi * ss;
	do
	{
		int ui = u >> PREC;
		if (ui >= 0 && ui < sw)
		{
			const byte *sample = sp + (ui * (1+sa));
			int a = (sa ? sample[1] : 255);
			if (a != 0)
			{
				int x = sample[0];
				int t = 255 - a;
				if (t == 0)
				{
					dp[0] = x;
					dp[1] = x;
					dp[2] = x;
					if (da)
						dp[3] = a;
					if (hp)
						hp[0] = a;
					if (gp)
						gp[0] = a;
				}
				else
				{
					dp[0] = x + fz_mul255(dp[0], t);
					dp[1] = x + fz_mul255(dp[1], t);
					dp[2] = x + fz_mul255(dp[2], t);
					if (da)
						dp[3] = a + fz_mul255(dp[3], t);
					if (hp)
						hp[0] = a + fz_mul255(hp[0], t);
					if (gp)
						gp[0] = a + fz_mul255(gp[0], t);
				}
			}
		}
		dp += 3 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
	}
	while (--w);
}

static inline void
template_affine_solid_g2rgb_near(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	do
	{
		int ui = u >> PREC;
		int vi = v >> PREC;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			const byte *sample = sp + (vi * ss) + (ui * (1+sa));
			int a = sa ? sample[1] : 255;
			if (a != 0)
			{
				int x = sample[0];
				int t = 255 - a;
				if (t == 0)
				{
					dp[0] = x;
					dp[1] = x;
					dp[2] = x;
					if (da)
						dp[3] = a;
					if (hp)
						hp[0] = a;
					if (gp)
						gp[0] = a;
				}
				else
				{
					dp[0] = x + fz_mul255(dp[0], t);
					dp[1] = x + fz_mul255(dp[1], t);
					dp[2] = x + fz_mul255(dp[2], t);
					if (da)
						dp[3] = a + fz_mul255(dp[3], t);
					if (hp)
						hp[0] = a + fz_mul255(hp[0], t);
					if (gp)
						gp[0] = a + fz_mul255(gp[0], t);
				}
			}
		}
		dp += 3 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

/* Blend non-premultiplied color in source image mask over destination */

static inline void
template_affine_color_N_lerp(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int u, int v, int fa, int fb, int w, int dn1, int sn1, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int sa = color[dn1];
	int k;

	do
	{
		if (u + HALF >= 0 && u + ONE < sw && v + HALF >= 0 && v + ONE < sh)
		{
			int ui = u >> PREC;
			int vi = v >> PREC;
			int uf = u & MASK;
			int vf = v & MASK;
			const byte *a = sample_nearest(sp, sw, sh, ss, 1, ui, vi);
			const byte *b = sample_nearest(sp, sw, sh, ss, 1, ui+1, vi);
			const byte *c = sample_nearest(sp, sw, sh, ss, 1, ui, vi+1);
			const byte *d = sample_nearest(sp, sw, sh, ss, 1, ui+1, vi+1);
			int ma = bilerp(a[0], b[0], c[0], d[0], uf, vf);
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			if (masa != 0)
			{
				for (k = 0; k < dn1; k++)
					dp[k] = FZ_BLEND(color[k], dp[k], masa);
				if (da)
					dp[dn1] = FZ_BLEND(255, dp[dn1], masa);
				if (hp)
					hp[0] = FZ_BLEND(255, hp[0], ma);
				if (gp)
					gp[0] = FZ_BLEND(255, gp[0], masa);
			}
		}
		dp += dn1 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_color_N_lerp_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int u, int v, int fa, int fb, int w, int dn1, int sn1, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	int sa = color[dn1];
	int k;

	do
	{
		if (u + HALF >= 0 && u + ONE < sw && v + HALF >= 0 && v + ONE < sh)
		{
			int ui = u >> PREC;
			int vi = v >> PREC;
			int uf = u & MASK;
			int vf = v & MASK;
			const byte *a = sample_nearest(sp, sw, sh, ss, 1, ui, vi);
			const byte *b = sample_nearest(sp, sw, sh, ss, 1, ui+1, vi);
			const byte *c = sample_nearest(sp, sw, sh, ss, 1, ui, vi+1);
			const byte *d = sample_nearest(sp, sw, sh, ss, 1, ui+1, vi+1);
			int ma = bilerp(a[0], b[0], c[0], d[0], uf, vf);
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			if (masa != 0)
			{
				for (k = 0; k < dn1; k++)
					if (fz_overprint_component(eop, k))
						dp[k] = FZ_BLEND(color[k], dp[k], masa);
				if (da)
					dp[dn1] = FZ_BLEND(255, dp[dn1], masa);
				if (hp)
					hp[0] = FZ_BLEND(255, hp[0], ma);
				if (gp)
					gp[0] = FZ_BLEND(255, gp[0], masa);
			}
		}
		dp += dn1 + da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_color_N_near(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int u, int v, int fa, int fb, int w, int dn1, int sn1, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp)
{
	int sa = color[dn1];
	int k;

	do
	{
		int ui = u >> PREC;
		int vi = v >> PREC;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			int ma = sp[vi * ss + ui];
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			if (masa)
			{
				for (k = 0; k < dn1; k++)
					dp[k] = FZ_BLEND(color[k], dp[k], masa);
				if (da)
					dp[dn1] = FZ_BLEND(255, dp[dn1], masa);
				if (hp)
					hp[0] = FZ_BLEND(255, hp[0], ma);
				if (gp)
					gp[0] = FZ_BLEND(255, gp[0], masa);
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static inline void
template_affine_color_N_near_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int u, int v, int fa, int fb, int w, int dn1, int sn1, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	int sa = color[dn1];
	int k;

	do
	{
		int ui = u >> PREC;
		int vi = v >> PREC;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			int ma = sp[vi * ss + ui];
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			if (masa)
			{
				for (k = 0; k < dn1; k++)
					if (fz_overprint_component(eop, k))
						dp[k] = FZ_BLEND(color[k], dp[k], masa);
				if (da)
					dp[dn1] = FZ_BLEND(255, dp[dn1], masa);
				if (hp)
					hp[0] = FZ_BLEND(255, hp[0], ma);
				if (gp)
					gp[0] = FZ_BLEND(255, gp[0], masa);
			}
		}
		dp += dn1+da;
		if (hp)
			hp++;
		if (gp)
			gp++;
		u += fa;
		v += fb;
	}
	while (--w);
}

static void
paint_affine_lerp_da_sa_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 0, 0, hp, gp);
}

static void
paint_affine_lerp_da_sa_alpha_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 0, 0, alpha, hp, gp);
}

static void
paint_affine_lerp_da_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 0, 0, hp, gp);
}

static void
paint_affine_lerp_da_alpha_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 0, 0, alpha, hp, gp);
}

static void
paint_affine_lerp_da_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_lerp_da_alpha_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_lerp_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_lerp_alpha_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

#if FZ_PLOTTERS_G
static void
paint_affine_lerp_da_sa_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_lerp_da_sa_alpha_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_lerp_sa_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_lerp_sa_alpha_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}
#endif /* FZ_PLOTTERS_G */

#if FZ_PLOTTERS_RGB
static void
paint_affine_lerp_da_sa_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_lerp_da_sa_alpha_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_lerp_da_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_lerp_da_alpha_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_lerp_sa_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_lerp_sa_alpha_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_lerp_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_lerp_alpha_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}
#endif /* FZ_PLOTTERS_RGB */

#if FZ_PLOTTERS_CMYK
static void
paint_affine_lerp_da_sa_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_lerp_da_sa_alpha_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_lerp_da_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_lerp_da_alpha_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_lerp_sa_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_lerp_sa_alpha_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_lerp_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_lerp_alpha_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}
#endif /* FZ_PLOTTERS_CMYK */

#if FZ_PLOTTERS_N
static void
paint_affine_lerp_da_sa_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_lerp_da_sa_alpha_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_lerp_da_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_lerp_da_alpha_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_lerp_sa_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_lerp_sa_alpha_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_lerp_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_lerp_alpha_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}
#endif /* FZ_PLOTTERS_N */

#if FZ_ENABLE_SPOT_RENDERING
static void
paint_affine_lerp_N_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_lerp_op(dp, da, sp, sw, sh, ss, sa, u, v, fa, fb, w, dn, sn, hp, gp, eop);
}

static void
paint_affine_lerp_alpha_N_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_lerp_op(dp, da, sp, sw, sh, ss, sa, u, v, fa, fb, w, dn, sn, alpha, hp, gp, eop);
}
#endif /* FZ_ENABLE_SPOT_RENDERING */

static paintfn_t *
fz_paint_affine_lerp(int da, int sa, int fa, int fb, int n, int alpha, const fz_overprint * FZ_RESTRICT eop)
{
#if FZ_ENABLE_SPOT_RENDERING
	if (fz_overprint_required(eop))
	{
		if (alpha == 255)
			return paint_affine_lerp_N_op;
		else if (alpha > 0)
			return paint_affine_lerp_alpha_N_op;
		else
			return NULL;
	}
#endif /* FZ_ENABLE_SPOT_RENDERING */

	switch(n)
	{
	case 0:
		if (da)
		{
			if (sa)
			{
				if (alpha == 255)
					return paint_affine_lerp_da_sa_0;
				else if (alpha > 0)
					return paint_affine_lerp_da_sa_alpha_0;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_da_0;
				else if (alpha > 0)
					return paint_affine_lerp_da_alpha_0;
			}
		}
		break;

	case 1:
		if (sa)
		{
#if FZ_PLOTTERS_G
			if (da)
			{
				if (alpha == 255)
					return paint_affine_lerp_da_sa_1;
				else if (alpha > 0)
					return paint_affine_lerp_da_sa_alpha_1;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_sa_1;
				else if (alpha > 0)
					return paint_affine_lerp_sa_alpha_1;
			}
#else
			goto fallback;
#endif /* FZ_PLOTTERS_H */
		}
		else
		{
			if (da)
			{
				if (alpha == 255)
					return paint_affine_lerp_da_1;
				else if (alpha > 0)
					return paint_affine_lerp_da_alpha_1;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_1;
				else if (alpha > 0)
					return paint_affine_lerp_alpha_1;
			}
		}
		break;

#if FZ_PLOTTERS_RGB
	case 3:
		if (da)
		{
			if (sa)
			{
				if (alpha == 255)
					return paint_affine_lerp_da_sa_3;
				else if (alpha > 0)
					return paint_affine_lerp_da_sa_alpha_3;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_da_3;
				else if (alpha > 0)
					return paint_affine_lerp_da_alpha_3;
			}
		}
		else
		{
			if (sa)
			{
				if (alpha == 255)
					return paint_affine_lerp_sa_3;
				else if (alpha > 0)
					return paint_affine_lerp_sa_alpha_3;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_3;
				else if (alpha > 0)
					return paint_affine_lerp_alpha_3;
			}
		}
		break;
#endif /* FZ_PLOTTERS_RGB */

#if FZ_PLOTTERS_CMYK
	case 4:
		if (da)
		{
			if (sa)
			{
				if (alpha == 255)
					return paint_affine_lerp_da_sa_4;
				else if (alpha > 0)
					return paint_affine_lerp_da_sa_alpha_4;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_da_4;
				else if (alpha > 0)
					return paint_affine_lerp_da_alpha_4;
			}
		}
		else
		{
			if (sa)
			{
				if (alpha == 255)
					return paint_affine_lerp_sa_4;
				else if (alpha > 0)
					return paint_affine_lerp_sa_alpha_4;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_4;
				else if (alpha > 0)
					return paint_affine_lerp_alpha_4;
			}
		}
		break;
#endif /* FZ_PLOTTERS_CMYK */

#if !FZ_PLOTTERS_G
fallback:
#endif /* FZ_PLOTTERS_G */
	default:
#if FZ_PLOTTERS_N
		if (da)
		{
			if (sa)
			{
				if (alpha == 255)
					return paint_affine_lerp_da_sa_N;
				else if (alpha > 0)
					return paint_affine_lerp_da_sa_alpha_N;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_da_N;
				else if (alpha > 0)
					return paint_affine_lerp_da_alpha_N;
			}
		}
		else
		{
			if (sa)
			{
				if (alpha == 255)
					return paint_affine_lerp_sa_N;
				else if (alpha > 0)
					return paint_affine_lerp_sa_alpha_N;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_lerp_N;
				else if (alpha > 0)
					return paint_affine_lerp_alpha_N;
			}
		}
#endif /* FZ_PLOTTERS_N */
		break;
	}
	return NULL;
}

#if FZ_ENABLE_SPOT_RENDERING
static paintfn_t *
fz_paint_affine_lerp_spots(int da, int sa, int fa, int fb, int dn, int sn, int alpha, const fz_overprint * FZ_RESTRICT eop)
{
	if (fz_overprint_required(eop))
	{
		if (alpha == 255)
			return paint_affine_lerp_N_op;
		else if (alpha > 0)
			return paint_affine_lerp_alpha_N_op;
	}
	else if (da)
	{
		if (sa)
		{
			if (alpha == 255)
				return paint_affine_lerp_da_sa_N;
			else if (alpha > 0)
				return paint_affine_lerp_da_sa_alpha_N;
		}
		else
		{
			if (alpha == 255)
				return paint_affine_lerp_da_N;
			else if (alpha > 0)
				return paint_affine_lerp_da_alpha_N;
		}
	}
	else
	{
		if (sa)
		{
			if (alpha == 255)
				return paint_affine_lerp_sa_N;
			else if (alpha > 0)
				return paint_affine_lerp_sa_alpha_N;
		}
		else
		{
			if (alpha == 255)
				return paint_affine_lerp_N;
			else if (alpha > 0)
				return paint_affine_lerp_alpha_N;
		}
	}
	return NULL;
}
#endif /* FZ_ENABLE_SPOT_RENDERING */

#if FZ_PLOTTERS_RGB
static void
paint_affine_lerp_da_sa_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_lerp_da_sa_g2rgb_alpha(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_lerp(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_lerp_da_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_lerp_da_g2rgb_alpha(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_lerp(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_lerp_sa_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_lerp_sa_g2rgb_alpha(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_lerp(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_lerp_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_lerp_g2rgb_alpha(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_lerp(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, alpha, hp, gp);
}

static paintfn_t *
fz_paint_affine_g2rgb_lerp(int da, int sa, int fa, int fb, int n, int alpha)
{
	if (da)
	{
		if (sa)
		{
			if (alpha == 255)
				return paint_affine_lerp_da_sa_g2rgb;
			else if (alpha > 0)
				return paint_affine_lerp_da_sa_g2rgb_alpha;
		}
		else
		{
			if (alpha == 255)
				return paint_affine_lerp_da_g2rgb;
			else if (alpha > 0)
				return paint_affine_lerp_da_g2rgb_alpha;
		}
	}
	else
	{
		if (sa)
		{
			if (alpha == 255)
				return paint_affine_lerp_sa_g2rgb;
			else if (alpha > 0)
				return paint_affine_lerp_sa_g2rgb_alpha;
		}
		else
		{
			if (alpha == 255)
				return paint_affine_lerp_g2rgb;
			else if (alpha > 0)
				return paint_affine_lerp_g2rgb_alpha;
		}
	}
	return NULL;
}
#endif /* FZ_PLOTTERS_RGB */

static void
paint_affine_near_da_sa_0_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 0, 0, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_0_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 0, 0, alpha, hp, gp);
}

static void
paint_affine_near_da_0_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 0, 0, hp, gp);
}

static void
paint_affine_near_da_alpha_0_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 0, 0, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_0_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 0, 0, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_0_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 0, 0, alpha, hp, gp);
}

static void
paint_affine_near_da_0_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 0, 0, hp, gp);
}

static void
paint_affine_near_da_alpha_0_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 0, 0, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 0, 0, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 0, 0, alpha, hp, gp);
}

static void
paint_affine_near_da_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 0, 0, hp, gp);
}

static void
paint_affine_near_da_alpha_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 0, 0, alpha, hp, gp);
}

static void
paint_affine_near_1_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int snn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_1_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_alpha_1_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_alpha_1_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_alpha_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_da_1_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_da_alpha_1_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_da_1_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_da_alpha_1_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_da_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_da_alpha_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

#if FZ_PLOTTERS_G
static void
paint_affine_near_da_sa_1_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_1_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_sa_1_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_sa_alpha_1_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_1_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_1_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_sa_alpha_1_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_sa_1_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}

static void
paint_affine_near_sa_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, hp, gp);
}

static void
paint_affine_near_sa_alpha_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 1, 1, alpha, hp, gp);
}
#endif /* FZ_PLOTTERS_G */

#if FZ_PLOTTERS_RGB
static void
paint_affine_near_da_sa_3_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_3_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_da_3_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_da_alpha_3_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_sa_3_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_sa_alpha_3_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_3_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_alpha_3_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_3_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_3_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_da_3_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_da_alpha_3_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_sa_alpha_3_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_alpha_3_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_3_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_sa_3_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_da_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_sa_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_da_alpha_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_sa_alpha_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}

static void
paint_affine_near_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, hp, gp);
}

static void
paint_affine_near_alpha_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 3, 3, alpha, hp, gp);
}
#endif /* FZ_PLOTTERS_RGB */

#if FZ_PLOTTERS_CMYK
static void
paint_affine_near_da_sa_4_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_4_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_da_4_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_da_alpha_4_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_sa_4_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_sa_alpha_4_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_4_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_alpha_4_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_4_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_4_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_da_4_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_da_alpha_4_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_sa_4_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_sa_alpha_4_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_4_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_alpha_4_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_da_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_da_alpha_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_sa_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_sa_alpha_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}

static void
paint_affine_near_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, hp, gp);
}

static void
paint_affine_near_alpha_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, 4, 4, alpha, hp, gp);
}
#endif /* FZ_PLOTTERS_CMYK */

#if FZ_PLOTTERS_N
static void
paint_affine_near_da_sa_N_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_N_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_da_N_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_da_alpha_N_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_sa_N_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_sa_alpha_N_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_N_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_alpha_N_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_N_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_N_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_da_N_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_da_alpha_N_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_sa_N_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_sa_alpha_N_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_N_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_alpha_N_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_da_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_da_alpha_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_sa_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_sa_alpha_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}

static void
paint_affine_near_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, hp, gp);
}

static void
paint_affine_near_alpha_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, dn, sn, alpha, hp, gp);
}
#endif /* FZ_PLOTTERS_N */

#if FZ_ENABLE_SPOT_RENDERING
static void
paint_affine_near_N_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_N_near_op(dp, da, sp, sw, sh, ss, sa, u, v, fa, fb, w, dn, sn, hp, gp, eop);
}

static void
paint_affine_near_alpha_N_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_N_near_op(dp, da, sp, sw, sh, ss, sa, u, v, fa, fb, w, dn, sn, alpha, hp, gp, eop);
}
#endif /* FZ_ENABLE_SPOT_RENDERING */

static paintfn_t *
fz_paint_affine_near(int da, int sa, int fa, int fb, int n, int alpha, const fz_overprint * FZ_RESTRICT eop)
{
#if FZ_ENABLE_SPOT_RENDERING
	if (fz_overprint_required(eop))
	{
		if (alpha == 255)
			return paint_affine_near_N_op;
		else if (alpha > 0)
			return paint_affine_near_alpha_N_op;
		else
			return NULL;
	}
#endif /* FZ_ENABLE_SPOT_RENDERING */
	switch(n)
	{
	case 0:
		if (da)
		{
			if (sa)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_0_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_0_fb0;
					else
						return paint_affine_near_da_sa_0;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_alpha_0_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_alpha_0_fb0;
					else
						return paint_affine_near_da_sa_alpha_0;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_0_fa0;
					else if (fb == 0)
						return paint_affine_near_da_0_fb0;
					else
						return paint_affine_near_da_0;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_alpha_0_fa0;
					else if (fb == 0)
						return paint_affine_near_da_alpha_0_fb0;
					else
						return paint_affine_near_da_alpha_0;
				}
			}
		}
		break;

	case 1:
		if (sa)
		{
#if FZ_PLOTTERS_G
			if (da)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_1_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_1_fb0;
					else
						return paint_affine_near_da_sa_1;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_alpha_1_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_alpha_1_fb0;
					else
						return paint_affine_near_da_sa_alpha_1;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_sa_1_fa0;
					else if (fb == 0)
						return paint_affine_near_sa_1_fb0;
					else
						return paint_affine_near_sa_1;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_sa_alpha_1_fa0;
					else if (fb == 0)
						return paint_affine_near_sa_alpha_1_fb0;
					else
						return paint_affine_near_sa_alpha_1;
				}
			}
#else
			goto fallback;
#endif /* FZ_PLOTTERS_G */
		}
		else
		{
			if (da)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_1_fa0;
					else if (fb == 0)
						return paint_affine_near_da_1_fb0;
					else
						return paint_affine_near_da_1;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_alpha_1_fa0;
					else if (fb == 0)
						return paint_affine_near_da_alpha_1_fb0;
					else
						return paint_affine_near_da_alpha_1;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_1_fa0;
					else if (fb == 0)
						return paint_affine_near_1_fb0;
					else
						return paint_affine_near_1;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_alpha_1_fa0;
					else if (fb == 0)
						return paint_affine_near_alpha_1_fb0;
					else
						return paint_affine_near_alpha_1;
				}
			}
		}
		break;

#if FZ_PLOTTERS_RGB
	case 3:
		if (da)
		{
			if (sa)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_3_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_3_fb0;
					else
						return paint_affine_near_da_sa_3;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_alpha_3_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_alpha_3_fb0;
					else
						return paint_affine_near_da_sa_alpha_3;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_3_fa0;
					else if (fb == 0)
						return paint_affine_near_da_3_fb0;
					else
						return paint_affine_near_da_3;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_alpha_3_fa0;
					else if (fb == 0)
						return paint_affine_near_da_alpha_3_fb0;
					else
						return paint_affine_near_da_alpha_3;
				}
			}
		}
		else
		{
			if (sa)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_sa_3_fa0;
					else if (fb == 0)
						return paint_affine_near_sa_3_fb0;
					else
						return paint_affine_near_sa_3;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_sa_alpha_3_fa0;
					else if (fb == 0)
						return paint_affine_near_sa_alpha_3_fb0;
					else
						return paint_affine_near_sa_alpha_3;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_3_fa0;
					else if (fb == 0)
						return paint_affine_near_3_fb0;
					else
						return paint_affine_near_3;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_alpha_3_fa0;
					else if (fb == 0)
						return paint_affine_near_alpha_3_fb0;
					else
						return paint_affine_near_alpha_3;
				}
			}
		}
		break;
#endif /* FZ_PLOTTERS_RGB */

#if FZ_PLOTTERS_CMYK
	case 4:
		if (da)
		{
			if (sa)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_4_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_4_fb0;
					else
						return paint_affine_near_da_sa_4;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_alpha_4_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_alpha_4_fb0;
					else
						return paint_affine_near_da_sa_alpha_4;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_4_fa0;
					else if (fb == 0)
						return paint_affine_near_da_4_fb0;
					else
						return paint_affine_near_da_4;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_alpha_4_fa0;
					else if (fb == 0)
						return paint_affine_near_da_alpha_4_fb0;
					else
						return paint_affine_near_da_alpha_4;
				}
			}
		}
		else
		{
			if (sa)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_sa_4_fa0;
					else if (fb == 0)
						return paint_affine_near_sa_4_fb0;
					else
						return paint_affine_near_sa_4;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_sa_alpha_4_fa0;
					else if (fb == 0)
						return paint_affine_near_sa_alpha_4_fb0;
					else
						return paint_affine_near_sa_alpha_4;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_4_fa0;
					else if (fb == 0)
						return paint_affine_near_4_fb0;
					else
						return paint_affine_near_4;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_alpha_4_fa0;
					else if (fb == 0)
						return paint_affine_near_alpha_4_fb0;
					else
						return paint_affine_near_alpha_4;
				}
			}
		}
		break;
#endif /* FZ_PLOTTERS_CMYK */

#if !FZ_PLOTTERS_G
fallback:
#endif /* FZ_PLOTTERS_G */
	default:
#if FZ_PLOTTERS_N
		if (da)
		{
			if (sa)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_N_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_N_fb0;
					else
						return paint_affine_near_da_sa_N;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_sa_alpha_N_fa0;
					else if (fb == 0)
						return paint_affine_near_da_sa_alpha_N_fb0;
					else
						return paint_affine_near_da_sa_alpha_N;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_da_N_fa0;
					else if (fb == 0)
						return paint_affine_near_da_N_fb0;
					else
						return paint_affine_near_da_N;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_da_alpha_N_fa0;
					else if (fb == 0)
						return paint_affine_near_da_alpha_N_fb0;
					else
						return paint_affine_near_da_alpha_N;
				}
			}
		}
		else
		{
			if (sa)
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_sa_N_fa0;
					else if (fb == 0)
						return paint_affine_near_sa_N_fb0;
					else
						return paint_affine_near_sa_N;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_sa_alpha_N_fa0;
					else if (fb == 0)
						return paint_affine_near_sa_alpha_N_fb0;
					else
						return paint_affine_near_sa_alpha_N;
				}
			}
			else
			{
				if (alpha == 255)
				{
					if (fa == 0)
						return paint_affine_near_N_fa0;
					else if (fb == 0)
						return paint_affine_near_N_fb0;
					else
						return paint_affine_near_N;
				}
				else if (alpha > 0)
				{
					if (fa == 0)
						return paint_affine_near_alpha_N_fa0;
					else if (fb == 0)
						return paint_affine_near_alpha_N_fb0;
					else
						return paint_affine_near_alpha_N;
				}
			}
		}
#endif /* FZ_PLOTTERS_N */
		break;
	}
	return NULL;
}

#if FZ_ENABLE_SPOT_RENDERING
static paintfn_t *
fz_paint_affine_near_spots(int da, int sa, int fa, int fb, int dn, int sn, int alpha, const fz_overprint * FZ_RESTRICT eop)
{
	if (fz_overprint_required(eop))
	{
		if (alpha == 255)
			return paint_affine_near_N_op;
		else if (alpha > 0)
			return paint_affine_near_alpha_N_op;
	}
	else if (da)
	{
		if (sa)
		{
			if (alpha == 255)
			{
				if (fa == 0)
					return paint_affine_near_da_sa_N_fa0;
				else if (fb == 0)
					return paint_affine_near_da_sa_N_fb0;
				else
					return paint_affine_near_da_sa_N;
			}
			else if (alpha > 0)
			{
				if (fa == 0)
					return paint_affine_near_da_sa_alpha_N_fa0;
				else if (fb == 0)
					return paint_affine_near_da_sa_alpha_N_fb0;
				else
					return paint_affine_near_da_sa_alpha_N;
			}
		}
		else
		{
			if (alpha == 255)
			{
				if (fa == 0)
					return paint_affine_near_da_N_fa0;
				else if (fb == 0)
					return paint_affine_near_da_N_fb0;
				else
					return paint_affine_near_da_N;
			}
			else if (alpha > 0)
			{
				if (fa == 0)
					return paint_affine_near_da_alpha_N_fa0;
				else if (fb == 0)
					return paint_affine_near_da_alpha_N_fb0;
				else
					return paint_affine_near_da_alpha_N;
			}
		}
	}
	else
	{
		if (sa)
		{
			if (alpha == 255)
			{
				if (fa == 0)
					return paint_affine_near_sa_N_fa0;
				else if (fb == 0)
					return paint_affine_near_sa_N_fb0;
				else
					return paint_affine_near_sa_N;
			}
			else if (alpha > 0)
			{
				if (fa == 0)
					return paint_affine_near_sa_alpha_N_fa0;
				else if (fb == 0)
					return paint_affine_near_sa_alpha_N_fb0;
				else
					return paint_affine_near_sa_alpha_N;
			}
		}
		else
		{
			if (alpha == 255)
			{
				if (fa == 0)
					return paint_affine_near_N_fa0;
				else if (fb == 0)
					return paint_affine_near_N_fb0;
				else
					return paint_affine_near_N;
			}
			else if (alpha > 0)
			{
				if (fa == 0)
					return paint_affine_near_alpha_N_fa0;
				else if (fb == 0)
					return paint_affine_near_alpha_N_fb0;
				else
					return paint_affine_near_alpha_N;
			}
		}
	}
	return NULL;
}
#endif /* FZ_ENABLE_SPOT_RENDERING */

#if FZ_PLOTTERS_RGB
static void
paint_affine_near_da_sa_g2rgb_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_g2rgb_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near_fa0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_da_g2rgb_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_da_alpha_g2rgb_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near_fa0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_sa_g2rgb_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_sa_alpha_g2rgb_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near_fa0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_g2rgb_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_alpha_g2rgb_fa0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near_fa0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_g2rgb_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_g2rgb_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near_fb0(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_da_g2rgb_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_da_alpha_g2rgb_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near_fb0(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_sa_g2rgb_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_sa_alpha_g2rgb_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near_fb0(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_g2rgb_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_alpha_g2rgb_fb0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near_fb0(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_da_sa_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_da_sa_alpha_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near(dp, 1, sp, sw, sh, ss, 1, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_da_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_da_alpha_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near(dp, 1, sp, sw, sh, ss, 0, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_sa_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_sa_alpha_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near(dp, 0, sp, sw, sh, ss, 1, u, v, fa, fb, w, alpha, hp, gp);
}

static void
paint_affine_near_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_solid_g2rgb_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, hp, gp);
}

static void
paint_affine_near_alpha_g2rgb(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn1, int sn1, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_alpha_g2rgb_near(dp, 0, sp, sw, sh, ss, 0, u, v, fa, fb, w, alpha, hp, gp);
}

static paintfn_t *
fz_paint_affine_g2rgb_near(int da, int sa, int fa, int fb, int n, int alpha)
{
	if (da)
	{
		if (sa)
		{
			if (fa == 0)
			{
				if (alpha == 255)
					return paint_affine_near_da_sa_g2rgb_fa0;
				else if (alpha > 0)
					return paint_affine_near_da_sa_alpha_g2rgb_fa0;
			}
			else if (fb == 0)
			{
				if (alpha == 255)
					return paint_affine_near_da_sa_g2rgb_fb0;
				else if (alpha > 0)
					return paint_affine_near_da_sa_alpha_g2rgb_fb0;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_near_da_sa_g2rgb;
				else if (alpha > 0)
					return paint_affine_near_da_sa_alpha_g2rgb;
			}
		}
		else
		{
			if (fa == 0)
			{
				if (alpha == 255)
					return paint_affine_near_da_g2rgb_fa0;
				else if (alpha > 0)
					return paint_affine_near_da_alpha_g2rgb_fa0;
			}
			else if (fb == 0)
			{
				if (alpha == 255)
					return paint_affine_near_da_g2rgb_fb0;
				else if (alpha > 0)
					return paint_affine_near_da_alpha_g2rgb_fb0;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_near_da_g2rgb;
				else if (alpha > 0)
					return paint_affine_near_da_alpha_g2rgb;
			}
		}
	}
	else
	{
		if (sa)
		{
			if (fa == 0)
			{
				if (alpha == 255)
					return paint_affine_near_sa_g2rgb_fa0;
				else if (alpha > 0)
					return paint_affine_near_sa_alpha_g2rgb_fa0;
			}
			else if (fb == 0)
			{
				if (alpha == 255)
					return paint_affine_near_sa_g2rgb_fb0;
				else if (alpha > 0)
					return paint_affine_near_sa_alpha_g2rgb_fb0;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_near_sa_g2rgb;
				else if (alpha > 0)
					return paint_affine_near_sa_alpha_g2rgb;
			}
		}
		else
		{
			if (fa == 0)
			{
				if (alpha == 255)
					return paint_affine_near_g2rgb_fa0;
				else if (alpha > 0)
					return paint_affine_near_alpha_g2rgb_fa0;
			}
			else if (fb == 0)
			{
				if (alpha == 255)
					return paint_affine_near_g2rgb_fb0;
				else if (alpha > 0)
					return paint_affine_near_alpha_g2rgb_fb0;
			}
			else
			{
				if (alpha == 255)
					return paint_affine_near_g2rgb;
				else if (alpha > 0)
					return paint_affine_near_alpha_g2rgb;
			}
		}
	}
	return NULL;
}
#endif /* FZ_PLOTTERS_RGB */

static void
paint_affine_color_lerp_da_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, 0, 0, color, hp, gp);
}

#if FZ_PLOTTERS_G
static void
paint_affine_color_lerp_da_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, 1, 1, color, hp, gp);
}

static void
paint_affine_color_lerp_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, 1, 1, color, hp, gp);
}
#endif /* FZ_PLOTTERS_G */

#if FZ_PLOTTERS_RGB
static void
paint_affine_color_lerp_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, 3, 3, color, hp, gp);
}

static void
paint_affine_color_lerp_da_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, 3, 3, color, hp, gp);
}
#endif /* FZ_PLOTTERS_RGB */

#if FZ_PLOTTERS_CMYK
static void
paint_affine_color_lerp_da_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, 4, 4, color, hp, gp);
}

static void
paint_affine_color_lerp_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, 4, 4, color, hp, gp);
}
#endif /* FZ_PLOTTERS_G */

#if FZ_PLOTTERS_N
static void
paint_affine_color_lerp_da_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, dn, sn, color, hp, gp);
}

static void
paint_affine_color_lerp_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, dn, sn, color, hp, gp);
}
#endif /* FZ_PLOTTERS_N */

#if FZ_ENABLE_SPOT_RENDERING
static void
paint_affine_color_lerp_N_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_lerp_op(dp, da, sp, sw, sh, ss, u, v, fa, fb, w, dn, sn, color, hp, gp, eop);
}
#endif /* FZ_ENABLE_SPOT_RENDERING */

static paintfn_t *
fz_paint_affine_color_lerp(int da, int sa, int fa, int fb, int n, int alpha, const fz_overprint * FZ_RESTRICT eop)
{
#if FZ_ENABLE_SPOT_RENDERING
	if (fz_overprint_required(eop))
		return paint_affine_color_lerp_N_op;
#endif /* FZ_ENABLE_SPOT_RENDERING */
	switch (n)
	{
	case 0: return da ? paint_affine_color_lerp_da_0 : NULL;
#if FZ_PLOTTERS_G
	case 1: return da ? paint_affine_color_lerp_da_1 : paint_affine_color_lerp_1;
#endif /* FZ_PLOTTERS_G */
#if FZ_PLOTTERS_RGB
	case 3: return da ? paint_affine_color_lerp_da_3 : paint_affine_color_lerp_3;
#endif /* FZ_PLOTTERS_RGB */
#if FZ_PLOTTERS_CMYK
	case 4: return da ? paint_affine_color_lerp_da_4 : paint_affine_color_lerp_4;
#endif /* FZ_PLOTTERS_CMYK */
#if FZ_PLOTTERS_N
	default: return da ? paint_affine_color_lerp_da_N : paint_affine_color_lerp_N;
#endif /* FZ_PLOTTERS_N */
	}
	return NULL;
}

#if FZ_ENABLE_SPOT_RENDERING
static paintfn_t *
fz_paint_affine_color_lerp_spots(int da, int sa, int fa, int fb, int dn, int sn, int alpha, const fz_overprint * FZ_RESTRICT eop)
{
	if (fz_overprint_required(eop))
		return paint_affine_color_lerp_N_op;
	return da ? paint_affine_color_lerp_da_N : paint_affine_color_lerp_N;
}
#endif /* FZ_ENABLE_SPOT_RENDERING */

static void
paint_affine_color_near_da_0(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, 0, 0, color, hp, gp);
}

#if FZ_PLOTTERS_G
static void
paint_affine_color_near_da_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, 1, 1, color, hp, gp);
}

static void
paint_affine_color_near_1(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, 1, 1, color, hp, gp);
}
#endif /* FZ_PLOTTERS_G */

#if FZ_PLOTTERS_RGB
static void
paint_affine_color_near_da_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, 3, 3, color, hp, gp);
}

static void
paint_affine_color_near_3(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, 3, 3, color, hp, gp);
}
#endif /* FZ_PLOTTERS_RGB */

#if FZ_PLOTTERS_CMYK
static void
paint_affine_color_near_da_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, 4, 4, color, hp, gp);
}

static void
paint_affine_color_near_4(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, 4, 4, color, hp, gp);
}
#endif /* FZ_PLOTTERS_CMYK */

#if FZ_PLOTTERS_N
static void
paint_affine_color_near_da_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, dn, sn, color, hp, gp);
}

static void
paint_affine_color_near_N(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, dn, sn, color, hp, gp);
}
#endif /* FZ_PLOTTERS_N */

#if FZ_ENABLE_SPOT_RENDERING
static void
paint_affine_color_near_da_N_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near_op(dp, 1, sp, sw, sh, ss, u, v, fa, fb, w, dn, sn, color, hp, gp, eop);
}

static void
paint_affine_color_near_N_op(byte * FZ_RESTRICT dp, int da, const byte * FZ_RESTRICT sp, int sw, int sh, int ss, int sa, int u, int v, int fa, int fb, int w, int dn, int sn, int alpha, const byte * FZ_RESTRICT color, byte * FZ_RESTRICT hp, byte * FZ_RESTRICT gp, const fz_overprint * FZ_RESTRICT eop)
{
	TRACK_FN();
	template_affine_color_N_near_op(dp, 0, sp, sw, sh, ss, u, v, fa, fb, w, dn, sn, color, hp, gp, eop);
}
#endif /* FZ_ENABLE_SPOT_RENDERING */

static paintfn_t *
fz_paint_affine_color_near(int da, int sa, int fa, int fb, int n, int alpha, const fz_overprint * FZ_RESTRICT eop)
{
#if FZ_ENABLE_SPOT_RENDERING
	if (fz_overprint_required(eop))
		return da ? paint_affine_color_near_da_N_op : paint_affine_color_near_N_op;
#endif /* FZ_ENABLE_SPOT_RENDERING */
	switch (n)
	{
	case 0: return da ? paint_affine_color_near_da_0 : NULL;
#if FZ_PLOTTERS_G
	case 1: return da ? paint_affine_color_near_da_1 : paint_affine_color_near_1;
#endif /* FZ_PLOTTERS_G */
#if FZ_PLOTTERS_RGB
	case 3: return da ? paint_affine_color_near_da_3 : paint_affine_color_near_3;
#endif /* FZ_PLOTTERS_RGB */
#if FZ_PLOTTERS_CMYK
	case 4: return da ? paint_affine_color_near_da_4 : paint_affine_color_near_4;
#endif /* FZ_PLOTTERS_CMYK */
#if FZ_PLOTTERS_N
	default: return da ? paint_affine_color_near_da_N : paint_affine_color_near_N;
#else
	default: return NULL;
#endif /* FZ_PLOTTERS_N */
	}
}

#if FZ_ENABLE_SPOT_RENDERING
static paintfn_t *
fz_paint_affine_color_near_spots(int da, int sa, int fa, int fb, int dn, int sn, int alpha, const fz_overprint * FZ_RESTRICT eop)
{
	if (fz_overprint_required(eop))
		return paint_affine_color_near_N_op;
	return da ? paint_affine_color_near_da_N : paint_affine_color_near_N;
}
#endif /* FZ_ENABLE_SPOT_RENDERING */

/* RJW: The following code was originally written to be sensitive to
 * FLT_EPSILON. Given the way the 'minimum representable difference'
 * between 2 floats changes size as we scale, we now pick a larger
 * value to ensure idempotency even with rounding problems. The
 * value we pick is still far smaller than would ever show up with
 * antialiasing.
 */
#define MY_EPSILON 0.001f

/* We have 2 possible ways of gridfitting images. The first way, considered
 * 'safe' in all cases, is to expand an image out to fill a box that entirely
 * covers all the pixels touched by the current image. This is our 'standard'
 * mechanism.
 *
 * The alternative, used when we know images are tiled across a page, is to
 * round the edge of each image to the closest integer pixel boundary. This
 * would not be safe in the general case, but gives less distortion across
 * neighbouring images when tiling is used.
 */
fz_matrix
fz_gridfit_matrix(int as_tiled, fz_matrix m)
{
	if (fabsf(m.b) < FLT_EPSILON && fabsf(m.c) < FLT_EPSILON)
	{
		if (as_tiled)
		{
			float f;
			/* Nearest boundary for left */
			f = (float)(int)(m.e + 0.5f);
			m.a += m.e - f; /* Adjust width for change */
			m.e = f;
			/* Nearest boundary for right (width really) */
			m.a = (float)(int)(m.a + 0.5f);
		}
		else if (m.a > 0)
		{
			float f;
			/* Adjust left hand side onto pixel boundary */
			f = (float)(int)(m.e);
			if (f - m.e > MY_EPSILON)
				f -= 1.0f; /* Ensure it moves left */
			m.a += m.e - f; /* width gets wider as f <= m.e */
			m.e = f;
			/* Adjust right hand side onto pixel boundary */
			f = (float)(int)(m.a);
			if (m.a - f > MY_EPSILON)
				f += 1.0f; /* Ensure it moves right */
			m.a = f;
		}
		else if (m.a < 0)
		{
			float f;
			/* Adjust right hand side onto pixel boundary */
			f = (float)(int)(m.e);
			if (m.e - f > MY_EPSILON)
				f += 1.0f; /* Ensure it moves right */
			m.a += m.e - f; /* width gets wider (more -ve) */
			m.e = f;
			/* Adjust left hand side onto pixel boundary */
			f = (float)(int)(m.a);
			if (f - m.a > MY_EPSILON)
				f -= 1.0f; /* Ensure it moves left */
			m.a = f;
		}
		if (as_tiled)
		{
			float f;
			/* Nearest boundary for top */
			f = (float)(int)(m.f + 0.5f);
			m.d += m.f - f; /* Adjust width for change */
			m.f = f;
			/* Nearest boundary for bottom (height really) */
			m.d = (float)(int)(m.d + 0.5f);
		}
		else if (m.d > 0)
		{
			float f;
			/* Adjust top onto pixel boundary */
			f = (float)(int)(m.f);
			if (f - m.f > MY_EPSILON)
				f -= 1.0f; /* Ensure it moves upwards */
			m.d += m.f - f; /* width gets wider as f <= m.f */
			m.f = f;
			/* Adjust bottom onto pixel boundary */
			f = (float)(int)(m.d);
			if (m.d - f > MY_EPSILON)
				f += 1.0f; /* Ensure it moves down */
			m.d = f;
		}
		else if (m.d < 0)
		{
			float f;
			/* Adjust bottom onto pixel boundary */
			f = (float)(int)(m.f);
			if (m.f - f > MY_EPSILON)
				f += 1.0f; /* Ensure it moves down */
			m.d += m.f - f; /* width gets wider (more -ve) */
			m.f = f;
			/* Adjust top onto pixel boundary */
			f = (float)(int)(m.d);
			if (f - m.d > MY_EPSILON)
				f -= 1.0f; /* Ensure it moves up */
			m.d = f;
		}
	}
	else if (fabsf(m.a) < FLT_EPSILON && fabsf(m.d) < FLT_EPSILON)
	{
		if (as_tiled)
		{
			float f;
			/* Nearest boundary for left */
			f = (float)(int)(m.e + 0.5f);
			m.b += m.e - f; /* Adjust width for change */
			m.e = f;
			/* Nearest boundary for right (width really) */
			m.b = (float)(int)(m.b + 0.5f);
		}
		else if (m.b > 0)
		{
			float f;
			/* Adjust left hand side onto pixel boundary */
			f = (float)(int)(m.f);
			if (f - m.f > MY_EPSILON)
				f -= 1.0f; /* Ensure it moves left */
			m.b += m.f - f; /* width gets wider as f <= m.f */
			m.f = f;
			/* Adjust right hand side onto pixel boundary */
			f = (float)(int)(m.b);
			if (m.b - f > MY_EPSILON)
				f += 1.0f; /* Ensure it moves right */
			m.b = f;
		}
		else if (m.b < 0)
		{
			float f;
			/* Adjust right hand side onto pixel boundary */
			f = (float)(int)(m.f);
			if (m.f - f > MY_EPSILON)
				f += 1.0f; /* Ensure it moves right */
			m.b += m.f - f; /* width gets wider (more -ve) */
			m.f = f;
			/* Adjust left hand side onto pixel boundary */
			f = (float)(int)(m.b);
			if (f - m.b > MY_EPSILON)
				f -= 1.0f; /* Ensure it moves left */
			m.b = f;
		}
		if (as_tiled)
		{
			float f;
			/* Nearest boundary for left */
			f = (float)(int)(m.f + 0.5f);
			m.c += m.f - f; /* Adjust width for change */
			m.f = f;
			/* Nearest boundary for right (width really) */
			m.c = (float)(int)(m.c + 0.5f);
		}
		else if (m.c > 0)
		{
			float f;
			/* Adjust top onto pixel boundary */
			f = (float)(int)(m.e);
			if (f - m.e > MY_EPSILON)
				f -= 1.0f; /* Ensure it moves upwards */
			m.c += m.e - f; /* width gets wider as f <= m.e */
			m.e = f;
			/* Adjust bottom onto pixel boundary */
			f = (float)(int)(m.c);
			if (m.c - f > MY_EPSILON)
				f += 1.0f; /* Ensure it moves down */
			m.c = f;
		}
		else if (m.c < 0)
		{
			float f;
			/* Adjust bottom onto pixel boundary */
			f = (float)(int)(m.e);
			if (m.e - f > MY_EPSILON)
				f += 1.0f; /* Ensure it moves down */
			m.c += m.e - f; /* width gets wider (more -ve) */
			m.e = f;
			/* Adjust top onto pixel boundary */
			f = (float)(int)(m.c);
			if (f - m.c > MY_EPSILON)
				f -= 1.0f; /* Ensure it moves up */
			m.c = f;
		}
	}
	return m;
}

/* Draw an image with an affine transform on destination */

static void
fz_paint_image_imp(fz_context *ctx,
	fz_pixmap *dst,
	const fz_irect *scissor,
	fz_pixmap *shape,
	fz_pixmap *group_alpha,
	fz_pixmap *img,
	fz_matrix ctm,
	const byte *color,
	int alpha,
	int lerp_allowed,
	int as_tiled,
	const fz_overprint *eop)
{
	byte *dp, *sp, *hp, *gp;
	int u, v, fa, fb, fc, fd;
	int x, y, w, h;
	int sw, sh, ss, sa, sn, hs, da, dn, gs;
	fz_irect bbox;
	int dolerp;
	paintfn_t *paintfn;
	int is_rectilinear;

	if (alpha == 0)
		return;

	/* grid fit the image */
	ctm = fz_gridfit_matrix(as_tiled, ctm);

	/* turn on interpolation for upscaled and non-rectilinear transforms */
	dolerp = 0;
	is_rectilinear = fz_is_rectilinear(ctm);
	if (!is_rectilinear)
		dolerp = lerp_allowed;
	if (sqrtf(ctm.a * ctm.a + ctm.b * ctm.b) > img->w)
		dolerp = lerp_allowed;
	if (sqrtf(ctm.c * ctm.c + ctm.d * ctm.d) > img->h)
		dolerp = lerp_allowed;

	/* except when we shouldn't, at large magnifications */
	if (!(img->flags & FZ_PIXMAP_FLAG_INTERPOLATE))
	{
		if (sqrtf(ctm.a * ctm.a + ctm.b * ctm.b) > img->w * 2)
			dolerp = 0;
		if (sqrtf(ctm.c * ctm.c + ctm.d * ctm.d) > img->h * 2)
			dolerp = 0;
	}

	bbox = fz_irect_from_rect(fz_transform_rect(fz_unit_rect, ctm));
	bbox = fz_intersect_irect(bbox, *scissor);

	x = bbox.x0;
	if (shape && shape->x > x)
		x = shape->x;
	if (group_alpha && group_alpha->x > x)
		x = group_alpha->x;
	y = bbox.y0;
	if (shape && shape->y > y)
		y = shape->y;
	if (group_alpha && group_alpha->y > y)
		y = group_alpha->y;
	w = bbox.x1;
	if (shape && shape->x + shape->w < w)
		w = shape->x + shape->w;
	if (group_alpha && group_alpha->x + group_alpha->w < w)
		w = group_alpha->x + group_alpha->w;
	w -= x;
	h = bbox.y1;
	if (shape && shape->y + shape->h < h)
		h = shape->y + shape->h;
	if (group_alpha && group_alpha->y + group_alpha->h < h)
		h = group_alpha->y + group_alpha->h;
	h -= y;
	if (w <= 0 || h <= 0)
		return;

	/* map from screen space (x,y) to image space (u,v) */
	ctm = fz_pre_scale(ctm, 1.0f / img->w, 1.0f / img->h);
	ctm = fz_invert_matrix(ctm);

	fa = (int)(ctm.a *= ONE);
	fb = (int)(ctm.b *= ONE);
	fc = (int)(ctm.c *= ONE);
	fd = (int)(ctm.d *= ONE);
	ctm.e *= ONE;
	ctm.f *= ONE;

	/* Calculate initial texture positions. Do a half step to start. */
	/* Bug 693021: Keep calculation in float for as long as possible to
	 * avoid overflow. */
	u = (int)((ctm.a * x) + (ctm.c * y) + ctm.e + ((ctm.a + ctm.c) * .5f));
	v = (int)((ctm.b * x) + (ctm.d * y) + ctm.f + ((ctm.b + ctm.d) * .5f));

	dp = dst->samples + (unsigned int)((y - dst->y) * dst->stride + (x - dst->x) * dst->n);
	da = dst->alpha;
	dn = dst->n - da;
	sp = img->samples;
	sw = img->w;
	sh = img->h;
	ss = img->stride;
	sa = img->alpha;
	sn = img->n - sa;
	if (shape)
	{
		hs = shape->stride;
		hp = shape->samples + (unsigned int)((y - shape->y) * shape->stride + x - shape->x);
	}
	else
	{
		hs = 0;
		hp = NULL;
	}
	if (group_alpha)
	{
		gs = group_alpha->stride;
		gp = group_alpha->samples + (unsigned int)((y - group_alpha->y) * group_alpha->stride + x - group_alpha->x);
	}
	else
	{
		gs = 0;
		gp = NULL;
	}

	/* image size overflows fixed point math */
	if (sw >= LIMIT || sh >= LIMIT)
	{
		fz_warn(ctx, "image too large for fixed point math: %d x %d", sw, sh);
		return;
	}

	/* TODO: if (fb == 0 && fa == 1) call fz_paint_span */

	/* Sometimes we can get an alpha only input to be
	 * plotted. In this case treat it as a greyscale
	 * input. */
	if (img->n == sa && color)
	{
		sa = 0;
		sn = 1;
	}

#if FZ_PLOTTERS_RGB
	if (dn == 3 && img->n == 1 + sa && !color && !fz_overprint_required(eop))
	{
		if (dolerp)
			paintfn = fz_paint_affine_g2rgb_lerp(da, sa, fa, fb, dn, alpha);
		else
			paintfn = fz_paint_affine_g2rgb_near(da, sa, fa, fb, dn, alpha);
	}
	else
#endif /* FZ_PLOTTERS_RGB */
#if FZ_ENABLE_SPOT_RENDERING
	if (sn != dn)
	{
		if (dolerp)
		{
			if (color)
				paintfn = fz_paint_affine_color_lerp_spots(da, sa, fa, fb, dn, sn, alpha, eop);
			else
				paintfn = fz_paint_affine_lerp_spots(da, sa, fa, fb, dn, sn, alpha, eop);
		}
		else
		{
			if (color)
				paintfn = fz_paint_affine_color_near_spots(da, sa, fa, fb, dn, sn, alpha, eop);
			else
				paintfn = fz_paint_affine_near_spots(da, sa, fa, fb, dn, sn, alpha, eop);
		}
	}
	else
#endif /* FZ_ENABLE_SPOT_RENDERING */
	{
		assert((!color && sn == dn) || (color && sn + sa == 1));
		if (dolerp)
		{
			if (color)
				paintfn = fz_paint_affine_color_lerp(da, sa, fa, fb, dn, alpha, eop);
			else
				paintfn = fz_paint_affine_lerp(da, sa, fa, fb, dn, alpha, eop);
		}
		else
		{
			if (color)
				paintfn = fz_paint_affine_color_near(da, sa, fa, fb, dn, alpha, eop);
			else
				paintfn = fz_paint_affine_near(da, sa, fa, fb, dn, alpha, eop);
		}
	}

	assert(paintfn);
	if (paintfn == NULL)
		return;

	if (dolerp)
	{
		u -= HALF;
		v -= HALF;
		sw = (sw<<PREC) + HALF;
		sh = (sh<<PREC) + HALF;
	}

	while (h--)
	{
		paintfn(dp, da, sp, sw, sh, ss, sa, u, v, fa, fb, w, dn, sn, alpha, color, hp, gp, eop);
		dp += dst->stride;
		hp += hs;
		gp += gs;
		u += fc;
		v += fd;
	}
}

void
fz_paint_image_with_color(fz_context *ctx, fz_pixmap * FZ_RESTRICT dst, const fz_irect * FZ_RESTRICT scissor, fz_pixmap * FZ_RESTRICT shape, fz_pixmap * FZ_RESTRICT group_alpha, fz_pixmap * FZ_RESTRICT img, fz_matrix ctm, const byte * FZ_RESTRICT color, int lerp_allowed, int as_tiled, const fz_overprint * FZ_RESTRICT eop)
{
	assert(img->n == 1);
	fz_paint_image_imp(ctx, dst, scissor, shape, group_alpha, img, ctm, color, 255, lerp_allowed, as_tiled, eop);
}

void
fz_paint_image(fz_context *ctx, fz_pixmap * FZ_RESTRICT dst, const fz_irect * FZ_RESTRICT scissor, fz_pixmap * FZ_RESTRICT shape, fz_pixmap * FZ_RESTRICT group_alpha, fz_pixmap * FZ_RESTRICT img, fz_matrix ctm, int alpha, int lerp_allowed, int as_tiled, const fz_overprint * FZ_RESTRICT eop)
{
	fz_paint_image_imp(ctx, dst, scissor, shape, group_alpha, img, ctm, NULL, alpha, lerp_allowed, as_tiled, eop);
}
