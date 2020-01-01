/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Support for Truevision TGA files
// (as e.g. produced by EngineDump and mudraw)
// spec: http://www.gamers.org/dEngine/quake3/TGA.ps.gz

namespace tga {

bool HasSignature(const char* data, size_t len);
Gdiplus::Bitmap* ImageFromData(const char* data, size_t len);

unsigned char* SerializeBitmap(HBITMAP hbmp, size_t* bmpBytesOut);

} // namespace tga
