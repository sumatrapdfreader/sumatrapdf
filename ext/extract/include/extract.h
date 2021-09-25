#ifndef ARITFEX_EXTRACT_H
#define ARITFEX_EXTRACT_H

/* Functions for creating docx, odt, html and text files from information about
raw characters, images and graphic elements.

Unless otherwise stated, all functions return 0 on success or -1 with errno
set.
*/

#include "extract_alloc.h"
#include "extract_buffer.h"



typedef struct extract_t extract_t;
/* Abstract state for processing a document. */


typedef enum
{
    extract_format_ODT,
    extract_format_DOCX,
    extract_format_HTML,
    extract_format_TEXT
} extract_format_t;
/* Specifies the output document type.

extract_format_ODT
extract_format_DOCX
    Uses internal template ODT and DOCX documents.

extract_format_HTML:
    Uses <p> for paragraphs, and <b> and <i> for bold and italic text.

extract_format_TEXT:
    Outputs one line per paragraph, encoding text as utf8. Ligatures and and
    some unicode characters such as dash (0x2212) are converted into ascii
    equvalents.
*/


int extract_begin(
        extract_alloc_t*    alloc,
        extract_format_t    format,
        extract_t**         pextract
        );
/* Creates a new extract_t* for use by other extract_*() functions. All
allocation will be done with <alloc> (which can be NULL in which case we use
malloc/free, or from extract_alloc_create()). */


int extract_page_begin(extract_t*  extract);
/* Must be called before extract_span_begin(). */


int extract_span_begin(
        extract_t*  extract,
        const char* font_name,
        int         font_bold,
        int         font_italic,
        int         wmode,
        double      ctm_a,
        double      ctm_b,
        double      ctm_c,
        double      ctm_d,
        double      ctm_e,
        double      ctm_f,
        double      trm_a,
        double      trm_b,
        double      trm_c,
        double      trm_d,
        double      trm_e,
        double      trm_f
        );
/* Starts a new span. 
extract
    As passed to earlier call to extract_begin().
font_name
    .
font_bold
    0 or 1.
font_italic
    0 or 1.
wmode
    0 or 1.
ctm_*
    Matrix values.
trm_*
    Matrix values.
*/


int extract_add_char(
        extract_t*  extract,
        double      x,
        double      y,
        unsigned    ucs,
        double      adv,
        int         autosplit
        );
/* Appends specified character to current span.
extract
    As passed to earlier call to extract_begin().
autosplit
x
y
    Position on page.
ucs
    Unicode value.
adv
    Advance of this character.
autosplit
    Ignored as of 2021-07-02.
*/


int extract_span_end(extract_t* extract);
/* Must be called before starting a new span or ending current page. */


typedef void (*extract_image_data_free)(void* handle, void* image_data);
/* Callback for freeing image data. See extract_add_image(). */


int extract_add_image(
        extract_t*              extract,
        const char*             type,
        double                  x,
        double                  y,
        double                  w,
        double                  h,
        void*                   data,
        size_t                  data_size,
        extract_image_data_free data_free,
        void*                   data_free_handle
        );
/* Adds an image to the current page.

type
    E.g. 'png'. Is copied so no need to persist after we return.
x y w h
    Location and size of image.
data data_size
    The raw image data.
data_free
    If not NULL, extract code will call data_free(data) when it has finished
    with <data>. Otherwise the lifetime of <data> is the responsibility of the
    caller and it must persist for at least the lifetime of <extract>.
*/


int extract_add_path4(
        extract_t*  extract,
        double ctm_a,
        double ctm_b,
        double ctm_c,
        double ctm_d,
        double ctm_e,
        double ctm_f,
        double x0,
        double y0,
        double x1,
        double y1,
        double x2,
        double y2,
        double x3,
        double y3,
        double color
        );
/* Adds a four-element path. Paths that define thin vertical/horizontal
rectangles are used to find tables. */


