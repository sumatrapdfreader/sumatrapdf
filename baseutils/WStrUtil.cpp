/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */

/* The most basic things, including string handling functions */
#include "BaseUtil.h"
#include "WStrUtil.h"

WCHAR * wstr_cat_s(WCHAR * dest, size_t dst_cch_size, const WCHAR * src)
{
    return wstr_catn_s(dest, dst_cch_size, src, wstrlen(src));
}

WCHAR * wstr_catn_s(WCHAR *dst, size_t dst_cch_size, const WCHAR *src, size_t src_cch_size)
{
    WCHAR *dstEnd = dst + wstrlen(dst);
    size_t len = min(src_cch_size + 1, dst_cch_size - (dstEnd - dst));
    if (dst_cch_size <= (size_t)(dstEnd - dst))
        return NULL;
    
    wcsncpy(dstEnd, src, len);
    dstEnd[len - 1] = L'\0';
    
    if (src_cch_size >= len)
        return NULL;
    return dst;
}

/* Concatenate 3 strings. Any string can be NULL.
   Caller needs to free() memory. */
WCHAR *wstr_cat3(const WCHAR *str1, const WCHAR *str2, const WCHAR *str3)
{
    if (!str1)
        str1 = L"";
    if (!str2)
        str2 = L"";
    if (!str3)
        str3 = L"";

    return wstr_printf(L"%s%s%s", str1, str2, str3);
}

/* Concatenate 2 strings. Any string can be NULL.
   Caller needs to free() memory. */
WCHAR *wstr_cat(const WCHAR *str1, const WCHAR *str2)
{
    return wstr_cat3(str1, str2, NULL);
}

WCHAR *wstr_dupn(const WCHAR *str, size_t str_len_cch)
{
    WCHAR *copy;

    if (!str)
        return NULL;
    copy = (WCHAR *)memdup((void *)str, (str_len_cch + 1) * sizeof(WCHAR));
    if (copy)
        copy[str_len_cch] = 0;
    return copy;
}

int wstr_copyn(WCHAR *dst, size_t dst_cch_size, const WCHAR *src, size_t src_cch_size)
{
    size_t len = min(src_cch_size + 1, dst_cch_size);
    
    wcsncpy(dst, src, len);
    dst[len - 1] = L'\0';
    
    if (src_cch_size >= dst_cch_size)
        return FALSE;
    return TRUE;
}

int wstr_copy(WCHAR *dst, size_t dst_cch_size, const WCHAR *src)
{
    return wstr_copyn(dst, dst_cch_size, src, wstrlen(src));
}

int wstr_eq(const WCHAR *str1, const WCHAR *str2)
{
    if (!str1 && !str2)
        return TRUE;
    if (!str1 || !str2)
        return FALSE;
    if (0 == wcscmp(str1, str2))
        return TRUE;
    return FALSE;
}

int wstr_ieq(const WCHAR *str1, const WCHAR *str2)
{
    if (!str1 && !str2)
        return TRUE;
    if (!str1 || !str2)
        return FALSE;
    if (0 == _wcsicmp(str1, str2))
        return TRUE;
    return FALSE;
}

int wstr_eqn(const WCHAR *str1, const WCHAR *str2, size_t len)
{
    if (!str1 && !str2)
        return TRUE;
    if (!str1 || !str2)
        return FALSE;
    if (0 == wcsncmp(str1, str2, len))
        return TRUE;
    return FALSE;
}

/* return true if 'str' starts with 'txt', case-sensitive */
int wstr_startswith(const WCHAR *str, const WCHAR *txt)
{
    return wstr_eqn(str, txt, wcslen(txt));
}

int wstr_endswith(const WCHAR *txt, const WCHAR *end)
{
    size_t end_len;
    size_t txt_len;

    if (!txt || !end)
        return FALSE;

    txt_len = wstrlen(txt);
    end_len = wstrlen(end);
    if (end_len > txt_len)
        return FALSE;
    if (wstr_eq(txt+txt_len-end_len, end))
        return TRUE;
    return FALSE;
}

int wstr_endswithi(const WCHAR *txt, const WCHAR *end)
{
    size_t end_len;
    size_t txt_len;

    if (!txt || !end)
        return FALSE;

    txt_len = wstrlen(txt);
    end_len = wstrlen(end);
    if (end_len > txt_len)
        return FALSE;
    if (wstr_ieq(txt+txt_len-end_len, end))
        return TRUE;
    return FALSE;
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
int wstr_startswithi(const WCHAR *str, const WCHAR *txt)
{
    if (!str && !txt)
        return TRUE;
    if (!str || !txt)
        return FALSE;

    if (0 == _wcsnicmp(str, txt, wcslen(txt)))
        return TRUE;
    return FALSE;
}

int wstr_empty(const WCHAR *str)
{
    if (!str)
        return TRUE;
    if (0 == *str)
        return TRUE;
    return FALSE;
}

int wstr_contains(const WCHAR *str, WCHAR c)
{
    while (*str) {
        if (c == *str++)
            return TRUE;
    }
    return FALSE;
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
        buf = wstr_dup(message);

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
    int requiredBufSize = WideCharToMultiByte(CodePage, 0, txt, -1, NULL, 0, NULL, NULL);
    char *res = SAZA(char, requiredBufSize);
    if (!res)
        return NULL;
    WideCharToMultiByte(CodePage, 0, txt, -1, res, requiredBufSize, NULL, NULL);
    return res;
}

/* Caller needs to free() the result */
char *wstr_to_utf8(const WCHAR *txt)
{
    return wstr_to_multibyte(txt, CP_UTF8);
}

/* Caller needs to free() the result */
WCHAR *multibyte_to_wstr(const char *src, UINT CodePage)
{
    int requiredBufSize = MultiByteToWideChar(CodePage, 0, src, -1, NULL, 0);
    WCHAR *res = SAZA(WCHAR, requiredBufSize);
    if (!res)
        return NULL;
    MultiByteToWideChar(CodePage, 0, src, -1, res, requiredBufSize);
    return res;
}

/* Caller needs to free() the result */
WCHAR *utf8_to_wstr(const char *utf8)
{
    return multibyte_to_wstr(utf8, CP_UTF8);
}

/* replace a string pointed by <dst> with a copy of <src>
   (i.e. free existing <dst>).
   Returns FALSE if failed to replace (due to out of memory) */
BOOL wstr_dup_replace(WCHAR **dst, const WCHAR *src)
{
    WCHAR *dup = wstr_dup(src);
    if (!dup)
        return FALSE;
    free(*dst);
    *dst = dup;
    return TRUE;
}

void win32_dbg_outW(const WCHAR *format, ...)
{
    WCHAR   buf[4096];
    WCHAR * p = buf;
    va_list args;

    va_start(args, format);
    _vsnwprintf(p, dimof(buf), format, args);
    OutputDebugStringW(buf);
    va_end(args);
}
