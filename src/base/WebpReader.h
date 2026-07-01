/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

namespace webp {

bool HasSignature(const Str&);
Size SizeFromData(const Str&);
Pixmap* PixmapFromData(const Str&);

} // namespace webp
