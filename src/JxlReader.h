/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

namespace jxl {

bool HasSignature(Str);
Size SizeFromData(Str);
Pixmap* PixmapFromData(Str);
typedef u8* (*AllocDstFn)(void* user, int dx, int dy, bool hasAlpha, int* stride);
bool DecodeRgbInto(Str, AllocDstFn, void* user);

} // namespace jxl
