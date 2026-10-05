/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

Size AvifSizeFromData(Str);
Pixmap* PixmapFromAvifData(Str);
bool AvifExifBlobFromData(Str d, u8** outData, size_t* outSize);