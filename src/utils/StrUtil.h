/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

#define UTF8_BOM "\xEF\xBB\xBF"
#define UTF16_BOM "\xFF\xFE"
#define UTF16BE_BOM "\xFE\xFF"

bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd);
bool isLegalUTF8String(const u8** source, const u8* sourceEnd);
int utf8StrLen(const u8* s);
int utf8RuneLen(const u8* s);

struct ByteSlice {
    u8* d = nullptr;
    size_t sz = 0;

    ByteSlice() = default;
    ~ByteSlice() = default;
    ByteSlice(const char* str) {
        d = (u8*)str;
        sz = strlen(str);
    }
    ByteSlice(char* str) {
        d = (u8*)str;
        sz = strlen(str);
    }
    ByteSlice(const u8* data, size_t size) {
        d = (u8*)data;
        sz = size;
    }
    ByteSlice(const ByteSlice& data) {
        d = data.data();
        sz = data.size();
    }
    ByteSlice& operator=(const ByteSlice& other) {
        d = other.d;
        sz = other.sz;
        return *this;
    }
    void Set(u8* data, size_t size) {
        d = data;
        sz = size;
    }
    void Set(char* data, size_t size) {
        d = (u8*)data;
        sz = size;
    }
    u8* data() const { return d; }
    u8* Get() const { return d; }
    size_t size() const { return sz; }
    int Size() const { return (int)sz; }
    bool empty() const { return !d; }
    bool IsEmpty() const { return !d; }
    ByteSlice Clone() const {
        if (empty()) {
            return {};
        }
        u8* res = (u8*)memdup(d, sz, 1);
        return {res, size()};
    }
    void Free() {
        free(d);
        d = nullptr;
        sz = 0;
    }
    operator const char*() { return (const char*)d; }
};

bool IsEqual(const ByteSlice&, const ByteSlice&);

FORCEINLINE Str AsStr(ByteSlice bs) {
    return Str((char*)bs.data(), bs.Size());
}

