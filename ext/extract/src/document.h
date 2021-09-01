#ifndef ARTIFEX_EXTRACT_DOCUMENT_H
#define ARTIFEX_EXTRACT_DOCUMENT_H

static const double pi = 3.141592653589793;

typedef struct
{
    double x;
    double y;
} point_t;

typedef struct
{
    double  a;
    double  b;
    double  c;
    double  d;
    double  e;
    double  f;
} matrix_t;

double matrix_expansion(matrix_t m);

int matrix_cmp4(const matrix_t* lhs, const matrix_t* rhs)
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
    
    /* font size is matrix_expansion(trm). */
    
    struct {
        unsigned font_bold      : 1;
        unsigned font_italic    : 1;
        unsigned wmode          : 1;
    } flags;
    
    char_t*     chars;
    int         chars_num;
} span_t;
/* List of chars that have same font and are usually adjacent. */

char_t* span_char_last(span_t* span);
/* Returns last character in span. */

int span_append_c(extract_alloc_t* alloc, span_t* span, int c);
/* Appends new char_t to an span_t with .ucs=c and all other
fields zeroed. */

const char* span_string(extract_alloc_t* alloc, span_t* span);
/* Returns static string containing info about span_t. */

typedef struct
{
    span_t**    spans;
    int         spans_num;
} line_t;
/* List of spans that are aligned on same line. */

span_t* line_span_first(line_t* line);
/* Returns first span in a line. */

span_t* line_span_last(line_t* line);
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

} extract_page_t;
/* A page. Contains different representations of the list of spans. NB not
called page_t because this clashes with a system type on hpux. */

typedef struct
{
    extract_page_t**    pages;
    int         pages_num;
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

double extract_matrices_to_font_size(matrix_t* ctm, matrix_t* trm);


#endif
