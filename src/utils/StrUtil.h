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
    explicit ByteSlice(Str str) {
        d = (u8*)str.s;
        sz = (size_t)str.len;
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
    operator const char*() { return (const char*)d; } // str-port: C-string
};

bool IsEqual(const ByteSlice&, const ByteSlice&);

FORCEINLINE Str AsStr(ByteSlice bs) {
    return Str((char*)bs.data(), bs.Size()); // str-port: byte slice view
}

namespace str {

enum class TrimOpt {
    Left,
    Right,
    Both
};

void Free(Str s);
// catch passing a raw char* (e.g. the .s member): pass the Str directly instead
// -- going through the pointer builds an unnecessary temp Str (with a strlen).
// To free a raw owned pointer use ::free().
void Free(const char*) = delete;
void FreePtr(Str* s);

Str Dup(Arena*, Str str);
Str Dup(Str s);
Str Dup(const ByteSlice&);
TempStr DupTemp(Str s);
TempWStr DupTemp(WStr s);

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
TempStr JoinTemp(Str s1, Str s2, Str s3 = {});
TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4);
TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4, Str s5);
TempWStr JoinTemp(WStr s1, WStr s2, WStr s3 = {});

bool Eq(Str s1, Str s2);
bool Eq(const ByteSlice& sp1, const ByteSlice& sp2);
bool EqI(Str s1, Str s2);
bool EqIS(Str s1, Str s2);
bool EqN(Str s1, Str s2, size_t len);
bool EqNI(Str s1, Str s2, size_t len);
bool IsNull(const Str& s);
bool IsEmpty(Str s);
bool StartsWith(Str str, Str prefix);

bool StartsWithI(Str str, Str prefix);
bool EndsWith(Str txt, Str end);
bool EndsWithI(Str txt, Str end);
bool EqNIx(Str s, size_t len, Str s2);

Str ToLowerInPlace(Str s);

Str ToLower(Str s);

Str ToUpperInPlace(Str s);

void Utf8Encode(char* buf, int& off, int c); // str-port: owned heap write buffer

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

bool BufFmtV(char* buf, size_t bufCchSize, Str fmt, va_list args); // str-port: C-string
bool BufFmt(char* buf, size_t bufCchSize, Str fmt, ...);           // str-port: C-string
// formatting functions take the format string as a plain const char* (as an
// exception to the Str rule): it's almost always a string literal, and a
// const char* is what vsnprintf needs anyway (no NUL-termination footgun).
TempStr FmtVTemp(const char* fmt, va_list args);
TempStr FormatTemp(const char* fmt, ...);

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith);
TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith);

size_t TrimWSInPlace(Str s, TrimOpt opt);

size_t TransCharsInPlace(Str str, Str oldChars, Str newChars);

size_t NormalizeWSInPlace(Str str);
size_t NormalizeNewlinesInPlace(Str s, Str endExclusive);
size_t NormalizeNewlinesInPlace(Str s);
size_t RemoveCharsInPlace(Str str, Str toRemove);

int BufSet(char* dst, int dstCchSize, Str src);  // str-port: C-string
int BufAppend(char* dst, int dstCchSize, Str s); // str-port: C-string

TempStr MemToHexTemp(const u8* buf, size_t len);
bool HexToMem(Str s, u8* buf, size_t bufLen);

Str Parse(Str str, Str fmt, ...);
Str Parse(Str str, size_t len, Str fmt, ...);

int CmpNatural(Str a, Str b);

TempStr FormatFloatWithThousandSepTemp(double number, LCID locale = LOCALE_USER_DEFAULT, bool stripTrailingZero = true);
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale = LOCALE_USER_DEFAULT);
TempStr FormatSizeShortTemp(i64 size);
TempStr FormatSizeShortTemp(i64 size, Str const* sizeUnits);
TempStr FormatFileSizeTemp(i64);
TempStr FormatRomanNumeralTemp(int number);

bool IsEmptyOrWhiteSpace(Str s);
bool Skip(Str& s, Str toSkip);
bool SkipChar(Str& s, char toSkip);

int BufSet(WCHAR* dst, int dstCchSize, Str src); // str-port: Win32 (wide dst, utf8 src)

WStr CastToWCHAR(Str s);
} // namespace str

// wide (WStr/WCHAR) counterparts of the str:: functions above
namespace wstr {

void Free(WStr s);
// catch passing a raw wchar_t* (e.g. the .s member): pass the WStr directly
// instead. To free a raw owned pointer use ::free().
void Free(const wchar_t*) = delete;
void FreePtr(WStr* s);

WStr Dup(Arena*, WStr str);
WStr Dup(WStr s);
WStr Join(WStr, WStr, WStr s3 = {});
WStr Join(Arena*, WStr, WStr, WStr s3);
bool Eq(WStr s1, WStr s2);
bool EqI(WStr s1, WStr s2);
bool EqN(WStr s1, WStr s2, size_t len);
bool IsNull(const WStr& s);
bool IsEmpty(WStr s);
bool StartsWith(WStr str, WStr prefix);
bool StartsWithI(WStr str, WStr prefix);
bool EndsWith(WStr txt, WStr end);
bool EndsWithI(WStr txt, WStr end);
WStr ToLower(WStr s);
WStr ToLowerInPlace(WStr s);
WStr Parse(WStr str, WStr format, ...);
int BufSet(WCHAR* dst, int dstCchSize, WStr src);
size_t NormalizeWSInPlace(WStr str);
size_t RemoveCharsInPlace(WStr str, WStr toRemove);
int FindCharIdx(WStr str, WCHAR c);
WStr FindChar(WStr str, WCHAR c);
WStr Find(WStr str, WStr find);
bool IsWs(WCHAR c);
bool IsDigit(WCHAR c);
bool IsNonCharacter(WCHAR c);
size_t TransCharsInPlace(WStr str, WStr oldChars, WStr newChars);
WStr Replace(WStr s, WStr toReplace, WStr replaceWith);

} // namespace wstr

