/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef FileUtil_h
#define FileUtil_h

namespace path {

const WCHAR *GetBaseName(const WCHAR *path);
const char * GetBaseName(const char *path);
const TCHAR *GetExt(const TCHAR *path);

TCHAR *      GetDir(const TCHAR *path);
TCHAR *      Join(const TCHAR *path, const TCHAR *filename);
TCHAR *      Normalize(const TCHAR *path);
bool         IsSame(const TCHAR *path1, const TCHAR *path2);
bool         IsOnRemovableDrive(const TCHAR *path);
bool         Match(const TCHAR *path, const TCHAR *filter);

}

namespace file {

bool         Exists(const TCHAR *filePath);
char *       ReadAll(const TCHAR *filePath, size_t *fileSizeOut);
bool         ReadAll(const TCHAR *filePath, char *buffer, size_t bufferLen);
bool         WriteAll(const TCHAR *filePath, void *data, size_t dataLen);
size_t       GetSize(const TCHAR *filePath);
bool         Delete(const TCHAR *filePath);
FILETIME     GetModificationTime(const TCHAR *filePath);
bool         SetModificationTime(const TCHAR *filePath, FILETIME lastMod);
bool         StartsWith(const TCHAR *filePath, const char *magicNumber, size_t len=-1);
int          GetZoneIdentifier(const TCHAR *filePath);
bool         SetZoneIdentifier(const TCHAR *filePath, int zoneId=URLZONE_INTERNET);

}

namespace dir {

bool         Exists(const TCHAR *dir);
bool         Create(const TCHAR *dir);
bool         CreateAll(const TCHAR *dir);

}

#endif
