/*
This code does smooth scaling of a pixmap.
 
This function returns a new pixmap representing the area starting at (0,0)
given by taking the source pixmap src, scaling it to width w, and height h,
and then positioning it at (frac(x),frac(y)).
*/

#include "fitz.h"

#ifdef DEBUG_SCALING
#include <windows.h>
static void debug_print(const char *fmt, ...)
{
	va_list args;
	char text[256];
	va_start(args, fmt);
	vsprintf(text, fmt, args);
	va_end(args);
	OutputDebugStringA(text);
}
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

typedef struct fz_scalefilter_s fz_scalefilter;

struct fz_scalefilter_s
{
	int width;
	float (*fn)(fz_scalefilter *, float);
};

/* Image scale filters */

static float
triangle(fz_scalefilter *filter, float f)
{
	if (f >= 1)
		return 0;
	return 1-f;
}

static float
box(fz_scalefilter *filter, float f)
{
	if (f >= 0.5f)
		return 0;
	return 1;
}

static float
simple(fz_scalefilter *filter, float x)
{
	if (x >= 1)
		return 0;
	return 1 + (2*x - 3)*x*x;
}

static float
lanczos2(fz_scalefilter *filter, float x)
{
	if (x >= 2)
		return 0;
	return sinf(M_PI*x) * sinf(M_PI*x/2) / (M_PI*x) / (M_PI*x/2);
}

static float
lanczos3(fz_scalefilter *filter, float f)
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
mitchell(fz_scalefilter *filter, float x)
{
	if (x >= 2)
		return 0;
	if (x >= 1)
		return (32 + x*(-60 + x*(36 - 7*x)))/18;
	return (16 + x*x*(-36 + 21*x))/18;
}

fz_scalefilter fz_scalefilter_box = { 1, box };
fz_scalefilter fz_scalefilter_triangle = { 1, triangle };
fz_scalefilter fz_scalefilter_simple = { 1, simple };
fz_scalefilter fz_scalefilter_lanczos2 = { 2, lanczos2 };
fz_scalefilter fz_scalefilter_lanczos3 = { 3, lanczos3 };
fz_scalefilter fz_scalefilter_mitchell = { 2, mitchell };

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
	int index[1];
};

