#ifndef ARTIFEX_EXTRACT_DOCUMENT_H
#define ARTIFEX_EXTRACT_DOCUMENT_H

#include "extract/extract.h"
#include "extract/alloc.h"

#include "compat_stdint.h"
#include <assert.h>

typedef struct span_t span_t;
typedef struct line_t line_t;
typedef struct paragraph_t paragraph_t;
typedef struct image_t image_t;
typedef struct table_t table_t;
typedef struct block_t block_t;
typedef struct structure_t structure_t;

static const double pi = 3.141592653589793;

/*
All content is stored as content_t nodes in a doubly linked-list.
The first node in the list is a 'content_root' node. The last
node in the list is the same node again.

Thus:
  Every node in a list (including the root) has next and prev != NULL.
  The root node in an empty list has next and prev pointing to itself.
  Any non-root node with prev and next == NULL is not in a list.

Content nodes record a 'type' for the node. Each node is 'derived' in
an OO style from the basic content_t.

The different content types form a heirarchy:

A spans is an array of "char_t"s (note, an array, NOT a content list).

Lines contain a content list, which should mostly consist of spans.

Paragraphs contain a content list, which should mostly consist of lines.

Image nodes contains details of a bitmap image.

Table nodes contain an array of cells, each of which contains a content
list that can contain any other type.

Blocks contain a content list consisting of paragraphs, tables and images.
Conceptually these represent a block of content on a page.
*/
typedef enum {
	content_root,
	content_span,
	content_line,
	content_paragraph,
	content_image,
	content_table,
	content_block
} content_type_t;

typedef struct content_t {
	/* The type field tells us what derived type we actually are. */
	content_type_t type;

	/* This holds us in the linked list of sibling content nodes. */
	struct content_t *prev;
	struct content_t *next;
} content_t;

/* Initialise a content_t (just the base struct). */
void content_init(content_t *content, content_type_t type);

/* Unlink a (non-root) content_t from any list. */
void content_unlink(content_t *content);

/* Unlink a span_t from any list. */
void content_unlink_span(span_t *span);

typedef struct {
	content_t  base;
	content_t *parent;
} content_root_t;

void content_init_root(content_root_t *root, content_t *parent);

/* Free all the content, from a (root) content_t. */
void content_clear(extract_alloc_t* alloc, content_root_t *root);

span_t *content_first_span(const content_root_t *root);
span_t *content_last_span(const content_root_t *root);
line_t *content_first_line(const content_root_t *root);
line_t *content_last_line(const content_root_t *root);
paragraph_t *content_first_paragraph(const content_root_t *root);
paragraph_t *content_last_paragraph(const content_root_t *root);

span_t *content_next_span(const content_t *node);
span_t *content_prev_span(const content_t *node);
line_t *content_next_line(const content_t *node);
line_t *content_prev_line(const content_t *node);
paragraph_t *content_next_paragraph(const content_t *node);
paragraph_t *content_prev_paragraph(const content_t *node);

int content_count(content_root_t *root);
int content_count_images(content_root_t *root);
int content_count_spans(content_root_t *root);
int content_count_lines(content_root_t *root);
int content_count_paragraphs(content_root_t *root);
int content_count_tables(content_root_t *root);

int content_new_root(extract_alloc_t *alloc, content_root_t **proot);
int content_new_span(extract_alloc_t *alloc, span_t **pspan, structure_t *structure);
int content_new_line(extract_alloc_t *alloc, line_t **pline);
int content_new_paragraph(extract_alloc_t *alloc, paragraph_t **pparagraph);
int content_new_table(extract_alloc_t *alloc, table_t **ptable);
int content_new_block(extract_alloc_t *alloc, block_t **pblock);

int content_append_new_span(extract_alloc_t* alloc, content_root_t *root, span_t **pspan, structure_t *structure);
int content_append_new_line(extract_alloc_t* alloc, content_root_t *root, line_t **pline);
int content_append_new_paragraph(extract_alloc_t* alloc, content_root_t *root, paragraph_t **pparagraph);
int content_append_new_image(extract_alloc_t* alloc, content_root_t *root, image_t **pimage);
int content_append_new_table(extract_alloc_t* alloc, content_root_t *root, table_t **ptable);
int content_append_new_block(extract_alloc_t* alloc, content_root_t *root, block_t **pblock);

