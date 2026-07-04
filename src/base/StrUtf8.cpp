/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Base.h"
#include <locale.h>

#if defined(_MSC_VER)
static _locale_t GetUtf8FormatLocale() {
    // wrapped in a struct so the locale is freed at exit (keeps leak
    // detectors quiet); after the destructor runs, callers see nullptr
    // and fall back to plain vsnprintf
    struct Locale {
        _locale_t loc = _create_locale(LC_ALL, ".UTF-8");
        ~Locale() {
            if (loc) {
                _free_locale(loc);
                loc = nullptr;
            }
        }
    };
    static Locale l;
    return l.loc;
}
#endif

// The format string is a plain const char* because this is a thin wrapper around
// vsnprintf and is almost always called with a string literal.
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

int utf8RuneLen(const u8* s) {
    int n = trailingBytesForUTF8[*s] + 1;
    return n;
}

bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd) {
    int n = utf8RuneLen(source);
    if (source + n > sourceEnd) {
        return false;
    }
    return isLegalUTF8(source, n);
}

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

void str::Utf8Encode(char* buf, int& off, int c) {
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

int Utf8CodepointAtByte(Str s, int byteIdx, int* bytesOut) {
    if (bytesOut) {
        *bytesOut = 0;
    }
    if (!s || byteIdx < 0 || byteIdx >= s.len) {
        return 0;
    }

    const u8* p = (const u8*)s.s + byteIdx;
    int n = utf8RuneLen(p);
    if (n <= 0 || byteIdx + n > s.len || !isLegalUTF8Sequence(p, p + n)) {
        if (bytesOut) {
            *bytesOut = 1;
        }
        return *p;
    }
    if (bytesOut) {
        *bytesOut = n;
    }
    if (n == 1) {
        return p[0];
    }
    int rune = p[0] & ((1 << (7 - n)) - 1);
    for (int i = 1; i < n; i++) {
        rune = (rune << 6) | (p[i] & 0x3f);
    }
    return rune;
}

int Utf8CodepointCount(Str s) {
    int nCodepoints = 0;
    for (int byteIdx = 0; s && byteIdx < s.len; nCodepoints++) {
        Utf8CodepointNext(s, byteIdx);
    }
    return nCodepoints;
}

int Utf8CodepointNext(Str s, int& byteIdx) {
    if (!s || byteIdx < 0 || byteIdx >= s.len) {
        return 0;
    }
    int n = 0;
    int c = Utf8CodepointAtByte(s, byteIdx, &n);
    byteIdx += n > 0 ? n : 1;
    return c;
}

int Utf8CodepointPrev(Str s, int& byteIdx) {
    if (!s || byteIdx <= 0) {
        return 0;
    }
    if (byteIdx > s.len) {
        byteIdx = s.len;
    }
    int prevByte = byteIdx - 1;
    while (prevByte > 0 && (((u8)s.s[prevByte] & 0xc0) == 0x80)) {
        prevByte--;
    }
    byteIdx = prevByte;
    return Utf8CodepointAtByte(s, byteIdx);
}

int Utf8CodepointToByteIndex(Str s, int codepointIdx) {
    if (!s || codepointIdx <= 0) {
        return 0;
    }
    int byteIdx = 0;
    int cp = 0;
    while (byteIdx < s.len && cp < codepointIdx) {
        Utf8CodepointNext(s, byteIdx);
        cp++;
    }
    return byteIdx;
}

int Utf8AdvanceCodepoints(Str s, int byteIdx, int nCodepoints) {
    if (!s || byteIdx < 0) {
        return 0;
    }
    if (byteIdx > s.len) {
        return s.len;
    }
    for (int i = 0; i < nCodepoints && byteIdx < s.len; i++) {
        Utf8CodepointNext(s, byteIdx);
    }
    return byteIdx;
}

Str Utf8SliceByCodepoints(Str s, int startCodepoint, int nCodepoints) {
    if (!s || nCodepoints <= 0) {
        return {};
    }
    if (startCodepoint < 0) {
        startCodepoint = 0;
    }
    int startByte = Utf8CodepointToByteIndex(s, startCodepoint);
    int endByte = Utf8AdvanceCodepoints(s, startByte, nCodepoints);
    return Str(s.s + startByte, endByte - startByte);
}

static TempStr ShortenStringTemp(Str s, int maxLen) {
    int sLen = len(s);
    if (sLen <= maxLen) {
        return s;
    }
    char* ret = AllocArrayTemp<char>(maxLen + 2);
    const int half = maxLen / 2;
    for (int i = 0; i < half; i++) {
        ret[i] = s.s[i];
        ret[i + half] = s.s[sLen - half + i];
    }
    ret[half - 2] = ret[half - 1] = ret[half] = '.';
    return Str(ret, maxLen + 2);
}

TempStr ShortenStringUtf8Temp(Str s, int maxRunes) {
    int nRunes = utf8StrLen((u8*)s.s);
    if (nRunes < 0) {
        int sLen = len(s);
        if (sLen <= maxRunes) {
            return s;
        }
        int keep = maxRunes - 3;
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
    int keep = maxRunes - 3;
    if (keep < 0) {
        keep = 0;
    }
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

TempStr ShortenStringUtf8InTheMiddleTemp(Str s, int maxRunes) {
    int nRunes = utf8StrLen((u8*)s.s);
    if (nRunes < 0) {
        return ShortenStringTemp(s, maxRunes);
    }
    if (nRunes <= maxRunes) {
        return s;
    }
    int toRemove = (nRunes - maxRunes) + 3;
    int removeStartingAt = (nRunes / 2) - (toRemove / 2);
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

static wchar_t emptyWideStr[1] = {0};

Str ToUtf8(Arena* arena, WStr wide) {
    if (IsEmpty(wide)) {
        return Str();
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.s, wide.len, nullptr, 0, nullptr, nullptr);
    char* utf8 = (char*)Alloc(arena, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wide.s, wide.len, utf8, len, nullptr, nullptr);
    utf8[len] = 0;
    return Str(utf8, len);
}

Str ToUtf8Temp(WStr wide) {
    return ToUtf8(GetTempArena(), wide);
}

WStr ToWStrTemp(Str s) {
    if (IsEmpty(s)) {
        return WStr(&emptyWideStr[0], 0);
    }
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, nullptr, 0);
    wchar_t* wide = (wchar_t*)AllocTemp((wideLen + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, wide, wideLen);
    wide[wideLen] = 0;
    return WStr(wide, wideLen);
}

// Converts a UTF-8 Str to a NUL-terminated WCHAR* temp. Use when the wide
// result is only needed as a C/win32 string pointer.
WCHAR* CWStrTemp(Str s) {
    return ToWStrTemp(s).s;
}

WCHAR* CWStrTemp(Str s, int& cch) {
    WStr ws = ToWStrTemp(s);
    cch = ws.len;
    return ws.s;
}
