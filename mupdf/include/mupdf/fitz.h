#ifndef MUDPF_FITZ_H
#define MUDPF_FITZ_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mupdf/fitz/version.h"
#include "mupdf/fitz/config.h"
#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

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
#include "mupdf/fitz/output.h"
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
