/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: Simplified BSD (see ./COPYING) */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"

namespace Path {

static inline bool IsSep(WCHAR c)
{
    return L'\\' == c || L'/' == c;
}

static inline bool IsSep(char c)
{
    return '\\' == c || '/' == c;
}

// Note: returns pointer inside <path>, do not free
const WCHAR *GetBaseName(const WCHAR *path)
{
    const WCHAR *fileBaseName = path + Str::Len(path);
    for (; fileBaseName > path; fileBaseName--)
        if (IsSep(fileBaseName[-1]))
            break;
    return fileBaseName;
}

const char *GetBaseName(const char *path)
{
    const char *fileBaseName = path + Str::Len(path);
    for (; fileBaseName > path; fileBaseName--)
        if (IsSep(fileBaseName[-1]))
            break;
    return fileBaseName;
}

TCHAR *GetDir(const TCHAR *path)
{
    const TCHAR *baseName = GetBaseName(path);
    int dirLen;
    if (baseName <= path + 1)
        dirLen = Str::Len(path);
    else if (baseName[-2] == ':')
        dirLen = baseName - path;
    else
        dirLen = baseName - path - 1;
    return Str::DupN(path, dirLen);
}

TCHAR *Join(const TCHAR *path, const TCHAR *filename)
{
    if (IsSep(*filename))
        filename++;
    TCHAR *sep = NULL;
    if (!IsSep(path[Str::Len(path) - 1]))
        sep = _T("\\");
    return Str::Join(path, sep, filename);
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
TCHAR *Normalize(const TCHAR *path)
{
    // convert to absolute path, change slashes into backslashes
    DWORD cch = GetFullPathName(path, 0, NULL, NULL);
    if (!cch)
        return NULL;
    TCHAR *normpath = SAZA(TCHAR, cch);
    if (!path)
        return NULL;
    GetFullPathName(path, cch, normpath, NULL);

    // convert to long form
    cch = GetLongPathName(normpath, NULL, 0);
    if (!cch)
        return normpath;
    TCHAR *tmp = (TCHAR *)realloc(normpath, cch * sizeof(TCHAR));
    if (!tmp)
        return normpath;
    normpath = tmp;

    GetLongPathName(normpath, normpath, cch);
    return normpath;
}

// Compare two file path.
// Returns 0 if the paths lhs and rhs point to the same file.
//         1 if the paths point to different files
//         -1 if an error occured
static int Compare(const TCHAR *lhs, const TCHAR *rhs)
{
    ScopedMem<TCHAR> nl(Normalize(lhs));
    ScopedMem<TCHAR> nr(Normalize(rhs));
    if (!nl || !nr)
        return -1;
    if (!Str::EqI(nl, nr))
        return 1;
    return 0;
}

// Code adapted from http://stackoverflow.com/questions/562701/best-way-to-determine-if-two-path-reference-to-same-file-in-c-c/562830#562830
// Determine if 2 paths point ot the same file...
bool IsSame(const TCHAR *path1, const TCHAR *path2)
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
            needFallback = FALSE;
        }
    }

    CloseHandle(handle1);
    CloseHandle(handle2);

    if (needFallback)
        return Compare(path1, path2) == 0;
    return isSame;
}

}

namespace File {

bool Exists(const TCHAR *filePath)
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

size_t GetSize(const TCHAR *filePath)
{
    WIN32_FILE_ATTRIBUTE_DATA   fileInfo;

    if (NULL == filePath)
        return INVALID_FILE_SIZE;

    BOOL res = GetFileAttributesEx(filePath, GetFileExInfoStandard, &fileInfo);
    if (0 == res)
        return INVALID_FILE_SIZE;

    size_t size = fileInfo.nFileSizeLow;;
#ifdef _WIN64
    size += fileInfo.nFileSizeHigh << 32;
#else
    if (fileInfo.nFileSizeHigh > 0)
        return INVALID_FILE_SIZE;
#endif

    return size;
}

char *ReadAll(const TCHAR *filePath, size_t *fileSizeOut)
{
    char *data = NULL;

    HANDLE h = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,  
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    DWORD size = GetFileSize(h, NULL);
    if (INVALID_FILE_SIZE == size)
        goto Exit;

    /* allocate one byte more and 0-terminate just in case it's a text
       file we'll want to treat as C string. Doesn't hurt for binary
       files */
    data = SAZA(char, size + 1);
    if (!data)
        goto Exit;

    DWORD sizeRead;
    bool ok = ReadFile(h, data, size, &sizeRead, NULL);
    if (!ok || sizeRead != size) {
        free(data);
        data = NULL;
    } else if (fileSizeOut)
        *fileSizeOut = size;
Exit:
    CloseHandle(h);
    return data;
}

bool WriteAll(const TCHAR *filePath, void *data, size_t dataLen)
{
    HANDLE h = CreateFile(filePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,  
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;

    DWORD size;
    BOOL ok = WriteFile(h, data, (DWORD)dataLen, &size, NULL);
    assert(!ok || (dataLen == size));
    CloseHandle(h);

    return ok && dataLen == size;
}

// Return true if the file wasn't there or was successfully deleted
bool Delete(const TCHAR *filePath)
{
    BOOL ok = DeleteFile(filePath);
    if (ok)
        return true;
    DWORD err = GetLastError();
    return ((ERROR_PATH_NOT_FOUND == err) || (ERROR_FILE_NOT_FOUND == err));
}
}

namespace Dir {

bool Exists(const TCHAR *dir)
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
bool Create(const TCHAR *dir)
{
    BOOL ok = CreateDirectory(dir, NULL);
    if (ok)
        return true;
    return ERROR_ALREADY_EXISTS == GetLastError();
}

}

