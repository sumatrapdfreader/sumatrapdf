/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileUtil.h"

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

WCHAR *Join(const WCHAR *path, const WCHAR *filename)
{
    if (IsSep(*filename))
        filename++;
    WCHAR *sep = NULL;
    if (!IsSep(path[str::Len(path) - 1]))
        sep = L"\\";
    return str::Join(path, sep, filename);
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
    WCHAR *normpath = AllocArray<WCHAR>(cch);
    GetLongPathName(fullpath, normpath, cch);
    return normpath;
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
                     fi1.nFileIndexHigh == fi2.nFileIndexHigh &&
                     fi1.nFileIndexLow == fi2.nFileIndexLow;
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
    if (!npath1 || !npath2)
        return false;
    return str::EqI(npath1, npath2);
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

static bool MatchWildcardsRec(const WCHAR *filename, const WCHAR *filter)
{
#define AtEndOf(str) (*(str) == '\0')
    switch (*filter) {
    case '\0': case ';':
        return AtEndOf(filename);
    case '*':
        filter++;
        while (!AtEndOf(filename) && !MatchWildcardsRec(filename, filter))
            filename++;
        return !AtEndOf(filename) || AtEndOf(filter) || *filter == ';';
    case '?':
        return !AtEndOf(filename) && MatchWildcardsRec(filename + 1, filter + 1);
    default:
        return towlower(*filename) == towlower(*filter) &&
               MatchWildcardsRec(filename + 1, filter + 1);
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

}

namespace file {

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
    ScopedHandle h(CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
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

char *ReadAll(const WCHAR *filePath, size_t *fileSizeOut)
{
    int64 size64 = GetSize(filePath);
    if (size64 < 0)
        return NULL;
    size_t size = (size_t)size64;
#ifdef _WIN64
    CrashIf(size != size64);
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
    char *data = (char *)malloc(size + sizeof(WCHAR));
    if (!data)
        return NULL;

    if (!ReadAll(filePath, data, size)) {
        free(data);
        return NULL;
    }

    // zero-terminate for convenience
    data[size] = data[size + 1] = '\0';

    if (fileSizeOut)
        *fileSizeOut = size;
    return data;
}

bool ReadAll(const WCHAR *filePath, char *buffer, size_t bufferLen)
{
    ScopedHandle h(CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
    if (h == INVALID_HANDLE_VALUE)
        return false;

    DWORD sizeRead;
    BOOL ok = ReadFile(h, buffer, (DWORD)bufferLen, &sizeRead, NULL);
    return ok && sizeRead == bufferLen;
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

// Return true if the file wasn't there or was successfully deleted
bool Delete(const WCHAR *filePath)
{
    BOOL ok = DeleteFile(filePath);
    return ok || GetLastError() == ERROR_FILE_NOT_FOUND;
}

FILETIME GetModificationTime(const WCHAR *filePath)
{
    FILETIME lastMod = { 0 };
    ScopedHandle h(CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
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

bool StartsWith(const WCHAR *filePath, const char *magicNumber, size_t len)
{
    if (len == (size_t)-1)
        len = str::Len(magicNumber);
    ScopedMem<char> header(AllocArray<char>(len));
    if (!header)
        return false;

    ReadAll(filePath, header, len);
    return memeq(header, magicNumber, len);
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
