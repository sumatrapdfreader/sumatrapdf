#include "fitz-base.h"
#include "fitz-world.h"
#include "fitz-draw.h"

/* src and msk are identical size, fits within dst */
/* for now, src=3,0 msk=0,1 dst=3,1 */
void fzd_ainboverc(fz_pixmap *src, fz_pixmap *msk, fz_pixmap *dst)
{
	unsigned char *sp0, *dp0, *mp0;
	int x, y, h, w0;
	int sn = src->n + src->a;
	int mn = msk->n + msk->a;
	int dn = dst->n + dst->a;

	assert(sn==3 || sn==4);
	assert(mn==1);
	assert(dn==3 || dn==4);

	x = src->x;
	y = src->y;
	w0 = src->w;
	h = src->h;

	sp0 = src->p + (y - src->y) * src->s + (x - src->x) * sn;
	mp0 = msk->p + (y - msk->y) * msk->s + (x - msk->x) * mn;
	dp0 = dst->p + (y - dst->y) * dst->s + (x - dst->x) * dn;

	while (h--)
	{
		unsigned char *sp = sp0;
		unsigned char *mp = mp0;
		unsigned char *dp = dp0;
		int w = w0;
		while (w--)
		{
			unsigned char ma, sa, ssa;
			ma = mp[0];
			if (sn == 4)
				sa = fz_mul255(sp[3], ma);
			else
				sa = ma;
			ssa = 255 - sa;
			dp[0] = fz_mul255(sp[0], ma) + fz_mul255(dp[0], ssa);
			dp[1] = fz_mul255(sp[1], ma) + fz_mul255(dp[1], ssa);
			dp[2] = fz_mul255(sp[2], ma) + fz_mul255(dp[2], ssa);
			if (sn == 4)
				dp[3] = fz_mul255(sp[3], ma) + fz_mul255(dp[3], ssa);
			else
				dp[3] = 0xff;
#if 0
			/* TODO: validate this */
			int k;
			unsigned char ma = mp[0];
			unsigned char sa = ma; // fz_mul255(sp[0], ma);
			unsigned char ssa = 255 - sa;
			for (k = 0; k < sn; k++)
			{
				dp[k] = fz_mul255(sp[k], ma) + fz_mul255(dp[k], ssa);
			}
			dp[0] = 0xff;
#endif
			sp += sn;
			mp += mn;
			dp += dn;
		}
		sp0 += src->s;
		mp0 += msk->s;
		dp0 += dst->s;
	}
}

void fzd_solidover(unsigned char *solid, fz_pixmap *dest)
{
	int n = dest->n + dest->a;
	unsigned char *p;
	int x, y, i;

	for (y = 0; y < dest->h; y++)
	{
		p = dest->p + y * dest->s;
		for (x = 0; x < dest->w; x++)
		{
			for (i = 0; i < n; i++)
			{
				*p++ = solid[i];
			}
		}
	}
}

void fzd_maskover(fz_scanargs *args, unsigned char *mp0,
		int x, int y, int w0, int h, int ms)
{
	fz_pixmap *dest = args->dest;
	unsigned char *mp, *dp, *dp0;
	int ds = dest->s;
	int w;

	dp0 = dest->p + (y - dest->y) * dest->s + (x - dest->x) * (dest->n + dest->a);
		
	while (h--)
	{
		w = w0;
		dp = dp0;
		mp = mp0;
		while (w--)
		{
			*dp = *mp + *dp;
			mp++;
			dp++;
		}
		dp0 += ds;
		mp0 += ms;
	}
}

void fzd_maskoverwithsolid() {}
void fzd_imageover() {}
void fzd_imageoverwithsolid() {}
void fzd_imageadd() {}
void fzd_shadeover() {}

#if 0

/*
 * Primitives.
 * These are the only functions -needed- to render.
 * Default implementations (fzrd_*)
 * Called via function pointers (fzr_*)
 * Can be replaced by versions in arch*.c
 */

