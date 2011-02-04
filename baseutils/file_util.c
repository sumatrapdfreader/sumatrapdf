/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#include "base_util.h"
#include "tstr_util.h"
#include "file_util.h"

#include <sys/types.h>
#include <sys/stat.h>

char *FilePath_ConcatA(const char *path, const char *name)
{
    assert(path && name && *path);
    if (!path || !name || !*path) return NULL;

    if (char_is_dir_sep(path[str_len(path) - 1]))
        return str_cat(path, name);
    else
        return str_cat3(path, DIR_SEP_STR, name);
}

const TCHAR *FilePath_GetBaseName(const TCHAR *path)
{
    const TCHAR *fileBaseName = path + tstr_len(path);
    while (fileBaseName > path) {
        if (char_is_dir_sep((char)fileBaseName[-1])) {
            return fileBaseName;
        }
        --fileBaseName;
    }
    return fileBaseName;
}

TCHAR *FilePath_GetDir(const TCHAR *path)
{
    TCHAR *baseName;
    TCHAR *dir = tstr_dup(path);
    if (!dir) return NULL;
    baseName = (TCHAR *)FilePath_GetBaseName(dir);
    if (baseName > dir)
        baseName[-1] = '\0';
    return dir;
}

// Normalize a file path.
//  remove relative path component (..\ and .\),
//  replace slashes by backslashes,
//  conver to long form,
//  convert to lowercase (if bLowerCase=TRUE).
//
// Returns a pointer to a memory allocated block containing the normalized string.
//   The caller is responsible for freeing the block.
//   Returns NULL if the file does not exist or if a memory allocation fails.
//
// Precondition: the file must exist on the file system.
//
// Note: if bLowerCase=FALSE then the case is changed as follows:
//   - the case of the root component is preserved
//   - the case of rest is set to the wayt it is stored on the file system
//
// e.g. suppose the a file "C:\foo\Bar.Pdf" exists on the file system then
//    "c:\foo\bar.pdf" becomes "c:\foo\Bar.Pdf"
//    "C:\foo\BAR.PDF" becomes "C:\foo\Bar.Pdf"
TCHAR *FilePath_Normalize(const TCHAR *f, BOOL bLowerCase)
{
    TCHAR *path, *tmp;
    DWORD cb;

    // convert to absolute path, change slashes into backslashes
    cb = GetFullPathName(f, 0, NULL, NULL);
    if (!cb)
        return NULL;
    path = malloc(sizeof(TCHAR) * cb);
    if (!path)
        return NULL;
    GetFullPathName(f, cb, path, NULL);

    // convert to long form
    cb = GetLongPathName(path, NULL, 0);
    if (!cb)
        return path;
    tmp = realloc(path, sizeof(TCHAR) * cb);
    if (!tmp)
        return path;
    path = tmp;

    GetLongPathName(path, path, cb);

    // convert to lower case
    if (bLowerCase) {
        for (tmp = path; *tmp; tmp++)
            *tmp = _totlower(*tmp);
    }

    return path;
}

// Compare two file path.
// Returns 0 if the paths lhs and rhs point to the same file.
//         1 if the paths point to different files
//         -1 if an error occured
int FilePath_Compare(const TCHAR *lhs, const TCHAR *rhs)
{
    LPTSTR nl = NULL, nr = NULL;
    int ret = 0;

    nl = FilePath_Normalize(lhs, TRUE);
    if (!nl)
        goto CleanUp;

    nr = FilePath_Normalize(rhs, TRUE);
    if (!nr)
        goto CleanUp;

    ret = tstr_eq(nl, nr) ? 0 : 1;

CleanUp:
    free(nr);
    free(nl);
    return ret;
}

// Code adapted from http://stackoverflow.com/questions/562701/best-way-to-determine-if-two-path-reference-to-same-file-in-c-c/562830#562830
// Determine if 2 paths point ot the same file...
BOOL FilePath_IsSameFile(const TCHAR *path1, const TCHAR *path2)
{
    BOOL isSame = FALSE, needFallback = TRUE;
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

    if (handle1 != INVALID_HANDLE_VALUE)
        CloseHandle(handle1);
    if (handle2 != INVALID_HANDLE_VALUE)
        CloseHandle(handle2);

    if (needFallback)
        return FilePath_Compare(path1, path2) == 1;
    return isSame;
}

BOOL file_exists(const TCHAR *file_path)
{
    struct _stat buf;
    int          res;

    res = _tstat(file_path, &buf);
    if (0 != res)
        return FALSE;
    if ((buf.st_mode & _S_IFDIR))
        return FALSE;
    return TRUE;
}


#ifdef _WIN32
uint64_t file_size_get(const TCHAR *file_path)
{
    int                         ok;
    WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
    uint64_t                    res;

    if (NULL == file_path)
        return INVALID_FILE_SIZE;

    ok = GetFileAttributesEx(file_path, GetFileExInfoStandard, &fileInfo);
    if (!ok)
        return (uint64_t)INVALID_FILE_SIZE;

    res = fileInfo.nFileSizeHigh;
    res = (res << 32) + fileInfo.nFileSizeLow;

    return res;
}
#else
/* TODO: I think sth. must be done to get 64-bit size */
uint64_t file_size_get(const char *file_path)
{
    struct stat stat_buf;
    int         res;
    if (NULL == file_path)
        return INVALID_FILE_SIZE;
    res = stat(file_path, &stat_buf);
    if (0 != res)
        return INVALID_FILE_SIZE;
    return stat_buf.st_size;
}
#endif

#ifdef _WIN32
char *file_read_all(const TCHAR *file_path, size_t *file_size_out)
{
    DWORD       size, size_read;
    HANDLE      h;
    char *      data = NULL;
    int         f_ok;

    h = CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, NULL,  
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    size = GetFileSize(h, NULL);
    if (INVALID_FILE_SIZE == size)
        goto Exit;

    /* allocate one byte more and 0-terminate just in case it's a text
       file we'll want to treat as C string. Doesn't hurt for binary
       files */
    data = malloc(size + 1);
    if (!data)
        goto Exit;
    data[size] = 0;

    f_ok = ReadFile(h, data, size, &size_read, NULL);
    if (!f_ok) {
        free(data);
        data = NULL;
    }
    *file_size_out = size;
Exit:
    CloseHandle(h);
    return data;
}
#else
char *file_read_all(const char *file_path, size_t *file_size_out)
{
    FILE *fp = NULL;
    char *data = NULL;
    size_t read;

    size_t file_size = file_size_get(file_path);
    if (INVALID_FILE_SIZE == file_size)
        return NULL;

    data = malloc(file_size + 1);
    if (!data)
        goto Exit;
    data[file_size] = 0;

    fp = fopen(file_path, "rb");
    if (!fp)
        goto Error;

    read = fread(data, 1, file_size, fp);
    if (ferror(fp))
        goto Error;
    assert(read == file_size);
    if (read != file_size)
        goto Error;
    fclose(fp);
    return data;
Error:
    if (fp)
        fclose(fp);
    free(data);
    return NULL;
}
#endif

#ifdef _WIN32
BOOL write_to_file(const TCHAR *file_path, void *data, size_t data_len)
{
    DWORD       size;
    HANDLE      h;
    BOOL        f_ok;

    h = CreateFile(file_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,  
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;

    f_ok = WriteFile(h, data, data_len, &size, NULL);
    assert(!f_ok || (data_len == size));
    CloseHandle(h);
    return f_ok;
}
#else
// not supported
#endif
