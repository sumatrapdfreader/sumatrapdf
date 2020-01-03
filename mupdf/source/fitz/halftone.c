#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <assert.h>

struct fz_halftone_s
{
	int refs;
	int n;
	fz_pixmap *comp[1];
};

static fz_halftone *
fz_new_halftone(fz_context *ctx, int comps)
{
	fz_halftone *ht;
	int i;

	ht = Memento_label(fz_malloc(ctx, sizeof(fz_halftone) + (comps-1)*sizeof(fz_pixmap *)), "fz_halftone");
	ht->refs = 1;
	ht->n = comps;
	for (i = 0; i < comps; i++)
		ht->comp[i] = NULL;

	return ht;
}

fz_halftone *
fz_keep_halftone(fz_context *ctx, fz_halftone *ht)
{
	return fz_keep_imp(ctx, ht, &ht->refs);
}

void
fz_drop_halftone(fz_context *ctx, fz_halftone *ht)
{
	int i;
	if (fz_drop_imp(ctx, ht, &ht->refs))
	{
		for (i = 0; i < ht->n; i++)
			fz_drop_pixmap(ctx, ht->comp[i]);
		fz_free(ctx, ht);
	}
}

/* Default mono halftone, lifted from Ghostscript. */
/* The 0x00 entry has been changed to 0x01 to avoid problems with white
 * pixels appearing in the output; as we use < 0 should not appear in the
 * array. I think that gs scales this slightly and hence never actually uses
 * the raw values here. */
static unsigned char mono_ht[] =
{
	0x0E, 0x8E, 0x2E, 0xAE, 0x06, 0x86, 0x26, 0xA6, 0x0C, 0x8C, 0x2C, 0xAC, 0x04, 0x84, 0x24, 0xA4,
	0xCE, 0x4E, 0xEE, 0x6E, 0xC6, 0x46, 0xE6, 0x66, 0xCC, 0x4C, 0xEC, 0x6C, 0xC4, 0x44, 0xE4, 0x64,
	0x3E, 0xBE, 0x1E, 0x9E, 0x36, 0xB6, 0x16, 0x96, 0x3C, 0xBC, 0x1C, 0x9C, 0x34, 0xB4, 0x14, 0x94,
	0xFE, 0x7E, 0xDE, 0x5E, 0xF6, 0x76, 0xD6, 0x56, 0xFC, 0x7C, 0xDC, 0x5C, 0xF4, 0x74, 0xD4, 0x54,
	0x01, 0x81, 0x21, 0xA1, 0x09, 0x89, 0x29, 0xA9, 0x03, 0x83, 0x23, 0xA3, 0x0B, 0x8B, 0x2B, 0xAB,
	0xC1, 0x41, 0xE1, 0x61, 0xC9, 0x49, 0xE9, 0x69, 0xC3, 0x43, 0xE3, 0x63, 0xCB, 0x4B, 0xEB, 0x6B,
	0x31, 0xB1, 0x11, 0x91, 0x39, 0xB9, 0x19, 0x99, 0x33, 0xB3, 0x13, 0x93, 0x3B, 0xBB, 0x1B, 0x9B,
	0xF1, 0x71, 0xD1, 0x51, 0xF9, 0x79, 0xD9, 0x59, 0xF3, 0x73, 0xD3, 0x53, 0xFB, 0x7B, 0xDB, 0x5B,
	0x0D, 0x8D, 0x2D, 0xAD, 0x05, 0x85, 0x25, 0xA5, 0x0F, 0x8F, 0x2F, 0xAF, 0x07, 0x87, 0x27, 0xA7,
	0xCD, 0x4D, 0xED, 0x6D, 0xC5, 0x45, 0xE5, 0x65, 0xCF, 0x4F, 0xEF, 0x6F, 0xC7, 0x47, 0xE7, 0x67,
	0x3D, 0xBD, 0x1D, 0x9D, 0x35, 0xB5, 0x15, 0x95, 0x3F, 0xBF, 0x1F, 0x9F, 0x37, 0xB7, 0x17, 0x97,
	0xFD, 0x7D, 0xDD, 0x5D, 0xF5, 0x75, 0xD5, 0x55, 0xFF, 0x7F, 0xDF, 0x5F, 0xF7, 0x77, 0xD7, 0x57,
	0x02, 0x82, 0x22, 0xA2, 0x0A, 0x8A, 0x2A, 0xAA, 0x01 /*0x00*/, 0x80, 0x20, 0xA0, 0x08, 0x88, 0x28, 0xA8,
	0xC2, 0x42, 0xE2, 0x62, 0xCA, 0x4A, 0xEA, 0x6A, 0xC0, 0x40, 0xE0, 0x60, 0xC8, 0x48, 0xE8, 0x68,
	0x32, 0xB2, 0x12, 0x92, 0x3A, 0xBA, 0x1A, 0x9A, 0x30, 0xB0, 0x10, 0x90, 0x38, 0xB8, 0x18, 0x98,
	0xF2, 0x72, 0xD2, 0x52, 0xFA, 0x7A, 0xDA, 0x5A, 0xF0, 0x70, 0xD0, 0x50, 0xF8, 0x78, 0xD8, 0x58
};

