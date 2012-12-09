#include "fitz-internal.h"

#define SLOWCMYK

void
fz_free_colorspace_imp(fz_context *ctx, fz_storable *cs_)
{
	fz_colorspace *cs = (fz_colorspace *)cs_;

	if (cs->free_data && cs->data)
		cs->free_data(ctx, cs);
	fz_free(ctx, cs);
}

fz_colorspace *
fz_new_colorspace(fz_context *ctx, char *name, int n)
{
	fz_colorspace *cs = fz_malloc(ctx, sizeof(fz_colorspace));
	FZ_INIT_STORABLE(cs, 1, fz_free_colorspace_imp);
	cs->size = sizeof(fz_colorspace);
	fz_strlcpy(cs->name, name, sizeof cs->name);
	cs->n = n;
	cs->to_rgb = NULL;
	cs->from_rgb = NULL;
	cs->free_data = NULL;
	cs->data = NULL;
	return cs;
}

fz_colorspace *
fz_keep_colorspace(fz_context *ctx, fz_colorspace *cs)
{
	return (fz_colorspace *)fz_keep_storable(ctx, &cs->storable);
}

void
fz_drop_colorspace(fz_context *ctx, fz_colorspace *cs)
{
	fz_drop_storable(ctx, &cs->storable);
}

/* Device colorspace definitions */

static void gray_to_rgb(fz_context *ctx, fz_colorspace *cs, float *gray, float *rgb)
{
	rgb[0] = gray[0];
	rgb[1] = gray[0];
	rgb[2] = gray[0];
}

static void rgb_to_gray(fz_context *ctx, fz_colorspace *cs, float *rgb, float *gray)
{
	float r = rgb[0];
	float g = rgb[1];
	float b = rgb[2];
	gray[0] = r * 0.3f + g * 0.59f + b * 0.11f;
}

static void rgb_to_rgb(fz_context *ctx, fz_colorspace *cs, float *rgb, float *xyz)
{
	xyz[0] = rgb[0];
	xyz[1] = rgb[1];
	xyz[2] = rgb[2];
}

static void bgr_to_rgb(fz_context *ctx, fz_colorspace *cs, float *bgr, float *rgb)
{
	rgb[0] = bgr[2];
	rgb[1] = bgr[1];
	rgb[2] = bgr[0];
}

static void rgb_to_bgr(fz_context *ctx, fz_colorspace *cs, float *rgb, float *bgr)
{
	bgr[0] = rgb[2];
	bgr[1] = rgb[1];
	bgr[2] = rgb[0];
}

static void cmyk_to_rgb(fz_context *ctx, fz_colorspace *cs, float *cmyk, float *rgb)
{
#ifdef SLOWCMYK /* from poppler */
	float c = cmyk[0], m = cmyk[1], y = cmyk[2], k = cmyk[3];
	float r, g, b, x;
	float cm = c * m;
	float c1m = m - cm;
	float cm1 = c - cm;
	float c1m1 = 1 - m - cm1;
	float c1m1y = c1m1 * y;
	float c1m1y1 = c1m1 - c1m1y;
	float c1my = c1m * y;
	float c1my1 = c1m - c1my;
	float cm1y = cm1 * y;
	float cm1y1 = cm1 - cm1y;
	float cmy = cm * y;
	float cmy1 = cm - cmy;

	/* this is a matrix multiplication, unrolled for performance */
	x = c1m1y1 * k;		/* 0 0 0 1 */
	r = g = b = c1m1y1 - x;	/* 0 0 0 0 */
	r += 0.1373 * x;
	g += 0.1216 * x;
	b += 0.1255 * x;

	x = c1m1y * k;		/* 0 0 1 1 */
	r += 0.1098 * x;
	g += 0.1020 * x;
	x = c1m1y - x;		/* 0 0 1 0 */
	r += x;
	g += 0.9490 * x;

	x = c1my1 * k;		/* 0 1 0 1 */
	r += 0.1412 * x;
	x = c1my1 - x;		/* 0 1 0 0 */
	r += 0.9255 * x;
	b += 0.5490 * x;

	x = c1my * k;		/* 0 1 1 1 */
	r += 0.1333 * x;
	x = c1my - x;		/* 0 1 1 0 */
	r += 0.9294 * x;
	g += 0.1098 * x;
	b += 0.1412 * x;

	x = cm1y1 * k;		/* 1 0 0 1 */
	g += 0.0588 * x;
	b += 0.1412 * x;
	x = cm1y1 - x;		/* 1 0 0 0 */
	g += 0.6784 * x;
	b += 0.9373 * x;

	x = cm1y * k;		/* 1 0 1 1 */
	g += 0.0745 * x;
	x = cm1y - x;		/* 1 0 1 0 */
	g += 0.6510 * x;
	b += 0.3137 * x;

	x = cmy1 * k;		/* 1 1 0 1 */
	b += 0.0078 * x;
	x = cmy1 - x;		/* 1 1 0 0 */
	r += 0.1804 * x;
	g += 0.1922 * x;
	b += 0.5725 * x;

	x = cmy * (1-k);	/* 1 1 1 0 */
	r += 0.2118 * x;
	g += 0.2119 * x;
	b += 0.2235 * x;
	rgb[0] = fz_clamp(r, 0, 1);
	rgb[1] = fz_clamp(g, 0, 1);
	rgb[2] = fz_clamp(b, 0, 1);
#else
	rgb[0] = 1 - fz_min(1, cmyk[0] + cmyk[3]);
	rgb[1] = 1 - fz_min(1, cmyk[1] + cmyk[3]);
	rgb[2] = 1 - fz_min(1, cmyk[2] + cmyk[3]);
#endif
}

