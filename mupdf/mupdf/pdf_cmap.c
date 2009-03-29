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

#include "fitz.h"
#include "mupdf.h"

/*
 * Allocate, destroy and simple parameters.
 */

fz_error
pdf_newcmap(pdf_cmap **cmapp)
{
    pdf_cmap *cmap;

    cmap = *cmapp = fz_malloc(sizeof(pdf_cmap));
    if (!cmap)
	return fz_rethrow(-1, "out of memory: cmap struct");

    cmap->refs = 1;
    strcpy(cmap->cmapname, "");

    strcpy(cmap->usecmapname, "");
    cmap->usecmap = nil;

    cmap->wmode = 0;

    cmap->ncspace = 0;

    cmap->rlen = 0;
    cmap->rcap = 0;
    cmap->ranges = nil;

    cmap->tlen = 0;
    cmap->tcap = 0;
    cmap->table = nil;

    return fz_okay;
}

pdf_cmap *
pdf_keepcmap(pdf_cmap *cmap)
{
    if (cmap->refs >= 0)
	cmap->refs ++;
    return cmap;
}

void
pdf_dropcmap(pdf_cmap *cmap)
{
    if (cmap->refs >= 0)
    {
	if (--cmap->refs == 0)
	{
	    if (cmap->usecmap)
		pdf_dropcmap(cmap->usecmap);
	    fz_free(cmap->ranges);
	    fz_free(cmap->table);
	    fz_free(cmap);
	}
    }
}

pdf_cmap *
pdf_getusecmap(pdf_cmap *cmap)
{
    return cmap->usecmap;
}

void
pdf_setusecmap(pdf_cmap *cmap, pdf_cmap *usecmap)
{
    int i;

    if (cmap->usecmap)
	pdf_dropcmap(cmap->usecmap);
    cmap->usecmap = pdf_keepcmap(usecmap);

    if (cmap->ncspace == 0)
    {
	cmap->ncspace = usecmap->ncspace;
	for (i = 0; i < usecmap->ncspace; i++)
	    cmap->cspace[i] = usecmap->cspace[i];
    }
}

int
pdf_getwmode(pdf_cmap *cmap)
{
    return cmap->wmode;
}

void
pdf_setwmode(pdf_cmap *cmap, int wmode)
{
    cmap->wmode = wmode;
}