namespace str {

enum class TrimOpt {
    Left,
    Right,
    Both
};

void Free(Str s);
void Free(WStr s);
FORCEINLINE void Free(const char* s) {
    Free(Str(s));
} // str-port shim
FORCEINLINE void Free(char* s) {
    Free(Str(s));
} // str-port shim
FORCEINLINE void Free(const u8* s) {
    Free(Str((const char*)s));
} // str-port shim
FORCEINLINE void Free(u8* s) {
    Free(Str((char*)s));
} // str-port shim
FORCEINLINE void Free(const WCHAR* s) {
    Free(WStr(s));
} // str-port shim
FORCEINLINE void Free(WCHAR* s) {
    Free(WStr(s));
} // str-port shim

void FreePtr(Str* s);
void FreePtr(WStr* s);
FORCEINLINE void FreePtr(const char** s) { // str-port shim
    Free(Str(*s));
    *s = nullptr;
}
FORCEINLINE void FreePtr(char** s) { // str-port shim
    Free(Str(*s));
    *s = nullptr;
}
FORCEINLINE void FreePtr(const WCHAR** s) { // str-port shim
    Free(WStr(*s));
    *s = nullptr;
}
FORCEINLINE void FreePtr(WCHAR** s) { // str-port shim
    Free(WStr(*s));
    *s = nullptr;
}

Str Dup(Arena*, Str str, size_t cch = (size_t)-1);
Str Dup(Str s, size_t cch = (size_t)-1);
Str Dup(const ByteSlice&);

void ReplacePtr(Str* s, Str snew);

void ReplaceWithCopy(Str* s, Str snew);

Str Join(Arena*, Str, Str, Str);
Str Join(Arena*, Str, Str, Str, Str, Str);
Str Join(Str s1, Str s2, Str s3 = {});
FORCEINLINE Str Join(Arena* a, Str s1, Str s2, Str s3, Str s4) {
    return Join(a, s1, s2, s3, s4, Str{});
}
FORCEINLINE Str Join(Str s1, Str s2, Str s3, Str s4) {
    return Join(nullptr, s1, s2, s3, s4);
}

bool Eq(Str s1, Str s2);
bool Eq(const ByteSlice& sp1, const ByteSlice& sp2);
bool EqI(Str s1, Str s2);
bool EqIS(Str s1, Str s2);
bool EqN(Str s1, Str s2, size_t len);
bool EqNI(Str s1, Str s2, size_t len);
bool IsEmpty(Str s);
bool StartsWith(Str str, Str prefix);
bool StartsWith(const u8* str, Str prefix);

bool StartsWithI(Str str, Str prefix);
bool EndsWith(Str txt, Str end);
bool EndsWithI(Str txt, Str end);
bool EqNIx(Str s, size_t len, Str s2);

Str ToLowerInPlace(Str s);

Str ToLower(Str s);

Str ToUpperInPlace(Str s);

void Utf8Encode(char*& dst, int c);

bool IsDigit(char c);
bool IsWs(char c);
bool IsAlNum(char c);

Str FindChar(Str str, char c);
Str FindCharLast(Str str, char c);
int FindCharIdx(Str str, char c);
Str Find(Str str, Str find);
Str FindI(Str str, Str find);
int BufFind(Str buf, Str toFind);

bool Contains(Str s, Str txt);
bool ContainsI(Str s, Str txt);

bool BufFmtV(char* buf, size_t bufCchSize, const char* fmt, va_list args);
bool BufFmt(char* buf, size_t bufCchSize, const char* fmt, ...);
Str FmtVWithArena(Arena* a, const char* fmt, va_list args);
Str FmtV(const char* fmt, va_list args);
Str Format(const char* fmt, ...);

size_t TrimWSInPlace(Str s, TrimOpt opt);

size_t TransCharsInPlace(Str str, Str oldChars, Str newChars);

size_t NormalizeWSInPlace(Str str);
size_t NormalizeNewlinesInPlace(Str s, Str endExclusive);
size_t NormalizeNewlinesInPlace(Str s);
size_t RemoveCharsInPlace(Str str, Str toRemove);

int BufSet(char* dst, int dstCchSize, Str src);
int BufAppend(char* dst, int dstCchSize, Str s);

Str MemToHex(const u8* buf, size_t len);
bool HexToMem(Str s, u8* buf, size_t bufLen);

Str Parse(Str str, const char* fmt, ...);
Str Parse(Str str, size_t len, const char* fmt, ...);

int CmpNatural(Str a, Str b);

TempStr FormatFloatWithThousandSepTemp(double number, LCID locale = LOCALE_USER_DEFAULT, bool stripTrailingZero = true);
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale = LOCALE_USER_DEFAULT);
TempStr FormatSizeShortTemp(i64 size);
TempStr FormatSizeShortTemp(i64 size, Str const* sizeUnits);
TempStr FormatFileSizeTemp(i64);
TempStr FormatRomanNumeralTemp(int number);

bool IsEmptyOrWhiteSpace(Str s);
bool Skip(Str& s, Str toSkip);
Str SkipChar(Str s, char toSkip);

WStr Dup(Arena*, WStr str, size_t cch = (size_t)-1);
WStr Dup(WStr s, size_t cch = (size_t)-1);
WStr Join(WStr, WStr, WStr s3 = {});
WStr Join(Arena*, WStr, WStr, WStr s3);
bool Eq(WStr s1, WStr s2);
bool EqI(WStr s1, WStr s2);
bool EqN(WStr s1, WStr s2, size_t len);
bool IsEmpty(WStr s);
bool StartsWith(WStr str, WStr prefix);
bool StartsWithI(WStr str, WStr prefix);
bool EndsWith(WStr txt, WStr end);
bool EndsWithI(WStr txt, WStr end);
WStr ToLower(WStr s);
WStr ToLowerInPlace(WStr s);
WStr Parse(WStr str, const WCHAR* format, ...);
int BufSet(WCHAR* dst, int dstCchSize, WStr src);
int BufSet(WCHAR* dst, int dstCchSize, Str src);
size_t NormalizeWSInPlace(WStr str);
size_t RemoveCharsInPlace(WStr str, WStr toRemove);
WStr FindChar(WStr str, WCHAR c);
WStr Find(WStr str, WStr find);
bool IsWs(WCHAR c);
bool IsDigit(WCHAR c);
bool IsNonCharacter(WCHAR c);
size_t TransCharsInPlace(WStr str, WStr oldChars, WStr newChars);
WStr Replace(WStr s, WStr toReplace, WStr replaceWith);

