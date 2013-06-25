#include "mupdf/fitz.h"

/* Assemble span soup into blocks and lines. */

#define MY_EPSILON 0.001f

#undef DEBUG_LINE_HEIGHTS
#undef DEBUG_MASKS
#undef DEBUG_ALIGN
#undef DEBUG_INDENTS

#undef SPOT_LINE_NUMBERS

typedef struct line_height_s
{
	float height;
	int count;
	fz_text_style *style;
} line_height;

typedef struct line_heights_s
{
	fz_context *ctx;
	int cap;
	int len;
	line_height *lh;
} line_heights;

static line_heights *
new_line_heights(fz_context *ctx)
{
	line_heights *lh = fz_malloc_struct(ctx, line_heights);
	lh->ctx = ctx;
	return lh;
}

static void
free_line_heights(line_heights *lh)
{
	if (!lh)
		return;
	fz_free(lh->ctx, lh->lh);
	fz_free(lh->ctx, lh);
}

static void
insert_line_height(line_heights *lh, fz_text_style *style, float height)
{
	int i;

#ifdef DEBUG_LINE_HEIGHTS
	printf("style=%x height=%g\n", style, height);
#endif

	/* If we have one already, add it in */
	for (i=0; i < lh->len; i++)
	{
		/* Match if we are within 5% */
		if (lh->lh[i].style == style && lh->lh[i].height * 0.95 <= height && lh->lh[i].height * 1.05 >= height)
		{
			/* Ensure that the average height is correct */
			lh->lh[i].height = (lh->lh[i].height * lh->lh[i].count + height) / (lh->lh[i].count+1);
			lh->lh[i].count++;
			return;
		}
	}

	/* Otherwise extend (if required) and add it */
	if (lh->cap == lh->len)
	{
		int newcap = (lh->cap ? lh->cap * 2 : 4);
		lh->lh = fz_resize_array(lh->ctx, lh->lh, newcap, sizeof(line_height));
		lh->cap = newcap;
	}

	lh->lh[lh->len].count = 1;
	lh->lh[lh->len].height = height;
	lh->lh[lh->len].style = style;
	lh->len++;
}

static void
cull_line_heights(line_heights *lh)
{
	int i, j, k;

#ifdef DEBUG_LINE_HEIGHTS
	printf("Before culling:\n");
	for (i = 0; i < lh->len; i++)
	{
		fz_text_style *style = lh->lh[i].style;
		printf("style=%x height=%g count=%d\n", style, lh->lh[i].height, lh->lh[i].count);
	}
#endif
	for (i = 0; i < lh->len; i++)
	{
		fz_text_style *style = lh->lh[i].style;
		int count = lh->lh[i].count;
		int max = i;

		/* Find the max for this style */
		for (j = i+1; j < lh->len; j++)
		{
			if (lh->lh[j].style == style && lh->lh[j].count > count)
			{
				max = j;
				count = lh->lh[j].count;
			}
		}

		/* Destroy all the ones other than the max */
		if (max != i)
		{
			lh->lh[i].count = count;
			lh->lh[i].height = lh->lh[max].height;
			lh->lh[max].count = 0;
		}
		j = i+1;
		for (k = j; k < lh->len; k++)
		{
			if (lh->lh[k].style != style)
				lh->lh[j++] = lh->lh[k];
		}
		lh->len = j;
	}
#ifdef DEBUG_LINE_HEIGHTS
	printf("After culling:\n");
	for (i = 0; i < lh->len; i++)
	{
		fz_text_style *style = lh->lh[i].style;
		printf("style=%x height=%g count=%d\n", style, lh->lh[i].height, lh->lh[i].count);
	}
#endif
}

static float
line_height_for_style(line_heights *lh, fz_text_style *style)
{
	int i;

	for (i=0; i < lh->len; i++)
	{
		if (lh->lh[i].style == style)
			return lh->lh[i].height;
	}
	return 0.0; /* Never reached */
}

static void
split_block(fz_context *ctx, fz_text_page *page, int block_num, int linenum)
{
	int split_len;
	fz_text_block *block, *block2;

	if (page->len == page->cap)
	{
		int new_cap = fz_maxi(16, page->cap * 2);
		page->blocks = fz_resize_array(ctx, page->blocks, new_cap, sizeof(*page->blocks));
		page->cap = new_cap;
	}

	memmove(page->blocks+block_num+1, page->blocks+block_num, (page->len - block_num)*sizeof(*page->blocks));
	page->len++;

	block2 = fz_malloc_struct(ctx, fz_text_block);
	block = page->blocks[block_num].u.text;

	page->blocks[block_num+1].type = FZ_PAGE_BLOCK_TEXT;
	page->blocks[block_num+1].u.text = block2;
	split_len = block->len - linenum;
	block2->bbox = block->bbox; /* FIXME! */
	block2->cap = 0;
	block2->len = 0;
	block2->lines = NULL;
	block2->lines = fz_malloc_array(ctx, split_len, sizeof(fz_text_line));
	block2->cap = block2->len;
	block2->len = split_len;
	block->len = linenum;
	memcpy(block2->lines, block->lines + linenum, split_len * sizeof(fz_text_line));
	block2->lines[0].distance = 0;
}

