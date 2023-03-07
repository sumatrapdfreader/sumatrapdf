#include "document.h"
#include "outf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void
content_init(content_t *content, content_type_t type)
{
	content->type = type;
	content->next = content->prev = (type == content_root) ? content : NULL;
}

void
content_init_root(content_root_t *content, content_t *parent)
{
	content->base.type = content_root;
	content->base.next = content->base.prev = &content->base;
	content->parent = parent;
}

void
content_unlink(content_t *content)
{
	if (content == NULL)
		return;

	assert(content->type != content_root);

	if (content->prev == NULL)
	{
		assert(content->next == NULL);
		/* Already unlinked */
	}
	else
	{
		assert(content->next != content && content->prev != content);
		content->prev->next = content->next;
		content->next->prev = content->prev;
		content->next = content->prev = NULL;
	}
}

void content_unlink_span(span_t *span)
{
	content_unlink(&span->base);
}

void extract_span_init(span_t *span, structure_t *structure)
{
	static const span_t blank = { 0 };

	*span = blank;
	content_init(&span->base, content_span);
	span->structure = structure;
}

void extract_span_free(extract_alloc_t *alloc, span_t **pspan)
{
	if (*pspan == NULL)
		return;

	content_unlink(&(*pspan)->base);
	extract_free(alloc, &(*pspan)->font_name);
	extract_free(alloc, &(*pspan)->chars);
	extract_free(alloc, pspan);
}

void extract_line_init(line_t *line)
{
	static const line_t blank = { 0 };

	*line = blank;
	content_init(&line->base, content_line);
	content_init_root(&line->content, &line->base);
}

void extract_paragraph_init(paragraph_t *paragraph)
{
	static const paragraph_t blank = { 0 };

	*paragraph = blank;
	content_init(&paragraph->base, content_paragraph);
	content_init_root(&paragraph->content, &paragraph->base);
}

void extract_block_init(block_t *block)
{
	static const block_t blank = { 0 };

	*block = blank;
	content_init(&block->base, content_block);
	content_init_root(&block->content, &block->base);
}

void extract_table_init(table_t *table)
{
	static const table_t blank = { 0 };

	*table = blank;
	content_init(&table->base, content_table);
}

void extract_image_init(image_t *image)
{
	static const image_t blank = { 0 };

	*image = blank;
	content_init(&image->base, content_image);
}

void
content_clear(extract_alloc_t *alloc, content_root_t *proot)
{
	content_t *content, *next;

	assert(proot->base.type == content_root && proot->base.next != NULL && proot->base.prev != NULL);
	for (content = proot->base.next; content != &proot->base; content = next)
	{
		assert(content->type != content_root);
		next = content->next;
		switch (content->type)
		{
		default:
		case content_root:
			assert("This never happens" == NULL);
			break;
		case content_span:
			extract_span_free(alloc, (span_t **)&content);
			break;
		case content_line:
			extract_line_free(alloc, (line_t **)&content);
			break;
		case content_paragraph:
			extract_paragraph_free(alloc, (paragraph_t **)&content);
			break;
		case content_block:
			extract_block_free(alloc, (block_t **)&content);
			break;
		case content_table:
			extract_table_free(alloc, (table_t **)&content);
			break;
		case content_image:
			extract_image_free(alloc, (image_t **)&content);
			break;
		}
	}
}

int
content_count(content_root_t *root)
{
	int n = 0;
	content_t *s;

	for (s = root->base.next; s != &root->base; s = s->next)
		n++;

	return n;
}

static int
content_count_type(content_root_t *root, content_type_t type)
{
	int n = 0;
	content_t *s;

	for (s = root->base.next; s != &root->base; s = s->next)
		if (s->type == type) n++;

	return n;
}

int content_count_spans(content_root_t *root)
{
	return content_count_type(root, content_span);
}

int content_count_images(content_root_t *root)
{
	return content_count_type(root, content_image);
}

int content_count_lines(content_root_t *root)
{
	return content_count_type(root, content_line);
}

int content_count_paragraphs(content_root_t *root)
{
	return content_count_type(root, content_paragraph);
}

int content_count_tables(content_root_t *root)
{
	return content_count_type(root, content_table);
}

static content_t *
content_first_of_type(const content_root_t *root, content_type_t type)
{
	content_t *content;
	assert(root && root->base.type == content_root);

	for (content = root->base.next; content != &root->base; content = content->next)
	{
		if (content->type == type)
			return content;
	}
	return NULL;
}

