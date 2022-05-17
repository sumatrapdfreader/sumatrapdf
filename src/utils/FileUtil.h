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
WCHAR* ShortPath(const WCHAR* path);
bool IsSame(const WCHAR* path1, const WCHAR* path2);
bool HasVariableDriveLetter(const WCHAR* path);
bool HasVariableDriveLetter(const char* path);
bool IsOnFixedDrive(const WCHAR* path);
bool Match(const WCHAR* path, const WCHAR* filter);
bool IsAbsolute(const WCHAR* path);
bool IsAbsolute(const char* path);

WCHAR* GetTempFilePath(const WCHAR* filePrefix = nullptr);
WCHAR* GetPathOfFileInAppDir(const WCHAR* fileName = nullptr);
} // namespace path

namespace file {

bool Exists(const char* path);
bool Exists(const WCHAR* path);

FILE* OpenFILE(const char* path);
FILE* OpenFILE(const WCHAR* path);
HANDLE OpenReadOnly(const char*);

ByteSlice ReadFileWithAllocator(const char* path, Allocator*);

ByteSlice ReadFile(const char* path);
ByteSlice ReadFile(const WCHAR* filePath);

int ReadN(const char* path, char* buf, size_t toRead);

i64 GetSize(const char*);

bool WriteFile(const WCHAR* path, ByteSlice);
bool WriteFile(const char* path, ByteSlice);

bool Delete(const WCHAR* path);
bool Delete(const char* path);

FILETIME GetModificationTime(const char* path);

bool SetModificationTime(const WCHAR* path, FILETIME lastMod);

bool StartsWithN(const WCHAR* path, const char* s, size_t len);
bool StartsWith(const WCHAR* path, const char* s);
bool StartsWith(const char* path, const char* s);

int GetZoneIdentifier(const char* path);
bool SetZoneIdentifier(const char* path, int zoneId = URLZONE_INTERNET);
bool DeleteZoneIdentifier(const char* path);

bool Copy(const WCHAR* dst, const WCHAR* src, bool dontOverwrite);
bool Copy(const char* dst, const char* src, bool dontOverwrite);

} // namespace file

namespace dir {

bool Exists(const WCHAR* dir);
bool Exists(const char*);

bool Create(const WCHAR* dir);
bool CreateForFile(const WCHAR* path);
bool CreateAll(const WCHAR* dir);
bool RemoveAll(const WCHAR* dir);
} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b);
