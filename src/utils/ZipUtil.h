/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef ZipUtil_h
#define ZipUtil_h

#include "BaseUtil.h"

struct FileToUnzip {
    const char *    fileNamePrefix;
    const TCHAR *   unzippedName; // optional, will use fileNamePrefix if NULL
    bool            wasUnzipped;
};

bool UnzipFilesStartingWith(const TCHAR *zipFile, FileToUnzip *files, const TCHAR *dir);

#endif

