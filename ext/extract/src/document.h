#ifndef ARTIFEX_EXTRACT_DOCUMENT_H
#define ARTIFEX_EXTRACT_DOCUMENT_H

#include "../include/extract.h"

#ifdef _MSC_VER
    #include "compat_stdint.h"
#else
    #include <stdint.h>
#endif


static const double pi = 3.141592653589793;

typedef struct
{
    double x;
    double y;
} point_t;

const char* extract_point_string(const point_t* point);

typedef struct
{
    point_t min;
    point_t max;
} rect_t;

const char* extract_rect_string(const rect_t* rect);

typedef struct
{
    double  a;
    double  b;
    double  c;
    double  d;
    double  e;
    double  f;
} matrix_t;

const char* extract_matrix_string(const matrix_t* matrix);

double      extract_matrix_expansion(matrix_t m);
/* Returns a*d - b*c. */

point_t     extract_multiply_matrix_point(matrix_t m, point_t p);
matrix_t    extract_multiply_matrix_matrix(matrix_t m1, matrix_t m2);

int extract_matrix_cmp4(const matrix_t* lhs, const matrix_t* rhs)
;
/* Returns zero if first four members of *lhs and *rhs are equal, otherwise
+/-1. */

typedef struct
{
    /* (x,y) before transformation by ctm and trm. */
    double      pre_x;
    double      pre_y;
    
    /* (x,y) after transformation by ctm and trm. */
    double      x;
    double      y;
    
    unsigned    ucs;
    double      adv;
} char_t;
/* A single char in a span.
*/

typedef struct
{
    matrix_t    ctm;
    matrix_t    trm;
    char*       font_name;
    
    /* font size is extract_matrix_cmp4(trm). */
    
    struct {
        unsigned font_bold      : 1;
        unsigned font_italic    : 1;
        unsigned wmode          : 1;
    } flags;
    
    char_t*     chars;
    int         chars_num;
} span_t;
/* List of chars that have same font and are usually adjacent. */

void extract_span_init(span_t* span);

void extract_span_free(extract_alloc_t* alloc, span_t** pspan);
/* Frees a span_t, returning with *pspan set to NULL. */

void extract_spans_free(extract_alloc_t* alloc, span_t*** pspans, int spans_num);

char_t* extract_span_char_last(span_t* span);
/* Returns last character in span. */

int extract_span_append_c(extract_alloc_t* alloc, span_t* span, int c);
/* Appends new char_t to an span_t with .ucs=c and all other
fields zeroed. */

const char* extract_span_string(extract_alloc_t* alloc, span_t* span);
/* Returns static string containing info about span_t. */

typedef struct
{
    span_t**    spans;
    int         spans_num;
} line_t;
/* List of spans that are aligned on same line. */

void extract_line_free(extract_alloc_t* alloc, line_t** pline);
void extract_lines_free(extract_alloc_t* alloc, line_t*** plines, int lines_num);

span_t* extract_line_span_first(line_t* line);
/* Returns first span in a line. */

span_t* extract_line_span_last(line_t* line);
/* Returns last span in a line. */

typedef struct
{
    line_t**    lines;
    int         lines_num;
} paragraph_t;
/* List of lines that are aligned and adjacent to each other so as to form a
paragraph. */

typedef struct
{
    char*   type;   /* jpg, png etc. */
    char*   name;   /* Name of image file within docx. */
    char*   id;     /* ID of image within docx. */
    double  x;
    double  y;
    double  w;
    double  h;
    void*   data;
    size_t  data_size;
    
    extract_image_data_free data_free;
    void*                   data_free_handle;
    
} image_t;
/* Information about an image. <type> is as passed to extract_add_image();
<name> and <id> are created to be unique identifiers for use in generated docx
file. */

void extract_image_clear(extract_alloc_t* alloc, image_t* image);

typedef struct
{
    float   color;
    rect_t  rect;
} tableline_t;
/* A line that is part of a table. */

typedef struct
{
    tableline_t*    tablelines;
    int             tablelines_num;
} tablelines_t;


typedef struct
{
    rect_t          rect;
    
    /* If left/above is true, this cell is not obscured by cell to its
    left/above. */
    uint8_t         left;
    uint8_t         above;
    
    /* extend_right and extend_down are 1 for normal cells, 2 for cells which
    extend right/down to cover an additional column/row, 3 to cover two
    additional columns/rows etc. */
    int             extend_right;
    int             extend_down;
    
    /* Contents of this cell. */
    line_t**        lines;
    int             lines_num;
    paragraph_t**   paragraphs;
    int             paragraphs_num;
} cell_t;
/* A cell within a table. */

void extract_cell_init(cell_t* cell);
void extract_cell_free(extract_alloc_t* alloc, cell_t** pcell);

typedef struct
{
    point_t     pos;    /* top-left. */
    
    /* Array of cells_num_x*cells_num_y cells; cell (x, y) is:
        cells_num_x * y + x.
    */
    cell_t**    cells;
    int         cells_num_x;
    int         cells_num_y;
} table_t;


typedef struct
{
    span_t**    spans;
    int         spans_num;
    
    image_t*    images;
    int         images_num;

    line_t**    lines;
    int         lines_num;
    /* These refer to items in .spans. Initially empty, then set by
    extract_join(). */

    paragraph_t**   paragraphs;
    int             paragraphs_num;
    /* These refer to items in .lines. Initially empty, then set
    by extract_join(). */
    
    tablelines_t    tablelines_horizontal;
    tablelines_t    tablelines_vertical;
    
    table_t**   tables;
    int         tables_num;

} extract_page_t;
/* A page. Contains different representations of the list of spans. NB not
+called page_t because this clashes with a system type on hpux. */


typedef struct
{
    extract_page_t**    pages;
    int                 pages_num;
} document_t;
/* A list of pages. */


typedef struct
{
    image_t*    images;
    int         images_num;
    char**      imagetypes;
    int         imagetypes_num;
} images_t;


int extract_document_join(extract_alloc_t* alloc, document_t* document);
/* This does all the work of finding paragraphs and tables. */

double extract_matrices_to_font_size(matrix_t* ctm, matrix_t* trm);

/* Things below here are used when generating output. */

typedef struct
{
    char*   name;
    double  size;
    int     bold;
    int     italic;
} font_t;
/* Basic information about current font. */

typedef struct
{
    font_t      font;
    matrix_t*   ctm_prev;
} content_state_t;
/* Used to keep track of font information when writing paragraphs of odt
content, e.g. so we know whether a font has changed so need to start a new odt
span. */


#endif
