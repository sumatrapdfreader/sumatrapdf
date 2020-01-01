/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace webp {

bool HasSignature(const char* data, size_t len);
Gdiplus::Size SizeFromData(const char* data, size_t len);
Gdiplus::Bitmap* ImageFromData(const char* data, size_t len);

} // namespace webp
