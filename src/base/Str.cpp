/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Base.h"

#if !defined(_MSC_VER)
#define _strdup strdup
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
// TODO: not sure if that's correct
#define sscanf_s sscanf
#endif

#if defined(_MSC_VER)
static _locale_t GetUtf8FormatLocale() {
    static _locale_t loc = _create_locale(LC_ALL, ".UTF-8");
    return loc;
}
#endif

int str::VsnprintfUtf8(Str buf, const char* fmt, va_list args) {
#if defined(_MSC_VER)
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vsnprintf_l(buf.s, (size_t)buf.len, fmt, loc, args);
    }
#endif
    return vsnprintf(buf.s, (size_t)buf.len, fmt, args);
}

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

static bool isLegalUTF8(const u8* src, int length) {
    u8 a;
    for (int i = 0; i < length; i++) {
        a = src[i];
        if (a == 0) {
            return false;
        }
    }
    const u8* end = src + length;

    switch (length) {
        default:
            return false;
        /* Everything else falls through when "true"... */
        case 4:
            a = (*--end);
            if (a < 0x80 || a > 0xBF) {
                return false;
            }
            __fallthrough;
        case 3:
            a = (*--end);
            if (a < 0x80 || a > 0xBF) {
                return false;
            }
            __fallthrough;
        case 2:
            a = (*--end);
            if (a > 0xBF) {
                return false;
            }

            switch (*src) {
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
            __fallthrough;
        case 1:
            if (*src >= 0x80 && *src < 0xC2) {
                return false;
            }
    }

    return *src <= 0xF4;
}

/* --------------------------------------------------------------------- */

int utf8RuneLen(const u8* s) {
    int n = trailingBytesForUTF8[*s] + 1;
    return n;
}

/*
 * return true if a UTF-8 sequence is legal
 */
bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd) {
    int n = utf8RuneLen(source);
    if (source + n > sourceEnd) {
        return false;
    }
    return isLegalUTF8(source, n);
}

/*
 * return true if UTF-8 string is legal.
 */
bool isLegalUTF8String(const u8** source, const u8* sourceEnd) {
    const u8* s = *source;
    while (s != sourceEnd) {
        int n = utf8RuneLen(s);
        if (n > sourceEnd - s || !isLegalUTF8(s, n)) {
            return false;
        }
        s += n;
    }
    *source = s;
    return true;
}

// return -1 if not a valid utf8 string
int utf8StrLen(const u8* s) {
    int len = 0;
    while (*s) {
        int n = utf8RuneLen(s);
        if (!isLegalUTF8(s, n)) {
            return -1;
        }
        s += n;
        len++;
    }
    return len;
}

// --- end of Unicode, Inc. utf8 code

namespace str {

void Free(Str s) {
    free(s.s);
}

} // namespace str
namespace wstr {

void Free(WStr s) {
    free(s.s);
}

} // namespace wstr
namespace str {

void FreePtr(Str* s) {
    str::Free(*s);
    *s = {};
}

} // namespace str
namespace wstr {

void FreePtr(WStr* s) {
    wstr::Free(*s);
    *s = {};
}

} // namespace wstr
namespace str {

static Str WrapAllocated(char* s, size_t cch = (size_t)-1) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        return Str(s);
    }
    return Str(s, (int)cch);
}

Str Dup(Arena* a, Str s) {
    if (str::IsNull(s) || s.len < 0) {
        return {};
    }
    size_t cch = (size_t)s.len;
    return WrapAllocated((char*)MemDup(a, s.s, cch * sizeof(char), sizeof(char)), cch);
}

Str Dup(Str s) {
    return Dup(nullptr, s);
}

} // namespace str
namespace wstr {

static WStr WrapAllocatedW(WCHAR* s, size_t cch = (size_t)-1) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        return WStr(s);
    }
    return WStr(s, (int)cch);
}

WStr Dup(Arena* a, WStr s) {
    if (wstr::IsNull(s) || s.len < 0) {
        return {};
    }
    size_t cch = (size_t)s.len;
    return WrapAllocatedW((WCHAR*)MemDup(a, s.s, cch * sizeof(WCHAR), sizeof(WCHAR)), cch);
}

WStr Dup(WStr s) {
    return Dup(nullptr, s);
}

} // namespace wstr
namespace str {

// return true if s1 == s2, case sensitive
bool Eq(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    int len1 = 0;
    while (!str::IsNull(s1) && len1 < s1.len && s1.s[len1]) {
        len1++;
    }
    int len2 = 0;
    while (!str::IsNull(s2) && len2 < s2.len && s2.s[len2]) {
        len2++;
    }
    if (len1 != len2) {
        return false;
    }
    if (len1 == 0) {
        return true;
    }
    if (str::IsNull(s1) || str::IsNull(s2)) {
        return false;
    }
    return memeq(s1.s, s2.s, (size_t)len1);
}

// return true if s1 == s2, case insensitive
bool EqI(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (s1.len != s2.len) {
        return false;
    }
    if (s1.len == 0) {
        return true;
    }
    if (str::IsNull(s1) || str::IsNull(s2)) {
        return false;
    }
    return 0 == _strnicmp(s1.s, s2.s, (size_t)s1.len);
}

// compares two strings ignoring case and whitespace
bool EqIS(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }

    int i1 = 0;
    int i2 = 0;
    while (i1 < s1.len && i2 < s2.len) {
        while (i1 < s1.len && IsWs(s1.s[i1])) {
            i1++;
        }
        while (i2 < s2.len && IsWs(s2.s[i2])) {
            i2++;
        }
        if (i1 >= s1.len || i2 >= s2.len) {
            break;
        }
        if (tolower(s1.s[i1]) != tolower(s2.s[i2])) {
            return false;
        }
        i1++;
        i2++;
    }
    while (i1 < s1.len && IsWs(s1.s[i1])) {
        i1++;
    }
    while (i2 < s2.len && IsWs(s2.s[i2])) {
        i2++;
    }
    return i1 >= s1.len && i2 >= s2.len;
}

bool EqN(Str s1, Str s2, size_t len) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2 || len == 0) {
        return len == 0;
    }
    if ((size_t)s1.len < len || (size_t)s2.len < len) {
        return false;
    }
    return memeq(s1.s, s2.s, len);
}

bool EqNI(Str s1, Str s2, size_t len) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2 || len == 0) {
        return len == 0;
    }
    if ((size_t)s1.len < len || (size_t)s2.len < len) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (tolower(s1.s[i]) != tolower(s2.s[i])) {
            return false;
        }
    }
    return true;
}

bool IsNull(const Str& s) {
    return !s.s;
}

bool IsEmpty(Str s) {
    return str::IsNull(s) || s.len == 0 || (0 == *s.s);
}

bool StartsWith(Str s, Str prefix) {
    return EqN(s, prefix, len(prefix));
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(Str s, Str prefix) {
    if (s.s == prefix.s) {
        return true;
    }
    if (!s || !prefix) {
        return false;
    }
    return 0 == _strnicmp(s.s, prefix.s, len(prefix));
}

bool Contains(Str s, Str txt) {
    return str::IndexOf(s, txt) >= 0;
}

bool ContainsI(Str s, Str txt) {
    return str::IndexOfI(s, txt) >= 0;
}

bool EndsWith(Str txt, Str end) {
    if (!txt || !end) {
        return false;
    }
    int txtLen = len(txt);
    int endLen = len(end);
    if (endLen > txtLen) {
        return false;
    }
    return str::Eq(Str(txt.s + txtLen - endLen, endLen), end);
}

bool EndsWithI(Str txt, Str end) {
    if (!txt || !end) {
        return false;
    }
    int txtLen = len(txt);
    int endLen = len(end);
    if (endLen > txtLen) {
        return false;
    }
    return str::EqI(Str(txt.s + txtLen - endLen, endLen), end);
}

bool EqNIx(Str s, int len, Str s2) {
    return ::len(s2) == len && str::StartsWithI(s, s2);
}

// Locale-independent Unicode lowercase folding for case-insensitive matching.
// CharLowerBuffW folds accented / Cyrillic / Greek letters regardless of the
// CRT locale (unlike towlower()), and U+0130 is special-cased to 'i' the same
// way as our full-text search (see TextSearch.cpp's FoldCaseForSearch), so the
// two stay consistent. Folding is 1:1 in WCHAR count, so it doesn't change
// character offsets.
static void FoldCaseForFindW(WStr s) {
    if (!s) {
        return;
    }
    CharLowerBuffW(s.s, (DWORD)s.len);
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == 0x0130) {
            s.s[i] = L'i';
        }
    }
}

// case-insensitive variant of IndexOf: returns the byte offset of the first
// match of toFind in s, or -1 if not found
int IndexOfI(Str s, Str toFind) {
    if (!s || !toFind) {
        return -1;
    }

    if (toFind.len <= 0) {
        return -1;
    }
    char first = (char)tolower(toFind.s[0]);
    if (!first) {
        return -1;
    }

    // Fast path: an ASCII needle can be matched byte-wise against a UTF-8
    // haystack (ASCII bytes never occur inside multi-byte UTF-8 sequences)
    // without any allocation. The Unicode path below is only needed to
    // case-fold a non-ASCII needle (e.g. Cyrillic), so that case-insensitive
    // search works for non-Latin text too (issue #5717).
    bool asciiNeedle = true;
    for (int i = 0; i < toFind.len; i++) {
        if ((u8)toFind.s[i] >= 0x80) {
            asciiNeedle = false;
            break;
        }
    }
    if (asciiNeedle) {
        for (int off = 0; off < s.len && s.s[off]; off++) {
            char c = (char)tolower(s.s[off]);
            if (c == first) {
                if (str::StartsWithI(Str(s.s + off, s.len - off), toFind)) {
                    return off;
                }
            }
        }
        return -1;
    }

    // Unicode path: case-fold both strings (UTF-16) and search, then map the
    // match position back to a byte offset in the original UTF-8 string so the
    // returned offset keeps IndexOfI's contract (an offset into s).
    //
    // Scratch buffers come from the temporary arena; we restore it to its entry
    // position before returning so repeated calls (e.g. the command palette
    // filtering every item) don't grow the arena unbounded.
    ArenaSavepoint sp = ArenaGetSavepoint(GetTempArena());

    TempWStr ws = ToWStrTemp(s); // unfolded, used to map the match back to bytes
    TempWStr wsLo = str::DupTemp(ws);
    TempWStr wfLo = ToWStrTemp(toFind);
    FoldCaseForFindW(wsLo);
    FoldCaseForFindW(wfLo);

    int res = -1;
    int idx = WStrFindSubstr(wsLo, wfLo); // common/str_util.cpp
    if (idx >= 0) {
        int nbytes = 0;
        if (idx > 0) {
            nbytes = WideCharToMultiByte(CP_UTF8, 0, ws.s, idx, nullptr, 0, nullptr, nullptr);
        }
        res = nbytes;
    }
    ArenaRestoreSavepoint(sp);
    return res;
}

void ReplacePtr(Str* s, Str snew) {
    if (s->s != snew.s) {
        str::Free(*s);
        *s = snew;
    }
}

void ReplaceWithCopy(Str* s, Str snew) {
    // dup before free so it's safe even if snew aliases *s; dup is always a
    // fresh allocation so it can never alias the old s->s -- no check needed
    Str dup = str::Dup(snew);
    str::Free(*s);
    *s = dup;
}

