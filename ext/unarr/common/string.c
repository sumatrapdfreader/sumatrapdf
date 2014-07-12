/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "unarr-imp.h"

#ifdef _WIN32

#include <windows.h>

size_t ar_conv_rune_to_utf8(wchar_t rune, char *out, size_t size)
{
    return WideCharToMultiByte(CP_UTF8, 0, &rune, 1, out, (int)size, NULL, NULL);
}

wchar_t *ar_conv_utf8_to_wide(const char *str)
{
    int res = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t *wstr = calloc(res, sizeof(wchar_t));
    if (wstr)
        MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, res);
    return wstr;
}

#define CP_DOS 437

char *ar_conv_dos_to_utf8_wide(const char *astr, wchar_t **wstr_opt)
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

#include <time.h>

/* data from http://en.wikipedia.org/wiki/Cp437 */
static const wchar_t gCp437[256] = {
    0, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266C, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
    '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', 0x2302,
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
};

size_t ar_conv_rune_to_utf8(wchar_t rune, char *out, size_t size)
{
    if (size < 1)
        return 0;
    if (rune < 0x0080) {
        *out++ = rune & 0x7F;
        return 1;
    }
    if (rune < 0x0800 && size >= 2) {
        *out++ = 0xC0 | ((rune >> 6) & 0x1F);
        *out++ = 0x80 | (rune & 0x3F);
        return 2;
    }
    if (rune < 0x10000 && size >= 3) {
        *out++ = 0xE0 | ((rune >> 12) & 0x0F);
        *out++ = 0x80 | ((rune >> 6) & 0x3F);
        *out++ = 0x80 | (rune & 0x3F);
        return 3;
    }
    *out++ = '?';
    return 1;
}

wchar_t *ar_conv_utf8_to_wide(const char *str)
{
    size_t size = strlen(str) + 1;
    wchar_t *wstr = calloc(size, sizeof(wchar_t));

    const char *in;
    wchar_t *wout;

    if (!wstr)
        return NULL;

    for (in = str, wout = wstr; *in; in++) {
        if (*in < 0x80)
            *wout++ = *in;
        else if (*in < 0xC0)
            *wout++ = '?';
        else if (*in < 0xE0 && (*(in + 1) & 0xC0) == 0x80)
            *wout++ = ((*in & 0x1F) << 5) | (*(in + 1) & 0x3F);
        else if (*in < 0xF0 && (*(in + 1) & 0xC0) == 0x80 && (*(in + 2) & 0xC0) == 0x80)
            *wout++ = ((*in & 0x0F) << 12) | ((*(in + 1) & 0x3F) << 6) | (*(in + 2) & 0x3F);
        else
            *wout++ = '?';
        while ((*(in + 1) & 0xC0) == 0x80)
            in++;
    }

    return wstr;
}

char *ar_conv_dos_to_utf8_wide(const char *astr, wchar_t **wstr_opt)
{
    size_t size = strlen(astr) + 1;
    char *str = calloc(size, 3);
    char *end_out = str + size * 3;
    wchar_t *wout = NULL;

    const char *in;
    char *out;

    if (!str)
        return NULL;
    if (wstr_opt)
        wout = *wstr_opt = calloc(size, sizeof(wchar_t));

    for (in = astr, out = str; *in; in++) {
        out += ar_conv_rune_to_utf8(gCp437[(uint8_t)*in], out, end_out - out);
        if (wout)
            *wout++ = gCp437[(uint8_t)*in];
    }

    return str;
}

time64_t ar_conv_dosdate_to_filetime(uint32_t dosdate)
{
    struct tm tm;

    tm.tm_sec = (dosdate & 0x1F) * 2;
    tm.tm_min = (dosdate >> 5) & 0x3F;
    tm.tm_hour = (dosdate >> 11) & 0x1F;
    tm.tm_mday = (dosdate >> 16) & 0x1F;
    tm.tm_mon = (dosdate >> 21) & 0x0F;
    tm.tm_year = ((dosdate >> 25) & 0x7F) + 80;
    tm.tm_isdst = -1;

    return (uint64_t)(mktime(&tm) + 11644473600) * 10000000;
}

#endif
