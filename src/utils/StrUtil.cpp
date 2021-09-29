/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

#if !defined(_MSC_VER)
#define _strdup strdup
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
// TODO: not sure if that's correct
#define sscanf_s sscanf
#endif

// --- copyright for utf8 code below

/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
 * left as-is for anyone who may want to do such conversion, which was
 * allowed in earlier algorithms.
 */
static const u8 trailingBytesForUTF8[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};

/*
 * Utility routine to tell whether a sequence of bytes is legal UTF-8.
 * This must be called with the length pre-determined by the first byte.
 * If not calling this from ConvertUTF8to*, then the length can be set by:
 *  length = trailingBytesForUTF8[*source]+1;
 * and the sequence is illegal right away if there aren't that many bytes
 * available.
 * If presented with a length > 4, this returns false.  The Unicode
 * definition of UTF-8 goes up to 4-byte sequences.
 */

static bool isLegalUTF8(const u8* source, int length) {
    u8 a;
    const u8* srcptr = source + length;

    switch (length) {
        default:
            return false;
        /* Everything else falls through when "true"... */
        case 4:
            if ((a = (*--srcptr)) < 0x80 || a > 0xBF) {
                return false;
            }
        case 3:
            if ((a = (*--srcptr)) < 0x80 || a > 0xBF) {
                return false;
            }
        case 2:
            if ((a = (*--srcptr)) > 0xBF) {
                return false;
            }

            switch (*source) {
                /* no fall-through in this inner switch */
                case 0xE0:
                    if (a < 0xA0) {
                        return false;
                    }
                    break;
                case 0xED:
                    if (a > 0x9F) {
                        return false;
                    }
                    break;
                case 0xF0:
                    if (a < 0x90) {
                        return false;
                    }
                    break;
                case 0xF4:
                    if (a > 0x8F) {
                        return false;
                    }
                    break;
                default:
                    if (a < 0x80) {
                        return false;
                    }
            }

        case 1:
            if (*source >= 0x80 && *source < 0xC2) {
                return false;
            }
    }

    return *source <= 0xF4;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return whether a UTF-8 sequence is legal or not.
 * This is not used here; it's just exported.
 */
bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd) {
    int n = trailingBytesForUTF8[*source] + 1;
    if (source + n > sourceEnd) {
        return false;
    }
    return isLegalUTF8(source, n);
}

/*
 * Exported function to return whether a UTF-8 string is legal or not.
 * This is not used here; it's just exported.
 */
bool isLegalUTF8String(const u8** source, const u8* sourceEnd) {
    while (*source != sourceEnd) {
        int n = trailingBytesForUTF8[**source] + 1;
        if (n > sourceEnd - *source || !isLegalUTF8(*source, n)) {
            return false;
        }
        *source += n;
    }
    return true;
}

// --- end of Unicode, Inc. utf8 code

namespace str {

size_t Len(const char* s) {
    return s ? strlen(s) : 0;
}

size_t Len(const WCHAR* s) {
    return s ? wcslen(s) : 0;
}

void Free(const char* s) {
    free((void*)s);
}

void Free(const u8* s) {
    free((void*)s);
}

void Free(std::string_view sv) {
    free((void*)sv.data());
}

void Free(ByteSlice d) {
    free((void*)d.data());
}

void Free(const WCHAR* s) {
    free((void*)s);
}

void Free(std::wstring_view sv) {
    free((void*)sv.data());
}

void FreePtr(const char** s) {
    str::Free(*s);
    *s = nullptr;
}

void FreePtr(char** s) {
    str::Free(*s);
    *s = nullptr;
}

void FreePtr(const WCHAR** s) {
    str::Free(*s);
    *s = nullptr;
}

void FreePtr(WCHAR** s) {
    str::Free(*s);
    *s = nullptr;
}

char* Dup(Allocator* a, const char* s, size_t cch) {
    CrashIf(!s && (int)cch > 0);
    if (cch == (size_t)-1) {
        cch = str::Len(s);
    }
    return (char*)Allocator::MemDup(a, s, cch * sizeof(char), sizeof(char));
}

char* Dup(const char* s, size_t cch) {
    return Dup(nullptr, s, cch);
}

// allocates a copy of the source string inside the allocator.
// it's only safe in PoolAllocator because allocated data
// never moves in memory
char* Dup(Allocator* a, std::string_view sv) {
    return Dup(a, sv.data(), sv.size());
}

char* Dup(const std::string_view sv) {
    return Dup(nullptr, sv.data(), sv.size());
}

char* Dup(const ByteSlice d) {
    return Dup(nullptr, (const char*)d.data(), d.size());
}

WCHAR* Dup(Allocator* a, const WCHAR* s, size_t cch) {
    if (cch == (size_t)-1) {
        cch = str::Len(s);
    }
    return (WCHAR*)Allocator::MemDup(a, s, cch * sizeof(WCHAR), sizeof(WCHAR));
}

WCHAR* Dup(const WCHAR* s, size_t cch) {
    return Dup(nullptr, s, cch);
}

WCHAR* Dup(std::wstring_view sv) {
    return Dup(nullptr, sv.data(), sv.size());
}

// return true if s1 == s2, case sensitive
bool Eq(const char* s1, const char* s2) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == strcmp(s1, s2);
}

bool Eq(std::string_view s1, const char* s2) {
    return EqN(s1.data(), s2, s1.size());
}

bool Eq(ByteSlice sp1, ByteSlice sp2) {
    if (sp1.size() != sp2.size()) {
        return false;
    }
    if (sp1.empty()) {
        return true;
    }
    const char* s1 = (const char*)sp1.data();
    const char* s2 = (const char*)sp2.data();
    return 0 == strcmp(s1, s2);
}

bool EqI(std::string_view s1, const char* s2) {
    return EqNI(s1.data(), s2, s1.size());
}

// return true if s1 == s2, case insensitive
bool EqI(const char* s1, const char* s2) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == _stricmp(s1, s2);
}

// compares two strings ignoring case and whitespace
bool EqIS(const char* s1, const char* s2) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }

    while (*s1 && *s2) {
        // skip whitespace
        for (; IsWs(*s1); s1++) {
            // do nothing
        }
        for (; IsWs(*s2); s2++) {
            // do nothing
        }

        if (tolower(*s1) != tolower(*s2)) {
            return false;
        }
        if (*s1) {
            s1++;
            s2++;
        }
    }

    return !*s1 && !*s2;
}

bool EqN(const char* s1, const char* s2, size_t len) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == strncmp(s1, s2, len);
}

bool EqNI(const char* s1, const char* s2, size_t len) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == _strnicmp(s1, s2, len);
}

bool IsEmpty(const char* s) {
    return !s || (0 == *s);
}

bool StartsWith(const char* s, const char* prefix) {
    return EqN(s, prefix, Len(prefix));
}

bool StartsWith(const u8* str, const char* prefix) {
    return StartsWith((const char*)str, prefix);
}