static void rgb_to_cmyk(fz_context *ctx, fz_colorspace *cs, float *rgb, float *cmyk)
{
	float c, m, y, k;
	c = 1 - rgb[0];
	m = 1 - rgb[1];
	y = 1 - rgb[2];
	k = fz_min(c, fz_min(m, y));
	cmyk[0] = c - k;
	cmyk[1] = m - k;
	cmyk[2] = y - k;
	cmyk[3] = k;
}

static fz_colorspace k_device_gray = { {-1, fz_free_colorspace_imp}, 0, "DeviceGray", 1, gray_to_rgb, rgb_to_gray };
static fz_colorspace k_device_rgb = { {-1, fz_free_colorspace_imp}, 0, "DeviceRGB", 3, rgb_to_rgb, rgb_to_rgb };
static fz_colorspace k_device_bgr = { {-1, fz_free_colorspace_imp}, 0, "DeviceRGB", 3, bgr_to_rgb, rgb_to_bgr };
static fz_colorspace k_device_cmyk = { {-1, fz_free_colorspace_imp}, 0, "DeviceCMYK", 4, cmyk_to_rgb, rgb_to_cmyk };

fz_colorspace *fz_device_gray = &k_device_gray;
fz_colorspace *fz_device_rgb = &k_device_rgb;
fz_colorspace *fz_device_bgr = &k_device_bgr;
fz_colorspace *fz_device_cmyk = &k_device_cmyk;

fz_colorspace *
fz_find_device_colorspace(fz_context *ctx, char *name)
{
	if (!strcmp(name, "DeviceGray"))
		return fz_device_gray;
	if (!strcmp(name, "DeviceRGB"))
		return fz_device_rgb;
	if (!strcmp(name, "DeviceBGR"))
		return fz_device_bgr;
	if (!strcmp(name, "DeviceCMYK"))
		return fz_device_cmyk;
	assert(!"unknown device colorspace");
	return NULL;
}

/* Fast pixmap color conversions */

static void fast_gray_to_rgb(fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = s[0];
		d[1] = s[0];
		d[2] = s[0];
		d[3] = s[1];
		s += 2;
		d += 4;
	}
}

static void fast_gray_to_cmyk(fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = 0;
		d[1] = 0;
		d[2] = 0;
		d[3] = s[0];
		d[4] = s[1];
		s += 2;
		d += 5;
	}
}

static void fast_rgb_to_gray(fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = ((s[0]+1) * 77 + (s[1]+1) * 150 + (s[2]+1) * 28) >> 8;
		d[1] = s[3];
		s += 4;
		d += 2;
	}
}

static void fast_bgr_to_gray(fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = ((s[0]+1) * 28 + (s[1]+1) * 150 + (s[2]+1) * 77) >> 8;
		d[1] = s[3];
		s += 4;
		d += 2;
	}
}

static void fast_rgb_to_cmyk(fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		unsigned char c = 255 - s[0];
		unsigned char m = 255 - s[1];
		unsigned char y = 255 - s[2];
		unsigned char k = (unsigned char)fz_mini(c, fz_mini(m, y));
		d[0] = c - k;
		d[1] = m - k;
		d[2] = y - k;
		d[3] = k;
		d[4] = s[3];
		s += 4;
		d += 5;
	}
}

static void fast_bgr_to_cmyk(fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		unsigned char c = 255 - s[2];
		unsigned char m = 255 - s[1];
		unsigned char y = 255 - s[0];
		unsigned char k = (unsigned char)fz_mini(c, fz_mini(m, y));
		d[0] = c - k;
		d[1] = m - k;
		d[2] = y - k;
		d[3] = k;
		d[4] = s[3];
		s += 4;
		d += 5;
	}
}

static void fast_cmyk_to_gray(fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		unsigned char c = fz_mul255(s[0], 77);
		unsigned char m = fz_mul255(s[1], 150);
		unsigned char y = fz_mul255(s[2], 28);
		d[0] = 255 - (unsigned char)fz_mini(c + m + y + s[3], 255);
		d[1] = s[4];
		s += 5;
		d += 2;
	}
}

#ifdef ARCH_ARM
static void
fast_cmyk_to_rgb_ARM(unsigned char *dst, unsigned char *src, int n)
__attribute__((naked));

