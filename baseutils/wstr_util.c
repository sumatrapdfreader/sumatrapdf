/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */

/* The most basic things, including string handling functions */

#include "base_util.h"
#include "wstr_util.h"

#include <str_strsafe.h>
#include <assert.h>

WCHAR * wstr_cat_s(WCHAR * dest, size_t dst_cch_size, const WCHAR * src)
{
    size_t len = wstr_len(dest);
    int count = dst_cch_size - len;
    int ret = _snwprintf(dest + len, count, L"%s", src);
    return (ret<count ) ? dest : NULL;
}

WCHAR * wstr_catn_s(WCHAR *dst, size_t dst_cch_size, const WCHAR *src, size_t src_cch_size)
{
    size_t len = wstr_len(dst);
    if (dst_cch_size > len + src_cch_size) {
        memcpy(dst + len, src, src_cch_size * sizeof *src);
        dst[len] = 0;
        return dst;
    }
    else
        return NULL;
}

WCHAR *wstr_cat4(const WCHAR *str1, const WCHAR *str2, const WCHAR *str3, const WCHAR *str4)
{
    WCHAR *str;
    WCHAR *tmp;
    size_t str1_len = 0;
    size_t str2_len = 0;
    size_t str3_len = 0;
    size_t str4_len = 0;

    if (str1)
        str1_len = wstrlen(str1);
    if (str2)
        str2_len = wstrlen(str2);
    if (str3)
        str3_len = wstrlen(str3);
    if (str4)
        str4_len = wstrlen(str4);

    str = (WCHAR*)zmalloc((str1_len + str2_len + str3_len + str4_len + 1)*sizeof(WCHAR));
    if (!str)
        return NULL;

    tmp = str;
    if (str1) {
        memcpy(tmp, str1, str1_len*sizeof(WCHAR));
        tmp += str1_len;
    }
    if (str2) {
        memcpy(tmp, str2, str2_len*sizeof(WCHAR));
        tmp += str2_len;
    }
    if (str3) {
        memcpy(tmp, str3, str3_len*sizeof(WCHAR));
        tmp += str3_len;
    }
    if (str4) {
        memcpy(tmp, str4, str1_len*sizeof(WCHAR));
    }
    return str;
}

WCHAR *wstr_cat3(const WCHAR *str1, const WCHAR *str2, const WCHAR *str3)
{
    return wstr_cat4(str1, str2, str3, NULL);
}

WCHAR *wstr_cat(const WCHAR *str1, const WCHAR *str2)
{
    return wstr_cat4(str1, str2, NULL, NULL);
}

WCHAR *wstr_dupn(const WCHAR *str, int str_len_cch)
{
    WCHAR *copy;

    if (!str)
        return NULL;
    copy = (WCHAR*)malloc((str_len_cch+1)*sizeof(WCHAR));
    if (!copy)
        return NULL;
    memcpy(copy, str, str_len_cch*sizeof(WCHAR));
    copy[str_len_cch] = 0;
    return copy;
}

WCHAR *wstr_dup(const WCHAR *str)
{
    return wstr_cat4(str, NULL, NULL, NULL);
}

int wstr_copyn(WCHAR *dst, int dst_cch_size, const WCHAR *src, int src_cch_size)
{
    WCHAR *end = dst + dst_cch_size - 1;
    if (0 == dst_cch_size) {
        if (0 == src_cch_size)
            return TRUE;
        else
            return FALSE;
    }

    while ((dst < end) && (src_cch_size > 0)) {
        *dst++ = *src++;
        --src_cch_size;
    }
    *dst = 0;
    if (0 == src_cch_size)
        return TRUE;
    else
        return FALSE;
}

