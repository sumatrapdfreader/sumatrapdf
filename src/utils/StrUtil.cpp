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

static int VsnprintfUtf8(char* buf, size_t bufCchSize, const char* fmt, va_list args) {
#if defined(_MSC_VER)
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vsnprintf_l(buf, bufCchSize, fmt, loc, args);
    }
#endif
    return vsnprintf(buf, bufCchSize, fmt, args);
}

static int VscprintfUtf8(const char* fmt, va_list args) {
#if defined(_MSC_VER)
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vscprintf_l(fmt, loc, args);
    }
#endif
    va_list argsCopy;
    va_copy(argsCopy, args);
    int res = vsnprintf(nullptr, 0, fmt, argsCopy);
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

void Free(const char* s) {
    free((void*)s);
}

void Free(const u8* s) {
    free((void*)s);
}

void Free(const WCHAR* s) {
    free((void*)s);
}

void Free(const Str& s) {
    free(s.s);
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

void FreePtr(Str* s) {
    str::Free(*s);
    *s = {};
}

void FreePtr(WStr* s) {
    str::Free(s->s);
    *s = {};
}

static Str WrapAllocated(char* s, size_t cch = (size_t)-1) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        return Str(s);
    }
    return Str(s, (int)cch);
}

Str Dup(Arena* a, Str s, size_t cch) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        cch = (size_t)s.len;
    }
    return WrapAllocated((char*)MemDup(a, s.s, cch * sizeof(char), sizeof(char)), cch);
}

Str Dup(Str s, size_t cch) {
    return Dup(nullptr, s, cch);
}

Str Dup(const ByteSlice& d) {
    return Dup(Str((char*)d.data(), (int)d.size()));
}

static WStr WrapAllocatedW(WCHAR* s, size_t cch = (size_t)-1) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        return WStr(s);
    }
    return WStr(s, (int)cch);
}

WStr Dup(Arena* a, WStr s, size_t cch) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        cch = (size_t)s.len;
    }
    return WrapAllocatedW((WCHAR*)MemDup(a, s.s, cch * sizeof(WCHAR), sizeof(WCHAR)), cch);
}

WStr Dup(WStr s, size_t cch) {
    return Dup(nullptr, s, cch);
}

// return true if s1 == s2, case sensitive
bool Eq(Str s1, Str s2) {
    return StrEq(s1, s2);
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
bool EqI(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == _stricmp(s1.s, s2.s);
}

// compares two strings ignoring case and whitespace
bool EqIS(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }

    const char* p1 = s1.s;
    const char* p2 = s2.s;
    while (*p1 && *p2) {
        for (; IsWs(*p1); p1++) {
        }
        for (; IsWs(*p2); p2++) {
        }

        if (tolower(*p1) != tolower(*p2)) {
            return false;
        }
        if (*p1) {
            p1++;
            p2++;
        }
    }

    return !*p1 && !*p2;
}

bool EqN(Str s1, Str s2, size_t len) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == strncmp(s1.s, s2.s, len);
}

bool EqNI(Str s1, Str s2, size_t len) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == _strnicmp(s1.s, s2.s, len);
}

bool IsEmpty(Str s) {
    return !s || s.len == 0 || (0 == *s.s);
}

bool StartsWith(Str s, Str prefix) {
    return EqN(s, prefix, Len(prefix));
}

