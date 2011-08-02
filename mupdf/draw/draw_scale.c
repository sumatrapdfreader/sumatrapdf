/*
This code does smooth scaling of a pixmap.

This function returns a new pixmap representing the area starting at (0,0)
given by taking the source pixmap src, scaling it to width w, and height h,
and then positioning it at (frac(x),frac(y)).
*/

#include "fitz.h"

/* Do we special case handling of single pixel high/wide images? The
 * 'purest' handling is given by not special casing them, but certain
 * files that use such images 'stack' them to give full images. Not
 * special casing them results in then being fainter and giving noticable
 * rounding errors.
 */
#define SINGLE_PIXEL_SPECIALS

#ifdef DEBUG_SCALING
#ifdef WIN32
#include <windows.h>
static void debug_print(const char *fmt, ...)
{
	va_list args;
	char text[256];
	va_start(args, fmt);
	vsprintf(text, fmt, args);
	va_end(args);
	OutputDebugStringA(text);
	printf(text);
}
#else
static void debug_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}
#endif
#endif
#ifdef DEBUG_SCALING
#define DBUG(A) debug_print A
#else
#define DBUG(A) do {} while(0==1)
#endif

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

static float
lanczos2(fz_scale_filter *filter, float x)
{
	if (x >= 2)
		return 0;
	return sinf(M_PI*x) * sinf(M_PI*x/2) / (M_PI*x) / (M_PI*x/2);
}

static float
lanczos3(fz_scale_filter *filter, float f)
{
	if (f >= 3)
		return 0;
	return sinf(M_PI*f) * sinf(M_PI*f/3) / (M_PI*f) / (M_PI*f/3);
}

/*
The Mitchell family of filters is defined:

	f(x) =	1 { (12-9B-6C)x^3 + (-18+12B+6C)x^2 + (6-2B)	for x < 1
		- {
		6 { (-B-6C)x^3+(6B+30C)x^2+(-12B-48C)x+(8B+24C)	for 1<=x<=2

The 'best' ones lie along the line B+2C = 1.
The literature suggests that B=1/3, C=1/3 is best.

	f(x) =	1 { (12-3-2)x^3 - (-18+4+2)x^2 + (16/3)	for x < 1
		- {
		6 { (-7/3)x^3 + 12x^2 - 20x + (32/3)	for 1<=x<=2

	f(x) =	1 { 21x^3 - 36x^2 + 16			for x < 1
		- {
		18{ -7x^3 + 36x^2 - 60x + 32		for 1<=x<=2
*/

static float
mitchell(fz_scale_filter *filter, float x)
{
	if (x >= 2)
		return 0;
	if (x >= 1)
		return (32 + x*(-60 + x*(36 - 7*x)))/18;
	return (16 + x*x*(-36 + 21*x))/18;
}

fz_scale_filter fz_scale_filter_box = { 1, box };
fz_scale_filter fz_scale_filter_triangle = { 1, triangle };
fz_scale_filter fz_scale_filter_simple = { 1, simple };
fz_scale_filter fz_scale_filter_lanczos2 = { 2, lanczos2 };
fz_scale_filter fz_scale_filter_lanczos3 = { 3, lanczos3 };
fz_scale_filter fz_scale_filter_mitchell = { 2, mitchell };

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

struct fz_weights_s
{
	int count;
	int max_len;
	int n;
	int flip;
	int new_line;
	int index[1];
};

static fz_weights *
new_weights(fz_scale_filter *filter, int src_w, float dst_w, int dst_w_i, int n, int flip)
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
	 * plus dst_w*sizeof(int) for the index
	 * plus (2+max_len)*sizeof(int) for the weights
	 * plus room for an extra set of weights for reordering.
	 */
	weights = fz_malloc(sizeof(*weights)+(max_len+3)*(dst_w_i+1)*sizeof(int));
	if (weights == NULL)
		return NULL;
	weights->count = -1;
	weights->max_len = max_len;
	weights->index[0] = dst_w_i;
	weights->n = n;
	weights->flip = flip;
	return weights;
}