static inline int
is_unicode_wspace(int c)
{
	return (c == 9 || /* TAB */
		c == 0x0a || /* HT */
		c == 0x0b || /* LF */
		c == 0x0c || /* VT */
		c == 0x0d || /* FF */
		c == 0x20 || /* CR */
		c == 0x85 || /* NEL */
		c == 0xA0 || /* No break space */
		c == 0x1680 || /* Ogham space mark */
		c == 0x180E || /* Mongolian Vowel Separator */
		c == 0x2000 || /* En quad */
		c == 0x2001 || /* Em quad */
		c == 0x2002 || /* En space */
		c == 0x2003 || /* Em space */
		c == 0x2004 || /* Three-per-Em space */
		c == 0x2005 || /* Four-per-Em space */
		c == 0x2006 || /* Five-per-Em space */
		c == 0x2007 || /* Figure space */
		c == 0x2008 || /* Punctuation space */
		c == 0x2009 || /* Thin space */
		c == 0x200A || /* Hair space */
		c == 0x2028 || /* Line separator */
		c == 0x2029 || /* Paragraph separator */
		c == 0x202F || /* Narrow no-break space */
		c == 0x205F || /* Medium mathematical space */
		c == 0x3000); /* Ideographic space */
}

static inline int
is_unicode_bullet(int c)
{
	/* The last 2 aren't strictly bullets, but will do for our usage here */
	return (c == 0x2022 || /* Bullet */
		c == 0x2023 || /* Triangular bullet */
		c == 0x25e6 || /* White bullet */
		c == 0x2043 || /* Hyphen bullet */
		c == 0x2219 || /* Bullet operator */
		c == 149 || /* Ascii bullet */
		c == '*');
}

static inline int
is_number(int c)
{
	return ((c >= '0' && c <= '9') ||
		(c == '.'));
}

