#ifndef MUPDF_FITZ_XML_H
#define MUPDF_FITZ_XML_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/*
	XML document model
*/

typedef struct fz_xml_doc_s fz_xml_doc;
typedef struct fz_xml_s fz_xml;

fz_xml_doc *fz_parse_xml(fz_context *ctx, fz_buffer *buf, int preserve_white, int for_html);

void fz_drop_xml(fz_context *ctx, fz_xml_doc *xml);

void fz_detach_xml(fz_context *ctx, fz_xml_doc *xml, fz_xml *node);

fz_xml *fz_xml_root(fz_xml_doc *xml);

fz_xml *fz_xml_prev(fz_xml *item);

fz_xml *fz_xml_next(fz_xml *item);

fz_xml *fz_xml_up(fz_xml *item);

fz_xml *fz_xml_down(fz_xml *item);

int fz_xml_is_tag(fz_xml *item, const char *name);

char *fz_xml_tag(fz_xml *item);

char *fz_xml_att(fz_xml *item, const char *att);

char *fz_xml_text(fz_xml *item);

void fz_debug_xml(fz_xml *item, int level);

fz_xml *fz_xml_find(fz_xml *item, const char *tag);
fz_xml *fz_xml_find_next(fz_xml *item, const char *tag);
fz_xml *fz_xml_find_down(fz_xml *item, const char *tag);

#endif
