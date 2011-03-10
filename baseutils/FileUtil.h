/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef FileUtil_h
#define FileUtil_h

const TCHAR *   FilePath_GetBaseName(const TCHAR *path);
TCHAR *         FilePath_GetDir(const TCHAR *path);
TCHAR *         FilePath_Join(const TCHAR *path, const TCHAR *filename);
TCHAR *         FilePath_Normalize(const TCHAR *path);
BOOL            FilePath_IsSameFile(const TCHAR *path1, const TCHAR *path2);

BOOL            file_exists(const TCHAR *file_path);

char *          file_read_all(const TCHAR *file_path, size_t *file_size_out);
BOOL            write_to_file(const TCHAR *file_path, void *data, size_t data_len);
size_t          file_size_get(const TCHAR *file_path);

#endif
