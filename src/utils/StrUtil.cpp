/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

/* The most basic things, including string handling functions */
#include "BaseUtil.h"
#include "StrUtil.h"

namespace str {

#define EntryCheck(arg1, arg2) \
    if (arg1 == arg2) \
        return true; \
    if (!arg1 || !arg2) \
        return false

// return true if s1 == s2, case sensitive
bool Eq(const char *s1, const char *s2)
{
    EntryCheck(s1, s2);
    return 0 == strcmp(s1, s2);
}

// return true if s1 == s2, case sensitive
bool Eq(const WCHAR *s1, const WCHAR *s2)
{
    EntryCheck(s1, s2);
    return 0 == wcscmp(s1, s2);
}

// return true if s1 == s2, case insensitive
bool EqI(const char *s1, const char *s2)
{
    EntryCheck(s1, s2);
    return 0 == _stricmp(s1, s2);
}

// return true if s1 == s2, case insensitive
bool EqI(const WCHAR *s1, const WCHAR *s2)
{
    EntryCheck(s1, s2);
    return 0 == _wcsicmp(s1, s2);
}

// compares two strings ignoring case and whitespace
bool EqIS(const TCHAR *s1, const TCHAR *s2)
{
    while (*s1 && *s2) {
        // skip whitespace
        for (; _istspace(*s1); s1++);
        for (; _istspace(*s2); s2++);

        if (_totlower(*s1) != _totlower(*s2))
            return false;
        if (*s1) { s1++; s2++; }
    }

    return !*s1 && !*s2;
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
    return 0 == _strnicmp(str, txt, str::Len(txt));
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(const WCHAR *str, const WCHAR *txt)
{
    EntryCheck(str, txt);
    return 0 == _wcsnicmp(str, txt, str::Len(txt));
}

#undef EntryCheck

// TODO: implement with templates? (must happen in the header, though)
// template <typename T> bool EndsWith(const T*, const T*) ?

bool EndsWith(const char *txt, const char *end)
{
    if (!txt || !end)
        return false;
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen)
        return false;
    return str::Eq(txt + txtLen - endLen, end);
}

bool EndsWith(const WCHAR *txt, const WCHAR *end)
{
    if (!txt || !end)
        return false;
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen)
        return false;
    return str::Eq(txt + txtLen - endLen, end);
}

bool EndsWithI(const char *txt, const char *end)
{
    if (!txt || !end)
        return false;
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen)
        return false;
    return str::EqI(txt + txtLen - endLen, end);
}

bool EndsWithI(const WCHAR *txt, const WCHAR *end)
{
    if (!txt || !end)
        return false;
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen)
        return false;
    return str::EqI(txt + txtLen - endLen, end);
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

void ToLower(char *s)
{
    if (!s) return;
    for (; *s; s++)
        *s = tolower(*s);
}

void ToLower(WCHAR *s)
{
    if (!s) return;
    for (; *s; s++)
        *s = towlower(*s);
}

/* Caller needs to free() the result */
char *ToMultiByte(const WCHAR *txt, UINT codePage)
{
    assert(txt);
    if (!txt) return NULL;

    int requiredBufSize = WideCharToMultiByte(codePage, 0, txt, -1, NULL, 0, NULL, NULL);
    char *res = SAZA(char, requiredBufSize);
    if (!res)
        return NULL;
    WideCharToMultiByte(codePage, 0, txt, -1, res, requiredBufSize, NULL, NULL);
    return res;
}

/* Caller needs to free() the result */
char *ToMultiByte(const char *src, UINT codePageSrc, UINT codePageDest)
{
    assert(src);
    if (!src) return NULL;

    if (codePageSrc == codePageDest)
        return str::Dup(src);

    ScopedMem<WCHAR> tmp(ToWideChar(src, codePageSrc));
    if (!tmp)
        return NULL;

    return ToMultiByte(tmp.Get(), codePageDest);
}

/* Caller needs to free() the result */
WCHAR *ToWideChar(const char *src, UINT codePage)
{
    assert(src);
    if (!src) return NULL;

    int requiredBufSize = MultiByteToWideChar(codePage, 0, src, -1, NULL, 0);
    WCHAR *res = SAZA(WCHAR, requiredBufSize);
    if (!res)
        return NULL;
    MultiByteToWideChar(codePage, 0, src, -1, res, requiredBufSize);
    return res;
}

