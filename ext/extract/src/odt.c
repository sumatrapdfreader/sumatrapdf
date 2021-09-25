/* These extract_odt_*() functions generate odt content and odt zip archive
data.

Caller must call things in a sensible order to create valid content -
e.g. don't call odt_paragraph_start() twice without intervening call to
odt_paragraph_finish(). */

#include "../include/extract.h"

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


static int s_odt_paragraph_start(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "\n\n<text:p>");
}

static int s_odt_paragraph_finish(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "</text:p>");
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
    extract_odt_style_t*    styles;
    int                     styles_num;
};

static int s_odt_style_compare(extract_odt_style_t* a, extract_odt_style_t*b)
{
    int d;
    double dd;
    if ((d = strcmp(a->font.name, b->font.name)))   return d;
    if ((dd = a->font.size - b->font.size) != 0.0)  return (dd > 0.0) ? 1 : -1;
    if ((d = a->font.bold - b->font.bold))          return d;
    if ((d = a->font.italic - b->font.italic))      return d;
    return 0;
}

static int s_odt_style_append_definition(extract_alloc_t* alloc, extract_odt_style_t* style, extract_astring_t* text)
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

void extract_odt_styles_free(extract_alloc_t* alloc, extract_odt_styles_t* styles)
{
    int i;
    for (i=0; i<styles->styles_num; ++i)
    {
        extract_odt_style_t* style = &styles->styles[i];
        extract_free(alloc, &style->font.name);
    }
    extract_free(alloc, &styles->styles);
}

