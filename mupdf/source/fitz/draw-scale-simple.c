/*
This code does smooth scaling of a pixmap.

This function returns a new pixmap representing the area starting at (0,0)
given by taking the source pixmap src, scaling it to width w, and height h,
and then positioning it at (frac(x),frac(y)).

This is a cut-down version of draw_scale.c that only copes with filters
that return values strictly in the 0..1 range, and uses bytes for
intermediate results rather than ints.
*/

#include "mupdf/fitz.h"
#include "draw-imp.h"

#include <math.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

/* Do we special case handling of single pixel high/wide images? The
 * 'purest' handling is given by not special casing them, but certain
 * files that use such images 'stack' them to give full images. Not
 * special casing them results in then being fainter and giving noticeable
 * rounding errors.
 */
#define SINGLE_PIXEL_SPECIALS

/*
Consider a row of source samples, src, of width src_w, positioned at x,
scaled to width dst_w.

src[i] is centred at: x + (i + 0.5)*dst_w/src_w

Therefore the distance between the centre of the jth output pixel and
the centre of the ith source sample is:

dist[j,i] = j + 0.5 - (x + (i + 0.5)*dst_w/src_w)

When scaling up, therefore:

dst[j] = SUM(filter(dist[j,i]) * src[i])
	(for all ints i)

This can be simplified by noticing that filters are only non zero within
a given filter width (henceforth called W). So:

dst[j] = SUM(filter(dist[j,i]) * src[i])
	(for ints i, s.t. (j*src_w/dst_w)-W < i < (j*src_w/dst_w)+W)

When scaling down, each filtered source sample is stretched to be wider
to avoid aliasing issues. This effectively reduces the distance between
centres.

dst[j] = SUM(filter(dist[j,i] * F) * F * src[i])
	(where F = dst_w/src_w)
	(for ints i, s.t. (j-W)/F < i < (j+W)/F)

*/

typedef struct fz_scale_filter_s fz_scale_filter;

struct fz_scale_filter_s
{
	int width;
	float (*fn)(fz_scale_filter *, float);
};

/* Image scale filters */

static float
triangle(fz_scale_filter *filter, float f)
{
	if (f >= 1)
		return 0;
	return 1-f;
}

static float
box(fz_scale_filter *filter, float f)
{
	if (f >= 0.5f)
		return 0;
	return 1;
}

static float
simple(fz_scale_filter *filter, float x)
{
	if (x >= 1)
		return 0;
	return 1 + (2*x - 3)*x*x;
}

fz_scale_filter fz_scale_filter_box = { 1, box };
fz_scale_filter fz_scale_filter_triangle = { 1, triangle };
fz_scale_filter fz_scale_filter_simple = { 1, simple };

/*
We build ourselves a set of tables to contain the precalculated weights
for a given set of scale settings.

The first dst_w entries in index are the index into index of the
sets of weight for each destination pixel.

Each of the sets of weights is a set of values consisting of:
	the minimum source pixel index used for this destination pixel
	the number of weights used for this destination pixel
	the weights themselves

So to calculate dst[i] we do the following:

	weights = &index[index[i]];
	min = *weights++;
	len = *weights++;
	dst[i] = 0;
	while (--len > 0)
		dst[i] += src[min++] * *weights++

in addition, we guarantee that at the end of this process weights will now
point to the weights value for dst pixel i+1.

In the simplest version of this algorithm, we would scale the whole image
horizontally first into a temporary buffer, then scale that temporary
buffer again vertically to give us our result. Using such a simple
algorithm would mean that could use the same style of weights for both
horizontal and vertical scaling.

Unfortunately, this would also require a large temporary buffer,
particularly in the case where we are scaling up.

We therefore modify the algorithm as follows; we scale scanlines from the
source image horizontally into a temporary buffer, until we have all the
contributors for a given output scanline. We then produce that output
scanline from the temporary buffer. In this way we restrict the height
of the temporary buffer to a small fraction of the final size.

Unfortunately, this means that the pseudo code for recombining a
scanline of fully scaled pixels is as follows:

	weights = &index[index[y]];
	min = *weights++;
	len = *weights++;
	for (x=0 to dst_w)
		min2 = min
		len2 = len
		weights2 = weights
		dst[x] = 0;
		while (--len2 > 0)
			dst[x] += temp[x][(min2++) % tmp_buf_height] * *weights2++

i.e. it requires a % operation for every source pixel - this is typically
expensive.

To avoid this, we alter the order in which vertical weights are stored,
so that they are ordered in the same order as the temporary buffer lines
would appear. This simplifies the algorithm to:

	weights = &index[index[y]];
	min = *weights++;
	len = *weights++;
	for (x=0 to dst_w)
		min2 = 0
		len2 = len
		weights2 = weights
		dst[x] = 0;
		while (--len2 > 0)
			dst[x] += temp[i][min2++] * *weights2++

This means that len may be larger than it needs to be (due to the
possible inclusion of a zero weight row or two), but in practise this
is only an increase of 1 or 2 at worst.

We implement this by generating the weights as normal (but ensuring we
leave enough space) and then reordering afterwards.

*/

typedef struct fz_weights_s fz_weights;

/* This structure is accessed from ARM code - bear this in mind before
 * altering it! */
struct fz_weights_s
{
	int flip;	/* true if outputting reversed */
	int count;	/* number of output pixels we have records for in this table */
	int max_len;	/* Maximum number of weights for any one output pixel */
	int n;		/* number of components (src->n) */
	int new_line;	/* True if no weights for the current output pixel */
	int patch_l;	/* How many output pixels we skip over */
	int index[1];
};

struct fz_scale_cache_s
{
	int src_w;
	float x;
	float dst_w;
	fz_scale_filter *filter;
	int vertical;
	int dst_w_int;
	int patch_l;
	int patch_r;
	int n;
	int flip;
	fz_weights *weights;
};

static fz_weights *
new_weights(fz_context *ctx, fz_scale_filter *filter, int src_w, float dst_w, int patch_w, int n, int flip, int patch_l)
{
	int max_len;
	fz_weights *weights;

	if (src_w > dst_w)
	{
		/* Scaling down, so there will be a maximum of
		 * 2*filterwidth*src_w/dst_w src pixels
		 * contributing to each dst pixel. */
		max_len = (int)ceilf((2 * filter->width * src_w)/dst_w);
		if (max_len > src_w)
			max_len = src_w;
	}
	else
	{
		/* Scaling up, so there will be a maximum of
		 * 2*filterwidth src pixels contributing to each dst pixel.
		 */
		max_len = 2 * filter->width;
	}
	/* We need the size of the struct,
	 * plus patch_w*sizeof(int) for the index
	 * plus (2+max_len)*sizeof(int) for the weights
	 * plus room for an extra set of weights for reordering.
	 */
	weights = fz_malloc(ctx, sizeof(*weights)+(max_len+3)*(patch_w+1)*sizeof(int));
	if (!weights)
		return NULL;
	weights->count = -1;
	weights->max_len = max_len;
	weights->index[0] = patch_w;
	weights->n = n;
	weights->patch_l = patch_l;
	weights->flip = flip;
	return weights;
}

/* j is destination pixel in the patch_l..patch_l+patch_w range */
static void
init_weights(fz_weights *weights, int j)
{
	int index;

	j -= weights->patch_l;
	assert(weights->count == j-1);
	weights->count++;
	weights->new_line = 1;
	if (j == 0)
		index = weights->index[0];
	else
	{
		index = weights->index[j-1];
		index += 2 + weights->index[index+1];
	}
	weights->index[j] = index; /* row pointer */
	weights->index[index] = 0; /* min */
	weights->index[index+1] = 0; /* len */
}

