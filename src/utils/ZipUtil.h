/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef ZipUtil_h
#define ZipUtil_h

#include "BaseUtil.h"

struct FileToUnzip {
    const char *    fileName;
    const TCHAR *   unzippedName; // optional, will use fileNamePrefix if NULL
};

bool UnzipFiles(const TCHAR *zipFile, FileToUnzip *files, const TCHAR *dir);

#endif
