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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>


static int extract_odt_paragraph_start(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "\n\n<text:p>");
}

static int extract_odt_paragraph_finish(extract_alloc_t* alloc, extract_astring_t* content)
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
    char*   font_name;
    double  font_size;
    int     font_bold;
    int     font_italic;
};

struct extract_odt_styles_t
{
    /* Styles are stored sorted. */
    extract_odt_style_t*    styles;
    int                     styles_num;
};

static int extract_odt_style_compare(extract_odt_style_t* a, extract_odt_style_t*b)
{
    int d;
    double dd;
    if ((d = strcmp(a->font_name, b->font_name)))   return d;
    if ((dd = a->font_size - b->font_size) != 0.0)  return (dd > 0.0) ? 1 : -1;
    if ((d = a->font_bold - b->font_bold))          return d;
    if ((d = a->font_italic - b->font_italic))      return d;
    return 0;
}

static int extract_odt_style_append_definition(extract_alloc_t* alloc, extract_odt_style_t* style, extract_astring_t* text)
{
    const char* font_name = style->font_name;
    /* This improves output e.g. for zlib.3.pdf, but clearly a hack. */
    if (0 && strstr(font_name, "Helvetica"))
    {
        font_name = "Liberation Sans";
    }
    outf("style->font_name=%s font_name=%s", style->font_name, font_name);
    if (extract_astring_catf(alloc, text, "<style:style style:name=\"T%i\" style:family=\"text\">", style->id)) return -1;
    if (extract_astring_catf(alloc, text, "<style:text-properties style:font-name=\"%s\"", font_name)) return -1;
    if (extract_astring_catf(alloc, text, " fo:font-size=\"%.2fpt\"", style->font_size)) return -1;
    if (extract_astring_catf(alloc, text, " fo:font-weight=\"%s\"", style->font_bold ? "bold" : "normal")) return -1;
    if (extract_astring_catf(alloc, text, " fo:font-style=\"%s\"", style->font_italic ? "italic" : "normal")) return -1;
    if (extract_astring_cat(alloc, text, " /></style:style>")) return -1;
    return 0;
}

void extract_odt_styles_free(extract_alloc_t* alloc, extract_odt_styles_t* styles)
{
    extract_free(alloc, &styles->styles);
}

