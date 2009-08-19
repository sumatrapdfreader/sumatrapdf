/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef FILE_UTILS_H_
#define FILE_UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

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

const char *    FilePath_GetBaseNameA(const char *path);
const WCHAR *   FilePath_GetBaseNameW(const WCHAR *path);
#ifdef _UNICODE
#define FilePath_GetBaseName   FilePath_GetBaseNameW
#else
#define FilePath_GetBaseName   FilePath_GetBaseNameA
#endif

char *          FilePath_GetDirA(const char *path);
WCHAR *         FilePath_GetDirW(const WCHAR *path);
#ifdef _UNICODE
#define FilePath_GetDir   FilePath_GetDirW
#else
#define FilePath_GetDir   FilePath_GetDirA
#endif

WCHAR *         FilePath_NormalizeW(const WCHAR *f, BOOL bLowerCase);
char *         FilePath_NormalizeA(const char *f, BOOL bLowerCase);
#ifdef _UNICODE
#define FilePath_Normalize   FilePath_NormalizeW
#else
#define FilePath_Normalize   FilePath_NormalizeA
#endif

int             FilePath_CompareW(const WCHAR *lhs, const WCHAR *rhs);
int             FilePath_CompareA(const char *lhs, const char *rhs);
#ifdef _UNICODE
#define FilePath_Compare   FilePath_CompareW
#else
#define FilePath_Compare   FilePath_CompareA
#endif

#ifdef _WIN32
char *          file_read_all(const TCHAR *file_path, uint64_t *file_size_out);
#else
char *          file_read_all(const char *file_path, uint64_t *file_size_out);
#endif
uint64_t        file_size_get(const char *file_path);
BOOL            write_to_file(const TCHAR *file_path, void *data, uint64_t data_len);
BOOL            file_exists(const char *file_path);

#ifdef __cplusplus
}
#endif

#endif
