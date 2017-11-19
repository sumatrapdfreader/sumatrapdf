/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace path {

bool IsSep(char c);

const char *GetBaseName(const char *path);
const char *GetExt(const char *path);

char *JoinUtf(const char *path, const char *fileName, Allocator *allocator);

bool IsSep(WCHAR c);
const WCHAR *GetBaseName(const WCHAR *path);
const WCHAR *GetExt(const WCHAR *path);

WCHAR *Normalize(const WCHAR *path);
WCHAR *ShortPath(const WCHAR *path);
bool IsSame(const WCHAR *path1, const WCHAR *path2);
bool HasVariableDriveLetter(const WCHAR *path);
bool IsOnFixedDrive(const WCHAR *path);
bool Match(const WCHAR *path, const WCHAR *filter);
bool IsAbsolute(const WCHAR *path);

WCHAR *GetDir(const WCHAR *path);
WCHAR *Join(const WCHAR *path, const WCHAR *fileName);

WCHAR *GetTempPath(const WCHAR *filePrefix = nullptr);
WCHAR *GetAppPath(const WCHAR *fileName = nullptr);
}

namespace file {

FILE *OpenFILE(const WCHAR *path);
FILE *OpenFILE(const char *path);

bool Exists(const WCHAR *path);
char *ReadAll(const WCHAR *path, size_t *fileSizeOut, Allocator *allocator = nullptr);
char *ReadAllUtf(const char *path, size_t *fileSizeOut, Allocator *allocator = nullptr);
bool ReadN(const WCHAR *path, char *buf, size_t toRead);
bool WriteAll(const WCHAR *path, const void *data, size_t dataLen);
bool WriteAllUtf(const char *path, const void *data, size_t dataLen);
int64 GetSize(const WCHAR *path);
bool Delete(const WCHAR *path);
FILETIME GetModificationTime(const WCHAR *path);
bool SetModificationTime(const WCHAR *path, FILETIME lastMod);
bool StartsWithN(const WCHAR *path, const char *magicNumber, size_t len);
bool StartsWith(const WCHAR *path, const char *magicNumber);
int GetZoneIdentifier(const WCHAR *path);
bool SetZoneIdentifier(const WCHAR *path, int zoneId = URLZONE_INTERNET);

HANDLE OpenReadOnly(const WCHAR *path);
}

namespace dir {

bool Exists(const WCHAR *dir);
bool Create(const WCHAR *dir);
bool CreateAll(const WCHAR *dir);
}

inline bool FileTimeEq(const FILETIME &a, const FILETIME &b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

HINSTANCE GetInstance();
