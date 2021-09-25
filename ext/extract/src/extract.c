#include "../include/extract.h"
#include "../include/extract_alloc.h"

#include "astring.h"
#include "document.h"
#include "docx.h"
#include "docx_template.h"
#include "html.h"
#include "mem.h"
#include "memento.h"
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




double extract_matrix_expansion(matrix_t m)
{
    return sqrt(fabs(m.a * m.d - m.b * m.c));
}


static void char_init(char_t* item)
{
    item->pre_x = 0;
    item->pre_y = 0;
    item->x = 0;
    item->y = 0;
    item->ucs = 0;
    item->adv = 0;
}

const char* extract_point_string(const point_t* point)
{
    static char buffer[128];
    snprintf(buffer, sizeof(buffer), "(%f %f)", point->x, point->y);
    return buffer;
}

const char* extract_rect_string(const rect_t* rect)
{
    static char buffer[2][256];
    static int i = 0;
    i = (i + 1) % 2;
    snprintf(buffer[i], sizeof(buffer[i]), "((%f %f) (%f %f))", rect->min.x, rect->min.y, rect->max.x, rect->max.y);
    return buffer[i];
}

const char* extract_span_string(extract_alloc_t* alloc, span_t* span)
{
    static extract_astring_t ret = {0};
    double x0 = 0;
    double y0 = 0;
    point_t pre0 = {0, 0};
    double x1 = 0;
    double y1 = 0;
    point_t pre1 = {0, 0};
    int c0 = 0;
    int c1 = 0;
    int i;
    extract_astring_free(alloc, &ret);
    if (!span) {
        /* This frees our internal data, and is used by extract_internal_end().
        */
        return NULL;
    }
    if (span->chars_num) {
        c0 = span->chars[0].ucs;
        x0 = span->chars[0].x;
        y0 = span->chars[0].y;
        pre0.x = span->chars[0].pre_x;
        pre0.y = span->chars[0].pre_y;
        c1 = span->chars[span->chars_num-1].ucs;
        x1 = span->chars[span->chars_num-1].x;
        y1 = span->chars[span->chars_num-1].y;
        pre1.x = span->chars[span->chars_num-1].pre_x;
        pre1.y = span->chars[span->chars_num-1].pre_y;
    }
    {
        char buffer[400];
        snprintf(buffer, sizeof(buffer),
                "span ctm=%s trm=%s chars_num=%i (%c:%f,%f pre(%f %f))..(%c:%f,%f pre(%f %f)) font=%s:(%f,%f) wmode=%i chars_num=%i: ",
                extract_matrix_string(&span->ctm),
                extract_matrix_string(&span->trm),
                span->chars_num,
                c0, x0, y0, pre0.x, pre0.y,
                c1, x1, y1, pre1.x, pre1.y,
                span->font_name,
                span->trm.a,
                span->trm.d,
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
    for (i=0; i<span->chars_num; ++i) {
        extract_astring_catc(alloc, &ret, (char) span->chars[i].ucs);
    }
    extract_astring_catc(alloc, &ret, '"');
    return ret.chars;
}

int extract_span_append_c(extract_alloc_t* alloc, span_t* span, int c)
{
    char_t* item;
    if (extract_realloc2(
            alloc,
            &span->chars,
            sizeof(*span->chars) * span->chars_num,
            sizeof(*span->chars) * (span->chars_num + 1)
            )) {
        return -1;
    }
    item = &span->chars[span->chars_num];
    span->chars_num += 1;
    char_init(item);
    item->ucs = c;
    return 0;
}

char_t* extract_span_char_last(span_t* span)
{
    assert(span->chars_num > 0);
    return &span->chars[span->chars_num-1];
}

/* Unused but useful to keep code here. */
#if 0
/* Returns static string containing info about line_t. */
static const char* line_string(line_t* line)
{
    static extract_astring_t ret = {0};
    char    buffer[32];
    extract_astring_free(&ret);
    snprintf(buffer, sizeof(buffer), "line spans_num=%i:", line->spans_num);
    extract_astring_cat(&ret, buffer);
    int i;
    for (i=0; i<line->spans_num; ++i) {
        extract_astring_cat(&ret, " ");
        extract_astring_cat(&ret, extract_span_string(line->spans[i]));
    }
    return ret.chars;
}
#endif

/* Returns first span in a line. */
span_t* extract_line_span_last(line_t* line)
{
    assert(line->spans_num > 0);
    return line->spans[line->spans_num - 1];
}

span_t* extract_line_span_first(line_t* line)
{
    assert(line->spans_num > 0);
    return line->spans[0];
}


static void table_free(extract_alloc_t* alloc, table_t** ptable)
{
    int c;
    table_t* table = *ptable;
    outf("table->cells_num_x=%i table->cells_num_y=%i",
            table->cells_num_x,
            table->cells_num_y
            );
    for (c = 0;  c< table->cells_num_x * table->cells_num_y; ++c)
    {
        extract_cell_free(alloc, &table->cells[c]);
    }
    extract_free(alloc, &table->cells);
    extract_free(alloc, ptable);
}

static void page_free(extract_alloc_t* alloc, extract_page_t** ppage)
{
    extract_page_t* page = *ppage;
    if (!page) return;

    outf0("page=%p page->spans_num=%i page->lines_num=%i",
            page, page->spans_num, page->lines_num);
    extract_spans_free(alloc, &page->spans, page->spans_num);

    extract_lines_free(alloc, &page->lines, page->lines_num);

    {
        int p;
        for (p=0; p<page->paragraphs_num; ++p) {
            paragraph_t* paragraph = page->paragraphs[p];
            /* We don't call extract_lines_free(&paragraph->lines) because
            these point into the same data as page->lines, which we have
            already freed above. */
            if (paragraph) extract_free(alloc, &paragraph->lines);
            extract_free(alloc, &page->paragraphs[p]);
        }
    }
    extract_free(alloc, &page->paragraphs);
    
    {
        int i;
        for (i=0; i<page->images_num; ++i) {
            extract_image_clear(alloc, &page->images[i]);
        }
        extract_free(alloc, &page->images);
    }
    extract_free(alloc, &page->images);

    extract_free(alloc, &page->tablelines_horizontal.tablelines);
    extract_free(alloc, &page->tablelines_vertical.tablelines);
    
    {
        int t;
        outf("page=%p page->tables_num=%i", page, page->tables_num);
        for (t=0; t<page->tables_num; ++t)
        {
            table_free(alloc, &page->tables[t]);
        }
        extract_free(alloc, &page->tables);
    }
    
    extract_free(alloc, ppage);
}

static span_t* page_span_append(extract_alloc_t* alloc, extract_page_t* page)
/* Appends new empty span_ to an extract_page_t; returns NULL with errno set on
error. */
{
    span_t* span;
    if (extract_malloc(alloc, &span, sizeof(*span))) return NULL;
    extract_span_init(span);
    if (extract_realloc2(
            alloc,
            &page->spans,
            sizeof(*page->spans) * page->spans_num,
            sizeof(*page->spans) * (page->spans_num + 1)
            )) {
        extract_free(alloc, &span);
        return NULL;
    }
    page->spans[page->spans_num] = span;
    page->spans_num += 1;
    return span;
}


static void extract_images_free(extract_alloc_t* alloc, images_t* images)
{
    int i;
    for (i=0; i<images->images_num; ++i) {
        extract_image_clear(alloc, &images->images[i]);
    }
    extract_free(alloc, &images->images);
    extract_free(alloc, &images->imagetypes);
    images->images_num = 0;
    images->imagetypes_num = 0;
}


static int extract_document_images(extract_alloc_t* alloc, document_t* document, images_t* o_images)
/* Moves image_t's from document->page[] to *o_images.

On return document->page[].images* will be NULL etc.
*/
{
    int e = -1;
    int p;
    images_t   images = {0};
    outf("extract_document_images(): images.images_num=%i", images.images_num);
    for (p=0; p<document->pages_num; ++p)
    {
        extract_page_t* page = document->pages[p];
        int i;
        for (i=0; i<page->images_num; ++i)
        {
            image_t* image;
            if (extract_realloc2(
                    alloc,
                    &images.images,
                    sizeof(image_t) * images.images_num,
                    sizeof(image_t) * (images.images_num + 1)
                    )) goto end;
            image = &page->images[i];
            outf("p=%i i=%i image->name=%s image->id=%s", p, i, image->name, image->id);
            assert(image->name);
            images.images[images.images_num] = *image;
            images.images_num += 1;
            
            /* Add image type if we haven't seen it before. */
            {
                int it;
                for (it=0; it<images.imagetypes_num; ++it)
                {
                    outf("it=%i images.imagetypes[it]=%s image->type=%s",
                            it, images.imagetypes[it], image->type);
                    if (!strcmp(images.imagetypes[it], image->type)) {
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
            
            /* We've taken ownership of image->* so NULL the original values
            here to ensure we can't use things after free. */
            image->type = NULL;
            image->name = NULL;
            image->id = NULL;
            image->data = NULL;
            image->data_size = 0;
        }
        extract_free(alloc, &page->images);
        page->images_num = 0;
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

static void extract_document_free(extract_alloc_t* alloc, document_t* document)
{
    int p;
    if (!document) {
        return;
    }
    for (p=0; p<document->pages_num; ++p) {
        extract_page_t* page = document->pages[p];
        page_free(alloc, &page);
    }
    extract_free(alloc, &document->pages);
    document->pages = NULL;
    document->pages_num = 0;
}


/* Returns +1, 0 or -1 depending on sign of x. */
static int s_sign(double x)
{
    if (x < 0)  return -1;
    if (x > 0)  return +1;
    return 0;
}

int extract_matrix_cmp4(const matrix_t* lhs, const matrix_t* rhs)
{
    int ret;
    ret = s_sign(lhs->a - rhs->a);  if (ret) return ret;
    ret = s_sign(lhs->b - rhs->b);  if (ret) return ret;
    ret = s_sign(lhs->c - rhs->c);  if (ret) return ret;
    ret = s_sign(lhs->d - rhs->d);  if (ret) return ret;
    return 0;
}


point_t extract_multiply_matrix_point(matrix_t m, point_t p)
{
    double x = p.x;
    p.x = m.a * x + m.c * p.y;
    p.y = m.b * x + m.d * p.y;
    return p;
}

matrix_t extract_multiply_matrix_matrix(matrix_t m1, matrix_t m2)
{
    matrix_t ret;
    ret.a = m1.a * m2.a + m1.b * m2.c;
    ret.b = m1.a * m2.b + m1.b * m2.d;
    ret.c = m1.c * m2.a + m1.d * m2.c;
    ret.d = m1.c * m2.b + m1.d * m2.d;
    ret.e = m1.e + m2.e;
    ret.f = m1.f + m2.f;
    return ret;
}

static int s_matrix_read(const char* text, matrix_t* matrix)
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
            &matrix->f
            );
    if (n != 6) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}


static void s_document_init(document_t* document)
{
    document->pages = NULL;
    document->pages_num = 0;
}


static int page_span_end_clean(extract_alloc_t* alloc, extract_page_t* page)
/* Does preliminary processing of the end of the last span in a page; intended
to be called as we load span information.

Looks at last two char_t's in last span_t of <page>, and either
leaves unchanged, or removes space in last-but-one position, or moves last
char_t into a new span_t. */
{
    int ret = -1;
    span_t* span;
    char_t* char_;
    double font_size;
    double x;
    double y;
    double err_x;
    double err_y;
    point_t dir;
    
    assert(page->spans_num);
    span = page->spans[page->spans_num-1];
    assert(span->chars_num);

    /* Last two char_t's are char_[-2] and char_[-1]. */
    char_ = &span->chars[span->chars_num];

    if (span->chars_num == 1) {
        return 0;
    }

    font_size = extract_matrix_expansion(span->trm)
            * extract_matrix_expansion(span->ctm);

    if (span->flags.wmode) {
        dir.x = 0;
        dir.y = 1;
    }
    else {
        dir.x = 1;
        dir.y = 0;
    }
    dir = extract_multiply_matrix_point(span->trm, dir);

    x = char_[-2].pre_x + char_[-2].adv * dir.x;
    y = char_[-2].pre_y + char_[-2].adv * dir.y;

    err_x = (char_[-1].pre_x - x) / font_size;
    err_y = (char_[-1].pre_y - y) / font_size;

    if (span->chars_num >= 2 && span->chars[span->chars_num-2].ucs == ' ') {
        int remove_penultimate_space = 0;
        if (err_x < -span->chars[span->chars_num-2].adv / 2
                && err_x > -span->chars[span->chars_num-2].adv
                ) {
            remove_penultimate_space = 1;
        }
        if ((char_[-1].pre_x - char_[-2].pre_x) / font_size < char_[-1].adv / 10) {
            outfx(
                    "removing penultimate space because space very narrow:"
                    "char_[-1].pre_x-char_[-2].pre_x=%f font_size=%f"
                    " char_[-1].adv=%f",
                    char_[-1].pre_x - char_[-2].pre_x,
                    font_size,
                    char_[-1].adv
                    );
            remove_penultimate_space = 1;
        }
        if (remove_penultimate_space) {
            /* This character overlaps with previous space
            character. We discard previous space character - these
            sometimes seem to appear in the middle of words for some
            reason. */
            outfx("removing space before final char in: %s",
                    extract_span_string(span));
            span->chars[span->chars_num-2] = span->chars[span->chars_num-1];
            span->chars_num -= 1;
            outfx("span is now:                         %s", extract_span_string(span));
            return 0;
        }
    }
    else if (fabs(err_x) > 0.01 || fabs(err_y) > 0.01) {
        /* This character doesn't seem to be a continuation of
        previous characters, so split into two spans. This often
        splits text incorrectly, but this is corrected later when
        we join spans into lines. */
        outfx(
                "Splitting last char into new span. font_size=%f dir.x=%f"
                " char[-1].pre=(%f, %f) err=(%f, %f): %s",
                font_size,
                dir.x,
                char_[-1].pre_x,
                char_[-1].pre_y,
                err_x,
                err_y,
                span_string2(span)
                );
        {
            span_t* span2 = page_span_append(alloc, page);
            if (!span2) goto end;
            *span2 = *span;
            if (extract_strdup(alloc, span->font_name, &span2->font_name)) goto end;
            span2->chars_num = 1;
            if (extract_malloc(alloc, &span2->chars, sizeof(char_t) * span2->chars_num)) goto end;
            span2->chars[0] = char_[-1];
            span->chars_num -= 1;
        }
        return 0;
    }
    ret = 0;
    end:
    return ret;
}


struct extract_t
{
    extract_alloc_t*    alloc;
    
    document_t          document;
    
    int                 num_spans_split;
    /* Number of extra spans from page_span_end_clean(). */
    
    int                 num_spans_autosplit;
    /* Number of extra spans from autosplit=1. */
    
    double              span_offset_x;
    double              span_offset_y;
    /* Only used if autosplit is non-zero. */
    
    int                 image_n;
    /* Used to generate unique ids for images. */
    
    /* List of strings that are the generated docx content for each page. When
    zip_* can handle appending of data, we will be able to remove this list. */
    extract_astring_t*  contentss;
    int                 contentss_num;
    
    images_t            images;
    
    extract_format_t    format;
    extract_odt_styles_t odt_styles;
    
    char*               tables_csv_format;
    int                 tables_csv_i;
    
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
            matrix_t    ctm;
            double      color;
            point_t     points[4];
            int         n;
        } fill;
        
        struct
        {
            matrix_t    ctm;
            double      color;
            double      width;
            point_t     point0;
            int         point0_set;
            point_t     point;
            int         point_set;
        } stroke;
    
    } path;
};


int extract_begin(
        extract_alloc_t*    alloc,
        extract_format_t    format,
        extract_t**         pextract
        )
{
    int e = -1;
    extract_t*  extract;
    
    if (1
            && format != extract_format_ODT
            && format != extract_format_DOCX
            && format != extract_format_HTML
            && format != extract_format_TEXT
            )
    {
        outf0("Invalid format=%i\n", format);
        errno = EINVAL;
        return -1;
    }
    
    /* Use a temporary extract_alloc_t to allocate space for the extract_t. */
    if (extract_malloc(alloc, &extract, sizeof(*extract))) goto end;
    
    extract_bzero(extract, sizeof(*extract));
    extract->alloc = alloc;
    s_document_init(&extract->document);
    
    /* Start at 10 because template document might use some low-numbered IDs.
    */
    extract->image_n = 10;
    
    extract->format = format;
    extract->tables_csv_format = NULL;
    extract->tables_csv_i = 0;
    
    e = 0;
    
    end:
    *pextract = (e) ? NULL : extract;
    return e;
}

int extract_tables_csv_format(extract_t* extract, const char* path_format)
{
    return extract_strdup(extract->alloc, path_format, &extract->tables_csv_format);
}


static void image_free_fn(void* handle, void* image_data)
{
    (void) handle;
    free(image_data);
}

int extract_read_intermediate(extract_t* extract, extract_buffer_t* buffer, int autosplit)
{
    int ret = -1;
    
    document_t* document = &extract->document;
    char*   image_data = NULL;
    int     num_spans = 0;

    extract_xml_tag_t   tag;
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

    We convert this into a list of extract_page_t's, each containing a list of
    span_t's, each containing a list of char_t's.

    While doing this, we do some within-span processing by calling
    page_span_end_clean():
        Remove spurious spaces.
        Split spans in two where there seem to be large gaps between glyphs.
    */
    for(;;) {
        extract_page_t* page;
        int e = extract_xml_pparse_next(buffer, &tag);
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
        if (extract_page_begin(extract)) goto end;
        page = extract->document.pages[extract->document.pages_num-1];
        if (!page) goto end;

        for(;;) {
            if (extract_xml_pparse_next(buffer, &tag)) goto end;
            if (!strcmp(tag.name, "/page")) {
                num_spans += page->spans_num;
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
                    const char* c;
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
                matrix_t    ctm;
                matrix_t    trm;
                char*       font_name;
                char*       font_name2;
                int         font_bold;
                int         font_italic;
                int         wmode;
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
                if (extract_span_begin(
                        extract,
                        font_name,
                        font_bold,
                        font_italic,
                        wmode,
                        ctm.a,
                        ctm.b,
                        ctm.c,
                        ctm.d,
                        ctm.e,
                        ctm.f,
                        trm.a,
                        trm.b,
                        trm.c,
                        trm.d,
                        trm.e,
                        trm.f
                        )) goto end;
                
                for(;;) {
                    double      x;
                    double      y;
                    double      adv;
                    unsigned    ucs;

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
                    
                    if (extract_add_char(extract, x, y, ucs, adv, autosplit)) goto end;
                }

                extract_xml_tag_free(extract->alloc, &tag);
            }
        }
        if (extract_page_end(extract)) goto end;
        outf("page=%i page->num_spans=%i",
                document->pages_num, page->spans_num);
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
        )
{
    int e = -1;
    extract_page_t* page;
    span_t* span;
    assert(extract->document.pages_num > 0);
    page = extract->document.pages[extract->document.pages_num-1];
    outf("extract_span_begin(): ctm=(%f %f %f %f %f %f) trm=(%f %f %f %f %f %f) font_name=%s, wmode=%i",
            ctm_a,
            ctm_b,
            ctm_c,
            ctm_d,
            ctm_e,
            ctm_f,
            trm_a,
            trm_b,
            trm_c,
            trm_d,
            trm_e,
            trm_f,
            font_name,
            wmode
            );
    span = page_span_append(extract->alloc, page);
    if (!span) goto end;
    span->ctm.a = ctm_a;
    span->ctm.b = ctm_b;
    span->ctm.c = ctm_c;
    span->ctm.d = ctm_d;
    span->ctm.e = ctm_e;
    span->ctm.f = ctm_f;
    
    span->trm.a = trm_a;
    span->trm.b = trm_b;
    span->trm.c = trm_c;
    span->trm.d = trm_d;
    span->trm.e = trm_e;
    span->trm.f = trm_f;
    
    {
        const char* ff = strchr(font_name, '+');
        const char* f = (ff) ? ff+1 : font_name;
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


int extract_add_char(
        extract_t*  extract,
        double      x,
        double      y,
        unsigned    ucs,
        double      adv,
        int         autosplit
        )
{
    int e = -1;
    char_t* char_;
    extract_page_t* page = extract->document.pages[extract->document.pages_num-1];
    span_t* span = page->spans[page->spans_num - 1];
    
    outf("(%f %f) ucs=% 5i=%c adv=%f", x, y, ucs, (ucs >=32 && ucs< 127) ? ucs : ' ', adv);
    /* Ignore the specified <autosplit> - there seems no advantage to not
    splitting spans on multiple lines, and not doing so causes problems with
    missing spaces in the output. */
    autosplit = 1;
    
    if (span->chars_num)
    {
        char_t* char_prev = &span->chars[span->chars_num - 1];
        double xx = span->ctm.a * x + span->ctm.c * y + span->ctm.e;
        double yy = span->ctm.b * x + span->ctm.d * y + span->ctm.f;
        double dx = xx - char_prev->x;
        double dy = yy - char_prev->y;
        double a = atan2(dy, dx);
        double span_a;
        matrix_t m = extract_multiply_matrix_matrix(span->trm, span->ctm);
        point_t dir = {1 - span->flags.wmode, span->flags.wmode};
        dir = extract_multiply_matrix_point(m, dir);
        span_a = atan2(dir.y, dir.x);
        if (fabs(span_a - a) > 0.01)
        {
            /* Create new span. */
            span_t* span0 = span;
            outf("chars_num=%i prev=(%f %f) => (%f %f) xy=(%f %f) => xxyy=(%f %f) delta=(%f %f) a=%f not in line with dir=(%f %f) a=%f: ",
                    span->chars_num,
                    char_prev->pre_x, char_prev->pre_y,
                    char_prev->x, char_prev->y,
                    x, y,
                    xx, yy,
                    dx, dy, a,
                    dir.x, dir.y, span_a
                    );
            extract->num_spans_autosplit += 1;
            span = page_span_append(extract->alloc, page);
            if (!span) goto end;
            *span = *span0;
            span->chars = NULL;
            span->chars_num = 0;
            if (extract_strdup(extract->alloc, span0->font_name, &span->font_name)) goto end;
        }
    }
    
    if (0 && autosplit && y - extract->span_offset_y != 0) {
        
        double e = span->ctm.e + span->ctm.a * (x - extract->span_offset_x)
                + span->ctm.b * (y - extract->span_offset_y);
        double f = span->ctm.f + span->ctm.c * (x - extract->span_offset_x)
                + span->ctm.d * (y - extract->span_offset_y);
        extract->span_offset_x = x;
        extract->span_offset_y = y;
        outfx("autosplit: char_pre_y=%f offset_y=%f",
                char_pre_y, offset_y);
        outfx(
                "autosplit: changing ctm.{e,f} from (%f, %f) to (%f, %f)",
                span->ctm.e,
                span->ctm.f,
                e, f
                );
        if (span->chars_num > 0) {
            /* Create new span. */
            span_t* span0 = span;
            extract->num_spans_autosplit += 1;
            span = page_span_append(extract->alloc, page);
            if (!span) goto end;
            *span = *span0;
            span->chars = NULL;
            span->chars_num = 0;
            if (extract_strdup(extract->alloc, span0->font_name, &span->font_name)) goto end;
        }
        span->ctm.e = e;
        span->ctm.f = f;
        outfx("autosplit: char_pre_y=%f offset_y=%f",
                char_pre_y, offset_y);
    }
    
    if (extract_span_append_c(extract->alloc, span, 0 /*c*/)) goto end;
    char_ = &span->chars[ span->chars_num-1];
    
    char_->pre_x = x;
    char_->pre_y = y;

    char_->x = span->ctm.a * char_->pre_x + span->ctm.c * char_->pre_y + span->ctm.e;
    char_->y = span->ctm.b * char_->pre_x + span->ctm.d * char_->pre_y + span->ctm.f;
    
    char_->adv = adv;
    char_->ucs = ucs;

    {
        int page_spans_num_old = page->spans_num;
        if (page_span_end_clean(extract->alloc, page)) goto end;
        span = page->spans[page->spans_num-1];  /* fixme: unnecessary. */
        if (page->spans_num != page_spans_num_old) {
            extract->num_spans_split += 1;
        }
    }
    e = 0;
    
    end:
    return e;
}


int extract_span_end(extract_t* extract)
{
    extract_page_t* page = extract->document.pages[extract->document.pages_num-1];
    span_t* span = page->spans[page->spans_num - 1];
    if (span->chars_num == 0) {
        /* Calling code called extract_span_begin() then extract_span_end()
        without any call to extract_add_char(). Our joining code assumes that
        all spans are non-empty, so we need to delete this span. */
        extract_free(extract->alloc, &page->spans[page->spans_num - 1]);
        page->spans_num -= 1;
    }
    return 0;
}


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
        )
{
    int e = -1;
    extract_page_t* page = extract->document.pages[extract->document.pages_num-1];
    image_t image_temp = {0};
    
    extract->image_n += 1;
    image_temp.x = x;
    image_temp.y = y;
    image_temp.w = w;
    image_temp.h = h;
    image_temp.data = data;
    image_temp.data_size = data_size;
    image_temp.data_free = data_free;
    image_temp.data_free_handle = data_free_handle;
    if (extract_strdup(extract->alloc, type, &image_temp.type)) goto end;
    if (extract_asprintf(extract->alloc, &image_temp.id, "rId%i", extract->image_n) < 0) goto end;
    if (extract_asprintf(extract->alloc, &image_temp.name, "image%i.%s", extract->image_n, image_temp.type) < 0) goto end;
    
    if (extract_realloc2(
            extract->alloc,
            &page->images,
            sizeof(image_t) * page->images_num,
            sizeof(image_t) * (page->images_num + 1)
            )) goto end;
    
    page->images[page->images_num] = image_temp;
    page->images_num += 1;
    outf("page->images_num=%i", page->images_num);
    
    e = 0;
    
    end:
    
    if (e) {
        extract_free(extract->alloc, &image_temp.type);
        extract_free(extract->alloc, &image_temp.data);
        extract_free(extract->alloc, &image_temp.id);
        extract_free(extract->alloc, &image_temp.name);
    }
    
    return e;
}


static int tablelines_append(extract_alloc_t* alloc, tablelines_t* tablelines, rect_t* rect, double color)
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

static point_t transform(double x, double y, 
        double ctm_a,
        double ctm_b,
        double ctm_c,
        double ctm_d,
        double ctm_e,
        double ctm_f
        )
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
        )
{
    extract_page_t* page = extract->document.pages[extract->document.pages_num-1];
    point_t points[4] = {
            transform(x0, y0, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f),
            transform(x1, y1, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f),
            transform(x2, y2, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f),
            transform(x3, y3, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f)
            };
    rect_t rect;
    int i;
    double dx;
    double dy;
    if (0 && color == 1)
    {
        return 0;
    }
    outf("cmt=(%f %f %f %f %f %f) points=[(%f %f) (%f %f) (%f %f) (%f %f)]",
            ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f,
            x0, y0, x1, y1, x2, y2, x3, y3
            );
    outf("extract_add_path4(): [(%f %f) (%f %f) (%f %f) (%f %f)]",
            x0, y0, x1, y1, x2, y2, x3, y3);
    /* Find first step with dx > 0. */
    for (i=0; i<4; ++i)
    {
        if (points[(i+1) % 4].x > points[(i+0) % 4].x)    break;
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
        if (tablelines_append(extract->alloc, &page->tablelines_horizontal, &rect, color)) return -1;
    }
    else if (dy / dx > 5)
    {
        /* Vertical line. */
        outf("have found vertical line: %s", extract_rect_string(&rect));
        if (tablelines_append(extract->alloc, &page->tablelines_vertical, &rect, color)) return -1;
    }
    return 0;
}


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
        )
{
    extract_page_t* page = extract->document.pages[extract->document.pages_num-1];
    point_t p0 = transform(x0, y0, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f);
    point_t p1 = transform(x1, y1, ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f);
    double width2 = width * sqrt( fabs( ctm_a * ctm_d - ctm_b * ctm_c));
    rect_t  rect;
    (void) color;
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
        return tablelines_append(extract->alloc, &page->tablelines_vertical, &rect, color);
    }
    else if (rect.min.y == rect.max.y)
    {
        rect.min.y -= width2 / 2;
        rect.max.y += width2 / 2;
        return tablelines_append(extract->alloc, &page->tablelines_horizontal, &rect, color);
    }
    return 0;
}


int extract_page_begin(extract_t* extract)
{
    /* Appends new empty extract_page_t to an extract->document. */
    extract_page_t* page;
    if (extract_malloc(extract->alloc, &page, sizeof(extract_page_t))) return -1;
    page->spans = NULL;
    page->spans_num = 0;
    page->lines = NULL;
    page->lines_num = 0;
    page->paragraphs = NULL;
    page->paragraphs_num = 0;
    page->images = NULL;
    page->images_num = 0;
    page->tablelines_horizontal.tablelines = NULL;
    page->tablelines_horizontal.tablelines_num = 0;
    page->tablelines_vertical.tablelines = NULL;
    page->tablelines_vertical.tablelines_num = 0;
    page->tables = NULL;
    page->tables_num = 0;
    
    if (extract_realloc2(
            extract->alloc,
            &extract->document.pages,
            sizeof(extract_page_t*) * extract->document.pages_num + 1,
            sizeof(extract_page_t*) * (extract->document.pages_num + 1)
            )) {
        extract_free(extract->alloc, &page);
        return -1;
    }
    extract->document.pages[extract->document.pages_num] = page;
    extract->document.pages_num += 1;
    return 0;
}

int extract_fill_begin(
        extract_t*  extract,
        double ctm_a,
        double ctm_b,
        double ctm_c,
        double ctm_d,
        double ctm_e,
        double ctm_f,
        double color
        )
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
        extract_t*  extract,
        double ctm_a,
        double ctm_b,
        double ctm_c,
        double ctm_d,
        double ctm_e,
        double ctm_f,
        double line_width,
        double color
        )
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

int extract_moveto(extract_t* extract, double x, double y)
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

int extract_lineto(extract_t* extract, double x, double y)
{
    if (extract->path_type == path_type_FILL)
    {
        if (extract->path.fill.n == -1)    return 0;
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
                    extract->path.stroke.color
                    ))
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

int extract_closepath(extract_t* extract)
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
                    extract->path.fill.color
                    );
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
                    extract->path.stroke.color
                    ))
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


