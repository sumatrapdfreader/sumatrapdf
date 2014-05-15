#include "mupdf/pdf.h"

/*
 * Allocate, destroy and simple parameters.
 */

void
pdf_free_cmap_imp(fz_context *ctx, fz_storable *cmap_)
{
	pdf_cmap *cmap = (pdf_cmap *)cmap_;
	if (cmap->usecmap)
		pdf_drop_cmap(ctx, cmap->usecmap);
	fz_free(ctx, cmap->ranges);
	fz_free(ctx, cmap->xranges);
	fz_free(ctx, cmap->mranges);
	fz_free(ctx, cmap);
}

pdf_cmap *
pdf_new_cmap(fz_context *ctx)
{
	pdf_cmap *cmap = fz_malloc_struct(ctx, pdf_cmap);
	FZ_INIT_STORABLE(cmap, 1, pdf_free_cmap_imp);
	return cmap;
}

/* Could be a macro for speed */
pdf_cmap *
pdf_keep_cmap(fz_context *ctx, pdf_cmap *cmap)
{
	return (pdf_cmap *)fz_keep_storable(ctx, &cmap->storable);
}

/* Could be a macro for speed */
void
pdf_drop_cmap(fz_context *ctx, pdf_cmap *cmap)
{
	fz_drop_storable(ctx, &cmap->storable);
}

void
pdf_set_usecmap(fz_context *ctx, pdf_cmap *cmap, pdf_cmap *usecmap)
{
	int i;

	if (cmap->usecmap)
		pdf_drop_cmap(ctx, cmap->usecmap);
	cmap->usecmap = pdf_keep_cmap(ctx, usecmap);

	if (cmap->codespace_len == 0)
	{
		cmap->codespace_len = usecmap->codespace_len;
		for (i = 0; i < usecmap->codespace_len; i++)
			cmap->codespace[i] = usecmap->codespace[i];
	}
}

int
pdf_cmap_wmode(fz_context *ctx, pdf_cmap *cmap)
{
	return cmap->wmode;
}

void
pdf_set_cmap_wmode(fz_context *ctx, pdf_cmap *cmap, int wmode)
{
	cmap->wmode = wmode;
}

/*
 * Add a codespacerange section.
 * These ranges are used by pdf_decode_cmap to decode
 * multi-byte encoded strings.
 */
void
pdf_add_codespace(fz_context *ctx, pdf_cmap *cmap, unsigned int low, unsigned int high, int n)
{
	if (cmap->codespace_len + 1 == nelem(cmap->codespace))
	{
		fz_warn(ctx, "assert: too many code space ranges");
		return;
	}

	cmap->codespace[cmap->codespace_len].n = n;
	cmap->codespace[cmap->codespace_len].low = low;
	cmap->codespace[cmap->codespace_len].high = high;
	cmap->codespace_len ++;
}

/*
 * Add a range.
 */
static void
add_range(fz_context *ctx, pdf_cmap *cmap, unsigned int low, unsigned int high, unsigned int out)
{
	if (low > high)
	{
		fz_warn(ctx, "range limits out of range in cmap %s", cmap->cmap_name);
		return;
	}

	if (low <= 0xFFFF && high <= 0xFFFF && out <= 0xFFFF)
	{
		if (cmap->rlen + 1 > cmap->rcap)
		{
			int new_cap = cmap->rcap ? cmap->rcap * 2 : 256;
			cmap->ranges = fz_resize_array(ctx, cmap->ranges, new_cap, sizeof *cmap->ranges);
			cmap->rcap = new_cap;
		}
		cmap->ranges[cmap->rlen].low = low;
		cmap->ranges[cmap->rlen].high = high;
		cmap->ranges[cmap->rlen].out = out;
		cmap->rlen++;
	}
	else
	{
		if (cmap->xlen + 1 > cmap->xcap)
		{
			int new_cap = cmap->xcap ? cmap->xcap * 2 : 256;
			cmap->xranges = fz_resize_array(ctx, cmap->xranges, new_cap, sizeof *cmap->xranges);
			cmap->xcap = new_cap;
		}
		cmap->xranges[cmap->xlen].low = low;
		cmap->xranges[cmap->xlen].high = high;
		cmap->xranges[cmap->xlen].out = out;
		cmap->xlen++;
	}
}

