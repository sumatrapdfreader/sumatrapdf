// Copyright (C) 2004-2024 Artifex Software, Inc.
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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_FITZ_XML_H
#define MUPDF_FITZ_XML_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/pool.h"
#include "mupdf/fitz/archive.h"

/**
	XML document model
*/

typedef struct fz_xml fz_xml;

/* For backwards compatibility */
typedef fz_xml fz_xml_doc;

/**
	Parse the contents of buffer into a tree of xml nodes.

	preserve_white: whether to keep or delete all-whitespace nodes.
*/
fz_xml *fz_parse_xml(fz_context *ctx, fz_buffer *buf, int preserve_white);

/**
	Parse the contents of buffer into a tree of xml nodes.

	preserve_white: whether to keep or delete all-whitespace nodes.
*/
fz_xml *fz_parse_xml_stream(fz_context *ctx, fz_stream *stream, int preserve_white);

/**
	Parse the contents of an archive entry into a tree of xml nodes.

	preserve_white: whether to keep or delete all-whitespace nodes.
*/
fz_xml *fz_parse_xml_archive_entry(fz_context *ctx, fz_archive *dir, const char *filename, int preserve_white);

/**
	Try and parse the contents of an archive entry into a tree of xml nodes.

	preserve_white: whether to keep or delete all-whitespace nodes.

	Will return NULL if the archive entry can't be found. Otherwise behaves
	the same as fz_parse_xml_archive_entry. May throw exceptions.
*/
fz_xml *fz_try_parse_xml_archive_entry(fz_context *ctx, fz_archive *dir, const char *filename, int preserve_white);

/**
	Parse the contents of a buffer into a tree of XML nodes,
	using the HTML5 parsing algorithm.
*/
fz_xml *fz_parse_xml_from_html5(fz_context *ctx, fz_buffer *buf);

/**
	Add a reference to the XML.
*/
fz_xml *fz_keep_xml(fz_context *ctx, fz_xml *xml);

/**
	Drop a reference to the XML. When the last reference is
	dropped, the node and all its children and siblings will
	be freed.
*/
void fz_drop_xml(fz_context *ctx, fz_xml *xml);

/**
	Detach a node from the tree, unlinking it from its parent,
	and setting the document root to the node.
*/
void fz_detach_xml(fz_context *ctx, fz_xml *node);

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
	Add an attribute to an XML node.
*/
void fz_xml_add_att(fz_context *ctx, fz_pool *pool, fz_xml *node, const char *key, const char *val);

/**
	Return the text content of an XML node.
	Return NULL if the node is a tag.
*/
char *fz_xml_text(fz_xml *item);

/**
	Pretty-print an XML tree to given output.
*/
void fz_output_xml(fz_context *ctx, fz_output *out, fz_xml *item, int level);

/**
	Pretty-print an XML tree to stdout. (Deprecated, use
	fz_output_xml in preference).
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
	the first with the given tag (or any tag if tag is NULL), and
	with a matching attribute.

	Return NULL if none found.
*/
fz_xml *fz_xml_find_match(fz_xml *item, const char *tag, const char *att, const char *match);

/**
	Search the siblings of XML nodes starting with the first sibling
	of item looking for the first with the given tag (or any tag if tag
	is NULL), and with a matching attribute.

	Return NULL if none found.
*/
fz_xml *fz_xml_find_next_match(fz_xml *item, const char *tag, const char *att, const char *match);

/**
	Search the siblings of XML nodes starting with the first child
	of item looking for the first with the given tag (or any tag if
	tag is NULL), and with a matching attribute.

	Return NULL if none found.
*/
fz_xml *fz_xml_find_down_match(fz_xml *item, const char *tag, const char *att, const char *match);

/**
	Perform a depth first search from item, returning the first
	child that matches the given tag (or any tag if tag is NULL),
	with the given attribute (if att is non NULL), that matches
	match (if match is non NULL).
*/
fz_xml *fz_xml_find_dfs(fz_xml *item, const char *tag, const char *att, const char *match);

/**
	Perform a depth first search from item, returning the first
	child that matches the given tag (or any tag if tag is NULL),
	with the given attribute (if att is non NULL), that matches
	match (if match is non NULL). The search stops if it ever
	reaches the top of the tree, or the declared 'top' item.
*/
fz_xml *fz_xml_find_dfs_top(fz_xml *item, const char *tag, const char *att, const char *match, fz_xml *top);

/**
	Perform a depth first search onwards from item, returning the first
	child that matches the given tag (or any tag if tag is NULL),
	with the given attribute (if att is non NULL), that matches
	match (if match is non NULL).
*/
fz_xml *fz_xml_find_next_dfs(fz_xml *item, const char *tag, const char *att, const char *match);

/**
	Perform a depth first search onwards from item, returning the first
	child that matches the given tag (or any tag if tag is NULL),
	with the given attribute (if att is non NULL), that matches
	match (if match is non NULL). The search stops if it ever reaches
	the top of the tree, or the declared 'top' item.
*/
fz_xml *fz_xml_find_next_dfs_top(fz_xml *item, const char *tag, const char *att, const char *match, fz_xml *top);

/**
	Extract and concatenate all plain text data from XML tree into a new string.
*/
char *fz_new_text_from_xml(fz_context *ctx, fz_xml *root);

/**
	DOM-like functions for html in xml.
*/

