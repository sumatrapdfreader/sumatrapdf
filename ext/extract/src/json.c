/* These extract_json_*() functions generate json content data. */

#include "extract/extract.h"

#include "astring.h"
#include "document.h"
#include "html.h"
#include "mem.h"
#include "memento.h"
#include "outf.h"
#include "sys.h"
#include "text.h"
#include "zip.h"

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
	//extract_astring_cat(alloc, content, "<html>\n");
	//extract_astring_cat(alloc, content, "<body>\n");

	/* Write paragraphs into <content>. */
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
						// flush stored text.
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
					 // Nothing to do for lines and paragraphs as they just enclose spans.
					 // Nothing to do for the others for now.
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
