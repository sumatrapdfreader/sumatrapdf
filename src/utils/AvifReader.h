/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

Size AvifSizeFromData(const ByteSlice&);
Gdiplus::Bitmap* AvifImageFromData(const ByteSlice&);
// Returns TIFF EXIF payload (caller frees *outData). Skips 4-byte HEIF Exif prefix.
bool AvifExifBlobFromData(const ByteSlice& d, u8** outData, size_t* outSize);