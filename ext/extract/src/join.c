#include "extract/extract.h"
#include "extract/alloc.h"

#include "astring.h"
#include "document.h"
#include "mem.h"
#include "outf.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>


static char_t *span_char_first(span_t *span)
{
	assert(span->chars_num > 0);
	return &span->chars[0];
}

static char_t *span_char_last(span_t *span)
{
	assert(span->chars_num > 0);
	return &span->chars[span->chars_num-1];
}

const char *extract_matrix_string(const matrix_t *matrix)
{
	static char ret[5][64];
	static int i = 0;
	i = (i + 1) % 5;
	snprintf(ret[i], sizeof(ret[i]), "{%f %f %f %f %f %f}",
		matrix->a,
		matrix->b,
		matrix->c,
		matrix->d,
		matrix->e,
		matrix->f);

	return ret[i];
}

const char *extract_matrix4_string(const matrix4_t *matrix)
{
	static char ret[5][64];
	static int i = 0;
	i = (i + 1) % 5;
	snprintf(ret[i], sizeof(ret[i]), "{%f %f %f %f}",
		matrix->a,
		matrix->b,
		matrix->c,
		matrix->d);
	return ret[i];
}

/* Returns first line in paragraph. */
static line_t *paragraph_line_first(const paragraph_t *paragraph)
{
	return content_first_line(&paragraph->content);
}

/* Returns last line in paragraph. */
static line_t *paragraph_line_last(const paragraph_t *paragraph)
{
	return content_last_line(&paragraph->content);
}



/* Things for direct conversion of text spans into lines and paragraphs. */

static int
matrices_are_compatible(const matrix4_t *ctm_a, const matrix4_t *ctm_b, int wmode)
{
	double  dot, pdot;

	/* Calculate the dot product between the direction vector transformed by each ctm. This should be large for
	 * compatible lines (cos they should be colinear). Also calculate the dot product between the direction
	 * vector transformed by the first ctm, and perp(direction vector) transformed by the second ctm. This
	 * should be zero. */
	if (wmode)
	{
		dot  = ctm_a->c * ctm_b->c + ctm_a->d * ctm_b->d;
		pdot = ctm_a->c * ctm_b->d - ctm_a->d * ctm_b->c;
	}
	else
	{
		dot  = ctm_a->a * ctm_b->a + ctm_a->b * ctm_b->b;
		pdot = ctm_a->a * ctm_b->b - ctm_a->b * ctm_b->a;
	}
	/* Negative dot means lines are in opposite sense. */
	if (dot <= 0)
		return 0;
	/* Remove the scaling from pdot to get back to a unit vector. */
	pdot /= dot;

	return (fabs(pdot) < 0.1);
}

/* Returns 1 if lines have same wmode and have the same baseline vector, else 0. */
static int
lines_are_compatible(line_t *a,
					 line_t *b)
{
	span_t *first_span_a = content_first_span(&a->content);
	span_t *first_span_b = content_first_span(&b->content);

	if (a == b) return 0;
	if (!first_span_a || !first_span_b) return 0;

	/* We only join lines with the same wmode. */
	if (first_span_a->flags.wmode != first_span_b->flags.wmode)
		return 0;

	return matrices_are_compatible(&first_span_a->ctm, &first_span_b->ctm, first_span_a->flags.wmode);
}


static const unsigned ucs_NONE = ((unsigned) -1);

/* Returns with <o_span> containing char_t's from <span> that are inside
rect, and *span modified to remove any char_t's that we have moved to
<o_span>.

May return with span->chars_num == 0, in which case the caller must remove the
span (including freeing .font_name), because lots of code assumes that there
are no empty spans. */
static int
span_inside_rect(
		extract_alloc_t *alloc,
		span_t          *span,
		rect_t          *rect,
		span_t          *o_span)
{
	int       c;
	content_t save = *(content_t *)o_span;

	*o_span = *span;
	*(content_t *)o_span = save; /* Avoid changing prev/next. */
	extract_strdup(alloc, span->font_name, &o_span->font_name);
	o_span->chars = NULL;
	o_span->chars_num = 0;
	for (c=0; c<span->chars_num; ++c)
	{
		/* For now we just look at whether span's (x, y) is within any
		rects[]. We could instead try to find character's bounding box etc. */
		char_t *char_ = &span->chars[c];
		if (char_->x >= rect->min.x &&
			char_->x <  rect->max.x &&
			char_->y >= rect->min.y &&
			char_->y <  rect->max.y)
		{
			char_t *c = extract_span_append_c(alloc, o_span, char_->ucs);
			if (c == NULL) return -1;
			*c = *char_;
			char_->ucs = ucs_NONE; /* Mark for removal below, so it is not used again. */
		}
	}

	/* Remove any char_t's that we've used. */
	{
		int cc = 0;
		for (c=0; c<span->chars_num; ++c)
		{
			char_t* char_ = &span->chars[c];
			if (char_->ucs != ucs_NONE)
			{
				span->chars[cc] = span->chars[c];
				cc += 1;
			}
		}
		/* This might set span->chars_num to zero; our caller needs to remove
		the span - lots of code assumes that all spans contain at least one
		character. */
		span->chars_num = cc;
	}

	if (o_span->chars_num)
	{
		outf("o_span: %s", extract_span_string(alloc, o_span));
	}
	return 0;
}