void content_replace(content_t *current, content_t *replacement);
int content_replace_new_line(extract_alloc_t* alloc, content_t *current, line_t **pline);
int content_replace_new_paragraph(extract_alloc_t* alloc, content_t *current, paragraph_t **pparagraph);
int content_replace_new_block(extract_alloc_t* alloc, content_t *current, block_t **pblock);


void content_append(content_root_t *root, content_t *content);
void content_append_span(content_root_t *root, span_t *span);
void content_append_line(content_root_t *root, line_t *line);
void content_append_paragraph(content_root_t *root, paragraph_t *paragraph);
void content_append_table(content_root_t *root, table_t *table);
void content_append_block(content_root_t *root, block_t *block);

void content_concat(content_root_t *dst, content_root_t *src);

void content_dump(const content_root_t *content);
void content_dump_line(const line_t *line);
void content_dump_span(const span_t *span);
void content_dump_brief(const content_root_t *content);


typedef int (content_cmp_fn)(const content_t *, const content_t *);

void content_sort(content_root_t *content, content_cmp_fn *cmp);

/* To iterate over the line elements of a content list:

content_line_iterator it;
line_t *line;

for(line = content_line_iterator_line_init(&it, content); line != NULL; line = content_line_iterator_next(&it))
{
}

*/

typedef struct {
	content_root_t *root;
	content_t      *next;
} content_paragraph_iterator;

static inline paragraph_t *content_paragraph_iterator_next(content_paragraph_iterator *it)
{
	content_t *next;

	do {
		next = it->next;
		if (next == &it->root->base)
			return NULL;
		assert(next->type != content_root);
		it->next = next->next;
	} while (next->type != content_paragraph);

	return (paragraph_t *)next;
}

static inline paragraph_t *content_paragraph_iterator_init(content_paragraph_iterator *it, content_root_t *root)
{
	it->root = root;
	it->next = root->base.next;

	return content_paragraph_iterator_next(it);
}

typedef struct {
	content_root_t *root;
	content_t      *next;
} content_line_iterator;

static inline line_t *content_line_iterator_next(content_line_iterator *it)
{
	content_t *next;

	do {
		next = it->next;
		if (next == &it->root->base)
			return NULL;
		assert(next->type != content_root);
			it->next = next->next;
	} while (next->type != content_line);

	return (line_t *)next;
}

static inline line_t *content_line_iterator_init(content_line_iterator *it, content_root_t *root)
{
	it->root = root;
	it->next = root->base.next;

	return content_line_iterator_next(it);
}

typedef struct {
	content_root_t *root;
	content_t      *next;
} content_span_iterator;

static inline span_t *content_span_iterator_next(content_span_iterator *it)
{
	content_t *next;

	do {
		next = it->next;
		if (next == &it->root->base)
			return NULL;
		assert(next->type != content_root);
		it->next = next->next;
	} while (next->type != content_span);

	return (span_t *)next;
}

static inline span_t *content_span_iterator_init(content_span_iterator *it, content_root_t *root)
{
	it->root = root;
	it->next = root->base.next;

	return content_span_iterator_next(it);
}

typedef struct {
	content_root_t *root;
	content_t      *next;
} content_image_iterator;

static inline image_t *content_image_iterator_next(content_image_iterator *it)
{
	content_t *next;

	do {
		next = it->next;
		if (next == &it->root->base)
			return NULL;
		assert(next->type != content_root);
		it->next = next->next;
	} while (next->type != content_image);

	return (image_t *)next;
}

static inline image_t *content_image_iterator_init(content_image_iterator *it, content_root_t *root)
{
	it->root = root;
	it->next = root->base.next;

	return content_image_iterator_next(it);
}

typedef struct {
	content_root_t *root;
	content_t      *next;
} content_table_iterator;

