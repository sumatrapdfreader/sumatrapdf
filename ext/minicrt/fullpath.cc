/*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*_TSCHAR *_fullpath( _TSCHAR *buf, const _TSCHAR *path, maxlen );
*
*Purpose:
*
*       _fullpath - combines the current directory with path to form
*       an absolute path. i.e. _fullpath takes care of .\ and ..\
*       in the path.
*
*       The result is placed in buf. If the length of the result
*       is greater than maxlen NULL is returned, otherwise
*       the address of buf is returned.
*
*       If buf is NULL then a buffer is malloc'ed and maxlen is
*       ignored. If there are no errors then the address of this
*       buffer is returned.
*
*       If path specifies a drive, the curent directory of this
*       drive is combined with path. If the drive is not valid
*       and _fullpath needs the current directory of this drive
*       then NULL is returned.  If the current directory of this
*       non existant drive is not needed then a proper value is
*       returned.
*       For example:  path = "z:\\pop" does not need z:'s current
*       directory but path = "z:pop" does.
*
*
*
*Entry:
*       _TSCHAR *buf  - pointer to a buffer maintained by the user;
*       _TSCHAR *path - path to "add" to the current directory
*       int maxlen - length of the buffer pointed to by buf
*
*Exit:
*       Returns pointer to the buffer containing the absolute path
*       (same as buf if non-NULL; otherwise, malloc is
*       used to allocate a buffer)
*
*Exceptions:
*
*******************************************************************************/

#include <tchar.h>
#include <malloc.h>
#include <stdlib.h>
#include <windows.h>


_TSCHAR * __cdecl _tfullpath(_TSCHAR *UserBuf, const _TSCHAR *path, size_t maxlen) {
        _TSCHAR *buf;
        _TSCHAR *pfname;
        unsigned long count;

        // don't handle this case to reduce dependancies, add this later if required
        if (!path || !*path)  /* no work to do */
            // return( _tgetcwd( UserBuf, (int)maxlen ) );
            return( NULL );

        /* allocate buffer if necessary */

        if (!UserBuf)
            if (!(buf = reinterpret_cast<_TSCHAR*>(malloc(_MAX_PATH * sizeof(_TSCHAR))))) {
                // errno = ENOMEM;
                return( NULL );
            }
            else
                maxlen = _MAX_PATH;
        else
            buf = UserBuf;

        count = GetFullPathName( path,
                                  (int)maxlen,
                                  buf,
                                  &pfname );

        if (count >= maxlen) {
            if (!UserBuf)
                free(buf);
            // errno = ERANGE;
            return( NULL );
        }
        else if (count == 0) {
            if (!UserBuf)
                free(buf);
            // _dosmaperr( GetLastError() );
            return( NULL );
        }

        return( buf );

}