/*
On entry:
	<lines> is a list of span_t's.

On exit:
	<lines> is a list of line_t's, made up by having pulled as many of the span_t's
	as are appropriate together.
*/
static int
make_lines(
	extract_alloc_t *alloc,
	content_root_t  *lines,
	double           master_space_guess)
{
	int                    ret = -1;
	int                    a;
	content_line_iterator  lit;
	line_t                *line_a;
	content_span_iterator  sit;
	span_t                *span;

	/* On entry <lines> contains spans. Make each span part of a <line>. */
	for (a = 0, span = content_span_iterator_init(&sit, lines); span != NULL; span = content_span_iterator_next(&sit), a++)
	{
		line_t *line;

		if (content_replace_new_line(alloc, &span->base, &line)) goto end;
		content_append_span(&line->content, span);
		outfx("initial line a=%i: %s", a, line_string(line));
	}

	/* For each line, look for nearest aligned line, and append if found. */
	for (a=0, line_a = content_line_iterator_init(&lit, lines); line_a != NULL; a++, line_a = content_line_iterator_next(&lit))
	{
		content_line_iterator  lit2;
		line_t                *line_b;
		int                    b;
		int                    nearest_line_b = -1;
		double                 nearest_score = 0;
		line_t                *nearest_line = NULL;
		double                 nearest_colinear = 0;
		double                 nearest_space_guess = 0;
		span_t                *span_a;

		span_a = extract_line_span_last(line_a);

		for (b = 0, line_b = content_line_iterator_init(&lit2, lines); line_b != NULL; b++, line_b = content_line_iterator_next(&lit2))
		{
			if (line_a == line_b)
				continue;

			if (!lines_are_compatible(line_a, line_b))
				continue;

			{
				span_t *span_b = extract_line_span_first(line_b);
				char_t *last_a = extract_span_char_last(span_a);
				/* Predict the end of span_a. */
				point_t dir = { last_a->adv * (1 - span_a->flags.wmode), last_a->adv * span_a->flags.wmode };
				point_t tdir = extract_matrix4_transform_point(span_a->ctm, dir);
				point_t span_a_end = { last_a->x + tdir.x, last_a->y + tdir.y };
				/* Find the difference between the end of span_a and the start of span_b. */
				char_t *first_b = span_char_first(span_b);
				point_t diff = { first_b->x - span_a_end.x, first_b->y - span_a_end.y };
				double scale_squared = ((span_a->flags.wmode) ?
										(span_a->ctm.c * span_a->ctm.c + span_a->ctm.d * span_a->ctm.d) :
										(span_a->ctm.a * span_a->ctm.a + span_a->ctm.b * span_a->ctm.b));
				/* Now find the differences in position, both colinear and perpendicular. */
				double colinear = (diff.x * tdir.x + diff.y * tdir.y) / last_a->adv / scale_squared;
				double perp     = (diff.x * tdir.y - diff.y * tdir.x) / last_a->adv / scale_squared;
				/* colinear and perp are now both pre-transform space distances, to match adv etc. */
				double score;
				double space_guess = (last_a->adv + first_b->adv)/2 * master_space_guess;

				/* Heuristic: perpendicular distance larger than half of adv rules it out as a match. */
				/* Ideally we should be using font bbox here, but we don't have that, currently. */
				/* NOTE: We should match the logic in extract_add_char here! */
				if (fabs(perp) > 3*space_guess/2 || fabs(colinear) > space_guess * 8)
					 continue;

				/* We now form a score for this match. */
				score = fabs(colinear);
				if (score < fabs(perp) * 10) /* perpendicular distance matters much more. */
					score = fabs(perp) * 10;

				if (!nearest_line || score < nearest_score)
				{
					nearest_line = line_b;
					nearest_score = score;
					nearest_line_b = b;
					nearest_colinear = colinear;
					nearest_space_guess = space_guess;
				}
			}
		}

		if (nearest_line)
		{
			/* line_a and nearest_line are aligned so we can move line_b's
			spans on to the end of line_a. */
			span_t *span_b = extract_line_span_first(nearest_line);
			b = nearest_line_b;

			if (extract_span_char_last(span_a)->ucs != ' ' &&
				span_char_first(span_b)->ucs != ' ')
			{
				/* Again, match the logic in extract_add_char here. */
				int insert_space = (nearest_colinear > 2*nearest_space_guess/3);
				if (insert_space)
				{
					/* Append space to span_a before concatenation. */
					char_t *item = extract_span_append_c(alloc, span_a, ' ');
					if (item == NULL) goto end;
					item->adv = 0; /* FIXME */
					/* This is a hack to give our extra space a vaguely useful
					(x,y) coordinate - this can be used later on when ordering
					paragraphs. We could try to be more accurate by adding
					item[-1]'s .adv suitably transformed by .wmode, .ctm and
					.trm. */
					item->x = item[-1].x;
					item->y = item[-1].y;
				}
			}

			/* We might end up with two adjacent spaces here. But removing a
			space could result in an empty line_t, which could break various
			assumptions elsewhere. */

			/* Move all the content from nearest_line to line_a. */
			content_concat(&line_a->content, &nearest_line->content);

			/* Ensure that we ignore nearest_line from now on. */
			if (lit.next == &nearest_line->base)
				lit.next = lit.next->next;
			extract_line_free(alloc, &nearest_line);

			if (b > a) {
				/* We haven't yet tried appending any spans to nearest_line, so
				the new extended line_a needs checking again. */
				lit.next = &line_a->base;
				a--;
			} else {
				a--; /* b is before a, so a's number needs to move back one. */
			}
		}
	}

	ret = 0;

end:
	if (ret) {
		/* Free everything. */
		extract_span_free(alloc, &span);
		content_clear(alloc, lines);
	}
	return ret;
}


/* A comparison function, for sorting paragraphs within a
page. */
static int paragraphs_cmp(const content_t *a, const content_t *b)
{
	const paragraph_t *a_paragraph = (const paragraph_t *)a;
	const paragraph_t *b_paragraph = (const paragraph_t *)b;
	line_t *a_line, *b_line;
	span_t *a_span, *b_span;

	if (a->type != content_paragraph || b->type != content_paragraph)
		return 0;

	a_line = paragraph_line_first(a_paragraph);
	b_line = paragraph_line_first(b_paragraph);
	a_span = extract_line_span_first(a_line);
	b_span = extract_line_span_first(b_line);

	/* We can't directly compare stuff with different wmodes. */
	if (a_span->flags.wmode != b_span->flags.wmode)
	{
		return a_span->flags.wmode - b_span->flags.wmode;
	}

	/* If matrices are compatible (i.e. they share the same baseline vector), don't consider that as
	 * part of the sort.
	 *
	 * Actually, is this safe? We don't necessarily have transitivity here due to the epsilon in the
	 * comparison. A might be compatible with B, and B with C, but A might not be with C.
	 * Worry about that if we get an example.
	 */
	if (!matrices_are_compatible(&a_span->ctm, &b_span->ctm, a_span->flags.wmode))
	{
		/* If ctm matrices differ, always return this diff first. Note that we
		ignore .e and .f because if data is from ghostscript then .e and .f
		vary for each span, and we don't care about these differences. */
		return extract_matrix4_cmp(&a_span->ctm, &b_span->ctm);
	}

	/* So, we know the matrices are compatible - i.e. the baselines are parallel.
	 * Just sort on how far down the page we are going. */
	{
		span_t *span_a = content_first_span(&a_line->content);
		span_t *span_b = content_first_span(&b_line->content);
		point_t dir  = { 1 - span_a->flags.wmode, span_a->flags.wmode };
		point_t tdir = extract_matrix4_transform_point(span_a->ctm, dir);
		point_t diff = { span_a->chars[0].x - span_b->chars[0].x, span_a->chars[0].y - span_b->chars[0].y };
		double perp     = (diff.x * tdir.y - diff.y * tdir.x);

#if 0
		printf("Comparing:\n");
		content_dump_brief(&a_line->content);
		printf("And:\n");
		content_dump_brief(&b_line->content);
		printf("perp=%g\n", perp);
#endif

		if (perp < 0)
			return 1;
		if (perp > 0)
			return -1;
	}
	return 0;
}