WStr CastToWCHAR(Str s);
} // namespace str

namespace url {

void DecodeInPlace(Str url);
bool IsAbsolute(Str url);
TempStr GetFullPathTemp(Str url);
TempStr GetFileNameTemp(Str url);

} // namespace url

using SeqStrings = const char*;

void SeqStrNext(const char*& s);
void SeqStrNext(const char*& s, int* idxInOut);
int SeqStrIndex(SeqStrings strs, Str toFind);
int SeqStrIndexIS(SeqStrings strs, Str toFind);
Str SeqStrByIndex(SeqStrings strs, int idx);

// SeqStrNum: like SeqStrings but each entry is <string>\0<varint i64>, sequence ends with \0.
// Varint is unsigned LEB128 of zigzag-encoded i64 (small for non-negative values).
// Use when mapping strings to arbitrary numbers (not just sequential indices).
// Parallel string[] + number[] tables elsewhere in the codebase (candidates for SeqStrNum):
//   Accelerators.cpp: gVirtKeysNum (generated by cmd/gen-code.ts)
// Index-is-the-number (SeqStrings suffices today): displayModeNames, gArgNames, gToolNames,
//   permNames, gScrollbarModeNames, gFileActionNames, gAnnotNames, kPdfFilterStateStrs, gLangCodes.
using SeqStrNum = const char*;

void SeqStrNumNext(const char*& s, int* idxInOut);
void SeqStrNumNext(const char*& s);
int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut);
int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut);
Str SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut);
Str SeqStrNumStrByNumber(SeqStrNum strs, i64 num);

#define _MemToHex(ptr) str::MemToHex((const u8*)(ptr), sizeof(*ptr))
#define _HexToMem(txt, ptr) str::HexToMem(txt, (u8*)(ptr), sizeof(*ptr))

struct StrBuilder {
    // allocator is not owned by Vec and must outlive it
    Arena* allocator = nullptr;
    // TODO: to save space (8 bytes), combine els and buf?
    char* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    char buf[32];

    int nReallocs = 0;

    static constexpr size_t kBufChars = dimof(buf);

    explicit StrBuilder(size_t capHint = 0, Arena* allocator = nullptr);
    StrBuilder(const StrBuilder& that);
    StrBuilder& operator=(const StrBuilder& that);
    StrBuilder(Str s);
    StrBuilder(const char* s) : StrBuilder(Str(s)) {} // NOLINT str-port shim

    ~StrBuilder();

    void Reset();
    char& at(size_t idx) const;
    char& at(int idx) const;
    char& operator[](size_t idx) const;
    char& operator[](long idx) const;
    char& operator[](int idx) const;
#if defined(_WIN64)
    char& at(u32 idx) const;
    char& operator[](u32 idx) const;
#endif
    size_t size() const;
    int Size() const;
    bool InsertAt(size_t idx, char el);
    bool AppendChar(char c);
    bool Append(const char* src, size_t count = -1);
    bool Append(const Str&);
    bool Append(const StrBuilder& s);
    char RemoveAt(size_t idx, size_t count = 1);
    char RemoveLast();
    char& Last() const;
    Str StealData(Arena* a = nullptr);
    char* LendData() const;
    bool Contains(Str s);
    bool IsEmpty() const;
    ByteSlice AsByteSlice() const;
    ByteSlice StealAsByteSlice();
    bool Append(const u8* src, size_t size = -1);
    bool AppendSlice(const ByteSlice& d);
    void AppendFmt(const char* fmt, ...);
    void Set(Str s);
    void Set(const char* s) { Set(Str(s)); } // str-port shim
    Str Get() const;
    bool Contains(const char* s, size_t sLen = 0) {
        return Contains(sLen ? Str(s, (int)sLen) : Str(s));
    } // str-port shim
    char* CStr() const;
    char LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = char*;

    iterator begin() const { return &(els[0]); }
    iterator end() const { return &(els[len]); }
};

