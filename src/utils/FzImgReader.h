/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef FzImgReader_h
#define FzImgReader_h

#include "BaseUtil.h"

namespace fitz {

Gdiplus::Bitmap *ImageFromData(const char *data, size_t len);

}

#endif
