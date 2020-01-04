/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

namespace str {

bool IsWs(WCHAR c) {
    return iswspace(c);
}

bool IsDigit(WCHAR c) {
    return ('0' <= c) && (c <= '9');
}

bool IsNonCharacter(WCHAR c) {
    return c >= 0xFFFE || (c & ~1) == 0xDFFE || (0xFDD0 <= c && c <= 0xFDEF);
}

size_t Len(const WCHAR* s) {
    return s ? wcslen(s) : 0;
}

void Free(const WCHAR* s) {
    free((void*)s);
}

WCHAR* Dup(const WCHAR* s) {
    return s ? _wcsdup(s) : nullptr;
}

// return true if s1 == s2, case sensitive
bool Eq(const WCHAR* s1, const WCHAR* s2) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    return 0 == wcscmp(s1, s2);
}

// return true if s1 == s2, case insensitive
bool EqI(const WCHAR* s1, const WCHAR* s2) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    return 0 == _wcsicmp(s1, s2);
}

// compares two strings ignoring case and whitespace
bool EqIS(const WCHAR* s1, const WCHAR* s2) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;

    while (*s1 && *s2) {
        // skip whitespace
        for (; IsWs(*s1); s1++) {
            // do nothing
        }
        for (; IsWs(*s2); s2++) {
            // do nothing
        }

        if (towlower(*s1) != towlower(*s2))
            return false;
        if (*s1) {
            s1++;
            s2++;
        }
    }

    return !*s1 && !*s2;
}

bool EqN(const WCHAR* s1, const WCHAR* s2, size_t len) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    return 0 == wcsncmp(s1, s2, len);
}

bool EqNI(const WCHAR* s1, const WCHAR* s2, size_t len) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    return 0 == _wcsnicmp(s1, s2, len);
}

bool IsEmpty(const WCHAR* s) {
    return !s || (0 == *s);
}

bool StartsWith(const WCHAR* str, const WCHAR* txt) {
    return EqN(str, txt, Len(txt));
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(const WCHAR* str, const WCHAR* txt) {
    if (str == txt)
        return true;
    if (!str || !txt)
        return false;
    return 0 == _wcsnicmp(str, txt, str::Len(txt));
}

bool EndsWith(const WCHAR* txt, const WCHAR* end) {
    if (!txt || !end)
        return false;
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen)
        return false;
    return str::Eq(txt + txtLen - endLen, end);
}

bool EndsWithI(const WCHAR* txt, const WCHAR* end) {
    if (!txt || !end)
        return false;
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen)
        return false;
    return str::EqI(txt + txtLen - endLen, end);
}

const WCHAR* FindChar(const WCHAR* str, const WCHAR c) {
    return wcschr(str, c);
}

WCHAR* FindChar(WCHAR* str, const WCHAR c) {
    return wcschr(str, c);
}

const WCHAR* FindCharLast(const WCHAR* str, const WCHAR c) {
    return wcsrchr(str, c);
}

WCHAR* FindCharLast(WCHAR* str, const WCHAR c) {
    return wcsrchr(str, c);
}

const WCHAR* Find(const WCHAR* str, const WCHAR* find) {
    return wcsstr(str, find);
}

const WCHAR* FindI(const WCHAR* s, const WCHAR* toFind) {
    if (!s || !toFind)
        return nullptr;

    WCHAR first = towlower(*toFind);
    if (!first)
        return s;
    while (*s) {
        WCHAR c = towlower(*s);
        if (c == first) {
            if (str::StartsWithI(s, toFind)) {
                return s;
            }
        }
        s++;
    }
    return nullptr;
}

void ReplacePtr(WCHAR** s, const WCHAR* snew) {
    free(*s);
    *s = str::Dup(snew);
}

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
WCHAR* Join(const WCHAR* s1, const WCHAR* s2, const WCHAR* s3) {
    // don't use str::Format(L"%s%s%s", s1, s2, s3) since the strings
    // might contain non-characters which str::Format fails to handle
    size_t s1Len = str::Len(s1), s2Len = str::Len(s2), s3Len = str::Len(s3);
    WCHAR* res = AllocArray<WCHAR>(s1Len + s2Len + s3Len + 1);
    memcpy(res, s1, s1Len * sizeof(WCHAR));
    memcpy(res + s1Len, s2, s2Len * sizeof(WCHAR));
    memcpy(res + s1Len + s2Len, s3, s3Len * sizeof(WCHAR));
    res[s1Len + s2Len + s3Len] = '\0';
    return res;
}

