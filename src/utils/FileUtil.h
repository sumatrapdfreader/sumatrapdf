/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef FileUtil_h
#define FileUtil_h

namespace path {

bool         IsSep(WCHAR c);
bool         IsSep(char c);

const WCHAR *GetBaseName(const WCHAR *path);
const char * GetBaseName(const char *path);
const WCHAR *GetExt(const WCHAR *path);

WCHAR *      GetDir(const WCHAR *path);
WCHAR *      Join(const WCHAR *path, const WCHAR *filename);
char *       JoinUtf(const char *path, const char *fileName, Allocator *allocator);
WCHAR *      Normalize(const WCHAR *path);
WCHAR *      ShortPath(const WCHAR *path);
bool         IsSame(const WCHAR *path1, const WCHAR *path2);
bool         HasVariableDriveLetter(const WCHAR *path);
bool         IsOnFixedDrive(const WCHAR *path);
bool         Match(const WCHAR *path, const WCHAR *filter);
bool         IsAbsolute(const WCHAR *path);

WCHAR *      GetTempPath(const WCHAR *filePrefix=NULL);

}

namespace file {

bool         Exists(const WCHAR *filePath);
char *       ReadAll(const WCHAR *filePath, size_t *fileSizeOut, Allocator *allocator=NULL);
char *       ReadAllUtf(const char *filePath, size_t *fileSizeOut, Allocator *allocator=NULL);
bool         ReadAll(const WCHAR *filePath, char *buffer, size_t bufferLen);
bool         WriteAll(const WCHAR *filePath, const void *data, size_t dataLen);
bool         WriteAllUtf(const char *filePath, const void *data, size_t dataLen);
int64        GetSize(const WCHAR *filePath);
bool         Delete(const WCHAR *filePath);
FILETIME     GetModificationTime(const WCHAR *filePath);
bool         SetModificationTime(const WCHAR *filePath, FILETIME lastMod);
bool         StartsWith(const WCHAR *filePath, const char *magicNumber, size_t len=-1);
int          GetZoneIdentifier(const WCHAR *filePath);
bool         SetZoneIdentifier(const WCHAR *filePath, int zoneId=URLZONE_INTERNET);

}

namespace dir {

bool         Exists(const WCHAR *dir);
bool         Create(const WCHAR *dir);
bool         CreateAll(const WCHAR *dir);

}

#endif
