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

#ifndef MUPDF_PDF_H
#define MUPDF_PDF_H

#include "mupdf/fitz.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "mupdf/pdf/object.h"
#include "mupdf/pdf/document.h"
#include "mupdf/pdf/parse.h"
#include "mupdf/pdf/xref.h"
#include "mupdf/pdf/crypt.h"

#include "mupdf/pdf/page.h"
#include "mupdf/pdf/resource.h"
#include "mupdf/pdf/cmap.h"
#include "mupdf/pdf/font.h"
#include "mupdf/pdf/interpret.h"

#include "mupdf/pdf/annot.h"
#include "mupdf/pdf/form.h"
#include "mupdf/pdf/event.h"
#include "mupdf/pdf/javascript.h"

#include "mupdf/pdf/clean.h"
#include "mupdf/pdf/recolor.h"
#include "mupdf/pdf/image-rewriter.h"
#include "mupdf/pdf/zugferd.h"


#ifdef __cplusplus
}
#endif

#endif
