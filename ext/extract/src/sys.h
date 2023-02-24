#ifndef EXTRACT_SYS_H
#define EXTRACT_SYS_H

#include "extract/alloc.h"

#include <stdio.h>


int extract_systemf(extract_alloc_t* alloc, const char* format, ...);
/* Like system() but takes printf-style format and args. Also, if we return +ve
we set errno to EIO.

On iOS we always -1:ENOTSUP because the system() function is not available. */

int  extract_read_all(extract_alloc_t* alloc, FILE* in, char** o_out);
/* Reads until eof into zero-terminated malloc'd buffer. */

int  extract_read_all_path(extract_alloc_t* alloc, const char* path, char** o_text);
/* Reads entire file into zero-terminated malloc'd buffer. */

int  extract_write_all(const void* data, size_t data_size, const char* path);

int extract_check_path_shell_safe(const char* path);
/* Returns -1 with errno=EINVAL if <path> contains sequences that could make it
unsafe in shell commands. */

int extract_remove_directory(extract_alloc_t* alloc, const char* path);
/* Internally calls extract_systemf(); returns error if
extract_check_path_shell_safe(path) returns an error, but this is probably not
to be relied on. */

int extract_mkdir(const char* path, int mode);
/* Compatibility wrapper to cope on Windows. */

#endif
