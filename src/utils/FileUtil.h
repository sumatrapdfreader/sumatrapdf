/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace path {

bool IsSep(char c);

TempStr GetExtTemp(const char* path);
TempStr GetBaseNameTemp(const char* path);
TempStr GetPathNoExtTemp(const char* path);

TempStr GetDirTemp(const char* path);
TempWStr GetDirTemp(const WCHAR* path);

char* Join(Allocator* allocator, const char* path, const char* fileName);
char* Join(const char* path, const char* fileName);
WCHAR* Join(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);
TempStr JoinTemp(const char* path, const char* fileName, const char* fileName2 = nullptr);
TempWStr JoinTemp(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);

bool IsDirectory(const char*);

WCHAR* Normalize(const WCHAR* path);
char* NormalizeTemp(const char* path);

char* ShortPath(const char* pathA);
bool IsSame(const char* path1, const char* path2);
bool HasVariableDriveLetter(const char* path);
bool IsOnFixedDrive(const char* path);
bool IsAbsolute(const char* path);

bool Match(const char* path, const char* filter);

char* GetTempFilePath(const char* filePrefix = nullptr);
TempStr GetPathOfFileInAppDirTemp(const char* fileName = nullptr);
} // namespace path

namespace file {

bool Exists(const char* path);

FILE* OpenFILE(const char* path);
HANDLE OpenReadOnly(const char*);
ByteSlice ReadFileWithAllocator(const char* path, Allocator*);
ByteSlice ReadFile(const char* path);
int ReadN(const char* path, char* buf, size_t toRead);
bool WriteFile(const char* path, const ByteSlice&);

i64 GetSize(const char*);
bool Delete(const char* path);

FILETIME GetModificationTime(const char* path);

bool SetModificationTime(const char* path, FILETIME lastMod);

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

bool Create(const char* dir);
bool CreateForFile(const char* path);
bool CreateAll(const char* dir);
bool RemoveAll(const char* dir);

} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b);
