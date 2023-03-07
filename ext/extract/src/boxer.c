#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "document.h"
#include "outf.h"

#define DEBUG_WRITE_AS_PS
/* #define DEBUG_PRINT */

typedef struct boxer_s boxer_t;

typedef struct {
	int    len;
	int    max;
	rect_t list[1];
} rectlist_t;

struct boxer_s {
	extract_alloc_t *alloc;
	rect_t           mediabox;
	rectlist_t      *list;
};

static rectlist_t *
rectlist_create(extract_alloc_t *alloc, int max)
{
	rectlist_t *list;

	if (extract_malloc(alloc, &list, sizeof(rectlist_t) + sizeof(rect_t)*(max-1)))
		return NULL;

	list->len = 0;
	list->max = max;

	return list;
}

/* Push box onto rectlist, unless it is completely enclosed by
 * another box, or completely encloses others (in which case they
 * are replaced by it). */
static void
rectlist_append(rectlist_t *list, rect_t *box)
{
	int i;

	for (i = 0; i < list->len; i++)
	{
		rect_t *r = &list->list[i];
		rect_t smaller, larger;
		/* We allow ourselves a fudge factor of 4 points when checking for inclusion. */
		double r_fudge = 4;

		smaller.min.x = r->min.x + r_fudge;
		larger. min.x = r->min.x - r_fudge;
		smaller.min.y = r->min.y + r_fudge;
		larger. min.y = r->min.y - r_fudge;
		smaller.max.x = r->max.x - r_fudge;
		larger. max.x = r->max.x + r_fudge;
		smaller.max.y = r->max.y - r_fudge;
		larger. max.y = r->max.y + r_fudge;

		if (extract_rect_contains_rect(larger, *box))
			return; /* box is enclosed! Nothing to do. */
		if (extract_rect_contains_rect(*box, smaller))
		{
			/* box encloses r. Ditch r. */
			/* Shorten the list */
			--list->len;
			/* If the one that just got chopped off wasn't r, move it down. */
			if (i < list->len)
			{
				memcpy(r, &list->list[list->len], sizeof(*r));
				i--; /* Reconsider this entry next time. */
			}
		}
	}

	assert(list->len < list->max);
	memcpy(&list->list[list->len], box, sizeof(*box));
	list->len++;
}

static boxer_t *
boxer_create_length(extract_alloc_t *alloc, rect_t *mediabox, int len)
{
	boxer_t *boxer;

	if (extract_malloc(alloc, &boxer, sizeof(*boxer)))
		return NULL;

	boxer->alloc = alloc;
	memcpy(&boxer->mediabox, mediabox, sizeof(*mediabox));
	boxer->list = rectlist_create(alloc, len);

	return boxer;
}

/* Create a boxer structure for a page of size mediabox. */
static boxer_t *
boxer_create(extract_alloc_t *alloc, rect_t *mediabox)
{
	boxer_t *boxer = boxer_create_length(alloc, mediabox, 1);

	if (boxer == NULL)
		return NULL;
	rectlist_append(boxer->list, mediabox);

	return boxer;
}

static void
push_if_intersect_suitable(rectlist_t *dst, const rect_t *a, const rect_t *b)
{
	rect_t c;

	/* Intersect a and b. */
	c = extract_rect_intersect(*a, *b);
	/* If no intersection, nothing to push. */
	if (!extract_rect_valid(c))
		return;

	/* If the intersect is too narrow or too tall, ignore it.
	* We don't care about inter character spaces, for example.
	* Arbitrary 4 point threshold. */
#define THRESHOLD 4
	if (c.min.x + THRESHOLD >= c.max.x || c.min.y+THRESHOLD >= c.max.y)
		return;

	rectlist_append(dst, &c);
}

static void
boxlist_feed_intersect(rectlist_t *dst, const rectlist_t *src, const rect_t *box)
{
	int i;

	for (i = 0; i < src->len; i++)
		push_if_intersect_suitable(dst, &src->list[i], box);
}