static void
init_weights(fz_weights *weights, int j)
{
	int index;

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
	if (weight == 0)
		return;

	/* wrap i back into range */
#ifdef MIRROR_WRAP
	do
	{
		if (i < 0)
			i = -1-i;
		else if (i >= src_w)
			i = 2*src_w-1-i;
		else
			break;
	}
	while (1);
#elif defined(WRAP)
	if (i < 0)
		i = 0;
	else if (i >= src_w)
		i = src_w-1;
#else
	if (i < 0)
	{
		i = 0;
		weight = 0;
	}
	else if (i >= src_w)
	{
		i = src_w-1;
		weight = 0;
	}
	if (weight == 0)
		return;
#endif

	DBUG(("add_weight[%d][%d] = %d(%g) dist=%g\n",j,i,weight,f,dist));

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
	int idx = weights->index[j];
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

	idx = weights->index[j];
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
	else if ((j == 0) && (x < 0.0001F) && (sum != 256))
		weights->index[maxidx-1] += 256-sum;
	/* Finally, if we are the last pixel, and it's fully covered, then
	 * adjust it. */
	else if ((j == w-1) && ((float)w-wf < 0.0001F) && (sum != 256))
		weights->index[maxidx-1] += 256-sum;
	DBUG(("total weight %d = %d\n", j, sum));
}

static fz_weights *
make_weights(int src_w, float x, float dst_w, fz_scale_filter *filter, int vertical, int dst_w_int, int n, int flip)
{
	fz_weights *weights;
	float F, G;
	float window;
	int j;

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
	DBUG(("make_weights src_w=%d x=%g dst_w=%g dst_w_int=%d F=%g window=%g\n", src_w, x, dst_w, dst_w_int, F, window));
	weights	= new_weights(filter, src_w, dst_w, dst_w_int, n, flip);
	if (weights == NULL)
		return NULL;
	for (j = 0; j < dst_w_int; j++)
	{
		/* find the position of the centre of dst[j] in src space */
		float centre = (j - x + 0.5f)*src_w/dst_w - 0.5f;
		int l, r;
		l = ceilf(centre - window);
		r = floorf(centre + window);
		DBUG(("%d: centre=%g l=%d r=%d\n", j, centre, l, r));
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
	return weights;
}

static void
scale_row_to_temp(int *dst, unsigned char *src, fz_weights *weights)
{
	int *contrib = &weights->index[weights->index[0]];
	int len, i, j, n;
	unsigned char *min;

	n = weights->n;
	if (weights->flip)
	{
		dst += (weights->count-1)*n;
		for (i=weights->count; i > 0; i--)
		{
			min = &src[n * *contrib++];
			len = *contrib++;
			for (j = 0; j < n; j++)
				dst[j] = 0;
			while (len-- > 0)
			{
				for (j = n; j > 0; j--)
					*dst++ += *min++ * *contrib;
				dst -= n;
				contrib++;
			}
			dst -= n;
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			min = &src[n * *contrib++];
			len = *contrib++;
			for (j = 0; j < n; j++)
				dst[j] = 0;
			while (len-- > 0)
			{
				for (j = n; j > 0; j--)
					*dst++ += *min++ * *contrib;
				dst -= n;
				contrib++;
			}
			dst += n;
		}
	}
}

static void
scale_row_to_temp1(int *dst, unsigned char *src, fz_weights *weights)
{
	int *contrib = &weights->index[weights->index[0]];
	int len, i;
	unsigned char *min;

	assert(weights->n == 1);
	if (weights->flip)
	{
		dst += weights->count;
		for (i=weights->count; i > 0; i--)
		{
			int val = 0;
			min = &src[*contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				val += *min++ * *contrib++;
			}
			*--dst = val;
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			int val = 0;
			min = &src[*contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				val += *min++ * *contrib++;
			}
			*dst++ = val;
		}
	}
}

static void
scale_row_to_temp2(int *dst, unsigned char *src, fz_weights *weights)
{
	int *contrib = &weights->index[weights->index[0]];
	int len, i;
	unsigned char *min;

	assert(weights->n == 2);
	if (weights->flip)
	{
		dst += 2*weights->count;
		for (i=weights->count; i > 0; i--)
		{
			int c1 = 0;
			int c2 = 0;
			min = &src[2 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				c1 += *min++ * *contrib;
				c2 += *min++ * *contrib++;
			}
			*--dst = c2;
			*--dst = c1;
		}
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			int c1 = 0;
			int c2 = 0;
			min = &src[2 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				c1 += *min++ * *contrib;
				c2 += *min++ * *contrib++;
			}
			*dst++ = c1;
			*dst++ = c2;
		}
	}
}