int wstr_copy(WCHAR *dst, int dst_cch_size, const WCHAR *src)
{
    WCHAR *end = dst + dst_cch_size - 1;
    if (0 == dst_cch_size)
        return FALSE;

    while ((dst < end) && *src) {
        *dst++ = *src++;
    }
    *dst = 0;
    if (0 == *src)
        return TRUE;
    else
        return FALSE;
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

/* return true if 'str' starts with 'txt', case-sensitive */
int  wstr_startswith(const WCHAR *str, const WCHAR *txt)
{
    if (!str && !txt)
        return TRUE;
    if (!str || !txt)
        return FALSE;

    if (0 == wcsncmp(str, txt, wcslen(txt)))
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
int  wstr_startswithi(const WCHAR *str, const WCHAR *txt)
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

static void wchar_to_hex(WCHAR c, WCHAR* buffer)
{
    const WCHAR* numbers = L"0123456789ABCDEF";
    buffer[0]=numbers[c / 16];
    buffer[1]=numbers[c % 16];
}

int wstr_contains(const WCHAR *str, WCHAR c)
{
    while (*str) {
        if (c == *str++)
            return TRUE;
    }
    return FALSE;
}

static int wchar_is_ws(char c)
{
    switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return TRUE;
    }
    return FALSE;
}

/* If the string at <*strp> starts with string at <expect>, skip <*strp> past
    it and return TRUE; otherwise return FALSE. */
int wstr_skip(const WCHAR **strp, const WCHAR *expect)
{
    size_t len = wstr_len(expect);
    if (0 == wcsncmp(*strp, expect, len)) {
        *strp += len;
        return TRUE;
    }
    else {
        return FALSE;
    }
}

/* Copy the string from <*strp> into <dst> until <stop> is found, and point
    <*strp> at the end. Returns TRUE unless <dst_size> isn't big enough, in
    which case <*strp> is still updated, but FALE is returned and <dst> is
    truncated. If <delim> is not found, <*strp> will point to the end of the
    string and FALSE is returned. */
int
wstr_copy_skip_until(const WCHAR **strp, WCHAR *dst, size_t dst_size, WCHAR stop)
{
    const WCHAR *const str = *strp;
    size_t len = wstr_len(str);
    *strp = wmemchr(str, stop, len);
    if (NULL==*strp) {
        *strp = str+len;
        return FALSE;
    }
    else
        return wstr_copyn(dst, dst_size, str, *strp - str);
}


/* Given a pointer to a string in '*txt', skip past whitespace in the string
   and put the result in '*txt' */
void wstr_skip_ws(WCHAR **txtInOut)
{
    WCHAR *cur;
    if (!txtInOut)
        return;
    cur = *txtInOut;
    if (!cur)
        return;
    while (wchar_is_ws(*cur)) {
        ++cur;
    }
    *txtInOut = cur;
}

#define WCHAR_URL_DONT_ENCODE L"-_.!~*'()"

int wchar_needs_url_encode(WCHAR c)
{
    if ((c >= L'a') && (c <= L'z'))
        return FALSE;
    if ((c >= L'A') && (c <= L'Z'))
        return FALSE;
    if ((c >= L'0') && (c <= L'9'))
        return FALSE;
    if (wstr_contains(WCHAR_URL_DONT_ENCODE, c))
        return FALSE;
    return TRUE;
}

WCHAR *wstr_url_encode(const WCHAR *str)
{
    WCHAR *         encoded;
    WCHAR *         result;
    int             res_len = 0;
    const WCHAR *   tmp = str;

    while (*tmp) {
        if (wchar_needs_url_encode(*tmp))
            res_len += 3;
        else
            ++res_len;
        tmp++;
    }
    if (0 == res_len)
        return NULL;

    encoded = (WCHAR*)malloc((res_len+1)*sizeof(WCHAR));
    if (!encoded)
        return NULL;

    result = encoded;
    tmp = str;
    while (*tmp) {
        if (wchar_needs_url_encode(*tmp)) {
            *encoded++ = L'%';
            wchar_to_hex(*tmp, encoded);
            encoded += 2;
        } else {
            if (L' ' == *tmp)
                *encoded++ = L'+';
            else
                *encoded++ = *tmp;
        }
        tmp++;
    }
    *encoded = 0;
    return result;
}

WCHAR *wstr_escape(const WCHAR *txt)
{
    /* TODO: */
    return wstr_dup(txt);
}

WCHAR *wstr_printf(const WCHAR *format, ...)
{
    va_list     args;
    WCHAR       message[256] = {0};
    WCHAR  *    buf;
    size_t      bufCchSize;

    buf = &(message[0]);
    bufCchSize = sizeof(message)/sizeof(message[0]);

    va_start(args, format);
    for (;;)
    {
    #ifdef __GNUC__
        if (vsnwprintf(buf, bufCchSize, format, args) < bufCchSize)
            break;
    #else
        HRESULT hr;
        hr = StringCchVPrintfW(buf, bufCchSize, format, args);
        if (S_OK == hr)
            break;
        if (STRSAFE_E_INSUFFICIENT_BUFFER != hr)
        {
            /* any error other than buffer not big enough:
               a) should not happen
               b) means we give up */
            assert(FALSE);
            goto Error;
        }
    #endif
        /* we have to make the buffer bigger. The algorithm used to calculate
           the new size is arbitrary (aka. educated guess) */
        if (buf != &(message[0]))
            free(buf);
        if (bufCchSize < 4*1024)
            bufCchSize += bufCchSize;
        else
            bufCchSize += 1024;
        buf = (WCHAR *)malloc(bufCchSize*sizeof(WCHAR));
        if (NULL == buf)
            goto Error;
    }
    va_end(args);

    /* free the buffer if it was dynamically allocated */
    if (buf == &(message[0]))
        return wstr_dup(buf);

    return buf;
Error:
    if (buf != &(message[0]))
        free((void*)buf);

    return NULL;
}


/* Find character 'c' in string 'txt'.
   Return pointer to this character or NULL if not found */
const WCHAR *wstr_find_char(const WCHAR *txt, WCHAR c)
{
    while (*txt != c) {
        if (0 == *txt)
            return NULL;
        ++txt;
    }
    return txt;
}

/* A simplistic (and potentially wrong) conversion from ascii to unicode by
   setting unicode character value to ascii code, without taking encoding
   into account. 
   TODO: This is a band-aid and all callers should be changed
   to use the right conversion, eventually.
   The caller needs to free() return value.
*/
WCHAR *str_to_wstr_simplistic(const char *s)
{
    WCHAR *tmp;
    WCHAR *ret;

    assert(s);
    ret = (WCHAR*)malloc(sizeof(WCHAR)*(strlen(s)+1));
    if (!ret)
        return NULL;
    tmp = ret;
    while (*s) {
        *tmp++ = *s++;
    }
    *tmp = 0;
    return ret;
}

/* Caller needs to free() the result */
char *wstr_to_multibyte(const WCHAR *txt,  UINT CodePage)
{
    char *res;
    int requiredBufSize = WideCharToMultiByte(CodePage, 0, txt, -1, NULL, 0, NULL, NULL);
    res = (char*)malloc(requiredBufSize);
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
    WCHAR *res;
    int requiredBufSize = MultiByteToWideChar(CodePage, 0, src, -1, NULL, 0);
    res = (WCHAR*)malloc(requiredBufSize * sizeof(WCHAR));
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

static WCHAR *wstr_parse_quoted(WCHAR **txt)
{
    WCHAR *     strStart;
    WCHAR *     strCopy;
    WCHAR *     cur;
    WCHAR *     dst;
    WCHAR       c;
    size_t      len;

    assert(txt);
    if (!txt) return NULL;
    strStart = *txt;
    assert(strStart);
    if (!strStart) return NULL;

    assert('"' == *strStart);
    /* TODO: rewrite as 2-phase logic so that counting and copying are always in sync */
    ++strStart;
    cur = strStart;
    len = 0;
    for (;;) {
        c = *cur;
        if ((0 == c) || ('"' == c))
            break;
        if ('\\' == c) {
            /* TODO: should I un-escape more than '"' ?
               I used to un-escape '\' as well, but it wasn't right and
               files with UNC path like "\\foo\file.pdf" failed to load */
            if ('"' == cur[1]) {
                ++cur;
                c = *cur;
            }
        }
        ++cur;
        ++len;
    }

    strCopy = (WCHAR *)malloc(sizeof(WCHAR)*(len+1));
    if (!strCopy)
        return NULL;

    cur = strStart;
    dst = strCopy;
    for (;;) {
        c = *cur;
        if (0 == c)
            break;
        if ('"' == c) {
            ++cur;
            break;
        }
        if ('\\' == c) {
            /* TODO: should I un-escape more than '"' ?
               I used to un-escape '\' as well, but it wasn't right and
               files with UNC path like "\\foo\file.pdf" failed to load */
            if ('"' == cur[1]) {
                ++cur;
                c = *cur;
            }
        }
        *dst++ = c;
        ++cur;
    }
    *dst = 0;
    *txt = cur;
    return strCopy;
}

static int wchar_is_ws_or_zero(WCHAR c)
{
    switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case 0:
            return TRUE;
    }
    return FALSE;
}

static WCHAR *wstr_parse_non_quoted(WCHAR **txt)
{
    WCHAR * cur;
    WCHAR * strStart;
    WCHAR * strCopy;
    WCHAR   c;
    size_t  strLen;

    strStart = *txt;
    assert(strStart);
    if (!strStart) return NULL;
    assert('"' != *strStart);
    cur = strStart;
    for (;;) {
        c = *cur;
        if (wchar_is_ws_or_zero(c))
            break;
        ++cur;
    }

    strLen = cur - strStart;
    assert(strLen > 0);
    strCopy = wstr_dupn(strStart, strLen);
    *txt = cur;
    return strCopy;
}

/* replace a string pointed by <dst> with a copy of <src>
   (i.e. free existing <dst>).
   Returns FALSE if failed to replace (due to out of memory) */
BOOL wstr_dup_replace(WCHAR **dst, const WCHAR *src)
{
    WCHAR *dup = (WCHAR *)wstr_dup(src);
    if (!dup)
        return FALSE;
    free(*dst);
    *dst = dup;
    return TRUE;
}

/* replace in <str> the chars from <oldChars> with their equivalents
   from <newChars> (similar to UNIX's tr command)
   Returns the number of replaced characters. */
int wstr_trans_chars(WCHAR *str, const WCHAR *oldChars, const WCHAR *newChars)
{
    int findCount = 0;
    WCHAR *c = str;
    while (*c) {
        const WCHAR *found = wstr_find_char(oldChars, *c);
        if (found) {
            *c = newChars[found - oldChars];
            findCount++;
        }
        c++;
    }

    return findCount;
}


/* 'txt' is path that can be:
  - escaped, in which case it starts with '"', ends with '"' and each '"' that is part of the name is escaped
    with '\'
  - unescaped, in which case it start with != '"' and ends with ' ' or eol (0)
  This function extracts escaped or unescaped path from 'txt'. Returns NULL in case of error.
  Caller needs to free() the result. */
WCHAR *wstr_parse_possibly_quoted(WCHAR **txt)
{
    WCHAR *  cur;
    WCHAR *  str_copy;

    if (!txt)
        return NULL;
    cur = *txt;
    if (!cur)
        return NULL;

    wstr_skip_ws(&cur);
    if (0 == *cur)
        return NULL;
    if (L'"' == *cur)
        str_copy = wstr_parse_quoted(&cur);
    else
        str_copy = wstr_parse_non_quoted(&cur);
    *txt = cur;
    return str_copy;
}

static int hex_wchar_to_num(WCHAR c)
{
    if ((c >= '0') && (c <= '9'))
        return c - '0';
    if ((c >= 'a') && (c <= 'f'))
        return c - 'a' + 10;
    if ((c >= 'A') && (c <= 'F'))
        return c - 'A' + 10;
    return -1;
}

int hex_wstr_decode_byte(const WCHAR **txt)
{
    const WCHAR *s;
    int c1, c2;
    if (!txt) 
        return -1;
    s = *txt;
    c1 = hex_wchar_to_num(*s++);
    if (-1 == c1)
        return -1;
    c2 = hex_wchar_to_num(*s++);
    if (-1 == c2)
        return -1;
    *txt = s;
    return (16 * c1) + c2;
}

