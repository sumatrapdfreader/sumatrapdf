/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#if !defined(_MSC_VER)
#define _strdup strdup
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
// TODO: not sure if that's correct
#define sscanf_s sscanf
#endif

// StrArena: u32 handle from ArenaPtrCompress. Arena layout is unsigned LEB128
// length, length bytes of payload, trailing 0 for C APIs. 0 is the null handle.

static int StrArenaUlebSize(u32 n) {
    int i = 1;
    while (n >= 0x80) {
        n >>= 7;
        i++;
    }
    return i;
}

static int StrArenaUlebEncode(u8* dst, u32 n) {
    int i = 0;
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

static bool StrArenaUlebDecode(const u8*& p, u32* out) {
    u32 n = 0;
    int shift = 0;
    for (;;) {
        u8 b = *p++;
        n |= (u32)(b & 0x7f) << shift;
        if (!(b & 0x80)) {
            *out = n;
            return true;
        }
        shift += 7;
        if (shift >= 35) {
            return false;
        }
    }
}

// Allocate [uleb(size)][size bytes][0]. Body is uninitialized; terminator is set.
// Caller fills via StrArenaToStr(a, handle).s.
StrArena StrArenaAlloc(Arena* a, int size) {
    if (!a || size < 0) {
        return 0;
    }
    int vlen = StrArenaUlebSize((u32)size);
    int total = vlen + size + 1;
    u8* mem = (u8*)a->Push((u64)total, 1, false);
    if (!mem) {
        return 0;
    }
    StrArenaUlebEncode(mem, (u32)size);
    mem[vlen + size] = 0;
    return ArenaPtrCompress(a, mem);
}

StrArena StrArenaDupStr(Arena* a, Str s) {
    if (!a) {
        return 0;
    }
    int size = s.len;
    size = std::max(size, 0);
    StrArena sa = StrArenaAlloc(a, size);
    if (!sa) {
        return 0;
    }
    if (size > 0 && s.s) {
        Str out = StrArenaToStr(a, sa);
        memcpy(out.s, s.s, (size_t)size);
    }
    return sa;
}

Str StrArenaToStr(Arena* a, StrArena sa) {
    if (!a || !sa) {
        return {};
    }
    u8* mem = (u8*)ArenaPtrUncompress(a, sa);
    if (!mem) {
        return {};
    }
    const u8* p = mem;
    u32 size = 0;
    if (!StrArenaUlebDecode(p, &size)) {
        return {};
    }
    return Str((char*)p, (int)size);
}

// Locale-independent Unicode lowercase fold for one WCHAR.
// On Windows, CharLowerBuffW matches FoldCaseWInPlace; on POSIX a small table
// covers Latin/Cyrillic/Greek used by tests and falls back to towlower().
static WCHAR FoldCaseWChar(WCHAR c) {
#if OS_WIN
    WCHAR ch = c;
    CharLowerBuffW(&ch, 1);
    return ch;
#else
    if (c >= L'A' && c <= L'Z') {
        return c + 32;
    }
    if (c >= 0x00C0 && c <= 0x00DE && c != 0x00D7) {
        return c + 32;
    }
    if (c >= 0x0410 && c <= 0x042F) {
        return c + 32;
    }
    if (c == 0x0401) {
        return 0x0451;
    }
    if ((c >= 0x0391 && c <= 0x03A1) || (c >= 0x03A3 && c <= 0x03AB)) {
        return c + 32;
    }
    return (WCHAR)towlower(c);
#endif
}

// Locale-independent Unicode lowercase folding for case-insensitive matching.
static void FoldCaseWInPlace(WStr s) {
#if OS_WIN
    CharLowerBuffW(s.s, (DWORD)s.len);
#else
    for (int i = 0; i < s.len; i++) {
        s.s[i] = FoldCaseWChar(s.s[i]);
    }
#endif
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == 0x0130) {
            s.s[i] = L'i';
        }
    }
}

static int Utf8ByteOffsetForWCharOffset(Str s, int wcharOff) {
    if (wcharOff <= 0) {
        return 0;
    }
    int byteOff = 0;
    int nWide = 0;
    while (byteOff < s.len && nWide < wcharOff) {
        int prevByteOff = byteOff;
        int codepoint = Utf8CodepointNext(s, byteOff);
        int wcharUnits = sizeof(wchar_t) == 2 && codepoint > 0xffff ? 2 : 1;
        if (nWide + wcharUnits > wcharOff) {
            return prevByteOff;
        }
        nWide += wcharUnits;
    }
    return byteOff;
}

#if !OS_WIN
static bool IsRtlCodepoint(wchar_t c) {
    return (c >= 0x0590 && c <= 0x08ff) || (c >= 0xfb1d && c <= 0xfdff) || (c >= 0xfe70 && c <= 0xfeff) ||
           (c >= 0x10800 && c <= 0x10fff) || (c >= 0x1e800 && c <= 0x1edff);
}

