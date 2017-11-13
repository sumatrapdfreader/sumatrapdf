/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// TODO: move windows-only functions to StrUtil_win.cpp

#if !defined(_MSC_VER)
#define _strdup strdup
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
// TODO: not sure if that's correct
#define sprintf_s snprintf
#define sscanf_s sscanf
#endif

namespace str {

size_t Len(const char* s) {
    return s ? strlen(s) : 0;
}

char* Dup(const char* s) {
    return s ? _strdup(s) : nullptr;
}

// return true if s1 == s2, case sensitive
bool Eq(const char* s1, const char* s2) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    return 0 == strcmp(s1, s2);
}

// return true if s1 == s2, case insensitive
bool EqI(const char* s1, const char* s2) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    return 0 == _stricmp(s1, s2);
}

// compares two strings ignoring case and whitespace
bool EqIS(const char* s1, const char* s2) {
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

        if (tolower(*s1) != tolower(*s2))
            return false;
        if (*s1) {
            s1++;
            s2++;
        }
    }

    return !*s1 && !*s2;
}

bool EqN(const char* s1, const char* s2, size_t len) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    return 0 == strncmp(s1, s2, len);
}

bool EqNI(const char* s1, const char* s2, size_t len) {
    if (s1 == s2)
        return true;
    if (!s1 || !s2)
        return false;
    return 0 == _strnicmp(s1, s2, len);
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(const char* str, const char* txt) {
    if (str == txt)
        return true;
    if (!str || !txt)
        return false;
    return 0 == _strnicmp(str, txt, str::Len(txt));
}

bool EndsWith(const char* txt, const char* end) {
    if (!txt || !end)
        return false;
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen)
        return false;
    return str::Eq(txt + txtLen - endLen, end);
}

bool EndsWithI(const char* txt, const char* end) {
    if (!txt || !end)
        return false;
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen)
        return false;
    return str::EqI(txt + txtLen - endLen, end);
}

const char* FindI(const char* s, const char* toFind) {
    if (!s || !toFind)
        return nullptr;

    char first = (char)tolower(*toFind);
    if (!first)
        return s;
    while (*s) {
        char c = (char)tolower(*s);
        if (c == first) {
            if (str::StartsWithI(s, toFind)) {
                return s;
            }
        }
        s++;
    }
    return nullptr;
}

void ReplacePtr(char** s, const char* snew) {
    free(*s);
    *s = str::Dup(snew);
}

void ReplacePtr(const char** s, const char* snew) {
    free((char*)*s);
    *s = str::Dup(snew);
}

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
char* Join(const char* s1, const char* s2, const char* s3) {
    return Join(s1, s2, s3, nullptr);
}

char* Join(const char* s1, const char* s2, const char* s3, Allocator* allocator) {
    size_t s1Len = str::Len(s1);
    size_t s2Len = str::Len(s2);
    size_t s3Len = str::Len(s3);
    size_t len = s1Len + s2Len + s3Len + 1;
    char* res = (char*)Allocator::Alloc(allocator, len);

    char* s = res;
    memcpy(s, s1, s1Len);
    s += s1Len;
    memcpy(s, s2, s2Len);
    s += s2Len;
    memcpy(s, s3, s3Len);
    s += s3Len;
    *s = 0;

    return res;
}

char* DupN(const char* s, size_t lenCch) {
    if (!s)
        return nullptr;
    char* res = (char*)memdup((void*)s, lenCch + 1);
    if (res)
        res[lenCch] = 0;
    return res;
}

char* ToLowerInPlace(char* s) {
    char* res = s;
    for (; s && *s; s++) {
        *s = (char)tolower(*s);
    }
    return res;
}

// Encode unicode character as utf8 to dst buffer and advance dst pointer.
// The caller must ensure there is enough free space (4 bytes) in dst
void Utf8Encode(char*& dst, int c) {
    uint8* tmp = (uint8*)dst;
    if (c < 0x00080) {
        *tmp++ = (uint8)(c & 0xFF);
    } else if (c < 0x00800) {
        *tmp++ = 0xC0 + (uint8)((c >> 6) & 0x1F);
        *tmp++ = 0x80 + (uint8)(c & 0x3F);
    } else if (c < 0x10000) {
        *tmp++ = 0xE0 + (uint8)((c >> 12) & 0x0F);
        *tmp++ = 0x80 + (uint8)((c >> 6) & 0x3F);
        *tmp++ = 0x80 + (uint8)(c & 0x3F);
    } else {
        *tmp++ = 0xF0 + (uint8)((c >> 18) & 0x07);
        *tmp++ = 0x80 + (uint8)((c >> 12) & 0x3F);
        *tmp++ = 0x80 + (uint8)((c >> 6) & 0x3F);
        *tmp++ = 0x80 + (uint8)(c & 0x3F);
    }
    dst = (char*)tmp;
}