Str Join(Arena* allocator, Str s1, Str s2, Str s3, Str s4, Str s5) {
    int s1Len = len(s1);
    int s2Len = len(s2);
    int s3Len = len(s3);
    int s4Len = len(s4);
    int s5Len = len(s5);
    int len = s1Len + s2Len + s3Len + s4Len + s5Len + 1;
    char* res = (char*)Alloc(allocator, len);

    char* s = res;
    memcpy(s, s1.s, s1Len);
    s += s1Len;
    memcpy(s, s2.s, s2Len);
    s += s2Len;
    memcpy(s, s3.s, s3Len);
    s += s3Len;
    memcpy(s, s4.s, s4Len);
    s += s4Len;
    memcpy(s, s5.s, s5Len);
    s += s5Len;
    *s = 0;

    return Str(res, len - 1);
}

Str Join(Arena* allocator, Str s1, Str s2, Str s3) {
    return Join(allocator, s1, s2, s3, Str{}, Str{});
}

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
Str Join(Str s1, Str s2, Str s3) {
    return Join(nullptr, s1, s2, s3);
}

// trim suffix (exact match) from s, returning the shortened view
Str TrimSuffix(Str s, Str suffix) {
    if (str::EndsWith(s, suffix)) {
        return Str(s.s, s.len - suffix.len);
    }
    return s;
}

// index of last occurrence of c in s, or -1
int LastIndexOfChar(Str s, char c) {
    for (int i = s.len - 1; i >= 0; i--) {
        if (s.s[i] == c) {
            return i;
        }
    }
    return -1;
}

// trim trailing whitespace in place (writes a NUL at the new end), returns the shortened view
Str TrimSuffixWhitespace(Str s) {
    while (s.len > 0) {
        char c = s.s[s.len - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        s.len--;
        s.s[s.len] = 0;
    }
    return s;
}

} // namespace str
namespace wstr {

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
WStr Join(Arena* allocator, WStr s1, WStr s2, WStr s3) {
    size_t s1Len = (size_t)s1.len, s2Len = (size_t)s2.len, s3Len = (size_t)s3.len;
    size_t len = s1Len + s2Len + s3Len + 1;
    WCHAR* res = (WCHAR*)Alloc(allocator, len * sizeof(WCHAR));
    memcpy(res, s1.s, s1Len * sizeof(WCHAR));
    memcpy(res + s1Len, s2.s, s2Len * sizeof(WCHAR));
    memcpy(res + s1Len + s2Len, s3.s, s3Len * sizeof(WCHAR));
    res[s1Len + s2Len + s3Len] = '\0';
    return WStr(res);
}

WStr Join(WStr s1, WStr s2, WStr s3) {
    return Join(nullptr, s1, s2, s3);
}

} // namespace wstr
namespace str {

Str ToLowerInPlace(Str s) {
    for (int i = 0; i < s.len; i++) {
        s.s[i] = (char)tolower((u8)s.s[i]);
    }
    return s;
}

Str ToLower(Str s) {
    Str s2 = str::Dup(s);
    return ToLowerInPlace(s2);
}

// Encode unicode character as utf8 to buf at off and advance off.
// The caller must ensure there is enough free space (4 bytes) in buf
void Utf8Encode(char* buf, int& off, int c) {
    u8* tmp = (u8*)(buf + off);
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
    off = (int)((char*)tmp - buf);
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

int IndexOfChar(Str s, char c) {
    if (!s) {
        return -1;
    }
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == c) {
            return i;
        }
    }
    return -1;
}

bool ContainsChar(Str s, char c) {
    return IndexOfChar(s, c) >= 0;
}

Str FindChar(Str str, char c) {
    int idx = IndexOfChar(str, c);
    if (idx < 0) {
        return {};
    }
    return Str(str.s + idx, str.len - idx);
}

Str FindCharLast(Str str, char c) {
    if (!str) {
        return {};
    }
    for (int i = str.len - 1; i >= 0; i--) {
        if (str.s[i] == c) {
            return Str(str.s + i, str.len - i);
        }
    }
    return {};
}

int IndexOf(Str buf, Str toFind) {
    if (!buf || !toFind) {
        return -1;
    }
    int toFindLen = toFind.len;
    if (toFindLen <= 0 || buf.len < toFindLen) {
        return -1;
    }
    char c = toFind.s[0];
    int end = buf.len - toFindLen;
    for (int i = 0; i <= end; i++) {
        if (buf.s[i] == c && memeq(buf.s + i, toFind.s, (size_t)toFindLen)) {
            return i;
        }
    }
    return -1;
}

// offset just past the first occurrence of needle in s, or -1 if not found
int IndexOfAfter(Str s, Str needle) {
    int idx = IndexOf(s, needle);
    if (idx < 0) {
        return -1;
    }
    return idx + needle.len;
}

// Splits s around the first occurrence of sep (Go's strings.Cut). When sep is
// found, *before is the text before it and *after the text after it; returns
// true. When sep is not found, *before is all of s, *after is {} and it returns
// false. before/after may be null if not needed.
bool Cut(Str s, Str sep, Str* before, Str* after) {
    int idx = IndexOf(s, sep);
    if (idx < 0) {
        if (before) {
            *before = s;
        }
        if (after) {
            *after = {};
        }
        return false;
    }
    if (before) {
        *before = Str(s.s, idx);
    }
    if (after) {
        int off = idx + sep.len;
        *after = Str(s.s + off, s.len - off);
    }
    return true;
}

// Extracts the next line from s (up to a CR, LF or CRLF terminator) into line
// and sets rest to the remainder after the terminator. line excludes the
// terminator. Returns false when s is empty. Safe to alias s and rest, e.g.
// while (str::NextLine(rest, line, rest)) { ... }
bool NextLine(Str s, Str& line, Str& rest) {
    if (str::IsEmpty(s)) {
        return false;
    }
    int idx = -1;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        if (c == '\n' || c == '\r') {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        line = s;
        rest = {};
        return true;
    }
    line = Str(s.s, idx);
    int off = idx + 1;
    // treat CRLF as a single line terminator
    if (s.s[idx] == '\r' && off < s.len && s.s[off] == '\n') {
        off++;
    }
    rest = Str(s.s + off, s.len - off);
    return true;
}

/* replace in <str> the chars from <oldChars> with their equivalents
   from <newChars> (similar to UNIX's tr command)
   Returns the number of replaced characters. */
size_t TransCharsInPlace(Str str, Str oldChars, Str newChars) {
    if (!str) {
        return 0;
    }
    size_t findCount = 0;
    for (int i = 0; i < str.len; i++) {
        int idx = str::IndexOfChar(oldChars, str.s[i]);
        if (idx >= 0) {
            str.s[i] = newChars.s[idx];
            findCount++;
        }
    }

    return findCount;
}

// Trim whitespace characters, in-place, inside s.
// Returns number of trimmed characters.
size_t TrimWSInPlace(Str s, TrimOpt opt) {
    if (str::IsNull(s)) {
        return 0;
    }
    int start = 0;
    int end = s.len;
    if ((TrimOpt::Left == opt) || (TrimOpt::Both == opt)) {
        while (start < end && IsWs(s.s[start])) {
            start++;
        }
    }

    if ((TrimOpt::Right == opt) || (TrimOpt::Both == opt)) {
        while (end > start && IsWs(s.s[end - 1])) {
            end--;
        }
    }
    if (end < s.len) {
        s.s[end] = 0;
    }
    size_t trimmed = (size_t)start + (size_t)(s.len - end);
    if (start != 0) {
        memmove(s.s, s.s + start, (size_t)(end - start) + 1);
    }
    return trimmed;
}

// replaces all whitespace characters with spaces, collapses several
// consecutive spaces into one and strips heading/trailing ones
// returns the number of removed characters
size_t NormalizeWSInPlace(Str s) {
    if (!s) {
        return 0;
    }
    int dst = 0;
    bool addedSpace = true;

    for (int src = 0; src < s.len; src++) {
        if (!IsWs(s.s[src])) {
            s.s[dst++] = s.s[src];
            addedSpace = false;
        } else if (!addedSpace) {
            s.s[dst++] = ' ';
            addedSpace = true;
        }
    }

    if (dst > 0 && IsWs(s.s[dst - 1])) {
        dst--;
    }
    s.s[dst] = '\0';

    return (size_t)(s.len - dst);
}

static bool isNl(char c) {
    return '\r' == c || '\n' == c;
}

// replaces '\r\n' and '\r' with just '\n' and removes empty lines
size_t NormalizeNewlinesInPlace(Str s, Str endExclusive) {
    if (!s) {
        return 0;
    }
    int endOff = endExclusive.s ? (int)(endExclusive.s - s.s) : s.len;
    int read = 0;
    while (read < endOff && isNl(s.s[read])) {
        read++;
    }

    int dst = 0;
    bool inNewline = false;
    while (read < endOff) {
        if (isNl(s.s[read])) {
            if (!inNewline) {
                s.s[dst++] = '\n';
            }
            inNewline = true;
            read++;
        } else {
            s.s[dst++] = s.s[read++];
            inNewline = false;
        }
    }
    if (dst < endOff) {
        s.s[dst] = 0;
    }
    while (dst > 0 && s.s[dst - 1] == '\n') {
        dst--;
        s.s[dst] = 0;
    }
    return (size_t)dst;
}

size_t NormalizeNewlinesInPlace(Str s) {
    return NormalizeNewlinesInPlace(s, Str(s.s + s.len, 0));
}

// Remove all characters in "toRemove" from "str", in place.
// Returns number of removed characters.
size_t RemoveCharsInPlace(Str str, Str toRemove) {
    if (!str) {
        return 0;
    }
    size_t removed = 0;
    int dst = 0;
    for (int src = 0; src < str.len; src++) {
        char c = str.s[src];
        if (!str::ContainsChar(toRemove, c)) {
            str.s[dst++] = c;
        } else {
            ++removed;
        }
    }
    str.s[dst] = '\0';
    return removed;
}

