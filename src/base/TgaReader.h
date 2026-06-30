/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Support for Truevision TGA files
// (as e.g. produced by EngineDump and mudraw)
// spec: http://www.gamers.org/dEngine/quake3/TGA.ps.gz

struct Pixmap;

namespace tga {

bool HasSignature(const ByteSlice&);
Pixmap* PixmapFromData(const ByteSlice&);

ByteSlice SerializeBitmap(HBITMAP hbmp);

} // namespace tga