static bool IsLtrCodepoint(wchar_t c) {
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= 0x00c0 && c <= 0x02af) ||
           (c >= 0x0370 && c <= 0x052f) || (c >= 0x1e00 && c <= 0x1fff);
}
#endif

// One allocation: sizeofi(StrNode) + s.len + 1. a==null => malloc; else arena.
StrNode* AllocStrNode(Arena* a, Str s) {
    int n = s.len;
    n = std::max(n, 0);
    int cb = sizeofi(StrNode) + n + 1;
    auto* node = (StrNode*)Alloc(a, cb);
    if (!node) {
        return nullptr;
    }
    char* dst = (char*)node + sizeofi(StrNode);
    if (n > 0 && s.s) {
        memcpy(dst, s.s, (size_t)n);
    }
    dst[n] = 0;
    node->next = nullptr;
    node->s = Str(dst, n);
    return node;
}

// first node whose string equals s (case-sensitive), null if none
StrNode* FindStrNode(StrNode* root, Str s) {
    StrNode* curr = root;
    while (curr) {
        if (str::Eq(curr->s, s)) {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

// Malloc path (a==null): free each node. Arena path: no per-node free.
// Frees the list with free() when a==null (malloc path). Arena path is a no-op.
void FreeStrNode(Arena* a, StrNode* head) {
    if (a) {
        return;
    }
    while (head) {
        StrNode* next = head->next;
        free(head);
        head = next;
    }
}

// Append n as the new last node. Clears n->next. List does not free nodes.
void StrNodeListPush(StrNodeList* list, StrNode* n) {
    ReportIf(!list || !n);
    n->next = nullptr;
    if (list->tail) {
        list->tail->next = n;
    } else {
        list->head = n;
    }
    list->tail = n;
}

// Unlink the last node. Does not free it; list becomes empty if it was the only node.
void StrNodeListPop(StrNodeList* list) {
    ReportIf(!list || !list->tail);
    if (list->head == list->tail) {
        list->head = nullptr;
        list->tail = nullptr;
        return;
    }
    StrNode* prev = list->head;
    while (prev->next != list->tail) {
        prev = prev->next;
    }
    prev->next = nullptr;
    list->tail = prev;
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

static Str WrapAllocated(char* s, int cch = -1) {
    if (!s) {
        return {};
    }
    if (cch < 0) {
        return Str(s);
    }
    return Str(s, cch);
}

Str Dup(Arena* a, Str s) {
    if (str::IsNull(s) || s.len < 0) {
        return {};
    }
    int cch = s.len;
    return WrapAllocated((char*)MemDup(a, s.s, (size_t)cch * sizeof(char), sizeof(char)), cch);
}

Str Dup(Str s) {
    return Dup(nullptr, s);
}

} // namespace str
namespace wstr {

static WStr WrapAllocatedW(WCHAR* s, int cch = -1) {
    if (!s) {
        return {};
    }
    if (cch < 0) {
        return WStr(s);
    }
    return WStr(s, cch);
}

WStr Dup(Arena* a, WStr s) {
    if (wstr::IsNull(s) || s.len < 0) {
        return {};
    }
    int cch = s.len;
    return WrapAllocatedW((WCHAR*)MemDup(a, s.s, (size_t)cch * sizeof(WCHAR), sizeof(WCHAR)), cch);
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

// strcmp-style (<0, 0, >0). Empty/null sorts before non-empty. Prefer Eq when only equality matters.
int Cmp(Str a, Str b) {
    if (a.s == b.s) {
        return 0;
    }
    if (str::IsNull(a) || a.len == 0) {
        return (str::IsNull(b) || b.len == 0) ? 0 : -1;
    }
    if (str::IsNull(b) || b.len == 0) {
        return 1;
    }
    int n = std::min(a.len, b.len);
    int r = memcmp(a.s, b.s, (size_t)n);
    if (r != 0) {
        return r;
    }
    return a.len - b.len;
}

// strcasecmp-style (<0, 0, >0). Prefer EqI when only equality matters.
int CmpI(Str a, Str b) {
    if (a.s == b.s) {
        return 0;
    }
    if (str::IsNull(a) || a.len == 0) {
        return (str::IsNull(b) || b.len == 0) ? 0 : -1;
    }
    if (str::IsNull(b) || b.len == 0) {
        return 1;
    }
    int n = std::min(a.len, b.len);
    for (int i = 0; i < n; i++) {
        int c1 = tolower((u8)a.s[i]);
        int c2 = tolower((u8)b.s[i]);
        if (c1 != c2) {
            return c1 - c2;
        }
    }
    return a.len - b.len;
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

bool StartsWith(Str s, Str prefix) {
    return EqN(s, prefix, len(prefix));
}

// Removes prefix from the string view, without modifying the underlying data.
bool TrimPrefix(Str& s, Str prefix) {
    if (!StartsWith(s, prefix)) {
        return false;
    }
    s.s += prefix.len;
    s.len -= prefix.len;
    return true;
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
bool StartsWithI(Str s, Str prefix) {
    return EqNI(s, prefix, len(prefix));
}

bool Contains(Str s, Str sub) {
    return str::IndexOf(s, sub) >= 0;
}

bool ContainsI(Str s, Str sub) {
    return str::IndexOfI(s, sub) >= 0;
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
            if (c == first && str::StartsWithI(Str(s.s + off, s.len - off), toFind)) {
                return off;
            }
        }
        return -1;
    }

    // Unicode path: case-fold both strings (UTF-16) and search, then map the
    // match position back to a byte offset in the original UTF-8 string so the
    // returned offset keeps IndexOfI's contract (an offset into s).
    //
    // Scratch buffers come from the temporary arena; AutoArenaSavepoint restores
    // it to its entry position on return so repeated calls (e.g. the command
    // palette filtering every item) don't grow the arena unbounded.
    AutoArenaSavepoint scratch;

    TempWStr ws = ToWStrTemp(s); // unfolded, used to map the match back to bytes
    TempWStr wsLo = str::DupTemp(ws);
    TempWStr wfLo = ToWStrTemp(toFind);
    FoldCaseWInPlace(wsLo);
    FoldCaseWInPlace(wfLo);

    int res = -1;
    int idx = WStrFindSubstr(wsLo, wfLo); // common/str_util.cpp
    if (idx >= 0) {
        res = Utf8ByteOffsetForWCharOffset(s, idx);
    }
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

Str Join(Arena* a, Str s1, Str s2, Str s3, Str s4, Str s5) {
    int s1Len = len(s1);
    int s2Len = len(s2);
    int s3Len = len(s3);
    int s4Len = len(s4);
    int s5Len = len(s5);
    int n = s1Len + s2Len + s3Len + s4Len + s5Len + 1;
    char* res = (char*)Alloc(a, n);

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

Str Join(Arena* a, Str s1, Str s2, Str s3) {
    return Join(a, s1, s2, s3, Str{}, Str{});
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
WStr Join(Arena* a, WStr s1, WStr s2, WStr s3) {
    int s1Len = s1.len, s2Len = s2.len, s3Len = s3.len;
    int n = s1Len + s2Len + s3Len + 1;
    WCHAR* res = (WCHAR*)Alloc(a, n * sizeofi(WCHAR));
    memcpy(res, s1.s, (size_t)s1Len * sizeof(WCHAR));
    memcpy(res + s1Len, s2.s, (size_t)s2Len * sizeof(WCHAR));
    memcpy(res + s1Len + s2Len, s3.s, (size_t)s3Len * sizeof(WCHAR));
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

// true if s contains any one of the chars (each char of `chars` is a candidate,
// not a substring to find)
bool ContainsCharAny(Str s, Str chars) {
    for (int i = 0; i < s.len; i++) {
        if (IndexOfChar(chars, s.s[i]) >= 0) {
            return true;
        }
    }
    return false;
}

Str SliceFromChar(Str str, char c) {
    int idx = IndexOfChar(str, c);
    if (idx < 0) {
        return {};
    }
    return Str(str.s + idx, str.len - idx);
}

Str SliceFromCharLast(Str str, char c) {
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
    if (len(s) == 0) {
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

// replace in str the chars from oldChars with their equivalents from newChars
// (similar to UNIX's tr command).
void TransCharsInPlace(Str& str, Str oldChars, Str newChars) {
    int nDiff = len(oldChars) - len(newChars);
    ReportIf(nDiff < 0);
    int nChanged = 0;
    for (int i = 0; i < str.len; i++) {
        int idx = str::IndexOfChar(oldChars, str.s[i]);
        if (idx >= 0) {
            str.s[i] = newChars.s[idx];
            nChanged++;
        }
    }
    if (nChanged * nDiff > 0) {
        str.s[str.len] = '\0';
    }
}

// Trim whitespace characters, in-place, inside s.
// Updates s.len. Returns number of trimmed characters.
int TrimWSInPlace(Str& s, TrimOpt opt) {
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
    s.len = end - start;
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

// like NormalizeWSInPlace but non-mutating: returns s with whitespace runs
// collapsed to single spaces and leading/trailing whitespace removed. Allocates
// a temp copy only when normalization would change something; otherwise returns
// s unchanged (no allocation).
TempStr NormalizeWSTemp(Str s) {
    int n = s.len;
    if (n == 0) {
        return s;
    }
    // decide whether normalizing changes anything, so we can skip allocating
    bool changed = IsWs(s.s[0]) || IsWs(s.s[n - 1]);
    for (int i = 0; !changed && i < n; i++) {
        char c = s.s[i];
        if (IsWs(c)) {
            // a non-space whitespace char becomes ' ', or a run collapses to one
            changed = (c != ' ') || (i + 1 < n && IsWs(s.s[i + 1]));
        }
    }
    if (!changed) {
        return s;
    }
    TempStr res = DupTemp(s);
    res.len -= NormalizeWSInPlace(res);
    return res;
}

static bool isNl(char c) {
    return '\r' == c || '\n' == c;
}

// replaces '\r\n' and '\r' with just '\n' and removes empty lines
int NormalizeNewlinesInPlace(Str s, Str endExclusive) {
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
    char* ret = AllocArrayTemp<char>((2 * n) + 1);
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
    int bufLen = buf.len;
    int needed = bufLen * 2;
    if (s.len < needed) {
        return false;
    }
    for (int i = 0; i < bufLen; i++) {
        int off = i * 2;
        int hi = HexDigitVal(s.s[off]);
        int lo = HexDigitVal(s.s[off + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        buf.s[i] = (char)((hi << 4) | lo);
    }
    return s.len == needed || (s.len > needed && s.s[needed] == '\0');
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
    for (int i = 0; i < s.len; i++) {
        if (!str::IsWs(s.s[i])) {
            return false;
        }
    }
    return true;
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

// Percent-decodes url into the temp arena ("%20" -> ' ', "%C3%A4" -> the two
// UTF-8 bytes of 'ä'); an escape that isn't two hex digits is left as is.
// Returns a new (NUL-terminated) string rather than decoding in place because
// decoding shrinks the string: the in-place version this replaces could only
// shorten its caller's buffer, and a caller left holding the encoded length
// carried the bytes past the NUL along (a markdown file named "a ä.md" looked
// up "a ä.md\0.md" and was reported as missing; #5926).
TempStr DecodeTemp(Str url) {
    if (str::IsNull(url)) {
        return {};
    }
    TempStr res = str::DupTemp(url);
    int n = res.len;
    int dst = 0;
    for (int src = 0; src < n; src++) {
        int val;
        if (res.s[src] == '%' && src + 2 < n && !str::IsNull(str::Parse(Str(res.s + src, n - src), "%%%2x", &val))) {
            res.s[dst++] = (char)val;
            src += 2;
        } else {
            res.s[dst++] = res.s[src];
        }
    }
    res.s[dst] = '\0';
    res.len = dst;
    return res;
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
    return {strs + off};
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
static int VarIntEncode(u8* dst, i64 val) {
    u64 n = ((u64)val << 1) ^ (u64)(val >> 63);
    int i = 0;
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
    int n = VarIntEncode(buf, num);
    b->Append(Str((char*)buf, n));
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
static constexpr int kPadding = 1;

// using external scratch, or no storage yet (not heap)
static bool IsExternalOrEmpty(const str::Builder* s) {
    return !s->els || (s->buf.s && s->els == s->buf.s);
}

static char* EnsureCap(str::Builder* s, int needed) {
    // only use external buf if we haven't moved to the heap yet.
    // RemoveAt() can shrink len enough for needed to fit again and switching
    // back would lose the data and leak the heap allocation.
    if (IsExternalOrEmpty(s) && s->buf.s && needed + kPadding <= s->buf.len) {
        s->els = s->buf.s;
        return s->els;
    }

    int capacityHint = s->cap;
    // tricky: to save space we reuse cap for capacityHint while still on
    // external/empty storage (cap was set from constructor hint)
    if (IsExternalOrEmpty(s)) {
        s->cap = 0;
    }

    if (s->els && s->cap >= needed) {
        return s->els;
    }

    int newCap = s->cap * 2;
    newCap = std::max(needed, newCap);
    newCap = std::max(newCap, capacityHint);

    int newElCount = newCap + kPadding;

    s->nReallocs++;

    int allocSize = newElCount;
    char* newEls;
    if (IsExternalOrEmpty(s)) {
        newEls = (char*)Alloc(s->a, allocSize);
        if (newEls && s->els && s->len > 0) {
            memcpy(newEls, s->els, (size_t)s->len + 1);
        } else if (newEls) {
            newEls[0] = 0;
        }
    } else {
        newEls = (char*)Realloc(s->a, s->els, (size_t)allocSize, (size_t)s->len + kPadding);
    }
    if (!newEls) {
        ReportIf(AtomicIntGet(&gAllowAllocFailure) == 0);
        return nullptr;
    }
    s->els = newEls;
    s->cap = newCap;
    return newEls;
}

static char* MakeSpaceAt(str::Builder* s, int idx, int count) {
    ReportIf(count == 0);
    int newLen = std::max(s->len, idx) + count;
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
        memmove(dst, src, (size_t)(s->len - idx));
    }
    s->len = newLen;
    // ZeroMemory(res, count);
    return res;
}

static void StrBuilderReset(str::Builder* s) {
    s->len = 0;
    // keep an existing heap buffer for re-use; only bind external buf when
    // we have not allocated heap yet
    if (!s->els || (s->buf.s && s->els == s->buf.s)) {
        s->els = s->buf.s; // may be null when no external buf
    }
    if (s->els) {
        s->els[0] = 0;
    }
}

static void StrBuilderFree(str::Builder* s) {
    if (s->els && !(s->buf.s && s->els == s->buf.s)) {
        Free(s->a, s->els);
    }
    s->len = 0;
    s->cap = 0;
    s->els = s->buf.s;
    if (s->els) {
        s->els[0] = 0;
    }
}

void str::Builder::Reset(Str s) {
    StrBuilderReset(this);
    Append(s); // no-op if s is empty
}

// arena is not owned by Builder; set .a after construction if needed
// capHint: preferred capacity after first grow
// capHint: preferred capacity after first grow
str::Builder::Builder(Str externalBuf) {
    this->buf = externalBuf;
    Reset();
}

// capHint: preferred capacity after first grow
// capHint: preferred capacity after first grow
str::Builder::Builder(int capHint) {
    Reset();
    cap = capHint + kPadding; // + kPadding for terminating 0
}

str::Builder::~Builder() {
    StrBuilderFree(this);
}

char& str::Builder::operator[](int idx) const {
    ReportIf(idx < 0 || idx >= len);
    return els[idx];
}

int len(const str::Builder& b) {
    return b.len;
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
    return InsertAt(len, c);
}

bool str::Builder::Append(Str src) {
    if (str::IsNull(src) || 0 == src.len) {
        return true;
    }
    char* dst = MakeSpaceAt(this, len, src.len);
    if (!dst) {
        return false;
    }
    memcpy(dst, src.s, (size_t)src.len);
    return true;
}

char str::Builder::RemoveAt(int idx, int count) {
    char res = els[idx];
    if (len > idx + count) {
        char* dst = els + idx;
        char* src = els + idx + count;
        int nToMove = len - idx - count;
        memmove(dst, src, (size_t)nToMove);
    }
    len -= count;
    memset(els + len, 0, (size_t)count);
    return res;
}

char str::Builder::RemoveLast() {
    if (len == 0) {
        return 0;
    }
    return RemoveAt(len - 1);
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
    int n = len;
    char* res = els;
    if (!els || n == 0) {
        Reset();
        return Str{};
    }
    if (buf.s && els == buf.s) {
        // data is in the external buffer, so we have to duplicate it
        res = (char*)MemDup(this->a, els, (size_t)n + kPadding);
        els = buf.s;
    } else {
        // we're returning the heap allocation; rebind to external if any
        els = buf.s;
    }

    Reset();
    return Str(res, n);
}

bool str::Contains(const str::Builder& b, Str sub) {
    return str::Contains(ToStr(b), sub);
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

// using external scratch, or no storage yet (not heap)
static bool IsExternalOrEmpty(const wstr::Builder* s) {
    return !s->els || (s->buf.s && s->els == s->buf.s);
}

static WCHAR* EnsureCap(wstr::Builder* s, int needed) {
    // only use external buf if we haven't moved to the heap yet.
    // RemoveAt() can shrink len enough for needed to fit again and switching
    // back would lose the data and leak the heap allocation.
    if (IsExternalOrEmpty(s) && s->buf.s && needed + kPadding <= s->buf.len) {
        s->els = s->buf.s;
        return s->els;
    }

    int capacityHint = (int)s->cap;
    // tricky: to save space we reuse cap for capacityHint while still on
    // external/empty storage (cap was set from constructor hint)
    if (IsExternalOrEmpty(s)) {
        s->cap = 0;
    }

    if (s->els && (int)s->cap >= needed) {
        return s->els;
    }

    int newCap = (int)s->cap * 2;
    newCap = std::max(needed, newCap);
    newCap = std::max(newCap, capacityHint);

    int newElCount = newCap + kPadding;

    s->nReallocs++;

    int allocSize = newElCount * wstr::Builder::kElSize;
    WCHAR* newEls;
    if (IsExternalOrEmpty(s)) {
        newEls = (WCHAR*)Alloc(s->a, allocSize);
        if (newEls && s->els && s->len > 0) {
            memcpy(newEls, s->els, (size_t)wstr::Builder::kElSize * (s->len + 1));
        } else if (newEls) {
            newEls[0] = 0;
        }
    } else {
        newEls = (WCHAR*)Realloc(s->a, s->els, (size_t)allocSize, (size_t)wstr::Builder::kElSize * (s->len + kPadding));
    }

    if (!newEls) {
        ReportIf(AtomicIntGet(&gAllowAllocFailure) == 0);
        return nullptr;
    }
    s->els = newEls;
    s->cap = (u32)newCap;
    return newEls;
}

static WCHAR* MakeSpaceAt(wstr::Builder* s, int idx, int count) {
    ReportIf(count == 0);
    int newLen = std::max((int)s->len, idx) + count;
    WCHAR* buf = EnsureCap(s, newLen);
    if (!buf) {
        return nullptr;
    }
    buf[newLen] = 0;
    WCHAR* res = &(buf[idx]);
    if ((int)s->len > idx) {
        WCHAR* src = buf + idx;
        WCHAR* dst = buf + idx + count;
        memmove(dst, src, (size_t)((int)s->len - idx) * wstr::Builder::kElSize);
    }
    s->len = (u32)newLen;
    return res;
}

static void WStrBuilderReset(wstr::Builder* s) {
    s->len = 0;
    // keep an existing heap buffer for re-use; only bind external buf when
    // we have not allocated heap yet
    if (!s->els || (s->buf.s && s->els == s->buf.s)) {
        s->els = s->buf.s; // may be null when no external buf
    }
    if (s->els) {
        s->els[0] = 0;
    }
}

static void WStrBuilderFree(wstr::Builder* s) {
    if (s->els && !(s->buf.s && s->els == s->buf.s)) {
        Free(s->a, s->els);
    }
    s->len = 0;
    s->cap = 0;
    s->els = s->buf.s;
    if (s->els) {
        s->els[0] = 0;
    }
}

void wstr::Builder::Reset(WStr s) {
    WStrBuilderReset(this);
    Append(s); // no-op if s is empty
}

// arena is not owned by Builder; set .a after construction if needed
// capHint: preferred capacity after first grow
// capHint: preferred capacity after first grow
wstr::Builder::Builder(WStr externalBuf) {
    this->buf = externalBuf;
    Reset();
}

// capHint: preferred capacity after first grow
// capHint: preferred capacity after first grow
wstr::Builder::Builder(int capHint) {
    Reset();
    cap = (u32)(capHint + kPadding); // + kPadding for terminating 0
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
    WCHAR* dst = MakeSpaceAt(this, (int)len, src.len);
    if (!dst) {
        return false;
    }
    memcpy(dst, src.s, (size_t)src.len * kElSize);
    return true;
}

WCHAR wstr::Builder::RemoveAt(int idx, int count) {
    WCHAR res = els[idx];
    if ((int)len > idx + count) {
        WCHAR* dst = els + idx;
        WCHAR* src = els + idx + count;
        memmove(dst, src, (size_t)((int)len - idx - count) * kElSize);
    }
    len -= (u32)count;
    memset(els + len, 0, (size_t)count * kElSize);
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
    if (!els || n == 0) {
        Reset();
        return WStr{};
    }
    if (buf.s && els == buf.s) {
        // data is in the external buffer, so we have to duplicate it
        res = (WCHAR*)MemDup(a, els, (size_t)(n + kPadding) * kElSize);
        els = buf.s;
    } else {
        // we're returning the heap allocation; rebind to external if any
        els = buf.s;
    }
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
    if (!s.els || !wstr::FindFrom(ToWStr(s), toReplace)) {
        return false;
    }
    WStr newStr = wstr::Replace(ToWStr(s), toReplace, replaceWith);
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

// Reinterpret a UTF-16 byte buffer held in a Str as a WStr without a
// char*→WCHAR* cast (CodeQL cpp/incorrect-string-type-conversion).
WStr CastStrToWStr(Str s) {
    if (!s) {
        return {};
    }
    WCHAR* w = nullptr;
    static_assert(sizeof(char*) == sizeof(WCHAR*), "pointer sizes must match");
    memcpy((void*)&w, (const void*)&s.s, sizeof(w));
    return WStr(w, s.len / sizeofi(WCHAR));
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

bool EqNI(WStr s1, WStr s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2) {
        return n == 0;
    }
    if (n == 0) {
        return true;
    }
    if (s1.len < n || s2.len < n) {
        return false;
    }
    WCHAR* a = AllocArrayTemp<WCHAR>(n);
    WCHAR* b = AllocArrayTemp<WCHAR>(n);
    if (!a || !b) {
        return false;
    }
    memcpy(a, s1.s, (size_t)n * sizeof(WCHAR));
    memcpy(b, s2.s, (size_t)n * sizeof(WCHAR));
    WStr wa(a, n);
    WStr wb(b, n);
    FoldCaseWInPlace(wa);
    FoldCaseWInPlace(wb);
    return EqN(wa, wb, n);
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
    return EqNI(s1, s2, s1.len);
}

// wcscmp-style (<0, 0, >0). Empty/null sorts before non-empty.
int Cmp(WStr a, WStr b) {
    if (a.s == b.s) {
        return 0;
    }
    if (wstr::IsNull(a) || a.len == 0) {
        return (wstr::IsNull(b) || b.len == 0) ? 0 : -1;
    }
    if (wstr::IsNull(b) || b.len == 0) {
        return 1;
    }
    int n = std::min(a.len, b.len);
    for (int i = 0; i < n; i++) {
        if (a.s[i] != b.s[i]) {
            return a.s[i] < b.s[i] ? -1 : 1;
        }
    }
    return a.len - b.len;
}

// case-insensitive WCHAR compare (<0, 0, >0). Prefer EqI when only equality matters.
int CmpI(WStr a, WStr b) {
    if (a.s == b.s) {
        return 0;
    }
    if (wstr::IsNull(a) || a.len == 0) {
        return (wstr::IsNull(b) || b.len == 0) ? 0 : -1;
    }
    if (wstr::IsNull(b) || b.len == 0) {
        return 1;
    }
    int n = std::min(a.len, b.len);
    for (int i = 0; i < n; i++) {
        WCHAR c1 = FoldCaseWChar(a.s[i]);
        WCHAR c2 = FoldCaseWChar(b.s[i]);
        if (c1 != c2) {
            return c1 < c2 ? -1 : 1;
        }
    }
    return a.len - b.len;
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

bool StartsWith(WStr str, WStr prefix) {
    if (!prefix) {
        return true;
    }
    if (!str || prefix.len > str.len) {
        return false;
    }
    return EqN(str, prefix, prefix.len);
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
    return EqNI(str, prefix, prefix.len);
}

bool EndsWith(WStr txt, WStr end) {
    if (!txt || !end) {
        return false;
    }
    if (end.len > txt.len) {
        return false;
    }
    return Eq(WStr(txt.s + txt.len - end.len, end.len), end);
}

bool EndsWithI(WStr txt, WStr end) {
    if (!txt || !end) {
        return false;
    }
    if (end.len > txt.len) {
        return false;
    }
    return EqI(WStr(txt.s + txt.len - end.len, end.len), end);
}

int IndexOfChar(WStr s, WCHAR c) {
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
    for (int i = 0; i < s.len; i++) {
        s.s[i] = (char)toupper((u8)s.s[i]);
    }
    return s;
}

} // namespace str
namespace wstr {

WStr ToLowerInPlace(WStr s) {
    for (int i = 0; i < s.len; i++) {
        s.s[i] = towlower(s.s[i]);
    }
    return s;
}

WStr ToLower(WStr s) {
    WStr s2 = wstr::Dup(s);
    return ToLowerInPlace(s2);
}

void TransCharsInPlace(WStr& str, WStr oldChars, WStr newChars) {
    int nDiff = len(oldChars) - len(newChars);
    ReportIf(nDiff < 0);
    int nChanged = 0;
    for (int i = 0; i < str.len; i++) {
        int idx = wstr::IndexOfChar(oldChars, str.s[i]);
        if (idx >= 0) {
            str.s[i] = newChars.s[idx];
            nChanged++;
        }
    }
    if (nChanged * nDiff > 0) {
        str.s[str.len] = L'\0';
    }
}

// free() the result via str::Free(s) or str::FreePtr(&s)
WStr Replace(WStr s, WStr toReplace, WStr replaceWith) {
    if (!s || len(toReplace) == 0 || !replaceWith) {
        return {};
    }

    wstr::Builder result(s.len);
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

// Bounded null-terminated copy into a fixed buffer (replaces lstrcpyn / strcpy_s /
// StringCchCopy). Only for OS structs with fixed fields — prefer owned Str/WStr
// otherwise. dst.len is capacity including the terminator. Returns chars written
// excluding the terminator.
int BufSet(Str dst, Str src) {
    int cchDst = dst.len;
    if (0 == cchDst || !dst.s) {
        ReportIf(true);
        return 0;
    }
    if (!src) {
        *dst.s = 0;
        return 0;
    }

    int toCopy = std::min(cchDst - 1, src.len);

    memcpy(dst.s, src.s, (size_t)toCopy);
    dst.s[toCopy] = '\0';

    return toCopy;
}

} // namespace str
namespace wstr {

// WCHAR overload of BufSet — replaces lstrcpynW / wcscpy_s / wcsncpy_s / StringCchCopyW.
int BufSet(WStr dst, WStr src) {
    int cchDst = dst.len;
    if (0 == cchDst || !dst.s) {
        ReportIf(true);
        return 0;
    }
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

// UTF-8 Str → fixed WCHAR buffer (converts then BufSet).
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

    memcpy(dst.s + currDstCchLen, s.s, (size_t)toCopy);
    dst.s[currDstCchLen + toCopy] = '\0';

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
    return DecodeTemp(path);
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
    return DecodeTemp(baseStr);
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
        value = (value * 10) + (s.s[off] - '0');
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
        value = (value * 10) + (s.s[off] - '0');
    }
    return negative ? -value : value;
}

// the only valid chars are 0-9, . and newlines.
// a valid version has to match the regex /^\d+(\.\d+)*(\r?\n)?$/
// Return false if it contains anything else.
bool IsValidProgramVersion(Str ver) {
    if (!ver || !str::IsDigit(ver.s[0])) {
        return false;
    }

    for (int i = 0; i < ver.len; i++) {
        char c = ver.s[i];
        if (str::IsDigit(c)) {
            continue;
        }
        if (c == '.' && i + 1 < ver.len && str::IsDigit(ver.s[i + 1])) {
            continue;
        }
        if (c == '\r' && i + 1 < ver.len && ver.s[i + 1] == '\n') {
            continue;
        }
        if (c == '\n' && i + 1 == ver.len) {
            continue;
        }
        return false;
    }

    return true;
}

static unsigned int ExtractNextNumber(Str txt, int& off) {
    unsigned int val = 0;
    if (off >= txt.len) {
        off = txt.len;
        return 0;
    }
    Str slice(txt.s + off, txt.len - off);
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
int CompareProgramVersion(Str ver1, Str ver2) {
    int off1 = 0;
    int off2 = 0;
    while (off1 < ver1.len || off2 < ver2.len) {
        unsigned int v1 = ExtractNextNumber(ver1, off1);
        unsigned int v2 = ExtractNextNumber(ver2, off2);
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
#if OS_WIN
    WORD* charTypes = AllocArrayTemp<WORD>(n + 1);
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
#else
    for (int i = 0; i < n; i++) {
        wchar_t c = s.s[i];
        if (IsRtlCodepoint(c)) {
            nRtl++;
        } else if (IsLtrCodepoint(c)) {
            nLtr++;
        }
    }
#endif
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
    int capHint = s.len + 1 + (lenDiff * 6);
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
// Temporary, guaranteed zero-terminated copy of s (lives in the temp arena).
// Use when passing a Str/WStr to a C or win32 API that requires a
// NUL-terminated string; the name documents that intent at the call site.
// Returns non-const so it implicitly converts to both char* and const char*
// (some C/win32 APIs take non-const), avoiding casts at the call site.
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
// str::Builder/wstr::Builder always keep their data NUL-terminated.
// ToStr() returns a {ptr,len} view (may contain embedded NULs).
// ToCStr() returns the NUL-terminated buffer, for passing to C/win32 code we
// don't control that expects a zero-terminated char*/WCHAR*.
Str ToStr(const str::Builder& b) {
    return Str(b.els, (int)b.len);
}

// NO_INLINE: this is called in many places; keeping it out of line trims code size
// owning temp-arena copy of the builder's content (unlike ToStr()'s view)
NO_INLINE TempStr ToStrTemp(const str::Builder& b) {
    return str::DupTemp(ToStr(b));
}

// str::Builder always keeps its data NUL-terminated, so we can hand out the
// buffer directly for C/win32 APIs we don't control that want a char*
char* ToCStr(const str::Builder& b) {
    if (!b.els) {
        static char empty = 0;
        return &empty;
    }
    return b.els;
}

WStr ToWStr(const wstr::Builder& b) {
    return WStr(b.els, (int)b.len);
}

// wstr::Builder always keeps its data NUL-terminated, so we can hand out the
// buffer directly for C/win32 APIs we don't control that want a WCHAR*
WCHAR* ToWCStr(const wstr::Builder& b) {
    if (!b.els) {
        static WCHAR empty = 0;
        return &empty;
    }
    return b.els;
}

// --- begin: merged from former src/common/str_util.cpp ---
wchar_t ToLowerW(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return c + (L'a' - L'A');
    return c;
}

int WStrFindSubstr(WStr str, WStr substr) {
    if (len(substr) == 0) return -1; // Empty search - no highlight
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

// Format file size with comma separators, returns Str
// Str utilities
Str FormatFileSize(Arena* arena, u64 size) {
    char buf[32];

    if (size == 0) {
        return str::Dup(arena, StrL("0"));
    }

    // Convert to string (reversed)
    char temp[32];
    int i = 0;
    while (size > 0 && i < 31) {
        temp[i++] = (char)('0' + (size % 10));
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

    const u64 TB = 1024ULL * 1024 * 1024 * 1024;
    const u64 GB = 1024ULL * 1024 * 1024;
    const u64 MB = 1024ULL * 1024;
    const u64 KB = 1024ULL;

    Str suffix;
    u64 divisor;

    if (size >= TB) {
        suffix = StrL(" TB");
        divisor = TB;
    } else if (size >= GB) {
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
