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

#if defined(_MSC_VER)
static _locale_t GetUtf8FormatLocale() {
    static _locale_t loc = _create_locale(LC_ALL, ".UTF-8");
    return loc;
}
#endif

static int VsnprintfUtf8(char* buf, size_t bufCchSize, Str fmt, va_list args) { // str-port: C-string
    char* fmtZ = CStrTemp(fmt);
#if defined(_MSC_VER)
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vsnprintf_l(buf, bufCchSize, fmtZ, loc, args);
    }
#endif
    return vsnprintf(buf, bufCchSize, fmtZ, args);
}

static int VscprintfUtf8(Str fmt, va_list args) {
    char* fmtZ = CStrTemp(fmt);
#if defined(_MSC_VER)
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vscprintf_l(fmtZ, loc, args);
    }
#endif
    va_list argsCopy;
    va_copy(argsCopy, args);
    int res = vsnprintf(nullptr, 0, fmtZ, argsCopy);
    va_end(argsCopy);
    return res;
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

bool IsEqual(const ByteSlice& d1, const ByteSlice& d2) {
    if (d1.sz != d2.sz) {
        return false;
    }
    if (d1.sz == 0) {
        return true;
    }
    ReportIf(!d1.d || !d2.d);
    int res = memcmp(d1.d, d2.d, d1.sz);
    return res == 0;
}

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

static Str WrapAllocated(char* s, size_t cch = (size_t)-1) { // str-port: owned heap
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
    return WrapAllocated((char*)MemDup(a, s.s, cch * sizeof(char), sizeof(char)), cch); // str-port: owned heap
}

Str Dup(Str s) {
    return Dup(nullptr, s);
}

Str Dup(const ByteSlice& d) {
    return Dup(AsStr(d));
}

} // namespace str
namespace wstr {

static WStr WrapAllocatedW(WCHAR* s, size_t cch = (size_t)-1) { // str-port: owned heap
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
    return WrapAllocatedW((WCHAR*)MemDup(a, s.s, cch * sizeof(WCHAR), sizeof(WCHAR)), cch); // str-port: owned heap
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

bool Eq(const ByteSlice& sp1, const ByteSlice& sp2) {
    if (sp1.size() != sp2.size()) {
        return false;
    }
    if (sp1.empty()) {
        return true;
    }
    return memeq(sp1.data(), sp2.data(), sp1.size());
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
    return EqN(s, prefix, Len(prefix));
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(Str s, Str prefix) {
    if (s.s == prefix.s) {
        return true;
    }
    if (!s || !prefix) {
        return false;
    }
    return 0 == _strnicmp(s.s, prefix.s, str::Len(prefix));
}

bool Contains(Str s, Str txt) {
    Str found = str::Find(s, txt);
    return (bool)found;
}

bool ContainsI(Str s, Str txt) {
    Str found = str::FindI(s, txt);
    return (bool)found;
}

bool EndsWith(Str txt, Str end) {
    if (!txt || !end) {
        return false;
    }
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen) {
        return false;
    }
    return str::Eq(Str(txt.s + txtLen - endLen, (int)endLen), end);
}

bool EndsWithI(Str txt, Str end) {
    if (!txt || !end) {
        return false;
    }
    size_t txtLen = str::Len(txt);
    size_t endLen = str::Len(end);
    if (endLen > txtLen) {
        return false;
    }
    return str::EqI(Str(txt.s + txtLen - endLen, (int)endLen), end);
}

bool EqNIx(Str s, size_t len, Str s2) {
    return str::Len(s2) == len && str::StartsWithI(s, s2);
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

Str FindI(Str s, Str toFind) {
    if (!s || !toFind) {
        return {};
    }

    if (toFind.len <= 0) {
        return s;
    }
    char first = (char)tolower(toFind.s[0]);
    if (!first) {
        return s;
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
                    return Str(s.s + off, s.len - off);
                }
            }
        }
        return {};
    }

    // Unicode path: case-fold both strings (UTF-16) and search, then map the
    // match position back to a byte offset in the original UTF-8 string so the
    // returned pointer keeps FindI's contract (a pointer into s).
    //
    // Scratch buffers come from the temporary arena; we restore it to its entry
    // position before returning so repeated calls (e.g. the command palette
    // filtering every item) don't grow the arena unbounded. The result points
    // into the caller's original string s, not the arena, so it stays valid.
    ArenaSavepoint sp = ArenaGetSavepoint(GetTempArena());