const char *FindI(const char *s, const char *find)
{
    while (*s) {
        if (StartsWithI(s, find))
            return s;
        s++;
    }
    return NULL;
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
        buf = str::Dup(message);

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
        buf = str::Dup(message);

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

// Trim whitespace characters, in-place, inside s.
// Returns number of trimmed characters.
size_t TrimWS(TCHAR *s, TrimOpt opt)
{
    size_t sLen = str::Len(s);
    TCHAR *ns = s;
    TCHAR *e = s + sLen;
    TCHAR *ne = e;
    if ((TrimLeft == opt) || (TrimBoth == opt)) {
        while (_istspace(*ns)) {
            ++ns;
        }
    }

    if ((TrimRight == opt) || (TrimBoth == opt)) {
        while (((ne - 1) >= ns) && _istspace(ne[-1])) {
            --ne;
        }
    }
    *ne = 0;
    size_t trimmed = (ns - s) + (e - ne);
    if (ns != s) {
        size_t toCopy = (sLen - trimmed + 1) * sizeof(TCHAR); // +1 for terminating 0
        memmove(s, ns, toCopy);
    }
    return trimmed;
}

/* replace in <str> the chars from <oldChars> with their equivalents
   from <newChars> (similar to UNIX's tr command)
   Returns the number of replaced characters. */
size_t TransChars(char *str, const char *oldChars, const char *newChars)
{
    size_t findCount = 0;

    for (char *c = str; *c; c++) {
        const char *found = str::FindChar(oldChars, *c);
        if (found) {
            *c = newChars[found - oldChars];
            findCount++;
        }
    }

    return findCount;
}

size_t TransChars(WCHAR *str, const WCHAR *oldChars, const WCHAR *newChars)
{
    size_t findCount = 0;

    for (WCHAR *c = str; *c; c++) {
        const WCHAR *found = str::FindChar(oldChars, *c);
        if (found) {
            *c = newChars[found - oldChars];
            findCount++;
        }
    }

    return findCount;
}

// replaces all whitespace characters with spaces, collapses several
// consecutive spaces into one and strips heading/trailing ones
// returns the number of removed characters
size_t NormalizeWS(TCHAR *str)
{
    TCHAR *src = str, *dst = str;
    bool addedSpace = true;

    for (; *src; src++) {
        if (!_istspace(*src)) {
            *dst++ = *src;
            addedSpace = false;
        }
        else if (!addedSpace) {
            *dst++ = ' ';
            addedSpace = true;
        }
    }

    if (dst > str && _istspace(*(dst - 1)))
        dst--;
    *dst = '\0';

    return src - dst;
}

// Remove all characters in "toRemove" from "str", in place.
// Returns number of removed characters.
size_t RemoveChars(char *str, const char *toRemove)
{
    size_t removed = 0;
    char *dst = str;
    while (*str) {
        char c = *str++;
        if (!str::FindChar(toRemove, c))
            *dst++ = c;
        else
            ++removed;
    }
    *dst = '\0';
    return removed;
}

size_t RemoveChars(WCHAR *str, const WCHAR *toRemove)
{
    size_t removed = 0;
    WCHAR *dst = str;
    while (*str) {
        WCHAR c = *str++;
        if (!str::FindChar(toRemove, c))
            *dst++ = c;
        else
            ++removed;
    }
    *dst = '\0';
    return removed;
}


// Note: BufSet() should only be used when absolutely necessary (e.g. when
// handling buffers in OS-defined structures)
// returns the number of characters written (without the terminating \0)
size_t BufSet(char *dst, size_t dstCchSize, const char *src)
{
    if (0 == dstCchSize) return 0;

    size_t srcCchSize = str::Len(src);
    size_t size = min(dstCchSize - 1, srcCchSize);

    strncpy(dst, src, size + 1);
    dst[size] = '\0';

    return size;
}

size_t BufSet(WCHAR *dst, size_t dstCchSize, const WCHAR *src)
{
    if (0 == dstCchSize) return 0;

    size_t srcCchSize = str::Len(src);
    size_t size = min(dstCchSize - 1, srcCchSize);

    wcsncpy(dst, src, size + 1);
    dst[size] = '\0';

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

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
TCHAR *FormatNumWithThousandSep(size_t num, const TCHAR *sep)
{
    TCHAR thousandSep[4];
    if (!sep) {
        if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, thousandSep, dimof(thousandSep)))
            sep = thousandSep;
        else
            sep = _T("");
    }
    ScopedMem<TCHAR> buf(str::Format(_T("%Iu"), num));

    size_t resLen = str::Len(buf) + str::Len(sep) * (str::Len(buf) + 3) / 3 + 1;
    TCHAR *res = SAZA(TCHAR, resLen);
    TCHAR *next = res;
    int i = 3 - (str::Len(buf) % 3);
    for (TCHAR *src = buf.Get(); *src;) {
        *next++ = *src++;
        if (*src && i == 2)
            next += str::BufSet(next, resLen - (next - res), sep);
        i = (i + 1) % 3;
    }

    return res;
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
TCHAR *FormatFloatWithThousandSep(double number)
{
    size_t num = (size_t)(number * 100 + 0.5);

    ScopedMem<TCHAR> tmp(FormatNumWithThousandSep(num / 100));
    TCHAR decimal[4];
    if (!GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decimal, dimof(decimal)))
        str::BufSet(decimal, dimof(decimal), _T("."));

    // always add between one and two decimals after the point
    ScopedMem<TCHAR> buf(str::Format(_T("%s%s%02d"), tmp, decimal, num % 100));
    if (str::EndsWith(buf, _T("0")))
        buf[str::Len(buf) - 1] = '\0';

    return buf.StealData();
}

