/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

/* The most basic things, including string handling functions */
#include "BaseUtil.h"
#include "StrUtil.h"

namespace Str {

#define EntryCheck(arg1, arg2) \
    if (arg1 == arg2) \
        return true; \
    if (!arg1 || !arg2) \
        return false

bool Eq(const char *s1, const char *s2)
{
    EntryCheck(s1, s2);
    return 0 == strcmp(s1, s2);
}

bool Eq(const WCHAR *s1, const WCHAR *s2)
{
    EntryCheck(s1, s2);
    return 0 == wcscmp(s1, s2);
}

bool EqI(const char *s1, const char *s2)
{
    EntryCheck(s1, s2);
    return 0 == _stricmp(s1, s2);
}

bool EqI(const WCHAR *s1, const WCHAR *s2)
{
    EntryCheck(s1, s2);
    return 0 == _wcsicmp(s1, s2);
}

bool EqN(const char *s1, const char *s2, size_t len)
{
    EntryCheck(s1, s2);
    return 0 == strncmp(s1, s2, len);
}

bool EqN(const WCHAR *s1, const WCHAR *s2, size_t len)
{
    EntryCheck(s1, s2);
    return 0 == wcsncmp(s1, s2, len);
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(const char *str, const char *txt)
{
    EntryCheck(str, txt);
    return 0 == _strnicmp(str, txt, Str::Len(txt));
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(const WCHAR *str, const WCHAR *txt)
{
    EntryCheck(str, txt);
    return 0 == _wcsnicmp(str, txt, Str::Len(txt));
}

#undef EntryCheck

// TODO: could those be done as
// template <typename T> bool EndsWith(const T*, const T*) ?
// (I tried by failed)
bool EndsWith(const char *txt, const char *end)
{
    if (!txt || !end)
        return false;
    size_t txtLen = Str::Len(txt);
    size_t endLen = Str::Len(end);
    if (endLen > txtLen)
        return false;
    return Str::Eq(txt + txtLen - endLen, end);
}

bool EndsWith(const WCHAR *txt, const WCHAR *end)
{
    if (!txt || !end)
        return false;
    size_t txtLen = Str::Len(txt);
    size_t endLen = Str::Len(end);
    if (endLen > txtLen)
        return false;
    return Str::Eq(txt + txtLen - endLen, end);
}

bool EndsWithI(const char *txt, const char *end)
{
    if (!txt || !end)
        return false;
    size_t txtLen = Str::Len(txt);
    size_t endLen = Str::Len(end);
    if (endLen > txtLen)
        return false;
    return Str::EqI(txt + txtLen - endLen, end);
}

bool EndsWithI(const WCHAR *txt, const WCHAR *end)
{
    if (!txt || !end)
        return false;
    size_t txtLen = Str::Len(txt);
    size_t endLen = Str::Len(end);
    if (endLen > txtLen)
        return false;
    return Str::EqI(txt + txtLen - endLen, end);
}

/* Concatenate 2 strings. Any string can be NULL.
   Caller needs to free() memory. */
char *Join(const char *s1, const char *s2, const char *s3)
{
    if (!s1) s1= "";
    if (!s2) s2 = "";
    if (!s3) s3 = "";

    return str_printf("%s%s%s", s1, s2, s3);
}

/* Concatenate 2 strings. Any string can be NULL.
   Caller needs to free() memory. */
WCHAR *Join(const WCHAR *s1, const WCHAR *s2, const WCHAR *s3)
{
    if (!s1) s1 = L"";
    if (!s2) s2 = L"";
    if (!s3) s3 = L"";

    return wstr_printf(L"%s%s%s", s1, s2, s3);
}

char *DupN(const char *s, size_t lenCch)
{
    if (!s)
        return NULL;
    char *res = (char *)memdup((void *)s, lenCch + 1);
    if (res)
        res[lenCch] = 0;
    return res;
}

WCHAR *DupN(const WCHAR *s, size_t lenCch)
{
    if (!s)
        return NULL;
    WCHAR *res = (WCHAR *)memdup((void *)s, (lenCch + 1) * sizeof(WCHAR));
    if (res)
        res[lenCch] = 0;
    return res;
}

/* Caller needs to free() the result */
char *ToMultiByte(const WCHAR *txt, UINT CodePage)
{
    assert(txt);
    if (!txt) return NULL;

    int requiredBufSize = WideCharToMultiByte(CodePage, 0, txt, -1, NULL, 0, NULL, NULL);
    char *res = SAZA(char, requiredBufSize);
    if (!res)
        return NULL;
    WideCharToMultiByte(CodePage, 0, txt, -1, res, requiredBufSize, NULL, NULL);
    return res;
}

/* Caller needs to free() the result */
char *ToMultiByte(const char *src, UINT CodePageSrc, UINT CodePageDest)
{
    assert(src);
    if (!src) return NULL;

    if (CodePageSrc == CodePageDest)
        return Str::Dup(src);

    ScopedMem<WCHAR> tmp(ToWideChar(src, CodePageSrc));
    if (!tmp)
        return NULL;

    return ToMultiByte(tmp.Get(), CodePageDest);
}

/* Caller needs to free() the result */
WCHAR *ToWideChar(const char *src, UINT CodePage)
{
    assert(src);
    if (!src) return NULL;

    int requiredBufSize = MultiByteToWideChar(CodePage, 0, src, -1, NULL, 0);
    WCHAR *res = SAZA(WCHAR, requiredBufSize);
    if (!res)
        return NULL;
    MultiByteToWideChar(CodePage, 0, src, -1, res, requiredBufSize);
    return res;
}

}

char * str_cat_s(char * dst, size_t dst_cch_size, const char * src)
{
    size_t dstLen = Str::Len(dst);
    if (dst_cch_size <= dstLen)
        return NULL;

    int ok = str_copy(dst + dstLen, dst_cch_size - dstLen, src);
    if (!ok)
        return NULL;
    return dst;
}

int str_copy(char *dst, size_t dst_cch_size, const char *src)
{
    size_t src_cch_size = Str::Len(src);
    size_t len = min(src_cch_size + 1, dst_cch_size);
    
    strncpy(dst, src, len);
    dst[len - 1] = '\0';
    
    if (src_cch_size >= dst_cch_size)
        return FALSE;
    return TRUE;
}

/* Convert binary data in <buf> of size <len> to a hex-encoded string */
char *mem_to_hexstr(const unsigned char *buf, int len)
{
    int i;
    /* 2 hex chars per byte, +1 for terminating 0 */
    char *ret = (char *)calloc((size_t)len + 1, 2);
    if (!ret)
        return NULL;
    for (i = 0; i < len; i++) {
        sprintf(ret + 2 * i, "%02x", *buf++);
    }
    ret[2 * len] = '\0';
    return ret;
}

/* Reverse of mem_to_hexstr. Convert a 0-terminatd hex-encoded string <s> to
   binary data pointed by <buf> of max sisze bufLen.
   Returns FALSE if size of <s> doesn't match <bufLen>. */
bool hexstr_to_mem(const char *s, unsigned char *buf, int bufLen)
{
    for (; bufLen > 0; bufLen--) {
        int c;
        if (1 != sscanf(s, "%02x", &c))
            return false;
        s += 2;
        *buf++ = (unsigned char)c;
    }
    return *s == '\0';
}

char *str_printf(const char *format, ...)
{
    va_list args;
    char    message[256];
    size_t  bufCchSize = dimof(message);
    char  * buf = message;

    va_start(args, format);
    for (;;)
    {
        int count = vsnprintf(buf, bufCchSize, format, args);
        if (0 <= count && (size_t)count < bufCchSize)
            break;
        /* we have to make the buffer bigger. The algorithm used to calculate
           the new size is arbitrary (aka. educated guess) */
        if (buf != message)
            free(buf);
        if (bufCchSize < 4*1024)
            bufCchSize += bufCchSize;
        else
            bufCchSize += 1024;
        buf = SAZA(char, bufCchSize);
        if (!buf)
            break;
    }
    va_end(args);

    if (buf == message)
        buf = Str::Dup(message);

    return buf;
}

int str_printf_s(char *out, size_t out_cch_size, const char *format, ...)
{
    va_list args;
    int count;

    va_start(args, format);
    count = vsnprintf(out, out_cch_size, format, args);
    if (count < 0 || (size_t)count >= out_cch_size)
        out[out_cch_size - 1] = '\0';
    va_end(args);

    return count;
}

void win32_dbg_out(const char *format, ...)
{
    char        buf[4096];
    va_list     args;

    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    OutputDebugStringA(buf);
    va_end(args);
}

void win32_dbg_out_hex(const char *dsc, const unsigned char *data, int dataLen)
{
    char *hexStr;
    if (!data) dataLen = 0;

    hexStr = mem_to_hexstr(data, dataLen);
    win32_dbg_out("%s%s\n", dsc ? dsc : "", hexStr);
    free(hexStr);
}


// TODO: temporary
WCHAR * wstr_cat_s(WCHAR * dst, size_t dst_cch_size, const WCHAR * src)
{
    size_t dstLen = Str::Len(dst);
    if (dst_cch_size <= dstLen)
        return NULL;

    int ok = wstr_copy(dst + dstLen, dst_cch_size - dstLen, src);
    if (!ok)
        return NULL;
    return dst;
}

int wstr_copy(WCHAR *dst, size_t dst_cch_size, const WCHAR *src)
{
    size_t src_cch_size = Str::Len(src);
    size_t len = min(src_cch_size + 1, dst_cch_size);
    
    wcsncpy(dst, src, len);
    dst[len - 1] = L'\0';
    
    if (src_cch_size >= dst_cch_size)
        return FALSE;
    return TRUE;
}

WCHAR *wstr_printf(const WCHAR *format, ...)
{
    va_list args;
    WCHAR   message[256];
    size_t  bufCchSize = dimof(message);
    WCHAR * buf = message;

    va_start(args, format);
    for (;;)
    {
        int count = _vsnwprintf(buf, bufCchSize, format, args);
        if (0 <= count && (size_t)count < bufCchSize)
            break;
        /* we have to make the buffer bigger. The algorithm used to calculate
           the new size is arbitrary (aka. educated guess) */
        if (buf != message)
            free(buf);
        if (bufCchSize < 4*1024)
            bufCchSize += bufCchSize;
        else
            bufCchSize += 1024;
        buf = SAZA(WCHAR, bufCchSize);
        if (!buf)
            break;
    }
    va_end(args);

    if (buf == message)
        buf = Str::Dup(message);

    return buf;
}

int wstr_printf_s(WCHAR *out, size_t out_cch_size, const WCHAR *format, ...)
{
    va_list args;
    int count;

    va_start(args, format);
    count = _vsnwprintf(out, out_cch_size, format, args);
    if (count < 0 || (size_t)count >= out_cch_size)
        out[out_cch_size - 1] = '\0';
    va_end(args);

    return count;
}

void win32_dbg_outW(const WCHAR *format, ...)
{
    WCHAR   buf[4096];
    va_list args;

    va_start(args, format);
    _vsnwprintf(buf, dimof(buf), format, args);
    OutputDebugStringW(buf);
    va_end(args);
}