bool StartsWith(std::string_view s, const char* prefix) {
    size_t n = Len(prefix);
    if (n > s.size()) {
        return false;
    }
    return EqN(s.data(), prefix, n);
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(const char* s, const char* prefix) {
    if (s == prefix) {
        return true;
    }
    if (!s || !prefix) {
        return false;
    }
    return 0 == _strnicmp(s, prefix, str::Len(prefix));
}

ByteSlice ToSpan(const char* s) {
    size_t n = str::Len(s);
    return {(u8*)s, n};
}

bool Contains(std::string_view s, const char* txt) {
    // TODO: needs to respect s.size()
    const char* p = str::Find(s.data(), txt);
    bool contains = p != nullptr;
    return contains;
}

bool EndsWith(const char* txt, const char* end) {
    if (!txt || !end) {
        return false;
    }
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen) {
        return false;
    }
    return str::Eq(txt + txtLen - endLen, end);
}

bool EndsWithI(const char* txt, const char* end) {
    if (!txt || !end) {
        return false;
    }
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen) {
        return false;
    }
    return str::EqI(txt + txtLen - endLen, end);
}

bool EqNIx(const char* s, size_t len, const char* s2) {
    return str::Len(s2) == len && str::StartsWithI(s, s2);
}

const char* FindI(const char* s, const char* toFind) {
    if (!s || !toFind) {
        return nullptr;
    }

    char first = (char)tolower(*toFind);
    if (!first) {
        return s;
    }
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

void ReplacePtr(const char** s, const char* snew) {
    if (*s != snew) {
        str::Free(*s);
        *s = (char*)snew;
    }
}

void ReplacePtr(char** s, const char* snew) {
    ReplacePtr((const char**)s, snew);
}

void ReplacePtr(const WCHAR** s, const WCHAR* snew) {
    if (*s != snew) {
        str::Free(*s);
        *s = (WCHAR*)snew;
    }
}

void ReplaceWithCopy(const char** s, const char* snew) {
    if (*s != snew) {
        str::Free(*s);
        *s = str::Dup(snew);
    }
}

void ReplaceWithCopy(char** s, const char* snew) {
    ReplaceWithCopy((const char**)s, snew);
}

void ReplaceWithCopy(const WCHAR** s, const WCHAR* snew) {
    if (*s != snew) {
        str::Free(*s);
        *s = str::Dup(snew);
    }
}

void ReplaceWithCopy(WCHAR** s, const WCHAR* snew) {
    ReplaceWithCopy((const WCHAR**)s, snew);
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

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
char* Join(const char* s1, const char* s2, const char* s3) {
    return Join(s1, s2, s3, nullptr);
}

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
WCHAR* Join(const WCHAR* s1, const WCHAR* s2, const WCHAR* s3, Allocator* allocator) {
    // don't use str::Format(L"%s%s%s", s1, s2, s3) since the strings
    // might contain non-characters which str::Format fails to handle
    size_t s1Len = str::Len(s1), s2Len = str::Len(s2), s3Len = str::Len(s3);
    size_t len = s1Len + s2Len + s3Len + 1;
    WCHAR* res = (WCHAR*)Allocator::Alloc(allocator, len * sizeof(WCHAR));
    memcpy(res, s1, s1Len * sizeof(WCHAR));
    memcpy(res + s1Len, s2, s2Len * sizeof(WCHAR));
    memcpy(res + s1Len + s2Len, s3, s3Len * sizeof(WCHAR));
    res[s1Len + s2Len + s3Len] = '\0';
    return res;
}

WCHAR* Join(const WCHAR* s1, const WCHAR* s2, const WCHAR* s3) {
    return Join(s1, s2, s3, nullptr);
}

char* ToLowerInPlace(char* s) {
    char* res = s;
    for (; s && *s; s++) {
        *s = (char)tolower(*s);
    }
    return res;
}

char* ToLower(const char* s) {
    char* s2 = str::Dup(s);
    return ToLowerInPlace(s2);
}

// Encode unicode character as utf8 to dst buffer and advance dst pointer.
// The caller must ensure there is enough free space (4 bytes) in dst
void Utf8Encode(char*& dst, int c) {
    u8* tmp = (u8*)dst;
    if (c < 0x00080) {
        *tmp++ = (u8)(c & 0xFF);
    } else if (c < 0x00800) {
        *tmp++ = 0xC0 + (u8)((c >> 6) & 0x1F);
        *tmp++ = 0x80 + (u8)(c & 0x3F);
    } else if (c < 0x10000) {
        *tmp++ = 0xE0 + (u8)((c >> 12) & 0x0F);
        *tmp++ = 0x80 + (u8)((c >> 6) & 0x3F);
        *tmp++ = 0x80 + (u8)(c & 0x3F);
    } else {
        *tmp++ = 0xF0 + (u8)((c >> 18) & 0x07);
        *tmp++ = 0x80 + (u8)((c >> 12) & 0x3F);
        *tmp++ = 0x80 + (u8)((c >> 6) & 0x3F);
        *tmp++ = 0x80 + (u8)(c & 0x3F);
    }
    dst = (char*)tmp;
}

// Note: I tried an optimization: return (unsigned)(c - '0') < 10;
// but it seems to mis-compile in release builds
bool IsDigit(char c) {
    return ('0' <= c) && (c <= '9');
}

bool IsWs(char c) {
    if (' ' == c) {
        return true;
    }
    if (('\t' <= c) && (c <= '\r')) {
        return true;
    }
    return false;
}

const char* FindChar(const char* str, char c) {
    return strchr(str, c);
}

char* FindChar(char* str, char c) {
    return strchr(str, c);
}

const char* FindCharLast(const char* str, char c) {
    return strrchr(str, c);
}
char* FindCharLast(char* str, char c) {
    return strrchr(str, c);
}

const char* Find(const char* str, const char* find) {
    return strstr(str, find);
}

// format string to a buffer profided by the caller
// the hope here is to avoid allocating memory (assuming vsnprintf
// doesn't allocate)
bool BufFmtV(char* buf, size_t bufCchSize, const char* fmt, va_list args) {
    int count = vsnprintf(buf, bufCchSize, fmt, args);
    buf[bufCchSize - 1] = 0;
    return (count >= 0) && ((size_t)count < bufCchSize);
}

bool BufFmt(char* buf, size_t bufCchSize, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    auto res = BufFmtV(buf, bufCchSize, fmt, args);
    va_end(args);
    return res;
}

// TODO: need to finish StrFormat and use it instead.
char* FmtV(const char* fmt, va_list args) {
    char message[256] = {0};
    size_t bufCchSize = dimof(message);
    char* buf = message;
    for (;;) {
        int count = vsnprintf(buf, bufCchSize, fmt, args);
        // happened in https://github.com/sumatrapdfreader/sumatrapdf/issues/878
        // when %S string had certain Unicode characters
        CrashIf(count == -1);
        if ((count >= 0) && ((size_t)count < bufCchSize)) {
            break;
        }
        /* we have to make the buffer bigger. The algorithm used to calculate
           the new size is arbitrary (aka. educated guess) */
        if (buf != message) {
            free(buf);
        }
        if (bufCchSize < 4 * 1024) {
            bufCchSize += bufCchSize;
        } else {
            bufCchSize += 1024;
        }
        buf = AllocArray<char>(bufCchSize);
        if (!buf) {
            break;
        }
    }

    if (buf == message) {
        buf = str::Dup(message);
    }

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
size_t TransCharsInPlace(char* str, const char* oldChars, const char* newChars) {
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

// Trim whitespace characters, in-place, inside s.
// Returns number of trimmed characters.
size_t TrimWSInPlace(char* s, TrimOpt opt) {
    size_t sLen = str::Len(s);
    char* ns = s;
    char* e = s + sLen;
    char* ne = e;
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
        size_t toCopy = sLen - trimmed + 1; // +1 for terminating 0
        memmove(s, ns, toCopy);
    }
    return trimmed;
}

// the result needs to be free()d
char* Replace(const char* s, const char* toReplace, const char* replaceWith) {
    if (!s || str::IsEmpty(toReplace) || !replaceWith) {
        return nullptr;
    }

    str::Str result(str::Len(s));
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
size_t NormalizeWSInPlace(char* str) {
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

    if (dst > str && IsWs(*(dst - 1))) {
        dst--;
    }
    *dst = '\0';

    return src - dst;
}

static bool isNl(char c) {
    return '\r' == c || '\n' == c;
}

// replaces '\r\n' and '\r' with just '\n' and removes empty lines
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
            if (!inNewline) {
                *dst++ = '\n';
            }
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
size_t RemoveCharsInPlace(char* str, const char* toRemove) {
    size_t removed = 0;
    char* dst = str;
    while (*str) {
        char c = *str++;
        if (!str::FindChar(toRemove, c)) {
            *dst++ = c;
        } else {
            ++removed;
        }
    }
    *dst = '\0';
    return removed;
}

// Remove all characters in "toRemove" from "str", in place.
// Returns number of removed characters.
size_t RemoveCharsInPlace(WCHAR* str, const WCHAR* toRemove) {
    size_t removed = 0;
    WCHAR* dst = str;
    while (*str) {
        WCHAR c = *str++;
        if (!str::FindChar(toRemove, c)) {
            *dst++ = c;
        } else {
            ++removed;
        }
    }
    *dst = '\0';
    return removed;
}

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
    if (currDstCchLen + 1 >= dstCchSize) {
        return 0;
    }
    size_t left = dstCchSize - currDstCchLen - 1;
    size_t srcCchSize = str::Len(s);
    size_t toCopy = std::min(left, srcCchSize);

    errno_t err = strncat_s(dst, dstCchSize, s, toCopy);
    CrashIf(err || dst[currDstCchLen + toCopy] != '\0');

    return toCopy;
}

/* Convert binary data in <buf> of size <len> to a hex-encoded string */
char* MemToHex(const u8* buf, size_t len) {
    /* 2 hex chars per byte, +1 for terminating 0 */
    char* ret = AllocArray<char>(2 * len + 1);
    if (!ret) {
        return nullptr;
    }
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
bool HexToMem(const char* s, u8* buf, size_t bufLen) {
    for (; bufLen > 0; bufLen--) {
        unsigned int c;
        if (1 != sscanf_s(s, "%02x", &c)) {
            return false;
        }
        s += 2;
        *buf++ = (u8)c;
    }
    return *s == '\0';
}

static char* ExtractUntil(const char* pos, char c, const char** endOut) {
    *endOut = FindChar(pos, c);
    if (!*endOut) {
        return nullptr;
    }
    return str::Dup(pos, *endOut - pos);
}

static const char* ParseLimitedNumber(const char* str, const char* format, const char** endOut, void* valueOut) {
    unsigned int width;
    char f2[] = "% ";
    const char* endF = Parse(format, "%u%c", &width, &f2[1]);
    if (endF && FindChar("udx", f2[1]) && width <= Len(str)) {
        char limited[16]; // 32-bit integers are at most 11 characters long
        str::BufSet(limited, std::min((size_t)width + 1, dimof(limited)), str);
        const char* end = Parse(limited, f2, valueOut);
        if (end && !*end) {
            *endOut = str + width;
        }
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
            if (*f != *str) {
                return nullptr;
            }
            str++;
            continue;
        }
        f++;

        const char* end = nullptr;
        if ('u' == *f) {
            *va_arg(args, unsigned int*) = strtoul(str, (char**)&end, 10);
        } else if ('d' == *f) {
            *va_arg(args, int*) = strtol(str, (char**)&end, 10);
        } else if ('x' == *f) {
            *va_arg(args, unsigned int*) = strtoul(str, (char**)&end, 16);
        } else if ('f' == *f) {
            *va_arg(args, float*) = (float)strtod(str, (char**)&end);
        } else if ('g' == *f) {
            *va_arg(args, float*) = (float)strtod(str, (char**)&end);
        } else if ('c' == *f) {
            *va_arg(args, char*) = *str, end = str + 1;
        } else if ('s' == *f) {
            *va_arg(args, char**) = ExtractUntil(str, *(f + 1), &end);
        } else if ('S' == *f) {
            va_arg(args, AutoFree*)->Set(ExtractUntil(str, *(f + 1), &end));
        } else if ('$' == *f && !*str) {
            continue; // don't fail, if we're indeed at the end of the string
        } else if ('%' == *f && *f == *str) {
            end = str + 1;
        } else if (' ' == *f && str::IsWs(*str)) {
            end = str + 1;
        } else if ('_' == *f) {
            if (!str::IsWs(*str)) {
                continue; // don't fail, if there's no whitespace at all
            }
            for (end = str + 1; str::IsWs(*end); end++) {
                // do nothing
            }
        } else if ('?' == *f && *(f + 1)) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            if (*str != *++f) {
                continue;
            }
            end = (char*)str + 1;
        } else if (str::IsDigit(*f)) {
            f = ParseLimitedNumber(str, f, &end, va_arg(args, void*)) - 1;
        }
        if (!end || end == str) {
            return nullptr;
        }
        str = end;
    }
    return str;
}

const char* Parse(const char* str, const char* fmt, ...) {
    if (!str || !fmt) {
        return nullptr;
    }

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

    if (!str || !fmt) {
        return nullptr;
    }

    if (len < dimof(buf)) {
        memcpy(buf, str, len);
    } else {
        s = Dup(str, len);
    }

    va_list args;
    va_start(args, fmt);
    const char* res = ParseV(s, fmt, args);
    va_end(args);

    if (res) {
        res = str + (res - s);
    }
    if (s != buf) {
        free(s);
    }
    return res;
}

bool IsAlNum(char c) {
    if (c >= '0' && c <= '9') {
        return true;
    }
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        return true;
    }
    return false;
}

/* compares two strings "naturally" by sorting numbers within a string
   numerically instead of by pure ASCII order; we imitate Windows Explorer
   by sorting special characters before alphanumeric characters
   (e.g. ".hg" < "2.pdf" < "100.pdf" < "zzz")
   // TODO: this should be utf8-aware, see e.g. cbx\bug1234-*.cbr file
*/
int CmpNatural(const char* a, const char* b) {
    CrashAlwaysIf(!a || !b);
    const char *aStart = a, *bStart = b;
    int diff = 0;

    while (diff == 0) {
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
        if (!*a && !*b) {
            return strcmp(aStart, bStart);
        }

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
                if (!str::IsDigit(*a)) {
                    return -1;
                }
                if (!str::IsDigit(*b)) {
                    return 1;
                }
                // remember the difference for when the numbers are of the same magnitude
                if (0 == diff) {
                    diff = *a - *b;
                }
            }
            // neither *a nor *b is a digit, so continue with them (unless diff != 0)
            a--;
            b--;
        } else if (str::IsAlNum(*a) && str::IsAlNum(*b)) {
            // sort letters case-insensitively
            diff = tolower(*a) - tolower(*b);
        } else if (str::IsAlNum(*a)) {
            // sort special characters before text and numbers
            return 1;
        } else if (str::IsAlNum(*b)) {
            return -1;
        } else {
            // sort special characters by ASCII code
            diff = *a - *b;
        }
        a++;
        b++;
    }

    return diff;
}

