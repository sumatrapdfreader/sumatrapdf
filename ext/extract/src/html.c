/* These extract_html_*() functions generate docx content and docx zip archive
data.

Caller must call things in a sensible order to create valid content -
e.g. don't call docx_paragraph_start() twice without intervening call to
docx_paragraph_finish(). */

#include "../include/extract.h"

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


static void content_state_init(content_state_t* content_state)
{
    content_state->font.name = NULL;
    content_state->font.size = 0;
    content_state->font.bold = 0;
    content_state->font.italic = 0;
    content_state->ctm_prev = NULL;
}

static int content_state_reset(extract_alloc_t* alloc, content_state_t* content_state, extract_astring_t* content)
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

static int paragraph_to_html_content(
        extract_alloc_t*    alloc,
        content_state_t*    content_state,
        paragraph_t*        paragraph,
        int                 single_line,
        extract_astring_t*  content
        )
{
    int e = -1;
    char* endl = (single_line) ? "" : "\n";
    int l;
    if (extract_astring_catf(alloc, content, "%s%s<p>", endl, endl)) goto end;

    for (l=0; l<paragraph->lines_num; ++l)
    {
        line_t* line = paragraph->lines[l];
        int s;
        for (s=0; s<line->spans_num; ++s)
        {
            int c;
            span_t* span = line->spans[s];
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

        if (content->chars_num && l+1 < paragraph->lines_num)
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


static int paragraphs_to_html_content(
        extract_alloc_t*    alloc,
        content_state_t*    state,
        paragraph_t**       paragraphs,
        int                 paragraphs_num,
        int                 single_line,
        extract_astring_t*  content
        )
/* Append html for paragraphs[] to <content>. Updates *state if we change font
etc. */
{
    int e = -1;
    int p;
    for (p=0; p<paragraphs_num; ++p)
    {
        paragraph_t* paragraph = paragraphs[p];
        if (paragraph_to_html_content(alloc, state, paragraph, single_line, content)) goto end;
    }
    
    if (content_state_reset(alloc, state, content)) goto end;
    e = 0;
    
    end:
    return e;
}

static int append_table(extract_alloc_t* alloc, content_state_t* state, table_t* table, extract_astring_t* content)
{
    int e = -1;
    int y;
    
    if (extract_astring_cat(alloc, content, "\n\n<table border=\"1\" style=\"border-collapse:collapse\">\n")) goto end;
    
    for (y=0; y<table->cells_num_y; ++y)
    {
        /* If 1, we put each <td>...</td> on a separate line. */
        int multiline = 0;
        int x;
        if (extract_astring_cat(alloc, content, "    <tr>\n")) goto end;
        if (!multiline)
        {
            if (extract_astring_cat(alloc, content, "        ")) goto end;
        }
        for (x=0; x<table->cells_num_x; ++x)
        {
            cell_t* cell = table->cells[y*table->cells_num_x + x];
            if (!cell->above || !cell->left)
            {
                /* HTML does not require anything for cells that are subsumed
                by other cells that extend horizontally and vertically. */
                continue;
            }
            if (multiline)
            {
                if (extract_astring_cat(alloc, content, "        ")) goto end;
            }
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

            if (paragraphs_to_html_content(alloc, state, cell->paragraphs, cell->paragraphs_num, 1 /* single_line*/, content)) goto end;
            if (extract_astring_cat(alloc, content, "</td>")) goto end;
            if (multiline)
            {
                if (extract_astring_cat(alloc, content, "\n")) goto end;
            }

            if (content_state_reset(alloc, state, content)) goto end;
        }
        if (!multiline)
        {
            if (extract_astring_cat(alloc, content, "\n")) goto end;
        }
        if (extract_astring_cat(alloc, content, "    </tr>\n")) goto end;
    }
    if (extract_astring_cat(alloc, content, "</table>\n\n")) goto end;
    e = 0;
    
    end:
    return e;
}


static char_t* paragraph_first_char(const paragraph_t* paragraph)
{
    line_t* line = paragraph->lines[paragraph->lines_num - 1];
    span_t* span = line->spans[line->spans_num - 1];
    return &span->chars[0];
}

static int compare_paragraph_y(const void* a, const void* b)
{
    const paragraph_t* const* a_paragraph = a;
    const paragraph_t* const* b_paragraph = b;
    double a_y = paragraph_first_char(*a_paragraph)->y;
    double b_y = paragraph_first_char(*b_paragraph)->y;
    if (a_y > b_y)  return +1;
    if (a_y < b_y)  return -1;
    return 0;
}

int extract_document_to_html_content(
        extract_alloc_t*    alloc,
        document_t*         document,
        int                 rotation,
        int                 images,
        extract_astring_t*  content
        )
{
    int ret = -1;
    int p;
    paragraph_t** paragraphs = NULL;
    
    (void) rotation;
    (void) images;
    
    extract_astring_cat(alloc, content, "<html>\n");
    extract_astring_cat(alloc, content, "<body>\n");
    
    /* Write paragraphs into <content>. */
    for (p=0; p<document->pages_num; ++p)
    {
        extract_page_t* page = document->pages[p];
        int p;
        int t;
        content_state_t state;
        content_state_init(&state);
        extract_free(alloc, &paragraphs);
        
        /* Output paragraphs and tables in order of increasing <y> coordinate.

        Unfortunately the paragraph ordering we do in page->paragraphs[]
        isn't quite right and results in bad ordering if ctm/trm matrices are
        inconsistent. So we create our own list of paragraphs sorted strictly
        by y coordinate of the first char of each paragraph. */
        if (extract_malloc(alloc, &paragraphs, sizeof(*paragraphs) * page->paragraphs_num)) goto end;
        for (p = 0; p < page->paragraphs_num; ++p)
        {
            paragraphs[p] = page->paragraphs[p];
        }
        qsort(paragraphs, page->paragraphs_num, sizeof(*paragraphs), compare_paragraph_y);
        
        if (0)
        {
            int p;
            outf0("paragraphs are:");
            for (p=0; p<page->paragraphs_num; ++p)
            {
                paragraph_t* paragraph = page->paragraphs[p];
                line_t* line = paragraph->lines[0];
                span_t* span = line->spans[0];
                outf0("    p=%i: %s", p, extract_span_string(NULL, span));
            }
        }

        p = 0;
        t = 0;        
        for(;;)
        {
            double y_paragraph;
            double y_table;
            paragraph_t* paragraph = (p == page->paragraphs_num) ? NULL : paragraphs[p];
            table_t* table = (t == page->tables_num) ? NULL : page->tables[t];
            if (!paragraph && !table) break;
            y_paragraph = (paragraph) ? paragraph->lines[0]->spans[0]->chars[0].y : DBL_MAX;
            y_table = (table) ? table->pos.y : DBL_MAX;
            outf("p=%i y_paragraph=%f", p, y_paragraph);
            outf("t=%i y_table=%f", t, y_table);
            if (paragraph && y_paragraph < y_table)
            {
                //extract_astring_catf(alloc, content, "<p>@@@ paragraph %i y=%f @@@)</p>\n", p, y_paragraph);
                if (paragraph_to_html_content(alloc, &state, paragraph, 0 /*single_line*/, content)) goto end;
                if (content_state_reset(alloc, &state, content)) goto end;
                p += 1;
            }
            else if (table)
            {
                //extract_astring_catf(alloc, content, "<p>@@@ table %t y=%f @@@)</p>\n", p, y_table);
                if (append_table(alloc, &state, table, content)) goto end;
                t += 1;
            }
        }
    }
    extract_astring_cat(alloc, content, "</body>\n");
    extract_astring_cat(alloc, content, "</html>\n");
    ret = 0;

    end:
    extract_free(alloc, &paragraphs);
    return ret;
}