WCHAR* DupN(const WCHAR* s, size_t lenCch) {
    if (!s)
        return nullptr;
    WCHAR* res = (WCHAR*)memdup((void*)s, (lenCch + 1) * sizeof(WCHAR));
    if (res)
        res[lenCch] = 0;
    return res;
}

WCHAR* ToLowerInPlace(WCHAR* s) {
    WCHAR* res = s;
    for (; s && *s; s++) {
        *s = towlower(*s);
    }
    return res;
}

WCHAR* ToLower(const WCHAR* s) {
    WCHAR* s2 = str::Dup(s);
    return ToLowerInPlace(s2);
}

bool BufFmtV(WCHAR* buf, size_t bufCchSize, const WCHAR* fmt, va_list args) {
    int count = _vsnwprintf_s(buf, bufCchSize, _TRUNCATE, fmt, args);
    buf[bufCchSize - 1] = 0;
    if ((count >= 0) && ((size_t)count < bufCchSize))
        return true;
    return false;
}

WCHAR* FmtV(const WCHAR* fmt, va_list args) {
    WCHAR message[256];
    size_t bufCchSize = dimof(message);
    WCHAR* buf = message;
    for (;;) {
        // TODO: _vsnwprintf_s fails for certain inputs (e.g. strings containing U+FFFF)
        //       but doesn't correctly set errno, either, so there's no way of telling
        //       the failures apart
        int count = _vsnwprintf_s(buf, bufCchSize, _TRUNCATE, fmt, args);
        if ((count >= 0) && ((size_t)count < bufCchSize))
            break;
        // always grow the buffer exponentially (cf. TODO above)
        if (buf != message)
            free(buf);
        bufCchSize = bufCchSize / 2 * 3;
        buf = AllocArray<WCHAR>(bufCchSize);
        if (!buf)
            break;
    }
    if (buf == message)
        buf = str::Dup(message);

    return buf;
}

WCHAR* Format(const WCHAR* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WCHAR* res = FmtV(fmt, args);
    va_end(args);
    return res;
}

// Trim whitespace characters, in-place, inside s.
// Returns number of trimmed characters.
size_t TrimWS(WCHAR* s, TrimOpt opt) {
    size_t sLen = str::Len(s);
    WCHAR* ns = s;
    WCHAR* e = s + sLen;
    WCHAR* ne = e;
    if ((TrimOpt::Left == opt) || (TrimOpt::Both == opt)) {
        while (IsWs(*ns)) {
            ++ns;
        }
    }

    if ((TrimOpt::Right == opt) || (TrimOpt::Both == opt)) {
        while (((ne - 1) >= ns) && IsWs(ne[-1])) {
            --ne;
        }
    }
    *ne = 0;
    size_t trimmed = (ns - s) + (e - ne);
    if (ns != s) {
        size_t toCopy = (sLen - trimmed + 1) * sizeof(WCHAR); // +1 for terminating 0
        memmove(s, ns, toCopy);
    }
    return trimmed;
}

size_t TransChars(WCHAR* str, const WCHAR* oldChars, const WCHAR* newChars) {
    size_t findCount = 0;

    for (WCHAR* c = str; *c; c++) {
        const WCHAR* found = str::FindChar(oldChars, *c);
        if (found) {
            *c = newChars[found - oldChars];
            findCount++;
        }
    }

    return findCount;
}

// free() the result
WCHAR* Replace(const WCHAR* s, const WCHAR* toReplace, const WCHAR* replaceWith) {
    if (!s || str::IsEmpty(toReplace) || !replaceWith)
        return nullptr;

    str::WStr result(str::Len(s));
    size_t findLen = str::Len(toReplace), replLen = str::Len(replaceWith);
    const WCHAR *start = s, *end;
    while ((end = str::Find(start, toReplace)) != nullptr) {
        result.Append(start, end - start);
        result.Append(replaceWith, replLen);
        start = end + findLen;
    }
    result.Append(start);
    return result.StealData();
}

