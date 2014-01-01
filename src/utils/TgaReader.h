/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef TgaReader_h
#define TgaReader_h

// Support for Truevision TGA files
// (as e.g. produced by EngineDump and mudraw)
// spec: http://www.gamers.org/dEngine/quake3/TGA.ps.gz

#include "BaseUtil.h"

namespace tga {

bool                HasSignature(const char *data, size_t len);
Gdiplus::Bitmap *   ImageFromData(const char *data, size_t len);

unsigned char *     SerializeBitmap(HBITMAP hbmp, size_t *bmpBytesOut);

}

#endif