    TempWStr ws = ToWStrTemp(s); // unfolded, used to map the match back to bytes
    TempWStr wsLo = str::DupTemp(ws);
    TempWStr wfLo = ToWStrTemp(toFind);
    FoldCaseForFindW(wsLo);
    FoldCaseForFindW(wfLo);

    Str res = {};
    int idx = WStrFindSubstr(wsLo, wfLo); // common/str_util.cpp
    if (idx >= 0) {
        int nbytes = 0;
        if (idx > 0) {
            nbytes = WideCharToMultiByte(CP_UTF8, 0, ws.s, idx, nullptr, 0, nullptr, nullptr);
        }
        res = Str(s.s + nbytes, s.len - nbytes);
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
    size_t s1Len = str::Len(s1);
    size_t s2Len = str::Len(s2);
    size_t s3Len = str::Len(s3);
    size_t s4Len = str::Len(s4);
    size_t s5Len = str::Len(s5);
    size_t len = s1Len + s2Len + s3Len + s4Len + s5Len + 1;
    char* res = (char*)Alloc(allocator, len); // str-port: owned heap

    char* s = res; // str-port: owned heap
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

    return Str(res, (int)(len - 1));
}

Str Join(Arena* allocator, Str s1, Str s2, Str s3) {
    return Join(allocator, s1, s2, s3, Str{}, Str{});
}

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
Str Join(Str s1, Str s2, Str s3) {
    return Join(nullptr, s1, s2, s3);
}

} // namespace str
namespace wstr {

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
WStr Join(Arena* allocator, WStr s1, WStr s2, WStr s3) {
    // don't use str::Format(L"%s%s%s", s1, s2, s3) since the strings
    // might contain non-characters which str::Format fails to handle
    size_t s1Len = (size_t)s1.len, s2Len = (size_t)s2.len, s3Len = (size_t)s3.len;
    size_t len = s1Len + s2Len + s3Len + 1;
    WCHAR* res = (WCHAR*)Alloc(allocator, len * sizeof(WCHAR)); // str-port: owned heap
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
void Utf8Encode(char* buf, int& off, int c) { // str-port: owned heap
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
    off = (int)((char*)tmp - buf); // str-port: C-string
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

int FindCharIdx(Str str, char c) {
    if (!str) {
        return -1;
    }
    for (int i = 0; i < str.len; i++) {
        if (str.s[i] == c) {
            return i;
        }
    }
    return -1;
}

Str FindChar(Str str, char c) {
    int idx = FindCharIdx(str, c);
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

int BufFind(Str buf, Str toFind) {
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

Str Find(Str str, Str find) {
    int idx = BufFind(str, find);
    if (idx < 0) {
        return {};
    }
    return Str(str.s + idx, str.len - idx);
}

// format string to a buffer provided by the caller
// the hope here is to avoid allocating memory (assuming vsnprintf
// doesn't allocate)
bool BufFmtV(char* buf, size_t bufCchSize, Str fmt, va_list args) { // str-port: C-string
    int count = VsnprintfUtf8(buf, bufCchSize, fmt, args);
    buf[bufCchSize - 1] = 0;
    return (count >= 0) && ((size_t)count < bufCchSize);
}

bool BufFmt(char* buf, size_t bufCchSize, Str fmt, ...) { // str-port: C-string
    va_list args;
    va_start(args, fmt);
    auto res = BufFmtV(buf, bufCchSize, fmt, args);
    va_end(args);
    return res;
}

// TODO: need to finish StrFormat and use it instead.
Str FmtVWithArena(Arena* a, Str fmt, va_list args) {
    char message[512]{};
    va_list argsCopy;
    va_copy(argsCopy, args);
    int count = VsnprintfUtf8(message, dimof(message), fmt, argsCopy);
    va_end(argsCopy);
    if ((count >= 0) && (count < dimofi(message))) {
        return str::Dup(a, Str(message, count));
    }

    va_copy(argsCopy, args);
    count = VscprintfUtf8(fmt, argsCopy);
    va_end(argsCopy);
    // happened in https://github.com/sumatrapdfreader/sumatrapdf/issues/878
    // when %S string had certain Unicode characters
    ReportIf(count == -1);
    if (count < 0) {
        return str::Dup(a, StrL("vsnprintf() returned -1"));
    }

    char* buf = AllocArray<char>(a, count + 1); // str-port: owned heap
    if (!buf) {
        return {};
    }

    va_copy(argsCopy, args);
    int count2 = VsnprintfUtf8(buf, (size_t)count + 1, fmt, argsCopy);
    va_end(argsCopy);
    ReportIf(count2 != count);
    if (count2 < 0) {
        Free(a, buf);
        return str::Dup(a, StrL("vsnprintf() returned -1"));
    }
    return Str(buf, count);
}

Str FmtV(Str fmt, va_list args) {
    return FmtVWithArena(nullptr, fmt, args);
}

// caller needs to str::Free()
Str Format(Str fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Str res = FmtV(fmt, args);
    va_end(args);
    return res;
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
        Str found = str::FindChar(oldChars, str.s[i]);
        if (found) {
            int idx = (int)(found.s - oldChars.s);
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
        if (!str::FindChar(toRemove, c)) {
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
        if (!wstr::FindChar(toRemove, c)) {
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
    char* ret = AllocArrayTemp<char>(2 * len + 1); // str-port: temp arena
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
    Str found = FindChar(slice, c);
    if (str::IsNull(found)) {
        return {};
    }
    int endOff = (int)(found.s - str.s);
    *endOffOut = endOff;
    return str::Dup(Str(str.s + off, endOff - off));
}

static int ParseLimitedNumber(Str str, int p, int formatOff, Str format, int* endOffOut, void* valueOut) {
    unsigned int width;
    char f2[] = "% ";
    Str formatAt = Str(format.s + formatOff);
    Str endF = Parse(formatAt, "%u%c", &width, &f2[1]);
    if (!str::IsNull(endF) && !str::IsNull(FindChar(StrL("udx"), f2[1])) && width <= (unsigned)(str.len - p)) {
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
        char* endPtr = nullptr; // str-port: C-string
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

Str Parse(Str str, Str fmt, ...) {
    if (str::IsNull(str) || str::IsNull(fmt)) {
        return {};
    }

    va_list args;
    va_start(args, fmt);
    Str res = ParseV(str, fmt, args);
    va_end(args);
    return res;
}

Str Parse(Str str, size_t len, Str fmt, ...) {
    if (str::IsNull(str) || str::IsNull(fmt)) {
        return {};
    }

    int useLen = (int)std::min(len, (size_t)str.len);
    Str bounded = Str(str.s, useLen);

    va_list args;
    va_start(args, fmt);
    Str res = ParseV(bounded, fmt, args);
    va_end(args);

    if (str::IsNull(res)) {
        return {};
    }
    int off = (int)(res.s - bounded.s);
    return Str(str.s + off, str.len - off);
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
Str SeqStrAt(SeqStrings strs, int off) {
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
    off += str::Leni(strs + off) + 1;
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
Str SeqStrByIndex(SeqStrings strs, int idx) {
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
    int next = off + str::Leni(strs + off) + 1;
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
    const u8* p = (const u8*)(strs + off + str::Leni(strs + off) + 1);
    if (numOut) {
        VarIntDecode(p, numOut);
    }
}

void SeqStrNumAppend(StrBuilder* b, Str s, i64 num) {
    b->Append(s);
    b->AppendChar('\0');
    u8 buf[12];
    size_t n = VarIntEncode(buf, num);
    b->AppendSlice(ByteSlice(buf, n));
}

void SeqStrNumFinish(StrBuilder* b) {
    b->AppendChar('\0');
}

Str SeqStrNumAt(SeqStrNum strs, int off) {
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

Str SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut) {
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

Str SeqStrNumStrByNumber(SeqStrNum strs, i64 num) {
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

static char* EnsureCap(StrBuilder* s, size_t needed) { // str-port: owned heap
    if (needed + kPadding <= StrBuilder::kBufChars) {
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
    char* newEls; // str-port: owned heap
    if (s->buf == s->els) {
        newEls = (char*)Alloc(s->allocator, allocSize); // str-port: owned heap
        if (newEls) {
            memcpy(newEls, s->buf, s->len + 1);
        }
    } else {
        newEls = (char*)Realloc(s->allocator, s->els, allocSize); // str-port: owned heap
    }
    if (!newEls) {
        ReportIf(InterlockedExchangeAdd(&gAllowAllocFailure, 0) == 0);
        return nullptr;
    }
    s->els = newEls;
    s->cap = (u32)newCap;
    return newEls;
}

static char* MakeSpaceAt(StrBuilder* s, size_t idx, size_t count) { // str-port: owned heap
    ReportIf(count == 0);
    u32 newLen = std::max(s->len, (u32)idx) + (u32)count;
    char* buf = EnsureCap(s, newLen); // str-port: owned heap
    if (!buf) {
        return nullptr;
    }
    buf[newLen] = 0;
    char* res = &(buf[idx]); // str-port: owned heap
    if (s->len > idx) {
        // inserting in the middle of string, have to copy
        char* src = buf + idx;         // str-port: owned heap
        char* dst = buf + idx + count; // str-port: owned heap
        memmove(dst, src, s->len - idx);
    }
    s->len = newLen;
    // ZeroMemory(res, count);
    return res;
}

static void StrBuilderFree(StrBuilder* s) {
    if (!s->els || (s->els == s->buf)) {
        return;
    }
    Free(s->allocator, s->els);
    s->els = nullptr;
}

void StrBuilder::Reset() {
    StrBuilderFree(this);
    len = 0;
    cap = 0;
    els = buf;

#if defined(DEBUG)
#define kFillerStr "01234567890123456789012345678901"
    // to catch mistakes earlier, fill the buffer with a known string
    constexpr size_t nFiller = sizeof(kFillerStr) - 1;
    static_assert(nFiller == StrBuilder::kBufChars);
    memcpy(buf, kFillerStr, kBufChars);
#endif

    buf[0] = 0;
}

// allocator is not owned by Vec and must outlive it
StrBuilder::StrBuilder(size_t capHint, Arena* a) {
    allocator = a;
    Reset();
    cap = (u32)(capHint + kPadding); // + kPadding for terminating 0
}

// ensure that a Vec never shares its els buffer with another after a clone/copy
// note: we don't inherit allocator as it's not needed for our use cases
StrBuilder::StrBuilder(const StrBuilder& that) {
    Reset();
    char* s = EnsureCap(this, that.len); // str-port: owned heap
    Str sOrig = that.Get();
    len = that.len;
    size_t n = len + kPadding;
    memcpy(s, sOrig.s, n);
}

StrBuilder::StrBuilder(Str s) {
    Reset();
    Append(s);
}

StrBuilder& StrBuilder::operator=(const StrBuilder& that) {
    if (this == &that) {
        return *this;
    }
    Reset();
    char* s = EnsureCap(this, that.len); // str-port: owned heap
    Str sOrig = that.Get();
    len = that.len;
    size_t n = len + kPadding;
    memcpy(s, sOrig.s, n);
    return *this;
}

StrBuilder::~StrBuilder() {
    StrBuilderFree(this);
}

char& StrBuilder::at(size_t idx) const {
    ReportIf(idx >= (u32)len);
    return els[idx];
}

char& StrBuilder::at(int idx) const {
    ReportIf(idx < 0);
    return at((size_t)idx);
}

char& StrBuilder::operator[](long idx) const {
    ReportIf(idx < 0);
    return at((size_t)idx);
}

char& StrBuilder::operator[](int idx) const {
    ReportIf(idx < 0);
    return at((size_t)idx);
}

#if defined(_WIN64)
char& StrBuilder::at(u32 idx) const {
    return at((size_t)idx);
}

char& StrBuilder::operator[](u32 idx) const {
    return at((size_t)idx);
}
#endif

size_t StrBuilder::size() const {
    return len;
}

int StrBuilder::Size() const {
    return (int)len;
}

bool StrBuilder::InsertAt(size_t idx, char el) {
    char* p = MakeSpaceAt(this, idx, 1); // str-port: owned heap
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool StrBuilder::AppendChar(char c) {
    return InsertAt(len, c);
}

bool StrBuilder::Append(Str src, size_t count) {
    if ((size_t)-1 == count) {
        count = (size_t)src.len;
    }
    if (str::IsNull(src) || 0 == count) {
        return true;
    }
    char* dst = MakeSpaceAt(this, len, count); // str-port: owned heap
    if (!dst) {
        return false;
    }
    memcpy(dst, src.s, count);
    return true;
}

bool StrBuilder::Append(const StrBuilder& s) {
    return Append(s.LendData());
}

char StrBuilder::RemoveAt(size_t idx, size_t count) {
    char res = at(idx);
    if (len > idx + count) {
        char* dst = els + idx;         // str-port: owned heap
        char* src = els + idx + count; // str-port: owned heap
        size_t nToMove = len - idx - count;
        memmove(dst, src, nToMove);
    }
    len -= (u32)count;
    memset(els + len, 0, count);
    return res;
}

char StrBuilder::RemoveLast() {
    if (len == 0) {
        return 0;
    }
    return RemoveAt(len - 1);
}

char& StrBuilder::Last() const {
    ReportIf(0 == len);
    return at(len - 1);
}

// perf hack for using as a buffer: client can get accumulated data
// without duplicate allocation. Note: since Vec over-allocates, this
// is likely to use more memory than strictly necessary, but in most cases
// it doesn't matter
Str StrBuilder::StealData(Arena* a) {
    int n = (int)len;
    char* res = els; // str-port: owned heap
    if (a) {
        // if allocator is specified, have to duplicate
        res = (char*)MemDup(a, els, len + kPadding); // str-port: owned heap
    } else {
        if (els == buf) {
            a = (a != nullptr) ? a : this->allocator;
            res = (char*)MemDup(a, els, len + kPadding); // str-port: owned heap
        } else {
            // we're returning els, so reset to small buf
            els = buf;
        }
    }

    Reset();
    return Str(res, n);
}

Str StrBuilder::LendData() const {
    return Get();
}

bool StrBuilder::Contains(Str s) {
    if (!s) {
        return false;
    }
    int sLen = s.len;
    if (sLen > (int)len) {
        return false;
    }
    // must account for possibility of 0 in the string
    char c = s.s[0];
    int nLeft = (int)len - sLen;
    for (int i = 0; i <= nLeft; i++) {
        if (c != els[i]) {
            continue;
        }
        if (str::EqN(s.s, els + i, (size_t)sLen)) {
            return true;
        }
    }
    return false;
}

bool StrBuilder::IsEmpty() const {
    return len == 0;
}

ByteSlice StrBuilder::AsByteSlice() const {
    return {(u8*)els, size()};
}

ByteSlice StrBuilder::StealAsByteSlice() {
    size_t n = size();
    Str d = StealData();
    return {(u8*)d.s, n};
}

bool StrBuilder::Append(const u8* src, size_t size) {
    if ((size_t)-1 == size) {
        return this->Append(Str((const char*)src)); // str-port: C-string
    }
    return AppendSlice(ByteSlice(src, size));
}

bool StrBuilder::AppendSlice(const ByteSlice& d) {
    if (d.empty()) {
        return true;
    }
    return this->Append(AsStr(d));
}

void StrBuilder::AppendFmt(Str fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Str res = str::FmtV(fmt, args);
    if (res) {
        Append(res);
        str::Free(res);
    }
    va_end(args);
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

void StrBuilder::Set(Str s) {
    Reset();
    Append(s);
}

Str StrBuilder::Get() const {
    return Str(els, (int)len);
}

Str StrBuilder::CStr() const {
    return Get();
}

char StrBuilder::LastChar() const {
    auto n = this->len;
    if (n == 0) {
        return 0;
    }
    return at(n - 1);
}

static WCHAR* EnsureCap(WStrBuilder* s, size_t needed) { // str-port: owned heap
    if (needed + kPadding <= StrBuilder::kBufChars) {
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

    size_t allocSize = newElCount * WStrBuilder::kElSize;
    WCHAR* newEls; // str-port: owned heap
    if (s->buf == s->els) {
        newEls = (WCHAR*)Alloc(s->allocator, allocSize); // str-port: owned heap
        if (newEls) {
            memcpy(newEls, s->buf, WStrBuilder::kElSize * (s->len + 1));
        }
    } else {
        newEls = (WCHAR*)Realloc(s->allocator, s->els, allocSize); // str-port: owned heap
    }

    if (!newEls) {
        ReportIf(InterlockedExchangeAdd(&gAllowAllocFailure, 0) == 0);
        return nullptr;
    }
    s->els = newEls;
    s->cap = (u32)newCap;
    return newEls;
}

static WCHAR* MakeSpaceAt(WStrBuilder* s, size_t idx, size_t count) { // str-port: owned heap
    ReportIf(count == 0);
    u32 newLen = std::max(s->len, (u32)idx) + (u32)count;
    WCHAR* buf = EnsureCap(s, newLen); // str-port: owned heap
    if (!buf) {
        return nullptr;
    }
    buf[newLen] = 0;
    WCHAR* res = &(buf[idx]); // str-port: owned heap
    if (s->len > idx) {
        WCHAR* src = buf + idx;         // str-port: owned heap
        WCHAR* dst = buf + idx + count; // str-port: owned heap
        memmove(dst, src, (s->len - idx) * WStrBuilder::kElSize);
    }
    s->len = newLen;
    return res;
}

static void WStrBuilderFree(WStrBuilder* s) {
    if (!s->els || (s->els == s->buf)) {
        return;
    }
    Free(s->allocator, s->els);
    s->els = nullptr;
}

void WStrBuilder::Reset() {
    WStrBuilderFree(this);
    len = 0;
    cap = 0;
    els = buf;

#if defined(DEBUG)
#define kFillerWStr L"01234567890123456789012345678901"
    // to catch mistakes earlier, fill the buffer with a known string
    constexpr size_t nFiller = sizeof(kFillerStr) - 1;
    static_assert(nFiller == StrBuilder::kBufChars);
    memcpy(buf, kFillerWStr, nFiller * kElSize);
#endif

    buf[0] = 0;
}

// allocator is not owned by Vec and must outlive it
WStrBuilder::WStrBuilder(size_t capHint, Arena* a) {
    allocator = a;
    Reset();
    cap = (u32)(capHint + kPadding); // + kPadding for terminating 0
}

// ensure that a Vec never shares its els buffer with another after a clone/copy
// note: we don't inherit allocator as it's not needed for our use cases
WStrBuilder::WStrBuilder(const WStrBuilder& that) {
    Reset();
    WCHAR* s = EnsureCap(this, that.cap); // str-port: owned heap
    WStr sOrig = that.Get();
    len = that.len;
    size_t n = (len + kPadding) * kElSize;
    memcpy(s, sOrig.s, n);
}

WStrBuilder::WStrBuilder(WStr s) {
    Reset();
    Append(s);
}

WStrBuilder& WStrBuilder::operator=(const WStrBuilder& that) {
    if (this == &that) {
        return *this;
    }
    Reset();
    WCHAR* s = EnsureCap(this, that.cap); // str-port: owned heap
    WStr sOrig = that.Get();
    len = that.len;
    size_t n = (len + kPadding) * kElSize;
    memcpy(s, sOrig.s, n);
    return *this;
}

WStrBuilder::~WStrBuilder() {
    WStrBuilderFree(this);
}

WCHAR& WStrBuilder::at(size_t idx) const {
    ReportIf(idx >= len);
    return els[idx];
}

WCHAR& WStrBuilder::at(int idx) const {
    ReportIf(idx < 0);
    return at((size_t)idx);
}

WCHAR& WStrBuilder::operator[](size_t idx) const {
    return at(idx);
}

WCHAR& WStrBuilder::operator[](long idx) const {
    ReportIf(idx < 0);
    return at((size_t)idx);
}

WCHAR& WStrBuilder::operator[](ULONG idx) const {
    return at((size_t)idx);
}

WCHAR& WStrBuilder::operator[](int idx) const {
    ReportIf(idx < 0);
    return at((size_t)idx);
}

#if defined(_WIN64)
WCHAR& WStrBuilder::at(u32 idx) const {
    return at((size_t)idx);
}

WCHAR& WStrBuilder::operator[](u32 idx) const {
    return at((size_t)idx);
}
#endif

size_t WStrBuilder::size() const {
    return len;
}
int WStrBuilder::isize() const {
    return (int)len;
}

bool WStrBuilder::InsertAt(size_t idx, const WCHAR& el) {
    WCHAR* p = MakeSpaceAt(this, idx, 1); // str-port: owned heap
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool WStrBuilder::AppendChar(WCHAR c) {
    return InsertAt(len, c);
}

bool WStrBuilder::Append(WStr src, size_t count) {
    if ((size_t)-1 == count) {
        count = (size_t)src.len;
    }
    if (wstr::IsNull(src) || 0 == count) {
        return true;
    }
    WCHAR* dst = MakeSpaceAt(this, len, count); // str-port: owned heap
    if (!dst) {
        return false;
    }
    memcpy(dst, src.s, count * kElSize);
    return true;
}

WCHAR WStrBuilder::RemoveAt(size_t idx, size_t count) {
    WCHAR res = at(idx);
    if (len > idx + count) {
        WCHAR* dst = els + idx;         // str-port: owned heap
        WCHAR* src = els + idx + count; // str-port: owned heap
        memmove(dst, src, (len - idx - count) * kElSize);
    }
    len -= (u32)count;
    memset(els + len, 0, count * kElSize);
    return res;
}

WCHAR WStrBuilder::RemoveLast() {
    if (len == 0) {
        return 0;
    }
    return RemoveAt(len - 1);
}

WCHAR& WStrBuilder::Last() const {
    ReportIf(0 == len);
    return at(len - 1);
}

// perf hack for using as a buffer: client can get accumulated data
// without duplicate allocation. Note: since Vec over-allocates, this
// is likely to use more memory than strictly necessary, but in most cases
// it doesn't matter
WStr WStrBuilder::StealData() {
    int n = (int)len;
    WCHAR* res = els;
    if (els == buf) {
        res = (WCHAR*)MemDup(allocator, buf, (len + kPadding) * kElSize); // str-port: owned heap
    }
    els = buf;
    Reset();
    return WStr(res, n);
}

WStr WStrBuilder::LendData() const {
    return Get();
}

int WStrBuilder::Find(const WCHAR& el, size_t startAt) const {
    for (size_t i = startAt; i < len; i++) {
        if (els[i] == el) {
            return (int)i;
        }
    }
    return -1;
}

bool WStrBuilder::Contains(const WCHAR& el) const {
    return -1 != Find(el);
}

// returns position of removed element or -1 if not removed
int WStrBuilder::Remove(const WCHAR& el) {
    int i = Find(el);
    if (-1 == i) {
        return -1;
    }
    RemoveAt(i);
    return i;
}

bool WStrBuilder::IsEmpty() const {
    return len == 0;
}

void WStrBuilder::Set(WStr s) {
    Reset();
    Append(s);
}

WStr WStrBuilder::Get() const {
    return WStr(els, (int)len);
}

WCHAR WStrBuilder::LastChar() const {
    auto n = this->len;
    if (n == 0) {
        return 0;
    }
    return at(n - 1);
}

namespace wstr {

// returns true if was replaced
bool Replace(WStrBuilder& s, WStr toReplace, WStr replaceWith) {
    // fast path: nothing to replace
    if (!wstr::Find(WStr(s.els), toReplace)) {
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
    return WStr((WCHAR*)s.s, s.len / (int)sizeof(WCHAR)); // str-port: byte reinterpret
}

} // namespace str
namespace wstr {

// return true if s1 == s2, case sensitive
bool Eq(WStr s1, WStr s2) {
    return WStrEq(s1, s2);
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

int FindCharIdx(WStr str, WCHAR c) {
    if (!str) {
        return -1;
    }
    for (int i = 0; i < str.len; i++) {
        if (str.s[i] == c) {
            return i;
        }
    }
    return -1;
}

WStr FindChar(WStr str, WCHAR c) {
    int idx = FindCharIdx(str, c);
    if (idx < 0) {
        return {};
    }
    return WStr(str.s + idx, str.len - idx);
}

WStr Find(WStr str, WStr find) {
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
        WStr pos = wstr::FindChar(oldChars, str.s[i]);
        if (pos) {
            size_t idx = (size_t)(pos.s - oldChars.s);
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

    WStrBuilder result((size_t)s.len);
    int findLen = toReplace.len;
    int start = 0;
    while (start < s.len) {
        WStr rest(s.s + start, s.len - start);
        WStr match = wstr::Find(rest, toReplace);
        if (!match) {
            result.Append(WStr(s.s + start, s.len - start));
            break;
        }
        int matchOff = (int)(match.s - s.s);
        result.Append(WStr(s.s + start, matchOff - start));
        result.Append(replaceWith);
        start = matchOff + findLen;
    }
    return result.StealData();
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
int BufSet(char* dst, int cchDst, Str src) { // str-port: caller-owned out-buffer
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

int BufSet(WCHAR* dst, int cchDst, WStr src) { // str-port: caller-owned out-buffer
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

int BufSet(WCHAR* dst, int dstCchSize, Str src) { // str-port: caller-owned out-buffer
    return wstr::BufSet(dst, dstCchSize, ToWStrTemp(src));
}

// append as much of s at the end of dst (which must be properly null-terminated)
// as will fit.
int BufAppend(char* dst, int dstCch, Str s) { // str-port: caller-owned out-buffer
    ReportIf(0 == dstCch);

    int currDstCchLen = str::Leni(dst);
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
    TempStr buf = fmt::FormatTemp(StrL("%d"), num);

    StrBuilder res;
    int i = 3 - (buf.len % 3);
    for (int src = 0; src < buf.len; src++) {
        res.AppendChar(buf.s[src]);
        if (src + 1 < buf.len && i == 2) {
            res.Append(thousandSep);
        }
        i = (i + 1) % 3;
    }

    return str::DupTemp(res.Get());
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
    TempStr buf = fmt::FormatTemp(StrL("%s%s%02d"), tmp, Str(decimal), num % 100);
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
    return fmt::FormatTemp(StrL("%s %s"), sizestr, unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTemp(i64 size) {
    if (size <= 0) {
        return str::FormatTemp("%d", (int)size);
    }
    TempStr n1 = str::FormatSizeShortTemp(size);
    TempStr n2 = str::FormatNumWithThousandSepTemp(size);
    return fmt::FormatTemp(StrL("%s (%s %s)"), n1, n2, StrL("Bytes"));
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

    StrBuilder roman;
    for (int i = 0; i < dimof(romandata); i++) {
        auto&& el = romandata[i];
        for (; n >= el.value; n -= el.value) {
            roman.Append(el.numeral);
        }
    }
    return str::DupTemp(roman.Get());
}

} // namespace str
namespace wstr {

static WStr ExtractUntilW(WStr str, int off, WCHAR c, int* endOffOut) {
    if (off < 0 || off > str.len) {
        return {};
    }
    WStr slice = WStr(str.s + off, str.len - off);
    WStr found = FindChar(slice, c);
    if (wstr::IsNull(found)) {
        return {};
    }
    int endOff = (int)(found.s - str.s);
    *endOffOut = endOff;
    return wstr::Dup(WStr(str.s + off, endOff - off));
}

static int ParseLimitedNumberW(WStr str, int p, int formatOff, WStr format, int* endOffOut, void* valueOut) {
    unsigned int width;
    WCHAR f2[] = L"% ";
    WStr formatAt = WStr(format.s + formatOff, format.len - formatOff);
    WStr endF = Parse(formatAt, L"%u%c", &width, &f2[1]);
    if (!wstr::IsNull(endF) && !wstr::IsNull(FindChar(WStr(L"udx"), f2[1])) && width <= (unsigned)(str.len - p)) {
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
    WCHAR* sliceZ = AllocArrayTemp<WCHAR>(rem + 1); // str-port: wcstod NUL-term boundary
    memcpy(sliceZ, str.s + off, rem * sizeof(WCHAR));
    sliceZ[rem] = 0;
    WCHAR* endPtr = nullptr; // str-port: wcstod out-param
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

WStr Parse(WStr str, WStr format, ...) {
    if (wstr::IsNull(str) || wstr::IsNull(format)) {
        return {};
    }
    va_list args;
    va_start(args, format);
    WStr res = ParseVW(str, format, args);
    va_end(args);
    return res;
}

} // namespace wstr

namespace url {

bool IsAbsolute(Str url) {
    Str colon = str::FindChar(url, ':');
    Str hash = str::FindChar(url, '#');
    return colon && (!hash || hash.s > colon.s);
}

TempStr GetFullPathTemp(Str url) {
    TempStr path = str::DupTemp(url);
    str::TransCharsInPlace(path, "#?", "\0\0");
    path.len = str::Leni(path.s);
    DecodeInPlace(path);
    return path;
}

TempStr GetFileNameTemp(Str url) {
    TempStr path = str::DupTemp(url);
    str::TransCharsInPlace(path, "#?", "\0\0");
    path.len = str::Leni(path.s);
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
    int sLen = str::Leni(s);
    if (sLen <= maxLen) {
        return s;
    }
    char* ret = AllocArrayTemp<char>(maxLen + 2); // str-port: owned heap
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
        int sLen = str::Leni(s);
        if (sLen <= maxRunes) {
            return s;
        }
        int keep = maxRunes - 3; // 3 for "..."
        if (keep < 0) {
            keep = 0;
        }
        char* ret = AllocArrayTemp<char>(keep + 4); // str-port: temp arena slice
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
    char* ret = AllocArrayTemp<char>(maxRunes * 4 + 1); // str-port: owned heap
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
    char* ret = AllocArrayTemp<char>(maxRunes * 4 + 1); // str-port: owned heap
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