void SeqStrNumAppend(StrBuilder* b, Str s, i64 num);
void SeqStrNumFinish(StrBuilder* b);

struct WStrBuilder {
    // allocator is not owned by Vec and must outlive it
    Arena* allocator = nullptr;
    WCHAR* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    WCHAR buf[32];

    static constexpr size_t kBufChars = dimof(buf);
    static constexpr size_t kElSize = sizeof(WCHAR);

    explicit WStrBuilder(size_t capHint = 0, Arena* allocator = nullptr);
    WStrBuilder(const WStrBuilder&);
    WStrBuilder(WStr s);
    WStrBuilder(const WCHAR* s) : WStrBuilder(WStr(s)) {} // NOLINT str-port shim
    WStrBuilder& operator=(const WStrBuilder& that);
    ~WStrBuilder();
    void Reset();
    WCHAR& at(size_t idx) const;
    WCHAR& at(int idx) const;
    WCHAR& operator[](size_t idx) const;
    WCHAR& operator[](long idx) const;
    WCHAR& operator[](ULONG idx) const;
    WCHAR& operator[](int idx) const;
#if defined(_WIN64)
    WCHAR& at(u32 idx) const;
    WCHAR& operator[](u32 idx) const;
#endif
    size_t size() const;
    int isize() const;
    bool InsertAt(size_t idx, const WCHAR& el);
    bool AppendChar(WCHAR);
    bool Append(WStr s);
    bool Append(const WCHAR* src, size_t count = -1);
    WCHAR RemoveAt(size_t idx, size_t count = 1);
    WCHAR RemoveLast();
    WCHAR& Last() const;
    WStr StealData();
    WCHAR* LendData() const;
    int Find(const WCHAR& el, size_t startAt = 0) const;
    bool Contains(const WCHAR& el) const;
    int Remove(const WCHAR& el);
    bool IsEmpty() const;
    void Set(WStr s);
    void Set(const WCHAR* s) { Set(WStr(s)); } // str-port shim
    WStr Get() const;
    WCHAR LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = WCHAR*;

    iterator begin() const { return &(els[0]); }
    iterator end() const { return &(els[len]); }
};

