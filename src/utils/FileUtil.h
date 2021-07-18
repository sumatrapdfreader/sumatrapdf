/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace path {

bool IsSep(char c);

std::string_view GetBaseName(std::string_view path);

const char* GetBaseNameTemp(const char* path);
const char* GetExtNoFreeTemp(const char* path);

char* Join(const char* path, const char* fileName, Allocator* allocator);

std::string_view GetDir(std::string_view path);
bool IsDirectory(std::string_view);
bool IsDirectory(std::wstring_view);

bool IsSep(WCHAR c);
const WCHAR* GetBaseNameTemp(const WCHAR* path);
const WCHAR* GetExtNoFreeTemp(const WCHAR* path);

WCHAR* Normalize(const WCHAR* path);
WCHAR* ShortPath(const WCHAR* path);
bool IsSame(const WCHAR* path1, const WCHAR* path2);
bool HasVariableDriveLetter(const WCHAR* path);
bool IsOnFixedDrive(const WCHAR* path);
bool Match(const WCHAR* path, const WCHAR* filter);
bool IsAbsolute(const WCHAR* path);

WCHAR* GetDir(const WCHAR* path);
WCHAR* Join(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);

WCHAR* GetTempFilePath(const WCHAR* filePrefix = nullptr);
WCHAR* GetPathOfFileInAppDir(const WCHAR* fileName = nullptr);
} // namespace path

namespace file {

FILE* OpenFILE(const char* path);
std::span<u8> ReadFileWithAllocator(const char* path, Allocator*);
bool WriteFile(const char* path, std::span<u8>);

std::span<u8> ReadFile(std::string_view path);

bool Exists(std::string_view path);

FILE* OpenFILE(const WCHAR* path);
bool Exists(const WCHAR* path);
std::span<u8> ReadFileWithAllocator(const WCHAR* filePath, Allocator* allocator);
std::span<u8> ReadFile(const WCHAR* filePath);

i64 GetSize(std::string_view path);

int ReadN(const WCHAR* path, char* buf, size_t toRead);
bool WriteFile(const WCHAR* path, std::span<u8>);
bool Delete(const WCHAR* path);
FILETIME GetModificationTime(const WCHAR* path);
bool SetModificationTime(const WCHAR* path, FILETIME lastMod);
bool StartsWithN(const WCHAR* path, const char* magicNumber, size_t len);
bool StartsWith(const WCHAR* path, const char* magicNumber);

int GetZoneIdentifier(const char* path);
bool SetZoneIdentifier(const char* path, int zoneId = URLZONE_INTERNET);
bool DeleteZoneIdentifier(const char* path);

HANDLE OpenReadOnly(const WCHAR* path);

bool Copy(const WCHAR* dst, const WCHAR* src, bool dontOverwrite);

} // namespace file

namespace dir {

bool Exists(const WCHAR* dir);
bool Create(const WCHAR* dir);
bool CreateAll(const WCHAR* dir);
bool RemoveAll(const WCHAR* dir);
} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b);