/*
	Create a 'default' halftone structure
	for the given number of components.

	num_comps: The number of components to use.

	Returns a simple default halftone. The default halftone uses
	the same halftone tile for each plane, which may not be ideal
	for all purposes.
*/
fz_halftone *fz_default_halftone(fz_context *ctx, int num_comps)
{
	fz_halftone *ht = fz_new_halftone(ctx, num_comps);

	fz_try(ctx)
	{
		int i;
		for (i = 0; i < num_comps; i++)
			ht->comp[i] = fz_new_pixmap_with_data(ctx, NULL, 16, 16, NULL, 1, 16, mono_ht);
	}
	fz_catch(ctx)
	{
		fz_drop_halftone(ctx, ht);
		fz_rethrow(ctx);
	}

	return ht;
}

/* Finally, code to actually perform halftoning. */
static void make_ht_line(unsigned char *buf, fz_halftone *ht, int x, int y, int w)
{
	int k, n;
	n = ht->n;
	for (k = 0; k < n; k++)
	{
		fz_pixmap *tile = ht->comp[k];
		unsigned char *b = buf++;
		unsigned char *t;
		unsigned char *tbase;
		int px = x + tile->x;
		int py = y + tile->y;
		int tw = tile->w;
		int th = tile->h;
		int w2 = w;
		int len;
		px = px % tw;
		if (px < 0)
			px += tw;
		py = py % th;
		if (py < 0)
			py += th;

		assert(tile->n == 1);

		/* Left hand section; from x to tile width */
		tbase = tile->samples + (unsigned int)(py * tw);
		t = tbase + px;
		len = tw - px;
		if (len > w2)
			len = w2;
		w2 -= len;
		while (len--)
		{
			*b = *t++;
			b += n;
		}

		/* Centre section - complete copies */
		w2 -= tw;
		while (w2 >= 0)
		{
			len = tw;
			t = tbase;
			while (len--)
			{
				*b = *t++;
				b += n;
			}
			w2 -= tw;
		}
		w2 += tw;

		/* Right hand section - stragglers */
		t = tbase;
		while (w2--)
		{
			*b = *t++;
			b += n;
		}
	}
}

/* Inner mono thresholding code */
typedef void (threshold_fn)(const unsigned char * FZ_RESTRICT ht_line, const unsigned char * FZ_RESTRICT pixmap, unsigned char * FZ_RESTRICT out, int w, int ht_len);

#ifdef ARCH_ARM
static void
do_threshold_1(const unsigned char * FZ_RESTRICT ht_line, const unsigned char * FZ_RESTRICT pixmap, unsigned char * FZ_RESTRICT out, int w, int ht_len)
__attribute__((naked));