static void
fast_cmyk_to_rgb_ARM(unsigned char *dst, unsigned char *src, int n)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r4-r11,r14}					\n"
	"@ r0 = dst							\n"
	"@ r1 = src							\n"
	"@ r2 = n							\n"
	"mov	r12, #0			@ r12= CMYK = 0			\n"
	"b	2f			@ enter loop			\n"
	"1:				@ White or Black		\n"
	"@ Cunning trick: On entry r11 = 0 if black, r11 = FF if white	\n"
	"ldrb	r7, [r1],#1		@ r8 = s[4]			\n"
	"strb	r11,[r0],#1		@ d[0] = r			\n"
	"strb	r11,[r0],#1		@ d[1] = g			\n"
	"strb	r11,[r0],#1		@ d[2] = b			\n"
	"strb	r7, [r0],#1		@ d[3] = s[4]			\n"
	"subs	r2, r2, #1		@ r2 = n--			\n"
	"beq	9f							\n"
	"2:				@ Main loop starts here		\n"
	"ldrb	r3, [r1], #4		@ r3 = c			\n"
	"ldrb	r6, [r1, #-1]		@ r6 = k			\n"
	"ldrb	r5, [r1, #-2]		@ r5 = y			\n"
	"ldrb	r4, [r1, #-3]		@ r4 = m			\n"
	"eors	r11,r6, #0xFF		@ if (k == 255)			\n"
	"beq	1b			@   goto black			\n"
	"orr	r7, r3, r4, LSL #8					\n"
	"orr	r14,r5, r6, LSL #8					\n"
	"orrs	r7, r7, r14,LSL #16	@ r7 = cmyk			\n"
	"beq	1b			@ if (cmyk == 0) white		\n"
	"@ At this point, we have to decode a new pixel			\n"
	"@ r0 = dst  r1 = src  r2 = n  r7 = cmyk			\n"
	"3:				@ unmatched			\n"
	"stmfd	r13!,{r0-r1,r7}		@ stash regs for space		\n"
	"add	r3, r3, r3, LSR #7	@ r3 = c += c>>7		\n"
	"add	r4, r4, r4, LSR #7	@ r4 = m += m>>7		\n"
	"add	r5, r5, r5, LSR #7	@ r5 = y += y>>7		\n"
	"add	r6, r6, r6, LSR #7	@ r6 = k += k>>7		\n"
	"mov	r5, r5, LSR #1		@ sacrifice 1 bit of Y		\n"
	"mul	r8, r3, r4		@ r8 = cm     = c * m		\n"
	"rsb	r9, r8, r4, LSL #8	@ r9 = c1m    = (m<<8) - cm	\n"
	"rsb	r3, r8, r3, LSL #8	@ r3 = cm1    = (c<<8) - cm	\n"
	"rsb	r4, r4, #0x100		@ r4 = 256-m			\n"
	"rsb	r4, r3, r4, LSL #8	@ r4 = c1m1   =((256-m)<<8)-cm1	\n"
	"mul	r7, r4, r5		@ r7 = c1m1y  = c1m1 * y	\n"
	"rsb	r4, r7, r4, LSL #7	@ r4 = c1m1y1 = (c1m1<<7)-c1m1y	\n"
	"mul	r10,r9, r5		@ r10= c1my   = c1m * y		\n"
	"rsb	r9, r10,r9, LSL #7	@ r9 = c1my1  = (c1m<<7) - c1my \n"
	"mul	r11,r3, r5		@ r11= cm1y   = cm1 * y		\n"
	"rsb	r3, r11,r3, LSL #7	@ r3 = cm1y1  = (cm1<<7) - cm1y	\n"
	"mul	r5, r8, r5		@ r5 = cmy    = cm * y		\n"
	"rsb	r8, r5, r8, LSL #7	@ r8 = cmy1   = (cm<<7) - cmy	\n"
	"@ Register recap:						\n"
	"@ r3 = cm1y1							\n"
	"@ r4 = c1m1y1							\n"
	"@ r5 = cmy							\n"
	"@ r6 = k							\n"
	"@ r7 = c1m1y							\n"
	"@ r8 = cmy1							\n"
	"@ r9 = c1my1							\n"
	"@ r10= c1my							\n"
	"@ r11= cm1y							\n"
	"@ The actual matrix multiplication				\n"
	"mul	r14,r4, r6		@ r14= x1 = c1m1y1 * k		\n"
	"rsb	r4, r14,r4, LSL #8	@ r4 = x0 = (c1m1y1<<8) - x1	\n"
	"add	r4, r4, r14,LSR #8-5	@ r4 = b = x0 + 32*(x1>>8)	\n"
	"sub	r1, r4, r14,LSR #8	@ r1 = g = x0 + 31*(x1>>8)	\n"
	"add	r0, r1, r14,LSR #8-2	@ r0 = r = x0 + 35*(x1>>8)	\n"
	"								\n"
	"mul	r14,r7, r6		@ r14= x1 = c1m1y * k		\n"
	"rsb	r7, r14,r7, LSL #8	@ r7 = x0 = (c1m1y<<8) - x1	\n"
	"add	r0, r0, r7		@ r0 = r += x0			\n"
	"add	r1, r1, r7		@ r1 = g += (x0>>8 * 256)	\n"
	"sub	r1, r1, r7, LSR #8-3	@                    248	\n"
	"sub	r1, r1, r7, LSR #8-2	@                    244	\n"
	"sub	r1, r1, r7, LSR #8	@                    243	\n"
	"sub	r7, r14,r14,LSR #3	@ r7 = 28*(x1>>5)		\n"
	"add	r0, r0, r7, LSR #8-5	@ r0 = r += 28 * x1		\n"
	"sub	r7, r7, r14,LSR #4	@ r7 = 26*(x1>>5)		\n"
	"add	r1, r1, r7, LSR #8-5	@ r1 = g += 26 * x1		\n"
	"								\n"
	"mul	r14,r9, r6		@ r14= x1 = c1my1 * k		\n"
	"sub	r9, r9, r14,LSR #8	@ r9 = x0>>8 = c1my1 - (x1>>8)	\n"
	"add	r0, r0, r14,LSR #8-5	@ r0 = r += (x1>>8)*32		\n"
	"add	r0, r0, r14,LSR #8-2	@ r0 = r += (x1>>8)*36		\n"
	"mov	r14,#237		@ r14= 237			\n"
	"mla	r0,r14,r9,r0		@ r14= r += x0*237		\n"
	"mov	r14,#141		@ r14= 141			\n"
	"mla	r4,r14,r9,r4		@ r14= b += x0*141		\n"
	"								\n"
	"mul	r14,r10,r6		@ r14= x1 = c1my * k		\n"
	"sub	r10,r10,r14,LSR #8	@ r10= x0>>8 = c1my - (x1>>8)	\n"
	"add	r0, r0, r14,LSR #8-5	@ r0 = r += 32 * x1		\n"
	"add	r0, r0, r14,LSR #8-1	@ r0 = r += 34 * x1		\n"
	"mov	r14,#238		@ r14= 238			\n"
	"mla	r0,r14,r10,r0		@ r0 = r += 238 * x0		\n"
	"mov	r14,#28			@ r14= 28			\n"
	"mla	r1,r14,r10,r1		@ r1 = g += 28 * x0		\n"
	"mov	r14,#36			@ r14= 36			\n"
	"mla	r4,r14,r10,r4		@ r4 = b += 36 * x0		\n"
	"								\n"
	"mul	r14,r3, r6		@ r14= x1 = cm1y1 * k		\n"
	"sub	r3, r3, r14,LSR #8	@ r3 = x1>>8 = cm1y1 - (x1>>8)	\n"
	"add	r1, r1, r14,LSR #8-4	@ r1 = g += 16*x1		\n"
	"sub	r1, r1, r14,LSR #8	@           15*x1		\n"
	"add	r4, r4, r14,LSR #8-5	@ r4 = b += 32*x1		\n"
	"add	r4, r4, r14,LSR #8-2	@           36*x1		\n"
	"mov	r14,#174		@ r14= 174			\n"
	"mla	r1, r14,r3, r1		@ r1 = g += 174 * x0		\n"
	"mov	r14,#240		@ r14= 240			\n"
	"mla	r4, r14,r3, r4		@ r4 = b += 240 * x0		\n"
	"								\n"
	"mul	r14,r11,r6		@ r14= x1 = cm1y * k		\n"
	"sub	r11,r11,r14,LSR #8	@ r11= x0>>8 = cm1y - (x1>>8)	\n"
	"add	r1, r1, r14,LSR #8-4	@ r1 = g += x1 * 16		\n"
	"add	r1, r1, r14,LSR #8	@           x1 * 17		\n"
	"add	r1, r1, r14,LSR #8-1	@           x1 * 19		\n"
	"mov	r14,#167		@ r14 = 167			\n"
	"mla	r1, r14,r11,r1		@ r1 = g += 167 * x0		\n"
	"mov	r14,#80			@ r14 = 80			\n"
	"mla	r4, r14,r11,r4		@ r4 = b += 80 * x0		\n"
	"								\n"
	"mul	r14,r8, r6		@ r14= x1 = cmy1 * k		\n"
	"sub	r8, r8, r14,LSR #8	@ r8 = x0>>8 = cmy1 - (x1>>8)	\n"
	"add	r4, r4, r14,LSR #8-1	@ r4 = b += x1 * 2		\n"
	"mov	r14,#46			@ r14=46			\n"
	"mla	r0, r14,r8, r0		@ r0 = r += 46 * x0		\n"
	"mov	r14,#49			@ r14=49			\n"
	"mla	r1, r14,r8, r1		@ r1 = g += 49 * x0		\n"
	"mov	r14,#147		@ r14=147			\n"
	"mla	r4, r14,r8, r4		@ r4 = b += 147 * x0		\n"
	"								\n"
	"rsb	r6, r6, #256		@ r6 = k = 256-k		\n"
	"mul	r14,r5, r6		@ r14= x0 = cmy * (256-k)	\n"
	"mov	r11,#54			@ r11= 54			\n"
	"mov	r14,r14,LSR #8		@ r14= (x0>>8)			\n"
	"mov	r8,#57			@ r8 = 57			\n"
	"mla	r0,r14,r11,r0		@ r0 = r += 54*x0		\n"
	"mla	r1,r14,r11,r1		@ r1 = g += 54*x0		\n"
	"mla	r4,r14,r8, r4		@ r4 = b += 57*x0		\n"
	"								\n"
	"sub	r8, r0, r0, LSR #8	@ r8 = r -= (r>>8)		\n"
	"sub	r9, r1, r1, LSR #8	@ r9 = g -= (r>>8)		\n"
	"sub	r10,r4, r4, LSR #8	@ r10= b -= (r>>8)		\n"
	"ldmfd	r13!,{r0-r1,r12}					\n"
	"mov	r8, r8, LSR #23		@ r8 = r>>23			\n"
	"mov	r9, r9, LSR #23		@ r9 = g>>23			\n"
	"mov	r10,r10,LSR #23		@ r10= b>>23			\n"
	"ldrb	r14,[r1],#1		@ r8 = s[4]			\n"
	"strb	r8, [r0],#1		@ d[0] = r			\n"
	"strb	r9, [r0],#1		@ d[1] = g			\n"
	"strb	r10,[r0],#1		@ d[2] = b			\n"
	"strb	r14,[r0],#1		@ d[3] = s[4]			\n"
	"subs	r2, r2, #1		@ r2 = n--			\n"
	"beq	9f							\n"
	"@ At this point, we've just decoded a pixel			\n"
	"@ r0 = dst  r1 = src  r2 = n  r8 = r  r9 = g  r10= b r12= CMYK \n"
	"4:								\n"
	"ldrb	r3, [r1], #4		@ r3 = c			\n"
	"ldrb	r6, [r1, #-1]		@ r6 = k			\n"
	"ldrb	r5, [r1, #-2]		@ r5 = y			\n"
	"ldrb	r4, [r1, #-3]		@ r4 = m			\n"
	"eors	r11,r6, #0xFF		@ if (k == 255)			\n"
	"beq	1b			@   goto black			\n"
	"orr	r7, r3, r4, LSL #8					\n"
	"orr	r14,r5, r6, LSL #8					\n"
	"orrs	r7, r7, r14,LSL #16	@ r7 = cmyk			\n"
	"beq	1b			@ if (cmyk == 0) white		\n"
	"cmp	r7, r12			@ if (cmyk != CMYK)		\n"
	"bne	3b			@   not the same, loop		\n"
	"@ If we get here, we just matched a pixel we have just decoded \n"
	"ldrb	r3, [r1],#1		@ r8 = s[4]			\n"
	"strb	r8, [r0],#1		@ d[0] = r			\n"
	"strb	r9, [r0],#1		@ d[1] = g			\n"
	"strb	r10,[r0],#1		@ d[2] = b			\n"
	"strb	r3, [r0],#1		@ d[3] = s[4]			\n"
	"subs	r2, r2, #1		@ r2 = n--			\n"
	"bne	4b							\n"
	"9:								\n"
	"ldmfd	r13!,{r4-r11,PC}	@ pop, return to thumb		\n"
	ENTER_THUMB
	);
}
#endif

