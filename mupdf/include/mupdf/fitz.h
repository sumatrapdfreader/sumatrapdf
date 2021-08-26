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

#ifndef MUDPF_FITZ_H
#define MUDPF_FITZ_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mupdf/fitz/version.h"
#include "mupdf/fitz/config.h"
#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/log.h"

#include "mupdf/fitz/crypt.h"
#include "mupdf/fitz/getopt.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/hash.h"
#include "mupdf/fitz/pool.h"
#include "mupdf/fitz/string-util.h"
#include "mupdf/fitz/tree.h"
#include "mupdf/fitz/bidi.h"
#include "mupdf/fitz/xml.h"

/* I/O */
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"
#include "mupdf/fitz/compress.h"
#include "mupdf/fitz/compressed-buffer.h"
#include "mupdf/fitz/filter.h"
#include "mupdf/fitz/archive.h"

/* Resources */
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/color.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/bitmap.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/shade.h"
#include "mupdf/fitz/font.h"
#include "mupdf/fitz/path.h"
#include "mupdf/fitz/text.h"
#include "mupdf/fitz/separation.h"
#include "mupdf/fitz/glyph.h"

#include "mupdf/fitz/device.h"
#include "mupdf/fitz/display-list.h"
#include "mupdf/fitz/structured-text.h"

#include "mupdf/fitz/transition.h"
#include "mupdf/fitz/glyph-cache.h"

/* Document */
#include "mupdf/fitz/link.h"
#include "mupdf/fitz/outline.h"
#include "mupdf/fitz/document.h"

#include "mupdf/fitz/util.h"

/* Output formats */
#include "mupdf/fitz/writer.h"
#include "mupdf/fitz/band-writer.h"
#include "mupdf/fitz/write-pixmap.h"
#include "mupdf/fitz/output-svg.h"

#ifdef __cplusplus
}
#endif

#endif