namespace str {

bool Replace(WStrBuilder& s, WStr toReplace, WStr replaceWith);

FORCEINLINE size_t Len(Str s) {
    return (size_t)s.len;
}
FORCEINLINE size_t Len(WStr s) {
    return (size_t)s.len;
}
// str-port: char*/WCHAR* shims; remove when callers use Str/WStr
FORCEINLINE size_t Len(const char* s) {
    return Len(Str(s));
}
FORCEINLINE size_t Len(char* s) {
    return Len(Str(s));
}
FORCEINLINE size_t Len(const WCHAR* s) {
    return Len(WStr(s));
}
FORCEINLINE size_t Len(WCHAR* s) {
    return Len(WStr(s));
}
FORCEINLINE int Leni(Str s) {
    return s.len;
}
FORCEINLINE int Leni(WStr s) {
    return s.len;
}
FORCEINLINE int Leni(const char* s) {
    return Leni(Str(s));
}
FORCEINLINE int Leni(char* s) {
    return Leni(Str(s));
}
FORCEINLINE int Leni(const WCHAR* s) {
    return Leni(WStr(s));
}
FORCEINLINE int Leni(WCHAR* s) {
    return Leni(WStr(s));
}
FORCEINLINE bool Eq(const char* s1, const char* s2) {
    return Eq(Str(s1), Str(s2));
}
FORCEINLINE bool Eq(Str s1, const char* s2) {
    return Eq(s1, Str(s2));
}
FORCEINLINE bool Eq(const char* s1, Str s2) {
    return Eq(Str(s1), s2);
}
FORCEINLINE bool Eq(const WCHAR* s1, const WCHAR* s2) {
    return Eq(WStr(s1), WStr(s2));
}
FORCEINLINE bool Eq(WStr s1, const WCHAR* s2) {
    return Eq(s1, WStr(s2));
}
FORCEINLINE bool Eq(const WCHAR* s1, WStr s2) {
    return Eq(WStr(s1), s2);
}
FORCEINLINE bool EqI(const char* s1, const char* s2) {
    return EqI(Str(s1), Str(s2));
}
FORCEINLINE bool EqI(Str s1, const char* s2) {
    return EqI(s1, Str(s2));
}
FORCEINLINE bool EqI(const char* s1, Str s2) {
    return EqI(Str(s1), s2);
}
FORCEINLINE bool EqI(const WCHAR* s1, const WCHAR* s2) {
    return EqI(WStr(s1), WStr(s2));
}
FORCEINLINE bool EqI(WStr s1, const WCHAR* s2) {
    return EqI(s1, WStr(s2));
}
FORCEINLINE bool EqI(const WCHAR* s1, WStr s2) {
    return EqI(WStr(s1), s2);
}
FORCEINLINE bool IsEmpty(const WCHAR* s) {
    return !s || !*s;
}
FORCEINLINE bool StartsWith(const u8* str, const char* prefix) {
    return StartsWith(str, Str(prefix));
}
FORCEINLINE bool StartsWith(const char* str, const char* prefix) {
    return StartsWith(Str(str), Str(prefix));
}
FORCEINLINE bool StartsWith(Str str, const char* prefix) {
    return StartsWith(str, Str(prefix));
}
FORCEINLINE bool StartsWith(const WCHAR* str, const WCHAR* prefix) {
    return StartsWith(WStr(str), WStr(prefix));
}
FORCEINLINE bool StartsWith(WStr str, const WCHAR* prefix) {
    return StartsWith(str, WStr(prefix));
}
FORCEINLINE bool StartsWith(const WCHAR* str, WStr prefix) {
    return StartsWith(WStr(str), prefix);
}
FORCEINLINE bool StartsWithI(const char* str, const char* prefix) {
    return StartsWithI(Str(str), Str(prefix));
}
FORCEINLINE bool StartsWithI(Str str, const char* prefix) {
    return StartsWithI(str, Str(prefix));
}
FORCEINLINE bool StartsWithI(const WCHAR* str, const WCHAR* prefix) {
    return StartsWithI(WStr(str), WStr(prefix));
}
FORCEINLINE bool StartsWithI(WStr str, const WCHAR* prefix) {
    return StartsWithI(str, WStr(prefix));
}
FORCEINLINE bool StartsWithI(const WCHAR* str, WStr prefix) {
    return StartsWithI(WStr(str), prefix);
}
FORCEINLINE bool EndsWith(const char* txt, const char* end) {
    return EndsWith(Str(txt), Str(end));
}
FORCEINLINE bool EndsWith(Str txt, const char* end) {
    return EndsWith(txt, Str(end));
}
FORCEINLINE bool EndsWith(const WCHAR* txt, const WCHAR* end) {
    return EndsWith(WStr(txt), WStr(end));
}
FORCEINLINE bool EndsWith(WStr txt, const WCHAR* end) {
    return EndsWith(txt, WStr(end));
}
FORCEINLINE bool EndsWith(const WCHAR* txt, WStr end) {
    return EndsWith(WStr(txt), end);
}
FORCEINLINE bool EndsWithI(const char* txt, const char* end) {
    return EndsWithI(Str(txt), Str(end));
}
FORCEINLINE bool EndsWithI(Str txt, const char* end) {
    return EndsWithI(txt, Str(end));
}
FORCEINLINE bool EndsWithI(const char* txt, Str end) {
    return EndsWithI(Str(txt), end);
}
FORCEINLINE bool EndsWithI(const WCHAR* txt, const WCHAR* end) {
    return EndsWithI(WStr(txt), WStr(end));
}
FORCEINLINE bool EndsWithI(WStr txt, const WCHAR* end) {
    return EndsWithI(txt, WStr(end));
}
FORCEINLINE bool EndsWithI(const WCHAR* txt, WStr end) {
    return EndsWithI(WStr(txt), end);
}
FORCEINLINE bool Contains(const char* s, const char* txt) {
    return Contains(Str(s), Str(txt));
}
FORCEINLINE bool Contains(Str s, const char* txt) {
    return Contains(s, Str(txt));
}
FORCEINLINE Str ToLowerInPlace(const char* s) {
    return ToLowerInPlace(Str((char*)s));
}
FORCEINLINE Str ToUpperInPlace(const char* s) {
    return ToUpperInPlace(Str((char*)s));
}

FORCEINLINE size_t TrimWSInPlace(char* s, TrimOpt opt) {
    return TrimWSInPlace(Str(s), opt);
}
FORCEINLINE size_t NormalizeWSInPlace(char* str) {
    return NormalizeWSInPlace(Str(str));
}
FORCEINLINE size_t NormalizeWSInPlace(WCHAR* str) {
    return NormalizeWSInPlace(WStr(str));
}

// str-port: char*/WCHAR* shims; remove when callers use Str/WStr
FORCEINLINE int BufSet(char* dst, int dstCchSize, const char* src) {
    return BufSet(dst, dstCchSize, Str(src));
}
FORCEINLINE int BufAppend(char* dst, int dstCchSize, const char* s) {
    return BufAppend(dst, dstCchSize, Str(s));
}
FORCEINLINE int BufSet(WCHAR* dst, int dstCchSize, const WCHAR* src) {
    return BufSet(dst, dstCchSize, WStr(src));
}
FORCEINLINE int BufSet(WCHAR* dst, int dstCchSize, const char* src) {
    return BufSet(dst, dstCchSize, Str(src));
}
FORCEINLINE int BufSet(WStr dst, int dstCchSize, WStr src) {
    return BufSet(dst.s, dstCchSize, src);
}
FORCEINLINE Str Dup(Arena* a, const char* s, size_t cch = (size_t)-1) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        return Dup(a, Str(s));
    }
    return Dup(a, Str((char*)s, (int)cch));
}
FORCEINLINE Str Dup(const char* s, size_t cch = (size_t)-1) {
    return Dup(nullptr, s, cch);
}
FORCEINLINE WStr Dup(Arena* a, const WCHAR* s, size_t cch = (size_t)-1) {
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        return Dup(a, WStr(s));
    }
    return Dup(a, WStr((WCHAR*)s, (int)cch));
}
FORCEINLINE WStr Dup(const WCHAR* s, size_t cch = (size_t)-1) {
    return Dup(nullptr, s, cch);
}
FORCEINLINE Str Find(const Str& str, const char* find) {
    return Find(str, Str(find));
}
FORCEINLINE Str FindI(const Str& str, const char* find) {
    return FindI(str, Str(find));
}
FORCEINLINE Str ToLower(const char* s) {
    return ToLower(Str(s));
}
FORCEINLINE bool EqIS(const char* s1, const char* s2) {
    return EqIS(Str(s1), Str(s2));
}
FORCEINLINE bool EqN(const char* s1, const char* s2, size_t len) {
    return EqN(Str(s1), Str(s2), len);
}
FORCEINLINE bool EqNI(const char* s1, const char* s2, size_t len) {
    return EqNI(Str(s1), Str(s2), len);
}
FORCEINLINE bool EqNIx(const char* s, size_t len, const char* s2) {
    return EqNIx(Str(s), len, Str(s2));
}
FORCEINLINE bool ContainsI(const char* s, const char* txt) {
    return ContainsI(Str(s), Str(txt));
}
FORCEINLINE bool ContainsI(Str s, const char* txt) {
    return ContainsI(s, Str(txt));
}
FORCEINLINE bool ContainsI(const char* s, Str txt) {
    return ContainsI(Str(s), txt);
}
FORCEINLINE char* FindChar(char* str, char c) {
    Str res = FindChar(Str(str), c);
    return res.s;
}
FORCEINLINE const char* FindChar(const char* str, char c) {
    Str res = FindChar(Str((char*)str), c);
    return res.s;
}
FORCEINLINE const WCHAR* FindChar(const WCHAR* str, WCHAR c) {
    return FindChar(WStr(str), c).s;
}
FORCEINLINE WCHAR* FindChar(WCHAR* str, WCHAR c) {
    return FindChar(WStr(str), c).s;
}
FORCEINLINE char* FindCharLast(char* str, char c) {
    Str res = FindCharLast(Str(str), c);
    return res.s;
}
FORCEINLINE const char* FindCharLast(const char* str, char c) {
    Str res = FindCharLast(Str((char*)str), c);
    return res.s;
}
FORCEINLINE int FindCharIdx(const char* str, char c) {
    return FindCharIdx(Str((char*)str), c);
}
FORCEINLINE const WCHAR* Find(const WCHAR* str, const WCHAR* find) {
    return Find(WStr(str), WStr(find)).s;
}
FORCEINLINE const WCHAR* Find(const WStr& str, const WCHAR* find) {
    return Find(str, WStr(find)).s;
}
FORCEINLINE const WCHAR* Find(const WCHAR* str, WStr find) {
    return Find(WStr(str), find).s;
}
FORCEINLINE bool HexToMem(const char* s, u8* buf, size_t bufLen) {
    return HexToMem(Str((char*)s), buf, bufLen);
}
const char* Parse(const char* str, const char* fmt, ...);
const char* Parse(const char* str, size_t len, const char* fmt, ...);
FORCEINLINE int CmpNatural(const char* a, const char* b) {
    return CmpNatural(Str(a), Str(b));
}

