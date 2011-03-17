/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef FileUtil_h
#define FileUtil_h

namespace Path {

const TCHAR *GetBaseName(const TCHAR *path);
TCHAR *      GetDir(const TCHAR *path);
TCHAR *      Join(const TCHAR *path, const TCHAR *filename);
TCHAR *      Normalize(const TCHAR *path);
bool         IsSame(const TCHAR *path1, const TCHAR *path2);

}

namespace File {

bool         Exists(const TCHAR *filePath);
char *       ReadAll(const TCHAR *filePath, size_t *fileSizeOut);
bool         WriteAll(const TCHAR *filePath, void *data, size_t dataLen);
size_t       GetSize(const TCHAR *filePath);

}

#endif