// format string to a buffer profided by the caller
// the hope here is to avoid allocating memory (assuming vsnprintf
// doesn't allocate)
bool BufFmtV(char* buf, size_t bufCchSize, const char* fmt, va_list args) {
#if defined(_MSC_VER)
    int count = _vsnprintf_s(buf, bufCchSize, _TRUNCATE, fmt, args);
#else
    int count = vsnprintf(buf, bufCchSize, fmt, args);
#endif
    buf[bufCchSize - 1] = 0;
    if ((count >= 0) && ((size_t)count < bufCchSize))
        return true;
    return false;
}

char* FmtV(const char* fmt, va_list args) {
    char message[256];
    size_t bufCchSize = dimof(message);
    char* buf = message;
    for (;;) {
#if defined(_MSC_VER)
        int count = _vsnprintf_s(buf, bufCchSize, _TRUNCATE, fmt, args);
#else
        int count = vsnprintf(buf, bufCchSize, fmt, args);
#endif
        if ((count >= 0) && ((size_t)count < bufCchSize))
            break;
        /* we have to make the buffer bigger. The algorithm used to calculate
           the new size is arbitrary (aka. educated guess) */
        if (buf != message)
            free(buf);
        if (bufCchSize < 4 * 1024)
            bufCchSize += bufCchSize;
        else
            bufCchSize += 1024;
        buf = AllocArray<char>(bufCchSize);
        if (!buf)
            break;
    }

    if (buf == message)
        buf = str::Dup(message);

    return buf;
}

char* Format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* res = FmtV(fmt, args);
    va_end(args);
    return res;
}

/* replace in <str> the chars from <oldChars> with their equivalents
   from <newChars> (similar to UNIX's tr command)
   Returns the number of replaced characters. */
size_t TransChars(char* str, const char* oldChars, const char* newChars) {
    size_t findCount = 0;

    for (char* c = str; *c; c++) {
        const char* found = str::FindChar(oldChars, *c);
        if (found) {
            *c = newChars[found - oldChars];
            findCount++;
        }
    }

    return findCount;
}

// potentially moves e backwards, skipping over whitespace
void TrimWsEnd(char* s, char*& e) {
    while (e > s) {
        --e;
        if (!str::IsWs(*e)) {
            ++e;
            return;
        }
    }
}

