/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace webp {

bool HasSignature(ByteSlice);
Size SizeFromData(ByteSlice);
Gdiplus::Bitmap* ImageFromData(ByteSlice);

} // namespace webp