/* Mark a given box as being occupied (typically by a glyph) */
static int boxer_feed(boxer_t *boxer, rect_t *bbox)
{
	rect_t box;
	/* When we feed a box into a the boxer, we can never make
	* the list more than 4 times as long. */
	rectlist_t *newlist = rectlist_create(boxer->alloc, boxer->list->len * 4);
	if (newlist == NULL)
		return -1;

#ifdef DEBUG_WRITE_AS_PS
	printf("0 0 1 setrgbcolor\n");
	printf("%g %g moveto %g %g lineto %g %g lineto %g %g lineto closepath fill\n",
		bbox->min.x, bbox->min.y,
		bbox->min.x, bbox->max.y,
		bbox->max.x, bbox->max.y,
		bbox->max.x, bbox->min.y
	);
#endif

	/* Left (0,0) (min.x,H) */
	box.min.x = boxer->mediabox.min.x;
	box.min.y = boxer->mediabox.min.y;
	box.max.x = bbox->min.x;
	box.max.y = boxer->mediabox.max.y;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	/* Right (max.x,0) (W,H) */
	box.min.x = bbox->max.x;
	box.min.y = boxer->mediabox.min.y;
	box.max.x = boxer->mediabox.max.x;
	box.max.y = boxer->mediabox.max.y;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	/* Bottom (0,0) (W,min.y) */
	box.min.x = boxer->mediabox.min.x;
	box.min.y = boxer->mediabox.min.y;
	box.max.x = boxer->mediabox.max.x;
	box.max.y = bbox->min.y;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	/* Top (0,max.y) (W,H) */
	box.min.x = boxer->mediabox.min.x;
	box.min.y = bbox->max.y;
	box.max.x = boxer->mediabox.max.x;
	box.max.y = boxer->mediabox.max.y;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	extract_free(boxer->alloc, &boxer->list);
	boxer->list = newlist;

	return 0;
}

static int
compare_areas(const void *a_, const void *b_)
{
	const rect_t *a = (const rect_t *)a_;
	const rect_t *b = (const rect_t *)b_;
	double area_a = (a->max.x-a->min.x) * (a->max.y-a->min.y);
	double area_b = (b->max.x-b->min.x) * (b->max.y-b->min.y);

	if (area_a < area_b)
		return 1;
	else if (area_a > area_b)
		return -1;
	else
		return 0;
}

/* Sort the rectangle list to be largest area first. For ease of humans
 * reading debug output. */
static void boxer_sort(boxer_t *boxer)
{
	qsort(boxer->list->list, boxer->list->len, sizeof(rect_t), compare_areas);
}

/* Get the rectangle list for a given boxer. Return value is the length of
 * the list. Lifespan is until the boxer is modified or freed. */
static int boxer_results(boxer_t *boxer, rect_t **list)
{
	*list = boxer->list->list;
	return boxer->list->len;
}

/* Destroy a boxer. */
static void boxer_destroy(boxer_t *boxer)
{
	if (!boxer)
		return;

	extract_free(boxer->alloc, &boxer->list);
	extract_free(boxer->alloc, &boxer);
}

/* Find the margins for a given boxer. */
static rect_t boxer_margins(boxer_t *boxer)
{
	rectlist_t *list = boxer->list;
	int i;
	rect_t margins = boxer->mediabox;

	for (i = 0; i < list->len; i++)
	{
		rect_t *r = &list->list[i];
		if (r->min.x <= margins.min.x && r->min.y <= margins.min.y && r->max.y >= margins.max.y)
			margins.min.x = r->max.x; /* Left Margin */
		else if (r->max.x >= margins.max.x && r->min.y <= margins.min.y && r->max.y >= margins.max.y)
			margins.max.x = r->min.x; /* Right Margin */
		else if (r->min.x <= margins.min.x && r->max.x >= margins.max.x && r->min.y <= margins.min.y)
			margins.min.y = r->max.y; /* Top Margin */
		else if (r->min.x <= margins.min.x && r->max.x >= margins.max.x && r->max.y >= margins.max.y)
			margins.max.y = r->min.y; /* Bottom Margin */
	}

	return margins;
}

/* Create a new boxer from a subset of an old one. */
static boxer_t *boxer_subset(boxer_t *boxer, rect_t rect)
{
	boxer_t *new_boxer = boxer_create_length(boxer->alloc, &rect, boxer->list->len);
	int i;

	if (new_boxer == NULL)
		return NULL;

	for (i = 0; i < boxer->list->len; i++)
	{
		rect_t r = extract_rect_intersect(boxer->list->list[i], rect);

		if (!extract_rect_valid(r))
			continue;
		rectlist_append(new_boxer->list, &r);
	}

	return new_boxer;
}

/* Consider a boxer for subdivision.
 * Returns 0 if no suitable subdivision point found.
 * Returns 1, and sets *boxer1 and *boxer2 to new boxer structures for the the subdivisions
 * if a subdivision point is found.*/
