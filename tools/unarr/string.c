/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: GPLv3 */

#include "unarr-internals.h"

#ifdef _WIN32

WCHAR *conv_utf8_to_utf16(const char *str)
{
    int res = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    WCHAR *wstr = malloc(res);
    if (wstr)
        MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, res);
    return wstr;
}

char *conv_utf16_to_utf8(const WCHAR *wstr)
{
    int res = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char *str = malloc(res);
    if (str)
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, res, NULL, NULL);
    return str;
}

char *conv_ansi_to_utf8_utf16(const char *astr, WCHAR **wstr_opt)
{
    char *str = NULL;
    int res = MultiByteToWideChar(CP_ACP, 0, astr, -1, NULL, 0);
    WCHAR *wstr = malloc(res * sizeof(WCHAR));
    if (wstr) {
        MultiByteToWideChar(CP_ACP, 0, astr, -1, wstr, res);
        str = conv_utf16_to_utf8(wstr);
    }
    if (wstr_opt)
        *wstr_opt = wstr;
    else
        free(wstr);
    return str;
}

#else

#error implement string conversion functions

#endif