static void
add_weight(fz_weights *weights, int j, int i, fz_scale_filter *filter,
	float x, float F, float G, int src_w, float dst_w)
{
	float dist = j - x + 0.5f - ((i + 0.5f)*dst_w/src_w);
	float f;
	int min, len, index, weight;

	dist *= G;
	if (dist < 0)
		dist = -dist;
	f = filter->fn(filter, dist)*F;
	weight = (int)(256*f+0.5f);

	/* Ensure i is in range */
	if (i < 0 || i >= src_w)
		return;
	if (weight == 0)
	{
		/* We add a fudge factor here to allow for extreme downscales
		 * where all the weights round to 0. Ensure that at least one
		 * (arbitrarily the first one) is non zero. */
		if (weights->new_line && f > 0)
			weight = 1;
		else
			return;
	}

	/* Move j from patch_l...patch_l+patch_w range to 0..patch_w range */
	j -= weights->patch_l;
	if (weights->new_line)
	{
		/* New line */
		weights->new_line = 0;
		index = weights->index[j]; /* row pointer */
		weights->index[index] = i; /* min */
		weights->index[index+1] = 0; /* len */
	}
	index = weights->index[j];
	min = weights->index[index++];
	len = weights->index[index++];
	while (i < min)
	{
		/* This only happens in rare cases, but we need to insert
		 * one earlier. In exceedingly rare cases we may need to
		 * insert more than one earlier. */
		int k;

		for (k = len; k > 0; k--)
		{
			weights->index[index+k] = weights->index[index+k-1];
		}
		weights->index[index] = 0;
		min--;
		len++;
		weights->index[index-2] = min;
		weights->index[index-1] = len;
	}
	if (i-min >= len)
	{
		/* The usual case */
		while (i-min >= ++len)
		{
			weights->index[index+len-1] = 0;
		}
		assert(len-1 == i-min);
		weights->index[index+i-min] = weight;
		weights->index[index-1] = len;
		assert(len <= weights->max_len);
	}
	else
	{
		/* Infrequent case */
		weights->index[index+i-min] += weight;
	}
}

static void
reorder_weights(fz_weights *weights, int j, int src_w)
{
	int idx = weights->index[j - weights->patch_l];
	int min = weights->index[idx++];
	int len = weights->index[idx++];
	int max = weights->max_len;
	int tmp = idx+max;
	int i, off;

	/* Copy into the temporary area */
	memcpy(&weights->index[tmp], &weights->index[idx], sizeof(int)*len);

	/* Pad out if required */
	assert(len <= max);
	assert(min+len <= src_w);
	off = 0;
	if (len < max)
	{
		memset(&weights->index[tmp+len], 0, sizeof(int)*(max-len));
		len = max;
		if (min + len > src_w)
		{
			off = min + len - src_w;
			min = src_w - len;
			weights->index[idx-2] = min;
		}
		weights->index[idx-1] = len;
	}

	/* Copy back into the proper places */
	for (i = 0; i < len; i++)
	{
		weights->index[idx+((min+i+off) % max)] = weights->index[tmp+i];
	}
}

/* Due to rounding and edge effects, the sums for the weights sometimes don't
 * add up to 256. This causes visible rendering effects. Therefore, we take
 * pains to ensure that they 1) never exceed 256, and 2) add up to exactly
 * 256 for all pixels that are completely covered. See bug #691629. */
static void
check_weights(fz_weights *weights, int j, int w, float x, float wf)
{
	int idx, len;
	int sum = 0;
	int max = -256;
	int maxidx = 0;
	int i;

	idx = weights->index[j - weights->patch_l];
	idx++; /* min */
	len = weights->index[idx++];

	for(i=0; i < len; i++)
	{
		int v = weights->index[idx++];
		sum += v;
		if (v > max)
		{
			max = v;
			maxidx = idx;
		}
	}
	/* If we aren't the first or last pixel, OR if the sum is too big
	 * then adjust it. */
	if (((j != 0) && (j != w-1)) || (sum > 256))
		weights->index[maxidx-1] += 256-sum;
	/* Otherwise, if we are the first pixel, and it's fully covered, then
	 * adjust it. */
	else if ((j == 0) && (x < 0.0001f) && (sum != 256))
		weights->index[maxidx-1] += 256-sum;
	/* Finally, if we are the last pixel, and it's fully covered, then
	 * adjust it. */
	else if ((j == w-1) && (w - wf < 0.0001f) && (sum != 256))
		weights->index[maxidx-1] += 256-sum;
}

static fz_weights *
make_weights(fz_context *ctx, int src_w, float x, float dst_w, fz_scale_filter *filter, int vertical, int dst_w_int, int patch_l, int patch_r, int n, int flip, fz_scale_cache *cache)
{
	fz_weights *weights;
	float F, G;
	float window;
	int j;

	if (cache)
	{
		if (cache->src_w == src_w && cache->x == x && cache->dst_w == dst_w &&
			cache->filter == filter && cache->vertical == vertical &&
			cache->dst_w_int == dst_w_int &&
			cache->patch_l == patch_l && cache->patch_r == patch_r &&
			cache->n == n && cache->flip == flip)
		{
			return cache->weights;
		}
		cache->src_w = src_w;
		cache->x = x;
		cache->dst_w = dst_w;
		cache->filter = filter;
		cache->vertical = vertical;
		cache->dst_w_int = dst_w_int;
		cache->patch_l = patch_l;
		cache->patch_r = patch_r;
		cache->n = n;
		cache->flip = flip;
		fz_free(ctx, cache->weights);
		cache->weights = NULL;
	}

	if (dst_w < src_w)
	{
		/* Scaling down */
		F = dst_w / src_w;
		G = 1;
	}
	else
	{
		/* Scaling up */
		F = 1;
		G = src_w / dst_w;
	}
	window = filter->width / F;
	weights	= new_weights(ctx, filter, src_w, dst_w, patch_r-patch_l, n, flip, patch_l);
	if (!weights)
		return NULL;
	for (j = patch_l; j < patch_r; j++)
	{
		/* find the position of the centre of dst[j] in src space */
		float centre = (j - x + 0.5f)*src_w/dst_w - 0.5f;
		int l, r;
		l = ceilf(centre - window);
		r = floorf(centre + window);
		init_weights(weights, j);
		for (; l <= r; l++)
		{
			add_weight(weights, j, l, filter, x, F, G, src_w, dst_w);
		}
		check_weights(weights, j, dst_w_int, x, dst_w);
		if (vertical)
		{
			reorder_weights(weights, j, src_w);
		}
	}
	weights->count++; /* weights->count = dst_w_int now */
	if (cache)
	{
		cache->weights = weights;
	}
	return weights;
}

static void
scale_row_to_temp(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	const int *contrib = &weights->index[weights->index[0]];
	int len, i, j, n;
	const unsigned char *min;
	int tmp[FZ_MAX_COLORS];
	int *t = tmp;

	n = weights->n;
	for (j = 0; j < n; j++)
		tmp[j] = 128;
	if (weights->flip)
	{
		dst += (weights->count-1)*n;
		for (i=weights->count; i > 0; i--)
		{
			min = &src[n * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				for (j = n; j > 0; j--)
					*t++ += *min++ * *contrib;
				t -= n;
				contrib++;
			}
			for (j = n; j > 0; j--)
			{
				*dst++ = (unsigned char)(*t>>8);
				*t++ = 128;
			}
			t -= n;
			dst -= n*2;
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			min = &src[n * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				for (j = n; j > 0; j--)
					*t++ += *min++ * *contrib;
				t -= n;
				contrib++;
			}
			for (j = n; j > 0; j--)
			{
				*dst++ = (unsigned char)(*t>>8);
				*t++ = 128;
			}
			t -= n;
		}
	}
}

#ifdef ARCH_ARM

