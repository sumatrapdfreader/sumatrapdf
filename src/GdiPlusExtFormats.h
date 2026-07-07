/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef GdiPlusExtFormats_h
#define GdiPlusExtFormats_h

struct Pixmap;
enum class FileType : u8;

Pixmap* PixmapFromExtFormatsData(Str bmpData, FileType kind);

#endif