/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: Simplified BSD (see ./COPYING) */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "Vec.h"
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
            needFallback = false;
        }
    }

    CloseHandle(handle1);
    CloseHandle(handle2);

    if (!needFallback)
        return isSame;

    ScopedMem<TCHAR> npath1(Normalize(path1));
    ScopedMem<TCHAR> npath2(Normalize(path2));
    // consider the files different, if their paths can't be normalized
    if (!npath1 || !npath2)
        return false;
    return Str::EqI(npath1, npath2);
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

    size_t size = fileInfo.nFileSizeLow;
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
    size_t size = GetSize(filePath);
    if (INVALID_FILE_SIZE == size)
        return NULL;

    /* allocate one byte more and 0-terminate just in case it's a text
       file we'll want to treat as C string. Doesn't hurt for binary
       files */
    char *data = SAZA(char, size + 1);
    if (!data)
        return NULL;

    if (!ReadAll(filePath, data, size)) {
        free(data);
        return NULL;
    }

    if (fileSizeOut)
        *fileSizeOut = size;
    return data;
}

bool ReadAll(const TCHAR *filePath, char *buffer, size_t bufferLen)
{
    HANDLE h = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,  
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h == INVALID_HANDLE_VALUE)
        return false;

    DWORD sizeRead;
    BOOL ok = ReadFile(h, buffer, bufferLen, &sizeRead, NULL);
    CloseHandle(h);

    return ok && sizeRead == bufferLen;
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

FILETIME GetModificationTime(const TCHAR *filePath)
{
    FILETIME lastMod = { 0 };
    HANDLE h = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,  
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h != INVALID_HANDLE_VALUE)
        GetFileTime(h, NULL, NULL, &lastMod);
    CloseHandle(h);
    return lastMod;
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
TCHAR *FormatNumWithThousandSep(size_t num)
{
    TCHAR thousandSep[4];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, thousandSep, dimof(thousandSep));
    ScopedMem<TCHAR> buf(Str::Format(_T("%Iu"), num));

    Str::Str<TCHAR> res(32);
    int i = 3 - (Str::Len(buf) % 3);
    for (TCHAR *src = buf.Get(); *src; src++) {
        res.Append(*src);
        if (*(src + 1) && i == 2)
            res.Append(thousandSep);
        i = (i + 1) % 3;
    }

    return res.StealData();
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
TCHAR *FormatFloatWithThousandSep(double number, const TCHAR *unit)
{
    size_t num = (size_t)(number * 100);

    ScopedMem<TCHAR> tmp(FormatNumWithThousandSep(num / 100));
    TCHAR decimal[4];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decimal, dimof(decimal));

    // always add between one and two decimals after the point
    ScopedMem<TCHAR> buf(Str::Format(_T("%s%s%02d"), tmp, decimal, num % 100));
    if (Str::EndsWith(buf, _T("0")))
        buf[Str::Len(buf) - 1] = '\0';

    return unit ? Str::Format(_T("%s %s"), buf, unit) : Str::Dup(buf);
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
