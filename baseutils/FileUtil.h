/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: Simplified BSD (see ./COPYING) */

#ifndef FileUtil_h
#define FileUtil_h

namespace Path {

const WCHAR *GetBaseName(const WCHAR *path);
const char * GetBaseName(const char *path);

TCHAR *      GetDir(const TCHAR *path);
TCHAR *      Join(const TCHAR *path, const TCHAR *filename);
TCHAR *      Normalize(const TCHAR *path);
bool         IsSame(const TCHAR *path1, const TCHAR *path2);

}

namespace File {

bool         Exists(const TCHAR *filePath);
char *       ReadAll(const TCHAR *filePath, size_t *fileSizeOut);
bool         ReadAll(const TCHAR *filePath, char *buffer, size_t bufferLen);
bool         WriteAll(const TCHAR *filePath, void *data, size_t dataLen);
size_t       GetSize(const TCHAR *filePath);
bool         Delete(const TCHAR *filePath);
FILETIME     GetModificationTime(const TCHAR *filePath);

}

namespace Dir {

bool         Exists(const TCHAR *dir);
bool         Create(const TCHAR *dir);

}

#endif