static void
scale_row_to_temp1(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
__attribute__((naked));

static void
scale_row_to_temp2(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
__attribute__((naked));

static void
scale_row_to_temp3(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
__attribute__((naked));

static void
scale_row_to_temp4(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
__attribute__((naked));

static void
scale_row_from_temp(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights, int width, int n, int row)
__attribute__((naked));

static void
scale_row_from_temp_alpha(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights, int width, int n, int row)
__attribute__((naked));

static void
scale_row_to_temp1(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	asm volatile(
	ENTER_ARM
    ".syntax unified\n"
	"stmfd	r13!,{r4-r7,r9,r14}				\n"
	"@ r0 = dst						\n"
	"@ r1 = src						\n"
	"@ r2 = weights						\n"
	"ldr	r12,[r2],#4		@ r12= flip		\n"
	"ldr	r3, [r2],#20		@ r3 = count r2 = &index\n"
	"ldr	r4, [r2]		@ r4 = index[0]		\n"
	"cmp	r12,#0			@ if (flip)		\n"
	"beq	5f			@ {			\n"
	"add	r2, r2, r4, LSL #2	@ r2 = &index[index[0]] \n"
	"add	r0, r0, r3		@ dst += count		\n"
	"1:							\n"
	"ldr	r4, [r2], #4		@ r4 = *contrib++	\n"
	"ldr	r9, [r2], #4		@ r9 = len = *contrib++	\n"
	"mov	r5, #128		@ r5 = a = 128		\n"
	"add	r4, r1, r4		@ r4 = min = &src[r4]	\n"
	"subs	r9, r9, #1		@ len--			\n"
	"blt	3f			@ while (len >= 0)	\n"
	"2:				@ {			\n"
	"ldrgt	r6, [r2], #4		@ r6 = *contrib++	\n"
	"ldrbgt	r7, [r4], #1		@ r7 = *min++		\n"
	"ldr	r12,[r2], #4		@ r12 = *contrib++	\n"
	"ldrb	r14,[r4], #1		@ r14 = *min++		\n"
	"mlagt	r5, r6, r7, r5		@ g += r6 * r7		\n"
	"subs	r9, r9, #2		@ r9 = len -= 2		\n"
	"mla	r5, r12,r14,r5		@ g += r14 * r12	\n"
	"bge	2b			@ }			\n"
	"3:							\n"
	"mov	r5, r5, lsr #8		@ g >>= 8		\n"
	"strb	r5,[r0, #-1]!		@ *--dst=a		\n"
	"subs	r3, r3, #1		@ i--			\n"
	"bgt	1b			@ 			\n"
	"ldmfd	r13!,{r4-r7,r9,PC}	@ pop, return to thumb	\n"
	"5:"
	"add	r2, r2, r4, LSL #2	@ r2 = &index[index[0]] \n"
	"6:"
	"ldr	r4, [r2], #4		@ r4 = *contrib++	\n"
	"ldr	r9, [r2], #4		@ r9 = len = *contrib++	\n"
	"mov	r5, #128		@ r5 = a = 128		\n"
	"add	r4, r1, r4		@ r4 = min = &src[r4]	\n"
	"subs	r9, r9, #1		@ len--			\n"
	"blt	9f			@ while (len > 0)	\n"
	"7:				@ {			\n"
	"ldrgt	r6, [r2], #4		@ r6 = *contrib++	\n"
	"ldrbgt	r7, [r4], #1		@ r7 = *min++		\n"
	"ldr	r12,[r2], #4		@ r12 = *contrib++	\n"
	"ldrb	r14,[r4], #1		@ r14 = *min++		\n"
	"mlagt	r5, r6,r7,r5		@ a += r6 * r7		\n"
	"subs	r9, r9, #2		@ r9 = len -= 2		\n"
	"mla	r5, r12,r14,r5		@ a += r14 * r12	\n"
	"bge	7b			@ }			\n"
	"9:							\n"
	"mov	r5, r5, LSR #8		@ a >>= 8		\n"
	"strb	r5, [r0], #1		@ *dst++=a		\n"
	"subs	r3, r3, #1		@ i--			\n"
	"bgt	6b			@ 			\n"
	"ldmfd	r13!,{r4-r7,r9,PC}	@ pop, return to thumb	\n"
	ENTER_THUMB
	);
}

static void
scale_row_to_temp2(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r4-r6,r9-r11,r14}				\n"
	"@ r0 = dst						\n"
	"@ r1 = src						\n"
	"@ r2 = weights						\n"
	"ldr	r12,[r2],#4		@ r12= flip		\n"
	"ldr	r3, [r2],#20		@ r3 = count r2 = &index\n"
	"ldr	r4, [r2]		@ r4 = index[0]		\n"
	"cmp	r12,#0			@ if (flip)		\n"
	"beq	4f			@ {			\n"
	"add	r2, r2, r4, LSL #2	@ r2 = &index[index[0]] \n"
	"add	r0, r0, r3, LSL #1	@ dst += 2*count	\n"
	"1:							\n"
	"ldr	r4, [r2], #4		@ r4 = *contrib++	\n"
	"ldr	r9, [r2], #4		@ r9 = len = *contrib++	\n"
	"mov	r5, #128		@ r5 = g = 128		\n"
	"mov	r6, #128		@ r6 = a = 128		\n"
	"add	r4, r1, r4, LSL #1	@ r4 = min = &src[2*r4]	\n"
	"cmp	r9, #0			@ while (len-- > 0)	\n"
	"beq	3f			@ {			\n"
	"2:							\n"
	"ldr	r14,[r2], #4		@ r14 = *contrib++	\n"
	"ldrb	r11,[r4], #1		@ r11 = *min++		\n"
	"ldrb	r12,[r4], #1		@ r12 = *min++		\n"
	"subs	r9, r9, #1		@ r9 = len--		\n"
	"mla	r5, r14,r11,r5		@ g += r11 * r14	\n"
	"mla	r6, r14,r12,r6		@ a += r12 * r14	\n"
	"bgt	2b			@ }			\n"
	"3:							\n"
	"mov	r5, r5, lsr #8		@ g >>= 8		\n"
	"mov	r6, r6, lsr #8		@ a >>= 8		\n"
	"strb	r5, [r0, #-2]!		@ *--dst=a		\n"
	"strb	r6, [r0, #1]		@ *--dst=g		\n"
	"subs	r3, r3, #1		@ i--			\n"
	"bgt	1b			@ 			\n"
	"ldmfd	r13!,{r4-r6,r9-r11,PC}	@ pop, return to thumb	\n"
	"4:"
	"add	r2, r2, r4, LSL #2	@ r2 = &index[index[0]] \n"
	"5:"
	"ldr	r4, [r2], #4		@ r4 = *contrib++	\n"
	"ldr	r9, [r2], #4		@ r9 = len = *contrib++	\n"
	"mov	r5, #128		@ r5 = g = 128		\n"
	"mov	r6, #128		@ r6 = a = 128		\n"
	"add	r4, r1, r4, LSL #1	@ r4 = min = &src[2*r4]	\n"
	"cmp	r9, #0			@ while (len-- > 0)	\n"
	"beq	7f			@ {			\n"
	"6:							\n"
	"ldr	r14,[r2], #4		@ r10 = *contrib++	\n"
	"ldrb	r11,[r4], #1		@ r11 = *min++		\n"
	"ldrb	r12,[r4], #1		@ r12 = *min++		\n"
	"subs	r9, r9, #1		@ r9 = len--		\n"
	"mla	r5, r14,r11,r5		@ g += r11 * r14	\n"
	"mla	r6, r14,r12,r6		@ a += r12 * r14	\n"
	"bgt	6b			@ }			\n"
	"7:							\n"
	"mov	r5, r5, lsr #8		@ g >>= 8		\n"
	"mov	r6, r6, lsr #8		@ a >>= 8		\n"
	"strb	r5, [r0], #1		@ *dst++=g		\n"
	"strb	r6, [r0], #1		@ *dst++=a		\n"
	"subs	r3, r3, #1		@ i--			\n"
	"bgt	5b			@ 			\n"
	"ldmfd	r13!,{r4-r6,r9-r11,PC}	@ pop, return to thumb	\n"
	ENTER_THUMB
	);
}

static void
scale_row_to_temp3(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r4-r11,r14}				\n"
	"@ r0 = dst						\n"
	"@ r1 = src						\n"
	"@ r2 = weights						\n"
	"ldr	r12,[r2],#4		@ r12= flip		\n"
	"ldr	r3, [r2],#20		@ r3 = count r2 = &index\n"
	"ldr	r4, [r2]		@ r4 = index[0]		\n"
	"cmp	r12,#0			@ if (flip)		\n"
	"beq	4f			@ {			\n"
	"add	r2, r2, r4, LSL #2	@ r2 = &index[index[0]]	\n"
	"add	r0, r0, r3, LSL #1	@			\n"
	"add	r0, r0, r3		@ dst += 3*count	\n"
	"1:							\n"
	"ldr	r4, [r2], #4		@ r4 = *contrib++	\n"
	"ldr	r9, [r2], #4		@ r9 = len = *contrib++	\n"
	"mov	r5, #128		@ r5 = r = 128		\n"
	"mov	r6, #128		@ r6 = g = 128		\n"
	"add	r7, r1, r4, LSL #1	@			\n"
	"add	r4, r7, r4		@ r4 = min = &src[3*r4]	\n"
	"mov	r7, #128		@ r7 = b = 128		\n"
	"cmp	r9, #0			@ while (len-- > 0)	\n"
	"beq	3f			@ {			\n"
	"2:							\n"
	"ldr	r14,[r2], #4		@ r14 = *contrib++	\n"
	"ldrb	r8, [r4], #1		@ r8  = *min++		\n"
	"ldrb	r11,[r4], #1		@ r11 = *min++		\n"
	"ldrb	r12,[r4], #1		@ r12 = *min++		\n"
	"subs	r9, r9, #1		@ r9 = len--		\n"
	"mla	r5, r14,r8, r5		@ r += r8  * r14	\n"
	"mla	r6, r14,r11,r6		@ g += r11 * r14	\n"
	"mla	r7, r14,r12,r7		@ b += r12 * r14	\n"
	"bgt	2b			@ }			\n"
	"3:							\n"
	"mov	r5, r5, lsr #8		@ r >>= 8		\n"
	"mov	r6, r6, lsr #8		@ g >>= 8		\n"
	"mov	r7, r7, lsr #8		@ b >>= 8		\n"
	"strb	r5, [r0, #-3]!		@ *--dst=r		\n"
	"strb	r6, [r0, #1]		@ *--dst=g		\n"
	"strb	r7, [r0, #2]		@ *--dst=b		\n"
	"subs	r3, r3, #1		@ i--			\n"
	"bgt	1b			@ 			\n"
	"ldmfd	r13!,{r4-r11,PC}	@ pop, return to thumb	\n"
	"4:"
	"add	r2, r2, r4, LSL #2	@ r2 = &index[index[0]]	\n"
	"5:"
	"ldr	r4, [r2], #4		@ r4 = *contrib++	\n"
	"ldr	r9, [r2], #4		@ r9 = len = *contrib++	\n"
	"mov	r5, #128		@ r5 = r = 128		\n"
	"mov	r6, #128		@ r6 = g = 128		\n"
	"add	r7, r1, r4, LSL #1	@ r7 = min = &src[2*r4]	\n"
	"add	r4, r7, r4		@ r4 = min = &src[3*r4]	\n"
	"mov	r7, #128		@ r7 = b = 128		\n"
	"cmp	r9, #0			@ while (len-- > 0)	\n"
	"beq	7f			@ {			\n"
	"6:							\n"
	"ldr	r14,[r2], #4		@ r10 = *contrib++	\n"
	"ldrb	r8, [r4], #1		@ r8  = *min++		\n"
	"ldrb	r11,[r4], #1		@ r11 = *min++		\n"
	"ldrb	r12,[r4], #1		@ r12 = *min++		\n"
	"subs	r9, r9, #1		@ r9 = len--		\n"
	"mla	r5, r14,r8, r5		@ r += r8  * r14	\n"
	"mla	r6, r14,r11,r6		@ g += r11 * r14	\n"
	"mla	r7, r14,r12,r7		@ b += r12 * r14	\n"
	"bgt	6b			@ }			\n"
	"7:							\n"
	"mov	r5, r5, lsr #8		@ r >>= 8		\n"
	"mov	r6, r6, lsr #8		@ g >>= 8		\n"
	"mov	r7, r7, lsr #8		@ b >>= 8		\n"
	"strb	r5, [r0], #1		@ *dst++=r		\n"
	"strb	r6, [r0], #1		@ *dst++=g		\n"
	"strb	r7, [r0], #1		@ *dst++=b		\n"
	"subs	r3, r3, #1		@ i--			\n"
	"bgt	5b			@ 			\n"
	"ldmfd	r13!,{r4-r11,PC}	@ pop, return to thumb	\n"
	ENTER_THUMB
	);
}

