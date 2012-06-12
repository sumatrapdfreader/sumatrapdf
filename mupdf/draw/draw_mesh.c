#include "fitz-internal.h"

#ifdef CLUSTER
#define LOCAL_TRIG_FNS
#endif

#ifdef LOCAL_TRIG_FNS
/*
 * Trig functions
 */
static float
my_atan_table[258] =
{
0.0000000000f, 0.00390623013f,0.00781234106f,0.0117182136f,
0.0156237286f, 0.0195287670f, 0.0234332099f, 0.0273369383f,
0.0312398334f, 0.0351417768f, 0.0390426500f, 0.0429423347f,
0.0468407129f, 0.0507376669f, 0.0546330792f, 0.0585268326f,
0.0624188100f, 0.0663088949f, 0.0701969711f, 0.0740829225f,
0.0779666338f, 0.0818479898f, 0.0857268758f, 0.0896031775f,
0.0934767812f, 0.0973475735f, 0.1012154420f, 0.1050802730f,
0.1089419570f, 0.1128003810f, 0.1166554350f, 0.1205070100f,
0.1243549950f, 0.1281992810f, 0.1320397620f, 0.1358763280f,
0.1397088740f, 0.1435372940f, 0.1473614810f, 0.1511813320f,
0.1549967420f, 0.1588076080f, 0.1626138290f, 0.1664153010f,
0.1702119250f, 0.1740036010f, 0.1777902290f, 0.1815717110f,
0.1853479500f, 0.1891188490f, 0.1928843120f, 0.1966442450f,
0.2003985540f, 0.2041471450f, 0.2078899270f, 0.2116268090f,
0.2153577000f, 0.2190825110f, 0.2228011540f, 0.2265135410f,
0.2302195870f, 0.2339192060f, 0.2376123140f, 0.2412988270f,
0.2449786630f, 0.2486517410f, 0.2523179810f, 0.2559773030f,
0.2596296290f, 0.2632748830f, 0.2669129880f, 0.2705438680f,
0.2741674510f, 0.2777836630f, 0.2813924330f, 0.2849936890f,
0.2885873620f, 0.2921733830f, 0.2957516860f, 0.2993222020f,
0.3028848680f, 0.3064396190f, 0.3099863910f, 0.3135251230f,
0.3170557530f, 0.3205782220f, 0.3240924700f, 0.3275984410f,
0.3310960770f, 0.3345853220f, 0.3380661230f, 0.3415384250f,
0.3450021770f, 0.3484573270f, 0.3519038250f, 0.3553416220f,
0.3587706700f, 0.3621909220f, 0.3656023320f, 0.3690048540f,
0.3723984470f, 0.3757830650f, 0.3791586690f, 0.3825252170f,
0.3858826690f, 0.3892309880f, 0.3925701350f, 0.3959000740f,
0.3992207700f, 0.4025321870f, 0.4058342930f, 0.4091270550f,
0.4124104420f, 0.4156844220f, 0.4189489670f, 0.4222040480f,
0.4254496370f, 0.4286857080f, 0.4319122350f, 0.4351291940f,
0.4383365600f, 0.4415343100f, 0.4447224240f, 0.4479008790f,
0.4510696560f, 0.4542287350f, 0.4573780990f, 0.4605177290f,
0.4636476090f, 0.4667677240f, 0.4698780580f, 0.4729785980f,
0.4760693300f, 0.4791502430f, 0.4822213240f, 0.4852825630f,
0.4883339510f, 0.4913754780f, 0.4944071350f, 0.4974289160f,
0.5004408130f, 0.5034428210f, 0.5064349340f, 0.5094171490f,
0.5123894600f, 0.5153518660f, 0.5183043630f, 0.5212469510f,
0.5241796290f, 0.5271023950f, 0.5300152510f, 0.5329181980f,
0.5358112380f, 0.5386943730f, 0.5415676050f, 0.5444309400f,
0.5472843810f, 0.5501279330f, 0.5529616020f, 0.5557853940f,
0.5585993150f, 0.5614033740f, 0.5641975770f, 0.5669819340f,
0.5697564530f, 0.5725211450f, 0.5752760180f, 0.5780210840f,
0.5807563530f, 0.5834818390f, 0.5861975510f, 0.5889035040f,
0.5915997100f, 0.5942861830f, 0.5969629370f, 0.5996299860f,
0.6022873460f, 0.6049350310f, 0.6075730580f, 0.6102014430f,
0.6128202020f, 0.6154293530f, 0.6180289120f, 0.6206188990f,
0.6231993300f, 0.6257702250f, 0.6283316020f, 0.6308834820f,
0.6334258830f, 0.6359588250f, 0.6384823300f, 0.6409964180f,
0.6435011090f, 0.6459964250f, 0.6484823880f, 0.6509590190f,
0.6534263410f, 0.6558843770f, 0.6583331480f, 0.6607726790f,
0.6632029930f, 0.6656241120f, 0.6680360620f, 0.6704388650f,
0.6728325470f, 0.6752171330f, 0.6775926450f, 0.6799591110f,
0.6823165550f, 0.6846650020f, 0.6870044780f, 0.6893350100f,
0.6916566220f, 0.6939693410f, 0.6962731940f, 0.6985682070f,
0.7008544080f, 0.7031318220f, 0.7054004770f, 0.7076604000f,
0.7099116190f, 0.7121541600f, 0.7143880520f, 0.7166133230f,
0.7188300000f, 0.7210381110f, 0.7232376840f, 0.7254287490f,
0.7276113330f, 0.7297854640f, 0.7319511710f, 0.7341084830f,
0.7362574290f, 0.7383980370f, 0.7405303370f, 0.7426543560f,
0.7447701260f, 0.7468776740f, 0.7489770290f, 0.7510682220f,
0.7531512810f, 0.7552262360f, 0.7572931160f, 0.7593519510f,
0.7614027700f, 0.7634456020f, 0.7654804790f, 0.7675074280f,
0.7695264800f, 0.7715376650f, 0.7735410110f, 0.7755365500f,
0.7775243100f, 0.7795043220f, 0.7814766150f, 0.7834412190f,
0.7853981630f, 0.7853981630f /* Extended by 1 for interpolation */
};