void
fzrd_pixmap_over(fz_pixmap *src, fz_pixmap *dst)
{
	fz_irect sr, dr, cr;
	int i, x, y, n, w, h, w0;
	unsigned char *sp0, *dp0;
	unsigned char *sp, *dp;

	unsigned char fs, qs, as;
	unsigned char fb, qb, ab;
	unsigned char fr, qr, ar;
	unsigned char Cr, Cb, Cs;
	unsigned char t0, t1, t2;

printf("fzrd_pixmap_over (%d,%d) (%d,%d)\n", src->n,src->a, dst->n,dst->a);

	assert(src->n == dst->n);

	sr.x0 = src->x;
	sr.y0 = src->y;
	sr.x1 = src->x + src->w;
	sr.y1 = src->y + src->h;

	dr.x0 = dst->x;
	dr.y0 = dst->y;
	dr.x1 = dst->x + dst->w;
	dr.y1 = dst->y + dst->h;

	cr = fz_intersectirects(sr, dr);
	x = cr.x0;
	y = cr.y0;
	w0 = cr.x1 - cr.x0;
	h = cr.y1 - cr.y0;
	n = dst->n;

assert(w0 == src->w);
assert(h == src->h);

printf("  s[%d %d]\n", src->w,src->h);
printf("  d[%d %d]\n", dst->w,dst->h);
printf("  =[%d %d]\n", w0,h);

	sp0 = src->p + ((y - src->y) * src->w + (x - src->x)) * (src->n + src->a);
	dp0 = dst->p + ((y - dst->y) * dst->w + (x - dst->x)) * (dst->n + dst->a);

	while (h--)
	{
		sp = sp0;
		dp = dp0;
		w = w0;

		while (w--)
		{
			qs = (src->a > 1) ? sp[src->n + 1] : 0xFF;
			fs = (src->a > 0) ? sp[src->n] : 0xFF;
			as = fz_mul255(fs, qs);

			qb = (dst->a > 1) ? dp[dst->n + 1] : 0xFF;
			fb = (dst->a > 0) ? dp[dst->n] : 0xFF;
			ab = fz_mul255(fb, qb);

			fr = fz_union255(fb, fs);
			ar = fz_union255(ab, as);
			qr = fz_div255(ar, fr);

// printf("  qs=%d fs=%d as=%d ab=%d ar=%d\n", qs,fs,as,ab,ar);

			for (i = 0; i < n; i++)
			{
				Cs = sp[i];
				Cb = dp[i];

				// Cr = (1-as/ar) * Cb + as/ar * ((1-ab)*Cs + ab*B(Cb,Cs));

				t0 = 255 * as / ar;
				t1 = fz_mul255(255 - t0, Cb);
				t2 = fz_mul255(t0, Cs);
				Cr = t1 + t2;

				dp[i] = Cr;
			}

			if (dst->a == 2) { dp[n] = fr; dp[n+1] = qr; }
			if (dst->a == 1) { dp[n] = ar; }

			sp += src->n + src->a;
			dp += dst->n + dst->a;
		}

		sp0 += src->w * (src->n + src->a);
		dp0 += dst->w * (dst->n + dst->a);
	}
}

void
fzrd_solid_over(unsigned char *solid,
	fz_pixmap *dst)
{
	unsigned char *dp = dst->p;
	int w, h, i;
	printf("fzrd_solid_over;\n");

	h = dst->h;
	while (h--)
	{
		w = dst->w;
		while (w--)
		{
			for (i = 0; i < dst->n + dst->a; i++)
				*dp++ = solid[i];
		}
	}
}

void
fzrd_path_over(unsigned char *path, int n, int x, int y,
	fz_pixmap *dst)
{
	unsigned char *dp;

	x -= dst->x;
	y -= dst->y;

//	printf("fzrd_path_over n=%d x=%d y=%d;\n", n, x, y);
	assert(dst->n == 0);
	assert(dst->a == 1);

	dp = dst->p + (y * dst->w + x) * (dst->n + dst->a);
	while (n--)
		*dp++ = *path++;
}

void
fzrd_text_over(fz_glyph *g, int x, int y,
	fz_pixmap *dst)
{
	unsigned char *dp;
	int w, h;

	x -= dst->x;
	y -= dst->y;

	h = g->h;
	while (h--)
	{
		w = g->w;
		while (w--)
		{
			dst->p[(y + g->y + h) * dst->w + x + g->x + w] =
				g->p[h * g->w + w];
		}
	}
}

void
fzrd_image_over(fz_pixmap *img,
	int u0, int v0, int fa, int fb, int fc, int fd,
	fz_pixmap *dst)
{
}

void
fzrd_shade_over(fz_shade *shade, fz_matrix ctm, fz_colorspace *pcm,
	fz_pixmap *dst)
{
}

/*
 * Initialize the function pointers.
 */

void (*fzr_pixmap_over)() = fzrd_pixmap_over;
void (*fzr_solid_over)() = fzrd_solid_over;
void (*fzr_path_over)() = fzrd_path_over;
void (*fzr_text_over)() = fzrd_text_over;
void (*fzr_image_over)() = fzrd_image_over;
void (*fzr_shade_over)() = fzrd_shade_over;

#endif

