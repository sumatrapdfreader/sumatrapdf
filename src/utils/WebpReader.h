/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace webp {

bool HasSignature(std::span<u8>);
Size SizeFromData(std::span<u8>);
Gdiplus::Bitmap* ImageFromData(std::span<u8>);

} // namespace webp
