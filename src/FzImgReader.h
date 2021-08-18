/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

Gdiplus::Bitmap* FzImageFromData(ByteSlice);

Gdiplus::Bitmap* BitmapFromData(ByteSlice);
RenderedBitmap* LoadRenderedBitmap(const char* path);