float my_sinf(float x)
{
	float x2, xn;
	int i;
	/* Map x into the -PI to PI range. We could do this using:
	 * x = fmodf(x, (float)(2.0 * M_PI));
	 * but that's C99, and seems to misbehave with negative numbers
	 * on some platforms. */
	x -= (float)M_PI;
	i = x / (float)(2.0f * M_PI);
	x -= i * (float)(2.0f * M_PI);
	if (x < 0.0f)
		x += (float)(2.0f * M_PI);
	x -= (float)M_PI;
	if (x <= (float)(-M_PI/2.0))
		x = -(float)M_PI-x;
	else if (x >= (float)(M_PI/2.0))
		x = (float)M_PI-x;
	x2 = x*x;
	xn = x*x2/6.0f;
	x -= xn;
	xn *= x2/20.0f;
	x += xn;
	xn *= x2/42.0f;
	x -= xn;
	xn *= x2/72.0f;
	x += xn;
	return x;
}

float my_atan2f(float o, float a)
{
	int negate = 0, flip = 0, i;
	float r, s;
	if (o == 0.0f)
	{
		if (a > 0)
			return 0.0f;
		else
			return (float)M_PI;
	}
	if (o < 0)
		o = -o, negate = 1;
	if (a < 0)
		a = -a, flip = 1;
	if (o < a)
		i = (int)(65536.0f*o/a + 0.5f);
	else
		i = (int)(65536.0f*a/o + 0.5f);
	r = my_atan_table[i>>8];
	s = my_atan_table[(i>>8)+1];
	r += (s-r)*(i&255)/256.0f;
	if (o >= a)
		r = (float)(M_PI/2.0f) - r;
	if (flip)
		r = (float)M_PI - r;
	if (negate)
		r = -r;
	return r;
}

#define my_cosf(x) my_sinf(((float)(M_PI/2.0f)) + (x))
#else
#define my_sinf(x) sinf(x)
#define my_cosf(x) cosf(x)
#define my_atan2f(x,y) atan2f(x,y)
#endif

/*
 * polygon clipping
 */

enum { IN, OUT, ENTER, LEAVE };
enum { MAXV = 3 + 4 };
enum { MAXN = 2 + FZ_MAX_COLORS };

