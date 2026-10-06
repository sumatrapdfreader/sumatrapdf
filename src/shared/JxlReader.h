/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

namespace jxl {

Pixmap* PixmapFromData(Str);
bool DecodeRgbInto(Str, DecodeDstAllocFn, void* user);

} // namespace jxl
