#include "extract/extract.h"
#include "extract/alloc.h"

#include "astring.h"
#include "document.h"
#include "docx.h"
#include "docx_template.h"
#include "html.h"
#include "json.h"
#include "mem.h"
#include "odt.h"
#include "odt_template.h"
#include "outf.h"
#include "xml.h"
#include "zip.h"


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
		/* This frees our internal data, and is used by extract_internal_end(). */
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

/* Returns first span in a line. */
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

	/* Unlink content from anywhere it might be. */
	content_unlink(content);

	/* Sanity check root. */
	if (root->base.next == &root->base)
	{
		assert(root->base.prev == &root->base);
	}

	/* And append content */
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

/* Appends new empty span content to a content_list_t; returns -1 with errno set on error. */
int content_append_new_span(extract_alloc_t *alloc, content_root_t *root, span_t **pspan, structure_t *structure)
{
	if (content_new_span(alloc, pspan, structure)) return -1;
	content_append(root, &(*pspan)->base);

	return 0;
}

/* Appends new empty line content to a content_list_t; returns -1 with errno set on error. */
int content_append_new_line(extract_alloc_t *alloc, content_root_t *root, line_t **pline)
{
	if (content_new_line(alloc, pline)) return -1;
	content_append(root, &(*pline)->base);

	return 0;
}

/* Appends new empty paragraph content to a content_list_t; returns -1 with errno set on error. */
int content_append_new_paragraph(extract_alloc_t *alloc, content_root_t *root, paragraph_t **pparagraph)
{
	if (content_new_paragraph(alloc, pparagraph)) return -1;
	content_append(root, &(*pparagraph)->base);

	return 0;
}

/* Appends new empty block content to a content_list_t; returns -1 with errno set on error. */
int content_append_new_block(extract_alloc_t *alloc, content_root_t *root, block_t **pblock)
{
	if (content_new_block(alloc, pblock)) return -1;
	content_append(root, &(*pblock)->base);

	return 0;
}

/* Appends new empty table content to a content_list_t; returns -1 with errno set on error. */
int content_append_new_table(extract_alloc_t *alloc, content_root_t *root, table_t **ptable)
{
	if (content_new_table(alloc, ptable)) return -1;
	content_append(root, &(*ptable)->base);

	return 0;
}

/* Appends new empty image content to a content_list_t; returns -1 with errno set on error. */
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
	/* Unlink replacement. */
	if (replacement->prev)
	{
		replacement->prev->next = replacement->next;
		replacement->next->prev = replacement->prev;
	}
	/* Insert replacement */
	replacement->prev = current->prev;
	current->prev->next = replacement;
	replacement->next = current->next;
	current->next->prev = replacement;
	/* Unlink current */
	current->prev = NULL;
	current->next = NULL;
}

/* Replaces current element with a new empty paragraph content; returns -1 with errno set on error. */
int content_replace_new_paragraph(extract_alloc_t *alloc, content_t *current, paragraph_t **pparagraph)
{
	if (content_new_paragraph(alloc, pparagraph)) return -1;
	content_replace(current, &(*pparagraph)->base);

	return 0;
}

/* Replaces current element with a new empty block content; returns -1 with errno set on error. */
int content_replace_new_block(extract_alloc_t *alloc, content_t *current, block_t **pblock)
{
	if (content_new_block(alloc, pblock)) return -1;
	content_replace(current, &(*pblock)->base);

	return 0;
}

/* Replaces current element with a new empty line content; returns -1 with errno set on error. */
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


/* Move image_t's from document->subpage[] to *o_images.

On return document->subpage[].images* will be NULL etc.
*/
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

				/* Add image type if we haven't seen it before. */
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
						/* We haven't seen this image type before. */
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


/* Returns +1, 0 or -1 depending on sign of x. */
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

/* If we exceed MAX_STRUCT_NEST then this probably indicates that
 * structure nesting is not to be trusted. */
#define MAX_STRUCT_NEST 64

struct extract_t
{
	extract_alloc_t         *alloc;
	int                      layout_analysis;
	double                   master_space_guess;
	document_t               document;

	/* Number of extra spans from subpage_span_end_clean(). */
	int                      num_spans_split;

	/* Number of extra spans from autosplit=1. */
	int                      num_spans_autosplit;

	/* Only used if autosplit is non-zero. */
	double                   span_offset_x;
	double                   span_offset_y;

	/* Used to generate unique ids for images. */
	int                      image_n;

	/* List of strings that are the generated docx content for each page. When
	 * zip_* can handle appending of data, we will be able to remove this list. */
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

	/* Create the extract structure. */
	if (extract_malloc(alloc, &extract, sizeof(*extract)))
		return -1;

	extract_bzero(extract, sizeof(*extract));
	extract->alloc = alloc;
	extract->master_space_guess = 0.5;
	document_init(&extract->document);

	/* FIXME: Start at 10 because template document might use some low-numbered IDs.
	*/
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

