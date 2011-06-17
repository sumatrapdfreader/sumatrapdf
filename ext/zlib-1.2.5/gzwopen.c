/* gzwopen.c -- open files with Unicode paths (for use in a DLL) */

#include <string.h>
#include <io.h>
#include <fcntl.h>

#include "zlib.h"

gzFile ZEXPORT gzwopen(const wchar_t *path, const char *mode)
{
    int flags = (strchr(mode, 'r') ? _O_RDONLY :
                 strchr(mode, 'w') ? _O_WRONLY | _O_CREAT :
                 strchr(mode, 'a') ? _O_APPEND | _O_CREAT : 0) |
                (strchr(mode, 'b') ? _O_BINARY : 0);
    int fd = _wopen(path, flags, 0);

    gzFile file = gzdopen(fd, mode);
    if (!file)
        _close(fd);
    return file;
}