void
pdf_debugcmap(pdf_cmap *cmap)
{
    int i, k, n;

    printf("cmap $%p /%s {\n", (void *) cmap, cmap->cmapname);

    if (cmap->usecmapname[0])
	printf("  usecmap /%s\n", cmap->usecmapname);
    if (cmap->usecmap)
	printf("  usecmap $%p\n", (void *) cmap->usecmap);

    printf("  wmode %d\n", cmap->wmode);

    printf("  codespaces {\n");
    for (i = 0; i < cmap->ncspace; i++)
    {
	printf("    <%x> <%x>\n", cmap->cspace[i].low, cmap->cspace[i].high);
    }
    printf("  }\n");

    printf("  ranges (%d,%d) {\n", cmap->rlen, cmap->tlen);
    for (i = 0; i < cmap->rlen; i++)
    {
	pdf_range *r = &cmap->ranges[i];
	printf("    <%04x> <%04x> ", r->low, r->high);
	if (r->flag == PDF_CMAP_TABLE)
	{
	    printf("[ ");
	    for (k = 0; k < r->high - r->low + 1; k++)
		printf("%d ", cmap->table[r->offset + k]);
	    printf("]\n");
	}
	else if (r->flag == PDF_CMAP_MULTI)
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
    printf("  }\n}\n");
}

/*
 * Add a codespacerange section.
 * These ranges are used by pdf_decodecmap to decode
 * multi-byte encoded strings.
 */
fz_error
pdf_addcodespace(pdf_cmap *cmap, int low, int high, int n)
{
    if (cmap->ncspace + 1 == nelem(cmap->cspace))
	return fz_throw("assert: too many code space ranges");

    cmap->cspace[cmap->ncspace].n = n;
    cmap->cspace[cmap->ncspace].low = low;
    cmap->cspace[cmap->ncspace].high = high;

    cmap->ncspace ++;

    return fz_okay;
}

/*
 * Add an integer to the table.
 */
static fz_error
addtable(pdf_cmap *cmap, int value)
{
    if (cmap->tlen + 1 > cmap->tcap)
    {
	int newcap = cmap->tcap == 0 ? 256 : cmap->tcap * 2;
	unsigned short *newtable = fz_realloc(cmap->table, newcap * sizeof(unsigned short));
	if (!newtable)
	    return fz_rethrow(-1, "out of memory: cmap table");
	cmap->tcap = newcap;
	cmap->table = newtable;
    }

    cmap->table[cmap->tlen++] = value;

    return fz_okay;
}

/*
 * Add a range.
 */
static fz_error
addrange(pdf_cmap *cmap, int low, int high, int flag, int offset)
{
    if (cmap->rlen + 1 > cmap->rcap)
    {
	pdf_range *newranges;
	int newcap = cmap->rcap == 0 ? 256 : cmap->rcap * 2;
	newranges = fz_realloc(cmap->ranges, newcap * sizeof(pdf_range));
	if (!newranges)
	    return fz_rethrow(-1, "out of memory: cmap ranges");
	cmap->rcap = newcap;
	cmap->ranges = newranges;
    }

    cmap->ranges[cmap->rlen].low = low;
    cmap->ranges[cmap->rlen].high = high;
    cmap->ranges[cmap->rlen].flag = flag;
    cmap->ranges[cmap->rlen].offset = offset;
    cmap->rlen ++;

    return fz_okay;
}

/*
 * Add a range-to-table mapping.
 */
fz_error
pdf_maprangetotable(pdf_cmap *cmap, int low, int *table, int len)
{
    fz_error error;
    int offset;
    int high;
    int i;

    high = low + len;
    offset = cmap->tlen;

    for (i = 0; i < len; i++)
    {
	error = addtable(cmap, table[i]);
	if (error)
	    return fz_rethrow(error, "cannot add range-to-table index");
    }

    error = addrange(cmap, low, high, PDF_CMAP_TABLE, offset);
    if (error)
	return fz_rethrow(error, "cannot add range-to-table range");

    return fz_okay;
}

/*
 * Add a range of contiguous one-to-one mappings (ie 1..5 maps to 21..25)
 */
fz_error
pdf_maprangetorange(pdf_cmap *cmap, int low, int high, int offset)
{
    fz_error error;
    error = addrange(cmap, low, high, high - low == 0 ? PDF_CMAP_SINGLE : PDF_CMAP_RANGE, offset);
    if (error)
	return fz_rethrow(error, "cannot add range-to-range mapping");
    return fz_okay;
}

/*
 * Add a single one-to-many mapping.
 */
fz_error
pdf_maponetomany(pdf_cmap *cmap, int low, int *values, int len)
{
    fz_error error;
    int offset;
    int i;

    if (len == 1)
    {
	error = addrange(cmap, low, low, PDF_CMAP_SINGLE, values[0]);
	if (error)
	    return fz_rethrow(error, "cannot add one-to-one mapping");
	return fz_okay;
    }

    offset = cmap->tlen;

    error = addtable(cmap, len);
    if (error)
	return fz_rethrow(error, "cannot add one-to-many table length");

    for (i = 0; i < len; i++)
    {
	error = addtable(cmap, values[i]);
	if (error)
	    return fz_rethrow(error, "cannot add one-to-many table index");
    }

    error = addrange(cmap, low, low, PDF_CMAP_MULTI, offset);
    if (error)
	return fz_rethrow(error, "cannot add one-to-many mapping");

    return fz_okay;
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

fz_error
pdf_sortcmap(pdf_cmap *cmap)
{
    fz_error error;
    pdf_range *newranges;
    unsigned short *newtable;
    pdf_range *a;			/* last written range on output */
    pdf_range *b;			/* current range examined on input */

    if (cmap->rlen == 0)
	return fz_okay;

    qsort(cmap->ranges, cmap->rlen, sizeof(pdf_range), cmprange);

    a = cmap->ranges;
    b = cmap->ranges + 1;

    while (b < cmap->ranges + cmap->rlen)
    {
	/* ignore one-to-many mappings */
	if (b->flag == PDF_CMAP_MULTI)
	{
	    *(++a) = *b;
	}

	/* input contiguous */
	else if (a->high + 1 == b->low)
	{
	    /* output contiguous */
	    if (a->high - a->low + a->offset + 1 == b->offset)
	    {
		/* SR -> R and SS -> R and RR -> R and RS -> R */
		if (a->flag == PDF_CMAP_SINGLE || a->flag == PDF_CMAP_RANGE)
		{
		    a->flag = PDF_CMAP_RANGE;
		    a->high = b->high;
		}

		/* LS -> L */
		else if (a->flag == PDF_CMAP_TABLE && b->flag == PDF_CMAP_SINGLE)
		{
		    a->high = b->high;
		    error = addtable(cmap, b->offset);
		    if (error)
			return fz_rethrow(error, "cannot convert LS -> L");
		}

		/* LR -> LR */
		else if (a->flag == PDF_CMAP_TABLE && b->flag == PDF_CMAP_RANGE)
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
		if (a->flag == PDF_CMAP_SINGLE && b->flag == PDF_CMAP_SINGLE)
		{
		    a->flag = PDF_CMAP_TABLE;
		    a->high = b->high;

		    error = addtable(cmap, a->offset);
		    if (error)
			return fz_rethrow(error, "cannot convert SS -> L");

		    error = addtable(cmap, b->offset);
		    if (error)
			return fz_rethrow(error, "cannot convert SS -> L");

		    a->offset = cmap->tlen - 2;
		}

		/* LS -> L */
		else if (a->flag == PDF_CMAP_TABLE && b->flag == PDF_CMAP_SINGLE)
		{
		    a->high = b->high;
		    error = addtable(cmap, b->offset);
		    if (error)
			return fz_rethrow(error, "cannot convert LS -> L");
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

    newranges = fz_realloc(cmap->ranges, cmap->rlen * sizeof(pdf_range));
    if (!newranges)
	return fz_rethrow(-1, "out of memory: cmap ranges");
    cmap->rcap = cmap->rlen;
    cmap->ranges = newranges;

    if (cmap->tlen)
    {
	newtable = fz_realloc(cmap->table, cmap->tlen * sizeof(unsigned short));
	if (!newtable)
	    return fz_rethrow(-1, "out of memory: cmap table");
	cmap->tcap = cmap->tlen;
	cmap->table = newtable;
    }

    return fz_okay;
}

/*
 * Lookup the mapping of a codepoint.
 */
int
pdf_lookupcmap(pdf_cmap *cmap, int cpt)
{
    int l = 0;
    int r = cmap->rlen - 1;
    int m;

    while (l <= r)
    {
	m = (l + r) >> 1;
	if (cpt < cmap->ranges[m].low)
	    r = m - 1;
	else if (cpt > cmap->ranges[m].high)
	    l = m + 1;
	else
	{
	    int i = cpt - cmap->ranges[m].low + cmap->ranges[m].offset;
	    if (cmap->ranges[m].flag == PDF_CMAP_TABLE)
		return cmap->table[i];
	    if (cmap->ranges[m].flag == PDF_CMAP_MULTI)
		return -1;
	    return i;
	}
    }

    if (cmap->usecmap)
	return pdf_lookupcmap(cmap->usecmap, cpt);

    return -1;
}

/*
 * Use the codespace ranges to extract a codepoint from a
 * multi-byte encoded string.
 */
unsigned char *
pdf_decodecmap(pdf_cmap *cmap, unsigned char *buf, int *cpt)
{
    int k, n, c;

    c = 0;
    for (n = 0; n < 4; n++)
    {
	c = (c << 8) | buf[n];
	for (k = 0; k < cmap->ncspace; k++)
	{
	    if (cmap->cspace[k].n == n + 1)
	    {
		if (c >= cmap->cspace[k].low && c <= cmap->cspace[k].high)
		{
		    *cpt = c;
		    return buf + n + 1;
		}
	    }
	}
    }

    *cpt = 0;
    return buf + 1;
}

