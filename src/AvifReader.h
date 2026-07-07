/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

Size AvifSizeFromData(Str);
Pixmap* PixmapFromAvifData(Str);
// Returns TIFF EXIF payload (caller frees *outData). Skips 4-byte HEIF Exif prefix.
bool AvifExifBlobFromData(Str d, u8** outData, size_t* outSize);