static int clipx(float val, int ismax, float *v1, float *v2, int n)
{
	float t;
	int i;
	int v1o = ismax ? v1[0] > val : v1[0] < val;
	int v2o = ismax ? v2[0] > val : v2[0] < val;
	if (v1o + v2o == 0)
		return IN;
	if (v1o + v2o == 2)
		return OUT;
	if (v2o)
	{
		t = (val - v1[0]) / (v2[0] - v1[0]);
		v2[0] = val;
		v2[1] = v1[1] + t * (v2[1] - v1[1]);
		for (i = 2; i < n; i++)
			v2[i] = v1[i] + t * (v2[i] - v1[i]);
		return LEAVE;
	}
	else
	{
		t = (val - v2[0]) / (v1[0] - v2[0]);
		v1[0] = val;
		v1[1] = v2[1] + t * (v1[1] - v2[1]);
		for (i = 2; i < n; i++)
			v1[i] = v2[i] + t * (v1[i] - v2[i]);
		return ENTER;
	}
}

static int clipy(float val, int ismax, float *v1, float *v2, int n)
{
	float t;
	int i;
	int v1o = ismax ? v1[1] > val : v1[1] < val;
	int v2o = ismax ? v2[1] > val : v2[1] < val;
	if (v1o + v2o == 0)
		return IN;
	if (v1o + v2o == 2)
		return OUT;
	if (v2o)
	{
		t = (val - v1[1]) / (v2[1] - v1[1]);
		v2[0] = v1[0] + t * (v2[0] - v1[0]);
		v2[1] = val;
		for (i = 2; i < n; i++)
			v2[i] = v1[i] + t * (v2[i] - v1[i]);
		return LEAVE;
	}
	else
	{
		t = (val - v2[1]) / (v1[1] - v2[1]);
		v1[0] = v2[0] + t * (v1[0] - v2[0]);
		v1[1] = val;
		for (i = 2; i < n; i++)
			v1[i] = v2[i] + t * (v1[i] - v2[i]);
		return ENTER;
	}
}

static inline void copy_vert(float *dst, float *src, int n)
{
	while (n--)
		*dst++ = *src++;
}

static int clip_poly(float src[MAXV][MAXN],
	float dst[MAXV][MAXN], int len, int n,
	float val, int isy, int ismax)
{
	float cv1[MAXN];
	float cv2[MAXN];
	int v1, v2, cp;
	int r;

	v1 = len - 1;
	cp = 0;

	for (v2 = 0; v2 < len; v2++)
	{
		copy_vert(cv1, src[v1], n);
		copy_vert(cv2, src[v2], n);

		if (isy)
			r = clipy(val, ismax, cv1, cv2, n);
		else
			r = clipx(val, ismax, cv1, cv2, n);

		switch (r)
		{
		case IN:
			copy_vert(dst[cp++], cv2, n);
			break;
		case OUT:
			break;
		case LEAVE:
			copy_vert(dst[cp++], cv2, n);
			break;
		case ENTER:
			copy_vert(dst[cp++], cv1, n);
			copy_vert(dst[cp++], cv2, n);
			break;
		}
		v1 = v2;
	}

	return cp;
}

/*
 * gouraud shaded polygon scan conversion
 */

static void paint_scan(fz_pixmap *pix, int y, int x1, int x2, int *v1, int *v2, int n)
{
	unsigned char *p = pix->samples + (unsigned int)(((y - pix->y) * pix->w + (x1 - pix->x)) * pix->n);
	int v[FZ_MAX_COLORS];
	int dv[FZ_MAX_COLORS];
	int w = x2 - x1;
	int k;

	assert(w >= 0);
	assert(y >= pix->y);
	assert(y < pix->y + pix->h);
	assert(x1 >= pix->x);
	assert(x2 <= pix->x + pix->w);

	if (w == 0)
		return;

	for (k = 0; k < n; k++)
	{
		v[k] = v1[k];
		dv[k] = (v2[k] - v1[k]) / w;
	}

	while (w--)
	{
		for (k = 0; k < n; k++)
		{
			*p++ = v[k] >> 16;
			v[k] += dv[k];
		}
		*p++ = 255;
	}
}

static int find_next(int gel[MAXV][MAXN], int len, int a, int *s, int *e, int d)
{
	int b;

	while (1)
	{
		b = a + d;
		if (b == len)
			b = 0;
		if (b == -1)
			b = len - 1;

		if (gel[b][1] == gel[a][1])
		{
			a = b;
			continue;
		}

		if (gel[b][1] > gel[a][1])
		{
			*s = a;
			*e = b;
			return 0;
		}

		return 1;
	}
}

