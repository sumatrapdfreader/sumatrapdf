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

/* TODO: handle TCHAR (UNICODE) properly by converting to path to unicode
   if UNICODE or _UNICODE symbols are defined */
/* Start iteration of all files 'path'. 'path' should be a directory
   name but doesn't have to end with "\" (we append it if it's missing)
   Retuns NULL if there was an error.
   When no longer needed, the result should be deleted with
  'DirIter_Delete'.
*/
DirIterState *DirIter_New(const char *path)
{
    DirIterState *state;

    assert(path);

    if (!path)
        return NULL;

    state = SA(DirIterState);
    if (!state)
        return NULL;

    /* TODO: made state->cleanPath cannonical */
    state->cleanPath = str_dup(path);
    state->iterPath = FilePath_ConcatA(path, "*");
    if (!state->cleanPath || !state->iterPath) {
        DirIter_Delete(state);
        return NULL;
    }
    state->dir = INVALID_HANDLE_VALUE;   
    return state;
}

/* Get information about next file in a directory.
   Returns FALSE on end of iteration (or error). */
BOOL DirIter_Next(DirIterState *s)
{
    BOOL    found;

    if (INVALID_HANDLE_VALUE == s->dir) {
        s->dir = FindFirstFileA(s->iterPath, &(s->fileInfo));
        if (INVALID_HANDLE_VALUE == s->dir)
            return FALSE;
        goto CheckFile;
    }

    for(;;) {
        found = FindNextFileA(s->dir, &(s->fileInfo));
        if (!found)
            return FALSE;
        else
CheckFile:
            return TRUE;
    }
}

/* Free memory associated with 'state' */
void DirIter_Delete(DirIterState *state)
{
    if (state) {
        free(state->cleanPath);
        free(state->iterPath);
    }
    free(state);
}

static FileList *FileList_New(char *dir)
{
    FileList *fl;

    assert(dir);

    fl = SAZ(FileList);
    if (!fl)
        return NULL;
    fl->dirName = str_dup(dir);
    if (!fl->dirName) {
        free((void*)fl);
        return NULL;
    }
    return fl;
}

static BOOL FileList_InsertFileInfo(FileList *fl, FileInfo *fi)
{
    int real_count;
    FileInfo *last_fi;

    assert(fl);
    if (!fl)
        return FALSE;
    assert(fi);
    if (!fi)
        return FALSE;
    /* TODO: use scheme where we also track the last node, so that
       insert is O(1) and not O(n) */
    assert(!fi->next);
    fi->next = NULL;
    if (!fl->first) {
        assert(0 == fl->filesCount);
        fl->first = fi;
        fl->filesCount = 1;
        return TRUE;
    }

    last_fi = fl->first;
    assert(last_fi);
    real_count = 1;
    while (last_fi->next) {
        ++real_count;
        last_fi = last_fi->next;
    }

    assert(real_count == fl->filesCount);
    last_fi->next = fi;
    ++fl->filesCount;
    return TRUE;
}

void FileInfo_Delete(FileInfo *fi)
{
    if (!fi) return;
    free(fi->name);
    free(fi->path);
    free(fi);
}

static FileInfo *FileInfo_New(char *path, char *name, uint64_t size, DWORD attr, FILETIME *modificationTime)
{
    FileInfo *fi;

    assert(name);
    if (!name)
        return NULL;

    assert(modificationTime);
    if (!modificationTime)
        return NULL;

    fi = SAZ(FileInfo);
    if (!fi)
        return NULL;

    fi->name = str_dup(name);
    fi->path = str_dup(path);
    if (!fi->name || !fi->path) {
        FileInfo_Delete(fi);
        return NULL;
    }

    fi->size = size;
    fi->attr = attr;
    fi->modificationTime = *modificationTime;
    return fi;
}

static FileInfo* FileInfo_FromDirIterState(DirIterState *state)
{
    FileInfo *       fi;
    WIN32_FIND_DATAA *fd;
    char *          fileName;
    uint64_t         size;
    char *          filePath;

    assert(state);
    if (!state) return NULL;

    fd = &state->fileInfo;
    size = fd->nFileSizeHigh;
    size = size >> 32;
    size += fd->nFileSizeLow;
    /* TODO: handle UNICODE */
    fileName = fd->cFileName;
    filePath = FilePath_ConcatA(state->cleanPath, fileName);
    fi = FileInfo_New(filePath, fileName, size, fd->dwFileAttributes, &fd->ftLastWriteTime);
    return fi;
}

BOOL FileInfo_IsFile(FileInfo *fi)
{
    DWORD attr;
    assert(fi);
    if (!fi) return FALSE;
    attr = fi->attr;

    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
        return TRUE;
    return FALSE;
}

int FileInfo_IsDir(FileInfo *fi)
{
    DWORD attr;
    assert(fi);
    if (!fi) return FALSE;
    attr = fi->attr;

    if (attr & FILE_ATTRIBUTE_DIRECTORY)
        return TRUE;
    return FALSE;
}

