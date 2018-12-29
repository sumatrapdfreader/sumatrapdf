/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileUtil.h"

#if OS_WIN
#include "ScopedWin.h"
#include "WinUtil.h"
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
const char* GetBaseName(const char* path) {
    const char* fileBaseName = path + str::Len(path);
    for (; fileBaseName > path; fileBaseName--) {
        if (IsSep(fileBaseName[-1]))
            break;
    }
    return fileBaseName;
}

// Note: returns pointer inside <path>, do not free
const char* GetExt(const char* path) {
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
    if (IsSep(*fileName))
        fileName++;
    const char* sepStr = nullptr;
    if (!IsSep(path[str::Len(path) - 1]))
        sepStr = "\\";
    return str::Join(path, sepStr, fileName, allocator);
}

#if OS_WIN
bool IsSep(WCHAR c) {
    return '\\' == c || '/' == c;
}

// Note: returns pointer inside <path>, do not free
const WCHAR* GetBaseName(const WCHAR* path) {
    const WCHAR* fileBaseName = path + str::Len(path);
    for (; fileBaseName > path; fileBaseName--) {
        if (IsSep(fileBaseName[-1])) {
            break;
        }
    }
    return fileBaseName;
}

// Note: returns pointer inside <path>, do not free
const WCHAR* GetExt(const WCHAR* path) {
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
    const WCHAR* baseName = GetBaseName(path);
    if (baseName == path) // relative directory
        return str::Dup(L".");
    if (baseName == path + 1) // relative root
        return str::DupN(path, 1);
    if (baseName == path + 3 && path[1] == ':') // local drive root
        return str::DupN(path, 3);
    if (baseName == path + 2 && str::StartsWith(path, L"\\\\")) // server root
        return str::Dup(path);
    // any subdirectory
    return str::DupN(path, baseName - path - 1);
}

WCHAR* Join(const WCHAR* path, const WCHAR* fileName) {
    if (IsSep(*fileName))
        fileName++;
    WCHAR* sepStr = nullptr;
    if (!IsSep(path[str::Len(path) - 1]))
        sepStr = L"\\";
    return str::Join(path, sepStr, fileName);
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
    if (!cch)
        return str::Dup(path);
    AutoFreeW fullpath(AllocArray<WCHAR>(cch));
    GetFullPathName(path, cch, fullpath, nullptr);
    // convert to long form
    cch = GetLongPathName(fullpath, nullptr, 0);
    if (!cch)
        return fullpath.StealData();
    AutoFreeW normpath(AllocArray<WCHAR>(cch));
    GetLongPathName(fullpath, normpath, cch);
    if (cch <= MAX_PATH)
        return normpath.StealData();
    // handle overlong paths: first, try to shorten the path
    cch = GetShortPathName(fullpath, nullptr, 0);
    if (cch && cch <= MAX_PATH) {
        AutoFreeW shortpath(AllocArray<WCHAR>(cch));
        GetShortPathName(fullpath, shortpath, cch);
        if (str::Len(path::GetBaseName(normpath)) + path::GetBaseName(shortpath) - shortpath < MAX_PATH) {
            // keep the long filename if possible
            *(WCHAR*)path::GetBaseName(shortpath) = '\0';
            return str::Join(shortpath, path::GetBaseName(normpath));
        }
        return shortpath.StealData();
    }
    // else mark the path as overlong
    if (str::StartsWith(normpath.Get(), L"\\\\?\\"))
        return normpath.StealData();
    return str::Join(L"\\\\?\\", normpath);
}

// Normalizes the file path and the converts it into a short form that
// can be used for interaction with non-UNICODE aware applications
WCHAR* ShortPath(const WCHAR* path) {
    AutoFreeW normpath(Normalize(path));
    DWORD cch = GetShortPathName(normpath, nullptr, 0);
    if (!cch)
        return normpath.StealData();
    WCHAR* shortpath = AllocArray<WCHAR>(cch);
    GetShortPathName(normpath, shortpath, cch);
    return shortpath;
}

