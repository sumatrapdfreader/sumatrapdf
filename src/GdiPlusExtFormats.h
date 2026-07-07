/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusExtFormats_h
#define GdiPlusExtFormats_h

struct Pixmap;
struct Size;
struct ByteReader;
using Kind = const char*;

Pixmap* PixmapFromExtFormatsData(Str bmpData, Kind kind);
bool WebpImageSizeFromData(ByteReader r, Size& result);
bool AvifImageSizeFromData(ByteReader r, Size& result);
int JpegExifOrientationFromTiff(ByteReader r, int tiffBase);

#endif