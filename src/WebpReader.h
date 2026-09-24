/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

namespace webp {

Pixmap* PixmapFromData(const Str&);
bool DecodeRgbInto(Str, DecodeDstAllocFn, void* user);

} // namespace webp
