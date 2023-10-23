// Copyright (C) 2022 Artifex Software, Inc.
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

#ifndef XML_IMP_H

#define XML_IMP_H

#include "mupdf/fitz.h"

/* These types are required for basic XML operation. */

struct attribute
{
	char *value;
	struct attribute *next;
	char name[1];
};

/**
	We use a slightly grotty representation for an XML tree.

	The topmost element of the tree is an fz_xml with up == NULL.
	This signifies that we are a 'doc', rather than a 'node'.

	We only ever get a 'doc' node at the root, and this contains
	a reference count for the entire tree, together with the
	fz_pool pointer used to allocate nodes.

	All other structures are 'nodes'. If down is MAGIC_TEXT then
	they are text nodes (with no children or attributes).
	Otherwise, they are standard XML nodes with attributes
	and children.
*/

struct fz_xml
{
	fz_xml *up, *down;
	union
	{
		struct /* up != NULL */
		{
			fz_xml *prev, *next;
#ifdef FZ_XML_SEQ
			int seq;
#endif
			union
			{
				char text[1]; /* down == MAGIC_TEXT */
				struct /* down != MAGIC_TEXT */
				{
					struct attribute *atts;
					char name[1];
				} d;
			} u;
		} node;
		struct /* up == NULL */
		{
			int refs;
			fz_pool *pool;
		} doc;
	} u;
};

#define MAGIC_TEXT ((fz_xml *)1)

#define FZ_TEXT_ITEM(item) (item && item->down == MAGIC_TEXT)
#define FZ_DOCUMENT_ITEM(item) (item && item->up == NULL)

size_t xml_parse_entity(int *c, const char *a);

#endif