	if (extract_xml_pparse_init(extract->alloc, buffer, NULL /*first_line*/)) {
		outf("Failed to read start of intermediate data: %s", strerror(errno));
		goto end;
	}
	/* Data read from <path> is expected to be XML looking like:

	<page>
		<span>
			<char ...>
			<char ...>
			...
		</span>
		<span>
			...
		</span>
		...
	</page>
	<page>
		...
	</page>
	...

	We convert this into a list of subpage_t's, each containing a list of
	span_t's, each containing a list of char_t's.

	While doing this, we do some within-span processing by calling
	subpage_span_end_clean():
		Remove spurious spaces.
		Split spans in two where there seem to be large gaps between glyphs.
	*/
	for(;;) {
		extract_page_t *page;
		subpage_t      *subpage;
		rect_t          mediabox = extract_rect_infinite; /* Fake mediabox */
		int             e = extract_xml_pparse_next(buffer, &tag);

		if (e == 1) break; /* EOF. */
		if (e) goto end;
		if (!strcmp(tag.name, "?xml")) {
			/* We simply skip this if we find it. As of 2020-07-31, mutool adds
			this header to mupdf raw output, but gs txtwrite does not include
			it. */
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
					/* Compressed. */
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
							0 /*x*/,
							0 /*y*/,
							0 /*w*/,
							0 /*h*/,
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

					/* BBox is bogus here. Analysis will fail. */
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

	/* FIXME: RJW: Should continue the last span if everything is the same. */

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

/* Create a new empty span, based on the current one. */
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

	save = span->base; /* Avoid overwriting linked list. */
	*span = *span0;
	span->base = save;
	span->font_name = name;
	span->chars = NULL;
	span->chars_num = 0;

	return span;
}

/*
This routine returns the previous non-space-char, UNLESS the span
starts with a space, in which case we accept that one.
*/
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

	/* Is there a previous span to which we should consider attaching this char. */
	span0 = find_previous_non_space_char_ish(&subpage->content, &char_num0, &intervening_space);

	/* Spans can't continue over different structure elements. */
	if (span0 && span0->structure != extract->document.current)
		span0 = NULL;

	if (span0 == NULL)
	{
		/* No previous continuable span. */
		outf("%c x=%g y=%g adv=%g\n", ucs, x, y, adv);
	}
	else
	{
		/* We have a span. Check whether we need to break to a new line, or add (or subtract) a space. */
		char_t *char_prev = &span0->chars[char_num0];
		double adv0 = char_prev->adv;
		point_t predicted_end_of_char0 = extract_predicted_end_of_char(char_prev, span0);
		/* We don't currently have access to the size of the advance for a space.
		 * Typically it's around 1 to 1/2 that of a real char. So guess at that
		 * using the 2 advances we have available to us. */
		double space_guess = (adv0 + adv)/2 * extract->master_space_guess;

		/* Use dot product to calculate the distance that we have moved along the direction vector. */
		dist = (x - predicted_end_of_char0.x) * dir.x + (y - predicted_end_of_char0.y) * dir.y;
		/* Use dot product to calculate the distance that we have moved perpendicular to the direction vector. */
		perp = (x - predicted_end_of_char0.x) * dir.y - (y - predicted_end_of_char0.y) * dir.x;
		/* Both dist and perp are multiplied by scale_squared. */
		dist /= scale_squared;
		perp /= scale_squared;
		/* So now, dist, perp, adv, adv0 and space_guess are all in pre-transform space. */

		/* So fabs(dist) is expected to be 0, and perp is expected to be 0 for characters
		 * "naturally placed" on a line. */
		outf("%c x=%g y=%g adv=%g dist=%g perp=%g\n", ucs, x, y, adv, dist, perp);

		/* Arbitrary fractions here; ideally we should consult the font bbox, but we don't currently
		 * have that. */
		if (fabs(perp) > 3*space_guess/2 || fabs(dist) > space_guess * 8)
		{
			/* Create new span. */
			if (span->chars_num > 0)
			{
				extract->num_spans_autosplit += 1;
				span = split_to_new_span(extract->alloc, &subpage->content, span);
				if (span == NULL) goto end;
			}
		}
		else if (intervening_space)
		{
			/* Some files, notably zlib.3.pdf appear to contain stray extra spaces within the PDF
			 * content themselves. e.g. "suppor ts". We therefore spot when the
			 * space allocated for a space isn't used, and remove the space. */
			/* MAGIC NUMBER WARNING. zlib.pdf says that /4 is not sensitive enough. /3 is OK. */
			if (dist < space_guess/3)
			{
				if (span->chars_num > 0)
				{
					span->chars_num--;
					/* Don't need to worry about it being empty, as we're about to add another char! */
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
		/* MAGIC NUMBER WARNING: We expect the space char to be about 1/2 as wide of a standard char.
		 * zlib3.pdf shows that sometimes we need to insert a space when it's *just* smaller than
		 * this. (e.g. 'eveninthe'). */
		else if (!intervening_space && dist > 2*space_guess/3)
		{
			/* Larger gap than expected. Add an extra space. */
			/* Where should the space go? At the predicted position where the previous char
			 * ended. */
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
		/* Calling code called extract_span_begin() then extract_span_end()
		without any call to extract_add_char(). Our joining code assumes that
		all spans are non-empty, so we need to delete this span. */
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
	/* Find first step with dx > 0. */
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
		/* Horizontal line. */
		outf("have found horizontal line: %s", extract_rect_string(&rect));
		if (tablelines_append(extract->alloc, &subpage->tablelines_horizontal, &rect, color)) return -1;
	}
	else if (dy / dx > 5)
	{
		/* Vertical line. */
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

/* Appends new empty subpage_t to the last page of an extract->document. */
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

/* Appends new empty page_t to an extract->document. */
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
			/* We are closing a four-element path, so this could be a thin
			rectangle that defines a line in a table. */
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
		/* New topmost entry. */
		document->current = structure;
		document->structure = structure;
	}
	else
	{
		/* Add a child */
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
				/* We encode each character as utf8. */
				char_t* char_ = &span->chars[c];
				unsigned cc = char_->ucs;
				if (extract_astring_catc_unicode(
						alloc,
						text,
						cc,
						0 /*xml*/,
						1 /*ascii_ligatures*/,
						1 /*ascii_dash*/,
						1 /*ascii_apostrophe*/
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
						/* Reference cvs output trims trailing spaces. */
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
				NULL /*o_actual*/
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

	/* Round font_size to nearest 0.01. */
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

	/* Construct a matrix to undo the rotation that we are about to put into
	 * the file. i.e. get us a matrix that maps us from where the chars are
	 * positioned back to the pre-rotated position. These pre-rotated positions
	 * can then be used to calculate the origin/extent of the area that we
	 * need to put into the file. */

	/* The well know rotation matrixes:
	 *
	 *  CW: [  cos(theta)	 sin(theta) ]  CCW: [  cos(theta)	-sin(theta) ]
	 *	  [ -sin(theta)	 cos(theta) ]	   [  sin(theta)	 cos(theta) ]
	 */

	/* Word gives us an angle to rotate by clockwise. So the inverse is the
	 * CCW matrix: */
	unrotate.a = cos(angle);
	unrotate.b = -sin(angle);
	unrotate.c = -unrotate.b;
	unrotate.d = unrotate.a;
	/* And the forward rotation is the CW matrix: */
	rotate.a = unrotate.a;  /* cos(theta) =  cos(-theta) */
	rotate.b = -unrotate.b; /* sin(theta) = -sin(-theta) */
	rotate.c = -rotate.b;
	rotate.d = rotate.a;

	/* So ctm.unrotate.rotate = ctm, by construction. ctm.unrotate should
	 * (in the common cases where the ctm is just a scale + rotation)  map
	 * all our character locations back to a rectangular region. We now
	 * calculate that region as pre_box. */

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

			/* Allow for the height of the span here. */
			hoff *= sqrt(span0->ctm.c * span0->ctm.c + span0->ctm.d * span0->ctm.d);

			if (start.y < end.y)
				start.y -= hoff;
			else
				end.y -= hoff;
			pre_box = extract_rect_union_point(pre_box, start);
			pre_box = extract_rect_union_point(pre_box, end);
		}
	}

	/* So pre_box rotated around the origin by angle should give us the region we want. */
	/* BUT word etc rotate around the centre of the box. So we need to offset the region to
	 * allow for this. */
	/* So word, takes the declared box, and subtracts the centre vector from it. Then it
	 * does the rotation (around the origin - now the centre of the box). Then it adds the
	 * centre vector to it again. So the centre of the box does not change. Unfortunately,
	 * we haven't easily got the centre vector of the transformed box to hand, so calculate
	 * it by rerotating the centre vector of the pre_box.*/
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

	/* So the centre of our pre_box should be trans_centre not centre. */
	centre.x -= trans_centre.x;
	centre.y -= trans_centre.y;
	pre_box.min.x -= centre.x;
	pre_box.min.y -= centre.y;
	pre_box.max.x -= centre.x;
	pre_box.max.y -= centre.y;

#if 0
	/* So, as a sanity check, convert the 4 corners back to a quad. */
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

	/* And a further adjustment. If we mess up line widths, text can wrap too early,
	 * resulting in content extending too far down the page, and truncating at the
	 * bottom of the text frame. Similarly, line spacing. We can't tell word 'make
	 * the box large enough', so we have to add a fudge factor and extend the bottom
	 * of the box ourselves. As long as we aren't filling the background, or drawing
	 * a bounding box, this should be fine.
	 *
	 * Unfortunately, we can't just extend pre_box downwards, because we rotate from
	 * the centre of the box, so we need to adjust for that.
	 */
	/* Double the height of the box. */
	{
		/* extra = how much to extend the box downwards. */
		double extra = pre_box.max.y - pre_box.min.y;
		/* So we are offsetting the centre of the box by offset. */
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