static content_t *
content_last_of_type(const content_root_t *root, content_type_t type)
{
	content_t *content;
	assert(root && root->base.type == content_root);

	for (content = root->base.prev; content != &root->base; content = content->prev)
	{
		if (content->type == type)
			return content;
	}
	return NULL;
}

span_t *content_first_span(const content_root_t *root)
{
	return (span_t *)content_first_of_type(root, content_span);
}

span_t *content_last_span(const content_root_t *root)
{
	return (span_t *)content_last_of_type(root, content_span);
}

line_t *content_first_line(const content_root_t *root)
{
	return (line_t *)content_first_of_type(root, content_line);
}

line_t *content_last_line(const content_root_t *root)
{
	return (line_t *)content_last_of_type(root, content_line);
}

paragraph_t *content_first_paragraph(const content_root_t *root)
{
	return (paragraph_t *)content_first_of_type(root, content_paragraph);
}

paragraph_t *content_last_paragraph(const content_root_t *root)
{
	return (paragraph_t *)content_last_of_type(root, content_paragraph);
}

static content_t *
content_next_of_type(const content_t *node, content_type_t type)
{
	content_t *content;
	assert(node && node->type != content_root);

	for (content = node->next; content->type != content_root; content = content->next)
	{
		if (content->type == type)
			return content;
	}
	return NULL;
}

static content_t *
content_prev_of_type(const content_t *node, content_type_t type)
{
	content_t *content;
	assert(node && node->type != content_root);

	for (content = node->prev; content->type != content_root; content = content->prev)
	{
		if (content->type == type)
			return content;
	}
	return NULL;
}

span_t *content_next_span(const content_t *root)
{
	return (span_t *)content_next_of_type(root, content_span);
}

span_t *content_prev_span(const content_t *root)
{
	return (span_t *)content_prev_of_type(root, content_span);
}

line_t *content_next_line(const content_t *root)
{
	return (line_t *)content_next_of_type(root, content_line);
}

line_t *content_prev_line(const content_t *root)
{
	return (line_t *)content_prev_of_type(root, content_line);
}

paragraph_t *content_next_paragraph(const content_t *root)
{
	return (paragraph_t *)content_next_of_type(root, content_paragraph);
}

paragraph_t *content_prev_paragraph(const content_t *root)
{
	return (paragraph_t *)content_prev_of_type(root, content_paragraph);
}

void
content_concat(content_root_t *dst, content_root_t *src)
{
	content_t *walk, *walk_next;

	if (src == NULL)
		return;

	for (walk = src->base.next; walk != &src->base; walk = walk_next)
	{
		walk_next = walk->next;
		content_append(dst, walk);
	}
}

void extract_line_free(extract_alloc_t* alloc, line_t **pline)
{
	line_t *line = *pline;

	content_unlink(&line->base);
	content_clear(alloc, &line->content);
	extract_free(alloc, pline);
}

void extract_image_clear(extract_alloc_t *alloc, image_t *image)
{
	extract_free(alloc, &image->type);
	extract_free(alloc, &image->name);
	extract_free(alloc, &image->id);
	if (image->data_free)
	{
		image->data_free(image->data_free_handle, image->data);
		image->data_free = NULL;
		image->data_free_handle = NULL;
		image->data = NULL;
	}
}

void extract_image_free(extract_alloc_t *alloc, image_t **pimage)
{
	if (*pimage == NULL)
		return;
	extract_image_clear(alloc, *pimage);
	extract_free(alloc, pimage);
}

void extract_cell_free(extract_alloc_t *alloc, cell_t **pcell)
{
	cell_t *cell = *pcell;

	if (cell == NULL)
		return;

	content_clear(alloc, &cell->content);

	extract_free(alloc, pcell);
}

int
extract_split_alloc(extract_alloc_t *alloc, split_type_t type, int count, split_t **psplit)
{
	split_t *split;

	if (extract_malloc(alloc, psplit, sizeof(*split) + (count-1) * sizeof(split_t *)))
		return -1;

	split = *psplit;
	split->type = type;
	split->weight = 0;
	split->count = count;
	memset(&split->split[0], 0, sizeof(split_t *) * count);

	return 0;
}

void extract_split_free(extract_alloc_t *alloc, split_t **psplit)
{
	int i;
	split_t *split = *psplit;

	if (split == NULL)
		return;

	for (i = 0; i < split->count; i++)
		extract_split_free(alloc, &split->split[i]);
	extract_free(alloc, psplit);
}