static void
scale_row_to_temp4(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r4-r11,r14}				\n"
	"@ r0 = dst						\n"
	"@ r1 = src						\n"
	"@ r2 = weights						\n"
	"ldr	r12,[r2],#4		@ r12= flip		\n"
	"ldr	r3, [r2],#20		@ r3 = count r2 = &index\n"
	"ldr	r4, [r2]		@ r4 = index[0]		\n"
	"ldr	r5,=0x00800080		@ r5 = rounding		\n"
	"ldr	r6,=0x00FF00FF		@ r7 = 0x00FF00FF	\n"
	"cmp	r12,#0			@ if (flip)		\n"
	"beq	4f			@ {			\n"
	"add	r2, r2, r4, LSL #2	@ r2 = &index[index[0]] \n"
	"add	r0, r0, r3, LSL #2	@ dst += 4*count	\n"
	"1:							\n"
	"ldr	r4, [r2], #4		@ r4 = *contrib++	\n"
	"ldr	r9, [r2], #4		@ r9 = len = *contrib++	\n"
	"mov	r7, r5			@ r7 = b = rounding	\n"
	"mov	r8, r5			@ r8 = a = rounding	\n"
	"add	r4, r1, r4, LSL #2	@ r4 = min = &src[4*r4]	\n"
	"cmp	r9, #0			@ while (len-- > 0)	\n"
	"beq	3f			@ {			\n"
	"2:							\n"
	"ldr	r11,[r4], #4		@ r11 = *min++		\n"
	"ldr	r10,[r2], #4		@ r10 = *contrib++	\n"
	"subs	r9, r9, #1		@ r9 = len--		\n"
	"and	r12,r6, r11		@ r12 = __22__00	\n"
	"and	r11,r6, r11,LSR #8	@ r11 = __33__11	\n"
	"mla	r7, r10,r12,r7		@ b += r14 * r10	\n"
	"mla	r8, r10,r11,r8		@ a += r11 * r10	\n"
	"bgt	2b			@ }			\n"
	"3:							\n"
	"and	r7, r6, r7, lsr #8	@ r7 = __22__00		\n"
	"bic	r8, r8, r6		@ r8 = 33__11__		\n"
	"orr	r7, r7, r8		@ r7 = 33221100		\n"
	"str	r7, [r0, #-4]!		@ *--dst=r		\n"
	"subs	r3, r3, #1		@ i--			\n"
	"bgt	1b			@ 			\n"
	"ldmfd	r13!,{r4-r11,PC}	@ pop, return to thumb	\n"
	"4:							\n"
	"add	r2, r2, r4, LSL #2	@ r2 = &index[index[0]] \n"
	"5:							\n"
	"ldr	r4, [r2], #4		@ r4 = *contrib++	\n"
	"ldr	r9, [r2], #4		@ r9 = len = *contrib++	\n"
	"mov	r7, r5			@ r7 = b = rounding	\n"
	"mov	r8, r5			@ r8 = a = rounding	\n"
	"add	r4, r1, r4, LSL #2	@ r4 = min = &src[4*r4]	\n"
	"cmp	r9, #0			@ while (len-- > 0)	\n"
	"beq	7f			@ {			\n"
	"6:							\n"
	"ldr	r11,[r4], #4		@ r11 = *min++		\n"
	"ldr	r10,[r2], #4		@ r10 = *contrib++	\n"
	"subs	r9, r9, #1		@ r9 = len--		\n"
	"and	r12,r6, r11		@ r12 = __22__00	\n"
	"and	r11,r6, r11,LSR #8	@ r11 = __33__11	\n"
	"mla	r7, r10,r12,r7		@ b += r14 * r10	\n"
	"mla	r8, r10,r11,r8		@ a += r11 * r10	\n"
	"bgt	6b			@ }			\n"
	"7:							\n"
	"and	r7, r6, r7, lsr #8	@ r7 = __22__00		\n"
	"bic	r8, r8, r6		@ r8 = 33__11__		\n"
	"orr	r7, r7, r8		@ r7 = 33221100		\n"
	"str	r7, [r0], #4		@ *dst++=r		\n"
	"subs	r3, r3, #1		@ i--			\n"
	"bgt	5b			@ 			\n"
	"ldmfd	r13!,{r4-r11,PC}	@ pop, return to thumb	\n"
	ENTER_THUMB
	);
}

