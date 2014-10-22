/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileUtil.h"

// cf. http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define CURRENT_HMODULE ((HMODULE)&__ImageBase)

namespace path {

bool IsSep(WCHAR c)
{
    return '\\' == c || '/' == c;
}

bool IsSep(char c)
{
    return '\\' == c || '/' == c;
}

// Note: returns pointer inside <path>, do not free
const WCHAR *GetBaseName(const WCHAR *path)
{
    const WCHAR *fileBaseName = path + str::Len(path);
    for (; fileBaseName > path; fileBaseName--)
        if (IsSep(fileBaseName[-1]))
            break;
    return fileBaseName;
}

// Note: returns pointer inside <path>, do not free
const char *GetBaseName(const char *path)
{
    const char *fileBaseName = path + str::Len(path);
    for (; fileBaseName > path; fileBaseName--) {
        if (IsSep(fileBaseName[-1]))
            break;
    }
    return fileBaseName;
}

// Note: returns pointer inside <path>, do not free
const WCHAR *GetExt(const WCHAR *path)
{
    const WCHAR *ext = path + str::Len(path);
    for (; ext > path && !IsSep(*ext); ext--) {
        if (*ext == '.')
            return ext;
    }
    return path + str::Len(path);
}

// Caller has to free()
WCHAR *GetDir(const WCHAR *path)
{
    const WCHAR *baseName = GetBaseName(path);
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

WCHAR *Join(const WCHAR *path, const WCHAR *fileName)
{
    if (IsSep(*fileName))
        fileName++;
    WCHAR *sepStr = NULL;
    if (!IsSep(path[str::Len(path) - 1]))
        sepStr = L"\\";
    return str::Join(path, sepStr, fileName);
}

char *JoinUtf(const char *path, const char *fileName, Allocator *allocator)
{
    if (IsSep(*fileName))
        fileName++;
    char *sepStr = NULL;
    if (!IsSep(path[str::Len(path) - 1]))
        sepStr = "\\";
    return str::Join(path, sepStr, fileName, allocator);
}

// Normalize a file path.
//  remove relative path component (..\ and .\),
//  replace slashes by backslashes,
//  convert to long form.
//
// Returns a pointer to a memory allocated block containing the normalized string.
//   The caller is responsible for freeing the block.
//   Returns NULL if the file does not exist or if a memory allocation fails.
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
WCHAR *Normalize(const WCHAR *path)
{
    // convert to absolute path, change slashes into backslashes
    DWORD cch = GetFullPathName(path, 0, NULL, NULL);
    if (!cch)
        return str::Dup(path);
    ScopedMem<WCHAR> fullpath(AllocArray<WCHAR>(cch));
    GetFullPathName(path, cch, fullpath, NULL);
    // convert to long form
    cch = GetLongPathName(fullpath, NULL, 0);
    if (!cch)
        return fullpath.StealData();
    ScopedMem<WCHAR> normpath(AllocArray<WCHAR>(cch));
    GetLongPathName(fullpath, normpath, cch);
    if (cch <= MAX_PATH)
        return normpath.StealData();
    // handle overlong paths: first, try to shorten the path
    cch = GetShortPathName(fullpath, NULL, 0);
    if (cch && cch <= MAX_PATH) {
        ScopedMem<WCHAR> shortpath(AllocArray<WCHAR>(cch));
        GetShortPathName(fullpath, shortpath, cch);
        if (str::Len(path::GetBaseName(normpath)) + path::GetBaseName(shortpath) - shortpath < MAX_PATH) {
            // keep the long filename if possible
            *(WCHAR *)path::GetBaseName(shortpath) = '\0';
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
WCHAR *ShortPath(const WCHAR *path)
{
    ScopedMem<WCHAR> normpath(Normalize(path));
    DWORD cch = GetShortPathName(normpath, NULL, 0);
    if (!cch)
        return normpath.StealData();
    WCHAR *shortpath = AllocArray<WCHAR>(cch);
    GetShortPathName(normpath, shortpath, cch);
    return shortpath;
}

// Code adapted from http://stackoverflow.com/questions/562701/best-way-to-determine-if-two-path-reference-to-same-file-in-c-c/562830#562830
// Determine if 2 paths point ot the same file...
bool IsSame(const WCHAR *path1, const WCHAR *path2)
{
    bool isSame = false, needFallback = true;
    HANDLE handle1 = CreateFile(path1, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    HANDLE handle2 = CreateFile(path2, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (handle1 != INVALID_HANDLE_VALUE && handle2 != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION fi1, fi2;
        if (GetFileInformationByHandle(handle1, &fi1) && GetFileInformationByHandle(handle2, &fi2)) {
            isSame = fi1.dwVolumeSerialNumber == fi2.dwVolumeSerialNumber &&
                     fi1.nFileIndexLow == fi2.nFileIndexLow && fi1.nFileIndexHigh == fi2.nFileIndexHigh &&
                     fi1.nFileSizeLow == fi2.nFileSizeLow && fi1.nFileSizeHigh == fi2.nFileSizeHigh &&
                     fi1.dwFileAttributes == fi2.dwFileAttributes && fi1.nNumberOfLinks == fi2.nNumberOfLinks &&
                     FileTimeEq(fi1.ftLastWriteTime, fi2.ftLastWriteTime) && FileTimeEq(fi1.ftCreationTime, fi2.ftCreationTime);
            needFallback = false;
        }
    }

    CloseHandle(handle1);
    CloseHandle(handle2);

    if (!needFallback)
        return isSame;

    ScopedMem<WCHAR> npath1(Normalize(path1));
    ScopedMem<WCHAR> npath2(Normalize(path2));
    // consider the files different, if their paths can't be normalized
    return npath1 && str::EqI(npath1, npath2);
}

bool HasVariableDriveLetter(const WCHAR *path)
{
    WCHAR root[] = L"?:\\";
    root[0] = towupper(path[0]);
    if (root[0] < 'A' || 'Z' < root[0])
        return false;

    UINT driveType = GetDriveType(root);
    return DRIVE_REMOVABLE == driveType ||
           DRIVE_CDROM == driveType ||
           DRIVE_NO_ROOT_DIR == driveType;
}

bool IsOnFixedDrive(const WCHAR *path)
{
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

static bool MatchWildcardsRec(const WCHAR *fileName, const WCHAR *filter)
{
#define AtEndOf(str) (*(str) == '\0')
    switch (*filter) {
    case '\0': case ';':
        return AtEndOf(fileName);
    case '*':
        filter++;
        while (!AtEndOf(fileName) && !MatchWildcardsRec(fileName, filter))
            fileName++;
        return !AtEndOf(fileName) || AtEndOf(filter) || *filter == ';';
    case '?':
        return !AtEndOf(fileName) && MatchWildcardsRec(fileName + 1, filter + 1);
    default:
        return towlower(*fileName) == towlower(*filter) &&
               MatchWildcardsRec(fileName + 1, filter + 1);
    }
#undef AtEndOf
}

/* matches the filename of a path against a list of semicolon
   separated filters as used by the common file dialogs
   (e.g. "*.pdf;*.xps;?.*" will match all PDF and XPS files and
   all filenames consisting of only a single character and
   having any extension) */
bool Match(const WCHAR *path, const WCHAR *filter)
{
    path = GetBaseName(path);
    while (str::FindChar(filter, ';')) {
        if (MatchWildcardsRec(path, filter))
            return true;
        filter = str::FindChar(filter, ';') + 1;
    }
    return MatchWildcardsRec(path, filter);
}

bool IsAbsolute(const WCHAR *path)
{
    return !PathIsRelative(path);
}

// returns the path to either the %TEMP% directory or a
// non-existing file inside whose name starts with filePrefix
WCHAR *GetTempPath(const WCHAR *filePrefix)
{
    WCHAR tempDir[MAX_PATH - 14];
    DWORD res = ::GetTempPath(dimof(tempDir), tempDir);
    if (!res || res >= dimof(tempDir))
        return NULL;
    if (!filePrefix)
        return str::Dup(tempDir);
    WCHAR path[MAX_PATH];
    if (!GetTempFileName(tempDir, filePrefix, 0, path))
        return NULL;
    return str::Dup(path);
}

// returns a path to the application module's directory
// with either the given fileName or the module's name
// (module is the EXE or DLL in which path::GetAppPath resides)
WCHAR *GetAppPath(const WCHAR *fileName)
{
    WCHAR modulePath[MAX_PATH];
    modulePath[0] = '\0';
    GetModuleFileName(CURRENT_HMODULE, modulePath, dimof(modulePath));
    modulePath[dimof(modulePath) - 1] = '\0';
    if (!fileName)
        return str::Dup(modulePath);
    ScopedMem<WCHAR> moduleDir(path::GetDir(modulePath));
    return path::Join(moduleDir, fileName);
}

}

namespace file {

HANDLE OpenReadOnly(const WCHAR *filePath)
{
    return CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

bool Exists(const WCHAR *filePath)
{
    if (NULL == filePath)
        return false;

    WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
    BOOL res = GetFileAttributesEx(filePath, GetFileExInfoStandard, &fileInfo);
    if (0 == res)
        return false;

    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return false;
    return true;
}

// returns -1 on error (can't use INVALID_FILE_SIZE because it won't cast right)
int64 GetSize(const WCHAR *filePath)
{
    CrashIf(!filePath);
    if (!filePath) return -1;

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

char *ReadAll(const WCHAR *filePath, size_t *fileSizeOut, Allocator *allocator)
{
    int64 size64 = GetSize(filePath);
    if (size64 < 0)
        return NULL;
    size_t size = (size_t)size64;
#ifdef _WIN64
    CrashIf(size != (size_t)size64);
#else
    if (size != size64)
        return NULL;
#endif

    // overflow check
    if (size + sizeof(WCHAR) < sizeof(WCHAR))
        return NULL;
    /* allocate one character more and zero-terminate just in case it's a
       text file we'll want to treat as C string. Doesn't hurt for binary
       files (note: two byte terminator for UTF-16 files) */
    char *data = (char *)Allocator::Alloc(allocator, size + sizeof(WCHAR));
    if (!data)
        return NULL;

    if (!ReadN(filePath, data, size)) {
        Allocator::Free(allocator, data);
        return NULL;
    }

    // zero-terminate for convenience
    data[size] = data[size + 1] = '\0';

    if (fileSizeOut)
        *fileSizeOut = size;
    return data;
}

char *ReadAllUtf(const char *filePath, size_t *fileSizeOut, Allocator *allocator)
{
    WCHAR buf[512];
    str::Utf8ToWcharBuf(filePath, str::Len(filePath), buf, dimof(buf));
    return ReadAll(buf, fileSizeOut, allocator);
}

// buf must be at least toRead in size (note: it won't be zero-terminated)
bool ReadN(const WCHAR *filePath, char *buf, size_t toRead)
{
    ScopedHandle h(OpenReadOnly(filePath));
    if (h == INVALID_HANDLE_VALUE)
        return false;

    DWORD nRead;
    BOOL ok = ReadFile(h, buf, (DWORD)toRead, &nRead, NULL);
    return ok && nRead == toRead;
}

bool WriteAll(const WCHAR *filePath, const void *data, size_t dataLen)
{
    ScopedHandle h(CreateFile(filePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
    if (INVALID_HANDLE_VALUE == h)
        return false;

    DWORD size;
    BOOL ok = WriteFile(h, data, (DWORD)dataLen, &size, NULL);
    assert(!ok || (dataLen == (size_t)size));
    return ok && dataLen == (size_t)size;
}

bool WriteAllUtf(const char *filePath, const void *data, size_t dataLen)
{
    WCHAR buf[512];
    str::Utf8ToWcharBuf(filePath, str::Len(filePath), buf, dimof(buf));
    return WriteAll(buf, data, dataLen);
}

// Return true if the file wasn't there or was successfully deleted
bool Delete(const WCHAR *filePath)
{
    BOOL ok = DeleteFile(filePath);
    return ok || GetLastError() == ERROR_FILE_NOT_FOUND;
}

FILETIME GetModificationTime(const WCHAR *filePath)
{
    FILETIME lastMod = { 0 };
    ScopedHandle h(OpenReadOnly(filePath));
    if (h != INVALID_HANDLE_VALUE)
        GetFileTime(h, NULL, NULL, &lastMod);
    return lastMod;
}

bool SetModificationTime(const WCHAR *filePath, FILETIME lastMod)
{
    ScopedHandle h(CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                              OPEN_EXISTING, 0, NULL));
    if (INVALID_HANDLE_VALUE == h)
        return false;
    return SetFileTime(h, NULL, NULL, &lastMod);
}

// return true if a file starts with string s of size len
bool StartsWithN(const WCHAR *filePath, const char *s, size_t len)
{
    ScopedMem<char> buf(AllocArray<char>(len));
    if (!buf)
        return false;

    if (!ReadN(filePath, buf, len))
        return false;
    return memeq(buf, s, len);
}

// return true if a file starts with null-terminated string s
bool StartsWith(const WCHAR *filePath, const char *s)
{
    return file::StartsWithN(filePath, s, str::Len(s));
}

int GetZoneIdentifier(const WCHAR *filePath)
{
    ScopedMem<WCHAR> path(str::Join(filePath, L":Zone.Identifier"));
    return GetPrivateProfileInt(L"ZoneTransfer", L"ZoneId", URLZONE_INVALID, path);
}

bool SetZoneIdentifier(const WCHAR *filePath, int zoneId)
{
    ScopedMem<WCHAR> path(str::Join(filePath, L":Zone.Identifier"));
    ScopedMem<WCHAR> id(str::Format(L"%d", zoneId));
    return WritePrivateProfileString(L"ZoneTransfer", L"ZoneId", id, path);
}

}

namespace dir {

bool Exists(const WCHAR *dir)
{
    if (NULL == dir)
        return false;

    WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
    BOOL res = GetFileAttributesEx(dir, GetFileExInfoStandard, &fileInfo);
    if (0 == res)
        return false;

    if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return true;
    return false;
}

// Return true if a directory already exists or has been successfully created
bool Create(const WCHAR *dir)
{
    BOOL ok = CreateDirectory(dir, NULL);
    if (ok)
        return true;
    return ERROR_ALREADY_EXISTS == GetLastError();
}

// creates a directory and all its parent directories that don't exist yet
bool CreateAll(const WCHAR *dir)
{
    ScopedMem<WCHAR> parent(path::GetDir(dir));
    if (!str::Eq(parent, dir) && !Exists(parent))
        CreateAll(parent);
    return Create(dir);
}

}