static void load_edge(int gel[MAXV][MAXN], int s, int e, int *ael, int *del, int n)
{
	int swp, k, dy;

	if (gel[s][1] > gel[e][1])
	{
		swp = s; s = e; e = swp;
	}

	dy = gel[e][1] - gel[s][1];

	ael[0] = gel[s][0];
	del[0] = (gel[e][0] - gel[s][0]) / dy;
	for (k = 2; k < n; k++)
	{
		ael[k] = gel[s][k];
		del[k] = (gel[e][k] - gel[s][k]) / dy;
	}
}

static inline void step_edge(int *ael, int *del, int n)
{
	int k;
	ael[0] += del[0];
	for (k = 2; k < n; k++)
		ael[k] += del[k];
}

static void
fz_paint_triangle(fz_pixmap *pix, float *av, float *bv, float *cv, int n, fz_bbox bbox)
{
	float poly[MAXV][MAXN];
	float temp[MAXV][MAXN];
	float cx0 = bbox.x0;
	float cy0 = bbox.y0;
	float cx1 = bbox.x1;
	float cy1 = bbox.y1;

	int gel[MAXV][MAXN];
	int ael[2][MAXN];
	int del[2][MAXN];
	int y, s0, s1, e0, e1;
	int top, bot, len;

	int i, k;

	copy_vert(poly[0], av, n);
	copy_vert(poly[1], bv, n);
	copy_vert(poly[2], cv, n);

	len = clip_poly(poly, temp, 3, n, cx0, 0, 0);
	len = clip_poly(temp, poly, len, n, cx1, 0, 1);
	len = clip_poly(poly, temp, len, n, cy0, 1, 0);
	len = clip_poly(temp, poly, len, n, cy1, 1, 1);

	if (len < 3)
		return;

	for (i = 0; i < len; i++)
	{
		gel[i][0] = floorf(poly[i][0] + 0.5f) * 65536; /* trunc and fix */
		gel[i][1] = floorf(poly[i][1] + 0.5f);	/* y is not fixpoint */
		for (k = 2; k < n; k++)
			gel[i][k] = poly[i][k] * 65536;	/* fix with precision */
	}

	top = bot = 0;
	for (i = 0; i < len; i++)
	{
		if (gel[i][1] < gel[top][1])
			top = i;
		if (gel[i][1] > gel[bot][1])
			bot = i;
	}

	if (gel[bot][1] - gel[top][1] == 0)
		return;

	y = gel[top][1];

	if (find_next(gel, len, top, &s0, &e0, 1))
		return;
	if (find_next(gel, len, top, &s1, &e1, -1))
		return;

	load_edge(gel, s0, e0, ael[0], del[0], n);
	load_edge(gel, s1, e1, ael[1], del[1], n);

	while (1)
	{
		int x0 = ael[0][0] >> 16;
		int x1 = ael[1][0] >> 16;

		if (ael[0][0] < ael[1][0])
			paint_scan(pix, y, x0, x1, ael[0]+2, ael[1]+2, n-2);
		else
			paint_scan(pix, y, x1, x0, ael[1]+2, ael[0]+2, n-2);

		step_edge(ael[0], del[0], n);
		step_edge(ael[1], del[1], n);
		y ++;

		if (y >= gel[e0][1])
		{
			if (find_next(gel, len, e0, &s0, &e0, 1))
				return;
			load_edge(gel, s0, e0, ael[0], del[0], n);
		}

		if (y >= gel[e1][1])
		{
			if (find_next(gel, len, e1, &s1, &e1, -1))
				return;
			load_edge(gel, s1, e1, ael[1], del[1], n);
		}
	}
}

static void
fz_paint_quad(fz_pixmap *pix,
		fz_point p0, fz_point p1, fz_point p2, fz_point p3,
		float c0, float c1, float c2, float c3,
		int n, fz_bbox bbox)
{
	float v[4][3];

	v[0][0] = p0.x;
	v[0][1] = p0.y;
	v[0][2] = c0;

	v[1][0] = p1.x;
	v[1][1] = p1.y;
	v[1][2] = c1;

	v[2][0] = p2.x;
	v[2][1] = p2.y;
	v[2][2] = c2;

	v[3][0] = p3.x;
	v[3][1] = p3.y;
	v[3][2] = c3;

	fz_paint_triangle(pix, v[0], v[2], v[3], n, bbox);
	fz_paint_triangle(pix, v[0], v[3], v[1], n, bbox);
}