static int FileList_Append(char *path, FileList *fl, int (*filter)(FileInfo *))
{
    FileInfo *      fi;
    DirIterState *  state;
    int             shouldInsert;

    if (!path || !fl)
        return 0;

    state = DirIter_New(path);
    if (!state) {
        return 0;
    }

    /* TODO: handle errors from DirIter_Next */        
    while (DirIter_Next(state)) {
        fi = FileInfo_FromDirIterState(state);
        if (!fi) {
            DirIter_Delete(state);
            return 0;
        }
        if (fi) {
            shouldInsert = 1;
            if (filter && !(*filter)(fi))
                shouldInsert = 0;
            if (shouldInsert)
                FileList_InsertFileInfo(fl, fi);
        }
    }
    DirIter_Delete(state);
    return 1;
}

/* Return a list of files/directories in a 'path'. Use filter function
   to filter out files that should not get included (return 0 from the function
   to exclude it from the list.
   Returns NULL in case of an error.
   Use FileList_Delete() to free up all memory associated with this data.
   Doesn't recurse into subdirectores, use FileList_GetRecursive for that. */
/* TODO: 'filter' needs to be implemented. */
/* TODO: add 'filterRegexp' argument that would allow filtering via regular
   expression */
FileList *FileList_Get(char* path, int (*filter)(FileInfo *))
{
    FileList *      fl;
    int             ok;

    if (!path)
        return NULL;

    /* TODO: should I expand "." ? */
    fl = FileList_New(path);
    if (!fl)
        return NULL;

    ok = FileList_Append(path, fl, filter);
    if (!ok) {
        FileList_Delete(fl);
        return NULL;
    }
    return fl;
}

/* Like FileList_Get() except recurses into sub-directories */
/* TODO: 'filter' needs to be implemented. */
/* TODO: add 'filterRegexp' argument that would allow filtering via regular
   expression */
FileList *FileList_GetRecursive(char* path, int (*filter)(FileInfo *))
{
#if 0
    StrList *toVisit = NULL;
    FileList *fl = NULL;
#endif
    assert(0);
    /* TODO: clearly, implement */
    return NULL;
}

void FileList_Delete(FileList *fl)
{
    FileInfo *fi;
    FileInfo *fi_next;
    if (!fl)
        return;
    fi = fl->first;
    while (fi) {
        fi_next = fi->next;
        FileInfo_Delete(fi);
        fi = fi_next;
    }
    free((void*)fl->dirName);
    free((void*)fl);
}

int FileList_Len(FileList *fl)
{
    return fl->filesCount;
}

FileInfo *FileList_GetFileInfo(FileList *fl, int file_no)
{
    FileInfo *fi;
    if (!fl)
        return NULL;
    if (file_no >= fl->filesCount)
        return NULL;
    fi = fl->first;
    while (file_no > 0) {
        assert(fi->next);
        if (!fi->next)
            return NULL;
        fi = fi->next;
        --file_no;
    }
    return fi;
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

    ok = GetFileAttributesEx(file_path, GetFileExInfoStandard, (void*)&fileInfo);
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
    unsigned long size;
    if (NULL == file_path)
        return INVALID_FILE_SIZE;
    res = stat(file_path, &stat_buf);
    if (0 != res)
        return INVALID_FILE_SIZE;
    size = (uint64_t)stat_buf.st_size;
    return size;
}
#endif

#ifdef _WIN32
char *file_read_all(const TCHAR *file_path, uint64_t *file_size_out)
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
    if (-1 == size)
        goto Exit;

    /* allocate one byte more and 0-terminate just in case it's a text
       file we'll want to treat as C string. Doesn't hurt for binary
       files */
    data = (char*)malloc(size + 1);
    if (!data)
        goto Exit;
    data[size] = 0;

    f_ok = ReadFile(h, data, size, &size_read, NULL);
    if (!f_ok) {
        free(data);
        data = NULL;
    }
    *file_size_out = (size_t)size;
Exit:
    CloseHandle(h);
    return data;
}
#else
char *file_read_all(const char *file_path, uint64_t *file_size_out)
{
    FILE *fp = NULL;
    char *data = NULL;
    size_t read;

    size_t file_size = file_size_get(file_path);
    if (INVALID_FILE_SIZE == file_size)
        return NULL;

    data = (char*)malloc(file_size + 1);
    if (!data)
        goto Exit;
    data[file_size] = 0;

    fp = fopen(file_path, "rb");
    if (!fp)
        goto Error;

    read = fread((void*)data, 1, file_size, fp);
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
    free((void*)data);
    return NULL;
}
#endif

#ifdef _WIN32
BOOL write_to_file(const TCHAR *file_path, void *data, uint64_t data_len)
{
    DWORD       size;
    HANDLE      h;
    BOOL        f_ok;

    h = CreateFile(file_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,  
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;

    f_ok = WriteFile(h, data, (DWORD)data_len, &size, NULL);
    assert(!f_ok || ((DWORD)data_len == size));
    CloseHandle(h);
    return f_ok;
}
#else
// not supported
#endif