static void
scale_row_to_temp4(int *dst, unsigned char *src, fz_weights *weights)
{
	int *contrib = &weights->index[weights->index[0]];
#ifndef ARCH_ARM
	int len, i;
	unsigned char *min;
#endif

	assert(weights->n == 4);
	if (weights->flip)
	{
		dst += 4*weights->count;
#ifdef ARCH_ARM
		asm volatile(
		"1:"
		"ldr	r4, [%2], #4		@ r4 = *contrib++	\n"
		"ldr	r9, [%2], #4		@ r9 = len = *contrib++	\n"
		"mov	r5, #0			@ r5 = r = 0		\n"
		"mov	r6, #0			@ r6 = g = 0		\n"
		"mov	r7, #0			@ r7 = b = 0		\n"
		"mov	r8, #0			@ r8 = a = 0		\n"
		"add	r4, %1, r4, LSL #2	@ r4 = min = &src[4*r4]	\n"
		"cmp	r9, #0			@ while (len-- > 0)	\n"
		"beq	3f			@ {			\n"
		"2:							\n"
		"ldr	r10,[%2], #4		@ r10 = *contrib++	\n"
		"ldrb	r11,[r4], #1		@ r11 = *min++		\n"
		"ldrb	r12,[r4], #1		@ r12 = *min++		\n"
		"ldrb	r14,[r4], #1		@ r14 = *min++		\n"
		"mla	r5, r10,r11,r5		@ r += r11 * r10	\n"
		"ldrb	r11,[r4], #1		@ r11 = *min++		\n"
		"mla	r6, r10,r12,r6		@ g += r12 * r10	\n"
		"mla	r7, r10,r14,r7		@ b += r14 * r10	\n"
		"mla	r8, r10,r11,r8		@ a += r11 * r10	\n"
		"subs	r9, r9, #1		@ r9 = len--		\n"
		"bgt	2b			@ }			\n"
		"stmdb	%0!,{r5,r6,r7,r8}	@ *--dst=a;*--dst=b;	\n"
		"3:				@ *--dst=g;*--dst=r;	\n"
		"subs	%3, %3, #1		@ i--			\n"
		"bgt	1b			@ 			\n"
		:
		:
		"r" (dst),
		"r" (src),
		"r" (contrib),
		"r" (weights->count)
		:
		"r4","r5","r6","r7","r8","r9","r10","r11","r12","r14",
		"memory","cc"
		);
#else
		for (i=weights->count; i > 0; i--)
		{
			int r = 0;
			int g = 0;
			int b = 0;
			int a = 0;
			min = &src[4 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				r += *min++ * *contrib;
				g += *min++ * *contrib;
				b += *min++ * *contrib;
				a += *min++ * *contrib++;
			}
			*--dst = a;
			*--dst = b;
			*--dst = g;
			*--dst = r;
		}
#endif
	}
	else
	{
#ifdef ARCH_ARM
		asm volatile(
		"1:"
		"ldr	r4, [%2], #4		@ r4 = *contrib++	\n"
		"ldr	r9, [%2], #4		@ r9 = len = *contrib++	\n"
		"mov	r5, #0			@ r5 = r = 0		\n"
		"mov	r6, #0			@ r6 = g = 0		\n"
		"mov	r7, #0			@ r7 = b = 0		\n"
		"mov	r8, #0			@ r8 = a = 0		\n"
		"add	r4, %1, r4, LSL #2	@ r4 = min = &src[4*r4]	\n"
		"cmp	r9, #0			@ while (len-- > 0)	\n"
		"beq	3f			@ {			\n"
		"2:							\n"
		"ldr	r10,[%2], #4		@ r10 = *contrib++	\n"
		"ldrb	r11,[r4], #1		@ r11 = *min++		\n"
		"ldrb	r12,[r4], #1		@ r12 = *min++		\n"
		"ldrb	r14,[r4], #1		@ r14 = *min++		\n"
		"mla	r5, r10,r11,r5		@ r += r11 * r10	\n"
		"ldrb	r11,[r4], #1		@ r11 = *min++		\n"
		"mla	r6, r10,r12,r6		@ g += r12 * r10	\n"
		"mla	r7, r10,r14,r7		@ b += r14 * r10	\n"
		"mla	r8, r10,r11,r8		@ a += r11 * r10	\n"
		"subs	r9, r9, #1		@ r9 = len--		\n"
		"bgt	2b			@ }			\n"
		"stmia	%0!,{r5,r6,r7,r8}	@ *dst++=r;*dst++=g;	\n"
		"3:				@ *dst++=b;*dst++=a;	\n"
		"subs	%3, %3, #1		@ i--			\n"
		"bgt	1b			@ 			\n"
		:
		:
		"r" (dst),
		"r" (src),
		"r" (contrib),
		"r" (weights->count)
		:
		"r4","r5","r6","r7","r8","r9","r10","r11","r12","r14",
		"memory","cc"
		);
#else
		for (i=weights->count; i > 0; i--)
		{
			int r = 0;
			int g = 0;
			int b = 0;
			int a = 0;
			min = &src[4 * *contrib++];
			len = *contrib++;
			while (len-- > 0)
			{
				r += *min++ * *contrib;
				g += *min++ * *contrib;
				b += *min++ * *contrib;
				a += *min++ * *contrib++;
			}
			*dst++ = r;
			*dst++ = g;
			*dst++ = b;
			*dst++ = a;
		}
#endif
	}
}