static int extract_odt_styles_definitions(
        extract_alloc_t*        alloc,
        extract_odt_styles_t*   styles,
        extract_astring_t*      out
        )
{
    int i;
    if (extract_astring_cat(alloc, out, "<office:automatic-styles>")) return -1;
    for (i=0; i<styles->styles_num; ++i)
    {
        if (extract_odt_style_append_definition(alloc, &styles->styles[i], out)) return -1;
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

static int styles_add(
        extract_alloc_t*        alloc,
        extract_odt_styles_t*   styles,
        const char*             font_name,
        double                  font_size,
        int                     font_bold,
        int                     font_italic,
        extract_odt_style_t**   o_style
    )
/* Adds specified style to <styles> if not already present. Sets *o_style to
point to the style_t within <styles>. */
{
    extract_odt_style_t style = {0 /*id*/, (char*) font_name, font_size, font_bold, font_italic};
    int i;
    /* We keep styles->styles[] sorted; todo: use bsearch or similar when
    searching. */
    for (i=0; i<styles->styles_num; ++i)
    {
        int d = extract_odt_style_compare(&style, &styles->styles[i]);
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
    if (extract_strdup(alloc, font_name, &styles->styles[i].font_name)) return -1;
    styles->styles[i].font_size = font_size;
    styles->styles[i].font_bold = font_bold;
    styles->styles[i].font_italic = font_italic;
    *o_style = &styles->styles[i];
    return 0;
}

static int extract_odt_run_start(
        extract_alloc_t* alloc,
        extract_astring_t* content,
        extract_odt_styles_t* styles,
        const char* font_name,
        double font_size,
        int bold,
        int italic
        )
/* Starts a new run. Caller must ensure that extract_odt_run_finish() was
called to terminate any previous run. */
{
    extract_odt_style_t* style;
    if (styles_add(alloc, styles, font_name, font_size, bold, italic, &style)) return -1;
    if (extract_astring_catf(alloc, content, "<text:span text:style-name=\"T%i\">", style->id)) return -1;
    return 0;
}

static int extract_odt_run_finish(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "</text:span>");
}

static int extract_odt_paragraph_empty(extract_alloc_t* alloc, extract_astring_t* content, extract_odt_styles_t* styles)
/* Append an empty paragraph to *content. */
{
    int e = -1;
    if (extract_odt_paragraph_start(alloc, content)) goto end;
    /* [This comment is from docx, haven't checked odt.] It seems like our
    choice of font size here doesn't make any difference to the ammount of
    vertical space, unless we include a non-space character. Presumably
    something to do with the styles in the template document. */
    if (extract_odt_run_start(
            alloc,
            content,
            styles,
            "OpenSans",
            10 /*font_size*/,
            0 /*font_bold*/,
            0 /*font_italic*/
            )) goto end;
    //docx_char_append_string(content, "&#160;");   /* &#160; is non-break space. */
    if (extract_odt_run_finish(alloc, content)) goto end;
    if (extract_odt_paragraph_finish(alloc, content)) goto end;
    e = 0;
    end:
    return e;
}


typedef struct
{
    const char* font_name;
    double      font_size;
    int         font_bold;
    int         font_italic;
    matrix_t*   ctm_prev;
    /* todo: add extract_odt_styles_t member? */
} content_state_t;
/* Used to keep track of font information when writing paragraphs of odt
content, e.g. so we know whether a font has changed so need to start a new odt
span. */


static int extract_document_to_odt_content_paragraph(
        extract_alloc_t*        alloc,
        content_state_t*        state,
        paragraph_t*            paragraph,
        extract_astring_t*      content,
        extract_odt_styles_t*   styles
        )
/* Append odt xml for <paragraph> to <content>. Updates *state if we change
font. */
{
    int e = -1;
    int l;

    if (extract_odt_paragraph_start(alloc, content)) goto end;

    for (l=0; l<paragraph->lines_num; ++l)
    {
        line_t* line = paragraph->lines[l];
        int s;
        for (s=0; s<line->spans_num; ++s)
        {
            int si;
            span_t* span = line->spans[s];
            double font_size_new;
            state->ctm_prev = &span->ctm;
            font_size_new = extract_matrices_to_font_size(&span->ctm, &span->trm);
            if (!state->font_name
                    || strcmp(span->font_name, state->font_name)
                    || span->flags.font_bold != state->font_bold
                    || span->flags.font_italic != state->font_italic
                    || font_size_new != state->font_size
                    )
            {
                if (state->font_name)
                {
                    if (extract_odt_run_finish(alloc, content)) goto end;
                }
                state->font_name = span->font_name;
                state->font_bold = span->flags.font_bold;
                state->font_italic = span->flags.font_italic;
                state->font_size = font_size_new;
                if (extract_odt_run_start(
                        alloc,
                        content,
                        styles,
                        state->font_name,
                        state->font_size,
                        state->font_bold,
                        state->font_italic
                        )) goto end;
            }

            for (si=0; si<span->chars_num; ++si)
            {
                char_t* char_ = &span->chars[si];
                int c = char_->ucs;
                if (extract_astring_cat_xmlc(alloc, content, c)) goto end;
            }
            /* Remove any trailing '-' at end of line. */
            if (astring_char_truncate_if(content, '-')) goto end;
        }
    }
    if (state->font_name)
    {
        if (extract_odt_run_finish(alloc, content)) goto end;
        state->font_name = NULL;
    }
    if (extract_odt_paragraph_finish(alloc, content)) goto end;
    
    e = 0;
    
    end:
    return e;
}

static int extract_document_append_image(
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


static int extract_document_output_rotated_paragraphs(
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
        content_state_t*    state
        )
/* Writes paragraph to content inside rotated text box. */
{
    int e = 0;
    int p;
    double pt_to_inch = 1/72.0;
    outf("rotated paragraphs: rotation_rad=%f (x y)=(%i %i) (w h)=(%i %i)", rotation_rad, x_pt, y_pt, w_pt, h_pt);
    
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
        if (!e) e = extract_document_to_odt_content_paragraph(alloc, state, paragraph, content, styles);
    }
    
    if (!e) e = extract_astring_cat(alloc, content, "\n");
    if (!e) e = extract_astring_cat(alloc, content, "</draw:text-box>\n");
    if (!e) e = extract_astring_cat(alloc, content, "</draw:frame>\n");
    
    if (!e) e = extract_astring_cat(alloc, content, "</text:p>\n");
    
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
        int p;
        content_state_t state;
        state.font_name = NULL;
        state.font_size = 0;
        state.font_bold = 0;
        state.font_italic = 0;
        state.ctm_prev = NULL;
        
        for (p=0; p<page->paragraphs_num; ++p)
        {
            paragraph_t* paragraph = page->paragraphs[p];
            const matrix_t* ctm = &paragraph->lines[0]->spans[0]->ctm;
            double rotate = atan2(ctm->b, ctm->a);
            
            if (spacing
                    && state.ctm_prev
                    && paragraph->lines_num
                    && paragraph->lines[0]->spans_num
                    && matrix_cmp4(
                            state.ctm_prev,
                            &paragraph->lines[0]->spans[0]->ctm
                            )
                    )
            {
                /* Extra vertical space between paragraphs that were at
                different angles in the original document. */
                if (extract_odt_paragraph_empty(alloc, content, styles)) goto end;
            }

            if (spacing)
            {
                /* Extra vertical space between paragraphs. */
                if (extract_odt_paragraph_empty(alloc, content, styles)) goto end;
            }
            
            if (rotation && rotate != 0)
            {
                /* Find extent of paragraphs with this same rotation. extent
                will contain max width and max height of paragraphs, in units
                before application of ctm, i.e. before rotation. */
                point_t extent = {0, 0};
                int p0 = p;
                int p1;
                
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

                    for (p=p0; p<page->paragraphs_num; ++p)
                    {
                        paragraph = page->paragraphs[p];
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
                                span_t* span = line_span_last(line);
                                char_t* char_ = span_char_last(span);
                                double adv = char_->adv * matrix_expansion(span->trm);
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
                                if (0) outf("rotate=%f p=%i: origin=(%f %f) xy=(%f %f) dxy=(%f %f) xxyy=(%f %f) span: %s",
                                        rotate, p, origin.x, origin.y, x, y, dx, dy, xx, yy, span_string(alloc, span));
                            }
                        }
                    }
                    p1 = p;
                    rotate = rotate0;
                    ctm = ctm0;
                    outf("rotate=%f p0=%i p1=%i. extent is: (%f %f)",
                            rotate, p0, p1, extent.x, extent.y);
                }
                
                /* Paragraphs p0..p1-1 have same rotation. We output them into
                a single rotated text box. */
                
                /* We need unique id for text box. */
                text_box_id += 1;
                
                if (extract_document_output_rotated_paragraphs(
                        alloc,
                        page,
                        p0,
                        p1,
                        rotate,
                        ctm->e,
                        ctm->f,
                        extent.x,
                        extent.y,
                        text_box_id,
                        content,
                        styles,
                        &state
                        )) goto end;
                p = p1 - 1;
            }
            else
            {
                if (extract_document_to_odt_content_paragraph(alloc, &state, paragraph, content, styles)) goto end;
            }
        
        }
        
        outf("images=%i", images);
        if (images)
        {
            int i;
            outf("page->images_num=%i", page->images_num);
            for (i=0; i<page->images_num; ++i)
            {
                extract_document_append_image(alloc, content, &page->images[i]);
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

        if (extract_content_insert(
                alloc,
                text,
                NULL /*single*/,
                NULL,
                "</office:text>",
                contentss,
                contentss_num,
                &text_intermediate
                )) goto end;
        outf("text_intermediate: %s", text_intermediate);
        
        if (extract_odt_styles_definitions(alloc, styles, &styles_definitions)) goto end;
        
        e = extract_content_insert(
                alloc,
                text_intermediate,
                "<office:automatic-styles/>" /*single*/,
                NULL,
                NULL, //"</office:automatic-styles>",
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
    outf("e=%i errno=%i text2=%s", e, errno, text2);
    if (e)
    {
        /* We might have set <text2> to new content. */
        extract_free(alloc, text2);
        /* We might have used <temp> as a temporary buffer. */
        extract_astring_free(alloc, &temp);
    }
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
    FILE*   f = NULL;
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
    outf("");
    extract_free(alloc, &path);
    if (extract_asprintf(alloc, &path, "%s/Pictures", path_tempdir) < 0) goto end;
    if (extract_mkdir(path, 0777))
    {
        outf("Failed to mkdir %s", path);
        goto end;
    }
    outf("");
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
    //extract_odt_styles_free(alloc, &styles);
    if (f)  fclose(f);

    if (e)
    {
        outf("Failed to create %s", path_out);
    }
    return e;
}
