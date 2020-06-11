/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace webp {

bool HasSignature(std::span<u8>);
Gdiplus::Size SizeFromData(const u8* data, size_t len);
Gdiplus::Bitmap* ImageFromData(const u8* data, size_t len);

} // namespace webp