static inline int
is_latin_char(int c)
{
	return ((c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z'));
}

static inline int
is_roman(int c)
{
	return (c == 'i' || c == 'I' ||
		c == 'v' || c == 'V' ||
		c == 'x' || c == 'X' ||
		c == 'l' || c == 'L' ||
		c == 'c' || c == 'C' ||
		c == 'm' || c == 'M');
}

static int
is_list_entry(fz_text_line *line, fz_text_span *span, int *char_num_ptr)
{
	int char_num;
	fz_text_char *chr;

	/* First, skip over any whitespace */
	for (char_num = 0; char_num < span->len; char_num++)
	{
		chr = &span->text[char_num];
		if (!is_unicode_wspace(chr->c))
			break;
	}
	*char_num_ptr = char_num;

	if (span != line->first_span || char_num >= span->len)
		return 0;

	/* Now we check for various special cases, which we consider to mean
	 * that this is probably a list entry and therefore should always count
	 * as a separate paragraph (and hence not be entered in the line height
	 * table). */
	chr = &span->text[char_num];

	/* Is the first char on the line, a bullet point? */
	if (is_unicode_bullet(chr->c))
		return 1;

#ifdef SPOT_LINE_NUMBERS
	/* Is the entire first span a number? Or does it start with a number
	 * followed by ) or : ? Allow to involve single latin chars too. */
	if (is_number(chr->c) || is_latin_char(chr->c))
	{
		int cn = char_num;
		int met_char = is_latin_char(chr->c);
		for (cn = char_num+1; cn < span->len; cn++)
		{
			fz_text_char *chr2 = &span->text[cn];

			if (is_latin_char(chr2->c) && !met_char)
			{
				met_char = 1;
				continue;
			}
			met_char = 0;
			if (!is_number(chr2->c) && !is_unicode_wspace(chr2->c))
				break;
			else if (chr2->c == ')' || chr2->c == ':')
			{
				cn = span->len;
				break;
			}
		}
		if (cn == span->len)
			return 1;
	}

	/* Is the entire first span a roman numeral? Or does it start with
	 * a roman numeral followed by ) or : ? */
	if (is_roman(chr->c))
	{
		int cn = char_num;
		for (cn = char_num+1; cn < span->len; cn++)
		{
			fz_text_char *chr2 = &span->text[cn];

			if (!is_roman(chr2->c) && !is_unicode_wspace(chr2->c))
				break;
			else if (chr2->c == ')' || chr2->c == ':')
			{
				cn = span->len;
				break;
			}
		}
		if (cn == span->len)
			return 1;
	}
#endif
	return 0;
}

typedef struct region_masks_s region_masks;

typedef struct region_mask_s region_mask;

typedef struct region_s region;

struct region_s
{
	float start;
	float stop;
	float ave_start;
	float ave_stop;
	int align;
	float colw;
};

struct region_mask_s
{
	fz_context *ctx;
	int freq;
	fz_point blv;
	int cap;
	int len;
	float size;
	region *mask;
};

struct region_masks_s
{
	fz_context *ctx;
	int cap;
	int len;
	region_mask **mask;
};

static region_masks *
new_region_masks(fz_context *ctx)
{
	region_masks *rms = fz_malloc_struct(ctx, region_masks);
	rms->ctx = ctx;
	rms->cap = 0;
	rms->len = 0;
	rms->mask = NULL;
	return rms;
}

static void
free_region_mask(region_mask *rm)
{
	if (!rm)
		return;
	fz_free(rm->ctx, rm->mask);
	fz_free(rm->ctx, rm);
}

static void
free_region_masks(region_masks *rms)
{
	int i;

	if (!rms)
		return;
	for (i=0; i < rms->len; i++)
	{
		free_region_mask(rms->mask[i]);
	}
	fz_free(rms->ctx, rms->mask);
	fz_free(rms->ctx, rms);
}

static int region_masks_mergeable(const region_mask *rm1, const region_mask *rm2, float *score)
{
	int i1, i2;
	int count = 0;

	*score = 0;
	if (fabsf(rm1->blv.x-rm2->blv.x) >= MY_EPSILON || fabsf(rm1->blv.y-rm2->blv.y) >= MY_EPSILON)
		return 0;

	for (i1 = 0, i2 = 0; i1 < rm1->len && i2 < rm2->len; )
	{
		if (rm1->mask[i1].stop < rm2->mask[i2].start)
		{
			/* rm1's region is entirely before rm2's */
			*score += rm1->mask[i1].stop - rm1->mask[i1].start;
			i1++;
		}
		else if (rm1->mask[i1].start > rm2->mask[i2].stop)
		{
			/* rm2's region is entirely before rm1's */
			*score += rm2->mask[i2].stop - rm2->mask[i2].start;
			i2++;
		}
		else
		{
			float lscore, rscore;
			if (rm1->mask[i1].start < rm2->mask[i2].start)
			{
				if (i2 > 0 && rm2->mask[i2-1].stop >= rm1->mask[i1].start)
					return 0; /* Not compatible */
				lscore = rm2->mask[i2].start - rm1->mask[i1].start;
			}
			else
			{
				if (i1 > 0 && rm1->mask[i1-1].stop >= rm2->mask[i2].start)
					return 0; /* Not compatible */
				lscore = rm1->mask[i1].start - rm2->mask[i2].start;
			}
			if (rm1->mask[i1].stop > rm2->mask[i2].stop)
			{
				if (i2+1 < rm2->len && rm2->mask[i2+1].start <= rm1->mask[i1].stop)
					return 0; /* Not compatible */
				rscore = rm1->mask[i1].stop - rm2->mask[i2].stop;
			}
			else
			{
				if (i1+1 < rm1->len && rm1->mask[i1+1].start <= rm2->mask[i2].stop)
					return 0; /* Not compatible */
				rscore = rm2->mask[i2].stop - rm1->mask[i1].stop;
			}
			/* In order to allow a region to merge, either the
			 * left, the right, or the centre must agree */
			if (lscore < 1)
			{
				if (rscore < 1)
				{
					rscore = 0;
				}
				lscore = 0;
			}
			else if (rscore < 1)
			{
				rscore = 0;
			}
			else
			{
				/* Neither Left or right agree. Does the centre? */
				float ave1 = rm1->mask[i1].start + rm1->mask[i1].stop;
				float ave2 = rm2->mask[i2].start + rm2->mask[i2].stop;
				if (fabsf(ave1-ave2) > 1)
				{
					/* Nothing agrees, so don't merge */
					return 0;
				}
				lscore = 0;
				rscore = 0;
			}
			*score += lscore + rscore;
			/* These two regions could be merged */
			i1++;
			i2++;
		}
		count++;
	}
	count += rm1->len-i1 + rm2->len-i2;
	return count;
}

static int region_mask_matches(const region_mask *rm1, const region_mask *rm2, float *score)
{
	int i1, i2;
	int close = 1;

	*score = 0;
	if (fabsf(rm1->blv.x-rm2->blv.x) >= MY_EPSILON || fabsf(rm1->blv.y-rm2->blv.y) >= MY_EPSILON)
		return 0;

	for (i1 = 0, i2 = 0; i1 < rm1->len && i2 < rm2->len; )
	{
		if (rm1->mask[i1].stop < rm2->mask[i2].start)
		{
			/* rm1's region is entirely before rm2's */
			*score += rm1->mask[i1].stop - rm1->mask[i1].start;
			i1++;
		}
		else if (rm1->mask[i1].start > rm2->mask[i2].stop)
		{
			/* Not compatible */
			return 0;
		}
		else
		{
			float lscore, rscore;
			if (rm1->mask[i1].start > rm2->mask[i2].start)
			{
				/* Not compatible */
				return 0;
			}
			if (rm1->mask[i1].stop < rm2->mask[i2].stop)
			{
				/* Not compatible */
				return 0;
			}
			lscore = rm2->mask[i2].start - rm1->mask[i1].start;
			rscore = rm1->mask[i1].stop - rm2->mask[i2].stop;
			if (lscore < 1)
			{
				if (rscore < 1)
					close++;
				close++;
			}
			else if (rscore < 1)
				close++;
			else if (fabsf(lscore - rscore) < 1)
			{
				lscore = fabsf(lscore-rscore);
				rscore = 0;
				close++;
			}
			*score += lscore + rscore;
			i1++;
			i2++;
		}
	}
	if (i1 < rm1->len)
	{
		/* Still more to go in rm1 */
		if (rm1->mask[i1].start < rm2->mask[rm2->len-1].stop)
			return 0;
	}
	else if (i2 < rm2->len)
	{
		/* Still more to go in rm2 */
		if (rm2->mask[i2].start < rm1->mask[rm1->len-1].stop)
			return 0;
	}

	return close;
}

static void region_mask_merge(region_mask *rm1, const region_mask *rm2, int newlen)
{
	int o, i1, i2;

	/* First, ensure that rm1 is long enough */
	if (rm1->cap < newlen)
	{
		int newcap = rm1->cap ? rm1->cap : 2;
		do
		{
			newcap *= 2;
		}
		while (newcap < newlen);
		rm1->mask = fz_resize_array(rm1->ctx, rm1->mask, newcap, sizeof(*rm1->mask));
		rm1->cap = newcap;
	}

	/* Now run backwards along rm1, filling it out with the merged regions */
	for (o = newlen-1, i1 = rm1->len-1, i2 = rm2->len-1; o >= 0; o--)
	{
		/* So we read from i1 and i2 and store in o */
		if (i1 < 0)
		{
			/* Just copy i2 */
			rm1->mask[o] = rm2->mask[i2];
			i2--;
		}
		else if (i2 < 0)
		{
			/* Just copy i1 */
			rm1->mask[o] = rm1->mask[i1];
			i1--;
		}
		else if (rm1->mask[i1].stop < rm2->mask[i2].start)
		{
			/* rm1's region is entirely before rm2's - copy rm2's */
			rm1->mask[o] = rm2->mask[i2];
			i2--;
		}
		else if (rm2->mask[i2].stop < rm1->mask[i1].start)
		{
			/* rm2's region is entirely before rm1's - copy rm1's */
			rm1->mask[o] = rm1->mask[i1];
			i1--;
		}
		else
		{
			/* We must be merging */
			rm1->mask[o].ave_start = (rm1->mask[i1].start * rm1->freq + rm2->mask[i2].start * rm2->freq)/(rm1->freq + rm2->freq);
			rm1->mask[o].ave_stop = (rm1->mask[i1].stop * rm1->freq + rm2->mask[i2].stop * rm2->freq)/(rm1->freq + rm2->freq);
			rm1->mask[o].start = fz_min(rm1->mask[i1].start, rm2->mask[i2].start);
			rm1->mask[o].stop = fz_max(rm1->mask[i1].stop, rm2->mask[i2].stop);
			i1--;
			i2--;
		}
	}
	rm1->freq += rm2->freq;
	rm1->len = newlen;
}

static region_mask *region_masks_match(const region_masks *rms, const region_mask *rm, fz_text_line *line, region_mask *prev_match)
{
	int i;
	float best_score = 9999999;
	float score;
	int best = -1;
	int best_count = 0;

	/* If the 'previous match' matches, use it regardless. */
	if (prev_match && region_mask_matches(prev_match, rm, &score))
	{
		return prev_match;
	}

	/* Run through and find the 'most compatible' region mask. We are
	 * guaranteed that there will always be at least one compatible one!
	 */
	for (i=0; i < rms->len; i++)
	{
		int count = region_mask_matches(rms->mask[i], rm, &score);
		if (count > best_count || (count == best_count && (score < best_score || best == -1)))
		{
			best = i;
			best_score = score;
			best_count = count;
		}
	}
	assert(best >= 0 && best < rms->len);

	/* So we have the matching mask. */
	return rms->mask[best];
}

#ifdef DEBUG_MASKS
static void
dump_region_mask(const region_mask *rm)
{
	int j;
	for (j = 0; j < rm->len; j++)
	{
		printf("%g->%g ", rm->mask[j].start, rm->mask[j].stop);
	}
	printf("* %d\n", rm->freq);
}

static void
dump_region_masks(const region_masks *rms)
{
	int i;

	for (i = 0; i < rms->len; i++)
	{
		region_mask *rm = rms->mask[i];
		dump_region_mask(rm);
	}
}
#endif

static void region_masks_add(region_masks *rms, region_mask *rm)
{
	/* Add rm to rms */
	if (rms->len == rms->cap)
	{
		int newcap = (rms->cap ? rms->cap * 2 : 4);
		rms->mask = fz_resize_array(rms->ctx, rms->mask, newcap, sizeof(*rms->mask));
		rms->cap = newcap;
	}
	rms->mask[rms->len] = rm;
	rms->len++;
}

static void region_masks_sort(region_masks *rms)
{
	int i, j;

	/* First calculate sizes */
	for (i=0; i < rms->len; i++)
	{
		region_mask *rm = rms->mask[i];
		float size = 0;
		for (j=0; j < rm->len; j++)
		{
			size += rm->mask[j].stop - rm->mask[j].start;
		}
		rm->size = size;
	}

	/* Now, sort on size */
	/* FIXME: bubble sort - use heapsort for efficiency */
	for (i=0; i < rms->len-1; i++)
	{
		for (j=i+1; j < rms->len; j++)
		{
			if (rms->mask[i]->size < rms->mask[j]->size)
			{
				region_mask *tmp = rms->mask[i];
				rms->mask[i] = rms->mask[j];
				rms->mask[j] = tmp;
			}
		}
	}
}

static void region_masks_merge(region_masks *rms, region_mask *rm)
{
	int i;
	float best_score = 9999999;
	float score;
	int best = -1;
	int best_count = 0;

#ifdef DEBUG_MASKS
	printf("\nAdding:\n");
	dump_region_mask(rm);
	printf("To:\n");
	dump_region_masks(rms);
#endif
	for (i=0; i < rms->len; i++)
	{
		int count = region_masks_mergeable(rms->mask[i], rm, &score);
		if (count && (score < best_score || best == -1))
		{
			best = i;
			best_count = count;
			best_score = score;
		}
	}
	if (best != -1)
	{
		region_mask_merge(rms->mask[best], rm, best_count);
#ifdef DEBUG_MASKS
		printf("Merges to give:\n");
		dump_region_masks(rms);
#endif
		free_region_mask(rm);
		return;
	}
	region_masks_add(rms, rm);
#ifdef DEBUG_MASKS
	printf("Adding new one to give:\n");
	dump_region_masks(rms);
#endif
}

static region_mask *
new_region_mask(fz_context *ctx, const fz_point *blv)
{
	region_mask *rm = fz_malloc_struct(ctx, region_mask);
	rm->ctx = ctx;
	rm->freq = 1;
	rm->blv = *blv;
	rm->cap = 0;
	rm->len = 0;
	rm->mask = NULL;
	return rm;
}

static void
region_mask_project(const region_mask *rm, const fz_point *min, const fz_point *max, float *start, float *end)
{
	/* We project min and max down onto the blv */
	float s = min->x * rm->blv.x + min->y * rm->blv.y;
	float e = max->x * rm->blv.x + max->y * rm->blv.y;
	if (s > e)
	{
		*start = e;
		*end = s;
	}
	else
	{
		*start = s;
		*end = e;
	}
}

static void
region_mask_add(region_mask *rm, const fz_point *min, const fz_point *max)
{
	float start, end;
	int i, j;

	region_mask_project(rm, min, max, &start, &end);

	/* Now add start/end into our region list. Typically we will be adding
	 * to the end of the region list, so search from there backwards. */
	for (i = rm->len; i > 0;)
	{
		if (start > rm->mask[i-1].stop)
			break;
		i--;
	}
	/* So we know that our interval can only affect list items >= i.
	 * We know that start is after our previous end. */
	if (i == rm->len || end < rm->mask[i].start)
	{
		/* Insert new one. No overlap. No merging */
		if (rm->len == rm->cap)
		{
			int newcap = (rm->cap ? rm->cap * 2 : 4);
			rm->mask = fz_resize_array(rm->ctx, rm->mask, newcap, sizeof(*rm->mask));
			rm->cap = newcap;
		}
		if (rm->len > i)
			memmove(&rm->mask[i+1], &rm->mask[i], (rm->len - i) * sizeof(*rm->mask));
		rm->mask[i].ave_start = start;
		rm->mask[i].ave_stop = end;
		rm->mask[i].start = start;
		rm->mask[i].stop = end;
		rm->len++;
	}
	else
	{
		/* Extend current one down. */
		rm->mask[i].ave_start = start;
		rm->mask[i].start = start;
		if (rm->mask[i].stop < end)
		{
			rm->mask[i].stop = end;
			rm->mask[i].ave_stop = end;
			/* Our region may now extend upwards too far */
			i++;
			j = i;
			while (j < rm->len && rm->mask[j].start <= end)
			{
				rm->mask[i-1].stop = end = rm->mask[j].stop;
				j++;
			}
			if (i != j)
			{
				/* Move everything from j down to i */
				while (j < rm->len)
				{
					rm->mask[i++] = rm->mask[j++];
				}
			}
			rm->len -= j-i;
		}
	}
}

static int
region_mask_column(region_mask *rm, const fz_point *min, const fz_point *max, int *align, float *colw, float *left_)
{
	float start, end, left, right;
	int i;

	region_mask_project(rm, min, max, &start, &end);

	for (i = 0; i < rm->len; i++)
	{
		/* The use of MY_EPSILON here is because we might be matching
		 * start/end values calculated with slightly different blv's */
		if (rm->mask[i].start - MY_EPSILON <= start && rm->mask[i].stop + MY_EPSILON >= end)
			break;
	}
	if (i >= rm->len)
	{
		*align = 0;
		*colw = 0;
		return 0;
	}
	left = start - rm->mask[i].start;
	right = rm->mask[i].stop - end;
	if (left < 1 && right < 1)
		*align = rm->mask[i].align;
	else if (left*2 <= right)
		*align = 0; /* Left */
	else if (right * 2 < left)
		*align = 2; /* Right */
	else
		*align = 1;
	*left_ = left;
	*colw = rm->mask[i].colw;
	return i;
}

static void
region_mask_alignment(region_mask *rm)
{
	int i;
	float width = 0;

	for (i = 0; i < rm->len; i++)
	{
		width += rm->mask[i].stop - rm->mask[i].start;
	}
	for (i = 0; i < rm->len; i++)
	{
		region *r = &rm->mask[i];
		float left = r->ave_start - r->start;
		float right = r->stop - r->ave_stop;
		if (left*2 <= right)
			r->align = 0; /* Left */
		else if (right * 2 < left)
			r->align = 2; /* Right */
		else
			r->align = 1;
		r->colw = 100 * (rm->mask[i].stop - rm->mask[i].start) / width;
	}
}

static void
region_masks_alignment(region_masks *rms)
{
	int i;

	for (i = 0; i < rms->len; i++)
	{
		region_mask_alignment(rms->mask[i]);
	}
}

static int
is_unicode_hyphen(int c)
{
	/* We omit 0x2011 (Non breaking hyphen) and 0x2043 (Hyphen Bullet)
	 * from this list. */
	return (c == '-' ||
		c == 0x2010 || /* Hyphen */
		c == 0x002d || /* Hyphen-Minus */
		c == 0x00ad || /* Soft hyphen */
		c == 0x058a || /* Armenian Hyphen */
		c == 0x1400 || /* Canadian Syllabive Hyphen */
		c == 0x1806); /* Mongolian Todo soft hyphen */
}

static int
is_unicode_hyphenatable(int c)
{
	/* This is a pretty ad-hoc collection. It may need tuning. */
	return ((c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z') ||
		(c >= 0x00c0 && c <= 0x00d6) ||
		(c >= 0x00d8 && c <= 0x00f6) ||
		(c >= 0x00f8 && c <= 0x02af) ||
		(c >= 0x1d00 && c <= 0x1dbf) ||
		(c >= 0x1e00 && c <= 0x1eff) ||
		(c >= 0x2c60 && c <= 0x2c7f) ||
		(c >= 0xa722 && c <= 0xa78e) ||
		(c >= 0xa790 && c <= 0xa793) ||
		(c >= 0xa7a8 && c <= 0xa7af) ||
		(c >= 0xfb00 && c <= 0xfb07) ||
		(c >= 0xff21 && c <= 0xff3a) ||
		(c >= 0xff41 && c <= 0xff5a));
}

static void
dehyphenate(fz_text_span *s1, fz_text_span *s2)
{
	int i;

	for (i = s1->len-1; i > 0; i--)
		if (!is_unicode_wspace(s1->text[i].c))
			break;
	/* Can't leave an empty span. */
	if (i == 0)
		return;

	if (!is_unicode_hyphen(s1->text[i].c))
		return;
	if (!is_unicode_hyphenatable(s1->text[i-1].c))
		return;
	if (!is_unicode_hyphenatable(s2->text[0].c))
		return;
	s1->len = i;
	s2->spacing = 0;
}

void
fz_analyze_text(fz_context *ctx, fz_text_sheet *sheet, fz_text_page *page)
{
	fz_text_line *line;
	fz_text_span *span;
	line_heights *lh;
	region_masks *rms;
	int block_num;

	/* Simple paragraph analysis; look for the most common 'inter line'
	 * spacing. This will be assumed to be our line spacing. Anything
	 * more than 25% wider than this will be assumed to be a paragraph
	 * space. */

	/* Step 1: Gather the line height information */
	lh = new_line_heights(ctx);
	for (block_num = 0; block_num < page->len; block_num++)
	{
		fz_text_block *block;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;

		for (line = block->lines; line < block->lines + block->len; line++)
		{
			/* For every style in the line, add lineheight to the
			 * record for that style. FIXME: This is a nasty n^2
			 * algorithm at the moment. */
			fz_text_style *style = NULL;

			if (line->distance == 0)
				continue;

			for (span = line->first_span; span; span = span->next)
			{
				int char_num;

				if (is_list_entry(line, span, &char_num))
					goto list_entry;

				for (; char_num < span->len; char_num++)
				{
					fz_text_char *chr = &span->text[char_num];

					/* Ignore any whitespace chars */
					if (is_unicode_wspace(chr->c))
						continue;

					if (chr->style != style)
					{
						/* Have we had this style before? */
						int match = 0;
						fz_text_span *span2;
						for (span2 = line->first_span; span2; span2 = span2->next)
						{
							int char_num2;
							for (char_num2 = 0; char_num2 < span2->len; char_num2++)
							{
								fz_text_char *chr2 = &span2->text[char_num2];
								if (chr2->style == chr->style)
								{
									match = 1;
									break;
								}
							}
						}
						if (char_num > 0 && match == 0)
						{
							fz_text_span *span2 = span;
							int char_num2;
							for (char_num2 = 0; char_num2 < char_num; char_num2++)
							{
								fz_text_char *chr2 = &span2->text[char_num2];
								if (chr2->style == chr->style)
								{
									match = 1;
									break;
								}
							}
						}
						if (match == 0)
							insert_line_height(lh, chr->style, line->distance);
						style = chr->style;
					}
				}
list_entry:
				{}
			}
		}
	}

	/* Step 2: Find the most popular line height for each style */
	cull_line_heights(lh);

	/* Step 3: Run through the blocks, breaking each block into two if
	 * the line height isn't right. */
	for (block_num = 0; block_num < page->len; block_num++)
	{
		int line_num;
		fz_text_block *block;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;

		for (line_num = 0; line_num < block->len; line_num++)
		{
			/* For every style in the line, check to see if lineheight
			 * is correct for that style. FIXME: We check each style
			 * more than once, currently. */
			int ok = 0; /* -1 = early exit, split now. 0 = split. 1 = don't split. */
			fz_text_style *style = NULL;
			line = &block->lines[line_num];

			if (line->distance == 0)
				continue;

#ifdef DEBUG_LINE_HEIGHTS
			printf("line height=%g nspans=%d\n", line->distance, line->len);
#endif
			for (span = line->first_span; span; span = span->next)
			{
				int char_num;

				if (is_list_entry(line, span, &char_num))
					goto force_paragraph;

				/* Now we do the rest of the line */
				for (; char_num < span->len; char_num++)
				{
					fz_text_char *chr = &span->text[char_num];

					/* Ignore any whitespace chars */
					if (is_unicode_wspace(chr->c))
						continue;

					if (chr->style != style)
					{
						float proper_step = line_height_for_style(lh, chr->style);
						if (proper_step * 0.95 <= line->distance && line->distance <= proper_step * 1.05)
						{
							ok = 1;
							break;
						}
						style = chr->style;
					}
				}
				if (ok)
					break;
			}
			if (!ok)
			{
force_paragraph:
				split_block(ctx, page, block_num, line_num);
				break;
			}
		}
	}
	free_line_heights(lh);

	/* Simple line region analysis:
	 * For each line:
	 *    form a list of 'start/stop' points (henceforth a 'region mask')
	 *    find the normalised baseline vector for the line.
	 *    Store the region mask and baseline vector.
	 * Collate lines that have compatible region masks and identical
	 * baseline vectors.
	 * If the collated masks are column-like, then split into columns.
	 * Otherwise split into tables.
	 */
	rms = new_region_masks(ctx);

	/* Step 1: Form the region masks and store them into a list with the
	 * normalised baseline vectors. */
	for (block_num = 0; block_num < page->len; block_num++)
	{
		fz_text_block *block;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;

		for (line = block->lines; line < block->lines + block->len; line++)
		{
			fz_point blv;
			region_mask *rm;

#ifdef DEBUG_MASKS
			printf("Line: ");
			dump_line(line);
#endif
			blv = line->first_span->max;
			blv.x -= line->first_span->min.x;
			blv.y -= line->first_span->min.y;
			fz_normalize_vector(&blv);

			rm = new_region_mask(ctx, &blv);
			for (span = line->first_span; span; span = span->next)
			{
				fz_point *region_min = &span->min;
				fz_point *region_max = &span->max;

				/* Treat adjacent spans as one big region */
				while (span->next && span->next->spacing < 1.5)
				{
					span = span->next;
					region_max = &span->max;
				}

				region_mask_add(rm, region_min, region_max);
			}
#ifdef DEBUG_MASKS
			dump_region_mask(rm);
#endif
			region_masks_add(rms, rm);
		}
	}

	/* Step 2: Sort the region_masks by size of masked region */
	region_masks_sort(rms);

#ifdef DEBUG_MASKS
	printf("Sorted list of regions:\n");
	dump_region_masks(rms);
#endif
	/* Step 3: Merge the region masks where possible (large ones first) */
	{
		int i;
		region_masks *rms2;
		rms2 = new_region_masks(ctx);
		for (i=0; i < rms->len; i++)
		{
			region_mask *rm = rms->mask[i];
			rms->mask[i] = NULL;
			region_masks_merge(rms2, rm);
		}
		free_region_masks(rms);
		rms = rms2;
	}

#ifdef DEBUG_MASKS
	printf("Merged list of regions:\n");
	dump_region_masks(rms);
#endif

	/* Step 4: Figure out alignment */
	region_masks_alignment(rms);

	/* Step 5: At this point, we should probably look at the region masks
	 * to try to guess which ones represent columns on the page. With our
	 * current code, we could only get blocks of lines that span 2 or more
	 * columns if the PDF producer wrote text out horizontally across 2
	 * or more columns, and we've never seen that (yet!). So we skip this
	 * step for now. */

	/* Step 6: Run through the lines again, deciding which ones fit into
	 * which region mask. */
	{
	region_mask *prev_match = NULL;
	for (block_num = 0; block_num < page->len; block_num++)
	{
		fz_text_block *block;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;

		for (line = block->lines; line < block->lines + block->len; line++)
		{
			fz_point blv;
			region_mask *rm;
			region_mask *match;

			blv = line->first_span->max;
			blv.x -= line->first_span->min.x;
			blv.y -= line->first_span->min.y;
			fz_normalize_vector(&blv);

#ifdef DEBUG_MASKS
			dump_line(line);
#endif
			rm = new_region_mask(ctx, &blv);
			for (span = line->first_span; span; span = span->next)
			{
				fz_point *region_min = &span->min;
				fz_point *region_max = &span->max;

				/* Treat adjacent spans as one big region */
				while (span->next && span->next->spacing < 1.5)
				{
					span = span->next;
					region_max = &span->max;
				}

				region_mask_add(rm, region_min, region_max);
			}
#ifdef DEBUG_MASKS
			printf("Mask: ");
			dump_region_mask(rm);
#endif
			match = region_masks_match(rms, rm, line, prev_match);
			prev_match = match;
#ifdef DEBUG_MASKS
			printf("Matches: ");
			dump_region_mask(match);
#endif
			free_region_mask(rm);
			span = line->first_span;
			while (span)
			{
				fz_point *region_min = &span->min;
				fz_point *region_max = &span->max;
				fz_text_span *sn;
				int col, align;
				float colw, left;

				/* Treat adjacent spans as one big region */
#ifdef DEBUG_ALIGN
				dump_span(span);
#endif
				for (sn = span->next; sn && sn->spacing < 1.5; sn = sn->next)
				{
					region_max = &sn->max;
#ifdef DEBUG_ALIGN
					dump_span(sn);
#endif
				}
				col = region_mask_column(match, region_min, region_max, &align, &colw, &left);
#ifdef DEBUG_ALIGN
				printf(" = col%d colw=%g align=%d\n", col, colw, align);
#endif
				do
				{
					span->column = col;
					span->align = align;
					span->indent = left;
					span->column_width = colw;
					span = span->next;
				}
				while (span != sn);

				if (span)
					span = span->next;
			}
			line->region = match;
		}
	}
	free_region_masks(rms);
	}

	/* Step 7: Collate lines within a block that share the same region
	 * mask. */
	for (block_num = 0; block_num < page->len; block_num++)
	{
		int line_num;
		int prev_line_num;

		fz_text_block *block;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;

		/* First merge lines. This may leave empty lines behind. */
		for (prev_line_num = 0, line_num = 1; line_num < block->len; line_num++)
		{
			fz_text_line *prev_line;
			line = &block->lines[line_num];
			if (!line->first_span)
				continue;
			prev_line = &block->lines[prev_line_num];
			if (prev_line->region == line->region)
			{
				/* We only merge lines if the second line
				 * only uses 1 of the columns. */
				int col = line->first_span->column;
				/* Copy the left value for the first span
				 * in the first column in this line forward
				 * for all the rest of the spans in the same
				 * column. */
				float indent = line->first_span->indent;
				for (span = line->first_span->next; span; span = span->next)
				{
					if (col != span->column)
						break;
					span->indent = indent;
				}
				if (span)
				{
					prev_line_num = line_num;
					continue;
				}

				/* Merge line into prev_line */
				{
					fz_text_span **prev_line_span = &prev_line->first_span;
					int try_dehyphen = -1;
					fz_text_span *prev_span = NULL;
					span = line->first_span;
					while (span)
					{
						/* Skip forwards through the original
						 * line, until we find a place where
						 * span should go. */
						if ((*prev_line_span)->column <= span->column)
						{
							/* The current span we are considering
							 * in prev_line is earlier than span.
							 * Just skip forwards in prev_line. */
							prev_span = (*prev_line_span);
							prev_line_span = &prev_span->next;
							try_dehyphen = span->column;
						}
						else
						{
							/* We want to copy span into prev_line. */
							fz_text_span *next = (*prev_line_span)->next;

							if (prev_line_span == &prev_line->first_span)
								prev_line->first_span = span;
							if (next == NULL)
								prev_line->last_span = span;
							if (try_dehyphen == span->column)
								dehyphenate(prev_span, span);
							try_dehyphen = -1;
							prev_span = *prev_line_span = span;
							span = span->next;
							(*prev_line_span)->next = next;
							prev_line_span = &span->next;
						}
					}
					while (span || *prev_line_span);
					line->first_span = NULL;
					line->last_span = NULL;
				}
			}
			else
				prev_line_num = line_num;
		}

		/* Now get rid of the empty lines */
		for (prev_line_num = 0, line_num = 0; line_num < block->len; line_num++)
		{
			line = &block->lines[line_num];
			if (line->first_span)
				block->lines[prev_line_num++] = *line;
		}
		block->len = prev_line_num;

		/* Now try to spot indents */
		for (line_num = 0; line_num < block->len; line_num++)
		{
			fz_text_span *span_num, *sn;
			int col, count;
			line = &block->lines[line_num];

			/* Run through the spans... */
			span_num = line->first_span;
			{
				float indent = 0;
				/* For each set of spans that share the same
				 * column... */
				col = span_num->column;
#ifdef DEBUG_INDENTS
				printf("Indent %g: ", span_num->indent);
				dump_span(span_num);
				printf("\n");
#endif

				/* find the average indent of all but the first.. */
				for (sn = span_num->next, count = 0; sn && sn->column == col; sn = sn->next, count++)
				{
#ifdef DEBUG_INDENTS
					printf("Indent %g: ", sn->indent);
					dump_span(sn);
				printf("\n");
#endif
					indent += sn->indent;
					sn->indent = 0;
				}
				if (sn != span_num->next)
					indent /= count;

				/* And compare this indent with the first one... */
#ifdef DEBUG_INDENTS
				printf("Average indent %g ", indent);
#endif
				indent -= span_num->indent;
#ifdef DEBUG_INDENTS
				printf("delta %g ", indent);
#endif
				if (fabsf(indent) < 1)
				{
					/* No indent worth speaking of */
					indent = 0;
				}
#ifdef DEBUG_INDENTS
				printf("recorded %g\n", indent);
#endif
				span_num->indent = indent;
				span_num = sn;
			}
			for (; span_num; span_num = span_num->next)
			{
				span_num->indent = 0;
			}
		}
	}
}
