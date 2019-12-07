/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace path {

bool IsSep(char c);

const char* GetBaseNameNoFree(const char* path);
const char* GetExt(const char* path);

char* JoinUtf(const char* path, const char* fileName, Allocator* allocator);

#if OS_WIN
bool IsSep(WCHAR c);
const WCHAR* GetBaseNameNoFree(const WCHAR* path);
const WCHAR* GetExt(const WCHAR* path);

WCHAR* Normalize(const WCHAR* path);
WCHAR* ShortPath(const WCHAR* path);
bool IsSame(const WCHAR* path1, const WCHAR* path2);
bool HasVariableDriveLetter(const WCHAR* path);
bool IsOnFixedDrive(const WCHAR* path);
bool Match(const WCHAR* path, const WCHAR* filter);
bool IsAbsolute(const WCHAR* path);

WCHAR* GetDir(const WCHAR* path);
WCHAR* Join(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2 = nullptr);

WCHAR* GetTempPath(const WCHAR* filePrefix = nullptr);
WCHAR* GetPathOfFileInAppDir(const WCHAR* fileName = nullptr);
#endif
} // namespace path

namespace file {

FILE* OpenFILE(const char* path);
char* ReadFileWithAllocator(const char* path, size_t* fileSizeOut, Allocator* allocator);
std::tuple<char*, size_t> ReadFileWithAllocator(const char* filePath, Allocator* allocator);
bool WriteFile(const char* path, const void* data, size_t dataLen);

std::tuple<char*, size_t> ReadFile2(std::string_view path);
std::tuple<char*, size_t> ReadFile2(const WCHAR* filePath);
OwnedData ReadFile(std::string_view path);
OwnedData ReadFile(const char* path);

bool Exists(std::string_view path);

#if OS_WIN
FILE* OpenFILE(const WCHAR* path);
bool Exists(const WCHAR* path);
char* ReadFileWithAllocator(const WCHAR* path, size_t* fileSizeOut, Allocator* allocator);
std::tuple<char*, size_t> ReadFileWithAllocator(const WCHAR* filePath, Allocator* allocator);
OwnedData ReadFile(const WCHAR* path);
bool ReadN(const WCHAR* path, char* buf, size_t toRead);
bool WriteFile(const WCHAR* path, const void* data, size_t dataLen);
int64_t GetSize(const WCHAR* path);
bool Delete(const WCHAR* path);
FILETIME GetModificationTime(const WCHAR* path);
bool SetModificationTime(const WCHAR* path, FILETIME lastMod);
bool StartsWithN(const WCHAR* path, const char* magicNumber, size_t len);
bool StartsWith(const WCHAR* path, const char* magicNumber);
int GetZoneIdentifier(const WCHAR* path);
bool SetZoneIdentifier(const WCHAR* path, int zoneId = URLZONE_INTERNET);

HANDLE OpenReadOnly(const WCHAR* path);
#endif
} // namespace file

namespace dir {

#if OS_WIN
bool Exists(const WCHAR* dir);
bool Create(const WCHAR* dir);
bool CreateAll(const WCHAR* dir);
bool RemoveAll(const WCHAR* dir);
#endif
} // namespace dir

#if OS_WIN
inline bool FileTimeEq(const FILETIME& a, const FILETIME& b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}
#endif