static void
scale_row_from_temp(unsigned char *dst, int *src, fz_weights *weights, int width, int row)
{
	int *contrib = &weights->index[weights->index[row]];
	int len, x;

	contrib++; /* Skip min */
	len = *contrib++;
	for (x=width; x > 0; x--)
	{
		int *min = src;
		int val = 0;
		int len2 = len;
		int *contrib2 = contrib;

		while (len2-- > 0)
		{
			val += *min * *contrib2++;
			min += width;
		}
		val = (val+(1<<15))>>16;
		if (val < 0)
			val = 0;
		else if (val > 255)
			val = 255;
		*dst++ = val;
		src++;
	}
}

#ifdef SINGLE_PIXEL_SPECIALS
static void
duplicate_single_pixel(unsigned char *dst, unsigned char *src, int n, int w, int h)
{
	int i;

	for (i = n; i > 0; i--)
		*dst++ = *src++;
	for (i = (w*h-1)*n; i > 0; i--)
	{
		*dst = dst[-n];
		dst++;
	}
}

static void
scale_single_row(unsigned char *dst, unsigned char *src, fz_weights *weights, int src_w, int h)
{
	int *contrib = &weights->index[weights->index[0]];
	int min, len, i, j, val, n;
	int tmp[FZ_MAX_COLORS];

	n = weights->n;
	/* Scale a single row */
	if (weights->flip)
	{
		dst += (weights->count-1)*n;
		for (i=weights->count; i > 0; i--)
		{
			min = *contrib++;
			len = *contrib++;
			min *= n;
			for (j = 0; j < n; j++)
				tmp[j] = 0;
			while (len-- > 0)
			{
				for (j = 0; j < n; j++)
					tmp[j] += src[min++] * *contrib;
				contrib++;
			}
			for (j = 0; j < n; j++)
			{
				val = (tmp[j]+(1<<7))>>8;
				if (val < 0)
					val = 0;
				else if (val > 255)
					val = 255;
				*dst++ = val;
			}
			dst -= 2*n;
		}
		dst += n * (weights->count+1);
	}
	else
	{
		for (i=weights->count; i > 0; i--)
		{
			min = *contrib++;
			len = *contrib++;
			min *= n;
			for (j = 0; j < n; j++)
				tmp[j] = 0;
			while (len-- > 0)
			{
				for (j = 0; j < n; j++)
					tmp[j] += src[min++] * *contrib;
				contrib++;
			}
			for (j = 0; j < n; j++)
			{
				val = (tmp[j]+(1<<7))>>8;
				if (val < 0)
					val = 0;
				else if (val > 255)
					val = 255;
				*dst++ = val;
			}
		}
	}
	/* And then duplicate it h times */
	n *= weights->count;
	while (--h > 0)
	{
		memcpy(dst, dst-n, n);
		dst += n;
	}
}