static void
do_threshold_1(const unsigned char * FZ_RESTRICT ht_line, const unsigned char * FZ_RESTRICT pixmap, unsigned char * FZ_RESTRICT out, int w, int ht_len)
{
	asm volatile(
	ENTER_ARM
	// Store one more reg that required to keep double stack alignment
	".syntax unified\n"
	"stmfd	r13!,{r4-r7,r9,r14}				\n"
	"@ r0 = ht_line						\n"
	"@ r1 = pixmap						\n"
	"@ r2 = out						\n"
	"@ r3 = w						\n"
	"@ <> = ht_len						\n"
	"ldr	r9, [r13,#6*4]		@ r9 = ht_len		\n"
	"subs	r3, r3, #7		@ r3 = w -= 7		\n"
	"ble	2f			@ while (w > 0) {	\n"
	"mov	r12,r9			@ r12= l = ht_len	\n"
	"b	1f						\n"
	"9:							\n"
	"strb	r14,[r2], #1		@ *out++ = 0		\n"
	"subs	r12,r12,#8		@ r12 = l -= 8		\n"
	"moveq	r12,r9			@ if(l==0) l = ht_len	\n"
	"subeq	r0, r0, r9		@          ht_line -= l	\n"
	"subs	r3, r3, #8		@ w -= 8		\n"
	"ble	2f			@ }			\n"
	"1:							\n"
	"ldr	r14,[r1], #4		@ r14= pixmap[0..3]	\n"
	"ldr	r5, [r1], #4		@ r5 = pixmap[4..7]	\n"
	"ldrb	r4, [r0], #8		@ r0 = ht_line += 8	\n"
	"adds   r14,r14,#1		@ set eq iff r14=-1	\n"
	"addseq	r5, r5, #1		@ set eq iff r14=r5=-1	\n"
	"beq	9b			@	white		\n"
	"ldrb	r5, [r1, #-8]		@ r5 = pixmap[0]	\n"
	"ldrb	r6, [r0, #-7]		@ r6 = ht_line[1]	\n"
	"ldrb	r7, [r1, #-7]		@ r7 = pixmap[1]	\n"
	"mov	r14,#0			@ r14= h = 0		\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x80		@	h |= 0x80	\n"
	"ldrb	r4, [r0, #-6]		@ r4 = ht_line[2]	\n"
	"ldrb	r5, [r1, #-6]		@ r5 = pixmap[2]	\n"
	"cmp	r7, r6			@ if (r7 < r6)		\n"
	"orrlt	r14,r14,#0x40		@	h |= 0x40	\n"
	"ldrb	r6, [r0, #-5]		@ r6 = ht_line[3]	\n"
	"ldrb	r7, [r1, #-5]		@ r7 = pixmap[3]	\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x20		@	h |= 0x20	\n"
	"ldrb	r4, [r0, #-4]		@ r4 = ht_line[4]	\n"
	"ldrb	r5, [r1, #-4]		@ r5 = pixmap[4]	\n"
	"cmp	r7, r6			@ if (r7 < r6)		\n"
	"orrlt	r14,r14,#0x10		@	h |= 0x10	\n"
	"ldrb	r6, [r0, #-3]		@ r6 = ht_line[5]	\n"
	"ldrb	r7, [r1, #-3]		@ r7 = pixmap[5]	\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x08		@	h |= 0x08	\n"
	"ldrb	r4, [r0, #-2]		@ r4 = ht_line[6]	\n"
	"ldrb	r5, [r1, #-2]		@ r5 = pixmap[6]	\n"
	"cmp	r7, r6			@ if (r7 < r6)		\n"
	"orrlt	r14,r14,#0x04		@	h |= 0x04	\n"
	"ldrb	r6, [r0, #-1]		@ r6 = ht_line[7]	\n"
	"ldrb	r7, [r1, #-1]		@ r7 = pixmap[7]	\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x02		@	h |= 0x02	\n"
	"cmp	r7, r6			@ if (r7 < r6)		\n"
	"orrlt	r14,r14,#0x01		@	h |= 0x01	\n"
	"subs	r12,r12,#8		@ r12 = l -= 8		\n"
	"strb	r14,[r2], #1		@ *out++ = h		\n"
	"moveq	r12,r9			@ if(l==0) l = ht_len	\n"
	"subeq	r0, r0, r9		@          ht_line -= l	\n"
	"subs	r3, r3, #8		@ w -= 8		\n"
	"bgt	1b			@ }			\n"
	"2:							\n"
	"adds	r3, r3, #7		@ w += 7		\n"
	"ble	4f			@ if (w >= 0) {		\n"
	"ldrb	r4, [r0], #1		@ r4 = ht_line[0]	\n"
	"ldrb	r5, [r1], #1		@ r5 = pixmap[0]	\n"
	"mov	r14, #0			@ r14= h = 0		\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x80		@	h |= 0x80	\n"
	"cmp	r3, #1			@			\n"
	"ldrbgt	r4, [r0], #1		@ r6 = ht_line[1]	\n"
	"ldrbgt	r5, [r1], #1		@ r7 = pixmap[1]	\n"
	"ble	3f			@			\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x40		@	h |= 0x40	\n"
	"cmp	r3, #2			@			\n"
	"ldrbgt	r4, [r0], #1		@ r6 = ht_line[2]	\n"
	"ldrbgt	r5, [r1], #1		@ r7 = pixmap[2]	\n"
	"ble	3f			@			\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x20		@	h |= 0x20	\n"
	"cmp	r3, #3			@			\n"
	"ldrbgt	r4, [r0], #1		@ r6 = ht_line[3]	\n"
	"ldrbgt	r5, [r1], #1		@ r7 = pixmap[3]	\n"
	"ble	3f			@			\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x10		@	h |= 0x10	\n"
	"cmp	r3, #4			@			\n"
	"ldrbgt	r4, [r0], #1		@ r6 = ht_line[4]	\n"
	"ldrbgt	r5, [r1], #1		@ r7 = pixmap[4]	\n"
	"ble	3f			@			\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x08		@	h |= 0x08	\n"
	"cmp	r3, #5			@			\n"
	"ldrbgt	r4, [r0], #1		@ r6 = ht_line[5]	\n"
	"ldrbgt	r5, [r1], #1		@ r7 = pixmap[5]	\n"
	"ble	3f			@			\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x04		@	h |= 0x04	\n"
	"cmp	r3, #6			@			\n"
	"ldrbgt	r4, [r0], #1		@ r6 = ht_line[6]	\n"
	"ldrbgt	r5, [r1], #1		@ r7 = pixmap[6]	\n"
	"ble	3f			@			\n"
	"cmp	r5, r4			@ if (r5 < r4)		\n"
	"orrlt	r14,r14,#0x02		@	h |= 0x02	\n"
	"3:							\n"
	"strb	r14,[r2]		@ *out = h		\n"
	"4:							\n"
	"ldmfd	r13!,{r4-r7,r9,PC}	@ pop, return to thumb	\n"
	ENTER_THUMB
	);
}
#else
static void do_threshold_1(const unsigned char * FZ_RESTRICT ht_line, const unsigned char * FZ_RESTRICT pixmap, unsigned char * FZ_RESTRICT out, int w, int ht_len)
{
	int h;
	int l = ht_len;

	w -= 7;
	while (w > 0)
	{
		h = 0;
		if (pixmap[0] < ht_line[0])
			h |= 0x80;
		if (pixmap[1] < ht_line[1])
			h |= 0x40;
		if (pixmap[2] < ht_line[2])
			h |= 0x20;
		if (pixmap[3] < ht_line[3])
			h |= 0x10;
		if (pixmap[4] < ht_line[4])
			h |= 0x08;
		if (pixmap[5] < ht_line[5])
			h |= 0x04;
		if (pixmap[6] < ht_line[6])
			h |= 0x02;
		if (pixmap[7] < ht_line[7])
			h |= 0x01;
		pixmap += 8;
		ht_line += 8;
		l -= 8;
		if (l == 0)
		{
			l = ht_len;
			ht_line -= ht_len;
		}
		*out++ = h;
		w -= 8;
	}
	if (w > -7)
	{
		h = 0;
		if (pixmap[0] < ht_line[0])
			h |= 0x80;
		if (w > -6 && pixmap[1] < ht_line[1])
			h |= 0x40;
		if (w > -5 && pixmap[2] < ht_line[2])
			h |= 0x20;
		if (w >	-4 && pixmap[3] < ht_line[3])
			h |= 0x10;
		if (w > -3 && pixmap[4] < ht_line[4])
			h |= 0x08;
		if (w > -2 && pixmap[5] < ht_line[5])
			h |= 0x04;
		if (w > -1 && pixmap[6] < ht_line[6])
			h |= 0x02;
		*out++ = h;
	}
}
#endif