static inline table_t *content_table_iterator_next(content_table_iterator *it)
{
	content_t *next;

	do {
		next = it->next;
		if (next == &it->root->base)
			return NULL;
		assert(next->type != content_root);
		it->next = next->next;
	} while (next->type != content_table);

    return (table_t *)next;
}

static inline table_t *content_table_iterator_init(content_table_iterator *it, content_root_t *root)
{
	it->root = root;
	it->next = root->base.next;

	return content_table_iterator_next(it);
}

typedef struct {
	content_root_t *root;
	content_t      *next;
} content_iterator;

static inline content_t *content_iterator_next(content_iterator *it)
{
	content_t *next = it->next;

	if (next == &it->root->base)
		return NULL;
	assert(next->type != content_root);
	it->next = next->next;

	return next;
}

static inline content_t *content_iterator_init(content_iterator *it, content_root_t *root)
{
	it->root = root;
	it->next = root->base.next;

	return content_iterator_next(it);
}

typedef struct
{
	double x;
	double y;
} point_t;

const char *extract_point_string(const point_t *point);

typedef struct
{
	point_t min;
	point_t max;
} rect_t;

extern const rect_t extract_rect_infinite;
extern const rect_t extract_rect_empty;

rect_t extract_rect_intersect(rect_t a, rect_t b);

rect_t extract_rect_union(rect_t a, rect_t b);

rect_t extract_rect_union_point(rect_t a, point_t b);

int extract_rect_contains_rect(rect_t a, rect_t b);

int extract_rect_valid(rect_t a);

const char *extract_rect_string(const rect_t *rect);

typedef struct
{
	double  a;
	double  b;
	double  c;
	double  d;
	double  e;
	double  f;
} matrix_t;

typedef struct
{
	double  a;
	double  b;
	double  c;
	double  d;
} matrix4_t;

const char *extract_matrix_string(const matrix_t *matrix);
const char *extract_matrix4_string(const matrix4_t *matrix);

/* Returns a*d - b*c. */
double      extract_matrix_expansion(matrix4_t m);

/* Returns the inverse of a matrix (or identity for degenerate). */
matrix4_t   extract_matrix4_invert(const matrix4_t *ctm);

point_t     extract_matrix4_transform_point(matrix4_t m, point_t p);
point_t     extract_matrix4_transform_xy(matrix4_t m, double x, double y);
matrix_t    extract_multiply_matrix_matrix(matrix_t m1, matrix_t m2);
matrix4_t   extract_multiply_matrix4_matrix4(matrix4_t m1, matrix4_t m2);

/* Returns zero if first four members of *lhs and *rhs are equal, otherwise
+/-1. */
int extract_matrix4_cmp(const matrix4_t *lhs, const matrix4_t *rhs);

/* A single char in a span. */
typedef struct
{
	/* (x,y) after transformation by ctm. */
	double      x;
	double      y;

	unsigned    ucs;
	double      adv; /* Advance, before transform by ctm */

	rect_t      bbox;
} char_t;

/* List of chars that have same font and are usually adjacent. */
struct span_t
{
	content_t    base;
	matrix4_t    ctm;
	char        *font_name;
	rect_t       font_bbox;
	structure_t *structure;

	struct {
		unsigned font_bold      : 1;
		unsigned font_italic    : 1;
		unsigned wmode          : 1;
	} flags;

	char_t     *chars;
	int         chars_num;
};

void extract_span_init(span_t *span, structure_t *structure);

/* Frees a span_t, returning with *pspan set to NULL. */
void extract_span_free(extract_alloc_t *alloc, span_t **pspan);

/* Returns last character in span. */
char_t *extract_span_char_last(span_t *span);

/* Appends new char_t to an span_t with .ucs=c and all other
fields zeroed. Returns pointer to new char_t record, or NULL if allocation
failed. */
char_t *extract_span_append_c(extract_alloc_t *alloc, span_t *span, int c);

/* Returns static string containing info about span_t. */
const char *extract_span_string(extract_alloc_t *alloc, span_t *span);

