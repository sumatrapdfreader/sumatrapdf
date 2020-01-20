/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"

#if OS_WIN
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#endif

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
#define ZERO_PADDING_COUNT 3

namespace path {

bool IsSep(char c) {
    return '\\' == c || '/' == c;
}

// Note: returns pointer inside <path>, do not free
const char* GetBaseNameNoFree(const char* path) {
    const char* fileBaseName = path + str::Len(path);
    for (; fileBaseName > path; fileBaseName--) {
        if (IsSep(fileBaseName[-1])) {
            break;
        }
    }
    return fileBaseName;
}

// Note: returns pointer inside <path>, do not free
const char* GetExtNoFree(const char* path) {
    const char* ext = nullptr;
    char c = *path;
    while (c) {
        if (c == '.') {
            ext = path;
        } else if (IsSep(c)) {
            ext = nullptr;
        }
        path++;
        c = *path;
    }
    if (nullptr == ext) {
        return path; // empty string
    }
    return ext;
}

char* JoinUtf(const char* path, const char* fileName, Allocator* allocator) {
    if (IsSep(*fileName)) {
        fileName++;
    }
    const char* sepStr = nullptr;
    if (!IsSep(path[str::Len(path) - 1])) {
        sepStr = "\\";
    }
    return str::Join(path, sepStr, fileName, allocator);
}

#if OS_WIN
bool IsSep(WCHAR c) {
    return '\\' == c || '/' == c;
}

// Note: returns pointer inside <path>, do not free
const WCHAR* GetBaseNameNoFree(const WCHAR* path) {
    const WCHAR* end = path + str::Len(path);
    while (end > path) {
        if (IsSep(end[-1])) {
            break;
        }
        --end;
    }
    return end;
}

// Note: returns pointer inside <path>, do not free
const WCHAR* GetExtNoFree(const WCHAR* path) {
    const WCHAR* ext = path + str::Len(path);
    while ((ext > path) && !IsSep(*ext)) {
        if (*ext == '.') {
            return ext;
        }
        ext--;
    }
    return path + str::Len(path);
}

// Caller has to free()
WCHAR* GetDir(const WCHAR* path) {
    const WCHAR* baseName = GetBaseNameNoFree(path);
    if (baseName == path) {
        // relative directory
        return str::Dup(L".");
    }
    if (baseName == path + 1) {
        // relative root
        return str::DupN(path, 1);
    }
    if (baseName == path + 3 && path[1] == ':') {
        // local drive root
        return str::DupN(path, 3);
    }
    if (baseName == path + 2 && str::StartsWith(path, L"\\\\")) {
        // server root
        return str::Dup(path);
    }
    // any subdirectory
    return str::DupN(path, baseName - path - 1);
}

WCHAR* Join(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2) {
    if (IsSep(*fileName)) {
        fileName++;
    }
    WCHAR* sepStr = nullptr;
    if (!IsSep(path[str::Len(path) - 1])) {
        sepStr = L"\\";
    }
    WCHAR* res = str::Join(path, sepStr, fileName);
    if (fileName2) {
        WCHAR* toFree = res;
        res = Join(res, fileName2);
        free(toFree);
    }
    return res;
}

// Normalize a file path.
//  remove relative path component (..\ and .\),
//  replace slashes by backslashes,
//  convert to long form.
//
// Returns a pointer to a memory allocated block containing the normalized string.
//   The caller is responsible for freeing the block.
//   Returns nullptr if the file does not exist or if a memory allocation fails.
//
// Precondition: the file must exist on the file system.
//
// Note:
//   - the case of the root component is preserved
//   - the case of rest is set to the way it is stored on the file system
//
// e.g. suppose the a file "C:\foo\Bar.Pdf" exists on the file system then
//    "c:\foo\bar.pdf" becomes "c:\foo\Bar.Pdf"
//    "C:\foo\BAR.PDF" becomes "C:\foo\Bar.Pdf"
WCHAR* Normalize(const WCHAR* path) {
    // convert to absolute path, change slashes into backslashes
    DWORD cch = GetFullPathName(path, 0, nullptr, nullptr);
    if (!cch) {
        return str::Dup(path);
    }

    AutoFreeWstr fullpath(AllocArray<WCHAR>(cch));
    GetFullPathName(path, cch, fullpath, nullptr);
    // convert to long form
    cch = GetLongPathName(fullpath, nullptr, 0);
    if (!cch) {
        return fullpath.StealData();
    }

    AutoFreeWstr normpath(AllocArray<WCHAR>(cch));
    GetLongPathName(fullpath, normpath, cch);
    if (cch <= MAX_PATH) {
        return normpath.StealData();
    }

    // handle overlong paths: first, try to shorten the path
    cch = GetShortPathName(fullpath, nullptr, 0);
    if (cch && cch <= MAX_PATH) {
        AutoFreeWstr shortpath(AllocArray<WCHAR>(cch));
        GetShortPathName(fullpath, shortpath, cch);
        if (str::Len(path::GetBaseNameNoFree(normpath)) + path::GetBaseNameNoFree(shortpath) - shortpath < MAX_PATH) {
            // keep the long filename if possible
            *(WCHAR*)path::GetBaseNameNoFree(shortpath) = '\0';
            return str::Join(shortpath, path::GetBaseNameNoFree(normpath));
        }
        return shortpath.StealData();
    }
    // else mark the path as overlong
    if (str::StartsWith(normpath.Get(), L"\\\\?\\")) {
        return normpath.StealData();
    }
    return str::Join(L"\\\\?\\", normpath);
}

// Normalizes the file path and the converts it into a short form that
// can be used for interaction with non-UNICODE aware applications
WCHAR* ShortPath(const WCHAR* path) {
    AutoFreeWstr normpath(Normalize(path));
    DWORD cch = GetShortPathName(normpath, nullptr, 0);
    if (!cch) {
        return normpath.StealData();
    }
    WCHAR* shortpath = AllocArray<WCHAR>(cch);
    GetShortPathName(normpath, shortpath, cch);
    return shortpath;
}

static bool IsSameFileHandleInformation(BY_HANDLE_FILE_INFORMATION& fi1, BY_HANDLE_FILE_INFORMATION fi2) {
    if (fi1.dwVolumeSerialNumber != fi2.dwVolumeSerialNumber) {
        return false;
    }
    if (fi1.nFileIndexLow != fi2.nFileIndexLow) {
        return false;
    }
    if (fi1.nFileIndexHigh != fi2.nFileIndexHigh) {
        return false;
    }
    if (fi1.nFileSizeLow != fi2.nFileSizeLow) {
        return false;
    }
    if (fi1.nFileSizeHigh != fi2.nFileSizeHigh) {
        return false;
    }
    if (fi1.dwFileAttributes != fi2.dwFileAttributes) {
        return false;
    }
    if (fi1.nNumberOfLinks != fi2.nNumberOfLinks) {
        return false;
    }
    if (!FileTimeEq(fi1.ftLastWriteTime, fi2.ftLastWriteTime)) {
        return false;
    }
    if (!FileTimeEq(fi1.ftCreationTime, fi2.ftCreationTime)) {
        return false;
    }
    return true;
}

// Code adapted from
// http://stackoverflow.com/questions/562701/best-way-to-determine-if-two-path-reference-to-same-file-in-c-c/562830#562830
// Determine if 2 paths point ot the same file...
bool IsSame(const WCHAR* path1, const WCHAR* path2) {
    if (str::EqI(path1, path2)) {
        return true;
    }

    // we assume that if the last part doesn't match, they can't be the same
    const WCHAR* base1 = path::GetBaseNameNoFree(path1);
    const WCHAR* base2 = path::GetBaseNameNoFree(path2);
    if (!str::EqI(base1, base2)) {
        return false;
    }

    bool isSame = false;
    bool needFallback = true;
    // CreateFile might fail for already opened files
    HANDLE h1 = CreateFileW(path1, 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    HANDLE h2 = CreateFileW(path2, 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

    if (h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION fi1, fi2;
        if (GetFileInformationByHandle(h1, &fi1) && GetFileInformationByHandle(h2, &fi2)) {
            isSame = IsSameFileHandleInformation(fi1, fi2);
            needFallback = false;
        }
    }

    CloseHandle(h1);
    CloseHandle(h2);

    if (!needFallback) {
        return isSame;
    }

    AutoFreeWstr npath1(Normalize(path1));
    AutoFreeWstr npath2(Normalize(path2));
    // consider the files different, if their paths can't be normalized
    return npath1 && str::EqI(npath1, npath2);
}

bool HasVariableDriveLetter(const WCHAR* path) {
    WCHAR root[] = L"?:\\";
    root[0] = towupper(path[0]);
    if (root[0] < 'A' || 'Z' < root[0]) {
        return false;
    }

    UINT driveType = GetDriveType(root);
    switch (driveType) {
        case DRIVE_REMOVABLE:
        case DRIVE_CDROM:
        case DRIVE_NO_ROOT_DIR:
            return true;
    }
    return false;
}

bool IsOnFixedDrive(const WCHAR* path) {
    if (PathIsNetworkPath(path)) {
        return false;
    }

    UINT type;
    WCHAR root[MAX_PATH];
    if (GetVolumePathName(path, root, dimof(root))) {
        type = GetDriveType(root);
    } else {
        type = GetDriveType(path);
    }
    return DRIVE_FIXED == type;
}

static bool MatchWildcardsRec(const WCHAR* fileName, const WCHAR* filter) {
#define AtEndOf(str) (*(str) == '\0')
    switch (*filter) {
        case '\0':
        case ';':
            return AtEndOf(fileName);
        case '*':
            filter++;
            while (!AtEndOf(fileName) && !MatchWildcardsRec(fileName, filter))
                fileName++;
            return !AtEndOf(fileName) || AtEndOf(filter) || *filter == ';';
        case '?':
            return !AtEndOf(fileName) && MatchWildcardsRec(fileName + 1, filter + 1);
        default:
            return towlower(*fileName) == towlower(*filter) && MatchWildcardsRec(fileName + 1, filter + 1);
    }
#undef AtEndOf
}

/* matches the filename of a path against a list of semicolon
   separated filters as used by the common file dialogs
   (e.g. "*.pdf;*.xps;?.*" will match all PDF and XPS files and
   all filenames consisting of only a single character and
   having any extension) */
bool Match(const WCHAR* path, const WCHAR* filter) {
    path = GetBaseNameNoFree(path);
    while (str::FindChar(filter, ';')) {
        if (MatchWildcardsRec(path, filter)) {
            return true;
        }
        filter = str::FindChar(filter, ';') + 1;
    }
    return MatchWildcardsRec(path, filter);
}

bool IsAbsolute(const WCHAR* path) {
    return !PathIsRelative(path);
}

// returns the path to either the %TEMP% directory or a
// non-existing file inside whose name starts with filePrefix
WCHAR* GetTempPath(const WCHAR* filePrefix) {
    WCHAR tempDir[MAX_PATH - 14] = {0};
    DWORD res = ::GetTempPath(dimof(tempDir), tempDir);
    if (!res || res >= dimof(tempDir)) {
        return nullptr;
    }
    if (!filePrefix) {
        return str::Dup(tempDir);
    }
    WCHAR path[MAX_PATH] = {0};
    if (!GetTempFileName(tempDir, filePrefix, 0, path)) {
        return nullptr;
    }
    return str::Dup(path);
}

// returns a path to the application module's directory
// with either the given fileName or the module's name
// (module is the EXE or DLL in which path::GetPathOfFileInAppDir resides)
WCHAR* GetPathOfFileInAppDir(const WCHAR* fileName) {
    WCHAR modulePath[MAX_PATH] = {0};
    GetModuleFileName(GetInstance(), modulePath, dimof(modulePath));
    modulePath[dimof(modulePath) - 1] = '\0';
    if (!fileName) {
        return str::Dup(modulePath);
    }
    AutoFreeWstr moduleDir = path::GetDir(modulePath);
    AutoFreeWstr path = path::Join(moduleDir, fileName);
    return path::Normalize(path);
}

#endif // OS_WIN
} // namespace path

namespace file {

FILE* OpenFILE(const char* path) {
    CrashIf(!path);
    if (!path) {
        return nullptr;
    }
#if OS_WIN
    AutoFreeWstr pathW = strconv::Utf8ToWstr(path);
    return OpenFILE(pathW.Get());
#else
    return fopen(path, "rb");
#endif
}

std::string_view ReadFileWithAllocator(const char* filePath, Allocator* allocator) {
#if 0 // OS_WIN
    WCHAR buf[512];
    strconv::Utf8ToWcharBuf(filePath, str::Len(filePath), buf, dimof(buf));
    return ReadFileWithAllocator(buf, fileSizeOut, allocator);
#else
    FILE* fp = OpenFILE(filePath);
    if (!fp) {
        return {};
    }
    char* d = nullptr;
    int res = fseek(fp, 0, SEEK_END);
    if (res != 0) {
        return {};
    }
    size_t size = ftell(fp);
    if (addOverflows<size_t>(size, ZERO_PADDING_COUNT)) {
        goto Error;
    }
    d = (char*)Allocator::AllocZero(allocator, size + ZERO_PADDING_COUNT);
    if (!d) {
        goto Error;
    }
    res = fseek(fp, 0, SEEK_SET);
    if (res != 0) {
        return {};
    }

    size_t nRead = fread((void*)d, 1, size, fp);
    if (nRead != size) {
        int err = ferror(fp);
        CrashIf(err == 0);
        int isEof = feof(fp);
        CrashIf(isEof != 0);
        goto Error;
    }

    fclose(fp);
    return {d, size};
Error:
    fclose(fp);
    Allocator::Free(allocator, (void*)d);
    return {};
#endif
}

std::string_view ReadFile(std::string_view path) {
    return ReadFileWithAllocator(path.data(), nullptr);
}

std::string_view ReadFile(const WCHAR* filePath) {
    AutoFree path = strconv::WstrToUtf8(filePath);
    return ReadFileWithAllocator(path.data, nullptr);
}

bool WriteFile(const char* filePath, std::string_view d) {
#if OS_WIN
    WCHAR buf[512];
    strconv::Utf8ToWcharBuf(filePath, str::Len(filePath), buf, dimof(buf));
    return WriteFile(buf, d);
#else
    CrashAlwaysIf(true);
    UNUSED(filePath);
    UNUSED(data);
    UNUSED(dataLen);
    return false;
#endif
}

#if OS_WIN
bool Exists(std::string_view path) {
    WCHAR* wpath = strconv::Utf8ToWstr(path);
    bool exists = Exists(wpath);
    free(wpath);
    return exists;
}

#else
bool Exists(std::string_view path) {
    UNUSED(path);
    // TODO: NYI
    CrashMe();
    return false;
}

#endif

#if OS_WIN
HANDLE OpenReadOnly(const WCHAR* filePath) {
    return CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

HANDLE OpenReadOnly(std::string_view path) {
    AutoFreeWstr filePath = strconv::Utf8ToWstr(path);
    return CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

FILE* OpenFILE(const WCHAR* path) {
    if (!path) {
        return nullptr;
    }
    return _wfopen(path, L"rb");
}

bool Exists(const WCHAR* filePath) {
    if (nullptr == filePath) {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(filePath, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }

    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }
    return true;
}

// returns -1 on error (can't use INVALID_FILE_SIZE because it won't cast right)
i64 GetSize(std::string_view filePath) {
    CrashIf(filePath.empty());
    if (filePath.empty()) {
        return -1;
    }

    AutoCloseHandle h = OpenReadOnly(filePath);
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }

    // Don't use GetFileAttributesEx to retrieve the file size, as
    // that function doesn't interact well with symlinks, etc.
    LARGE_INTEGER size{};
    BOOL ok = GetFileSizeEx(h, &size);
    if (!ok) {
        return -1;
    }
    return size.QuadPart;
}

std::string_view ReadFileWithAllocator(const WCHAR* path, Allocator* allocator) {
    AutoFree pathUtf8 = strconv::WstrToUtf8(path);
    return ReadFileWithAllocator(pathUtf8.data, allocator);
}

// buf must be at least toRead in size (note: it won't be zero-terminated)
// returns -1 for error
int ReadN(const WCHAR* filePath, char* buf, size_t toRead) {
    AutoCloseHandle h = OpenReadOnly(filePath);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    ZeroMemory(buf, toRead);
    DWORD nRead = 0;
    BOOL ok = ReadFile(h, (void*)buf, (DWORD)toRead, &nRead, nullptr);
    if (!ok) {
        return -1;
    }
    return (int)nRead;
}

bool WriteFile(const WCHAR* filePath, std::string_view d) {
    const void* data = d.data();
    size_t dataLen = d.size();
    DWORD access = GENERIC_WRITE;
    DWORD share = FILE_SHARE_READ;
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    auto fh = CreateFileW(filePath, access, share, nullptr, CREATE_ALWAYS, flags, nullptr);
    if (INVALID_HANDLE_VALUE == fh) {
        return false;
    }
    AutoCloseHandle h(fh);

    DWORD size = 0;
    BOOL ok = WriteFile(h, data, (DWORD)dataLen, &size, nullptr);
    AssertCrash(!ok || (dataLen == (size_t)size));
    return ok && dataLen == (size_t)size;
}

// Return true if the file wasn't there or was successfully deleted
bool Delete(const WCHAR* filePath) {
    BOOL ok = DeleteFile(filePath);
    return ok || GetLastError() == ERROR_FILE_NOT_FOUND;
}

FILETIME GetModificationTime(const WCHAR* filePath) {
    FILETIME lastMod = {0};
    AutoCloseHandle h(OpenReadOnly(filePath));
    if (h.IsValid()) {
        GetFileTime(h, nullptr, nullptr, &lastMod);
    }
    return lastMod;
}

bool SetModificationTime(const WCHAR* filePath, FILETIME lastMod) {
    AutoCloseHandle h(CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
    if (INVALID_HANDLE_VALUE == h) {
        return false;
    }
    return SetFileTime(h, nullptr, nullptr, &lastMod);
}

// return true if a file starts with string s of size len
bool StartsWithN(const WCHAR* filePath, const char* s, size_t len) {
    AutoFree buf(AllocArray<char>(len));
    if (!buf) {
        return false;
    }

    if (!ReadN(filePath, buf.get(), len)) {
        return false;
    }
    return memeq(buf, s, len);
}

// return true if a file starts with null-terminated string s
bool StartsWith(const WCHAR* filePath, const char* s) {
    return file::StartsWithN(filePath, s, str::Len(s));
}

int GetZoneIdentifier(const WCHAR* filePath) {
    AutoFreeWstr path(str::Join(filePath, L":Zone.Identifier"));
    return GetPrivateProfileInt(L"ZoneTransfer", L"ZoneId", URLZONE_INVALID, path);
}

bool SetZoneIdentifier(const WCHAR* filePath, int zoneId) {
    AutoFreeWstr path(str::Join(filePath, L":Zone.Identifier"));
    AutoFreeWstr id(str::Format(L"%d", zoneId));
    return WritePrivateProfileString(L"ZoneTransfer", L"ZoneId", id, path);
}

#endif // OS_WIN
} // namespace file

namespace dir {

#if OS_WIN
bool Exists(const WCHAR* dir) {
    if (nullptr == dir) {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(dir, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }

    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return true;
    }
    return false;
}

// Return true if a directory already exists or has been successfully created
bool Create(const WCHAR* dir) {
    BOOL ok = CreateDirectoryW(dir, nullptr);
    if (ok) {
        return true;
    }
    return ERROR_ALREADY_EXISTS == GetLastError();
}

// creates a directory and all its parent directories that don't exist yet
bool CreateAll(const WCHAR* dir) {
    AutoFreeWstr parent(path::GetDir(dir));
    if (!str::Eq(parent, dir) && !Exists(parent)) {
        CreateAll(parent);
    }
    return Create(dir);
}

// remove directory and all its children
bool RemoveAll(const WCHAR* dir) {
    // path must be doubly terminated
    // (https://docs.microsoft.com/en-us/windows/desktop/api/shellapi/ns-shellapi-_shfileopstructa)
    size_t n = str::Len(dir) + 2;
    WCHAR* path = AllocArray<WCHAR>(n);
    str::BufSet(path, n, dir);
    FILEOP_FLAGS flags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI;
    UINT op = FO_DELETE;
    SHFILEOPSTRUCTW shfo = {nullptr, op, path, nullptr, flags, FALSE, nullptr, nullptr};
    int res = SHFileOperationW(&shfo);
    str::Free(path);
    return res == 0;
}

#endif // OS_WIN

} // namespace dir

#if OS_WIN
bool FileTimeEq(const FILETIME& a, const FILETIME& b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}
#endif