// Code adapted from
// http://stackoverflow.com/questions/562701/best-way-to-determine-if-two-path-reference-to-same-file-in-c-c/562830#562830
// Determine if 2 paths point ot the same file...
bool IsSame(const WCHAR* path1, const WCHAR* path2) {
    bool isSame = false, needFallback = true;
    HANDLE handle1 = CreateFile(path1, 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    HANDLE handle2 = CreateFile(path2, 0, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

    if (handle1 != INVALID_HANDLE_VALUE && handle2 != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION fi1, fi2;
        if (GetFileInformationByHandle(handle1, &fi1) && GetFileInformationByHandle(handle2, &fi2)) {
            isSame = fi1.dwVolumeSerialNumber == fi2.dwVolumeSerialNumber && fi1.nFileIndexLow == fi2.nFileIndexLow &&
                     fi1.nFileIndexHigh == fi2.nFileIndexHigh && fi1.nFileSizeLow == fi2.nFileSizeLow &&
                     fi1.nFileSizeHigh == fi2.nFileSizeHigh && fi1.dwFileAttributes == fi2.dwFileAttributes &&
                     fi1.nNumberOfLinks == fi2.nNumberOfLinks && FileTimeEq(fi1.ftLastWriteTime, fi2.ftLastWriteTime) &&
                     FileTimeEq(fi1.ftCreationTime, fi2.ftCreationTime);
            needFallback = false;
        }
    }

    CloseHandle(handle1);
    CloseHandle(handle2);

    if (!needFallback)
        return isSame;

    AutoFreeW npath1(Normalize(path1));
    AutoFreeW npath2(Normalize(path2));
    // consider the files different, if their paths can't be normalized
    return npath1 && str::EqI(npath1, npath2);
}

bool HasVariableDriveLetter(const WCHAR* path) {
    WCHAR root[] = L"?:\\";
    root[0] = towupper(path[0]);
    if (root[0] < 'A' || 'Z' < root[0])
        return false;

    UINT driveType = GetDriveType(root);
    return DRIVE_REMOVABLE == driveType || DRIVE_CDROM == driveType || DRIVE_NO_ROOT_DIR == driveType;
}

bool IsOnFixedDrive(const WCHAR* path) {
    if (PathIsNetworkPath(path))
        return false;

    UINT type;
    WCHAR root[MAX_PATH];
    if (GetVolumePathName(path, root, dimof(root)))
        type = GetDriveType(root);
    else
        type = GetDriveType(path);
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
    path = GetBaseName(path);
    while (str::FindChar(filter, ';')) {
        if (MatchWildcardsRec(path, filter))
            return true;
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
    WCHAR tempDir[MAX_PATH - 14];
    DWORD res = ::GetTempPath(dimof(tempDir), tempDir);
    if (!res || res >= dimof(tempDir))
        return nullptr;
    if (!filePrefix)
        return str::Dup(tempDir);
    WCHAR path[MAX_PATH];
    if (!GetTempFileName(tempDir, filePrefix, 0, path))
        return nullptr;
    return str::Dup(path);
}

// returns a path to the application module's directory
// with either the given fileName or the module's name
// (module is the EXE or DLL in which path::GetPathOfFileInAppDir resides)
// TODO: normalize the path
WCHAR* GetPathOfFileInAppDir(const WCHAR* fileName) {
    WCHAR modulePath[MAX_PATH];
    modulePath[0] = '\0';
    GetModuleFileName(GetInstance(), modulePath, dimof(modulePath));
    modulePath[dimof(modulePath) - 1] = '\0';
    if (!fileName)
        return str::Dup(modulePath);
    const WCHAR* moduleDir(path::GetDir(modulePath));
    defer { str::Free(moduleDir); };
    const WCHAR* path = path::Join(moduleDir, fileName);
    defer { str::Free(path); };
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
    AutoFreeW pathW(str::conv::FromUtf8(path));
    return OpenFILE(pathW.Get());
#else
    return fopen(path, "rb");
#endif
}

char* ReadFileWithAllocator(const char* filePath, size_t* fileSizeOut, Allocator* allocator) {
#if 0 // OS_WIN
    WCHAR buf[512];
    str::Utf8ToWcharBuf(filePath, str::Len(filePath), buf, dimof(buf));
    return ReadFileWithAllocator(buf, fileSizeOut, allocator);
#else
    FILE* fp = OpenFILE(filePath);
    if (!fp) {
        return nullptr;
    }
    char* d = nullptr;
    int res = fseek(fp, 0, SEEK_END);
    if (res != 0) {
        return nullptr;
    }
    size_t nRead;
    size_t size = ftell(fp);
    if (addOverflows<size_t>(size, ZERO_PADDING_COUNT)) {
        goto Error;
    }
    d = (char*)Allocator::AllocZero(allocator, size + ZERO_PADDING_COUNT);
    res = fseek(fp, 0, SEEK_SET);
    if (res != 0) {
        return nullptr;
    }

    nRead = fread((void*)d, 1, size, fp);
    if (nRead != size) {
        int err = ferror(fp);
        CrashIf(err == 0);
        int isEof = feof(fp);
        CrashIf(isEof != 0);
        goto Error;
    }

Exit:
    if (fileSizeOut) {
        *fileSizeOut = size;
    }
    fclose(fp);
    return d;
Error:
    Allocator::Free(allocator, (void*)d);
    d = nullptr;
    goto Exit;
#endif
}

OwnedData ReadFile(const char* path) {
    size_t size;
    char* data = ReadFileWithAllocator(path, &size, nullptr);
    return {data, size};
}

bool WriteFile(const char* filePath, const void* data, size_t dataLen) {
#if OS_WIN
    WCHAR buf[512];
    str::Utf8ToWcharBuf(filePath, str::Len(filePath), buf, dimof(buf));
    return WriteFile(buf, data, dataLen);
#else
    CrashAlwaysIf(true);
    UNUSED(filePath);
    UNUSED(data);
    UNUSED(dataLen);
    return false;
#endif
}

#if OS_WIN
HANDLE OpenReadOnly(const WCHAR* filePath) {
    return CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

FILE* OpenFILE(const WCHAR* path) {
    if (!path) {
        return nullptr;
    }
    return _wfopen(path, L"rb");
}

bool Exists(const WCHAR* filePath) {
    if (nullptr == filePath)
        return false;

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(filePath, GetFileExInfoStandard, &fileInfo);
    if (0 == res)
        return false;

    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return false;
    return true;
}

// returns -1 on error (can't use INVALID_FILE_SIZE because it won't cast right)
int64_t GetSize(const WCHAR* filePath) {
    CrashIf(!filePath);
    if (!filePath)
        return -1;

    ScopedHandle h(OpenReadOnly(filePath));
    if (h == INVALID_HANDLE_VALUE)
        return -1;

    // Don't use GetFileAttributesEx to retrieve the file size, as
    // that function doesn't interact well with symlinks, etc.
    LARGE_INTEGER size;
    BOOL ok = GetFileSizeEx(h, &size);
    if (!ok)
        return -1;
    return size.QuadPart;
}

char* ReadFileWithAllocator(const WCHAR* path, size_t* fileSizeOut, Allocator* allocator) {
    OwnedData s = str::conv::ToUtf8(path);
    return ReadFileWithAllocator(s.Get(), fileSizeOut, allocator);
}

#if 0
char* ReadFileWithAllocator(const WCHAR* path, size_t* fileSizeOut, Allocator* allocator) {
    int64_t size64 = GetSize(path);
    if (size64 < 0) {
        return nullptr;
    }
    size_t size = (size_t)size64;
#ifdef _WIN64
    CrashIf(size != (size_t)size64);
#else
    if (size != size64) {
        return nullptr;
    }
#endif

    if (addOverflows<size_t>(size, ZERO_PADDING_COUNT)) {
        return nullptr;
    }
    /* allocate one character more and zero-terminate just in case it's a
       text file we'll want to treat as C string. Doesn't hurt for binary
       files (note: three byte terminator for UTF-16 files) */
    char* data = (char*)Allocator::AllocZero(allocator, size + ZERO_PADDING_COUNT);
    if (!data) {
        return nullptr;
    }

    if (!ReadN(path, data, size)) {
        Allocator::Free(allocator, data);
        return nullptr;
    }

    if (fileSizeOut) {
        *fileSizeOut = size;
    }
    return data;
}
#endif

OwnedData ReadFile(const WCHAR* path) {
    size_t size;
    char* data = ReadFileWithAllocator(path, &size, nullptr);
    return {data, size};
}

// buf must be at least toRead in size (note: it won't be zero-terminated)
bool ReadN(const WCHAR* filePath, char* buf, size_t toRead) {
    ScopedHandle h(OpenReadOnly(filePath));
    if (h == INVALID_HANDLE_VALUE)
        return false;

    DWORD nRead;
    BOOL ok = ReadFile(h, buf, (DWORD)toRead, &nRead, nullptr);
    return ok && nRead == toRead;
}

bool WriteFile(const WCHAR* filePath, const void* data, size_t dataLen) {
    ScopedHandle h(
        CreateFile(filePath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (INVALID_HANDLE_VALUE == h)
        return false;

    DWORD size;
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
    ScopedHandle h(OpenReadOnly(filePath));
    if (h != INVALID_HANDLE_VALUE)
        GetFileTime(h, nullptr, nullptr, &lastMod);
    return lastMod;
}

bool SetModificationTime(const WCHAR* filePath, FILETIME lastMod) {
    ScopedHandle h(CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
    if (INVALID_HANDLE_VALUE == h)
        return false;
    return SetFileTime(h, nullptr, nullptr, &lastMod);
}

// return true if a file starts with string s of size len
bool StartsWithN(const WCHAR* filePath, const char* s, size_t len) {
    AutoFree buf(AllocArray<char>(len));
    if (!buf)
        return false;

    if (!ReadN(filePath, buf, len))
        return false;
    return memeq(buf, s, len);
}

// return true if a file starts with null-terminated string s
bool StartsWith(const WCHAR* filePath, const char* s) {
    return file::StartsWithN(filePath, s, str::Len(s));
}

int GetZoneIdentifier(const WCHAR* filePath) {
    AutoFreeW path(str::Join(filePath, L":Zone.Identifier"));
    return GetPrivateProfileInt(L"ZoneTransfer", L"ZoneId", URLZONE_INVALID, path);
}

bool SetZoneIdentifier(const WCHAR* filePath, int zoneId) {
    AutoFreeW path(str::Join(filePath, L":Zone.Identifier"));
    AutoFreeW id(str::Format(L"%d", zoneId));
    return WritePrivateProfileString(L"ZoneTransfer", L"ZoneId", id, path);
}

#endif // OS_WIN
} // namespace file

namespace dir {

#if OS_WIN
bool Exists(const WCHAR* dir) {
    if (nullptr == dir)
        return false;

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL res = GetFileAttributesEx(dir, GetFileExInfoStandard, &fileInfo);
    if (0 == res)
        return false;

    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return true;
    return false;
}

// Return true if a directory already exists or has been successfully created
bool Create(const WCHAR* dir) {
    BOOL ok = CreateDirectoryW(dir, nullptr);
    if (ok)
        return true;
    return ERROR_ALREADY_EXISTS == GetLastError();
}

// creates a directory and all its parent directories that don't exist yet
bool CreateAll(const WCHAR* dir) {
    AutoFreeW parent(path::GetDir(dir));
    if (!str::Eq(parent, dir) && !Exists(parent))
        CreateAll(parent);
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
// cf. http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// A convenient way to grab the same value as HINSTANCE passed to WinMain
HINSTANCE GetInstance() {
    return (HINSTANCE)&__ImageBase;
}
#endif