static void fast_cmyk_to_rgb(fz_context *ctx, fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
#ifdef ARCH_ARM
	fast_cmyk_to_rgb_ARM(d, s, n);
#else
	unsigned int C,M,Y,K,r,g,b;

	C = 0;
	M = 0;
	Y = 0;
	K = 0;
	r = 255;
	g = 255;
	b = 255;

	while (n--)
	{
#ifdef SLOWCMYK
		/* SumatraPDF: prevent rendering regression */
#if 0
		unsigned int c = s[0];
		unsigned int m = s[1];
		unsigned int y = s[2];
		unsigned int k = s[3];
		unsigned int cm, c1m, cm1, c1m1, c1m1y, c1m1y1, c1my, c1my1, cm1y, cm1y1, cmy, cmy1;
		unsigned int x0, x1;

		if (c == C && m == M && y == Y && k == K)
		{
			/* Nothing to do */
		}
		else if (k == 0 && c == 0 && m == 0 && y == 0)
		{
			r = g = b = 255;
		}
		else if (k == 255)
		{
			r = g = b = 0;
		}
		else
		{
			c += c>>7;
			m += m>>7;
			y += y>>7;
			k += k>>7;
			y >>= 1; /* Ditch 1 bit of Y to avoid overflow */
			cm = c * m;
			c1m = (m<<8) - cm;
			cm1 = (c<<8) - cm;
			c1m1 = ((256 - m)<<8) - cm1;
			c1m1y = c1m1 * y;
			c1m1y1 = (c1m1<<7) - c1m1y;
			c1my = c1m * y;
			c1my1 = (c1m<<7) - c1my;
			cm1y = cm1 * y;
			cm1y1 = (cm1<<7) - cm1y;
			cmy = cm * y;
			cmy1 = (cm<<7) - cmy;

			/* this is a matrix multiplication, unrolled for performance */
			x1 = c1m1y1 * k;	/* 0 0 0 1 */
			x0 = (c1m1y1<<8) - x1;	/* 0 0 0 0 */
			x1 = x1>>8;		/* From 23 fractional bits to 15 */
			r = g = b = x0;
			r += 35 * x1;	/* 0.1373 */
			g += 31 * x1;	/* 0.1216 */
			b += 32 * x1;	/* 0.1255 */

			x1 = c1m1y * k;		/* 0 0 1 1 */
			x0 = (c1m1y<<8) - x1;	/* 0 0 1 0 */
			x1 >>= 8;		/* From 23 fractional bits to 15 */
			r += 28 * x1;	/* 0.1098 */
			g += 26 * x1;	/* 0.1020 */
			r += x0;
			x0 >>= 8;		/* From 23 fractional bits to 15 */
			g += 243 * x0;	/* 0.9490 */

			x1 = c1my1 * k;		/* 0 1 0 1 */
			x0 = (c1my1<<8) - x1;	/* 0 1 0 0 */
			x1 >>= 8;		/* From 23 fractional bits to 15 */
			x0 >>= 8;		/* From 23 fractional bits to 15 */
			r += 36 * x1;	/* 0.1412 */
			r += 237 * x0;	/* 0.9255 */
			b += 141 * x0;	/* 0.5490 */

			x1 = c1my * k;		/* 0 1 1 1 */
			x0 = (c1my<<8) - x1;	/* 0 1 1 0 */
			x1 >>= 8;		/* From 23 fractional bits to 15 */
			x0 >>= 8;		/* From 23 fractional bits to 15 */
			r += 34 * x1;	/* 0.1333 */
			r += 238 * x0;	/* 0.9294 */
			g += 28 * x0;	/* 0.1098 */
			b += 36 * x0;	/* 0.1412 */

			x1 = cm1y1 * k;		/* 1 0 0 1 */
			x0 = (cm1y1<<8) - x1;	/* 1 0 0 0 */
			x1 >>= 8;		/* From 23 fractional bits to 15 */
			x0 >>= 8;		/* From 23 fractional bits to 15 */
			g += 15 * x1;	/* 0.0588 */
			b += 36 * x1;	/* 0.1412 */
			g += 174 * x0;	/* 0.6784 */
			b += 240 * x0;	/* 0.9373 */

			x1 = cm1y * k;		/* 1 0 1 1 */
			x0 = (cm1y<<8) - x1;	/* 1 0 1 0 */
			x1 >>= 8;		/* From 23 fractional bits to 15 */
			x0 >>= 8;		/* From 23 fractional bits to 15 */
			g += 19 * x1;	/* 0.0745 */
			g += 167 * x0;	/* 0.6510 */
			b += 80 * x0;	/* 0.3137 */

			x1 = cmy1 * k;		/* 1 1 0 1 */
			x0 = (cmy1<<8) - x1;	/* 1 1 0 0 */
			x1 >>= 8;		/* From 23 fractional bits to 15 */
			x0 >>= 8;		/* From 23 fractional bits to 15 */
			b += 2 * x1;	/* 0.0078 */
			r += 46 * x0;	/* 0.1804 */
			g += 49 * x0;	/* 0.1922 */
			b += 147 * x0;	/* 0.5725 */

			x0 = cmy * (256-k);	/* 1 1 1 0 */
			x0 >>= 8;		/* From 23 fractional bits to 15 */
			r += 54 * x0;	/* 0.2118 */
			g += 54 * x0;	/* 0.2119 */
			b += 57 * x0;	/* 0.2235 */

			r -= (r>>8);
			g -= (g>>8);
			b -= (b>>8);
			r = r>>23;
			g = g>>23;
			b = b>>23;
			C = c;
			M = m;
			Y = y;
			K = k;
		}
		d[0] = r;
		d[1] = g;
		d[2] = b;
#else
		float cmyk[4], rgb[3];
		cmyk[0] = s[0] / 255.0f;
		cmyk[1] = s[1] / 255.0f;
		cmyk[2] = s[2] / 255.0f;
		cmyk[3] = s[3] / 255.0f;
		cmyk_to_rgb(ctx, NULL, cmyk, rgb);
		d[0] = rgb[0] * 255;
		d[1] = rgb[1] * 255;
		d[2] = rgb[2] * 255;
#endif
#else
		d[0] = 255 - (unsigned char)fz_mini(s[0] + s[3], 255);
		d[1] = 255 - (unsigned char)fz_mini(s[1] + s[3], 255);
		d[2] = 255 - (unsigned char)fz_mini(s[2] + s[3], 255);
#endif
		d[3] = s[4];
		s += 5;
		d += 4;
	}
#endif
}