static void
scale_single_col(unsigned char *dst, unsigned char *src, fz_weights *weights, int src_w, int n, int w, int flip_y)
{
	int *contrib = &weights->index[weights->index[0]];
	int min, len, i, j, val;
	int tmp[FZ_MAX_COLORS];

	if (flip_y)
	{
		src_w = (src_w-1)*n;
		w = (w-1)*n;
		for (i=weights->count; i > 0; i--)
		{
			/* Scale the next pixel in the column */
			min = *contrib++;
			len = *contrib++;
			min = src_w-min*n;
			for (j = 0; j < n; j++)
				tmp[j] = 0;
			while (len-- > 0)
			{
				for (j = 0; j < n; j++)
					tmp[j] += src[src_w-min+j] * *contrib;
				contrib++;
			}
			for (j = 0; j < n; j++)
			{
				val = (tmp[j]+(1<<7))>>8;
				if (val < 0)
					val = 0;
				else if (val > 255)
					val = 255;
				*dst++ = val;
			}
			/* And then duplicate it across the row */
			for (j = w; j > 0; j--)
			{
				*dst = dst[-n];
				dst++;
			}
		}
	}
	else
	{
		w = (w-1)*n;
		for (i=weights->count; i > 0; i--)
		{
			/* Scale the next pixel in the column */
			min = *contrib++;
			len = *contrib++;
			min *= n;
			for (j = 0; j < n; j++)
				tmp[j] = 0;
			while (len-- > 0)
			{
				for (j = 0; j < n; j++)
					tmp[j] += src[min++] * *contrib;
				contrib++;
			}
			for (j = 0; j < n; j++)
			{
				val = (tmp[j]+(1<<7))>>8;
				if (val < 0)
					val = 0;
				else if (val > 255)
					val = 255;
				*dst++ = val;
			}
			/* And then duplicate it across the row */
			for (j = w; j > 0; j--)
			{
				*dst = dst[-n];
				dst++;
			}
		}
	}
}
#endif /* SINGLE_PIXEL_SPECIALS */

fz_pixmap *
fz_scale_pixmap_gridfit(fz_pixmap *src, float x, float y, float w, float h, int gridfit)
{
	if (gridfit) {
		float n;
		if (w > 0) {
			/* Adjust the left hand edge, leftwards to a pixel boundary */
			n = (float)(int)x;   /* n is now on a pixel boundary */
			if (n > x)           /* Ensure it's the pixel boundary BELOW x */
				n -= 1.0f;
			w += x-n;            /* width gets wider as x >= n */
			x = n;
			/* Adjust the right hand edge rightwards to a pixel boundary */
			n = (float)(int)w;   /* n is now the integer width <= w */
			if (n != w)          /* If w isn't an integer already, bump it */
				w = 1.0f + n;/* up to the next integer. */
		} else {
			/* Adjust the right hand edge, rightwards to a pixel boundary */
			n = (float)(int)x;   /* n is now on a pixel boundary */
			if (n > x)           /* Ensure it's the pixel boundary <= x */
				n -= 1.0f;
			if (n != x)          /* If x isn't on a pixel boundary already, */
				n += 1.0f;   /* make n be the pixel boundary above x. */
			w -= n-x;            /* Expand width (more negative!) as n >= x */
			x = n;
			/* Adjust the left hand edge leftwards to a pixel boundary */
			n = (float)(int)w;
			if (n != w)
				w = n - 1.0f;
		}
		if (h > 0) {
			/* Adjust the bottom edge, downwards to a pixel boundary */
			n = (float)(int)y;   /* n is now on a pixel boundary */
			if (n > y)           /* Ensure it's the pixel boundary BELOW y */
				n -= 1.0f;
			h += y-n;            /* height gets larger as y >= n */
			y = n;
			/* Adjust the top edge upwards to a pixel boundary */
			n = (float)(int)h;   /* n is now the integer height <= h */
			if (n != h)          /* If h isn't an integer already, bump it */
				h = 1.0f + n;/* up to the next integer. */
		} else {
			/* Adjust the top edge, upwards to a pixel boundary */
			n = (float)(int)y;   /* n is now on a pixel boundary */
			if (n > y)           /* Ensure it's the pixel boundary <= y */
			n -= 1.0f;
			if (n != y)          /* If y isn't on a pixel boundary already, */
				n += 1.0f;   /* make n be the pixel boundary above y. */
			h -= n-y;            /* Expand height (more negative!) as n >= y */
			y = n;
			/* Adjust the bottom edge downwards to a pixel boundary */
			n = (float)(int)h;
			if (n != h)
				h = n - 1.0f;
		}
	}
	return fz_scale_pixmap(src, x, y, w, h);
}

