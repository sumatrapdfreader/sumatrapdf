/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

/* The most basic things, including string handling functions */
#include "BaseUtil.h"
#include "StrUtil.h"
#include "WStrUtil.h"

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

/* Caller needs to free() the result */
char *wstr_to_multibyte(const WCHAR *txt,  UINT CodePage)
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
WCHAR *multibyte_to_wstr(const char *src, UINT CodePage)
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

void win32_dbg_outW(const WCHAR *format, ...)
{
    WCHAR   buf[4096];
    va_list args;

    va_start(args, format);
    _vsnwprintf(buf, dimof(buf), format, args);
    OutputDebugStringW(buf);
    va_end(args);
}