FORCEINLINE bool IsEmptyOrWhiteSpace(const char* s) {
    return IsEmptyOrWhiteSpace(Str(s));
}
FORCEINLINE bool Skip(const char*& s, const char* toSkip) {
    Str slice((char*)s);
    if (Skip(slice, Str(toSkip))) {
        s = slice.s;
        return true;
    }
    return false;
}
FORCEINLINE const char* SkipChar(const char* s, char toSkip) {
    return SkipChar(Str((char*)s), toSkip).s;
}
FORCEINLINE WStr CastToWCHAR(const char* s) {
    return CastToWCHAR(Str((char*)s));
}
FORCEINLINE int SeqStrIndex(SeqStrings strs, const char* toFind) {
    return SeqStrIndex(strs, Str(toFind));
}
FORCEINLINE int SeqStrIndexIS(SeqStrings strs, const char* toFind) {
    return SeqStrIndexIS(strs, Str(toFind));
}
FORCEINLINE int SeqStrNumIndex(SeqStrNum strs, const char* toFind, i64* numOut) {
    return SeqStrNumIndex(strs, Str(toFind), numOut);
}
FORCEINLINE int SeqStrNumIndexIS(SeqStrNum strs, const char* toFind, i64* numOut) {
    return SeqStrNumIndexIS(strs, Str(toFind), numOut);
}
FORCEINLINE void SeqStrNumAppend(StrBuilder* b, const char* s, i64 num) {
    SeqStrNumAppend(b, Str(s), num);
}

} // namespace str