static void
scale_row_from_temp(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights, int width, int n, int row)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r4-r11,r14}				\n"
	"@ r0 = dst						\n"
	"@ r1 = src						\n"
	"@ r2 = &weights->index[0]				\n"
	"@ r3 = width						\n"
	"@ r12= row						\n"
	"ldr	r14,[r13,#4*9]		@ r14= n		\n"
	"ldr	r12,[r13,#4*10]		@ r12= row		\n"
	"add	r2, r2, #24		@ r2 = weights->index	\n"
	"mul    r3, r14, r3		@ r3 = width *= n       \n"
	"ldr	r4, [r2, r12, LSL #2]	@ r4 = index[row]	\n"
	"add	r2, r2, #4		@ r2 = &index[1]	\n"
	"subs	r6, r3, #4		@ r6 = x = width-4	\n"
	"ldr	r14,[r2, r4, LSL #2]!	@ r2 = contrib = index[index[row]+1]\n"
	"				@ r14= len = *contrib	\n"
	"blt	4f			@ while (x >= 0) {	\n"
#ifndef ARCH_UNALIGNED_OK
	"tst	r3, #3			@ if ((r3 & 3)		\n"
	"tsteq	r1, #3			@	|| (r1 & 3))	\n"
	"bne	4f			@ can't do fast code	\n"
#endif
	"ldr	r9, =0x00FF00FF		@ r9 = 0x00FF00FF	\n"
	"1:							\n"
	"ldr	r7, =0x00800080		@ r5 = val0 = round	\n"
	"stmfd	r13!,{r1,r2,r7}		@ stash r1,r2,r5	\n"
	"				@ r1 = min = src	\n"
	"				@ r2 = contrib2-4	\n"
	"movs	r8, r14			@ r8 = len2 = len	\n"
	"mov	r5, r7			@ r7 = val1 = round	\n"
	"ble	3f			@ while (len2-- > 0) {	\n"
	"2:							\n"
	"ldr	r12,[r1], r3		@ r12 = *min	r5 = min += width\n"
	"ldr	r10,[r2, #4]!		@ r10 = *contrib2++	\n"
	"subs	r8, r8, #1		@ len2--		\n"
	"and	r11,r9, r12		@ r11= __22__00		\n"
	"and	r12,r9, r12,LSR #8	@ r12= __33__11		\n"
	"mla	r5, r10,r11,r5		@ r5 = val0 += r11 * r10\n"
	"mla	r7, r10,r12,r7		@ r7 = val1 += r12 * r10\n"
	"bgt	2b			@ }			\n"
	"and	r5, r9, r5, LSR #8	@ r5 = __22__00		\n"
	"and	r7, r7, r9, LSL #8	@ r7 = 33__11__		\n"
	"orr	r5, r5, r7		@ r5 = 33221100		\n"
	"3:							\n"
	"ldmfd	r13!,{r1,r2,r7}		@ restore r1,r2,r7	\n"
	"subs	r6, r6, #4		@ x--			\n"
	"add	r1, r1, #4		@ src++			\n"
	"str	r5, [r0], #4		@ *dst++ = val		\n"
	"bge	1b			@ 			\n"
	"4:				@ } (Less than 4 to go)	\n"
	"adds	r6, r6, #4		@ r6 = x += 4		\n"
	"beq	8f			@ if (x == 0) done	\n"
	"5:							\n"
	"mov	r5, r1			@ r5 = min = src	\n"
	"mov	r7, #128		@ r7 = val = 128	\n"
	"movs	r8, r14			@ r8 = len2 = len	\n"
	"add	r9, r2, #4		@ r9 = contrib2		\n"
	"ble	7f			@ while (len2-- > 0) {	\n"
	"6:							\n"
	"ldr	r10,[r9], #4		@ r10 = *contrib2++	\n"
	"ldrb	r12,[r5], r3		@ r12 = *min	r5 = min += width\n"
	"subs	r8, r8, #1		@ len2--		\n"
	"@ stall r12						\n"
	"mla	r7, r10,r12,r7		@ val += r12 * r10	\n"
	"bgt	6b			@ }			\n"
	"7:							\n"
	"mov	r7, r7, asr #8		@ r7 = val >>= 8	\n"
	"subs	r6, r6, #1		@ x--			\n"
	"add	r1, r1, #1		@ src++			\n"
	"strb	r7, [r0], #1		@ *dst++ = val		\n"
	"bgt	5b			@ 			\n"
	"8:							\n"
	"ldmfd	r13!,{r4-r11,PC}	@ pop, return to thumb	\n"
	".ltorg							\n"
	ENTER_THUMB
	);
}

static void
scale_row_from_temp_alpha(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights, int width, int n, int row)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r4-r11,r14}				\n"
	"mov	r11,#255		@ r11= 255		\n"
	"ldr	r12,[r13,#4*10]		@ r12= row		\n"
	"@ r0 = dst						\n"
	"@ r1 = src						\n"
	"@ r2 = &weights->index[0]				\n"
	"@ r3 = width						\n"
	"@ r11= 255						\n"
	"@ r12= row						\n"
	"add	r2, r2, #24		@ r2 = weights->index	\n"
	"ldr	r4, [r2, r12, LSL #2]	@ r4 = index[row]	\n"
	"add	r2, r2, #4		@ r2 = &index[1]	\n"
	"mov	r6, r3			@ r6 = x = width	\n"
	"ldr	r14,[r2, r4, LSL #2]!	@ r2 = contrib = index[index[row]+1]\n"
	"				@ r14= len = *contrib	\n"
	"5:							\n"
	"ldr	r4,[r13,#4*9]		@ r10= nn = n		\n"
	"1:							\n"
	"mov	r5, r1			@ r5 = min = src	\n"
	"mov	r7, #128		@ r7 = val = 128	\n"
	"movs	r8, r14			@ r8 = len2 = len	\n"
	"add	r9, r2, #4		@ r9 = contrib2		\n"
	"ble	7f			@ while (len2-- > 0) {	\n"
	"6:							\n"
	"ldr	r10,[r9], #4		@ r10 = *contrib2++	\n"
	"ldrb	r12,[r5], r3		@ r12 = *min	r5 = min += width\n"
	"subs	r8, r8, #1		@ len2--		\n"
	"@ stall r12						\n"
	"mla	r7, r10,r12,r7		@ val += r12 * r10	\n"
	"bgt	6b			@ }			\n"
	"7:							\n"
	"mov	r7, r7, asr #8		@ r7 = val >>= 8	\n"
	"subs	r4, r4, #1		@ r4 = nn--		\n"
	"add	r1, r1, #1		@ src++			\n"
	"strb	r7, [r0], #1		@ *dst++ = val		\n"
	"bgt	1b			@ 			\n"
	"subs	r6, r6, #1		@ x--			\n"
	"strb	r11,[r0], #1		@ *dst++ = 255		\n"
	"bgt	5b			@ 			\n"
	"ldmfd	r13!,{r4-r11,PC}	@ pop, return to thumb	\n"
	".ltorg							\n"
	ENTER_THUMB
	);
}
#else