int extract_add_line(
        extract_t*  extract,
        double ctm_a,
        double ctm_b,
        double ctm_c,
        double ctm_d,
        double ctm_e,
        double ctm_f,
        double width,
        double x0,
        double y0,
        double x1,
        double y1,
        double color
        );
/* Adds a stroked line. Vertical/horizontal lines are used to find tables. */


int extract_fill_begin(
        extract_t*  extract,
        double ctm_a,
        double ctm_b,
        double ctm_c,
        double ctm_d,
        double ctm_e,
        double ctm_f,
        double color
        );
/* Alternative to extract_add_path4(). Should be followed by calls to
extract_moveto(), extract_lineto() and extract_closepath(), which are used
to find lines that may define tables. extract_fill_end() should be called
afterwards. */

int extract_stroke_begin(
        extract_t*  extract,
        double ctm_a,
        double ctm_b,
        double ctm_c,
        double ctm_d,
        double ctm_e,
        double ctm_f,
        double line_width,
        double color
        );
/* Alternative to extract_add_line(). Should be followed by calls to
extract_moveto(), extract_lineto() and extract_closepath(), which are used
to find lines that may define tables. extract_fill_end() should be called
afterwards. */

int extract_moveto(extract_t* extract, double x, double y);

int extract_lineto(extract_t* extract, double x, double y);

int extract_closepath(extract_t* extract);

int extract_fill_end(extract_t* extract);

int extract_stroke_end(extract_t* extract);


int extract_page_end(extract_t* extract);
/* Must be called to finish page started by extract_page_begin(). */


int extract_process(
        extract_t*  extract,
        int         spacing,
        int         rotation,
        int         images
        );
/* Evaluates all un-processed pages to generate output data and frees internal
page data (individual spans, lines, paragraphs etc). E.g. call this after
extract_page_end() to reduce internal data use. */


int extract_write(extract_t* extract, extract_buffer_t* buffer);
/* Writes output document to buffer.

For docx and odt, uses an internal template document. */


int extract_write_content(extract_t* extract, extract_buffer_t* buffer);
/* Writes paragraph content only into buffer.

For docx and odt, this is the xml containing paragraphs of text that is
inserted into the template word/document.xml object within the docx/odt zip
archive by extract_write()). */


int extract_write_template(
        extract_t*  extract, 
        const char* path_template,
        const char* path_out,
        int         preserve_dir
        );
/* Like extract_write() but uses a provided template document. Only works with
docx and odt output.

Uses the 'zip' and 'unzip' commands internally.

extract:
    .
path_template:
    Name of docx/odt file to use as a template.
path_out:
    Name of docx/odt file to create. Must not contain single-quote, double
    quote, space or ".." sequence - these will force EINVAL error because they
    could make internal shell commands unsafe.
preserve_dir:
    If true, we don't delete the temporary directory <path_out>.dir containing
    the output document contents prior to zipping.
*/


void extract_end( extract_t** pextract);
/* Frees all data associated with *pextract and sets *pextract to NULL. */


/* Things below are not generally used. */

int extract_tables_csv_format(extract_t* extract, const char* path_format);
/* Causes extract_process() to also write each tableas CSV to a file with path
asprintf(path_format, n) where <n> is the table number, starting from 0. */

int extract_read_intermediate(
        extract_t*          extract,
        extract_buffer_t*   buffer,
        int                 autosplit
        );
/* Reads XML specification of spans and images from <buffer> and adds to
<extract>.

(Makes internal calls to extract_span_begin(), extract_add_image() etc.) */


void extract_internal_end(void);
/* Cleans up internal singelton state that can look like a memory leak when
running under Memento or valgrind. */


void extract_exp_min(extract_t* extract, size_t size);
/* If size is non-zero, sets minimum actual allocation size, and we only
allocate in powers of two times this size. This is an attempt to improve speed
with memento squeeze. Default is 0 (every call to extract_realloc() calls
realloc(). */

#endif