static split_type_t
boxer_subdivide(boxer_t *boxer, boxer_t **boxer1, boxer_t **boxer2)
{
	rectlist_t *list = boxer->list;
	int num_h = 0, num_v = 0;
	double max_h = 0, max_v = 0;
	rect_t best_h = {0}, best_v = {0};
	int i;

	*boxer1 = NULL;
	*boxer2 = NULL;

	for (i = 0; i < list->len; i++)
	{
		rect_t r = boxer->list->list[i];

		if (r.min.x <= boxer->mediabox.min.x && r.max.x >= boxer->mediabox.max.x)
		{
			/* Horizontal divider */
			double size = r.max.y - r.min.y;
			if (size > max_h)
			{
				max_h = size;
				best_h = r;
			}
			num_h++;
		}
		if (r.min.y <= boxer->mediabox.min.y && r.max.y >= boxer->mediabox.max.y)
		{
			/* Vertical divider */
			double size = r.max.x - r.min.x;
			if (size > max_v)
			{
				max_v = size;
				best_v = r;
			}
			num_v++;
		}
	}

	outf("num_h=%d num_v=%d\n", num_h, num_v);
	outf("max_h=%g max_v=%g\n", max_h, max_v);

	if (max_h > max_v)
	{
		rect_t r;
		/* Divider runs horizontally. */
		r = boxer->mediabox;
		r.max.y = best_h.min.y;
		*boxer1 = boxer_subset(boxer, r);
		r = boxer->mediabox;
		r.min.y = best_h.max.y;
		*boxer2 = boxer_subset(boxer, r);
		return SPLIT_VERTICAL;
	}
	else if (max_v > 0)
	{
		rect_t r;
		/* Divider runs vertically. */
		r = boxer->mediabox;
		r.max.x = best_v.min.x;
		*boxer1 = boxer_subset(boxer, r);
		r = boxer->mediabox;
		r.min.x = best_v.max.x;
		*boxer2 = boxer_subset(boxer, r);
		return SPLIT_HORIZONTAL;
	}

	return SPLIT_NONE;
}


/* Extract specifics */
static rect_t
extract_span_bbox(span_t *span)
{
	int j;
	rect_t bbox = extract_rect_empty;

	for (j = 0; j < span->chars_num; j++)
	{
		char_t *char_ = &span->chars[j];
		bbox = extract_rect_union(bbox, char_->bbox);
	}
	return bbox;
}


static int
extract_subpage_subset(extract_alloc_t *alloc, extract_page_t *page, subpage_t *subpage, rect_t mediabox)
{
	content_span_iterator  sit;
	span_t                *span;
	subpage_t             *target;

	if (extract_subpage_alloc(alloc, mediabox, page, &target))
	return -1;

	for (span = content_span_iterator_init(&sit, &subpage->content); span != NULL; span = content_span_iterator_next(&sit))
	{
		rect_t bbox = extract_span_bbox(span);

		if (bbox.min.x >= mediabox.min.x && bbox.min.y >= mediabox.min.y && bbox.max.x <= mediabox.max.x && bbox.max.y <= mediabox.max.y)
		{
			content_unlink(&span->base);
			content_append_span(&target->content, span);
		}
	}

	return 0;
}

enum {
	MAX_ANALYSIS_DEPTH = 6
};

static int
analyse_sub(extract_page_t *page, subpage_t *subpage, boxer_t *big_boxer, split_t **psplit, int depth)
{
	rect_t margins;
	boxer_t *boxer;
	boxer_t *boxer1;
	boxer_t *boxer2;
	int ret;
	split_type_t split_type;
	split_t *split;

	margins = boxer_margins(big_boxer);
#ifdef DEBUG_WRITE_AS_PS
	printf("\n\n%% MARGINS %g %g %g %g\n", margins.min.x, margins.min.y, margins.max.x, margins.max.y);
#endif

	boxer = boxer_subset(big_boxer, margins);

	if (depth < MAX_ANALYSIS_DEPTH &&
		(split_type = boxer_subdivide(boxer, &boxer1, &boxer2)) != SPLIT_NONE)
	{
		if (boxer1 == NULL || boxer2 == NULL ||
			extract_split_alloc(boxer->alloc, split_type, 2, psplit))
		{
			ret = -1;
			goto fail_mid_split;
		}
		split = *psplit;
		outf("depth=%d %s\n", depth, split_type == SPLIT_HORIZONTAL ? "H" : "V");
		ret = analyse_sub(page, subpage, boxer1, &split->split[0], depth+1);
		if (!ret) ret = analyse_sub(page, subpage, boxer2, &split->split[1], depth+1);
		if (!ret)
		{
			if (split_type == SPLIT_HORIZONTAL)
			{
				split->split[0]->weight = boxer1->mediabox.max.x - boxer1->mediabox.min.x;
				split->split[1]->weight = boxer2->mediabox.max.x - boxer2->mediabox.min.x;
			}
			else
			{
				split->split[0]->weight = boxer1->mediabox.max.y - boxer1->mediabox.min.y;
				split->split[1]->weight = boxer2->mediabox.max.y - boxer2->mediabox.min.y;
			}
		}
fail_mid_split:
		boxer_destroy(boxer1);
		boxer_destroy(boxer2);
		boxer_destroy(boxer);
		return ret;
	}

	outf("depth=%d LEAF\n", depth);

	if (extract_split_alloc(boxer->alloc, SPLIT_NONE, 0, psplit))
	{
		boxer_destroy(boxer);
		return -1;
	}
	split = *psplit;

	ret = extract_subpage_subset(boxer->alloc, page, subpage, boxer->mediabox);

#ifdef DEBUG_WRITE_AS_PS
	{
		int i, n;
		rect_t *list;
		boxer_sort(boxer);
		n = boxer_results(boxer, &list);

		printf("%% SUBDIVISION\n");
		for (i = 0; i < n; i++)
		{
			printf("%% %g %g %g %g\n",
				list[i].min.x, list[i].min.y, list[i].max.x, list[i].max.y);
		}

		printf("0 0 0 setrgbcolor\n");
		for (i = 0; i < n; i++) {
			printf("%g %g moveto\n%g %g lineto\n%g %g lineto\n%g %g lineto\nclosepath\nstroke\n\n",
				list[i].min.x, list[i].min.y,
				list[i].min.x, list[i].max.y,
				list[i].max.x, list[i].max.y,
				list[i].max.x, list[i].min.y);
		}

		printf("1 0 0 setrgbcolor\n");
		printf("%g %g moveto\n%g %g lineto\n%g %g lineto\n%g %g lineto\nclosepath\nstroke\n\n",
			margins.min.x, margins.min.y,
			margins.min.x, margins.max.y,
			margins.max.x, margins.max.y,
			margins.max.x, margins.min.y);
	}
#endif
	boxer_destroy(boxer);

	return ret;
}