int ParseInt(Str s);
i64 ParseInt64(Str s);
bool IsValidProgramVersion(Str ver);
int CompareProgramVersion(Str ver1, Str ver2);
FORCEINLINE int ParseInt(const char* s) {
    return ParseInt(Str(s));
} // str-port shim
FORCEINLINE i64 ParseInt64(const char* s) {
    return ParseInt64(Str(s));
} // str-port shim
FORCEINLINE bool IsValidProgramVersion(const char* ver) {
    return IsValidProgramVersion(Str(ver));
} // str-port shim
FORCEINLINE int CompareProgramVersion(const char* ver1, const char* ver2) {
    return CompareProgramVersion(Str(ver1), Str(ver2));
} // str-port shim
TempStr ShortenStringUtf8Temp(Str s, int maxRunes);
FORCEINLINE TempStr ShortenStringUtf8Temp(const char* s, int maxRunes) {
    return ShortenStringUtf8Temp(Str(s), maxRunes);
}
TempStr ShortenStringUtf8InTheMiddleTemp(Str s, int maxRunes);
FORCEINLINE TempStr ShortenStringUtf8InTheMiddleTemp(const char* s, int maxRunes) {
    return ShortenStringUtf8InTheMiddleTemp(Str(s), maxRunes);
}
bool IsTextRtl(WStr s);
bool IsTextRtl(Str s);
FORCEINLINE bool IsTextRtl(const WCHAR* s) {
    return IsTextRtl(WStr(s));
} // str-port shim
FORCEINLINE bool IsTextRtl(const char* s) {
    return IsTextRtl(Str(s));
} // str-port shim