bool EmptyOrWhiteSpaceOnly(std::string_view sv) {
    size_t n = sv.size();
    if (n == 0) {
        return true;
    }
    for (size_t i = 0; i < n; i++) {
        char c = sv[i];
        if (!str::IsWs(c)) {
            return false;
        }
    }
    return true;
}

} // namespace str

namespace url {

void DecodeInPlace(char* url) {
    for (char* src = url; *src; src++, url++) {
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
// return nullptr if end of strings
const char* SkipStr(const char* s) {
    // empty string marks the end, means idx was too high
    if (!*s) {
        return nullptr;
    }
    // skip past next '\0' char
    while (*s) {
        s++;
    }
    return s + 1;
}

char* SkipStr(char* s) {
    auto res = SkipStr((const char*)s);
    return (char*)res;
}

// Returns nullptr if s is the same as toFind
// If they are not equal, returns end of s + 1
static inline const char* StrEqWeird(const char* s, const char* toFind) {
    char c;
    for (;;) {
        c = *s++;
        if (0 == c) {
            if (0 == *toFind) {
                return nullptr;
            }
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
int StrToIdx(const char* strs, const char* toFind) {
    if (!toFind) {
        return -1;
    }
    const char* s = strs;
    int idx = 0;
    while (*s) {
        s = StrEqWeird(s, toFind);
        if (nullptr == s) {
            return idx;
        }
        ++idx;
    }
    return -1;
}

// like StrToIdx but ignores case and whitespace
int StrToIdxIS(const char* strs, const char* toFind) {
    if (!toFind) {
        return -1;
    }
    const char* s = strs;
    int idx = 0;
    while (*s) {
        if (str::EqIS(s, toFind)) {
            return idx;
        }
        s = s + str::Len(s) + 1;
        ++idx;
    }
    return -1;
}

// Given an index in the "array" of sequentially laid out strings,
// returns a strings at that index.
const char* IdxToStr(const char* strs, int idx) {
    CrashIf(idx < 0);
    const char* s = strs;
    while (idx > 0) {
        s = SkipStr(s);
        if (!s) {
            return nullptr;
        }
        --idx;
    }
    return s;
}

} // namespace seqstrings

namespace str {

// for compatibility with C string, the last character is always 0
// kPadding is number of characters needed for terminating character
static constexpr size_t kPadding = 1;

static char* EnsureCap(Str* s, size_t needed) {
    if (needed + kPadding <= Str::kBufChars) {
        s->els = s->buf; // TODO: not needed?
        return s->buf;
    }

    size_t capacityHint = s->cap;
    // tricky: to save sapce we reuse cap for capacityHint
    if (!s->els || (s->els == s->buf)) {
        // on first expand cap might be capacityHint
        s->cap = 0;
    }

    if (s->cap >= needed) {
        return s->els;
    }

    size_t newCap = s->cap * 2;
    if (needed > newCap) {
        newCap = needed;
    }
    if (newCap < capacityHint) {
        newCap = capacityHint;
    }

    size_t newElCount = newCap + kPadding;

    s->nReallocs++;

    size_t allocSize = newElCount;
    char* newEls;
    if (s->buf == s->els) {
        newEls = (char*)Allocator::Alloc(s->allocator, allocSize);
        if (newEls) {
            memcpy(newEls, s->buf, s->len + 1);
        }
    } else {
        newEls = (char*)Allocator::Realloc(s->allocator, s->els, allocSize);
    }
    if (!newEls) {
        CrashAlwaysIf(gAllowAllocFailure.load() == 0);
        return nullptr;
    }
    s->els = newEls;
    s->cap = (u32)newCap;
    return newEls;
}

static char* MakeSpaceAt(Str* s, size_t idx, size_t count) {
    CrashIf(count == 0);
    u32 newLen = std::max(s->len, (u32)idx) + (u32)count;
    char* buf = EnsureCap(s, newLen);
    if (!buf) {
        return nullptr;
    }
    buf[newLen] = 0;
    char* res = &(buf[idx]);
    if (s->len > idx) {
        // inserting in the middle of string, have to copy
        char* src = buf + idx;
        char* dst = buf + idx + count;
        memmove(dst, src, s->len - idx);
    }
    s->len = newLen;
    // ZeroMemory(res, count);
    return res;
}

static void Free(Str* s) {
    if (!s->els || (s->els == s->buf)) {
        return;
    }
    Allocator::Free(s->allocator, s->els);
    s->els = nullptr;
}

void Str::Reset() {
    Free(this);
    len = 0;
    cap = 0;
    els = buf;

#if defined(DEBUG)
#define kFillerStr "01234567890123456789012345678901"
    // to catch mistakes earlier, fill the buffer with a known string
    constexpr size_t nFiller = sizeof(kFillerStr) - 1;
    static_assert(nFiller == Str::kBufChars);
    memcpy(buf, kFillerStr, kBufChars);
#endif

    buf[0] = 0;
}

// allocator is not owned by Vec and must outlive it
Str::Str(size_t capHint, Allocator* a) {
    allocator = a;
    Reset();
    cap = (u32)(capHint + kPadding); // + kPadding for terminating 0
}

// ensure that a Vec never shares its els buffer with another after a clone/copy
// note: we don't inherit allocator as it's not needed for our use cases
Str::Str(const Str& that) {
    Reset();
    char* s = EnsureCap(this, that.len);
    char* sOrig = that.Get();
    len = that.len;
    size_t n = len + kPadding;
    memcpy(s, sOrig, n);
}

Str::Str(std::string_view s) {
    Reset();
    AppendView(s);
}

Str::Str(const char* s) {
    Reset();
    Append(s);
}

Str& Str::operator=(const Str& that) {
    if (this == &that) {
        return *this;
    }
    Reset();
    char* s = EnsureCap(this, that.len);
    char* sOrig = that.Get();
    len = that.len;
    size_t n = len + kPadding;
    memcpy(s, sOrig, n);
    return *this;
}

Str::~Str() {
    Free(this);
}

char& Str::at(size_t idx) const {
    CrashIf(idx >= (u32)len);
    return els[idx];
}

char& Str::at(int idx) const {
    CrashIf(idx < 0);
    return at((size_t)idx);
}

char& Str::operator[](size_t idx) const {
    return at(idx);
}

char& Str::operator[](long idx) const {
    CrashIf(idx < 0);
    return at((size_t)idx);
}

char& Str::operator[](ULONG idx) const {
    return at((size_t)idx);
}

char& Str::operator[](int idx) const {
    CrashIf(idx < 0);
    return at((size_t)idx);
}

#if defined(_WIN64)
char& Str::at(u32 idx) const {
    return at((size_t)idx);
}

char& Str::operator[](u32 idx) const {
    return at((size_t)idx);
}
#endif

size_t Str::size() const {
    return len;
}
int Str::isize() const {
    return (int)len;
}

bool Str::InsertAt(size_t idx, char el) {
    char* p = MakeSpaceAt(this, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool Str::Append(char el) {
    return InsertAt(len, el);
}

bool Str::Append(const char* src, size_t count) {
    if (-1 == count) {
        count = str::Len(src);
    }
    if (!src || 0 == count) {
        return true;
    }
    char* dst = MakeSpaceAt(this, len, count);
    if (!dst) {
        return false;
    }
    memcpy(dst, src, count);
    return true;
}

char Str::RemoveAt(size_t idx, size_t count) {
    char res = at(idx);
    if (len > idx + count) {
        char* dst = els + idx;
        char* src = els + idx + count;
        size_t nToMove = len - idx - count;
        memmove(dst, src, nToMove);
    }
    len -= (u32)count;
    memset(els + len, 0, count);
    return res;
}

char Str::RemoveLast() {
    if (len == 0) {
        return 0;
    }
    return RemoveAt(len - 1);
}

char& Str::Last() const {
    CrashIf(0 == len);
    return at(len - 1);
}

// perf hack for using as a buffer: client can get accumulated data
// without duplicate allocation. Note: since Vec over-allocates, this
// is likely to use more memory than strictly necessary, but in most cases
// it doesn't matter
char* Str::StealData() {
    char* res = els;
    if (els == buf) {
        res = (char*)Allocator::MemDup(allocator, buf, len + kPadding);
    }
    els = buf;
    Reset();
    return res;
}

char* Str::LendData() const {
    return els;
}

int Str::Find(char el, size_t startAt) const {
    for (size_t i = startAt; i < len; i++) {
        if (els[i] == el) {
            return (int)i;
        }
    }
    return -1;
}

bool Str::Contains(char el) const {
    return -1 != Find(el);
}

// returns position of removed element or -1 if not removed
int Str::Remove(char el) {
    int i = Find(el);
    if (-1 == i) {
        return -1;
    }
    RemoveAt(i);
    return i;
}

void Str::Reverse() const {
    for (size_t i = 0; i < len / 2; i++) {
        std::swap(els[i], els[len - i - 1]);
    }
}

char& Str::FindEl(const std::function<bool(char&)>& check) const {
    for (size_t i = 0; i < len; i++) {
        if (check(els[i])) {
            return els[i];
        }
    }
    return els[len]; // nullptr-sentinel
}

bool Str::IsEmpty() const {
    return len == 0;
}

std::string_view Str::AsView() const {
    return {Get(), size()};
}

ByteSlice Str::AsSpan() const {
    return {(u8*)Get(), size()};
}

ByteSlice Str::AsByteSlice() const {
    return {(u8*)Get(), size()};
}

std::string_view Str::StealAsView() {
    size_t len = size();
    char* d = StealData();
    return {d, len};
}

bool Str::AppendChar(char c) {
    return InsertAt(len, c);
}

bool Str::Append(const u8* src, size_t size) {
    return this->Append((const char*)src, size);
}

bool Str::AppendView(const std::string_view sv) {
    if (sv.empty()) {
        return true;
    }
    return this->Append(sv.data(), sv.size());
}

bool Str::AppendSpan(ByteSlice d) {
    if (d.empty()) {
        return true;
    }
    return this->Append(d.data(), d.size());
}

void Str::AppendFmt(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* res = FmtV(fmt, args);
    AppendAndFree(res);
    va_end(args);
}

bool Str::AppendAndFree(const char* s) {
    if (!s) {
        return true;
    }
    bool ok = Append(s, str::Len(s));
    str::Free(s);
    return ok;
}

// returns true if was replaced
bool Replace(Str& s, const char* toReplace, const char* replaceWith) {
    // fast path: nothing to replace
    if (!str::Find(s.els, toReplace)) {
        return false;
    }
    char* newStr = str::Replace(s.els, toReplace, replaceWith);
    s.Reset();
    s.AppendAndFree(newStr);
    return true;
}

void Str::Set(std::string_view sv) {
    Reset();
    AppendView(sv);
}

char* Str::Get() const {
    return els;
}

char Str::LastChar() const {
    auto n = this->len;
    if (n == 0) {
        return 0;
    }
    return at(n - 1);
}

// WStr

static WCHAR* EnsureCap(WStr* s, size_t needed) {
    if (needed + kPadding <= Str::kBufChars) {
        s->els = s->buf; // TODO: not needed?
        return s->buf;
    }

    size_t capacityHint = s->cap;
    // tricky: to save sapce we reuse cap for capacityHint
    if (!s->els || (s->els == s->buf)) {
        // on first expand cap might be capacityHint
        s->cap = 0;
    }

    if (s->cap >= needed) {
        return s->els;
    }

    size_t newCap = s->cap * 2;
    if (needed > newCap) {
        newCap = needed;
    }
    if (newCap < capacityHint) {
        newCap = capacityHint;
    }

    size_t newElCount = newCap + kPadding;

    size_t allocSize = newElCount * WStr::kElSize;
    WCHAR* newEls;
    if (s->buf == s->els) {
        newEls = (WCHAR*)Allocator::Alloc(s->allocator, allocSize);
        if (newEls) {
            memcpy(newEls, s->buf, WStr::kElSize * (s->len + 1));
        }
    } else {
        newEls = (WCHAR*)Allocator::Realloc(s->allocator, s->els, allocSize);
    }

    if (!newEls) {
        CrashAlwaysIf(gAllowAllocFailure.load() == 0);
        return nullptr;
    }
    s->els = newEls;
    s->cap = (u32)newCap;
    return newEls;
}

static WCHAR* MakeSpaceAt(WStr* s, size_t idx, size_t count) {
    CrashIf(count == 0);
    u32 newLen = std::max(s->len, (u32)idx) + (u32)count;
    WCHAR* buf = EnsureCap(s, newLen);
    if (!buf) {
        return nullptr;
    }
    buf[newLen] = 0;
    WCHAR* res = &(buf[idx]);
    if (s->len > idx) {
        WCHAR* src = buf + idx;
        WCHAR* dst = buf + idx + count;
        memmove(dst, src, (s->len - idx) * WStr::kElSize);
    }
    s->len = newLen;
    return res;
}

static void Free(WStr* s) {
    if (!s->els || (s->els == s->buf)) {
        return;
    }
    Allocator::Free(s->allocator, s->els);
    s->els = nullptr;
}

void WStr::Reset() {
    Free(this);
    len = 0;
    cap = 0;
    els = buf;

#if defined(DEBUG)
#define kFillerWStr L"01234567890123456789012345678901"
    // to catch mistakes earlier, fill the buffer with a known string
    constexpr size_t nFiller = sizeof(kFillerStr) - 1;
    static_assert(nFiller == Str::kBufChars);
    memcpy(buf, kFillerWStr, nFiller * kElSize);
#endif

    buf[0] = 0;
}

// allocator is not owned by Vec and must outlive it
WStr::WStr(size_t capHint, Allocator* a) {
    allocator = a;
    Reset();
    cap = (u32)(capHint + kPadding); // + kPadding for terminating 0
}

// ensure that a Vec never shares its els buffer with another after a clone/copy
// note: we don't inherit allocator as it's not needed for our use cases
WStr::WStr(const WStr& that) {
    Reset();
    WCHAR* s = EnsureCap(this, that.cap);
    WCHAR* sOrig = that.Get();
    len = that.len;
    size_t n = (len + kPadding) * kElSize;
    memcpy(s, sOrig, n);
}

WStr::WStr(std::wstring_view s) {
    Reset();
    AppendView(s);
}

WStr::WStr(const WCHAR* s) {
    Reset();
    std::wstring_view ws{s};
    AppendView(ws);
}

WStr& WStr::operator=(const WStr& that) {
    if (this == &that) {
        return *this;
    }
    Reset();
    WCHAR* s = EnsureCap(this, that.cap);
    WCHAR* sOrig = that.Get();
    len = that.len;
    size_t n = (len + kPadding) * kElSize;
    memcpy(s, sOrig, n);
    return *this;
}

WStr::~WStr() {
    Free(this);
}

WCHAR& WStr::at(size_t idx) const {
    CrashIf(idx >= len);
    return els[idx];
}

WCHAR& WStr::at(int idx) const {
    CrashIf(idx < 0);
    return at((size_t)idx);
}

WCHAR& WStr::operator[](size_t idx) const {
    return at(idx);
}

WCHAR& WStr::operator[](long idx) const {
    CrashIf(idx < 0);
    return at((size_t)idx);
}

WCHAR& WStr::operator[](ULONG idx) const {
    return at((size_t)idx);
}

WCHAR& WStr::operator[](int idx) const {
    CrashIf(idx < 0);
    return at((size_t)idx);
}

#if defined(_WIN64)
WCHAR& WStr::at(u32 idx) const {
    return at((size_t)idx);
}

WCHAR& WStr::operator[](u32 idx) const {
    return at((size_t)idx);
}
#endif

size_t WStr::size() const {
    return len;
}
int WStr::isize() const {
    return (int)len;
}

bool WStr::InsertAt(size_t idx, const WCHAR& el) {
    WCHAR* p = MakeSpaceAt(this, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool WStr::Append(const WCHAR& el) {
    return InsertAt(len, el);
}

bool WStr::Append(const WCHAR* src, size_t count) {
    if (-1 == count) {
        count = str::Len(src);
    }
    if (!src || 0 == count) {
        return true;
    }
    WCHAR* dst = MakeSpaceAt(this, len, count);
    if (!dst) {
        return false;
    }
    memcpy(dst, src, count * kElSize);
    return true;
}

WCHAR WStr::RemoveAt(size_t idx, size_t count) {
    WCHAR res = at(idx);
    if (len > idx + count) {
        WCHAR* dst = els + idx;
        WCHAR* src = els + idx + count;
        memmove(dst, src, (len - idx - count) * kElSize);
    }
    len -= (u32)count;
    memset(els + len, 0, count * kElSize);
    return res;
}

WCHAR WStr::RemoveLast() {
    if (len == 0) {
        return 0;
    }
    return RemoveAt(len - 1);
}

WCHAR& WStr::Last() const {
    CrashIf(0 == len);
    return at(len - 1);
}

// perf hack for using as a buffer: client can get accumulated data
// without duplicate allocation. Note: since Vec over-allocates, this
// is likely to use more memory than strictly necessary, but in most cases
// it doesn't matter
WCHAR* WStr::StealData() {
    WCHAR* res = els;
    if (els == buf) {
        res = (WCHAR*)Allocator::MemDup(allocator, buf, (len + kPadding) * kElSize);
    }
    els = buf;
    Reset();
    return res;
}

WCHAR* WStr::LendData() const {
    return els;
}

int WStr::Find(const WCHAR& el, size_t startAt) const {
    for (size_t i = startAt; i < len; i++) {
        if (els[i] == el) {
            return (int)i;
        }
    }
    return -1;
}

bool WStr::Contains(const WCHAR& el) const {
    return -1 != Find(el);
}

// returns position of removed element or -1 if not removed
int WStr::Remove(const WCHAR& el) {
    int i = Find(el);
    if (-1 == i) {
        return -1;
    }
    RemoveAt(i);
    return i;
}

void WStr::Reverse() const {
    for (size_t i = 0; i < len / 2; i++) {
        std::swap(els[i], els[len - i - 1]);
    }
}

WCHAR& WStr::FindEl(const std::function<bool(WCHAR&)>& check) const {
    for (size_t i = 0; i < len; i++) {
        if (check(els[i])) {
            return els[i];
        }
    }
    return els[len]; // nullptr-sentinel
}

bool WStr::IsEmpty() const {
    return len == 0;
}

std::wstring_view WStr::AsView() const {
    return {Get(), size()};
}

std::span<WCHAR> WStr::AsSpan() const {
    return {Get(), size()};
}

std::wstring_view WStr::StealAsView() {
    size_t len = size();
    WCHAR* d = StealData();
    return {d, len};
}

bool WStr::AppendChar(WCHAR c) {
    return InsertAt(len, c);
}

bool WStr::AppendView(const std::wstring_view sv) {
    if (sv.empty()) {
        return true;
    }
    return this->Append(sv.data(), sv.size());
}

bool WStr::AppendSpan(std::span<WCHAR> d) {
    if (d.empty()) {
        return true;
    }
    return this->Append(d.data(), d.size());
}

void WStr::AppendFmt(const WCHAR* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WCHAR* res = FmtV(fmt, args);
    AppendAndFree(res);
    va_end(args);
}

bool WStr::AppendAndFree(const WCHAR* s) {
    if (!s) {
        return true;
    }
    bool ok = Append(s, str::Len(s));
    str::Free(s);
    return ok;
}

// returns true if was replaced
bool Replace(WStr& s, const WCHAR* toReplace, const WCHAR* replaceWith) {
    // fast path: nothing to replace
    if (!str::Find(s.els, toReplace)) {
        return false;
    }
    WCHAR* newStr = str::Replace(s.els, toReplace, replaceWith);
    s.Reset();
    s.AppendAndFree(newStr);
    return true;
}

void WStr::Set(std::wstring_view sv) {
    Reset();
    AppendView(sv);
}

WCHAR* WStr::Get() const {
    return els;
}

WCHAR WStr::LastChar() const {
    auto n = this->len;
    if (n == 0) {
        return 0;
    }
    return at(n - 1);
}

} // namespace str

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

// return true if s1 == s2, case sensitive
bool Eq(const WCHAR* s1, const WCHAR* s2) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == wcscmp(s1, s2);
}

// return true if s1 == s2, case insensitive
bool EqI(const WCHAR* s1, const WCHAR* s2) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == _wcsicmp(s1, s2);
}

// compares two strings ignoring case and whitespace
bool EqIS(const WCHAR* s1, const WCHAR* s2) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }

    while (*s1 && *s2) {
        // skip whitespace
        for (; IsWs(*s1); s1++) {
            // do nothing
        }
        for (; IsWs(*s2); s2++) {
            // do nothing
        }

        if (towlower(*s1) != towlower(*s2)) {
            return false;
        }
        if (*s1) {
            s1++;
            s2++;
        }
    }

    return !*s1 && !*s2;
}

bool EqN(const WCHAR* s1, const WCHAR* s2, size_t len) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == wcsncmp(s1, s2, len);
}

bool EqNI(const WCHAR* s1, const WCHAR* s2, size_t len) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == _wcsnicmp(s1, s2, len);
}

bool IsEmpty(const WCHAR* s) {
    return !s || (0 == *s);
}

bool StartsWith(const WCHAR* str, const WCHAR* prefix) {
    return EqN(str, prefix, Len(prefix));
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(const WCHAR* str, const WCHAR* prefix) {
    if (str == prefix) {
        return true;
    }
    if (!str || !prefix) {
        return false;
    }
    return 0 == _wcsnicmp(str, prefix, str::Len(prefix));
}

bool EndsWith(const WCHAR* txt, const WCHAR* end) {
    if (!txt || !end) {
        return false;
    }
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen) {
        return false;
    }
    return str::Eq(txt + txtLen - endLen, end);
}

bool EndsWithI(const WCHAR* txt, const WCHAR* end) {
    if (!txt || !end) {
        return false;
    }
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen) {
        return false;
    }
    return str::EqI(txt + txtLen - endLen, end);
}

const WCHAR* FindChar(const WCHAR* str, WCHAR c) {
    return (const WCHAR*)wcschr(str, c);
}

WCHAR* FindChar(WCHAR* str, WCHAR c) {
    return (WCHAR*)wcschr(str, c);
}

const WCHAR* FindCharLast(const WCHAR* str, WCHAR c) {
    return (const WCHAR*)wcsrchr(str, c);
}

WCHAR* FindCharLast(WCHAR* str, const WCHAR c) {
    return wcsrchr(str, c);
}

const WCHAR* Find(const WCHAR* str, const WCHAR* find) {
    return wcsstr(str, find);
}

const WCHAR* FindI(const WCHAR* s, const WCHAR* toFind) {
    if (!s || !toFind) {
        return nullptr;
    }

    WCHAR first = towlower(*toFind);
    if (!first) {
        return s;
    }
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
    return (count >= 0) && ((size_t)count < bufCchSize);
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
        if ((count >= 0) && ((size_t)count < bufCchSize)) {
            break;
        }
        // always grow the buffer exponentially (cf. TODO above)
        if (buf != message) {
            free(buf);
        }
        bufCchSize = bufCchSize / 2 * 3;
        buf = AllocArray<WCHAR>(bufCchSize);
        if (!buf) {
            break;
        }
    }
    if (buf == message) {
        buf = str::Dup(message);
    }

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
size_t TrimWSInPlace(WCHAR* s, TrimOpt opt) {
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

size_t TransCharsInPlace(WCHAR* str, const WCHAR* oldChars, const WCHAR* newChars) {
    size_t nReplaced = 0;

    for (WCHAR* c = str; *c; c++) {
        const WCHAR* pos = str::FindChar(oldChars, *c);
        if (pos) {
            size_t idx = pos - oldChars;
            *c = newChars[idx];
            nReplaced++;
        }
    }

    return nReplaced;
}

// free() the result
WCHAR* Replace(const WCHAR* s, const WCHAR* toReplace, const WCHAR* replaceWith) {
    if (!s || str::IsEmpty(toReplace) || !replaceWith) {
        return nullptr;
    }

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
size_t NormalizeWSInPlace(WCHAR* str) {
    if (!str) {
        return 0;
    }
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

    if (dst > str && IsWs(*(dst - 1))) {
        dst--;
    }
    *dst = '\0';

    return src - dst;
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
    if (currDstCchLen + 1 >= dstCchSize) {
        return 0;
    }
    size_t left = dstCchSize - currDstCchLen - 1;
    size_t srcCchSize = str::Len(s);
    size_t toCopy = std::min(left, srcCchSize);

    errno_t err = wcsncat_s(dst, dstCchSize, s, toCopy);
    CrashIf(err || dst[currDstCchLen + toCopy] != '\0');

    return toCopy;
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
WCHAR* FormatNumWithThousandSep(i64 num, LCID locale) {
    WCHAR thousandSep[4] = {0};
    if (!GetLocaleInfo(locale, LOCALE_STHOUSAND, thousandSep, dimof(thousandSep))) {
        str::BufSet(thousandSep, dimof(thousandSep), L",");
    }
    AutoFreeWstr buf(str::Format(L"%Iu", (size_t)num));

    size_t resLen = str::Len(buf) + str::Len(thousandSep) * (str::Len(buf) + 3) / 3 + 1;
    WCHAR* res = AllocArray<WCHAR>(resLen);
    if (!res) {
        return nullptr;
    }
    WCHAR* next = res;
    int i = 3 - (str::Len(buf) % 3);
    for (const WCHAR* src = buf; *src;) {
        *next++ = *src++;
        if (*src && i == 2) {
            next += str::BufSet(next, resLen - (next - res), thousandSep);
        }
        i = (i + 1) % 3;
    }
    *next = '\0';

    return res;
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
WCHAR* FormatFloatWithThousandSep(double number, LCID locale) {
    i64 num = (i64)(number * 100 + 0.5);

    AutoFreeWstr tmp(FormatNumWithThousandSep(num / 100, locale));
    WCHAR decimal[4];
    if (!GetLocaleInfo(locale, LOCALE_SDECIMAL, decimal, dimof(decimal))) {
        str::BufSet(decimal, dimof(decimal), L".");
    }

    // always add between one and two decimals after the point
    AutoFreeWstr buf(str::Format(L"%s%s%02d", tmp.Get(), decimal, (int)(num % 100)));
    if (str::EndsWith(buf, L"0")) {
        buf[str::Len(buf) - 1] = '\0';
    }

    return buf.StealData();
}

// cf. http://rosettacode.org/wiki/Roman_numerals/Encode#C.2B.2B
WCHAR* FormatRomanNumeral(int number) {
    if (number < 1) {
        return nullptr;
    }

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
    CrashIf(len == 0);

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
        if (!*a && !*b) {
            return wcscmp(aStart, bStart);
        }
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
                if (!str::IsDigit(*a)) {
                    return -1;
                }
                if (!str::IsDigit(*b)) {
                    return 1;
                }
                // remember the difference for when the numbers are of the same magnitude
                if (0 == diff) {
                    diff = *a - *b;
                }
            }
            // neither *a nor *b is a digit, so continue with them (unless diff != 0)
            a--;
            b--;
        }
        // sort letters case-insensitively
        else if (iswalnum(*a) && iswalnum(*b)) {
            diff = towlower(*a) - towlower(*b);
            // sort special characters before text and numbers
        } else if (iswalnum(*a)) {
            return 1;
        } else if (iswalnum(*b)) {
            return -1;
            // sort special characters by ASCII code
        } else {
            diff = *a - *b;
        }
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
        if (end && !*end) {
            *endOut = str + width;
        }
    }
    return endF;
}

static WCHAR* ExtractUntil(const WCHAR* pos, WCHAR c, const WCHAR** endOut) {
    *endOut = FindChar(pos, c);
    if (!*endOut) {
        return nullptr;
    }
    return str::Dup(pos, *endOut - pos);
}

const WCHAR* Parse(const WCHAR* str, const WCHAR* format, ...) {
    if (!str) {
        return nullptr;
    }
    va_list args;
    va_start(args, format);
    for (const WCHAR* f = format; *f; f++) {
        if (*f != '%') {
            if (*f != *str) {
                goto Failure;
            }
            str++;
            continue;
        }
        f++;

        const WCHAR* end = nullptr;
        if ('u' == *f) {
            *va_arg(args, unsigned int*) = wcstoul(str, (WCHAR**)&end, 10);
        } else if ('d' == *f) {
            *va_arg(args, int*) = wcstol(str, (WCHAR**)&end, 10);
        } else if ('x' == *f) {
            *va_arg(args, unsigned int*) = wcstoul(str, (WCHAR**)&end, 16);
        } else if ('f' == *f) {
            *va_arg(args, float*) = (float)wcstod(str, (WCHAR**)&end);
        } else if ('c' == *f) {
            *va_arg(args, WCHAR*) = *str, end = str + 1;
        } else if ('s' == *f) {
            *va_arg(args, WCHAR**) = ExtractUntil(str, *(f + 1), &end);
        } else if ('S' == *f) {
            va_arg(args, AutoFreeWstr*)->Set(ExtractUntil(str, *(f + 1), &end));
        } else if ('$' == *f && !*str) {
            continue; // don't fail, if we're indeed at the end of the string
        } else if ('%' == *f && *f == *str) {
            end = str + 1;
        } else if (' ' == *f && str::IsWs(*str)) {
            end = str + 1;
        } else if ('_' == *f) {
            if (!str::IsWs(*str)) {
                continue; // don't fail, if there's no whitespace at all
            }
            for (end = str + 1; str::IsWs(*end); end++) {
                // do nothing
            }
        } else if ('?' == *f && *(f + 1)) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            if (*str != *++f) {
                continue;
            }
            end = str + 1;
        } else if (str::IsDigit(*f)) {
            f = ParseLimitedNumber(str, f, &end, va_arg(args, void*)) - 1;
        }
        if (!end || end == str) {
            goto Failure;
        }
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
    auto urlA(ToUtf8Temp(url));
    DecodeInPlace(urlA.Get());
    // convert back in place
    CrashIf(str::Len(url) >= INT_MAX);
    MultiByteToWideChar(CP_UTF8, 0, urlA.Get(), -1, url, (int)str::Len(url) + 1);
}

WCHAR* GetFullPath(const WCHAR* url) {
    WCHAR* path = str::Dup(url);
    str::TransCharsInPlace(path, L"#?", L"\0\0");
    DecodeInPlace(path);
    return path;
}

WCHAR* GetFileName(const WCHAR* url) {
    AutoFreeWstr path(str::Dup(url));
    str::TransCharsInPlace(path, L"#?", L"\0\0");
    WCHAR* base = path + str::Len(path);
    for (; base > path; base--) {
        if ('/' == base[-1] || '\\' == base[-1]) {
            break;
        }
    }
    if (str::IsEmpty(base)) {
        return nullptr;
    }
    DecodeInPlace(base);
    return str::Dup(base);
}

} // namespace url

namespace seqstrings {

// advance to next string
// return nullptr if end of strings
const WCHAR* SkipStr(const WCHAR* s) {
    // empty string marks the end, means idx was too high
    if (!*s) {
        return nullptr;
    }
    // skip past next '\0' char
    while (*s) {
        s++;
    }
    return s + 1;
}

// Returns nullptr if s is the same as toFind
// If they are not equal, returns end of s + 1
static inline const char* StrEqWeird(const char* s, const WCHAR* toFind) {
    WCHAR wc;
    char c, c2;
    for (;;) {
        c = *s++;
        if (0 == c) {
            if (0 == *toFind) {
                return nullptr;
            }
            return s;
        }
        wc = *toFind++;
        if (wc > 255) {
            return nullptr;
        }
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
int StrToIdx(const char* strs, const WCHAR* toFind) {
    const char* s = strs;
    int idx = 0;
    while (*s) {
        s = StrEqWeird(s, toFind);
        if (nullptr == s) {
            return idx;
        }
        ++idx;
    }
    return -1;
}

// Given an index in the "array" of sequentially laid out strings,
// returns a strings at that index.
const WCHAR* IdxToStr(const WCHAR* strs, int idx) {
    CrashIf(idx < 0);
    const WCHAR* s = strs;
    while (idx > 0) {
        s = SkipStr(s);
        if (!s) {
            return nullptr;
        }
        --idx;
    }
    return s;
}

} // namespace seqstrings

ByteSlice ToSpanU8(std::string_view sv) {
    return {(u8*)sv.data(), sv.size()};
}

int VecStrIndex::ItemsLeft() const {
    return kVecStrIndexSize - nStrings;
}

int VecStr::Size() const {
    VecStrIndex* idx = firstIndex;
    int n = 0;
    while (idx) {
        n += idx->nStrings;
        idx = idx->next;
    }
    return n;
}

std::string_view VecStr::at(int i) const {
    CrashIf(i < 0);
    VecStrIndex* idx = firstIndex;
    while (idx) {
        if (idx->nStrings > i) {
            break;
        }
        i -= idx->nStrings;
        idx = idx->next;
    }
    if (idx == nullptr) {
        CrashMe();
        return {};
    }
    CrashIf(idx->nStrings <= i);
    if (idx->nStrings <= i) {
        return {};
    }
    const char* s = (const char*)idx->offsets[i];
    i32 size = idx->sizes[i];
    return {s, (size_t)size};
}

static bool allocateIndexIfNeeded(VecStr& v) {
    if (v.currIndex && v.currIndex->ItemsLeft() > 0) {
        return true;
    }

    VecStrIndex* idx = v.allocator.AllocStruct<VecStrIndex>();

    if (!idx) {
        CrashAlwaysIf(gAllowAllocFailure.load() == 0);
        return false;
    }

    idx->next = nullptr;
    idx->nStrings = 0;

    if (!v.firstIndex) {
        v.firstIndex = idx;
        v.currIndex = idx;
    } else {
        CrashIf(!v.firstIndex);
        CrashIf(!v.currIndex);
        v.currIndex->next = idx;
        v.currIndex = idx;
    }
    return true;
}

bool VecStr::Append(std::string_view sv) {
    bool ok = allocateIndexIfNeeded(*this);
    if (!ok) {
        return false;
    }
    constexpr size_t maxLen = (size_t)std::numeric_limits<i32>::max();
    CrashIf(sv.size() > maxLen);
    if (sv.size() > maxLen) {
        return false;
    }
    std::string_view res = str::Dup(&allocator, sv);

    int n = currIndex->nStrings;
    currIndex->offsets[n] = (char*)res.data();
    currIndex->sizes[n] = (i32)res.size();
    currIndex->nStrings++;
    return true;
}

void VecStr::Reset() {
    allocator.Reset();
    firstIndex = nullptr;
    currIndex = nullptr;
}

StrVec::StrVec() {
    // add one character so that we can use index 0 as "not found"
    str.AppendChar(0);
}

void StrVec::Reset() {
    str.Reset();
    str.AppendChar(0);
    index.Reset();
}

void StrVec::Append(const char* s) {
    u32 idx = kNullIdx;
    size_t sLen;
    if (s == nullptr) {
        goto Append;
    }
    sLen = str::Len(s);
    idx = (u32)str.size();
    str.Append(s, sLen + 1);
Append:
    index.Append(idx);
}

int StrVec::size() {
    return index.isize();
}

std::string_view StrVec::at(int idx) {
    int n = size();
    ReportIf(idx < 0 || idx >= n);
    u32 start = index.at(idx);
    u32 end = (u32)str.size();
    if (idx + 1 < n) {
        end = (u32)index.at(idx + 1);
    }
    const char* s = str.LendData() + start;
    size_t len = (size_t)(end - start - 1);
    return {s, len};
}