/* List of spans that are aligned on same line. */
struct line_t
{
	content_t base;
	double ascender;
	double descender;
	content_root_t content;
};

void extract_line_init(line_t *line);

void extract_line_free(extract_alloc_t* alloc, line_t **pline);

/* Returns first span in a line. */
span_t *extract_line_span_first(line_t *line);

/* Returns last span in a line. */
span_t *extract_line_span_last(line_t *line);

/* List of lines that are aligned and adjacent to each other so as to form a
paragraph. */
struct paragraph_t
{
	content_t      base;
	int            line_flags;
	content_root_t content;
};

typedef enum
{
	/* If the paragraph is ever not aligned to the left hand edge, we set this flag. */
	paragraph_not_aligned_left = 1,

	/* If the paragraph is ever not aligned to the right hand edge, we set this flag. */
	paragraph_not_aligned_right = 2,

	/* If the paragraph ever has a line that doesn't look centred, we set this flag. */
	paragraph_not_centred = 4,

	/* If the paragraph ever has a line that doesn't look fully justified, we set this flag. */
	paragraph_not_fully_justified = 8,

	/* If the paragraph ever breaks at a place where it looks like first word from the
	* next line could have fitted, then set this flag.*/
	paragraph_breaks_strangely = 16
} paragraph_flags;

void extract_paragraph_init(paragraph_t *paragraph);

void extract_paragraph_free(extract_alloc_t *alloc, paragraph_t **pparagraph);

/* List of content that we believe should be treated as a whole. */
struct block_t
{
	content_t      base;
	content_root_t content;
};

void extract_block_init(block_t *block);

void extract_block_free(extract_alloc_t *alloc, block_t **pblock);



/* Information about an image. <type> is as passed to extract_add_image();
<name> and <id> are created to be unique identifiers for use in generated docx
file. */
struct image_t
{
	content_t                base;
	char                    *type;   /* jpg, png etc. */
	char                    *name;   /* Name of image file within docx. */
	char                    *id;     /* ID of image within docx. */
	double                   x;
	double                   y;
	double                   w;
	double                   h;
	void                    *data;
	size_t                   data_size;

	extract_image_data_free *data_free;
	void                    *data_free_handle;
};

void extract_image_init(image_t *image);

void extract_image_clear(extract_alloc_t *alloc, image_t *image);

void extract_image_free(extract_alloc_t *alloc, image_t **pimage);

/* A line that is part of a table. */
typedef struct
{
	float   color;
	rect_t  rect;
} tableline_t;

typedef struct
{
	tableline_t *tablelines;
	int          tablelines_num;
} tablelines_t;


/* A cell within a table. */
typedef struct
{
	rect_t          rect;

	/* If left/above is true, this cell is not obscured by cell to its
	 * left/above. */
	uint8_t         left;
	uint8_t         above;

	/* extend_right and extend_down are 1 for normal cells, 2 for cells which
	 * extend right/down to cover an additional column/row, 3 to cover two
	 * additional columns/rows etc. */
	int             extend_right;
	int             extend_down;

	/* Contents of this cell. */
	content_root_t  content;
} cell_t;

void extract_cell_init(cell_t *cell);
void extract_cell_free(extract_alloc_t *alloc, cell_t **pcell);
void extract_table_init(table_t *table);

struct table_t
{
	content_t   base;
	point_t     pos;    /* top-left. */

	/* Array of cells_num_x*cells_num_y cells; cell (x, y) is:
	 * cells_num_x * y + x.
	 */
	cell_t    **cells;
	int         cells_num_x;
	int         cells_num_y;
};

void extract_table_free(extract_alloc_t *alloc, table_t **ptable);

typedef enum
{
	SPLIT_NONE = 0,
	SPLIT_HORIZONTAL,
	SPLIT_VERTICAL
} split_type_t;


typedef struct split_t
{
	split_type_t    type;
	double          weight;
	int             count;
	struct split_t *split[1];
} split_t;

struct structure_t
{
	structure_t       *parent;
	structure_t       *sibling_next;
	structure_t       *sibling_prev;
	structure_t       *kids_first;
	structure_t      **kids_tail;
	int                uid;
	extract_struct_t   type;
	int                score;
};

