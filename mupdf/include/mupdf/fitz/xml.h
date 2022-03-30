// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#ifndef MUPDF_FITZ_XML_H
#define MUPDF_FITZ_XML_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"

/**
	XML document model
*/

typedef struct fz_xml_doc fz_xml_doc;
typedef struct fz_xml fz_xml;

/**
	Parse the contents of buffer into a tree of xml nodes.

	preserve_white: whether to keep or delete all-whitespace nodes.
*/
fz_xml_doc *fz_parse_xml(fz_context *ctx, fz_buffer *buf, int preserve_white);

/**
	Parse the contents of a buffer into a tree of XML nodes,
	using the HTML5 parsing algorithm.
*/
fz_xml_doc *fz_parse_xml_from_html5(fz_context *ctx, fz_buffer *buf);

/**
	Free the XML node and all its children and siblings.
*/
void fz_drop_xml(fz_context *ctx, fz_xml_doc *xml);

/**
	Detach a node from the tree, unlinking it from its parent,
	and setting the document root to the node.
*/
void fz_detach_xml(fz_context *ctx, fz_xml_doc *xml, fz_xml *node);

/**
	Return the topmost XML node of a document.
*/
fz_xml *fz_xml_root(fz_xml_doc *xml);

/**
	Return previous sibling of XML node.
*/
fz_xml *fz_xml_prev(fz_xml *item);

/**
	Return next sibling of XML node.
*/
fz_xml *fz_xml_next(fz_xml *item);

/**
	Return parent of XML node.
*/
fz_xml *fz_xml_up(fz_xml *item);

/**
	Return first child of XML node.
*/
fz_xml *fz_xml_down(fz_xml *item);

/**
	Return true if the tag name matches.
*/
int fz_xml_is_tag(fz_xml *item, const char *name);

/**
	Return tag of XML node. Return NULL for text nodes.
*/
char *fz_xml_tag(fz_xml *item);

/**
	Return the value of an attribute of an XML node.
	NULL if the attribute doesn't exist.
*/
char *fz_xml_att(fz_xml *item, const char *att);

/**
	Return the value of an attribute of an XML node.
	If the first attribute doesn't exist, try the second.
	NULL if neither attribute exists.
*/
char *fz_xml_att_alt(fz_xml *item, const char *one, const char *two);

/**
	Check for a matching attribute on an XML node.

	If the node has the requested attribute (name), and the value
	matches (match) then return 1. Otherwise, 0.
*/
int fz_xml_att_eq(fz_xml *item, const char *name, const char *match);

/**
	Return the text content of an XML node.
	Return NULL if the node is a tag.
*/
char *fz_xml_text(fz_xml *item);

/**
	Pretty-print an XML tree to stdout.
*/
void fz_debug_xml(fz_xml *item, int level);

/**
	Search the siblings of XML nodes starting with item looking for
	the first with the given tag.

	Return NULL if none found.
*/
fz_xml *fz_xml_find(fz_xml *item, const char *tag);

/**
	Search the siblings of XML nodes starting with the first sibling
	of item looking for the first with the given tag.

	Return NULL if none found.
*/
fz_xml *fz_xml_find_next(fz_xml *item, const char *tag);

/**
	Search the siblings of XML nodes starting with the first child
	of item looking for the first with the given tag.

	Return NULL if none found.
*/
fz_xml *fz_xml_find_down(fz_xml *item, const char *tag);

/**
	Search the siblings of XML nodes starting with item looking for
	the first with the given tag, and with a matching attribute.

	Return NULL if none found.
*/
fz_xml *fz_xml_find_match(fz_xml *item, const char *tag, const char *att, const char *match);

/**
	Search the siblings of XML nodes starting with the first sibling
	of item looking for the first with the given tag, and with a
	matching attribute.

	Return NULL if none found.
*/
fz_xml *fz_xml_find_next_match(fz_xml *item, const char *tag, const char *att, const char *match);

/**
	Search the siblings of XML nodes starting with the first child
	of item looking for the first with the given tag, and with a
	matching attribute.

	Return NULL if none found.
*/
fz_xml *fz_xml_find_down_match(fz_xml *item, const char *tag, const char *att, const char *match);

#endif
