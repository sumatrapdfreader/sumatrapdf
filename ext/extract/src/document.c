#include "document.h"
#include "outf.h"


void extract_span_init(span_t* span)
{
    span->font_name = NULL;
    span->chars = NULL;
    span->chars_num = 0;
}

void extract_span_free(extract_alloc_t* alloc, span_t** pspan)
{
    if (!*pspan) return;
    extract_free(alloc, &(*pspan)->font_name);
    extract_free(alloc, &(*pspan)->chars);
    extract_free(alloc, pspan);
}

void extract_spans_free(extract_alloc_t* alloc, span_t*** pspans, int spans_num)
{
    span_t** spans = *pspans;
    int s;
    for (s=0; s<spans_num; ++s)
    {
        extract_span_free(alloc, &spans[s]);
    }
    extract_free(alloc, pspans);
}

void extract_line_free(extract_alloc_t* alloc, line_t** pline)
{
    line_t* line = *pline;
    int s;
    for (s=0; s<line->spans_num; ++s)
    {
        extract_span_free(alloc, &line->spans[s]);
    }
    extract_free(alloc, &line->spans);
    extract_free(alloc, pline);
}

void extract_lines_free(extract_alloc_t* alloc, line_t*** plines, int lines_num)
{
    int l;
    line_t** lines = *plines;
    for (l=0; l<lines_num; ++l)
    {
        extract_line_free(alloc, &lines[l]);
    }
    extract_free(alloc, plines);
}

void extract_image_clear(extract_alloc_t* alloc, image_t* image)
{
    extract_free(alloc, &image->type);
    extract_free(alloc, &image->name);
    extract_free(alloc, &image->id);
    if (image->data_free) {
        image->data_free(image->data_free_handle, image->data);
    }
}

void extract_cell_free(extract_alloc_t* alloc, cell_t** pcell)
{
    int p;
    cell_t* cell = *pcell;
    if (!cell) return;
    
    outf("cell->lines_num=%i", cell->lines_num);
    outf("cell->paragraphs_num=%i", cell->paragraphs_num);
    extract_lines_free(alloc, &cell->lines, cell->lines_num);
    
    outf("cell=%p cell->paragraphs_num=%i", cell, cell->paragraphs_num);
    for (p=0; p<cell->paragraphs_num; ++p)
    {
        paragraph_t* paragraph = cell->paragraphs[p];
        outf("paragraph->lines_num=%i", paragraph->lines_num);
        /* We don't attempt to free paragraph->lines[] because they point into
        cell->lines which are already freed. */
        extract_free(alloc, &paragraph->lines);
        extract_free(alloc, &cell->paragraphs[p]);
    }
    extract_free(alloc, &cell->paragraphs);
    extract_free(alloc, pcell);
}


