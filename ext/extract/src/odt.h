#ifndef ARTIFEX_EXTRACT_ODT_H
#define ARTIFEX_EXTRACT_ODT_H

/* Only for internal use by extract code.  */

/* Things for creating odt files. */

typedef struct extract_odt_style_t extract_odt_style_t;

typedef struct
{
    extract_odt_style_t*    styles;
    int                     styles_num;
} extract_odt_styles_t;

void extract_odt_styles_free(extract_alloc_t* alloc, extract_odt_styles_t* styles);

int extract_document_to_odt_content(
        extract_alloc_t*    alloc,
        document_t*         document,
        int                 spacing,
        int                 rotation,
        int                 images,
        extract_astring_t*  o_content,
        extract_odt_styles_t* o_styles
        );
/* Makes *o_content point to a string containing all paragraphs in *document in
odt XML format.

Also writes style definitions to <o_styles>.

<o_content> and <o_styles> can be passed to extract_odt_content_item() or
extract_odt_write_template() to be inserted into an odt archive. */


int extract_odt_write_template(
        extract_alloc_t*    alloc,
        extract_astring_t*  contentss,
        int                 contentss_num,
        extract_odt_styles_t* styles,
        images_t*           images,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        );
/* Creates a new odt file using a provided template document.

Uses the 'zip' and 'unzip' commands internally.

contents
contentss_num
    Content to be inserted into word/document.xml.
document
    .
images
    Information about images.
path_template
    Name of odt file to use as a template.
path_out
    Name of odt file to create. Must not contain single-quote, double quote,
    space or ".." sequence - these will force EINVAL error because they could
    make internal shell commands unsafe.
preserve_dir
    If true, we don't delete the temporary directory <path_out>.dir containing
    unzipped odt content.
*/


int extract_odt_content_item(
        extract_alloc_t*    alloc,
        extract_astring_t*  contentss,
        int                 contentss_num,
        extract_odt_styles_t* styles,
        images_t*           images,
        const char*         name,
        const char*         text,
        char**              text2
        );
/* Determines content of <name> in odt archive.

content
content_length
    Text to insert if <name> is word/document.xml.
styles
    Text containing style definitions.
images
    Information about images. If <name> is word/document.xml we insert
    relationship information mapping from image ids to image names;
    <text> should already contain reference ids for images. If <name> is
    [Content_Types].xml we insert information about image types.
name
    Path within the odt zip archive.
text
    Content of <name> in template odt file.
text2
    Out-param. Set to NULL if <text> should be used unchanged. Otherwise set to
    point to desired text, allocated with malloc() which caller should free.
*/

#endif
