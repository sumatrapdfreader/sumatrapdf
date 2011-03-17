/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#include "BaseUtil.h"
#include "TStrUtil.h"
#include "FileUtil.h"
#include <sys/stat.h>

namespace Path {

static inline bool IsSep(TCHAR c)
{
    return '\\' == c || '//' == c;
}

const TCHAR *GetBaseName(const TCHAR *path)
{
    const TCHAR *fileBaseName = path + Str::Len(path);
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
    return tstr_dupn(path, dirLen);
}

TCHAR *Join(const TCHAR *path, const TCHAR *filename)
{
    if (IsSep(*filename))
        filename++;
    bool needsSep = !IsSep(path[Str::Len(path) - 1]);
    if (needsSep)
        return tstr_printf(_T("%s\\%s"), path, filename);
    return tstr_cat(path, filename);
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
//   - the case of rest is set to the wayt it is stored on the file system
//
// e.g. suppose the a file "C:\foo\Bar.Pdf" exists on the file system then
//    "c:\foo\bar.pdf" becomes "c:\foo\Bar.Pdf"
//    "C:\foo\BAR.PDF" becomes "C:\foo\Bar.Pdf"
TCHAR *Normalize(const TCHAR *path)
{
    // convert to absolute path, change slashes into backslashes
    DWORD cb = GetFullPathName(path, 0, NULL, NULL);
    if (!cb)
        return NULL;
    TCHAR *normpath = SAZA(TCHAR, cb);
    if (!path)
        return NULL;
    GetFullPathName(path, cb, normpath, NULL);

    // convert to long form
    cb = GetLongPathName(normpath, NULL, 0);
    if (!cb)
        return normpath;
    TCHAR *tmp = (TCHAR *)realloc(normpath, cb * sizeof(TCHAR));
    if (!tmp)
        return normpath;
    normpath = tmp;

    GetLongPathName(normpath, normpath, cb);
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
    if (!tstr_ieq(nl, nr))
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
    struct _stat buf;
    int          res;

    res = _tstat(filePath, &buf);
    if (0 != res)
        return false;
    if ((buf.st_mode & _S_IFDIR))
        return false;
    return true;
}

size_t GetSize(const TCHAR *filePath)
{
    WIN32_FILE_ATTRIBUTE_DATA   fileInfo;

    if (NULL == filePath)
        return INVALID_FILE_SIZE;

    bool ok = GetFileAttributesEx(filePath, GetFileExInfoStandard, &fileInfo);
    if (!ok)
        return INVALID_FILE_SIZE;

    size_t res = fileInfo.nFileSizeLow;;
#ifdef _WIN64
    res += fileInfo.nFileSizeHigh << 32;
#else
    if (fileInfo.nFileSizeHigh > 0)
        return INVALID_FILE_SIZE;
#endif

    return res;
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
    data[size] = 0;

    DWORD sizeRead;
    bool f_ok = ReadFile(h, data, size, &sizeRead, NULL);
    if (!f_ok || sizeRead != size) {
        free(data);
        data = NULL;
    }
    else if (fileSizeOut)
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
    bool f_ok = WriteFile(h, data, (DWORD)dataLen, &size, NULL);
    assert(!f_ok || (dataLen == size));
    CloseHandle(h);

    return f_ok && dataLen == size;
}

}