static void space_prefix(int depth)
{
	while (depth-- > 0)
		putc(' ', stdout);
}

static void
content_dump_aux(const content_root_t *content, int depth);

static void dump_span(const span_t *span, int depth)
{
	int i;
	for (i = 0; i < span->chars_num; i++)
	{
		char_t *c = &span->chars[i];
		space_prefix(depth);
		printf("<char ucs=\"");
		if (c->ucs >= 32 && c->ucs <= 127)
			putc((char)c->ucs, stdout);
		else
			printf("<%04x>", c->ucs);
		printf("\" x=%f y=%f adv=%f />\n", c->x, c->y, c->adv);
	}
}

static void
dump_structure_path(structure_t *structure)
{
	if (structure->parent)
	{
		dump_structure_path(structure->parent);
		printf("/");
	}
	printf("%s(%d)", extract_struct_string(structure->type), structure->uid);
}

static void
content_dump_span_aux(const span_t *span, int depth)
{
	space_prefix(depth);
	printf("<span ctm=[%f %f %f %f]\n",
		span->ctm.a, span->ctm.b, span->ctm.c, span->ctm.d);
	if (span->structure)
	{
		space_prefix(depth);
		printf("      structure=\"");
		dump_structure_path(span->structure);
		printf("\"\n");
	}
	space_prefix(depth);
	printf("      font-name=\"%s\" font_bbox=[%f %f %f %f]>\n",
		span->font_name,
		span->font_bbox.min.x, span->font_bbox.min.y,
		span->font_bbox.max.x, span->font_bbox.max.y);
	dump_span(span, depth+1);
	space_prefix(depth);
	printf("</span>\n");
}

void
content_dump_span(const span_t *span)
{
	content_dump_span_aux(span, 0);
}

static void
content_dump_line_aux(const line_t *line, int depth)
{
	span_t *span0 = content_first_span(&line->content);
	span_t *span1 = content_last_span(&line->content);
	char_t *char0 = (span0 && span0->chars_num > 0) ? &span0->chars[0] : NULL;
	char_t *char1 = (span1 && span1->chars_num > 0) ? &span1->chars[span1->chars_num-1] : NULL;
	space_prefix(depth);
	printf("<line");
	if (char0 && char1)
	{
		printf(" x0=%g y0=%g x1=%g y1=%g\n", char0->x, char0->y, char1->x, char1->y);
	}
	content_dump_aux(&line->content, depth+1);
	space_prefix(depth);
	printf("</line>\n");
}

void
content_dump_line(const line_t *line)
{
	content_dump_line_aux(line, 0);
}

static void
content_dump_aux(const content_root_t *content, int depth)
{
	const content_t *walk;

	assert(content->base.type == content_root);
	for (walk = content->base.next; walk != &content->base; walk = walk->next)
	{
		assert(walk->next->prev == walk && walk->prev->next == walk);
		switch (walk->type)
		{
		case content_span:
			content_dump_span_aux((const span_t *)walk, depth);
			break;
		case content_line:
			content_dump_line_aux((const line_t *)walk, depth);
			break;
		case content_paragraph:
			space_prefix(depth);
			printf("<paragraph>\n");
			content_dump_aux(&((const paragraph_t *)walk)->content, depth+1);
			space_prefix(depth);
			printf("</paragraph>\n");
			break;
		case content_block:
			space_prefix(depth);
			printf("<block>\n");
			content_dump_aux(&((const block_t *)walk)->content, depth+1);
			space_prefix(depth);
			printf("</block>\n");
			break;
		case content_table:
		{
			const table_t *table = (const table_t *)walk;
			int i, j, k;
			space_prefix(depth);
			printf("<table w=%d h=%d>\n", table->cells_num_x, table->cells_num_y);
			k = 0;
			for (j = 0; j < table->cells_num_y; j++)
			{
				for (i = 0; i < table->cells_num_x; i++)
				{
					space_prefix(depth+1);
					printf("<cell>\n");
					content_dump_aux(&table->cells[k]->content, depth+2);
					space_prefix(depth+1);
					printf("</cell>\n");
					k++;
				}
			}
			space_prefix(depth);
			printf("</table>\n");
			break;
		}
		case content_image:
			space_prefix(depth);
			printf("<image/>\n");
			break;
		default:
			assert("Unexpected type found while dumping content list." == NULL);
			break;
		}
	}
}