/*
	Note that the tests in do_threshold_4 are inverted compared to those
	in do_threshold_1. This is to allow for the fact that the CMYK
	contone renderings have white = 0, whereas rgb, and greyscale have
	white = 0xFF. Reversing these tests enables us to maintain that
	BlackIs1 in bitmaps.
*/
#ifdef ARCH_ARM
static void
do_threshold_4(const unsigned char * FZ_RESTRICT ht_line, const unsigned char * FZ_RESTRICT pixmap, unsigned char * FZ_RESTRICT out, int w, int ht_len)
__attribute__((naked));

static void
do_threshold_4(const unsigned char * FZ_RESTRICT ht_line, const unsigned char * FZ_RESTRICT pixmap, unsigned char * FZ_RESTRICT out, int w, int ht_len)
{
	asm volatile(
	ENTER_ARM
	// Store one more reg that required to keep double stack alignment
	"stmfd	r13!,{r4-r7,r9,r14}				\n"
	"@ r0 = ht_line						\n"
	"@ r1 = pixmap						\n"
	"@ r2 = out						\n"
	"@ r3 = w						\n"
	"@ <> = ht_len						\n"
	"ldr	r9, [r13,#6*4]		@ r9 = ht_len		\n"
	"subs	r3, r3, #1		@ r3 = w -= 1		\n"
	"ble	2f			@ while (w > 0) {	\n"
	"mov	r12,r9			@ r12= l = ht_len	\n"
	"b	1f			@			\n"
	"9:				@			\n"
	"strb	r14,[r2], #1		@ *out++ = h		\n"
	"subs	r12,r12,#2		@ r12 = l -= 2		\n"
	"moveq	r12,r9			@ if(l==0) l = ht_len	\n"
	"subeq	r0, r0, r9, LSL #2	@          ht_line -= l	\n"
	"subs	r3, r3, #2		@ w -= 2		\n"
	"beq	2f			@ }			\n"
	"blt	3f			@			\n"
	"1:							\n"
	"ldr	r5, [r1], #4		@ r5 = pixmap[0..3]	\n"
	"ldr	r7, [r1], #4		@ r7 = pixmap[4..7]	\n"
	"add	r0, r0, #8		@ r0 = ht_line += 8	\n"
	"mov	r14,#0			@ r14= h = 0		\n"
	"orrs	r5, r5, r7		@ if (r5 | r7 == 0)	\n"
	"beq	9b			@	white		\n"
	"ldrb	r4, [r0, #-8]		@ r4 = ht_line[0]	\n"
	"ldrb	r5, [r1, #-8]		@ r5 = pixmap[0]	\n"
	"ldrb	r6, [r0, #-7]		@ r6 = ht_line[1]	\n"
	"ldrb	r7, [r1, #-7]		@ r7 = pixmap[1]	\n"
	"cmp	r4, r5			@ if (r4 < r5)		\n"
	"orrle	r14,r14,#0x80		@	h |= 0x80	\n"
	"ldrb	r4, [r0, #-6]		@ r4 = ht_line[2]	\n"
	"ldrb	r5, [r1, #-6]		@ r5 = pixmap[2]	\n"
	"cmp	r6, r7			@ if (r6 < r7)		\n"
	"orrle	r14,r14,#0x40		@	h |= 0x40	\n"
	"ldrb	r6, [r0, #-5]		@ r6 = ht_line[3]	\n"
	"ldrb	r7, [r1, #-5]		@ r7 = pixmap[3]	\n"
	"cmp	r4, r5			@ if (r4 < r5)		\n"
	"orrle	r14,r14,#0x20		@	h |= 0x20	\n"
	"ldrb	r4, [r0, #-4]		@ r4 = ht_line[4]	\n"
	"ldrb	r5, [r1, #-4]		@ r5 = pixmap[4]	\n"
	"cmp	r6, r7			@ if (r6 < r7)		\n"
	"orrle	r14,r14,#0x10		@	h |= 0x10	\n"
	"ldrb	r6, [r0, #-3]		@ r6 = ht_line[5]	\n"
	"ldrb	r7, [r1, #-3]		@ r7 = pixmap[5]	\n"
	"cmp	r4, r5			@ if (r4 < r5)		\n"
	"orrle	r14,r14,#0x08		@	h |= 0x08	\n"
	"ldrb	r4, [r0, #-2]		@ r4 = ht_line[6]	\n"
	"ldrb	r5, [r1, #-2]		@ r5 = pixmap[6]	\n"
	"cmp	r6, r7			@ if (r6 < r7)		\n"
	"orrle	r14,r14,#0x04		@	h |= 0x04	\n"
	"ldrb	r6, [r0, #-1]		@ r6 = ht_line[7]	\n"
	"ldrb	r7, [r1, #-1]		@ r7 = pixmap[7]	\n"
	"cmp	r4, r5			@ if (r4 < r5)		\n"
	"orrle	r14,r14,#0x02		@	h |= 0x02	\n"
	"cmp	r6, r7			@ if (r7 < r6)		\n"
	"orrle	r14,r14,#0x01		@	h |= 0x01	\n"
	"subs	r12,r12,#2		@ r12 = l -= 2		\n"
	"strb	r14,[r2], #1		@ *out++ = h		\n"
	"moveq	r12,r9			@ if(l==0) l = ht_len	\n"
	"subeq	r0, r0, r9, LSL #2	@          ht_line -= l	\n"
	"subs	r3, r3, #2		@ w -= 2		\n"
	"bgt	1b			@ }			\n"
	"blt	3f			@			\n"
	"2:							\n"
	"ldrb	r4, [r0], #1		@ r4 = ht_line[0]	\n"
	"ldrb	r5, [r1], #1		@ r5 = pixmap[0]	\n"
	"mov	r14, #0			@ r14= h = 0		\n"
	"ldrb	r6, [r0], #1		@ r6 = ht_line[1]	\n"
	"ldrb	r7, [r1], #1		@ r7 = pixmap[1]	\n"
	"cmp	r4, r5			@ if (r4 < r5)		\n"
	"orrle	r14,r14,#0x80		@	h |= 0x80	\n"
	"ldrb	r4, [r0], #1		@ r6 = ht_line[2]	\n"
	"ldrb	r5, [r1], #1		@ r7 = pixmap[2]	\n"
	"cmp	r6, r7			@ if (r6 < r7)		\n"
	"orrle	r14,r14,#0x40		@	h |= 0x40	\n"
	"ldrb	r6, [r0], #1		@ r6 = ht_line[1]	\n"
	"ldrb	r7, [r1], #1		@ r7 = pixmap[3]	\n"
	"cmp	r4, r5			@ if (r4 < r5)		\n"
	"orrle	r14,r14,#0x20		@	h |= 0x20	\n"
	"cmp	r6, r7			@ if (r6 < r7)		\n"
	"orrle	r14,r14,#0x10		@	h |= 0x10	\n"
	"strb	r14,[r2]		@ *out = h		\n"
	"3:							\n"
	"ldmfd	r13!,{r4-r7,r9,PC}	@ pop, return to thumb	\n"
	ENTER_THUMB
	);
}
#else
static void do_threshold_4(const unsigned char * FZ_RESTRICT ht_line, const unsigned char * FZ_RESTRICT pixmap, unsigned char * FZ_RESTRICT out, int w, int ht_len)
{
	int l = ht_len;

	w--;
	while (w > 0)
	{
		int h = 0;
		if (pixmap[0] >= ht_line[0])
			h |= 0x80;
		if (pixmap[1] >= ht_line[1])
			h |= 0x40;
		if (pixmap[2] >= ht_line[2])
			h |= 0x20;
		if (pixmap[3] >= ht_line[3])
			h |= 0x10;
		if (pixmap[4] >= ht_line[4])
			h |= 0x08;
		if (pixmap[5] >= ht_line[5])
			h |= 0x04;
		if (pixmap[6] >= ht_line[6])
			h |= 0x02;
		if (pixmap[7] >= ht_line[7])
			h |= 0x01;
		*out++ = h;
		l -= 2;
		if (l == 0)
		{
			l = ht_len;
			ht_line -= ht_len<<2;
		}
		pixmap += 8;
		ht_line += 8;
		w -= 2;
	}
	if (w == 0)
	{
		int h = 0;
		if (pixmap[0] >= ht_line[0])
			h |= 0x80;
		if (pixmap[1] >= ht_line[1])
			h |= 0x40;
		if (pixmap[2] >= ht_line[2])
			h |= 0x20;
		if (pixmap[3] >= ht_line[3])
			h |= 0x10;
		*out = h;
	}
}
#endif