static void
scale_row_to_temp1(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	const int *contrib = &weights->index[weights->index[0]];
	int len, i;
	const unsigned char *min;

	assert(weights->n == 1);
	if (weights->flip)
	{
		dst += weights->count;
		for (i=weights->count; i > 0; i--)
		{
			int val = 128;
			min = &src[*contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				val += *min++ * *contrib++;
			}
			*--dst = (unsigned char)(val>>8);
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			int val = 128;
			min = &src[*contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				val += *min++ * *contrib++;
			}
			*dst++ = (unsigned char)(val>>8);
		}
	}
}

static void
scale_row_to_temp2(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	const int *contrib = &weights->index[weights->index[0]];
	int len, i;
	const unsigned char *min;

	assert(weights->n == 2);
	if (weights->flip)
	{
		dst += 2*weights->count;
		for (i=weights->count; i > 0; i--)
		{
			int c1 = 128;
			int c2 = 128;
			min = &src[2 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				c1 += *min++ * *contrib;
				c2 += *min++ * *contrib++;
			}
			*--dst = (unsigned char)(c2>>8);
			*--dst = (unsigned char)(c1>>8);
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			int c1 = 128;
			int c2 = 128;
			min = &src[2 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				c1 += *min++ * *contrib;
				c2 += *min++ * *contrib++;
			}
			*dst++ = (unsigned char)(c1>>8);
			*dst++ = (unsigned char)(c2>>8);
		}
	}
}

static void
scale_row_to_temp3(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	const int *contrib = &weights->index[weights->index[0]];
	int len, i;
	const unsigned char *min;

	assert(weights->n == 3);
	if (weights->flip)
	{
		dst += 3*weights->count;
		for (i=weights->count; i > 0; i--)
		{
			int c1 = 128;
			int c2 = 128;
			int c3 = 128;
			min = &src[3 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				int c = *contrib++;
				c1 += *min++ * c;
				c2 += *min++ * c;
				c3 += *min++ * c;
			}
			*--dst = (unsigned char)(c3>>8);
			*--dst = (unsigned char)(c2>>8);
			*--dst = (unsigned char)(c1>>8);
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			int c1 = 128;
			int c2 = 128;
			int c3 = 128;
			min = &src[3 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				int c = *contrib++;
				c1 += *min++ * c;
				c2 += *min++ * c;
				c3 += *min++ * c;
			}
			*dst++ = (unsigned char)(c1>>8);
			*dst++ = (unsigned char)(c2>>8);
			*dst++ = (unsigned char)(c3>>8);
		}
	}
}

static void
scale_row_to_temp4(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights)
{
	const int *contrib = &weights->index[weights->index[0]];
	int len, i;
	const unsigned char *min;

	assert(weights->n == 4);
	if (weights->flip)
	{
		dst += 4*weights->count;
		for (i=weights->count; i > 0; i--)
		{
			int r = 128;
			int g = 128;
			int b = 128;
			int a = 128;
			min = &src[4 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				r += *min++ * *contrib;
				g += *min++ * *contrib;
				b += *min++ * *contrib;
				a += *min++ * *contrib++;
			}
			*--dst = (unsigned char)(a>>8);
			*--dst = (unsigned char)(b>>8);
			*--dst = (unsigned char)(g>>8);
			*--dst = (unsigned char)(r>>8);
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			int r = 128;
			int g = 128;
			int b = 128;
			int a = 128;
			min = &src[4 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				r += *min++ * *contrib;
				g += *min++ * *contrib;
				b += *min++ * *contrib;
				a += *min++ * *contrib++;
			}
			*dst++ = (unsigned char)(r>>8);
			*dst++ = (unsigned char)(g>>8);
			*dst++ = (unsigned char)(b>>8);
			*dst++ = (unsigned char)(a>>8);
		}
	}
}

static void
scale_row_from_temp(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights, int w, int n, int row)
{
	const int *contrib = &weights->index[weights->index[row]];
	int len, x;
	int width = w * n;

	contrib++; /* Skip min */
	len = *contrib++;
	for (x=width; x > 0; x--)
	{
		const unsigned char *min = src;
		int val = 128;
		int len2 = len;
		const int *contrib2 = contrib;

		while (len2-- > 0)
		{
			val += *min * *contrib2++;
			min += width;
		}
		*dst++ = (unsigned char)(val>>8);
		src++;
	}
}

static void
scale_row_from_temp_alpha(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights, int w, int n, int row)
{
	const int *contrib = &weights->index[weights->index[row]];
	int len, x;
	int width = w * n;

	contrib++; /* Skip min */
	len = *contrib++;
	for (x=w; x > 0; x--)
	{
		int nn;
		for (nn = n; nn > 0; nn--)
		{
			const unsigned char *min = src;
			int val = 128;
			int len2 = len;
			const int *contrib2 = contrib;

			while (len2-- > 0)
			{
				val += *min * *contrib2++;
				min += width;
			}
			*dst++ = (unsigned char)(val>>8);
			src++;
		}
		*dst++ = 255;
	}
}
#endif

#ifdef SINGLE_PIXEL_SPECIALS
static void
duplicate_single_pixel(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, int n, int forcealpha, int w, int h, int stride)
{
	int i;

	for (i = n; i > 0; i--)
		*dst++ = *src++;
	if (forcealpha)
		*dst++ = 255;
	n += forcealpha;
	for (i = w-1; i > 0; i--)
	{
		memcpy(dst, dst-n, n);
		dst += n;
	}
	w *= n;
	dst -= w;
	h--;
	while (h--)
	{
		memcpy(dst+stride, dst, w);
		dst += stride;
	}
}

static void
scale_single_row(unsigned char * FZ_RESTRICT dst, int dstride, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights, int src_w, int h, int forcealpha)
{
	const int *contrib = &weights->index[weights->index[0]];
	int min, len, i, j, n, nf;
	int tmp[FZ_MAX_COLORS];

	n = weights->n;
	nf = n + forcealpha;
	/* Scale a single row */
	for (j = 0; j < nf; j++)
		tmp[j] = 128;
	if (weights->flip)
	{
		dst += (weights->count-1)*nf;
		for (i=weights->count; i > 0; i--)
		{
			min = *contrib++;
			len = *contrib++;
			min *= n;
			while (len-- > 0)
			{
				int c = *contrib++;
				for (j = 0; j < n; j++)
					tmp[j] += src[min++] * c;
				if (forcealpha)
					tmp[j] += 255 * c;
			}
			for (j = 0; j < nf; j++)
			{
				*dst++ = (unsigned char)(tmp[j]>>8);
				tmp[j] = 128;
			}
			dst -= 2*nf;
		}
		dst += nf + dstride;
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			min = *contrib++;
			len = *contrib++;
			min *= n;
			while (len-- > 0)
			{
				int c = *contrib++;
				for (j = 0; j < n; j++)
					tmp[j] += src[min++] * c;
				if (forcealpha)
					tmp[j] += 255 * c;
			}
			for (j = 0; j < nf; j++)
			{
				*dst++ = (unsigned char)(tmp[j]>>8);
				tmp[j] = 128;
			}
		}
		dst += dstride - weights->count * nf;
	}
	/* And then duplicate it h times */
	nf *= weights->count;
	while (--h > 0)
	{
		memcpy(dst, dst-dstride, nf);
		dst += dstride;
	}
}

