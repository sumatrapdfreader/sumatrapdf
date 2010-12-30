/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */

/* The most basic things, including string handling functions */
#include "base_util.h"
#include "tstr_util.h"

/* replace in <str> the chars from <oldChars> with their equivalents
   from <newChars> (similar to UNIX's tr command)
   Returns the number of replaced characters. */
int tstr_trans_chars(tchar_t *str, const tchar_t *oldChars, const tchar_t *newChars)
{
    int findCount = 0;
    tchar_t *c = str;
    while (*c) {
        const tchar_t *found = tstr_find_char(oldChars, *c);
        if (found) {
            *c = newChars[found - oldChars];
            findCount++;
        }
        c++;
    }

    return findCount;
}

#define CHAR_URL_DONT_ENCODE   _T("-_.!~*'()")

static int tchar_needs_url_encode(tchar_t c)
{
    if (_istalnum(c))
        return FALSE;
    if (tstr_contains(CHAR_URL_DONT_ENCODE, c))
        return FALSE;
    return TRUE;
}

/* url-encode 'str'. Returns NULL in case of error.
   Caller needs to free() the result */
tchar_t *tstr_url_encode(const tchar_t *str)
{
    tchar_t *       result;
    tchar_t *       encoded;
    int             res_len = 0;
    const tchar_t * tmp;

    /* calc the size of the string after url encoding */
    for (tmp = str; *tmp; tmp++) {
        if (tchar_needs_url_encode(*tmp))
            res_len += 3;
        else
            ++res_len;
    }

    result = malloc((res_len + 1) * sizeof(tchar_t));
    if (!result)
        return NULL;

    encoded = result;
    for (tmp = str; *tmp; tmp++) {
        if (tchar_needs_url_encode(*tmp)) {
            _stprintf(encoded, _T("%%%2x"), *tmp);
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
int tstr_skip(const tchar_t **strp, const tchar_t *expect)
{
    if (tstr_startswith(*strp, expect)) {
        size_t len = tstr_len(expect);
        *strp += len;
        return TRUE;
    }

    return FALSE;
}

/* Copy the string from <*strp> into <dst> until <stop> is found, and point
    <*strp> at the end. Returns TRUE unless <dst_size> isn't big enough, in
    which case <*strp> is still updated, but FALSE is returned and <dst> is
    truncated. If <delim> is not found, <*strp> will point to the end of the
    string and FALSE is returned. */
int tstr_copy_skip_until(const tchar_t **strp, tchar_t *dst, size_t dst_size, tchar_t stop)
{
    const tchar_t *start = *strp;
    tchar_t *end = tstr_find_char(start, stop);

    if (!end) {
        size_t len = tstr_len(*strp);
        *strp += len;
        return FALSE;
    }

    *strp = end;
    return tstr_copyn(dst, dst_size, start, end - start);
}

/* Given a pointer to a string in '*txt', skip past whitespace in the string
   and put the result in '*txt' */
static void tstr_skip_ws(tchar_t **txt)
{
    assert(txt && *txt);
    while (tchar_is_ws(**txt))
        (*txt)++;
}

/* returns the next character in '*txt' that isn't a backslash */
static tchar_t tstr_skip_backslashs(tchar_t *txt)
{
    assert(txt && '\\' == *txt);
    while ('\\' == *++txt);
    return *txt;
}

static tchar_t *tstr_parse_quoted(tchar_t **txt)
{
    tchar_t * strStart;
    tchar_t * token;
    tchar_t * cur;
    tchar_t * dst;
    size_t    len;

    assert(txt && *txt && '"' == **txt);
    strStart = *txt + 1;

    for (cur = strStart; *cur && *cur != '"'; cur++) {
        // skip escaped quotation marks according to
        // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
        if ('\\' == *cur && '"' == tstr_skip_backslashs(cur))
            cur++;
    }
    len = cur - strStart;
    token = malloc((len + 1) * sizeof(tchar_t));
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

static tchar_t *tstr_parse_non_quoted(tchar_t **txt)
{
    tchar_t * cur;
    tchar_t * token;
    size_t    strLen;

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
tchar_t *tstr_parse_possibly_quoted(tchar_t **txt)
{
    tchar_t * cur;
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
