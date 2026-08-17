// Copyright (C) 2021 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_TYPES_H
#define MUPDF_FITZ_TYPES_H

typedef struct fz_document fz_document;

/**
	Locations within the document are referred to in terms of
	chapter and page, rather than just a page number. For some
	documents (such as epub documents with large numbers of pages
	broken into many chapters) this can make navigation much faster
	as only the required chapter needs to be decoded at a time.
*/
typedef struct
{
	int chapter;
	int page;
} fz_location;

#endif