// Remove all characters in "toRemove" from "str", in place.
// Returns number of removed characters.
} // namespace str
namespace wstr {

size_t RemoveCharsInPlace(WStr str, WStr toRemove) {
    if (!str) {
        return 0;
    }
    size_t removed = 0;
    int dst = 0;
    for (int src = 0; src < str.len; src++) {
        WCHAR c = str.s[src];
        if (!wstr::ContainsChar(toRemove, c)) {
            str.s[dst++] = c;
        } else {
            ++removed;
        }
    }
    str.s[dst] = '\0';
    return removed;
}

} // namespace wstr
namespace str {

/* Convert binary data in <buf> of size <len> to a hex-encoded string */
TempStr MemToHexTemp(const u8* buf, size_t len) {
    /* 2 hex chars per byte, +1 for terminating 0 */
    char* ret = AllocArrayTemp<char>(2 * len + 1);
    if (!ret) {
        return {};
    }
    static const char hex[] = "0123456789abcdef";
    int dst = 0;
    for (size_t i = 0; i < len; i++) {
        u8 b = buf[i];
        ret[dst++] = hex[b >> 4];
        ret[dst++] = hex[b & 0x0f];
    }
    ret[dst] = 0;
    return Str(ret, dst);
}

/* Reverse of MemToHexTemp. Convert a 0-terminatd hex-encoded string <s> to
   binary data pointed by <buf> of max size bufLen.
   Returns false if size of <s> doesn't match bufLen or is not a valid
   hex string. */
static int HexDigitVal(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool HexToMem(Str s, u8* buf, size_t bufLen) {
    size_t needed = bufLen * 2;
    if (s.len < (int)needed) {
        return false;
    }
    for (size_t i = 0; i < bufLen; i++) {
        int off = (int)(i * 2);
        int hi = HexDigitVal(s.s[off]);
        int lo = HexDigitVal(s.s[off + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        buf[i] = (u8)((hi << 4) | lo);
    }
    return s.len == (int)needed || (s.len > (int)needed && s.s[needed] == '\0');
}

static Str ExtractUntil(Str str, int off, char c, int* endOffOut) {
    if (off < 0 || off > str.len) {
        return {};
    }
    Str slice = Str(str.s + off, str.len - off);
    int foundOff = IndexOfChar(slice, c);
    if (foundOff < 0) {
        return {};
    }
    int endOff = off + foundOff;
    *endOffOut = endOff;
    return str::Dup(Str(str.s + off, foundOff));
}

static int ParseLimitedNumber(Str str, int p, int formatOff, Str format, int* endOffOut, void* valueOut) {
    unsigned int width;
    char f2[] = "% ";
    Str formatAt = Str(format.s + formatOff, format.len - formatOff);
    Str endF = Parse(formatAt, "%u%c", &width, &f2[1]);
    if (!str::IsNull(endF) && str::ContainsChar(StrL("udx"), f2[1]) && width <= (unsigned)(str.len - p)) {
        char limited[16]; // 32-bit integers are at most 11 characters long
        str::BufSet(limited, std::min((int)width + 1, dimofi(limited)), Str(str.s + p, (int)width));
        Str end = Parse(Str(limited), f2, valueOut);
        if (!str::IsNull(end) && !end.s[0]) {
            *endOffOut = p + (int)width;
            return (int)(endF.s - format.s) - 1;
        }
    }
    return -1;
}

static bool ParseULongAt(Str str, int off, int base, unsigned long* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    unsigned long v = 0;
    int i = off;
    while (i < str.len && str::IsWs(str.s[i])) {
        i++;
    }
    if (base == 16 && i + 1 < str.len && str.s[i] == '0' && (str.s[i + 1] == 'x' || str.s[i + 1] == 'X')) {
        i += 2;
    }
    bool any = false;
    while (i < str.len) {
        char c = str.s[i];
        int digit = -1;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (base == 16) {
            digit = HexDigitVal(c);
        }
        if (digit < 0 || (unsigned)digit >= (unsigned)base) {
            break;
        }
        any = true;
        v = v * (unsigned long)base + (unsigned long)digit;
        i++;
    }
    if (!any) {
        return false;
    }
    *val = v;
    *endOff = i;
    return true;
}

static bool ParseLongAt(Str str, int off, int base, long* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    bool neg = false;
    int i = off;
    while (i < str.len && str::IsWs(str.s[i])) {
        i++;
    }
    if (i >= str.len) {
        return false;
    }
    if (str.s[i] == '-') {
        neg = true;
        i++;
    } else if (str.s[i] == '+') {
        i++;
    }
    unsigned long uv = 0;
    int end = i;
    if (!ParseULongAt(Str(str.s + i, str.len - i), 0, base, &uv, &end)) {
        return false;
    }
    *val = neg ? -(long)uv : (long)uv;
    *endOff = i + end;
    return true;
}

static bool ParseDoubleAt(Str str, int off, double* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    char* sliceZ = CStrTemp(Str(str.s + off, str.len - off));
    ptrdiff_t consumed = 0;
    {
        char* endPtr = nullptr;
        *val = strtod(sliceZ, &endPtr);
        if (!endPtr || endPtr == sliceZ) {
            return false;
        }
        consumed = endPtr - sliceZ;
    }
    *endOff = off + (int)consumed;
    return true;
}

/* Parses a string into several variables sscanf-style (i.e. pass in pointers
   to where the parsed values are to be stored). Returns a pointer to the first
   character that's not been parsed when successful and nullptr otherwise.

   Supported formats:
     %u - parses an unsigned int
     %d - parses a signed int
     %x - parses an unsigned hex-int
     %f - parses a float
     %c - parses a single char
     %s - parses a string into an AutoFree (also on failure!)
     %S - parses a string into an AutoFree
     %? - makes the next single character optional (e.g. "x%?,y" parses both "xy" and "x,y")
     %$ - causes the parsing to fail if it's encountered when not at the end of the string
     %  - skips a single whitespace character
     %_ - skips one or multiple whitespace characters (or none at all)
     %% - matches a single '%'

   %u, %d and %x accept an optional width argument, indicating exactly how many
   characters must be read for parsing the number (e.g. "%4d" parses -123 out of "-12345"
   and doesn't parse "123" at all).
*/
static Str ParseV(Str str, Str format, va_list args) {
    if (str::IsNull(str) || str::IsNull(format)) {
        return {};
    }
    int p = 0;
    for (int fi = 0; fi < format.len; fi++) {
        char fc = format.s[fi];
        if (fc != '%') {
            if (p >= str.len || fc != str.s[p]) {
                return {};
            }
            p++;
            continue;
        }
        fi++;
        if (fi >= format.len) {
            return {};
        }
        char spec = format.s[fi];

        int end = -1;
        if ('u' == spec) {
            unsigned long v = 0;
            if (!ParseULongAt(str, p, 10, &v, &end)) {
                return {};
            }
            *va_arg(args, unsigned int*) = (unsigned int)v;
        } else if ('d' == spec) {
            long v = 0;
            if (!ParseLongAt(str, p, 10, &v, &end)) {
                return {};
            }
            *va_arg(args, int*) = (int)v;
        } else if ('x' == spec) {
            unsigned long v = 0;
            if (!ParseULongAt(str, p, 16, &v, &end)) {
                return {};
            }
            *va_arg(args, unsigned int*) = (unsigned int)v;
        } else if ('f' == spec) {
            double v = 0;
            if (!ParseDoubleAt(str, p, &v, &end)) {
                return {};
            }
            *va_arg(args, float*) = (float)v;
        } else if ('g' == spec) {
            double v = 0;
            if (!ParseDoubleAt(str, p, &v, &end)) {
                return {};
            }
            *va_arg(args, float*) = (float)v;
        } else if ('c' == spec) {
            if (p >= str.len) {
                return {};
            }
            *va_arg(args, char*) = str.s[p];
            end = p + 1;
        } else if ('s' == spec || 'S' == spec) {
            if (fi + 1 < format.len) {
                va_arg(args, AutoFree*)->Set(ExtractUntil(str, p, format.s[fi + 1], &end).s);
            } else {
                va_arg(args, AutoFree*)->Set(str::Dup(Str(str.s + p, str.len - p)).s);
                end = str.len;
            }
        } else if ('$' == spec && p >= str.len) {
            continue; // don't fail, if we're indeed at the end of the string
        } else if ('%' == spec) {
            if (p >= str.len || spec != str.s[p]) {
                return {};
            }
            end = p + 1;
        } else if (' ' == spec) {
            if (p >= str.len || !str::IsWs(str.s[p])) {
                return {};
            }
            end = p + 1;
        } else if ('_' == spec) {
            if (p >= str.len || !str::IsWs(str.s[p])) {
                continue; // don't fail, if there's no whitespace at all
            }
            for (end = p + 1; end < str.len && str::IsWs(str.s[end]); end++) {
                // do nothing
            }
        } else if ('?' == spec && fi + 1 < format.len) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            fi++;
            if (p >= str.len || str.s[p] != format.s[fi]) {
                continue;
            }
            end = p + 1;
        } else if (str::IsDigit(spec)) {
            int formatIdx = ParseLimitedNumber(str, p, fi, format, &end, va_arg(args, void*));
            if (formatIdx < 0) {
                return {};
            }
            fi = formatIdx;
        }
        if (end < 0 || end == p) {
            return {};
        }
        p = end;
    }
    return Str(str.s + p, str.len - p);
}

Str Parse(Str str, const char* fmt, ...) {
    if (str::IsNull(str) || !fmt) {
        return {};
    }

    va_list args;
    va_start(args, fmt);
    Str res = ParseV(str, fmt, args);
    va_end(args);
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
static bool CmpNaturalAtEnd(Str s, int i) {
    return i >= s.len || s.s[i] == '\0';
}

static char CmpNaturalAt(Str s, int i) {
    if (CmpNaturalAtEnd(s, i)) {
        return '\0';
    }
    return s.s[i];
}

static int CmpNaturalLex(Str a, Str b) {
    int minLen = std::min(a.len, b.len);
    for (int i = 0; i < minLen; i++) {
        if (a.s[i] != b.s[i]) {
            return (unsigned char)a.s[i] - (unsigned char)b.s[i];
        }
    }
    return a.len - b.len;
}

int CmpNatural(Str aIn, Str bIn) {
    ReportIf(!aIn || !bIn);
    int ai = 0;
    int bi = 0;
    int diff = 0;

    while (diff == 0) {
        // ignore leading and trailing spaces, and differences in whitespace only
        if (ai == 0 || bi == 0 || CmpNaturalAtEnd(aIn, ai) || CmpNaturalAtEnd(bIn, bi) ||
            (IsWs(aIn.s[ai]) && IsWs(bIn.s[bi]))) {
            while (!CmpNaturalAtEnd(aIn, ai) && IsWs(aIn.s[ai])) {
                ai++;
            }
            while (!CmpNaturalAtEnd(bIn, bi) && IsWs(bIn.s[bi])) {
                bi++;
            }
        }
        // if two strings are identical when ignoring case, leading zeroes and
        // whitespace, compare them traditionally for a stable sort order
        if (CmpNaturalAtEnd(aIn, ai) && CmpNaturalAtEnd(bIn, bi)) {
            return CmpNaturalLex(aIn, bIn);
        }

        char ca = CmpNaturalAt(aIn, ai);
        char cb = CmpNaturalAt(bIn, bi);

        if (str::IsDigit(ca) && str::IsDigit(cb)) {
            // ignore leading zeroes
            while (!CmpNaturalAtEnd(aIn, ai) && aIn.s[ai] == '0') {
                ai++;
            }
            while (!CmpNaturalAtEnd(bIn, bi) && bIn.s[bi] == '0') {
                bi++;
            }
            // compare the two numbers as (positive) integers
            for (diff = 0; str::IsDigit(CmpNaturalAt(aIn, ai)) || str::IsDigit(CmpNaturalAt(bIn, bi)); ai++, bi++) {
                // if either isn't a number, they differ in magnitude
                if (!str::IsDigit(CmpNaturalAt(aIn, ai))) {
                    return -1;
                }
                if (!str::IsDigit(CmpNaturalAt(bIn, bi))) {
                    return 1;
                }
                // remember the difference for when the numbers are of the same magnitude
                if (0 == diff) {
                    diff = (unsigned char)aIn.s[ai] - (unsigned char)bIn.s[bi];
                }
            }
            // neither is a digit, so continue with them (unless diff != 0)
            ai--;
            bi--;
        } else if (str::IsAlNum(ca) && str::IsAlNum(cb)) {
            // sort letters case-insensitively
            diff = tolower((u8)ca) - tolower((u8)cb);
        } else if (str::IsAlNum(ca)) {
            // sort special characters before text and numbers
            return 1;
        } else if (str::IsAlNum(cb)) {
            return -1;
        } else {
            // sort special characters by ASCII code
            diff = (unsigned char)ca - (unsigned char)cb;
        }
        ai++;
        bi++;
    }

    return diff;
}

bool IsEmptyOrWhiteSpace(Str s) {
    if (!s) {
        return true;
    }
    for (int i = 0; i < s.len; i++) {
        if (!str::IsWs(s.s[i])) {
            return false;
        }
    }
    return true;
}

bool Skip(Str& s, Str toSkip) {
    if (str::StartsWith(s, toSkip)) {
        s.s += toSkip.len;
        s.len -= toSkip.len;
        return true;
    }
    return false;
}

// advances s past any leading toSkip chars (in place); returns whether it skipped any
bool SkipChar(Str& s, char toSkip) {
    int i = 0;
    while (i < s.len && s.s[i] == toSkip) {
        i++;
    }
    s.s += i;
    s.len -= i;
    return i > 0;
}

} // namespace str

namespace url {

void DecodeInPlace(Str url) {
    if (str::IsNull(url)) {
        return;
    }
    int dst = 0;
    for (int src = 0; src < url.len; src++) {
        int val;
        if (url.s[src] == '%' && src + 2 < url.len &&
            !str::IsNull(str::Parse(Str(url.s + src, url.len - src), "%%%2x", &val))) {
            url.s[dst++] = (char)val;
            src += 2;
        } else {
            url.s[dst++] = url.s[src];
        }
    }
    url.s[dst] = '\0';
    url.len = dst;
}
} // namespace url

// SeqStrings (SeqStr* helpers) is for size-efficient implementation of:
// string -> int and int->string.
// it's even more efficient than using char *[] array
// it comes at the cost of speed, so it's not good for places
// that are critial for performance. On the other hand, it's
// not that bad: linear scanning of memory is fast due to the magic
// of L1 cache
TempStr SeqStrAt(SeqStrings strs, int off) {
    if (!strs || off < 0 || !strs[off]) {
        return {};
    }
    return Str(strs + off);
}

bool SeqStrAdvance(SeqStrings strs, int& off, int* idxInOut) {
    if (!strs || off < 0 || !strs[off]) {
        off = -1;
        if (idxInOut) {
            *idxInOut = -1;
        }
        return false;
    }
    off += len(strs + off) + 1;
    if (!strs[off]) {
        off = -1;
        return false;
    }
    if (idxInOut) {
        (*idxInOut)++;
    }
    return true;
}

// conceptually strings is an array of 0-terminated strings where, laid
// out sequentially in memory, terminated with a 0-length string
// Returns index of toFind string in strings
// Returns -1 if string doesn't exist
int SeqStrIndex(SeqStrings strs, Str toFind) {
    if (!toFind) {
        return -1;
    }
    int off = 0;
    int idx = 0;
    while (strs[off]) {
        if (str::Eq(SeqStrAt(strs, off), toFind)) {
            return idx;
        }
        if (!SeqStrAdvance(strs, off)) {
            break;
        }
        idx++;
    }
    return -1;
}

// like SeqStrIndex but ignores case and whitespace
int SeqStrIndexIS(SeqStrings strs, Str toFind) {
    if (!toFind) {
        return -1;
    }
    int off = 0;
    int idx = 0;
    while (strs[off]) {
        if (str::EqIS(SeqStrAt(strs, off), toFind)) {
            return idx;
        }
        if (!SeqStrAdvance(strs, off)) {
            break;
        }
        idx++;
    }
    return -1;
}

// Given an index in the "array" of sequentially laid out strings,
// returns a strings at that index.
TempStr SeqStrByIndex(SeqStrings strs, int idx) {
    ReportIf(idx < 0);
    int off = 0;
    while (idx > 0) {
        if (!SeqStrAdvance(strs, off)) {
            return {};
        }
        idx--;
    }
    return SeqStrAt(strs, off);
}

// flat sequence of (extension, mime type) pairs
static SeqStrings gMimeTypes =
    ".html\0text/html\0"
    ".htm\0text/html\0"
    ".gif\0image/gif\0"
    ".png\0image/png\0"
    ".jpg\0image/jpeg\0"
    ".jpeg\0image/jpeg\0"
    ".bmp\0image/bmp\0"
    ".css\0text/css\0"
    ".js\0text/javascript\0"
    ".svg\0image/svg+xml\0"
    ".txt\0text/plain\0"
    ".md\0text/plain\0"
    ".json\0application/json\0";

// ext is like ".png"; returns e.g. "image/png", or {} if the extension is not a
// known type. If the matched type is an image and imgExt (the real extension
// detected from the file's data) is given, imgExt's type wins over the ext's.
TempStr MimeTypeFromExtTemp(Str ext, Str imgExt) {
    int idx = SeqStrIndexIS(gMimeTypes, ext);
    if (idx < 0) {
        return {};
    }
    Str mime = SeqStrByIndex(gMimeTypes, idx + 1);
    // trust an image's actual data over its extension
    if (imgExt && str::StartsWith(mime, StrL("image/"))) {
        int j = SeqStrIndex(gMimeTypes, imgExt);
        if (j >= 0) {
            return SeqStrByIndex(gMimeTypes, j + 1);
        }
    }
    return mime;
}

// unsigned LEB128 of zigzag-encoded i64
static size_t VarIntEncode(u8* dst, i64 val) {
    u64 n = ((u64)val << 1) ^ (u64)(val >> 63);
    size_t i = 0;
    for (;;) {
        u8 b = (u8)(n & 0x7f);
        n >>= 7;
        if (n) {
            b |= 0x80;
        }
        dst[i++] = b;
        if (!n) {
            return i;
        }
    }
}

static bool VarIntDecode(const u8*& p, i64* out) {
    u64 n = 0;
    int shift = 0;
    for (;;) {
        u8 b = *p++;
        n |= (u64)(b & 0x7f) << shift;
        if (!(b & 0x80)) {
            *out = (i64)((n >> 1) ^ (~(n & 1) + 1));
            return true;
        }
        shift += 7;
        if (shift >= 64) {
            return false;
        }
    }
}

static int SeqStrNumEntryEndOff(SeqStrNum strs, int off) {
    if (!strs || off < 0 || !strs[off]) {
        return off;
    }
    int next = off + len(strs + off) + 1;
    const u8* p = (const u8*)(strs + next);
    while (*p & 0x80) {
        p++;
    }
    return next + (int)(p - (const u8*)(strs + next)) + 1;
}

static void SeqStrNumEntryParts(SeqStrNum strs, int off, Str* strOut, i64* numOut) {
    if (strOut) {
        *strOut = SeqStrAt(strs, off);
    }
    const u8* p = (const u8*)(strs + off + len(strs + off) + 1);
    if (numOut) {
        VarIntDecode(p, numOut);
    }
}

void SeqStrNumAppend(str::Builder* b, Str s, i64 num) {
    b->Append(s);
    b->AppendChar('\0');
    u8 buf[12];
    size_t n = VarIntEncode(buf, num);
    b->Append(Str((char*)buf, (int)n));
}

void SeqStrNumFinish(str::Builder* b) {
    b->AppendChar('\0');
}

TempStr SeqStrNumAt(SeqStrNum strs, int off) {
    return SeqStrAt(strs, off);
}

bool SeqStrNumAdvance(SeqStrNum strs, int& off, int* idxInOut) {
    if (!strs || off < 0 || !strs[off]) {
        off = -1;
        if (idxInOut) {
            *idxInOut = -1;
        }
        return false;
    }
    off = SeqStrNumEntryEndOff(strs, off);
    if (!strs[off]) {
        off = -1;
        return false;
    }
    if (idxInOut) {
        (*idxInOut)++;
    }
    return true;
}

int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut) {
    if (!toFind) {
        return -1;
    }
    int off = 0;
    int idx = 0;
    while (strs && strs[off]) {
        if (str::Eq(SeqStrNumAt(strs, off), toFind)) {
            if (numOut) {
                SeqStrNumEntryParts(strs, off, nullptr, numOut);
            }
            return idx;
        }
        if (!SeqStrNumAdvance(strs, off)) {
            break;
        }
        idx++;
    }
    return -1;
}

int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut) {
    if (!toFind) {
        return -1;
    }
    int off = 0;
    int idx = 0;
    while (strs && strs[off]) {
        if (str::EqIS(SeqStrNumAt(strs, off), toFind)) {
            if (numOut) {
                SeqStrNumEntryParts(strs, off, nullptr, numOut);
            }
            return idx;
        }
        if (!SeqStrNumAdvance(strs, off)) {
            break;
        }
        idx++;
    }
    return -1;
}

TempStr SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut) {
    ReportIf(idx < 0);
    int off = 0;
    while (idx > 0) {
        if (!SeqStrNumAdvance(strs, off)) {
            return {};
        }
        idx--;
    }
    if (!strs || !strs[off]) {
        return {};
    }
    if (numOut) {
        SeqStrNumEntryParts(strs, off, nullptr, numOut);
    }
    return SeqStrNumAt(strs, off);
}

