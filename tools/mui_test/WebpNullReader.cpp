/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "WebpReader.h"

namespace webp {

// dummy methods for when libwebp hasn't been built
bool HasSignature(const char *data, size_t len) { return false; }
Gdiplus::Size SizeFromData(const char *data, size_t len) { return Gdiplus::Size(); }
Gdiplus::Bitmap *ImageFromData(const char *data, size_t len) { return NULL; }

}
