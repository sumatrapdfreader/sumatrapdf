/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

Pixmap* PixmapFromAvifData(Str);
bool AvifExifBlobFromData(Str d, u8** outData, size_t* outSize);