static void fast_cmyk_to_bgr(fz_context *ctx, fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
#ifdef SLOWCMYK
		float cmyk[4], rgb[3];
		cmyk[0] = s[0] / 255.0f;
		cmyk[1] = s[1] / 255.0f;
		cmyk[2] = s[2] / 255.0f;
		cmyk[3] = s[3] / 255.0f;
		cmyk_to_rgb(ctx, NULL, cmyk, rgb);
		d[0] = rgb[2] * 255;
		d[1] = rgb[1] * 255;
		d[2] = rgb[0] * 255;
#else
		d[0] = 255 - (unsigned char)fz_mini(s[2] + s[3], 255);
		d[1] = 255 - (unsigned char)fz_mini(s[1] + s[3], 255);
		d[2] = 255 - (unsigned char)fz_mini(s[0] + s[3], 255);
#endif
		d[3] = s[4];
		s += 5;
		d += 4;
	}
}

static void fast_rgb_to_bgr(fz_pixmap *dst, fz_pixmap *src)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = s[2];
		d[1] = s[1];
		d[2] = s[0];
		d[3] = s[3];
		s += 4;
		d += 4;
	}
}

static void
fz_std_conv_pixmap(fz_context *ctx, fz_pixmap *dst, fz_pixmap *src)
{
	float srcv[FZ_MAX_COLORS];
	float dstv[FZ_MAX_COLORS];
	int srcn, dstn;
	int k, i;
	unsigned int xy;

	fz_colorspace *ss = src->colorspace;
	fz_colorspace *ds = dst->colorspace;

	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;

	assert(src->w == dst->w && src->h == dst->h);
	assert(src->n == ss->n + 1);
	assert(dst->n == ds->n + 1);

	srcn = ss->n;
	dstn = ds->n;

	xy = (unsigned int)(src->w * src->h);

	/* Special case for Lab colorspace (scaling of components to float) */
	if (!strcmp(ss->name, "Lab") && srcn == 3)
	{
		fz_color_converter cc;

		fz_find_color_converter(&cc, ctx, ds, ss);
		for (; xy > 0; xy--)
		{
			srcv[0] = *s++ / 255.0f * 100;
			srcv[1] = *s++ - 128;
			srcv[2] = *s++ - 128;

			cc.convert(&cc, dstv, srcv);

			for (k = 0; k < dstn; k++)
				*d++ = dstv[k] * 255;

			*d++ = *s++;
		}
	}

	/* Brute-force for small images */
	else if (xy < 256)
	{
		fz_color_converter cc;

		fz_find_color_converter(&cc, ctx, ds, ss);
		for (; xy > 0; xy--)
		{
			for (k = 0; k < srcn; k++)
				srcv[k] = *s++ / 255.0f;

			cc.convert(&cc, dstv, srcv);

			for (k = 0; k < dstn; k++)
				*d++ = dstv[k] * 255;

			*d++ = *s++;
		}
	}

	/* 1-d lookup table for separation and similar colorspaces */
	else if (srcn == 1)
	{
		unsigned char lookup[FZ_MAX_COLORS * 256];
		fz_color_converter cc;

		fz_find_color_converter(&cc, ctx, ds, ss);
		for (i = 0; i < 256; i++)
		{
			srcv[0] = i / 255.0f;
			cc.convert(&cc, dstv, srcv);
			for (k = 0; k < dstn; k++)
				lookup[i * dstn + k] = dstv[k] * 255;
		}

		for (; xy > 0; xy--)
		{
			i = *s++;
			for (k = 0; k < dstn; k++)
				*d++ = lookup[i * dstn + k];
			*d++ = *s++;
		}
	}

	/* Memoize colors using a hash table for the general case */
	else
	{
		fz_hash_table *lookup;
		unsigned char *color;
		unsigned char dummy = s[0] ^ 255;
		unsigned char *sold = &dummy;
		fz_color_converter cc;

		fz_find_color_converter(&cc, ctx, ds, ss);
		lookup = fz_new_hash_table(ctx, 509, srcn, -1);

		for (; xy > 0; xy--)
		{
			if (*s == *sold && memcmp(sold,s,srcn) == 0)
			{
				sold = s;
				memcpy(d, d-dstn-1, dstn);
				d += dstn;
				s += srcn;
				*d++ = *s++;
			}
			else
			{
				sold = s;
				color = fz_hash_find(ctx, lookup, s);
				if (color)
				{
					memcpy(d, color, dstn);
					s += srcn;
					d += dstn;
					*d++ = *s++;
				}
				else
				{
					for (k = 0; k < srcn; k++)
						srcv[k] = *s++ / 255.0f;
					cc.convert(&cc, dstv, srcv);
					for (k = 0; k < dstn; k++)
						*d++ = dstv[k] * 255;

					fz_hash_insert(ctx, lookup, s - srcn, d - dstn);

					*d++ = *s++;
				}
			}
		}

		fz_free_hash(ctx, lookup);
	}
}