/*
 * linear, radial and mesh painting
 */

#define HUGENUM 32000 /* how far to extend axial/radial shadings */
#define RADSEGS 32 /* how many segments to generate for radial meshes */

static fz_point
fz_point_on_circle(fz_point p, float r, float theta)
{
	p.x = p.x + my_cosf(theta) * r;
	p.y = p.y + my_sinf(theta) * r;

	return p;
}

static void
fz_paint_linear(fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox)
{
	fz_point p0, p1;
	fz_point v0, v1, v2, v3;
	fz_point e0, e1;
	float theta;

	p0.x = shade->mesh[0];
	p0.y = shade->mesh[1];
	p0 = fz_transform_point(ctm, p0);

	p1.x = shade->mesh[3];
	p1.y = shade->mesh[4];
	p1 = fz_transform_point(ctm, p1);

	theta = my_atan2f(p1.y - p0.y, p1.x - p0.x);
	theta += (float)M_PI * 0.5f;

	v0 = fz_point_on_circle(p0, HUGENUM, theta);
	v1 = fz_point_on_circle(p1, HUGENUM, theta);
	v2 = fz_point_on_circle(p0, -HUGENUM, theta);
	v3 = fz_point_on_circle(p1, -HUGENUM, theta);

	fz_paint_quad(dest, v0, v1, v2, v3, 0, 255, 0, 255, 3, bbox);

	if (shade->extend[0])
	{
		e0.x = v0.x - (p1.x - p0.x) * HUGENUM;
		e0.y = v0.y - (p1.y - p0.y) * HUGENUM;

		e1.x = v2.x - (p1.x - p0.x) * HUGENUM;
		e1.y = v2.y - (p1.y - p0.y) * HUGENUM;

		fz_paint_quad(dest, e0, e1, v0, v2, 0, 0, 0, 0, 3, bbox);
	}

	if (shade->extend[1])
	{
		e0.x = v1.x + (p1.x - p0.x) * HUGENUM;
		e0.y = v1.y + (p1.y - p0.y) * HUGENUM;

		e1.x = v3.x + (p1.x - p0.x) * HUGENUM;
		e1.y = v3.y + (p1.y - p0.y) * HUGENUM;

		fz_paint_quad(dest, e0, e1, v1, v3, 255, 255, 255, 255, 3, bbox);
	}
}

static void
fz_paint_annulus(fz_matrix ctm,
		fz_point p0, float r0, float c0,
		fz_point p1, float r1, float c1,
		fz_pixmap *dest, fz_bbox bbox)
{
	fz_point t0, t1, t2, t3, b0, b1, b2, b3;
	float theta, step;
	int i;

	theta = my_atan2f(p1.y - p0.y, p1.x - p0.x);
	step = (float)M_PI * 2 / RADSEGS;

	for (i = 0; i < RADSEGS / 2; i++)
	{
		t0 = fz_point_on_circle(p0, r0, theta + i * step);
		t1 = fz_point_on_circle(p0, r0, theta + i * step + step);
		t2 = fz_point_on_circle(p1, r1, theta + i * step);
		t3 = fz_point_on_circle(p1, r1, theta + i * step + step);
		b0 = fz_point_on_circle(p0, r0, theta - i * step);
		b1 = fz_point_on_circle(p0, r0, theta - i * step - step);
		b2 = fz_point_on_circle(p1, r1, theta - i * step);
		b3 = fz_point_on_circle(p1, r1, theta - i * step - step);

		t0 = fz_transform_point(ctm, t0);
		t1 = fz_transform_point(ctm, t1);
		t2 = fz_transform_point(ctm, t2);
		t3 = fz_transform_point(ctm, t3);
		b0 = fz_transform_point(ctm, b0);
		b1 = fz_transform_point(ctm, b1);
		b2 = fz_transform_point(ctm, b2);
		b3 = fz_transform_point(ctm, b3);

		fz_paint_quad(dest, t0, t1, t2, t3, c0, c0, c1, c1, 3, bbox);
		fz_paint_quad(dest, b0, b1, b2, b3, c0, c0, c1, c1, 3, bbox);
	}
}

