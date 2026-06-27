/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

namespace jxl {

bool HasSignature(const ByteSlice&);
Size SizeFromData(const ByteSlice&);
Pixmap* PixmapFromData(const ByteSlice&);

} // namespace jxl
