/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "utils/Log.h"

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
#define ZERO_PADDING_COUNT 3

namespace path {

bool IsSep(char c) {
    return '\\' == c || '/' == c;
}

// do not free, returns pointer inside <path>
// Note: if want to change to returning TempStr, would have
// to audit caller as they depend of in-place nature of returned
// value
TempStr GetBaseNameTemp(const char* path) {
    char* s = (char*)path + str::Len(path);
    for (; s > path; s--) {
        if (IsSep(s[-1])) {
            break;
        }
    }
    return s;
}

static const char* GetExtPos(const char* path) {
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
    return ext;
}

TempStr GetExtTemp(const char* path) {
    const char* ext = GetExtPos(path);
    if (nullptr == ext) {
        return TempStr("");
    }
    return str::DupTemp(ext);
}

TempStr GetPathNoExtTemp(const char* path) {
    const char* ext = GetExtPos(path);
    if (nullptr == ext) {
        return str::DupTemp(path);
    }
    size_t n = ext - path;
    return str::DupTemp(path, n);
}

TempStr JoinTemp(const char* path, const char* fileName, const char* fileName2) {
    // TODO: not sure if should allow null path
    if (IsSep(*fileName)) {
        fileName++;
    }
    const char* sepStr = nullptr;
    size_t pathLen = str::Len(path);
    if (pathLen > 0) {
        if (!IsSep(path[pathLen - 1])) {
            sepStr = "\\";
        }
    }
    TempStr res = str::JoinTemp(path, sepStr, fileName);
    if (fileName2) {
        res = JoinTemp(res, fileName2);
    }
    return res;
}

char* Join(Allocator* allocator, const char* path, const char* fileName) {
    if (IsSep(*fileName)) {
        fileName++;
    }
    const char* sepStr = nullptr;
    if (!IsSep(path[str::Len(path) - 1])) {
        sepStr = "\\";
    }
    return str::Join(allocator, path, sepStr, fileName);
}

char* Join(const char* path, const char* fileName) {
    return Join(nullptr, path, fileName);
}

bool IsDirectory(const char* path) {
    auto pathW = ToWStrTemp(path);
    DWORD attrs = GetFileAttributesW(pathW);
    if (INVALID_FILE_ATTRIBUTES == attrs) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool IsSep(WCHAR c) {
    return '\\' == c || '/' == c;
}

// do not free, returns pointer inside <path>
static const WCHAR* GetBaseNameTemp(const WCHAR* path) {
    const WCHAR* end = path + str::Len(path);
    while (end > path) {
        if (IsSep(end[-1])) {
            break;
        }
        --end;
    }
    return end;
}

TempWStr GetDirTemp(const WCHAR* path) {
    const WCHAR* baseName = GetBaseNameTemp(path);
    if (baseName == path) {
        // relative directory
        return str::DupTemp(L".");
    }
    if (baseName == path + 1) {
        // relative root
        return str::DupTemp(path, 1);
    }
    if (baseName == path + 3 && path[1] == ':') {
        // local drive root
        return str::DupTemp(path, 3);
    }
    if (baseName == path + 2 && str::StartsWith(path, L"\\\\")) {
        // server root
        return str::DupTemp(path);
    }
    // any subdirectory
    return str::DupTemp(path, baseName - path - 1);
}

TempStr GetDirTemp(const char* path) {
    TempStr baseName = GetBaseNameTemp(path);
    if (baseName == path) {
        // relative directory
        return str::DupTemp(".");
    }
    if (baseName == path + 1) {
        // relative root
        return str::DupTemp(path, 1);
    }
    if (baseName == path + 3 && path[1] == ':') {
        // local drive root
        return str::DupTemp(path, 3);
    }
    if (baseName == path + 2 && str::StartsWith(path, "\\\\")) {
        // server root
        return str::DupTemp(path);
    }
    // any subdirectory
    return str::DupTemp(path, baseName - path - 1);
}

TempWStr JoinTemp(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2) {
    // TODO: not sure if should allow null path
    if (IsSep(*fileName)) {
        fileName++;
    }
    const WCHAR* sepStr = nullptr;
    size_t pathLen = str::Len(path);
    if (pathLen > 0) {
        if (!IsSep(path[pathLen - 1])) {
            sepStr = L"\\";
        }
    }
    TempWStr res = str::JoinTemp(path, sepStr, fileName);
    if (fileName2) {
        res = JoinTemp(res, fileName2);
    }
    return res;
}

WCHAR* Join(const WCHAR* path, const WCHAR* fileName, const WCHAR* fileName2) {
    WCHAR* res = JoinTemp(path, fileName, fileName2);
    return str::Dup(res);
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
    DWORD cch = GetFullPathNameW(path, 0, nullptr, nullptr);
    if (!cch) {
        return str::Dup(path);
    }

    AutoFreeWstr fullpath(AllocArray<WCHAR>(cch));
    GetFullPathNameW(path, cch, fullpath, nullptr);
    // convert to long form
    cch = GetLongPathNameW(fullpath, nullptr, 0);
    if (!cch) {
        return fullpath.StealData();
    }

    AutoFreeWstr normpath(AllocArray<WCHAR>(cch));
    GetLongPathNameW(fullpath, normpath, cch);
    if (cch <= MAX_PATH) {
        return normpath.StealData();
    }

    // handle overlong paths: first, try to shorten the path
    cch = GetShortPathNameW(fullpath, nullptr, 0);
    if (cch && cch <= MAX_PATH) {
        AutoFreeWstr shortpath(AllocArray<WCHAR>(cch));
        GetShortPathNameW(fullpath, shortpath, cch);
        if (str::Len(path::GetBaseNameTemp(normpath)) + path::GetBaseNameTemp(shortpath) - shortpath < MAX_PATH) {
            // keep the long filename if possible
            *(WCHAR*)path::GetBaseNameTemp(shortpath) = '\0';
            return str::Join(shortpath, path::GetBaseNameTemp(normpath));
        }
        return shortpath.StealData();
    }
    // else mark the path as overlong
    if (str::StartsWith(normpath.Get(), L"\\\\?\\")) {
        return normpath.StealData();
    }
    return str::Join(L"\\\\?\\", normpath);
}

char* NormalizeTemp(const char* path) {
    WCHAR* s = ToWStrTemp(path);
    AutoFreeWstr ws = Normalize(s);
    char* res = ToUtf8Temp(ws);
    return res;
}

// Normalizes the file path and the converts it into a short form that
// can be used for interaction with non-UNICODE aware applications
char* ShortPath(const char* pathA) {
    WCHAR* path = ToWStrTemp(pathA);
    AutoFreeWstr normpath(Normalize(path));
    DWORD cch = GetShortPathNameW(normpath, nullptr, 0);
    if (!cch) {
        return ToUtf8(normpath.Get());
    }
    WCHAR* shortPath = AllocArray<WCHAR>(cch + 1);
    GetShortPathNameW(normpath, shortPath, cch);
    char* res = ToUtf8(shortPath);
    str::Free(shortPath);
    return res;
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
bool IsSame(const char* path1, const char* path2) {
    if (!path1 || !path2) {
        return false;
    }
    if (str::EqI(path1, path2)) {
        return true;
    }

    // we assume that if the last part doesn't match, they can't be the same
    TempStr base1 = path::GetBaseNameTemp(path1);
    TempStr base2 = path::GetBaseNameTemp(path2);
    if (!str::EqI(base1, base2)) {
        return false;
    }

    WCHAR* path1W = ToWStrTemp(path1);
    WCHAR* path2W = ToWStrTemp(path2);
    bool isSame = false;
    bool needFallback = true;
    // CreateFile might fail for already opened files
    HANDLE h1 = CreateFileW(path1W, 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    HANDLE h2 = CreateFileW(path2W, 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

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

    char* npath1 = NormalizeTemp(path1);
    char* npath2 = NormalizeTemp(path2);
    // consider the files different, if their paths can't be normalized
    return npath1 && str::EqI(npath1, npath2);
}

bool HasVariableDriveLetter(const char* path) {
    char root[] = R"(?:\)";
    root[0] = (char)toupper(path[0]);
    if (root[0] < 'A' || 'Z' < root[0]) {
        return false;
    }

    uint driveType = GetDriveTypeA(root);
    switch (driveType) {
        case DRIVE_REMOVABLE:
        case DRIVE_CDROM:
        case DRIVE_NO_ROOT_DIR:
            return true;
    }
    return false;
}

bool IsOnFixedDrive(const char* pathA) {
    WCHAR* path = ToWStrTemp(pathA);
    if (PathIsNetworkPathW(path)) {
        return false;
    }

    uint type;
    WCHAR root[MAX_PATH];
    if (GetVolumePathNameW(path, root, dimof(root))) {
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
            while (!AtEndOf(fileName) && !MatchWildcardsRec(fileName, filter)) {
                fileName++;
            }
            return !AtEndOf(fileName) || AtEndOf(filter) || *filter == ';';
        case '?':
            return !AtEndOf(fileName) && MatchWildcardsRec(fileName + 1, filter + 1);
        default:
            return towlower(*fileName) == towlower(*filter) && MatchWildcardsRec(fileName + 1, filter + 1);
    }
#undef AtEndOf
}

static bool MatchWildcardsRec(const char* fileName, const char* filter) {
#define AtEndOf(str) (*(str) == '\0')
    switch (*filter) {
        case '\0':
        case ';':
            return AtEndOf(fileName);
        case '*':
            filter++;
            while (!AtEndOf(fileName) && !MatchWildcardsRec(fileName, filter)) {
                fileName++;
            }
            return !AtEndOf(fileName) || AtEndOf(filter) || *filter == ';';
        case '?':
            return !AtEndOf(fileName) && MatchWildcardsRec(fileName + 1, filter + 1);
        default:
            return tolower(*fileName) == tolower(*filter) && MatchWildcardsRec(fileName + 1, filter + 1);
    }
#undef AtEndOf
}

/* matches the filename of a path against a list of semicolon
   separated filters as used by the common file dialogs
   (e.g. "*.pdf;*.xps;?.*" will match all PDF and XPS files and
   all filenames consisting of only a single character and
   having any extension) */
bool Match(const char* path, const char* filter) {
    path = GetBaseNameTemp(path);
    while (str::FindChar(filter, L';')) {
        if (MatchWildcardsRec(path, filter)) {
            return true;
        }
        filter = str::FindChar(filter, ';') + 1;
    }
    return MatchWildcardsRec(path, filter);
}

bool IsAbsolute(const char* path) {
    WCHAR* ws = ToWStrTemp(path);
    return !PathIsRelativeW(ws);
}

// returns the path to either the %TEMP% directory or a
// non-existing file inside whose name starts with filePrefix
char* GetTempFilePath(const char* filePrefix) {
    WCHAR tempDir[MAX_PATH]{};
    DWORD res = ::GetTempPathW(dimof(tempDir), tempDir);
    if (!res || res >= dimof(tempDir)) {
        return nullptr;
    }
    if (!filePrefix) {
        return ToUtf8(tempDir);
    }
    WCHAR path[MAX_PATH]{};
    WCHAR* filePrefixW = ToWStrTemp(filePrefix);
    if (!GetTempFileNameW(tempDir, filePrefixW, 0, path)) {
        return nullptr;
    }
    return ToUtf8(path);
}

// returns a path to the application module's directory
// with either the given fileName or the module's name
// (module is the EXE or DLL in which path::GetPathOfFileInAppDir resides)
TempStr GetPathOfFileInAppDirTemp(const char* fileName) {
    WCHAR modulePath[MAX_PATH]{};
    GetModuleFileNameW(GetInstance(), modulePath, dimof(modulePath));
    modulePath[dimof(modulePath) - 1] = '\0';
    if (!fileName) {
        return ToUtf8Temp(modulePath);
    }
    WCHAR* moduleDir = path::GetDirTemp(modulePath);
    WCHAR* fileNameW = ToWStrTemp(fileName);
    WCHAR* path = path::JoinTemp(moduleDir, fileNameW);
    path = path::Normalize(path);
    TempStr res = ToUtf8Temp(path);
    str::Free(path);
    return res;
}
} // namespace path

namespace file {

FILE* OpenFILE(const char* path) {
    CrashIf(!path);
    if (!path) {
        return nullptr;
    }
    WCHAR* pathW = ToWStrTemp(path);
    return _wfopen(pathW, L"rb");
}

ByteSlice ReadFileWithAllocator(const char* filePath, Allocator* allocator) {
#if 0 // OS_WIN
    WCHAR buf[512];
    strconv::Utf8ToWcharBuf(filePath, str::Len(filePath), buf, dimof(buf));
    return ReadFileWithAllocator(buf, fileSizeOut, allocator);
#else
    char* d = nullptr;
    int res;
    FILE* fp = OpenFILE(filePath);
    if (!fp) {
        return {};
    }
    defer {
        fclose(fp);
    };
    res = fseek(fp, 0, SEEK_END);
    if (res != 0) {
        return {};
    }
    size_t size = ftell(fp);
    size_t nRead = 0;
    if (addOverflows<size_t>(size, ZERO_PADDING_COUNT)) {
        goto Error;
    }
    d = Allocator::AllocArray<char>(allocator, size + ZERO_PADDING_COUNT);
    if (!d) {
        goto Error;
    }
    res = fseek(fp, 0, SEEK_SET);
    if (res != 0) {
        return {};
    }

    nRead = fread((void*)d, 1, size, fp);
    if (nRead != size) {
        int err = ferror(fp);
        int isEof = feof(fp);
        logf("ReadFileWithAllocator: fread() failed, path: '%s', size: %d, nRead: %d, err: %d, isEof: %d\n", filePath,
             (int)size, (int)nRead, err, isEof);
        // we should either get eof or err
        // either way shouldn't happen because we're reading the exact size of file
        // I've seen this in crash reports so maybe the files are over-written
        // between the time I do fseek() and fread()
        CrashIf(!(isEof || (err != 0)));
        goto Error;
    }

    return {(u8*)d, size};
Error:
    Allocator::Free(allocator, (void*)d);
    return {};
#endif
}

ByteSlice ReadFile(const char* path) {
    return ReadFileWithAllocator(path, nullptr);
}

bool WriteFile(const char* path, const ByteSlice& d) {
    WCHAR* pathW = ToWStrTemp(path);
    const void* data = d.data();
    size_t dataLen = d.size();
    DWORD access = GENERIC_WRITE;
    DWORD share = FILE_SHARE_READ;
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    auto fh = CreateFileW(pathW, access, share, nullptr, CREATE_ALWAYS, flags, nullptr);
    if (INVALID_HANDLE_VALUE == fh) {
        return false;
    }
    AutoCloseHandle h(fh);

    DWORD size = 0;
    BOOL ok = WriteFile(h, data, (DWORD)dataLen, &size, nullptr);
    CrashIf(ok && (dataLen != (size_t)size));
    return ok && dataLen == (size_t)size;
}

HANDLE OpenReadOnly(const char* path) {
    WCHAR* filePath = ToWStrTemp(path);
    return CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool Exists(const char* path) {
    if (!path) {
        return false;
    }

    WCHAR* pathW = ToWStrTemp(path);
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(pathW, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }
    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return false;
    }
    return true;
}

// returns -1 on error (can't use INVALID_FILE_SIZE because it won't cast right)
i64 GetSize(const char* path) {
    CrashIf(!path);
    if (!path) {
        return -1;
    }

    AutoCloseHandle h = OpenReadOnly(path);
    if (!h.IsValid()) {
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

// buf must be at least toRead in size (note: it won't be zero-terminated)
// returns -1 for error
int ReadN(const char* path, char* buf, size_t toRead) {
    AutoCloseHandle h = OpenReadOnly(path);
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

// Return true if the file wasn't there or was successfully deleted
bool Delete(const char* filePathA) {
    if (!filePathA) {
        return false;
    }
    WCHAR* filePath = ToWStrTemp(filePathA);
    BOOL ok = DeleteFileW(filePath);
    ok |= (GetLastError() == ERROR_FILE_NOT_FOUND);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

bool Copy(const char* dst, const char* src, bool dontOverwrite) {
    WCHAR* dstW = ToWStrTemp(dst);
    WCHAR* srcW = ToWStrTemp(src);
    BOOL ok = CopyFileW(srcW, dstW, (BOOL)dontOverwrite);
    if (!ok) {
        LogLastError();
        return false;
    }
    return true;
}

FILETIME GetModificationTime(const char* filePath) {
    FILETIME lastMod{};
    AutoCloseHandle h(OpenReadOnly(filePath));
    if (h.IsValid()) {
        GetFileTime(h, nullptr, nullptr, &lastMod);
    }
    return lastMod;
}

DWORD GetAttributes(const char* path) {
    WCHAR* pathW = ToWStrTemp(path);
    return GetFileAttributesW(pathW);
}

bool SetAttributes(const char* path, DWORD attrs) {
    WCHAR* pathW = ToWStrTemp(path);
    return SetFileAttributesW(pathW, attrs);
}

bool SetModificationTime(const char* path, FILETIME lastMod) {
    WCHAR* pathW = ToWStrTemp(path);
    DWORD access = GENERIC_READ | GENERIC_WRITE;
    DWORD disp = OPEN_EXISTING;
    AutoCloseHandle h(CreateFileW(pathW, access, 0, nullptr, disp, 0, nullptr));
    if (INVALID_HANDLE_VALUE == h) {
        return false;
    }
    return SetFileTime(h, nullptr, nullptr, &lastMod);
}

// return true if a file starts with string s of size len
bool StartsWithN(const char* path, const char* s, size_t len) {
    char* buf = AllocArrayTemp<char>(len);
    if (!buf) {
        return false;
    }
    if (!ReadN(path, buf, len)) {
        return false;
    }
    return memeq(buf, s, len);
}

// return true if a file starts with null-terminated string s
bool StartsWith(const char* path, const char* s) {
    return file::StartsWithN(path, s, str::Len(s));
}

int GetZoneIdentifier(const char* filePath) {
    char* path = str::JoinTemp(filePath, ":Zone.Identifier");
    WCHAR* pathW = ToWStrTemp(path);
    return GetPrivateProfileIntW(L"ZoneTransfer", L"ZoneId", URLZONE_INVALID, pathW);
}

bool SetZoneIdentifier(const char* filePath, int zoneId) {
    TempStr path = str::JoinTemp(filePath, ":Zone.Identifier");
    TempStr id = str::FormatTemp("%d", zoneId);
    TempWStr idw = ToWStrTemp(id);
    TempWStr pathW = ToWStrTemp(path);
    return WritePrivateProfileStringW(L"ZoneTransfer", L"ZoneId", idw, pathW);
}

bool DeleteZoneIdentifier(const char* filePath) {
    char* path = str::JoinTemp(filePath, ":Zone.Identifier");
    return Delete(path);
}

} // namespace file

namespace dir {

// TODO: duplicate with path::IsDirectory()
bool Exists(const WCHAR* dir) {
    if (nullptr == dir) {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(dir, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }

    return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
bool Exists(const char* dirA) {
    if (nullptr == dirA) {
        return false;
    }
    WCHAR* dir = ToWStrTemp(dirA);

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(dir, GetFileExInfoStandard, &fileInfo);
    if (0 == res) {
        return false;
    }

    return (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Return true if a directory already exists or has been successfully created
bool Create(const char* dir) {
    WCHAR* dirW = ToWStrTemp(dir);
    BOOL ok = CreateDirectoryW(dirW, nullptr);
    if (ok) {
        return true;
    }
    return ERROR_ALREADY_EXISTS == GetLastError();
}

// creates a directory and all its parent directories that don't exist yet
bool CreateAll(const char* dir) {
    char* parent = path::GetDirTemp(dir);
    if (!str::Eq(parent, dir) && !Exists(parent)) {
        CreateAll(parent);
    }
    return Create(dir);
}

bool CreateForFile(const char* path) {
    char* dir = path::GetDirTemp(path);
    return CreateAll(dir);
}

// remove directory and all its children
bool RemoveAll(const char* dir) {
    WCHAR* dirW = ToWStrTemp(dir);
    // path must be doubly terminated
    // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shfileopstructa#fo_rename
    auto n = str::Len(dirW) + 2;
    WCHAR* dirDoubleTerminated = AllocArrayTemp<WCHAR>(n);
    str::BufSet(dirDoubleTerminated, (int)n, dirW);
    FILEOP_FLAGS flags = FOF_NO_UI;
    uint op = FO_DELETE;
    SHFILEOPSTRUCTW shfo = {nullptr, op, dirDoubleTerminated, nullptr, flags, FALSE, nullptr, nullptr};
    int res = SHFileOperationW(&shfo);
    return res == 0;
}

} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}
