/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "unarr-imp.h"

#ifdef _WIN32

#include <windows.h>

size_t ar_conv_rune_to_utf8(wchar_t rune, char *out)
{
    return WideCharToMultiByte(CP_UTF8, 0, &rune, 1, out, 3, NULL, NULL);
}

wchar_t *ar_conv_utf8_to_utf16(const char *str)
{
    int res = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t *wstr = calloc(res, sizeof(wchar_t));
    if (wstr)
        MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, res);
    return wstr;
}

#define CP_DOS 437

char *ar_conv_dos_to_utf8_utf16(const char *astr, wchar_t **wstr_opt)
{
    char *str = NULL;
    int res = MultiByteToWideChar(CP_DOS, 0, astr, -1, NULL, 0);
    wchar_t *wstr = calloc(res, sizeof(wchar_t));
    if (wstr) {
        MultiByteToWideChar(CP_DOS, 0, astr, -1, wstr, res);
        res = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
        str = malloc(res);
        if (str)
            WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, res, NULL, NULL);
    }
    if (wstr_opt)
        *wstr_opt = wstr;
    else
        free(wstr);
    return str;
}

time64_t ar_conv_dosdate_to_filetime(uint32_t dosdate)
{
    FILETIME ft;
    DosDateTimeToFileTime(HIWORD(dosdate), LOWORD(dosdate), &ft);
    return ft.dwLowDateTime | (time64_t)ft.dwHighDateTime << 32;
}

#else

#error implement string conversion functions

#endif