static int
collate_splits(extract_alloc_t *alloc, split_t **psplit)
{
	split_t *split = *psplit;
	int s;
	int n = 0;
	int i;
	int j;
	split_t *newsplit;

	/* Recurse into all our children to ensure they are collated.
	* Count how many children we'll have once we pull all the
	* children of children that match our type up into us. */
	for (s = 0; s < split->count; s++)
	{
		if (collate_splits(alloc, &split->split[s]))
			return -1;
		if (split->split[s]->type == split->type)
			n += split->split[s]->count;
		else
			n++;
	}

	/* No change in the number of children? Just exit. */
	if (n == split->count)
		return 0;

	if (extract_split_alloc(alloc, split->type, n, &newsplit))
		return -1;

	newsplit->weight = split->weight;

	/* Now, run across our children. */
	i = 0;
	for (s = 0; s < split->count; s++)
	{
		split_t *sub = split->split[s];
		if (sub->type == split->type)
		{
			/* If the type matches, pull the grandchildren into newsplit. */
			for (j = 0; j < sub->count; j++)
			{
				newsplit->split[i++] = sub->split[j];
				sub->split[j] = NULL;
			}
		}
		else
		{
			/* Otherwise just move the child into newsplit. */
			newsplit->split[i++] = sub;
			split->split[s] = NULL;
		}
	}

	extract_split_free(alloc, psplit);
	*psplit = newsplit;

	return 0;
}

int extract_page_analyse(extract_alloc_t *alloc, extract_page_t *page)
{
	boxer_t               *boxer;
	subpage_t             *subpage = page->subpages[0];
	content_span_iterator  sit;
	span_t                *span;

	/* This code will only work if the page contains a single subpage.
	* This should always be the case if we're called from a page
	* generated via extract_page_begin. */
	if (page->subpages_num != 1) return 0;

	/* Take the old subpages out from the page. */
	page->subpages_num = 0;
	extract_free(alloc, &page->subpages);

#ifdef DEBUG_WRITE_AS_PS
	printf("1 -1 scale 0 -%g translate\n", page->mediabox.max.y-page->mediabox.min.y);
#endif

	boxer = boxer_create(alloc, (rect_t *)&subpage->mediabox);

	for (span = content_span_iterator_init(&sit, &subpage->content); span != NULL; span = content_span_iterator_next(&sit))
	{
		rect_t bbox = extract_span_bbox(span);
		if (boxer_feed(boxer, &bbox))
			goto fail;
	}

	if (analyse_sub(page, subpage, boxer, &page->split, 0))
		goto fail;

	if (collate_splits(boxer->alloc, &page->split))
		goto fail;

#ifdef DEBUG_WRITE_AS_PS
	printf("showpage\n");
#endif

	boxer_destroy(boxer);
	extract_subpage_free(alloc, &subpage);

	return 0;

fail:
	outf("Analysis failed!\n");
	boxer_destroy(boxer);
	extract_subpage_free(alloc, &subpage);

	return -1;
}