void content_dump(const content_root_t *content)
{
	content_dump_aux(content, 0);
}

static void
content_dump_brief_aux(const content_root_t *content, int depth);

static void
content_dump_brief_span_aux(const span_t *span)
{
	int i;

	printf("\"");
	for (i = 0; i < span->chars_num; i++)
	{
		char_t *c = &span->chars[i];
		if (c->ucs >= 32 && c->ucs <= 127)
			putc((char)c->ucs, stdout);
		else
			printf("<%04x>", c->ucs);
	}
	printf("\"");
}

static void
content_dump_brief_line_aux(const line_t *line, int depth)
{
	printf("<line text=");
	content_dump_brief_aux(&line->content, depth+1);
	printf(">\n");
}

static void
content_dump_brief_aux(const content_root_t *content, int depth)
{
	const content_t *walk;

	assert(content->base.type == content_root);
	for (walk = content->base.next; walk != &content->base; walk = walk->next)
	{
		assert(walk->next->prev == walk && walk->prev->next == walk);
		switch (walk->type)
		{
		case content_span:
			content_dump_brief_span_aux((const span_t *)walk);
			break;
		case content_line:
			content_dump_brief_line_aux((const line_t *)walk, depth);
			break;
		case content_paragraph:
			content_dump_brief_aux(&((const paragraph_t *)walk)->content, depth+1);
			break;
		case content_block:
			content_dump_brief_aux(&((const block_t *)walk)->content, depth+1);
			break;
		case content_table:
		{
			const table_t *table = (const table_t *)walk;
			int i, j, k;
			k = 0;
			for (j = 0; j < table->cells_num_y; j++)
			{
				for (i = 0; i < table->cells_num_x; i++)
				{
					content_dump_brief_aux(&table->cells[k]->content, depth+2);
					k++;
				}
			}
			break;
		}
		case content_image:
			break;
		default:
			assert("Unexpected type found while dumping content list." == NULL);
			break;
		}
	}
}

void content_dump_brief(const content_root_t *content)
{
	content_dump_brief_aux(content, 0);
}

static content_t *
cmp_and_merge(content_t *q1, int q1pos, int len1, int n, content_cmp_fn *cmp)
{
	int len2 = q1pos + len1*2; /* end of both lists assuming we don't overrun */
	int p;
	content_t *q2 = q1;

	/* Don't overrun the end, and then convert to length from end. */
	if (len2 > n)
		len2 = n;
	len2 -= q1pos + len1;

	if (len2 <= 0)
		len1 += len2;

	/* Find the start of q2. We know this fits. */
	for (p = 0; p < len1; p++)
		q2 = q2->next;

	if (len2 <= 0)
		return q2;

	/* So we have [q1..(q1+len1)) as the first list to merge, and
	 * [q2..q2+len2)) as the second list to merge. We know that
	 * q2 = q1+len1. So, if we can reduce len1 or len2 to 0, we have
	 * the lists sorted. */
	while (1)
	{
		if (cmp(q1, q2) > 0)
		{
			/* q2 is smaller. q2 should be before q1. Move it. */
			/* So:
			*    a<->q1<->c..d<->q2<->b  =>  a<->q2<->q1<->c..d<->b
			* (where c and d can either be the same, or can be q2 and q1!)
			*/
			content_t *a = q1->prev;
			content_t *b = q2->next;
			content_t *d = q2->prev;
			d->next = b;
			b->prev = d;
			a->next = q2;
			q2->prev = a;
			q2->next = q1;
			q1->prev = q2;
			/* Now advance q2 */
			q2 = b;
			len2--;
			if (len2 == 0)
				break;
		} else {
			/* Advance q1 */
			q1 = q1->next;
			len1--;
			if (len1 == 0)
				break;
		}
	}

	while (len2)
	{
		q2 = q2->next;
		len2--;
	}

	return q2;
}

/* Spiffy in-place merge sort. */
void content_sort(content_root_t *content, content_cmp_fn *cmp)
{
	int n = content_count(content);
	int size;

	for (size = 1; size < n; size <<= 1)
	{
		int q1_idx = 0;
		content_t *q1 = content->base.next;
		for (q1_idx = 0; q1_idx < n; q1_idx += size*2)
			q1 = cmp_and_merge(q1, q1_idx, size, n, cmp);
		assert(q1->type == content_root);
	}
	assert(content_count(content) == n);
}