static double
font_size_from_ctm(const matrix4_t *ctm)
{
	if (ctm->b == 0)
		return fabs(ctm->a);
	if (ctm->a == 0)
		return fabs(ctm->b);

	return sqrt(ctm->a * ctm->a + ctm->b * ctm->b);
}

static void
calculate_line_height(line_t *line)
{
	content_span_iterator  sit;
	span_t                *span;
	double                 asc = 0, desc = 0;

	for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
	{
		double span_font_size = font_size_from_ctm(&span->ctm);
		double min_y = span->font_bbox.min.y * span_font_size;
		double max_y = span->font_bbox.max.y * span_font_size;
		if (min_y < desc)
			desc = min_y;
		if (max_y > asc)
			asc = max_y;
	}
	line->ascender = asc;
	line->descender = desc;
}

/*
On entry:
  <content> is a list of lines.

On exit:
  <content> is a list of paragraphs, formed from pulling appropriate lines
  together.
*/
static int
make_paragraphs(
	extract_alloc_t *alloc,
	content_root_t  *content)
{
	int                         ret = -1;
	int                         a;
	content_line_iterator       lit;
	line_t                     *line;
	content_paragraph_iterator  pit;
	paragraph_t                *paragraph_a;

	/* Convert every line_t to be a paragraph_t containing that line_t. */
	for (line = content_line_iterator_init(&lit, content); line != NULL; line = content_line_iterator_next(&lit))
	{
		paragraph_t *paragraph;
		if (content_replace_new_paragraph(alloc, &line->base, &paragraph))
			goto end;
		content_append_line(&paragraph->content, line);
		calculate_line_height(line);
	}

	/* Now join paragraphs together where possible. */
	for (a=0, paragraph_a = content_paragraph_iterator_init(&pit, content); paragraph_a != NULL; a++, paragraph_a = content_paragraph_iterator_next(&pit)) {
		paragraph_t                *nearest_paragraph = NULL;
		int                         nearest_paragraph_b = -1;
		double                      nearest_score = 0;
		line_t                     *line_a;
		paragraph_t                *paragraph_b;
		content_paragraph_iterator  pit2;
		int b;
		span_t                     *span_a;

		line_a = paragraph_line_last(paragraph_a);
		assert(line_a != NULL);
		span_a = extract_line_span_last(line_a);
		assert(span_a != NULL);

		/* Look for nearest paragraph_t that could be appended to
		paragraph_a. */
		for (b=0, paragraph_b = content_paragraph_iterator_init(&pit2, content); paragraph_b != NULL; b++, paragraph_b = content_paragraph_iterator_next(&pit2))
		{
			line_t *line_b;

			if (paragraph_a == paragraph_b)
				continue;
			line_b = paragraph_line_first(paragraph_b);
			if (!lines_are_compatible(line_a, line_b)) {
				continue;
			}

			{
				span_t *line_a_first_span = extract_line_span_first(line_a);
				span_t *line_a_last_span  = extract_line_span_last(line_a);
				span_t *line_b_first_span = extract_line_span_first(line_b);
				span_t *line_b_last_span  = extract_line_span_last(line_b);
				char_t *first_a = span_char_first(line_a_first_span);
				char_t *first_b = span_char_first(line_b_first_span);
				char_t *last_a = span_char_last(line_a_last_span);
				char_t *last_b = span_char_last(line_b_last_span);
				point_t dir = { 1 - span_a->flags.wmode, span_a->flags.wmode };
				point_t tdir_a = extract_matrix4_transform_point(line_a_last_span->ctm, dir);
				point_t tdir_b = extract_matrix4_transform_point(line_b_last_span->ctm, dir);
				/* Find the difference between the start of span_a and the start of span_b. */
				point_t start_diff = { first_b->x - first_a->x, first_b->y - first_a->y };
				point_t end_a = { last_a->x + last_a->adv * tdir_a.x, last_a->y + last_a->adv * tdir_a.y };
				point_t end_b = { last_b->x + last_b->adv * tdir_b.x, last_b->y + last_b->adv * tdir_b.y };
				/* Now find the perpendicular difference in position. */
				double scale_squared = ((span_a->flags.wmode) ?
										(span_a->ctm.c * span_a->ctm.c + span_a->ctm.d * span_a->ctm.d) :
										(span_a->ctm.a * span_a->ctm.a + span_a->ctm.b * span_a->ctm.b));
				double perp     = (start_diff.x * tdir_a.y - start_diff.y * tdir_a.x) / sqrt(scale_squared);
				/* perp is now a post-transform space distance. */
				double score;
				/* Now consider the linear difference between: 1) start of a to end of a, 2) start of a to start of b,
				 * 3) start of a and the end of b. */
				point_t saea = { end_a.x    - first_a->x, end_a.y    - first_a->y };
				point_t sasb = { first_b->x - first_a->x, first_b->y - first_a->y };
				point_t saeb = { end_b.x    - first_a->x, end_b.y    - first_a->y };
				double dot_saea = ( saea.x * tdir_a.x + saea.y * tdir_a.y );
				double dot_sasb = ( sasb.x * tdir_a.x + sasb.y * tdir_a.y );
				double dot_saeb = ( saeb.x * tdir_a.x + saeb.y * tdir_a.y );

				/* We are only interested in scoring down the page. */
				score = -perp;

#if 0
				printf("Comparing:\n");
				content_dump_brief(&paragraph_a->content);
				printf("And:\n");
				content_dump_brief(&paragraph_b->content);
				printf("score=%g\n", score);
				printf("saea=%g sasb=%g saeb=%g\n", dot_saea, dot_sasb, dot_saeb);
#endif

				/* Check for "horizontal" alignment of the two lines. If line_b starts
				 * entirely to the right of the end of line_a, we can't join with it. */
				if (dot_sasb > dot_saea)
					continue;
				/* If line_b ends entirely to the left of the start of line_a, we can't
				 * join with it. */
				if (dot_saeb < 0)
					continue;

				if (score >= 0 && (!nearest_paragraph || score < nearest_score))
				{
					nearest_paragraph = paragraph_b;
					nearest_score = score;
					nearest_paragraph_b = b;
				}
			}
		}

		if (nearest_paragraph) {
			double line_a_height = line_a->ascender - line_a->descender;
			line_t *line_b = paragraph_line_first(nearest_paragraph);
			double line_b_height = line_b->ascender - line_b->descender;
			double expected_height = (line_a_height + line_b_height)/2;

#if 0
			printf("Best score = %g, expected_height=%g\n", nearest_score, expected_height);
#endif

			if (nearest_score > 0 && nearest_score < 2 * expected_height) {
				/* Paragraphs are close together vertically compared to maximum
				font size of first line in second paragraph, so we'll join them
				into a single paragraph. */
				span_t *a_span = extract_line_span_last(line_a);

				if (extract_span_char_last(a_span)->ucs == '-' ||
					extract_span_char_last(a_span)->ucs == 0x2212 /* unicode dash */)
				{
					/* remove trailing '-' at end of prev line. char_t doesn't
					contain any malloc-heap pointers so this doesn't leak. */
					a_span->chars_num -= 1;
					if (a_span->chars_num == 0)
					{
						/* The span is now empty, unlink and free it. */
						extract_span_free(alloc, &a_span);

						/* If this leaves the line empty, remove the line. */
						if (line_a->content.base.next == &line_a->content.base)
						{
							extract_line_free(alloc, &line_a);
							a--; /* Keep the index correct. */
						}
					}
				}
				else if (extract_span_char_last(a_span)->ucs == ' ')
				{
				}
				else if (extract_span_char_last(a_span)->ucs == '/')
				{
				}
				else
				{
					/* Insert space before joining adjacent lines. */
					char_t *c_prev;
					char_t *c = extract_span_append_c(alloc, extract_line_span_last(line_a), ' ');
					if (c == NULL) goto end;
					c_prev = &a_span->chars[ a_span->chars_num-2];
					c->x = c_prev->x + c_prev->adv * a_span->ctm.a;
					c->y = c_prev->y + c_prev->adv * a_span->ctm.c;
				}

				/* Join the two paragraphs by moving content from nearest_paragraph to paragraph_a. */
				content_concat(&paragraph_a->content, &nearest_paragraph->content);

#if 0
				printf("Joining to give:\n");
				content_dump_brief(&paragraph_a->content);
#endif

				/* Ensure that we skip nearest_paragraph in future. */
				if (pit.next == &nearest_paragraph->base)
					pit.next = pit.next->next;
				extract_paragraph_free(alloc, &nearest_paragraph);

				if (nearest_paragraph_b > a) {
					/* We haven't yet tried appending any paragraphs to
					nearest_paragraph_b, so the new extended paragraph_a needs
					checking again. */
					pit.next = &paragraph_a->base;
					a -= 1;
				} else {
					a -= 1;
				}
			}
		}
	}

	/* Sort paragraphs so they appear in correct order, using paragraphs_cmp().
	*/
	content_sort(content, paragraphs_cmp);

	ret = 0;

end:

	return ret;
}

