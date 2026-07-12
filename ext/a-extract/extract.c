#include "extract/extract.h"
#include "extract/buffer.h"
#include "memento.h"

#include "extract/alloc.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct extract_alloc_t
{
	extract_realloc_fn_t     *realloc_fn;
	extract_caller_context_t *realloc_state;
	size_t                    exp_min_alloc_size;
	extract_alloc_stats_t     stats;
};

int
extract_alloc_create(	extract_realloc_fn_t *realloc_fn,
			void     *realloc_state,
			extract_alloc_t  **palloc)
{
	assert(realloc_fn);
	assert(palloc);
	*palloc = realloc_fn(realloc_state, NULL , sizeof(**palloc));
	if (!*palloc) {
		errno = ENOMEM;
		return -1;
	}
	memset(*palloc, 0, sizeof(**palloc));
	(*palloc)->realloc_fn = realloc_fn;
	(*palloc)->realloc_state = realloc_state;
	(*palloc)->exp_min_alloc_size = 0;
	return 0;
}

void extract_alloc_destroy(extract_alloc_t **palloc)
{
	if (!*palloc) return;
	(*palloc)->realloc_fn((*palloc)->realloc_state, *palloc, 0 );
	*palloc = NULL;
}

extract_alloc_stats_t *extract_alloc_stats(extract_alloc_t *alloc)
{
	return &alloc->stats;
}

static size_t round_up(extract_alloc_t *alloc, size_t n)
{
	size_t ret;

	if (alloc == NULL || alloc->exp_min_alloc_size || n == 0)
		return n;

	ret = alloc->exp_min_alloc_size;
	while (ret < n) {
		size_t ret_old = ret;
		ret *= 2;
		if (ret <= ret_old)
			ret = n;
	}

	return ret;
}

int (extract_malloc)(extract_alloc_t *alloc, void **pptr, size_t size)
{
	void *p;

	size = round_up(alloc, size);
	p = (alloc) ? alloc->realloc_fn(alloc->realloc_state, NULL, size) : malloc(size);
	*pptr = p;
	if (!p && size)
	{
		if (alloc) errno = ENOMEM;
		return -1;
	}
	if (alloc)  alloc->stats.num_malloc += 1;
	return 0;
}

int (extract_realloc)(extract_alloc_t *alloc, void **pptr, size_t newsize)
{
	void *p = (alloc) ? alloc->realloc_fn(alloc->realloc_state, *pptr, newsize) : realloc(*pptr, newsize);
	if (!p && newsize)
	{
		if (alloc) errno = ENOMEM;
		return -1;
	}
	*pptr = p;
	if (alloc) alloc->stats.num_realloc += 1;
	return 0;
}

int (extract_realloc2)(extract_alloc_t *alloc, void **pptr, size_t oldsize, size_t newsize)
{

	oldsize = (*pptr) ? round_up(alloc, oldsize) : 0;
	newsize = round_up(alloc, newsize);
	if (newsize == oldsize) return 0;
	return (extract_realloc)(alloc, pptr, newsize);
}

void (extract_free)(extract_alloc_t *alloc, void **pptr)
{
	if (alloc)
		(void)alloc->realloc_fn(alloc->realloc_state, *pptr, 0);
	else
		free(*pptr);
	*pptr = NULL;
	if (alloc) alloc->stats.num_free += 1;
}

void extract_alloc_exp_min(extract_alloc_t *alloc, size_t size)
{
	alloc->exp_min_alloc_size = size;
}

#ifndef ARTIFEX_EXTRACT_AUTOSTRING_XML
#define ARTIFEX_EXTRACT_AUTOSTRING_XML

#include "extract/alloc.h"

typedef struct
{
	char   *chars;
	size_t  chars_num;
} extract_astring_t;

void extract_astring_init(extract_astring_t *string);

void extract_astring_free(extract_alloc_t *alloc, extract_astring_t *string);

int extract_astring_catl(extract_alloc_t *alloc, extract_astring_t *string, const char *s, size_t s_len);

int extract_astring_catc(extract_alloc_t *alloc, extract_astring_t *string, char c);

int extract_astring_cat(extract_alloc_t *alloc, extract_astring_t *string, const char *s);
int extract_astring_catf(extract_alloc_t *alloc, extract_astring_t *string, const char *format, ...);

int extract_astring_truncate(extract_astring_t *content, int len);

int extract_astring_char_truncate_if(extract_astring_t *content, char c);

int extract_astring_cat_xmlc(extract_alloc_t *alloc, extract_astring_t *string, int c);

int extract_astring_catc_unicode(extract_alloc_t   *alloc,
				extract_astring_t *string,
				int c,
				int xml,
				int ascii_ligatures,
				int ascii_dash,
				int ascii_apostrophe);

int extract_astring_catc_unicode_xml(extract_alloc_t *alloc, extract_astring_t *string, int c);

#endif

#ifndef EXTRACT_MEM_H
#define EXTRACT_MEM_H

#include "extract/alloc.h"

#include <stdarg.h>
#include <string.h>

void extract_bzero(void *b, size_t len);

int extract_vasprintf(extract_alloc_t* alloc, char** out, const char* format, va_list va)
        #ifdef __GNUC__
        __attribute__ ((format (printf, 3, 0)))
        #endif
        ;

int extract_asprintf(extract_alloc_t* alloc, char** out, const char* format, ...)
        #ifdef __GNUC__
        __attribute__ ((format (printf, 3, 4)))
        #endif
        ;

int extract_strdup(extract_alloc_t* alloc, const char* s, char** o_out);

#endif

#include "memento.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void extract_astring_init(extract_astring_t *string)
{
	string->chars = NULL;
	string->chars_num = 0;
}

void extract_astring_free(extract_alloc_t *alloc, extract_astring_t *string)
{
	extract_free(alloc, &string->chars);
	extract_astring_init(string);
}

int extract_astring_catl(extract_alloc_t *alloc, extract_astring_t *string, const char *s, size_t s_len)
{
	if (extract_realloc2(alloc, &string->chars, string->chars_num+1, string->chars_num + s_len + 1))
		return -1;

	memcpy(string->chars + string->chars_num, s, s_len);
	string->chars[string->chars_num + s_len] = 0;
	string->chars_num += s_len;
	return 0;
}

int extract_astring_catc(extract_alloc_t *alloc, extract_astring_t *string, char c)
{
	return extract_astring_catl(alloc, string, &c, 1);
}

int extract_astring_cat(extract_alloc_t *alloc, extract_astring_t *string, const char *s)
{
	return extract_astring_catl(alloc, string, s, strlen(s));
}

int extract_astring_catf(extract_alloc_t *alloc, extract_astring_t *string, const char *format, ...)
{
	char    *buffer = NULL;
	int      e;
	va_list  va;

	va_start(va, format);
	e = extract_vasprintf(alloc, &buffer, format, va);
	va_end(va);
	if (e < 0) return e;
	e = extract_astring_cat(alloc, string, buffer);
	extract_free(alloc, &buffer);

	return e;
}

int extract_astring_truncate(extract_astring_t *content, int len)
{
	assert((size_t) len <= content->chars_num);

	content->chars_num -= len;
	content->chars[content->chars_num] = 0;

	return 0;
}

int extract_astring_char_truncate_if(extract_astring_t *content, char c)
{
	if (content->chars_num && content->chars[content->chars_num-1] == c)
		extract_astring_truncate(content, 1);

	return 0;
}

int extract_astring_catc_unicode(extract_alloc_t  *alloc,
				extract_astring_t *string,
				int                c,
				int                xml,
				int                ascii_ligatures,
				int                ascii_dash,
				int                ascii_apostrophe)
{
	int ret = -1;

	if (0) {}

	else if (xml && c == '<')  extract_astring_cat(alloc, string, "&lt;");
	else if (xml && c == '>')  extract_astring_cat(alloc, string, "&gt;");
	else if (xml && c == '&')  extract_astring_cat(alloc, string, "&amp;");
	else if (xml && c == '"')  extract_astring_cat(alloc, string, "&quot;");
	else if (xml && c == '\'') extract_astring_cat(alloc, string, "&apos;");

	else if (ascii_ligatures && c == 0xFB00)
	{
		if (extract_astring_cat(alloc, string, "ff")) goto end;
	}
	else if (ascii_ligatures && c == 0xFB01)
	{
		if (extract_astring_cat(alloc, string, "fi")) goto end;
	}
	else if (ascii_ligatures && c == 0xFB02)
	{
		if (extract_astring_cat(alloc, string, "fl")) goto end;
	}
	else if (ascii_ligatures && c == 0xFB03)
	{
		if (extract_astring_cat(alloc, string, "ffi")) goto end;
	}
	else if (ascii_ligatures && c == 0xFB04)
	{
		if (extract_astring_cat(alloc, string, "ffl")) goto end;
	}

	else if (ascii_dash && c == 0x2212)
	{
		if (extract_astring_catc(alloc, string, '-')) goto end;
	}
	else if (ascii_apostrophe && c == 0x2019)
	{
		if (extract_astring_catc(alloc, string, '\'')) goto end;
	}

	else if (c >= 32 && c <= 127)
	{
		if (extract_astring_catc(alloc, string, (char) c)) goto end;
	}

	else if (xml)
	{
		char	buffer[32];
		if (c < 32 && (c != 0x9 && c != 0xa && c != 0xd))
		{

			c = 0xfffd;
		}
		snprintf(buffer, sizeof(buffer), "&#x%x;", c);
		if (extract_astring_cat(alloc, string, buffer)) goto end;
	}
	else
	{

		if (c < 0x80)
		{
			if (extract_astring_catc(alloc, string, (char) c)) return -1;
		}
		else if (c < 0x0800)
		{
			char cc[2] = {	(char) (((c >> 6) & 0x1f) | 0xc0),
					(char) (((c >> 0) & 0x3f) | 0x80) };
			if (extract_astring_catl(alloc, string, cc, sizeof(cc))) return -1;
		}
		else if (c < 0x10000)
		{
			char cc[3] = {	(char) (((c >> 12) & 0x0f) | 0xe0),
					(char) (((c >>  6) & 0x3f) | 0x80),
					(char) (((c >>  0) & 0x3f) | 0x80) };
			if (extract_astring_catl(alloc, string, cc, sizeof(cc))) return -1;
		}
		else if (c < 0x110000)
		{
			char cc[4] = {	(char) (((c >> 18) & 0x07) | 0xf0),
					(char) (((c >> 12) & 0x3f) | 0x80),
					(char) (((c >>  6) & 0x3f) | 0x80),
					(char) (((c >>  0) & 0x3f) | 0x80) };
			if (extract_astring_catl(alloc, string, cc, sizeof(cc))) return -1;
		}
		else
		{

			char cc[4] = { (char) 0xef, (char) 0xbf, (char) 0xbd, 0};
			if (extract_astring_catl(alloc, string, cc, sizeof(cc))) return -1;
		}
	}

	ret = 0;

end:
	return ret;
}

int extract_astring_catc_unicode_xml(extract_alloc_t *alloc, extract_astring_t *string, int c)
{

	return extract_astring_catc_unicode(
					alloc,
					string,
					c,
					1 ,
					1 ,
					0 ,
					0
					);
}

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#ifndef ARTIFEX_EXTRACT_DOCUMENT_H
#define ARTIFEX_EXTRACT_DOCUMENT_H

#include "extract/extract.h"
#include "extract/alloc.h"

#ifndef ARTIFEX_EXTRACT_COMPAT_STDINT_H
#define ARTIFEX_EXTRACT_COMPAT_STDINT_H

#if defined(_MSC_VER) && (_MSC_VER < 1700)
	typedef signed char         int8_t;
	typedef short int           int16_t;
	typedef int                 int32_t;
	typedef __int64             int64_t;
	typedef unsigned char       uint8_t;
	typedef unsigned short int  uint16_t;
	typedef unsigned int        uint32_t;
	typedef unsigned __int64    uint64_t;
	#ifndef INT64_MAX
		#define INT64_MAX 9223372036854775807i64
	#endif
	#ifndef SIZE_MAX
		#define SIZE_MAX ((size_t) -1)
	#endif
#else
	#include <stdint.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1800)
	#define strtoll( text, end, base) (long long) _strtoi64(text, end, base)
	#define strtoull( text, end, base) (unsigned long long) _strtoi64(text, end, base)
#endif

#endif

#include <assert.h>

typedef struct span_t span_t;
typedef struct line_t line_t;
typedef struct paragraph_t paragraph_t;
typedef struct image_t image_t;
typedef struct table_t table_t;
typedef struct block_t block_t;
typedef struct structure_t structure_t;

static const double pi = 3.141592653589793;

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

	content_type_t type;

	struct content_t *prev;
	struct content_t *next;
} content_t;

void content_init(content_t *content, content_type_t type);

void content_unlink(content_t *content);

void content_unlink_span(span_t *span);

typedef struct {
	content_t  base;
	content_t *parent;
} content_root_t;

void content_init_root(content_root_t *root, content_t *parent);

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

double      extract_matrix_expansion(matrix4_t m);

matrix4_t   extract_matrix4_invert(const matrix4_t *ctm);

point_t     extract_matrix4_transform_point(matrix4_t m, point_t p);
point_t     extract_matrix4_transform_xy(matrix4_t m, double x, double y);
matrix_t    extract_multiply_matrix_matrix(matrix_t m1, matrix_t m2);
matrix4_t   extract_multiply_matrix4_matrix4(matrix4_t m1, matrix4_t m2);

int extract_matrix4_cmp(const matrix4_t *lhs, const matrix4_t *rhs);

typedef struct
{

	double      x;
	double      y;

	unsigned    ucs;
	double      adv;

	rect_t      bbox;
} char_t;

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

void extract_span_free(extract_alloc_t *alloc, span_t **pspan);

char_t *extract_span_char_last(span_t *span);

char_t *extract_span_append_c(extract_alloc_t *alloc, span_t *span, int c);

const char *extract_span_string(extract_alloc_t *alloc, span_t *span);

struct line_t
{
	content_t base;
	double ascender;
	double descender;
	content_root_t content;
};

void extract_line_init(line_t *line);

void extract_line_free(extract_alloc_t* alloc, line_t **pline);

span_t *extract_line_span_first(line_t *line);

span_t *extract_line_span_last(line_t *line);

struct paragraph_t
{
	content_t      base;
	int            line_flags;
	content_root_t content;
};

typedef enum
{

	paragraph_not_aligned_left = 1,

	paragraph_not_aligned_right = 2,

	paragraph_not_centred = 4,

	paragraph_not_fully_justified = 8,

	paragraph_breaks_strangely = 16
} paragraph_flags;

void extract_paragraph_init(paragraph_t *paragraph);

void extract_paragraph_free(extract_alloc_t *alloc, paragraph_t **pparagraph);

struct block_t
{
	content_t      base;
	content_root_t content;
};

void extract_block_init(block_t *block);

void extract_block_free(extract_alloc_t *alloc, block_t **pblock);

struct image_t
{
	content_t                base;
	char                    *type;
	char                    *name;
	char                    *id;
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

typedef struct
{
	rect_t          rect;

	uint8_t         left;
	uint8_t         above;

	int             extend_right;
	int             extend_down;

	content_root_t  content;
} cell_t;

void extract_cell_init(cell_t *cell);
void extract_cell_free(extract_alloc_t *alloc, cell_t **pcell);
void extract_table_init(table_t *table);

struct table_t
{
	content_t   base;
	point_t     pos;

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

typedef struct
{
	rect_t          mediabox;

	int             images_num;

	content_root_t  content;

	tablelines_t    tablelines_horizontal;
	tablelines_t    tablelines_vertical;

	content_root_t  tables;
} subpage_t;

typedef struct
{
	rect_t      mediabox;

	subpage_t **subpages;
	int         subpages_num;

	split_t    *split;
} extract_page_t;

typedef struct
{
	extract_page_t **pages;
	int              pages_num;

	structure_t    *structure;

	structure_t    *current;
} document_t;

typedef struct
{
	image_t **images;
	int       images_num;
	char    **imagetypes;
	int       imagetypes_num;
} images_t;

int extract_document_join(extract_alloc_t *alloc, document_t *document, int layout_analysis, double master_space_guess);

double extract_font_size(matrix4_t *ctm);

typedef struct
{
	char   *name;
	double  size;
	int     bold;
	int     italic;
} font_t;

typedef struct
{
	font_t     font;
	matrix4_t *ctm_prev;
} content_state_t;

int extract_page_analyse(extract_alloc_t *alloc, extract_page_t *page);

int extract_subpage_alloc(extract_alloc_t *extract, rect_t mediabox, extract_page_t *page, subpage_t **psubpage);

void extract_subpage_free(extract_alloc_t *alloc, subpage_t **psubpage);

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

static inline span_t *content_head_as_span(content_root_t *root)
{
	assert(root != NULL && root->base.type == content_root && (root->base.next == NULL || root->base.next->type == content_span));
	return (span_t *)root->base.next;
}

point_t extract_predicted_end_of_char(char_t *char_, const span_t *span);

point_t extract_end_of_span(const span_t *span);

rect_t extract_block_pre_rotation_bounds(block_t *block, double rotate);

double extract_baseline_angle(const matrix4_t *ctm);

#endif

#ifndef ARTIFEX_EXTRACT_OUTF_H
#define ARTIFEX_EXTRACT_OUTF_H

#if defined(__GNUC__) || defined(__clang__) || defined(_WIN32)
	#define extract_FUNCTION __FUNCTION__
#else
	#define extract_FUNCTION ""
#endif

#define outf(format, ...) \
		(1 > extract_outf_verbose) ? (void) 0 : (extract_outf)(1, __FILE__, __LINE__, extract_FUNCTION, 1 , format, ##__VA_ARGS__)

#define outf0(format, ...) \
		(0 > extract_outf_verbose) ? (void) 0 : (extract_outf)(0, __FILE__, __LINE__, extract_FUNCTION, 1 , format, ##__VA_ARGS__)

#define outfx(format, ...)

extern int extract_outf_verbose;

void (extract_outf)(
		int level,
		const char *file, int line,
		const char *fn,
		int ln,
		const char *format,
		...
		)
		#ifdef __GNUC__
		__attribute__ ((format (printf, 6, 7)))
		#endif
		;

void extract_outf_verbose_set(int verbose);

#endif

#define DEBUG_WRITE_AS_PS

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

static void
rectlist_append(rectlist_t *list, rect_t *box)
{
	int i;

	for (i = 0; i < list->len; i++)
	{
		rect_t *r = &list->list[i];
		rect_t smaller, larger;

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
			return;
		if (extract_rect_contains_rect(*box, smaller))
		{

			--list->len;

			if (i < list->len)
			{
				memcpy(r, &list->list[list->len], sizeof(*r));
				i--;
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

	c = extract_rect_intersect(*a, *b);

	if (!extract_rect_valid(c))
		return;

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

static int boxer_feed(boxer_t *boxer, rect_t *bbox)
{
	rect_t box;

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

	box.min.x = boxer->mediabox.min.x;
	box.min.y = boxer->mediabox.min.y;
	box.max.x = bbox->min.x;
	box.max.y = boxer->mediabox.max.y;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	box.min.x = bbox->max.x;
	box.min.y = boxer->mediabox.min.y;
	box.max.x = boxer->mediabox.max.x;
	box.max.y = boxer->mediabox.max.y;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	box.min.x = boxer->mediabox.min.x;
	box.min.y = boxer->mediabox.min.y;
	box.max.x = boxer->mediabox.max.x;
	box.max.y = bbox->min.y;
	boxlist_feed_intersect(newlist, boxer->list, &box);

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

static void boxer_sort(boxer_t *boxer)
{
	qsort(boxer->list->list, boxer->list->len, sizeof(rect_t), compare_areas);
}

static int boxer_results(boxer_t *boxer, rect_t **list)
{
	*list = boxer->list->list;
	return boxer->list->len;
}

static void boxer_destroy(boxer_t *boxer)
{
	if (!boxer)
		return;

	extract_free(boxer->alloc, &boxer->list);
	extract_free(boxer->alloc, &boxer);
}

static rect_t boxer_margins(boxer_t *boxer)
{
	rectlist_t *list = boxer->list;
	int i;
	rect_t margins = boxer->mediabox;

	for (i = 0; i < list->len; i++)
	{
		rect_t *r = &list->list[i];
		if (r->min.x <= margins.min.x && r->min.y <= margins.min.y && r->max.y >= margins.max.y)
			margins.min.x = r->max.x;
		else if (r->max.x >= margins.max.x && r->min.y <= margins.min.y && r->max.y >= margins.max.y)
			margins.max.x = r->min.x;
		else if (r->min.x <= margins.min.x && r->max.x >= margins.max.x && r->min.y <= margins.min.y)
			margins.min.y = r->max.y;
		else if (r->min.x <= margins.min.x && r->max.x >= margins.max.x && r->max.y >= margins.max.y)
			margins.max.y = r->min.y;
	}

	return margins;
}

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

	for (s = 0; s < split->count; s++)
	{
		if (collate_splits(alloc, &split->split[s]))
			return -1;
		if (split->split[s]->type == split->type)
			n += split->split[s]->count;
		else
			n++;
	}

	if (n == split->count)
		return 0;

	if (extract_split_alloc(alloc, split->type, n, &newsplit))
		return -1;

	newsplit->weight = split->weight;

	i = 0;
	for (s = 0; s < split->count; s++)
	{
		split_t *sub = split->split[s];
		if (sub->type == split->type)
		{

			for (j = 0; j < sub->count; j++)
			{
				newsplit->split[i++] = sub->split[j];
				sub->split[j] = NULL;
			}
		}
		else
		{

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

	if (page->subpages_num != 1) return 0;

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

#include "extract/buffer.h"
#include "extract/alloc.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct extract_buffer_t
{

	extract_buffer_cache_t   cache;
	extract_alloc_t         *alloc;
	void                    *handle;
	extract_buffer_fn_read  *fn_read;
	extract_buffer_fn_write *fn_write;
	extract_buffer_fn_cache *fn_cache;
	extract_buffer_fn_close *fn_close;
	size_t                   pos;
};

extract_alloc_t *extract_buffer_alloc(extract_buffer_t* buffer)
{
	return buffer->alloc;
}

int extract_buffer_open(extract_alloc_t          *alloc,
			void                     *handle,
			extract_buffer_fn_read   *fn_read,
			extract_buffer_fn_write  *fn_write,
			extract_buffer_fn_cache  *fn_cache,
			extract_buffer_fn_close  *fn_close,
			extract_buffer_t        **o_buffer)
{
	extract_buffer_t *buffer;

	if (extract_malloc(alloc, &buffer, sizeof(*buffer)))
		return -1;

	buffer->alloc = alloc;
	buffer->handle = handle;
	buffer->fn_read = fn_read;
	buffer->fn_write = fn_write;
	buffer->fn_cache = fn_cache;
	buffer->fn_close = fn_close;
	buffer->cache.cache = NULL;
	buffer->cache.numbytes = 0;
	buffer->cache.pos = 0;
	buffer->pos = 0;

	*o_buffer = buffer;

	return 0;
}

size_t extract_buffer_pos(extract_buffer_t *buffer)
{
	size_t ret = buffer->pos;

	if (buffer->cache.cache)
		ret += buffer->cache.pos;

	return ret;
}

static int cache_flush(extract_buffer_t *buffer, size_t *o_actual)
{
	int e = -1;
	size_t p = 0;

	assert(buffer->cache.pos <= buffer->cache.numbytes);

	while (p != buffer->cache.pos)
	{
		size_t actual;
		if (buffer->fn_write(
			buffer->handle,
			(char*) buffer->cache.cache + p,
			buffer->cache.pos - p,
			&actual
			)) goto end;
		buffer->pos += actual;
		p += actual;
		if (actual == 0)
		{

			outf("*** buffer->fn_write() EOF\n");
			e = 0;
			goto end;
		}
	}
	outfx("cache flush, buffer->pos=%i p=buffer->cache.pos=%i\n",
		buffer->pos, p);
	buffer->cache.cache = NULL;
	buffer->cache.numbytes = 0;
	buffer->cache.pos = 0;

	e = 0;
end:
	*o_actual = p;

	return e;
}

int extract_buffer_close(extract_buffer_t **p_buffer)
{
	extract_buffer_t *buffer = *p_buffer;
	int e = -1;

	if (buffer == NULL)
		return 0;

	if (buffer->cache.cache && buffer->fn_write)
	{

		size_t cache_bytes = buffer->cache.pos;
		size_t actual;
		if (cache_flush(buffer, &actual)) goto end;
		if (actual != cache_bytes)
		{
			e = 1;
			goto end;
		}
	}

	if (buffer->fn_close)
		buffer->fn_close(buffer->handle);

	e = 0;
end:
	extract_free(buffer->alloc, &buffer);
	*p_buffer = NULL;

	return e;
}

static int simple_cache(void *handle, void **o_cache, size_t *o_numbytes)
{

	(void) handle;
	*o_cache = NULL;
	*o_numbytes = 0;

	return 0;
}

int extract_buffer_open_simple(extract_alloc_t           *alloc,
				const void               *data,
				size_t                    numbytes,
				void                     *handle,
				extract_buffer_fn_close  *fn_close,
				extract_buffer_t        **o_buffer)
{
	extract_buffer_t *buffer;

	if (extract_malloc(alloc, &buffer, sizeof(*buffer)))
		return -1;

	buffer->alloc = alloc;
	buffer->cache.cache = (void*) data;
	buffer->cache.numbytes = numbytes;
	buffer->cache.pos = 0;
	buffer->handle = handle;
	buffer->fn_read = NULL;
	buffer->fn_write = NULL;
	buffer->fn_cache = simple_cache;
	buffer->fn_close = fn_close;
	*o_buffer = buffer;

	return 0;
}

static int file_read(void *handle, void *data, size_t numbytes, size_t *o_actual)
{
	FILE   *file = handle;
	size_t  n    = fread(data, 1, numbytes, file);

	outfx("file=%p numbytes=%i => n=%zi", file, numbytes, n);
	assert(o_actual);

	*o_actual = n;
	if (n == 0 && ferror(file))
	{
		errno = EIO;
		return -1;
	}

	return 0;
}

static int file_write(void *handle, const void *data, size_t numbytes, size_t *o_actual)
{
	FILE   *file = handle;
	size_t  n    = fwrite(data, 1 , numbytes , file);

	outfx("file=%p numbytes=%i => n=%zi", file, numbytes, n);
	assert(o_actual);

	*o_actual = n;
	if (n == 0 && ferror(file))
	{
		errno = EIO;
		return -1;
	}

	return 0;
}

static void file_close(void *handle)
{
	FILE *file = handle;

	if (file)
		fclose(file);
}

int extract_buffer_open_file(extract_alloc_t *alloc, const char *path, int writable, extract_buffer_t **o_buffer)
{
	int   e    = -1;
	FILE *file = fopen(path, (writable) ? "wb" : "rb");

	if (!file)
	{
		outf("failed to open '%s': %s", path, strerror(errno));
		goto end;
	}

	if (extract_buffer_open(alloc,
				file ,
				writable ? NULL : file_read,
				writable ? file_write : NULL,
				NULL ,
				file_close,
				o_buffer)) goto end;

	e = 0;
end:

	if (e)
	{
		if (file)
			fclose(file);
		*o_buffer = NULL;
	}

	return e;
}

int extract_buffer_read_internal(extract_buffer_t *buffer,
				void              *destination,
				size_t             numbytes,
				size_t            *o_actual)
{
	int    e   = -1;
	size_t pos = 0;

	while (pos != numbytes)
	{
		size_t n = buffer->cache.numbytes - buffer->cache.pos;
		if (n)
		{

			if (n > numbytes - pos) n = numbytes - pos;
			memcpy((char *)destination + pos, (char *)buffer->cache.cache + buffer->cache.pos, n);
			pos += n;
			buffer->cache.pos += n;
		}

		else if (buffer->fn_read &&
				(buffer->fn_cache == NULL ||
					(buffer->cache.numbytes && numbytes - pos > buffer->cache.numbytes / 2)))
		{

			size_t actual;
			outfx("using buffer->fn_read() directly for numbytes-pos=%i\n", numbytes-pos);
			if (buffer->fn_read(buffer->handle, (char*) destination + pos, numbytes - pos, &actual))
				goto end;
			if (actual == 0)
				break;
			pos += actual;
			buffer->pos += actual;
		}
		else
		{

			outfx("using buffer->fn_cache() for buffer->cache.numbytes=%i\n", buffer->cache.numbytes);
			if (buffer->fn_cache(buffer->handle, &buffer->cache.cache, &buffer->cache.numbytes))
				goto end;
			buffer->pos += buffer->cache.pos;
			buffer->cache.pos = 0;
			if (buffer->cache.numbytes == 0)
				break;
		}
	}

	e = 0;
end:

	if (o_actual)
		*o_actual = pos;
	if (e == 0 && pos != numbytes)
		return +1;

	return e;
}

int extract_buffer_write_internal(extract_buffer_t *buffer,
                                  const void       *source,
                                  size_t            numbytes,
                                  size_t           *o_actual)
{
	int    e   = -1;
	size_t pos = 0;

	if (buffer->fn_write == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	while (pos != numbytes)
	{
		size_t n = buffer->cache.numbytes - buffer->cache.pos;
		outfx("numbytes=%i pos=%i. buffer->cache.numbytes=%i buffer->cache.pos=%i\n",
			numbytes, pos, buffer->cache.numbytes, buffer->cache.pos);
		if (n)
		{

			if (n > numbytes - pos)
				n = numbytes - pos;
			outfx("writing to cache: numbytes=%i n=%i\n", numbytes, n);
			memcpy((char*) buffer->cache.cache + buffer->cache.pos, (char*) source + pos, n);
			pos += n;
			buffer->cache.pos += n;
		}
		else
		{

			outfx("cache empty. pos=%i. buffer->cache.numbytes=%i buffer->cache.pos=%i\n",
				pos, buffer->cache.numbytes, buffer->cache.pos);
			{

				size_t actual;
				size_t b = buffer->cache.numbytes;
				ptrdiff_t delta;
				int ee = cache_flush(buffer, &actual);
				assert(actual <= b);
				delta = actual - b;
				pos += delta;
				buffer->pos += delta;
				if (delta)
				{

					outf("failed to flush. actual=%li delta=%li\n", (long) actual, (long) delta);
					e = 0;
					goto end;
				}
				if (ee) goto end;
			}

			if (buffer->fn_cache == NULL ||
				(buffer->cache.numbytes && numbytes - pos > buffer->cache.numbytes / 2))
			{

				size_t actual;
				if (buffer->fn_write(buffer->handle, (char*) source + pos, numbytes - pos, &actual))
					goto end;
				if (actual == 0)
					break;
				outfx("direct write numbytes-pos=%i actual=%i buffer->pos=%i => %i\n",
					numbytes-pos, actual, buffer->pos, buffer->pos + actual);
				pos += actual;
				buffer->pos += actual;
			}
			else
			{

				outfx("repopulating cache buffer->pos=%i", buffer->pos);
				if (buffer->fn_cache(buffer->handle, &buffer->cache.cache, &buffer->cache.numbytes))
					goto end;
				buffer->cache.pos = 0;
				if (buffer->cache.numbytes == 0)
					break;
			}
		}
	}

	e = 0;
end:

	if (o_actual)
		*o_actual = pos;
	if (e == 0 && pos != numbytes)
		e = +1;

	return e;
}

static int expanding_memory_buffer_write(void *handle, const void *source, size_t numbytes, size_t *o_actual)
{

	extract_buffer_expanding_t *ebe = handle;
	if ((char *)source >= ebe->data && (char *)source < ebe->data + ebe->alloc_size)
	{

		assert((size_t) ((char *)source - ebe->data) == ebe->data_size);
		assert((size_t) ((char *)source - ebe->data + numbytes) <= ebe->alloc_size);
		ebe->data_size += numbytes;
	}
	else
	{

		if (extract_realloc2(ebe->buffer->alloc, &ebe->data, ebe->alloc_size, ebe->data_size + numbytes))
			return -1;
		ebe->alloc_size = ebe->data_size + numbytes;
		memcpy(ebe->data + ebe->data_size, source, numbytes);
		ebe->data_size += numbytes;
	}
	*o_actual = numbytes;

	return 0;
}

static int expanding_memory_buffer_cache(void *handle, void **o_cache, size_t *o_numbytes)
{
	extract_buffer_expanding_t *ebe   = handle;
	size_t                      delta = 4096;

	if (extract_realloc2(ebe->buffer->alloc, &ebe->data, ebe->alloc_size, ebe->data_size + delta))
		return -1;

	ebe->alloc_size = ebe->data_size + delta;
	*o_cache = ebe->data + ebe->data_size;
	*o_numbytes = delta;

	return 0;
}

int extract_buffer_expanding_create(extract_alloc_t *alloc, extract_buffer_expanding_t *ebe)
{
	ebe->data = NULL;
	ebe->data_size = 0;
	ebe->alloc_size = 0;
	if (extract_buffer_open(alloc,
				ebe,
				NULL ,
				expanding_memory_buffer_write,
				expanding_memory_buffer_cache,
				NULL ,
				&ebe->buffer))
		return -1;

	return 0;
}

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
	int len2 = q1pos + len1*2;
	int p;
	content_t *q2 = q1;

	if (len2 > n)
		len2 = n;
	len2 -= q1pos + len1;

	if (len2 <= 0)
		len1 += len2;

	for (p = 0; p < len1; p++)
		q2 = q2->next;

	if (len2 <= 0)
		return q2;

	while (1)
	{
		if (cmp(q1, q2) > 0)
		{

			content_t *a = q1->prev;
			content_t *b = q2->next;
			content_t *d = q2->prev;
			d->next = b;
			b->prev = d;
			a->next = q2;
			q2->prev = a;
			q2->next = q1;
			q1->prev = q2;

			q2 = b;
			len2--;
			if (len2 == 0)
				break;
		} else {

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

#include "extract/extract.h"

#ifndef EXTRACT_DOCX_TEMPLATE_H
#define EXTRACT_DOCX_TEMPLATE_H

typedef struct
{
    const char* name;
    const char* text;
} docx_template_item_t;

extern const docx_template_item_t docx_template_items[];
extern int docx_template_items_num;

#endif

#ifndef ARTIFEX_EXTRACT_DOCX_H
#define ARTIFEX_EXTRACT_DOCX_H

int extract_document_to_docx_content(
		extract_alloc_t   *alloc,
		document_t        *document,
		int                spacing,
		int                rotation,
		int                images,
		extract_astring_t *content);

int extract_docx_write_template(
		extract_alloc_t   *alloc,
		extract_astring_t *contentss,
		int                contentss_num,
		images_t          *images,
		const char        *path_template,
		const char        *path_out,
		int                preserve_dir);

int extract_docx_content_item(
		extract_alloc_t    *alloc,
		extract_astring_t  *contentss,
		int                 contentss_num,
		images_t           *images,
		const char         *name,
		const char         *text,
		char              **text2);

#endif

#include "memento.h"
#ifndef EXTRACT_SYS_H
#define EXTRACT_SYS_H

#include "extract/alloc.h"

#include <stdio.h>

int extract_systemf(extract_alloc_t* alloc, const char* format, ...);

int  extract_read_all(extract_alloc_t* alloc, FILE* in, char** o_out);

int  extract_read_all_path(extract_alloc_t* alloc, const char* path, char** o_text);

int  extract_write_all(const void* data, size_t data_size, const char* path);

int extract_check_path_shell_safe(const char* path);

int extract_remove_directory(extract_alloc_t* alloc, const char* path);

int extract_mkdir(const char* path, int mode);

#endif

#ifndef EXTRACT_TEXT_H
#define EXTRACT_TEXT_H

#include "extract/alloc.h"

int extract_content_insert(
        extract_alloc_t*    alloc,
        const char*         original,
        const char*         single_name,
        const char*         mid_begin_name,
        const char*         mid_end_name,
        extract_astring_t*  contentss,
        int                 contentss_num,
        char**              o_out
        );

#endif

#ifndef ARTIFEX_EXTRACT_ZIP
#define ARTIFEX_EXTRACT_ZIP

#include "extract/buffer.h"

#include <stddef.h>

typedef struct extract_zip_t extract_zip_t;

int extract_zip_open(extract_buffer_t *buffer, extract_zip_t **o_zip);

int extract_zip_write_file(
		extract_zip_t *zip,
		const void    *data,
		size_t         data_length,
		const char    *name);

int extract_zip_close(extract_zip_t **pzip);

#endif

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

static int
docx_paragraph_start(extract_alloc_t *alloc, extract_astring_t *output)
{
	return extract_astring_cat(alloc, output, "\n\n<w:p>");
}

static int
docx_paragraph_finish(extract_alloc_t *alloc, extract_astring_t *output)
{
	return extract_astring_cat(alloc, output, "\n</w:p>");
}

static int
docx_run_start(	extract_alloc_t   *alloc,
		extract_astring_t *output,
		content_state_t   *content_state)
{
	int e = 0;

	if (!e) e = extract_astring_cat(alloc, output, "\n<w:r><w:rPr><w:rFonts w:ascii=\"");
	if (!e) e = extract_astring_cat(alloc, output, content_state->font.name);
	if (!e) e = extract_astring_cat(alloc, output, "\" w:hAnsi=\"");
	if (!e) e = extract_astring_cat(alloc, output, content_state->font.name);
	if (!e) e = extract_astring_cat(alloc, output, "\"/>");
	if (!e && content_state->font.bold) e = extract_astring_cat(alloc, output, "<w:b/>");
	if (!e && content_state->font.italic) e = extract_astring_cat(alloc, output, "<w:i/>");
	{
		char font_size_text[32];

		if (!e) e = extract_astring_cat(alloc, output, "<w:sz w:val=\"");
		snprintf(font_size_text, sizeof(font_size_text), "%f", content_state->font.size * 2);
		extract_astring_cat(alloc, output, font_size_text);
		extract_astring_cat(alloc, output, "\"/>");

		if (!e) e = extract_astring_cat(alloc, output, "<w:szCs w:val=\"");
		snprintf(font_size_text, sizeof(font_size_text), "%f", content_state->font.size * 2);
		extract_astring_cat(alloc, output, font_size_text);
		extract_astring_cat(alloc, output, "\"/>");
	}
	if (!e) e = extract_astring_cat(alloc, output, "</w:rPr><w:t xml:space=\"preserve\">");

	return e;
}

static int
docx_run_finish(extract_alloc_t   *alloc,
		content_state_t   *state,
		extract_astring_t *output)
{
	if (state) state->font.name = NULL;

	return extract_astring_cat(alloc, output, "</w:t></w:r>");
}

static int
docx_paragraph_empty(
		extract_alloc_t   *alloc,
		extract_astring_t *output)
{
	int e = -1;
	static char fontname[] = "OpenSans";
	content_state_t content_state = {0};

	if (docx_paragraph_start(alloc, output)) goto end;

	content_state.font.name = fontname;
	content_state.font.size = 10;
	content_state.font.bold = 0;
	content_state.font.italic = 0;

	if (docx_run_start(alloc, output, &content_state)) goto end;

	if (docx_run_finish(alloc, NULL , output)) goto end;
	if (docx_paragraph_finish(alloc, output)) goto end;

	e = 0;
end:

    return e;
}

static int
docx_char_truncate_if(extract_astring_t *output, char c)
{
	if (output->chars_num && output->chars[output->chars_num-1] == c)
		extract_astring_truncate(output, 1);

	return 0;
}

static int
document_to_docx_content_paragraph(
		extract_alloc_t   *alloc,
		content_state_t   *content_state,
		paragraph_t       *paragraph,
		extract_astring_t *content)
{
	int                    e = -1;
	content_line_iterator  lit;
	line_t                *line;

	if (docx_paragraph_start(alloc, content)) goto end;

	if ((paragraph->line_flags & paragraph_not_fully_justified) == 0)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"both\"/></w:pPr>"))
			goto end;
	}
	else if ((paragraph->line_flags & paragraph_not_centred) == 0)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"center\"/></w:pPr>"))
			goto end;
	}
	else if ((paragraph->line_flags & (paragraph_not_aligned_left | paragraph_not_aligned_right)) == paragraph_not_aligned_left)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"right\"/></w:pPr>"))
			goto end;
	}
	else if ((paragraph->line_flags & (paragraph_not_aligned_left | paragraph_not_aligned_right)) == paragraph_not_aligned_right)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"left\"/></w:pPr>"))
			goto end;
	}

	for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
	{
		content_span_iterator  sit;
		span_t                *span;

		for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
		{
			int si;
			double font_size_new;

			content_state->ctm_prev = &span->ctm;
			font_size_new = extract_font_size(&span->ctm);
			if (!content_state->font.name
				|| strcmp(span->font_name, content_state->font.name)
				|| span->flags.font_bold != content_state->font.bold
				|| span->flags.font_italic != content_state->font.italic
				|| font_size_new != content_state->font.size)
			{
				if (content_state->font.name)
					if (docx_run_finish(alloc, content_state, content))
						goto end;

				content_state->font.name = span->font_name;
				content_state->font.bold = span->flags.font_bold;
				content_state->font.italic = span->flags.font_italic;
				content_state->font.size = font_size_new;
				if (docx_run_start(alloc, content, content_state))
					goto end;
			}

			for (si=0; si<span->chars_num; ++si)
			{
				char_t* char_ = &span->chars[si];
				int c = char_->ucs;
				if (extract_astring_catc_unicode_xml(alloc, content, c))
					goto end;
			}

			if (docx_char_truncate_if(content, '-'))
				goto end;
		}
		if (paragraph->line_flags & paragraph_breaks_strangely)
		{
			if (extract_astring_cat(alloc, content, "<w:br/>"))
				goto end;
		}
	}
	if (content_state->font.name)
	{
		if (docx_run_finish(alloc, content_state, content)) goto
			end;
	}
	if (docx_paragraph_finish(alloc, content))
		goto end;

	e = 0;
end:

	return e;
}

static int
docx_append_image(
		extract_alloc_t   *alloc,
		extract_astring_t *output,
		image_t           *image)
{
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "     <w:p>\n");
	extract_astring_cat(alloc, output, "       <w:r>\n");
	extract_astring_cat(alloc, output, "         <w:rPr>\n");
	extract_astring_cat(alloc, output, "           <w:noProof/>\n");
	extract_astring_cat(alloc, output, "         </w:rPr>\n");
	extract_astring_cat(alloc, output, "         <w:drawing>\n");
	extract_astring_cat(alloc, output, "           <wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\" wp14:anchorId=\"7057A832\" wp14:editId=\"466EB3FB\">\n");

	extract_astring_cat(alloc, output, "             <wp:docPr id=\"1\" name=\"Picture 1\"/>\n");
	extract_astring_cat(alloc, output, "             <wp:cNvGraphicFramePr>\n");
	extract_astring_cat(alloc, output, "               <a:graphicFrameLocks xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" noChangeAspect=\"1\"/>\n");
	extract_astring_cat(alloc, output, "             </wp:cNvGraphicFramePr>\n");
	extract_astring_cat(alloc, output, "             <a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
	extract_astring_cat(alloc, output, "               <a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n");
	extract_astring_cat(alloc, output, "                 <pic:pic xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n");
	extract_astring_cat(alloc, output, "                   <pic:nvPicPr>\n");
	extract_astring_cat(alloc, output, "                     <pic:cNvPr id=\"1\" name=\"Picture 1\"/>\n");
	extract_astring_cat(alloc, output, "                     <pic:cNvPicPr>\n");
	extract_astring_cat(alloc, output, "                       <a:picLocks noChangeAspect=\"1\" noChangeArrowheads=\"1\"/>\n");
	extract_astring_cat(alloc, output, "                     </pic:cNvPicPr>\n");
	extract_astring_cat(alloc, output, "                   </pic:nvPicPr>\n");
	extract_astring_cat(alloc, output, "                   <pic:blipFill>\n");
	extract_astring_catf(alloc, output,"                     <a:blip r:embed=\"%s\">\n", image->id);
	extract_astring_cat(alloc, output, "                       <a:extLst>\n");
	extract_astring_cat(alloc, output, "                         <a:ext uri=\"{28A0092B-C50C-407E-A947-70E740481C1C}\">\n");
	extract_astring_cat(alloc, output, "                           <a14:useLocalDpi xmlns:a14=\"http://schemas.microsoft.com/office/drawing/2010/main\" val=\"0\"/>\n");
	extract_astring_cat(alloc, output, "                         </a:ext>\n");
	extract_astring_cat(alloc, output, "                       </a:extLst>\n");
	extract_astring_cat(alloc, output, "                     </a:blip>\n");

	extract_astring_cat(alloc, output, "                     <a:stretch>\n");
	extract_astring_cat(alloc, output, "                       <a:fillRect/>\n");
	extract_astring_cat(alloc, output, "                     </a:stretch>\n");
	extract_astring_cat(alloc, output, "                   </pic:blipFill>\n");
	extract_astring_cat(alloc, output, "                   <pic:spPr bwMode=\"auto\">\n");
	extract_astring_cat(alloc, output, "                     <a:xfrm>\n");
	extract_astring_cat(alloc, output, "                       <a:off x=\"0\" y=\"0\"/>\n");

	extract_astring_cat(alloc, output, "                     </a:xfrm>\n");
	extract_astring_cat(alloc, output, "                     <a:prstGeom prst=\"rect\">\n");
	extract_astring_cat(alloc, output, "                       <a:avLst/>\n");
	extract_astring_cat(alloc, output, "                     </a:prstGeom>\n");
	extract_astring_cat(alloc, output, "                     <a:noFill/>\n");
	extract_astring_cat(alloc, output, "                     <a:ln>\n");
	extract_astring_cat(alloc, output, "                       <a:noFill/>\n");
	extract_astring_cat(alloc, output, "                     </a:ln>\n");
	extract_astring_cat(alloc, output, "                   </pic:spPr>\n");
	extract_astring_cat(alloc, output, "                 </pic:pic>\n");
	extract_astring_cat(alloc, output, "               </a:graphicData>\n");
	extract_astring_cat(alloc, output, "             </a:graphic>\n");
	extract_astring_cat(alloc, output, "           </wp:inline>\n");
	extract_astring_cat(alloc, output, "         </w:drawing>\n");
	extract_astring_cat(alloc, output, "       </w:r>\n");
	extract_astring_cat(alloc, output, "     </w:p>\n");
	extract_astring_cat(alloc, output, "\n");

	return 0;
}

static int
docx_output_rotated_paragraphs(
		extract_alloc_t   *alloc,
		block_t           *block,
		int                rot,
		int                x,
		int                y,
		int                w,
		int                h,
		int                text_box_id,
		extract_astring_t *output,
		content_state_t   *state)
{
	int                         e = -1;
	paragraph_t                *paragraph;
	content_paragraph_iterator  pit;

	outf("x,y=%ik,%ik = %i,%i", x/1000, y/1000, x, y);
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "<w:p>\n");
	extract_astring_cat(alloc, output, "  <w:r>\n");
	extract_astring_cat(alloc, output, "    <mc:AlternateContent>\n");
	extract_astring_cat(alloc, output, "      <mc:Choice Requires=\"wps\">\n");
	extract_astring_cat(alloc, output, "        <w:drawing>\n");
	extract_astring_cat(alloc, output, "          <wp:anchor distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\" simplePos=\"0\" relativeHeight=\"0\" behindDoc=\"0\" locked=\"0\" layoutInCell=\"1\" allowOverlap=\"1\" wp14:anchorId=\"53A210D1\" wp14:editId=\"2B7E8016\">\n");
	extract_astring_cat(alloc, output, "            <wp:simplePos x=\"0\" y=\"0\"/>\n");
	extract_astring_cat(alloc, output, "            <wp:positionH relativeFrom=\"page\">\n");
	extract_astring_catf(alloc, output,"              <wp:posOffset>%i</wp:posOffset>\n", x);
	extract_astring_cat(alloc, output, "            </wp:positionH>\n");
	extract_astring_cat(alloc, output, "            <wp:positionV relativeFrom=\"page\">\n");
	extract_astring_catf(alloc, output,"              <wp:posOffset>%i</wp:posOffset>\n", y);
	extract_astring_cat(alloc, output, "            </wp:positionV>\n");
	extract_astring_catf(alloc, output,"            <wp:extent cx=\"%i\" cy=\"%i\"/>\n", w, h);

	extract_astring_cat(alloc, output, "            <wp:wrapNone/>\n");
	extract_astring_catf(alloc, output,"            <wp:docPr id=\"%i\" name=\"Text Box %i\"/>\n", text_box_id, text_box_id);
	extract_astring_cat(alloc, output, "            <wp:cNvGraphicFramePr/>\n");
	extract_astring_cat(alloc, output, "            <a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
	extract_astring_cat(alloc, output, "              <a:graphicData uri=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\">\n");
	extract_astring_cat(alloc, output, "                <wps:wsp>\n");
	extract_astring_cat(alloc, output, "                  <wps:cNvSpPr txBox=\"1\"/>\n");
	extract_astring_cat(alloc, output, "                  <wps:spPr>\n");
	extract_astring_catf(alloc, output,"                    <a:xfrm rot=\"%i\">\n", rot);
	extract_astring_cat(alloc, output, "                      <a:off x=\"0\" y=\"0\"/>\n");

	extract_astring_cat(alloc, output, "                    </a:xfrm>\n");
	extract_astring_cat(alloc, output, "                    <a:prstGeom prst=\"rect\">\n");
	extract_astring_cat(alloc, output, "                      <a:avLst/>\n");
	extract_astring_cat(alloc, output, "                    </a:prstGeom>\n");

	if (0) {
		extract_astring_cat(alloc, output, "                    <a:solidFill>\n");
		extract_astring_cat(alloc, output, "                      <a:schemeClr val=\"lt1\"/>\n");
		extract_astring_cat(alloc, output, "                    </a:solidFill>\n");
	}

	if (0) {
		extract_astring_cat(alloc, output, "                    <a:ln w=\"175\">\n");
		extract_astring_cat(alloc, output, "                      <a:solidFill>\n");
		extract_astring_cat(alloc, output, "                        <a:prstClr val=\"black\"/>\n");
		extract_astring_cat(alloc, output, "                      </a:solidFill>\n");
		extract_astring_cat(alloc, output, "                    </a:ln>\n");
	}

	extract_astring_cat(alloc, output, "                  </wps:spPr>\n");
	extract_astring_cat(alloc, output, "                  <wps:txbx>\n");
	extract_astring_cat(alloc, output, "                    <w:txbxContent>");

#if 0
	if (0) {

		extract_astring_catf(content, "<w:p>\n"
			"<w:r><w:rPr><w:rFonts w:ascii=\"OpenSans\" w:hAnsi=\"OpenSans\"/><w:sz w:val=\"20.000000\"/><w:szCs w:val=\"15.000000\"/></w:rPr><w:t xml:space=\"preserve\">*** rotate: %f rad, %f deg. rot=%i</w:t></w:r>\n"
			"</w:p>\n",
			rotate,
			rotate * 180 / pi,
			rot
			);
	}
#endif

	for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
		if (document_to_docx_content_paragraph(alloc, state, paragraph, output)) goto end;

	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "                    </w:txbxContent>\n");
	extract_astring_cat(alloc, output, "                  </wps:txbx>\n");
	extract_astring_cat(alloc, output, "                  <wps:bodyPr rot=\"0\" spcFirstLastPara=\"0\" vertOverflow=\"overflow\" horzOverflow=\"overflow\" vert=\"horz\" wrap=\"square\" lIns=\"91440\" tIns=\"45720\" rIns=\"91440\" bIns=\"45720\" numCol=\"1\" spcCol=\"0\" rtlCol=\"0\" fromWordArt=\"0\" anchor=\"t\" anchorCtr=\"0\" forceAA=\"0\" compatLnSpc=\"1\">\n");
	extract_astring_cat(alloc, output, "                    <a:prstTxWarp prst=\"textNoShape\">\n");
	extract_astring_cat(alloc, output, "                      <a:avLst/>\n");
	extract_astring_cat(alloc, output, "                    </a:prstTxWarp>\n");
	extract_astring_cat(alloc, output, "                    <a:noAutofit/>\n");
	extract_astring_cat(alloc, output, "                  </wps:bodyPr>\n");
	extract_astring_cat(alloc, output, "                </wps:wsp>\n");
	extract_astring_cat(alloc, output, "              </a:graphicData>\n");
	extract_astring_cat(alloc, output, "            </a:graphic>\n");
	extract_astring_cat(alloc, output, "          </wp:anchor>\n");
	extract_astring_cat(alloc, output, "        </w:drawing>\n");
	extract_astring_cat(alloc, output, "      </mc:Choice>\n");

#if 0

	extract_astring_cat(alloc, output, "      <mc:Fallback>\n");
	extract_astring_cat(alloc, output, "        <w:pict>\n");
	extract_astring_cat(alloc, output, "          <v:shapetype w14:anchorId=\"53A210D1\" id=\"_x0000_t202\" coordsize=\"21600,21600\" o:spt=\"202\" path=\"m,l,21600r21600,l21600,xe\">\n");
	extract_astring_cat(alloc, output, "            <v:stroke joinstyle=\"miter\"/>\n");
	extract_astring_cat(alloc, output, "            <v:path gradientshapeok=\"t\" o:connecttype=\"rect\"/>\n");
	extract_astring_cat(alloc, output, "          </v:shapetype>\n");
	extract_astring_catf(alloc, output,"          <v:shape id=\"Text Box %i\" o:spid=\"_x0000_s1026\" type=\"#_x0000_t202\" style=\"position:absolute;margin-left:71.25pt;margin-top:48.75pt;width:254.25pt;height:180pt;rotation:-2241476fd;z-index:251659264;visibility:visible;mso-wrap-style:square;mso-wrap-distance-left:9pt;mso-wrap-distance-top:0;mso-wrap-distance-right:9pt;mso-wrap-distance-bottom:0;mso-position-horizontal:absolute;mso-position-horizontal-relative:text;mso-position-vertical:absolute;mso-position-vertical-relative:text;v-text-anchor:top\" o:gfxdata=\"UEsDBBQABgAIAAAAIQC2gziS/gAAAOEBAAATAAAAW0NvbnRlbnRfVHlwZXNdLnhtbJSRQU7DMBBF&#10;90jcwfIWJU67QAgl6YK0S0CoHGBkTxKLZGx5TGhvj5O2G0SRWNoz/78nu9wcxkFMGNg6quQqL6RA&#10;0s5Y6ir5vt9lD1JwBDIwOMJKHpHlpr69KfdHjyxSmriSfYz+USnWPY7AufNIadK6MEJMx9ApD/oD&#10;OlTrorhX2lFEilmcO2RdNtjC5xDF9pCuTyYBB5bi6bQ4syoJ3g9WQ0ymaiLzg5KdCXlKLjvcW893&#10;SUOqXwnz5DrgnHtJTxOsQfEKIT7DmDSUCaxw7Rqn8787ZsmRM9e2VmPeBN4uqYvTtW7jvijg9N/y&#10;JsXecLq0q+WD6m8AAAD//wMAUEsDBBQABgAIAAAAIQA4/SH/1gAAAJQBAAALAAAAX3JlbHMvLnJl&#10;bHOkkMFqwzAMhu+DvYPRfXGawxijTi+j0GvpHsDYimMaW0Yy2fr2M4PBMnrbUb/Q94l/f/hMi1qR&#10;JVI2sOt6UJgd+ZiDgffL8ekFlFSbvV0oo4EbChzGx4f9GRdb25HMsYhqlCwG5lrLq9biZkxWOiqY&#10;22YiTra2kYMu1l1tQD30/bPm3wwYN0x18gb45AdQl1tp5j/sFB2T0FQ7R0nTNEV3j6o9feQzro1i&#10;OWA14Fm+Q8a1a8+Bvu/d/dMb2JY5uiPbhG/ktn4cqGU/er3pcvwCAAD//wMAUEsDBBQABgAIAAAA&#10;IQDQg5pQVgIAALEEAAAOAAAAZHJzL2Uyb0RvYy54bWysVE1v2zAMvQ/YfxB0X+2k+WiDOEXWosOA&#10;oi3QDj0rstwYk0VNUmJ3v35PipMl3U7DLgJFPj+Rj6TnV12j2VY5X5Mp+OAs50wZSWVtXgv+7fn2&#10;0wVnPghTCk1GFfxNeX61+Phh3tqZGtKadKkcA4nxs9YWfB2CnWWZl2vVCH9GVhkEK3KNCLi616x0&#10;ogV7o7Nhnk+yllxpHUnlPbw3uyBfJP6qUjI8VJVXgemCI7eQTpfOVTyzxVzMXp2w61r2aYh/yKIR&#10;tcGjB6obEQTbuPoPqqaWjjxV4UxSk1FV1VKlGlDNIH9XzdNaWJVqgTjeHmTy/49W3m8fHatL9I4z&#10;Ixq06Fl1gX2mjg2iOq31M4CeLGChgzsie7+HMxbdVa5hjiDu4HI8ml5MpkkLVMcAh+xvB6kjt4Tz&#10;fDi8uJyOOZOIwZ7keWpGtmOLrNb58EVRw6JRcIdeJlqxvfMBGQC6h0S4J12Xt7XW6RLnR11rx7YC&#10;ndch5YwvTlDasLbgk/NxnohPYpH68P1KC/k9Vn3KgJs2cEaNdlpEK3SrrhdoReUbdEvSQAZv5W0N&#10;3jvhw6NwGDQ4sTzhAUelCclQb3G2Jvfzb/6IR/8R5azF4Bbc/9gIpzjTXw0m43IwGsVJT5fReDrE&#10;xR1HVscRs2muCQqh+8gumREf9N6sHDUv2LFlfBUhYSTeLnjYm9dht07YUamWywTCbFsR7syTlZF6&#10;383n7kU42/czYBTuaT/iYvaurTts/NLQchOoqlPPo8A7VXvdsRepLf0Ox8U7vifU7z/N4hcAAAD/&#10;/wMAUEsDBBQABgAIAAAAIQBh17L63wAAAAoBAAAPAAAAZHJzL2Rvd25yZXYueG1sTI9BT4NAEIXv&#10;Jv6HzZh4s0ubgpayNIboSW3Syg9Y2BGI7CyyS0v99Y4nPU3ezMub72W72fbihKPvHClYLiIQSLUz&#10;HTUKyvfnuwcQPmgyuneECi7oYZdfX2U6Ne5MBzwdQyM4hHyqFbQhDKmUvm7Rar9wAxLfPtxodWA5&#10;NtKM+szhtperKEqk1R3xh1YPWLRYfx4nq8APVfz9VQxPb+WUNC+vZbGPDhelbm/mxy2IgHP4M8Mv&#10;PqNDzkyVm8h40bNer2K2Ktjc82RDEi+5XKVgHfNG5pn8XyH/AQAA//8DAFBLAQItABQABgAIAAAA&#10;IQC2gziS/gAAAOEBAAATAAAAAAAAAAAAAAAAAAAAAABbQ29udGVudF9UeXBlc10ueG1sUEsBAi0A&#10;FAAGAAgAAAAhADj9If/WAAAAlAEAAAsAAAAAAAAAAAAAAAAALwEAAF9yZWxzLy5yZWxzUEsBAi0A&#10;FAAGAAgAAAAhANCDmlBWAgAAsQQAAA4AAAAAAAAAAAAAAAAALgIAAGRycy9lMm9Eb2MueG1sUEsB&#10;Ai0AFAAGAAgAAAAhAGHXsvrfAAAACgEAAA8AAAAAAAAAAAAAAAAAsAQAAGRycy9kb3ducmV2Lnht&#10;bFBLBQYAAAAABAAEAPMAAAC8BQAAAAA=&#10;\" fillcolor=\"white [3201]\" strokeweight=\".5pt\">\n", text_box_id);
	extract_astring_cat(alloc, output, "            <v:textbox>\n");
	extract_astring_cat(alloc, output, "              <w:txbxContent>");

	for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
		if (document_to_docx_content_paragraph(alloc, state, paragraph, output)) goto end;

	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "              </w:txbxContent>\n");
	extract_astring_cat(alloc, output, "            </v:textbox>\n");
	extract_astring_cat(alloc, output, "          </v:shape>\n");
	extract_astring_cat(alloc, output, "        </w:pict>\n");
	extract_astring_cat(alloc, output, "      </mc:Fallback>\n");
#endif
	extract_astring_cat(alloc, output, "    </mc:AlternateContent>\n");
	extract_astring_cat(alloc, output, "  </w:r>\n");
	extract_astring_cat(alloc, output, "</w:p>");

	e = 0;
end:

	return e;
}

static int
docx_append_table(
		extract_alloc_t   *alloc,
		table_t           *table,
		extract_astring_t *output)
{
	int e = -1;
	int y;

	if (extract_astring_cat(alloc, output,
				"\n"
				"    <w:tbl>\n"
				"        <w:tblLayout w:type=\"autofit\"/>\n"))
		goto end;

	for (y=0; y<table->cells_num_y; ++y)
	{
		int x;
		if (extract_astring_cat(alloc, output,
					"        <w:tr>\n"
					"            <w:trPr/>\n")) goto end;

		for (x=0; x<table->cells_num_x; ++x)
		{
			cell_t* cell = table->cells[y*table->cells_num_x + x];
			if (!cell->left) continue;

			if (extract_astring_cat(alloc, output, "            <w:tc>\n"))
				goto end;

			{
				if (extract_astring_cat(alloc, output,
							"                <w:tcPr>\n"
							"                    <w:tcBorders>\n"
							"                        <w:top w:val=\"double\" w:sz=\"2\" w:space=\"0\" w:color=\"808080\"/>\n"
							"                        <w:start w:val=\"double\" w:sz=\"2\" w:space=\"0\" w:color=\"808080\"/>\n"
							"                        <w:bottom w:val=\"double\" w:sz=\"2\" w:space=\"0\" w:color=\"808080\"/>\n"
							"                        <w:end w:val=\"double\" w:sz=\"2\" w:space=\"0\" w:color=\"808080\"/>\n"
							"                    </w:tcBorders>\n"))
					goto end;
				if (cell->extend_right > 1)
				{
					if (extract_astring_catf(alloc, output, "                    <w:gridSpan w:val=\"%i\"/>\n", cell->extend_right))
						goto end;
				}
				if (cell->above)
				{
					if (cell->extend_down > 1)
					{
						if (extract_astring_catf(alloc, output, "                    <w:vMerge w:val=\"restart\"/>\n", cell->extend_down))
							goto end;
					}
				}
				else
				{
					if (extract_astring_catf(alloc, output, "                    <w:vMerge w:val=\"continue\"/>\n"))
						goto end;
				}
				if (extract_astring_cat(alloc, output, "                </w:tcPr>\n"))
					goto end;
			}

			{
				content_paragraph_iterator  pit;
				paragraph_t                *paragraph;
				size_t                      chars_num_old = output->chars_num;
				content_state_t             content_state = {0};

				content_state.font.name = NULL;
				content_state.ctm_prev = NULL;
				for (paragraph = content_paragraph_iterator_init(&pit, &cell->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
					if (document_to_docx_content_paragraph(alloc, &content_state, paragraph, output))
						goto end;

				if (content_state.font.name)
					if (docx_run_finish(alloc, &content_state, output)) goto end;

				if (output->chars_num == chars_num_old)
					if (extract_astring_catf(alloc, output, "<w:p/>\n"))
						goto end;
			}
			if (extract_astring_cat(alloc, output, "            </w:tc>\n"))
				goto end;
		}
		if (extract_astring_cat(alloc, output, "        </w:tr>\n"))
			goto end;
	}
	if (extract_astring_cat(alloc, output, "    </w:tbl>\n"))
		goto end;

	e = 0;
end:

	return e;
}

static int
docx_append_rotated_paragraphs(
		extract_alloc_t    *alloc,
		content_state_t    *state,
		block_t            *block,
		int                *text_box_id,
		double              angle,
		extract_astring_t  *output)
{

	int               e           = -1;
	rect_t            bounds;

	bounds = extract_block_pre_rotation_bounds(block, angle);

	outf("angle=%f pre-transform box is: (%f %f) to (%f %f)",
		angle, bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y);

	*text_box_id += 1;

	{

		int rot = (int) (angle * 180 / pi * 60000);

		double point_to_emu = 12700;
		int x = (int) (bounds.min.x * point_to_emu);
		int y = (int) (bounds.min.y * point_to_emu);
		int w = (int) ((bounds.max.x - bounds.min.x) * point_to_emu);
		int h = (int) ((bounds.max.y - bounds.min.y) * point_to_emu);

		if (0) outf("rotate: %f rad, %f deg. rot=%i", angle, angle*180/pi, rot);

		if (docx_output_rotated_paragraphs(alloc, block, rot, x, y, w, h, *text_box_id, output, state))
			goto end;
	}

	e = 0;
end:

	return e;
}

int
extract_document_to_docx_content(
		extract_alloc_t   *alloc,
		document_t        *document,
		int                spacing,
		int                rotation,
		int                images,
		extract_astring_t *output)
{
	int e = -1;
	int text_box_id = 0;
	int p;

	for (p=0; p<document->pages_num; ++p)
	{
		extract_page_t *page = document->pages[p];
		int c;

		for (c=0; c<page->subpages_num; ++c)
		{
			subpage_t                  *subpage = page->subpages[c];
			content_iterator            cit;
			content_t                  *content;
			content_table_iterator      tit;
			table_t                    *table;

			content_state_t content_state;
			content_state.font.name = NULL;
			content_state.font.size = 0;
			content_state.font.bold = 0;
			content_state.font.italic = 0;
			content_state.ctm_prev = NULL;

			content = content_iterator_init(&cit, &subpage->content);
			table = content_table_iterator_init(&tit, &subpage->tables);
			while (1)
			{
				double y_paragraph;
				double y_table;

				block_t *block = (content && content->type == content_block) ? (block_t *)content : NULL;

				paragraph_t *paragraph = (content && content->type == content_paragraph) ? (paragraph_t *)content : (block ? content_first_paragraph(&block->content) : NULL);
				line_t *first_line = paragraph ? content_first_line(&paragraph->content) : NULL;
				span_t *first_span = first_line ? content_head_as_span(&first_line->content) : NULL;

				if (!paragraph && !table) break;

				y_paragraph = (first_span) ? first_span->chars[0].y : DBL_MAX;
				y_table = (table) ? table->pos.y : DBL_MAX;

				if (first_span && y_paragraph < y_table)
				{
					const matrix4_t *ctm   = &first_span->ctm;
					double           angle = extract_baseline_angle(ctm);

					if (spacing
						&& content_state.ctm_prev
						&& first_line
						&& first_span
						&& extract_matrix4_cmp(content_state.ctm_prev,
									&first_span->ctm))
					{

						if (docx_paragraph_empty(alloc, output))
							goto end;
					}

					if (spacing)
						if (docx_paragraph_empty(alloc, output))
							goto end;

					if (rotation && angle != 0)
					{
						assert(block);
						if (docx_append_rotated_paragraphs(alloc, &content_state, block, &text_box_id, angle, output))
							goto end;
					}
					else if (block)
					{
						content_paragraph_iterator pit;
						int                        first = 1;

						for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
						{
							if (spacing && !first)
							{

								if (docx_paragraph_empty(alloc, output))
									goto end;
							}
							first = 0;

							if (document_to_docx_content_paragraph(alloc, &content_state, paragraph, output)) goto end;
						}
					}
					else
					{
						if (document_to_docx_content_paragraph(alloc, &content_state, paragraph, output))
							goto end;
					}
					content = content_iterator_next(&cit);
				}
				else if (table)
				{
					if (docx_append_table(alloc, table, output))
						goto end;
					table = content_table_iterator_next(&tit);
				}
			}

			if (images)
			{
				content_image_iterator  iit;
				image_t                *image;

				for (image = content_image_iterator_init(&iit, &subpage->content); image != NULL; image = content_image_iterator_next(&iit))
					docx_append_image(alloc, output, image);
			}
		}
	}

	e = 0;
end:

	return e;
}

static int
find_mid(
	const char  *text,
	const char  *begin,
	const char  *end,
	const char **o_begin,
	const char **o_end)
{
	*o_begin = strstr(text, begin);
	if (*o_begin == NULL)
		goto fail;
	*o_begin += strlen(begin);
	*o_end = strstr(*o_begin, end);
	if (*o_end == NULL)
		goto fail;

	return 0;

fail:
	errno = ESRCH;
	return -1;
}

int
extract_docx_content_item(
		extract_alloc_t    *alloc,
		extract_astring_t  *contentss,
		int                 contentss_num,
		images_t           *images,
		const char         *name,
		const char         *text,
		char              **text2)
{
	int               e    = -1;
	extract_astring_t temp = { 0 };

	*text2 = NULL;

	if (0)
	{}
	else if (!strcmp(name, "[Content_Types].xml"))
	{

		const char *begin;
		const char *end;
		const char *insert;
		int it;

		extract_astring_free(alloc, &temp);
		outf("text: %s", text);
		if (find_mid(text, "<Types ", "</Types>", &begin, &end)) goto end;

		insert = begin;
		insert = strchr(insert, '>');
		assert(insert);
		insert += 1;

		if (extract_astring_catl(alloc, &temp, text, insert - text)) goto end;
		outf("images->imagetypes_num=%i", images->imagetypes_num);
		for (it=0; it<images->imagetypes_num; ++it) {
			const char *imagetype = images->imagetypes[it];
			if (extract_astring_cat(alloc, &temp, "<Default Extension=\"")) goto end;
			if (extract_astring_cat(alloc, &temp, imagetype)) goto end;
			if (extract_astring_cat(alloc, &temp, "\" ContentType=\"image/")) goto end;
			if (extract_astring_cat(alloc, &temp, imagetype)) goto end;
			if (extract_astring_cat(alloc, &temp, "\"/>")) goto end;
		}
		if (extract_astring_cat(alloc, &temp, insert)) goto end;
		*text2 = temp.chars;
		extract_astring_init(&temp);
	}
	else if (!strcmp(name, "word/_rels/document.xml.rels"))
	{

		const char *begin;
		const char *end;
		int         j;

		extract_astring_free(alloc, &temp);
		if (find_mid(text, "<Relationships", "</Relationships>", &begin, &end)) goto end;
		if (extract_astring_catl(alloc, &temp, text, end - text)) goto end;
		outf("images.images_num=%i", images->images_num);
		for (j=0; j<images->images_num; ++j) {
			image_t* image = images->images[j];
			if (extract_astring_cat(alloc, &temp, "<Relationship Id=\"")) goto end;
			if (extract_astring_cat(alloc, &temp, image->id)) goto end;
			if (extract_astring_cat(alloc, &temp, "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/")) goto end;
			if (extract_astring_cat(alloc, &temp, image->name)) goto end;
			if (extract_astring_cat(alloc, &temp, "\"/>")) goto end;
		}
		if (extract_astring_cat(alloc, &temp, end)) goto end;
		*text2 = temp.chars;
		extract_astring_init(&temp);
	}
	else if (!strcmp(name, "word/document.xml"))
	{

		if (extract_content_insert(alloc,
				text,
				NULL ,
				"<w:body>",
				"</w:body>",
				contentss,
				contentss_num,
				text2)) goto end;
	}
	else
	{
	*text2 = NULL;
	}

	e = 0;
end:

	if (e)
	{

		extract_free(alloc, text2);

		extract_astring_free(alloc, &temp);
	}
	extract_astring_init(&temp);

	return e;
}

int
extract_docx_write_template(
		extract_alloc_t   *alloc,
		extract_astring_t *contentss,
		int                contentss_num,
		images_t          *images,
		const char        *path_template,
		const char        *path_out,
		int                preserve_dir)
{
	int   e = -1;
	int   i;
	char *path_tempdir = NULL;
	char *path = NULL;
	char *text = NULL;
	char *text2 = NULL;

	assert(path_out);
	assert(path_template);

	if (extract_check_path_shell_safe(path_out))
	{
		outf("path_out is unsafe: %s", path_out);
		goto end;
	}

	outf("images->images_num=%i", images->images_num);
	if (extract_asprintf(alloc, &path_tempdir, "%s.dir", path_out) < 0) goto end;
	if (extract_systemf(alloc, "rm -r '%s' 2>/dev/null", path_tempdir) < 0) goto end;

	if (extract_mkdir(path_tempdir, 0777)) {
		outf("Failed to create directory: %s", path_tempdir);
		goto end;
	}

	outf("Unzipping template document '%s' to tempdir: %s",
		path_template, path_tempdir);
	if (extract_systemf(alloc, "unzip -q -d '%s' '%s'", path_tempdir, path_template))
	{
		outf("Failed to unzip %s into %s",
			path_template, path_tempdir);
		goto end;
	}

	{
		const char *names[] = {
			"word/document.xml",
			"[Content_Types].xml",
			"word/_rels/document.xml.rels",
		};
		int names_num = sizeof(names) / sizeof(names[0]);
		for (i=0; i<names_num; ++i) {
			const char* name = names[i];
			extract_free(alloc, &path);
			extract_free(alloc, &text);
			extract_free(alloc, &text2);
			if (extract_asprintf(alloc, &path, "%s/%s", path_tempdir, name) < 0) goto end;
			if (extract_read_all_path(alloc, path, &text)) goto end;

			if (extract_docx_content_item(alloc,
					contentss,
					contentss_num,
					images,
					name,
					text,
					&text2)) goto end;

			{
				const char *text3 = (text2) ? text2 : text;
				if (extract_write_all(text3, strlen(text3), path)) goto end;
			}
		}
	}

	extract_free(alloc, &path);
	if (extract_asprintf(alloc, &path, "%s/word/media", path_tempdir) < 0) goto end;
	if (extract_mkdir(path, 0777)) goto end;

	for (i=0; i<images->images_num; ++i) {
		image_t* image = images->images[i];
		extract_free(alloc, &path);
		if (extract_asprintf(alloc, &path, "%s/word/media/%s", path_tempdir, image->name) < 0) goto end;
		if (extract_write_all(image->data, image->data_size, path)) goto end;
	}

	outf("Zipping tempdir to create %s", path_out);
	{
		const char *path_out_leaf = strrchr(path_out, '/');
		if (!path_out_leaf) path_out_leaf = path_out;
		if (extract_systemf(alloc, "cd '%s' && zip -q -r -D '../%s' .", path_tempdir, path_out_leaf))
		{
			outf("Zip command failed to convert '%s' directory into output file: %s",
				path_tempdir, path_out);
			goto end;
		}
	}

	if (!preserve_dir) {
		if (extract_remove_directory(alloc, path_tempdir)) goto end;
	}

	e = 0;
end:

	outf("e=%i", e);
	extract_free(alloc, &path_tempdir);
	extract_free(alloc, &path);
	extract_free(alloc, &text);
	extract_free(alloc, &text2);

	if (e)
	{
		outf("Failed to create %s", path_out);
	}

	return e;
}

const docx_template_item_t docx_template_items[] =
{
    {
        "[Content_Types].xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
                "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
                "<Override PartName=\"/word/settings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml\"/>"
                "<Override PartName=\"/word/webSettings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.webSettings+xml\"/>"
                "<Override PartName=\"/word/fontTable.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.fontTable+xml\"/>"
                "<Override PartName=\"/word/theme/theme1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.theme+xml\"/>"
                "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
                "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/></Types>"
    },

    {
        "_rels/.rels",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
                "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
                "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>"
    },

    {
        "docProps/app.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
                "<Template>Normal.dotm</Template>"
                "<TotalTime>3</TotalTime>"
                "<Pages>1</Pages>"
                "<Words>2</Words>"
                "<Characters>18</Characters>"
                "<Application>Microsoft Office Word</Application>"
                "<DocSecurity>0</DocSecurity>"
                "<Lines>1</Lines>"
                "<Paragraphs>1</Paragraphs>"
                "<ScaleCrop>false</ScaleCrop>"
                "<Company></Company>"
                "<LinksUpToDate>false</LinksUpToDate>"
                "<CharactersWithSpaces>19</CharactersWithSpaces>"
                "<SharedDoc>false</SharedDoc>"
                "<HyperlinksChanged>false</HyperlinksChanged>"
                "<AppVersion>16.0000</AppVersion></Properties>"
    },

    {
        "docProps/core.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
                "<dc:title></dc:title>"
                "<dc:subject></dc:subject>"
                "<dc:creator></dc:creator>"
                "<cp:keywords></cp:keywords>"
                "<dc:description></dc:description>"
                "<cp:lastModifiedBy></cp:lastModifiedBy>"
                "<cp:revision>1</cp:revision>"
                "<dcterms:created xsi:type=\"dcterms:W3CDTF\">2020-09-25T17:04:00Z</dcterms:created>"
                "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">2020-09-25T17:07:00Z</dcterms:modified></cp:coreProperties>"
    },

    {
        "word/document.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<w:document xmlns:wpc=\"http://schemas.microsoft.com/office/word/2010/wordprocessingCanvas\" xmlns:cx=\"http://schemas.microsoft.com/office/drawing/2014/chartex\" xmlns:cx1=\"http://schemas.microsoft.com/office/drawing/2015/9/8/chartex\" xmlns:cx2=\"http://schemas.microsoft.com/office/drawing/2015/10/21/chartex\" xmlns:cx3=\"http://schemas.microsoft.com/office/drawing/2016/5/9/chartex\" xmlns:cx4=\"http://schemas.microsoft.com/office/drawing/2016/5/10/chartex\" xmlns:cx5=\"http://schemas.microsoft.com/office/drawing/2016/5/11/chartex\" xmlns:cx6=\"http://schemas.microsoft.com/office/drawing/2016/5/12/chartex\" xmlns:cx7=\"http://schemas.microsoft.com/office/drawing/2016/5/13/chartex\" xmlns:cx8=\"http://schemas.microsoft.com/office/drawing/2016/5/14/chartex\" xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" xmlns:aink=\"http://schemas.microsoft.com/office/drawing/2016/ink\" xmlns:am3d=\"http://schemas.microsoft.com/office/drawing/2017/model3d\" xmlns:o=\"urn:schemas-microsoft-com:office:office\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\" xmlns:v=\"urn:schemas-microsoft-com:vml\" xmlns:wp14=\"http://schemas.microsoft.com/office/word/2010/wordprocessingDrawing\" xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" xmlns:w10=\"urn:schemas-microsoft-com:office:word\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" xmlns:w15=\"http://schemas.microsoft.com/office/word/2012/wordml\" xmlns:w16cex=\"http://schemas.microsoft.com/office/word/2018/wordml/cex\" xmlns:w16cid=\"http://schemas.microsoft.com/office/word/2016/wordml/cid\" xmlns:w16=\"http://schemas.microsoft.com/office/word/2018/wordml\" xmlns:w16se=\"http://schemas.microsoft.com/office/word/2015/wordml/symex\" xmlns:wpg=\"http://schemas.microsoft.com/office/word/2010/wordprocessingGroup\" xmlns:wpi=\"http://schemas.microsoft.com/office/word/2010/wordprocessingInk\" xmlns:wne=\"http://schemas.microsoft.com/office/word/2006/wordml\" xmlns:wps=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\" mc:Ignorable=\"w14 w15 w16se w16cid w16 w16cex wp14\">"
                "<w:body>"
                "<w:p w14:paraId=\"7C58A6F1\" w14:textId=\"3E2CAE3F\" w:rsidR=\"00610D78\" w:rsidRDefault=\"007F4427\">"
                "<w:r>"
                "<w:t>Hello world</w:t></w:r></w:p>"
                "<w:p w14:paraId=\"53256C58\" w14:textId=\"13022069\" w:rsidR=\"007F4427\" w:rsidRDefault=\"007F4427\">"
                "<w:r>"
                "<w:rPr>"
                "<w:noProof/></w:rPr>"
                "<mc:AlternateContent>"
                "<mc:Choice Requires=\"wps\">"
                "<w:drawing>"
                "<wp:anchor distT=\"0\" distB=\"0\" distL=\"114300\" distR=\"114300\" simplePos=\"0\" relativeHeight=\"251659264\" behindDoc=\"0\" locked=\"0\" layoutInCell=\"1\" allowOverlap=\"1\" wp14:anchorId=\"53A210D1\" wp14:editId=\"2B7E8016\">"
                "<wp:simplePos x=\"0\" y=\"0\"/>"
                "<wp:positionH relativeFrom=\"column\">"
                "<wp:posOffset>904875</wp:posOffset></wp:positionH>"
                "<wp:positionV relativeFrom=\"paragraph\">"
                "<wp:posOffset>619125</wp:posOffset></wp:positionV>"
                "<wp:extent cx=\"3228975\" cy=\"2286000\"/>"
                "<wp:effectExtent l=\"381000\" t=\"723900\" r=\"371475\" b=\"723900\"/>"
                "<wp:wrapNone/>"
                "<wp:docPr id=\"1\" name=\"Text Box 1\"/>"
                "<wp:cNvGraphicFramePr/>"
                "<a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
                "<a:graphicData uri=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\">"
                "<wps:wsp>"
                "<wps:cNvSpPr txBox=\"1\"/>"
                "<wps:spPr>"
                "<a:xfrm rot=\"19547867\">"
                "<a:off x=\"0\" y=\"0\"/>"
                "<a:ext cx=\"3228975\" cy=\"2286000\"/></a:xfrm>"
                "<a:prstGeom prst=\"rect\">"
                "<a:avLst/></a:prstGeom>"
                "<a:solidFill>"
                "<a:schemeClr val=\"lt1\"/></a:solidFill>"
                "<a:ln w=\"6350\">"
                "<a:solidFill>"
                "<a:prstClr val=\"black\"/></a:solidFill></a:ln></wps:spPr>"
                "<wps:txbx>"
                "<w:txbxContent>"
                "<w:p w14:paraId=\"31597E69\" w14:textId=\"2903B1F1\" w:rsidR=\"007F4427\" w:rsidRDefault=\"007F4427\">"
                "<w:r>"
                "<w:t>Hello. Qwerty. World</w:t></w:r></w:p>"
                "<w:p w14:paraId=\"0BD8A985\" w14:textId=\"1BFB8248\" w:rsidR=\"007F4427\" w:rsidRDefault=\"007F4427\">"
                "<w:proofErr w:type=\"spellStart\"/>"
                "<w:r>"
                "<w:t>mupdf</w:t></w:r>"
                "<w:proofErr w:type=\"spellEnd\"/></w:p></w:txbxContent></wps:txbx>"
                "<wps:bodyPr rot=\"0\" spcFirstLastPara=\"0\" vertOverflow=\"overflow\" horzOverflow=\"overflow\" vert=\"horz\" wrap=\"square\" lIns=\"91440\" tIns=\"45720\" rIns=\"91440\" bIns=\"45720\" numCol=\"1\" spcCol=\"0\" rtlCol=\"0\" fromWordArt=\"0\" anchor=\"t\" anchorCtr=\"0\" forceAA=\"0\" compatLnSpc=\"1\">"
                "<a:prstTxWarp prst=\"textNoShape\">"
                "<a:avLst/></a:prstTxWarp>"
                "<a:noAutofit/></wps:bodyPr></wps:wsp></a:graphicData></a:graphic></wp:anchor></w:drawing></mc:Choice>"
                "<mc:Fallback>"
                "<w:pict>"
                "<v:shapetype w14:anchorId=\"53A210D1\" id=\"_x0000_t202\" coordsize=\"21600,21600\" o:spt=\"202\" path=\"m,l,21600r21600,l21600,xe\">"
                "<v:stroke joinstyle=\"miter\"/>"
                "<v:path gradientshapeok=\"t\" o:connecttype=\"rect\"/></v:shapetype>"
                "<v:shape id=\"Text Box 1\" o:spid=\"_x0000_s1026\" type=\"#_x0000_t202\" style=\"position:absolute;margin-left:71.25pt;margin-top:48.75pt;width:254.25pt;height:180pt;rotation:-2241476fd;z-index:251659264;visibility:visible;mso-wrap-style:square;mso-wrap-distance-left:9pt;mso-wrap-distance-top:0;mso-wrap-distance-right:9pt;mso-wrap-distance-bottom:0;mso-position-horizontal:absolute;mso-position-horizontal-relative:text;mso-position-vertical:absolute;mso-position-vertical-relative:text;v-text-anchor:top\" o:gfxdata=\"UEsDBBQABgAIAAAAIQC2gziS/gAAAOEBAAATAAAAW0NvbnRlbnRfVHlwZXNdLnhtbJSRQU7DMBBF&#xA;90jcwfIWJU67QAgl6YK0S0CoHGBkTxKLZGx5TGhvj5O2G0SRWNoz/78nu9wcxkFMGNg6quQqL6RA&#xA;0s5Y6ir5vt9lD1JwBDIwOMJKHpHlpr69KfdHjyxSmriSfYz+USnWPY7AufNIadK6MEJMx9ApD/oD&#xA;OlTrorhX2lFEilmcO2RdNtjC5xDF9pCuTyYBB5bi6bQ4syoJ3g9WQ0ymaiLzg5KdCXlKLjvcW893&#xA;SUOqXwnz5DrgnHtJTxOsQfEKIT7DmDSUCaxw7Rqn8787ZsmRM9e2VmPeBN4uqYvTtW7jvijg9N/y&#xA;JsXecLq0q+WD6m8AAAD//wMAUEsDBBQABgAIAAAAIQA4/SH/1gAAAJQBAAALAAAAX3JlbHMvLnJl&#xA;bHOkkMFqwzAMhu+DvYPRfXGawxijTi+j0GvpHsDYimMaW0Yy2fr2M4PBMnrbUb/Q94l/f/hMi1qR&#xA;JVI2sOt6UJgd+ZiDgffL8ekFlFSbvV0oo4EbChzGx4f9GRdb25HMsYhqlCwG5lrLq9biZkxWOiqY&#xA;22YiTra2kYMu1l1tQD30/bPm3wwYN0x18gb45AdQl1tp5j/sFB2T0FQ7R0nTNEV3j6o9feQzro1i&#xA;OWA14Fm+Q8a1a8+Bvu/d/dMb2JY5uiPbhG/ktn4cqGU/er3pcvwCAAD//wMAUEsDBBQABgAIAAAA&#xA;IQDQg5pQVgIAALEEAAAOAAAAZHJzL2Uyb0RvYy54bWysVE1v2zAMvQ/YfxB0X+2k+WiDOEXWosOA&#xA;oi3QDj0rstwYk0VNUmJ3v35PipMl3U7DLgJFPj+Rj6TnV12j2VY5X5Mp+OAs50wZSWVtXgv+7fn2&#xA;0wVnPghTCk1GFfxNeX61+Phh3tqZGtKadKkcA4nxs9YWfB2CnWWZl2vVCH9GVhkEK3KNCLi616x0&#xA;ogV7o7Nhnk+yllxpHUnlPbw3uyBfJP6qUjI8VJVXgemCI7eQTpfOVTyzxVzMXp2w61r2aYh/yKIR&#xA;tcGjB6obEQTbuPoPqqaWjjxV4UxSk1FV1VKlGlDNIH9XzdNaWJVqgTjeHmTy/49W3m8fHatL9I4z&#xA;Ixq06Fl1gX2mjg2iOq31M4CeLGChgzsie7+HMxbdVa5hjiDu4HI8ml5MpkkLVMcAh+xvB6kjt4Tz&#xA;fDi8uJyOOZOIwZ7keWpGtmOLrNb58EVRw6JRcIdeJlqxvfMBGQC6h0S4J12Xt7XW6RLnR11rx7YC&#xA;ndch5YwvTlDasLbgk/NxnohPYpH68P1KC/k9Vn3KgJs2cEaNdlpEK3SrrhdoReUbdEvSQAZv5W0N&#xA;3jvhw6NwGDQ4sTzhAUelCclQb3G2Jvfzb/6IR/8R5azF4Bbc/9gIpzjTXw0m43IwGsVJT5fReDrE&#xA;xR1HVscRs2muCQqh+8gumREf9N6sHDUv2LFlfBUhYSTeLnjYm9dht07YUamWywTCbFsR7syTlZF6&#xA;383n7kU42/czYBTuaT/iYvaurTts/NLQchOoqlPPo8A7VXvdsRepLf0Ox8U7vifU7z/N4hcAAAD/&#xA;/wMAUEsDBBQABgAIAAAAIQBh17L63wAAAAoBAAAPAAAAZHJzL2Rvd25yZXYueG1sTI9BT4NAEIXv&#xA;Jv6HzZh4s0ubgpayNIboSW3Syg9Y2BGI7CyyS0v99Y4nPU3ezMub72W72fbihKPvHClYLiIQSLUz&#xA;HTUKyvfnuwcQPmgyuneECi7oYZdfX2U6Ne5MBzwdQyM4hHyqFbQhDKmUvm7Rar9wAxLfPtxodWA5&#xA;NtKM+szhtperKEqk1R3xh1YPWLRYfx4nq8APVfz9VQxPb+WUNC+vZbGPDhelbm/mxy2IgHP4M8Mv&#xA;PqNDzkyVm8h40bNer2K2Ktjc82RDEi+5XKVgHfNG5pn8XyH/AQAA//8DAFBLAQItABQABgAIAAAA&#xA;IQC2gziS/gAAAOEBAAATAAAAAAAAAAAAAAAAAAAAAABbQ29udGVudF9UeXBlc10ueG1sUEsBAi0A&#xA;FAAGAAgAAAAhADj9If/WAAAAlAEAAAsAAAAAAAAAAAAAAAAALwEAAF9yZWxzLy5yZWxzUEsBAi0A&#xA;FAAGAAgAAAAhANCDmlBWAgAAsQQAAA4AAAAAAAAAAAAAAAAALgIAAGRycy9lMm9Eb2MueG1sUEsB&#xA;Ai0AFAAGAAgAAAAhAGHXsvrfAAAACgEAAA8AAAAAAAAAAAAAAAAAsAQAAGRycy9kb3ducmV2Lnht&#xA;bFBLBQYAAAAABAAEAPMAAAC8BQAAAAA=&#xA;\" fillcolor=\"white [3201]\" strokeweight=\".5pt\">"
                "<v:textbox>"
                "<w:txbxContent>"
                "<w:p w14:paraId=\"31597E69\" w14:textId=\"2903B1F1\" w:rsidR=\"007F4427\" w:rsidRDefault=\"007F4427\">"
                "<w:r>"
                "<w:t>Hello. Qwerty. World</w:t></w:r></w:p>"
                "<w:p w14:paraId=\"0BD8A985\" w14:textId=\"1BFB8248\" w:rsidR=\"007F4427\" w:rsidRDefault=\"007F4427\">"
                "<w:proofErr w:type=\"spellStart\"/>"
                "<w:r>"
                "<w:t>mupdf</w:t></w:r>"
                "<w:proofErr w:type=\"spellEnd\"/></w:p></w:txbxContent></v:textbox></v:shape></w:pict></mc:Fallback></mc:AlternateContent></w:r>"
                "<w:r>"
                "<w:t>qwerty</w:t></w:r></w:p>"
                "<w:sectPr w:rsidR=\"007F4427\">"
                "<w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
                "<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\" w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/>"
                "<w:cols w:space=\"708\"/>"
                "<w:docGrid w:linePitch=\"360\"/></w:sectPr></w:body></w:document>"
    },

    {
        "word/fontTable.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<w:fonts xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" xmlns:w15=\"http://schemas.microsoft.com/office/word/2012/wordml\" xmlns:w16cex=\"http://schemas.microsoft.com/office/word/2018/wordml/cex\" xmlns:w16cid=\"http://schemas.microsoft.com/office/word/2016/wordml/cid\" xmlns:w16=\"http://schemas.microsoft.com/office/word/2018/wordml\" xmlns:w16se=\"http://schemas.microsoft.com/office/word/2015/wordml/symex\" mc:Ignorable=\"w14 w15 w16se w16cid w16 w16cex\">"
                "<w:font w:name=\"Calibri\">"
                "<w:panose1 w:val=\"020F0502020204030204\"/>"
                "<w:charset w:val=\"00\"/>"
                "<w:family w:val=\"swiss\"/>"
                "<w:pitch w:val=\"variable\"/>"
                "<w:sig w:usb0=\"E4002EFF\" w:usb1=\"C000247B\" w:usb2=\"00000009\" w:usb3=\"00000000\" w:csb0=\"000001FF\" w:csb1=\"00000000\"/></w:font>"
                "<w:font w:name=\"Times New Roman\">"
                "<w:panose1 w:val=\"02020603050405020304\"/>"
                "<w:charset w:val=\"00\"/>"
                "<w:family w:val=\"roman\"/>"
                "<w:pitch w:val=\"variable\"/>"
                "<w:sig w:usb0=\"E0002EFF\" w:usb1=\"C000785B\" w:usb2=\"00000009\" w:usb3=\"00000000\" w:csb0=\"000001FF\" w:csb1=\"00000000\"/></w:font>"
                "<w:font w:name=\"Calibri Light\">"
                "<w:panose1 w:val=\"020F0302020204030204\"/>"
                "<w:charset w:val=\"00\"/>"
                "<w:family w:val=\"swiss\"/>"
                "<w:pitch w:val=\"variable\"/>"
                "<w:sig w:usb0=\"E4002EFF\" w:usb1=\"C000247B\" w:usb2=\"00000009\" w:usb3=\"00000000\" w:csb0=\"000001FF\" w:csb1=\"00000000\"/></w:font></w:fonts>"
    },

    {
        "word/settings.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<w:settings xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" xmlns:o=\"urn:schemas-microsoft-com:office:office\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\" xmlns:v=\"urn:schemas-microsoft-com:vml\" xmlns:w10=\"urn:schemas-microsoft-com:office:word\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" xmlns:w15=\"http://schemas.microsoft.com/office/word/2012/wordml\" xmlns:w16cex=\"http://schemas.microsoft.com/office/word/2018/wordml/cex\" xmlns:w16cid=\"http://schemas.microsoft.com/office/word/2016/wordml/cid\" xmlns:w16=\"http://schemas.microsoft.com/office/word/2018/wordml\" xmlns:w16se=\"http://schemas.microsoft.com/office/word/2015/wordml/symex\" xmlns:sl=\"http://schemas.openxmlformats.org/schemaLibrary/2006/main\" mc:Ignorable=\"w14 w15 w16se w16cid w16 w16cex\">"
                "<w:zoom w:percent=\"100\"/>"
                "<w:proofState w:spelling=\"clean\" w:grammar=\"clean\"/>"
                "<w:defaultTabStop w:val=\"720\"/>"
                "<w:characterSpacingControl w:val=\"doNotCompress\"/>"
                "<w:compat>"
                "<w:compatSetting w:name=\"compatibilityMode\" w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"15\"/>"
                "<w:compatSetting w:name=\"overrideTableStyleFontSizeAndJustification\" w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"1\"/>"
                "<w:compatSetting w:name=\"enableOpenTypeFeatures\" w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"1\"/>"
                "<w:compatSetting w:name=\"doNotFlipMirrorIndents\" w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"1\"/>"
                "<w:compatSetting w:name=\"differentiateMultirowTableHeaders\" w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"1\"/>"
                "<w:compatSetting w:name=\"useWord2013TrackBottomHyphenation\" w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"0\"/></w:compat>"
                "<w:rsids>"
                "<w:rsidRoot w:val=\"007F4427\"/>"
                "<w:rsid w:val=\"00255448\"/>"
                "<w:rsid w:val=\"007F4427\"/></w:rsids>"
                "<m:mathPr>"
                "<m:mathFont m:val=\"Cambria Math\"/>"
                "<m:brkBin m:val=\"before\"/>"
                "<m:brkBinSub m:val=\"--\"/>"
                "<m:smallFrac m:val=\"0\"/>"
                "<m:dispDef/>"
                "<m:lMargin m:val=\"0\"/>"
                "<m:rMargin m:val=\"0\"/>"
                "<m:defJc m:val=\"centerGroup\"/>"
                "<m:wrapIndent m:val=\"1440\"/>"
                "<m:intLim m:val=\"subSup\"/>"
                "<m:naryLim m:val=\"undOvr\"/></m:mathPr>"
                "<w:themeFontLang w:val=\"en-GB\"/>"
                "<w:clrSchemeMapping w:bg1=\"light1\" w:t1=\"dark1\" w:bg2=\"light2\" w:t2=\"dark2\" w:accent1=\"accent1\" w:accent2=\"accent2\" w:accent3=\"accent3\" w:accent4=\"accent4\" w:accent5=\"accent5\" w:accent6=\"accent6\" w:hyperlink=\"hyperlink\" w:followedHyperlink=\"followedHyperlink\"/>"
                "<w:shapeDefaults>"
                "<o:shapedefaults v:ext=\"edit\" spidmax=\"1026\"/>"
                "<o:shapelayout v:ext=\"edit\">"
                "<o:idmap v:ext=\"edit\" data=\"1\"/></o:shapelayout></w:shapeDefaults>"
                "<w:decimalSymbol w:val=\".\"/>"
                "<w:listSeparator w:val=\",\"/>"
                "<w14:docId w14:val=\"32E52EF8\"/>"
                "<w15:chartTrackingRefBased/>"
                "<w15:docId w15:val=\"{A10F59F7-497D-44D4-A338-47719734E7A0}\"/></w:settings>"
    },

    {
        "word/styles.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<w:styles xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" xmlns:w15=\"http://schemas.microsoft.com/office/word/2012/wordml\" xmlns:w16cex=\"http://schemas.microsoft.com/office/word/2018/wordml/cex\" xmlns:w16cid=\"http://schemas.microsoft.com/office/word/2016/wordml/cid\" xmlns:w16=\"http://schemas.microsoft.com/office/word/2018/wordml\" xmlns:w16se=\"http://schemas.microsoft.com/office/word/2015/wordml/symex\" mc:Ignorable=\"w14 w15 w16se w16cid w16 w16cex\">"
                "<w:docDefaults>"
                "<w:rPrDefault>"
                "<w:rPr>"
                "<w:rFonts w:asciiTheme=\"minorHAnsi\" w:eastAsiaTheme=\"minorHAnsi\" w:hAnsiTheme=\"minorHAnsi\" w:cstheme=\"minorBidi\"/>"
                "<w:sz w:val=\"22\"/>"
                "<w:szCs w:val=\"22\"/>"
                "<w:lang w:val=\"en-GB\" w:eastAsia=\"en-US\" w:bidi=\"ar-SA\"/></w:rPr></w:rPrDefault>"
                "<w:pPrDefault>"
                "<w:pPr>"
                "<w:spacing w:after=\"160\" w:line=\"259\" w:lineRule=\"auto\"/></w:pPr></w:pPrDefault></w:docDefaults>"
                "<w:latentStyles w:defLockedState=\"0\" w:defUIPriority=\"99\" w:defSemiHidden=\"0\" w:defUnhideWhenUsed=\"0\" w:defQFormat=\"0\" w:count=\"376\">"
                "<w:lsdException w:name=\"Normal\" w:uiPriority=\"0\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 1\" w:uiPriority=\"9\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 2\" w:semiHidden=\"1\" w:uiPriority=\"9\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 3\" w:semiHidden=\"1\" w:uiPriority=\"9\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 4\" w:semiHidden=\"1\" w:uiPriority=\"9\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 5\" w:semiHidden=\"1\" w:uiPriority=\"9\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 6\" w:semiHidden=\"1\" w:uiPriority=\"9\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 7\" w:semiHidden=\"1\" w:uiPriority=\"9\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 8\" w:semiHidden=\"1\" w:uiPriority=\"9\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"heading 9\" w:semiHidden=\"1\" w:uiPriority=\"9\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"index 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index 5\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index 6\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index 7\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index 8\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index 9\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 1\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 2\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 3\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 4\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 5\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 6\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 7\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 8\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toc 9\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Normal Indent\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"footnote text\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"annotation text\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"header\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"footer\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"index heading\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"caption\" w:semiHidden=\"1\" w:uiPriority=\"35\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"table of figures\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"envelope address\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"envelope return\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"footnote reference\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"annotation reference\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"line number\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"page number\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"endnote reference\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"endnote text\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"table of authorities\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"macro\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"toa heading\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Bullet\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Number\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List 5\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Bullet 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Bullet 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Bullet 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Bullet 5\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Number 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Number 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Number 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Number 5\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Title\" w:uiPriority=\"10\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Closing\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Signature\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Default Paragraph Font\" w:semiHidden=\"1\" w:uiPriority=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Body Text\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Body Text Indent\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Continue\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Continue 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Continue 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Continue 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"List Continue 5\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Message Header\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Subtitle\" w:uiPriority=\"11\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Salutation\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Date\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Body Text First Indent\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Body Text First Indent 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Note Heading\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Body Text 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Body Text 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Body Text Indent 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Body Text Indent 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Block Text\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Hyperlink\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"FollowedHyperlink\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Strong\" w:uiPriority=\"22\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Emphasis\" w:uiPriority=\"20\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Document Map\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Plain Text\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"E-mail Signature\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Top of Form\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Bottom of Form\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Normal (Web)\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Acronym\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Address\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Cite\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Code\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Definition\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Keyboard\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Preformatted\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Sample\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Typewriter\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"HTML Variable\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Normal Table\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"annotation subject\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"No List\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Outline List 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Outline List 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Outline List 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Simple 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Simple 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Simple 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Classic 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Classic 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Classic 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Classic 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Colorful 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Colorful 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Colorful 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Columns 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Columns 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Columns 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Columns 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Columns 5\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid 5\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid 6\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid 7\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid 8\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table List 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table List 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table List 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table List 4\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table List 5\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table List 6\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table List 7\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table List 8\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table 3D effects 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table 3D effects 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table 3D effects 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Contemporary\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Elegant\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Professional\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Subtle 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Subtle 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Web 1\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Web 2\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Web 3\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Balloon Text\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Table Grid\" w:uiPriority=\"39\"/>"
                "<w:lsdException w:name=\"Table Theme\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Placeholder Text\" w:semiHidden=\"1\"/>"
                "<w:lsdException w:name=\"No Spacing\" w:uiPriority=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Light Shading\" w:uiPriority=\"60\"/>"
                "<w:lsdException w:name=\"Light List\" w:uiPriority=\"61\"/>"
                "<w:lsdException w:name=\"Light Grid\" w:uiPriority=\"62\"/>"
                "<w:lsdException w:name=\"Medium Shading 1\" w:uiPriority=\"63\"/>"
                "<w:lsdException w:name=\"Medium Shading 2\" w:uiPriority=\"64\"/>"
                "<w:lsdException w:name=\"Medium List 1\" w:uiPriority=\"65\"/>"
                "<w:lsdException w:name=\"Medium List 2\" w:uiPriority=\"66\"/>"
                "<w:lsdException w:name=\"Medium Grid 1\" w:uiPriority=\"67\"/>"
                "<w:lsdException w:name=\"Medium Grid 2\" w:uiPriority=\"68\"/>"
                "<w:lsdException w:name=\"Medium Grid 3\" w:uiPriority=\"69\"/>"
                "<w:lsdException w:name=\"Dark List\" w:uiPriority=\"70\"/>"
                "<w:lsdException w:name=\"Colorful Shading\" w:uiPriority=\"71\"/>"
                "<w:lsdException w:name=\"Colorful List\" w:uiPriority=\"72\"/>"
                "<w:lsdException w:name=\"Colorful Grid\" w:uiPriority=\"73\"/>"
                "<w:lsdException w:name=\"Light Shading Accent 1\" w:uiPriority=\"60\"/>"
                "<w:lsdException w:name=\"Light List Accent 1\" w:uiPriority=\"61\"/>"
                "<w:lsdException w:name=\"Light Grid Accent 1\" w:uiPriority=\"62\"/>"
                "<w:lsdException w:name=\"Medium Shading 1 Accent 1\" w:uiPriority=\"63\"/>"
                "<w:lsdException w:name=\"Medium Shading 2 Accent 1\" w:uiPriority=\"64\"/>"
                "<w:lsdException w:name=\"Medium List 1 Accent 1\" w:uiPriority=\"65\"/>"
                "<w:lsdException w:name=\"Revision\" w:semiHidden=\"1\"/>"
                "<w:lsdException w:name=\"List Paragraph\" w:uiPriority=\"34\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Quote\" w:uiPriority=\"29\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Intense Quote\" w:uiPriority=\"30\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Medium List 2 Accent 1\" w:uiPriority=\"66\"/>"
                "<w:lsdException w:name=\"Medium Grid 1 Accent 1\" w:uiPriority=\"67\"/>"
                "<w:lsdException w:name=\"Medium Grid 2 Accent 1\" w:uiPriority=\"68\"/>"
                "<w:lsdException w:name=\"Medium Grid 3 Accent 1\" w:uiPriority=\"69\"/>"
                "<w:lsdException w:name=\"Dark List Accent 1\" w:uiPriority=\"70\"/>"
                "<w:lsdException w:name=\"Colorful Shading Accent 1\" w:uiPriority=\"71\"/>"
                "<w:lsdException w:name=\"Colorful List Accent 1\" w:uiPriority=\"72\"/>"
                "<w:lsdException w:name=\"Colorful Grid Accent 1\" w:uiPriority=\"73\"/>"
                "<w:lsdException w:name=\"Light Shading Accent 2\" w:uiPriority=\"60\"/>"
                "<w:lsdException w:name=\"Light List Accent 2\" w:uiPriority=\"61\"/>"
                "<w:lsdException w:name=\"Light Grid Accent 2\" w:uiPriority=\"62\"/>"
                "<w:lsdException w:name=\"Medium Shading 1 Accent 2\" w:uiPriority=\"63\"/>"
                "<w:lsdException w:name=\"Medium Shading 2 Accent 2\" w:uiPriority=\"64\"/>"
                "<w:lsdException w:name=\"Medium List 1 Accent 2\" w:uiPriority=\"65\"/>"
                "<w:lsdException w:name=\"Medium List 2 Accent 2\" w:uiPriority=\"66\"/>"
                "<w:lsdException w:name=\"Medium Grid 1 Accent 2\" w:uiPriority=\"67\"/>"
                "<w:lsdException w:name=\"Medium Grid 2 Accent 2\" w:uiPriority=\"68\"/>"
                "<w:lsdException w:name=\"Medium Grid 3 Accent 2\" w:uiPriority=\"69\"/>"
                "<w:lsdException w:name=\"Dark List Accent 2\" w:uiPriority=\"70\"/>"
                "<w:lsdException w:name=\"Colorful Shading Accent 2\" w:uiPriority=\"71\"/>"
                "<w:lsdException w:name=\"Colorful List Accent 2\" w:uiPriority=\"72\"/>"
                "<w:lsdException w:name=\"Colorful Grid Accent 2\" w:uiPriority=\"73\"/>"
                "<w:lsdException w:name=\"Light Shading Accent 3\" w:uiPriority=\"60\"/>"
                "<w:lsdException w:name=\"Light List Accent 3\" w:uiPriority=\"61\"/>"
                "<w:lsdException w:name=\"Light Grid Accent 3\" w:uiPriority=\"62\"/>"
                "<w:lsdException w:name=\"Medium Shading 1 Accent 3\" w:uiPriority=\"63\"/>"
                "<w:lsdException w:name=\"Medium Shading 2 Accent 3\" w:uiPriority=\"64\"/>"
                "<w:lsdException w:name=\"Medium List 1 Accent 3\" w:uiPriority=\"65\"/>"
                "<w:lsdException w:name=\"Medium List 2 Accent 3\" w:uiPriority=\"66\"/>"
                "<w:lsdException w:name=\"Medium Grid 1 Accent 3\" w:uiPriority=\"67\"/>"
                "<w:lsdException w:name=\"Medium Grid 2 Accent 3\" w:uiPriority=\"68\"/>"
                "<w:lsdException w:name=\"Medium Grid 3 Accent 3\" w:uiPriority=\"69\"/>"
                "<w:lsdException w:name=\"Dark List Accent 3\" w:uiPriority=\"70\"/>"
                "<w:lsdException w:name=\"Colorful Shading Accent 3\" w:uiPriority=\"71\"/>"
                "<w:lsdException w:name=\"Colorful List Accent 3\" w:uiPriority=\"72\"/>"
                "<w:lsdException w:name=\"Colorful Grid Accent 3\" w:uiPriority=\"73\"/>"
                "<w:lsdException w:name=\"Light Shading Accent 4\" w:uiPriority=\"60\"/>"
                "<w:lsdException w:name=\"Light List Accent 4\" w:uiPriority=\"61\"/>"
                "<w:lsdException w:name=\"Light Grid Accent 4\" w:uiPriority=\"62\"/>"
                "<w:lsdException w:name=\"Medium Shading 1 Accent 4\" w:uiPriority=\"63\"/>"
                "<w:lsdException w:name=\"Medium Shading 2 Accent 4\" w:uiPriority=\"64\"/>"
                "<w:lsdException w:name=\"Medium List 1 Accent 4\" w:uiPriority=\"65\"/>"
                "<w:lsdException w:name=\"Medium List 2 Accent 4\" w:uiPriority=\"66\"/>"
                "<w:lsdException w:name=\"Medium Grid 1 Accent 4\" w:uiPriority=\"67\"/>"
                "<w:lsdException w:name=\"Medium Grid 2 Accent 4\" w:uiPriority=\"68\"/>"
                "<w:lsdException w:name=\"Medium Grid 3 Accent 4\" w:uiPriority=\"69\"/>"
                "<w:lsdException w:name=\"Dark List Accent 4\" w:uiPriority=\"70\"/>"
                "<w:lsdException w:name=\"Colorful Shading Accent 4\" w:uiPriority=\"71\"/>"
                "<w:lsdException w:name=\"Colorful List Accent 4\" w:uiPriority=\"72\"/>"
                "<w:lsdException w:name=\"Colorful Grid Accent 4\" w:uiPriority=\"73\"/>"
                "<w:lsdException w:name=\"Light Shading Accent 5\" w:uiPriority=\"60\"/>"
                "<w:lsdException w:name=\"Light List Accent 5\" w:uiPriority=\"61\"/>"
                "<w:lsdException w:name=\"Light Grid Accent 5\" w:uiPriority=\"62\"/>"
                "<w:lsdException w:name=\"Medium Shading 1 Accent 5\" w:uiPriority=\"63\"/>"
                "<w:lsdException w:name=\"Medium Shading 2 Accent 5\" w:uiPriority=\"64\"/>"
                "<w:lsdException w:name=\"Medium List 1 Accent 5\" w:uiPriority=\"65\"/>"
                "<w:lsdException w:name=\"Medium List 2 Accent 5\" w:uiPriority=\"66\"/>"
                "<w:lsdException w:name=\"Medium Grid 1 Accent 5\" w:uiPriority=\"67\"/>"
                "<w:lsdException w:name=\"Medium Grid 2 Accent 5\" w:uiPriority=\"68\"/>"
                "<w:lsdException w:name=\"Medium Grid 3 Accent 5\" w:uiPriority=\"69\"/>"
                "<w:lsdException w:name=\"Dark List Accent 5\" w:uiPriority=\"70\"/>"
                "<w:lsdException w:name=\"Colorful Shading Accent 5\" w:uiPriority=\"71\"/>"
                "<w:lsdException w:name=\"Colorful List Accent 5\" w:uiPriority=\"72\"/>"
                "<w:lsdException w:name=\"Colorful Grid Accent 5\" w:uiPriority=\"73\"/>"
                "<w:lsdException w:name=\"Light Shading Accent 6\" w:uiPriority=\"60\"/>"
                "<w:lsdException w:name=\"Light List Accent 6\" w:uiPriority=\"61\"/>"
                "<w:lsdException w:name=\"Light Grid Accent 6\" w:uiPriority=\"62\"/>"
                "<w:lsdException w:name=\"Medium Shading 1 Accent 6\" w:uiPriority=\"63\"/>"
                "<w:lsdException w:name=\"Medium Shading 2 Accent 6\" w:uiPriority=\"64\"/>"
                "<w:lsdException w:name=\"Medium List 1 Accent 6\" w:uiPriority=\"65\"/>"
                "<w:lsdException w:name=\"Medium List 2 Accent 6\" w:uiPriority=\"66\"/>"
                "<w:lsdException w:name=\"Medium Grid 1 Accent 6\" w:uiPriority=\"67\"/>"
                "<w:lsdException w:name=\"Medium Grid 2 Accent 6\" w:uiPriority=\"68\"/>"
                "<w:lsdException w:name=\"Medium Grid 3 Accent 6\" w:uiPriority=\"69\"/>"
                "<w:lsdException w:name=\"Dark List Accent 6\" w:uiPriority=\"70\"/>"
                "<w:lsdException w:name=\"Colorful Shading Accent 6\" w:uiPriority=\"71\"/>"
                "<w:lsdException w:name=\"Colorful List Accent 6\" w:uiPriority=\"72\"/>"
                "<w:lsdException w:name=\"Colorful Grid Accent 6\" w:uiPriority=\"73\"/>"
                "<w:lsdException w:name=\"Subtle Emphasis\" w:uiPriority=\"19\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Intense Emphasis\" w:uiPriority=\"21\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Subtle Reference\" w:uiPriority=\"31\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Intense Reference\" w:uiPriority=\"32\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Book Title\" w:uiPriority=\"33\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Bibliography\" w:semiHidden=\"1\" w:uiPriority=\"37\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"TOC Heading\" w:semiHidden=\"1\" w:uiPriority=\"39\" w:unhideWhenUsed=\"1\" w:qFormat=\"1\"/>"
                "<w:lsdException w:name=\"Plain Table 1\" w:uiPriority=\"41\"/>"
                "<w:lsdException w:name=\"Plain Table 2\" w:uiPriority=\"42\"/>"
                "<w:lsdException w:name=\"Plain Table 3\" w:uiPriority=\"43\"/>"
                "<w:lsdException w:name=\"Plain Table 4\" w:uiPriority=\"44\"/>"
                "<w:lsdException w:name=\"Plain Table 5\" w:uiPriority=\"45\"/>"
                "<w:lsdException w:name=\"Grid Table Light\" w:uiPriority=\"40\"/>"
                "<w:lsdException w:name=\"Grid Table 1 Light\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"Grid Table 2\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"Grid Table 3\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"Grid Table 4\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"Grid Table 5 Dark\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"Grid Table 6 Colorful\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"Grid Table 7 Colorful\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"Grid Table 1 Light Accent 1\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"Grid Table 2 Accent 1\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"Grid Table 3 Accent 1\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"Grid Table 4 Accent 1\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"Grid Table 5 Dark Accent 1\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"Grid Table 6 Colorful Accent 1\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"Grid Table 7 Colorful Accent 1\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"Grid Table 1 Light Accent 2\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"Grid Table 2 Accent 2\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"Grid Table 3 Accent 2\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"Grid Table 4 Accent 2\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"Grid Table 5 Dark Accent 2\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"Grid Table 6 Colorful Accent 2\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"Grid Table 7 Colorful Accent 2\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"Grid Table 1 Light Accent 3\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"Grid Table 2 Accent 3\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"Grid Table 3 Accent 3\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"Grid Table 4 Accent 3\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"Grid Table 5 Dark Accent 3\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"Grid Table 6 Colorful Accent 3\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"Grid Table 7 Colorful Accent 3\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"Grid Table 1 Light Accent 4\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"Grid Table 2 Accent 4\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"Grid Table 3 Accent 4\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"Grid Table 4 Accent 4\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"Grid Table 5 Dark Accent 4\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"Grid Table 6 Colorful Accent 4\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"Grid Table 7 Colorful Accent 4\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"Grid Table 1 Light Accent 5\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"Grid Table 2 Accent 5\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"Grid Table 3 Accent 5\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"Grid Table 4 Accent 5\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"Grid Table 5 Dark Accent 5\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"Grid Table 6 Colorful Accent 5\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"Grid Table 7 Colorful Accent 5\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"Grid Table 1 Light Accent 6\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"Grid Table 2 Accent 6\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"Grid Table 3 Accent 6\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"Grid Table 4 Accent 6\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"Grid Table 5 Dark Accent 6\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"Grid Table 6 Colorful Accent 6\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"Grid Table 7 Colorful Accent 6\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"List Table 1 Light\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"List Table 2\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"List Table 3\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"List Table 4\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"List Table 5 Dark\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"List Table 6 Colorful\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"List Table 7 Colorful\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"List Table 1 Light Accent 1\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"List Table 2 Accent 1\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"List Table 3 Accent 1\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"List Table 4 Accent 1\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"List Table 5 Dark Accent 1\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"List Table 6 Colorful Accent 1\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"List Table 7 Colorful Accent 1\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"List Table 1 Light Accent 2\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"List Table 2 Accent 2\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"List Table 3 Accent 2\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"List Table 4 Accent 2\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"List Table 5 Dark Accent 2\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"List Table 6 Colorful Accent 2\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"List Table 7 Colorful Accent 2\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"List Table 1 Light Accent 3\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"List Table 2 Accent 3\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"List Table 3 Accent 3\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"List Table 4 Accent 3\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"List Table 5 Dark Accent 3\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"List Table 6 Colorful Accent 3\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"List Table 7 Colorful Accent 3\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"List Table 1 Light Accent 4\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"List Table 2 Accent 4\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"List Table 3 Accent 4\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"List Table 4 Accent 4\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"List Table 5 Dark Accent 4\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"List Table 6 Colorful Accent 4\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"List Table 7 Colorful Accent 4\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"List Table 1 Light Accent 5\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"List Table 2 Accent 5\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"List Table 3 Accent 5\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"List Table 4 Accent 5\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"List Table 5 Dark Accent 5\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"List Table 6 Colorful Accent 5\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"List Table 7 Colorful Accent 5\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"List Table 1 Light Accent 6\" w:uiPriority=\"46\"/>"
                "<w:lsdException w:name=\"List Table 2 Accent 6\" w:uiPriority=\"47\"/>"
                "<w:lsdException w:name=\"List Table 3 Accent 6\" w:uiPriority=\"48\"/>"
                "<w:lsdException w:name=\"List Table 4 Accent 6\" w:uiPriority=\"49\"/>"
                "<w:lsdException w:name=\"List Table 5 Dark Accent 6\" w:uiPriority=\"50\"/>"
                "<w:lsdException w:name=\"List Table 6 Colorful Accent 6\" w:uiPriority=\"51\"/>"
                "<w:lsdException w:name=\"List Table 7 Colorful Accent 6\" w:uiPriority=\"52\"/>"
                "<w:lsdException w:name=\"Mention\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Smart Hyperlink\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Hashtag\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Unresolved Mention\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/>"
                "<w:lsdException w:name=\"Smart Link\" w:semiHidden=\"1\" w:unhideWhenUsed=\"1\"/></w:latentStyles>"
                "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\">"
                "<w:name w:val=\"Normal\"/>"
                "<w:qFormat/></w:style>"
                "<w:style w:type=\"character\" w:default=\"1\" w:styleId=\"DefaultParagraphFont\">"
                "<w:name w:val=\"Default Paragraph Font\"/>"
                "<w:uiPriority w:val=\"1\"/>"
                "<w:semiHidden/>"
                "<w:unhideWhenUsed/></w:style>"
                "<w:style w:type=\"table\" w:default=\"1\" w:styleId=\"TableNormal\">"
                "<w:name w:val=\"Normal Table\"/>"
                "<w:uiPriority w:val=\"99\"/>"
                "<w:semiHidden/>"
                "<w:unhideWhenUsed/>"
                "<w:tblPr>"
                "<w:tblInd w:w=\"0\" w:type=\"dxa\"/>"
                "<w:tblCellMar>"
                "<w:top w:w=\"0\" w:type=\"dxa\"/>"
                "<w:left w:w=\"108\" w:type=\"dxa\"/>"
                "<w:bottom w:w=\"0\" w:type=\"dxa\"/>"
                "<w:right w:w=\"108\" w:type=\"dxa\"/></w:tblCellMar></w:tblPr></w:style>"
                "<w:style w:type=\"numbering\" w:default=\"1\" w:styleId=\"NoList\">"
                "<w:name w:val=\"No List\"/>"
                "<w:uiPriority w:val=\"99\"/>"
                "<w:semiHidden/>"
                "<w:unhideWhenUsed/></w:style></w:styles>"
    },

    {
        "word/webSettings.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<w:webSettings xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" xmlns:w15=\"http://schemas.microsoft.com/office/word/2012/wordml\" xmlns:w16cex=\"http://schemas.microsoft.com/office/word/2018/wordml/cex\" xmlns:w16cid=\"http://schemas.microsoft.com/office/word/2016/wordml/cid\" xmlns:w16=\"http://schemas.microsoft.com/office/word/2018/wordml\" xmlns:w16se=\"http://schemas.microsoft.com/office/word/2015/wordml/symex\" mc:Ignorable=\"w14 w15 w16se w16cid w16 w16cex\">"
                "<w:optimizeForBrowser/>"
                "<w:allowPNG/></w:webSettings>"
    },

    {
        "word/_rels/document.xml.rels",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/webSettings\" Target=\"webSettings.xml\"/>"
                "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings\" Target=\"settings.xml\"/>"
                "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
                "<Relationship Id=\"rId5\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" Target=\"theme/theme1.xml\"/>"
                "<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/fontTable\" Target=\"fontTable.xml\"/></Relationships>"
    },

    {
        "word/theme/theme1.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
                ""
                "<a:theme xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" name=\"Office Theme\">"
                "<a:themeElements>"
                "<a:clrScheme name=\"Office\">"
                "<a:dk1>"
                "<a:sysClr val=\"windowText\" lastClr=\"000000\"/></a:dk1>"
                "<a:lt1>"
                "<a:sysClr val=\"window\" lastClr=\"FFFFFF\"/></a:lt1>"
                "<a:dk2>"
                "<a:srgbClr val=\"44546A\"/></a:dk2>"
                "<a:lt2>"
                "<a:srgbClr val=\"E7E6E6\"/></a:lt2>"
                "<a:accent1>"
                "<a:srgbClr val=\"4472C4\"/></a:accent1>"
                "<a:accent2>"
                "<a:srgbClr val=\"ED7D31\"/></a:accent2>"
                "<a:accent3>"
                "<a:srgbClr val=\"A5A5A5\"/></a:accent3>"
                "<a:accent4>"
                "<a:srgbClr val=\"FFC000\"/></a:accent4>"
                "<a:accent5>"
                "<a:srgbClr val=\"5B9BD5\"/></a:accent5>"
                "<a:accent6>"
                "<a:srgbClr val=\"70AD47\"/></a:accent6>"
                "<a:hlink>"
                "<a:srgbClr val=\"0563C1\"/></a:hlink>"
                "<a:folHlink>"
                "<a:srgbClr val=\"954F72\"/></a:folHlink></a:clrScheme>"
                "<a:fontScheme name=\"Office\">"
                "<a:majorFont>"
                "<a:latin typeface=\"Calibri Light\" panose=\"020F0302020204030204\"/>"
                "<a:ea typeface=\"\"/>"
                "<a:cs typeface=\"\"/>"
                "<a:font script=\"Jpan\" typeface=\"\xe6\xb8\xb8\xe3\x82\xb4\xe3\x82\xb7\xe3\x83\x83\xe3\x82\xaf Light\"/>"
                "<a:font script=\"Hang\" typeface=\"\xeb\xa7\x91\xec\x9d\x80 \xea\xb3\xa0\xeb\x94\x95\"/>"
                "<a:font script=\"Hans\" typeface=\"\xe7\xad\x89\xe7\xba\xbf Light\"/>"
                "<a:font script=\"Hant\" typeface=\"\xe6\x96\xb0\xe7\xb4\xb0\xe6\x98\x8e\xe9\xab\x94\"/>"
                "<a:font script=\"Arab\" typeface=\"Times New Roman\"/>"
                "<a:font script=\"Hebr\" typeface=\"Times New Roman\"/>"
                "<a:font script=\"Thai\" typeface=\"Angsana New\"/>"
                "<a:font script=\"Ethi\" typeface=\"Nyala\"/>"
                "<a:font script=\"Beng\" typeface=\"Vrinda\"/>"
                "<a:font script=\"Gujr\" typeface=\"Shruti\"/>"
                "<a:font script=\"Khmr\" typeface=\"MoolBoran\"/>"
                "<a:font script=\"Knda\" typeface=\"Tunga\"/>"
                "<a:font script=\"Guru\" typeface=\"Raavi\"/>"
                "<a:font script=\"Cans\" typeface=\"Euphemia\"/>"
                "<a:font script=\"Cher\" typeface=\"Plantagenet Cherokee\"/>"
                "<a:font script=\"Yiii\" typeface=\"Microsoft Yi Baiti\"/>"
                "<a:font script=\"Tibt\" typeface=\"Microsoft Himalaya\"/>"
                "<a:font script=\"Thaa\" typeface=\"MV Boli\"/>"
                "<a:font script=\"Deva\" typeface=\"Mangal\"/>"
                "<a:font script=\"Telu\" typeface=\"Gautami\"/>"
                "<a:font script=\"Taml\" typeface=\"Latha\"/>"
                "<a:font script=\"Syrc\" typeface=\"Estrangelo Edessa\"/>"
                "<a:font script=\"Orya\" typeface=\"Kalinga\"/>"
                "<a:font script=\"Mlym\" typeface=\"Kartika\"/>"
                "<a:font script=\"Laoo\" typeface=\"DokChampa\"/>"
                "<a:font script=\"Sinh\" typeface=\"Iskoola Pota\"/>"
                "<a:font script=\"Mong\" typeface=\"Mongolian Baiti\"/>"
                "<a:font script=\"Viet\" typeface=\"Times New Roman\"/>"
                "<a:font script=\"Uigh\" typeface=\"Microsoft Uighur\"/>"
                "<a:font script=\"Geor\" typeface=\"Sylfaen\"/>"
                "<a:font script=\"Armn\" typeface=\"Arial\"/>"
                "<a:font script=\"Bugi\" typeface=\"Leelawadee UI\"/>"
                "<a:font script=\"Bopo\" typeface=\"Microsoft JhengHei\"/>"
                "<a:font script=\"Java\" typeface=\"Javanese Text\"/>"
                "<a:font script=\"Lisu\" typeface=\"Segoe UI\"/>"
                "<a:font script=\"Mymr\" typeface=\"Myanmar Text\"/>"
                "<a:font script=\"Nkoo\" typeface=\"Ebrima\"/>"
                "<a:font script=\"Olck\" typeface=\"Nirmala UI\"/>"
                "<a:font script=\"Osma\" typeface=\"Ebrima\"/>"
                "<a:font script=\"Phag\" typeface=\"Phagspa\"/>"
                "<a:font script=\"Syrn\" typeface=\"Estrangelo Edessa\"/>"
                "<a:font script=\"Syrj\" typeface=\"Estrangelo Edessa\"/>"
                "<a:font script=\"Syre\" typeface=\"Estrangelo Edessa\"/>"
                "<a:font script=\"Sora\" typeface=\"Nirmala UI\"/>"
                "<a:font script=\"Tale\" typeface=\"Microsoft Tai Le\"/>"
                "<a:font script=\"Talu\" typeface=\"Microsoft New Tai Lue\"/>"
                "<a:font script=\"Tfng\" typeface=\"Ebrima\"/></a:majorFont>"
                "<a:minorFont>"
                "<a:latin typeface=\"Calibri\" panose=\"020F0502020204030204\"/>"
                "<a:ea typeface=\"\"/>"
                "<a:cs typeface=\"\"/>"
                "<a:font script=\"Jpan\" typeface=\"\xe6\xb8\xb8\xe6\x98\x8e\xe6\x9c\x9d\"/>"
                "<a:font script=\"Hang\" typeface=\"\xeb\xa7\x91\xec\x9d\x80 \xea\xb3\xa0\xeb\x94\x95\"/>"
                "<a:font script=\"Hans\" typeface=\"\xe7\xad\x89\xe7\xba\xbf\"/>"
                "<a:font script=\"Hant\" typeface=\"\xe6\x96\xb0\xe7\xb4\xb0\xe6\x98\x8e\xe9\xab\x94\"/>"
                "<a:font script=\"Arab\" typeface=\"Arial\"/>"
                "<a:font script=\"Hebr\" typeface=\"Arial\"/>"
                "<a:font script=\"Thai\" typeface=\"Cordia New\"/>"
                "<a:font script=\"Ethi\" typeface=\"Nyala\"/>"
                "<a:font script=\"Beng\" typeface=\"Vrinda\"/>"
                "<a:font script=\"Gujr\" typeface=\"Shruti\"/>"
                "<a:font script=\"Khmr\" typeface=\"DaunPenh\"/>"
                "<a:font script=\"Knda\" typeface=\"Tunga\"/>"
                "<a:font script=\"Guru\" typeface=\"Raavi\"/>"
                "<a:font script=\"Cans\" typeface=\"Euphemia\"/>"
                "<a:font script=\"Cher\" typeface=\"Plantagenet Cherokee\"/>"
                "<a:font script=\"Yiii\" typeface=\"Microsoft Yi Baiti\"/>"
                "<a:font script=\"Tibt\" typeface=\"Microsoft Himalaya\"/>"
                "<a:font script=\"Thaa\" typeface=\"MV Boli\"/>"
                "<a:font script=\"Deva\" typeface=\"Mangal\"/>"
                "<a:font script=\"Telu\" typeface=\"Gautami\"/>"
                "<a:font script=\"Taml\" typeface=\"Latha\"/>"
                "<a:font script=\"Syrc\" typeface=\"Estrangelo Edessa\"/>"
                "<a:font script=\"Orya\" typeface=\"Kalinga\"/>"
                "<a:font script=\"Mlym\" typeface=\"Kartika\"/>"
                "<a:font script=\"Laoo\" typeface=\"DokChampa\"/>"
                "<a:font script=\"Sinh\" typeface=\"Iskoola Pota\"/>"
                "<a:font script=\"Mong\" typeface=\"Mongolian Baiti\"/>"
                "<a:font script=\"Viet\" typeface=\"Arial\"/>"
                "<a:font script=\"Uigh\" typeface=\"Microsoft Uighur\"/>"
                "<a:font script=\"Geor\" typeface=\"Sylfaen\"/>"
                "<a:font script=\"Armn\" typeface=\"Arial\"/>"
                "<a:font script=\"Bugi\" typeface=\"Leelawadee UI\"/>"
                "<a:font script=\"Bopo\" typeface=\"Microsoft JhengHei\"/>"
                "<a:font script=\"Java\" typeface=\"Javanese Text\"/>"
                "<a:font script=\"Lisu\" typeface=\"Segoe UI\"/>"
                "<a:font script=\"Mymr\" typeface=\"Myanmar Text\"/>"
                "<a:font script=\"Nkoo\" typeface=\"Ebrima\"/>"
                "<a:font script=\"Olck\" typeface=\"Nirmala UI\"/>"
                "<a:font script=\"Osma\" typeface=\"Ebrima\"/>"
                "<a:font script=\"Phag\" typeface=\"Phagspa\"/>"
                "<a:font script=\"Syrn\" typeface=\"Estrangelo Edessa\"/>"
                "<a:font script=\"Syrj\" typeface=\"Estrangelo Edessa\"/>"
                "<a:font script=\"Syre\" typeface=\"Estrangelo Edessa\"/>"
                "<a:font script=\"Sora\" typeface=\"Nirmala UI\"/>"
                "<a:font script=\"Tale\" typeface=\"Microsoft Tai Le\"/>"
                "<a:font script=\"Talu\" typeface=\"Microsoft New Tai Lue\"/>"
                "<a:font script=\"Tfng\" typeface=\"Ebrima\"/></a:minorFont></a:fontScheme>"
                "<a:fmtScheme name=\"Office\">"
                "<a:fillStyleLst>"
                "<a:solidFill>"
                "<a:schemeClr val=\"phClr\"/></a:solidFill>"
                "<a:gradFill rotWithShape=\"1\">"
                "<a:gsLst>"
                "<a:gs pos=\"0\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:lumMod val=\"110000\"/>"
                "<a:satMod val=\"105000\"/>"
                "<a:tint val=\"67000\"/></a:schemeClr></a:gs>"
                "<a:gs pos=\"50000\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:lumMod val=\"105000\"/>"
                "<a:satMod val=\"103000\"/>"
                "<a:tint val=\"73000\"/></a:schemeClr></a:gs>"
                "<a:gs pos=\"100000\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:lumMod val=\"105000\"/>"
                "<a:satMod val=\"109000\"/>"
                "<a:tint val=\"81000\"/></a:schemeClr></a:gs></a:gsLst>"
                "<a:lin ang=\"5400000\" scaled=\"0\"/></a:gradFill>"
                "<a:gradFill rotWithShape=\"1\">"
                "<a:gsLst>"
                "<a:gs pos=\"0\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:satMod val=\"103000\"/>"
                "<a:lumMod val=\"102000\"/>"
                "<a:tint val=\"94000\"/></a:schemeClr></a:gs>"
                "<a:gs pos=\"50000\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:satMod val=\"110000\"/>"
                "<a:lumMod val=\"100000\"/>"
                "<a:shade val=\"100000\"/></a:schemeClr></a:gs>"
                "<a:gs pos=\"100000\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:lumMod val=\"99000\"/>"
                "<a:satMod val=\"120000\"/>"
                "<a:shade val=\"78000\"/></a:schemeClr></a:gs></a:gsLst>"
                "<a:lin ang=\"5400000\" scaled=\"0\"/></a:gradFill></a:fillStyleLst>"
                "<a:lnStyleLst>"
                "<a:ln w=\"6350\" cap=\"flat\" cmpd=\"sng\" algn=\"ctr\">"
                "<a:solidFill>"
                "<a:schemeClr val=\"phClr\"/></a:solidFill>"
                "<a:prstDash val=\"solid\"/>"
                "<a:miter lim=\"800000\"/></a:ln>"
                "<a:ln w=\"12700\" cap=\"flat\" cmpd=\"sng\" algn=\"ctr\">"
                "<a:solidFill>"
                "<a:schemeClr val=\"phClr\"/></a:solidFill>"
                "<a:prstDash val=\"solid\"/>"
                "<a:miter lim=\"800000\"/></a:ln>"
                "<a:ln w=\"19050\" cap=\"flat\" cmpd=\"sng\" algn=\"ctr\">"
                "<a:solidFill>"
                "<a:schemeClr val=\"phClr\"/></a:solidFill>"
                "<a:prstDash val=\"solid\"/>"
                "<a:miter lim=\"800000\"/></a:ln></a:lnStyleLst>"
                "<a:effectStyleLst>"
                "<a:effectStyle>"
                "<a:effectLst/></a:effectStyle>"
                "<a:effectStyle>"
                "<a:effectLst/></a:effectStyle>"
                "<a:effectStyle>"
                "<a:effectLst>"
                "<a:outerShdw blurRad=\"57150\" dist=\"19050\" dir=\"5400000\" algn=\"ctr\" rotWithShape=\"0\">"
                "<a:srgbClr val=\"000000\">"
                "<a:alpha val=\"63000\"/></a:srgbClr></a:outerShdw></a:effectLst></a:effectStyle></a:effectStyleLst>"
                "<a:bgFillStyleLst>"
                "<a:solidFill>"
                "<a:schemeClr val=\"phClr\"/></a:solidFill>"
                "<a:solidFill>"
                "<a:schemeClr val=\"phClr\">"
                "<a:tint val=\"95000\"/>"
                "<a:satMod val=\"170000\"/></a:schemeClr></a:solidFill>"
                "<a:gradFill rotWithShape=\"1\">"
                "<a:gsLst>"
                "<a:gs pos=\"0\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:tint val=\"93000\"/>"
                "<a:satMod val=\"150000\"/>"
                "<a:shade val=\"98000\"/>"
                "<a:lumMod val=\"102000\"/></a:schemeClr></a:gs>"
                "<a:gs pos=\"50000\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:tint val=\"98000\"/>"
                "<a:satMod val=\"130000\"/>"
                "<a:shade val=\"90000\"/>"
                "<a:lumMod val=\"103000\"/></a:schemeClr></a:gs>"
                "<a:gs pos=\"100000\">"
                "<a:schemeClr val=\"phClr\">"
                "<a:shade val=\"63000\"/>"
                "<a:satMod val=\"120000\"/></a:schemeClr></a:gs></a:gsLst>"
                "<a:lin ang=\"5400000\" scaled=\"0\"/></a:gradFill></a:bgFillStyleLst></a:fmtScheme></a:themeElements>"
                "<a:objectDefaults/>"
                "<a:extraClrSchemeLst/>"
                "<a:extLst>"
                "<a:ext uri=\"{05A4C25C-085E-4340-85A3-A5531E510DB2}\">"
                "<thm15:themeFamily xmlns:thm15=\"http://schemas.microsoft.com/office/thememl/2012/main\" name=\"Office Theme\" id=\"{62F939B6-93AF-4DB8-9C6B-D6C7DFDC589F}\" vid=\"{4A3C46E8-61CC-4603-A589-7422A47A8E4A}\"/></a:ext></a:extLst></a:theme>"
    },

};

int docx_template_items_num = 11;

#include "extract/extract.h"
#include "extract/alloc.h"

#ifndef ARTIFEX_EXTRACT_HTML_H
#define ARTIFEX_EXTRACT_HTML_H

#include "extract/extract.h"

int extract_document_to_html_content(
		extract_alloc_t    *alloc,
		document_t        *document,
		int                rotation,
		int                images,
		extract_astring_t *content
		);

#endif

#ifndef ARTIFEX_EXTRACT_JSON_H
#define ARTIFEX_EXTRACT_JSON_H

#include "extract/extract.h"

int extract_document_to_json_content(
		extract_alloc_t   *alloc,
		document_t        *document,
		int                rotation,
		int                images,
		extract_astring_t *content
		);

#endif

#ifndef ARTIFEX_EXTRACT_ODT_H
#define ARTIFEX_EXTRACT_ODT_H

typedef struct extract_odt_style_t extract_odt_style_t;

typedef struct
{
    extract_odt_style_t*    styles;
    int                     styles_num;
} extract_odt_styles_t;

void extract_odt_styles_free(extract_alloc_t* alloc, extract_odt_styles_t* styles);

int extract_document_to_odt_content(
        extract_alloc_t*    alloc,
        document_t*         document,
        int                 spacing,
        int                 rotation,
        int                 images,
        extract_astring_t*  o_content,
        extract_odt_styles_t* o_styles
        );

int extract_odt_write_template(
        extract_alloc_t*    alloc,
        extract_astring_t*  contentss,
        int                 contentss_num,
        extract_odt_styles_t* styles,
        images_t*           images,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        );

int extract_odt_content_item(
        extract_alloc_t*    alloc,
        extract_astring_t*  contentss,
        int                 contentss_num,
        extract_odt_styles_t* styles,
        images_t*           images,
        const char*         name,
        const char*         text,
        char**              text2
        );

#endif

#ifndef EXTRACT_ODT_TEMPLATE_H
#define EXTRACT_ODT_TEMPLATE_H

typedef struct
{
    const char* name;
    const char* text;
} odt_template_item_t;

extern const odt_template_item_t odt_template_items[];
extern int odt_template_items_num;

#endif

#ifndef ARTIFEX_EXTRACT_XML
#define ARTIFEX_EXTRACT_XML

#include "extract/buffer.h"

typedef struct {
	char *name;
	char *value;
} extract_xml_attribute_t;

typedef struct {
	char                    *name;
	extract_xml_attribute_t *attributes;
	int                      attributes_num;
	extract_astring_t        text;
} extract_xml_tag_t;

void extract_xml_tag_init(extract_xml_tag_t *tag);

void extract_xml_tag_free(extract_alloc_t *alloc, extract_xml_tag_t *tag);

int extract_xml_pparse_init(extract_alloc_t *alloc, extract_buffer_t *buffer, const char *first_line);

int extract_xml_pparse_next(extract_buffer_t *buffer, extract_xml_tag_t *out);

char *extract_xml_tag_attributes_find(extract_xml_tag_t *tag, const char *name);

int extract_xml_tag_attributes_find_float(
		extract_xml_tag_t *tag,
		const char        *name,
		float             *o_out);

int extract_xml_tag_attributes_find_double(
		extract_xml_tag_t *tag,
		const char        *name,
		double            *o_out);

int extract_xml_str_to_llint(const char *text, long long *o_out);

int extract_xml_str_to_ullint(const char *text, unsigned long long *o_out);

int extract_xml_str_to_int(const char *text, int *o_out);

int extract_xml_str_to_uint(const char *text, unsigned *o_out);

int extract_xml_str_to_size(const char *text, size_t *o_out);

int extract_xml_str_to_double(const char *text, double *o_out);

int extract_xml_str_to_float(const char *text, float *o_out);

int extract_xml_tag_attributes_find_int(
		extract_xml_tag_t *tag,
		const char        *name,
		int               *o_out);

int extract_xml_tag_attributes_find_uint(
		extract_xml_tag_t *tag,
		const char        *name,
		unsigned          *o_out);

int extract_xml_tag_attributes_find_size(
		extract_xml_tag_t *tag,
		const char        *name,
		size_t            *o_out);

#endif

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const rect_t extract_rect_infinite = { { -DBL_MAX, -DBL_MAX }, {  DBL_MAX,  DBL_MAX } };
const rect_t extract_rect_empty	= { {  DBL_MAX,  DBL_MAX }, { -DBL_MAX, -DBL_MAX } };

double extract_matrix_expansion(matrix4_t m)
{
	return sqrt(fabs(m.a * m.d - m.b * m.c));
}

matrix4_t extract_matrix4_invert(const matrix4_t *ctm)
{
	matrix4_t ctm_inverse = {1, 0, 0, 1};
	double	ctm_det	 = ctm->a*ctm->d - ctm->b*ctm->c;

	if (ctm_det == 0) {
		outf("cannot invert ctm=(%f %f %f %f)",
			ctm->a, ctm->b, ctm->c, ctm->d);
	}
	else
	{
		ctm_inverse.a = +ctm->d / ctm_det;
		ctm_inverse.b = -ctm->b / ctm_det;
		ctm_inverse.c = -ctm->c / ctm_det;
		ctm_inverse.d = +ctm->a / ctm_det;
	}

	return ctm_inverse;
}

static void char_init(char_t* item)
{
	item->x = 0;
	item->y = 0;
	item->ucs = 0;
	item->adv = 0;
	item->bbox = extract_rect_empty;
}

const char *extract_point_string(const point_t *point)
{
	static char buffer[128];

	snprintf(buffer, sizeof(buffer), "(%f %f)", point->x, point->y);

	return buffer;
}

const char *extract_rect_string(const rect_t *rect)
{
	static char buffer[2][256];
	static int i = 0;

	i = (i + 1) % 2;
	snprintf(buffer[i], sizeof(buffer[i]), "((%f %f) (%f %f))", rect->min.x, rect->min.y, rect->max.x, rect->max.y);

	return buffer[i];
}

const char *extract_span_string(extract_alloc_t *alloc, span_t *span)
{
	static extract_astring_t ret = {0};
	double x0 = 0;
	double y0 = 0;
	double x1 = 0;
	double y1 = 0;
	int c0 = 0;
	int c1 = 0;
	int i;

	extract_astring_free(alloc, &ret);
	if (span == NULL)
	{

		return NULL;
	}

	if (span->chars_num) {
		c0 = span->chars[0].ucs;
		x0 = span->chars[0].x;
		y0 = span->chars[0].y;
		c1 = span->chars[span->chars_num-1].ucs;
		x1 = span->chars[span->chars_num-1].x;
		y1 = span->chars[span->chars_num-1].y;
	}
	{
		char buffer[400];
		snprintf(buffer, sizeof(buffer),
			"span ctm=%s chars_num=%i (%c:%f,%f)..(%c:%f,%f) font=%s:(%f) wmode=%i chars_num=%i: ",
			extract_matrix4_string(&span->ctm),
			span->chars_num,
			c0, x0, y0,
			c1, x1, y1,
			span->font_name,
			extract_font_size(&span->ctm),
			span->flags.wmode,
			span->chars_num
			);
		extract_astring_cat(alloc, &ret, buffer);
		for (i=0; i<span->chars_num; ++i) {
			snprintf(
				buffer,
				sizeof(buffer),
				" i=%i {x=%f y=%f ucs=%i adv=%f}",
				i,
				span->chars[i].x,
				span->chars[i].y,
				span->chars[i].ucs,
				span->chars[i].adv
				);
			extract_astring_cat(alloc, &ret, buffer);
		}
	}
	extract_astring_cat(alloc, &ret, ": ");
	extract_astring_catc(alloc, &ret, '"');
	for (i=0; i<span->chars_num; ++i)
		extract_astring_catc(alloc, &ret, (char) span->chars[i].ucs);
	extract_astring_catc(alloc, &ret, '"');
	return ret.chars;
}

char_t *extract_span_append_c(extract_alloc_t *alloc, span_t *span, int c)
{
	char_t *item;

	if (extract_realloc2(alloc,
			&span->chars,
			sizeof(*span->chars) * span->chars_num,
			sizeof(*span->chars) * (span->chars_num + 1)))
	{
		return NULL;
	}
	item = &span->chars[span->chars_num];
	span->chars_num += 1;
	char_init(item);
	item->ucs = c;

	return item;
}

char_t *extract_span_char_last(span_t *span)
{
	assert(span->chars_num > 0);
	return &span->chars[span->chars_num-1];
}

span_t *extract_line_span_last(line_t *line)
{
	assert(line->content.base.prev != &line->content.base && line->content.base.prev->type == content_span);
	return (span_t *)line->content.base.prev;
}

span_t *extract_line_span_first(line_t *line)
{
	assert(line->content.base.next != &line->content.base && line->content.base.next->type == content_span);
	return (span_t *)line->content.base.next;
}

void extract_paragraph_free(extract_alloc_t *alloc, paragraph_t **pparagraph)
{
	paragraph_t *paragraph = *pparagraph;

	if (paragraph == NULL)
		return;

	content_unlink(&paragraph->base);
	content_clear(alloc, &paragraph->content);
	extract_free(alloc, pparagraph);
}

void extract_block_free(extract_alloc_t *alloc, block_t **pblock)
{
	block_t *block = *pblock;

	if (block == NULL)
		return;

	content_unlink(&block->base);
	content_clear(alloc, &block->content);
	extract_free(alloc, pblock);
}

void extract_table_free(extract_alloc_t *alloc, table_t **ptable)
{
	int	  c;
	table_t *table = *ptable;

	content_unlink(&table->base);
	for (c = 0; c< table->cells_num_x * table->cells_num_y; ++c)
	{
		extract_cell_free(alloc, &table->cells[c]);
	}
	extract_free(alloc, &table->cells);
	extract_free(alloc, ptable);
}

static void
structure_clear(extract_alloc_t *alloc, structure_t *structure)
{
	while (structure != NULL)
	{
		structure_t *next = structure->sibling_next;
		structure_clear(alloc, structure->kids_first);
		extract_free(alloc, &structure);
		structure = next;
	}
}

void extract_subpage_free(extract_alloc_t *alloc, subpage_t **psubpage)
{
	subpage_t *subpage = *psubpage;

	if (!subpage) return;

	content_clear(alloc, &subpage->content);
	content_clear(alloc, &subpage->tables);

	extract_free(alloc, &subpage->tablelines_horizontal.tablelines);
	extract_free(alloc, &subpage->tablelines_vertical.tablelines);

	extract_free(alloc, psubpage);
}

static void page_free(extract_alloc_t *alloc, extract_page_t **ppage)
{
	int c;
	extract_page_t *page = *ppage;

	if (!page) return;

	for (c=0; c<page->subpages_num; ++c)
	{
		subpage_t *subpage = page->subpages[c];
		extract_subpage_free(alloc, &subpage);
	}
	extract_split_free(alloc, &page->split);
	extract_free(alloc, &page->subpages);
	extract_free(alloc, ppage);
}

void content_append(content_root_t *root, content_t *content)
{
	assert(root && root->base.type == content_root);

	content_unlink(content);

	if (root->base.next == &root->base)
	{
		assert(root->base.prev == &root->base);
	}

	content->next = &root->base;
	content->prev = root->base.prev;
	content->prev->next = content;
	root->base.prev = content;
}

void content_append_span(content_root_t *root, span_t *span)
{
	content_append(root, &span->base);
}

void content_append_line(content_root_t *root, line_t *line)
{
	content_append(root, &line->base);
}

void content_append_paragraph(content_root_t *root, paragraph_t *paragraph)
{
	content_append(root, &paragraph->base);
}

void content_append_block(content_root_t *root, block_t *block)
{
	content_append(root, &block->base);
}

int content_new_root(extract_alloc_t *alloc, content_root_t **proot)
{
	if (extract_malloc(alloc, proot, sizeof(**proot))) return -1;
	content_init_root(*proot, NULL);

	return 0;
}

int content_new_span(extract_alloc_t *alloc, span_t **pspan, structure_t *structure)
{
	if (extract_malloc(alloc, pspan, sizeof(**pspan))) return -1;
	extract_span_init(*pspan, structure);

	return 0;
}

int content_new_line(extract_alloc_t *alloc, line_t **pline)
{
	if (extract_malloc(alloc, pline, sizeof(**pline))) return -1;
	extract_line_init(*pline);

	return 0;
}

int content_new_paragraph(extract_alloc_t *alloc, paragraph_t **pparagraph)
{
	if (extract_malloc(alloc, pparagraph, sizeof(**pparagraph))) return -1;
	extract_paragraph_init(*pparagraph);

	return 0;
}

int content_new_block(extract_alloc_t *alloc, block_t **pblock)
{
	if (extract_malloc(alloc, pblock, sizeof(**pblock))) return -1;
	extract_block_init(*pblock);

	return 0;
}

int content_new_table(extract_alloc_t *alloc, table_t **ptable)
{
	if (extract_malloc(alloc, ptable, sizeof(**ptable))) return -1;
	extract_table_init(*ptable);

	return 0;
}

int content_append_new_span(extract_alloc_t *alloc, content_root_t *root, span_t **pspan, structure_t *structure)
{
	if (content_new_span(alloc, pspan, structure)) return -1;
	content_append(root, &(*pspan)->base);

	return 0;
}

int content_append_new_line(extract_alloc_t *alloc, content_root_t *root, line_t **pline)
{
	if (content_new_line(alloc, pline)) return -1;
	content_append(root, &(*pline)->base);

	return 0;
}

int content_append_new_paragraph(extract_alloc_t *alloc, content_root_t *root, paragraph_t **pparagraph)
{
	if (content_new_paragraph(alloc, pparagraph)) return -1;
	content_append(root, &(*pparagraph)->base);

	return 0;
}

int content_append_new_block(extract_alloc_t *alloc, content_root_t *root, block_t **pblock)
{
	if (content_new_block(alloc, pblock)) return -1;
	content_append(root, &(*pblock)->base);

	return 0;
}

int content_append_new_table(extract_alloc_t *alloc, content_root_t *root, table_t **ptable)
{
	if (content_new_table(alloc, ptable)) return -1;
	content_append(root, &(*ptable)->base);

	return 0;
}

int content_append_new_image(extract_alloc_t *alloc, content_root_t *root, image_t **pimage)
{
	if (extract_malloc(alloc, pimage, sizeof(**pimage))) return -1;
	extract_image_init(*pimage);
	content_append(root, &(*pimage)->base);

	return 0;
}

void content_replace(content_t *current, content_t *replacement)
{
	assert(current->type != content_root && replacement->type != content_root);

	if (replacement->prev)
	{
		replacement->prev->next = replacement->next;
		replacement->next->prev = replacement->prev;
	}

	replacement->prev = current->prev;
	current->prev->next = replacement;
	replacement->next = current->next;
	current->next->prev = replacement;

	current->prev = NULL;
	current->next = NULL;
}

int content_replace_new_paragraph(extract_alloc_t *alloc, content_t *current, paragraph_t **pparagraph)
{
	if (content_new_paragraph(alloc, pparagraph)) return -1;
	content_replace(current, &(*pparagraph)->base);

	return 0;
}

int content_replace_new_block(extract_alloc_t *alloc, content_t *current, block_t **pblock)
{
	if (content_new_block(alloc, pblock)) return -1;
	content_replace(current, &(*pblock)->base);

	return 0;
}

int content_replace_new_line(extract_alloc_t *alloc, content_t *current, line_t **pline)
{
	if (content_new_line(alloc, pline)) return -1;
	content_replace(current, &(*pline)->base);

	return 0;
}

static void extract_images_free(extract_alloc_t *alloc, images_t *images)
{
	int i;
	for (i=0; i<images->images_num; ++i) {
		extract_image_clear(alloc, images->images[i]);
		extract_free(alloc, &images->images[i]);
	}
	extract_free(alloc, &images->images);
	extract_free(alloc, &images->imagetypes);
	images->images_num = 0;
	images->imagetypes_num = 0;
}

static int
extract_document_images(extract_alloc_t *alloc, document_t *document, images_t *o_images)
{
	int     e = -1;
	int      p;
	images_t images = {0};

	outf("extract_document_images(): images.images_num=%i", images.images_num);
	for (p=0; p<document->pages_num; ++p)
	{
		extract_page_t *page = document->pages[p];
		int c;
		for (c=0; c<page->subpages_num; ++c)
		{
			subpage_t              *subpage = page->subpages[c];
			content_image_iterator  iit;
			image_t                *image;
			int                     i;

			for (i = 0, image = content_image_iterator_init(&iit, &subpage->content); image != NULL; i++, image = content_image_iterator_next(&iit))
			{
				if (extract_realloc2(alloc,
						 &images.images,
						 sizeof(image_t) * images.images_num,
						 sizeof(image_t) * (images.images_num + 1))) goto end;
				outf("p=%i i=%i image->name=%s image->id=%s", p, i, image->name, image->id);
				assert(image->name);
				content_unlink(&image->base);
				images.images[images.images_num] = image;
				images.images_num += 1;

				{
					int it;
					for (it=0; it<images.imagetypes_num; ++it)
					{
						outf("it=%i images.imagetypes[it]=%s image->type=%s",
							it, images.imagetypes[it], image->type);
						if (!strcmp(images.imagetypes[it], image->type))
						{
							break;
						}
					}
					if (it == images.imagetypes_num)
					{

						if (extract_realloc2(
								alloc,
								&images.imagetypes,
								sizeof(char*) * images.imagetypes_num,
								sizeof(char*) * (images.imagetypes_num + 1)
							)) goto end;
						assert(image->type);
						images.imagetypes[images.imagetypes_num] = image->type;
						images.imagetypes_num += 1;
						outf("have added images.imagetypes_num=%i", images.imagetypes_num);
					}
				}
			}
		}
	}

	e = 0;
end:

	if (e)
	{
		extract_free(alloc, &images.images);
	}
	else
	{
		*o_images = images;
	}

	return e;
}

static void extract_document_free(extract_alloc_t *alloc, document_t *document)
{
	int p;

	if (!document) return;

	for (p=0; p<document->pages_num; ++p)
	{
		page_free(alloc, &document->pages[p]);
	}
	extract_free(alloc, &document->pages);
	document->pages = NULL;
	document->pages_num = 0;

	structure_clear(alloc, document->structure);
}

static int s_sign(double x)
{
	if (x < 0)  return -1;
	if (x > 0)  return +1;

	return 0;
}

int extract_matrix4_cmp(const matrix4_t *lhs, const matrix4_t *rhs)
{
	int ret;

	ret = s_sign(lhs->a - rhs->a);  if (ret) return ret;
	ret = s_sign(lhs->b - rhs->b);  if (ret) return ret;
	ret = s_sign(lhs->c - rhs->c);  if (ret) return ret;
	ret = s_sign(lhs->d - rhs->d);  if (ret) return ret;

	return 0;
}

point_t extract_matrix4_transform_point(matrix4_t m, point_t p)
{
	double x = p.x;

	p.x = m.a * x + m.c * p.y;
	p.y = m.b * x + m.d * p.y;

	return p;
}

point_t extract_matrix4_transform_xy(matrix4_t m, double x, double y)
{
	point_t p;

	p.x = m.a * x + m.c * y;
	p.y = m.b * x + m.d * y;

	return p;
}

matrix_t extract_multiply_matrix_matrix(matrix_t m1, matrix_t m2)
{
	matrix_t ret;

	ret.a = m1.a * m2.a + m1.b * m2.c;
	ret.b = m1.a * m2.b + m1.b * m2.d;
	ret.c = m1.c * m2.a + m1.d * m2.c;
	ret.d = m1.c * m2.b + m1.d * m2.d;
	ret.e = m1.e * m2.a + m1.f * m2.c + m2.e;
	ret.f = m1.e * m2.b + m1.f * m2.d + m2.f;

	return ret;
}

matrix4_t extract_multiply_matrix4_matrix4(matrix4_t m1, matrix4_t m2)
{
	matrix4_t ret;

	ret.a = m1.a * m2.a + m1.b * m2.c;
	ret.b = m1.a * m2.b + m1.b * m2.d;
	ret.c = m1.c * m2.a + m1.d * m2.c;
	ret.d = m1.c * m2.b + m1.d * m2.d;

	return ret;
}

static int s_matrix_read(const char *text, matrix_t *matrix)
{
	int n;

	if (!text) {
		outf("text is NULL in s_matrix_read()");
		errno = EINVAL;
		return -1;
	}
	n = sscanf(text,
			   "%lf %lf %lf %lf %lf %lf",
			   &matrix->a,
			   &matrix->b,
			   &matrix->c,
			   &matrix->d,
			   &matrix->e,
			   &matrix->f);
	if (n != 6) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

static void document_init(document_t *document)
{
	document->pages = NULL;
	document->pages_num = 0;

	document->structure = NULL;
	document->current = NULL;
}

#define MAX_STRUCT_NEST 64

struct extract_t
{
	extract_alloc_t         *alloc;
	int                      layout_analysis;
	double                   master_space_guess;
	document_t               document;

	int                      num_spans_split;

	int                      num_spans_autosplit;

	double                   span_offset_x;
	double                   span_offset_y;

	int                      image_n;

	extract_astring_t       *contentss;
	int                      contentss_num;

	images_t                 images;

	extract_format_t         format;
	extract_odt_styles_t     odt_styles;

	char                    *tables_csv_format;
	int                      tables_csv_i;

	enum
	{
		path_type_NONE,
		path_type_FILL,
		path_type_STROKE,
	} path_type;

	union
	{
		struct
		{
			matrix_t ctm;
			double   color;
			point_t  points[4];
			int      n;
		} fill;

		struct
		{
			matrix_t ctm;
			double   color;
			double   width;
			point_t  point0;
			int      point0_set;
			point_t  point;
			int      point_set;
		} stroke;
	} path;

	int next_uid;
};

int extract_begin(extract_alloc_t  *alloc,
		extract_format_t    format,
		extract_t         **pextract)
{
	extract_t *extract;

	*pextract = NULL;
	if (1
			&& format != extract_format_ODT
			&& format != extract_format_DOCX
			&& format != extract_format_HTML
			&& format != extract_format_TEXT
			&& format != extract_format_JSON
			)
	{
		outf0("Invalid format=%i\n", format);
		errno = EINVAL;
		return -1;
	}

	if (extract_malloc(alloc, &extract, sizeof(*extract)))
		return -1;

	extract_bzero(extract, sizeof(*extract));
	extract->alloc = alloc;
	extract->master_space_guess = 0.5;
	document_init(&extract->document);

	extract->image_n = 10;

	extract->format = format;
	extract->tables_csv_format = NULL;
	extract->tables_csv_i = 0;

	extract->next_uid = 1;

	*pextract = extract;

	return 0;
}

void extract_set_space_guess(extract_t *extract, double space_guess)
{
    extract->master_space_guess = space_guess;
}

int extract_set_layout_analysis(extract_t *extract, int enable)
{
	extract->layout_analysis = enable;
	return 0;
}

int extract_tables_csv_format(extract_t *extract, const char *path_format)
{
	return extract_strdup(extract->alloc, path_format, &extract->tables_csv_format);
}

static void image_free_fn(void *handle, void *image_data)
{
	(void) handle;
	free(image_data);
}

int extract_read_intermediate(extract_t *extract, extract_buffer_t *buffer)
{
	int                ret        = -1;
	document_t        *document   = &extract->document;
	char              *image_data = NULL;
	int                num_spans  = 0;
	extract_xml_tag_t  tag;

	extract_xml_tag_init(&tag);

	if (extract_xml_pparse_init(extract->alloc, buffer, NULL )) {
		outf("Failed to read start of intermediate data: %s", strerror(errno));
		goto end;
	}

	for(;;) {
		extract_page_t *page;
		subpage_t      *subpage;
		rect_t          mediabox = extract_rect_infinite;
		int             e = extract_xml_pparse_next(buffer, &tag);

		if (e == 1) break;
		if (e) goto end;
		if (!strcmp(tag.name, "?xml")) {

			continue;
		}
		if (strcmp(tag.name, "page")) {
			outf("Expected <page> but tag.name='%s'", tag.name);
			errno = ESRCH;
			goto end;
		}
		outfx("loading spans for page %i...", document->pages_num);
		if (extract_page_begin(extract, mediabox.min.x, mediabox.min.y, mediabox.max.x, mediabox.max.y)) goto end;
		page = extract->document.pages[extract->document.pages_num-1];
		if (!page) goto end;
		subpage = page->subpages[page->subpages_num-1];
		if (!subpage) goto end;

		for(;;) {
			if (extract_xml_pparse_next(buffer, &tag)) goto end;
			if (!strcmp(tag.name, "/page")) {
				num_spans += content_count_spans(&subpage->content);
				break;
			}
			if (!strcmp(tag.name, "image")) {
				const char* type = extract_xml_tag_attributes_find(&tag, "type");
				if (!type) {
					errno = EINVAL;
					goto end;
				}
				outf("image type=%s", type);
				if (!strcmp(type, "pixmap")) {
					int w;
					int h;
					int y;
					if (extract_xml_tag_attributes_find_int(&tag, "w", &w)) goto end;
					if (extract_xml_tag_attributes_find_int(&tag, "h", &h)) goto end;
					for (y=0; y<h; ++y) {
						int yy;
						if (extract_xml_pparse_next(buffer, &tag)) goto end;
						if (strcmp(tag.name, "line")) {
							outf("Expected <line> but tag.name='%s'", tag.name);
							errno = ESRCH;
							goto end;
						}
						if (extract_xml_tag_attributes_find_int(&tag, "y", &yy)) goto end;
						if (yy != y) {
							outf("Expected <line y=%i> but found <line y=%i>", y, yy);
							errno = ESRCH;
							goto end;
						}
						if (extract_xml_pparse_next(buffer, &tag)) goto end;
						if (strcmp(tag.name, "/line")) {
							outf("Expected </line> but tag.name='%s'", tag.name);
							errno = ESRCH;
							goto end;
						}
					}
				}
				else {

					size_t      image_data_size;
					const char *c;
					size_t      i;
					if (extract_xml_tag_attributes_find_size(&tag, "datasize", &image_data_size)) goto end;
					if (extract_malloc(extract->alloc, &image_data, image_data_size)) goto end;
					c = tag.text.chars;
					for(i=0;;) {
						int byte = 0;
						int cc;
						cc = *c;
						c += 1;
						if (cc == ' ' || cc == '\n') continue;
						if (cc >= '0' && cc <= '9') byte += cc-'0';
						else if (cc >= 'a' && cc <= 'f') byte += 10 + cc - 'a';
						else goto compressed_error;
						byte *= 16;

						cc = *c;
						c += 1;
						if (cc >= '0' && cc <= '9') byte += cc-'0';
						else if (cc >= 'a' && cc <= 'f') byte += 10 + cc - 'a';
						else goto compressed_error;

						image_data[i] = (char) byte;
						i += 1;
						if (i == image_data_size) {
							break;
						}
						continue;

						compressed_error:
						outf("Unrecognised hex character '%x' at offset %lli in image data", cc, (long long) (c-tag.text.chars));
						errno = EINVAL;
						goto end;
					}
					if (extract_add_image(
							extract,
							type,
							0 ,
							0 ,
							0 ,
							0 ,
							image_data,
							image_data_size,
							image_free_fn,
							NULL
							))
					{
						goto end;
					}
					image_data = NULL;
				}
				if (extract_xml_pparse_next(buffer, &tag)) goto end;
				if (strcmp(tag.name, "/image")) {
					outf("Expected </image> but tag.name='%s'", tag.name);
					errno = ESRCH;
					goto end;
				}
				continue;
			}
			if (strcmp(tag.name, "span")) {
				outf("Expected <span> but tag.name='%s'", tag.name);
				errno = ESRCH;
				goto end;
			}

			{
				matrix_t  ctm;
				matrix_t  trm;
				char     *font_name;
				char     *font_name2;
				int       font_bold;
				int       font_italic;
				int       wmode;
				if (s_matrix_read(extract_xml_tag_attributes_find(&tag, "ctm"), &ctm)) goto end;
				if (s_matrix_read(extract_xml_tag_attributes_find(&tag, "trm"), &trm)) goto end;
				font_name = extract_xml_tag_attributes_find(&tag, "font_name");
				if (!font_name) {
					outf("Failed to find attribute 'font_name'");
					goto end;
				}
				font_name2 = strchr(font_name, '+');
				if (font_name2)  font_name = font_name2 + 1;
				font_bold = strstr(font_name, "-Bold") ? 1 : 0;
				font_italic = strstr(font_name, "-Oblique") ? 1 : 0;
				if (extract_xml_tag_attributes_find_int(&tag, "wmode", &wmode)) goto end;
				if (extract_span_begin(extract,
									   font_name,
									   font_bold,
									   font_italic,
									   wmode,
									   ctm.a,
									   ctm.b,
									   ctm.c,
									   ctm.d,
									   0,0,0,0)) goto end;

				for(;;) {
					double       x;
					double       y;
					double       adv;
					unsigned int ucs;

					if (extract_xml_pparse_next(buffer, &tag)) {
						outf("Failed to find <char or </span");
						goto end;
					}
					if (!strcmp(tag.name, "/span")) {
						break;
					}
					if (strcmp(tag.name, "char")) {
						errno = ESRCH;
						outf("Expected <char> but tag.name='%s'", tag.name);
						goto end;
					}

					if (extract_xml_tag_attributes_find_double(&tag, "x", &x)) goto end;
					if (extract_xml_tag_attributes_find_double(&tag, "y", &y)) goto end;
					if (extract_xml_tag_attributes_find_double(&tag, "adv", &adv)) goto end;
					if (extract_xml_tag_attributes_find_uint(&tag, "ucs", &ucs)) goto end;

					if (extract_add_char(extract, x, y, ucs, adv, x, y, x + adv, y + adv)) goto end;
				}

				extract_xml_tag_free(extract->alloc, &tag);
			}
		}
		if (extract_page_end(extract)) goto end;
		outf("page=%i subpage->num_spans=%i",
				document->pages_num, content_count_spans(&subpage->content));
	}

	outf("num_spans=%i num_spans_split=%i num_spans_autosplit=%i",
			num_spans,
			extract->num_spans_split,
			extract->num_spans_autosplit
			);

	ret = 0;
end:

	extract_xml_tag_free(extract->alloc, &tag);
	extract_free(extract->alloc, &image_data);

	return ret;
}

int
extract_span_begin(
		extract_t  *extract,
		const char *font_name,
		int         font_bold,
		int         font_italic,
		int         wmode,
		double      ctm_a,
		double      ctm_b,
		double      ctm_c,
		double      ctm_d,
		double      bbox_x0,
		double      bbox_y0,
		double      bbox_x1,
		double      bbox_y1)
{
	int             e = -1;
	extract_page_t *page;
	subpage_t      *subpage;
	span_t         *span;
	document_t     *document = &extract->document;

	assert(document->pages_num > 0);
	page = document->pages[document->pages_num-1];
	subpage = page->subpages[page->subpages_num-1];
	outf("extract_span_begin(): ctm=(%f %f %f %f) font_name=%s, wmode=%i",
		 ctm_a,
		 ctm_b,
		 ctm_c,
		 ctm_d,
		 font_name,
		 wmode);
	if (content_append_new_span(extract->alloc, &subpage->content, &span, document->current)) goto end;
	span->ctm.a = ctm_a;
	span->ctm.b = ctm_b;
	span->ctm.c = ctm_c;
	span->ctm.d = ctm_d;
	span->font_bbox.min.x = bbox_x0;
	span->font_bbox.min.y = bbox_y0;
	span->font_bbox.max.x = bbox_x1;
	span->font_bbox.max.y = bbox_y1;

	{
		const char *ff = strchr(font_name, '+');
		const char *f = (ff) ? ff+1 : font_name;
		if (extract_strdup(extract->alloc, f, &span->font_name)) goto end;
		span->flags.font_bold = font_bold ? 1 : 0;
		span->flags.font_italic = font_italic ? 1 : 0;
		span->flags.wmode = wmode ? 1 : 0;
		extract->span_offset_x = 0;
		extract->span_offset_y = 0;
	}

	e = 0;
end:

	return e;
}

static span_t *
split_to_new_span(extract_alloc_t *alloc, content_root_t *content, span_t *span0)
{
	content_t  save;
	span_t    *span;
	char      *name;

	if (extract_strdup(alloc, span0->font_name, &name))
		return NULL;

	if (content_append_new_span(alloc, content, &span, span0->structure))
	{
		extract_free(alloc, &name);
		return NULL;
	}

	save = span->base;
	*span = *span0;
	span->base = save;
	span->font_name = name;
	span->chars = NULL;
	span->chars_num = 0;

	return span;
}

static span_t *
find_previous_non_space_char_ish(content_root_t *content, int *char_num, int *intervening_space)
{
	content_t *s;
	int i;

	*intervening_space = 0;
	for (s = content->base.prev; s != &content->base; s = s->prev)
	{
		span_t *span = (span_t *)s;

		if (s->type != content_span)
			continue;

		for (i = span->chars_num-1; i >= 0; i--)
		{
			if (span->chars[i].ucs != 32 || i == 0)
			{
				*char_num = i;
				return span;
			}
			*intervening_space = 1;
		}
	}

	return NULL;
}

point_t
extract_predicted_end_of_char(char_t *char_, const span_t *span)
{
	double adv = char_->adv;
	point_t dir = { adv * (1 - span->flags.wmode), adv * span->flags.wmode };

	dir = extract_matrix4_transform_point(span->ctm, dir);
	dir.x += char_->x;
	dir.y += char_->y;

	return dir;
}

point_t
extract_end_of_span(const span_t *span)
{
	assert(span && span->chars_num > 0);
	return extract_predicted_end_of_char(&span->chars[span->chars_num-1], span);
}

int extract_add_char(
		extract_t    *extract,
		double        x,
		double        y,
		unsigned int  ucs,
		double        adv,
		double        x0,
		double        y0,
		double        x1,
		double        y1)
{
	int             e       = -1;
	char_t         *char_;
	extract_page_t *page    = extract->document.pages[extract->document.pages_num-1];
	subpage_t      *subpage = page->subpages[page->subpages_num-1];
	span_t         *span    = content_last_span(&subpage->content);
	span_t         *span0;
	int             char_num0;
	double          dist, perp, scale_squared;
	point_t         dir;
	int             intervening_space;

	if (span->flags.wmode)
	{
		dir.x = 0;
		dir.y = 1;
		scale_squared = span->ctm.c * span->ctm.c + span->ctm.d * span->ctm.d;
	}
	else
	{
		dir.x = 1;
		dir.y = 0;
		scale_squared = span->ctm.a * span->ctm.a + span->ctm.b * span->ctm.b;
	}
	dir = extract_matrix4_transform_point(span->ctm, dir);

	outf("(%f %f) ucs=% 5i=%c adv=%f", x, y, ucs, (ucs >=32 && ucs< 127) ? ucs : ' ', adv);

	span0 = find_previous_non_space_char_ish(&subpage->content, &char_num0, &intervening_space);

	if (span0 && span0->structure != extract->document.current)
		span0 = NULL;

	if (span0 == NULL)
	{

		outf("%c x=%g y=%g adv=%g\n", ucs, x, y, adv);
	}
	else
	{

		char_t *char_prev = &span0->chars[char_num0];
		double adv0 = char_prev->adv;
		point_t predicted_end_of_char0 = extract_predicted_end_of_char(char_prev, span0);

		double space_guess = (adv0 + adv)/2 * extract->master_space_guess;

		dist = (x - predicted_end_of_char0.x) * dir.x + (y - predicted_end_of_char0.y) * dir.y;

		perp = (x - predicted_end_of_char0.x) * dir.y - (y - predicted_end_of_char0.y) * dir.x;

		dist /= scale_squared;
		perp /= scale_squared;

		outf("%c x=%g y=%g adv=%g dist=%g perp=%g\n", ucs, x, y, adv, dist, perp);

		if (fabs(perp) > 3*space_guess/2 || fabs(dist) > space_guess * 8)
		{

			if (span->chars_num > 0)
			{
				extract->num_spans_autosplit += 1;
				span = split_to_new_span(extract->alloc, &subpage->content, span);
				if (span == NULL) goto end;
			}
		}
		else if (intervening_space)
		{

			if (dist < space_guess/3)
			{
				if (span->chars_num > 0)
				{
					span->chars_num--;

				}
				else
				{
					span_t *space_span = content_prev_span(&span->base);
					assert(space_span->chars_num > 0);
					space_span->chars_num--;
					if (space_span->chars_num == 0)
						extract_span_free(extract->alloc, &space_span);
				}
			}
		}

		else if (!intervening_space && dist > 2*space_guess/3)
		{

			char_ = extract_span_append_c(extract->alloc, span, ' ');
			if (char_ == NULL) goto end;

			char_->x = predicted_end_of_char0.x;
			char_->y = predicted_end_of_char0.y;
		}
	}

	char_ = extract_span_append_c(extract->alloc, span, ucs);
	if (char_ == NULL) goto end;

	char_->x = x;
	char_->y = y;

	char_->adv = adv;
	char_->bbox.min.x = x0;
	char_->bbox.min.y = y0;
	char_->bbox.max.x = x1;
	char_->bbox.max.y = y1;

	e = 0;
end:

	if (span && span->chars_num == 0)
	{
		extract_span_free(extract->alloc, &span);
	}

	return e;
}

int extract_span_end(extract_t *extract)
{
	extract_page_t *page    = extract->document.pages[extract->document.pages_num-1];
	subpage_t      *subpage = page->subpages[page->subpages_num-1];
	span_t         *span    = content_last_span(&subpage->content);

	if (span->chars_num == 0) {

		extract_span_free(extract->alloc, &span);
	}

	return 0;
}

int extract_add_image(
		extract_t               *extract,
		const char              *type,
		double                   x,
		double                   y,
		double                   w,
		double                   h,
		void                    *data,
		size_t                   data_size,
		extract_image_data_free  data_free,
		void                    *data_free_handle)
{
	int             e       = -1;
	extract_page_t *page    = extract->document.pages[extract->document.pages_num-1];
	subpage_t      *subpage = page->subpages[page->subpages_num-1];
	image_t        *image;

	extract->image_n += 1;
	if (content_append_new_image(extract->alloc, &subpage->content, &image)) goto end;
	image->x = x;
	image->y = y;
	image->w = w;
	image->h = h;
	image->data = data;
	image->data_size = data_size;
	image->data_free = data_free;
	image->data_free_handle = data_free_handle;
	if (extract_strdup(extract->alloc, type, &image->type)) goto end;
	if (extract_asprintf(extract->alloc, &image->id, "rId%i", extract->image_n) < 0) goto end;
	if (extract_asprintf(extract->alloc, &image->name, "image%i.%s", extract->image_n, image->type) < 0) goto end;

	subpage->images_num += 1;
	outf("subpage->images_num=%i", subpage->images_num);

	e = 0;
end:

	if (e) {
		extract_image_free(extract->alloc, &image);
	}

	return e;
}

static int tablelines_append(extract_alloc_t *alloc, tablelines_t *tablelines, rect_t *rect, double color)
{
	if (extract_realloc(
			alloc,
			&tablelines->tablelines,
			sizeof(*tablelines->tablelines) * (tablelines->tablelines_num + 1)
			)) return -1;
	tablelines->tablelines[ tablelines->tablelines_num].rect = *rect;
	tablelines->tablelines[ tablelines->tablelines_num].color = (float) color;
	tablelines->tablelines_num += 1;

	return 0;
}

static point_t transform(
		double x,
		double y,
		double ctm_a,
		double ctm_b,
		double ctm_c,
		double ctm_d,
		double ctm_e,
		double ctm_f)
{
	point_t ret;

	ret.x = ctm_a * x + ctm_b * y + ctm_e;
	ret.y = ctm_c * x + ctm_d * y + ctm_f;

	return ret;
}

static double s_min(double a, double b)
{
	return (a < b) ? a : b;
}

static double s_max(double a, double b)
{
	return (a > b) ? a : b;
}

int extract_add_path4(
		extract_t *extract,
		double     ctm_a,
		double     ctm_b,
		double     ctm_c,
		double     ctm_d,
		double     ctm_e,
		double     ctm_f,
		double     x0,
		double     y0,
		double     x1,
		double     y1,
		double     x2,
		double     y2,
		double     x3,
		double     y3,
		double     color)
{
	extract_page_t *page = extract->document.pages[extract->document.pages_num-1];
	subpage_t      *subpage = page->subpages[page->subpages_num-1];
	point_t         points[4] = {
				transform(x0, y0, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f),
				transform(x1, y1, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f),
				transform(x2, y2, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f),
				transform(x3, y3, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f)
			};
	rect_t          rect;
	int             i;
	double          dx, dy;

	outf("cmt=(%f %f %f %f %f %f) points=[(%f %f) (%f %f) (%f %f) (%f %f)]",
			ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f,
			x0, y0, x1, y1, x2, y2, x3, y3
			);
	outf("extract_add_path4(): [(%f %f) (%f %f) (%f %f) (%f %f)]",
			x0, y0, x1, y1, x2, y2, x3, y3);

	for (i=0; i<4; ++i)
	{
		if (points[(i+1) % 4].x > points[(i+0) % 4].x)	break;
	}
	outf("i=%i", i);
	if (i == 4) return 0;
	rect.min.x = points[(i+0) % 4].x;
	rect.max.x = points[(i+1) % 4].x;
	if (points[(i+2) % 4].x != rect.max.x)  return 0;
	if (points[(i+3) % 4].x != rect.min.x)  return 0;
	y0 = points[(i+1) % 4].y;
	y1 = points[(i+2) % 4].y;
	if (y0 == y1)   return 0;
	if (points[(i+3) % 4].y != y1)  return 0;
	if (points[(i+4) % 4].y != y0)  return 0;
	rect.min.y = (y1 > y0) ? y0 : y1;
	rect.max.y = (y1 > y0) ? y1 : y0;

	dx = rect.max.x - rect.min.x;
	dy = rect.max.y - rect.min.y;
	if (dx / dy > 5)
	{

		outf("have found horizontal line: %s", extract_rect_string(&rect));
		if (tablelines_append(extract->alloc, &subpage->tablelines_horizontal, &rect, color)) return -1;
	}
	else if (dy / dx > 5)
	{

		outf("have found vertical line: %s", extract_rect_string(&rect));
		if (tablelines_append(extract->alloc, &subpage->tablelines_vertical, &rect, color)) return -1;
	}

	return 0;
}

int extract_add_line(
		extract_t *extract,
		double     ctm_a,
		double     ctm_b,
		double     ctm_c,
		double     ctm_d,
		double     ctm_e,
		double     ctm_f,
		double     width,
		double     x0,
		double     y0,
		double     x1,
		double     y1,
		double    color)
{
	extract_page_t *page = extract->document.pages[extract->document.pages_num-1];
	subpage_t      *subpage = page->subpages[page->subpages_num-1];
	point_t         p0 = transform(x0, y0, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f);
	point_t         p1 = transform(x1, y1, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f);
	double          width2 = width * sqrt( fabs( ctm_a * ctm_d - ctm_b * ctm_c));
	rect_t          rect;

	(void)color;
	rect.min.x = s_min(p0.x, p1.x);
	rect.min.y = s_min(p0.y, p1.y);
	rect.max.x = s_max(p0.x, p1.x);
	rect.max.y = s_max(p0.y, p1.y);

	outf("%s: width=%f ((%f %f)(%f %f)) rect=%s",
			extract_FUNCTION,
			width,
			x0, y0, x1, y1,
			extract_rect_string(&rect)
			);
	if (rect.min.x == rect.max.x && rect.min.y == rect.max.y)
	{
	}
	else if (rect.min.x == rect.max.x)
	{
		rect.min.x -= width2 / 2;
		rect.max.x += width2 / 2;
		return tablelines_append(extract->alloc, &subpage->tablelines_vertical, &rect, color);
	}
	else if (rect.min.y == rect.max.y)
	{
		rect.min.y -= width2 / 2;
		rect.max.y += width2 / 2;
		return tablelines_append(extract->alloc, &subpage->tablelines_horizontal, &rect, color);
	}

	return 0;
}

int extract_subpage_alloc(extract_alloc_t *alloc, rect_t mediabox, extract_page_t *page, subpage_t **psubpage)
{
	subpage_t *subpage;

	if (extract_malloc(alloc, psubpage, sizeof(subpage_t)))
	{
		return -1;
	}
	subpage = *psubpage;
	subpage->mediabox = mediabox;
	content_init_root(&subpage->content, NULL);
	subpage->images_num = 0;
	subpage->tablelines_horizontal.tablelines = NULL;
	subpage->tablelines_horizontal.tablelines_num = 0;
	subpage->tablelines_vertical.tablelines = NULL;
	subpage->tablelines_vertical.tablelines_num = 0;
	content_init_root(&subpage->tables, NULL);

	if (extract_realloc2(alloc,
			&page->subpages,
			sizeof(subpage_t*) * page->subpages_num,
			sizeof(subpage_t*) * (page->subpages_num + 1)))
	{
		extract_free(alloc, psubpage);
		return -1;
	}
	page->subpages[page->subpages_num] = subpage;
	page->subpages_num += 1;

	return 0;
}

static int extract_subpage_begin(extract_t *extract, double x0, double y0, double x1, double y1)
{
	extract_page_t *page = extract->document.pages[extract->document.pages_num - 1];
	subpage_t      *subpage;
	rect_t          mediabox = { { x0, y0 }, { x1, y1 } };
	int             e;

	e = extract_subpage_alloc(extract->alloc, mediabox, page, &subpage);

	if (e == 0)
	{
	}

	return e;
}

int extract_page_begin(extract_t *extract, double x0, double y0, double x1, double y1)
{
	extract_page_t *page;

	if (extract_malloc(extract->alloc, &page, sizeof(*page))) return -1;
	page->mediabox.min.x = x0;
	page->mediabox.min.y = y0;
	page->mediabox.max.x = x1;
	page->mediabox.max.y = y1;
	page->subpages = NULL;
	page->subpages_num = 0;
	page->split = NULL;

	if (extract_realloc2(
			extract->alloc,
			&extract->document.pages,
			sizeof(subpage_t*) * extract->document.pages_num,
			sizeof(subpage_t*) * (extract->document.pages_num + 1)
			)) {
		extract_free(extract->alloc, &page);
		return -1;
	}

	extract->document.pages[extract->document.pages_num] = page;
	extract->document.pages_num += 1;

	if (extract_subpage_begin(extract, x0, y0, x1, y1)) {
		extract->document.pages_num--;
		page_free(extract->alloc, &extract->document.pages[extract->document.pages_num]);
		return -1;
	}

	return 0;
}

int extract_fill_begin(
		extract_t *extract,
		double     ctm_a,
		double     ctm_b,
		double     ctm_c,
		double     ctm_d,
		double     ctm_e,
		double     ctm_f,
		double     color)
{
	assert(extract->path_type == path_type_NONE);

	extract->path_type = path_type_FILL;
	extract->path.fill.color = color;
	extract->path.fill.n = 0;
	extract->path.fill.ctm.a = ctm_a;
	extract->path.fill.ctm.b = ctm_b;
	extract->path.fill.ctm.c = ctm_c;
	extract->path.fill.ctm.d = ctm_d;
	extract->path.fill.ctm.e = ctm_e;
	extract->path.fill.ctm.f = ctm_f;

	return 0;
}

int extract_stroke_begin(
		extract_t *extract,
		double     ctm_a,
		double     ctm_b,
		double     ctm_c,
		double     ctm_d,
		double     ctm_e,
		double     ctm_f,
		double     line_width,
		double     color)
{
	assert(extract->path_type == path_type_NONE);

	extract->path_type = path_type_STROKE;
	extract->path.stroke.ctm.a = ctm_a;
	extract->path.stroke.ctm.b = ctm_b;
	extract->path.stroke.ctm.c = ctm_c;
	extract->path.stroke.ctm.d = ctm_d;
	extract->path.stroke.ctm.e = ctm_e;
	extract->path.stroke.ctm.f = ctm_f;
	extract->path.stroke.width = line_width;
	extract->path.stroke.color = color;
	extract->path.stroke.point0_set = 0;
	extract->path.stroke.point_set = 0;

	return 0;
}

int extract_moveto(extract_t *extract, double x, double y)
{
	if (extract->path_type == path_type_FILL)
	{
		if (extract->path.fill.n == -1) return 0;
		if (extract->path.fill.n != 0)
		{
			outf0("returning error. extract->path.fill.n=%i", extract->path.fill.n);
			extract->path.fill.n = -1;
			return 0;
		}
		extract->path.fill.points[extract->path.fill.n].x = x;
		extract->path.fill.points[extract->path.fill.n].y = y;
		extract->path.fill.n += 1;
		return 0;
	}
	else if (extract->path_type == path_type_STROKE)
	{
		extract->path.stroke.point.x = x;
		extract->path.stroke.point.y = y;
		extract->path.stroke.point_set = 1;
		if (!extract->path.stroke.point0_set)
		{
			extract->path.stroke.point0 = extract->path.stroke.point;
			extract->path.stroke.point0_set = 1;
		}
		return 0;
	}
	else
	{
		assert(0);
		return -1;
	}
}

int extract_lineto(extract_t *extract, double x, double y)
{
	if (extract->path_type == path_type_FILL)
	{
		if (extract->path.fill.n == -1)	return 0;
		if (extract->path.fill.n == 0 || extract->path.fill.n >= 4)
		{
			outf0("returning error. extract->path.fill.n=%i", extract->path.fill.n);
			extract->path.fill.n = -1;
			return 0;
		}
		extract->path.fill.points[extract->path.fill.n].x = x;
		extract->path.fill.points[extract->path.fill.n].y = y;
		extract->path.fill.n += 1;
		return 0;
	}
	else if (extract->path_type == path_type_STROKE)
	{
		if (extract->path.stroke.point_set)
		{
			if (extract_add_line(
					extract,
					extract->path.stroke.ctm.a,
					extract->path.stroke.ctm.b,
					extract->path.stroke.ctm.c,
					extract->path.stroke.ctm.d,
					extract->path.stroke.ctm.e,
					extract->path.stroke.ctm.f,
					extract->path.stroke.width,
					extract->path.stroke.point.x,
					extract->path.stroke.point.y,
					x,
					y,
					extract->path.stroke.color))
			{
				return -1;
			}
		}
		extract->path.stroke.point.x = x;
		extract->path.stroke.point.y = y;
		extract->path.stroke.point_set = 1;
		if (!extract->path.stroke.point0_set)
		{
			extract->path.stroke.point0 = extract->path.stroke.point;
			extract->path.stroke.point0_set = 1;
		}
		return 0;
	}
	else
	{
		assert(0);
		return -1;
	}
}

int extract_closepath(extract_t *extract)
{
	if (extract->path_type == path_type_FILL)
	{
		if (extract->path.fill.n == 4)
		{

			int e;
			e = extract_add_path4(
					extract,
					extract->path.fill.ctm.a,
					extract->path.fill.ctm.b,
					extract->path.fill.ctm.c,
					extract->path.fill.ctm.d,
					extract->path.fill.ctm.e,
					extract->path.fill.ctm.f,
					extract->path.fill.points[0].x,
					extract->path.fill.points[0].y,
					extract->path.fill.points[1].x,
					extract->path.fill.points[1].y,
					extract->path.fill.points[2].x,
					extract->path.fill.points[2].y,
					extract->path.fill.points[3].x,
					extract->path.fill.points[3].y,
					extract->path.fill.color);
			if (e) return e;
		}
		extract->path.fill.n = 0;
		return 0;
	}
	else if (extract->path_type == path_type_STROKE)
	{
		if (extract->path.stroke.point0_set && extract->path.stroke.point_set)
		{
			if (extract_add_line(
					extract,
					extract->path.stroke.ctm.a,
					extract->path.stroke.ctm.b,
					extract->path.stroke.ctm.c,
					extract->path.stroke.ctm.d,
					extract->path.stroke.ctm.e,
					extract->path.stroke.ctm.f,
					extract->path.stroke.width,
					extract->path.stroke.point.x,
					extract->path.stroke.point.y,
					extract->path.stroke.point0.x,
					extract->path.stroke.point0.y,
					extract->path.stroke.color))
			{
				return -1;
			}
			return 0;
		}
		extract->path.stroke.point = extract->path.stroke.point0;
		return 0;
	}
	else
	{
		assert(0);
		return -1;
	}
}

int extract_fill_end(extract_t *extract)
{
	assert(extract->path_type == path_type_FILL);
	extract->path_type = path_type_NONE;

	return 0;
}

int extract_stroke_end(extract_t *extract)
{
	assert(extract->path_type == path_type_STROKE);
	extract->path_type = path_type_NONE;

	return 0;
}

static int extract_subpage_end(extract_t *extract)
{
	(void) extract;
	return 0;
}

int extract_page_end(extract_t *extract)
{
	if (extract_subpage_end(extract))
		return -1;

	return 0;
}

int extract_begin_struct(extract_t *extract, extract_struct_t type, int uid, int score)
{
	document_t  *document = &extract->document;
	structure_t *structure;

	if (extract_malloc(extract->alloc, &structure, sizeof(*structure)))
		return -1;

	structure->parent = document->current;
	structure->sibling_next = NULL;
	structure->sibling_prev = NULL;
	structure->kids_first = NULL;
	structure->kids_tail = &structure->kids_first;
	structure->type = type;
	structure->score = score;
	structure->uid = uid;

	if (document->current == NULL)
	{

		document->current = structure;
		document->structure = structure;
	}
	else
	{

		*document->current->kids_tail = structure;
		document->current->kids_tail = &structure->sibling_next;
		document->current = structure;
	}

	return 0;
}

int extract_end_struct(extract_t *extract)
{
	document_t *document = &extract->document;

	assert(document->current != NULL);

	document->current = document->current->parent;

	return 0;
}

const char *extract_struct_string(extract_struct_t type)
{
	switch (type)
	{
	default:
		return "UNKNOWN";
	case extract_struct_INVALID:
		return "INVALID";
	case extract_struct_UNDEFINED:
		return "UNDEFINED";
	case extract_struct_DOCUMENT:
		return "DOCUMENT";
	case extract_struct_PART:
		return "PART";
	case extract_struct_ART:
		return "ART";
	case extract_struct_SECT:
		return "SECT";
	case extract_struct_DIV:
		return "DIV";
	case extract_struct_BLOCKQUOTE:
		return "BLOCKQUOTE";
	case extract_struct_CAPTION:
		return "CAPTION";
	case extract_struct_TOC:
		return "TOC";
	case extract_struct_TOCI:
		return "TOCI";
	case extract_struct_INDEX:
		return "INDEX";
	case extract_struct_NONSTRUCT:
		return "NONSTRUCT";
	case extract_struct_PRIVATE:
		return "PRIVATE";
	case extract_struct_DOCUMENTFRAGMENT:
		return "DOCUMENTFRAGMENT";
	case extract_struct_ASIDE:
		return "ASIDE";
	case extract_struct_TITLE:
		return "TITLE";
	case extract_struct_FENOTE:
		return "FENOTE";
	case extract_struct_SUB:
		return "SUB";
	case extract_struct_P:
		return "P";
	case extract_struct_H:
		return "H";
	case extract_struct_H1:
		return "H1";
	case extract_struct_H2:
		return "H2";
	case extract_struct_H3:
		return "H3";
	case extract_struct_H4:
		return "H4";
	case extract_struct_H5:
		return "H5";
	case extract_struct_H6:
		return "H6";
	case extract_struct_LIST:
		return "LIST";
	case extract_struct_LISTITEM:
		return "LISTITEM";
	case extract_struct_LABEL:
		return "LABEL";
	case extract_struct_LISTBODY:
		return "LISTBODY";
	case extract_struct_TABLE:
		return "TABLE";
	case extract_struct_TR:
		return "TR";
	case extract_struct_TH:
		return "TH";
	case extract_struct_TD:
		return "TD";
	case extract_struct_THEAD:
		return "THEAD";
	case extract_struct_TBODY:
		return "TBODY";
	case extract_struct_TFOOT:
		return "TFOOT";
	case extract_struct_SPAN:
		return "SPAN";
	case extract_struct_QUOTE:
		return "QUOTE";
	case extract_struct_NOTE:
		return "NOTE";
	case extract_struct_REFERENCE:
		return "REFERENCE";
	case extract_struct_BIBENTRY:
		return "BIBENTRY";
	case extract_struct_CODE:
		return "CODE";
	case extract_struct_LINK:
		return "LINK";
	case extract_struct_ANNOT:
		return "ANNOT";
	case extract_struct_EM:
		return "EM";
	case extract_struct_STRONG:
		return "STRONG";
	case extract_struct_RUBY:
		return "RUBY";
	case extract_struct_RB:
		return "RB";
	case extract_struct_RT:
		return "RT";
	case extract_struct_RP:
		return "RP";
	case extract_struct_WARICHU:
		return "WARICHU";
	case extract_struct_WT:
		return "WT";
	case extract_struct_WP:
		return "WP";
	case extract_struct_FIGURE:
		return "FIGURE";
	case extract_struct_FORMULA:
		return "FORMULA";
	case extract_struct_FORM:
		return "FORM";
	case extract_struct_ARTIFACT:
		return "ARTIFACT";
	}
}

static int
paragraph_to_text(
		extract_alloc_t   *alloc,
		paragraph_t       *paragraph,
		extract_astring_t *text)
{
	content_line_iterator  lit;
	line_t                *line;

	for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
	{
		content_span_iterator  sit;
		span_t                *span;

		for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
		{
			int c;

			for (c=0; c<span->chars_num; ++c)
			{

				char_t* char_ = &span->chars[c];
				unsigned cc = char_->ucs;
				if (extract_astring_catc_unicode(
						alloc,
						text,
						cc,
						0 ,
						1 ,
						1 ,
						1
						)) return -1;
			}
		}
	}
	if (extract_astring_catc(alloc, text, '\n')) return -1;

	return 0;
}

static int
paragraphs_to_text_content(
		extract_alloc_t   *alloc,
		content_root_t    *paragraphs,
		extract_astring_t *text)
{
	content_iterator  cit;
	content_t        *content;

	for (content = content_iterator_init(&cit, paragraphs); content != NULL; content = content_iterator_next(&cit))
	{
		if (content->type == content_paragraph)
		{
			if (paragraph_to_text(alloc, (paragraph_t *)content, text)) return -1;
		}
		else if (content->type == content_block)
		{
			block_t                    *block = (block_t *)content;
			content_paragraph_iterator  pit;
			paragraph_t                *paragraph;

			for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
			{
				if (paragraph_to_text(alloc, paragraph, text)) return -1;
			}
		}
	}
	return 0;
}

static int extract_write_tables_csv(extract_t *extract)
{
	int                ret = -1;
	int                p;
	char              *path = NULL;
	FILE              *f = NULL;
	extract_astring_t  text = {NULL, 0};

	if (!extract->tables_csv_format) return 0;

	outf("extract_write_tables_csv(): path_format=%s", extract->tables_csv_format);
	outf("extract->document.pages_num=%i", extract->document.pages_num);
	for (p=0; p<extract->document.pages_num; ++p)
	{
		int c;
		extract_page_t *page = extract->document.pages[p];
		for (c=0; c<page->subpages_num; ++c)
		{
			content_table_iterator  tit;
			table_t                *table;
			subpage_t              *subpage = page->subpages[c];

			outf("p=%i subpage->tables_num=%i", p, content_count_tables(&subpage->tables));
			for (table = content_table_iterator_init(&tit, &subpage->tables); table != NULL; table = content_table_iterator_next(&tit))
			{
				int y;
				extract_free(extract->alloc, &path);
				if (extract_asprintf(extract->alloc, &path, extract->tables_csv_format, extract->tables_csv_i) < 0) goto end;
				extract->tables_csv_i += 1;
				outf("Writing table to: %s", path);
				outf("table->cells_num_x=%i", table->cells_num_x);
				outf("table->cells_num_y=%i", table->cells_num_y);
				f = fopen(path, "w");
				if (!f) goto end;
				for (y=0; y<table->cells_num_y; ++y)
				{
					int x;
					int have_output = 0;
					for (x=0; x<table->cells_num_x; ++x)
					{
						cell_t* cell = table->cells[table->cells_num_x * y + x];
						extract_astring_free(extract->alloc, &text);
						if (y==0)
						{
							outf("y=0 x=%i cell->rect=%s", x, extract_rect_string(&cell->rect));
						}
						if (have_output) fprintf(f, ",");
						have_output = 1;
						if (paragraphs_to_text_content(
								extract->alloc,
								&cell->content,
								&text
								)) goto end;

						extract_astring_char_truncate_if(&text, ' ');
						fprintf(f, "\"%s\"", text.chars ? text.chars : "");
					}
					fprintf(f, "\n");
				}
				fclose(f);
				f = NULL;
			}
		}
	}

	ret = 0;
end:

	if (f) fclose(f);
	extract_free(extract->alloc, &path);
	extract_astring_free(extract->alloc, &text);

	return ret;
}

int extract_process(
		extract_t *extract,
		int        spacing,
		int        rotation,
		int        images)
{
	int e = -1;

	if (extract_realloc2(
			extract->alloc,
			&extract->contentss,
			sizeof(*extract->contentss) * extract->contentss_num,
			sizeof(*extract->contentss) * (extract->contentss_num + 1)
			)) goto end;
	extract_astring_init(&extract->contentss[extract->contentss_num]);
	extract->contentss_num += 1;

	if (extract_document_join(extract->alloc, &extract->document, extract->layout_analysis, extract->master_space_guess)) goto end;

	switch (extract->format)
	{
	case extract_format_ODT:
		if (extract_document_to_odt_content(
				extract->alloc,
				&extract->document,
				spacing,
				rotation,
				images,
				&extract->contentss[extract->contentss_num - 1],
				&extract->odt_styles
				)) goto end;
		break;
	case extract_format_DOCX:
		if (extract_document_to_docx_content(
				extract->alloc,
				&extract->document,
				spacing,
				rotation,
				images,
				&extract->contentss[extract->contentss_num - 1]
				)) goto end;
		break;
	case extract_format_HTML:
		if (extract_document_to_html_content(
				extract->alloc,
				&extract->document,
				rotation,
				images,
				&extract->contentss[extract->contentss_num - 1]
				)) goto end;
		break;
	case extract_format_JSON:
		if (extract_document_to_json_content(
				extract->alloc,
				&extract->document,
				rotation,
				images,
				&extract->contentss[extract->contentss_num - 1]
				)) goto end;
		break;
	case extract_format_TEXT:
	{
		int p;
		for (p=0; p<extract->document.pages_num; ++p)
		{
			extract_page_t* page = extract->document.pages[p];
			int c;
			for (c=0; c<page->subpages_num; ++c)
			{
				subpage_t* subpage = page->subpages[c];
				if (paragraphs_to_text_content(
						extract->alloc,
						&subpage->content,
						&extract->contentss[extract->contentss_num - 1]
					)) goto end;
			}
		}
		break;
	}
	default:
		outf0("Invalid format=%i", extract->format);
		assert(0);
		errno = EINVAL;
		return 1;
	}

	if (extract_document_images(extract->alloc, &extract->document, &extract->images)) goto end;

	if (extract->tables_csv_format)
	{
		extract_write_tables_csv(extract);
	}

	{
		int p;
		for (p=0; p<extract->document.pages_num; ++p) {
			page_free(extract->alloc, &extract->document.pages[p]);
		}
		extract_free(extract->alloc, &extract->document.pages);
		extract->document.pages_num = 0;
	}

	e = 0;
end:

	return e;
}

int extract_write(extract_t *extract, extract_buffer_t *buffer)
{
	int            e = -1;
	extract_zip_t *zip = NULL;
	char          *text2 = NULL;
	int            i;

	switch (extract->format)
	{
	case extract_format_ODT:
	{
		if (extract_zip_open(buffer, &zip)) goto end;
		for (i=0; i<odt_template_items_num; ++i) {
			const odt_template_item_t* item = &odt_template_items[i];
			extract_free(extract->alloc, &text2);
			outf("i=%i item->name=%s", i, item->name);
			if (extract_odt_content_item(
					extract->alloc,
					extract->contentss,
					extract->contentss_num,
					&extract->odt_styles,
					&extract->images,
					item->name,
					item->text,
					&text2
					))
			{
				goto end;
			}
			{
				const char* text3 = (text2) ? text2 : item->text;
				if (extract_zip_write_file(zip, text3, strlen(text3), item->name)) goto end;
			}
		}
		outf0("extract->images.images_num=%i", extract->images.images_num);
		for (i=0; i<extract->images.images_num; ++i) {
			image_t* image = extract->images.images[i];
			extract_free(extract->alloc, &text2);
			if (extract_asprintf(extract->alloc, &text2, "Pictures/%s", image->name) < 0) goto end;
			if (extract_zip_write_file(zip, image->data, image->data_size, text2)) goto end;
		}
		if (extract_zip_close(&zip)) goto end;
		break;
	}
	case extract_format_DOCX:
	{
		if (extract_zip_open(buffer, &zip)) goto end;
		for (i=0; i<docx_template_items_num; ++i) {
			const docx_template_item_t* item = &docx_template_items[i];
			extract_free(extract->alloc, &text2);
			outf("i=%i item->name=%s", i, item->name);
			if (extract_docx_content_item(
					extract->alloc,
					extract->contentss,
					extract->contentss_num,
					&extract->images,
					item->name,
					item->text,
					&text2
					))
			{
				goto end;
			}

			{
				const char* text3 = (text2) ? text2 : item->text;
				if (extract_zip_write_file(zip, text3, strlen(text3), item->name)) goto end;
			}
		}
		for (i=0; i<extract->images.images_num; ++i) {
			image_t* image = extract->images.images[i];
			extract_free(extract->alloc, &text2);
			if (extract_asprintf(extract->alloc, &text2, "word/media/%s", image->name) < 0) goto end;
			if (extract_zip_write_file(zip, image->data, image->data_size, text2)) goto end;
		}
		if (extract_zip_close(&zip)) goto end;
		break;
	}
	case extract_format_HTML:
	case extract_format_TEXT:
		for (i=0; i<extract->contentss_num; ++i)
		{
			if (extract_buffer_write(buffer, extract->contentss[i].chars, extract->contentss[i].chars_num, NULL)) goto end;
		}
		break;
	case extract_format_JSON:
	{
		int first = 1;
		if (extract_buffer_cat(buffer, "{\n\"elements\" : "))
			goto end;
		for (i=0; i<extract->contentss_num; ++i)
		{
			if (!first && extract_buffer_cat(buffer, ",\n"))
				goto end;
			if (extract->contentss[i].chars_num > 0)
				first = 0;
			if (extract_buffer_write(buffer, extract->contentss[i].chars, extract->contentss[i].chars_num, NULL)) goto end;
		}
		if (extract_buffer_cat(buffer, "\n}\n"))
			goto end;
		break;
	}
	default:
		outf0("Invalid format=%i", extract->format);
		assert(0);
		errno = EINVAL;
		return 1;
	}

	e = 0;
end:

	if (e)
	{
		outf("failed: %s", strerror(errno));
		extract_zip_close(&zip);
	}
	extract_free(extract->alloc, &text2);

	return e;
}

int extract_write_content(extract_t *extract, extract_buffer_t *buffer)
{
	int i;

	for (i=0; i<extract->contentss_num; ++i) {
		if (extract_buffer_write(
				buffer,
				extract->contentss[i].chars,
				extract->contentss[i].chars_num,
				NULL
				)) return -1;
	}

	return 0;
}

static int string_ends_with(const char *string, const char *end)
{
	size_t string_len = strlen(string);
	size_t end_len = strlen(end);

	if (end_len > string_len) return 0;

	return memcmp(string + string_len - end_len, end, end_len) == 0;
}

int extract_write_template(
		extract_t  *extract,
		const char *path_template,
		const char *path_out,
		int         preserve_dir)
{
	if (string_ends_with(path_out, ".odt"))
	{
		return extract_odt_write_template(
				extract->alloc,
				extract->contentss,
				extract->contentss_num,
				&extract->odt_styles,
				&extract->images,
				path_template,
				path_out,
				preserve_dir);
	}
	else
	{
		return extract_docx_write_template(
				extract->alloc,
				extract->contentss,
				extract->contentss_num,
				&extract->images,
				path_template,
				path_out,
				preserve_dir);
	}
}

void extract_end(extract_t **pextract)
{
	int i;
	extract_t *extract = *pextract;

	if (!extract) return;

	extract_document_free(extract->alloc, &extract->document);
	for (i=0; i<extract->contentss_num; ++i) {
		extract_astring_free(extract->alloc, &extract->contentss[i]);
	}
	extract_free(extract->alloc, &extract->contentss);
	extract_images_free(extract->alloc, &extract->images);
	extract_odt_styles_free(extract->alloc, &extract->odt_styles);

	extract_free(extract->alloc, pextract);
}

void extract_internal_end(void)
{
	extract_span_string(NULL, NULL);
}

void extract_exp_min(extract_t *extract, size_t size)
{
	extract_alloc_exp_min(extract->alloc, size);
}

double extract_font_size(matrix4_t *ctm)
{
	double font_size = extract_matrix_expansion(*ctm);

	font_size = (double) (int) (font_size * 100.0f + 0.5f) / 100.0f;

	return font_size;
}

rect_t extract_block_pre_rotation_bounds(block_t *block, double angle)
{
	content_paragraph_iterator  pit;
	paragraph_t                *paragraph;
	rect_t                      pre_box   = extract_rect_empty;
	matrix4_t                   unrotate, rotate;
	point_t                     centre, trans_centre;

	unrotate.a = cos(angle);
	unrotate.b = -sin(angle);
	unrotate.c = -unrotate.b;
	unrotate.d = unrotate.a;

	rotate.a = unrotate.a;
	rotate.b = -unrotate.b;
	rotate.c = -rotate.b;
	rotate.d = rotate.a;

	for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
	{
		content_line_iterator  lit;
		line_t				*line;

		for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
		{
			span_t  *span0 = content_first_span(&line->content);
			span_t  *span1 = content_last_span(&line->content);
			point_t  start = { span0->chars[0].x, span0->chars[0].y};
			point_t  end   = extract_end_of_span(span1);
			double   hoff  = span0->font_bbox.max.y - (span0->font_bbox.min.y < 0 ? span0->font_bbox.min.y : 0);

			outf("%f %f -> %f %f\n", start.x, start.y, end.x, end.y);
			start = extract_matrix4_transform_point(unrotate, start);
			end   = extract_matrix4_transform_point(unrotate, end);
			outf("   --------->	%f %f -> %f %f\n", start.x, start.y, end.x, end.y);

			hoff *= sqrt(span0->ctm.c * span0->ctm.c + span0->ctm.d * span0->ctm.d);

			if (start.y < end.y)
				start.y -= hoff;
			else
				end.y -= hoff;
			pre_box = extract_rect_union_point(pre_box, start);
			pre_box = extract_rect_union_point(pre_box, end);
		}
	}

	centre.x  = (pre_box.min.x + pre_box.max.x)/2;
	centre.y  = (pre_box.min.y + pre_box.max.y)/2;
	trans_centre = extract_matrix4_transform_point(rotate, centre);
#if 0
	{
		point_t centre2 = extract_matrix4_transform_point(unrotate, trans_centre);
		centre2 = centre2;
	}
#endif
#if 0
	printf("Centre of this paragraph should be %f %f\n", trans_centre.x, trans_centre.y);
#endif

	centre.x -= trans_centre.x;
	centre.y -= trans_centre.y;
	pre_box.min.x -= centre.x;
	pre_box.min.y -= centre.y;
	pre_box.max.x -= centre.x;
	pre_box.max.y -= centre.y;

#if 0

	{
		rect_t centred_box = { pre_box.min.x - trans_centre.x,
					pre_box.min.y - trans_centre.y,
					pre_box.max.x - trans_centre.x,
					pre_box.max.y - trans_centre.y };
		point_t corner;

		corner = extract_matrix4_transform_xy(rotate, centred_box.min.x, centred_box.min.y);
		corner.x += trans_centre.x;
		corner.y += trans_centre.y;
		printf("TL: %f %f\n", corner.x, corner.y);
		corner = extract_matrix4_transform_xy(rotate, centred_box.max.x, centred_box.min.y);
		corner.x += trans_centre.x;
		corner.y += trans_centre.y;
		printf("TR: %f %f\n", corner.x, corner.y);
		corner = extract_matrix4_transform_xy(rotate, centred_box.max.x, centred_box.max.y);
		corner.x += trans_centre.x;
		corner.y += trans_centre.y;
		printf("BR: %f %f\n", corner.x, corner.y);
		corner = extract_matrix4_transform_xy(rotate, centred_box.min.x, centred_box.max.y);
		corner.x += trans_centre.x;
		corner.y += trans_centre.y;
		printf("BL: %f %f\n", corner.x, corner.y);
	}
#endif

	{

		double extra = pre_box.max.y - pre_box.min.y;

		point_t offset = { 0, extra/2 };
		point_t toffset;
		pre_box.max.y += extra;
		toffset = extract_matrix4_transform_point(rotate, offset);
		pre_box.min.x += toffset.x - offset.x;
		pre_box.min.y += toffset.y - offset.y;
		pre_box.max.x += toffset.x - offset.x;
		pre_box.max.y += toffset.y - offset.y;
	}

	return pre_box;
}

double extract_baseline_angle(const matrix4_t *ctm)
{
	return atan2(ctm->b, ctm->a);
}

#include "extract/extract.h"

#include "memento.h"

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

static void content_state_init(content_state_t *content_state)
{
	content_state->font.name = NULL;
	content_state->font.size = 0;
	content_state->font.bold = 0;
	content_state->font.italic = 0;
	content_state->ctm_prev = NULL;
}

static int
content_state_reset(extract_alloc_t *alloc, content_state_t *content_state, extract_astring_t *content)
{
	int e = -1;

	if (content_state->font.bold)
	{
		if (extract_astring_cat(alloc, content, "</b>")) goto end;
		content_state->font.bold = 0;
	}
	if (content_state->font.italic)
	{
		if (extract_astring_cat(alloc, content, "</i>")) goto end;
		content_state->font.italic = 0;
	}

	e = 0;
end:

	return e;
}

static int
paragraph_to_html_content(
		extract_alloc_t   *alloc,
		content_state_t   *content_state,
		paragraph_t       *paragraph,
		int                single_line,
		extract_astring_t *content)
{
	int                    e = -1;
	const char            *endl = (single_line) ? "" : "\n";
	content_line_iterator  lit;
	line_t                *line;

	if (extract_astring_catf(alloc, content, "%s%s<p>", endl, endl)) goto end;

	for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
	{
		content_span_iterator  sit;
		span_t                *span;

		for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
		{
			int c;

			content_state->ctm_prev = &span->ctm;
			if (span->flags.font_bold != content_state->font.bold)
			{
				if (extract_astring_cat(alloc, content,
						span->flags.font_bold ? "<b>" : "</b>"
						)) goto end;
				content_state->font.bold = span->flags.font_bold;
			}
			if (span->flags.font_italic != content_state->font.italic)
			{
				if ( extract_astring_cat(alloc, content,
						span->flags.font_italic ? "<i>" : "</i>"
						)) goto end;
				content_state->font.italic = span->flags.font_italic;
			}

			for (c=0; c<span->chars_num; ++c)
			{
				char_t* char_ = &span->chars[c];
				if (extract_astring_catc_unicode_xml(alloc, content, char_->ucs)) goto end;
			}
		}

		if (content->chars_num && lit.next->type != content_root)
		{
			if (content->chars[content->chars_num-1] == '-')    content->chars_num -= 1;
			else if (content->chars[content->chars_num-1] != ' ')
			{
				extract_astring_catc(alloc, content, ' ');
			}
		}
	}
	if (extract_astring_catf(alloc, content, "%s</p>", endl)) goto end;

	e = 0;

	end:
	return e;
}

static int
paragraphs_to_html_content(
		extract_alloc_t    *alloc,
		content_state_t    *state,
		content_root_t     *paragraphs,
		int                 single_line,
		extract_astring_t  *content)
{
	content_paragraph_iterator  pit;
	paragraph_t                *paragraph;
	int e = -1;

	for (paragraph = content_paragraph_iterator_init(&pit, paragraphs); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
		if (paragraph_to_html_content(alloc, state, paragraph, single_line, content)) goto end;

	if (content_state_reset(alloc, state, content)) goto end;
	e = 0;

	end:
	return e;
}

static int
append_table(extract_alloc_t *alloc, content_state_t *state, table_t *table, extract_astring_t *content)
{
	int e = -1;
	int y;

	if (extract_astring_cat(alloc, content, "\n\n<table border=\"1\" style=\"border-collapse:collapse\">\n")) goto end;

	for (y=0; y<table->cells_num_y; ++y)
	{

		int x;
		if (extract_astring_cat(alloc, content, "    <tr>\n")) goto end;
		for (x=0; x<table->cells_num_x; ++x)
		{
			cell_t* cell = table->cells[y*table->cells_num_x + x];
			if (!cell->above || !cell->left)
			{

				continue;
			}
			if (extract_astring_cat(alloc, content, "        ")) goto end;
			if (extract_astring_cat(alloc, content, "<td")) goto end;

			if (cell->extend_right > 1)
			{
				if (extract_astring_catf(alloc, content, " colspan=\"%i\"", cell->extend_right)) goto end;
			}
			if (cell->extend_down > 1)
			{
				if (extract_astring_catf(alloc, content, " rowspan=\"%i\"", cell->extend_down)) goto end;
			}

			if (extract_astring_cat(alloc, content, ">")) goto end;

			if (paragraphs_to_html_content(alloc, state, &cell->content, 1 , content)) goto end;
			if (extract_astring_cat(alloc, content, "</td>")) goto end;
			if (extract_astring_cat(alloc, content, "\n")) goto end;

			if (content_state_reset(alloc, state, content)) goto end;
		}
		if (extract_astring_cat(alloc, content, "    </tr>\n")) goto end;
	}
	if (extract_astring_cat(alloc, content, "</table>\n\n")) goto end;
	e = 0;

	end:
	return e;
}

static char_t *
paragraph_first_char(const paragraph_t *paragraph)
{
	line_t *line = content_last_line(&paragraph->content);
	span_t *span = content_last_span(&line->content);
	return &span->chars[0];
}

static int compare_paragraph_y(const void *a, const void *b)
{
	const paragraph_t *const *a_paragraph = a;
	const paragraph_t *const *b_paragraph = b;
	double a_y = paragraph_first_char(*a_paragraph)->y;
	double b_y = paragraph_first_char(*b_paragraph)->y;

	if (a_y > b_y)  return +1;
	if (a_y < b_y)  return -1;

	return 0;
}

static int
split_to_html(extract_alloc_t *alloc, split_t *split, subpage_t ***ppsubpage, extract_astring_t *output)
{
	int                          p;
	int                          s;
	subpage_t                   *subpage;
	int                          paragraphs_num;
	paragraph_t                **paragraphs = NULL;
	content_paragraph_iterator   pit;
	paragraph_t                 *paragraph;
	content_table_iterator       tit;
	table_t                     *table;
	content_state_t              state;
	content_state_init(&state);

	if (split == NULL) {

	} else if (split->type == SPLIT_HORIZONTAL) {
		int ret = 0;
		double total = 0;
		for (s = 0; s < split->count; s++) {
			total += split->split[s]->weight;
		}
		if (split->count > 1)
			extract_astring_cat(alloc, output, "<div style=\"display:flex;\">\n");
		for (s = 0; s < split->count; s++) {
			if (split->count > 1)
			{
				if (total == 0)
				{
					extract_astring_catf(alloc, output, "<div>\n");
				}
				else
				{
					extract_astring_catf(alloc, output, "<div style=\"width:%g%%;\">\n", 100.0*split->split[s]->weight/total);
				}
			}
			ret = split_to_html(alloc, split->split[s], ppsubpage, output);
			if (ret)
				break;
			if (split->count > 1)
				extract_astring_cat(alloc, output, "</div>\n");
		}
		if (split->count > 1)
			extract_astring_cat(alloc, output, "</div>\n");
		return ret;
	} else if (split->type == SPLIT_VERTICAL) {
		int ret = 0;
		for (s = 0; s < split->count; s++) {
			ret = split_to_html(alloc, split->split[s], ppsubpage, output);
			if (ret)
				break;
		}
		return ret;
	}

	subpage = **ppsubpage;
	*ppsubpage = (*ppsubpage)+1;

	paragraphs_num = content_count_paragraphs(&subpage->content);
	if (extract_malloc(alloc, &paragraphs, sizeof(*paragraphs) * paragraphs_num)) goto end;
	for (p = 0, paragraph = content_paragraph_iterator_init(&pit, &subpage->content); paragraph != NULL; p++, paragraph = content_paragraph_iterator_next(&pit))
		paragraphs[p] = paragraph;
	qsort(paragraphs, paragraphs_num, sizeof(*paragraphs), compare_paragraph_y);

	if (0)
	{
		int p;
		outf0("paragraphs are:");
		for (p=0; p<paragraphs_num; ++p)
		{
			paragraph_t* paragraph = paragraphs[p];
			line_t *line = content_first_line(&paragraph->content);
			span_t *span = content_first_span(&line->content);
			outf0("    p=%i: %s", p, extract_span_string(NULL, span));
		}
	}

	p = 0;
	table = content_table_iterator_init(&tit, &subpage->tables);
	for(;;)
	{
		double y_paragraph;
		double y_table;
		paragraph_t* paragraph = (p == paragraphs_num) ? NULL : paragraphs[p];
		if (!paragraph && !table) break;
		y_paragraph = (paragraph) ? content_first_span(&content_first_line(&paragraph->content)->content)->chars[0].y : DBL_MAX;
		y_table = (table) ? table->pos.y : DBL_MAX;
		outf("p=%i y_paragraph=%f", p, y_paragraph);
		outf("y_table=%f", y_table);
		if (paragraph && y_paragraph < y_table)
		{

			if (paragraph_to_html_content(alloc, &state, paragraph, 0 , output)) goto end;
			if (content_state_reset(alloc, &state, output)) goto end;
			p += 1;
		}
		else if (table)
		{

			if (append_table(alloc, &state, table, output)) goto end;
			table = content_table_iterator_next(&tit);
		}
	}
	extract_free(alloc, &paragraphs);
	return 0;

end:
	extract_free(alloc, &paragraphs);
	return -1;
}

int extract_document_to_html_content(
		extract_alloc_t   *alloc,
		document_t        *document,
		int                rotation,
		int                images,
		extract_astring_t *content)
{
	int ret = -1;
	int n;
	paragraph_t **paragraphs = NULL;

	(void) rotation;
	(void) images;

	extract_astring_cat(alloc, content, "<html>\n");
	extract_astring_cat(alloc, content, "<body>\n");

	for (n=0; n<document->pages_num; ++n)
	{
		extract_page_t  *page     = document->pages[n];
		subpage_t      **psubpage = page->subpages;

		extract_astring_cat(alloc, content, "<div>\n");

		ret = split_to_html(alloc, page->split, &psubpage, content);
		if (ret)
			goto end;

		extract_astring_cat(alloc, content, "</div>\n");
	}
	extract_astring_cat(alloc, content, "</body>\n");
	extract_astring_cat(alloc, content, "</html>\n");

	ret = 0;
end:

	extract_free(alloc, &paragraphs);

	return ret;
}

#include "extract/extract.h"
#include "extract/alloc.h"

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

static line_t *paragraph_line_first(const paragraph_t *paragraph)
{
	return content_first_line(&paragraph->content);
}

static line_t *paragraph_line_last(const paragraph_t *paragraph)
{
	return content_last_line(&paragraph->content);
}

static int
matrices_are_compatible(const matrix4_t *ctm_a, const matrix4_t *ctm_b, int wmode)
{
	double  dot, pdot;

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

	if (dot <= 0)
		return 0;

	pdot /= dot;

	return (fabs(pdot) < 0.1);
}

static int
lines_are_compatible(line_t *a,
					 line_t *b)
{
	span_t *first_span_a = content_first_span(&a->content);
	span_t *first_span_b = content_first_span(&b->content);

	if (a == b) return 0;
	if (!first_span_a || !first_span_b) return 0;

	if (first_span_a->flags.wmode != first_span_b->flags.wmode)
		return 0;

	return matrices_are_compatible(&first_span_a->ctm, &first_span_b->ctm, first_span_a->flags.wmode);
}

static const unsigned ucs_NONE = ((unsigned) -1);

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
	*(content_t *)o_span = save;
	extract_strdup(alloc, span->font_name, &o_span->font_name);
	o_span->chars = NULL;
	o_span->chars_num = 0;
	for (c=0; c<span->chars_num; ++c)
	{

		char_t *char_ = &span->chars[c];
		if (char_->x >= rect->min.x &&
			char_->x <  rect->max.x &&
			char_->y >= rect->min.y &&
			char_->y <  rect->max.y)
		{
			char_t *c = extract_span_append_c(alloc, o_span, char_->ucs);
			if (c == NULL) return -1;
			*c = *char_;
			char_->ucs = ucs_NONE;
		}
	}

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

		span->chars_num = cc;
	}

	if (o_span->chars_num)
	{
		outf("o_span: %s", extract_span_string(alloc, o_span));
	}
	return 0;
}

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

	for (a = 0, span = content_span_iterator_init(&sit, lines); span != NULL; span = content_span_iterator_next(&sit), a++)
	{
		line_t *line;

		if (content_replace_new_line(alloc, &span->base, &line)) goto end;
		content_append_span(&line->content, span);
		outfx("initial line a=%i: %s", a, line_string(line));
	}

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

				point_t dir = { last_a->adv * (1 - span_a->flags.wmode), last_a->adv * span_a->flags.wmode };
				point_t tdir = extract_matrix4_transform_point(span_a->ctm, dir);
				point_t span_a_end = { last_a->x + tdir.x, last_a->y + tdir.y };

				char_t *first_b = span_char_first(span_b);
				point_t diff = { first_b->x - span_a_end.x, first_b->y - span_a_end.y };
				double scale_squared = ((span_a->flags.wmode) ?
										(span_a->ctm.c * span_a->ctm.c + span_a->ctm.d * span_a->ctm.d) :
										(span_a->ctm.a * span_a->ctm.a + span_a->ctm.b * span_a->ctm.b));

				double colinear = (diff.x * tdir.x + diff.y * tdir.y) / last_a->adv / scale_squared;
				double perp     = (diff.x * tdir.y - diff.y * tdir.x) / last_a->adv / scale_squared;

				double score;
				double space_guess = (last_a->adv + first_b->adv)/2 * master_space_guess;

				if (fabs(perp) > 3*space_guess/2 || fabs(colinear) > space_guess * 8)
					 continue;

				score = fabs(colinear);
				if (score < fabs(perp) * 10)
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

			span_t *span_b = extract_line_span_first(nearest_line);
			b = nearest_line_b;

			if (extract_span_char_last(span_a)->ucs != ' ' &&
				span_char_first(span_b)->ucs != ' ')
			{

				int insert_space = (nearest_colinear > 2*nearest_space_guess/3);
				if (insert_space)
				{

					char_t *item = extract_span_append_c(alloc, span_a, ' ');
					if (item == NULL) goto end;
					item->adv = 0;

					item->x = item[-1].x;
					item->y = item[-1].y;
				}
			}

			content_concat(&line_a->content, &nearest_line->content);

			if (lit.next == &nearest_line->base)
				lit.next = lit.next->next;
			extract_line_free(alloc, &nearest_line);

			if (b > a) {

				lit.next = &line_a->base;
				a--;
			} else {
				a--;
			}
		}
	}

	ret = 0;

end:
	if (ret) {

		extract_span_free(alloc, &span);
		content_clear(alloc, lines);
	}
	return ret;
}

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

	if (a_span->flags.wmode != b_span->flags.wmode)
	{
		return a_span->flags.wmode - b_span->flags.wmode;
	}

	if (!matrices_are_compatible(&a_span->ctm, &b_span->ctm, a_span->flags.wmode))
	{

		return extract_matrix4_cmp(&a_span->ctm, &b_span->ctm);
	}

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

	for (line = content_line_iterator_init(&lit, content); line != NULL; line = content_line_iterator_next(&lit))
	{
		paragraph_t *paragraph;
		if (content_replace_new_paragraph(alloc, &line->base, &paragraph))
			goto end;
		content_append_line(&paragraph->content, line);
		calculate_line_height(line);
	}

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

				point_t start_diff = { first_b->x - first_a->x, first_b->y - first_a->y };
				point_t end_a = { last_a->x + last_a->adv * tdir_a.x, last_a->y + last_a->adv * tdir_a.y };
				point_t end_b = { last_b->x + last_b->adv * tdir_b.x, last_b->y + last_b->adv * tdir_b.y };

				double scale_squared = ((span_a->flags.wmode) ?
										(span_a->ctm.c * span_a->ctm.c + span_a->ctm.d * span_a->ctm.d) :
										(span_a->ctm.a * span_a->ctm.a + span_a->ctm.b * span_a->ctm.b));
				double perp     = (start_diff.x * tdir_a.y - start_diff.y * tdir_a.x) / sqrt(scale_squared);

				double score;

				point_t saea = { end_a.x    - first_a->x, end_a.y    - first_a->y };
				point_t sasb = { first_b->x - first_a->x, first_b->y - first_a->y };
				point_t saeb = { end_b.x    - first_a->x, end_b.y    - first_a->y };
				double dot_saea = ( saea.x * tdir_a.x + saea.y * tdir_a.y );
				double dot_sasb = ( sasb.x * tdir_a.x + sasb.y * tdir_a.y );
				double dot_saeb = ( saeb.x * tdir_a.x + saeb.y * tdir_a.y );

				score = -perp;

#if 0
				printf("Comparing:\n");
				content_dump_brief(&paragraph_a->content);
				printf("And:\n");
				content_dump_brief(&paragraph_b->content);
				printf("score=%g\n", score);
				printf("saea=%g sasb=%g saeb=%g\n", dot_saea, dot_sasb, dot_saeb);
#endif

				if (dot_sasb > dot_saea)
					continue;

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

				span_t *a_span = extract_line_span_last(line_a);

				if (extract_span_char_last(a_span)->ucs == '-' ||
					extract_span_char_last(a_span)->ucs == 0x2212 )
				{

					a_span->chars_num -= 1;
					if (a_span->chars_num == 0)
					{

						extract_span_free(alloc, &a_span);

						if (line_a->content.base.next == &line_a->content.base)
						{
							extract_line_free(alloc, &line_a);
							a--;
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

					char_t *c_prev;
					char_t *c = extract_span_append_c(alloc, extract_line_span_last(line_a), ' ');
					if (c == NULL) goto end;
					c_prev = &a_span->chars[ a_span->chars_num-2];
					c->x = c_prev->x + c_prev->adv * a_span->ctm.a;
					c->y = c_prev->y + c_prev->adv * a_span->ctm.c;
				}

				content_concat(&paragraph_a->content, &nearest_paragraph->content);

#if 0
				printf("Joining to give:\n");
				content_dump_brief(&paragraph_a->content);
#endif

				if (pit.next == &nearest_paragraph->base)
					pit.next = pit.next->next;
				extract_paragraph_free(alloc, &nearest_paragraph);

				if (nearest_paragraph_b > a) {

					pit.next = &paragraph_a->base;
					a -= 1;
				} else {
					a -= 1;
				}
			}
		}
	}

	content_sort(content, paragraphs_cmp);

	ret = 0;

end:

	return ret;
}

static char_t *
last_non_space_char(span_t *span)
{
	int i = span->chars_num - 1;

	while (i > 0 && span->chars[i].ucs == 32)
		i--;

	return &span->chars[i];
}

static int
analyse_paragraphs(content_root_t *content)
{
	content_paragraph_iterator  pit;
	paragraph_t                *paragraph;

	for (paragraph = content_paragraph_iterator_init(&pit, content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
	{
		content_line_iterator  lit;
		line_t                *line;
		double                 para_l = 0, para_r = 0;
		int                    first_span_of_para = 1;
		matrix4_t              inverse;
		double                 space_guess = 0;
		int                    previous_line_flags = -1;
		double                 previous_line_spare = 0;
		int                    first_line = 1;

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

		for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
		{
			content_span_iterator  sit;
			span_t                *span;
			double                 line_l = 0, line_r = 0;
			int                    first_span = 1;

			int                    first_word = !first_line;
			int                    word_width_found = 0;
			point_t                word_end;
			int                    word_wmode;

			for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
			{
				char_t    *lc     = &span->chars[0];
				char_t    *rc     = last_non_space_char(span);
				point_t    dir    = { 1 - span->flags.wmode, span->flags.wmode };
				point_t    tdir   = extract_matrix4_transform_point(span->ctm, dir);
				point_t    left   = { lc->x, lc->y };
				point_t    right  = { rc->x + tdir.x * rc->adv, rc->y + tdir.y * rc->adv };
				double     l, r;

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

			if (word_width_found)
			{
				double w;
				word_end = extract_matrix4_transform_point(inverse, word_end);
				w = word_wmode ? word_end.y : word_end.x;
				w -= line_l;

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

	content_iterator  cit;
	content_t        *content;
	content_iterator  cit0 = { 0 };
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

				if (rotate == 0)
					flush = 1;
				else
					ctm_set = 1;

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

			block_t *block;
			content_t *c = content_iterator_next(&cit0);

			if (content_replace_new_block(alloc, content0, &block)) goto end;

			content_append(&block->content, content0);

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

		block_t *block;
		content_t *c = content_iterator_next(&cit0);

		if (content_replace_new_block(alloc, content0, &block)) goto end;

		content_append(&block->content, content0);

		for (; c != content; c = content_iterator_next(&cit0))
		{
			content_append(&block->content, c);
		}
	}

	ret = 0;
end:

	return ret;
}

static int
spans_within_rect(
		extract_alloc_t *alloc,
		content_root_t  *content,
		rect_t          *rect,
		content_root_t  *subset)
{
	content_span_iterator  it;
	span_t                *candidate;

	for (candidate = content_span_iterator_init(&it, content); candidate != NULL; candidate = content_span_iterator_next(&it))
	{
		span_t *span;

		if (candidate->chars_num == 0)
			continue;

		if (content_new_span(alloc, &span, candidate->structure))
			return -1;

		if (span_inside_rect(alloc, candidate, rect, span))
			return -1;
		if (span->chars_num)
		{

			content_append_span(subset, span);
		}
		else
		{

			extract_span_free(alloc, &span);
		}
		span = NULL;

		if (!candidate->chars_num)
		{

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

static int
table_find_cells_text(
		extract_alloc_t  *alloc,
		subpage_t        *subpage,
		cell_t          **cells,
		int               cells_num_x,
		int               cells_num_y,
		double            master_space_guess)
{

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

	if (content_append_new_table(alloc, &subpage->tables, &table)) goto end;
	table->pos.x = cells[0]->rect.min.x;
	table->pos.y = cells[0]->rect.min.y;
	table->cells = cells;
	table->cells_num_x = cells_num_x;
	table->cells_num_y = cells_num_y;

	if (0)
	{

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

static int
table_find(extract_alloc_t *alloc, subpage_t *subpage, double y_min, double y_max, double master_space_guess)
{
	tablelines_t *all_h = &subpage->tablelines_horizontal;
	tablelines_t *all_v = &subpage->tablelines_vertical;
	int e = -1;
	int i;

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

	qsort(tl_v.tablelines, tl_v.tablelines_num, sizeof(*tl_v.tablelines), tablelines_compare_x);

	if (0)
	{

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

		while (ih < subpage->tablelines_horizontal.tablelines_num)
		{
			if (subpage->tablelines_horizontal.tablelines[ih].color == 1)
			{

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

				table_find(alloc, subpage, miny - margin, maxy + margin, master_space_guess);
			}
			miny = tl->rect.min.y;
		}
		if (tl->rect.max.y > maxy)  maxy = tl->rect.max.y;
	}

	table_find(alloc, subpage, miny - margin, maxy + margin, master_space_guess);

	return 0;
}

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

static int
extract_join_subpage(
		extract_alloc_t *alloc,
		subpage_t       *subpage,
		double           master_space_guess)
{

	if (extract_subpage_tables_find(alloc, subpage, master_space_guess)) return -1;

	if (join_content(alloc, &subpage->content, master_space_guess))
		return -1;

	return 0;
}

int extract_document_join(extract_alloc_t *alloc, document_t *document, int layout_analysis, double master_space_guess)
{
	int p;

	for (p=0; p<document->pages_num; ++p) {
		extract_page_t* page = document->pages[p];
		int c;

		if (layout_analysis && extract_page_analyse(alloc, page)) return -1;

		for (c=0; c<page->subpages_num; ++c) {
			subpage_t* subpage = page->subpages[c];

			outf("processing page %i, subpage %i: num_spans=%i", p, c, content_count_spans(&subpage->content));
			if (extract_join_subpage(alloc, subpage, master_space_guess)) return -1;
		}
	}

	return 0;
}

#include "extract/extract.h"

#include "memento.h"

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

static int osp(extract_alloc_t *alloc, extract_astring_t *content, structure_t *structure)
{
	if (structure->parent)
	{
		if (osp(alloc, content, structure->parent) ||
			extract_astring_catc(alloc, content, '\\'))
			return -1;
	}

	if (structure->uid != 0)
	{
		if (extract_astring_catf(alloc, content, "%s[%d]", extract_struct_string(structure->type), structure->uid))
			return -1;
	}
	else
	{
		if (extract_astring_catf(alloc, content, "%s", extract_struct_string(structure->type)))
			return -1;
	}

	return 0;
}

static int output_structure_path(extract_alloc_t *alloc, extract_astring_t *content, structure_t *structure)
{
	if (structure == NULL)
		return 0;

	if (extract_astring_cat(alloc, content, ",\n\"Path\" : \"") ||
		osp(alloc, content, structure) ||
		extract_astring_cat(alloc, content, "\""))
		return -1;
	return 0;
}

static int flush(extract_alloc_t *alloc, extract_astring_t *content, span_t *span, structure_t *structure, extract_astring_t *text, rect_t *bbox)
{
	if (span == NULL)
		return 0;
	if (content->chars_num)
		if (extract_astring_cat(alloc, content, ",\n"))
			return -1;
	if (extract_astring_catf(alloc, content, "{\n\"Bounds\": [ %f, %f, %f, %f ],\n\"Text\": \"", bbox->min.x, bbox->min.y, bbox->max.x, bbox->max.y) ||
		extract_astring_catl(alloc, content, text->chars, text->chars_num) ||
		extract_astring_catf(alloc, content, "\",\n\"Font\": { \"family_name\": \"%s\" },\n\"TextSize\": %g", span->font_name, extract_font_size(&span->ctm)))
		return -1;
	if (output_structure_path(alloc, content, structure))
		return -1;
	if (extract_astring_cat(alloc, content, "\n}"))
		return -1;
	extract_astring_free(alloc, text);
	*bbox = extract_rect_empty;

	return 0;
}

int extract_document_to_json_content(
		extract_alloc_t   *alloc,
		document_t        *document,
		int                rotation,
		int                images,
		extract_astring_t *content)
{
	int ret = -1;
	int n;
	content_tree_iterator cti;
	extract_astring_t text;

	(void) rotation;
	(void) images;

	extract_astring_init(&text);

	for (n=0; n<document->pages_num; ++n)
	{
		int              i;
		extract_page_t  *page     = document->pages[n];
		subpage_t      **psubpage = page->subpages;

		for (i=0; i<page->subpages_num; ++i)
		{
			content_t *cont;
			structure_t *structure = NULL;
			span_t *last_span = NULL;
			rect_t bbox = extract_rect_empty;

			for (cont = content_tree_iterator_init(&cti, &psubpage[i]->content); cont != NULL; cont = content_tree_iterator_next(&cti))
			{
				switch (cont->type)
				{
				case content_span:
				{
					int j;
					span_t *span = (span_t *)cont;
					if (last_span &&
						(structure != span->structure ||
						 last_span->flags.font_bold != span->flags.font_bold ||
						 last_span->flags.font_italic != span->flags.font_italic ||
						 last_span->flags.wmode != span->flags.wmode ||
						 strcmp(last_span->font_name, span->font_name)))
					{

						flush(alloc, content, last_span, structure, &text, &bbox);
					}
					last_span = span;
					structure = span->structure;
					for (j = 0; j < span->chars_num; j++)
					{
						if (span->chars[j].ucs == (unsigned int)-1)
							continue;
						if (extract_astring_catc_unicode(alloc, &text, span->chars[j].ucs, 1, 0, 0, 0))
							goto end;
						bbox = extract_rect_union(bbox, span->chars[j].bbox);
					}
					break;
				}
				case content_image:
				case content_table:
				case content_block:
				case content_line:
				case content_paragraph:

					 break;
				default:
					 assert("This should never happen\n" == NULL);
					 break;
				}
			}
			flush(alloc, content, last_span, structure, &text, &bbox);
		}
	}

	ret = 0;
end:

	extract_astring_free(alloc, &text);

	return ret;
}

#include "extract/alloc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef ARTIFEX_EXTRACT_COMPAT_VA_COPY_H
#define ARTIFEX_EXTRACT_COMPAT_VA_COPY_H

#if defined(_MSC_VER) && (_MSC_VER < 1800)
	#define va_copy(dst, src) ((dst) = (src))
#endif

#endif

void extract_bzero(void *b, size_t len)
{
	memset(b, 0, len);
}

int extract_vasprintf(extract_alloc_t *alloc, char **out, const char *format, va_list va)
{
	int n;
	int ret;
	va_list va2;

	va_copy(va2, va);
	n = vsnprintf(NULL, 0, format, va);
	if (n < 0)
	{
		ret = n;
		goto end;
	}
	if (extract_malloc(alloc, out, n + 1))
	{
		ret = -1;
		goto end;
	}
	vsnprintf(*out, n + 1, format, va2);

	ret = 0;
end:

	va_end(va2);

	return ret;
}

int extract_asprintf(extract_alloc_t *alloc, char **out, const char *format, ...)
{
	va_list va;
	int     ret;

	va_start(va, format);
	ret = extract_vasprintf(alloc, out, format, va);
	va_end(va);

	return ret;
}

int extract_strdup(extract_alloc_t *alloc, const char *s, char **o_out)
{
	size_t l = strlen(s) + 1;

	if (extract_malloc(alloc, o_out, l)) return -1;
	memcpy(*o_out, s, l);

	return 0;
}

#define MEMENTO_DETAILS

#define MEMENTO_FREELIST_MAX_SINGLE_BLOCK (MEMENTO_FREELIST_MAX/4)

#define COMPILING_MEMENTO_C

#define _CRT_SECURE_NO_WARNINGS

#ifdef MEMENTO_GS_HACKS

#include "malloc_.h"
#include "memory_.h"
int atexit(void (*)(void));
#else
#ifdef MEMENTO_MUPDF_HACKS
#include "mupdf/memento.h"
#else
#include "memento.h"
#endif
#include <stdio.h>
#endif
#ifndef _MSC_VER
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef __ANDROID__
#define MEMENTO_ANDROID
#include <stdio.h>
#endif

#ifdef _MSC_VER
#define FMTZ "%llu"
#define FMTZ_CAST _int64
#define FMTP "0x%p"
#else
#define FMTZ "%zu"
#define FMTZ_CAST size_t
#define FMTP "%p"
#endif

#define UB(x) ((intptr_t)((x) & 0xFF))
#define B2I(x) (UB(x) | (UB(x)<<8) | (UB(x)<<16) | (UB(x)<<24))
#define B2P(x) ((void *)(B2I(x) | ((B2I(x)<<16)<<16)))
#define MEMENTO_PREFILL_UBYTE ((unsigned char)(MEMENTO_PREFILL))
#define MEMENTO_PREFILL_USHORT (((unsigned short)MEMENTO_PREFILL_UBYTE) | (((unsigned short)MEMENTO_PREFILL_UBYTE)<<8))
#define MEMENTO_PREFILL_UINT (((unsigned int)MEMENTO_PREFILL_USHORT) | (((unsigned int)MEMENTO_PREFILL_USHORT)<<16))
#define MEMENTO_PREFILL_PTR (void *)(((uintptr_t)MEMENTO_PREFILL_UINT) | ((((uintptr_t)MEMENTO_PREFILL_UINT)<<16)<<16))
#define MEMENTO_POSTFILL_UBYTE ((unsigned char)(MEMENTO_POSTFILL))
#define MEMENTO_POSTFILL_USHORT (((unsigned short)MEMENTO_POSTFILL_UBYTE) | (((unsigned short)MEMENTO_POSTFILL_UBYTE)<<8))
#define MEMENTO_POSTFILL_UINT (((unsigned int)MEMENTO_POSTFILL_USHORT) | (((unsigned int)MEMENTO_POSTFILL_USHORT)<<16))
#define MEMENTO_POSTFILL_PTR (void *)(((uintptr_t)MEMENTO_POSTFILL_UINT) | ((((uintptr_t)MEMENTO_POSTFILL_UINT)<<16)<<16))
#define MEMENTO_ALLOCFILL_UBYTE ((unsigned char)(MEMENTO_ALLOCFILL))
#define MEMENTO_ALLOCFILL_USHORT (((unsigned short)MEMENTO_ALLOCFILL_UBYTE) | (((unsigned short)MEMENTO_ALLOCFILL_UBYTE)<<8))
#define MEMENTO_ALLOCFILL_UINT (((unsigned int)MEMENTO_ALLOCFILL_USHORT) | (((unsigned int)MEMENTO_ALLOCFILL_USHORT)<<16))
#define MEMENTO_ALLOCFILL_PTR (void *)(((uintptr_t)MEMENTO_ALLOCFILL_UINT) | ((((uintptr_t)MEMENTO_ALLOCFILL_UINT)<<16)<<16))
#define MEMENTO_FREEFILL_UBYTE ((unsigned char)(MEMENTO_FREEFILL))
#define MEMENTO_FREEFILL_USHORT (((unsigned short)MEMENTO_FREEFILL_UBYTE) | (((unsigned short)MEMENTO_FREEFILL_UBYTE)<<8))
#define MEMENTO_FREEFILL_UINT (((unsigned int)MEMENTO_FREEFILL_USHORT) | (((unsigned int)MEMENTO_FREEFILL_USHORT)<<16))
#define MEMENTO_FREEFILL_PTR (void *)(((uintptr_t)MEMENTO_FREEFILL_UINT) | ((((uintptr_t)MEMENTO_FREEFILL_UINT)<<16)<<16))

#ifdef MEMENTO

#ifndef MEMENTO_CPP_EXTRAS_ONLY

#ifdef MEMENTO_ANDROID
#include <android/log.h>

static char log_buffer[4096];
static int log_fill = 0;

static char log_buffer2[4096];

static int
android_fprintf(FILE *file, const char *fmt, ...)
{
    va_list args;
    char *p, *q;

    va_start(args, fmt);
    vsnprintf(log_buffer2, sizeof(log_buffer2)-1, fmt, args);
    va_end(args);

    log_buffer2[sizeof(log_buffer2)-1] = 0;

    p = log_buffer2;
    q = p;
    do
    {

        while (*p && *p != '\n')
            p++;

        if (p - q >= sizeof(log_buffer)-1 - log_fill)
                p = q + sizeof(log_buffer)-1 - log_fill;

        memcpy(&log_buffer[log_fill], q, p-q);
        log_fill += p-q;
        if (*p == '\n')
        {
            log_buffer[log_fill] = 0;
            __android_log_print(ANDROID_LOG_ERROR, "memento", "%s", log_buffer);
            usleep(1);
            log_fill = 0;
            p++;
        }
        else if (log_fill >= sizeof(log_buffer)-1)
        {
            log_buffer[sizeof(log_buffer2)-1] = 0;
            __android_log_print(ANDROID_LOG_ERROR, "memento", "%s", log_buffer);
            usleep(1);
            log_fill = 0;
        }
        q = p;
    }
    while (*p);

    return 0;
}

#define fprintf android_fprintf
#define MEMENTO_STACKTRACE_METHOD 3
#endif

#ifdef _WIN32
#include <windows.h>

static int
windows_fprintf(FILE *file, const char *fmt, ...)
{
    va_list args;
    char text[4096];
    int ret;

    va_start(args, fmt);
    ret = vfprintf(file, fmt, args);
    va_end(args);

    va_start(args, fmt);
    vsnprintf(text, 4096, fmt, args);
    OutputDebugStringA(text);
    va_end(args);

    return ret;
}

#define fprintf windows_fprintf
#endif

#ifndef MEMENTO_STACKTRACE_METHOD
#ifdef __GNUC__
#define MEMENTO_STACKTRACE_METHOD 1
#endif
#ifdef _WIN32
#define MEMENTO_STACKTRACE_METHOD 2
#endif
#endif

#if defined(__linux__) || defined(__OpenBSD__)
#define MEMENTO_HAS_FORK
#elif defined(__APPLE__) && defined(__MACH__)
#define MEMENTO_HAS_FORK
#endif

void *MEMENTO_UNDERLYING_MALLOC(size_t);
void MEMENTO_UNDERLYING_FREE(void *);
void *MEMENTO_UNDERLYING_REALLOC(void *,size_t);
void *MEMENTO_UNDERLYING_CALLOC(size_t,size_t);

int atoi(const char *);
char *getenv(const char *);

#define MEMENTO_PTRSEARCH 65536

#ifndef MEMENTO_MAXPATTERN
#define MEMENTO_MAXPATTERN 0
#endif

#ifdef MEMENTO_GS_HACKS
#include "valgrind.h"
#else
#ifdef HAVE_VALGRIND
#include "valgrind/memcheck.h"
#else
#define VALGRIND_MAKE_MEM_NOACCESS(p,s)  do { } while (0==1)
#define VALGRIND_MAKE_MEM_UNDEFINED(p,s)  do { } while (0==1)
#define VALGRIND_MAKE_MEM_DEFINED(p,s)  do { } while (0==1)
#endif
#endif

enum {
    Memento_PreSize  = 16,
    Memento_PostSize = 16
};

typedef struct
{
    char MEMENTO_PRESIZE_MUST_BE_A_MULTIPLE_OF_4[Memento_PreSize & 3 ? -1 : 1];
    char MEMENTO_POSTSIZE_MUST_BE_A_MULTIPLE_OF_4[Memento_PostSize & 3 ? -1 : 1];
    char MEMENTO_POSTSIZE_MUST_BE_AT_LEAST_4[Memento_PostSize >= 4 ? 1 : -1];
    char MEMENTO_PRESIZE_MUST_BE_AT_LEAST_4[Memento_PreSize >= 4 ? 1 : -1];
} MEMENTO_SANITY_CHECK_STRUCT;

#define MEMENTO_UINT32 unsigned int
#define MEMENTO_UINT16 unsigned short

#define MEMENTO_PREFILL_UINT32  ((MEMENTO_UINT32)(MEMENTO_PREFILL  | (MEMENTO_PREFILL <<8) | (MEMENTO_PREFILL <<16) |(MEMENTO_PREFILL <<24)))
#define MEMENTO_POSTFILL_UINT16 ((MEMENTO_UINT16)(MEMENTO_POSTFILL | (MEMENTO_POSTFILL<<8)))
#define MEMENTO_POSTFILL_UINT32 ((MEMENTO_UINT32)(MEMENTO_POSTFILL | (MEMENTO_POSTFILL<<8) | (MEMENTO_POSTFILL<<16) |(MEMENTO_POSTFILL<<24)))
#define MEMENTO_FREEFILL_UINT16 ((MEMENTO_UINT16)(MEMENTO_FREEFILL | (MEMENTO_FREEFILL<<8)))
#define MEMENTO_FREEFILL_UINT32 ((MEMENTO_UINT32)(MEMENTO_FREEFILL | (MEMENTO_FREEFILL<<8) | (MEMENTO_FREEFILL<<16) |(MEMENTO_FREEFILL<<24)))

enum {
    Memento_Flag_OldBlock = 1,
    Memento_Flag_HasParent = 2,
    Memento_Flag_BreakOnFree = 4,
    Memento_Flag_BreakOnRealloc = 8,
    Memento_Flag_Freed = 16,
    Memento_Flag_KnownLeak = 32,
    Memento_Flag_Reported = 64
};

enum {
    Memento_EventType_malloc = 0,
    Memento_EventType_calloc = 1,
    Memento_EventType_realloc = 2,
    Memento_EventType_free = 3,
    Memento_EventType_new = 4,
    Memento_EventType_delete = 5,
    Memento_EventType_newArray = 6,
    Memento_EventType_deleteArray = 7,
    Memento_EventType_takeRef = 8,
    Memento_EventType_dropRef = 9,
    Memento_EventType_reference = 10,
    Memento_EventType_strdup = 11,
    Memento_EventType_asprintf = 12,
    Memento_EventType_vasprintf = 13
};

static const char *eventType[] =
{
    "malloc",
    "calloc",
    "realloc",
    "free",
    "new",
    "delete",
    "new[]",
    "delete[]",
    "takeRef",
    "dropRef",
    "reference",
    "strdup",
    "asprintf",
    "vasprintf"
};

#ifndef MEMENTO_SEARCH_SKIP
#ifdef MEMENTO_GS_HACKS
#define MEMENTO_SEARCH_SKIP (2*sizeof(void *))
#else
#define MEMENTO_SEARCH_SKIP 0
#endif
#endif

#define MEMENTO_CHILD_MAGIC   ((Memento_BlkHeader *)('M' | ('3' << 8) | ('m' << 16) | ('3' << 24)))
#define MEMENTO_SIBLING_MAGIC ((Memento_BlkHeader *)('n' | ('t' << 8) | ('0' << 16) | ('!' << 24)))

#ifdef MEMENTO_DETAILS
typedef struct Memento_BlkDetails Memento_BlkDetails;

struct Memento_BlkDetails
{
    Memento_BlkDetails *next;
    char                type;
    char                count;
    int                 sequence;
    void               *stack[1];
};
#endif

typedef struct Memento_BlkHeader Memento_BlkHeader;

struct Memento_BlkHeader
{
    size_t               rawsize;
    int                  sequence;
    int                  lastCheckedOK;
    int                  flags;
    Memento_BlkHeader   *next;
    Memento_BlkHeader   *prev;

    const char          *label;

    Memento_BlkHeader   *child;
    Memento_BlkHeader   *sibling;

#ifdef MEMENTO_DETAILS
    Memento_BlkDetails  *details;
    Memento_BlkDetails **details_tail;
#endif

    char                 preblk[Memento_PreSize];
};

typedef struct Memento_Blocks
{
    Memento_BlkHeader *head;
    Memento_BlkHeader *tail;
} Memento_Blocks;

#ifdef MEMENTO_LOCKLESS
typedef int Memento_mutex;

static void Memento_initMutex(Memento_mutex *m)
{
    (void)m;
}

#define MEMENTO_DO_LOCK() do { } while (0)
#define MEMENTO_DO_UNLOCK() do { } while (0)

#else
#if defined(_WIN32) || defined(_WIN64)

typedef CRITICAL_SECTION Memento_mutex;

static void Memento_initMutex(Memento_mutex *m)
{
    InitializeCriticalSection(m);
}

#define MEMENTO_DO_LOCK() \
    EnterCriticalSection(&memento.mutex)
#define MEMENTO_DO_UNLOCK() \
    LeaveCriticalSection(&memento.mutex)

#else
#include <pthread.h>
typedef pthread_mutex_t Memento_mutex;

static void Memento_initMutex(Memento_mutex *m)
{
    pthread_mutex_init(m, NULL);
}

#define MEMENTO_DO_LOCK() \
    pthread_mutex_lock(&memento.mutex)
#define MEMENTO_DO_UNLOCK() \
    pthread_mutex_unlock(&memento.mutex)

#endif
#endif

typedef struct {
    int begin;
    int end;
} Memento_range;

static struct {
    int            inited;
    Memento_Blocks used;
    Memento_Blocks free;
    size_t         freeListSize;
    int            sequence;
    int            paranoia;
    int            paranoidAt;
    int            countdown;
    int            lastChecked;
    int            breakAt;
    int            failAt;
    int            failing;
    int            nextFailAt;
    int            squeezeAt;
    int            squeezing;
    int            segv;
    int            pattern;
    int            nextPattern;
    int            patternBit;
    int            leaking;
    int            hideMultipleReallocs;
    int            abortOnLeak;
    int            abortOnCorruption;
    size_t         maxMemory;
    size_t         alloc;
    size_t         peakAlloc;
    size_t         totalAlloc;
    size_t         numMallocs;
    size_t         numFrees;
    size_t         numReallocs;
    Memento_mutex  mutex;
    Memento_range *squeezes;
    int            squeezes_num;
    int            squeezes_pos;
} memento;

#define MEMENTO_EXTRASIZE (sizeof(Memento_BlkHeader) + Memento_PostSize)

#define MEMENTO_ROUNDUP(S,N) ((S + N-1)&~(N-1))

#define MEMBLK_SIZE(s) MEMENTO_ROUNDUP(s + MEMENTO_EXTRASIZE, MEMENTO_MAXALIGN)

#define MEMBLK_FROMBLK(B)   (&((Memento_BlkHeader*)(void *)(B))[-1])
#define MEMBLK_TOBLK(B)     ((void*)(&((Memento_BlkHeader*)(void*)(B))[1]))
#define MEMBLK_POSTPTR(B) \
          (&((unsigned char *)(void *)(B))[(B)->rawsize + sizeof(Memento_BlkHeader)])

enum
{
    SkipStackBackTraceLevels = 4
};

#if defined(MEMENTO_STACKTRACE_METHOD) && MEMENTO_STACKTRACE_METHOD == 1
extern size_t backtrace(void **, int);
extern void backtrace_symbols_fd(void **, size_t, int);
extern char **backtrace_symbols(void **, size_t);

#define MEMENTO_BACKTRACE_MAX 256
static void (*print_stack_value)(void *address);

#ifdef HAVE_LIBDL
#include <dlfcn.h>

typedef void (*backtrace_error_callback) (void *data, const char *msg, int errnum);

typedef struct backtrace_state *(*backtrace_create_state_type)(
    const char *filename, int threaded,
    backtrace_error_callback error_callback, void *data);

typedef int (*backtrace_full_callback) (void *data, uintptr_t pc,
                                        const char *filename, int lineno,
                                        const char *function);

typedef int (*backtrace_pcinfo_type)(struct backtrace_state *state,
                                     uintptr_t pc,
                                     backtrace_full_callback callback,
                                     backtrace_error_callback error_callback,
                                     void *data);

typedef void (*backtrace_syminfo_callback) (void *data, uintptr_t pc,
                                            const char *symname,
                                            uintptr_t symval,
                                            uintptr_t symsize);

typedef int (*backtrace_syminfo_type)(struct backtrace_state *state,
                                      uintptr_t addr,
                                      backtrace_syminfo_callback callback,
                                      backtrace_error_callback error_callback,
                                      void *data);

static backtrace_syminfo_type backtrace_syminfo;
static backtrace_create_state_type backtrace_create_state;
static backtrace_pcinfo_type backtrace_pcinfo;
static struct backtrace_state *my_backtrace_state;
static void *libbt;
static char backtrace_exe[4096];
static void *current_addr;

static void error2_cb(void *data, const char *msg, int errnum)
{
    (void)data;
    (void)msg;
    (void)errnum;
}

static void syminfo_cb(void *data, uintptr_t pc, const char *symname, uintptr_t symval, uintptr_t symsize)
{
    (void)data;
    (void)symval;
    (void)symsize;
    if (sizeof(void *) == 4)
        fprintf(stderr, "    0x%08lx %s\n", pc, symname?symname:"?");
    else
        fprintf(stderr, "    0x%016lx %s\n", pc, symname?symname:"?");
}

static void error_cb(void *data, const char *msg, int errnum)
{
    (void)data;
    (void)msg;
    (void)errnum;
    backtrace_syminfo(my_backtrace_state,
                     (uintptr_t)current_addr,
                     syminfo_cb,
                     error2_cb,
                     NULL);
}

static int full_cb(void *data, uintptr_t pc, const char *fname, int line, const char *fn)
{
    (void)data;
    if (sizeof(void *) == 4)
        fprintf(stderr, "    0x%08lx %s(%s:%d)\n", pc, fn?fn:"?", fname?fname:"?", line);
    else
        fprintf(stderr, "    0x%016lx %s(%s:%d)\n", pc, fn?fn:"?", fname?fname:"?", line);
    return 0;
}

static void print_stack_libbt(void *addr)
{
    current_addr = addr;
    backtrace_pcinfo(my_backtrace_state,
                     (uintptr_t)addr,
                     full_cb,
                     error_cb,
                     NULL);
}

static void print_stack_libbt_failed(void *addr)
{
    char **strings;
#if 0

    static char command[1024];
    int e;
    static int gdb_invocation_failed = 0;

    if (gdb_invocation_failed == 0)
    {
        snprintf(command, sizeof(command),

                 "gdb -q --batch -p=%i -ex 'info line *%p' -ex quit 2>/dev/null| egrep -v '(Thread debugging using)|(Using host libthread_db library)|(A debugging session is active)|(will be detached)|(Quit anyway)|(No such file or directory)|(^0x)|(^$)'",
                 getpid(), addr);
    printf("%s\n", command);
        e = system(command);
        if (e == 0)
            return;
        gdb_invocation_failed = 1;
    }
#endif

    strings = backtrace_symbols(&addr, 1);

    if (strings == NULL || strings[0] == NULL)
    {
        if (sizeof(void *) == 4)
            fprintf(stderr, "    [0x%08lx]\n", (uintptr_t)addr);
        else
            fprintf(stderr, "    [0x%016lx]\n", (uintptr_t)addr);
    }
    else
    {
        fprintf(stderr, "    %s\n", strings[0]);
    }
    (free)(strings);
}

static int init_libbt(void)
{
    static int libbt_inited = 0;

    if (libbt_inited)
        return 0;
    libbt_inited = 1;

    libbt = dlopen("libbacktrace.so", RTLD_LAZY);
    if (libbt == NULL)
        libbt = dlopen("/opt/lib/libbacktrace.so", RTLD_LAZY);
    if (libbt == NULL)
        libbt = dlopen("/lib/libbacktrace.so", RTLD_LAZY);
    if (libbt == NULL)
        libbt = dlopen("/usr/lib/libbacktrace.so", RTLD_LAZY);
    if (libbt == NULL)
        libbt = dlopen("/usr/local/lib/libbacktrace.so", RTLD_LAZY);
    if (libbt == NULL)
        goto fail;

    backtrace_create_state = dlsym(libbt, "backtrace_create_state");
    backtrace_syminfo      = dlsym(libbt, "backtrace_syminfo");
    backtrace_pcinfo       = dlsym(libbt, "backtrace_pcinfo");

    if (backtrace_create_state == NULL ||
        backtrace_syminfo == NULL ||
        backtrace_pcinfo == NULL)
    {
        goto fail;
    }

    my_backtrace_state = backtrace_create_state(backtrace_exe,
                                                1 ,
                                                error_cb,
                                                NULL);
    if (my_backtrace_state == NULL)
        goto fail;

    print_stack_value = print_stack_libbt;

    return 1;

 fail:
    fprintf(stderr,
            "MEMENTO: libbacktrace.so failed to load; backtraces will be sparse.\n"
            "MEMENTO: See memento.h for how to rectify this.\n");
    libbt = NULL;
    backtrace_create_state = NULL;
    backtrace_syminfo = NULL;
    print_stack_value = print_stack_libbt_failed;
    return 0;
}
#endif

static void print_stack_default(void *addr)
{
    char **strings = backtrace_symbols(&addr, 1);

    if (strings == NULL || strings[0] == NULL)
    {
        fprintf(stderr, "    ["FMTP"]\n", addr);
    }
#ifdef HAVE_LIBDL
    else if (strchr(strings[0], ':') == NULL)
    {

        char *s = strchr(strings[0], ' ');

        if (s != strings[0])
        {
            memcpy(backtrace_exe, strings[0], s - strings[0]);
            backtrace_exe[s-strings[0]] = 0;
            init_libbt();
            print_stack_value(addr);
        }
    }
#endif
    else
    {
        fprintf(stderr, "    %s\n", strings[0]);
    }
    free(strings);
}

static void Memento_initStacktracer(void)
{
    print_stack_value = print_stack_default;
}

static int Memento_getStacktrace(void **stack, int *skip)
{
    size_t num;

    num = backtrace(&stack[0], MEMENTO_BACKTRACE_MAX);

    *skip = SkipStackBackTraceLevels;
    if (num <= SkipStackBackTraceLevels)
        return 0;
    return (int)(num-SkipStackBackTraceLevels);
}

static void Memento_showStacktrace(void **stack, int numberOfFrames)
{
    int i;

    for (i = 0; i < numberOfFrames; i++)
    {
        print_stack_value(stack[i]);
    }
}
#elif defined(MEMENTO_STACKTRACE_METHOD) && MEMENTO_STACKTRACE_METHOD == 2
#include <Windows.h>

#ifdef _WIN64
typedef DWORD64 DWORD_NATIVESIZED;
#else
typedef DWORD DWORD_NATIVESIZED;
#endif

#define MEMENTO_BACKTRACE_MAX 64

typedef USHORT (__stdcall *My_CaptureStackBackTraceType)(__in ULONG, __in ULONG, __out PVOID*, __out_opt PULONG);

typedef struct MY_IMAGEHLP_LINE {
    DWORD    SizeOfStruct;
    PVOID    Key;
    DWORD    LineNumber;
    PCHAR    FileName;
    DWORD_NATIVESIZED    Address;
} MY_IMAGEHLP_LINE, *MY_PIMAGEHLP_LINE;

typedef BOOL (__stdcall *My_SymGetLineFromAddrType)(HANDLE hProcess, DWORD_NATIVESIZED dwAddr, PDWORD pdwDisplacement, MY_PIMAGEHLP_LINE Line);

typedef struct MY_SYMBOL_INFO {
    ULONG       SizeOfStruct;
    ULONG       TypeIndex;
    ULONG64     Reserved[2];
    ULONG       info;
    ULONG       Size;
    ULONG64     ModBase;
    ULONG       Flags;
    ULONG64     Value;
    ULONG64     Address;
    ULONG       Register;
    ULONG       Scope;
    ULONG       Tag;
    ULONG       NameLen;
    ULONG       MaxNameLen;
    CHAR        Name[1];
} MY_SYMBOL_INFO, *MY_PSYMBOL_INFO;

typedef BOOL (__stdcall *My_SymFromAddrType)(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, MY_PSYMBOL_INFO Symbol);
typedef BOOL (__stdcall *My_SymInitializeType)(HANDLE hProcess, PSTR UserSearchPath, BOOL fInvadeProcess);

static My_CaptureStackBackTraceType Memento_CaptureStackBackTrace;
static My_SymGetLineFromAddrType Memento_SymGetLineFromAddr;
static My_SymFromAddrType Memento_SymFromAddr;
static My_SymInitializeType Memento_SymInitialize;
static HANDLE Memento_process;

static void Memento_initStacktracer(void)
{
    HMODULE mod = LoadLibrary("kernel32.dll");

    if (mod == NULL)
        return;
    Memento_CaptureStackBackTrace = (My_CaptureStackBackTraceType)(GetProcAddress(mod, "RtlCaptureStackBackTrace"));
    if (Memento_CaptureStackBackTrace == NULL)
        return;
    mod = LoadLibrary("Dbghelp.dll");
    if (mod == NULL) {
        Memento_CaptureStackBackTrace = NULL;
        return;
    }
    Memento_SymGetLineFromAddr =
            (My_SymGetLineFromAddrType)(GetProcAddress(mod,
#ifdef _WIN64
                                                       "SymGetLineFromAddr64"
#else
                                                       "SymGetLineFromAddr"
#endif
                                        ));
    if (Memento_SymGetLineFromAddr == NULL) {
        Memento_CaptureStackBackTrace = NULL;
        return;
    }
    Memento_SymFromAddr = (My_SymFromAddrType)(GetProcAddress(mod, "SymFromAddr"));
    if (Memento_SymFromAddr == NULL) {
        Memento_CaptureStackBackTrace = NULL;
        return;
    }
    Memento_SymInitialize = (My_SymInitializeType)(GetProcAddress(mod, "SymInitialize"));
    if (Memento_SymInitialize == NULL) {
        Memento_CaptureStackBackTrace = NULL;
        return;
    }
    Memento_process = GetCurrentProcess();
    Memento_SymInitialize(Memento_process, NULL, TRUE);
}

static int Memento_getStacktrace(void **stack, int *skip)
{
    if (Memento_CaptureStackBackTrace == NULL)
        return 0;

    *skip = 0;

    return Memento_CaptureStackBackTrace(SkipStackBackTraceLevels, 63-SkipStackBackTraceLevels, stack, NULL);
}

static void Memento_showStacktrace(void **stack, int numberOfFrames)
{
    MY_IMAGEHLP_LINE line;
    int i;
    char symbol_buffer[sizeof(MY_SYMBOL_INFO) + 1024 + 1];
    MY_SYMBOL_INFO *symbol = (MY_SYMBOL_INFO *)symbol_buffer;

    symbol->MaxNameLen = 1024;
    symbol->SizeOfStruct = sizeof(MY_SYMBOL_INFO);
    line.SizeOfStruct = sizeof(MY_IMAGEHLP_LINE);
    for (i = 0; i < numberOfFrames; i++)
    {
        DWORD64 dwDisplacement64;
        DWORD dwDisplacement;
        Memento_SymFromAddr(Memento_process, (DWORD64)(stack[i]), &dwDisplacement64, symbol);
        Memento_SymGetLineFromAddr(Memento_process, (DWORD_NATIVESIZED)(stack[i]), &dwDisplacement, &line);
        fprintf(stderr, "    %s in %s:%d\n", symbol->Name, line.FileName, line.LineNumber);
    }
}
#elif defined(MEMENTO_STACKTRACE_METHOD) && MEMENTO_STACKTRACE_METHOD == 3

#include <unwind.h>
#include <dlfcn.h>

extern char* __cxa_demangle(const char* mangled_name,
                            char*       output_buffer,
                            size_t*     length,
                            int*        status);

static void Memento_initStacktracer(void)
{
}

#define MEMENTO_BACKTRACE_MAX 256

typedef struct
{
    int count;
    void **addr;
} my_unwind_details;

static _Unwind_Reason_Code unwind_populate_callback(struct _Unwind_Context *context,
                                                    void *arg)
{
    my_unwind_details *uw = (my_unwind_details *)arg;
    int count = uw->count;

    if (count >= MEMENTO_BACKTRACE_MAX)
        return _URC_END_OF_STACK;

    uw->addr[count] = (void *)_Unwind_GetIP(context);
    uw->count++;

    return _URC_NO_REASON;
}

static int Memento_getStacktrace(void **stack, int *skip)
{
    my_unwind_details uw = { 0, stack };

    *skip = 0;

    _Unwind_Backtrace(unwind_populate_callback, &uw);
    if (uw.count <= SkipStackBackTraceLevels)
        return 0;

    *skip = SkipStackBackTraceLevels;
    return uw.count-SkipStackBackTraceLevels;
}

static void Memento_showStacktrace(void **stack, int numberOfFrames)
{
    int i;

    for (i = 0; i < numberOfFrames; i++)
    {
        Dl_info info;
        if (dladdr(stack[i], &info))
        {
            int status = 0;
            const char *sym = info.dli_sname ? info.dli_sname : "<unknown>";
            char *demangled = __cxa_demangle(sym, NULL, 0, &status);
            int offset = stack[i] - info.dli_saddr;
            fprintf(stderr, "    ["FMTP"]%s(+0x%x)\n", stack[i], demangled && status == 0 ? demangled : sym, offset);
            free(demangled);
        }
        else
        {
            fprintf(stderr, "    ["FMTP"]\n", stack[i]);
        }
    }
}

#else
static void Memento_initStacktracer(void)
{
}

static int Memento_getStacktrace(void **stack, int *skip)
{
    *skip = 0;
    return 0;
}

static void Memento_showStacktrace(void **stack, int numberOfFrames)
{
}
#endif

#ifdef MEMENTO_DETAILS
static void Memento_storeDetails(Memento_BlkHeader *head, int type)
{
    void *stack[MEMENTO_BACKTRACE_MAX];
    Memento_BlkDetails *details;
    int count;
    int skip;

    if (head == NULL)
        return;

#ifdef MEMENTO_STACKTRACE_METHOD
    count = Memento_getStacktrace(stack, &skip);
#else
    skip = 0;
    count = 0;
#endif

    details = MEMENTO_UNDERLYING_MALLOC(sizeof(*details) + (count-1) * sizeof(void *));
    if (details == NULL)
        return;

    if (count)
        memcpy(&details->stack, &stack[skip], count * sizeof(void *));

    details->type = (char)type;
    details->count = (char)count;
    details->sequence = memento.sequence;
    details->next = NULL;
    VALGRIND_MAKE_MEM_DEFINED(&head->details_tail, sizeof(head->details_tail));
    *head->details_tail = details;
    head->details_tail = &details->next;
    VALGRIND_MAKE_MEM_NOACCESS(&head->details_tail, sizeof(head->details_tail));
}
#endif

void (Memento_bt)(void)
{
#ifdef MEMENTO_STACKTRACE_METHOD
    void *stack[MEMENTO_BACKTRACE_MAX];
    int count;
    int skip;

    count = Memento_getStacktrace(stack, &skip);
    Memento_showStacktrace(&stack[skip-2], count-skip+2);
#endif
}

static void Memento_bt_internal(int skip2)
{
#ifdef MEMENTO_STACKTRACE_METHOD
    void *stack[MEMENTO_BACKTRACE_MAX];
    int count;
    int skip;

    count = Memento_getStacktrace(stack, &skip);
    Memento_showStacktrace(&stack[skip+skip2], count-skip-skip2);
#endif
}

static int Memento_checkAllMemoryLocked(void);

void Memento_breakpoint(void)
{

#if 0
#ifndef NDEBUG
#ifdef _MSC_VER
    __asm int 3;
#endif
#endif
#endif
}

static void Memento_init(void);

#define MEMENTO_LOCK() \
do { if (!memento.inited) Memento_init(); MEMENTO_DO_LOCK(); } while (0)

#define MEMENTO_UNLOCK() \
do { MEMENTO_DO_UNLOCK(); } while (0)

#define Memento_breakpointLocked() \
do { MEMENTO_UNLOCK(); Memento_breakpoint(); MEMENTO_LOCK(); } while (0)

static void Memento_addBlockHead(Memento_Blocks    *blks,
                                 Memento_BlkHeader *b,
                                 int                type)
{
    if (blks->tail == NULL)
        blks->tail = b;
    b->next    = blks->head;
    b->prev    = NULL;
    if (blks->head)
    {
        VALGRIND_MAKE_MEM_DEFINED(&blks->head->prev, sizeof(blks->head->prev));
        blks->head->prev = b;
        VALGRIND_MAKE_MEM_NOACCESS(&blks->head->prev, sizeof(blks->head->prev));
    }
    blks->head = b;
#ifndef MEMENTO_LEAKONLY
    memset(b->preblk, MEMENTO_PREFILL, Memento_PreSize);
    memset(MEMBLK_POSTPTR(b), MEMENTO_POSTFILL, Memento_PostSize);
#endif
    VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_POSTPTR(b), Memento_PostSize);
    if (type == 0) {
        VALGRIND_MAKE_MEM_UNDEFINED(MEMBLK_TOBLK(b), b->rawsize);
    } else if (type == 1) {
        VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_TOBLK(b), b->rawsize);
    }
    VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(Memento_BlkHeader));
}

static void Memento_addBlockTail(Memento_Blocks    *blks,
                                 Memento_BlkHeader *b,
                                 int                type)
{
    VALGRIND_MAKE_MEM_DEFINED(&blks->tail, sizeof(Memento_BlkHeader *));
    if (blks->head == NULL)
        blks->head = b;
    b->prev = blks->tail;
    b->next = NULL;
    if (blks->tail) {
        VALGRIND_MAKE_MEM_DEFINED(&blks->tail->next, sizeof(blks->tail->next));
        blks->tail->next = b;
        VALGRIND_MAKE_MEM_NOACCESS(&blks->tail->next, sizeof(blks->tail->next));
    }
    blks->tail = b;
#ifndef MEMENTO_LEAKONLY
    memset(b->preblk, MEMENTO_PREFILL, Memento_PreSize);
    memset(MEMBLK_POSTPTR(b), MEMENTO_POSTFILL, Memento_PostSize);
#endif
    VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_POSTPTR(b), Memento_PostSize);
    if (type == 0) {
        VALGRIND_MAKE_MEM_UNDEFINED(MEMBLK_TOBLK(b), b->rawsize);
    } else if (type == 1) {
        VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_TOBLK(b), b->rawsize);
    }
    VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(Memento_BlkHeader));
    VALGRIND_MAKE_MEM_NOACCESS(&blks->tail, sizeof(Memento_BlkHeader *));
}

typedef struct BlkCheckData {
    int found;
    int preCorrupt;
    int postCorrupt;
    int freeCorrupt;
    size_t index;
} BlkCheckData;

#ifndef MEMENTO_LEAKONLY
static int Memento_Internal_checkAllocedBlock(Memento_BlkHeader *b, void *arg)
{
    int             i;
    MEMENTO_UINT32 *ip;
    unsigned char  *p;
    BlkCheckData   *data = (BlkCheckData *)arg;

    ip = (MEMENTO_UINT32 *)(void *)(b->preblk);
    i = Memento_PreSize>>2;
    do {
        if (*ip++ != MEMENTO_PREFILL_UINT32)
            goto pre_corrupt;
    } while (--i);
    if (0) {
pre_corrupt:
        data->preCorrupt = 1;
    }

    p = MEMBLK_POSTPTR(b);
    i = Memento_PostSize-4;
    if ((intptr_t)p & 1)
    {
        if (*p++ != MEMENTO_POSTFILL)
            goto post_corrupt;
        i--;
    }
    if ((intptr_t)p & 2)
    {
        if (*(MEMENTO_UINT16 *)p != MEMENTO_POSTFILL_UINT16)
            goto post_corrupt;
        p += 2;
        i -= 2;
    }
    do {
        if (*(MEMENTO_UINT32 *)p != MEMENTO_POSTFILL_UINT32)
            goto post_corrupt;
        p += 4;
        i -= 4;
    } while (i >= 0);
    if (i & 2)
    {
        if (*(MEMENTO_UINT16 *)p != MEMENTO_POSTFILL_UINT16)
            goto post_corrupt;
        p += 2;
    }
    if (i & 1)
    {
        if (*p != MEMENTO_POSTFILL)
            goto post_corrupt;
    }
    if (0) {
post_corrupt:
        data->postCorrupt = 1;
    }
    if ((data->freeCorrupt | data->preCorrupt | data->postCorrupt) == 0) {
        b->lastCheckedOK = memento.sequence;
    }
    data->found |= 1;
    return 0;
}

static int Memento_Internal_checkFreedBlock(Memento_BlkHeader *b, void *arg)
{
    size_t         i;
    unsigned char *p;
    BlkCheckData  *data = (BlkCheckData *)arg;

    p = MEMBLK_TOBLK(b);
    i = b->rawsize;

    if (i >= 4) {
        i -= 4;
        do {
            if (*(MEMENTO_UINT32 *)p != MEMENTO_FREEFILL_UINT32)
                goto mismatch4;
            p += 4;
            i -= 4;
        } while (i > 0);
        i += 4;
    }
    if (i & 2) {
        if (*(MEMENTO_UINT16 *)p != MEMENTO_FREEFILL_UINT16)
            goto mismatch;
        p += 2;
        i -= 2;
    }
    if (0) {
mismatch4:
        i += 4;
    }
mismatch:
    while (i) {
        if (*p++ != (unsigned char)MEMENTO_FREEFILL)
            break;
        i--;
    }
    if (i) {
        data->freeCorrupt = 1;
        data->index       = b->rawsize-i;
    }
    return Memento_Internal_checkAllocedBlock(b, arg);
}
#endif

static void Memento_removeBlock(Memento_Blocks    *blks,
                                Memento_BlkHeader *b)
{
    VALGRIND_MAKE_MEM_DEFINED(b, sizeof(*b));
    if (b->next) {
        VALGRIND_MAKE_MEM_DEFINED(&b->next->prev, sizeof(b->next->prev));
        b->next->prev = b->prev;
        VALGRIND_MAKE_MEM_NOACCESS(&b->next->prev, sizeof(b->next->prev));
    }
    if (b->prev) {
        VALGRIND_MAKE_MEM_DEFINED(&b->prev->next, sizeof(b->prev->next));
        b->prev->next = b->next;
        VALGRIND_MAKE_MEM_NOACCESS(&b->prev->next, sizeof(b->prev->next));
    }
    if (blks->tail == b)
        blks->tail = b->prev;
    if (blks->head == b)
        blks->head = b->next;
}

static void free_block(Memento_BlkHeader *head)
{
#ifdef MEMENTO_DETAILS
    Memento_BlkDetails *details = head->details;

    while (details)
    {
        Memento_BlkDetails *next = details->next;
        MEMENTO_UNDERLYING_FREE(details);
        details = next;
    }
#endif
    MEMENTO_UNDERLYING_FREE(head);
}

static int Memento_Internal_makeSpace(size_t space)
{

    if (space > MEMENTO_FREELIST_MAX_SINGLE_BLOCK)
        return 0;

    memento.freeListSize += space;

    while (memento.freeListSize > MEMENTO_FREELIST_MAX) {
        Memento_BlkHeader *head = memento.free.head;
        VALGRIND_MAKE_MEM_DEFINED(head, sizeof(*head));
        memento.free.head = head->next;
        memento.freeListSize -= MEMBLK_SIZE(head->rawsize);
        free_block(head);
    }

    if (memento.free.head == NULL)
        memento.free.tail = NULL;
    return 1;
}

static int Memento_appBlocks(Memento_Blocks *blks,
                             int             (*app)(Memento_BlkHeader *,
                                                    void *),
                             void           *arg)
{
    Memento_BlkHeader *head = blks->head;
    Memento_BlkHeader *next;
    int                result;
    while (head) {
        VALGRIND_MAKE_MEM_DEFINED(head, sizeof(Memento_BlkHeader));
        VALGRIND_MAKE_MEM_DEFINED(MEMBLK_TOBLK(head),
                                  head->rawsize + Memento_PostSize);
        result = app(head, arg);
        next = head->next;
        VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_POSTPTR(head), Memento_PostSize);
        VALGRIND_MAKE_MEM_NOACCESS(head, sizeof(Memento_BlkHeader));
        if (result)
            return result;
        head = next;
    }
    return 0;
}

#ifndef MEMENTO_LEAKONLY

static int Memento_appBlockUser(Memento_Blocks    *blks,
                                int                (*app)(Memento_BlkHeader *,
                                                          void *),
                                void              *arg,
                                Memento_BlkHeader *b)
{
    Memento_BlkHeader *head = blks->head;
    Memento_BlkHeader *next;
    int                result;
    while (head && head != b) {
        VALGRIND_MAKE_MEM_DEFINED(head, sizeof(Memento_BlkHeader));
        next = head->next;
       VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_POSTPTR(head), Memento_PostSize);
        head = next;
    }
    if (head == b) {
        VALGRIND_MAKE_MEM_DEFINED(head, sizeof(Memento_BlkHeader));
        VALGRIND_MAKE_MEM_DEFINED(MEMBLK_TOBLK(head),
                                  head->rawsize + Memento_PostSize);
        result = app(head, arg);
        VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_POSTPTR(head), Memento_PostSize);
        VALGRIND_MAKE_MEM_NOACCESS(head, sizeof(Memento_BlkHeader));
        return result;
    }
    return 0;
}

static int Memento_appBlock(Memento_Blocks    *blks,
                            int                (*app)(Memento_BlkHeader *,
                                                      void *),
                            void              *arg,
                            Memento_BlkHeader *b)
{
    int result;
    (void)blks;
    VALGRIND_MAKE_MEM_DEFINED(b, sizeof(Memento_BlkHeader));
    VALGRIND_MAKE_MEM_DEFINED(MEMBLK_TOBLK(b),
                              b->rawsize + Memento_PostSize);
    result = app(b, arg);
    VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_POSTPTR(b), Memento_PostSize);
    VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(Memento_BlkHeader));
    return result;
}
#endif

static int showBlock(Memento_BlkHeader *b, int space)
{
    int seq;
    VALGRIND_MAKE_MEM_DEFINED(b, sizeof(Memento_BlkHeader));
    fprintf(stderr, FMTP":(size=" FMTZ ",num=%d)",
            MEMBLK_TOBLK(b), (FMTZ_CAST)b->rawsize, b->sequence);
    if (b->label)
        fprintf(stderr, "%c(%s)", space, b->label);
    if (b->flags & Memento_Flag_KnownLeak)
        fprintf(stderr, "(Known Leak)");
    seq = b->sequence;
    VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(Memento_BlkHeader));
    return seq;
}

static void blockDisplay(Memento_BlkHeader *b, int n)
{
    n++;
    while (n > 40)
    {
            fprintf(stderr, "*");
            n -= 40;
    }
    while(n > 0)
    {
        int i = n;
        if (i > 32)
            i = 32;
        n -= i;
        fprintf(stderr, "%s", &"                                "[32-i]);
    }
    showBlock(b, '\t');
    fprintf(stderr, "\n");
}

static int Memento_listBlock(Memento_BlkHeader *b,
                             void              *arg)
{
    size_t *counts = (size_t *)arg;
    blockDisplay(b, 0);
    counts[0]++;
    VALGRIND_MAKE_MEM_DEFINED(b, sizeof(Memento_BlkHeader));
    counts[1]+= b->rawsize;
    VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(Memento_BlkHeader));
    return 0;
}

static void doNestedDisplay(Memento_BlkHeader *b,
                            int depth)
{

    do {
        Memento_BlkHeader *c = NULL;
        blockDisplay(b, depth);
        VALGRIND_MAKE_MEM_DEFINED(b, sizeof(Memento_BlkHeader));
        if (b->sibling) {
            c = b->child;
            b = b->sibling;
        } else {
            b = b->child;
            depth++;
        }
        VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(Memento_BlkHeader));
        if (c)
            doNestedDisplay(c, depth+1);
    } while (b);
}

static int ptrcmp(const void *a_, const void *b_)
{
    const char **a = (const char **)a_;
    const char **b = (const char **)b_;
    return (int)(*a-*b);
}

static
int Memento_listBlocksNested(void)
{
    int count, i;
    size_t size;
    Memento_BlkHeader *b, *prev;
    void **blocks, *minptr, *maxptr;
    intptr_t mask;

    count = 0;
    size = 0;
    for (b = memento.used.head; b; b = b->next) {
        VALGRIND_MAKE_MEM_DEFINED(b, sizeof(*b));
        size += b->rawsize;
        count++;
    }

    blocks = MEMENTO_UNDERLYING_MALLOC(sizeof(void *) * count);
    if (blocks == NULL)
        return 1;

    b = memento.used.head;
    minptr = maxptr = MEMBLK_TOBLK(b);
    mask = (intptr_t)minptr;
    for (i = 0; b; b = b->next, i++) {
        void *p = MEMBLK_TOBLK(b);
        mask &= (intptr_t)p;
        if (p < minptr)
            minptr = p;
        if (p > maxptr)
            maxptr = p;
        blocks[i] = p;
        b->flags &= ~Memento_Flag_HasParent;
        b->child   = NULL;
        b->sibling = NULL;
        b->prev    = NULL;
    }
    qsort(blocks, count, sizeof(void *), ptrcmp);

    for (b = memento.used.head; b; b = b->next) {
        char *p = MEMBLK_TOBLK(b);
        size_t end = (b->rawsize < MEMENTO_PTRSEARCH ? b->rawsize : MEMENTO_PTRSEARCH);
        size_t z;
        VALGRIND_MAKE_MEM_DEFINED(p, end);
        if (end > sizeof(void *)-1)
            end -= sizeof(void *)-1;
        else
            end = 0;
        for (z = MEMENTO_SEARCH_SKIP; z < end; z += sizeof(void *)) {
            void *q = *(void **)(&p[z]);
            void **r;

            if ((mask & (intptr_t)q) != mask || q < minptr || q > maxptr)
                continue;

            r = bsearch(&q, blocks, count, sizeof(void *), ptrcmp);
            if (r) {

                Memento_BlkHeader *child = MEMBLK_FROMBLK(*r);
                Memento_BlkHeader *parent;

                if (child->prev != NULL)
                    continue;
                if (child->flags & Memento_Flag_HasParent)
                    continue;

                if (child == b)
                    continue;

                parent = b->prev;
                while (parent != NULL && parent != child)
                    parent = parent->prev;
                if (parent == child)
                    continue;

                child->sibling = b->child;
                b->child = child;
                child->prev = b;
                child->flags |= Memento_Flag_HasParent;
            }
        }
    }

    for (b = memento.used.head; b; b = b->next) {
        if ((b->flags & Memento_Flag_HasParent) == 0)
            doNestedDisplay(b, 0);
    }
    fprintf(stderr, " Total number of blocks = %d\n", count);
    fprintf(stderr, " Total size of blocks = "FMTZ"\n", (FMTZ_CAST)size);

    MEMENTO_UNDERLYING_FREE(blocks);

    prev = NULL;
    for (b = memento.used.head; b;) {
      Memento_BlkHeader *next = b->next;
      b->prev = prev;
      b->child = MEMENTO_CHILD_MAGIC;
      b->sibling = MEMENTO_SIBLING_MAGIC;
      prev = b;
      VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(*b));
      b = next;
    }

    return 0;
}

void Memento_listBlocks(void)
{
    MEMENTO_LOCK();
    fprintf(stderr, "Allocated blocks:\n");
    if (Memento_listBlocksNested())
    {
        size_t counts[2];
        counts[0] = 0;
        counts[1] = 0;
        Memento_appBlocks(&memento.used, Memento_listBlock, &counts[0]);
        fprintf(stderr, " Total number of blocks = "FMTZ"\n", (FMTZ_CAST)counts[0]);
        fprintf(stderr, " Total size of blocks = "FMTZ"\n", (FMTZ_CAST)counts[1]);
    }
    MEMENTO_UNLOCK();
}

static int Memento_listNewBlock(Memento_BlkHeader *b,
                                void              *arg)
{
    if (b->flags & Memento_Flag_OldBlock)
        return 0;
    b->flags |= Memento_Flag_OldBlock;
    return Memento_listBlock(b, arg);
}

void Memento_listNewBlocks(void)
{
    size_t counts[2];
    MEMENTO_LOCK();
    counts[0] = 0;
    counts[1] = 0;
    fprintf(stderr, "Blocks allocated and still extant since last list:\n");
    Memento_appBlocks(&memento.used, Memento_listNewBlock, &counts[0]);
    fprintf(stderr, "  Total number of blocks = "FMTZ"\n", (FMTZ_CAST)counts[0]);
    fprintf(stderr, "  Total size of blocks = "FMTZ"\n", (FMTZ_CAST)counts[1]);
    MEMENTO_UNLOCK();
}

static void Memento_endStats(void)
{
    fprintf(stderr, "Total memory malloced = "FMTZ" bytes\n", (FMTZ_CAST)memento.totalAlloc);
    fprintf(stderr, "Peak memory malloced = "FMTZ" bytes\n", (FMTZ_CAST)memento.peakAlloc);
    fprintf(stderr, FMTZ" mallocs, "FMTZ" frees, "FMTZ" reallocs\n", (FMTZ_CAST)memento.numMallocs,
            (FMTZ_CAST)memento.numFrees, (FMTZ_CAST)memento.numReallocs);
    fprintf(stderr, "Average allocation size "FMTZ" bytes\n", (FMTZ_CAST)
            (memento.numMallocs != 0 ? memento.totalAlloc/memento.numMallocs: 0));
}

void Memento_stats(void)
{
    MEMENTO_LOCK();
    fprintf(stderr, "Current memory malloced = "FMTZ" bytes\n", (FMTZ_CAST)memento.alloc);
    Memento_endStats();
    MEMENTO_UNLOCK();
}

#ifdef MEMENTO_DETAILS
static int showInfo(Memento_BlkHeader *b, void *arg)
{
    Memento_BlkDetails *details;

    (void)arg;

    fprintf(stderr, FMTP":(size="FMTZ",num=%d)",
            MEMBLK_TOBLK(b), (FMTZ_CAST)b->rawsize, b->sequence);
    if (b->label)
        fprintf(stderr, " (%s)", b->label);
    fprintf(stderr, "\nEvents:\n");

    for (details = b->details; details; details = details->next)
    {
        if (memento.hideMultipleReallocs &&
            details->type == Memento_EventType_realloc &&
            details->next &&
            details->next->type == Memento_EventType_realloc) {
            continue;
        }
        fprintf(stderr, "  Event %d (%s)\n", details->sequence, eventType[(int)details->type]);
        Memento_showStacktrace(details->stack, details->count);
    }
    return 0;
}
#endif

void Memento_listBlockInfo(void)
{
#ifdef MEMENTO_DETAILS
    MEMENTO_LOCK();
    fprintf(stderr, "Details of allocated blocks:\n");
    Memento_appBlocks(&memento.used, showInfo, NULL);
    MEMENTO_UNLOCK();
#endif
}

static int Memento_nonLeakBlocksLeaked(void)
{
    Memento_BlkHeader *blk = memento.used.head;
    while (blk)
    {
        Memento_BlkHeader *next;
        int leaked;
        VALGRIND_MAKE_MEM_DEFINED(blk, sizeof(*blk));
        leaked = ((blk->flags & Memento_Flag_KnownLeak) == 0);
        next = blk->next;
        VALGRIND_MAKE_MEM_DEFINED(blk, sizeof(*blk));
        if (leaked)
            return 1;
        blk = next;
    }
    return 0;
}

void Memento_fin(void)
{
    Memento_checkAllMemory();
    if (!memento.segv)
    {
        Memento_endStats();
        if (Memento_nonLeakBlocksLeaked()) {
            Memento_listBlocks();
#ifdef MEMENTO_DETAILS
            fprintf(stderr, "\n");
            Memento_listBlockInfo();
#endif
            Memento_breakpoint();
        }
    }
    if (memento.squeezing) {
        if (memento.pattern == 0)
            fprintf(stderr, "Memory squeezing @ %d complete%s\n", memento.squeezeAt, memento.segv ? " (with SEGV)" : "");
        else
            fprintf(stderr, "Memory squeezing @ %d (%d) complete%s\n", memento.squeezeAt, memento.pattern, memento.segv ? " (with SEGV)" : "");
    } else if (memento.segv) {
        fprintf(stderr, "Memento completed (with SEGV)\n");
    }
    if (memento.failing)
    {
        fprintf(stderr, "MEMENTO_FAILAT=%d\n", memento.failAt);
        fprintf(stderr, "MEMENTO_PATTERN=%d\n", memento.pattern);
    }
    if (memento.nextFailAt != 0)
    {
        fprintf(stderr, "MEMENTO_NEXTFAILAT=%d\n", memento.nextFailAt);
        fprintf(stderr, "MEMENTO_NEXTPATTERN=%d\n", memento.nextPattern);
    }
    if (Memento_nonLeakBlocksLeaked() && memento.abortOnLeak) {
        fprintf(stderr, "Calling abort() because blocks were leaked and MEMENTO_ABORT_ON_LEAK is set.\n");
        abort();
    }
}

static int read_number(const char *text, int *out, int *relative, char **end)
{
    if (text[0] == '+' || text[0] == '-')
        *relative = 1;
    else
        *relative = 0;
    errno = 0;
    *out = (int)strtol(text, end, 0 );
    if (errno || *end == text)
    {
        fprintf(stderr, "Failed to parse number at start of '%s'.\n", text);
        return -1;
    }
    if (0)
         fprintf(stderr, "text='%s': *out=%i *relative=%i\n",
                 text, *out, *relative);
    return 0;
}

static int read_number_delta(const char *text, int *out, char **end)
{
    int e;
    int relative;

    e = read_number(text, out, &relative, end);
    if (e)
        return e;
    if (relative) {
        fprintf(stderr, "Base number should not start with '+' or '-' at start of '%s'.\n",
                text);
        return -1;
    }
    if (*end) {
        if (**end == '-' || **end == '+') {
            int delta;
            e = read_number(*end, &delta, &relative, end);
            if (e)
                return e;
            *out += delta;
        }
    }
    if (0) fprintf(stderr, "text='%s': *out=%i\n", text, *out);

    return 0;
}

static int read_number_range(const char *text, int *begin, int *end, char **string_end)
{
    int e;
    e = read_number_delta(text, begin, string_end);
    if (e)
        return e;
    if (string_end && (*string_end)[0] == '.' && (*string_end)[1] == '.') {
        int relative;
        e = read_number((*string_end) + 2, end, &relative, string_end);
        if (e)
            return e;
        if (relative)
            *end += *begin;
    } else {
        *end = *begin + 1;
    }
    if (*end < *begin) {
        fprintf(stderr, "Range %i..%i has negative extent, at start of '%s'.\n",
                *begin, *end, text);
        return -1;
    }
    if (0) fprintf(stderr, "text='%s': *begin=%i *end=%i\n", text, *begin, *end);

    return 0;
}

static int Memento_add_squeezes(const char *text)
{
    int e = 0;
    for(;;) {
        int     begin;
        int     end;
        char   *string_end;
        if (!*text)
            break;
        e = read_number_range(text, &begin, &end, &string_end);
        if (e)
            break;
        if (*string_end && *string_end != ',') {
            fprintf(stderr, "Expecting comma at start of '%s'.\n", string_end);
            e = -1;
            break;
        }
        fprintf(stderr, "Adding squeeze range %i..%i.\n",
                begin, end);
        memento.squeezes_num += 1;
        memento.squeezes = MEMENTO_UNDERLYING_REALLOC(
                memento.squeezes,
                memento.squeezes_num * sizeof(*memento.squeezes)
                );
        if (!memento.squeezes) {
            fprintf(stderr, "Failed to allocate memory for memento.squeezes_num=%i\n",
                    memento.squeezes_num);
            e = -1;
            break;
        }
        memento.squeezes[memento.squeezes_num-1].begin = begin;
        memento.squeezes[memento.squeezes_num-1].end = end;

        if (*string_end == 0)
            break;
        text = string_end + 1;
    }

    return e;
}

static void Memento_init(void)
{
    char *env;
    memset(&memento, 0, sizeof(memento));
    memento.inited    = 1;
    memento.used.head = NULL;
    memento.used.tail = NULL;
    memento.free.head = NULL;
    memento.free.tail = NULL;
    memento.sequence  = 0;
    memento.countdown = 1024;
    memento.squeezes  = NULL;
    memento.squeezes_num = 0;
    memento.squeezes_pos = 0;

    env = getenv("MEMENTO_FAILAT");
    memento.failAt = (env ? atoi(env) : 0);

    env = getenv("MEMENTO_BREAKAT");
    memento.breakAt = (env ? atoi(env) : 0);

    env = getenv("MEMENTO_PARANOIA");
    memento.paranoia = (env ? atoi(env) : 0);
    if (memento.paranoia == 0)
        memento.paranoia = -1024;

    env = getenv("MEMENTO_PARANOIDAT");
    memento.paranoidAt = (env ? atoi(env) : 0);

    env = getenv("MEMENTO_SQUEEZEAT");
    memento.squeezeAt = (env ? atoi(env) : 0);

    env = getenv("MEMENTO_PATTERN");
    memento.pattern = (env ? atoi(env) : 0);

    env = getenv("MEMENTO_HIDE_MULTIPLE_REALLOCS");
    memento.hideMultipleReallocs = (env ? atoi(env) : 0);

    env = getenv("MEMENTO_ABORT_ON_LEAK");
    memento.abortOnLeak = (env ? atoi(env) : 0);

    env = getenv("MEMENTO_ABORT_ON_CORRUPTION");
    memento.abortOnCorruption = (env ? atoi(env) : 0);

    env = getenv("MEMENTO_SQUEEZES");
    if (env) {
        int e;
        fprintf(stderr, "Parsing squeeze ranges in MEMENTO_SQUEEZES=%s\n", env);
        e = Memento_add_squeezes(env);
        if (e) {
            fprintf(stderr, "Failed to parse MEMENTO_SQUEEZES=%s\n", env);
            exit(1);
        }
    }

    env = getenv("MEMENTO_MAXMEMORY");
    memento.maxMemory = (env ? atoi(env) : 0);

    atexit(Memento_fin);

    Memento_initMutex(&memento.mutex);

    Memento_initStacktracer();

    Memento_breakpoint();
}

typedef struct findBlkData {
    void              *addr;
    Memento_BlkHeader *blk;
    int                flags;
} findBlkData;

static int Memento_containsAddr(Memento_BlkHeader *b,
                                void *arg)
{
    findBlkData *data = (findBlkData *)arg;
    char *blkend = &((char *)MEMBLK_TOBLK(b))[b->rawsize];
    if ((MEMBLK_TOBLK(b) <= data->addr) &&
        ((void *)blkend > data->addr)) {
        data->blk = b;
        data->flags = 1;
        return 1;
    }
    if (((void *)b <= data->addr) &&
        (MEMBLK_TOBLK(b) > data->addr)) {
        data->blk = b;
        data->flags = 2;
        return 1;
    }
    if (((void *)blkend <= data->addr) &&
        ((void *)(blkend + Memento_PostSize) > data->addr)) {
        data->blk = b;
        data->flags = 3;
        return 1;
    }
    return 0;
}

void Memento_info(void *addr)
{
#ifdef MEMENTO_DETAILS
    findBlkData data;

    MEMENTO_LOCK();
    data.addr  = addr;
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&memento.used, Memento_containsAddr, &data);
    if (data.blk != NULL)
        showInfo(data.blk, NULL);
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&memento.free, Memento_containsAddr, &data);
    if (data.blk != NULL)
        showInfo(data.blk, NULL);
    MEMENTO_UNLOCK();
#else
    printf("Memento not compiled with details support\n");
#endif
}

#ifdef MEMENTO_HAS_FORK
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#ifdef MEMENTO_STACKTRACE_METHOD
#if MEMENTO_STACKTRACE_METHOD == 1
#include <signal.h>
#endif
#endif

#ifndef OPEN_MAX
#define OPEN_MAX 10240
#endif

int stashed_map[OPEN_MAX];

static void Memento_signal(int sig)
{
    (void)sig;
    fprintf(stderr, "SEGV at:\n");
    memento.segv = 1;
    Memento_bt_internal(0);

    exit(1);
}

static int squeeze(void)
{
    pid_t pid;
    int i, status;

    if (memento.patternBit < 0)
        return 1;
    if (memento.squeezing && memento.patternBit >= MEMENTO_MAXPATTERN)
        return 1;

    if (memento.patternBit == 0)
        memento.squeezeAt = memento.sequence;

    if (!memento.squeezing) {
        fprintf(stderr, "Memory squeezing @ %d\n", memento.squeezeAt);
    } else
        fprintf(stderr, "Memory squeezing @ %d (%x,%x)\n", memento.squeezeAt, memento.pattern, memento.patternBit);

    for (i = 0; i < OPEN_MAX; i++) {
        if (stashed_map[i] == 0) {
            int j = dup(i);
            if (j >= 0) {
                stashed_map[j] = i+1;
            }
        }
    }

    fprintf(stderr, "Failing at:\n");
    Memento_bt_internal(2);
    pid = fork();
    if (pid == 0) {

        signal(SIGSEGV, Memento_signal);

        for (i = 0; i < OPEN_MAX; i++) {
            if (stashed_map[i] != 0) {

                close(i);
            }
        }

        if (memento.patternBit == 0) {
            memento.patternBit = 1;
        } else
            memento.patternBit <<= 1;
        memento.squeezing = 1;

        memento.squeezes_num = 0;

        return 1;
    }

    memento.pattern |= memento.patternBit;
    memento.patternBit <<= 1;

    {
        struct timespec tm = { 0, 10 * 1000 * 1000 };
        int timeout = 30 * 1000 * 1000;
        while (waitpid(pid, &status, WNOHANG) == 0) {
            nanosleep(&tm, NULL);
            timeout -= (int)(tm.tv_nsec/1000);
            tm.tv_nsec *= 2;
            if (tm.tv_nsec > 999999999)
                tm.tv_nsec = 999999999;
            if (timeout <= 0) {
                char text[32];
                fprintf(stderr, "Child is taking a long time to die. Killing it.\n");
                sprintf(text, "kill %d", pid);
                system(text);
                break;
            }
        }
    }

    if (status != 0) {
        fprintf(stderr, "Child status=%d\n", status);
    }

    for (i = 0; i < OPEN_MAX; i++) {
        if (stashed_map[i] != 0) {
            dup2(i, stashed_map[i]-1);
            close(i);
            stashed_map[i] = 0;
        }
    }

    return 0;
}
#else
#include <signal.h>

static void Memento_signal(int sig)
{
    (void)sig;
    memento.segv = 1;

    if (getenv("MEMENTO_NOJIT"))
        exit(1);
    else
        Memento_fin();
}

static int squeeze(void)
{
    fprintf(stderr, "Memento memory squeezing disabled as no fork!\n");
    return 0;
}
#endif

static void Memento_startFailing(void)
{
    if (!memento.failing) {
        fprintf(stderr, "Starting to fail...\n");
        Memento_bt();
        fflush(stderr);
        memento.failing = 1;
        memento.failAt = memento.sequence;
        memento.nextFailAt = memento.sequence+1;
        memento.pattern = 0;
        memento.patternBit = 0;
        signal(SIGSEGV, Memento_signal);
        signal(SIGABRT, Memento_signal);
        Memento_breakpointLocked();
    }
}

static int Memento_event(void)
{
    memento.sequence++;
    if ((memento.sequence >= memento.paranoidAt) && (memento.paranoidAt != 0)) {
        memento.paranoia = 1;
        memento.countdown = 1;
    }
    if (--memento.countdown == 0) {
        Memento_checkAllMemoryLocked();
        if (memento.paranoia > 0)
            memento.countdown = memento.paranoia;
        else
        {
            memento.countdown = -memento.paranoia;
            if (memento.paranoia > INT_MIN/2)
                memento.paranoia *= 2;
        }
    }

    if (memento.sequence == memento.breakAt) {
        fprintf(stderr, "Breaking at event %d\n", memento.breakAt);
        return 1;
    }
    return 0;
}

int Memento_sequence(void)
{
    return memento.sequence;
}

int Memento_breakAt(int event)
{
    MEMENTO_LOCK();
    memento.breakAt = event;
    MEMENTO_UNLOCK();
    return event;
}

static void *safe_find_block(void *ptr)
{
    Memento_BlkHeader *block;
    int valid;

    if (ptr == NULL)
        return NULL;

    block = MEMBLK_FROMBLK(ptr);

    VALGRIND_MAKE_MEM_DEFINED(&block->child, sizeof(block->child));
    VALGRIND_MAKE_MEM_DEFINED(&block->sibling, sizeof(block->sibling));
    valid = (block->child == MEMENTO_CHILD_MAGIC &&
             block->sibling == MEMENTO_SIBLING_MAGIC);
    VALGRIND_MAKE_MEM_NOACCESS(&block->child, sizeof(block->child));
    VALGRIND_MAKE_MEM_NOACCESS(&block->sibling, sizeof(block->sibling));
    if (!valid)
    {
        findBlkData data;

        data.addr  = ptr;
        data.blk   = NULL;
        data.flags = 0;
        Memento_appBlocks(&memento.used, Memento_containsAddr, &data);
        if (data.blk == NULL)
            return NULL;
        block = data.blk;
    }
    return block;
}

void *Memento_label(void *ptr, const char *label)
{
    Memento_BlkHeader *block;

    if (ptr == NULL)
        return NULL;
    MEMENTO_LOCK();
    block = safe_find_block(ptr);
    if (block != NULL)
    {
        VALGRIND_MAKE_MEM_DEFINED(&block->label, sizeof(block->label));
        block->label = label;
        VALGRIND_MAKE_MEM_NOACCESS(&block->label, sizeof(block->label));
    }
    MEMENTO_UNLOCK();
    return ptr;
}

void Memento_tick(void)
{
    MEMENTO_LOCK();
    if (Memento_event()) Memento_breakpointLocked();
    MEMENTO_UNLOCK();
}

static int Memento_failThisEventLocked(void)
{
    int failThisOne;

    if (Memento_event()) Memento_breakpointLocked();

    if (!memento.squeezing && memento.squeezes_num) {

        for ( ; memento.squeezes_pos != memento.squeezes_num; memento.squeezes_pos++) {
            if (memento.sequence < memento.squeezes[memento.squeezes_pos].end)
                break;
        }

        if (memento.squeezes_pos < memento.squeezes_num) {
            int begin = memento.squeezes[memento.squeezes_pos].begin;
            int end   = memento.squeezes[memento.squeezes_pos].end;
            if (memento.sequence >= begin && memento.sequence < end) {
                if (1) {
                    fprintf(stderr,
                            "squeezes match memento.sequence=%i: memento.squeezes_pos=%i/%i %i..%i\n",
                            memento.sequence,
                            memento.squeezes_pos,
                            memento.squeezes_num,
                            memento.squeezes[memento.squeezes_pos].begin,
                            memento.squeezes[memento.squeezes_pos].end
                            );
                }
                return squeeze();
            }
        }
    }

    if ((memento.sequence >= memento.failAt) && (memento.failAt != 0))
        Memento_startFailing();
    if ((memento.squeezes_num==0) && (memento.sequence >= memento.squeezeAt) && (memento.squeezeAt != 0))
        return squeeze();

    if (!memento.failing)
        return 0;
    failThisOne = ((memento.patternBit & memento.pattern) == 0);

    if (memento.failing &&
        ((~(memento.patternBit-1) & memento.pattern) == 0) &&
        (memento.patternBit != 0) &&
        memento.nextPattern == 0)
    {

        memento.nextFailAt = memento.failAt;
        memento.nextPattern = memento.pattern | memento.patternBit;
    }
    memento.patternBit = (memento.patternBit ? memento.patternBit << 1 : 1);

    return failThisOne;
}

int Memento_failThisEvent(void)
{
    int ret;

    if (!memento.inited)
        Memento_init();

    MEMENTO_LOCK();
    ret = Memento_failThisEventLocked();
    MEMENTO_UNLOCK();
    return ret;
}

static void *do_malloc(size_t s, int eventType)
{
    Memento_BlkHeader *memblk;
    size_t             smem = MEMBLK_SIZE(s);

    (void)eventType;

    if (Memento_failThisEventLocked()) {
        errno = ENOMEM;
        return NULL;
    }

    if (s == 0)
        return NULL;

    memento.numMallocs++;

    if (memento.maxMemory != 0 && memento.alloc + s > memento.maxMemory) {
        errno = ENOMEM;
        return NULL;
    }

    memblk = MEMENTO_UNDERLYING_MALLOC(smem);
    if (memblk == NULL)
        return NULL;

    memento.alloc      += s;
    memento.totalAlloc += s;
    if (memento.peakAlloc < memento.alloc)
        memento.peakAlloc = memento.alloc;
#ifndef MEMENTO_LEAKONLY
    memset(MEMBLK_TOBLK(memblk), MEMENTO_ALLOCFILL, s);
#endif
    memblk->rawsize       = s;
    memblk->sequence      = memento.sequence;
    memblk->lastCheckedOK = memblk->sequence;
    memblk->flags         = 0;
    memblk->label         = 0;
    memblk->child         = MEMENTO_CHILD_MAGIC;
    memblk->sibling       = MEMENTO_SIBLING_MAGIC;
#ifdef MEMENTO_DETAILS
    memblk->details       = NULL;
    memblk->details_tail  = &memblk->details;
    Memento_storeDetails(memblk, eventType);
#endif
    Memento_addBlockHead(&memento.used, memblk, 0);

    if (memento.leaking > 0)
        memblk->flags |= Memento_Flag_KnownLeak;

    return MEMBLK_TOBLK(memblk);
}

char *Memento_strdup(const char *text)
{
    size_t len = strlen(text) + 1;
    char *ret;

    if (!memento.inited)
        Memento_init();

    MEMENTO_LOCK();
    ret = do_malloc(len, Memento_EventType_strdup);
    MEMENTO_UNLOCK();

    if (ret != NULL)
        memcpy(ret, text, len);

    return ret;
}

int Memento_asprintf(char **ret, const char *format, ...)
{
    va_list va;
    int n;
    int n2;

    if (!memento.inited)
        Memento_init();

    va_start(va, format);
    n = vsnprintf(NULL, 0, format, va);
    va_end(va);
    if (n < 0)
        return n;

    MEMENTO_LOCK();
    *ret = do_malloc(n+1, Memento_EventType_asprintf);
    MEMENTO_UNLOCK();
    if (*ret == NULL)
        return -1;

    va_start(va, format);
    n2 = vsnprintf(*ret, n + 1, format, va);
    va_end(va);

    return n2;
}

int Memento_vasprintf(char **ret, const char *format, va_list ap)
{
    int n;
    va_list ap2;
    va_copy(ap2, ap);

    if (!memento.inited)
        Memento_init();

    n = vsnprintf(NULL, 0, format, ap);
    if (n < 0) {
        va_end(ap2);
        return n;
    }

    MEMENTO_LOCK();
    *ret = do_malloc(n+1, Memento_EventType_vasprintf);
    MEMENTO_UNLOCK();
    if (*ret == NULL) {
        va_end(ap2);
        return -1;
    }

    n = vsnprintf(*ret, n + 1, format, ap2);
    va_end(ap2);

    return n;
}

void *Memento_malloc(size_t s)
{
    void *ret;

    if (!memento.inited)
        Memento_init();

    MEMENTO_LOCK();
    ret = do_malloc(s, Memento_EventType_malloc);
    MEMENTO_UNLOCK();

    return ret;
}

void *Memento_calloc(size_t n, size_t s)
{
    void *block;

    if (!memento.inited)
        Memento_init();

    MEMENTO_LOCK();
    block = do_malloc(n*s, Memento_EventType_calloc);
    MEMENTO_UNLOCK();
    if (block)
        memset(block, 0, n*s);

    return block;
}

static void do_reference(Memento_BlkHeader *blk, int event)
{
#ifdef MEMENTO_DETAILS
    Memento_storeDetails(blk, event);
#endif
}

int Memento_checkPointerOrNull(void *blk)
{
    if (blk == NULL)
        return 0;
    if (blk == MEMENTO_PREFILL_PTR)
        fprintf(stderr, "Prefill value found as pointer - buffer underrun?\n");
    else if (blk == MEMENTO_POSTFILL_PTR)
        fprintf(stderr, "Postfill value found as pointer - buffer overrun?\n");
    else if (blk == MEMENTO_ALLOCFILL_PTR)
        fprintf(stderr, "Allocfill value found as pointer - use of uninitialised value?\n");
    else if (blk == MEMENTO_FREEFILL_PTR)
        fprintf(stderr, "Allocfill value found as pointer - use after free?\n");
    else
        return 0;
#ifdef MEMENTO_DETAILS
    fprintf(stderr, "Current backtrace:\n");
    Memento_bt();
    fprintf(stderr, "History:\n");
    Memento_info(blk);
#endif
    return 1;
}

int Memento_checkBytePointerOrNull(void *blk)
{
    unsigned char i;
    if (blk == NULL)
        return 0;
    Memento_checkPointerOrNull(blk);

    i = *(unsigned char *)blk;

    if (i == MEMENTO_PREFILL_UBYTE)
        fprintf(stderr, "Prefill value found - buffer underrun?\n");
    else if (i == MEMENTO_POSTFILL_UBYTE)
        fprintf(stderr, "Postfill value found - buffer overrun?\n");
    else if (i == MEMENTO_ALLOCFILL_UBYTE)
        fprintf(stderr, "Allocfill value found - use of uninitialised value?\n");
    else if (i == MEMENTO_FREEFILL_UBYTE)
        fprintf(stderr, "Allocfill value found - use after free?\n");
    else
        return 0;
#ifdef MEMENTO_DETAILS
    fprintf(stderr, "Current backtrace:\n");
    Memento_bt();
    fprintf(stderr, "History:\n");
    Memento_info(blk);
#endif
    Memento_breakpoint();
    return 1;
}

int Memento_checkShortPointerOrNull(void *blk)
{
    unsigned short i;
    if (blk == NULL)
        return 0;
    Memento_checkPointerOrNull(blk);

    i = *(unsigned short *)blk;

    if (i == MEMENTO_PREFILL_USHORT)
        fprintf(stderr, "Prefill value found - buffer underrun?\n");
    else if (i == MEMENTO_POSTFILL_USHORT)
        fprintf(stderr, "Postfill value found - buffer overrun?\n");
    else if (i == MEMENTO_ALLOCFILL_USHORT)
        fprintf(stderr, "Allocfill value found - use of uninitialised value?\n");
    else if (i == MEMENTO_FREEFILL_USHORT)
        fprintf(stderr, "Allocfill value found - use after free?\n");
    else
        return 0;
#ifdef MEMENTO_DETAILS
    fprintf(stderr, "Current backtrace:\n");
    Memento_bt();
    fprintf(stderr, "History:\n");
    Memento_info(blk);
#endif
    Memento_breakpoint();
    return 1;
}

int Memento_checkIntPointerOrNull(void *blk)
{
    unsigned int i;
    if (blk == NULL)
        return 0;
    Memento_checkPointerOrNull(blk);

    i = *(unsigned int *)blk;

    if (i == MEMENTO_PREFILL_UINT)
        fprintf(stderr, "Prefill value found - buffer underrun?\n");
    else if (i == MEMENTO_POSTFILL_UINT)
        fprintf(stderr, "Postfill value found - buffer overrun?\n");
    else if (i == MEMENTO_ALLOCFILL_UINT)
        fprintf(stderr, "Allocfill value found - use of uninitialised value?\n");
    else if (i == MEMENTO_FREEFILL_UINT)
        fprintf(stderr, "Allocfill value found - use after free?\n");
    else
        return 0;
#ifdef MEMENTO_DETAILS
    fprintf(stderr, "Current backtrace:\n");
    Memento_bt();
    fprintf(stderr, "History:\n");
    Memento_info(blk);
#endif
    Memento_breakpoint();
    return 1;
}

static void *do_takeRef(void *blk)
{
    MEMENTO_LOCK();
    do_reference(safe_find_block(blk), Memento_EventType_takeRef);
    MEMENTO_UNLOCK();
    return blk;
}

void *Memento_takeByteRef(void *blk)
{
    if (!memento.inited)
        Memento_init();

    if (Memento_event()) Memento_breakpoint();

    if (!blk)
        return NULL;

    (void)Memento_checkBytePointerOrNull(blk);

    return do_takeRef(blk);
}

void *Memento_takeShortRef(void *blk)
{
    if (!memento.inited)
        Memento_init();

    if (Memento_event()) Memento_breakpoint();

    if (!blk)
        return NULL;

    (void)Memento_checkShortPointerOrNull(blk);

    return do_takeRef(blk);
}

void *Memento_takeIntRef(void *blk)
{
    if (!memento.inited)
        Memento_init();

    if (Memento_event()) Memento_breakpoint();

    if (!blk)
        return NULL;

    (void)Memento_checkIntPointerOrNull(blk);

    return do_takeRef(blk);
}

void *Memento_takeRef(void *blk)
{
    if (!memento.inited)
        Memento_init();

    if (Memento_event()) Memento_breakpoint();

    if (!blk)
        return NULL;

    return do_takeRef(blk);
}

static void *do_dropRef(void *blk)
{
    MEMENTO_LOCK();
    do_reference(safe_find_block(blk), Memento_EventType_dropRef);
    MEMENTO_UNLOCK();
    return blk;
}

void *Memento_dropByteRef(void *blk)
{
    if (!memento.inited)
        Memento_init();

    if (Memento_event()) Memento_breakpoint();

    if (!blk)
        return NULL;

    Memento_checkBytePointerOrNull(blk);

    return do_dropRef(blk);
}

void *Memento_dropShortRef(void *blk)
{
    if (!memento.inited)
        Memento_init();

    if (Memento_event()) Memento_breakpoint();

    if (!blk)
        return NULL;

    Memento_checkShortPointerOrNull(blk);

    return do_dropRef(blk);
}

void *Memento_dropIntRef(void *blk)
{
    if (!memento.inited)
        Memento_init();

    if (Memento_event()) Memento_breakpoint();

    if (!blk)
        return NULL;

    Memento_checkIntPointerOrNull(blk);

    return do_dropRef(blk);
}

void *Memento_dropRef(void *blk)
{
    if (!memento.inited)
        Memento_init();

    if (Memento_event()) Memento_breakpoint();

    if (!blk)
        return NULL;

    return do_dropRef(blk);
}

void *Memento_adjustRef(void *blk, int adjust)
{
    if (Memento_event()) Memento_breakpoint();

    if (blk == NULL)
        return NULL;

    while (adjust > 0)
    {
        do_takeRef(blk);
        adjust--;
    }
    while (adjust < 0)
    {
        do_dropRef(blk);
        adjust++;
    }

    return blk;
 }

void *Memento_reference(void *blk)
{
    if (!blk)
        return NULL;

    if (!memento.inited)
        Memento_init();

    MEMENTO_LOCK();
    do_reference(safe_find_block(blk), Memento_EventType_reference);
    MEMENTO_UNLOCK();
    return blk;
}

static int checkBlockUser(Memento_BlkHeader *memblk, const char *action)
{
#ifndef MEMENTO_LEAKONLY
    BlkCheckData data;

    memset(&data, 0, sizeof(data));
    Memento_appBlockUser(&memento.used, Memento_Internal_checkAllocedBlock,
                         &data, memblk);
    if (!data.found) {

        fprintf(stderr, "Attempt to %s block ", action);
        showBlock(memblk, 32);
        fprintf(stderr, "\n");
        Memento_breakpointLocked();
        return 1;
    } else if (data.preCorrupt || data.postCorrupt) {
        fprintf(stderr, "Block ");
        showBlock(memblk, ' ');
        fprintf(stderr, " found to be corrupted on %s!\n", action);
        if (data.preCorrupt) {
            fprintf(stderr, "Preguard corrupted\n");
        }
        if (data.postCorrupt) {
            fprintf(stderr, "Postguard corrupted\n");
        }
        fprintf(stderr, "Block last checked OK at allocation %d. Now %d.\n",
                memblk->lastCheckedOK, memento.sequence);
        if ((memblk->flags & Memento_Flag_Reported) == 0)
        {
            memblk->flags |= Memento_Flag_Reported;
            Memento_breakpointLocked();
        }
        return 1;
    }
#endif
    return 0;
}

static int checkBlock(Memento_BlkHeader *memblk, const char *action)
{
#ifndef MEMENTO_LEAKONLY
    BlkCheckData data;
#endif

    if (memblk->child != MEMENTO_CHILD_MAGIC ||
        memblk->sibling != MEMENTO_SIBLING_MAGIC)
    {

        fprintf(stderr, "Attempt to %s invalid block ", action);
        showBlock(memblk, 32);
        fprintf(stderr, "\n");
        Memento_breakpointLocked();
        return 1;
    }

#ifndef MEMENTO_LEAKONLY
    memset(&data, 0, sizeof(data));
    Memento_appBlock(&memento.used, Memento_Internal_checkAllocedBlock,
                     &data, memblk);
    if (!data.found) {

        fprintf(stderr, "Attempt to %s block ", action);
        showBlock(memblk, 32);
        fprintf(stderr, "\n");
        Memento_breakpointLocked();
        return 1;
    } else if (data.preCorrupt || data.postCorrupt) {
        fprintf(stderr, "Block ");
        showBlock(memblk, ' ');
        fprintf(stderr, " found to be corrupted on %s!\n", action);
        if (data.preCorrupt) {
            fprintf(stderr, "Preguard corrupted\n");
        }
        if (data.postCorrupt) {
            fprintf(stderr, "Postguard corrupted\n");
        }
        fprintf(stderr, "Block last checked OK at allocation %d. Now %d.\n",
                memblk->lastCheckedOK, memento.sequence);
        if ((memblk->flags & Memento_Flag_Reported) == 0)
        {
            memblk->flags |= Memento_Flag_Reported;
            Memento_breakpointLocked();
        }
        return 1;
    }
#endif
    return 0;
}

static void do_free(void *blk, int eventType)
{
    Memento_BlkHeader *memblk;

    (void)eventType;

    if (Memento_event()) Memento_breakpointLocked();

    if (blk == NULL)
        return;

    memblk = MEMBLK_FROMBLK(blk);
    VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
    if (checkBlock(memblk, "free"))
    {
        if (memento.abortOnCorruption) {
            fprintf(stderr, "*** memblk corrupted, calling abort()\n");
            abort();
        }
        return;
    }

#ifdef MEMENTO_DETAILS
    Memento_storeDetails(memblk, eventType);
#endif

    VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
    if (memblk->flags & Memento_Flag_BreakOnFree)
        Memento_breakpointLocked();

    memento.alloc -= memblk->rawsize;
    memento.numFrees++;

    Memento_removeBlock(&memento.used, memblk);

    VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
    if (Memento_Internal_makeSpace(MEMBLK_SIZE(memblk->rawsize))) {
        VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
        VALGRIND_MAKE_MEM_DEFINED(MEMBLK_TOBLK(memblk),
                                  memblk->rawsize + Memento_PostSize);
#ifndef MEMENTO_LEAKONLY
        memset(MEMBLK_TOBLK(memblk), MEMENTO_FREEFILL, memblk->rawsize);
#endif
        memblk->flags |= Memento_Flag_Freed;
        Memento_addBlockTail(&memento.free, memblk, 1);
    } else {
        free_block(memblk);
    }
}

void Memento_free(void *blk)
{
    if (!memento.inited)
        Memento_init();

    MEMENTO_LOCK();
    do_free(blk, Memento_EventType_free);
    MEMENTO_UNLOCK();
}

static void *do_realloc(void *blk, size_t newsize, int type)
{
    Memento_BlkHeader *memblk, *newmemblk;
    size_t             newsizemem;
    int                flags;

    if (Memento_failThisEventLocked()) {
        errno = ENOMEM;
        return NULL;
    }

    memblk     = MEMBLK_FROMBLK(blk);
    VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
    if (checkBlock(memblk, "realloc")) {
        errno = ENOMEM;
        return NULL;
    }

#ifdef MEMENTO_DETAILS
    Memento_storeDetails(memblk, type);
#endif

    VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
    if (memblk->flags & Memento_Flag_BreakOnRealloc)
        Memento_breakpointLocked();

    VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
    if (memento.maxMemory != 0 && memento.alloc - memblk->rawsize + newsize > memento.maxMemory) {
        errno = ENOMEM;
        return NULL;
    }

    newsizemem = MEMBLK_SIZE(newsize);
    Memento_removeBlock(&memento.used, memblk);
    VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
    flags = memblk->flags;
    newmemblk  = MEMENTO_UNDERLYING_REALLOC(memblk, newsizemem);
    if (newmemblk == NULL)
    {
        Memento_addBlockHead(&memento.used, memblk, 2);
        return NULL;
    }
    memento.numReallocs++;
    memento.totalAlloc += newsize;
    memento.alloc      -= newmemblk->rawsize;
    memento.alloc      += newsize;
    if (memento.peakAlloc < memento.alloc)
        memento.peakAlloc = memento.alloc;
    newmemblk->flags = flags;
#ifndef MEMENTO_LEAKONLY
    if (newmemblk->rawsize < newsize) {
        char *newbytes = ((char *)MEMBLK_TOBLK(newmemblk))+newmemblk->rawsize;
        VALGRIND_MAKE_MEM_DEFINED(newbytes, newsize - newmemblk->rawsize);
        memset(newbytes, MEMENTO_ALLOCFILL, newsize - newmemblk->rawsize);
        VALGRIND_MAKE_MEM_UNDEFINED(newbytes, newsize - newmemblk->rawsize);
    }
#endif
    newmemblk->rawsize = newsize;
#ifndef MEMENTO_LEAKONLY
    VALGRIND_MAKE_MEM_DEFINED(newmemblk->preblk, Memento_PreSize);
    memset(newmemblk->preblk, MEMENTO_PREFILL, Memento_PreSize);
    VALGRIND_MAKE_MEM_UNDEFINED(newmemblk->preblk, Memento_PreSize);
    VALGRIND_MAKE_MEM_DEFINED(MEMBLK_POSTPTR(newmemblk), Memento_PostSize);
    memset(MEMBLK_POSTPTR(newmemblk), MEMENTO_POSTFILL, Memento_PostSize);
    VALGRIND_MAKE_MEM_UNDEFINED(MEMBLK_POSTPTR(newmemblk), Memento_PostSize);
#endif
    Memento_addBlockHead(&memento.used, newmemblk, 2);
    return MEMBLK_TOBLK(newmemblk);
}

void *Memento_realloc(void *blk, size_t newsize)
{
    void *ret;

    if (!memento.inited)
        Memento_init();

    if (blk == NULL)
    {
        MEMENTO_LOCK();
        ret = do_malloc(newsize, Memento_EventType_realloc);
        MEMENTO_UNLOCK();
        if (!ret) errno = ENOMEM;
        return ret;
    }
    if (newsize == 0) {
        MEMENTO_LOCK();
        do_free(blk, Memento_EventType_realloc);
        MEMENTO_UNLOCK();
        return NULL;
    }

    MEMENTO_LOCK();
    ret = do_realloc(blk, newsize, Memento_EventType_realloc);
    MEMENTO_UNLOCK();
    if (!ret) errno = ENOMEM;
    return ret;
}

int Memento_checkBlock(void *blk)
{
    Memento_BlkHeader *memblk;
    int ret;

    if (blk == NULL)
        return 0;

    MEMENTO_LOCK();
    memblk = MEMBLK_FROMBLK(blk);
    ret = checkBlockUser(memblk, "check");
    MEMENTO_UNLOCK();
    return ret;
}

#ifndef MEMENTO_LEAKONLY
static int Memento_Internal_checkAllAlloced(Memento_BlkHeader *memblk, void *arg)
{
    BlkCheckData *data = (BlkCheckData *)arg;

    Memento_Internal_checkAllocedBlock(memblk, data);
    if (data->preCorrupt || data->postCorrupt) {
        if ((data->found & 2) == 0) {
            fprintf(stderr, "Allocated blocks:\n");
            data->found |= 2;
        }
        fprintf(stderr, "  Block ");
        showBlock(memblk, ' ');
        if (data->preCorrupt) {
            fprintf(stderr, " Preguard ");
        }
        if (data->postCorrupt) {
            fprintf(stderr, "%s Postguard ",
                    (data->preCorrupt ? "&" : ""));
        }
        fprintf(stderr, "corrupted.\n    "
                "Block last checked OK at allocation %d. Now %d.\n",
                memblk->lastCheckedOK, memento.sequence);
        data->preCorrupt  = 0;
        data->postCorrupt = 0;
        data->freeCorrupt = 0;
        if ((memblk->flags & Memento_Flag_Reported) == 0)
        {
            memblk->flags |= Memento_Flag_Reported;
            Memento_breakpointLocked();
        }
    }
    else
        memblk->lastCheckedOK = memento.sequence;
    return 0;
}

static int Memento_Internal_checkAllFreed(Memento_BlkHeader *memblk, void *arg)
{
    BlkCheckData *data = (BlkCheckData *)arg;

    Memento_Internal_checkFreedBlock(memblk, data);
    if (data->preCorrupt || data->postCorrupt || data->freeCorrupt) {
        if ((data->found & 4) == 0) {
            fprintf(stderr, "Freed blocks:\n");
            data->found |= 4;
        }
        fprintf(stderr, "  ");
        showBlock(memblk, ' ');
        if (data->freeCorrupt) {
            fprintf(stderr, " index %d (address "FMTP") onwards", (int)data->index,
                    &((char *)MEMBLK_TOBLK(memblk))[data->index]);
            if (data->preCorrupt) {
                fprintf(stderr, "+ preguard");
            }
            if (data->postCorrupt) {
                fprintf(stderr, "+ postguard");
            }
        } else {
            if (data->preCorrupt) {
                fprintf(stderr, " preguard");
            }
            if (data->postCorrupt) {
                fprintf(stderr, "%s Postguard",
                        (data->preCorrupt ? "+" : ""));
            }
        }
        VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(Memento_BlkHeader));
        fprintf(stderr, " corrupted.\n"
                "    Block last checked OK at allocation %d. Now %d.\n",
                memblk->lastCheckedOK, memento.sequence);
        if ((memblk->flags & Memento_Flag_Reported) == 0)
        {
            memblk->flags |= Memento_Flag_Reported;
            Memento_breakpointLocked();
        }
        VALGRIND_MAKE_MEM_NOACCESS(memblk, sizeof(Memento_BlkHeader));
        data->preCorrupt  = 0;
        data->postCorrupt = 0;
        data->freeCorrupt = 0;
    }
    else
        memblk->lastCheckedOK = memento.sequence;
    return 0;
}
#endif

static int Memento_checkAllMemoryLocked(void)
{
#ifndef MEMENTO_LEAKONLY
    BlkCheckData data;

    memset(&data, 0, sizeof(data));
    Memento_appBlocks(&memento.used, Memento_Internal_checkAllAlloced, &data);
    Memento_appBlocks(&memento.free, Memento_Internal_checkAllFreed, &data);
    return data.found;
#else
    return 0;
#endif
}

int Memento_checkAllMemory(void)
{
#ifndef MEMENTO_LEAKONLY
    int ret;

    MEMENTO_LOCK();
    ret = Memento_checkAllMemoryLocked();
    MEMENTO_UNLOCK();
    if (ret & 6) {
        Memento_breakpoint();
        return 1;
    }
    return 0;
#endif
}

int Memento_setParanoia(int i)
{
    memento.paranoia = i;
    if (memento.paranoia > 0)
        memento.countdown = memento.paranoia;
    else
        memento.countdown = -memento.paranoia;
    return i;
}

int Memento_paranoidAt(int i)
{
    memento.paranoidAt = i;
    return i;
}

int Memento_getBlockNum(void *b)
{
    Memento_BlkHeader *memblk;
    if (b == NULL)
        return 0;
    memblk = MEMBLK_FROMBLK(b);
    return (memblk->sequence);
}

int Memento_check(void)
{
    int result;

    fprintf(stderr, "Checking memory\n");
    result = Memento_checkAllMemory();
    fprintf(stderr, "Memory checked!\n");
    return result;
}

int Memento_find(void *a)
{
    findBlkData data;
    int s;

    MEMENTO_LOCK();
    data.addr  = a;
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&memento.used, Memento_containsAddr, &data);
    if (data.blk != NULL) {
        fprintf(stderr, "Address "FMTP" is in %sallocated block ",
                data.addr,
                (data.flags == 1 ? "" : (data.flags == 2 ?
                                         "preguard of " : "postguard of ")));
        s = showBlock(data.blk, ' ');
        fprintf(stderr, "\n");
        MEMENTO_UNLOCK();
        return s;
    }
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&memento.free, Memento_containsAddr, &data);
    if (data.blk != NULL) {
        fprintf(stderr, "Address "FMTP" is in %sfreed block ",
                data.addr,
                (data.flags == 1 ? "" : (data.flags == 2 ?
                                         "preguard of " : "postguard of ")));
        s = showBlock(data.blk, ' ');
        fprintf(stderr, "\n");
        MEMENTO_UNLOCK();
        return s;
    }
    MEMENTO_UNLOCK();
    return 0;
}

void Memento_breakOnFree(void *a)
{
    findBlkData data;

    MEMENTO_LOCK();
    data.addr  = a;
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&memento.used, Memento_containsAddr, &data);
    if (data.blk != NULL) {
        fprintf(stderr, "Will stop when address "FMTP" (in %sallocated block ",
                data.addr,
                (data.flags == 1 ? "" : (data.flags == 2 ?
                                         "preguard of " : "postguard of ")));
        showBlock(data.blk, ' ');
        fprintf(stderr, ") is freed\n");
        VALGRIND_MAKE_MEM_DEFINED(data.blk, sizeof(Memento_BlkHeader));
        data.blk->flags |= Memento_Flag_BreakOnFree;
        VALGRIND_MAKE_MEM_NOACCESS(data.blk, sizeof(Memento_BlkHeader));
        MEMENTO_UNLOCK();
        return;
    }
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&memento.free, Memento_containsAddr, &data);
    if (data.blk != NULL) {
        fprintf(stderr, "Can't stop on free; address "FMTP" is in %sfreed block ",
                data.addr,
                (data.flags == 1 ? "" : (data.flags == 2 ?
                                         "preguard of " : "postguard of ")));
        showBlock(data.blk, ' ');
        fprintf(stderr, "\n");
        MEMENTO_UNLOCK();
        return;
    }
    fprintf(stderr, "Can't stop on free; address "FMTP" is not in a known block.\n", a);
    MEMENTO_UNLOCK();
}

void Memento_breakOnRealloc(void *a)
{
    findBlkData data;

    MEMENTO_LOCK();
    data.addr  = a;
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&memento.used, Memento_containsAddr, &data);
    if (data.blk != NULL) {
        fprintf(stderr, "Will stop when address "FMTP" (in %sallocated block ",
                data.addr,
                (data.flags == 1 ? "" : (data.flags == 2 ?
                                         "preguard of " : "postguard of ")));
        showBlock(data.blk, ' ');
        fprintf(stderr, ") is freed (or realloced)\n");
        VALGRIND_MAKE_MEM_DEFINED(data.blk, sizeof(Memento_BlkHeader));
        data.blk->flags |= Memento_Flag_BreakOnFree | Memento_Flag_BreakOnRealloc;
        VALGRIND_MAKE_MEM_NOACCESS(data.blk, sizeof(Memento_BlkHeader));
        MEMENTO_UNLOCK();
        return;
    }
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&memento.free, Memento_containsAddr, &data);
    if (data.blk != NULL) {
        fprintf(stderr, "Can't stop on free/realloc; address "FMTP" is in %sfreed block ",
                data.addr,
                (data.flags == 1 ? "" : (data.flags == 2 ?
                                         "preguard of " : "postguard of ")));
        showBlock(data.blk, ' ');
        fprintf(stderr, "\n");
        MEMENTO_UNLOCK();
        return;
    }
    fprintf(stderr, "Can't stop on free/realloc; address "FMTP" is not in a known block.\n", a);
    MEMENTO_UNLOCK();
}

int Memento_failAt(int i)
{
    memento.failAt = i;
    if ((memento.sequence > memento.failAt) &&
        (memento.failing != 0))
        Memento_startFailing();
    return i;
}

size_t Memento_setMax(size_t max)
{
    memento.maxMemory = max;
    return max;
}

void Memento_startLeaking(void)
{
    memento.leaking++;
}

void Memento_stopLeaking(void)
{
    memento.leaking--;
}

int Memento_squeezing(void)
{
    return memento.squeezing;
}

#endif

#ifdef __cplusplus

void *operator new(size_t size)
{
    void *ret;

    if (!memento.inited)
        Memento_init();

    if (size == 0)
        size = 1;
    MEMENTO_LOCK();
    ret = do_malloc(size, Memento_EventType_new);
    MEMENTO_UNLOCK();
    return ret;
}

void  operator delete(void *pointer)
{
    if (!pointer)
        return;

    MEMENTO_LOCK();
    do_free(pointer, Memento_EventType_delete);
    MEMENTO_UNLOCK();
}

#ifndef MEMENTO_CPP_NO_ARRAY_CONSTRUCTORS
void *operator new[](size_t size)
{
    void *ret;
    if (!memento.inited)
        Memento_init();

    if (size == 0)
        size = 1;
    MEMENTO_LOCK();
    ret = do_malloc(size, Memento_EventType_newArray);
    MEMENTO_UNLOCK();
    return ret;
}

void  operator delete[](void *pointer)
{
    MEMENTO_LOCK();
    do_free(pointer, Memento_EventType_deleteArray);
    MEMENTO_UNLOCK();
}
#endif
#endif

#else

void (Memento_breakpoint)(void)
{
}

int (Memento_checkBlock)(void *b)
{
    return 0;
}

int (Memento_checkAllMemory)(void)
{
    return 0;
}

int (Memento_check)(void)
{
    return 0;
}

int (Memento_setParanoia)(int i)
{
    return 0;
}

int (Memento_paranoidAt)(int i)
{
    return 0;
}

int (Memento_breakAt)(int i)
{
    return 0;
}

int  (Memento_getBlockNum)(void *i)
{
    return 0;
}

int (Memento_find)(void *a)
{
    return 0;
}

int (Memento_failAt)(int i)
{
    return 0;
}

void (Memento_breakOnFree)(void *a)
{
}

void (Memento_breakOnRealloc)(void *a)
{
}

void *(Memento_takeRef)(void *a)
{
    return a;
}

void *(Memento_dropRef)(void *a)
{
    return a;
}

void *(Memento_adjustRef)(void *a, int adjust)
{
    return a;
}

void *(Memento_reference)(void *a)
{
    return a;
}

#undef Memento_malloc
#undef Memento_free
#undef Memento_realloc
#undef Memento_calloc
#undef Memento_strdup

void *Memento_malloc(size_t size)
{
    return MEMENTO_UNDERLYING_MALLOC(size);
}

void Memento_free(void *b)
{
    MEMENTO_UNDERLYING_FREE(b);
}

void *Memento_realloc(void *b, size_t s)
{
    return MEMENTO_UNDERLYING_REALLOC(b, s);
}

void *Memento_calloc(size_t n, size_t s)
{
    return MEMENTO_UNDERLYING_CALLOC(n, s);
}

char *Memento_strdup(const char *s)
{
    size_t len = strlen(s)+1;
    char *ret = MEMENTO_UNDERLYING_MALLOC(len);
    if (ret != NULL)
        memcpy(ret, s, len);
    return ret;
}

int Memento_asprintf(char **ret, const char *format, ...)
{
    va_list va;
    int n;
    int n2;

    va_start(va, format);
    n = vsnprintf(NULL, 0, format, va);
    va_end(va);
    if (n < 0)
        return n;

    *ret = MEMENTO_UNDERLYING_MALLOC(n+1);
    if (*ret == NULL)
        return -1;

    va_start(va, format);
    n2 = vsnprintf(*ret, n + 1, format, va);
    va_end(va);

    return n2;
}

int Memento_vasprintf(char **ret, const char *format, va_list ap)
{
    int n;
    va_list ap2;
    va_copy(ap2, ap);

    n = vsnprintf(NULL, 0, format, ap);
    if (n < 0) {
        va_end(ap2);
        return n;
    }

    *ret = MEMENTO_UNDERLYING_MALLOC(n+1);
    if (*ret == NULL) {
        va_end(ap2);
        return -1;
    }

    n = vsnprintf(*ret, n + 1, format, ap2);
    va_end(ap2);

    return n;
}

void (Memento_listBlocks)(void)
{
}

void (Memento_listNewBlocks)(void)
{
}

size_t (Memento_setMax)(size_t max)
{
    return 0;
}

void (Memento_stats)(void)
{
}

void *(Memento_label)(void *ptr, const char *label)
{
    return ptr;
}

void (Memento_info)(void *addr)
{
}

void (Memento_listBlockInfo)(void)
{
}

void (Memento_startLeaking)(void)
{
}

void (Memento_stopLeaking)(void)
{
}

int (Memento_squeezing)(void)
{
    return 0;
}

#endif

const odt_template_item_t odt_template_items[] =
{
    {
        "content.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                ""
                "<office:document-content xmlns:css3t=\"http://www.w3.org/TR/css3-text/\" xmlns:grddl=\"http://www.w3.org/2003/g/data-view#\" xmlns:xhtml=\"http://www.w3.org/1999/xhtml\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xforms=\"http://www.w3.org/2002/xforms\" xmlns:dom=\"http://www.w3.org/2001/xml-events\" xmlns:script=\"urn:oasis:names:tc:opendocument:xmlns:script:1.0\" xmlns:form=\"urn:oasis:names:tc:opendocument:xmlns:form:1.0\" xmlns:math=\"http://www.w3.org/1998/Math/MathML\" xmlns:number=\"urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0\" xmlns:field=\"urn:openoffice:names:experimental:ooo-ms-interop:xmlns:field:1.0\" xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\" xmlns:loext=\"urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0\" xmlns:officeooo=\"http://openoffice.org/2009/office\" xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" xmlns:chart=\"urn:oasis:names:tc:opendocument:xmlns:chart:1.0\" xmlns:tableooo=\"http://openoffice.org/2009/table\" xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\" xmlns:rpt=\"http://openoffice.org/2005/report\" xmlns:dr3d=\"urn:oasis:names:tc:opendocument:xmlns:dr3d:1.0\" xmlns:of=\"urn:oasis:names:tc:opendocument:xmlns:of:1.2\" xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:calcext=\"urn:org:documentfoundation:names:experimental:calc:xmlns:calcext:1.0\" xmlns:oooc=\"http://openoffice.org/2004/calc\" xmlns:drawooo=\"http://openoffice.org/2010/draw\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns:ooo=\"http://openoffice.org/2004/office\" xmlns:ooow=\"http://openoffice.org/2004/writer\" xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" xmlns:formx=\"urn:openoffice:names:experimental:ooxml-odf-interop:xmlns:form:1.0\" xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\" xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" office:version=\"1.3\">"
                "<office:scripts/>"
                "<office:font-face-decls>"
                "<style:font-face style:name=\"Liberation Serif\" svg:font-family=\"&apos;Liberation Serif&apos;\" style:font-family-generic=\"roman\" style:font-pitch=\"variable\"/>"
                "<style:font-face style:name=\"Liberation Sans\" svg:font-family=\"&apos;Liberation Sans&apos;\" style:font-family-generic=\"swiss\" style:font-pitch=\"variable\"/>"
                "<style:font-face style:name=\"Unifont\" svg:font-family=\"Unifont\" style:font-family-generic=\"system\" style:font-pitch=\"variable\"/></office:font-face-decls>"
                "<office:automatic-styles/>"
                "<office:body>"
                "<office:text>"
                "<text:sequence-decls>"
                "<text:sequence-decl text:display-outline-level=\"0\" text:name=\"Illustration\"/>"
                "<text:sequence-decl text:display-outline-level=\"0\" text:name=\"Table\"/>"
                "<text:sequence-decl text:display-outline-level=\"0\" text:name=\"Text\"/>"
                "<text:sequence-decl text:display-outline-level=\"0\" text:name=\"Drawing\"/>"
                "<text:sequence-decl text:display-outline-level=\"0\" text:name=\"Figure\"/></text:sequence-decls>"
                "<text:p text:style-name=\"Standard\"/></office:text></office:body></office:document-content>"
    },

    {
        "manifest.rdf",
        "\x3c\x3f\x78\x6d\x6c\x20\x76\x65\x72\x73\x69\x6f\x6e\x3d\x22"
        "\x31\x2e\x30\x22\x20\x65\x6e\x63\x6f\x64\x69\x6e\x67\x3d\x22\x75"
        "\x74\x66\x2d\x38\x22\x3f\x3e\x0a\x3c\x72\x64\x66\x3a\x52\x44\x46"
        "\x20\x78\x6d\x6c\x6e\x73\x3a\x72\x64\x66\x3d\x22\x68\x74\x74\x70"
        "\x3a\x2f\x2f\x77\x77\x77\x2e\x77\x33\x2e\x6f\x72\x67\x2f\x31\x39"
        "\x39\x39\x2f\x30\x32\x2f\x32\x32\x2d\x72\x64\x66\x2d\x73\x79\x6e"
        "\x74\x61\x78\x2d\x6e\x73\x23\x22\x3e\x0a\x20\x20\x3c\x72\x64\x66"
        "\x3a\x44\x65\x73\x63\x72\x69\x70\x74\x69\x6f\x6e\x20\x72\x64\x66"
        "\x3a\x61\x62\x6f\x75\x74\x3d\x22\x73\x74\x79\x6c\x65\x73\x2e\x78"
        "\x6d\x6c\x22\x3e\x0a\x20\x20\x20\x20\x3c\x72\x64\x66\x3a\x74\x79"
        "\x70\x65\x20\x72\x64\x66\x3a\x72\x65\x73\x6f\x75\x72\x63\x65\x3d"
        "\x22\x68\x74\x74\x70\x3a\x2f\x2f\x64\x6f\x63\x73\x2e\x6f\x61\x73"
        "\x69\x73\x2d\x6f\x70\x65\x6e\x2e\x6f\x72\x67\x2f\x6e\x73\x2f\x6f"
        "\x66\x66\x69\x63\x65\x2f\x31\x2e\x32\x2f\x6d\x65\x74\x61\x2f\x6f"
        "\x64\x66\x23\x53\x74\x79\x6c\x65\x73\x46\x69\x6c\x65\x22\x2f\x3e"
        "\x0a\x20\x20\x3c\x2f\x72\x64\x66\x3a\x44\x65\x73\x63\x72\x69\x70"
        "\x74\x69\x6f\x6e\x3e\x0a\x20\x20\x3c\x72\x64\x66\x3a\x44\x65\x73"
        "\x63\x72\x69\x70\x74\x69\x6f\x6e\x20\x72\x64\x66\x3a\x61\x62\x6f"
        "\x75\x74\x3d\x22\x22\x3e\x0a\x20\x20\x20\x20\x3c\x6e\x73\x30\x3a"
        "\x68\x61\x73\x50\x61\x72\x74\x20\x78\x6d\x6c\x6e\x73\x3a\x6e\x73"
        "\x30\x3d\x22\x68\x74\x74\x70\x3a\x2f\x2f\x64\x6f\x63\x73\x2e\x6f"
        "\x61\x73\x69\x73\x2d\x6f\x70\x65\x6e\x2e\x6f\x72\x67\x2f\x6e\x73"
        "\x2f\x6f\x66\x66\x69\x63\x65\x2f\x31\x2e\x32\x2f\x6d\x65\x74\x61"
        "\x2f\x70\x6b\x67\x23\x22\x20\x72\x64\x66\x3a\x72\x65\x73\x6f\x75"
        "\x72\x63\x65\x3d\x22\x73\x74\x79\x6c\x65\x73\x2e\x78\x6d\x6c\x22"
        "\x2f\x3e\x0a\x20\x20\x3c\x2f\x72\x64\x66\x3a\x44\x65\x73\x63\x72"
        "\x69\x70\x74\x69\x6f\x6e\x3e\x0a\x20\x20\x3c\x72\x64\x66\x3a\x44"
        "\x65\x73\x63\x72\x69\x70\x74\x69\x6f\x6e\x20\x72\x64\x66\x3a\x61"
        "\x62\x6f\x75\x74\x3d\x22\x63\x6f\x6e\x74\x65\x6e\x74\x2e\x78\x6d"
        "\x6c\x22\x3e\x0a\x20\x20\x20\x20\x3c\x72\x64\x66\x3a\x74\x79\x70"
        "\x65\x20\x72\x64\x66\x3a\x72\x65\x73\x6f\x75\x72\x63\x65\x3d\x22"
        "\x68\x74\x74\x70\x3a\x2f\x2f\x64\x6f\x63\x73\x2e\x6f\x61\x73\x69"
        "\x73\x2d\x6f\x70\x65\x6e\x2e\x6f\x72\x67\x2f\x6e\x73\x2f\x6f\x66"
        "\x66\x69\x63\x65\x2f\x31\x2e\x32\x2f\x6d\x65\x74\x61\x2f\x6f\x64"
        "\x66\x23\x43\x6f\x6e\x74\x65\x6e\x74\x46\x69\x6c\x65\x22\x2f\x3e"
        "\x0a\x20\x20\x3c\x2f\x72\x64\x66\x3a\x44\x65\x73\x63\x72\x69\x70"
        "\x74\x69\x6f\x6e\x3e\x0a\x20\x20\x3c\x72\x64\x66\x3a\x44\x65\x73"
        "\x63\x72\x69\x70\x74\x69\x6f\x6e\x20\x72\x64\x66\x3a\x61\x62\x6f"
        "\x75\x74\x3d\x22\x22\x3e\x0a\x20\x20\x20\x20\x3c\x6e\x73\x30\x3a"
        "\x68\x61\x73\x50\x61\x72\x74\x20\x78\x6d\x6c\x6e\x73\x3a\x6e\x73"
        "\x30\x3d\x22\x68\x74\x74\x70\x3a\x2f\x2f\x64\x6f\x63\x73\x2e\x6f"
        "\x61\x73\x69\x73\x2d\x6f\x70\x65\x6e\x2e\x6f\x72\x67\x2f\x6e\x73"
        "\x2f\x6f\x66\x66\x69\x63\x65\x2f\x31\x2e\x32\x2f\x6d\x65\x74\x61"
        "\x2f\x70\x6b\x67\x23\x22\x20\x72\x64\x66\x3a\x72\x65\x73\x6f\x75"
        "\x72\x63\x65\x3d\x22\x63\x6f\x6e\x74\x65\x6e\x74\x2e\x78\x6d\x6c"
        "\x22\x2f\x3e\x0a\x20\x20\x3c\x2f\x72\x64\x66\x3a\x44\x65\x73\x63"
        "\x72\x69\x70\x74\x69\x6f\x6e\x3e\x0a\x20\x20\x3c\x72\x64\x66\x3a"
        "\x44\x65\x73\x63\x72\x69\x70\x74\x69\x6f\x6e\x20\x72\x64\x66\x3a"
        "\x61\x62\x6f\x75\x74\x3d\x22\x22\x3e\x0a\x20\x20\x20\x20\x3c\x72"
        "\x64\x66\x3a\x74\x79\x70\x65\x20\x72\x64\x66\x3a\x72\x65\x73\x6f"
        "\x75\x72\x63\x65\x3d\x22\x68\x74\x74\x70\x3a\x2f\x2f\x64\x6f\x63"
        "\x73\x2e\x6f\x61\x73\x69\x73\x2d\x6f\x70\x65\x6e\x2e\x6f\x72\x67"
        "\x2f\x6e\x73\x2f\x6f\x66\x66\x69\x63\x65\x2f\x31\x2e\x32\x2f\x6d"
        "\x65\x74\x61\x2f\x70\x6b\x67\x23\x44\x6f\x63\x75\x6d\x65\x6e\x74"
        "\x22\x2f\x3e\x0a\x20\x20\x3c\x2f\x72\x64\x66\x3a\x44\x65\x73\x63"
        "\x72\x69\x70\x74\x69\x6f\x6e\x3e\x0a\x3c\x2f\x72\x64\x66\x3a\x52"
        "\x44\x46\x3e\x0a"
    },

    {
        "meta.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                ""
                "<office:document-meta xmlns:grddl=\"http://www.w3.org/2003/g/data-view#\" xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\" xmlns:ooo=\"http://openoffice.org/2004/office\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" office:version=\"1.3\">"
                "<office:meta>"
                "<meta:creation-date>2021-04-05T17:06:57.937137058</meta:creation-date>"
                "<meta:generator>LibreOffice/7.0.1.2$OpenBSD_X86_64 LibreOffice_project/00$Build-2</meta:generator>"
                "<dc:date>2021-04-06T17:14:51.409959656</dc:date>"
                "<meta:editing-duration>PT20S</meta:editing-duration>"
                "<meta:editing-cycles>1</meta:editing-cycles>"
                "<meta:document-statistic meta:table-count=\"0\" meta:image-count=\"0\" meta:object-count=\"0\" meta:page-count=\"1\" meta:paragraph-count=\"0\" meta:word-count=\"0\" meta:character-count=\"0\" meta:non-whitespace-character-count=\"0\"/></office:meta></office:document-meta>"
    },

    {
        "mimetype",
        "\x61\x70\x70\x6c\x69\x63\x61\x74\x69\x6f\x6e\x2f\x76\x6e\x64"
        "\x2e\x6f\x61\x73\x69\x73\x2e\x6f\x70\x65\x6e\x64\x6f\x63\x75\x6d"
        "\x65\x6e\x74\x2e\x74\x65\x78\x74"
    },

    {
        "settings.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                ""
                "<office:document-settings xmlns:config=\"urn:oasis:names:tc:opendocument:xmlns:config:1.0\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns:ooo=\"http://openoffice.org/2004/office\" xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" office:version=\"1.3\">"
                "<office:settings>"
                "<config:config-item-set config:name=\"ooo:view-settings\">"
                "<config:config-item config:name=\"ViewAreaTop\" config:type=\"long\">0</config:config-item>"
                "<config:config-item config:name=\"ViewAreaLeft\" config:type=\"long\">0</config:config-item>"
                "<config:config-item config:name=\"ViewAreaWidth\" config:type=\"long\">26275</config:config-item>"
                "<config:config-item config:name=\"ViewAreaHeight\" config:type=\"long\">19502</config:config-item>"
                "<config:config-item config:name=\"ShowRedlineChanges\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"InBrowseMode\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item-map-indexed config:name=\"Views\">"
                "<config:config-item-map-entry>"
                "<config:config-item config:name=\"ViewId\" config:type=\"string\">view2</config:config-item>"
                "<config:config-item config:name=\"ViewLeft\" config:type=\"long\">4343</config:config-item>"
                "<config:config-item config:name=\"ViewTop\" config:type=\"long\">2501</config:config-item>"
                "<config:config-item config:name=\"VisibleLeft\" config:type=\"long\">0</config:config-item>"
                "<config:config-item config:name=\"VisibleTop\" config:type=\"long\">0</config:config-item>"
                "<config:config-item config:name=\"VisibleRight\" config:type=\"long\">26273</config:config-item>"
                "<config:config-item config:name=\"VisibleBottom\" config:type=\"long\">19500</config:config-item>"
                "<config:config-item config:name=\"ZoomType\" config:type=\"short\">0</config:config-item>"
                "<config:config-item config:name=\"ViewLayoutColumns\" config:type=\"short\">1</config:config-item>"
                "<config:config-item config:name=\"ViewLayoutBookMode\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"ZoomFactor\" config:type=\"short\">100</config:config-item>"
                "<config:config-item config:name=\"IsSelectedFrame\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"AnchoredTextOverflowLegacy\" config:type=\"boolean\">false</config:config-item></config:config-item-map-entry></config:config-item-map-indexed></config:config-item-set>"
                "<config:config-item-set config:name=\"ooo:configuration-settings\">"
                "<config:config-item config:name=\"PrintBlackFonts\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintReversed\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"SaveThumbnail\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"EmbedFonts\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintControls\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"OutlineLevelYieldsNumbering\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintPageBackground\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"PrintAnnotationMode\" config:type=\"short\">0</config:config-item>"
                "<config:config-item config:name=\"PrintGraphics\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"EmbeddedDatabaseName\" config:type=\"string\"/>"
                "<config:config-item config:name=\"ProtectForm\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintLeftPages\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"PrintProspect\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintHiddenText\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintRightPages\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"PrintFaxName\" config:type=\"string\"/>"
                "<config:config-item config:name=\"PrintPaperFromSetup\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"TabsRelativeToIndent\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"RedlineProtectionKey\" config:type=\"base64Binary\"/>"
                "<config:config-item config:name=\"PrintTextPlaceholder\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"MathBaselineAlignment\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"ProtectBookmarks\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"IgnoreTabsAndBlanksForLineCalculation\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"ContinuousEndnotes\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"FieldAutoUpdate\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"EmptyDbFieldHidesPara\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"ApplyParagraphMarkFormatToNumbering\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintEmptyPages\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"TabOverMargin\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"EmbedAsianScriptFonts\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"EmbedLatinScriptFonts\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"DisableOffPagePositioning\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"EmbedOnlyUsedFonts\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"MsWordCompMinLineHeightByFly\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"SurroundTextWrapSmall\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"BackgroundParaOverDrawings\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"ClippedPictures\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"FloattableNomargins\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"UnbreakableNumberings\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"EmbedSystemFonts\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"TabOverflow\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"PrintTables\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"PrintDrawings\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"ConsiderTextWrapOnObjPos\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintSingleJobs\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"SmallCapsPercentage66\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"CollapseEmptyCellPara\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"HeaderSpacingBelowLastPara\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"RsidRoot\" config:type=\"int\">2052946</config:config-item>"
                "<config:config-item config:name=\"PrinterSetup\" config:type=\"base64Binary\"/>"
                "<config:config-item config:name=\"CurrentDatabaseCommand\" config:type=\"string\"/>"
                "<config:config-item config:name=\"AlignTabStopPosition\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"ClipAsCharacterAnchoredWriterFlyFrames\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"DoNotCaptureDrawObjsOnPage\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"SaveGlobalDocumentLinks\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"CurrentDatabaseCommandType\" config:type=\"int\">0</config:config-item>"
                "<config:config-item config:name=\"LoadReadonly\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"DoNotResetParaAttrsForNumFont\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"StylesNoDefault\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"LinkUpdateMode\" config:type=\"short\">1</config:config-item>"
                "<config:config-item config:name=\"DoNotJustifyLinesWithManualBreak\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PropLineSpacingShrinksFirstLine\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"TabAtLeftIndentForParagraphsInList\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"ProtectFields\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"UnxForceZeroExtLeading\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"CurrentDatabaseDataSource\" config:type=\"string\"/>"
                "<config:config-item config:name=\"UseFormerTextWrapping\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"UseFormerLineSpacing\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"AllowPrintJobCancel\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"SubtractFlysAnchoredAtFlys\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"AddParaSpacingToTableCells\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"AddExternalLeading\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"Rsid\" config:type=\"int\">2178852</config:config-item>"
                "<config:config-item config:name=\"AddVerticalFrameOffsets\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"TreatSingleColumnBreakAsPageBreak\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"AddFrameOffsets\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"IsLabelDocument\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"MsWordCompTrailingBlanks\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrinterPaperFromSetup\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"IgnoreFirstLineIndentInNumbering\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrinterName\" config:type=\"string\"/>"
                "<config:config-item config:name=\"IsKernAsianPunctuation\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrinterIndependentLayout\" config:type=\"string\">high-resolution</config:config-item>"
                "<config:config-item config:name=\"TableRowKeep\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"UpdateFromTemplate\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"EmbedComplexScriptFonts\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"UseOldPrinterMetrics\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"InvertBorderSpacing\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"PrintProspectRTL\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"ApplyUserData\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"AddParaTableSpacingAtStart\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"SaveVersionOnClose\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"CharacterCompressionType\" config:type=\"short\">0</config:config-item>"
                "<config:config-item config:name=\"UseOldNumbering\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"UseFormerObjectPositioning\" config:type=\"boolean\">false</config:config-item>"
                "<config:config-item config:name=\"ChartAutoUpdate\" config:type=\"boolean\">true</config:config-item>"
                "<config:config-item config:name=\"AddParaTableSpacing\" config:type=\"boolean\">true</config:config-item></config:config-item-set></office:settings></office:document-settings>"
    },

    {
        "styles.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                ""
                "<office:document-styles xmlns:css3t=\"http://www.w3.org/TR/css3-text/\" xmlns:grddl=\"http://www.w3.org/2003/g/data-view#\" xmlns:xhtml=\"http://www.w3.org/1999/xhtml\" xmlns:dom=\"http://www.w3.org/2001/xml-events\" xmlns:script=\"urn:oasis:names:tc:opendocument:xmlns:script:1.0\" xmlns:form=\"urn:oasis:names:tc:opendocument:xmlns:form:1.0\" xmlns:math=\"http://www.w3.org/1998/Math/MathML\" xmlns:number=\"urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0\" xmlns:field=\"urn:openoffice:names:experimental:ooo-ms-interop:xmlns:field:1.0\" xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\" xmlns:loext=\"urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0\" xmlns:officeooo=\"http://openoffice.org/2009/office\" xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" xmlns:chart=\"urn:oasis:names:tc:opendocument:xmlns:chart:1.0\" xmlns:tableooo=\"http://openoffice.org/2009/table\" xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\" xmlns:rpt=\"http://openoffice.org/2005/report\" xmlns:dr3d=\"urn:oasis:names:tc:opendocument:xmlns:dr3d:1.0\" xmlns:of=\"urn:oasis:names:tc:opendocument:xmlns:of:1.2\" xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:calcext=\"urn:org:documentfoundation:names:experimental:calc:xmlns:calcext:1.0\" xmlns:oooc=\"http://openoffice.org/2004/calc\" xmlns:drawooo=\"http://openoffice.org/2010/draw\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns:ooo=\"http://openoffice.org/2004/office\" xmlns:ooow=\"http://openoffice.org/2004/writer\" xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\" xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" office:version=\"1.3\">"
                "<office:font-face-decls>"
                "<style:font-face style:name=\"Liberation Serif\" svg:font-family=\"&apos;Liberation Serif&apos;\" style:font-family-generic=\"roman\" style:font-pitch=\"variable\"/>"
                "<style:font-face style:name=\"Liberation Sans\" svg:font-family=\"&apos;Liberation Sans&apos;\" style:font-family-generic=\"swiss\" style:font-pitch=\"variable\"/>"
                "<style:font-face style:name=\"Unifont\" svg:font-family=\"Unifont\" style:font-family-generic=\"system\" style:font-pitch=\"variable\"/></office:font-face-decls>"
                "<office:styles>"
                "<style:default-style style:family=\"graphic\">"
                "<style:graphic-properties svg:stroke-color=\"#3465a4\" draw:fill-color=\"#729fcf\" fo:wrap-option=\"no-wrap\" draw:shadow-offset-x=\"0.1181in\" draw:shadow-offset-y=\"0.1181in\" draw:start-line-spacing-horizontal=\"0.1114in\" draw:start-line-spacing-vertical=\"0.1114in\" draw:end-line-spacing-horizontal=\"0.1114in\" draw:end-line-spacing-vertical=\"0.1114in\" style:flow-with-text=\"false\"/>"
                "<style:paragraph-properties style:text-autospace=\"ideograph-alpha\" style:line-break=\"strict\" style:font-independent-line-spacing=\"false\">"
                "<style:tab-stops/></style:paragraph-properties>"
                "<style:text-properties style:use-window-font-color=\"true\" loext:opacity=\"0%\" style:font-name=\"Liberation Serif\" fo:font-size=\"12pt\" fo:language=\"en\" fo:country=\"US\" style:letter-kerning=\"true\" style:font-name-asian=\"Unifont\" style:font-size-asian=\"10.5pt\" style:language-asian=\"zh\" style:country-asian=\"CN\" style:font-name-complex=\"Unifont\" style:font-size-complex=\"12pt\" style:language-complex=\"hi\" style:country-complex=\"IN\"/></style:default-style>"
                "<style:default-style style:family=\"paragraph\">"
                "<style:paragraph-properties fo:orphans=\"2\" fo:widows=\"2\" fo:hyphenation-ladder-count=\"no-limit\" style:text-autospace=\"ideograph-alpha\" style:punctuation-wrap=\"hanging\" style:line-break=\"strict\" style:tab-stop-distance=\"0.4925in\" style:writing-mode=\"page\"/>"
                "<style:text-properties style:use-window-font-color=\"true\" loext:opacity=\"0%\" style:font-name=\"Liberation Serif\" fo:font-size=\"12pt\" fo:language=\"en\" fo:country=\"US\" style:letter-kerning=\"true\" style:font-name-asian=\"Unifont\" style:font-size-asian=\"10.5pt\" style:language-asian=\"zh\" style:country-asian=\"CN\" style:font-name-complex=\"Unifont\" style:font-size-complex=\"12pt\" style:language-complex=\"hi\" style:country-complex=\"IN\" fo:hyphenate=\"false\" fo:hyphenation-remain-char-count=\"2\" fo:hyphenation-push-char-count=\"2\" loext:hyphenation-no-caps=\"false\"/></style:default-style>"
                "<style:default-style style:family=\"table\">"
                "<style:table-properties table:border-model=\"collapsing\"/></style:default-style>"
                "<style:default-style style:family=\"table-row\">"
                "<style:table-row-properties fo:keep-together=\"auto\"/></style:default-style>"
                "<style:style style:name=\"Standard\" style:family=\"paragraph\" style:class=\"text\">"
                "<style:paragraph-properties fo:margin-top=\"0in\" fo:margin-bottom=\"0.0799in\" style:contextual-spacing=\"false\"/></style:style>"
                "<style:style style:name=\"Heading\" style:family=\"paragraph\" style:parent-style-name=\"Standard\" style:next-style-name=\"Text_20_body\" style:class=\"text\">"
                "<style:paragraph-properties fo:margin-top=\"0.1665in\" fo:margin-bottom=\"0.0835in\" style:contextual-spacing=\"false\" fo:keep-with-next=\"always\"/>"
                "<style:text-properties style:font-name=\"Liberation Sans\" fo:font-family=\"&apos;Liberation Sans&apos;\" style:font-family-generic=\"swiss\" style:font-pitch=\"variable\" fo:font-size=\"14pt\" style:font-name-asian=\"Unifont\" style:font-family-asian=\"Unifont\" style:font-family-generic-asian=\"system\" style:font-pitch-asian=\"variable\" style:font-size-asian=\"14pt\" style:font-name-complex=\"Unifont\" style:font-family-complex=\"Unifont\" style:font-family-generic-complex=\"system\" style:font-pitch-complex=\"variable\" style:font-size-complex=\"14pt\"/></style:style>"
                "<style:style style:name=\"Text_20_body\" style:display-name=\"Text body\" style:family=\"paragraph\" style:parent-style-name=\"Standard\" style:class=\"text\">"
                "<style:paragraph-properties fo:margin-top=\"0in\" fo:margin-bottom=\"0.0972in\" style:contextual-spacing=\"false\" fo:line-height=\"115%\"/></style:style>"
                "<style:style style:name=\"List\" style:family=\"paragraph\" style:parent-style-name=\"Text_20_body\" style:class=\"list\">"
                "<style:text-properties style:font-size-asian=\"12pt\"/></style:style>"
                "<style:style style:name=\"Caption\" style:family=\"paragraph\" style:parent-style-name=\"Standard\" style:class=\"extra\">"
                "<style:paragraph-properties fo:margin-top=\"0.0835in\" fo:margin-bottom=\"0.0835in\" style:contextual-spacing=\"false\" text:number-lines=\"false\" text:line-number=\"0\"/>"
                "<style:text-properties fo:font-size=\"12pt\" fo:font-style=\"italic\" style:font-size-asian=\"12pt\" style:font-style-asian=\"italic\" style:font-size-complex=\"12pt\" style:font-style-complex=\"italic\"/></style:style>"
                "<style:style style:name=\"Index\" style:family=\"paragraph\" style:parent-style-name=\"Standard\" style:class=\"index\">"
                "<style:paragraph-properties text:number-lines=\"false\" text:line-number=\"0\"/>"
                "<style:text-properties style:font-size-asian=\"12pt\"/></style:style>"
                "<text:outline-style style:name=\"Outline\">"
                "<text:outline-level-style text:level=\"1\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"2\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"3\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"4\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"5\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"6\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"7\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"8\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"9\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style>"
                "<text:outline-level-style text:level=\"10\" style:num-format=\"\">"
                "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
                "<style:list-level-label-alignment text:label-followed-by=\"listtab\"/></style:list-level-properties></text:outline-level-style></text:outline-style>"
                "<text:notes-configuration text:note-class=\"footnote\" style:num-format=\"1\" text:start-value=\"0\" text:footnotes-position=\"page\" text:start-numbering-at=\"document\"/>"
                "<text:notes-configuration text:note-class=\"endnote\" style:num-format=\"i\" text:start-value=\"0\"/>"
                "<text:linenumbering-configuration text:number-lines=\"false\" text:offset=\"0.1965in\" style:num-format=\"1\" text:number-position=\"left\" text:increment=\"5\"/></office:styles>"
                "<office:automatic-styles>"
                "<style:page-layout style:name=\"Mpm1\">"
                "<style:page-layout-properties fo:page-width=\"8.5in\" fo:page-height=\"11in\" style:num-format=\"1\" style:print-orientation=\"portrait\" fo:margin-top=\"0.7874in\" fo:margin-bottom=\"0.7874in\" fo:margin-left=\"0.7874in\" fo:margin-right=\"0.7874in\" style:writing-mode=\"lr-tb\" style:layout-grid-color=\"#c0c0c0\" style:layout-grid-lines=\"20\" style:layout-grid-base-height=\"0.278in\" style:layout-grid-ruby-height=\"0.139in\" style:layout-grid-mode=\"none\" style:layout-grid-ruby-below=\"false\" style:layout-grid-print=\"false\" style:layout-grid-display=\"false\" style:footnote-max-height=\"0in\">"
                "<style:footnote-sep style:width=\"0.0071in\" style:distance-before-sep=\"0.0398in\" style:distance-after-sep=\"0.0398in\" style:line-style=\"solid\" style:adjustment=\"left\" style:rel-width=\"25%\" style:color=\"#000000\"/></style:page-layout-properties>"
                "<style:header-style/>"
                "<style:footer-style/></style:page-layout></office:automatic-styles>"
                "<office:master-styles>"
                "<style:master-page style:name=\"Standard\" style:page-layout-name=\"Mpm1\"/></office:master-styles></office:document-styles>"
    },

    {
        "META-INF/manifest.xml",
        ""
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                ""
                "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" manifest:version=\"1.3\" xmlns:loext=\"urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0\">\n"
                " "
                "<manifest:file-entry manifest:full-path=\"/\" manifest:version=\"1.3\" manifest:media-type=\"application/vnd.oasis.opendocument.text\"/>\n"
                " "
                "<manifest:file-entry manifest:full-path=\"meta.xml\" manifest:media-type=\"text/xml\"/>\n"
                " "
                "<manifest:file-entry manifest:full-path=\"settings.xml\" manifest:media-type=\"text/xml\"/>\n"
                " "
                "<manifest:file-entry manifest:full-path=\"Configurations2/\" manifest:media-type=\"application/vnd.sun.xml.ui.configuration\"/>\n"
                " "
                "<manifest:file-entry manifest:full-path=\"manifest.rdf\" manifest:media-type=\"application/rdf+xml\"/>\n"
                " "
                "<manifest:file-entry manifest:full-path=\"styles.xml\" manifest:media-type=\"text/xml\"/>\n"
                " "
                "<manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>\n"
                " "
                "<manifest:file-entry manifest:full-path=\"Thumbnails/thumbnail.png\" manifest:media-type=\"image/png\"/>\n"
                "</manifest:manifest>"
    },

    {
        "Thumbnails/thumbnail.png",
        "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44"
        "\x52\x00\x00\x00\xc6\x00\x00\x01\x00\x08\x03\x00\x00\x00\xdf\x83"
        "\xf9\x72\x00\x00\x00\x09\x50\x4c\x54\x45\xff\xff\xff\x00\x00\x00"
        "\xff\xff\xff\x7e\xef\x8f\x4f\x00\x00\x00\x48\x49\x44\x41\x54\x78"
        "\xda\xed\xc1\x31\x01\x00\x00\x00\xc2\xa0\xf5\x4f\x6d\x0c\x1f\xa0"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x78\x18\xc7\x00\x00\x01\xf9\xd2\xb5\x9a\x00\x00\x00\x00\x49"
        "\x45\x4e\x44\xae\x42\x60\x82"
    },

};

int odt_template_items_num = 8;

#include "extract/extract.h"

#include "memento.h"

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

static int
odt_paragraph_start(
		extract_alloc_t   *alloc,
		extract_astring_t *output)
{
	return extract_astring_cat(alloc, output, "\n\n<text:p>");
}

static int
odt_paragraph_finish(
		extract_alloc_t   *alloc,
		extract_astring_t *output)
{
	return extract_astring_cat(alloc, output, "</text:p>");
}

struct extract_odt_style_t
{
	int     id;
	font_t  font;
};

struct extract_odt_styles_t
{

	extract_odt_style_t *styles;
	int                  styles_num;
};

static int
odt_style_compare(
		extract_odt_style_t *a,
		extract_odt_style_t *b)
{
	int d;
	double dd;

	if ((d = strcmp(a->font.name, b->font.name)))   return d;
	if ((dd = a->font.size - b->font.size) != 0.0)  return (dd > 0.0) ? 1 : -1;
	if ((d = a->font.bold - b->font.bold))          return d;
	if ((d = a->font.italic - b->font.italic))      return d;

	return 0;
}

static int
odt_style_append_definition(
		extract_alloc_t     *alloc,
		extract_odt_style_t *style,
		extract_astring_t   *text)
{
	const char* font_name = style->font.name;

	if (0 && strstr(font_name, "Helvetica"))
	{
		font_name = "Liberation Sans";
	}
	outf("style->font_name=%s font_name=%s", style->font.name, font_name);
	if (extract_astring_catf(alloc, text, "<style:style style:name=\"T%i\" style:family=\"text\">", style->id)) return -1;
	if (extract_astring_catf(alloc, text, "<style:text-properties style:font-name=\"%s\"", font_name)) return -1;
	if (extract_astring_catf(alloc, text, " fo:font-size=\"%.2fpt\"", style->font.size)) return -1;
	if (extract_astring_catf(alloc, text, " fo:font-weight=\"%s\"", style->font.bold ? "bold" : "normal")) return -1;
	if (extract_astring_catf(alloc, text, " fo:font-style=\"%s\"", style->font.italic ? "italic" : "normal")) return -1;
	if (extract_astring_cat(alloc, text, " /></style:style>")) return -1;

	return 0;
}

void
extract_odt_styles_free(
		extract_alloc_t      *alloc,
		extract_odt_styles_t *styles)
{
	int i;

	for (i=0; i<styles->styles_num; ++i)
	{
		extract_odt_style_t* style = &styles->styles[i];
		extract_free(alloc, &style->font.name);
	}
	extract_free(alloc, &styles->styles);
}

static int
odt_styles_definitions(
		extract_alloc_t      *alloc,
		extract_odt_styles_t *styles,
		extract_astring_t    *out)
{
	int i;

	if (extract_astring_cat(alloc, out, "<office:automatic-styles>")) return -1;
	for (i=0; i<styles->styles_num; ++i)
	{
		if (odt_style_append_definition(alloc, &styles->styles[i], out)) return -1;
	}
	extract_astring_cat(alloc, out, "<style:style style:name=\"gr1\" style:family=\"graphic\">\n");
	extract_astring_cat(alloc, out, "<style:graphic-properties"
			" draw:stroke=\"none\""
			" svg:stroke-color=\"#000000\""
			" draw:fill=\"none\""
			" draw:fill-color=\"#ffffff\""
			" fo:min-height=\"1.9898in\""
			" style:run-through=\"foreground\""
			" style:wrap=\"run-through\""
			" style:number-wrapped-paragraphs=\"no-limit\""
			" style:vertical-pos=\"from-top\""
			" style:vertical-rel=\"paragraph\""
			" style:horizontal-pos=\"from-left\""
			" style:horizontal-rel=\"paragraph\""
			" />\n"
			);
	extract_astring_cat(alloc, out, "<style:paragraph-properties style:writing-mode=\"lr-tb\"/>\n");
	extract_astring_cat(alloc, out, "</style:style>\n");

	extract_astring_cat(alloc, out, "<style:style style:name=\"fr1\" style:family=\"graphic\" style:parent-style-name=\"Graphics\">\n");
	extract_astring_cat(alloc, out, "<style:graphic-properties"
			" fo:margin-left=\"0in\""
			" fo:margin-right=\"0in\""
			" fo:margin-top=\"0in\""
			" fo:margin-bottom=\"0in\""
			" style:vertical-pos=\"top\""
			" style:vertical-rel=\"baseline\""
			" fo:background-color=\"transparent\""
			" draw:fill=\"none\""
			" draw:fill-color=\"#ffffff\""
			" fo:padding=\"0in\""
			" fo:border=\"none\""
			" style:mirror=\"none\""
			" fo:clip=\"rect(0in, 0in, 0in, 0in)\""
			" draw:luminance=\"0%\""
			" draw:contrast=\"0%\""
			" draw:red=\"0%\""
			" draw:green=\"0%\""
			" draw:blue=\"0%\""
			" draw:gamma=\"100%\""
			" draw:color-inversion=\"false\""
			" draw:image-opacity=\"100%\""
			" draw:color-mode=\"standard\""
			"/>\n");
	extract_astring_cat(alloc, out, "</style:style>\n");

	if (extract_astring_cat(alloc, out, "</office:automatic-styles>")) return -1;

	return 0;
}

static int
odt_styles_add(
	extract_alloc_t      *alloc,
	extract_odt_styles_t *styles,
	font_t               *font,
	extract_odt_style_t **o_style)
{
	extract_odt_style_t style = {0 , *font};
	int i;

	for (i=0; i<styles->styles_num; ++i)
	{
		int d = odt_style_compare(&style, &styles->styles[i]);
		if (d == 0)
		{
			*o_style = &styles->styles[i];
			return 0;
		}
		if (d > 0) break;
	}

	if (extract_realloc(alloc, &styles->styles, sizeof(styles->styles[0]) * (styles->styles_num+1))) return -1;
	memmove(&styles->styles[i+1], &styles->styles[i], sizeof(styles->styles[0]) * (styles->styles_num - i));
	styles->styles_num += 1;
	styles->styles[i].id = styles->styles_num + 10;
	if (extract_strdup(alloc, font->name, &styles->styles[i].font.name)) return -1;
	styles->styles[i].font.size = font->size;
	styles->styles[i].font.bold = font->bold;
	styles->styles[i].font.italic = font->italic;
	*o_style = &styles->styles[i];

	return 0;
}

static int
extract_odt_run_start(
		extract_alloc_t      *alloc,
		extract_astring_t    *content,
		extract_odt_styles_t *styles,
		content_state_t      *content_state)
{
	extract_odt_style_t* style;

	if (odt_styles_add(alloc,
					   styles,
					   &content_state->font,
					   &style)) return -1;
	if (extract_astring_catf(alloc, content, "<text:span text:style-name=\"T%i\">", style->id)) return -1;

	return 0;
}

static int
odt_run_finish(
	extract_alloc_t   *alloc,
	content_state_t   *content_state,
	extract_astring_t *content)
{
	if (content_state)
		content_state->font.name = NULL;
	return extract_astring_cat(alloc, content, "</text:span>");
}

static int
odt_append_empty_paragraph(
		extract_alloc_t      *alloc,
		extract_astring_t    *content,
		extract_odt_styles_t *styles)
{
	int e = -1;
	static char fontname[] = "OpenSans";
	content_state_t content_state = {0};

	if (odt_paragraph_start(alloc, content)) goto end;

	content_state.font.name = fontname;
	content_state.font.size = 10;
	content_state.font.bold = 0;
	content_state.font.italic = 0;
	if (extract_odt_run_start(alloc, content, styles, &content_state)) goto end;

	if (odt_run_finish(alloc, NULL , content)) goto end;
	if (odt_paragraph_finish(alloc, content)) goto end;
	e = 0;

end:
	return e;
}

static int
document_to_odt_content_paragraph(
		extract_alloc_t      *alloc,
		content_state_t      *content_state,
		paragraph_t          *paragraph,
		extract_astring_t    *content,
		extract_odt_styles_t *styles)
{
	int e = -1;
	content_line_iterator  lit;
	line_t                *line;

	if (odt_paragraph_start(alloc, content)) goto end;

	if ((paragraph->line_flags & paragraph_not_fully_justified) == 0)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"both\"/></w:pPr>")) goto end;
	}
	else if ((paragraph->line_flags & paragraph_not_centred) == 0)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"center\"/></w:pPr>")) goto end;
	}
	else if ((paragraph->line_flags & (paragraph_not_aligned_left | paragraph_not_aligned_right)) == paragraph_not_aligned_left)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"right\"/></w:pPr>")) goto end;
	}
	else if ((paragraph->line_flags & (paragraph_not_aligned_left | paragraph_not_aligned_right)) == paragraph_not_aligned_right)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"left\"/></w:pPr>")) goto end;
	}

	for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
	{
		content_span_iterator  sit;
		span_t                *span;

		for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
		{
			int si;
			double font_size_new;

			content_state->ctm_prev = &span->ctm;
			font_size_new = extract_font_size(&span->ctm);
			if (!content_state->font.name
					|| strcmp(span->font_name, content_state->font.name)
					|| span->flags.font_bold != content_state->font.bold
					|| span->flags.font_italic != content_state->font.italic
					|| font_size_new != content_state->font.size
					)
			{
				if (content_state->font.name)
				{
					if (odt_run_finish(alloc, content_state, content)) goto end;
				}
				content_state->font.name = span->font_name;
				content_state->font.bold = span->flags.font_bold;
				content_state->font.italic = span->flags.font_italic;
				content_state->font.size = font_size_new;
				if (extract_odt_run_start( alloc, content, styles, content_state)) goto end;
			}

			for (si=0; si<span->chars_num; ++si)
			{
				char_t* char_ = &span->chars[si];
				int c = char_->ucs;
				if (extract_astring_catc_unicode_xml(alloc, content, c)) goto end;
			}

			if (extract_astring_char_truncate_if(content, '-')) goto end;
		}
		if (paragraph->line_flags & paragraph_breaks_strangely)
		{
			if (extract_astring_cat(alloc, content, "<w:br/>")) goto end;
		}
	}
	if (content_state->font.name)
	{
		if (odt_run_finish(alloc, content_state, content)) goto end;
	}
	if (odt_paragraph_finish(alloc, content)) goto end;

	e = 0;

	end:
	return e;
}

static int
odt_append_image(
		extract_alloc_t   *alloc,
		extract_astring_t *output,
		image_t           *image)
{
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "<text:p text:style-name=\"Standard\">\n");
	extract_astring_catf(alloc, output, "<draw:frame draw:style-name=\"fr1\" draw:name=\"Picture %s\" text:anchor-type=\"as-char\" svg:width=\"%fin\" svg:height=\"%fin\" draw:z-index=\"0\">\n",
				image->id,
				image->w / 72.0,
				image->h / 72.0);
	extract_astring_catf(alloc, output, "<draw:image xlink:href=\"Pictures/%s\" xlink:type=\"simple\" xlink:show=\"embed\" xlink:actuate=\"onLoad\" draw:mime-type=\"image/%s\"/>\n",
			image->name,
			image->type);
	extract_astring_cat(alloc, output, "</draw:frame>\n");
	extract_astring_cat(alloc, output, "</text:p>\n");

	return 0;
}

static int
odt_output_rotated_paragraphs(
		extract_alloc_t      *alloc,
		block_t              *block,
		double                rotation_rad,
		double                x_pt,
		double                y_pt,
		double                w_pt,
		double                h_pt,
		int                   text_box_id,
		extract_astring_t    *content,
		extract_odt_styles_t *styles,
		content_state_t      *content_state)
{
	int                          e = 0;
	paragraph_t                *paragraph;
	content_paragraph_iterator  pit;
	double                      pt_to_inch = 1/72.0;

	outf("rotated paragraphs: rotation_rad=%f (x y)=(%f %f) (w h)=(%f %f)", rotation_rad, x_pt, y_pt, w_pt, h_pt);

	if (!e) e = extract_astring_cat(alloc, content, "\n");

	if (!e) e = extract_astring_cat(alloc, content, "<text:p text:style-name=\"Standard\">\n");
	if (!e) e = extract_astring_catf(alloc, content, "<draw:frame"
			" text:anchor-type=\"paragraph\""
			" draw:z-index=\"5\""
			" draw:name=\"Shape%i\""
			" draw:style-name=\"gr1\""
			" draw:text-style-name=\"Standard\""
			" svg:width=\"%fin\""
			" svg:height=\"%fin\""
			" draw:transform=\"rotate (%f) translate (%fin %fin)\""
			">\n"
			,
			text_box_id,
			w_pt * pt_to_inch,
			h_pt * pt_to_inch,
			-rotation_rad,
			x_pt * pt_to_inch,
			y_pt * pt_to_inch
			);
	if (!e) e = extract_astring_cat(alloc, content, "<draw:text-box>\n");

	for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
		if (!e) e = document_to_odt_content_paragraph(alloc, content_state, paragraph, content, styles);

	if (!e) e = extract_astring_cat(alloc, content, "\n");
	if (!e) e = extract_astring_cat(alloc, content, "</draw:text-box>\n");
	if (!e) e = extract_astring_cat(alloc, content, "</draw:frame>\n");

	if (!e) e = extract_astring_cat(alloc, content, "</text:p>\n");

	return e;
}

static int
odt_append_table(
		extract_alloc_t      *alloc,
		table_t              *table,
		extract_astring_t    *output,
		extract_odt_styles_t *styles)
{
	int e = -1;
	int y;

	{
		int x;
		static int table_number = 0;
		table_number += 1;
		if (extract_astring_catf(alloc, output,
				"\n"
				"    <table:table text:style-name=\"extract.table\" table:name=\"extract.table.%i\">\n"
				"        <table:table-columns>\n"
				,
				table_number
				)) goto end;

		for (x=0; x<table->cells_num_x; ++x)
		{
			if (extract_astring_cat(alloc, output,
					"            <table:table-column table:style-name=\"extract.table.column\"/>\n"
					)) goto end;
		}
		if (extract_astring_cat(alloc, output,
				"        </table:table-columns>\n"
				)) goto end;
	}
	for (y=0; y<table->cells_num_y; ++y)
	{
		int x;
		if (extract_astring_cat(alloc, output,
				"        <table:table-row>\n"
				)) goto end;

		for (x=0; x<table->cells_num_x; ++x)
		{
			cell_t                     *cell = table->cells[y*table->cells_num_x + x];
			content_paragraph_iterator  pit;
			paragraph_t                *paragraph;
			content_state_t             content_state;

			if (!cell->above || !cell->left)
			{
				if (extract_astring_cat(alloc, output, "            <table:covered-table-cell/>\n")) goto end;
				continue;
			}

			if (extract_astring_cat(alloc, output, "            <table:table-cell")) goto end;
			if (cell->extend_right > 1)
			{
				if (extract_astring_catf(alloc, output, " table:number-columns-spanned=\"%i\"", cell->extend_right)) goto end;
			}
			if (cell->extend_down > 1)
			{
				if (extract_astring_catf(alloc, output, " table:number-rows-spanned=\"%i\"", cell->extend_down)) goto end;
			}
			if (extract_astring_catf(alloc, output, ">\n")) goto end;

			content_state.font.name = NULL;
			content_state.ctm_prev = NULL;
			for (paragraph = content_paragraph_iterator_init(&pit, &cell->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
				if (document_to_odt_content_paragraph(alloc, &content_state, paragraph, output, styles)) goto end;
			if (content_state.font.name)
				if (odt_run_finish(alloc, &content_state, output)) goto end;
			if (extract_astring_cat(alloc, output, "\n")) goto end;
			if (extract_astring_cat(alloc, output, "            </table:table-cell>\n")) goto end;
		}
		if (extract_astring_cat(alloc, output, "        </table:table-row>\n")) goto end;
	}
	if (extract_astring_cat(alloc, output, "    </table:table>\n")) goto end;
	e = 0;

	end:
	return e;
}

static int
odt_append_rotated_paragraphs(
		extract_alloc_t       *alloc,
		content_state_t       *content_state,
		block_t               *block,
		int                   *text_box_id,
		const matrix4_t       *ctm,
		double                 rotate,
		extract_astring_t     *output,
		extract_odt_styles_t  *styles)
{

	int               e           = -1;
	point_t           extent      = {0, 0};
	content_iterator  cit;
	content_t        *content;
	paragraph_t      *paragraph   = content_first_paragraph(&block->content);

	span_t           *first_span  = content_first_span(&content_first_line(&paragraph->content)->content);
	point_t           origin      = { first_span->chars[0].x,
									 first_span->chars[0].y };
	matrix_t          ctm_inverse = {1, 0, 0, 1, 0, 0};
	double            ctm_det     = ctm->a*ctm->d - ctm->b*ctm->c;

	outf("rotate=%.2frad=%.1fdeg ctm: origin=(%f %f) abcd=(%f %f %f %f)",
		 rotate, rotate * 180 / pi,
		 origin.x,
		 origin.y,
		 ctm->a,
		 ctm->b,
		 ctm->c,
		 ctm->d
		 );

	if (ctm_det != 0)
	{
		ctm_inverse.a = +ctm->d / ctm_det;
		ctm_inverse.b = -ctm->b / ctm_det;
		ctm_inverse.c = -ctm->c / ctm_det;
		ctm_inverse.d = +ctm->a / ctm_det;
	}
	else
	{
		outf("cannot invert ctm=(%f %f %f %f)",
			 ctm->a, ctm->b, ctm->c, ctm->d);
	}

	for (content = content_iterator_init(&cit, &block->content); content != NULL; content = content_iterator_next(&cit))
	{
		content_line_iterator  lit;
		line_t                *line;
		paragraph_t           *paragraph;

		assert(content->type == content_paragraph);
		if (content->type != content_paragraph)
			continue;

		paragraph = (paragraph_t *)content;

		for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
		{
			span_t *span = extract_line_span_last(line);
			char_t *char_ = extract_span_char_last(span);
			double  adv = char_->adv * extract_font_size(&span->ctm);
			double  x = char_->x + adv * cos(rotate);
			double  y = char_->y + adv * sin(rotate);

			double  dx = x - origin.x;
			double  dy = y - origin.y;

			double  xx = ctm_inverse.a * dx + ctm_inverse.b * dy;
			double  yy = ctm_inverse.c * dx + ctm_inverse.d * dy;
			yy = -yy;
			if (xx > extent.x) extent.x = xx;
			if (yy > extent.y) extent.y = yy;
			if (0) outf("rotate=%f origin=(%f %f) xy=(%f %f) dxy=(%f %f) xxyy=(%f %f) span: %s",
						rotate, origin.x, origin.y, x, y, dx, dy, xx, yy, extract_span_string(alloc, span));
		}
	}
	outf("rotate=%f extent is: (%f %f)",
		 rotate, extent.x, extent.y);

	*text_box_id += 1;

	if (odt_output_rotated_paragraphs(
			alloc,
			block,
			rotate,
			origin.x,
			origin.y,
			extent.x,
			extent.y,
			*text_box_id,
			output,
			styles,
			content_state)) goto end;

	e = 0;
end:

	return e;
}

static int
extract_page_to_odt_content(
		extract_alloc_t      *alloc,
		extract_page_t       *page,
		int                   spacing,
		int                   rotation,
		int                   images,
		extract_astring_t    *output,
		extract_odt_styles_t *styles)
{
	int ret = -1;
	int text_box_id = 0;
	int c;

	for (c=0; c<page->subpages_num; ++c)
	{
		subpage_t                  *subpage = page->subpages[c];
		content_iterator            cit;
		content_t                  *content;
		content_table_iterator      tit;
		table_t                    *table;
		content_state_t content_state;
		content_state.font.name = NULL;
		content_state.font.size = 0;
		content_state.font.bold = 0;
		content_state.font.italic = 0;
		content_state.ctm_prev = NULL;

		content = content_iterator_init(&cit, &subpage->content);
		table = content_table_iterator_init(&tit, &subpage->tables);
		while (1)
		{
			double y_paragraph;
			double y_table;
			block_t *block = (content && content->type == content_block) ? (block_t *)content : NULL;
			paragraph_t *paragraph = (content && content->type == content_paragraph) ? (paragraph_t *)content : (block ? content_first_paragraph(&block->content) : NULL);
			line_t *first_line = paragraph ? content_first_line(&paragraph->content) : NULL;
			span_t *first_span = first_line ? content_first_span(&first_line->content) : NULL;
			if (!paragraph && !table)   break;
			y_paragraph = (first_span) ? first_span->chars[0].y : DBL_MAX;
			y_table = (table) ? table->pos.y : DBL_MAX;

			if (first_span && y_paragraph < y_table)
			{
				const matrix4_t *ctm = &first_span->ctm;
				double           rotate = atan2(ctm->b, ctm->a);

				if (spacing
					&& content_state.ctm_prev
					&& first_span
					&& extract_matrix4_cmp(content_state.ctm_prev,
								&first_span->ctm)
					)
				{

					if (odt_append_empty_paragraph(alloc, output, styles)) goto end;
				}

				if (spacing)
				{

					if (odt_append_empty_paragraph(alloc, output, styles)) goto end;
				}

				if (rotation && rotate != 0)
				{
					assert(block);
					if (odt_append_rotated_paragraphs(alloc, &content_state, block, &text_box_id, ctm, rotate, output, styles)) goto end;
				}
				else if (block)
				{
					content_paragraph_iterator pit;
					int                        first = 1;

					for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
					{
						if (spacing && !first)
						{

							if (odt_append_empty_paragraph(alloc, output, styles)) goto end;
						}
						first = 0;

						if (document_to_odt_content_paragraph(alloc, &content_state, paragraph, output, styles)) goto end;
					}
				}
				else
				{
					if (document_to_odt_content_paragraph(alloc, &content_state, paragraph, output, styles)) goto end;
				}
				content = content_iterator_next(&cit);
			}
			else if (table)
			{
				if (odt_append_table(alloc, table, output, styles)) goto end;
				table = content_table_iterator_next(&tit);
			}
		}

		outf("images=%i", images);
		if (images)
		{
			content_t *images, *next;
			outf("subpage->images_num=%i", content_count_images(&subpage->content));
			for (images = subpage->content.base.next; images != &subpage->content.base; images = next)
			{
				image_t *image = (image_t *)images;
				next = images->next;
				if (images->type != content_image)
					continue;
				odt_append_image(alloc, output, image);
			}
		}
	}
	ret = 0;

	end:

	return ret;
}

int
extract_document_to_odt_content(
		extract_alloc_t      *alloc,
		document_t           *document,
		int                   spacing,
		int                   rotation,
		int                   images,
		extract_astring_t    *content,
		extract_odt_styles_t *styles)
{
	int p;
	int ret = 0;

	for (p=0; p<document->pages_num; ++p)
	{
		extract_page_t *page = document->pages[p];

		ret = extract_page_to_odt_content(
				alloc,
				page,
				spacing,
				rotation,
				images,
				content,
				styles);
		if (ret) break;
	};

	return ret;
}

int
extract_odt_content_item(
		extract_alloc_t      *alloc,
		extract_astring_t    *contentss,
		int                   contentss_num,
		extract_odt_styles_t *styles,
		images_t             *images,
		const char           *name,
		const char           *text,
		char                **text2)
{
	int e = -1;
	extract_astring_t   temp;
	extract_astring_init(&temp);
	*text2 = NULL;

	(void) images;
	if (0)
	{}
	else if (!strcmp(name, "content.xml"))
	{

		char* text_intermediate = NULL;
		extract_astring_t   styles_definitions = {0};

		if (extract_content_insert(
				alloc,
				text,
				NULL ,
				NULL ,
				"</office:text>" ,
				contentss,
				contentss_num,
				&text_intermediate
				)) goto end;
		outf("text_intermediate: %s", text_intermediate);

		if (odt_styles_definitions(alloc, styles, &styles_definitions)) goto end;

		if (extract_astring_cat(alloc, &styles_definitions,
				"\n"
				"<style:style style:name=\"extract.table\" style:family=\"table\"/>\n"
				"<style:style style:name=\"extract.table.column\" style:family=\"table-column\"/>\n"
				)) goto end;

		e = extract_content_insert(
				alloc,
				text_intermediate,
				"<office:automatic-styles/>" ,
				NULL ,
				NULL ,
				&styles_definitions,
				1,
				text2
				);
		outf("e=%i errno=%i", e, errno);
		extract_free(alloc, &text_intermediate);
		extract_astring_free(alloc, &styles_definitions);
		outf("e=%i errno=%i", e, errno);
		if (e) goto end;
	}
	else if (!strcmp(name, "META-INF/manifest.xml"))
	{

		int e = 0;
		int i;
		for (i=0; i<images->images_num; ++i)
		{
			image_t* image = images->images[i];
			if (!e) e = extract_astring_catf(alloc, &temp, "<manifest:file-entry manifest:full-path=\"Pictures/%s\" manifest:media-type=\"image/%s\"/>\n",
					image->name,
					image->type
					);
		}
		if (!e) e = extract_content_insert(
				alloc,
				text,
				NULL ,
				NULL ,
				"</manifest:manifest>" ,
				&temp,
				1,
				text2
				);
		if (e) goto end;
	}
	else
	{
		*text2 = NULL;
	}
	e = 0;
	end:
	outf("e=%i errno=%i text2=%s", e, errno, text2 ? *text2 : "");
	if (e)
	{

		extract_free(alloc, text2);

	}
	extract_astring_free(alloc, &temp);
	extract_astring_init(&temp);
	return e;
}

int
extract_odt_write_template(
		extract_alloc_t      *alloc,
		extract_astring_t    *contentss,
		int                   contentss_num,
		extract_odt_styles_t *styles,
		images_t             *images,
		const char           *path_template,
		const char           *path_out,
		int                   preserve_dir)
{
	int     e = -1;
	int     i;
	char*   path_tempdir = NULL;
	char*   path = NULL;
	char*   text = NULL;
	char*   text2 = NULL;

	assert(path_out);
	assert(path_template);

	if (extract_check_path_shell_safe(path_out))
	{
		outf("path_out is unsafe: %s", path_out);
		goto end;
	}

	outf("images->images_num=%i", images->images_num);
	if (extract_asprintf(alloc, &path_tempdir, "%s.dir", path_out) < 0) goto end;
	if (extract_systemf(alloc, "rm -r '%s' 2>/dev/null", path_tempdir) < 0) goto end;

	if (extract_mkdir(path_tempdir, 0777))
	{
		outf("Failed to create directory: %s", path_tempdir);
		goto end;
	}

	outf("Unzipping template document '%s' to tempdir: %s",
			path_template, path_tempdir);
	if (extract_systemf(alloc, "unzip -q -d '%s' '%s'", path_tempdir, path_template))
	{
		outf("Failed to unzip %s into %s",
				path_template, path_tempdir);
		goto end;
	}

	{
		const char *names[] =
		{
				"content.xml",
				"META-INF/manifest.xml",
		};
		int names_num = sizeof(names) / sizeof(names[0]);
		for (i=0; i<names_num; ++i)
		{
			const char* name = names[i];
			extract_free(alloc, &path);
			extract_free(alloc, &text);
			extract_free(alloc, &text2);
			if (extract_asprintf(alloc, &path, "%s/%s", path_tempdir, name) < 0) goto end;
			if (extract_read_all_path(alloc, path, &text)) goto end;

			outf("before extract_odt_content_item() styles->styles_num=%i", styles->styles_num);
			if (extract_odt_content_item(
					alloc,
					contentss,
					contentss_num,
					styles,
					images,
					name,
					text,
					&text2
					))
			{
				outf("extract_odt_content_item() failed");
				goto end;
			}

			outf("after extract_odt_content_item styles->styles_num=%i", styles->styles_num);

			{
				const char* text3 = (text2) ? text2 : text;
				if (extract_write_all(text3, strlen(text3), path)) goto end;
				outf("have written to path=%s", path);
			}
		}
	}

	extract_free(alloc, &path);
	if (extract_asprintf(alloc, &path, "%s/Pictures", path_tempdir) < 0) goto end;
	if (extract_mkdir(path, 0777))
	{
		outf("Failed to mkdir %s", path);
		goto end;
	}
	for (i=0; i<images->images_num; ++i)
	{
		image_t* image = images->images[i];
		extract_free(alloc, &path);
		if (extract_asprintf(alloc, &path, "%s/Pictures/%s", path_tempdir, image->name) < 0) goto end;
		if (extract_write_all(image->data, image->data_size, path)) goto end;
	}

	outf("Zipping tempdir to create %s", path_out);
	{
		const char* path_out_leaf = strrchr(path_out, '/');
		if (!path_out_leaf) path_out_leaf = path_out;
		if (extract_systemf(alloc, "cd '%s' && zip -q -r -D '../%s' .", path_tempdir, path_out_leaf))
		{
			outf("Zip command failed to convert '%s' directory into output file: %s",
					path_tempdir, path_out);
			goto end;
		}
	}

	if (!preserve_dir)
	{
		if (extract_remove_directory(alloc, path_tempdir)) goto end;
	}

	e = 0;

	end:
	outf("e=%i", e);
	extract_free(alloc, &path_tempdir);
	extract_free(alloc, &path);
	extract_free(alloc, &text);
	extract_free(alloc, &text2);

	if (e)
	{
		outf("Failed to create %s", path_out);
	}
	return e;
}

#include "memento.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int extract_outf_verbose = 0;

void extract_outf_verbose_set(int verbose)
{
	extract_outf_verbose = verbose;
}

void (extract_outf)(
		int         level,
		const char *file,
		int         line,
		const char *fn,
		int         ln,
		const char *format,
		...
		)
{
	va_list va;
	if (level > extract_outf_verbose) {
		return;
	}

	if (ln) {
		fprintf(stderr, "%s:%i:%s: ", file, line, fn);
	}
	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
	if (ln) {
		size_t len = strlen(format);
		if (len == 0 || format[len-1] != '\n') {
			fprintf(stderr, "\n");
		}
	}
}

#include "extract/extract.h"

static inline double
mind(double a, double b)
{
	return (a < b) ? a : b;
}

static inline double
maxd(double a, double b)
{
	return (a > b) ? a : b;
}

rect_t extract_rect_intersect(rect_t a, rect_t b)
{
	rect_t r;

	r.min.x = maxd(a.min.x, b.min.x);
	r.min.y = maxd(a.min.y, b.min.y);
	r.max.x = mind(a.max.x, b.max.x);
	r.max.y = mind(a.max.y, b.max.y);

	return r;
}

rect_t extract_rect_union(rect_t a, rect_t b)
{
	rect_t r;

	r.min.x = mind(a.min.x, b.min.x);
	r.min.y = mind(a.min.y, b.min.y);
	r.max.x = maxd(a.max.x, b.max.x);
	r.max.y = maxd(a.max.y, b.max.y);

	return r;
}

rect_t extract_rect_union_point(rect_t a, point_t b)
{
	rect_t r;

	r.min.x = mind(a.min.x, b.x);
	r.min.y = mind(a.min.y, b.y);
	r.max.x = maxd(a.max.x, b.x);
	r.max.y = maxd(a.max.y, b.y);

	return r;
}

int extract_rect_contains_rect(rect_t a, rect_t b)
{
	if (a.min.x > b.min.x)
		return 0;
	if (a.min.y > b.min.y)
		return 0;
	if (a.max.x < b.max.x)
		return 0;
	if (a.max.y < b.max.y)
		return 0;

	return 1;
}

int extract_rect_valid(rect_t a)
{
	return (a.min.x <= a.max.x && a.min.y <= a.max.y);
}

#include <errno.h>
#include <stdarg.h>

#include <sys/stat.h>

#undef extract_APPLE_IOS
#ifdef __APPLE__
	#include "TargetConditionals.h"
	#ifdef TARGET_OS_IPHONE
		#define extract_APPLE_IOS
	#elif TARGET_IPHONE_SIMULATOR
		#define extract_APPLE_IOS
	#endif
#endif

int extract_systemf(extract_alloc_t *alloc, const char *format, ...)
{
#ifdef extract_APPLE_IOS

	(void) alloc;
	(void) format;
	errno = ENOTSUP;
	return -1;
#else

	int      e;
	char    *command;
	va_list  va;

	va_start(va, format);
	e = extract_vasprintf(alloc, &command, format, va);
	va_end(va);
	if (e < 0) return e;
	outf("running: %s", command);
	e = system(command);
	extract_free(alloc, &command);
	if (e > 0) {
		errno = EIO;
	}
	return e;

#endif
}

int  extract_read_all(extract_alloc_t *alloc, FILE *in, char **o_out)
{
	size_t  len = 0;
	size_t  delta = 128;
	for(;;) {
		size_t n;
		if (extract_realloc2(alloc, o_out, len, len + delta + 1)) {
			extract_free(alloc, o_out);
			return -1;
		}
		n = fread(*o_out + len, 1 , delta , in);
		len += n;
		if (feof(in)) {
			(*o_out)[len] = 0;
			return 0;
		}
		if (ferror(in)) {

			errno = EIO;
			extract_free(alloc, o_out);
			return -1;
		}
	}
}

int  extract_read_all_path(extract_alloc_t *alloc, const char *path, char **o_text)
{
	int e = -1;
	FILE *f = NULL;
	f = fopen(path, "rb");
	if (!f) goto end;
	if (extract_read_all(alloc, f, o_text)) goto end;
	e = 0;
	end:
	if (f) fclose(f);
	if (e) extract_free(alloc, o_text);
	return e;
}

int  extract_write_all(const void *data, size_t data_size, const char *path)
{
	int e = -1;
	FILE *f = fopen(path, "w");
	if (!f) goto end;
	if (fwrite(data, data_size, 1 , f) != 1) goto end;
	e = 0;
	end:
	if (f) fclose(f);
	return e;
}

int extract_check_path_shell_safe(const char *path)

{
	if (0
			|| strstr(path, "..")
			|| strchr(path, '\'')
			|| strchr(path, '"')
			|| strchr(path, ' ')
			) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}
int extract_remove_directory(extract_alloc_t *alloc, const char *path)
{
	if (extract_check_path_shell_safe(path)) {
		outf("path_out is unsafe: %s", path);
		return -1;
	}
	return extract_systemf(alloc, "rm -r '%s'", path);
}

#ifdef _WIN32
#include <direct.h>
int extract_mkdir(const char *path, int mode)
{
	(void) mode;
	return _mkdir(path);
}
#else
int extract_mkdir(const char *path, int mode)
{
	return mkdir(path, mode);
}
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>

int
extract_content_insert(
		extract_alloc_t    *alloc,
		const char         *original,
		const char         *single_name,
		const char         *mid_begin_name,
		const char         *mid_end_name,
		extract_astring_t  *contentss,
		int                 contentss_num,
		char              **o_out)
{
	int                e         = -1;
	const char        *mid_begin = NULL;
	const char        *mid_end   = NULL;
	const char        *single    = NULL;
	extract_astring_t  out;
	extract_astring_init(&out);

	assert(single_name || mid_begin_name || mid_end_name);

	if (single_name) single = strstr(original, single_name);

	if (single)
	{
		outf("Have found single_name='%s', using in preference to mid_begin_name=%s mid_end_name=%s",
			 single_name,
			 mid_begin_name,
			 mid_end_name);
		mid_begin = single;
		mid_end = single + strlen(single_name);
	}
	else
	{
		if (mid_begin_name) {
			mid_begin = strstr(original, mid_begin_name);
			if (!mid_begin) {
				outf("error: could not find '%s' in odt content", mid_begin_name);
				errno = ESRCH;
				goto end;
			}
			mid_begin += strlen(mid_begin_name);
		}
		if (mid_end_name) {
			mid_end = strstr(mid_begin ? mid_begin : original, mid_end_name);
			if (!mid_end) {
				outf("error: could not find '%s' in odt content", mid_end_name);
				e = -1;
				errno = ESRCH;
				goto end;
			}
		}
		if (!mid_begin) {
			mid_begin = mid_end;
		}
		if (!mid_end) {
			mid_end = mid_begin;
		}
	}

	if (extract_astring_catl(alloc, &out, original, mid_begin - original)) goto end;
	{
		int i;
		for (i=0; i<contentss_num; ++i) {
			if (extract_astring_catl(alloc, &out, contentss[i].chars, contentss[i].chars_num)) goto end;
		}
	}
	assert( mid_end);

	if (extract_astring_cat(alloc, &out, mid_end)) goto end;

	*o_out = out.chars;
	out.chars = NULL;

	e = 0;
end:

	if (e) {
		extract_astring_free(alloc, &out);
		*o_out = NULL;
	}

	return e;
}

#include "extract/alloc.h"

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <limits.h>

#include <stdlib.h>
#include <string.h>

static int str_catl(extract_alloc_t *alloc, char **p, const char *s, int s_len)
{
	size_t p_len = (*p) ? strlen(*p) : 0;

	if (extract_realloc2(alloc,
						 p,
						 p_len + 1,
						 p_len + s_len + 1)) return -1;
	memcpy(*p + p_len, s, s_len);
	(*p)[p_len + s_len] = 0;

	return 0;
}

static int str_catc(extract_alloc_t *alloc, char **p, char c)
{
	return str_catl(alloc, p, &c, 1);
}

#if 0

static int str_cat(extract_alloc_t *alloc, char **p, const char *s)
{
	return str_catl(alloc, p, s, strlen(s));
}
#endif

char *extract_xml_tag_attributes_find(extract_xml_tag_t *tag, const char *name)
{
	int i;

	for (i=0; i<tag->attributes_num; ++i) {
		if (!strcmp(tag->attributes[i].name, name)) {
			char* ret = tag->attributes[i].value;
			return ret;
		}
	}
	outf("Failed to find attribute '%s'",name);

	return NULL;
}

int extract_xml_tag_attributes_find_float(
		extract_xml_tag_t *tag,
		const char        *name,
		float             *o_out)
{
	const char *value = extract_xml_tag_attributes_find(tag, name);

	if (!value) {
		errno = ESRCH;
		return -1;
	}
	if (extract_xml_str_to_float(value, o_out)) return -1;

	return 0;
}

int extract_xml_tag_attributes_find_double(
		extract_xml_tag_t *tag,
		const char        *name,
		double            *o_out)
{
	const char *value = extract_xml_tag_attributes_find(tag, name);

	if (!value) {
		errno = ESRCH;
		return -1;
	}
	if (extract_xml_str_to_double(value, o_out)) return -1;

	return 0;
}

int extract_xml_tag_attributes_find_int(
		extract_xml_tag_t *tag,
		const char        *name,
		int               *o_out)
{
	const char *text = extract_xml_tag_attributes_find(tag, name);

	return extract_xml_str_to_int(text, o_out);
}

int extract_xml_tag_attributes_find_uint(
		extract_xml_tag_t *tag,
		const char        *name,
		unsigned          *o_out)
{
	const char *text = extract_xml_tag_attributes_find(tag, name);

	return extract_xml_str_to_uint(text, o_out);
}

int extract_xml_tag_attributes_find_size(
		extract_xml_tag_t *tag,
		const char        *name,
		size_t            *o_out)
{
	const char *text = extract_xml_tag_attributes_find(tag, name);

	return extract_xml_str_to_size(text, o_out);
}

int extract_xml_str_to_llint(const char *text, long long*o_out)
{
	char      *endptr;
	long long  x;

	if (!text) {
		errno = ESRCH;
		return -1;
	}
	if (text[0] == 0) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	x = strtoll(text, &endptr, 10 );
	if (errno) {
		return -1;
	}
	if (*endptr) {
		errno = EINVAL;
		return -1;
	}
	*o_out = x;

	return 0;
}

int extract_xml_str_to_ullint(const char *text, unsigned long long *o_out)
{
	char               *endptr;
	unsigned long long  x;

	if (!text) {
		errno = ESRCH;
		return -1;
	}
	if (text[0] == 0) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	x = strtoull(text, &endptr, 10 );
	if (errno) {
		return -1;
	}
	if (*endptr) {
		errno = EINVAL;
		return -1;
	}
	*o_out = x;

	return 0;
}

int extract_xml_str_to_int(const char *text, int *o_out)
{
	long long x;

	if (extract_xml_str_to_llint(text, &x)) return -1;
	if (x > INT_MAX || x < INT_MIN) {
		errno = ERANGE;
		return -1;
	}
	*o_out = (int) x;

	return 0;
}

int extract_xml_str_to_uint(const char *text, unsigned *o_out)
{
	unsigned long long x;

	if (extract_xml_str_to_ullint(text, &x)) return -1;
	if (x > UINT_MAX) {
		errno = ERANGE;
		return -1;
	}
	*o_out = (unsigned) x;

	return 0;
}

int extract_xml_str_to_size(const char *text, size_t *o_out)
{
	unsigned long long x;

	if (extract_xml_str_to_ullint(text, &x)) return -1;
	if (x > SIZE_MAX) {
		errno = ERANGE;
		return -1;
	}
	*o_out = (size_t) x;

	return 0;
}

int extract_xml_str_to_double(const char *text, double *o_out)
{
	char   *endptr;
	double  x;

	if (!text) {
		errno = ESRCH;
		return -1;
	}
	if (text[0] == 0) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	x = strtod(text, &endptr);
	if (errno) {
		return -1;
	}
	if (*endptr) {
		errno = EINVAL;
		return -1;
	}
	*o_out = x;

	return 0;
}

int extract_xml_str_to_float(const char *text, float *o_out)
{
	double x;

	if (extract_xml_str_to_double(text, &x)) {
		return -1;
	}
	if (x > FLT_MAX || x < -FLT_MAX) {
		errno = ERANGE;
		return -1;
	}
	*o_out = (float) x;

	return 0;
}

static int
extract_xml_tag_attributes_append(
		extract_alloc_t   *alloc,
		extract_xml_tag_t *tag,
		char              *name,
		char              *value)
{
	if (extract_realloc2(alloc,
						 &tag->attributes,
						 sizeof(extract_xml_attribute_t) * tag->attributes_num,
						 sizeof(extract_xml_attribute_t) * (tag->attributes_num+1)))
	{
		return -1;
	}
	tag->attributes[tag->attributes_num].name = name;
	tag->attributes[tag->attributes_num].value = value;
	tag->attributes_num += 1;

	return 0;
}

void extract_xml_tag_init(extract_xml_tag_t *tag)
{
	tag->name = NULL;
	tag->attributes = NULL;
	tag->attributes_num = 0;
	extract_astring_init(&tag->text);
}

void extract_xml_tag_free(extract_alloc_t *alloc, extract_xml_tag_t *tag)
{
	int i;

	if (tag == NULL)
		return;

	extract_free(alloc, &tag->name);
	for (i=0; i<tag->attributes_num; ++i) {
		extract_xml_attribute_t* attribute = &tag->attributes[i];
		extract_free(alloc, &attribute->name);
		extract_free(alloc, &attribute->value);
	}
	extract_free(alloc, &tag->attributes);
	extract_astring_free(alloc, &tag->text);
	extract_xml_tag_init(tag);
}

#if 0

static int extract_xml_strcmp_null(const char *a, const char *b)
{
	if (!a && !b) return 0;
	if (!a) return -1;
	if (!b) return 1;
	return strcmp(a, b);
}
#endif

#if 0

int extract_xml_compare_tags(const extract_xml_tag_t *lhs, const extract_xml_tag_t *rhs)
{
	int d;
	int i;
	d = extract_xml_strcmp_null(lhs->name, rhs->name);
	if (d)  return d;
	for(i=0;; ++i) {
		if (i >= lhs->attributes_num || i >= rhs->attributes_num) {
			break;
		}
		const extract_xml_attribute_t* lhs_attribute = &lhs->attributes[i];
		const extract_xml_attribute_t* rhs_attribute = &rhs->attributes[i];
		d = extract_xml_strcmp_null(lhs_attribute->name, rhs_attribute->name);
		if (d)  return d;
		d = extract_xml_strcmp_null(lhs_attribute->value, rhs_attribute->value);
		if (d)  return d;
	}
	if (lhs->attributes_num > rhs->attributes_num) return +1;
	if (lhs->attributes_num < rhs->attributes_num) return -1;
	return 0;
}
#endif

int extract_xml_pparse_init(extract_alloc_t *alloc, extract_buffer_t *buffer, const char *first_line)
{
	char *first_line_buffer = NULL;
	int   e = -1;

	if (first_line) {
		size_t first_line_len = strlen(first_line);
		size_t actual;
		if (extract_malloc(alloc, &first_line_buffer, first_line_len + 1)) goto end;

		if (extract_buffer_read(buffer, first_line_buffer, first_line_len, &actual)) {
			outf("error: failed to read first line.");
			goto end;
		}
		first_line_buffer[actual] = 0;
		if (strcmp(first_line, first_line_buffer)) {
			outf("Unrecognised prefix: %s", first_line_buffer);
			errno = ESRCH;
			goto end;
		}
	}

	for(;;) {
		char c;
		int ee = extract_buffer_read(buffer, &c, 1, NULL);
		if (ee) {
			if (ee==1) errno = ESRCH;
			goto end;
		}
		if (c == '<') {
			break;
		}
		else if (c == ' ' || c == '\n') {}
		else {
			outf("Expected '<' but found c=%i", c);
			goto end;
		}
	}

	e = 0;
end:

	extract_free(alloc, &first_line_buffer);

	return e;
}

static int s_next(extract_buffer_t *buffer, int *ret, char *o_c)

{
	int e = extract_buffer_read(buffer, o_c, 1, NULL);

	if (e == +1) {
		*ret = +1;
		errno = ESRCH;
	}

	return e;
}

static const char *
extract_xml_tag_string(extract_alloc_t *alloc, extract_xml_tag_t *tag)
{
	static char *buffer = NULL;

	extract_free(alloc, &buffer);
	if (extract_asprintf(alloc, &buffer, "<name=%s>", tag->name ? tag->name : ""))
	{
		return "";
	}

	return buffer;
}

int extract_xml_pparse_next(extract_buffer_t *buffer, extract_xml_tag_t *out)
{
	int              ret = -1;
	char            *attribute_name = NULL;
	char            *attribute_value = NULL;
	char             c;
	extract_alloc_t *alloc = extract_buffer_alloc(buffer);

	if (0) outf("out is: %s", extract_xml_tag_string(extract_buffer_alloc(buffer), out));
	assert(buffer);
	extract_xml_tag_free(alloc, out);

	if (str_catl( alloc, &out->name, NULL, 0)) goto end;
	for(;;) {
		int e = extract_buffer_read(buffer, &c, 1, NULL);
		if (e) {
			if (e == +1) ret = 1;
			goto end;
		}
		if (c == '>' || c == ' ')  break;
		if (str_catc(alloc, &out->name, c)) goto end;
	}
	if (c == ' ') {

		for(;;) {

			for(;;) {
				if (s_next(buffer, &ret, &c)) goto end;
				if (c == '=' || c == '>' || c == ' ') break;
				if (str_catc(alloc, &attribute_name, c)) goto end;
			}
			if (c == '>') break;

			if (c == '=') {

				int quote_single = 0;
				int quote_double = 0;
				size_t l;
				if (str_catl( alloc, &attribute_value, NULL, 0)) goto end;
				for(;;) {
					if (s_next(buffer, &ret, &c)) goto end;
					if (c == '\'')      quote_single = !quote_single;
					else if (c == '"')  quote_double = !quote_double;
					else if (!quote_single && !quote_double
							&& (c == ' ' || c == '/' || c == '>')
							) {

						break;
					}
					else if (c == '\\') {

						if (s_next(buffer, &ret, &c)) goto end;
					}
					if (str_catc(alloc, &attribute_value, c)) goto end;
				}

				l = strlen(attribute_value);
				if (l >= 2) {
					if (
							(attribute_value[0] == '"' && attribute_value[l-1] == '"')
							||
							(attribute_value[0] == '\'' && attribute_value[l-1] == '\'')
							) {
						memmove(attribute_value, attribute_value+1, l-2);
						attribute_value[l-2] = 0;
					}
				}
			}

			if (str_catl( alloc, &attribute_name, NULL, 0)) goto end;
			if (str_catl( alloc, &attribute_value, NULL, 0)) goto end;

			if (extract_xml_tag_attributes_append(alloc, out, attribute_name, attribute_value)) goto end;
			attribute_name = NULL;
			attribute_value = NULL;
			if (c == '/') {
				if (s_next(buffer, &ret, &c)) goto end;
			}
			if (c == '>') break;
		}
	}

	for(;;) {

		int e = extract_buffer_read(buffer, &c, 1, NULL);
		if (e == +1) {
			break;
		}
		if (e) goto end;
		if (c == '<') break;
		if (extract_astring_catc(alloc, &out->text, c)) goto end;
	}

	ret = 0;
end:

	extract_free(alloc, &attribute_name);
	extract_free(alloc, &attribute_value);
	if (ret) {
		extract_xml_tag_free(alloc, out);
	}

	return ret;
}

#include "extract/alloc.h"

#include <zlib.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

typedef struct
{
	int16_t  mtime;
	int16_t  mdate;
	int32_t  crc_sum;
	int32_t  size_compressed;
	int32_t  size_uncompressed;
	char    *name;
	uint32_t offset;
	uint16_t attr_internal;
	uint32_t attr_external;
} extract_zip_cd_file_t;

struct extract_zip_t
{
	extract_buffer_t      *buffer;
	extract_zip_cd_file_t *cd_files;
	int                    cd_files_num;

	int                    errno_;
	int                    eof;
	uint16_t               compression_method;
	int                    compress_level;

	uint16_t               mtime;
	uint16_t               mdate;
	uint16_t               version_creator;
	uint16_t               version_extract;
	uint16_t               general_purpose_bit_flag;
	uint16_t               file_attr_internal;
	uint32_t               file_attr_external;
	char                  *archive_comment;
};

int extract_zip_open(extract_buffer_t *buffer, extract_zip_t **o_zip)
{
	int              e = -1;
	extract_zip_t   *zip;
	extract_alloc_t *alloc = extract_buffer_alloc(buffer);

	if (extract_malloc(alloc, &zip, sizeof(*zip))) goto end;

	zip->cd_files = NULL;
	zip->cd_files_num = 0;
	zip->buffer = buffer;
	zip->errno_ = 0;
	zip->eof = 0;
	zip->compression_method = Z_DEFLATED;
	zip->compress_level = Z_DEFAULT_COMPRESSION;

	{
		time_t     t = time(NULL);
		struct tm *tm;
		#ifdef _POSIX_SOURCE
			struct tm   tm_local;
			tm = gmtime_r(&t, &tm_local);
		#else
			tm = gmtime(&t);
		#endif
		if (tm)
		{

			zip->mtime = (uint16_t) ((tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2));
			zip->mdate = (uint16_t) (((1900 + tm->tm_year - 1980) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday);
		}
		else
		{
			outf0("*** gmtime_r() failed");
			zip->mtime = 0;
			zip->mdate = 0;
		}
	}

	zip->version_creator = (0x3 << 8) + 30;
	zip->version_extract = 10;
	zip->general_purpose_bit_flag = 0;
	zip->file_attr_internal = 0;

	zip->file_attr_external = (0100644 << 16) + 0;
	if (extract_strdup(alloc, "Artifex", &zip->archive_comment)) goto end;

	e = 0;
end:

	if (e) {
		if (zip) extract_free(alloc, &zip->archive_comment);
		extract_free(alloc, &zip);
		*o_zip = NULL;
	}
	else {
		*o_zip = zip;
	}

	return e;
}

static int s_native_little_endinesss(void)
{
	static const char   a[] = { 1, 2};
	uint16_t b = *(uint16_t*) a;
	if (b == 1 + 2*256) {

		return 1;
	}
	else if (b == 2 + 1*256) {

		return 0;
	}

	assert(0);
	return 0;
}

static void *s_zalloc(void *opaque, unsigned items, unsigned size)
{
	extract_zip_t   *zip = opaque;
	extract_alloc_t *alloc = extract_buffer_alloc(zip->buffer);
	void            *ptr;

	if (extract_malloc(alloc, &ptr, items*size)) return NULL;

	return ptr;
}

static void s_zfree(void *opaque, void *ptr)
{
	extract_zip_t   *zip = opaque;
	extract_alloc_t *alloc = extract_buffer_alloc(zip->buffer);

	extract_free(alloc, &ptr);
}

static int
s_write_compressed(
		extract_zip_t *zip,
		const void    *data,
		size_t         data_length,
		size_t        *o_compressed_length)
{
	int      ze;
	z_stream zstream = {0};

	if (zip->errno_)    return -1;
	if (zip->eof)       return +1;

	zstream.zalloc = s_zalloc;
	zstream.zfree = s_zfree;
	zstream.opaque = zip;

	ze = deflateInit2(&zstream,
			zip->compress_level,
			Z_DEFLATED,
			-15 ,
			8 ,
			Z_DEFAULT_STRATEGY);
	if (ze != Z_OK)
	{
		errno = (ze == Z_MEM_ERROR) ? ENOMEM : EINVAL;
		zip->errno_ = errno;
		outf("deflateInit2() failed ze=%i", ze);
		return -1;
	}

	zstream.next_in = (void*) data;
	zstream.avail_in = (unsigned) data_length;

	if (o_compressed_length)
	{
		*o_compressed_length = 0;
	}

	for(;;)
	{

		unsigned char   buffer[1024];
		zstream.next_out = &buffer[0];
		zstream.avail_out = sizeof(buffer);
		ze = deflate(&zstream, zstream.avail_in ? Z_NO_FLUSH : Z_FINISH);
		if (ze != Z_STREAM_END && ze != Z_OK)
		{
			outf("deflate() failed ze=%i", ze);
			errno = EIO;
			zip->errno_ = errno;
			return -1;
		}
		{

			size_t  bytes_written;
			int e = extract_buffer_write(zip->buffer, buffer, zstream.next_out - buffer, &bytes_written);
			if (o_compressed_length)
			{
				*o_compressed_length += bytes_written;
			}
			if (e)
			{
				if (e == -1)    zip->errno_ = errno;
				if (e ==  +1)   zip->eof = 1;
				outf("extract_buffer_write() failed e=%i errno=%i", e, errno);
				return e;
			}
		}
		if (ze == Z_STREAM_END)
		{
			break;
		}
	}
	ze = deflateEnd(&zstream);
	if (ze != Z_OK)
	{
		outf("deflateEnd() failed ze=%i", ze);
		errno = EIO;
		zip->errno_ = errno;
		return -1;
	}
	if (o_compressed_length)
	{
		assert(*o_compressed_length == (size_t) zstream.total_out);
	}

	return 0;
}

static int s_write(extract_zip_t *zip, const void *data, size_t data_length)
{
	size_t actual;
	int e;

	if (zip->errno_)    return -1;
	if (zip->eof)       return +1;

	e = extract_buffer_write(zip->buffer, data, data_length, &actual);
	if (e == -1)    zip->errno_ = errno;
	if (e == +1)    zip->eof = 1;

	return e;
}

static int s_write_uint32(extract_zip_t *zip, uint32_t value)
{
	if (s_native_little_endinesss()) {
		return s_write(zip, &value, sizeof(value));
	}
	else {
		unsigned char value2[4] = {
				(unsigned char) (value >> 0),
				(unsigned char) (value >> 8),
				(unsigned char) (value >> 16),
				(unsigned char) (value >> 24)
				};
		return s_write(zip, &value2, sizeof(value2));
	}
}

static int s_write_uint16(extract_zip_t *zip, uint16_t value)
{
	if (s_native_little_endinesss()) {
		return s_write(zip, &value, sizeof(value));
	}
	else {
		unsigned char value2[2] = {
				(unsigned char) (value >> 0),
				(unsigned char) (value >> 8)
				};
		return s_write(zip, &value2, sizeof(value2));
	}
}

static int s_write_string(extract_zip_t *zip, const char *text)
{
	return s_write(zip, text, strlen(text));
}

int extract_zip_write_file(
		extract_zip_t *zip,
		const void    *data,
		size_t         data_length,
		const char    *name)
{
	int                    e = -1;
	extract_zip_cd_file_t *cd_file = NULL;
	extract_alloc_t       *alloc = extract_buffer_alloc(zip->buffer);

	if (data_length > INT_MAX) {
		assert(0);
		errno = EINVAL;
		return -1;
	}

	if (extract_realloc2(
			alloc,
			&zip->cd_files,
			sizeof(extract_zip_cd_file_t) * zip->cd_files_num,
			sizeof(extract_zip_cd_file_t) * (zip->cd_files_num+1)
			)) goto end;
	cd_file = &zip->cd_files[zip->cd_files_num];
	cd_file->name = NULL;

	cd_file->mtime = zip->mtime;
	cd_file->mdate = zip->mdate;
	cd_file->crc_sum = (int32_t) crc32(crc32(0, NULL, 0), data, (int) data_length);
	cd_file->size_uncompressed = (int) data_length;
	if (zip->compression_method == 0)
	{
		cd_file->size_compressed = cd_file->size_uncompressed;
	}
	if (extract_strdup(alloc, name, &cd_file->name)) goto end;
	cd_file->offset = (int) extract_buffer_pos(zip->buffer);
	cd_file->attr_internal = zip->file_attr_internal;
	cd_file->attr_external = zip->file_attr_external;
	if (!cd_file->name) goto end;

	{
		const char extra_local[] = "";
		uint16_t general_purpose_bit_flag = zip->general_purpose_bit_flag;
		if (zip->compression_method)    general_purpose_bit_flag |= 8;
		s_write_uint32(zip, 0x04034b50);
		s_write_uint16(zip, zip->version_extract);
		s_write_uint16(zip, general_purpose_bit_flag);
		s_write_uint16(zip, zip->compression_method);
		s_write_uint16(zip, cd_file->mtime);
		s_write_uint16(zip, cd_file->mdate);
		if (zip->compression_method)
		{
			s_write_uint32(zip, 0);
			s_write_uint32(zip, 0);
			s_write_uint32(zip, 0);
		}
		else
		{
			s_write_uint32(zip, cd_file->crc_sum);
			s_write_uint32(zip, cd_file->size_compressed);
			s_write_uint32(zip, cd_file->size_uncompressed);
		}
		s_write_uint16(zip, (uint16_t) strlen(name));
		s_write_uint16(zip, sizeof(extra_local)-1);
		s_write_string(zip, cd_file->name);
		s_write(zip, extra_local, sizeof(extra_local)-1);
	}

	if (zip->compression_method)
	{

		size_t  data_length_compressed;
		s_write_compressed(zip, data, data_length, &data_length_compressed);
		cd_file->size_compressed = (int) data_length_compressed;

		s_write_uint32(zip, 0x08074b50);
		s_write_uint32(zip, cd_file->crc_sum);
		s_write_uint32(zip, cd_file->size_compressed);
		s_write_uint32(zip, cd_file->size_uncompressed);
	}
	else
	{
		s_write(zip, data, data_length);
	}

	if (zip->errno_)    e = -1;
	else if (zip->eof)  e = +1;
	else e = 0;

end:

	if (e) {

		if (cd_file) extract_free(alloc, &cd_file->name);
	}
	else {

		zip->cd_files_num += 1;
	}

	return e;
}

int extract_zip_close(extract_zip_t **pzip)
{
	int              e = -1;
	size_t           pos;
	size_t           len;
	int              i;
	extract_zip_t   *zip = *pzip;
	extract_alloc_t *alloc;

	if (!zip) {
		return 0;
	}
	alloc = extract_buffer_alloc(zip->buffer);
	pos = extract_buffer_pos(zip->buffer);
	len = 0;

	for (i=0; i<zip->cd_files_num; ++i) {
		const char extra[] = "";
		size_t pos2 = extract_buffer_pos(zip->buffer);
		extract_zip_cd_file_t* cd_file = &zip->cd_files[i];
		s_write_uint32(zip, 0x02014b50);
		s_write_uint16(zip, zip->version_creator);
		s_write_uint16(zip, zip->version_extract);
		s_write_uint16(zip, zip->general_purpose_bit_flag);
		s_write_uint16(zip, zip->compression_method);
		s_write_uint16(zip, cd_file->mtime);
		s_write_uint16(zip, cd_file->mdate);
		s_write_uint32(zip, cd_file->crc_sum);
		s_write_uint32(zip, cd_file->size_compressed);
		s_write_uint32(zip, cd_file->size_uncompressed);
		s_write_uint16(zip, (uint16_t) strlen(cd_file->name));
		s_write_uint16(zip, sizeof(extra)-1);
		s_write_uint16(zip, 0);
		s_write_uint16(zip, 0);
		s_write_uint16(zip, cd_file->attr_internal);
		s_write_uint32(zip, cd_file->attr_external);
		s_write_uint32(zip, cd_file->offset);
		s_write_string(zip, cd_file->name);
		s_write(zip, extra, sizeof(extra)-1);
		len += extract_buffer_pos(zip->buffer) - pos2;
		extract_free(alloc, &cd_file->name);
	}
	extract_free(alloc, &zip->cd_files);

	s_write_uint32(zip, 0x06054b50);
	s_write_uint16(zip, 0);
	s_write_uint16(zip, 0);
	s_write_uint16(zip, (uint16_t) zip->cd_files_num);
	s_write_uint16(zip, (uint16_t) zip->cd_files_num);
	s_write_uint32(zip, (int) len);
	s_write_uint32(zip, (int) pos);

	s_write_uint16(zip, (uint16_t) strlen(zip->archive_comment));
	s_write_string(zip, zip->archive_comment);
	extract_free(alloc, &zip->archive_comment);

	if (zip->errno_)    e = -1;
	else if (zip->eof)  e = +1;
	else e = 0;

	extract_free(alloc, pzip);

	return e;
}