// cf. http://rosettacode.org/wiki/Roman_numerals/Encode#C.2B.2B
TCHAR *FormatRomanNumeral(int number)
{
    if (number < 1)
        return NULL;

    static struct {
        int value;
        const TCHAR *numeral;
    } romandata[] = {
        { 1000, _T("M") }, { 900, _T("CM") }, { 500, _T("D") }, { 400, _T("CD") },
        {  100, _T("C") }, {  90, _T("XC") }, {  50, _T("L") }, {  40, _T("XL") },
        {   10, _T("X") }, {   9, _T("IX") }, {   5, _T("V") }, {   4, _T("IV") }, { 1, _T("I") }
    };

    size_t len = 0;
    for (int num = number, i = 0; i < dimof(romandata); i++)
        for (; num >= romandata[i].value; num -= romandata[i].value)
            len += romandata[i].numeral[1] ? 2 : 1;
    assert(len > 0);

    TCHAR *roman = SAZA(TCHAR, len + 1), *c = roman;
    for (int num = number, i = 0; i < dimof(romandata); i++)
        for (; num >= romandata[i].value; num -= romandata[i].value)
            c += str::BufSet(c, romandata[i].numeral[1] ? 3 : 2, romandata[i].numeral);

    return roman;
}

/* compares two strings "naturally" by sorting numbers within a string
   numerically instead of by pure ASCII order; we imitate Windows Explorer
   by sorting special characters before alphanumeric characters
   (e.g. ".hg" < "2.pdf" < "100.pdf" < "zzz")
   TODO: use StrCmpLogicalW instead once we no longer support Windows 2000 */
int CmpNatural(const TCHAR *a, const TCHAR *b)
{
    const TCHAR *aStart = a, *bStart = b;
    int diff = 0;

    for (; 0 == diff; a++, b++) {
        // ignore leading and trailing spaces, and differences in whitespace only
        if (a == aStart || !*a || !*b || _istspace(*a) && _istspace(*b)) {
            for (; _istspace(*a); a++);
            for (; _istspace(*b); b++);
        }
        // if two strings are identical when ignoring case, leading zeroes and
        // whitespace, compare them traditionally for a stable sort order
        if (!*a && !*b)
            return _tcscmp(aStart, bStart);
        if (ChrIsDigit(*a) && ChrIsDigit(*b)) {
            // ignore leading zeroes
            for (; '0' == *a; a++);
            for (; '0' == *b; b++);
            // compare the two numbers as (positive) integers
            for (diff = 0; ChrIsDigit(*a) || ChrIsDigit(*b); a++, b++) {
                // if either *a or *b isn't a number, they differ in magnitude
                if (!ChrIsDigit(*a))
                    return -1;
                if (!ChrIsDigit(*b))
                    return 1;
                // remember the difference for when the numbers are of the same magnitude
                if (0 == diff)
                    diff = *a - *b;
            }
            // neither *a nor *b is a digit, so continue with them (unless diff != 0)
            a--;
            b--;
        }
        // sort letters case-insensitively
        else if (_istalnum(*a) && _istalnum(*b))
            diff = _totlower(*a) - _totlower(*b);
        // sort special characters before text and numbers
        else if (_istalnum(*a))
            return 1;
        else if (_istalnum(*b))
            return -1;
        // sort special characters by ASCII code
        else
            diff = *a - *b;
    }

    return diff;
}

