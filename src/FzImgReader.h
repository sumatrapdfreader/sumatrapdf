/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

Gdiplus::Bitmap* FzImageFromData(const ByteSlice&);

Gdiplus::Bitmap* BitmapFromData(const ByteSlice&);
RenderedBitmap* LoadRenderedBitmap(const char* path);
