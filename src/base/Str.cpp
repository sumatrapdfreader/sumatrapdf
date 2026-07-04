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
    return memeq(s1.s, s2.s, len1);
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

bool EqN(Str s1, Str s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2 || n == 0) {
        return n == 0;
    }
    if (s1.len < n || s2.len < n) {
        return false;
    }
    return memeq(s1.s, s2.s, n);
}

bool EqNI(Str s1, Str s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2 || n == 0) {
        return n == 0;
    }
    if (s1.len < n || s2.len < n) {
        return false;
    }
    for (int i = 0; i < n; i++) {
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

bool EqNIx(Str s, int n, Str s2) {
    return len(s2) == n && str::StartsWithI(s, s2);
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
    int n = s1Len + s2Len + s3Len + s4Len + s5Len + 1;
    char* res = (char*)Alloc(allocator, n);

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

    return Str(res, n - 1);
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
    size_t n = s1Len + s2Len + s3Len + 1;
    WCHAR* res = (WCHAR*)Alloc(allocator, n * sizeof(WCHAR));
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

Str SliceFromChar(Str str, char c) {
    int idx = IndexOfChar(str, c);
    if (idx < 0) {
        return {};
    }
    return Str(str.s + idx, str.len - idx);
}

Str SliceFromCharLast(Str str, char c) {
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
        if (buf.s[i] == c && memeq(buf.s + i, toFind.s, toFindLen)) {
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
// splits s into the part before the separator (found at idx, sepLen chars long)
// and the part after it. idx < 0 means "not found": before = s, after = {}.
static bool CutAtIdx(Str s, int idx, int sepLen, Str* before, Str* after) {
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
        int off = idx + sepLen;
        *after = Str(s.s + off, s.len - off);
    }
    return true;
}

bool Cut(Str s, Str sep, Str* before, Str* after) {
    return CutAtIdx(s, IndexOf(s, sep), sep.len, before, after);
}

// like Cut() but splits on the first occurrence of a single char
bool CutChar(Str s, char c, Str* before, Str* after) {
    return Cut(s, Str(&c, 1), before, after);
}

// like CutChar() but splits on the last occurrence of a single char
bool CutCharLast(Str s, char c, Str* before, Str* after) {
    return CutAtIdx(s, LastIndexOfChar(s, c), 1, before, after);
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
int TransCharsInPlace(Str str, Str oldChars, Str newChars) {
    if (!str) {
        return 0;
    }
    int findCount = 0;
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
int TrimWSInPlace(Str s, TrimOpt opt) {
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
    int trimmed = start + (s.len - end);
    if (start != 0) {
        memmove(s.s, s.s + start, (size_t)(end - start) + 1);
    }
    return trimmed;
}

// replaces all whitespace characters with spaces, collapses several
// consecutive spaces into one and strips heading/trailing ones
// returns the number of removed characters
int NormalizeWSInPlace(Str s) {
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

    return s.len - dst;
}

static bool isNl(char c) {
    return '\r' == c || '\n' == c;
}

// replaces '\r\n' and '\r' with just '\n' and removes empty lines
int NormalizeNewlinesInPlace(Str s, Str endExclusive) {
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
    return dst;
}

int NormalizeNewlinesInPlace(Str s) {
    return NormalizeNewlinesInPlace(s, Str(s.s + s.len, 0));
}

// Remove all characters in "toRemove" from "str", in place.
// Returns number of removed characters.
int RemoveCharsInPlace(Str str, Str toRemove) {
    if (!str) {
        return 0;
    }
    int removed = 0;
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

int RemoveCharsInPlace(WStr str, WStr toRemove) {
    if (!str) {
        return 0;
    }
    int removed = 0;
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

/* Convert binary data in <buf> to a hex-encoded string */
TempStr MemToHexTemp(Str buf) {
    int n = buf.len;
    /* 2 hex chars per byte, +1 for terminating 0 */
    char* ret = AllocArrayTemp<char>(2 * n + 1);
    if (!ret) {
        return {};
    }
    static const char hex[] = "0123456789abcdef";
    int dst = 0;
    for (int i = 0; i < n; i++) {
        u8 b = (u8)buf.s[i];
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

bool HexToMem(Str s, Str buf) {
    size_t bufLen = (size_t)buf.len;
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
        buf.s[i] = (char)((hi << 4) | lo);
    }
    return s.len == (int)needed || (s.len > (int)needed && s.s[needed] == '\0');
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

bool EqN(WStr s1, WStr s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2) {
        return false;
    }
    return 0 == wcsncmp(s1.s, s2.s, (size_t)n);
}

bool IsNull(const WStr& s) {
    return !s.s;
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

WStr SliceFromChar(WStr str, WCHAR c) {
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

int TransCharsInPlace(WStr str, WStr oldChars, WStr newChars) {
    if (!str) {
        return 0;
    }
    int nReplaced = 0;
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
    if (!s || len(toReplace) == 0 || !replaceWith) {
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
int NormalizeWSInPlace(WStr s) {
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

    return src - dst;
}

} // namespace wstr
namespace str {

// Note: BufSet() should only be used when absolutely necessary (e.g. when
// handling buffers in OS-defined structures)
// returns the number of characters written (without the terminating \0)
int BufSet(Str dst, Str src) {
    int cchDst = dst.len;
    ReportIf(0 == cchDst || !dst.s);
    if (!src) {
        *dst.s = 0;
        return 0;
    }

    int toCopy = std::min(cchDst - 1, src.len);

    errno_t err = strncpy_s(dst.s, (size_t)cchDst, src.s, (size_t)toCopy);
    ReportIf(err || dst.s[toCopy] != '\0');

    return toCopy;
}

} // namespace str
namespace wstr {

int BufSet(WStr dst, WStr src) {
    int cchDst = dst.len;
    ReportIf(0 == cchDst || !dst.s);
    if (!src) {
        *dst.s = 0;
        return 0;
    }

    int toCopy = std::min(cchDst - 1, src.len);

    memset(dst.s, 0, cchDst * sizeof(WCHAR));
    memcpy(dst.s, src.s, toCopy * sizeof(WCHAR));
    return toCopy;
}

} // namespace wstr
namespace str {

int BufSet(WCHAR* dst, int dstCchSize, Str src) {
    return wstr::BufSet(WStr(dst, dstCchSize), ToWStrTemp(src));
}

// append as much of s at the end of dst (which must be properly null-terminated)
// as will fit.
int BufAppend(Str dst, Str s) {
    int dstCch = dst.len;
    ReportIf(0 == dstCch);

    int currDstCchLen = len(dst.s);
    if (currDstCchLen + 1 >= dstCch) {
        return 0;
    }
    int left = dstCch - currDstCchLen - 1;
    int toCopy = std::min(left, s.len);

    errno_t err = strncat_s(dst.s, dstCch, s.s, toCopy);
    ReportIf(err || dst.s[currDstCchLen + toCopy] != '\0');

    return toCopy;
}

} // namespace str

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
    if (len(baseStr) == 0) {
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
// IsTextRtl is optimized version of checking if a string is rtl
// we look at max first 40 chars and
bool IsTextRtl(WStr s) {
    if (!s) {
        return false;
    }
    int n = s.len > 40 ? 40 : s.len;
    int nRtl = 0;
    int nLtr = 0;
    WORD* charTypes = AllocArray<WORD>(GetTempArena(), n + 1);
    if (!GetStringTypeExW(LOCALE_INVARIANT, CT_CTYPE2, s.s, n, charTypes)) {
        return false; // API failure
    }
    for (int i = 0; i < n; ++i) {
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
    if (str::IsNull(s) || len(toReplace) == 0 || str::IsNull(replaceWith)) {
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
        int n = snprintf(buf.s, buf.len, "%llu B", size);
        return n < buf.len ? n : buf.len - 1;
    }

    // Calculate with 2 decimal precision
    u64 whole = size / divisor;
    u64 remainder = size % divisor;
    int frac = (int)((remainder * 100) / divisor);

    int n;
    if (frac == 0) {
        n = snprintf(buf.s, buf.len, "%llu%s", whole, suffix.s);
    } else if (frac % 10 == 0) {
        n = snprintf(buf.s, buf.len, "%llu.%d%s", whole, frac / 10, suffix.s);
    } else {
        n = snprintf(buf.s, buf.len, "%llu.%02d%s", whole, frac, suffix.s);
    }
    return n < buf.len ? n : buf.len - 1;
}

// Wrapper that formats into wide string buffer
void FormatSizeHumanIntoWBuf(u64 size, WStr wbuf) {
    char temp[32];
    int n = FormatSizeHumanIntoBuf(size, Str(temp, 32));

    // Copy to wide buffer
    int maxLen = wbuf.len - 1;
    int i = 0;
    while (i < n && i < maxLen) {
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