void DbgOut(const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);
    ScopedMem<TCHAR> buf(FmtV(format, args));
    OutputDebugString(buf);
    va_end(args);
}

static TCHAR *ExtractUntil(const TCHAR *pos, TCHAR c, const TCHAR **endOut)
{
    *endOut = FindChar(pos, c);
    if (!*endOut)
        return NULL;
    return str::DupN(pos, *endOut - pos);
}

static const TCHAR *ParseLimitedNumber(const TCHAR *str, const TCHAR *format,
                                       TCHAR **endOut, void *valueOut)
{
    UINT width;
    TCHAR f2[] = _T("% ");
    const TCHAR *endF = Parse(format, _T("%u%c"), &width, &f2[1]);
    if (endF && FindChar(_T("udx"), f2[1]) && width <= Len(str)) {
        ScopedMem<TCHAR> limited(DupN(str, width));
        const TCHAR *end = Parse(limited, f2, valueOut);
        if (end && !*end)
            *endOut = (TCHAR *)str + width;
    }
    return endF;
}

/* Parses a string into several variables sscanf-style (i.e. pass in pointers
   to where the parsed values are to be stored). Returns a pointer to the first
   character that's not been parsed when successful and NULL otherwise.

   Supported formats:
     %u - parses an unsigned int
     %d - parses a signed int
     %x - parses an unsigned hex-int
     %f - parses a float
     %c - parses a single TCHAR
     %s - parses a string (pass in a TCHAR**, free after use - also on failure!)
     %S - parses a string into a ScopedMem<TCHAR>
     %? - makes the next single character optional (e.g. "x%?,y" parses both "xy" and "x,y")
     %$ - causes the parsing to fail if it's encountered when not at the end of the string
     %% - parses a single '%'

   %u, %d and %x accept an optional width argument, indicating exactly how many
   characters must be read for parsing the number (e.g. "%4d" parses -123 out of "-12345"
   and doesn't parse "123" at all).
   */
const TCHAR *Parse(const TCHAR *str, const TCHAR *format, ...)
{
    va_list args;
    va_start(args, format);
    for (const TCHAR *f = format; *f; f++) {
        if (*f != '%') {
            if (*f != *str)
                goto Failure;
            str++;
            continue;
        }
        f++;

        TCHAR *end = NULL;
        if ('u' == *f)
            *va_arg(args, unsigned int *) = _tcstoul(str, &end, 10);
        else if ('d' == *f)
            *va_arg(args, int *) = _tcstol(str, &end, 10);
        else if ('x' == *f)
            *va_arg(args, unsigned int *) = _tcstoul(str, &end, 16);
        else if ('f' == *f)
            *va_arg(args, float *) = (float)_tcstod(str, &end);
        else if ('c' == *f)
            *va_arg(args, TCHAR *) = *str, end = (TCHAR *)str + 1;
        else if ('s' == *f)
            *va_arg(args, TCHAR **) = ExtractUntil(str, *(f + 1), (const TCHAR **)&end);
        else if ('S' == *f)
            va_arg(args, ScopedMem<TCHAR> *)->Set(ExtractUntil(str, *(f + 1), (const TCHAR **)&end));
        else if ('$' == *f && !*str)
            continue; // don't fail, if we're indeed at the end of the string
        else if ('%' == *f && *f == *str)
            end = (TCHAR *)str + 1;
        else if ('?' == *f && *(f + 1)) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            if (*str != *++f)
                continue;
            end = (TCHAR *)str + 1;
        }
        else if (ChrIsDigit(*f))
            f = ParseLimitedNumber(str, f, &end, va_arg(args, void *)) - 1;
        if (!end || end == str)
            goto Failure;
        str = end;
    }
    va_end(args);
    return str;

Failure:
    va_end(args);
    return NULL;
}

}