void
fz_convert_pixmap(fz_context *ctx, fz_pixmap *dp, fz_pixmap *sp)
{
	fz_colorspace *ss = sp->colorspace;
	fz_colorspace *ds = dp->colorspace;

	assert(ss && ds);

	dp->interpolate = sp->interpolate;

	if (ss == fz_device_gray)
	{
		if (ds == fz_device_rgb) fast_gray_to_rgb(dp, sp);
		else if (ds == fz_device_bgr) fast_gray_to_rgb(dp, sp); /* bgr == rgb here */
		else if (ds == fz_device_cmyk) fast_gray_to_cmyk(dp, sp);
		else fz_std_conv_pixmap(ctx, dp, sp);
	}

	else if (ss == fz_device_rgb)
	{
		if (ds == fz_device_gray) fast_rgb_to_gray(dp, sp);
		else if (ds == fz_device_bgr) fast_rgb_to_bgr(dp, sp);
		else if (ds == fz_device_cmyk) fast_rgb_to_cmyk(dp, sp);
		else fz_std_conv_pixmap(ctx, dp, sp);
	}

	else if (ss == fz_device_bgr)
	{
		if (ds == fz_device_gray) fast_bgr_to_gray(dp, sp);
		else if (ds == fz_device_rgb) fast_rgb_to_bgr(dp, sp); /* bgr = rgb here */
		else if (ds == fz_device_cmyk) fast_bgr_to_cmyk(sp, dp);
		else fz_std_conv_pixmap(ctx, dp, sp);
	}

	else if (ss == fz_device_cmyk)
	{
		if (ds == fz_device_gray) fast_cmyk_to_gray(dp, sp);
		else if (ds == fz_device_bgr) fast_cmyk_to_bgr(ctx, dp, sp);
		else if (ds == fz_device_rgb) fast_cmyk_to_rgb(ctx, dp, sp);
		else fz_std_conv_pixmap(ctx, dp, sp);
	}

	else fz_std_conv_pixmap(ctx, dp, sp);
}