// the result needs to be free()d
char* Replace(const char* s, const char* toReplace, const char* replaceWith) {
    if (!s || str::IsEmpty(toReplace) || !replaceWith)
        return nullptr;

    str::Str<char> result(str::Len(s));
    size_t findLen = str::Len(toReplace), replLen = str::Len(replaceWith);
    const char *start = s, *end;
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
size_t NormalizeWS(char* str) {
    char *src = str, *dst = str;
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

static bool isNl(char c) {
    return '\r' == c || '\n' == c;
}

// replaces '\r\n' and 'r\' with just '\n' and removes empty lines
size_t NormalizeNewlinesInPlace(char* s, char* e) {
    char* start = s;
    char* dst = s;
    // remove newlines at the beginning
    while (s < e && isNl(*s)) {
        ++s;
    }

    bool inNewline = false;
    while (s < e) {
        if (isNl(*s)) {
            if (!inNewline)
                *dst++ = '\n';
            inNewline = true;
            ++s;
        } else {
            *dst++ = *s++;
            inNewline = false;
        }
    }
    if (dst < e) {
        *dst = 0;
    }
    // remove newlines from the end
    while (dst > start && dst[-1] == '\n') {
        --dst;
        *dst = 0;
    }
    return dst - start;
}

size_t NormalizeNewlinesInPlace(char* s) {
    return NormalizeNewlinesInPlace(s, s + str::Len(s));
}

// Remove all characters in "toRemove" from "str", in place.
// Returns number of removed characters.
size_t RemoveChars(char* str, const char* toRemove) {
    size_t removed = 0;
    char* dst = str;
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

#if !defined(_MSC_VER)
typedef int errno_t;

// based on https://github.com/coruus/safeclib/blob/88a3bf7c7e4cd6f0b7a8559050523bacb31362f3/src/safeclib/strncpy_s.c
// TODO: is this defined in <string.h> in C11? http://en.cppreference.com/w/c/string/byte/strncpy
// TODO: if not available, rewrite in a simpler way
errno_t strncpy_s(char* dest, size_t dmax, const char* src, size_t slen) {
    const char* overlap_bumper;

    if (dest == nullptr) {
        return (errno_t)-1;
    }

    if (dmax == 0) {
        return (errno_t)-1;
    }

    if (src == nullptr) {
        return (errno_t)-1;
    }

    if (slen == 0) {
        return (errno_t)-1;
    }

    if (dest < src) {
        overlap_bumper = src;

        while (dmax > 0) {
            if (dest == overlap_bumper) {
                return (errno_t)-1;
            }

            if (slen == 0) {
                /*
                 * Copying truncated to slen chars.  Note that the TR says to
                 * copy slen chars plus the null char.  We null the slack.
                 */
                *dest = '\0';
                return (errno_t)0;
            }

            *dest = *src;
            if (*dest == '\0') {
                return (errno_t)0;
            }

            dmax--;
            slen--;
            dest++;
            src++;
        }

    } else {
        overlap_bumper = dest;

        while (dmax > 0) {
            if (src == overlap_bumper) {
                return (errno_t)-1;
            }

            if (slen == 0) {
                /*
                 * Copying truncated to slen chars.  Note that the TR says to
                 * copy slen chars plus the null char.  We null the slack.
                 */
                *dest = '\0';
                return (errno_t)0;
            }

            *dest = *src;
            if (*dest == '\0') {
                return (errno_t)0;
            }

            dmax--;
            slen--;
            dest++;
            src++;
        }
    }

    /*
     * the entire src was not copied, so zero the string
     */
    return (errno_t)-1;
}

errno_t strncat_s(char* dest IS_UNUSED, size_t destsz IS_UNUSED, const char* src IS_UNUSED, size_t count IS_UNUSED) {
    CrashAlwaysIf(true);
    return (errno_t)0;
}
#endif

// Note: BufSet() should only be used when absolutely necessary (e.g. when
// handling buffers in OS-defined structures)
// returns the number of characters written (without the terminating \0)
size_t BufSet(char* dst, size_t dstCchSize, const char* src) {
    CrashAlwaysIf(0 == dstCchSize);

    size_t srcCchSize = str::Len(src);
    size_t toCopy = std::min(dstCchSize - 1, srcCchSize);

    errno_t err = strncpy_s(dst, dstCchSize, src, toCopy);
    CrashIf(err || dst[toCopy] != '\0');

    return toCopy;
}

// append as much of s at the end of dst (which must be properly null-terminated)
// as will fit.
size_t BufAppend(char* dst, size_t dstCchSize, const char* s) {
    CrashAlwaysIf(0 == dstCchSize);

    size_t currDstCchLen = str::Len(dst);
    if (currDstCchLen + 1 >= dstCchSize)
        return 0;
    size_t left = dstCchSize - currDstCchLen - 1;
    size_t srcCchSize = str::Len(s);
    size_t toCopy = std::min(left, srcCchSize);

    errno_t err = strncat_s(dst, dstCchSize, s, toCopy);
    CrashIf(err || dst[currDstCchLen + toCopy] != '\0');

    return toCopy;
}

/* Convert binary data in <buf> of size <len> to a hex-encoded string */
char* MemToHex(const unsigned char* buf, size_t len) {
    /* 2 hex chars per byte, +1 for terminating 0 */
    char* ret = AllocArray<char>(2 * len + 1);
    if (!ret)
        return nullptr;
    char* dst = ret;
    for (; len > 0; len--) {
        sprintf_s(dst, 3, "%02x", *buf++);
        dst += 2;
    }
    return ret;
}

/* Reverse of MemToHex. Convert a 0-terminatd hex-encoded string <s> to
   binary data pointed by <buf> of max size bufLen.
   Returns false if size of <s> doesn't match bufLen or is not a valid
   hex string. */
bool HexToMem(const char* s, unsigned char* buf, size_t bufLen) {
    for (; bufLen > 0; bufLen--) {
        int c;
        if (1 != sscanf_s(s, "%02x", &c))
            return false;
        s += 2;
        *buf++ = (unsigned char)c;
    }
    return *s == '\0';
}

static char* ExtractUntil(const char* pos, char c, const char** endOut) {
    *endOut = FindChar(pos, c);
    if (!*endOut)
        return nullptr;
    return str::DupN(pos, *endOut - pos);
}

static const char* ParseLimitedNumber(const char* str, const char* format, const char** endOut, void* valueOut) {
    unsigned int width;
    char f2[] = "% ";
    const char* endF = Parse(format, "%u%c", &width, &f2[1]);
    if (endF && FindChar("udx", f2[1]) && width <= Len(str)) {
        char limited[16]; // 32-bit integers are at most 11 characters long
        str::BufSet(limited, std::min((size_t)width + 1, dimof(limited)), str);
        const char* end = Parse(limited, f2, valueOut);
        if (end && !*end)
            *endOut = str + width;
    }
    return endF;
}

/* Parses a string into several variables sscanf-style (i.e. pass in pointers
   to where the parsed values are to be stored). Returns a pointer to the first
   character that's not been parsed when successful and nullptr otherwise.

   Supported formats:
     %u - parses an unsigned int
     %d - parses a signed int
     %x - parses an unsigned hex-int
     %f - parses a float
     %c - parses a single WCHAR
     %s - parses a string (pass in a WCHAR**, free after use - also on failure!)
     %S - parses a string into a AutoFreeW
     %? - makes the next single character optional (e.g. "x%?,y" parses both "xy" and "x,y")
     %$ - causes the parsing to fail if it's encountered when not at the end of the string
     %  - skips a single whitespace character
     %_ - skips one or multiple whitespace characters (or none at all)
     %% - matches a single '%'

   %u, %d and %x accept an optional width argument, indicating exactly how many
   characters must be read for parsing the number (e.g. "%4d" parses -123 out of "-12345"
   and doesn't parse "123" at all).
*/
static const char* ParseV(const char* str, const char* format, va_list args) {
    for (const char* f = format; *f; f++) {
        if (*f != '%') {
            if (*f != *str)
                return nullptr;
            str++;
            continue;
        }
        f++;

        const char* end = nullptr;
        if ('u' == *f)
            *va_arg(args, unsigned int*) = strtoul(str, (char**)&end, 10);
        else if ('d' == *f)
            *va_arg(args, int*) = strtol(str, (char**)&end, 10);
        else if ('x' == *f)
            *va_arg(args, unsigned int*) = strtoul(str, (char**)&end, 16);
        else if ('f' == *f)
            *va_arg(args, float*) = (float)strtod(str, (char**)&end);
        else if ('c' == *f)
            *va_arg(args, char*) = *str, end = str + 1;
        else if ('s' == *f)
            *va_arg(args, char**) = ExtractUntil(str, *(f + 1), &end);
        else if ('S' == *f)
            va_arg(args, AutoFree*)->Set(ExtractUntil(str, *(f + 1), &end));
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
            end = (char*)str + 1;
        } else if (str::IsDigit(*f))
            f = ParseLimitedNumber(str, f, &end, va_arg(args, void*)) - 1;
        if (!end || end == str)
            return nullptr;
        str = end;
    }
    return str;
}

const char* Parse(const char* str, const char* fmt, ...) {
    if (!str || !fmt)
        return nullptr;

    va_list args;
    va_start(args, fmt);
    const char* res = ParseV(str, fmt, args);
    va_end(args);
    return res;
}

// TODO: could optimize it by making the main Parse() implementation
// work with explicit length and not rely on zero-termination
const char* Parse(const char* str, size_t len, const char* fmt, ...) {
    char buf[128] = {0};
    char* s = buf;

    if (!str || !fmt)
        return nullptr;

    if (len < dimof(buf))
        memcpy(buf, str, len);
    else
        s = DupN(str, len);

    va_list args;
    va_start(args, fmt);
    const char* res = ParseV(s, fmt, args);
    va_end(args);

    if (res)
        res = str + (res - s);
    if (s != buf)
        free(s);
    return res;
}

} // namespace str

namespace url {

void DecodeInPlace(char* url) {
    for (char *src = url; *src; src++, url++) {
        int val;
        if (*src == '%' && str::Parse(src, "%%%2x", &val)) {
            *url = (char)val;
            src += 2;
        } else {
            *url = *src;
        }
    }
    *url = '\0';
}

} // namespace url

// seqstrings is for size-efficient implementation of:
// string -> int and int->string.
// it's even more efficient than using char *[] array
// it comes at the cost of speed, so it's not good for places
// that are critial for performance. On the other hand, it's
// not that bad: linear scanning of memory is fast due to the magic
// of L1 cache
namespace seqstrings {

// advance to next string
// return false if end of strings
bool SkipStr(const char*& s) {
    if (!*s) {
        return false;
    }
    while (*s) {
        s++;
    }
    s++;
    return true;
}

// advance to next string
// return false if end of strings
bool SkipStr(char*& s) {
    return SkipStr((const char*&)s);
}

// Returns nullptr if s is the same as toFind
// If they are not equal, returns end of s + 1
static inline const char* StrEqWeird(const char* s, const char* toFind) {
    char c;
    for (;;) {
        c = *s++;
        if (0 == c) {
            if (0 == *toFind)
                return nullptr;
            return s;
        }
        if (c != *toFind++) {
            while (*s) {
                s++;
            }
            return s + 1;
        }
        // were equal, check another char
    }
}

// conceptually strings is an array of 0-terminated strings where, laid
// out sequentially in memory, terminated with a 0-length string
// Returns index of toFind string in strings
// Returns -1 if string doesn't exist
int StrToIdx(const char* strings, const char* toFind) {
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

// Given an index in the "array" of sequentially laid out strings,
// returns a strings at that index.
const char* IdxToStr(const char* strings, int idx) {
    const char* s = strings;
    while (idx > 0) {
        while (*s) {
            s++;
        }
        s++;
        --idx;
    }
    return s;
}

} // namespace seqstrings
