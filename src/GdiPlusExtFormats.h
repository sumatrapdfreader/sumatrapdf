/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusExtFormats_h
#define GdiPlusExtFormats_h

struct Pixmap;
struct Size;
struct ByteReader;
enum class FileType : u8;

Pixmap* PixmapFromExtFormatsData(Str bmpData, FileType kind);
bool WebpImageSizeFromData(ByteReader r, Size& result);
bool AvifImageSizeFromData(ByteReader r, Size& result);

#endif