static void
fz_paint_radial(fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox)
{
	fz_point p0, p1;
	float r0, r1;
	fz_point e;
	float er, rs;

	p0.x = shade->mesh[0];
	p0.y = shade->mesh[1];
	r0 = shade->mesh[2];

	p1.x = shade->mesh[3];
	p1.y = shade->mesh[4];
	r1 = shade->mesh[5];

	if (shade->extend[0])
	{
		if (r0 < r1)
			rs = r0 / (r0 - r1);
		else
			rs = -HUGENUM;

		e.x = p0.x + (p1.x - p0.x) * rs;
		e.y = p0.y + (p1.y - p0.y) * rs;
		er = r0 + (r1 - r0) * rs;

		fz_paint_annulus(ctm, e, er, 0, p0, r0, 0, dest, bbox);
	}

	fz_paint_annulus(ctm, p0, r0, 0, p1, r1, 255, dest, bbox);

	if (shade->extend[1])
	{
		if (r0 > r1)
			rs = r1 / (r1 - r0);
		else
			rs = -HUGENUM;

		e.x = p1.x + (p0.x - p1.x) * rs;
		e.y = p1.y + (p0.y - p1.y) * rs;
		er = r1 + (r0 - r1) * rs;

		fz_paint_annulus(ctm, p1, r1, 255, e, er, 255, dest, bbox);
	}
}

static void
fz_paint_mesh(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox)
{
	float tri[3][MAXN];
	fz_point p;
	float *mesh;
	int ntris;
	int i, k;

	mesh = shade->mesh;

	if (shade->use_function)
		ntris = shade->mesh_len / 9;
	else
		ntris = shade->mesh_len / ((2 + shade->colorspace->n) * 3);

	while (ntris--)
	{
		for (k = 0; k < 3; k++)
		{
			p.x = *mesh++;
			p.y = *mesh++;
			p = fz_transform_point(ctm, p);
			tri[k][0] = p.x;
			tri[k][1] = p.y;
			if (shade->use_function)
				tri[k][2] = *mesh++ * 255;
			else
			{
				fz_convert_color(ctx, dest->colorspace, tri[k] + 2, shade->colorspace, mesh);
				for (i = 0; i < dest->colorspace->n; i++)
					tri[k][i + 2] *= 255;
				mesh += shade->colorspace->n;
			}
		}
		fz_paint_triangle(dest, tri[0], tri[1], tri[2], 2 + dest->colorspace->n, bbox);
	}
}

void
fz_paint_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox)
{
	unsigned char clut[256][FZ_MAX_COLORS];
	fz_pixmap *temp = NULL;
	fz_pixmap *conv = NULL;
	float color[FZ_MAX_COLORS];
	int i, k;

	fz_var(temp);
	fz_var(conv);

	fz_try(ctx)
	{
		ctm = fz_concat(shade->matrix, ctm);

		if (shade->use_function)
		{
			for (i = 0; i < 256; i++)
			{
				fz_convert_color(ctx, dest->colorspace, color, shade->colorspace, shade->function[i]);
				for (k = 0; k < dest->colorspace->n; k++)
					clut[i][k] = color[k] * 255;
				clut[i][k] = shade->function[i][shade->colorspace->n] * 255;
			}
			conv = fz_new_pixmap_with_bbox(ctx, dest->colorspace, bbox);
			temp = fz_new_pixmap_with_bbox(ctx, fz_device_gray, bbox);
			fz_clear_pixmap(ctx, temp);
		}
		else
		{
			temp = dest;
		}

		switch (shade->type)
		{
		case FZ_LINEAR: fz_paint_linear(shade, ctm, temp, bbox); break;
		case FZ_RADIAL: fz_paint_radial(shade, ctm, temp, bbox); break;
		case FZ_MESH: fz_paint_mesh(ctx, shade, ctm, temp, bbox); break;
		}

		if (shade->use_function)
		{
			unsigned char *s = temp->samples;
			unsigned char *d = conv->samples;
			int len = temp->w * temp->h;
			while (len--)
			{
				int v = *s++;
				int a = fz_mul255(*s++, clut[v][conv->n - 1]);
				for (k = 0; k < conv->n - 1; k++)
					*d++ = fz_mul255(clut[v][k], a);
				*d++ = a;
			}
			fz_paint_pixmap(dest, conv, 255);
			fz_drop_pixmap(ctx, conv);
			fz_drop_pixmap(ctx, temp);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, conv);
		fz_drop_pixmap(ctx, temp);
		fz_rethrow(ctx);
	}
}