/* Return the last non space char of a span (or the first one if
 * they are all spaces. */
static char_t *
last_non_space_char(span_t *span)
{
	int i = span->chars_num - 1;

	while (i > 0 && span->chars[i].ucs == 32)
		i--;

	return &span->chars[i];
}

/*
On entry:
  <content> is a list of paragraphs.

On exit:
  <content> is a list of paragraphs, with information about alignment etc.
*/
static int
analyse_paragraphs(content_root_t *content)
{
	content_paragraph_iterator  pit;
	paragraph_t                *paragraph;

	/* Examine each paragraph in turn. */
	for (paragraph = content_paragraph_iterator_init(&pit, content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
	{
		content_line_iterator  lit;
		line_t                *line;
		double                 para_l = 0, para_r = 0;
		int                    first_span_of_para = 1;
		matrix4_t              inverse;
		double                 space_guess = 0; /* Stop clever-clever compilers warning. */
		int                    previous_line_flags = -1;
		double                 previous_line_spare = 0;
		int                    first_line = 1;

		/* Bound this paragraph on the left and right, pre-transform. */
		for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
		{
			content_span_iterator  sit;
			span_t                *span;

			for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
			{
				char_t    *lc     = &span->chars[0];
				char_t    *rc     = last_non_space_char(span);
				point_t    dir    = { rc->adv * (1 - span->flags.wmode), rc->adv * span->flags.wmode };
				point_t    tdir   = extract_matrix4_transform_point(span->ctm, dir);
				point_t    left   = { lc->x, lc->y };
				point_t    right  = { rc->x + tdir.x, rc->y + tdir.y };
				double     l, r;

				/* We examine the ctm on the first span, and store its inverse. We then map all
				 * the coords we encounter back through this, to give us a position in a consistent
				 * source space (i.e. we don't use each different ctm we meet for different spans). */
				if (first_span_of_para)
				{
					inverse = extract_matrix4_invert(&span->ctm);
					space_guess = (span->font_bbox.max.x - span->font_bbox.min.x)/2;
				}

				left  = extract_matrix4_transform_point(inverse, left);
				right = extract_matrix4_transform_point(inverse, right);
				l = span->flags.wmode ? left.y  : left.x;
				r = span->flags.wmode ? right.y : right.x;

				if (l < para_l || first_span_of_para)
					para_l = l;
				if (r > para_r || first_span_of_para)
					para_r = r;
				first_span_of_para = 0;
			}
		}

		/* So now we know that para_l/para_r are the bounds for this paragraph, under the ctm of the first_span. */

		/* Now, look again at the lines that made up that paragraph, to figure out how well each line
		 * fills those bounds. */
		for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
		{
			content_span_iterator  sit;
			span_t                *span;
			double                 line_l = 0, line_r = 0;
			int                    first_span = 1;

			/* If we're not the first line, then we want to find the first word. */
			int                    first_word = !first_line;
			int                    word_width_found = 0;
			point_t                word_end;
			int                    word_wmode;

			/* For each line, find the line bounds. */
			for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
			{
				char_t    *lc     = &span->chars[0];
				char_t    *rc     = last_non_space_char(span);
				point_t    dir    = { 1 - span->flags.wmode, span->flags.wmode };
				point_t    tdir   = extract_matrix4_transform_point(span->ctm, dir);
				point_t    left   = { lc->x, lc->y };
				point_t    right  = { rc->x + tdir.x * rc->adv, rc->y + tdir.y * rc->adv };
				double     l, r;

				/* If we're not the first line, then calculate the length of the first word on the
				 * new line. */
				if (first_word)
				{
					int i;

					for (i = 0; i < span->chars_num; i++)
					{
						if (span->chars[i].ucs == 32)
							break;
					}
					if (i > 0)
					{
						double adv = span->chars[i-1].adv;
						word_end.x = span->chars[i-1].x + adv * tdir.x;
						word_end.y = span->chars[i-1].y + adv * tdir.y;
						word_wmode = span->flags.wmode;
						word_width_found = 1;
						if (i < span->chars_num)
							first_word = 0;
					}
				}

				left  = extract_matrix4_transform_point(inverse, left);
				right = extract_matrix4_transform_point(inverse, right);
				l = span->flags.wmode ? left.y  : left.x;
				r = span->flags.wmode ? right.y : right.x;

				if (l < line_l || first_span)
					line_l = l;
				if (r < line_r || first_span)
					line_r = r;
				first_span = 0;
			}

#if 0
			printf("Considering:\n");
			content_dump_brief(&line->content);
			printf("\n");
#endif

			/* Did we find a width for the first word? */
			if (word_width_found)
			{
				double w;
				word_end = extract_matrix4_transform_point(inverse, word_end);
				w = word_wmode ? word_end.y : word_end.x;
				w -= line_l;
				/* Now w = the extent of the word. */
				/* If the previous line had enough room for this extent, plus a space,
				 * then we know that this paragraph can't just be breaking lines when
				 * they get full. */
				if (previous_line_spare > w + space_guess)
					paragraph->line_flags |= paragraph_breaks_strangely;
			}

			if (previous_line_flags != -1)
			{
				paragraph->line_flags |= previous_line_flags;
			}
			previous_line_flags = 0;
			if (line_l > para_l + space_guess)
				previous_line_flags |= paragraph_not_aligned_left;
			if (line_r < para_r - space_guess)
				previous_line_flags |= paragraph_not_aligned_right;
			{
				/* In order to figure out if we are plausibly centred,
				 * calculate the l and r gaps. */
				double l = line_l - para_l;
				double r = para_r - line_r;

				if (fabs(l - r) > space_guess/2)
					paragraph->line_flags |= paragraph_not_centred;
				if (l > space_guess/2)
					paragraph->line_flags |= paragraph_not_fully_justified;
				if (r > space_guess/2)
					previous_line_flags |= paragraph_not_fully_justified;
			}
			previous_line_spare = para_r - line_r + line_l - para_l;
			first_line = 0;
		}

		if (previous_line_flags != -1)
		{
			paragraph->line_flags |= (previous_line_flags & paragraph_not_aligned_left);
		}
	}

	return 0;
}

static int
spot_rotated_blocks(
		extract_alloc_t *alloc,
		content_root_t  *lines)
{
	/* On entry, we have that the content in lines has been
	sorted so that paragraphs with the same rotation are together,
	and sorted in order of increasing y. All we have to do here is
	to spot a run of paragraphs with the same rotation, and to gather
	them into a block. */
	content_iterator  cit;
	content_t        *content;
	content_iterator  cit0 = { 0 }; /* Stop clever-clever compilers warning. */
	content_t        *content0 = NULL;
	int               ret = -1;
	matrix4_t         ctm0;
	int               wmode, wmode0;
	int               ctm0_set = 0;

	for (content = content_iterator_init(&cit, lines); content != NULL; content = content_iterator_next(&cit))
	{
		matrix4_t ctm;
		int       ctm_set = 0;
		int       flush = 0;

		switch (content->type)
		{
			case content_paragraph:
			{
				double rotate;
				span_t *span = content_first_span(&content_first_line(&((paragraph_t *)content)->content)->content);
				wmode = span->flags.wmode;
				ctm = span->ctm;
				rotate = atan2(ctm.b, ctm.a);
				/* We are not gathering rotated stuff into blocks. If the rotation returns to zero
				 * then flush any collection we might have found. Otherwise, remember that we have
				 * a ctm value set, so we can compare to it. */
				if (rotate == 0)
					flush = 1;
				else
					ctm_set = 1;
				/* If the ctm value differs from the first ctm0 we met for the current collection,
				 * flush the collection. */
				if (ctm0_set && (wmode != wmode0 || !matrices_are_compatible(&ctm, &ctm0, wmode0)))
					flush = 1;
				break;
			}
			default:
				flush = 1;
				break;
		}

		if (flush && content0)
		{
			/* Move [content0..content) to a block */
			block_t *block;
			content_t *c = content_iterator_next(&cit0);
			/* Replace content0 with new block. */
			if (content_replace_new_block(alloc, content0, &block)) goto end;
			/* Insert content0 into block. */
			content_append(&block->content, content0);
			/* Now move the rest of the list into block too. */
			for (; c != content; c = content_iterator_next(&cit0))
			{
				content_append(&block->content, c);
			}
			ctm0_set = 0;
			content0 = NULL;
		}
		if (ctm_set && !ctm0_set)
		{
			ctm0 = ctm;
			ctm0_set = 1;
			wmode0 = wmode;
			content0 = content;
			cit0 = cit;
		}
	}

	if (content0)
	{
		/* Move [content0..NULL) to a block */
		block_t *block;
		content_t *c = content_iterator_next(&cit0);
		/* Replace content0 with new block. */
		if (content_replace_new_block(alloc, content0, &block)) goto end;
		/* Insert content0 into block. */
		content_append(&block->content, content0);
		/* Now move the rest of the list into block too. */
		for (; c != content; c = content_iterator_next(&cit0))
		{
			content_append(&block->content, c);
		}
	}

	ret = 0;
end:

	return ret;
}

/* Take any content from content that falls inside rect, and append
 * it to subset. */
static int
spans_within_rect(
		extract_alloc_t *alloc,
		content_root_t  *content,
		rect_t          *rect,
		content_root_t  *subset)
{
	content_span_iterator  it;
	span_t                *candidate;

	/* Make <lines> contain new span_t's and char_t's that are inside rects[]. */
	for (candidate = content_span_iterator_init(&it, content); candidate != NULL; candidate = content_span_iterator_next(&it))
	{
		span_t *span;

		if (candidate->chars_num == 0)
			continue; /* In case used for table, */

		/* Create a new span. */
		if (content_new_span(alloc, &span, candidate->structure))
			return -1;
		/* Extract any chars from candidate that fall inside rect, inserting
		 * those chars into subset. */
		if (span_inside_rect(alloc, candidate, rect, span))
			return -1;
		if (span->chars_num)
		{
			/* We populated it with some chars! */
			/* Unlink span from where it was, and insert into subset. */
			content_append_span(subset, span);
		}
		else
		{
			/* No chars found. Bin it. */
			extract_span_free(alloc, &span);
		}
		span = NULL; /* Avoid us freeing on error now ownership has moved. */

		if (!candidate->chars_num)
		{
			/* All characters in this span are inside table, so remove
			 * the vestigial span. */
			extract_span_free(alloc, &candidate);
		}
	}

	return 0;
}

static int
join_content(
	extract_alloc_t *alloc,
	content_root_t  *lines,
	double master_space_guess)
{
	if (make_lines(alloc, lines, master_space_guess))
		return -1;
	if (make_paragraphs(alloc, lines))
		return -1;
	if (analyse_paragraphs(lines))
		return -1;
	if (spot_rotated_blocks(alloc, lines))
		return -1;

	return 0;
}


/* Compares two tableline_t's rectangles using x as primary key. */
static int tablelines_compare_x(const void *a, const void *b)
{
	const tableline_t *aa = a;
	const tableline_t *bb = b;

	if (aa->rect.min.x > bb->rect.min.x)    return +1;
	if (aa->rect.min.x < bb->rect.min.x)    return -1;
	if (aa->rect.min.y > bb->rect.min.y)    return +1;
	if (aa->rect.min.y < bb->rect.min.y)    return -1;

	return 0;
}

/* Compares two tableline_t's rectangles using y as primary key. */
static int tablelines_compare_y(const void *a, const void *b)
{
	const tableline_t *aa = a;
	const tableline_t *bb = b;

	if (aa->rect.min.y > bb->rect.min.y)    return +1;
	if (aa->rect.min.y < bb->rect.min.y)    return -1;
	if (aa->rect.min.x > bb->rect.min.x)    return +1;
	if (aa->rect.min.x < bb->rect.min.x)    return -1;

	return 0;
}

/* Makes <out> to contain all lines in <all> with y coordinate in the range
y_min..y_max. */
static int
table_find_y_range(
		extract_alloc_t *alloc,
		tablelines_t    *all,
		double           y_min,
		double           y_max,
		tablelines_t    *out)
{
	int i;

	for (i=0; i<all->tablelines_num; ++i)
	{
		if (all->tablelines[i].rect.min.y >= y_min && all->tablelines[i].rect.min.y < y_max)
		{
			if (extract_realloc(alloc, &out->tablelines, sizeof(*out->tablelines) * (out->tablelines_num + 1))) return -1;
			out->tablelines[out->tablelines_num] = all->tablelines[i];
			out->tablelines_num += 1;
		}
		else
		{
			outf("Excluding line because outside y=%f..%f: %s", y_min, y_max, extract_rect_string(&all->tablelines[i].rect));
		}
	}

	return 0;
}


/* Returns one if a_min..a_max significantly overlapps b_min..b_max, otherwise
zero. */
static int
overlap(double a_min, double a_max, double b_min, double b_max)
{
	double overlap;
	int ret0;
	int ret1;

	assert(a_min < a_max);
	assert(b_min < b_max);
	if (b_min < a_min)  b_min = a_min;
	if (b_max > a_max)  b_max = a_max;
	if (b_max < b_min)  b_max = b_min;
	overlap = (b_max - b_min) / (a_max - a_min);
	ret0 = overlap > 0.2;
	ret1 = overlap > 0.8;
	if (ret0 != ret1)
	{
		if (0) outf0("warning, unclear overlap=%f: a=%f..%f b=%f..%f", overlap, a_min, a_max, b_min, b_max);
	}

	return overlap > 0.8;
}

void extract_cell_init(cell_t *cell)
{
	cell->rect.min.x = 0;
	cell->rect.min.y = 0;
	cell->rect.max.x = 0;
	cell->rect.max.y = 0;
	cell->above = 0;
	cell->left = 0;
	cell->extend_right = 0;
	cell->extend_down = 0;
	content_init_root(&cell->content, NULL);
}


/* Find cell extensions to right and down by looking at cells' .left and
above flags.

For example for adjacent cells ABC..., we extend A to include cells BC..
until we reach a cell with .left set to one.

ABCDE
FGHIJ
KLMNO

When looking to extend cell A, we only look at cells in the same column or
same row, (i.e. in the above example we look at BCDE and FK, and not at
GHIJ and LMNO).

For example if BCDE have no left lines and FK have no above lines, we
ignore any lines in GHIJ and LMNO and make A extend to the entire 3x4
box. Having found this box, we set .above=0 and .left to 0 in all enclosed
cells, which simplifies html table generation code.
*/
static int table_find_extend(cell_t **cells, int cells_num_x, int cells_num_y)
{
	int y;

	for (y=0; y<cells_num_y; ++y)
	{
		int x;
		for (x=0; x<cells_num_x; ++x)
		{
			cell_t* cell = cells[y * cells_num_x + x];
			outf("xy=(%i %i) above=%i left=%i", x, y, cell->above, cell->left);
			if (cell->left && cell->above)
			{
				/* See how far this cell extends to right and down. */
				int xx;
				int yy;
				for (xx=x+1; xx<cells_num_x; ++xx)
				{
					if (cells[y * cells_num_x + xx]->left)  break;
				}
				cell->extend_right = xx - x;
				cell->rect.max.x = cells[y * cells_num_x + xx-1]->rect.max.x;
				for (yy=y+1; yy<cells_num_y; ++yy)
				{
					if (cells[yy * cells_num_x + x]->above) break;
				}
				cell->extend_down = yy - y;
				cell->rect.max.y = cells[(yy-1) * cells_num_x + x]->rect.max.y;

				/* Clear .above and .left in enclosed cells. */
				for (xx = x; xx < x + cell->extend_right; ++xx)
				{
					int yy;
					for (yy = y; yy < y + cell->extend_down; ++yy)
					{
						cell_t* cell2 = cells[cells_num_x * yy  + xx];
						if ( xx==x && yy==y)
						{}
						else
						{
							if (xx==x)
							{
								cell2->extend_right = cell->extend_right;
							}
							cell2->above = 0;
							/* We set .left to 1 for left-most cells - e.g. F
							and K in the above diagram; this allows us to
							generate correct html without lots of recursing
							looking for extend_down in earlier cells. */
							cell2->left = (xx == x);
							outf("xy=(%i %i) xxyy=(%i %i) have set cell2->above=%i left=%i",
									x, y, xx, yy, cell2->above, cell2->left
									);
						}
					}
				}
			}
		}
	}
	return 0;
}


/* Sets each cell to contain the text that is within the cell's boundary. We
remove any found text from the page. */
static int
table_find_cells_text(
		extract_alloc_t  *alloc,
		subpage_t        *subpage,
		cell_t          **cells,
		int               cells_num_x,
		int               cells_num_y,
		double            master_space_guess)
{
	/* Find text within each cell. We don't attempt to handle images within
	cells. */
	int      e = -1;
	int      i;
	int      cells_num = cells_num_x * cells_num_y;
	table_t *table;

	for (i=0; i<cells_num; ++i)
	{
		cell_t* cell = cells[i];
		if (!cell->above || !cell->left) continue;

		if (spans_within_rect(alloc, &subpage->content, &cell->rect, &cell->content))
			return -1;
		if (join_content(alloc, &cell->content, master_space_guess))
			return -1;
	}

	/* Append the table we have found to page->tables[]. */
	if (content_append_new_table(alloc, &subpage->tables, &table)) goto end;
	table->pos.x = cells[0]->rect.min.x;
	table->pos.y = cells[0]->rect.min.y;
	table->cells = cells;
	table->cells_num_x = cells_num_x;
	table->cells_num_y = cells_num_y;

	if (0)
	{
		/* For debugging. */
		int y;
		outf0("table:\n");
		for (y=0; y<cells_num_y; ++y)
		{
			int x;
			for (x=0; x<cells_num_x; ++x)
			{
				cell_t* cell = cells[cells_num_x * y + x];
				fprintf(stderr, "    %c%c x=%i y=% 3i 3i w=%i h=%i",
						cell->left ? '|' : ' ',
						cell->above ? '-' : ' ',
						x,
						y,
						cell->extend_right,
						cell->extend_down
						);
			}
			fprintf(stderr, "\n");
		}

	}

	e = 0;
end:

	return e;
}


/* Finds single table made from lines whose y coordinates are in the range
y_min..y_max. */
static int
table_find(extract_alloc_t *alloc, subpage_t *subpage, double y_min, double y_max, double master_space_guess)
{
	tablelines_t *all_h = &subpage->tablelines_horizontal;
	tablelines_t *all_v = &subpage->tablelines_vertical;
	int e = -1;
	int i;

	/* Find subset of vertical and horizontal lines that are within range
	y_min..y_max, and sort by y coordinate. */
	tablelines_t   tl_h = {NULL, 0};
	tablelines_t   tl_v = {NULL, 0};
	cell_t       **cells = NULL;
	int            cells_num = 0;
	int            cells_num_x = 0;
	int            cells_num_y = 0;
	int            x;
	int            y;

	outf("y=(%f %f)", y_min, y_max);

	if (table_find_y_range(alloc, all_h, y_min, y_max, &tl_h)) goto end;
	if (table_find_y_range(alloc, all_v, y_min, y_max, &tl_v)) goto end;
	/* Suppress false coverity warning - qsort() does not dereference null
	pointer if nmemb is zero. */
	/* coverity[var_deref_model] */
	qsort(tl_v.tablelines, tl_v.tablelines_num, sizeof(*tl_v.tablelines), tablelines_compare_x);

	if (0)
	{
		/* Show raw lines info. */
		outf0("all_h->tablelines_num=%i tl_h.tablelines_num=%i", all_h->tablelines_num, tl_h.tablelines_num);
		for (i=0; i<tl_h.tablelines_num; ++i)
		{
			outf0("    %i: %s", i, extract_rect_string(&tl_h.tablelines[i].rect));
		}

		outf0("all_v->tablelines_num=%i tl_v.tablelines_num=%i", all_v->tablelines_num, tl_v.tablelines_num);
		for (i=0; i<tl_v.tablelines_num; ++i)
		{
			outf0("    %i: %s", i, extract_rect_string(&tl_v.tablelines[i].rect));
		}
	}
	/* Find the cells defined by the vertical and horizontal lines.

	It seems that lines can be disjoint, e.g. what looks like a single
	horizontal line could be made up of multiple lines all with the same
	y coordinate, so we use i_next and j_next to skip these sublines when
	iterating. */
	cells = NULL;
	cells_num = 0;
	cells_num_x = 0;
	cells_num_y = 0;
	for (i=0; i<tl_h.tablelines_num; )
	{
		int i_next;
		int j;
		for (i_next=i+1; i_next<tl_h.tablelines_num; ++i_next)
		{
			if (tl_h.tablelines[i_next].rect.min.y - tl_h.tablelines[i].rect.min.y > 5) break;
		}
		if (i_next == tl_h.tablelines_num)
		{
			/* Ignore last row of points - cells need another row below. */
			break;
		}
		cells_num_y += 1;

		for (j=0; j<tl_v.tablelines_num; )
		{
			int j_next;
			int ii;
			int jj;
			cell_t* cell;

			for (j_next = j+1; j_next<tl_v.tablelines_num; ++j_next)
			{
				if (tl_v.tablelines[j_next].rect.min.x - tl_v.tablelines[j].rect.min.x > 0.5) break;
			}
			outf("i=%i j=%i tl_v.tablelines[j].rect=%s", i, j, extract_rect_string(&tl_v.tablelines[j].rect));

			if (j_next == tl_v.tablelines_num) break;

			if (extract_realloc(alloc, &cells, sizeof(*cells) * (cells_num+1))) goto end;
			if (extract_malloc(alloc, &cells[cells_num], sizeof(*cells[cells_num]))) goto end;
			cell = cells[cells_num];
			cells_num += 1;
			if (i==0)   cells_num_x += 1;

			cell->rect.min.x = tl_v.tablelines[j].rect.min.x;
			cell->rect.min.y = tl_h.tablelines[i].rect.min.y;
			cell->rect.max.x = (j_next < tl_v.tablelines_num) ? tl_v.tablelines[j_next].rect.min.x : cell->rect.min.x;
			cell->rect.max.y = (i_next < tl_h.tablelines_num) ? tl_h.tablelines[i_next].rect.min.y : cell->rect.min.y;
			cell->above = (i==0);
			cell->left = (j==0);
			cell->extend_right = 1;
			cell->extend_down = 1;
			content_init_root(&cell->content, NULL);

			/* Set cell->above if there is a horizontal line above the cell. */
			outf("Looking to set above for i=%i j=%i rect=%s", i, j, extract_rect_string(&cell->rect));
			for (ii = i; ii < i_next; ++ii)
			{
				tableline_t* h = &tl_h.tablelines[ii];
				if (overlap(
						cell->rect.min.x,
						cell->rect.max.x,
						h->rect.min.x,
						h->rect.max.x
						))
				{
					cell->above = 1;
					break;
				}
			}

			/* Set cell->left if there is a vertical line to the left of the cell. */
			for (jj = j; jj < j_next; ++jj)
			{
				tableline_t* v = &tl_v.tablelines[jj];
				if (overlap(
						cell->rect.min.y,
						cell->rect.max.y,
						v->rect.min.y,
						v->rect.max.y
						))
				{
					cell->left = 1;
					break;
				}
			}

			j = j_next;
		}

		i = i_next;
	}

	assert(cells_num == cells_num_x * cells_num_y);

	/* Remove cols and rows where no cells have .above and .left - these
	will not appear. It also avoids spurious empty columns when table uses
	closely-spaced double lines as separators. */
	for (x=0; x<cells_num_x; ++x)
	{
		int has_cells = 0;
		for (y=0; y<cells_num_y; ++y)
		{
			cell_t* cell = cells[y * cells_num_x + x];
			if (cell->above && cell->left)
			{
				has_cells = 1;
				break;
			}
		}
		if (!has_cells)
		{
			/* Remove column <x>. */
			int j = 0;
			outf("Removing column %i. cells_num=%i cells_num_x=%i cells_num_y=%i", x, cells_num, cells_num_x, cells_num_y);
			for (i=0; i<cells_num; ++i)
			{
				if (i % cells_num_x == x)
				{
					extract_cell_free(alloc, &cells[i]);
					continue;
				}
				cells[j] = cells[i];
				j += 1;
			}
			cells_num -= cells_num_y;
			cells_num_x -= 1;
		}
	}

	if (cells_num == 0)
	{
		e = 0;
		goto end;
	}

	if (table_find_extend(cells, cells_num_x, cells_num_y)) goto end;

	if (table_find_cells_text(alloc, subpage, cells, cells_num_x, cells_num_y, master_space_guess)) goto end;

	e = 0;
end:

	extract_free(alloc, &tl_h.tablelines);
	extract_free(alloc, &tl_v.tablelines);
	if (e)
	{
		for (i=0; i<cells_num; ++i)
		{
			extract_cell_free(alloc, &cells[i]);
		}
		extract_free(alloc, &cells);
	}

	return e;
}


/* Finds tables in <page> by looking for lines in page->tablelines_horizontal
and page->tablelines_vertical that look like table dividers.

Any text found inside tables is removed from page->spans[].
*/
static int extract_subpage_tables_find_lines(
		extract_alloc_t *alloc,
		subpage_t       *subpage,
		double           master_space_guess)
{
	double miny;
	double maxy;
	double margin = 1;
	int iv;
	int ih;
	outf("page->tablelines_horizontal.tablelines_num=%i", subpage->tablelines_horizontal.tablelines_num);
	outf("page->tablelines_vertical.tablelines_num=%i", subpage->tablelines_vertical.tablelines_num);

	/* Sort all lines by y coordinate. */
	qsort(subpage->tablelines_horizontal.tablelines,
		  subpage->tablelines_horizontal.tablelines_num,
		  sizeof(*subpage->tablelines_horizontal.tablelines),
		  tablelines_compare_y);
	qsort(subpage->tablelines_vertical.tablelines,
		  subpage->tablelines_vertical.tablelines_num,
		  sizeof(*subpage->tablelines_vertical.tablelines),
		  tablelines_compare_y);

	if (0)
	{
		/* Show info about lines. */
		int i;
		outf0("tablelines_horizontal:");
		for (i=0; i<subpage->tablelines_horizontal.tablelines_num; ++i)
		{
			outf0("    color=%f: %s",
					subpage->tablelines_horizontal.tablelines[i].color,
					extract_rect_string(&subpage->tablelines_horizontal.tablelines[i].rect)
					);
		}
		outf0("tablelines_vertical:");
		for (i=0; i<subpage->tablelines_vertical.tablelines_num; ++i)
		{
			outf0("    color=%f: %s",
					subpage->tablelines_vertical.tablelines[i].color,
					extract_rect_string(&subpage->tablelines_vertical.tablelines[i].rect)
					);
		}
	}

	/* Look for completely separate vertical regions that define different
	tables, by looking for vertical gaps between the rects of each
	horizontal/vertical line. */
	maxy = -DBL_MAX;
	miny = -DBL_MAX;
	iv = 0;
	ih = 0;
	for(;;)
	{
		tableline_t *tlv = NULL;
		tableline_t *tlh = NULL;
		tableline_t *tl;
		if (iv < subpage->tablelines_vertical.tablelines_num)
		{
			tlv = &subpage->tablelines_vertical.tablelines[iv];
		}
		/* We only consider horizontal lines that are not white. This is a bit
		of a cheat to get the right behaviour with twotables_2.pdf. */
		while (ih < subpage->tablelines_horizontal.tablelines_num)
		{
			if (subpage->tablelines_horizontal.tablelines[ih].color == 1)
			{
				/* Ignore white horizontal lines. */
				++ih;
			}
			else
			{
				tlh = &subpage->tablelines_horizontal.tablelines[ih];
				break;
			}
		}
		if (tlv && tlh)
		{
			tl = (tlv->rect.min.y < tlh->rect.min.y) ? tlv : tlh;
		}
		else if (tlv) tl = tlv;
		else if (tlh) tl = tlh;
		else break;
		if (tl == tlv)  iv += 1;
		else ih += 1;
		if (tl->rect.min.y > maxy + margin)
		{
			if (maxy > miny)
			{
				outf("New table. maxy=%f miny=%f", maxy, miny);
				/* Find table. */
				table_find(alloc, subpage, miny - margin, maxy + margin, master_space_guess);
			}
			miny = tl->rect.min.y;
		}
		if (tl->rect.max.y > maxy)  maxy = tl->rect.max.y;
	}

	/* Find last table. */
	table_find(alloc, subpage, miny - margin, maxy + margin, master_space_guess);

	return 0;
}


/* For debugging only. */
static void show_tables(content_root_t *tables)
{
	content_table_iterator  tit;
	table_t                *table;

	outf0("tables_num=%i", content_count_tables(tables));
	for (table = content_table_iterator_init(&tit, tables); table != NULL; table = content_table_iterator_next(&tit))
	{
		int y;
		outf0("table: cells_num_y=%i cells_num_x=%i", table->cells_num_y, table->cells_num_x);
		for (y=0; y<table->cells_num_y; ++y)
		{
			int x;
			for (x=0; x<table->cells_num_x; ++x)
			{
				cell_t* cell = table->cells[table->cells_num_x * y + x];
				outf0("cell: y=% 3i x=% 3i: left=%i above=%i rect=%s",
						y, x, cell->left, cell->above, extract_rect_string(&cell->rect));
			}
		}
	}
}

/* Find tables in <page>.

At the moment this only calls extract_page_tables_find_lines(), but in future
will call other functions that find tables in different ways, e.g. by analysing
an image of a page, or looking for blocks of whitespace in between chunks of
text. */
static int
extract_subpage_tables_find(
		extract_alloc_t *alloc,
		subpage_t       *subpage,
		double           master_space_guess)
{
	if (extract_subpage_tables_find_lines(alloc, subpage, master_space_guess)) return -1;

	if (0)
	{
		outf0("=== tables from extract_page_tables_find_lines():");
		show_tables(&subpage->tables);
	}

	return 0;
}

/* Finds tables and paragraphs on <page>. */
static int
extract_join_subpage(
		extract_alloc_t *alloc,
		subpage_t       *subpage,
		double           master_space_guess)
{
	/* Find tables on this page first. This will remove text that is within
	tables from page->spans, so that text doesn't appear more than once in
	the final output. */
	if (extract_subpage_tables_find(alloc, subpage, master_space_guess)) return -1;

	/* Now join remaining spans into lines and paragraphs. */
	if (join_content(alloc, &subpage->content, master_space_guess))
		return -1;

	return 0;
}


/* For each page in <document> we find tables and join spans into lines and paragraphs.

A line is a list of spans that are at the same angle and on the same
line. A paragraph is a list of lines that are at the same angle and close
together.
*/
int extract_document_join(extract_alloc_t *alloc, document_t *document, int layout_analysis, double master_space_guess)
{
	int p;

	for (p=0; p<document->pages_num; ++p) {
		extract_page_t* page = document->pages[p];
		int c;

		/* If we have layout analysis enabled, then we do our 'boxer' analysis to
		 * try to spot subdivisions and subpages. */
		if (layout_analysis && extract_page_analyse(alloc, page)) return -1;

		for (c=0; c<page->subpages_num; ++c) {
			subpage_t* subpage = page->subpages[c];

			outf("processing page %i, subpage %i: num_spans=%i", p, c, content_count_spans(&subpage->content));
			if (extract_join_subpage(alloc, subpage, master_space_guess)) return -1;
		}
	}

	return 0;
}