static int s_odt_styles_definitions(
        extract_alloc_t*        alloc,
        extract_odt_styles_t*   styles,
        extract_astring_t*      out
        )
{
    int i;
    if (extract_astring_cat(alloc, out, "<office:automatic-styles>")) return -1;
    for (i=0; i<styles->styles_num; ++i)
    {
        if (s_odt_style_append_definition(alloc, &styles->styles[i], out)) return -1;
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

static int s_odt_styles_add(
        extract_alloc_t*        alloc,
        extract_odt_styles_t*   styles,
        font_t*                 font,
        extract_odt_style_t**   o_style
    )
/* Adds specified style to <styles> if not already present. Sets *o_style to
point to the style_t within <styles>. */
{
    extract_odt_style_t style = {0 /*id*/, *font};
    int i;
    /* We keep styles->styles[] sorted; todo: use bsearch or similar when
    searching. */
    for (i=0; i<styles->styles_num; ++i)
    {
        int d = s_odt_style_compare(&style, &styles->styles[i]);
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

static int extract_odt_run_start(
        extract_alloc_t*        alloc,
        extract_astring_t*      content,
        extract_odt_styles_t*   styles,
        content_state_t*        content_state
        )
/* Starts a new run. Caller must ensure that s_odt_run_finish() was
called to terminate any previous run. */
{
    extract_odt_style_t* style;
    if (s_odt_styles_add(
            alloc,
            styles,
            &content_state->font,
            &style
            )) return -1;
    if (extract_astring_catf(alloc, content, "<text:span text:style-name=\"T%i\">", style->id)) return -1;
    return 0;
}

static int s_odt_run_finish(extract_alloc_t* alloc, content_state_t* content_state, extract_astring_t* content)
{
    if (content_state)  content_state->font.name = NULL;
    return extract_astring_cat(alloc, content, "</text:span>");
}

static int s_odt_append_empty_paragraph(extract_alloc_t* alloc, extract_astring_t* content, extract_odt_styles_t* styles)
/* Append an empty paragraph to *content. */
{
    int e = -1;
    content_state_t content_state = {0};
    if (s_odt_paragraph_start(alloc, content)) goto end;
    /* [This comment is from docx, haven't checked odt.] It seems like our
    choice of font size here doesn't make any difference to the ammount of
    vertical space, unless we include a non-space character. Presumably
    something to do with the styles in the template document. */
    content_state.font.name = "OpenSans";
    content_state.font.size = 10;
    content_state.font.bold = 0;
    content_state.font.italic = 0;
    if (extract_odt_run_start(alloc, content, styles, &content_state)) goto end;
    //docx_char_append_string(content, "&#160;");   /* &#160; is non-break space. */
    if (s_odt_run_finish(alloc, NULL /*content_state*/, content)) goto end;
    if (s_odt_paragraph_finish(alloc, content)) goto end;
    e = 0;
    end:
    return e;
}


static int s_document_to_odt_content_paragraph(
        extract_alloc_t*        alloc,
        content_state_t*        content_state,
        paragraph_t*            paragraph,
        extract_astring_t*      content,
        extract_odt_styles_t*   styles
        )
/* Append odt xml for <paragraph> to <content>. Updates *content_state if we
change font. */
{
    int e = -1;
    int l;

    if (s_odt_paragraph_start(alloc, content)) goto end;

    for (l=0; l<paragraph->lines_num; ++l)
    {
        line_t* line = paragraph->lines[l];
        int s;
        for (s=0; s<line->spans_num; ++s)
        {
            int si;
            span_t* span = line->spans[s];
            double font_size_new;
            content_state->ctm_prev = &span->ctm;
            font_size_new = extract_matrices_to_font_size(&span->ctm, &span->trm);
            if (!content_state->font.name
                    || strcmp(span->font_name, content_state->font.name)
                    || span->flags.font_bold != content_state->font.bold
                    || span->flags.font_italic != content_state->font.italic
                    || font_size_new != content_state->font.size
                    )
            {
                if (content_state->font.name)
                {
                    if (s_odt_run_finish(alloc, content_state, content)) goto end;
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
    }
    if (content_state->font.name)
    {
        if (s_odt_run_finish(alloc, content_state, content)) goto end;
    }
    if (s_odt_paragraph_finish(alloc, content)) goto end;
    
    e = 0;
    
    end:
    return e;
}

static int s_odt_append_image(
        extract_alloc_t*    alloc,
        extract_astring_t*  content,
        image_t*            image
        )
/* Write reference to image into odt content. */
{
    extract_astring_cat(alloc, content, "\n");
    extract_astring_cat(alloc, content, "<text:p text:style-name=\"Standard\">\n");
    extract_astring_catf(alloc, content, "<draw:frame draw:style-name=\"fr1\" draw:name=\"Picture %s\" text:anchor-type=\"as-char\" svg:width=\"%fin\" svg:height=\"%fin\" draw:z-index=\"0\">\n",
            image->id,
            image->w / 72.0,
            image->h / 72.0
            );
    extract_astring_catf(alloc, content, "<draw:image xlink:href=\"Pictures/%s\" xlink:type=\"simple\" xlink:show=\"embed\" xlink:actuate=\"onLoad\" draw:mime-type=\"image/%s\"/>\n",
            image->name,
            image->type
            );
    extract_astring_cat(alloc, content, "</draw:frame>\n");
    extract_astring_cat(alloc, content, "</text:p>\n");
    
    return 0;
}


static int s_odt_output_rotated_paragraphs(
        extract_alloc_t*    alloc,
        extract_page_t*     page,
        int                 paragraph_begin,
        int                 paragraph_end,
        double              rotation_rad,
        double              x_pt,
        double              y_pt,
        double              w_pt,
        double              h_pt,
        int                 text_box_id,
        extract_astring_t*  content,
        extract_odt_styles_t* styles,
        content_state_t*    content_state
        )
/* Writes paragraph to content inside rotated text box. */
{
    int e = 0;
    int p;
    double pt_to_inch = 1/72.0;
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
    
    for (p=paragraph_begin; p<paragraph_end; ++p)
    {
        paragraph_t* paragraph = page->paragraphs[p];
        if (!e) e = s_document_to_odt_content_paragraph(alloc, content_state, paragraph, content, styles);
    }
    
    if (!e) e = extract_astring_cat(alloc, content, "\n");
    if (!e) e = extract_astring_cat(alloc, content, "</draw:text-box>\n");
    if (!e) e = extract_astring_cat(alloc, content, "</draw:frame>\n");
    
    if (!e) e = extract_astring_cat(alloc, content, "</text:p>\n");
    
    return e;
}


static int s_odt_append_table(extract_alloc_t* alloc, table_t* table, extract_astring_t* content, extract_odt_styles_t* styles)
{
    int e = -1;
    int y;
    
    {
        int x;
        static int table_number = 0;
        table_number += 1;
        if (extract_astring_catf(alloc, content,
                "\n"
                "    <table:table text:style-name=\"extract.table\" table:name=\"extract.table.%i\">\n"
                "        <table:table-columns>\n"
                ,
                table_number
                )) goto end;

        for (x=0; x<table->cells_num_x; ++x)
        {
            if (extract_astring_cat(alloc, content,
                    "            <table:table-column table:style-name=\"extract.table.column\"/>\n"
                    )) goto end;
        }
        if (extract_astring_cat(alloc, content,
                "        </table:table-columns>\n"
                )) goto end;
  }
  for (y=0; y<table->cells_num_y; ++y)
    {
        int x;
        if (extract_astring_cat(alloc, content,
                "        <table:table-row>\n"
                )) goto end;
        
        for (x=0; x<table->cells_num_x; ++x)
        {
            cell_t* cell = table->cells[y*table->cells_num_x + x];
            if (!cell->above || !cell->left)
            {
                if (extract_astring_cat(alloc, content, "            <table:covered-table-cell/>\n")) goto end;
                continue;
            }
            
            if (extract_astring_cat(alloc, content, "            <table:table-cell")) goto end;
            if (cell->extend_right > 1)
            {
                if (extract_astring_catf(alloc, content, " table:number-columns-spanned=\"%i\"", cell->extend_right)) goto end;
            }
            if (cell->extend_down > 1)
            {
                if (extract_astring_catf(alloc, content, " table:number-rows-spanned=\"%i\"", cell->extend_down)) goto end;
            }
            if (extract_astring_catf(alloc, content, ">\n")) goto end;
            
            /* Write contents of this cell. */
            {
                int p;
                content_state_t content_state;
                content_state.font.name = NULL;
                content_state.ctm_prev = NULL;
                for (p=0; p<cell->paragraphs_num; ++p)
                {
                    paragraph_t* paragraph = cell->paragraphs[p];
                    if (s_document_to_odt_content_paragraph(alloc, &content_state, paragraph, content, styles)) goto end;
                }
                if (content_state.font.name)
                {
                    if (s_odt_run_finish(alloc, &content_state, content)) goto end;
                }
                if (extract_astring_cat(alloc, content, "\n")) goto end;
            }
            if (extract_astring_cat(alloc, content, "            </table:table-cell>\n")) goto end;
        }
        if (extract_astring_cat(alloc, content, "        </table:table-row>\n")) goto end;
    }
    if (extract_astring_cat(alloc, content, "    </table:table>\n")) goto end;
    e = 0;
    
    end:
    return e;
}


static int s_odt_append_rotated_paragraphs(
        extract_alloc_t*    alloc,
        extract_page_t*     page,
        content_state_t*    content_state,
        int*                p,
        int*                text_box_id,
        const matrix_t*     ctm,
        double              rotate,
        extract_astring_t*  content,
        extract_odt_styles_t* styles
        )
/* Appends paragraphs with same rotation, starting with page->paragraphs[*p]
and updates *p. */
{
    /* Find extent of paragraphs with this same rotation. extent
    will contain max width and max height of paragraphs, in units
    before application of ctm, i.e. before rotation. */
    int e = -1;
    point_t extent = {0, 0};
    int p0 = *p;
    int p1;
    paragraph_t* paragraph = page->paragraphs[*p];

    outf("rotate=%.2frad=%.1fdeg ctm: ef=(%f %f) abcd=(%f %f %f %f)",
            rotate, rotate * 180 / pi,
            ctm->e,
            ctm->f,
            ctm->a,
            ctm->b,
            ctm->c,
            ctm->d
            );

    {
        /* We assume that first span is at origin of text
        block. This assumes left-to-right text. */
        double rotate0 = rotate;
        const matrix_t* ctm0 = ctm;
        point_t origin =
        {
                paragraph->lines[0]->spans[0]->chars[0].x,
                paragraph->lines[0]->spans[0]->chars[0].y
        };
        matrix_t ctm_inverse = {1, 0, 0, 1, 0, 0};
        double ctm_det = ctm->a*ctm->d - ctm->b*ctm->c;
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

        for (*p=p0; *p<page->paragraphs_num; ++*p)
        {
            paragraph = page->paragraphs[*p];
            ctm = &paragraph->lines[0]->spans[0]->ctm;
            rotate = atan2(ctm->b, ctm->a);
            if (rotate != rotate0)
            {
                break;
            }

            /* Update <extent>. */
            {
                int l;
                for (l=0; l<paragraph->lines_num; ++l)
                {
                    line_t* line = paragraph->lines[l];
                    span_t* span = extract_line_span_last(line);
                    char_t* char_ = extract_span_char_last(span);
                    double adv = char_->adv * extract_matrix_expansion(span->trm);
                    double x = char_->x + adv * cos(rotate);
                    double y = char_->y + adv * sin(rotate);

                    double dx = x - origin.x;
                    double dy = y - origin.y;

                    /* Position relative to origin and before box rotation. */
                    double xx = ctm_inverse.a * dx + ctm_inverse.b * dy;
                    double yy = ctm_inverse.c * dx + ctm_inverse.d * dy;
                    yy = -yy;
                    if (xx > extent.x) extent.x = xx;
                    if (yy > extent.y) extent.y = yy;
                    if (0) outf("rotate=%f *p=%i: origin=(%f %f) xy=(%f %f) dxy=(%f %f) xxyy=(%f %f) span: %s",
                            rotate, *p, origin.x, origin.y, x, y, dx, dy, xx, yy, extract_span_string(alloc, span));
                }
            }
        }
        p1 = *p;
        rotate = rotate0;
        ctm = ctm0;
        outf("rotate=%f p0=%i p1=%i. extent is: (%f %f)",
                rotate, p0, p1, extent.x, extent.y);
    }

    /* Paragraphs p0..p1-1 have same rotation. We output them into
    a single rotated text box. */

    /* We need unique id for text box. */
    *text_box_id += 1;

    if (s_odt_output_rotated_paragraphs(
            alloc,
            page,
            p0,
            p1,
            rotate,
            ctm->e,
            ctm->f,
            extent.x,
            extent.y,
            *text_box_id,
            content,
            styles,
            content_state
            )) goto end;
    *p = p1 - 1;
    e = 0;
    
    end:
    return e;
}


int extract_document_to_odt_content(
        extract_alloc_t*    alloc,
        document_t*         document,
        int                 spacing,
        int                 rotation,
        int                 images,
        extract_astring_t*  content,
        extract_odt_styles_t* styles
        )
{
    int ret = -1;
    int text_box_id = 0;
    int p;

    /* Write paragraphs into <content>. */
    for (p=0; p<document->pages_num; ++p)
    {
        extract_page_t* page = document->pages[p];
        int p = 0;
        int t = 0;
        content_state_t content_state;
        content_state.font.name = NULL;
        content_state.font.size = 0;
        content_state.font.bold = 0;
        content_state.font.italic = 0;
        content_state.ctm_prev = NULL;
        
        for(;;)
        {
            paragraph_t* paragraph = (p == page->paragraphs_num) ? NULL : page->paragraphs[p];
            table_t* table = (t == page->tables_num) ? NULL : page->tables[t];
            double y_paragraph;
            double y_table;
            if (!paragraph && !table)   break;
            y_paragraph = (paragraph) ? paragraph->lines[0]->spans[0]->chars[0].y : DBL_MAX;
            y_table = (table) ? table->pos.y : DBL_MAX;
            
            if (y_paragraph < y_table)
            {
                const matrix_t* ctm = &paragraph->lines[0]->spans[0]->ctm;
                double rotate = atan2(ctm->b, ctm->a);

                if (spacing
                        && content_state.ctm_prev
                        && paragraph->lines_num
                        && paragraph->lines[0]->spans_num
                        && extract_matrix_cmp4(
                                content_state.ctm_prev,
                                &paragraph->lines[0]->spans[0]->ctm
                                )
                        )
                {
                    /* Extra vertical space between paragraphs that were at
                    different angles in the original document. */
                    if (s_odt_append_empty_paragraph(alloc, content, styles)) goto end;
                }

                if (spacing)
                {
                    /* Extra vertical space between paragraphs. */
                    if (s_odt_append_empty_paragraph(alloc, content, styles)) goto end;
                }

                if (rotation && rotate != 0)
                {
                    if (s_odt_append_rotated_paragraphs(alloc, page, &content_state, &p, &text_box_id, ctm, rotate, content, styles)) goto end;
                }
                else
                {
                    if (s_document_to_odt_content_paragraph(alloc, &content_state, paragraph, content, styles)) goto end;
                }
                p += 1;
            }
            else if (table)
            {
                if (s_odt_append_table(alloc, table, content, styles)) goto end;
                t += 1;
            }
        }
        
        outf("images=%i", images);
        if (images)
        {
            int i;
            outf("page->images_num=%i", page->images_num);
            for (i=0; i<page->images_num; ++i)
            {
                s_odt_append_image(alloc, content, &page->images[i]);
            }
        }
    }
    ret = 0;

    end:

    return ret;
}


#if 0
static int s_find_mid(const char* text, const char* begin, const char* end, const char** o_begin, const char** o_end)
/* Sets *o_begin to end of first occurrence of <begin> in <text>, and *o_end to
beginning of first occurtence of <end> in <text>. */
{
    *o_begin = strstr(text, begin);
    if (!*o_begin) goto fail;
    *o_begin += strlen(begin);
    *o_end = strstr(*o_begin, end);
    if (!*o_end) goto fail;
    return 0;
    fail:
    errno = ESRCH;
    return -1;
}
#endif

int extract_odt_content_item(
        extract_alloc_t*    alloc,
        extract_astring_t*  contentss,
        int                 contentss_num,
        extract_odt_styles_t* styles,
        images_t*           images,
        const char*         name,
        const char*         text,
        char**              text2
        )
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
        if (s_odt_styles_definitions(alloc, styles, &styles_definitions)) goto end;
        
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
            image_t* image = &images->images[i];
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

        

int extract_odt_write_template(
        extract_alloc_t*    alloc,
        extract_astring_t*  contentss,
        int                 contentss_num,
        extract_odt_styles_t* styles,
        images_t*           images,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        )
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
        const char* names[] =
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
        image_t* image = &images->images[i];
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