static void
scale_single_col(unsigned char * FZ_RESTRICT dst, int dstride, const unsigned char * FZ_RESTRICT src, int sstride, const fz_weights * FZ_RESTRICT weights, int src_w, int n, int w, int forcealpha)
{
	const int *contrib = &weights->index[weights->index[0]];
	int min, len, i, j;
	int tmp[FZ_MAX_COLORS];
	int nf = n + forcealpha;

	for (j = 0; j < nf; j++)
		tmp[j] = 128;
	if (weights->flip)
	{
		src_w = (src_w-1)*sstride;
		for (i=weights->count; i > 0; i--)
		{
			/* Scale the next pixel in the column */
			min = *contrib++;
			len = *contrib++;
			min = src_w-min*sstride;
			while (len-- > 0)
			{
				int c = *contrib++;
				for (j = 0; j < n; j++)
					tmp[j] += src[min+j] * c;
				if (forcealpha)
					tmp[j] += 255 * c;
				min -= sstride;
			}
			for (j = 0; j < nf; j++)
			{
				*dst++ = (unsigned char)(tmp[j]>>8);
				tmp[j] = 128;
			}
			/* And then duplicate it across the row */
			for (j = (w-1)*nf; j > 0; j--)
			{
				*dst = dst[-nf];
				dst++;
			}
			dst += dstride - w*nf;
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			/* Scale the next pixel in the column */
			min = *contrib++;
			len = *contrib++;
			min *= sstride;
			while (len-- > 0)
			{
				int c = *contrib++;
				for (j = 0; j < n; j++)
					tmp[j] += src[min+j] * c;
				if (forcealpha)
					tmp[j] += 255 * c;
				min += sstride;
			}
			for (j = 0; j < nf; j++)
			{
				*dst++ = (unsigned char)(tmp[j]>>8);
				tmp[j] = 128;
			}
			/* And then duplicate it across the row */
			for (j = (w-1)*nf; j > 0; j--)
			{
				*dst = dst[-nf];
				dst++;
			}
			dst += dstride - w*nf;
		}
	}
}
#endif /* SINGLE_PIXEL_SPECIALS */

static void
get_alpha_edge_values(const fz_weights * FZ_RESTRICT rows, int * FZ_RESTRICT tp, int * FZ_RESTRICT bp)
{
	const int *contrib = &rows->index[rows->index[0]];
	int len, i, t, b;

	/* Calculate the edge alpha values */
	contrib++; /* Skip min */
	len = *contrib++;
	t = 0;
	while (len--)
		t += *contrib++;
	for (i=rows->count-2; i > 0; i--)
	{
		contrib++; /* Skip min */
		len = *contrib++;
		contrib += len;
	}
	b = 0;
	if (i == 0)
	{
		contrib++;
		len = *contrib++;
		while (len--)
			b += *contrib++;
	}
	if (rows->flip && i == 0)
	{
		*tp = b;
		*bp = t;
	}
	else
	{
		*tp = t;
		*bp = b;
	}
}

static void
adjust_alpha_edges(fz_pixmap * FZ_RESTRICT pix, const fz_weights * FZ_RESTRICT rows, const fz_weights * FZ_RESTRICT cols)
{
	int t, l, r, b, tl, tr, bl, br, x, y;
	unsigned char *dp = pix->samples;
	int w = pix->w;
	int n = pix->n;
	int span = w >= 2 ? (w-1)*n : 0;
	int stride = pix->stride;

	get_alpha_edge_values(rows, &t, &b);
	get_alpha_edge_values(cols, &l, &r);

	l = (255 * l + 128)>>8;
	r = (255 * r + 128)>>8;
	tl = (l * t + 128)>>8;
	tr = (r * t + 128)>>8;
	bl = (l * b + 128)>>8;
	br = (r * b + 128)>>8;
	t = (255 * t + 128)>>8;
	b = (255 * b + 128)>>8;
	dp += n-1;
	*dp = tl;
	dp += n;
	for (x = w-2; x > 0; x--)
	{
		*dp = t;
		dp += n;
	}
	if (x == 0)
	{
		*dp = tr;
		dp += n;
	}
	dp += stride - w*n;
	for (y = pix->h-2; y > 0; y--)
	{
		dp[span] = r;
		*dp = l;
		dp += stride;
	}
	if (y == 0)
	{
		*dp = bl;
		dp += n;
		for (x = w-2; x > 0; x--)
		{
			*dp = b;
			dp += n;
		}
		if (x == 0)
		{
			*dp = br;
		}
	}
}

fz_pixmap *
fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip)
{
	return fz_scale_pixmap_cached(ctx, src, x, y, w, h, clip, NULL, NULL);
}