/* Convert a single color */

static void
std_conv_color(fz_color_converter *cc, float *dstv, float *srcv)
{
	float rgb[3];
	int i;
	fz_colorspace *srcs = cc->ss;
	fz_colorspace *dsts = cc->ds;
	fz_context *ctx = cc->ctx;

	if (srcs != dsts)
	{
		assert(srcs->to_rgb && dsts->from_rgb);
		srcs->to_rgb(ctx, srcs, srcv, rgb);
		dsts->from_rgb(ctx, dsts, rgb, dstv);
		for (i = 0; i < dsts->n; i++)
			dstv[i] = fz_clamp(dstv[i], 0, 1);
	}
	else
	{
		for (i = 0; i < srcs->n; i++)
			dstv[i] = srcv[i];
	}
}

static void
g2rgb(fz_color_converter *cc, float *dv, float *sv)
{
	dv[0] = sv[0];
	dv[1] = sv[0];
	dv[2] = sv[0];
}

static void
g2cmyk(fz_color_converter *cc, float *dv, float *sv)
{
	dv[0] = 0;
	dv[1] = 0;
	dv[2] = 0;
	dv[3] = sv[0];
}

static void
rgb2g(fz_color_converter *cc, float *dv, float *sv)
{
	dv[0] = sv[0] * 0.3f + sv[1] * 0.59f + sv[2] * 0.11f;
}

