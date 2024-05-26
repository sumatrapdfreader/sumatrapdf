/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef NO_AVIF

Size AvifSizeFromData(const ByteSlice&);
Gdiplus::Bitmap* AvifImageFromData(const ByteSlice&);

#endif