// replaces all whitespace characters with spaces, collapses several
// consecutive spaces into one and strips heading/trailing ones
// returns the number of removed characters
size_t NormalizeWS(WCHAR* str) {
    WCHAR *src = str, *dst = str;
    bool addedSpace = true;

    for (; *src; src++) {
        if (!IsWs(*src)) {
            *dst++ = *src;
            addedSpace = false;
        } else if (!addedSpace) {
            *dst++ = ' ';
            addedSpace = true;
        }
    }

    if (dst > str && IsWs(*(dst - 1)))
        dst--;
    *dst = '\0';

    return src - dst;
}

size_t RemoveChars(WCHAR* str, const WCHAR* toRemove) {
    size_t removed = 0;
    WCHAR* dst = str;
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

size_t BufSet(WCHAR* dst, size_t dstCchSize, const WCHAR* src) {
    CrashAlwaysIf(0 == dstCchSize);

    size_t srcCchSize = str::Len(src);
    size_t toCopy = std::min(dstCchSize - 1, srcCchSize);

    memset(dst, 0, dstCchSize * sizeof(WCHAR));
    memcpy(dst, src, toCopy * sizeof(WCHAR));
    return toCopy;
}

size_t BufAppend(WCHAR* dst, size_t dstCchSize, const WCHAR* s) {
    CrashAlwaysIf(0 == dstCchSize);

    size_t currDstCchLen = str::Len(dst);
    if (currDstCchLen + 1 >= dstCchSize)
        return 0;
    size_t left = dstCchSize - currDstCchLen - 1;
    size_t srcCchSize = str::Len(s);
    size_t toCopy = std::min(left, srcCchSize);

    errno_t err = wcsncat_s(dst, dstCchSize, s, toCopy);
    CrashIf(err || dst[currDstCchLen + toCopy] != '\0');

    return toCopy;
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
WCHAR* FormatNumWithThousandSep(size_t num, LCID locale) {
    WCHAR thousandSep[4] = {0};
    if (!GetLocaleInfo(locale, LOCALE_STHOUSAND, thousandSep, dimof(thousandSep)))
        str::BufSet(thousandSep, dimof(thousandSep), L",");
    AutoFreeWstr buf(str::Format(L"%Iu", num));

    size_t resLen = str::Len(buf) + str::Len(thousandSep) * (str::Len(buf) + 3) / 3 + 1;
    WCHAR* res = AllocArray<WCHAR>(resLen);
    if (!res)
        return nullptr;
    WCHAR* next = res;
    int i = 3 - (str::Len(buf) % 3);
    for (const WCHAR* src = buf; *src;) {
        *next++ = *src++;
        if (*src && i == 2)
            next += str::BufSet(next, resLen - (next - res), thousandSep);
        i = (i + 1) % 3;
    }
    *next = '\0';

    return res;
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
WCHAR* FormatFloatWithThousandSep(double number, LCID locale) {
    size_t num = (size_t)(number * 100 + 0.5);

    AutoFreeWstr tmp(FormatNumWithThousandSep(num / 100, locale));
    WCHAR decimal[4];
    if (!GetLocaleInfo(locale, LOCALE_SDECIMAL, decimal, dimof(decimal)))
        str::BufSet(decimal, dimof(decimal), L".");

    // always add between one and two decimals after the point
    AutoFreeWstr buf(str::Format(L"%s%s%02d", tmp.get(), decimal, num % 100));
    if (str::EndsWith(buf, L"0"))
        buf[str::Len(buf) - 1] = '\0';

    return buf.StealData();
}

// cf. http://rosettacode.org/wiki/Roman_numerals/Encode#C.2B.2B
WCHAR* FormatRomanNumeral(int number) {
    if (number < 1)
        return nullptr;

    static struct {
        int value;
        const WCHAR* numeral;
    } romandata[] = {{1000, L"M"}, {900, L"CM"}, {500, L"D"}, {400, L"CD"}, {100, L"C"}, {90, L"XC"}, {50, L"L"},
                     {40, L"XL"},  {10, L"X"},   {9, L"IX"},  {5, L"V"},    {4, L"IV"},  {1, L"I"}};

    size_t len = 0;
    for (int n = number, i = 0; i < dimof(romandata); i++) {
        for (; n >= romandata[i].value; n -= romandata[i].value) {
            len += romandata[i].numeral[1] ? 2 : 1;
        }
    }
    AssertCrash(len > 0);

    WCHAR *roman = AllocArray<WCHAR>(len + 1), *c = roman;
    for (int n = number, i = 0; i < dimof(romandata); i++) {
        for (; n >= romandata[i].value; n -= romandata[i].value) {
            c += str::BufSet(c, romandata[i].numeral[1] ? 3 : 2, romandata[i].numeral);
        }
    }

    return roman;
}

/* compares two strings "naturally" by sorting numbers within a string
   numerically instead of by pure ASCII order; we imitate Windows Explorer
   by sorting special characters before alphanumeric characters
   (e.g. ".hg" < "2.pdf" < "100.pdf" < "zzz")
*/
int CmpNatural(const WCHAR* a, const WCHAR* b) {
    CrashAlwaysIf(!a || !b);
    const WCHAR *aStart = a, *bStart = b;
    int diff = 0;

    for (; 0 == diff; a++, b++) {
        // ignore leading and trailing spaces, and differences in whitespace only
        if (a == aStart || !*a || !*b || IsWs(*a) && IsWs(*b)) {
            for (; IsWs(*a); a++) {
                // do nothing
            }
            for (; IsWs(*b); b++) {
                // do nothing
            }
        }
        // if two strings are identical when ignoring case, leading zeroes and
        // whitespace, compare them traditionally for a stable sort order
        if (!*a && !*b)
            return wcscmp(aStart, bStart);
        if (str::IsDigit(*a) && str::IsDigit(*b)) {
            // ignore leading zeroes
            for (; '0' == *a; a++) {
                // do nothing
            }
            for (; '0' == *b; b++) {
                // do nothing
            }
            // compare the two numbers as (positive) integers
            for (diff = 0; str::IsDigit(*a) || str::IsDigit(*b); a++, b++) {
                // if either *a or *b isn't a number, they differ in magnitude
                if (!str::IsDigit(*a))
                    return -1;
                if (!str::IsDigit(*b))
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
        else if (iswalnum(*a) && iswalnum(*b))
            diff = towlower(*a) - towlower(*b);
        // sort special characters before text and numbers
        else if (iswalnum(*a))
            return 1;
        else if (iswalnum(*b))
            return -1;
        // sort special characters by ASCII code
        else
            diff = *a - *b;
    }

    return diff;
}

static const WCHAR* ParseLimitedNumber(const WCHAR* str, const WCHAR* format, const WCHAR** endOut, void* valueOut) {
    unsigned int width;
    WCHAR f2[] = L"% ";
    const WCHAR* endF = Parse(format, L"%u%c", &width, &f2[1]);
    if (endF && FindChar(L"udx", f2[1]) && width <= Len(str)) {
        WCHAR limited[16]; // 32-bit integers are at most 11 characters long
        str::BufSet(limited, std::min((size_t)width + 1, dimof(limited)), str);
        const WCHAR* end = Parse(limited, f2, valueOut);
        if (end && !*end)
            *endOut = str + width;
    }
    return endF;
}

static WCHAR* ExtractUntil(const WCHAR* pos, WCHAR c, const WCHAR** endOut) {
    *endOut = FindChar(pos, c);
    if (!*endOut)
        return nullptr;
    return str::DupN(pos, *endOut - pos);
}

const WCHAR* Parse(const WCHAR* str, const WCHAR* format, ...) {
    if (!str)
        return nullptr;
    va_list args;
    va_start(args, format);
    for (const WCHAR* f = format; *f; f++) {
        if (*f != '%') {
            if (*f != *str)
                goto Failure;
            str++;
            continue;
        }
        f++;

        const WCHAR* end = nullptr;
        if ('u' == *f)
            *va_arg(args, unsigned int*) = wcstoul(str, (WCHAR**)&end, 10);
        else if ('d' == *f)
            *va_arg(args, int*) = wcstol(str, (WCHAR**)&end, 10);
        else if ('x' == *f)
            *va_arg(args, unsigned int*) = wcstoul(str, (WCHAR**)&end, 16);
        else if ('f' == *f)
            *va_arg(args, float*) = (float)wcstod(str, (WCHAR**)&end);
        else if ('c' == *f)
            *va_arg(args, WCHAR*) = *str, end = str + 1;
        else if ('s' == *f)
            *va_arg(args, WCHAR**) = ExtractUntil(str, *(f + 1), &end);
        else if ('S' == *f)
            va_arg(args, AutoFreeWstr*)->Set(ExtractUntil(str, *(f + 1), &end));
        else if ('$' == *f && !*str)
            continue; // don't fail, if we're indeed at the end of the string
        else if ('%' == *f && *f == *str)
            end = str + 1;
        else if (' ' == *f && str::IsWs(*str))
            end = str + 1;
        else if ('_' == *f) {
            if (!str::IsWs(*str))
                continue; // don't fail, if there's no whitespace at all
            for (end = str + 1; str::IsWs(*end); end++) {
                // do nothing
            }
        } else if ('?' == *f && *(f + 1)) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            if (*str != *++f)
                continue;
            end = str + 1;
        } else if (str::IsDigit(*f))
            f = ParseLimitedNumber(str, f, &end, va_arg(args, void*)) - 1;
        if (!end || end == str)
            goto Failure;
        str = end;
    }
    va_end(args);
    return str;

Failure:
    va_end(args);
    return nullptr;
}

} // namespace str

namespace url {

bool IsAbsolute(const WCHAR* url) {
    const WCHAR* colon = str::FindChar(url, ':');
    const WCHAR* hash = str::FindChar(url, '#');
    return colon && (!hash || hash > colon);
}

void DecodeInPlace(WCHAR* url) {
    if (!str::FindChar(url, '%')) {
        return;
    }
    // URLs are usually UTF-8 encoded
    AutoFree urlUtf8(strconv::WstrToUtf8(url));
    DecodeInPlace(urlUtf8.Get());
    // convert back in place
    CrashIf(str::Len(url) >= INT_MAX);
    MultiByteToWideChar(CP_UTF8, 0, urlUtf8.Get(), -1, url, (int)str::Len(url) + 1);
}

WCHAR* GetFullPath(const WCHAR* url) {
    WCHAR* path = str::Dup(url);
    str::TransChars(path, L"#?", L"\0\0");
    DecodeInPlace(path);
    return path;
}

WCHAR* GetFileName(const WCHAR* url) {
    AutoFreeWstr path(str::Dup(url));
    str::TransChars(path, L"#?", L"\0\0");
    WCHAR* base = path + str::Len(path);
    for (; base > path; base--) {
        if ('/' == base[-1] || '\\' == base[-1])
            break;
    }
    if (str::IsEmpty(base))
        return nullptr;
    DecodeInPlace(base);
    return str::Dup(base);
}

} // namespace url

namespace seqstrings {

// advance to next string
// return false if end of strings
bool SkipStr(const WCHAR*& s) {
    if (!*s) {
        return false;
    }
    while (*s) {
        s++;
    }
    s++;
    return true;
}

// Returns nullptr if s is the same as toFind
// If they are not equal, returns end of s + 1
static inline const char* StrEqWeird(const char* s, const WCHAR* toFind) {
    WCHAR wc;
    char c, c2;
    for (;;) {
        c = *s++;
        if (0 == c) {
            if (0 == *toFind)
                return nullptr;
            return s;
        }
        wc = *toFind++;
        if (wc > 255)
            return nullptr;
        c2 = (char)wc;
        if (c != c2) {
            while (*s) {
                s++;
            }
            return s + 1;
        }
        // were equal, check another char
    }
}

// optimization: allows finding WCHAR strings in char * strings array
// without the need to convert first
// returns -1 if toFind doesn't exist in strings, or its index if exists
int StrToIdx(const char* strings, const WCHAR* toFind) {
    const char* s = strings;
    int idx = 0;
    while (*s) {
        s = StrEqWeird(s, toFind);
        if (nullptr == s)
            return idx;
        ++idx;
    }
    return -1;
}

} // namespace seqstrings