fz_pixmap *
fz_scale_pixmap_cached(fz_context *ctx, const fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip, fz_scale_cache *cache_x, fz_scale_cache *cache_y)
{
	fz_scale_filter *filter = &fz_scale_filter_simple;
	fz_weights *contrib_rows = NULL;
	fz_weights *contrib_cols = NULL;
	fz_pixmap *output = NULL;
	unsigned char *temp = NULL;
	int max_row, temp_span, temp_rows, row;
	int dst_w_int, dst_h_int, dst_x_int, dst_y_int;
	int flip_x, flip_y, forcealpha;
	fz_rect patch;

	fz_var(contrib_cols);
	fz_var(contrib_rows);

	/* Avoid extreme scales where overflows become problematic. */
	if (w > (1<<24) || h > (1<<24) || w < -(1<<24) || h < -(1<<24))
		return NULL;
	if (x > (1<<24) || y > (1<<24) || x < -(1<<24) || y < -(1<<24))
		return NULL;

	/* Clamp small ranges of w and h */
	if (w <= -1)
	{
	}
	else if (w < 0)
	{
		w = -1;
	}
	else if (w < 1)
	{
		w = 1;
	}
	if (h <= -1)
	{
	}
	else if (h < 0)
	{
		h = -1;
	}
	else if (h < 1)
	{
		h = 1;
	}

	/* If the src has an alpha, we'll make the dst have an alpha automatically.
	 * We also need to force the dst to have an alpha if x/y/w/h aren't ints. */
	forcealpha = !src->alpha && (x != (float)(int)x || y != (float)(int)y || w != (float)(int)w || h != (float)(int)h);

	/* Find the destination bbox, width/height, and sub pixel offset,
	 * allowing for whether we're flipping or not. */
	/* The (x,y) position given describes where the top left corner
	 * of the source image should be mapped to (i.e. where (0,0) in image
	 * space ends up). Also there are differences in the way we scale
	 * horizontally and vertically. When scaling rows horizontally, we
	 * always read forwards through the source, and store either forwards
	 * or in reverse as required. When scaling vertically, we always store
	 * out forwards, but may feed source rows in in a different order.
	 *
	 * Consider the image rectangle 'r' to which the image is mapped,
	 * and the (possibly) larger rectangle 'R', given by expanding 'r' to
	 * complete pixels.
	 *
	 * x can either be r.xmin-R.xmin or R.xmax-r.xmax depending on whether
	 * the image is x flipped or not. Whatever happens 0 <= x < 1.
	 * y is always R.ymax - r.ymax.
	 */
	/* dst_x_int is calculated to be the left of the scaled image, and
	 * x (the sub pixel offset) is the distance in from either the left
	 * or right pixel expanded edge. */
	flip_x = (w < 0);
	if (flip_x)
	{
		float tmp;
		w = -w;
		dst_x_int = floorf(x-w);
		tmp = ceilf(x);
		dst_w_int = (int)tmp;
		x = tmp - x;
		dst_w_int -= dst_x_int;
	}
	else
	{
		dst_x_int = floorf(x);
		x -= dst_x_int;
		dst_w_int = (int)ceilf(x + w);
	}
	/* dst_y_int is calculated to be the top of the scaled image, and
	 * y (the sub pixel offset) is the distance in from either the top
	 * or bottom pixel expanded edge.
	 */
	flip_y = (h < 0);
	if (flip_y)
	{
		float tmp;
		h = -h;
		dst_y_int = floorf(y-h);
		tmp = ceilf(y);
		dst_h_int = (int)tmp;
		y = tmp - y;
		dst_h_int -= dst_y_int;
	}
	else
	{
		dst_y_int = floorf(y);
		y -= dst_y_int;
		dst_h_int = (int)ceilf(y + h);
	}

	fz_valgrind_pixmap(src);

	/* Step 0: Calculate the patch */
	patch.x0 = 0;
	patch.y0 = 0;
	patch.x1 = dst_w_int;
	patch.y1 = dst_h_int;
	if (clip)
	{
		if (flip_x)
		{
			if (dst_x_int + dst_w_int > clip->x1)
				patch.x0 = dst_x_int + dst_w_int - clip->x1;
			if (clip->x0 > dst_x_int)
			{
				patch.x1 = dst_w_int - (clip->x0 - dst_x_int);
				dst_x_int = clip->x0;
			}
		}
		else
		{
			if (dst_x_int + dst_w_int > clip->x1)
				patch.x1 = clip->x1 - dst_x_int;
			if (clip->x0 > dst_x_int)
			{
				patch.x0 = clip->x0 - dst_x_int;
				dst_x_int += patch.x0;
			}
		}

		if (flip_y)
		{
			if (dst_y_int + dst_h_int > clip->y1)
				patch.y1 = clip->y1 - dst_y_int;
			if (clip->y0 > dst_y_int)
			{
				patch.y0 = clip->y0 - dst_y_int;
				dst_y_int = clip->y0;
			}
		}
		else
		{
			if (dst_y_int + dst_h_int > clip->y1)
				patch.y1 = clip->y1 - dst_y_int;
			if (clip->y0 > dst_y_int)
			{
				patch.y0 = clip->y0 - dst_y_int;
				dst_y_int += patch.y0;
			}
		}
	}
	if (patch.x0 >= patch.x1 || patch.y0 >= patch.y1)
		return NULL;

	fz_try(ctx)
	{
		/* Step 1: Calculate the weights for columns and rows */
#ifdef SINGLE_PIXEL_SPECIALS
		if (src->w == 1)
			contrib_cols = NULL;
		else
#endif /* SINGLE_PIXEL_SPECIALS */
			contrib_cols = Memento_label(make_weights(ctx, src->w, x, w, filter, 0, dst_w_int, patch.x0, patch.x1, src->n, flip_x, cache_x), "contrib_cols");
#ifdef SINGLE_PIXEL_SPECIALS
		if (src->h == 1)
			contrib_rows = NULL;
		else
#endif /* SINGLE_PIXEL_SPECIALS */
			contrib_rows = Memento_label(make_weights(ctx, src->h, y, h, filter, 1, dst_h_int, patch.y0, patch.y1, src->n, flip_y, cache_y), "contrib_rows");

		output = fz_new_pixmap(ctx, src->colorspace, patch.x1 - patch.x0, patch.y1 - patch.y0, src->seps, src->alpha || forcealpha);
	}
	fz_catch(ctx)
	{
		if (!cache_x)
			fz_free(ctx, contrib_cols);
		if (!cache_y)
			fz_free(ctx, contrib_rows);
		fz_rethrow(ctx);
	}
	output->x = dst_x_int;
	output->y = dst_y_int;

	/* Step 2: Apply the weights */
#ifdef SINGLE_PIXEL_SPECIALS
	if (!contrib_rows)
	{
		/* Only 1 source pixel high. */
		if (!contrib_cols)
		{
			/* Only 1 pixel in the entire image! */
			duplicate_single_pixel(output->samples, src->samples, src->n, forcealpha, patch.x1-patch.x0, patch.y1-patch.y0, output->stride);
			fz_valgrind_pixmap(output);
		}
		else
		{
			/* Scale the row once, then copy it. */
			scale_single_row(output->samples, output->stride, src->samples, contrib_cols, src->w, patch.y1-patch.y0, forcealpha);
			fz_valgrind_pixmap(output);
		}
	}
	else if (!contrib_cols)
	{
		/* Only 1 source pixel wide. Scale the col and duplicate. */
		scale_single_col(output->samples, output->stride, src->samples, src->stride, contrib_rows, src->h, src->n, patch.x1-patch.x0, forcealpha);
		fz_valgrind_pixmap(output);
	}
	else
#endif /* SINGLE_PIXEL_SPECIALS */
	{
		void (*row_scale_in)(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights);
		void (*row_scale_out)(unsigned char * FZ_RESTRICT dst, const unsigned char * FZ_RESTRICT src, const fz_weights * FZ_RESTRICT weights, int w, int n, int row);

		temp_span = contrib_cols->count * src->n;
		temp_rows = contrib_rows->max_len;
		if (temp_span <= 0 || temp_rows > INT_MAX / temp_span)
			goto cleanup;
		fz_try(ctx)
		{
			temp = fz_calloc(ctx, temp_span*temp_rows, sizeof(unsigned char));
		}
		fz_catch(ctx)
		{
			fz_drop_pixmap(ctx, output);
			if (!cache_x)
				fz_free(ctx, contrib_cols);
			if (!cache_y)
				fz_free(ctx, contrib_rows);
			fz_rethrow(ctx);
		}
		switch (src->n)
		{
		default:
			row_scale_in = scale_row_to_temp;
			break;
		case 1: /* Image mask case or Greyscale case */
			row_scale_in = scale_row_to_temp1;
			break;
		case 2: /* Greyscale with alpha case */
			row_scale_in = scale_row_to_temp2;
			break;
		case 3: /* RGB case */
			row_scale_in = scale_row_to_temp3;
			break;
		case 4: /* RGBA or CMYK case */
			row_scale_in = scale_row_to_temp4;
			break;
		}
		row_scale_out = forcealpha ? scale_row_from_temp_alpha : scale_row_from_temp;
		max_row = contrib_rows->index[contrib_rows->index[0]];
		for (row = 0; row < contrib_rows->count; row++)
		{
			/*
			Which source rows do we need to have scaled into the
			temporary buffer in order to be able to do the final
			scale?
			*/
			int row_index = contrib_rows->index[row];
			int row_min = contrib_rows->index[row_index++];
			int row_len = contrib_rows->index[row_index];
			while (max_row < row_min+row_len)
			{
				/* Scale another row */
				assert(max_row < src->h);
				(*row_scale_in)(&temp[temp_span*(max_row % temp_rows)], &src->samples[(flip_y ? (src->h-1-max_row): max_row)*src->stride], contrib_cols);
				max_row++;
			}

			(*row_scale_out)(&output->samples[row*output->stride], temp, contrib_rows, contrib_cols->count, src->n, row);
		}
		fz_free(ctx, temp);

		if (forcealpha)
			adjust_alpha_edges(output, contrib_rows, contrib_cols);

		fz_valgrind_pixmap(output);
	}

cleanup:
	if (!cache_y)
		fz_free(ctx, contrib_rows);
	if (!cache_x)
		fz_free(ctx, contrib_cols);

	return output;
}

void
fz_drop_scale_cache(fz_context *ctx, fz_scale_cache *sc)
{
	if (!sc)
		return;
	fz_free(ctx, sc->weights);
	fz_free(ctx, sc);
}

fz_scale_cache *
fz_new_scale_cache(fz_context *ctx)
{
	return fz_malloc_struct(ctx, fz_scale_cache);
}