int extract_fill_end(extract_t* extract)
{
    assert(extract->path_type == path_type_FILL);
    extract->path_type = path_type_NONE;
    return 0;
}


int extract_stroke_end(extract_t* extract)
{
    assert(extract->path_type == path_type_STROKE);
    extract->path_type = path_type_NONE;
    return 0;
}



int extract_page_end(extract_t* extract)
{
    (void) extract;
    return 0;
}


static int paragraphs_to_text_content(
        extract_alloc_t* alloc,
        paragraph_t** paragraphs,
        int paragraphs_num,
        extract_astring_t* text
        )
{
    int p;
    for (p=0; p<paragraphs_num; ++p)
    {
        paragraph_t* paragraph = paragraphs[p];
        int l;
        for (l=0; l<paragraph->lines_num; ++l)
        {
            line_t* line = paragraph->lines[l];
            int s;
            for (s=0; s<line->spans_num; ++s)
            {
                span_t* span = line->spans[s];
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
    }
    return 0;
}


static int extract_write_tables_csv(extract_t* extract)
{
    int ret = -1;
    int p;
    char* path = NULL;
    FILE* f = NULL;
    extract_astring_t text = {NULL, 0};
    if (!extract->tables_csv_format) return 0;
    
    outf("extract_write_tables_csv(): path_format=%s", extract->tables_csv_format);
    outf("extract->document.pages_num=%i", extract->document.pages_num);
    for (p=0; p<extract->document.pages_num; ++p)
    {
        extract_page_t* page = extract->document.pages[p];
        int t;
        outf("p=%i page->tables_num=%i", p, page->tables_num);
        for (t=0; t<page->tables_num; ++t)
        {
            table_t* table = page->tables[t];
            int y;
            if (f) fclose(f);
            extract_free(extract->alloc, &path);
            if (extract_asprintf(extract->alloc, &path, extract->tables_csv_format, extract->tables_csv_i) < 0) goto end;
            extract->tables_csv_i += 1;
            outf("Writing table %i to: %s", t, path);
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
                            cell->paragraphs,
                            cell->paragraphs_num,
                            &text
                            )) goto end;
                    /* Reference cvs output trims trailing spaces. */
                    extract_astring_char_truncate_if(&text, ' ');
                    fprintf(f, "\"%s\"", text.chars ? text.chars : "");
                }
                fprintf(f, "\n");
            }
            fclose(f);
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
        extract_t*  extract,
        int         spacing,
        int         rotation,
        int         images
        )
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
    
    if (extract_document_join(extract->alloc, &extract->document)) goto end;
    
    if (extract->format == extract_format_ODT)
    {
        if (extract_document_to_odt_content(
                extract->alloc,
                &extract->document,
                spacing,
                rotation,
                images,
                &extract->contentss[extract->contentss_num - 1],
                &extract->odt_styles
                )) goto end;
    }
    else if (extract->format == extract_format_DOCX)
    {
        if (extract_document_to_docx_content(
                extract->alloc,
                &extract->document,
                spacing,
                rotation,
                images,
                &extract->contentss[extract->contentss_num - 1]
                )) goto end;
    }
    else if (extract->format == extract_format_HTML)
    {
        if (extract_document_to_html_content(
                extract->alloc,
                &extract->document,
                rotation,
                images,
                &extract->contentss[extract->contentss_num - 1]
                )) goto end;
    }
    else if (extract->format == extract_format_TEXT)
    {
        int p;
        for (p=0; p<extract->document.pages_num; ++p)
        {
            extract_page_t* page = extract->document.pages[p];
            if (paragraphs_to_text_content(
                    extract->alloc,
                    page->paragraphs,
                    page->paragraphs_num,
                    &extract->contentss[extract->contentss_num - 1]
                    )) goto end;
        }
    }
    else
    {
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
        int i;
        for (i=0; i<extract->document.pages_num; ++i) {
            page_free(extract->alloc, &extract->document.pages[i]);
        }
        extract_free(extract->alloc, &extract->document.pages);
        extract->document.pages_num = 0;
    }
    
    e = 0;
    
    end:
    return e;
}