fz_pixmap *
fz_scale_pixmap(fz_pixmap *src, float x, float y, float w, float h)
{
	fz_scale_filter *filter = &fz_scale_filter_simple;
	fz_weights *contrib_rows = NULL;
	fz_weights *contrib_cols = NULL;
	fz_pixmap *output = NULL;
	int *temp = NULL;
	int max_row, temp_span, temp_rows, row;
	int dst_w_int, dst_h_int, dst_x_int, dst_y_int;
	int flip_x, flip_y;

	DBUG(("Scale: (%d,%d) to (%g,%g) at (%g,%g)\n",src->w,src->h,w,h,x,y));

	/* Find the destination bbox, width/height, and sub pixel offset,
	 * allowing for whether we're flipping or not. */
	/* Note that the x and y sub pixel offsets here are different.
	 * The (x,y) position given describes where the bottom left corner
	 * of the source image should be mapped to (i.e. where (0,h) in image
	 * space ends up, not the more logical and sane (0,0)). Also there
	 * are differences in the way we scale horizontally and vertically.
	 * When scaling rows horizontally, we always read forwards through
	 * the source, and store either forwards or in reverse as required.
	 * When scaling vertically, we always store out forwards, but may
	 * feed source rows in in a different order.
	 *
	 * Consider the image rectange 'r' to which the image is mapped,
	 * and the (possibly) larger rectangle 'R', given by expanding 'r' to
	 * complete pixels.
	 *
	 * x can either be r.xmin-R.xmin or R.xmax-r.xmax depending on whether
	 * the image is x flipped or not. Whatever happens 0 <= x < 1.
	 * y is always R.ymax - r.ymax.
	 */
	/* dst_x_int is calculated to be the left of the scaled image, and
	 * x (the sub_pixel_offset) is the distance in from either the left
	 * or right pixel expanded edge. */
	flip_x = (w < 0);
	if (flip_x)
	{
		float tmp;
		w = -w;
		dst_x_int = floor(x-w);
		tmp = ceilf(x);
		dst_w_int = (int)tmp;
		x = tmp - x;
		dst_w_int -= dst_x_int;
	}
	else
	{
		dst_x_int = floor(x);
		x -= (float)dst_x_int;
		dst_w_int = (int)ceilf(x + w);
	}
	flip_y = (h < 0);
	/* dst_y_int is calculated to be the bottom of the scaled image, but
	 * y (the sub pixel offset) has to end up being the value at the top.
	 */
	if (flip_y)
	{
		h = -h;
		dst_y_int = floor(y-h);
		dst_h_int = (int)ceilf(y) - dst_y_int;
	} else {
		dst_y_int = floor(y);
		y += h;
		dst_h_int = (int)ceilf(y) - dst_y_int;
	}
	/* y is the top edge position in floats. We want it to be the
	 * distance down from the next pixel boundary. */
	y = ceilf(y) - y;

	DBUG(("Result image: (%d,%d) at (%d,%d) (subpix=%g,%g)\n", dst_w_int, dst_h_int, dst_x_int, dst_y_int, x, y));

	/* Step 1: Calculate the weights for columns and rows */
#ifdef SINGLE_PIXEL_SPECIALS
	if (src->w == 1)
	{
		contrib_cols = NULL;
	}
	else
#endif /* SINGLE_PIXEL_SPECIALS */
	{
		contrib_cols = make_weights(src->w, x, w, filter, 0, dst_w_int, src->n, flip_x);
		if (contrib_cols == NULL)
			goto cleanup;
	}
#ifdef SINGLE_PIXEL_SPECIALS
	if (src->h == 1)
	{
		contrib_rows = NULL;
	}
	else
#endif /* SINGLE_PIXEL_SPECIALS */
	{
		contrib_rows = make_weights(src->h, y, h, filter, 1, dst_h_int, src->n, flip_y);
		if (contrib_rows == NULL)
			goto cleanup;
	}

	assert(contrib_cols == NULL || contrib_cols->count == dst_w_int);
	assert(contrib_rows == NULL || contrib_rows->count == dst_h_int);
	output = fz_new_pixmap(src->colorspace, dst_w_int, dst_h_int);
	output->x = dst_x_int;
	output->y = dst_y_int;

	/* Step 2: Apply the weights */
#ifdef SINGLE_PIXEL_SPECIALS
	if (contrib_rows == NULL)
	{
		/* Only 1 source pixel high. */
		if (contrib_cols == NULL)
		{
			/* Only 1 pixel in the entire image! */
			duplicate_single_pixel(output->samples, src->samples, src->n, dst_w_int, dst_h_int);
		}
		else
		{
			/* Scale the row once, then copy it. */
			scale_single_row(output->samples, src->samples, contrib_cols, src->w, dst_h_int);
		}
	}
	else if (contrib_cols == NULL)
	{
		/* Only 1 source pixel wide. Scale the col and duplicate. */
		scale_single_col(output->samples, src->samples, contrib_rows, src->h, src->n, dst_w_int, flip_y);
	}
	else
#endif /* SINGLE_PIXEL_SPECIALS */
	{
		void (*row_scale)(int *dst, unsigned char *src, fz_weights *weights);

		temp_span = contrib_cols->count * src->n;
		temp_rows = contrib_rows->max_len;
		if (temp_span <= 0 || temp_rows > INT_MAX / temp_span)
			goto cleanup;
		temp = fz_calloc(temp_span*temp_rows, sizeof(int));
		if (temp == NULL)
			goto cleanup;
		switch (src->n)
		{
		default:
			row_scale = scale_row_to_temp;
			break;
		case 1: /* Image mask case */
			row_scale = scale_row_to_temp1;
			break;
		case 2: /* Greyscale with alpha case */
			row_scale = scale_row_to_temp2;
			break;
		case 4: /* RGBA */
			row_scale = scale_row_to_temp4;
			break;
		}
		max_row = 0;
		for (row = 0; row < contrib_rows->count; row++)
		{
			/*
			Which source rows do we need to have scaled into the
			temporary buffer in order to be able to do the final
			scale?
			*/
			int row_index = contrib_rows->index[row];
			int row_min = contrib_rows->index[row_index++];
			int row_len = contrib_rows->index[row_index++];
			while (max_row < row_min+row_len)
			{
				/* Scale another row */
				assert(max_row < src->h);
				DBUG(("scaling row %d to temp\n", max_row));
				(*row_scale)(&temp[temp_span*(max_row % temp_rows)], &src->samples[(flip_y ? (src->h-1-max_row): max_row)*src->w*src->n], contrib_cols);
				max_row++;
			}

			DBUG(("scaling row %d from temp\n", row));
			scale_row_from_temp(&output->samples[row*output->w*output->n], temp, contrib_rows, temp_span, row);
		}
		fz_free(temp);
	}

cleanup:
	fz_free(contrib_rows);
	fz_free(contrib_cols);
	return output;
}
