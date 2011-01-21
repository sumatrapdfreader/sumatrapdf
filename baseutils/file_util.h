/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef FILE_UTILS_H_
#define FILE_UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* TODO: Are DirIter_* and FileInfo_* and FileList_* needed at all? */

typedef struct DirIterState {
    char *          fileName;
    char *          cleanPath;
    char *          iterPath;
    WIN32_FIND_DATAA fileInfo;
    HANDLE          dir;
} DirIterState;

typedef struct FileInfo {
    struct FileInfo *next;
    char *      path; /* full path of the file e.g. c:\foo\bar.txt */
    char *      name; /* just the name part e.g. bar.txt, points into 'path' */
    uint64_t    size;
    FILETIME    modificationTime;
    FILETIME    accessTime;
    FILETIME    createTime;
    DWORD       attr;
    /* TODO: file attributes like file type etc. */
} FileInfo;

typedef struct FileList {
    FileInfo *  first;
    char *      dirName; /* directory where files lives e.g. c:\windows\ */
    int         filesCount;
} FileList;

DirIterState *  DirIter_New(const char *path);
BOOL            DirIter_Next(DirIterState *s);
void            DirIter_Delete(DirIterState *state);

BOOL            FileInfo_IsFile(FileInfo *fi);
BOOL            FileInfo_IsDir(FileInfo *fi);
void            FileInfo_Delete(FileInfo *fi);
FileList *      FileList_Get(char* path, int (*filter)(FileInfo *));
FileList *      FileList_GetRecursive(char* path, int (*filter)(FileInfo *));
void            FileList_Delete(FileList *fl);
int             FileList_Len(FileList *fl);
FileInfo *      FileList_GetFileInfo(FileList *fl, int file_no);

const TCHAR *   FilePath_GetBaseName(const TCHAR *path);
TCHAR *         FilePath_GetDir(const TCHAR *path);

TCHAR *         FilePath_Normalize(const TCHAR *f, BOOL bLowerCase);
int             FilePath_Compare(const TCHAR *lhs, const TCHAR *rhs);
BOOL            FilePath_IsSameFile(const TCHAR *path1, const TCHAR *path2);

BOOL            file_exists(const TCHAR *file_path);

#ifdef _WIN32
char *          file_read_all(const TCHAR *file_path, size_t *file_size_out);
BOOL            write_to_file(const TCHAR *file_path, void *data, size_t data_len);
#else
char *          file_read_all(const char *file_path, size_t *file_size_out);
#endif
uint64_t        file_size_get(const TCHAR *file_path);

#ifdef __cplusplus
}
#endif

#endif
