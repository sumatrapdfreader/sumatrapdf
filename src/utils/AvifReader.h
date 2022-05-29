/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool HasAvifSignature(ByteSlice);
Size AvifSizeFromData(ByteSlice);
Gdiplus::Bitmap* AvifImageFromData(ByteSlice);