TempStr SeqStrNumStrByNumber(SeqStrNum strs, i64 num) {
    int off = 0;
    while (strs && strs[off]) {
        i64 n = 0;
        Str s;
        SeqStrNumEntryParts(strs, off, &s, &n);
        if (n == num) {
            return s;
        }
        if (!SeqStrNumAdvance(strs, off)) {
            break;
        }
    }
    return {};
}

// for compatibility with C string, the last character is always 0
// kPadding is number of characters needed for terminating character
static constexpr size_t kPadding = 1;

static char* EnsureCap(str::Builder* s, size_t needed) {
    if (needed + kPadding <= str::Builder::kBufChars) {
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
        newEls = (char*)Alloc(s->allocator, allocSize);
        if (newEls) {
            memcpy(newEls, s->buf, s->len + 1);
        }
    } else {
        newEls = (char*)Realloc(s->allocator, s->els, allocSize);
    }
    if (!newEls) {
        ReportIf(InterlockedExchangeAdd(&gAllowAllocFailure, 0) == 0);
        return nullptr;
    }
    s->els = newEls;
    s->cap = (u32)newCap;
    return newEls;
}

static char* MakeSpaceAt(str::Builder* s, size_t idx, size_t count) {
    ReportIf(count == 0);
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

static void StrBuilderFree(str::Builder* s) {
    if (!s->els || (s->els == s->buf)) {
        return;
    }
    Free(s->allocator, s->els);
    s->els = nullptr;
}

void str::Builder::Reset(Str s) {
    StrBuilderFree(this);
    len = 0;
    cap = 0;
    els = buf;

#if defined(DEBUG)
#define kFillerStr "01234567890123456789012345678901"
    // to catch mistakes earlier, fill the buffer with a known string
    constexpr size_t nFiller = sizeof(kFillerStr) - 1;
    static_assert(nFiller == str::Builder::kBufChars);
    memcpy(buf, kFillerStr, kBufChars);
#endif

    buf[0] = 0;
    Append(s); // no-op if s is empty
}

// allocator is not owned by Vec and must outlive it
str::Builder::Builder(int capHint, Arena* a) {
    allocator = a;
    Reset();
    cap = (u32)(capHint + kPadding); // + kPadding for terminating 0
}

str::Builder::Builder(Str s) {
    Reset();
    Append(s);
}

str::Builder::~Builder() {
    StrBuilderFree(this);
}

char& str::Builder::operator[](int idx) const {
    ReportIf(idx < 0 || idx >= (int)len);
    return els[idx];
}

int len(const str::Builder& b) {
    return (int)b.len;
}

bool str::Builder::InsertAt(int idx, char el) {
    char* p = MakeSpaceAt(this, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool str::Builder::AppendChar(char c) {
    return InsertAt((int)len, c);
}

bool str::Builder::Append(Str src) {
    if (str::IsNull(src) || 0 == src.len) {
        return true;
    }
    char* dst = MakeSpaceAt(this, len, src.len);
    if (!dst) {
        return false;
    }
    memcpy(dst, src.s, src.len);
    return true;
}

char str::Builder::RemoveAt(int idx, int count) {
    char res = els[idx];
    if ((int)len > idx + count) {
        char* dst = els + idx;
        char* src = els + idx + count;
        int nToMove = (int)len - idx - count;
        memmove(dst, src, nToMove);
    }
    len -= (u32)count;
    memset(els + len, 0, count);
    return res;
}

char str::Builder::RemoveLast() {
    if (len == 0) {
        return 0;
    }
    return RemoveAt((int)len - 1);
}

char& str::Builder::Last() const {
    ReportIf(0 == len);
    return els[len - 1];
}

// perf hack for using as a buffer: client can get accumulated data
// without duplicate allocation. Note: since Vec over-allocates, this
// is likely to use more memory than strictly necessary, but in most cases
// it doesn't matter
Str str::Builder::TakeStr() {
    int n = (int)len;
    char* res = els;
    if (els == buf) {
        // data is in the inline buffer, so we have to duplicate it
        res = (char*)MemDup(this->allocator, els, len + kPadding);
    } else {
        // we're returning els, so reset to small buf
        els = buf;
    }

    Reset();
    return Str(res, n);
}

bool str::Contains(const str::Builder& b, Str s) {
    return str::Contains(ToStr(b), s);
}

bool str::Builder::IsEmpty() const {
    return len == 0;
}

char str::Builder::LastChar() const {
    auto n = this->len;
    if (n == 0) {
        return 0;
    }
    return els[n - 1];
}

static WCHAR* EnsureCap(wstr::Builder* s, size_t needed) {
    if (needed + kPadding <= str::Builder::kBufChars) {
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

    size_t allocSize = newElCount * wstr::Builder::kElSize;
    WCHAR* newEls;
    if (s->buf == s->els) {
        newEls = (WCHAR*)Alloc(s->allocator, allocSize);
        if (newEls) {
            memcpy(newEls, s->buf, wstr::Builder::kElSize * (s->len + 1));
        }
    } else {
        newEls = (WCHAR*)Realloc(s->allocator, s->els, allocSize);
    }

    if (!newEls) {
        ReportIf(InterlockedExchangeAdd(&gAllowAllocFailure, 0) == 0);
        return nullptr;
    }
    s->els = newEls;
    s->cap = (u32)newCap;
    return newEls;
}

static WCHAR* MakeSpaceAt(wstr::Builder* s, size_t idx, size_t count) {
    ReportIf(count == 0);
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
        memmove(dst, src, (s->len - idx) * wstr::Builder::kElSize);
    }
    s->len = newLen;
    return res;
}

static void WStrBuilderFree(wstr::Builder* s) {
    if (!s->els || (s->els == s->buf)) {
        return;
    }
    Free(s->allocator, s->els);
    s->els = nullptr;
}

void wstr::Builder::Reset(WStr s) {
    WStrBuilderFree(this);
    len = 0;
    cap = 0;
    els = buf;

#if defined(DEBUG)
#define kFillerWStr L"01234567890123456789012345678901"
    // to catch mistakes earlier, fill the buffer with a known string
    constexpr size_t nFiller = sizeof(kFillerStr) - 1;
    static_assert(nFiller == str::Builder::kBufChars);
    memcpy(buf, kFillerWStr, nFiller * kElSize);
#endif

    buf[0] = 0;
    Append(s); // no-op if s is empty
}

// allocator is not owned by Vec and must outlive it
wstr::Builder::Builder(int capHint, Arena* a) {
    allocator = a;
    Reset();
    cap = (u32)(capHint + kPadding); // + kPadding for terminating 0
}

// ensure that a Vec never shares its els buffer with another after a clone/copy
// note: we don't inherit allocator as it's not needed for our use cases
wstr::Builder::Builder(const wstr::Builder& that) {
    Reset();
    WCHAR* s = EnsureCap(this, that.cap);
    WStr sOrig = ToWStr(that);
    len = that.len;
    size_t n = (len + kPadding) * kElSize;
    memcpy(s, sOrig.s, n);
}

wstr::Builder::Builder(WStr s) {
    Reset();
    Append(s);
}

wstr::Builder& wstr::Builder::operator=(const wstr::Builder& that) {
    if (this == &that) {
        return *this;
    }
    Reset();
    WCHAR* s = EnsureCap(this, that.cap);
    WStr sOrig = ToWStr(that);
    len = that.len;
    size_t n = (len + kPadding) * kElSize;
    memcpy(s, sOrig.s, n);
    return *this;
}

wstr::Builder::~Builder() {
    WStrBuilderFree(this);
}

WCHAR& wstr::Builder::operator[](int idx) const {
    ReportIf(idx < 0 || idx >= (int)len);
    return els[idx];
}

int len(const wstr::Builder& b) {
    return (int)b.len;
}

bool wstr::Builder::InsertAt(int idx, const WCHAR& el) {
    WCHAR* p = MakeSpaceAt(this, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool wstr::Builder::AppendChar(WCHAR c) {
    return InsertAt((int)len, c);
}

bool wstr::Builder::Append(WStr src) {
    if (wstr::IsNull(src) || 0 == src.len) {
        return true;
    }
    WCHAR* dst = MakeSpaceAt(this, len, src.len);
    if (!dst) {
        return false;
    }
    memcpy(dst, src.s, src.len * kElSize);
    return true;
}

WCHAR wstr::Builder::RemoveAt(int idx, int count) {
    WCHAR res = els[idx];
    if ((int)len > idx + count) {
        WCHAR* dst = els + idx;
        WCHAR* src = els + idx + count;
        memmove(dst, src, ((int)len - idx - count) * kElSize);
    }
    len -= (u32)count;
    memset(els + len, 0, count * kElSize);
    return res;
}

WCHAR wstr::Builder::RemoveLast() {
    if (len == 0) {
        return 0;
    }
    return RemoveAt((int)len - 1);
}

// perf hack for using as a buffer: client can get accumulated data
// without duplicate allocation. Note: since Vec over-allocates, this
// is likely to use more memory than strictly necessary, but in most cases
// it doesn't matter
WStr wstr::Builder::TakeWStr() {
    int n = (int)len;
    WCHAR* res = els;
    if (els == buf) {
        res = (WCHAR*)MemDup(allocator, buf, (len + kPadding) * kElSize);
    }
    els = buf;
    Reset();
    return WStr(res, n);
}

bool wstr::ContainsChar(const wstr::Builder& b, WCHAR el) {
    return wstr::ContainsChar(ToWStr(b), el);
}

bool wstr::Builder::IsEmpty() const {
    return len == 0;
}

WCHAR wstr::Builder::LastChar() const {
    auto n = this->len;
    if (n == 0) {
        return 0;
    }
    return els[n - 1];
}

namespace wstr {

// returns true if was replaced
bool Replace(wstr::Builder& s, WStr toReplace, WStr replaceWith) {
    // fast path: nothing to replace
    if (!wstr::FindFrom(WStr(s.els), toReplace)) {
        return false;
    }
    WStr newStr = wstr::Replace(WStr(s.els), toReplace, replaceWith);
    s.Reset();
    if (newStr) {
        s.Append(newStr);
        wstr::Free(newStr);
    }
    return true;
}

bool IsWs(WCHAR c) {
    return iswspace(c);
}

bool IsDigit(WCHAR c) {
    return ('0' <= c) && (c <= '9');
}

bool IsNonCharacter(WCHAR c) {
    return c >= 0xFFFE || (c & ~1) == 0xDFFE || (0xFDD0 <= c && c <= 0xFDEF);
}

} // namespace wstr
namespace str {

// hack: to fool CodeQL which doesn't approve of char* => WCHAR* casts
// and doesn't allow any way to disable that warning
WStr CastToWCHAR(Str s) {
    if (!s) {
        return {};
    }
    return WStr((WCHAR*)s.s, s.len / (int)sizeof(WCHAR));
}

} // namespace str
namespace wstr {

// return true if s1 == s2, case sensitive
bool Eq(WStr s1, WStr s2) {
    if (s1.len != s2.len) {
        return false;
    }
    for (int i = 0; i < s1.len; i++) {
        if (s1.s[i] != s2.s[i]) {
            return false;
        }
    }
    return true;
}

// return true if s1 == s2, case insensitive
bool EqI(WStr s1, WStr s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (s1.len != s2.len) {
        return false;
    }
    if (s1.len == 0) {
        return true;
    }
    if (wstr::IsNull(s1) || wstr::IsNull(s2)) {
        return false;
    }
    return 0 == _wcsnicmp(s1.s, s2.s, (size_t)s1.len);
}

bool EqN(WStr s1, WStr s2, size_t len) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == wcsncmp(s1.s, s2.s, len);
}

bool IsNull(const WStr& s) {
    return !s.s;
}

bool IsEmpty(WStr s) {
    return wstr::IsNull(s) || s.len == 0;
}

bool StartsWith(WStr str, WStr prefix) {
    if (!prefix) {
        return true;
    }
    if (!str || prefix.len > str.len) {
        return false;
    }
    return EqN(str, prefix, (size_t)prefix.len);
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(WStr str, WStr prefix) {
    if (str.s == prefix.s) {
        return true;
    }
    if (!prefix) {
        return true;
    }
    if (!str || prefix.len > str.len) {
        return false;
    }
    return 0 == _wcsnicmp(str.s, prefix.s, (size_t)prefix.len);
}

bool EndsWith(WStr txt, WStr end) {
    if (!txt || !end) {
        return false;
    }
    if (end.len > txt.len) {
        return false;
    }
    return Eq(WStr(txt.s + txt.len - end.len, (int)end.len), end);
}

bool EndsWithI(WStr txt, WStr end) {
    if (!txt || !end) {
        return false;
    }
    if (end.len > txt.len) {
        return false;
    }
    return EqI(WStr(txt.s + txt.len - end.len, (int)end.len), end);
}

int IndexOfChar(WStr s, WCHAR c) {
    if (!s) {
        return -1;
    }
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == c) {
            return i;
        }
    }
    return -1;
}

bool ContainsChar(WStr s, WCHAR c) {
    return IndexOfChar(s, c) >= 0;
}

WStr FindChar(WStr str, WCHAR c) {
    int idx = IndexOfChar(str, c);
    if (idx < 0) {
        return {};
    }
    return WStr(str.s + idx, str.len - idx);
}

WStr FindFrom(WStr str, WStr find) {
    if (!str || !find || find.len > str.len) {
        return {};
    }
    for (int i = 0; i <= str.len - find.len; i++) {
        if (0 == wcsncmp(str.s + i, find.s, (size_t)find.len)) {
            return WStr(str.s + i, str.len - i);
        }
    }
    return {};
}

} // namespace wstr
namespace str {

Str ToUpperInPlace(Str s) {
    if (!s) {
        return {};
    }
    for (int i = 0; i < s.len; i++) {
        s.s[i] = (char)toupper((u8)s.s[i]);
    }
    return s;
}

} // namespace str
namespace wstr {

WStr ToLowerInPlace(WStr s) {
    if (!s) {
        return {};
    }
    for (int i = 0; i < s.len; i++) {
        s.s[i] = towlower(s.s[i]);
    }
    return s;
}

WStr ToLower(WStr s) {
    WStr s2 = wstr::Dup(s);
    return ToLowerInPlace(s2);
}

size_t TransCharsInPlace(WStr str, WStr oldChars, WStr newChars) {
    if (!str) {
        return 0;
    }
    size_t nReplaced = 0;
    for (int i = 0; i < str.len; i++) {
        int idx = wstr::IndexOfChar(oldChars, str.s[i]);
        if (idx >= 0) {
            str.s[i] = newChars.s[idx];
            nReplaced++;
        }
    }

    return nReplaced;
}

// free() the result via str::Free(s) or str::FreePtr(&s)
WStr Replace(WStr s, WStr toReplace, WStr replaceWith) {
    if (!s || wstr::IsEmpty(toReplace) || !replaceWith) {
        return {};
    }

    wstr::Builder result((size_t)s.len);
    int findLen = toReplace.len;
    int start = 0;
    while (start < s.len) {
        WStr rest(s.s + start, s.len - start);
        WStr match = wstr::FindFrom(rest, toReplace);
        if (!match) {
            result.Append(WStr(s.s + start, s.len - start));
            break;
        }
        int matchOff = (int)(match.s - s.s);
        result.Append(WStr(s.s + start, matchOff - start));
        result.Append(replaceWith);
        start = matchOff + findLen;
    }
    return result.TakeWStr();
}

// replaces all whitespace characters with spaces, collapses several
// consecutive spaces into one and strips heading/trailing ones
// returns the number of removed characters
size_t NormalizeWSInPlace(WStr s) {
    if (!s) {
        return 0;
    }
    int src = 0;
    int dst = 0;
    bool addedSpace = true;

    while (src < s.len) {
        if (!IsWs(s.s[src])) {
            s.s[dst++] = s.s[src];
            addedSpace = false;
        } else if (!addedSpace) {
            s.s[dst++] = L' ';
            addedSpace = true;
        }
        src++;
    }

    if (dst > 0 && IsWs(s.s[dst - 1])) {
        dst--;
    }
    s.s[dst] = L'\0';

    return (size_t)(src - dst);
}

} // namespace wstr
namespace str {

// Note: BufSet() should only be used when absolutely necessary (e.g. when
// handling buffers in OS-defined structures)
// returns the number of characters written (without the terminating \0)
int BufSet(char* dst, int cchDst, Str src) {
    ReportIf(0 == cchDst || !dst);
    if (!src) {
        *dst = 0;
        return 0;
    }

    int toCopy = std::min(cchDst - 1, src.len);

    errno_t err = strncpy_s(dst, (size_t)cchDst, src.s, (size_t)toCopy);
    ReportIf(err || dst[toCopy] != '\0');

    return toCopy;
}

} // namespace str
namespace wstr {

int BufSet(WCHAR* dst, int cchDst, WStr src) {
    ReportIf(0 == cchDst || !dst);
    if (!src) {
        *dst = 0;
        return 0;
    }

    int toCopy = std::min(cchDst - 1, src.len);

    memset(dst, 0, cchDst * sizeof(WCHAR));
    memcpy(dst, src.s, toCopy * sizeof(WCHAR));
    return toCopy;
}

} // namespace wstr
namespace str {

int BufSet(WCHAR* dst, int dstCchSize, Str src) {
    return wstr::BufSet(dst, dstCchSize, ToWStrTemp(src));
}

// append as much of s at the end of dst (which must be properly null-terminated)
// as will fit.
int BufAppend(char* dst, int dstCch, Str s) {
    ReportIf(0 == dstCch);

    int currDstCchLen = len(dst);
    if (currDstCchLen + 1 >= dstCch) {
        return 0;
    }
    int left = dstCch - currDstCchLen - 1;
    int toCopy = std::min(left, s.len);

    errno_t err = strncat_s(dst, dstCch, s.s, toCopy);
    ReportIf(err || dst[currDstCchLen + toCopy] != '\0');

    return toCopy;
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale) {
    WCHAR thousandSepW[4]{};
    if (!GetLocaleInfoW(locale, LOCALE_STHOUSAND, thousandSepW, dimof(thousandSepW))) {
        str::BufSet(thousandSepW, dimof(thousandSepW), ",");
    }
    TempStr thousandSep = ToUtf8Temp(thousandSepW);
    TempStr buf = strfmt::FormatTemp("%d", num);

    str::Builder res;
    int i = 3 - (buf.len % 3);
    for (int src = 0; src < buf.len; src++) {
        res.AppendChar(buf.s[src]);
        if (src + 1 < buf.len && i == 2) {
            res.Append(thousandSep);
        }
        i = (i + 1) % 3;
    }

    return ToStrTemp(res);
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
TempStr FormatFloatWithThousandSepTemp(double number, LCID locale, bool stripTrailingZero) {
    i64 num = (i64)(number * 100 + 0.5);

    TempStr tmp = FormatNumWithThousandSepTemp(num / 100, locale);
    WCHAR decimalW[4] = {};
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
    TempStr buf = strfmt::FormatTemp("%s%s%02d", tmp, Str(decimal), num % 100);
    if (stripTrailingZero && str::EndsWith(buf, StrL("0"))) {
        buf.s[buf.len - 1] = '\0';
        buf.len--;
    }

    return buf;
}

constexpr double KB = 1024;
constexpr double MB = (double)1024 * (double)1024;
constexpr double GB = (double)1024 * (double)1024 * (double)1024;

static Str sizeUnitsEnglish[3] = {StrL("GB"), StrL("MB"), StrL("KB")};

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// To be used in a context where translations are not yet available
TempStr FormatSizeShortTemp(i64 size) {
    return FormatSizeShortTemp(size, sizeUnitsEnglish);
}

TempStr FormatSizeShortTemp(i64 size, Str const* sizeUnits) {
    Str unit{};
    double s = (double)size;
    if (!sizeUnits) {
        sizeUnits = sizeUnitsEnglish;
    }
    if (s > GB) {
        s = s / GB;
        unit = sizeUnits[0];
    } else if (s > MB) {
        s = s / MB;
        unit = sizeUnits[1];
    } else {
        s = s / KB;
        unit = sizeUnits[2];
    }

    TempStr sizestr = str::FormatFloatWithThousandSepTemp(s, LOCALE_USER_DEFAULT, false);
    if (!unit) {
        return sizestr;
    }
    return strfmt::FormatTemp("%s %s", sizestr, unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTemp(i64 size) {
    if (size <= 0) {
        return strfmt::FormatTemp("%d", (int)size);
    }
    TempStr n1 = str::FormatSizeShortTemp(size);
    TempStr n2 = str::FormatNumWithThousandSepTemp(size);
    return strfmt::FormatTemp("%s (%s %s)", n1, n2, StrL("Bytes"));
}

// http://rosettacode.org/wiki/Roman_numerals/Encode#C.2B.2B
TempStr FormatRomanNumeralTemp(int n) {
    if (n < 1) {
        return {};
    }

    static struct {
        int value;
        Str numeral;
    } romandata[] = {{1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"}, {50, "L"},
                     {40, "XL"},  {10, "X"},   {9, "IX"},  {5, "V"},    {4, "IV"},  {1, "I"}};

    str::Builder roman;
    for (int i = 0; i < dimof(romandata); i++) {
        auto&& el = romandata[i];
        for (; n >= el.value; n -= el.value) {
            roman.Append(el.numeral);
        }
    }
    return ToStrTemp(roman);
}

} // namespace str
namespace wstr {

static WStr ExtractUntilW(WStr str, int off, WCHAR c, int* endOffOut) {
    if (off < 0 || off > str.len) {
        return {};
    }
    WStr slice = WStr(str.s + off, str.len - off);
    int foundOff = IndexOfChar(slice, c);
    if (foundOff < 0) {
        return {};
    }
    int endOff = off + foundOff;
    *endOffOut = endOff;
    return wstr::Dup(WStr(str.s + off, foundOff));
}

static int ParseLimitedNumberW(WStr str, int p, int formatOff, WStr format, int* endOffOut, void* valueOut) {
    unsigned int width;
    WCHAR f2[] = L"% ";
    WStr formatAt = WStr(format.s + formatOff, format.len - formatOff);
    WStr endF = Parse(formatAt, L"%u%c", &width, &f2[1]);
    if (!wstr::IsNull(endF) && ContainsChar(WStr(L"udx"), f2[1]) && width <= (unsigned)(str.len - p)) {
        WCHAR limited[16]; // 32-bit integers are at most 11 characters long
        wstr::BufSet(limited, std::min((int)width + 1, dimofi(limited)), WStr(str.s + p, (int)width));
        WStr end = Parse(WStr(limited), f2, valueOut);
        if (!wstr::IsNull(end) && !end.s[0]) {
            *endOffOut = p + (int)width;
            return (int)(endF.s - format.s) - 1;
        }
    }
    return -1;
}

static bool ParseULongAtW(WStr str, int off, int base, unsigned long* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    unsigned long v = 0;
    int i = off;
    while (i < str.len && wstr::IsWs(str.s[i])) {
        i++;
    }
    if (base == 16 && i + 1 < str.len && str.s[i] == L'0' && (str.s[i + 1] == L'x' || str.s[i + 1] == L'X')) {
        i += 2;
    }
    bool any = false;
    while (i < str.len) {
        WCHAR wc = str.s[i];
        int digit = -1;
        if (wc >= L'0' && wc <= L'9') {
            digit = (int)(wc - L'0');
        } else if (base == 16) {
            digit = str::HexDigitVal((char)wc);
        }
        if (digit < 0 || (unsigned)digit >= (unsigned)base) {
            break;
        }
        any = true;
        v = v * (unsigned long)base + (unsigned long)digit;
        i++;
    }
    if (!any) {
        return false;
    }
    *val = v;
    *endOff = i;
    return true;
}

static bool ParseLongAtW(WStr str, int off, int base, long* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    bool neg = false;
    int i = off;
    while (i < str.len && wstr::IsWs(str.s[i])) {
        i++;
    }
    if (i >= str.len) {
        return false;
    }
    if (str.s[i] == L'-') {
        neg = true;
        i++;
    } else if (str.s[i] == L'+') {
        i++;
    }
    unsigned long uv = 0;
    int end = i;
    if (!ParseULongAtW(WStr(str.s + i, str.len - i), 0, base, &uv, &end)) {
        return false;
    }
    *val = neg ? -(long)uv : (long)uv;
    *endOff = i + end;
    return true;
}

static bool ParseDoubleAtW(WStr str, int off, double* val, int* endOff) {
    if (off >= str.len) {
        return false;
    }
    int rem = str.len - off;
    WCHAR* sliceZ = AllocArrayTemp<WCHAR>(rem + 1);
    memcpy(sliceZ, str.s + off, rem * sizeof(WCHAR));
    sliceZ[rem] = 0;
    WCHAR* endPtr = nullptr;
    *val = wcstod(sliceZ, &endPtr);
    if (!endPtr || endPtr == sliceZ) {
        return false;
    }
    *endOff = off + (int)(endPtr - sliceZ);
    return true;
}

static WStr ParseVW(WStr str, WStr format, va_list args) {
    if (wstr::IsNull(str) || wstr::IsNull(format)) {
        return {};
    }
    int p = 0;
    for (int fi = 0; fi < format.len; fi++) {
        WCHAR fc = format.s[fi];
        if (fc != L'%') {
            if (p >= str.len || fc != str.s[p]) {
                return {};
            }
            p++;
            continue;
        }
        fi++;
        if (fi >= format.len) {
            return {};
        }
        WCHAR spec = format.s[fi];

        int end = -1;
        if (L'u' == spec) {
            unsigned long v = 0;
            if (!ParseULongAtW(str, p, 10, &v, &end)) {
                return {};
            }
            *va_arg(args, unsigned int*) = (unsigned int)v;
        } else if (L'd' == spec) {
            long v = 0;
            if (!ParseLongAtW(str, p, 10, &v, &end)) {
                return {};
            }
            *va_arg(args, int*) = (int)v;
        } else if (L'x' == spec) {
            unsigned long v = 0;
            if (!ParseULongAtW(str, p, 16, &v, &end)) {
                return {};
            }
            *va_arg(args, unsigned int*) = (unsigned int)v;
        } else if (L'f' == spec) {
            double v = 0;
            if (!ParseDoubleAtW(str, p, &v, &end)) {
                return {};
            }
            *va_arg(args, float*) = (float)v;
        } else if (L'c' == spec) {
            if (p >= str.len) {
                return {};
            }
            *va_arg(args, WCHAR*) = str.s[p];
            end = p + 1;
        } else if (L's' == spec || L'S' == spec) {
            if (fi + 1 < format.len) {
                va_arg(args, AutoFreeWStr*)->Set(ExtractUntilW(str, p, format.s[fi + 1], &end).s);
            } else {
                va_arg(args, AutoFreeWStr*)->Set(wstr::Dup(WStr(str.s + p, str.len - p)).s);
                end = str.len;
            }
        } else if (L'$' == spec && p >= str.len) {
            continue; // don't fail, if we're indeed at the end of the string
        } else if (L'%' == spec) {
            if (p >= str.len || spec != str.s[p]) {
                return {};
            }
            end = p + 1;
        } else if (L' ' == spec) {
            if (p >= str.len || !wstr::IsWs(str.s[p])) {
                return {};
            }
            end = p + 1;
        } else if (L'_' == spec) {
            if (p >= str.len || !wstr::IsWs(str.s[p])) {
                continue; // don't fail, if there's no whitespace at all
            }
            for (end = p + 1; end < str.len && wstr::IsWs(str.s[end]); end++) {
                // do nothing
            }
        } else if (L'?' == spec && fi + 1 < format.len) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            fi++;
            if (p >= str.len || str.s[p] != format.s[fi]) {
                continue;
            }
            end = p + 1;
        } else if (wstr::IsDigit(spec)) {
            int formatIdx = ParseLimitedNumberW(str, p, fi, format, &end, va_arg(args, void*));
            if (formatIdx < 0) {
                return {};
            }
            fi = formatIdx;
        }
        if (end < 0 || end == p) {
            return {};
        }
        p = end;
    }
    return WStr(str.s + p, str.len - p);
}

WStr Parse(WStr str, const WCHAR* fmt, ...) {
    if (wstr::IsNull(str) || !fmt) {
        return {};
    }
    va_list args;
    va_start(args, fmt);
    WStr res = ParseVW(str, fmt, args);
    va_end(args);
    return res;
}

} // namespace wstr

namespace url {

bool IsAbsolute(Str url) {
    int colon = str::IndexOfChar(url, ':');
    if (colon < 0) {
        return false;
    }
    int hash = str::IndexOfChar(url, '#');
    return hash < 0 || hash > colon;
}

TempStr GetFullPathTemp(Str url) {
    TempStr path = str::DupTemp(url);
    str::TransCharsInPlace(path, StrL("#?"), StrL("\0\0"));
    path.len = len(path.s);
    DecodeInPlace(path);
    return path;
}

TempStr GetFileNameTemp(Str url) {
    TempStr path = str::DupTemp(url);
    str::TransCharsInPlace(path, StrL("#?"), StrL("\0\0"));
    path.len = len(path.s);
    int base = path.len;
    for (; base > 0; base--) {
        if ('/' == path.s[base - 1] || '\\' == path.s[base - 1]) {
            break;
        }
    }
    Str baseStr(path.s + base, path.len - base);
    if (str::IsEmpty(baseStr)) {
        return {};
    }
    TempStr res = str::DupTemp(baseStr);
    DecodeInPlace(res);
    return res;
}

} // namespace url

int ParseInt(Str s) {
    if (!s) {
        return 0;
    }
    int off = 0;
    bool negative = s.s[0] == '-';
    if (negative) {
        off = 1;
    }
    int value = 0;
    int overflowCheck = negative ? 1 : 0;
    for (; off < s.len && str::IsDigit(s.s[off]); off++) {
        value = value * 10 + (s.s[off] - '0');
        // return 0 on overflow
        if (value - overflowCheck < 0) {
            return 0;
        }
    }
    return negative ? -value : value;
}

i64 ParseInt64(Str s) {
    if (!s) {
        return 0;
    }
    int off = 0;
    bool negative = s.s[0] == '-';
    if (negative) {
        off = 1;
    }
    i64 value = 0;
    for (; off < s.len && str::IsDigit(s.s[off]); off++) {
        value = value * 10 + (s.s[off] - '0');
    }
    return negative ? -value : value;
}

// the only valid chars are 0-9, . and newlines.
// a valid version has to match the regex /^\d+(\.\d+)*(\r?\n)?$/
// Return false if it contains anything else.
bool IsValidProgramVersion(Str txt) {
    if (!txt || !str::IsDigit(txt.s[0])) {
        return false;
    }

    for (int i = 0; i < txt.len; i++) {
        char c = txt.s[i];
        if (str::IsDigit(c)) {
            continue;
        }
        if (c == '.' && i + 1 < txt.len && str::IsDigit(txt.s[i + 1])) {
            continue;
        }
        if (c == '\r' && i + 1 < txt.len && txt.s[i + 1] == '\n') {
            continue;
        }
        if (c == '\n' && i + 1 == txt.len) {
            continue;
        }
        return false;
    }

    return true;
}

static unsigned int ExtractNextNumber(Str txt, int& off) {
    unsigned int val = 0;
    Str slice = off < txt.len ? Str(txt.s + off, txt.len - off) : Str{};
    Str next = str::Parse(slice, "%u%?.", &val);
    if (next) {
        off += (int)(next.s - slice.s);
    } else {
        off = txt.len;
    }
    return val;
}

// compare two version string. Return 0 if they are the same,
// > 0 if the first is greater than the second and < 0 otherwise.
// e.g.
//   0.9.3.900 is greater than 0.9.3
//   1.09.300 is greater than 1.09.3 which is greater than 1.9.1
//   1.2.0 is the same as 1.2
int CompareProgramVersion(Str txt1, Str txt2) {
    int off1 = 0;
    int off2 = 0;
    while (off1 < txt1.len || off2 < txt2.len) {
        unsigned int v1 = ExtractNextNumber(txt1, off1);
        unsigned int v2 = ExtractNextNumber(txt2, off2);
        if (v1 != v2) {
            return (int)v1 - (int)v2;
        }
    }
    return 0;
}

// shorten a string to maxLen characters, adding ellipsis in the middle
// ascii version that doesn't handle UTF-8
static TempStr ShortenStringTemp(Str s, int maxLen) {
    int sLen = len(s);
    if (sLen <= maxLen) {
        return s;
    }
    char* ret = AllocArrayTemp<char>(maxLen + 2);
    const int half = maxLen / 2;
    // copy first N/2 characters, move last N/2 characters to the halfway point
    for (int i = 0; i < half; i++) {
        ret[i] = s.s[i];
        ret[i + half] = s.s[sLen - half + i];
    }
    // add ellipsis in the middle
    ret[half - 2] = ret[half - 1] = ret[half] = '.';
    return Str(ret, maxLen + 2);
}

// shorten a string to maxRunes characters, adding ellipsis at the end
// works correctly with utf8 strings
TempStr ShortenStringUtf8Temp(Str s, int maxRunes) {
    int nRunes = utf8StrLen((u8*)s.s);
    if (nRunes < 0) {
        // not a valid utf8, fall back to byte truncation
        int sLen = len(s);
        if (sLen <= maxRunes) {
            return s;
        }
        int keep = maxRunes - 3; // 3 for "..."
        if (keep < 0) {
            keep = 0;
        }
        char* ret = AllocArrayTemp<char>(keep + 4);
        memcpy(ret, s.s, keep);
        ret[keep] = '.';
        ret[keep + 1] = '.';
        ret[keep + 2] = '.';
        ret[keep + 3] = 0;
        return Str(ret, keep + 3);
    }
    if (nRunes <= maxRunes) {
        return s;
    }
    int keep = maxRunes - 3; // 3 for "..."
    if (keep < 0) {
        keep = 0;
    }
    // over-allocate the result by 4x to be always safe
    char* ret = AllocArrayTemp<char>(maxRunes * 4 + 1);
    int src = 0;
    int tmp = 0;
    int n;
    for (int i = 0; i < keep; i++) {
        n = utf8RuneLen((const u8*)(s.s + src));
        ReportIf(n <= 0);
        switch (n) {
            default:
                ReportIf(true);
                break;
            case 4:
                ret[tmp++] = s.s[src++];
                __fallthrough;
            case 3:
                ret[tmp++] = s.s[src++];
                __fallthrough;
            case 2:
                ret[tmp++] = s.s[src++];
                __fallthrough;
            case 1:
                ret[tmp++] = s.s[src++];
        }
    }
    ret[tmp++] = '.';
    ret[tmp++] = '.';
    ret[tmp++] = '.';
    ret[tmp] = 0;
    return Str(ret, tmp);
}

// shorten a string to maxLen characters, adding ellipsis in the middle
// works correctly with utf8 strings
TempStr ShortenStringUtf8InTheMiddleTemp(Str s, int maxRunes) {
    int nRunes = utf8StrLen((u8*)s.s);
    if (nRunes < 0) {
        // not a valid utf8
        return ShortenStringTemp(s, maxRunes);
    }
    if (nRunes <= maxRunes) {
        return s;
    }
    int toRemove = (nRunes - maxRunes) + 3; // 3 for "..."
    int removeStartingAt = (nRunes / 2) - (toRemove / 2);
    // over-allocate the result by 4x to be always safe
    char* ret = AllocArrayTemp<char>(maxRunes * 4 + 1);
    int src = 0;
    int tmp = 0;
    int n;
    for (int i = 0; i < nRunes; i++) {
        n = utf8RuneLen((const u8*)(s.s + src));
        ReportIf(n <= 0);
        if (i < removeStartingAt || i >= removeStartingAt + toRemove) {
            switch (n) {
                default:
                    ReportIf(true);
                    break;
                case 4:
                    ret[tmp++] = s.s[src++];
                    __fallthrough;
                case 3:
                    ret[tmp++] = s.s[src++];
                    __fallthrough;
                case 2:
                    ret[tmp++] = s.s[src++];
                    __fallthrough;
                case 1:
                    ret[tmp++] = s.s[src++];
            }
        } else if (i == removeStartingAt) {
            ret[tmp++] = '.';
            ret[tmp++] = '.';
            ret[tmp++] = '.';
            src += n;
        } else {
            src += n;
        }
    }
    return Str(ret, tmp);
}

// IsTextRtl is optimized version of checking if a string is rtl
// we look at max first 40 chars and
bool IsTextRtl(WStr s) {
    if (!s) {
        return false;
    }
    int len = s.len > 40 ? 40 : s.len;
    int nRtl = 0;
    int nLtr = 0;
    WORD* charTypes = AllocArray<WORD>(GetTempArena(), len + 1);
    if (!GetStringTypeExW(LOCALE_INVARIANT, CT_CTYPE2, s.s, len, charTypes)) {
        return false; // API failure
    }
    for (int i = 0; i < len; ++i) {
        WORD type = charTypes[i];
        if (type == C2_LEFTTORIGHT) {
            nLtr++;
        } else if (type == C2_RIGHTTOLEFT) {
            nRtl++;
        }
    }
    return nRtl > nLtr;
}

bool IsTextRtl(Str s) {
    TempWStr ws = ToWStrTemp(s);
    return IsTextRtl(ws);
}

// ---- temp-arena variants of the str:: functions above ----

namespace str {
TempStr DupTemp(Str s) {
    return Dup(GetTempArena(), s);
}

TempWStr DupTemp(WStr s) {
    return wstr::Dup(GetTempArena(), s);
}

TempStr JoinTemp(Str s1, Str s2, Str s3) {
    return Join(GetTempArena(), s1, s2, s3);
}

TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4) {
    return Join(GetTempArena(), s1, s2, s3, s4, Str{});
}

TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4, Str s5) {
    return Join(GetTempArena(), s1, s2, s3, s4, s5);
}

TempWStr JoinTemp(WStr s1, WStr s2, WStr s3) {
    return wstr::Join(GetTempArena(), s1, s2, s3);
}

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith) {
    if (str::IsNull(s) || str::IsEmpty(toReplace) || str::IsNull(replaceWith)) {
        return {};
    }

    Str curr = s;
    int idx = str::IndexOf(curr, toReplace);
    if (idx < 0) {
        // optimization: nothing to replace so do nothing
        return s;
    }

    int findLen = toReplace.len;
    int replLen = replaceWith.len;
    int lenDiff = 0;
    if (replLen > findLen) {
        lenDiff = replLen - findLen;
    }
    // heuristic: allow 6 replacements without reallocating
    int capHint = s.len + 1 + lenDiff * 6;
    str::Builder result(capHint);
    bool ok;
    while (idx >= 0) {
        ok = result.Append(Str(curr.s, idx));
        if (!ok) {
            return {};
        }
        ok = result.Append(Str(replaceWith.s, replLen));
        if (!ok) {
            return {};
        }
        curr = Str(curr.s + idx + findLen, curr.len - idx - findLen);
        idx = str::IndexOf(curr, toReplace);
    }
    ok = result.Append(curr);
    if (!ok) {
        return {};
    }
    return ToStrTemp(result);
}

TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith) {
    int n = toReplace.len;
    int idx = str::IndexOfI(s, toReplace);
    if (idx < 0) {
        return s;
    }
    char* pos = s.s + idx;
    if (!memeq(pos, toReplace.s, n)) {
        toReplace = str::DupTemp(Str(pos, n));
    }
    return str::ReplaceTemp(s, toReplace, replaceWith);
}
} // namespace str

// Temporary, guaranteed zero-terminated copy, for passing to C / win32 APIs
// that require a NUL-terminated string.
char* CStrTemp(Str s) {
    return str::DupTemp(s).s;
}

WCHAR* CWStrTemp(WStr s) {
    return str::DupTemp(s).s;
}

// converts a UTF-8 Str to a NUL-terminated WCHAR* temp (same intent as the
// WStr overload, but also transcodes); use when the wide result is only needed
// as a C/win32 string pointer (never for its length)
WCHAR* CWStrTemp(Str s) {
    return ToWStrTemp(s).s;
}

WCHAR* CWStrTemp(Str s, int& cch) {
    WStr ws = ToWStrTemp(s);
    cch = ws.len;
    return ws.s;
}

WCHAR* CWStrTemp(WStr s, int& cch) {
    WStr ws = str::DupTemp(s);
    cch = ws.len;
    return ws.s;
}

// handles embedded 0 in the string
Str ToStr(const str::Builder& b) {
    return Str(b.els, (int)b.len);
}

// NO_INLINE: this is called in many places; keeping it out of line trims code size
NO_INLINE TempStr ToStrTemp(const str::Builder& b) {
    return str::DupTemp(ToStr(b));
}

// str::Builder always keeps its data NUL-terminated, so we can hand out the
// buffer directly for C/win32 APIs we don't control that want a char*
char* ToCStr(const str::Builder& b) {
    return b.els;
}

WStr ToWStr(const wstr::Builder& b) {
    return WStr(b.els, (int)b.len);
}

WCHAR* ToWCStr(const wstr::Builder& b) {
    return b.els;
}

// --- begin: merged from former src/common/str_util.cpp ---
wchar_t ToLowerW(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return c + (L'a' - L'A');
    return c;
}

int WStrFindSubstr(WStr str, WStr substr) {
    if (IsEmpty(substr)) return -1; // Empty search - no highlight
    if (substr.len > str.len) return -1;

    for (int i = 0; i <= str.len - substr.len; i++) {
        bool match = true;
        for (int j = 0; j < substr.len; j++) {
            if (ToLowerW(str.s[i + j]) != ToLowerW(substr.s[j])) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return -1;
}

int WStrCmpNoCase(WStr a, WStr b) {
    int minLen = a.len < b.len ? a.len : b.len;
    for (int i = 0; i < minLen; i++) {
        wchar_t ca = ToLowerW(a.s[i]);
        wchar_t cb = ToLowerW(b.s[i]);
        if (ca != cb) return ca - cb;
    }
    return a.len - b.len;
}

static wchar_t emptyWideStr[1] = {0};

#if 0
// Convert UTF-8 to UTF-16 (wide string), allocate with gTempArena
WStr ToWStrTemp(const char* utf8) {
    if (!utf8 || !utf8[0]) {
        return WStr(&emptyWideStr[0], 0);
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    wchar_t* wide = (wchar_t*)AllocTemp(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return WStr(wide, len - 1); // Exclude null terminator from length
}
#endif

// Convert UTF-16 to UTF-8
Str ToUtf8(Arena* arena, WStr wide) {
    if (IsEmpty(wide)) {
        return Str();
    }
    // Use explicit length instead of -1 (null-terminated)
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.s, wide.len, nullptr, 0, nullptr, nullptr);
    char* utf8 = (char*)Alloc(arena, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wide.s, wide.len, utf8, len, nullptr, nullptr);
    utf8[len] = 0;
    return Str(utf8, len);
}

Str ToUtf8Temp(WStr wide) {
    return ToUtf8(GetTempArena(), wide);
}

// Convert Str to wide string (optimized - uses known length)
WStr ToWStrTemp(Str s) {
    if (IsEmpty(s)) {
        return WStr(&emptyWideStr[0], 0);
    }
    // Use explicit length instead of -1 (null-terminated)
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, nullptr, 0);
    wchar_t* wide = (wchar_t*)AllocTemp((wideLen + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, wide, wideLen);
    wide[wideLen] = 0;
    return WStr(wide, wideLen);
}

bool IsWhiteSpace(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return true;
        default:
            return false;
    }
}

// Format file size with comma separators, returns Str
Str FormatFileSize(Arena* arena, u64 size) {
    char buf[32];

    if (size == 0) {
        return str::Dup(arena, StrL("0"));
    }

    // Convert to string (reversed)
    char temp[32];
    int i = 0;
    while (size > 0 && i < 31) {
        temp[i++] = '0' + (size % 10);
        size /= 10;
    }
    int numDigits = i;

    // Calculate position of first comma (from left)
    int firstCommaAfter = numDigits % 3;
    if (firstCommaAfter == 0) firstCommaAfter = 3;

    // Reverse into buf with comma separators
    int j = 0;
    int digitPos = 0;
    while (i > 0 && j < 31) {
        buf[j++] = temp[--i];
        digitPos++;
        if (digitPos == firstCommaAfter || (digitPos > firstCommaAfter && (digitPos - firstCommaAfter) % 3 == 0)) {
            if (i > 0 && j < 31) {
                buf[j++] = ',';
            }
        }
    }

    return str::Dup(arena, Str(buf, j));
}

// Format file size with comma separators directly into wide string buffer
void FormatFileSizeToWstrBuf(u64 size, WStr buf) {
    if (buf.len < 1) return;

    if (size == 0) {
        buf.s[0] = L'0';
        buf.s[1] = 0;
        return;
    }

    // Convert to string (reversed)
    wchar_t temp[32];
    int i = 0;
    while (size > 0 && i < 31) {
        temp[i++] = L'0' + (size % 10);
        size /= 10;
    }
    int numDigits = i;

    // Calculate position of first comma (from left)
    int firstCommaAfter = numDigits % 3;
    if (firstCommaAfter == 0) firstCommaAfter = 3;

    // Reverse into buf with comma separators
    int j = 0;
    int digitPos = 0;
    int maxLen = buf.len - 1; // Leave room for null terminator
    while (i > 0 && j < maxLen) {
        buf.s[j++] = temp[--i];
        digitPos++;
        if (digitPos == firstCommaAfter || (digitPos > firstCommaAfter && (digitPos - firstCommaAfter) % 3 == 0)) {
            if (i > 0 && j < maxLen) {
                buf.s[j++] = L',';
            }
        }
    }
    buf.s[j] = 0;
}

// Format size in human readable form (e.g., "1.23 GB", "456 KB")
// Returns length written (excluding null terminator)
int FormatSizeHumanIntoBuf(u64 size, Str buf) {
    if (buf.len < 2) return 0;

    const u64 GB = 1024ULL * 1024 * 1024;
    const u64 MB = 1024ULL * 1024;
    const u64 KB = 1024ULL;

    Str suffix;
    u64 divisor;

    if (size >= GB) {
        suffix = StrL(" GB");
        divisor = GB;
    } else if (size >= MB) {
        suffix = StrL(" MB");
        divisor = MB;
    } else if (size >= KB) {
        suffix = StrL(" KB");
        divisor = KB;
    } else {
        // Bytes - just format as integer
        int len = snprintf(buf.s, buf.len, "%llu B", size);
        return len < buf.len ? len : buf.len - 1;
    }

    // Calculate with 2 decimal precision
    u64 whole = size / divisor;
    u64 remainder = size % divisor;
    int frac = (int)((remainder * 100) / divisor);

    int len;
    if (frac == 0) {
        len = snprintf(buf.s, buf.len, "%llu%s", whole, suffix.s);
    } else if (frac % 10 == 0) {
        len = snprintf(buf.s, buf.len, "%llu.%d%s", whole, frac / 10, suffix.s);
    } else {
        len = snprintf(buf.s, buf.len, "%llu.%02d%s", whole, frac, suffix.s);
    }
    return len < buf.len ? len : buf.len - 1;
}

// Wrapper that formats into wide string buffer
void FormatSizeHumanIntoWBuf(u64 size, WStr wbuf) {
    char temp[32];
    int len = FormatSizeHumanIntoBuf(size, Str(temp, 32));

    // Copy to wide buffer
    int maxLen = wbuf.len - 1;
    int i = 0;
    while (i < len && i < maxLen) {
        wbuf.s[i] = (wchar_t)temp[i];
        i++;
    }
    wbuf.s[i] = 0;
}

static bool IsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void SplitStrByWhitespace(Arena* arena, const Str& s, VecStr& vecOut) {
    vecOut.len = 0;
    vecOut.cap = 0;
    vecOut.els = nullptr;

    int i = 0;
    while (i < s.len) {
        // Skip whitespace
        while (i < s.len && IsWhitespace(s.s[i])) {
            i++;
        }
        if (i >= s.len) break;

        // Find end of token
        int start = i;
        while (i < s.len && !IsWhitespace(s.s[i])) {
            i++;
        }

        // Add token (points into original string, no allocation)
        Str token(s.s + start, i - start);
        VecPush(arena, vecOut, token);
    }
}
// --- end: merged from former src/common/str_util.cpp ---
