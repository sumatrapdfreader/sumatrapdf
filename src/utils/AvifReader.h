/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool HasAvifSignature(const ByteSlice&);
Size AvifSizeFromData(const ByteSlice&);
Gdiplus::Bitmap* AvifImageFromData(const ByteSlice&);
