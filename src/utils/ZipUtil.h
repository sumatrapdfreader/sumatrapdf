/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ZipUtil_h
#define ZipUtil_h

#include "BaseUtil.h"
class Allocator;

struct FileToUnzip {
    const char *    fileName;
    const TCHAR *   unzippedName; // optional, will use fileNamePrefix if NULL
};

bool UnzipFiles(Allocator *allocator, const TCHAR *zipFile, FileToUnzip *files, const TCHAR *dir);

#endif