/*
 * Add a one-to-many mapping.
 */
static void
add_mrange(fz_context *ctx, pdf_cmap *cmap, unsigned int low, int *out, int len)
{
	int i;
	if (cmap->mlen + 1 > cmap->mcap)
	{
		int new_cap = cmap->mcap ? cmap->mcap * 2 : 256;
		cmap->mranges = fz_resize_array(ctx, cmap->mranges, new_cap, sizeof *cmap->mranges);
		cmap->mcap = new_cap;
	}
	cmap->mranges[cmap->mlen].low = low;
	cmap->mranges[cmap->mlen].len = len;
	for (i = 0; i < len; ++i)
		cmap->mranges[cmap->mlen].out[i] = out[i];
	for (; i < PDF_MRANGE_CAP; ++i)
		cmap->mranges[cmap->mlen].out[i] = 0;
	cmap->mlen++;
}

/*
 * Add a range-to-table mapping.
 */
void
pdf_map_range_to_table(fz_context *ctx, pdf_cmap *cmap, unsigned int low, int *table, int len)
{
	int i;
	for (i = 0; i < len; i++)
		add_range(ctx, cmap, low + i, low + i, table[i]);
}

/*
 * Add a range of contiguous one-to-one mappings (ie 1..5 maps to 21..25)
 */
void
pdf_map_range_to_range(fz_context *ctx, pdf_cmap *cmap, unsigned int low, unsigned int high, int out)
{
	add_range(ctx, cmap, low, high, out);
}

/*
 * Add a single one-to-many mapping.
 */
void
pdf_map_one_to_many(fz_context *ctx, pdf_cmap *cmap, unsigned int low, int *values, int len)
{
	if (len == 1)
	{
		add_range(ctx, cmap, low, low, values[0]);
		return;
	}

	/* Decode unicode surrogate pairs. */
	/* Only the *-UCS2 CMaps use one-to-many mappings, so assuming unicode should be safe. */
	if (len == 2 &&
		values[0] >= 0xD800 && values[0] <= 0xDBFF &&
		values[1] >= 0xDC00 && values[1] <= 0xDFFF)
	{
		int rune = ((values[0] - 0xD800) << 10) + (values[1] - 0xDC00) + 0x10000;
		add_range(ctx, cmap, low, low, rune);
		return;
	}

	if (len > PDF_MRANGE_CAP)
	{
		fz_warn(ctx, "ignoring one to many mapping in cmap %s", cmap->cmap_name);
		return;
	}

	add_mrange(ctx, cmap, low, values, len);
}

/*
 * Sort the input ranges.
 * Merge contiguous ranges.
 */

static int cmprange(const void *va, const void *vb)
{
	unsigned int a = ((const pdf_range*)va)->low;
	unsigned int b = ((const pdf_range*)vb)->low;
	return a < b ? -1 : a > b ? 1 : 0;
}

static int cmpxrange(const void *va, const void *vb)
{
	unsigned int a = ((const pdf_xrange*)va)->low;
	unsigned int b = ((const pdf_xrange*)vb)->low;
	return a < b ? -1 : a > b ? 1 : 0;
}

static int cmpmrange(const void *va, const void *vb)
{
	unsigned int a = ((const pdf_mrange*)va)->low;
	unsigned int b = ((const pdf_mrange*)vb)->low;
	return a < b ? -1 : a > b ? 1 : 0;
}

