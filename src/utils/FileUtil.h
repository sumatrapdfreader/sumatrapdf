/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace path {

bool IsSep(char c);
bool IsSep(WCHAR c);

const char* GetExtTemp(const char* path);
const WCHAR* GetExtTemp(const WCHAR* path);

const char* GetBaseNameTemp(const char* path);
const WCHAR* GetBaseNameTemp(const WCHAR* path);

TempStr GetDirTemp(const char* path);
TempWstr GetDirTemp(const WCHAR* path);

char* Join(const char* path, const char* fileName, Allocator* allocator = nullptr);
WCHAR* Join(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);
TempStr JoinTemp(const char* path, const char* fileName, const char* fileName2 = nullptr);
TempWstr JoinTemp(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);

bool IsDirectory(const char*);

WCHAR* Normalize(const WCHAR* path);
char* NormalizeTemp(const char* path);

WCHAR* ShortPath(const WCHAR* path);
char* ShortPath(const char* pathA);

bool IsSame(const WCHAR* path1, const WCHAR* path2);
bool IsSame(const char* path1, const char* path2);

bool HasVariableDriveLetter(const WCHAR* path);
bool HasVariableDriveLetter(const char* path);

bool IsOnFixedDrive(const WCHAR* path);
bool IsOnFixedDrive(const char* path);

bool Match(const WCHAR* path, const WCHAR* filter);
bool Match(const char* path, const char* filter);

bool IsAbsolute(const WCHAR* path);
bool IsAbsolute(const char* path);

WCHAR* GetTempFilePath(const WCHAR* filePrefix = nullptr);
char* GetTempFilePath(const char* filePrefix = nullptr);

WCHAR* GetPathOfFileInAppDir(const WCHAR* fileName = nullptr);
char* GetPathOfFileInAppDir(const char* fileName = nullptr);
} // namespace path

namespace file {

bool Exists(const char* path);
bool Exists(const WCHAR* path);

FILE* OpenFILE(const char* path);
HANDLE OpenReadOnly(const char*);

ByteSlice ReadFileWithAllocator(const char* path, Allocator*);

ByteSlice ReadFile(const char* path);

int ReadN(const char* path, char* buf, size_t toRead);

i64 GetSize(const char*);

bool WriteFile(const char* path, ByteSlice);

bool Delete(const WCHAR* path);
bool Delete(const char* path);

FILETIME GetModificationTime(const char* path);

bool SetModificationTime(const WCHAR* path, FILETIME lastMod);

DWORD GetAttributes(const char* path);
bool SetAttributes(const char* path, DWORD attrs);

bool StartsWithN(const char* path, const char* s, size_t len);
bool StartsWith(const char* path, const char* s);

int GetZoneIdentifier(const char* path);
bool SetZoneIdentifier(const char* path, int zoneId = URLZONE_INTERNET);
bool DeleteZoneIdentifier(const char* path);

bool Copy(const char* dst, const char* src, bool dontOverwrite);

} // namespace file

namespace dir {

bool Exists(const WCHAR* dir);
bool Exists(const char*);

bool Create(const WCHAR* dir);
bool Create(const char* dir);

bool CreateForFile(const WCHAR* path);
bool CreateForFile(const char* path);

bool CreateAll(const WCHAR* dir);
bool CreateAll(const char* dir);

bool RemoveAll(const WCHAR* dir);
bool RemoveAll(const char* dir);

} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b);