/*
	Make a bitmap from a pixmap and a halftone.

	pix: The pixmap to generate from. Currently must be a single color
	component with no alpha.

	ht: The halftone to use. NULL implies the default halftone.

	Returns the resultant bitmap. Throws exceptions in the case of
	failure to allocate.
*/
fz_bitmap *fz_new_bitmap_from_pixmap(fz_context *ctx, fz_pixmap *pix, fz_halftone *ht)
{
	return fz_new_bitmap_from_pixmap_band(ctx, pix, ht, 0);
}

/* TAOCP, vol 2, p337 */
static int gcd(int u, int v)
{
	int r;

	do
	{
		if (v == 0)
			return u;
		r = u % v;
		u = v;
		v = r;
	}
	while (1);
}

/*
	Make a bitmap from a pixmap and a
	halftone, allowing for the position of the pixmap within an
	overall banded rendering.

	pix: The pixmap to generate from. Currently must be a single color
	component with no alpha.

	ht: The halftone to use. NULL implies the default halftone.

	band_start: Vertical offset within the overall banded rendering
	(in pixels)

	Returns the resultant bitmap. Throws exceptions in the case of
	failure to allocate.
*/
fz_bitmap *fz_new_bitmap_from_pixmap_band(fz_context *ctx, fz_pixmap *pix, fz_halftone *ht, int band_start)
{
	fz_bitmap *out = NULL;
	unsigned char *ht_line = NULL;
	unsigned char *o, *p;
	int w, h, x, y, n, pstride, ostride, lcm, i;
	fz_halftone *ht_ = NULL;
	threshold_fn *thresh;

	fz_var(ht_line);

	if (!pix)
		return NULL;

	if (pix->alpha != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap may not have alpha channel to convert to bitmap");

	n = pix->n;

	switch(n)
	{
	case 1:
		thresh = do_threshold_1;
		break;
	case 4:
		thresh = do_threshold_4;
		break;
	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or CMYK to convert to bitmap");
		return NULL;
	}

	if (ht == NULL)
		ht_ = ht = fz_default_halftone(ctx, n);

	/* Find the minimum length for the halftone line. This
	 * is the LCM of the halftone lengths and 8. (We need a
	 * multiple of 8 for the unrolled threshold routines - if
	 * we ever use SSE, we may need longer.) We use the fact
	 * that LCM(a,b) = a * b / GCD(a,b) and use euclids
	 * algorithm.
	 */
	lcm = 8;
	for (i = 0; i < ht->n; i++)
	{
		w = ht->comp[i]->w;
		lcm = lcm / gcd(lcm, w) * w;
	}

	fz_try(ctx)
	{
		ht_line = fz_malloc(ctx, lcm * n);
		out = fz_new_bitmap(ctx, pix->w, pix->h, n, pix->xres, pix->yres);
		o = out->samples;
		p = pix->samples;

		h = pix->h;
		x = pix->x;
		y = pix->y + band_start;
		w = pix->w;
		ostride = out->stride;
		pstride = pix->stride;
		while (h--)
		{
			make_ht_line(ht_line, ht, x, y++, lcm);
			thresh(ht_line, p, o, w, lcm);
			o += ostride;
			p += pstride;
		}
	}
	fz_always(ctx)
	{
		fz_drop_halftone(ctx, ht_);
		fz_free(ctx, ht_line);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return out;
}