namespace url {

void DecodeInPlace(Str url);
bool IsAbsolute(Str url);
TempStr GetFullPathTemp(Str url);
TempStr GetFileNameTemp(Str url);

} // namespace url

using SeqStrings = const char*; // str-port: packed generated string table base

Str SeqStrAt(SeqStrings strs, int off);
bool SeqStrAdvance(SeqStrings strs, int& off, int* idxInOut = nullptr);
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
using SeqStrNum = const char*; // str-port: packed generated string+number table base

Str SeqStrNumAt(SeqStrNum strs, int off);
bool SeqStrNumAdvance(SeqStrNum strs, int& off, int* idxInOut = nullptr);
int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut);
int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut);
Str SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut);
Str SeqStrNumStrByNumber(SeqStrNum strs, i64 num);

struct StrBuilder {
    // allocator is not owned by Vec and must outlive it
    Arena* allocator = nullptr;
    // TODO: to save space (8 bytes), combine els and buf?
    char* els = nullptr; // str-port: owned heap
    u32 len = 0;
    u32 cap = 0;
    char buf[32];

    int nReallocs = 0;

    static constexpr size_t kBufChars = dimof(buf);

    explicit StrBuilder(size_t capHint = 0, Arena* allocator = nullptr);
    StrBuilder(const StrBuilder& that);
    StrBuilder& operator=(const StrBuilder& that);
    StrBuilder(Str s);

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
    bool Append(Str src, size_t count = (size_t)-1);
    bool Append(const StrBuilder& s);
    char RemoveAt(size_t idx, size_t count = 1);
    char RemoveLast();
    char& Last() const;
    Str StealData(Arena* a = nullptr);
    Str LendData() const;
    bool Contains(Str s);
    bool IsEmpty() const;
    ByteSlice AsByteSlice() const;
    ByteSlice StealAsByteSlice();
    bool Append(const u8* src, size_t size = -1);
    bool AppendSlice(const ByteSlice& d);
    void AppendFmt(const char* fmt, ...);
    void Set(Str s);
    Str Get() const;
    Str CStr() const;
    char LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = char*; // str-port: owned heap

    iterator begin() const { return &(els[0]); }
    iterator end() const { return &(els[len]); }
};

void SeqStrNumAppend(StrBuilder* b, Str s, i64 num);
void SeqStrNumFinish(StrBuilder* b);

struct WStrBuilder {
    // allocator is not owned by Vec and must outlive it
    Arena* allocator = nullptr;
    WCHAR* els = nullptr; // str-port: owned heap
    u32 len = 0;
    u32 cap = 0;
    WCHAR buf[32];

    static constexpr size_t kBufChars = dimof(buf);
    static constexpr size_t kElSize = sizeof(WCHAR);

    explicit WStrBuilder(size_t capHint = 0, Arena* allocator = nullptr);
    WStrBuilder(const WStrBuilder&);
    WStrBuilder(WStr s);
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
    bool Append(WStr src, size_t count = (size_t)-1);
    WCHAR RemoveAt(size_t idx, size_t count = 1);
    WCHAR RemoveLast();
    WCHAR& Last() const;
    WStr StealData();
    WStr LendData() const;
    int Find(const WCHAR& el, size_t startAt = 0) const;
    bool Contains(const WCHAR& el) const;
    int Remove(const WCHAR& el);
    bool IsEmpty() const;
    void Set(WStr s);
    WStr Get() const;
    WCHAR LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = WCHAR*; // str-port: owned heap

    iterator begin() const { return &(els[0]); }
    iterator end() const { return &(els[len]); }
};

namespace str {

FORCEINLINE size_t Len(Str s) {
    return (size_t)s.len;
}
FORCEINLINE int Leni(Str s) {
    return s.len;
}

} // namespace str

namespace wstr {

bool Replace(WStrBuilder& s, WStr toReplace, WStr replaceWith);

FORCEINLINE size_t Len(WStr s) {
    return (size_t)s.len;
}
FORCEINLINE int Leni(WStr s) {
    return s.len;
}

FORCEINLINE int BufSet(WStr dst, int dstCchSize, WStr src) {
    return BufSet(dst.s, dstCchSize, src);
}

} // namespace wstr

int ParseInt(Str s);
i64 ParseInt64(Str s);
bool IsValidProgramVersion(Str ver);
int CompareProgramVersion(Str ver1, Str ver2);
TempStr ShortenStringUtf8Temp(Str s, int maxRunes);
TempStr ShortenStringUtf8InTheMiddleTemp(Str s, int maxRunes);
bool IsTextRtl(WStr s);
bool IsTextRtl(Str s);

// Temporary, guaranteed zero-terminated copy of s (lives in the temp arena).
// Use when passing a Str/WStr to a C or win32 API that requires a
// NUL-terminated string; the name documents that intent at the call site.
// Returns non-const so it implicitly converts to both char* and const char*
// (some C/win32 APIs take non-const), avoiding casts at the call site.
char* CStrTemp(Str s);
WCHAR* CWStrTemp(WStr s);

TempWStr ToWStrTempFromBuilder(const StrBuilder& s);
