/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

namespace jxl {

bool HasSignature(Str);
Size SizeFromData(Str);
Pixmap* PixmapFromData(Str);

} // namespace jxl