static fz_weights *
fz_newweights(fz_scalefilter *filter, int src_w, float dst_w, int dst_w_i)
{
	int max_len;
	fz_weights *weights;

	if (src_w > dst_w)
	{
		/* Scaling down, so there will be a maximum of
		 * 2*filterwidth*src_w/dst_w src pixels
		 * contributing to each dst pixel. */
		max_len = (int)ceilf((2 * filter->width * src_w)/dst_w);
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
	return weights;
}

static void
add_weight(fz_weights *weights, int j, int i, fz_scalefilter *filter,
	float x, float F, float G, int src_w, float dst_w)
{
	float dist = j + 0.5f - (x + (i + 0.5f)*dst_w/src_w);
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
#else
	if (i < 0)
		i = 0;
	else if (i >= src_w)
		i = src_w-1;
#endif

	DBUG(("add_weight[%d][%d] = %g dist=%g\n",j,i,f,dist));

	if (weights->count != j)
	{
		/* New line */
		assert(weights->count == j-1);
		weights->count++;
		if (j == 0)
			index = weights->index[0];
		else
		{
			index = weights->index[j-1];
			index += 2 + weights->index[index+1];
		}
		weights->index[j] = index; /* row pointer */
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
	int i;

	/* Copy into the temporary area */
	memcpy(&weights->index[tmp], &weights->index[idx], sizeof(int)*len);

	/* Pad out if required */
	assert(len <= max);
	assert(min+len <= src_w);
	if (len < max)
	{
		memset(&weights->index[tmp+len], 0, sizeof(int)*(max-len));
		len = max;
		if (min + len > src_w)
		{
			min = src_w - len;
			weights->index[idx-2] = min;
		}
		weights->index[idx-1] = len;
	}

	/* Copy back into the proper places */
	for (i = 0; i < len; i++)
	{
		weights->index[idx+((min+i) % max)] = weights->index[tmp+i];
	}
}

static void
check_weights(fz_weights *weights, int j)
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
	weights->index[maxidx-1] += 256-sum;
	DBUG(("total weight %d = %d\n", j, sum));
}

static fz_weights *
make_weights(int src_w, float x, float dst_w, fz_scalefilter *filter, int vertical)
{
	fz_weights *weights;
	int dst_w_int = (int)ceilf(x + dst_w);
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
	weights	= fz_newweights(filter, src_w, dst_w, dst_w_int);
	if (weights == NULL)
		return NULL;
	for (j = 0; j < dst_w_int; j++)
	{
		/* find the position of the centre of dst[j] in src space */
		float centre = (j+0.5f)*src_w/dst_w - 0.5f;
		int l, r;
		l = ceilf(centre - window);
		r = floorf(centre + window);
		DBUG(("%d: centre=%g l=%d r=%d\n", j, centre, l, r));
		for (; l <= r; l++)
		{
			add_weight(weights, j, l, filter, x, F, G, src_w, dst_w);
		}
		check_weights(weights, j);
		if (vertical)
		{
			reorder_weights(weights, j, src_w);
		}
	}
	weights->count++; /* weights->count = dst_w_int now */
	return weights;
}

static void
scale_row_to_temp(int *dst, unsigned char *src, fz_weights *weights, int n)
{
	int *contrib = &weights->index[weights->index[0]];
	int min, len, i, j;

	for (i=weights->count; i > 0; i--)
	{
		min = *contrib++;
		len = *contrib++;
		min *= n;
		for (j = 0; j < n; j++)
			dst[j] = 0;
		while (len-- > 0)
		{
			for (j = 0; j < n; j++)
				dst[j] += src[min++] * *contrib;
			contrib++;
		}
		dst += n;
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
		int min = 0;
		int val = 0;
		int len2 = len;
		int *contrib2 = contrib;

		while (len2-- > 0)
		{
			val += src[min] * *contrib2++;
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

fz_pixmap *
fz_smoothscalepixmap(fz_pixmap *src, float x, float y, float w, float h)
{
	fz_scalefilter *filter = &fz_scalefilter_simple;
	fz_weights *contrib_rows = NULL;
	fz_weights *contrib_cols = NULL;
	fz_pixmap *output = NULL;
	int *temp = NULL;
	int max_row, temp_span, temp_rows, row;

	x -= floorf(x);
	y -= floorf(y);

	DBUG(("Scale: (%d,%d) to (%g,%g) at (%g,%g)\n",src->w,src->h,w,h,x,y));
	/* Step 1: Calculate the weights for columns and rows */
	contrib_cols = make_weights(src->w, x, w, filter, 0);
	if (contrib_cols == NULL)
		goto cleanup;
	contrib_rows = make_weights(src->h, y, h, filter, 1);
	if (contrib_rows == NULL)
		goto cleanup;

	temp_span = contrib_cols->count * src->n;
	temp_rows = contrib_rows->max_len;
	temp = fz_malloc(sizeof(int)*temp_span*temp_rows);
	if (temp == NULL)
		goto cleanup;

	output = fz_newpixmap(src->colorspace, 0, 0, contrib_cols->count, contrib_rows->count);
	if (output == NULL)
		goto cleanup;

	/* Step 2: Apply the weights */
	max_row = 0;
	for (row = 0; row < contrib_rows->count; row++)
	{
		/*
		Which source rows do we need to have scaled into the temporary
		buffer in order to be able to do the final scale?
		*/
		int row_index = contrib_rows->index[row];
		int row_min = contrib_rows->index[row_index++];
		int row_len = contrib_rows->index[row_index++];
		while (max_row < row_min+row_len)
		{
			/* Scale another row */
			assert(max_row < src->h);
			DBUG(("scaling row %d to temp\n", max_row));
			scale_row_to_temp(&temp[temp_span*(max_row % temp_rows)], &src->samples[max_row*src->w*src->n], contrib_cols, src->n);
			max_row++;
		}

		DBUG(("scaling row %d from temp\n", row));
		scale_row_from_temp(&output->samples[row*output->w*output->n], temp, contrib_rows, temp_span, row);
	}

cleanup:
	fz_free(contrib_rows);
	fz_free(contrib_cols);
	fz_free(temp);
	return output;
}
