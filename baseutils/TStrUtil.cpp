/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

/* The most basic things, including string handling functions */
#include "BaseUtil.h"
#include "TStrUtil.h"

/* replace in <str> the chars from <oldChars> with their equivalents
   from <newChars> (similar to UNIX's tr command)
   Returns the number of replaced characters. */
int tstr_trans_chars(TCHAR *str, const TCHAR *oldChars, const TCHAR *newChars)
{
    int findCount = 0;
    TCHAR *c = str;
    while (*c) {
        const TCHAR *found = tstr_find_char(oldChars, *c);
        if (found) {
            *c = newChars[found - oldChars];
            findCount++;
        }
        c++;
    }

    return findCount;
}

#define CHAR_URL_DONT_ENCODE _T(" -_.!~*'()")

static int tchar_needs_url_encode(TCHAR c)
{
    if (_istalnum(c) && _istascii(c))
        return FALSE;
    if (tstr_find_char(CHAR_URL_DONT_ENCODE, c))
        return FALSE;
    return TRUE;
}

/* url-encode 'str'. Returns NULL in case of error.
   Caller needs to free() the result */
TCHAR *tstr_url_encode(const TCHAR *str)
{
    TCHAR *         result;
    TCHAR *         encoded;
    int             res_len = 0;
    const TCHAR *   tmp;

    if (StrLen(str) >= INT_MAX / 3)
        return NULL;

    /* calc the size of the string after url encoding */
    for (tmp = str; *tmp; tmp++) {
        if (tchar_needs_url_encode(*tmp))
            res_len += 3;
        else
            ++res_len;
    }

    result = SAZA(TCHAR, res_len + 1);
    if (!result)
        return NULL;

    encoded = result;
    for (tmp = str; *tmp; tmp++) {
        if (tchar_needs_url_encode(*tmp)) {
            _stprintf(encoded, _T("%%%02x"), *tmp);
            encoded += 3;
        } else if (_T(' ') == *tmp) {
            *encoded++ = _T('+');
        } else {
            *encoded++ = *tmp;
        }
    }
    *encoded = _T('\0');

    return result;
}

/* If the string at <*strp> starts with string at <expect>, skip <*strp> past
   it and return TRUE; otherwise return FALSE. */
int tstr_skip(const TCHAR **strp, const TCHAR *expect)
{
    if (tstr_startswith(*strp, expect)) {
        size_t len = StrLen(expect);
        *strp += len;
        return TRUE;
    }

    return FALSE;
}

/* Copy the string from <*strp> into <dst> until <stop> is found, and point
    <*strp> at the end (after <stop>). Returns TRUE unless <dst_size> isn't
    big enough, in which case <*strp> is still updated, but FALSE is returned
    and <dst> is truncated. If <delim> is not found, <*strp> will point to
    the end of the string and FALSE is returned. */
int tstr_copy_skip_until(const TCHAR **strp, TCHAR *dst, size_t dst_size, TCHAR stop)
{
    const TCHAR *start = *strp;
    const TCHAR *end = tstr_find_char(start, stop);

    if (!end) {
        size_t len = StrLen(*strp);
        *strp += len;
        return FALSE;
    }

    *strp = end + 1;
    return tstr_copyn(dst, dst_size, start, end - start);
}

/* Given a pointer to a string in '*txt', skip past whitespace in the string
   and put the result in '*txt' */
static void tstr_skip_ws(TCHAR **txt)
{
    assert(txt && *txt);
    while (tchar_is_ws(**txt))
        (*txt)++;
}

/* returns the next character in '*txt' that isn't a backslash */
static TCHAR tstr_skip_backslashs(TCHAR *txt)
{
    assert(txt && '\\' == *txt);
    while ('\\' == *++txt);
    return *txt;
}

static TCHAR *tstr_parse_quoted(TCHAR **txt)
{
    TCHAR * strStart;
    TCHAR * token;
    TCHAR * cur;
    TCHAR * dst;
    size_t  len;

    assert(txt && *txt && '"' == **txt);
    strStart = *txt + 1;

    for (cur = strStart; *cur && *cur != '"'; cur++) {
        // skip escaped quotation marks according to
        // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
        if ('\\' == *cur && '"' == tstr_skip_backslashs(cur))
            cur++;
    }
    len = cur - strStart;
    token = SAZA(TCHAR, len + 1);
    if (!token)
        return NULL;

    dst = token;
    for (cur = strStart; *cur && *cur != '"'; cur++) {
        if ('\\' == *cur && '"' == tstr_skip_backslashs(cur))
            cur++;
        *dst++ = *cur;
    }
    *dst = _T('\0');

    if ('"' == *cur)
        cur++;
    *txt = cur;
    return token;
}

static TCHAR *tstr_parse_non_quoted(TCHAR **txt)
{
    TCHAR * cur;
    TCHAR * token;
    size_t  strLen;

    assert(txt && *txt && **txt && '"' != **txt && !tchar_is_ws(**txt));

    // contrary to http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
    // we don't treat quotation marks or backslashes in non-quoted
    // arguments in any special way
    for (cur = *txt; *cur && !tchar_is_ws(*cur); cur++);
    strLen = cur - *txt;
    token = tstr_dupn(*txt, strLen);

    *txt = cur;
    return token;
}

/* 'txt' is path that can be:
  - escaped, in which case it starts with '"', ends with '"' and
    each '"' that is part of the name is escaped with '\'
  - unescaped, in which case it start with != '"' and ends with ' ' or eol (0)
  This function extracts escaped or unescaped path from 'txt'. Returns NULL in case of error.
  Caller needs to free() the result. */
TCHAR *tstr_parse_possibly_quoted(TCHAR **txt)
{
    TCHAR * cur;
    if (!txt || !*txt)
        return NULL;

    cur = *txt;
    tstr_skip_ws(&cur);
    if (0 == *cur)
        return NULL;
    *txt = cur;

    if ('"' == **txt)
        return tstr_parse_quoted(txt);
    return tstr_parse_non_quoted(txt);
}
