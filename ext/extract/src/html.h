#ifndef ARTIFEX_EXTRACT_HTML_H
#define ARTIFEX_EXTRACT_HTML_H

#include "extract/extract.h"

/* Only for internal use by extract code.  */

/* Things for creating docx files. */

int extract_document_to_html_content(
		extract_alloc_t    *alloc,
		document_t        *document,
		int                rotation,
		int                images,
		extract_astring_t *content
		);
/* Makes *o_content point to a string containing all paragraphs in *document in
docx XML format.

This string can be passed to extract_docx_content_item() or
extract_docx_write_template() to be inserted into a docx archive's
word/document.xml. */


#endif