void
pdf_sort_cmap(fz_context *ctx, pdf_cmap *cmap)
{
	pdf_range *a, *b;
	pdf_xrange *x, *y;

	if (cmap->rlen)
	{
		qsort(cmap->ranges, cmap->rlen, sizeof *cmap->ranges, cmprange);
		a = cmap->ranges;
		for (b = a + 1; b < cmap->ranges + cmap->rlen; ++b)
		{
			if (b->low == a->high + 1 && b->out == a->out + (a->high - a->low) + 1)
				a->high = b->high;
			else
				*(++a) = *b;
		}
		cmap->rlen = a - cmap->ranges + 1;
	}

	if (cmap->xlen)
	{
		qsort(cmap->xranges, cmap->xlen, sizeof *cmap->xranges, cmpxrange);
		x = cmap->xranges;
		for (y = x + 1; y < cmap->xranges + cmap->xlen; ++y)
		{
			if (y->low == x->high + 1 && y->out == x->out + (x->high - x->low) + 1)
				x->high = y->high;
			else
				*(++x) = *y;
		}
		cmap->xlen = x - cmap->xranges + 1;
	}

	if (cmap->mlen)
	{
		qsort(cmap->mranges, cmap->mlen, sizeof *cmap->mranges, cmpmrange);
	}
}

/*
 * Lookup the mapping of a codepoint.
 */
int
pdf_lookup_cmap(pdf_cmap *cmap, unsigned int cpt)
{
	pdf_range *ranges = cmap->ranges;
	pdf_xrange *xranges = cmap->xranges;
	int l, r, m;

	l = 0;
	r = cmap->rlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < ranges[m].low)
			r = m - 1;
		else if (cpt > ranges[m].high)
			l = m + 1;
		else
			return cpt - ranges[m].low + ranges[m].out;
	}

	l = 0;
	r = cmap->xlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < xranges[m].low)
			r = m - 1;
		else if (cpt > xranges[m].high)
			l = m + 1;
		else
			return cpt - xranges[m].low + xranges[m].out;
	}

	if (cmap->usecmap)
		return pdf_lookup_cmap(cmap->usecmap, cpt);

	return -1;
}

int
pdf_lookup_cmap_full(pdf_cmap *cmap, unsigned int cpt, int *out)
{
	pdf_range *ranges = cmap->ranges;
	pdf_xrange *xranges = cmap->xranges;
	pdf_mrange *mranges = cmap->mranges;
	unsigned int i;
	int l, r, m;

	l = 0;
	r = cmap->rlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < ranges[m].low)
			r = m - 1;
		else if (cpt > ranges[m].high)
			l = m + 1;
		else
		{
			out[0] = cpt - ranges[m].low + ranges[m].out;
			return 1;
		}
	}

	l = 0;
	r = cmap->xlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < xranges[m].low)
			r = m - 1;
		else if (cpt > xranges[m].high)
			l = m + 1;
		else
		{
			out[0] = cpt - xranges[m].low + xranges[m].out;
			return 1;
		}
	}

	l = 0;
	r = cmap->mlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < mranges[m].low)
			r = m - 1;
		else if (cpt > mranges[m].low)
			l = m + 1;
		else
		{
			for (i = 0; i < mranges[m].len; ++i)
				out[i] = mranges[m].out[i];
			return mranges[m].len;
		}
	}

	if (cmap->usecmap)
		return pdf_lookup_cmap_full(cmap->usecmap, cpt, out);

	return 0;
}

/*
 * Use the codespace ranges to extract a codepoint from a
 * multi-byte encoded string.
 */
int
pdf_decode_cmap(pdf_cmap *cmap, unsigned char *buf, unsigned char *end, unsigned int *cpt)
{
	unsigned int c;
	int k, n;
	int len = end - buf;

	if (len > 4)
		len = 4;

	c = 0;
	for (n = 0; n < len; n++)
	{
		c = (c << 8) | buf[n];
		for (k = 0; k < cmap->codespace_len; k++)
		{
			if (cmap->codespace[k].n == n + 1)
			{
				if (c >= cmap->codespace[k].low && c <= cmap->codespace[k].high)
				{
					*cpt = c;
					return n + 1;
				}
			}
		}
	}

	*cpt = 0;
	return 1;
}