bool StartsWith(const u8* str, Str prefix) {
    return StartsWith(Str((char*)str, prefix.len), prefix);
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
static void FoldCaseForFindW(WCHAR* s, int n) {
    if (n <= 0) {
        return;
    }
    CharLowerBuffW(s, (DWORD)n);
    for (int i = 0; i < n; i++) {
        if (s[i] == 0x0130) {
            s[i] = L'i';
        }
    }
}

Str FindI(Str s, Str toFind) {
    if (!s || !toFind) {
        return {};
    }

    char first = (char)tolower(*toFind.s);
    if (!first) {
        return s;
    }

    // Fast path: an ASCII needle can be matched byte-wise against a UTF-8
    // haystack (ASCII bytes never occur inside multi-byte UTF-8 sequences)
    // without any allocation. The Unicode path below is only needed to
    // case-fold a non-ASCII needle (e.g. Cyrillic), so that case-insensitive
    // search works for non-Latin text too (issue #5717).
    bool asciiNeedle = true;
    for (const char* p = toFind.s; *p; p++) {
        if ((u8)*p >= 0x80) {
            asciiNeedle = false;
            break;
        }
    }
    if (asciiNeedle) {
        const char* p = s.s;
        while (*p) {
            char c = (char)tolower(*p);
            if (c == first) {
                if (str::StartsWithI(Str((char*)p), toFind)) {
                    int off = (int)(p - s.s);
                    return Str(s.s + off, s.len - off);
                }
            }
            p++;
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
    FoldCaseForFindW(wsLo.s, str::Leni(wsLo));
    FoldCaseForFindW(wfLo.s, str::Leni(wfLo));

    Str res = {};
    const WCHAR* m = wcsstr(wsLo.s, wfLo.s);
    if (m) {
        int idx = (int)(m - wsLo.s); // WCHAR index, 1:1 with the unfolded ws
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
        str::Free(s->s);
        *s = snew;
    }
}

void ReplaceWithCopy(Str* s, Str snew) {
    if (s->s != snew.s) {
        str::Free(s->s);
        *s = snew;
    }
}

Str Join(Arena* allocator, Str s1, Str s2, Str s3, Str s4, Str s5) {
    size_t s1Len = str::Len(s1);
    size_t s2Len = str::Len(s2);
    size_t s3Len = str::Len(s3);
    size_t s4Len = str::Len(s4);
    size_t s5Len = str::Len(s5);
    size_t len = s1Len + s2Len + s3Len + s4Len + s5Len + 1;
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

/* Concatenate 2 strings. Any string can be nullptr.
   Caller needs to free() memory. */
WStr Join(Arena* allocator, WStr s1, WStr s2, WStr s3) {
    // don't use str::Format(L"%s%s%s", s1, s2, s3) since the strings
    // might contain non-characters which str::Format fails to handle
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

Str ToLowerInPlace(Str s) {
    for (char* p = s.s; p && *p; p++) {
        *p = (char)tolower(*p);
    }
    return s;
}

Str ToLower(Str s) {
    Str s2 = str::Dup(s);
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

Str FindChar(Str str, char c) {
    if (!str) {
        return {};
    }
    const char* p = strchr(str.s, c);
    if (!p) {
        return {};
    }
    int off = (int)(p - str.s);
    return Str((char*)p, str.len - off);
}

int FindCharIdx(Str str, char c) {
    if (!str) {
        return -1;
    }
    const char* start = str.s;
    for (const char* p = start; *p; p++) {
        if (*p == c) {
            return (int)(p - start);
        }
    }
    return -1;
}

Str FindCharLast(Str str, char c) {
    if (!str) {
        return {};
    }
    const char* p = strrchr(str.s, c);
    if (!p) {
        return {};
    }
    int off = (int)(p - str.s);
    return Str((char*)p, str.len - off);
}

Str Find(Str str, Str find) {
    if (!str || !find) {
        return {};
    }
    const char* p = strstr(str.s, find.s);
    if (!p) {
        return {};
    }
    int off = (int)(p - str.s);
    return Str((char*)p, str.len - off);
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
    const char* end = buf.s + (buf.len - toFindLen);
    const char* s = buf.s;
    while (s < end) {
        if (*s == c) {
            if (memeq((const void*)s, (const void*)toFind.s, (size_t)toFindLen)) {
                return (int)(s - buf.s);
            }
        }
        s++;
    }
    return -1;
}

// format string to a buffer provided by the caller
// the hope here is to avoid allocating memory (assuming vsnprintf
// doesn't allocate)
bool BufFmtV(char* buf, size_t bufCchSize, const char* fmt, va_list args) {
    int count = VsnprintfUtf8(buf, bufCchSize, fmt, args);
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
Str FmtVWithArena(Arena* a, const char* fmt, va_list args) {
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
        return str::Dup(a, Str("vsnprintf() returned -1"));
    }

    char* buf = AllocArray<char>(a, count + 1);
    if (!buf) {
        return {};
    }

    va_copy(argsCopy, args);
    int count2 = VsnprintfUtf8(buf, (size_t)count + 1, fmt, argsCopy);
    va_end(argsCopy);
    ReportIf(count2 != count);
    if (count2 < 0) {
        Free(a, buf);
        return str::Dup(a, Str("vsnprintf() returned -1"));
    }
    return Str(buf, count);
}

Str FmtV(const char* fmt, va_list args) {
    return FmtVWithArena(nullptr, fmt, args);
}

// caller needs to str::Free()
Str Format(const char* fmt, ...) {
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
    char* end = str.s + str.len;
    for (char* c = str.s; c < end; c++) {
        Str found = str::FindChar(oldChars, *c);
        if (found) {
            int idx = (int)(found.s - oldChars.s);
            *c = newChars.s[idx];
            findCount++;
        }
    }

    return findCount;
}

// Trim whitespace characters, in-place, inside s.
// Returns number of trimmed characters.
size_t TrimWSInPlace(Str s, TrimOpt opt) {
    if (!s) {
        return 0;
    }
    char* str = s.s;
    size_t sLen = (size_t)s.len;
    char* ns = str;
    char* e = str + sLen;
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
    size_t trimmed = (size_t)(ns - str) + (size_t)(e - ne);
    if (ns != str) {
        size_t toCopy = sLen - trimmed + 1; // +1 for terminating 0
        memmove(str, ns, toCopy);
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
    char* str = s.s;
    char* src = str;
    char* dst = str;
    char* end = str + s.len;
    bool addedSpace = true;

    while (src < end) {
        if (!IsWs(*src)) {
            *dst++ = *src;
            addedSpace = false;
        } else if (!addedSpace) {
            *dst++ = ' ';
            addedSpace = true;
        }
        src++;
    }

    if (dst > str && IsWs(*(dst - 1))) {
        dst--;
    }
    *dst = '\0';

    return (size_t)(src - dst);
}

static bool isNl(char c) {
    return '\r' == c || '\n' == c;
}

// replaces '\r\n' and '\r' with just '\n' and removes empty lines
size_t NormalizeNewlinesInPlace(Str s, Str endExclusive) {
    if (!s) {
        return 0;
    }
    char* start = s.s;
    char* dst = s.s;
    char* e = endExclusive.s ? endExclusive.s : s.s + s.len;
    // remove newlines at the beginning
    while (s.s < e && isNl(*s.s)) {
        ++s.s;
    }

    bool inNewline = false;
    while (s.s < e) {
        if (isNl(*s.s)) {
            if (!inNewline) {
                *dst++ = '\n';
            }
            inNewline = true;
            ++s.s;
        } else {
            *dst++ = *s.s++;
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
    return (size_t)(dst - start);
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
    char* dst = str.s;
    char* src = str.s;
    char* end = str.s + str.len;
    while (src < end) {
        char c = *src++;
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
size_t RemoveCharsInPlace(WStr str, WStr toRemove) {
    if (!str) {
        return 0;
    }
    size_t removed = 0;
    WCHAR* dst = str.s;
    WCHAR* src = str.s;
    WCHAR* end = str.s + str.len;
    while (src < end) {
        WCHAR c = *src++;
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
Str MemToHex(const u8* buf, size_t len) {
    /* 2 hex chars per byte, +1 for terminating 0 */
    char* ret = AllocArray<char>(2 * len + 1);
    if (!ret) {
        return {};
    }
    char* dst = ret;
    for (; len > 0; len--) {
        sprintf_s(dst, 3, "%02x", *buf++);
        dst += 2;
    }
    return Str(ret, (int)(2 * (dst - ret)));
}

/* Reverse of MemToHex. Convert a 0-terminatd hex-encoded string <s> to
   binary data pointed by <buf> of max size bufLen.
   Returns false if size of <s> doesn't match bufLen or is not a valid
   hex string. */
bool HexToMem(Str s, u8* buf, size_t bufLen) {
    const char* p = s.s;
    for (; bufLen > 0; bufLen--) {
        unsigned int c;
        if (1 != sscanf_s(p, "%02x", &c)) {
            return false;
        }
        p += 2;
        *buf++ = (u8)c;
    }
    return !p || p >= s.s + s.len || *p == '\0';
}

static Str ExtractUntil(const char* pos, char c, const char** endOut) {
    *endOut = FindChar(pos, c);
    if (!*endOut) {
        return {};
    }
    return str::Dup(Str((char*)pos, (int)(*endOut - pos)));
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
     %c - parses a single char
     %s - parses a string (pass in a char**, free after use - also on failure!)
     %S - parses a string into a AutoFree
     %? - makes the next single character optional (e.g. "x%?,y" parses both "xy" and "x,y")
     %$ - causes the parsing to fail if it's encountered when not at the end of the string
     %  - skips a single whitespace character
     %_ - skips one or multiple whitespace characters (or none at all)
     %% - matches a single '%'

   %u, %d and %x accept an optional width argument, indicating exactly how many
   characters must be read for parsing the number (e.g. "%4d" parses -123 out of "-12345"
   and doesn't parse "123" at all).
*/
static Str ParseV(Str str, const char* format, va_list args) {
    if (!str) {
        return {};
    }
    const char* start = str.s;
    const char* p = str.s;
    for (const char* f = format; *f; f++) {
        if (*f != '%') {
            if (*f != *p) {
                return {};
            }
            p++;
            continue;
        }
        f++;

        const char* end = nullptr;
        if ('u' == *f) {
            *va_arg(args, unsigned int*) = strtoul(p, (char**)&end, 10);
        } else if ('d' == *f) {
            *va_arg(args, int*) = strtol(p, (char**)&end, 10);
        } else if ('x' == *f) {
            *va_arg(args, unsigned int*) = strtoul(p, (char**)&end, 16);
        } else if ('f' == *f) {
            *va_arg(args, float*) = (float)strtod(p, (char**)&end);
        } else if ('g' == *f) {
            *va_arg(args, float*) = (float)strtod(p, (char**)&end);
        } else if ('c' == *f) {
            *va_arg(args, char*) = *p, end = p + 1;
        } else if ('s' == *f) {
            *va_arg(args, char**) = ExtractUntil(p, *(f + 1), &end).s;
        } else if ('S' == *f) {
            va_arg(args, AutoFree*)->Set(ExtractUntil(p, *(f + 1), &end).s);
        } else if ('$' == *f && !*p) {
            continue; // don't fail, if we're indeed at the end of the string
        } else if ('%' == *f && *f == *p) {
            end = p + 1;
        } else if (' ' == *f && str::IsWs(*p)) {
            end = p + 1;
        } else if ('_' == *f) {
            if (!str::IsWs(*p)) {
                continue; // don't fail, if there's no whitespace at all
            }
            for (end = p + 1; str::IsWs(*end); end++) {
                // do nothing
            }
        } else if ('?' == *f && *(f + 1)) {
            // skip the next format character, advance the string,
            // if it the optional character is the next character to parse
            if (*p != *++f) {
                continue;
            }
            end = (char*)p + 1;
        } else if (str::IsDigit(*f)) {
            f = ParseLimitedNumber(p, f, &end, va_arg(args, void*)) - 1;
        }
        if (!end || end == p) {
            return {};
        }
        p = end;
    }
    int off = (int)(p - start);
    return Str((char*)p, str.len - off);
}

Str Parse(Str str, const char* fmt, ...) {
    if (!str || !fmt) {
        return {};
    }

    va_list args;
    va_start(args, fmt);
    Str res = ParseV(str, fmt, args);
    va_end(args);
    return res;
}

// TODO: could optimize it by making the main Parse() implementation
// work with explicit length and not rely on zero-termination
Str Parse(Str str, size_t len, const char* fmt, ...) {
    char buf[128]{};
    char* s = buf;
    Str work = str;

    if (!str.s || !fmt) {
        return {};
    }

    if (len < dimof(buf)) {
        memcpy(buf, str.s, len);
        work = Str(buf, (int)len);
    } else {
        Str dup = Dup(Str(str.s, (int)len));
        s = dup.s;
        work = dup;
    }

    va_list args;
    va_start(args, fmt);
    Str res = ParseV(work, fmt, args);
    va_end(args);

    if (!res) {
        if (s != buf) {
            free(s);
        }
        return {};
    }
    int off = (int)(res.s - work.s);
    Str out((char*)(str.s + off), str.len - off);
    if (s != buf) {
        free(s);
    }
    return out;
}

const char* Parse(const char* str, const char* fmt, ...) {
    if (!str || !fmt) {
        return nullptr;
    }

    va_list args;
    va_start(args, fmt);
    Str res = ParseV(Str((char*)str), fmt, args);
    va_end(args);
    return res.s;
}

const char* Parse(const char* str, size_t len, const char* fmt, ...) {
    if (!str || !fmt) {
        return nullptr;
    }

    va_list args;
    va_start(args, fmt);
    char buf[128]{};
    char* s = buf;
    Str work((char*)str, (int)len);

    if (len < dimof(buf)) {
        memcpy(buf, str, len);
        work = Str(buf, (int)len);
    } else {
        Str dup = Dup(Str((char*)str, (int)len));
        s = dup.s;
        work = dup;
    }

    Str res = ParseV(work, fmt, args);
    va_end(args);

    if (!res) {
        if (s != buf) {
            free(s);
        }
        return nullptr;
    }
    const char* out = str + (res.s - work.s);
    if (s != buf) {
        free(s);
    }
    return out;
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
int CmpNatural(Str aIn, Str bIn) {
    ReportIf(!aIn || !bIn);
    const char* a = aIn.s;
    const char* b = bIn.s;
    const char* aStart = a;
    const char* bStart = b;
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

Str SkipChar(Str s, char toSkip) {
    if (!s) {
        return {};
    }
    const char* p = s.s;
    const char* end = s.s + s.len;
    while (p < end && *p == toSkip) {
        p++;
    }
    return Str((char*)p, s.len - (int)(p - s.s));
}

} // namespace str

namespace url {

void DecodeInPlace(Str url) {
    if (!url) {
        return;
    }
    char* dst = url.s;
    for (char* src = url.s; *src; src++, dst++) {
        int val;
        if (*src == '%' && str::Parse(Str(src), "%%%2x", &val)) {
            *dst = (char)val;
            src += 2;
        } else {
            *dst = *src;
        }
    }
    *dst = '\0';
    url.len = (int)strlen(url.s);
}
} // namespace url

// SeqStrings (SeqStr* helpers) is for size-efficient implementation of:
// string -> int and int->string.
// it's even more efficient than using char *[] array
// it comes at the cost of speed, so it's not good for places
// that are critial for performance. On the other hand, it's
// not that bad: linear scanning of memory is fast due to the magic
// of L1 cache
void SeqStrNext(const char*& s, int* idxInOut) {
    int idx = *idxInOut;
    if (!s || !*s || idx < 0) {
        s = nullptr;
        *idxInOut = -1;
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
    *idxInOut = idx;
}

void SeqStrNext(const char*& s) {
    int idxDummy = 0;
    SeqStrNext(s, &idxDummy);
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
int SeqStrIndex(SeqStrings strs, Str toFind) {
    if (!toFind) {
        return -1;
    }
    const char* s = strs;
    int idx = 0;
    while (*s) {
        s = StrEqWeird(s, toFind.s);
        if (nullptr == s) {
            return idx;
        }
        ++idx;
    }
    return -1;
}

// like SeqStrIndex but ignores case and whitespace
int SeqStrIndexIS(SeqStrings strs, Str toFind) {
    if (!toFind) {
        return -1;
    }
    const char* s = strs;
    int idx = 0;
    while (*s) {
        if (str::EqIS(Str(s), toFind)) {
            return idx;
        }
        s = s + str::Len(s) + 1;
        ++idx;
    }
    return -1;
}

// Given an index in the "array" of sequentially laid out strings,
// returns a strings at that index.
Str SeqStrByIndex(SeqStrings strs, int idx) {
    ReportIf(idx < 0);
    const char* s = strs;
    while (idx > 0) {
        SeqStrNext(s);
        if (!s) {
            return {};
        }
        --idx;
    }
    return Str(s);
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

static const char* SeqStrNumEntryEnd(const char* entry) {
    if (!entry || !*entry) {
        return entry;
    }
    entry += str::Len(entry) + 1;
    const u8* p = (const u8*)entry;
    while (*p & 0x80) {
        p++;
    }
    return (const char*)(p + 1);
}

static void SeqStrNumEntryParts(const char* entry, const char** strOut, i64* numOut) {
    *strOut = entry;
    const u8* p = (const u8*)(entry + str::Len(entry) + 1);
    VarIntDecode(p, numOut);
}

void SeqStrNumAppend(StrBuilder* b, Str s, i64 num) {
    b->Append(s);
    b->AppendChar('\0');
    u8 buf[12];
    size_t n = VarIntEncode(buf, num);
    b->Append((const char*)buf, n);
}

void SeqStrNumFinish(StrBuilder* b) {
    b->AppendChar('\0');
}

void SeqStrNumNext(const char*& s, int* idxInOut) {
    int idx = *idxInOut;
    if (!s || !*s || idx < 0) {
        s = nullptr;
        *idxInOut = -1;
        return;
    }
    s = SeqStrNumEntryEnd(s);
    if (!s || !*s) {
        s = nullptr;
        return;
    }
    idx++;
    *idxInOut = idx;
}

void SeqStrNumNext(const char*& s) {
    int idxDummy = 0;
    SeqStrNumNext(s, &idxDummy);
}

int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut) {
    if (!toFind) {
        return -1;
    }
    const char* s = strs;
    int idx = 0;
    while (*s) {
        if (str::Eq(Str(s), toFind)) {
            if (numOut) {
                SeqStrNumEntryParts(s, &s, numOut);
            }
            return idx;
        }
        s = SeqStrNumEntryEnd(s);
        ++idx;
    }
    return -1;
}

int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut) {
    if (!toFind) {
        return -1;
    }
    const char* s = strs;
    int idx = 0;
    while (*s) {
        if (str::EqIS(Str(s), toFind)) {
            if (numOut) {
                SeqStrNumEntryParts(s, &s, numOut);
            }
            return idx;
        }
        s = SeqStrNumEntryEnd(s);
        ++idx;
    }
    return -1;
}

Str SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut) {
    ReportIf(idx < 0);
    const char* s = strs;
    while (idx > 0) {
        s = SeqStrNumEntryEnd(s);
        if (!s || !*s) {
            return {};
        }
        --idx;
    }
    if (!s || !*s) {
        return {};
    }
    if (numOut) {
        SeqStrNumEntryParts(s, &s, numOut);
    }
    return Str(s);
}

Str SeqStrNumStrByNumber(SeqStrNum strs, i64 num) {
    const char* s = strs;
    while (s && *s) {
        i64 n = 0;
        const char* str = s;
        SeqStrNumEntryParts(s, &str, &n);
        if (n == num) {
            return Str(str);
        }
        s = SeqStrNumEntryEnd(s);
    }
    return {};
}

// for compatibility with C string, the last character is always 0
// kPadding is number of characters needed for terminating character
static constexpr size_t kPadding = 1;

static char* EnsureCap(StrBuilder* s, size_t needed) {
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

static char* MakeSpaceAt(StrBuilder* s, size_t idx, size_t count) {
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
    char* s = EnsureCap(this, that.len);
    char* sOrig = that.Get();
    len = that.len;
    size_t n = len + kPadding;
    memcpy(s, sOrig, n);
}

StrBuilder::StrBuilder(const char* s) {
    Reset();
    Append(s);
}

StrBuilder& StrBuilder::operator=(const StrBuilder& that) {
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
    char* p = MakeSpaceAt(this, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool StrBuilder::AppendChar(char c) {
    return InsertAt(len, c);
}

bool StrBuilder::Append(const Str& s) {
    return Append(s.s, (size_t)s.len);
}

bool StrBuilder::Append(const char* src, size_t count) {
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

bool StrBuilder::Append(const StrBuilder& s) {
    return Append(s.LendData(), s.size());
}

char StrBuilder::RemoveAt(size_t idx, size_t count) {
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
char* StrBuilder::StealData(Arena* a) {
    char* res = els;
    if (a) {
        // if allocator is specified, have to duplicate
        res = (char*)MemDup(a, els, len + kPadding);
    } else {
        if (els == buf) {
            a = (a != nullptr) ? a : this->allocator;
            res = (char*)MemDup(a, els, len + kPadding);
        } else {
            // we're returning els, so reset to small buf
            els = buf;
        }
    }

    Reset();
    return res;
}

char* StrBuilder::LendData() const {
    return els;
}

// TODO: rewrite as size_t Find(const char* s, size_t sLen, size_t start);
bool StrBuilder::Contains(const char* s, size_t sLen) {
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

bool StrBuilder::IsEmpty() const {
    return len == 0;
}

ByteSlice StrBuilder::AsByteSlice() const {
    return {(u8*)Get(), size()};
}

ByteSlice StrBuilder::StealAsByteSlice() {
    size_t n = size();
    char* d = StealData();
    return {(u8*)d, n};
}

bool StrBuilder::Append(const u8* src, size_t size) {
    return this->Append((const char*)src, size);
}

bool StrBuilder::AppendSlice(const ByteSlice& d) {
    if (d.empty()) {
        return true;
    }
    return this->Append(d.data(), d.size());
}

void StrBuilder::AppendFmt(const char* fmt, ...) {
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

void StrBuilder::Set(const char* s) {
    Reset();
    Append(s);
}

char* StrBuilder::Get() const {
    return els;
}

char* StrBuilder::CStr() const {
    return els;
}

char StrBuilder::LastChar() const {
    auto n = this->len;
    if (n == 0) {
        return 0;
    }
    return at(n - 1);
}

static WCHAR* EnsureCap(WStrBuilder* s, size_t needed) {
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
    WCHAR* newEls;
    if (s->buf == s->els) {
        newEls = (WCHAR*)Alloc(s->allocator, allocSize);
        if (newEls) {
            memcpy(newEls, s->buf, WStrBuilder::kElSize * (s->len + 1));
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

static WCHAR* MakeSpaceAt(WStrBuilder* s, size_t idx, size_t count) {
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
    WCHAR* s = EnsureCap(this, that.cap);
    WCHAR* sOrig = that.Get();
    len = that.len;
    size_t n = (len + kPadding) * kElSize;
    memcpy(s, sOrig, n);
}

WStrBuilder::WStrBuilder(const WCHAR* s) {
    Reset();
    Append(s);
}

WStrBuilder& WStrBuilder::operator=(const WStrBuilder& that) {
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
    WCHAR* p = MakeSpaceAt(this, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool WStrBuilder::AppendChar(WCHAR c) {
    return InsertAt(len, c);
}

bool WStrBuilder::Append(const WCHAR* src, size_t count) {
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

WCHAR WStrBuilder::RemoveAt(size_t idx, size_t count) {
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
WCHAR* WStrBuilder::StealData() {
    WCHAR* res = els;
    if (els == buf) {
        res = (WCHAR*)MemDup(allocator, buf, (len + kPadding) * kElSize);
    }
    els = buf;
    Reset();
    return res;
}

WCHAR* WStrBuilder::LendData() const {
    return els;
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

void WStrBuilder::Set(const WCHAR* s) {
    Reset();
    Append(s);
}

WCHAR* WStrBuilder::Get() const {
    return els;
}

WCHAR WStrBuilder::LastChar() const {
    auto n = this->len;
    if (n == 0) {
        return 0;
    }
    return at(n - 1);
}

namespace str {

// returns true if was replaced
bool Replace(WStrBuilder& s, WStr toReplace, WStr replaceWith) {
    // fast path: nothing to replace
    if (!str::Find(s.els, toReplace.s)) {
        return false;
    }
    WStr newStr = str::Replace(WStr(s.els), toReplace, replaceWith);
    s.Reset();
    if (newStr) {
        s.Append(newStr.s);
        str::Free(newStr.s);
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

// hack: to fool CodeQL which doesn't approve of char* => WCHAR* casts
// and doesn't allow any way to disable that warning
WCHAR* CastToWCHAR(Str s) {
    void* d = (void*)s.s;
    return (WCHAR*)d;
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

bool EqN(const WCHAR* s1, const WCHAR* s2, size_t len) {
    if (s1 == s2) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == wcsncmp(s1, s2, len);
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

const WCHAR* Find(const WCHAR* str, const WCHAR* find) {
    return wcsstr(str, find);
}

Str ToUpperInPlace(Str s) {
    if (!s) {
        return {};
    }
    for (int i = 0; i < s.len; i++) {
        s.s[i] = (char)toupper((u8)s.s[i]);
    }
    return s;
}

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
    WStr s2 = str::Dup(s);
    return ToLowerInPlace(s2);
}

size_t TransCharsInPlace(WStr str, WStr oldChars, WStr newChars) {
    if (!str) {
        return 0;
    }
    size_t nReplaced = 0;
    WCHAR* end = str.s + str.len;
    for (WCHAR* c = str.s; c < end; c++) {
        WCHAR* pos = str::FindChar(oldChars, *c);
        if (pos) {
            size_t idx = (size_t)(pos - oldChars.s);
            *c = newChars.s[idx];
            nReplaced++;
        }
    }

    return nReplaced;
}

// free() the result via str::Free(s.s) or str::FreePtr(&s)
WStr Replace(WStr s, WStr toReplace, WStr replaceWith) {
    if (!s || str::IsEmpty(toReplace) || !replaceWith) {
        return {};
    }

    WStrBuilder result((size_t)s.len);
    size_t findLen = (size_t)toReplace.len;
    size_t replLen = (size_t)replaceWith.len;
    const WCHAR* start = s.s;
    const WCHAR* end;
    while ((end = str::Find(start, toReplace.s)) != nullptr) {
        result.Append(start, (size_t)(end - start));
        result.Append(replaceWith.s, replLen);
        start = end + findLen;
    }
    result.Append(start);
    return WStr(result.StealData());
}

// replaces all whitespace characters with spaces, collapses several
// consecutive spaces into one and strips heading/trailing ones
// returns the number of removed characters
size_t NormalizeWSInPlace(WStr s) {
    if (!s) {
        return 0;
    }
    WCHAR* str = s.s;
    WCHAR* src = str;
    WCHAR* dst = str;
    WCHAR* end = str + s.len;
    bool addedSpace = true;

    while (src < end) {
        if (!IsWs(*src)) {
            *dst++ = *src;
            addedSpace = false;
        } else if (!addedSpace) {
            *dst++ = ' ';
            addedSpace = true;
        }
        src++;
    }

    if (dst > str && IsWs(*(dst - 1))) {
        dst--;
    }
    *dst = '\0';

    return (size_t)(src - dst);
}

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

int BufSet(WCHAR* dst, int dstCchSize, Str src) {
    return BufSet(dst, dstCchSize, ToWStrTemp(src));
}

// append as much of s at the end of dst (which must be properly null-terminated)
// as will fit.
int BufAppend(char* dst, int dstCch, Str s) {
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
    char* thousandSep = ToUtf8Temp(thousandSepW);
    char* buf = fmt::FormatTemp("%d", num);

    char res[128] = {};
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
TempStr FormatFloatWithThousandSepTemp(double number, LCID locale, bool stripTrailingZero) {
    i64 num = (i64)(number * 100 + 0.5);

    char* tmp = FormatNumWithThousandSepTemp(num / 100, locale);
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
    char* buf = fmt::FormatTemp("%s%s%02d", tmp, decimal, num % 100);
    if (stripTrailingZero && str::EndsWith(buf, "0")) {
        buf[str::Len(buf) - 1] = '\0';
    }

    return buf;
}

constexpr double KB = 1024;
constexpr double MB = (double)1024 * (double)1024;
constexpr double GB = (double)1024 * (double)1024 * (double)1024;

static Str sizeUnitsEnglish[3] = {Str("GB"), Str("MB"), Str("KB")};

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

    char* sizestr = str::FormatFloatWithThousandSepTemp(s, LOCALE_USER_DEFAULT, false);
    if (!unit) {
        return sizestr;
    }
    return fmt::FormatTemp("%s %s", sizestr, unit.s);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
TempStr FormatFileSizeTemp(i64 size) {
    if (size <= 0) {
        return str::FormatTemp("%d", (int)size);
    }
    char* n1 = str::FormatSizeShortTemp(size);
    char* n2 = str::FormatNumWithThousandSepTemp(size);
    return fmt::FormatTemp("%s (%s %s)", n1, n2, "Bytes");
}

// http://rosettacode.org/wiki/Roman_numerals/Encode#C.2B.2B
TempStr FormatRomanNumeralTemp(int n) {
    if (n < 1) {
        return {};
    }

    static struct {
        int value;
        const char* numeral;
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
            va_arg(args, AutoFreeWStr*)->Set(ExtractUntil(str, *(f + 1), &end));
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

bool IsAbsolute(Str url) {
    const char* colon = str::FindChar(url.s, ':');
    const char* hash = str::FindChar(url.s, '#');
    return colon && (!hash || hash > colon);
}

TempStr GetFullPathTemp(Str url) {
    TempStr path = str::DupTemp(url);
    str::TransCharsInPlace(path, "#?", "\0\0");
    DecodeInPlace(path);
    return path;
}

TempStr GetFileNameTemp(Str url) {
    TempStr path = str::DupTemp(url);
    str::TransCharsInPlace(path, "#?", "\0\0");
    char* base = path.s + str::Len(path);
    for (; base > path.s; base--) {
        if ('/' == base[-1] || '\\' == base[-1]) {
            break;
        }
    }
    if (str::IsEmpty(base)) {
        return {};
    }
    DecodeInPlace(base);
    return str::DupTemp(Str(base));
}

} // namespace url

int ParseInt(const char* s) {
    bool negative = *s == '-';
    if (negative) {
        s++;
    }
    int value = 0;
    int overflowCheck = negative ? 1 : 0;
    for (; str::IsDigit(*s); s++) {
        value = value * 10 + (*s - '0');
        // return 0 on overflow
        if (value - overflowCheck < 0) {
            return 0;
        }
    }
    return negative ? -value : value;
}

i64 ParseInt64(const char* s) {
    bool negative = *s == '-';
    if (negative) {
        s++;
    }
    i64 value = 0;
    for (; str::IsDigit(*s); s++) {
        value = value * 10 + (*s - '0');
    }
    return negative ? -value : value;
}

// the only valid chars are 0-9, . and newlines.
// a valid version has to match the regex /^\d+(\.\d+)*(\r?\n)?$/
// Return false if it contains anything else.
bool IsValidProgramVersion(const char* txt) {
    if (!str::IsDigit(*txt)) {
        return false;
    }

    for (; *txt; txt++) {
        if (str::IsDigit(*txt)) {
            continue;
        }
        if (*txt == '.' && str::IsDigit(*(txt + 1))) {
            continue;
        }
        if (*txt == '\r' && *(txt + 1) == '\n') {
            continue;
        }
        if (*txt == '\n' && !*(txt + 1)) {
            continue;
        }
        return false;
    }

    return true;
}

static unsigned int ExtractNextNumber(const char** txt) {
    unsigned int val = 0;
    const char* next = str::Parse(*txt, "%u%?.", &val);
    *txt = next ? next : *txt + str::Leni(*txt);
    return val;
}

// compare two version string. Return 0 if they are the same,
// > 0 if the first is greater than the second and < 0 otherwise.
// e.g.
//   0.9.3.900 is greater than 0.9.3
//   1.09.300 is greater than 1.09.3 which is greater than 1.9.1
//   1.2.0 is the same as 1.2
int CompareProgramVersion(const char* txt1, const char* txt2) {
    if (!txt1) {
        txt1 = "";
    }
    if (!txt2) {
        txt2 = "";
    }
    while (*txt1 || *txt2) {
        unsigned int v1 = ExtractNextNumber(&txt1);
        unsigned int v2 = ExtractNextNumber(&txt2);
        if (v1 != v2) {
            return v1 - v2;
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
    char* ret = AllocArrayTemp<char>(maxLen + 2);
    const int half = maxLen / 2;
    const int strSize = sLen + 1; // +1 for terminating \0
    // copy first N/2 characters, move last N/2 characters to the halfway point
    for (int i = 0; i < half; i++) {
        ret[i] = s.s[i];
        ret[i + half] = s.s[strSize - half + i];
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
    char* tmp = ret;
    const char* src = s.s;
    int n;
    for (int i = 0; i < keep; i++) {
        n = utf8RuneLen((const u8*)src);
        ReportIf(n <= 0);
        switch (n) {
            default:
                ReportIf(true);
                break;
            case 4:
                *tmp++ = *src++;
                __fallthrough;
            case 3:
                *tmp++ = *src++;
                __fallthrough;
            case 2:
                *tmp++ = *src++;
                __fallthrough;
            case 1:
                *tmp++ = *src++;
        }
    }
    *tmp++ = '.';
    *tmp++ = '.';
    *tmp++ = '.';
    *tmp = 0;
    return Str(ret, (int)(tmp - ret));
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
    char* tmp = ret;
    const char* src = s.s;
    int n;
    for (int i = 0; i < nRunes; i++) {
        n = utf8RuneLen((const u8*)src);
        ReportIf(n <= 0);
        if (i < removeStartingAt || i >= removeStartingAt + toRemove) {
            switch (n) {
                default:
                    ReportIf(true);
                    break;
                case 4:
                    *tmp++ = *src++;
                    __fallthrough;
                case 3:
                    *tmp++ = *src++;
                    __fallthrough;
                case 2:
                    *tmp++ = *src++;
                    __fallthrough;
                case 1:
                    *tmp++ = *src++;
            }
        } else if (i == removeStartingAt) {
            *tmp++ = '.';
            *tmp++ = '.';
            *tmp++ = '.';
            src += n;
        } else {
            src += n;
        }
    }
    return Str(ret, (int)(tmp - ret));
}

// IsTextRtl is optimized version of checking if a string is rtl
// we look at max first 40 chars and
bool IsTextRtl(const WCHAR* s) {
    if (!s || !*s) return false;
    int len = str::Leni(s);
    len = len > 40 ? 40 : len;
    int nRtl = 0;
    int nLtr = 0;
    WORD* charTypes = AllocArray<WORD>(GetTempArena(), len + 1);
    if (!GetStringTypeExW(LOCALE_INVARIANT, CT_CTYPE2, s, len, charTypes)) {
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

bool IsTextRtl(const char* s) {
    TempWStr ws = ToWStrTemp(s);
    return IsTextRtl(ws);
}
