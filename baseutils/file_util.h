/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef FILE_UTILS_H_
#define FILE_UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

const TCHAR *   FilePath_GetBaseName(const TCHAR *path);
TCHAR *         FilePath_GetDir(const TCHAR *path);

TCHAR *         FilePath_Normalize(const TCHAR *f, BOOL bLowerCase);
int             FilePath_Compare(const TCHAR *lhs, const TCHAR *rhs);
BOOL            FilePath_IsSameFile(const TCHAR *path1, const TCHAR *path2);

BOOL            file_exists(const TCHAR *file_path);
BOOL            dir_exists(const TCHAR *dir_path);

char *          file_read_all(const TCHAR *file_path, size_t *file_size_out);
BOOL            write_to_file(const TCHAR *file_path, void *data, size_t data_len);
size_t          file_size_get(const TCHAR *file_path);

#ifdef __cplusplus
}
#endif

#endif
