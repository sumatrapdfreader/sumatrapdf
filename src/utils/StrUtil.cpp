/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "StrFormat.h"

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

bool IsEqual(const ByteSlice& d1, const ByteSlice& d2) {
    if (d1.sz != d2.sz) {
        return false;
    }
    if (d1.sz == 0) {
        return true;
    }
    CrashIf(!d1.d || !d2.d);
    int res = memcmp(d1.d, d2.d, d1.sz);
    return res == 0;
}

namespace str {

size_t Len(const char* s) {
    return s ? strlen(s) : 0;
}

int Leni(const char* s) {
    return s ? (int)strlen(s) : 0;
}

size_t Len(const WCHAR* s) {
    return s ? wcslen(s) : 0;
}

int Leni(const WCHAR* s) {
    return s ? (int)wcslen(s) : 0;
}

void Free(const char* s) {
    free((void*)s);
}

void Free(const u8* s) {
    free((void*)s);
}

void Free(const WCHAR* s) {
    free((void*)s);
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
    if (!s) {
        return nullptr;
    }
    if (cch == (size_t)-1) {
        cch = str::Len(s);
    }
    return (char*)Allocator::MemDup(a, s, cch * sizeof(char), sizeof(char));
}

char* Dup(const char* s, size_t cch) {
    return Dup(nullptr, s, cch);
}

char* Dup(const ByteSlice& d) {
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

bool Eq(const ByteSlice& sp1, const ByteSlice& sp2) {
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

bool Contains(const char* s, const char* txt) {
    const char* p = str::Find(s, txt);
    bool contains = p != nullptr;
    return contains;
}

bool ContainsI(const char* s, const char* txt) {
    const char* p = str::FindI(s, txt);
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

void ReplaceWithCopy(const char** s, const ByteSlice& d) {
    if (*s != (const char*)d.data()) {
        str::Free(*s);
        *s = str::Dup((const char*)d.data(), d.size());
    }
}

void ReplaceWithCopy(char** s, const char* snew) {
    if (*s != snew) {
        str::Free(*s);
        *s = str::Dup(snew);
    }
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

char* Join(Allocator* allocator, const char* s1, const char* s2, const char* s3) {
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
    return Join(nullptr, s1, s2, s3);
}

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
WCHAR* Join(Allocator* allocator, const WCHAR* s1, const WCHAR* s2, const WCHAR* s3) {
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
    return Join(nullptr, s1, s2, s3);
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

// format string to a buffer provided by the caller
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
char* FmtVWithAllocator(Allocator* a, const char* fmt, va_list args) {
    char message[256]{};
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
        buf = Allocator::AllocArray<char>(a, bufCchSize);
        if (!buf) {
            break;
        }
    }

    if (buf == message) {
        buf = str::Dup(a, message);
    }

    return buf;
}

char* FmtV(const char* fmt, va_list args) {
    return FmtVWithAllocator(nullptr, fmt, args);
}

// caller needs to str::Free()
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
        str::BufSet(limited, std::min((int)width + 1, dimofi(limited)), str);
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
    char buf[128]{};
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
            for (; a && IsWs(*a); a++) {
                // do nothing
            }
            for (; b && IsWs(*b); b++) {
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

bool EmptyOrWhiteSpaceOnly(const char* s) {
    if (!s || !*s) {
        return true;
    }
    while (*s) {
        char c = *s++;
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

void Next(const char*& s, int& idx) {
    if (!s || !*s || idx < 0) {
        s = nullptr;
        idx = -1;
        return;
    }
    while (*s) {
        s++;
    }
    s++; // skip terminating 0
    if (!*s) {
        s = nullptr;
        return;
    }
    idx++;
}

void Next(const char*& s) {
    int idxDummy = 0;
    Next(s, idxDummy);
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
int StrToIdx(SeqStrings strs, const char* toFind) {
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
int StrToIdxIS(SeqStrings strs, const char* toFind) {
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
const char* IdxToStr(SeqStrings strs, int idx) {
    CrashIf(idx < 0);
    const char* s = strs;
    while (idx > 0) {
        Next(s);
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
        CrashAlwaysIf(InterlockedExchangeAdd(&gAllowAllocFailure, 0) == 0);
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

bool Str::AppendChar(char c) {
    return InsertAt(len, c);
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

bool Str::Append(const Str& s) {
    return Append(s.LendData(), s.size());
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

// TODO: rewrite as size_t Find(const char* s, size_t sLen, size_t start);
bool Str::Contains(const char* s, size_t sLen) {
    if (str::IsEmpty(s)) {
        return false;
    }
    if (sLen == 0) {
        sLen = str::Len(s);
    }
    if (sLen > len) {
        return false;
    }
    // must account for possibility of 0 in the string
    const char* curr = LendData();
    int nLeft = (int)(len - sLen);
    char c = *s;
    char c2;
    while (nLeft >= 0) {
        c2 = *curr++;
        nLeft--;
        if (c != c2) {
            continue;
        }
        if (str::EqN(s, curr - 1, sLen)) {
            return true;
        }
    }
    return false;
}

bool Str::IsEmpty() const {
    return len == 0;
}

ByteSlice Str::AsByteSlice() const {
    return {(u8*)Get(), size()};
}

ByteSlice Str::StealAsByteSlice() {
    size_t n = size();
    char* d = StealData();
    return {(u8*)d, n};
}

bool Str::Append(const u8* src, size_t size) {
    return this->Append((const char*)src, size);
}

bool Str::AppendSlice(const ByteSlice& d) {
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

#if 0
// returns true if was replaced
bool Replace(Str& s, const char* toReplace, const char* replaceWith) {
    // fast path: nothing to replace
    if (!str::Find(s.els, toReplace)) {
        return false;
    }
    char* newStr = str::ReplaceTemp(s.els, toReplace, replaceWith);
    s.Reset();
    s.Append(newStr);
    return true;
}
#endif

void Str::Set(const char* s) {
    Reset();
    Append(s);
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
        CrashAlwaysIf(InterlockedExchangeAdd(&gAllowAllocFailure, 0) == 0);
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

WStr::WStr(const WCHAR* s) {
    Reset();
    Append(s);
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

bool WStr::AppendChar(WCHAR c) {
    return InsertAt(len, c);
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

void WStr::Set(const WCHAR* s) {
    Reset();
    Append(s);
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

char* ToUpperInPlace(char* s) {
    char* res = s;
    for (; s && *s; s++) {
        *s = (char)toupper(*s);
    }
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

// Note: BufSet() should only be used when absolutely necessary (e.g. when
// handling buffers in OS-defined structures)
// returns the number of characters written (without the terminating \0)
int BufSet(char* dst, int cchDst, const char* src) {
    CrashAlwaysIf(0 == cchDst || !dst);
    if (!src) {
        *dst = 0;
        return 0;
    }

    int srcCchSize = (int)str::Len(src);
    int toCopy = std::min(cchDst - 1, srcCchSize);

    errno_t err = strncpy_s(dst, (size_t)cchDst, src, (size_t)toCopy);
    CrashIf(err || dst[toCopy] != '\0');

    return toCopy;
}

int BufSet(WCHAR* dst, int cchDst, const WCHAR* src) {
    CrashAlwaysIf(0 == cchDst || !dst);
    if (!src) {
        *dst = 0;
        return 0;
    }

    int srcCchSize = str::Leni(src);
    int toCopy = std::min(cchDst - 1, srcCchSize);

    memset(dst, 0, cchDst * sizeof(WCHAR));
    memcpy(dst, src, toCopy * sizeof(WCHAR));
    return toCopy;
}

int BufSet(WCHAR* dst, int dstCchSize, const char* src) {
    return BufSet(dst, dstCchSize, ToWStrTemp(src));
}

int BufAppend(WCHAR* dst, int cchDst, const WCHAR* s) {
    CrashAlwaysIf(0 == cchDst);

    int currDstCchLen = str::Leni(dst);
    if (currDstCchLen + 1 >= cchDst) {
        return 0;
    }
    int left = cchDst - currDstCchLen - 1;
    int srcCchSize = str::Leni(s);
    int toCopy = std::min(left, srcCchSize);

    errno_t err = wcsncat_s(dst, cchDst, s, toCopy);
    CrashIf(err || dst[currDstCchLen + toCopy] != '\0');

    return toCopy;
}

// append as much of s at the end of dst (which must be properly null-terminated)
// as will fit.
int BufAppend(char* dst, int dstCch, const char* s) {
    CrashAlwaysIf(0 == dstCch);

    int currDstCchLen = str::Leni(dst);
    if (currDstCchLen + 1 >= dstCch) {
        return 0;
    }
    int left = dstCch - currDstCchLen - 1;
    int srcCchSize = str::Leni(s);
    int toCopy = std::min(left, srcCchSize);

    errno_t err = strncat_s(dst, dstCch, s, toCopy);
    CrashIf(err || dst[currDstCchLen + toCopy] != '\0');

    return toCopy;
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale) {
    WCHAR thousandSepW[4]{};
    if (!GetLocaleInfoW(locale, LOCALE_STHOUSAND, thousandSepW, dimof(thousandSepW))) {
        str::BufSet(thousandSepW, dimof(thousandSepW), ",");
    }
    char* thousandSep = ToUtf8Temp(thousandSepW);
    char* buf = fmt::FormatTemp("%d", num);

    char res[128] = {0};
    int resLen = dimof(res);
    char* next = res;
    int i = 3 - (str::Len(buf) % 3);
    for (const char* src = buf; *src;) {
        *next++ = *src++;
        if (*src && i == 2) {
            next += str::BufSet(next, resLen - (int)(next - res), thousandSep);
        }
        i = (i + 1) % 3;
    }
    *next = '\0';

    return str::DupTemp(res);
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
TempStr FormatFloatWithThousandSepTemp(double number, LCID locale) {
    i64 num = (i64)(number * 100 + 0.5);

    char* tmp = FormatNumWithThousandSepTemp(num / 100, locale);
    WCHAR decimalW[4] = {0};
    if (!GetLocaleInfoW(locale, LOCALE_SDECIMAL, decimalW, dimof(decimalW))) {
        decimalW[0] = '.';
        decimalW[1] = 0;
    }
    char decimal[4];
    int i = 0;
    for (WCHAR c : decimalW) {
        decimal[i++] = (char)c;
    }

    // add between one and two decimals after the point
    char* buf = fmt::FormatTemp("%s%s%02d", tmp, decimal, num % 100);
    if (str::EndsWith(buf, "0")) {
        buf[str::Len(buf) - 1] = '\0';
    }

    return buf;
}

// http://rosettacode.org/wiki/Roman_numerals/Encode#C.2B.2B
TempStr FormatRomanNumeralTemp(int n) {
    if (n < 1) {
        return nullptr;
    }

    static struct {
        int value;
        const char* numeral;
    } romandata[] = {{1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"}, {50, "L"},
                     {40, "XL"},  {10, "X"},   {9, "IX"},  {5, "V"},    {4, "IV"},  {1, "I"}};

    str::Str roman;
    for (int i = 0; i < dimof(romandata); i++) {
        auto&& el = romandata[i];
        for (; n >= el.value; n -= el.value) {
            roman.Append(el.numeral);
        }
    }
    return str::DupTemp(roman.Get());
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
            for (; a && IsWs(*a); a++) {
                // do nothing
            }
            for (; b && IsWs(*b); b++) {
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
        str::BufSet(limited, std::min((int)width + 1, dimofi(limited)), str);
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

bool IsAbsolute(const char* url) {
    const char* colon = str::FindChar(url, ':');
    const char* hash = str::FindChar(url, '#');
    return colon && (!hash || hash > colon);
}

char* GetFullPathTemp(const char* url) {
    char* path = str::Dup(url);
    str::TransCharsInPlace(path, "#?", "\0\0");
    DecodeInPlace(path);
    return path;
}

char* GetFileName(const char* url) {
    char* path = str::DupTemp(url);
    str::TransCharsInPlace(path, "#?", "\0\0");
    char* base = path + str::Len(path);
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

//--- StrVec

/*
TODO:
 - StrVecWithData where it associate arbitrary data with each string
 - StrVecWithSubset - has additional index which contains a subset
   of strings which we create by providing a filter function.
   Could be used for efficiently managing strings in
   Command Palette
*/

// represents null string
constexpr u32 kNullIdx = (u32)-2;

void StrVec::Reset() {
    strings.Reset();
    index.Reset();
}

// returns index of inserted string
int StrVec::Append(const char* s, size_t sLen) {
    bool ok;
    if (s == nullptr) {
        ok = index.Append(kNullIdx);
        if (!ok) {
            return -1;
        }
        return Size() - 1;
    }
    if (sLen == 0) {
        sLen = str::Len(s);
    }
    u32 idx = (u32)strings.size();
    ok = strings.Append(s, sLen);
    // ensure to always zero-terminate
    ok &= strings.AppendChar(0);
    if (!ok) {
        return -1;
    }
    ok = index.Append(idx);
    if (!ok) {
        return -1;
    }
    return Size() - 1;
}

int StrVec::AppendIfNotExists(const char* s) {
    if (Contains(s)) {
        return -1;
    }
    return Append(s);
}

bool StrVec::InsertAt(int idx, const char* s) {
    size_t n = str::Len(s);
    u32 strIdx = (u32)strings.size();
    bool ok = strings.Append(s, n + 1); // also append terminating 0
    if (!ok) {
        return false;
    }
    return index.InsertAt(idx, strIdx);
}

void StrVec::SetAt(int idx, const char* s) {
    if (s == nullptr) {
        index[idx] = kNullIdx;
        return;
    }
    size_t n = str::Len(s);
    u32 strIdx = (u32)strings.size();
    bool ok = strings.Append(s, n + 1); // also append terminating 0
    if (!ok) {
        return;
    }
    index[idx] = strIdx;
}

size_t StrVec::size() const {
    return index.size();
}

int StrVec::Size() const {
    return index.isize();
}

char* StrVec::operator[](int idx) const {
    CrashIf(idx < 0);
    return at(idx);
}

char* StrVec::operator[](size_t idx) const {
    CrashIf((int)idx < 0);
    return at((int)idx);
}

char* StrVec::at(int idx) const {
    int n = Size();
    CrashIf(idx < 0 || idx >= n);
    u32 start = index.at(idx);
    if (start == kNullIdx) {
        return nullptr;
    }
    char* s = strings.LendData() + start;
    return s;
}

int StrVec::Find(const char* sv, int startAt) const {
    int n = Size();
    for (int i = startAt; i < n; i++) {
        auto s = at(i);
        if (str::Eq(sv, s)) {
            return i;
        }
    }
    return -1;
}

int StrVec::FindI(const char* sv, int startAt) const {
    int n = Size();
    for (int i = startAt; i < n; i++) {
        auto s = at(i);
        if (str::EqI(sv, s)) {
            return i;
        }
    }
    return -1;
}

bool StrVec::Contains(const char* s) const {
    int idx = Find(s);
    return idx != -1;
}

// TODO: remove, use RemoveAt() instead
char* StrVec::PopAt(int idx) {
    u32 strIdx = index[idx];
    index.RemoveAt(idx);
    char* res = strings.Get() + strIdx;
    return res;
}

// Note: returned string remains valid as long as StrVec is valid
char* StrVec::RemoveAt(int idx) {
    u32 strIdx = index[idx];
    index.RemoveAt(idx);
    char* res = (strIdx == kNullIdx) ? nullptr : strings.Get() + strIdx;
    return res;
}

// Note: returned string remains valid as long as StrVec is valid
char* StrVec::RemoveAtFast(size_t idx) {
    u32 strIdx = index[idx];
    index.RemoveAtFast(idx);
    char* res = (strIdx == kNullIdx) ? nullptr : strings.Get() + strIdx;
    return res;
}

// return true if did remove
bool StrVec::Remove(const char* s) {
    int idx = Find(s);
    if (idx >= 0) {
        RemoveAt(idx);
        return true;
    }
    return false;
}

static bool strLess(const char* s1, const char* s2) {
    if (str::IsEmpty(s1)) {
        if (str::IsEmpty(s2)) {
            return false;
        }
        return true;
    }
    if (str::IsEmpty(s2)) {
        return false;
    }
    int n = strcmp(s1, s2);
    return n < 0;
}

static bool strLessNoCase(const char* s1, const char* s2) {
    if (str::IsEmpty(s1)) {
        // null / empty string is smallest
        if (str::IsEmpty(s2)) {
            return false;
        }
        return true;
    }
    if (str::IsEmpty(s2)) {
        return false;
    }
    int n = _stricmp(s1, s2);
    return n < 0;
}

void StrVec::SortNoCase() {
    Sort(strLessNoCase);
}

static bool strLessNatural(const char* s1, const char* s2) {
    int n = str::CmpNatural(s1, s2);
    return n < 0; // TODO: verify it's < and not >
}

void StrVec::SortNatural() {
    Sort(strLessNatural);
}

void StrVec::Sort(StrLessFunc lessFn) {
    if (lessFn == nullptr) {
        lessFn = strLess;
    }
    std::sort(index.begin(), index.end(), [this, lessFn](u32 i1, u32 i2) -> bool {
        char* is1 = (i1 == kNullIdx) ? nullptr : strings.Get() + i1;
        char* is2 = (i2 == kNullIdx) ? nullptr : strings.Get() + i2;
        bool ret = lessFn(is1, is2);
        return ret;
    });
}

/* splits a string into several substrings, separated by the separator
(optionally collapsing several consecutive separators into one);
e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
(resp. "a", "b", "c" if separators are collapsed) */
size_t Split(StrVec& v, const char* s, const char* separator, bool collapse) {
    int startSize = v.Size();
    const char* next;
    while (true) {
        next = str::Find(s, separator);
        if (!next) {
            break;
        }
        if (!collapse || next > s) {
            v.Append(str::DupTemp(s, next - s));
        }
        s = next + str::Len(separator);
    }
    if (!collapse || *s) {
        v.Append(s);
    }

    return (size_t)(v.Size() - startSize);
}

static int CalcCapForJoin(const StrVec& v, const char* joint) {
    // it's ok to over-estimate
    int len = v.Size();
    size_t jointLen = str::Len(joint);
    int cap = len * (int)jointLen;
    for (int i = 0; i < len; i++) {
        char* s = v.at(i);
        cap += (int)str::Len(s);
    }
    return cap + 32; // arbitrary buffer
}

static char* JoinInner(const StrVec& v, const char* joint, str::Str& res) {
    int len = v.Size();
    size_t jointLen = str::Len(joint);
    int firstForJoint = 0;
    for (int i = 0; i < len; i++) {
        char* s = v.at(i);
        if (!s) {
            firstForJoint++;
            continue;
        }
        if (i > firstForJoint && jointLen > 0) {
            res.Append(joint, jointLen);
        }
        res.Append(s);
    }
    return res.StealData();
}

char* Join(const StrVec& v, const char* joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Str tmp(capHint);
    return JoinInner(v, joint, tmp);
}

TempStr JoinTemp(const StrVec& v, const char* joint) {
    int capHint = CalcCapForJoin(v, joint);
    str::Str tmp(capHint, GetTempAllocator());
    return JoinInner(v, joint, tmp);
}

ByteSlice ToByteSlice(const char* s) {
    size_t n = str::Len(s);
    return {(u8*)s, n};
}
