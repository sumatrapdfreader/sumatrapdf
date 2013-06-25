/*
 * The CMap data structure here is constructed on the fly by
 * adding simple range-to-range mappings. Then the data structure
 * is optimized to contain both range-to-range and range-to-table
 * lookups.
 *
 * Any one-to-many mappings are inserted as one-to-table
 * lookups in the beginning, and are not affected by the optimization
 * stage.
 *
 * There is a special function to add a 256-length range-to-table mapping.
 * The ranges do not have to be added in order.
 *
 * This code can be a lot simpler if we don't care about wasting memory,
 * or can trust the parser to give us optimal mappings.
 */

#include "mupdf/pdf.h"

/* Macros for accessing the combined extent_flags field */
#define pdf_range_high(r) ((r)->low + ((r)->extent_flags >> 2))
#define pdf_range_flags(r) ((r)->extent_flags & 3)
#define pdf_range_set_high(r, h) \
	((r)->extent_flags = (((r)->extent_flags & 3) | ((h - (r)->low) << 2)))
#define pdf_range_set_flags(r, f) \
	((r)->extent_flags = (((r)->extent_flags & ~3) | f))

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
	fz_free(ctx, cmap->table);
	fz_free(ctx, cmap);
}

pdf_cmap *
pdf_new_cmap(fz_context *ctx)
{
	pdf_cmap *cmap;

	cmap = fz_malloc_struct(ctx, pdf_cmap);
	FZ_INIT_STORABLE(cmap, 1, pdf_free_cmap_imp);

	strcpy(cmap->cmap_name, "");
	strcpy(cmap->usecmap_name, "");
	cmap->usecmap = NULL;
	cmap->wmode = 0;
	cmap->codespace_len = 0;

	cmap->rlen = 0;
	cmap->rcap = 0;
	cmap->ranges = NULL;

	cmap->tlen = 0;
	cmap->tcap = 0;
	cmap->table = NULL;

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

#ifndef NDEBUG
void
pdf_print_cmap(fz_context *ctx, pdf_cmap *cmap)
{
	int i, k, n;

	printf("cmap $%p /%s {\n", (void *) cmap, cmap->cmap_name);

	if (cmap->usecmap_name[0])
		printf("\tusecmap /%s\n", cmap->usecmap_name);
	if (cmap->usecmap)
		printf("\tusecmap $%p\n", (void *) cmap->usecmap);

	printf("\twmode %d\n", cmap->wmode);

	printf("\tcodespaces {\n");
	for (i = 0; i < cmap->codespace_len; i++)
	{
		printf("\t\t<%x> <%x>\n", cmap->codespace[i].low, cmap->codespace[i].high);
	}
	printf("\t}\n");

	printf("\tranges (%d,%d) {\n", cmap->rlen, cmap->tlen);
	for (i = 0; i < cmap->rlen; i++)
	{
		pdf_range *r = &cmap->ranges[i];
		printf("\t\t<%04x> <%04x> ", r->low, pdf_range_high(r));
		if (pdf_range_flags(r) == PDF_CMAP_TABLE)
		{
			printf("[ ");
			for (k = 0; k < pdf_range_high(r) - r->low + 1; k++)
				printf("%d ", cmap->table[r->offset + k]);
			printf("]\n");
		}
		else if (pdf_range_flags(r) == PDF_CMAP_MULTI)
		{
			printf("< ");
			n = cmap->table[r->offset];
			for (k = 0; k < n; k++)
				printf("%04x ", cmap->table[r->offset + 1 + k]);
			printf(">\n");
		}
		else
			printf("%d\n", r->offset);
	}
	printf("\t}\n}\n");
}
#endif

/*
 * Add a codespacerange section.
 * These ranges are used by pdf_decode_cmap to decode
 * multi-byte encoded strings.
 */
void
pdf_add_codespace(fz_context *ctx, pdf_cmap *cmap, int low, int high, int n)
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
 * Add an integer to the table.
 */
static void
add_table(fz_context *ctx, pdf_cmap *cmap, int value)
{
	if (cmap->tlen >= USHRT_MAX + 1)
	{
		fz_warn(ctx, "cmap table is full; ignoring additional entries");
		return;
	}
	if (cmap->tlen + 1 > cmap->tcap)
	{
		int new_cap = cmap->tcap > 1 ? (cmap->tcap * 3) / 2 : 256;
		cmap->table = fz_resize_array(ctx, cmap->table, new_cap, sizeof(unsigned short));
		cmap->tcap = new_cap;
	}
	cmap->table[cmap->tlen++] = value;
}

/*
 * Add a range.
 */
static void
add_range(fz_context *ctx, pdf_cmap *cmap, int low, int high, int flag, int offset)
{
	/* Sanity check ranges */
	if (low < 0 || low > 65535 || high < 0 || high > 65535 || low > high)
	{
		fz_warn(ctx, "range limits out of range in cmap %s", cmap->cmap_name);
		return;
	}
	/* If the range is too large to be represented, split it */
	if (high - low > 0x3fff)
	{
		add_range(ctx, cmap, low, low+0x3fff, flag, offset);
		add_range(ctx, cmap, low+0x3fff, high, flag, offset+0x3fff);
		return;
	}
	if (cmap->rlen + 1 > cmap->rcap)
	{
		int new_cap = cmap->rcap > 1 ? (cmap->rcap * 3) / 2 : 256;
		cmap->ranges = fz_resize_array(ctx, cmap->ranges, new_cap, sizeof(pdf_range));
		cmap->rcap = new_cap;
	}
	cmap->ranges[cmap->rlen].low = low;
	pdf_range_set_high(&cmap->ranges[cmap->rlen], high);
	pdf_range_set_flags(&cmap->ranges[cmap->rlen], flag);
	cmap->ranges[cmap->rlen].offset = offset;
	cmap->rlen ++;
}

/*
 * Add a range-to-table mapping.
 */
void
pdf_map_range_to_table(fz_context *ctx, pdf_cmap *cmap, int low, int *table, int len)
{
	int i;
	int high = low + len;
	int offset = cmap->tlen;
	if (cmap->tlen + len >= USHRT_MAX + 1)
		fz_warn(ctx, "cannot map range to table; table is full");
	else
	{
		for (i = 0; i < len; i++)
			add_table(ctx, cmap, table[i]);
		add_range(ctx, cmap, low, high, PDF_CMAP_TABLE, offset);
	}
}

/*
 * Add a range of contiguous one-to-one mappings (ie 1..5 maps to 21..25)
 */
void
pdf_map_range_to_range(fz_context *ctx, pdf_cmap *cmap, int low, int high, int offset)
{
	add_range(ctx, cmap, low, high, high - low == 0 ? PDF_CMAP_SINGLE : PDF_CMAP_RANGE, offset);
}

/*
 * Add a single one-to-many mapping.
 */
void
pdf_map_one_to_many(fz_context *ctx, pdf_cmap *cmap, int low, int *values, int len)
{
	int offset, i;

	if (len == 1)
	{
		add_range(ctx, cmap, low, low, PDF_CMAP_SINGLE, values[0]);
		return;
	}

	if (len > 8)
	{
		fz_warn(ctx, "one to many mapping is too large (%d); truncating", len);
		len = 8;
	}

	if (len == 2 &&
		values[0] >= 0xD800 && values[0] <= 0xDBFF &&
		values[1] >= 0xDC00 && values[1] <= 0xDFFF)
	{
		fz_warn(ctx, "ignoring surrogate pair mapping in cmap %s", cmap->cmap_name);
		return;
	}

	if (cmap->tlen + len + 1 >= USHRT_MAX + 1)
		fz_warn(ctx, "cannot map one to many; table is full");
	else
	{
		offset = cmap->tlen;
		add_table(ctx, cmap, len);
		for (i = 0; i < len; i++)
			add_table(ctx, cmap, values[i]);
		add_range(ctx, cmap, low, low, PDF_CMAP_MULTI, offset);
	}
}

/*
 * Sort the input ranges.
 * Merge contiguous input ranges to range-to-range if the output is contiguous.
 * Merge contiguous input ranges to range-to-table if the output is random.
 */

static int cmprange(const void *va, const void *vb)
{
	return ((const pdf_range*)va)->low - ((const pdf_range*)vb)->low;
}

void
pdf_sort_cmap(fz_context *ctx, pdf_cmap *cmap)
{
	pdf_range *a; /* last written range on output */
	pdf_range *b; /* current range examined on input */

	if (cmap->rlen == 0)
		return;

	qsort(cmap->ranges, cmap->rlen, sizeof(pdf_range), cmprange);

	if (cmap->tlen >= USHRT_MAX + 1)
	{
		fz_warn(ctx, "cmap table is full; will not combine ranges");
		return;
	}

	a = cmap->ranges;
	b = cmap->ranges + 1;

	while (b < cmap->ranges + cmap->rlen)
	{
		/* ignore one-to-many mappings */
		if (pdf_range_flags(b) == PDF_CMAP_MULTI)
		{
			*(++a) = *b;
		}

		/* input contiguous */
		else if (pdf_range_high(a) + 1 == b->low)
		{
			/* output contiguous */
			if (pdf_range_high(a) - a->low + a->offset + 1 == b->offset)
			{
				/* SR -> R and SS -> R and RR -> R and RS -> R */
				if ((pdf_range_flags(a) == PDF_CMAP_SINGLE || pdf_range_flags(a) == PDF_CMAP_RANGE) && (pdf_range_high(b) - a->low <= 0x3fff))
				{
					pdf_range_set_flags(a, PDF_CMAP_RANGE);
					pdf_range_set_high(a, pdf_range_high(b));
				}

				/* LS -> L */
				else if (pdf_range_flags(a) == PDF_CMAP_TABLE && pdf_range_flags(b) == PDF_CMAP_SINGLE && (pdf_range_high(b) - a->low <= 0x3fff))
				{
					pdf_range_set_high(a, pdf_range_high(b));
					add_table(ctx, cmap, b->offset);
				}

				/* LR -> LR */
				else if (pdf_range_flags(a) == PDF_CMAP_TABLE && pdf_range_flags(b) == PDF_CMAP_RANGE)
				{
					*(++a) = *b;
				}

				/* XX -> XX */
				else
				{
					*(++a) = *b;
				}
			}

			/* output separated */
			else
			{
				/* SS -> L */
				if (pdf_range_flags(a) == PDF_CMAP_SINGLE && pdf_range_flags(b) == PDF_CMAP_SINGLE)
				{
					pdf_range_set_flags(a, PDF_CMAP_TABLE);
					pdf_range_set_high(a, pdf_range_high(b));
					add_table(ctx, cmap, a->offset);
					add_table(ctx, cmap, b->offset);
					a->offset = cmap->tlen - 2;
				}

				/* LS -> L */
				else if (pdf_range_flags(a) == PDF_CMAP_TABLE && pdf_range_flags(b) == PDF_CMAP_SINGLE && (pdf_range_high(b) - a->low <= 0x3fff))
				{
					pdf_range_set_high(a, pdf_range_high(b));
					add_table(ctx, cmap, b->offset);
				}

				/* XX -> XX */
				else
				{
					*(++a) = *b;
				}
			}
		}

		/* input separated: XX -> XX */
		else
		{
			*(++a) = *b;
		}

		b ++;
	}

	cmap->rlen = a - cmap->ranges + 1;
}

/*
 * Lookup the mapping of a codepoint.
 */
int
pdf_lookup_cmap(pdf_cmap *cmap, int cpt)
{
	int l = 0;
	int r = cmap->rlen - 1;
	int m;

	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < cmap->ranges[m].low)
			r = m - 1;
		else if (cpt > pdf_range_high(&cmap->ranges[m]))
			l = m + 1;
		else
		{
			int i = cpt - cmap->ranges[m].low + cmap->ranges[m].offset;
			if (pdf_range_flags(&cmap->ranges[m]) == PDF_CMAP_TABLE)
				return cmap->table[i];
			if (pdf_range_flags(&cmap->ranges[m]) == PDF_CMAP_MULTI)
				return -1; /* should use lookup_cmap_full */
			return i;
		}
	}

	if (cmap->usecmap)
		return pdf_lookup_cmap(cmap->usecmap, cpt);

	return -1;
}

int
pdf_lookup_cmap_full(pdf_cmap *cmap, int cpt, int *out)
{
	int i, k, n;
	int l = 0;
	int r = cmap->rlen - 1;
	int m;

	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < cmap->ranges[m].low)
			r = m - 1;
		else if (cpt > pdf_range_high(&cmap->ranges[m]))
			l = m + 1;
		else
		{
			k = cpt - cmap->ranges[m].low + cmap->ranges[m].offset;
			if (pdf_range_flags(&cmap->ranges[m]) == PDF_CMAP_TABLE)
			{
				out[0] = cmap->table[k];
				return 1;
			}
			else if (pdf_range_flags(&cmap->ranges[m]) == PDF_CMAP_MULTI)
			{
				n = cmap->ranges[m].offset;
				for (i = 0; i < cmap->table[n]; i++)
					out[i] = cmap->table[n + i + 1];
				return cmap->table[n];
			}
			else
			{
				out[0] = k;
				return 1;
			}
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
pdf_decode_cmap(pdf_cmap *cmap, unsigned char *buf, int *cpt)
{
	int k, n, c;

	c = 0;
	for (n = 0; n < 4; n++)
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