/**
	Return a borrowed reference for the 'body' element of
	the given DOM.
*/
fz_xml *fz_dom_body(fz_context *ctx, fz_xml *dom);

/**
	Return a borrowed reference for the document (the top
	level element) of the DOM.
*/
fz_xml *fz_dom_document_element(fz_context *ctx, fz_xml *dom);

/**
	Create an element of a given tag type for the given DOM.

	The element is not linked into the DOM yet.
*/
fz_xml *fz_dom_create_element(fz_context *ctx, fz_xml *dom, const char *tag);

/**
	Create a text node for the given DOM.

	The element is not linked into the DOM yet.
*/
fz_xml *fz_dom_create_text_node(fz_context *ctx, fz_xml *dom, const char *text);

/**
	Find the first element matching the requirements in a depth first traversal from elt.

	The tagname must match tag, unless tag is NULL, when all tag names are considered to match.

	If att is NULL, then all tags match.
	Otherwise:
		If match is NULL, then only nodes that have an att attribute match.
		If match is non-NULL, then only nodes that have an att attribute that matches match match.

	Returns NULL (if no match found), or a borrowed reference to the first matching element.
*/
fz_xml *fz_dom_find(fz_context *ctx, fz_xml *elt, const char *tag, const char *att, const char *match);

/**
	Find the next element matching the requirements.
*/
fz_xml *fz_dom_find_next(fz_context *ctx, fz_xml *elt, const char *tag, const char *att, const char *match);

/**
	Insert an element as the last child of a parent, unlinking the
	child from its current position if required.
*/
void fz_dom_append_child(fz_context *ctx, fz_xml *parent, fz_xml *child);

/**
	Insert an element (new_elt), before another element (node),
	unlinking the new_elt from its current position if required.
*/
void fz_dom_insert_before(fz_context *ctx, fz_xml *node, fz_xml *new_elt);

/**
	Insert an element (new_elt), after another element (node),
	unlinking the new_elt from its current position if required.
*/
void fz_dom_insert_after(fz_context *ctx, fz_xml *node, fz_xml *new_elt);

/**
	Remove an element from the DOM. The element can be added back elsewhere
	if required.

	No reference counting changes for the element.
*/
void fz_dom_remove(fz_context *ctx, fz_xml *elt);

/**
	Clone an element (and its children).

	A borrowed reference to the clone is returned. The clone is not
	yet linked into the DOM.
*/
fz_xml *fz_dom_clone(fz_context *ctx, fz_xml *elt);

/**
	Return a borrowed reference to the first child of a node,
	or NULL if there isn't one.
*/
fz_xml *fz_dom_first_child(fz_context *ctx, fz_xml *elt);

/**
	Return a borrowed reference to the parent of a node,
	or NULL if there isn't one.
*/
fz_xml *fz_dom_parent(fz_context *ctx, fz_xml *elt);

/**
	Return a borrowed reference to the next sibling of a node,
	or NULL if there isn't one.
*/
fz_xml *fz_dom_next(fz_context *ctx, fz_xml *elt);

/**
	Return a borrowed reference to the previous sibling of a node,
	or NULL if there isn't one.
*/
fz_xml *fz_dom_previous(fz_context *ctx, fz_xml *elt);

/**
	Add an attribute to an element.

	Ownership of att and value remain with the caller.
*/
void fz_dom_add_attribute(fz_context *ctx, fz_xml *elt, const char *att, const char *value);

/**
	Remove an attribute from an element.
*/
void fz_dom_remove_attribute(fz_context *ctx, fz_xml *elt, const char *att);

/**
	Retrieve the value of a given attribute from a given element.

	Returns a borrowed pointer to the value or NULL if not found.
*/
const char *fz_dom_attribute(fz_context *ctx, fz_xml *elt, const char *att);

/**
	Enumerate through the attributes of an element.

	Call with i=0,1,2,3... to enumerate attributes.

	On return *att and the return value will be NULL if there are not
	that many attributes to read. Otherwise, *att will be filled in
	with a borrowed pointer to the attribute name, and the return
	value will be a borrowed pointer to the value.
*/
const char *fz_dom_get_attribute(fz_context *ctx, fz_xml *elt, int i, const char **att);

/**
	Make new xml dom root element.
*/
fz_xml *fz_new_dom(fz_context *ctx, const char *tag);

/**
	Create a new dom node.

	This will NOT be linked in yet.
*/
fz_xml *fz_new_dom_node(fz_context *ctx, fz_xml *dom, const char *tag);

/**
	Create a new dom text node.

	This will NOT be linked in yet.
*/
fz_xml *fz_new_dom_text_node(fz_context *ctx, fz_xml *dom, const char *text);

/**
	Write our xml structure out to an xml stream.

	Properly formatted XML is only allowed to have a single top-level node
	under which everything must sit. Our structures allow for multiple
	top level nodes. If required, we will output an extra 'ROOT' node
	at the top so that the xml is well-formed.

	If 'indented' is non-zero then additional whitespace will be added to
	make the XML easier to read in a text editor. It will NOT be properly
	compliant.
*/
void fz_write_xml(fz_context *ctx, fz_xml *root, fz_output *out, int indented);

/**
	As for fz_write_xml, but direct to a file.
*/
void fz_save_xml(fz_context *ctx, fz_xml *root, const char *path, int indented);

#endif
