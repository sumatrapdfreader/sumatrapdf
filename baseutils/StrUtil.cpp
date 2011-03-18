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

    return Format("%s%s%s", s1, s2, s3);
}

/* Concatenate 2 strings. Any string can be NULL.
   Caller needs to free() memory. */
WCHAR *Join(const WCHAR *s1, const WCHAR *s2, const WCHAR *s3)
{
    if (!s1) s1 = L"";
    if (!s2) s2 = L"";
    if (!s3) s3 = L"";

    return Format(L"%s%s%s", s1, s2, s3);
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

char *FmtV(const char *fmt, va_list args)
{
    char    message[256];
    size_t  bufCchSize = dimof(message);
    char  * buf = message;
    for (;;)
    {
        int count = _vsnprintf(buf, bufCchSize, fmt, args);
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

    if (buf == message)
        buf = Str::Dup(message);

    return buf;
}

char *Format(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *res = FmtV(fmt, args);
    va_end(args);
    return res;
}

WCHAR *FmtV(const WCHAR *fmt, va_list args)
{
    WCHAR   message[256];
    size_t  bufCchSize = dimof(message);
    WCHAR * buf = message;
    for (;;)
    {
        int count = _vsnwprintf(buf, bufCchSize, fmt, args);
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
    if (buf == message)
        buf = Str::Dup(message);

    return buf;
}

WCHAR *Format(const WCHAR *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    WCHAR *res = FmtV(fmt, args);    
    va_end(args);
    return res;
}

/* replace in <str> the chars from <oldChars> with their equivalents
   from <newChars> (similar to UNIX's tr command)
   Returns the number of replaced characters. */
size_t TransChars(TCHAR *str, const TCHAR *oldChars, const TCHAR *newChars)
{
    size_t findCount = 0;

    for (TCHAR *c = str; *c; c++) {
        const TCHAR *found = Str::FindChar(oldChars, *c);
        if (found) {
            *c = newChars[found - oldChars];
            findCount++;
        }
    }

    return findCount;
}

// Note: BufSet() should only be used when absolutely necessary (e.g. when
// handling buffers in OS-defined structures)
size_t BufSet(char *dst, size_t dstCchSize, const char *src)
{
    if (0 == dstCchSize) return 0;

    size_t srcCchSize = Str::Len(src);
    size_t size = min(dstCchSize, srcCchSize + 1);

    strncpy(dst, src, size);
    dst[size - 1] = '\0';

    return size;
}

size_t BufSet(WCHAR *dst, size_t dstCchSize, const WCHAR *src)
{
    if (0 == dstCchSize) return 0;

    size_t srcCchSize = Str::Len(src);
    size_t size = min(dstCchSize, srcCchSize + 1);

    wcsncpy(dst, src, size);
    dst[size - 1] = '\0';

    return size;
}

/* Convert binary data in <buf> of size <len> to a hex-encoded string */
char *MemToHex(const unsigned char *buf, int len)
{
    /* 2 hex chars per byte, +1 for terminating 0 */
    char *ret = SAZA(char, 2 * (size_t)len + 1);
    if (!ret)
        return NULL;
    for (int i = 0; i < len; i++)
        sprintf(ret + 2 * i, "%02x", *buf++);
    ret[2 * len] = '\0';
    return ret;
}

/* Reverse of MemToHex. Convert a 0-terminatd hex-encoded string <s> to
   binary data pointed by <buf> of max sisze bufLen.
   Returns false if size of <s> doesn't match <bufLen>. */
bool HexToMem(const char *s, unsigned char *buf, int bufLen)
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

void DbgOut(const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);
    ScopedMem<TCHAR> buf(FmtV(format, args));
    OutputDebugString(buf);
    va_end(args);
}

bool Parser::Init(const TCHAR *pos)
{
    this->pos = pos;
    return pos != NULL;
}

/* If the string at <pos> starts with either <str> or <alt>, skip <pos> past
   it and return true; otherwise return false. */
bool Parser::Skip(const TCHAR *str, const TCHAR *alt)
{
    if (str && StartsWith(pos, str)) {
        pos += Str::Len(str);
        return true;
    }
    if (alt && StartsWith(pos, alt)) {
        pos += Str::Len(alt);
        return true;
    }
    return false;
}

/* Copy the string from <pos> into <buffer> until <c> is found, and point
   <pos> at the end (after <c>). Returns true unless <bufSize> isn't
   big enough, in which case <pos> is still updated, but false is returned
   and <buffer> is truncated. If <c> is not found, <pos> will point to
   the end of the string and false is returned. */
bool Parser::CopyUntil(TCHAR c, TCHAR *buffer, size_t bufSize)
{
    const TCHAR *end = FindChar(pos, c);
    if (!end) {
        pos += Str::Len(pos);
        return false;
    }

    size_t len = min(bufSize, (size_t)(end - pos) + 1);
    Str::BufSet(buffer, len, pos);
    pos = end + 1;

    return len <= bufSize;
}

/* Exctract values from <pos> according to <format> similar to sscanf. */
bool Parser::Scan(const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);
    const TCHAR *cur = pos;
    for (const TCHAR *f = format; *f; f++) {
        if (*f != '%') {
            if (*f != *cur)
                goto Failure;
            cur++;
            continue;
        }
        f++;

        TCHAR *end = NULL;
        if ('u' == *f)
            *va_arg(args, unsigned int *) = _tcstoul(cur, &end, 10);
        else if ('d' == *f)
            *va_arg(args, int *) = _tcstol(cur, &end, 10);
        else if ('f' == *f)
            *va_arg(args, float *) = (float)_tcstod(cur, &end);
        else if ('%' == *f && *f == *cur)
            end = (TCHAR *)cur + 1;
        if (end <= cur)
            goto Failure;
        cur = end;
    }
    va_end(args);
    pos = cur;
    return true;

Failure:
    va_end(args);
    return false;
}

}