int extract_write(extract_t* extract, extract_buffer_t* buffer)
{
    int             e = -1;
    extract_zip_t*  zip = NULL;
    char*           text2 = NULL;
    int             i;
    
    if (extract->format == extract_format_ODT)
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
            image_t* image = &extract->images.images[i];
            extract_free(extract->alloc, &text2);
            if (extract_asprintf(extract->alloc, &text2, "Pictures/%s", image->name) < 0) goto end;
            if (extract_zip_write_file(zip, image->data, image->data_size, text2)) goto end;
        }
        if (extract_zip_close(&zip)) goto end;
    }
    else if (extract->format == extract_format_DOCX)
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
            image_t* image = &extract->images.images[i];
            extract_free(extract->alloc, &text2);
            if (extract_asprintf(extract->alloc, &text2, "word/media/%s", image->name) < 0) goto end;
            if (extract_zip_write_file(zip, image->data, image->data_size, text2)) goto end;
        }
        if (extract_zip_close(&zip)) goto end;
        
    }
    else if (extract->format == extract_format_HTML)
    {
        for (i=0; i<extract->contentss_num; ++i)
        {
            if (extract_buffer_write(buffer, extract->contentss[i].chars, extract->contentss[i].chars_num, NULL)) goto end;
        }
    }
    else if (extract->format == extract_format_TEXT)
    {
        for (i=0; i<extract->contentss_num; ++i)
        {
            if (extract_buffer_write(buffer, extract->contentss[i].chars, extract->contentss[i].chars_num, NULL)) goto end;
        }
    }
    else
    {
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

int extract_write_content(extract_t* extract, extract_buffer_t* buffer)
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

static int string_ends_with(const char* string, const char* end)
{
    size_t string_len = strlen(string);
    size_t end_len = strlen(end);
    if (end_len > string_len) return 0;
    return memcmp(string + string_len - end_len, end, end_len) == 0;
}

int extract_write_template(
        extract_t*  extract, 
        const char* path_template,
        const char* path_out,
        int         preserve_dir
        )
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
                preserve_dir
                );
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
                preserve_dir
                );
    }
}


void extract_end(extract_t** pextract)
{
    extract_t* extract = *pextract;
    if (!extract) return;
    extract_document_free(extract->alloc, &extract->document);
    
    {
        int i;
        for (i=0; i<extract->contentss_num; ++i) {
            extract_astring_free(extract->alloc, &extract->contentss[i]);
        }
        extract_free(extract->alloc, &extract->contentss);
    }
    extract_images_free(extract->alloc, &extract->images);
    extract_odt_styles_free(extract->alloc, &extract->odt_styles);
    extract_free(extract->alloc, pextract);
}

void extract_internal_end(void)
{
    extract_span_string(NULL, NULL);
}

void extract_exp_min(extract_t* extract, size_t size)
{
    extract_alloc_exp_min(extract->alloc, size);
}

double extract_matrices_to_font_size(matrix_t* ctm, matrix_t* trm)
{
    double font_size = extract_matrix_expansion(*trm)
            * extract_matrix_expansion(*ctm);
    /* Round font_size to nearest 0.01. */
    font_size = (double) (int) (font_size * 100.0f + 0.5f) / 100.0f;
    return font_size;
}
