/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Pixmap;

Size AvifSizeFromData(const ByteSlice&);
Pixmap* PixmapFromAvifData(const ByteSlice&);
// Returns TIFF EXIF payload (caller frees *outData). Skips 4-byte HEIF Exif prefix.
bool AvifExifBlobFromData(const ByteSlice& d, u8** outData, size_t* outSize);