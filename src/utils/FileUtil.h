/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace path {

bool IsSep(char c);

std::string_view GetBaseName(std::string_view path);

const char* GetBaseNameTemp(const char* path);
const char* GetExtTemp(const char* path);

char* Join(const char* path, const char* fileName, Allocator* allocator);
WCHAR* Join(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);

std::string_view GetDir(std::string_view path);
bool IsDirectory(std::string_view);
bool IsDirectory(std::wstring_view);

bool IsSep(WCHAR c);
const WCHAR* GetBaseNameTemp(const WCHAR* path);
const WCHAR* GetExtTemp(const WCHAR* path);

WCHAR* Normalize(const WCHAR* path);
WCHAR* ShortPath(const WCHAR* path);
bool IsSame(const WCHAR* path1, const WCHAR* path2);
bool HasVariableDriveLetter(const WCHAR* path);
bool HasVariableDriveLetter(const char* path);
bool IsOnFixedDrive(const WCHAR* path);
bool Match(const WCHAR* path, const WCHAR* filter);
bool IsAbsolute(const WCHAR* path);

WCHAR* GetDir(const WCHAR* path);

WCHAR* GetTempFilePath(const WCHAR* filePrefix = nullptr);
WCHAR* GetPathOfFileInAppDir(const WCHAR* fileName = nullptr);
} // namespace path

namespace file {

FILE* OpenFILE(const char* path);
ByteSlice ReadFileWithAllocator(const char* path, Allocator*);
bool WriteFile(const char* path, ByteSlice);

ByteSlice ReadFile(std::string_view path);

bool Exists(std::string_view path);

FILE* OpenFILE(const WCHAR* path);
bool Exists(const WCHAR* path);
ByteSlice ReadFileWithAllocator(const WCHAR* filePath, Allocator* allocator);
ByteSlice ReadFile(const WCHAR* filePath);

i64 GetSize(std::string_view path);

int ReadN(const WCHAR* path, char* buf, size_t toRead);
bool WriteFile(const WCHAR* path, ByteSlice);
bool Delete(const WCHAR* path);
bool Delete(const char* path);
FILETIME GetModificationTime(const WCHAR* path);
FILETIME GetModificationTime(const char* path);
bool SetModificationTime(const WCHAR* path, FILETIME lastMod);
bool StartsWithN(const WCHAR* path, const char* s, size_t len);
bool StartsWith(const WCHAR* path, const char* s);

int GetZoneIdentifier(const char* path);
bool SetZoneIdentifier(const char* path, int zoneId = URLZONE_INTERNET);
bool DeleteZoneIdentifier(const char* path);

HANDLE OpenReadOnly(const WCHAR* path);

bool Copy(const WCHAR* dst, const WCHAR* src, bool dontOverwrite);

} // namespace file

namespace dir {

bool Exists(const WCHAR* dir);
bool Create(const WCHAR* dir);
bool CreateForFile(const WCHAR* path);
bool CreateAll(const WCHAR* dir);
bool RemoveAll(const WCHAR* dir);
} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b);