/* A subpage. Contains different representations of the list of spans. */
typedef struct
{
	rect_t          mediabox;

	int             images_num;

	/* All the content on the page. */
	content_root_t  content;

	tablelines_t    tablelines_horizontal;
	tablelines_t    tablelines_vertical;

	content_root_t  tables;
} subpage_t;


/* A page. Contains a list of subpages. NB not
called page_t because this clashes with a system type on hpux. */
typedef struct
{
	rect_t      mediabox;

	subpage_t **subpages;
	int         subpages_num;

	split_t    *split;
} extract_page_t;


/* A list of pages. */
typedef struct
{
	extract_page_t **pages;
	int              pages_num;

	/* All the structure for the document. */
	structure_t    *structure;

	/* During construction, current points to the current point
	* within the structure tree where things should be added. */
	structure_t    *current;
} document_t;


typedef struct
{
	image_t **images;
	int       images_num;
	char    **imagetypes;
	int       imagetypes_num;
} images_t;


/* This does all the work of finding paragraphs and tables. */
int extract_document_join(extract_alloc_t *alloc, document_t *document, int layout_analysis, double master_space_guess);

double extract_font_size(matrix4_t *ctm);

/* Things below here are used when generating output. */

/* Basic information about current font. */
typedef struct
{
	char   *name;
	double  size;
	int     bold;
	int     italic;
} font_t;

/* Used to keep track of font information when writing paragraphs of odt
content, e.g. so we know whether a font has changed so need to start a new odt
span. */
typedef struct
{
	font_t     font;
	matrix4_t *ctm_prev;
} content_state_t;

/* Analyse page content for layouts. */
int extract_page_analyse(extract_alloc_t *alloc, extract_page_t *page);

/* subpage_t constructor. */
int extract_subpage_alloc(extract_alloc_t *extract, rect_t mediabox, extract_page_t *page, subpage_t **psubpage);

/* subpage_t destructor. */
void extract_subpage_free(extract_alloc_t *alloc, subpage_t **psubpage);

/* Allocate a split_t. */
int extract_split_alloc(extract_alloc_t *alloc, split_type_t type, int count, split_t **psplit);

void extract_split_free(extract_alloc_t *alloc, split_t **psplit);

typedef struct {
	content_root_t *root;
	content_t      *next;
} content_tree_iterator;

static inline content_t *content_tree_iterator_next(content_tree_iterator *it)
{
	content_t *next = it->next;

	while (next->type == content_root)
	{
		content_t *parent = ((content_root_t *)next)->parent;
		if (parent == NULL)
			return NULL;
		next = parent->next;
	}
	assert(next->type != content_root);

	switch (next->type)
	{
	default:
	case content_root:
		assert("Never happens!" == NULL);
		break;
	case content_span:
		it->next = next->next;
		break;
	case content_line:
		it->next = ((line_t *)next)->content.base.next;
		break;
	case content_paragraph:
		it->next = ((paragraph_t *)next)->content.base.next;
		break;
	}

	return next;
}

static inline content_t *content_tree_iterator_init(content_tree_iterator *it, content_root_t *root)
{
	it->root = root;
	it->next = root->base.next;

	return content_tree_iterator_next(it);
}

/* Some helper functions */

/* Return a span_t * pointer to the first element in a content list. */
static inline span_t *content_head_as_span(content_root_t *root)
{
	assert(root != NULL && root->base.type == content_root && (root->base.next == NULL || root->base.next->type == content_span));
	return (span_t *)root->base.next;
}

/* Return a point for the post-advance position of a char in a given span. */
point_t extract_predicted_end_of_char(char_t *char_, const span_t *span);

/* Return a point for the post-advance position of the final char in a given span. */
point_t extract_end_of_span(const span_t *span);

/* Return the bounds for a block before it was rotated around its origin. */
rect_t extract_block_pre_rotation_bounds(block_t *block, double rotate);

double extract_baseline_angle(const matrix4_t *ctm);

#endif
