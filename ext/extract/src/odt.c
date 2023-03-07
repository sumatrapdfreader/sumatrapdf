/* These extract_odt_*() functions generate odt content and odt zip archive
data.

Caller must call things in a sensible order to create valid content -
e.g. don't call odt_paragraph_start() twice without intervening call to
odt_paragraph_finish(). */

#include "extract/extract.h"

#include "odt_template.h"

#include "astring.h"
#include "document.h"
#include "odt.h"
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

/* ODT doesn't seem to support ad-hoc inline font specifications; instead
we have to define a style at the start of the content.xml file. So when
writing content we insert a style name and add the required styles to a
extract_odt_styles_t struct. */

struct extract_odt_style_t
{
	int     id; /* A unique id for this style. */
	font_t  font;
};

struct extract_odt_styles_t
{
	/* Styles are stored sorted. */
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

	/* This improves output e.g. for zlib.3.pdf, but clearly a hack. */
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

	/* Style for images. */
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

/* Adds specified style to <styles> if not already present. Sets *o_style to
point to the style_t within <styles>. */
static int
odt_styles_add(
	extract_alloc_t      *alloc,
	extract_odt_styles_t *styles,
	font_t               *font,
	extract_odt_style_t **o_style)
{
	extract_odt_style_t style = {0 /*id*/, *font};
	int i;

	/* We keep styles->styles[] sorted; todo: use bsearch or similar when
	searching. */
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
	/* Insert at position <i>. */
	if (extract_realloc(alloc, &styles->styles, sizeof(styles->styles[0]) * (styles->styles_num+1))) return -1;
	memmove(&styles->styles[i+1], &styles->styles[i], sizeof(styles->styles[0]) * (styles->styles_num - i));
	styles->styles_num += 1;
	styles->styles[i].id = styles->styles_num + 10; /* Leave space for template's built-in styles. */
	if (extract_strdup(alloc, font->name, &styles->styles[i].font.name)) return -1;
	styles->styles[i].font.size = font->size;
	styles->styles[i].font.bold = font->bold;
	styles->styles[i].font.italic = font->italic;
	*o_style = &styles->styles[i];

	return 0;
}

/* Starts a new run. Caller must ensure that s_odt_run_finish() was
called to terminate any previous run. */
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

/* Append an empty paragraph to *content. */
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
	/* [This comment is from docx, haven't checked odt.] It seems like our
	choice of font size here doesn't make any difference to the amount of
	vertical space, unless we include a non-space character. Presumably
	something to do with the styles in the template document. */
	content_state.font.name = fontname;
	content_state.font.size = 10;
	content_state.font.bold = 0;
	content_state.font.italic = 0;
	if (extract_odt_run_start(alloc, content, styles, &content_state)) goto end;
	//docx_char_append_string(content, "&#160;");   /* &#160; is non-break space. */
	if (odt_run_finish(alloc, NULL /*content_state*/, content)) goto end;
	if (odt_paragraph_finish(alloc, content)) goto end;
	e = 0;

end:
	return e;
}


/* Append odt xml for <paragraph> to <content>. Updates *content_state if we
change font. */
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

	/* Output justification. */
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
			/* Remove any trailing '-' at end of line. */
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

/* Write reference to image into odt content. */
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


/* Writes paragraph to content inside rotated text box. */
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

	// https://docs.oasis-open.org/office/OpenDocument/v1.3/cs02/part3-schema/OpenDocument-v1.3-cs02-part3-schema.html#attribute-draw_transform
	// says rotation is in degrees, but we seem to require -radians.
	//

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

			/* Write contents of this cell. */
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


/* Appends paragraphs with same rotation, starting with subpage->paragraphs[*p]
and updates *p. */
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
	/* Find extent of paragraphs with this same rotation. extent
	will contain max width and max height of paragraphs, in units
	before application of ctm, i.e. before rotation. */
	int               e           = -1;
	point_t           extent      = {0, 0};
	content_iterator  cit;
	content_t        *content;
	paragraph_t      *paragraph   = content_first_paragraph(&block->content);

	/* We assume that first span is at origin of text
	 * block. This assumes left-to-right text. */
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
			continue; /* This shouldn't happen for now! */

		paragraph = (paragraph_t *)content;

		/* Update <extent>. */
		for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
		{
			span_t *span = extract_line_span_last(line);
			char_t *char_ = extract_span_char_last(span);
			double  adv = char_->adv * extract_font_size(&span->ctm);
			double  x = char_->x + adv * cos(rotate);
			double  y = char_->y + adv * sin(rotate);

			double  dx = x - origin.x;
			double  dy = y - origin.y;

			/* Position relative to origin and before box rotation. */
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

	/* All the paragraphs have same rotation. We output them into
	 * a single rotated text box. */

	/* We need unique id for text box. */
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

	/* Write paragraphs into <content>. */
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
					/* Extra vertical space between paragraphs that were at
					different angles in the original document. */
					if (odt_append_empty_paragraph(alloc, output, styles)) goto end;
				}

				if (spacing)
				{
					/* Extra vertical space between paragraphs. */
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
							/* Extra vertical space between paragraphs. */
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

	/* Write paragraphs into <content>. */
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
		/* Insert paragraphs content. */
		char* text_intermediate = NULL;
		extract_astring_t   styles_definitions = {0};

		/* Insert content before '</office:text>'. */
		if (extract_content_insert(
				alloc,
				text,
				NULL /*single*/,
				NULL /*mid_begin_name*/,
				"</office:text>" /*mid_end_name*/,
				contentss,
				contentss_num,
				&text_intermediate
				)) goto end;
		outf("text_intermediate: %s", text_intermediate);

		/* Convert <styles> to text. */
		if (odt_styles_definitions(alloc, styles, &styles_definitions)) goto end;

		/* To make tables work, we seem to need to specify table and column
		styles, and these can be empty. todo: maybe specify exact sizes based
		on the pdf table and cell dimensions. */
		if (extract_astring_cat(alloc, &styles_definitions,
				"\n"
				"<style:style style:name=\"extract.table\" style:family=\"table\"/>\n"
				"<style:style style:name=\"extract.table.column\" style:family=\"table-column\"/>\n"
				)) goto end;

		/* Replace '<office:automatic-styles/>' with text from
		<styles_definitions>. */
		e = extract_content_insert(
				alloc,
				text_intermediate,
				"<office:automatic-styles/>" /*single*/,
				NULL /*mid_begin_name*/,
				NULL /*mid_end_name*/,
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
		/* Add images. */
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
				NULL /*single*/,
				NULL /*mid_begin_name*/,
				"</manifest:manifest>" /*mid_end_name*/,
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
		/* We might have set <text2> to new content. */
		extract_free(alloc, text2);
		/* We might have used <temp> as a temporary buffer. */
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

	/* Might be nice to iterate through all items in path_tempdir, but for now
	we look at just the items that we know extract_odt_content_item() will
	modify. */

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

	/* Copy images into <path_tempdir>/Pictures/. */
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