static void
rgb2bgr(fz_color_converter *cc, float *dv, float *sv)
{
	dv[0] = sv[2];
	dv[1] = sv[1];
	dv[2] = sv[0];
}

static void
rgb2cmyk(fz_color_converter *cc, float *dv, float *sv)
{
	float c = 1 - sv[0];
	float m = 1 - sv[1];
	float y = 1 - sv[2];
	float k = fz_min(c, fz_min(m, y));
	dv[0] = c - k;
	dv[1] = m - k;
	dv[2] = y - k;
	dv[3] = k;
}

static void
bgr2g(fz_color_converter *cc, float *dv, float *sv)
{
	dv[0] = sv[0] * 0.11f + sv[1] * 0.59f + sv[2] * 0.3f;
}

static void
bgr2cmyk(fz_color_converter *cc, float *dv, float *sv)
{
	float c = 1 - sv[2];
	float m = 1 - sv[1];
	float y = 1 - sv[0];
	float k = fz_min(c, fz_min(m, y));
	dv[0] = c - k;
	dv[1] = m - k;
	dv[2] = y - k;
	dv[3] = k;
}

static void
cmyk2g(fz_color_converter *cc, float *dv, float *sv)
{
	float c = sv[0] * 0.3f;
	float m = sv[1] * 0.59f;
	float y = sv[2] * 0.11f;
	dv[0] = 1 - fz_min(c + m + y + sv[3], 1);
}

static void
cmyk2rgb(fz_color_converter *cc, float *dv, float *sv)
{
#ifdef SLOWCMYK
	cmyk_to_rgb(cc->ctx, NULL, sv, dv);
#else
	dv[0] = 1 - fz_min(sv[0] + sv[3], 1);
	dv[1] = 1 - fz_min(sv[1] + sv[3], 1);
	dv[2] = 1 - fz_min(sv[2] + sv[3], 1);
#endif
}

static void
cmyk2bgr(fz_color_converter *cc, float *dv, float *sv)
{
#ifdef SLOWCMYK
	float rgb[3];
	cmyk_to_rgb(cc->ctx, NULL, sv, rgb);
	dv[0] = rgb[2];
	dv[1] = rgb[1];
	dv[2] = rgb[0];
#else
	dv[0] = 1 - fz_min(sv[2] + sv[3], 1);
	dv[1] = 1 - fz_min(sv[1] + sv[3], 1);
	dv[2] = 1 - fz_min(sv[0] + sv[3], 1);
#endif
}

void fz_find_color_converter(fz_color_converter *cc, fz_context *ctx, fz_colorspace *ds, fz_colorspace *ss)
{
	cc->ctx = ctx;
	cc->ds = ds;
	cc->ss = ss;
	if (ss == fz_device_gray)
	{
		if ((ds == fz_device_rgb) || (ds == fz_device_bgr))
			cc->convert = g2rgb;
		else if (ds == fz_device_cmyk)
			cc->convert = g2cmyk;
		else
			cc->convert = std_conv_color;
	}

	else if (ss == fz_device_rgb)
	{
		if (ds == fz_device_gray)
			cc->convert = rgb2g;
		else if (ds == fz_device_bgr)
			cc->convert = rgb2bgr;
		else if (ds == fz_device_cmyk)
			cc->convert = rgb2cmyk;
		else
			cc->convert = std_conv_color;
	}

	else if (ss == fz_device_bgr)
	{
		if (ds == fz_device_gray)
			cc->convert = bgr2g;
		else if (ds == fz_device_rgb)
			cc->convert = rgb2bgr;
		else if (ds == fz_device_cmyk)
			cc->convert = bgr2cmyk;
		else
			cc->convert = std_conv_color;
	}

	else if (ss == fz_device_cmyk)
	{
		if (ds == fz_device_gray)
			cc->convert = cmyk2g;
		else if (ds == fz_device_rgb)
			cc->convert = cmyk2rgb;
		else if (ds == fz_device_bgr)
			cc->convert = cmyk2bgr;
		else
			cc->convert = std_conv_color;
	}

	else
		cc->convert = std_conv_color;
}

void
fz_convert_color(fz_context *ctx, fz_colorspace *ds, float *dv, fz_colorspace *ss, float *sv)
{
	fz_color_converter cc;

	fz_find_color_converter(&cc, ctx, ds, ss);
	cc.convert(&cc, dv, sv);
}

/* SumatraPDF: support transfer functions */
fz_transfer_function *
fz_keep_transfer_function(fz_context *ctx, fz_transfer_function *tr)
{
	return tr ? (fz_transfer_function *)fz_keep_storable(ctx, &tr->storable) : NULL;
}

void
fz_drop_transfer_function(fz_context *ctx, fz_transfer_function *tr)
{
	if (tr)
		fz_drop_storable(ctx, &tr->storable);
}
