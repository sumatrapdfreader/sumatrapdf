/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

#define UTF8_BOM "\xEF\xBB\xBF"
#define UTF16_BOM "\xFF\xFE"
#define UTF16BE_BOM "\xFE\xFF"

// specialized for Str and WStr
inline bool IsEmpty(const Str& v) {
    return v.len == 0 || !v.s;
}
inline bool IsEmpty(const Str* v) {
    return !v || v->len == 0 || !v->s;
}
inline bool IsEmpty(const WStr& v) {
    return v.len == 0 || !v.s;
}
inline bool IsEmpty(const WStr* v) {
    return !v || v->len == 0 || !v->s;
}

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
        d = data.d;
        sz = data.sz;
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
    bool empty() const { return !d; }
    bool IsEmpty() const { return !d; }
    void Free() {
        free(d);
        d = nullptr;
        sz = 0;
    }
    operator const char*() { return (const char*)d; }
};

inline int len(const ByteSlice s) {
    return (int)s.sz;
}

bool IsEqual(const ByteSlice&, const ByteSlice&);

FORCEINLINE Str AsStr(ByteSlice bs) {
    return Str((char*)bs.data(), (int)bs.sz);
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
bool EqNIx(Str s, int len, Str s2);

Str ToLowerInPlace(Str s);

Str ToLower(Str s);

Str ToUpperInPlace(Str s);

void Utf8Encode(char* buf, int& off, int c);

bool IsDigit(char c);
bool IsWs(char c);
bool IsAlNum(char c);

Str FindChar(Str str, char c);
Str FindCharLast(Str str, char c);
int IndexOfChar(Str s, char c);
int IndexOf(Str buf, Str toFind);
int IndexOfI(Str s, Str toFind);
int IndexOfAfter(Str s, Str needle);
bool Cut(Str s, Str sep, Str* before, Str* after);
bool NextLine(Str s, Str& line, Str& rest);

bool Contains(Str s, Str txt);
bool ContainsI(Str s, Str txt);
bool ContainsChar(Str s, char c);

Str TrimSuffix(Str s, Str suffix);
int LastIndexOfChar(Str s, char c);
Str TrimSuffixWhitespace(Str s); // trims trailing whitespace in place

bool BufFmtV(char* buf, size_t bufCchSize, const char* fmt, va_list args);
bool BufFmt(char* buf, size_t bufCchSize, const char* fmt, ...);
// formatting functions take the format string as a plain const char* (as an
// exception to the Str rule): it's almost always a string literal, and a
// const char* is what vsnprintf needs anyway (no NUL-termination footgun).

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith);
TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith);

size_t TrimWSInPlace(Str s, TrimOpt opt);

size_t TransCharsInPlace(Str str, Str oldChars, Str newChars);

size_t NormalizeWSInPlace(Str str);
size_t NormalizeNewlinesInPlace(Str s, Str endExclusive);
size_t NormalizeNewlinesInPlace(Str s);
size_t RemoveCharsInPlace(Str str, Str toRemove);

int BufSet(char* dst, int dstCchSize, Str src);
int BufAppend(char* dst, int dstCchSize, Str s);

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

int BufSet(WCHAR* dst, int dstCchSize, Str src);

WStr CastToWCHAR(Str s);
} // namespace str

void SplitStrByWhitespace(Arena* arena, const Str& s, VecStr& vecOut);

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
int IndexOfChar(WStr s, WCHAR c);
bool ContainsChar(WStr s, WCHAR c);
WStr FindChar(WStr str, WCHAR c);
WStr FindFrom(WStr str, WStr find);
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

using SeqStrings = const char*;

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
using SeqStrNum = const char*;

Str SeqStrNumAt(SeqStrNum strs, int off);
bool SeqStrNumAdvance(SeqStrNum strs, int& off, int* idxInOut = nullptr);
int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut);
int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut);
Str SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut);
Str SeqStrNumStrByNumber(SeqStrNum strs, i64 num);

namespace str {
struct Builder {
    // allocator is not owned by Vec and must outlive it
    Arena* allocator = nullptr;
    // TODO: to save space (8 bytes), combine els and buf?
    char* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    char buf[32];

    int nReallocs = 0;

    static constexpr size_t kBufChars = dimof(buf);

    explicit Builder(int capHint = 0, Arena* allocator = nullptr);
    Builder(const Builder& that);
    Builder& operator=(const Builder& that);
    Builder(Str s);

    ~Builder();

    void Reset();
    char& operator[](int idx) const;
    bool InsertAt(int idx, char el);
    bool AppendChar(char c);
    bool Append(Str src);
    bool Append(const Builder& s);
    char RemoveAt(int idx, int count = 1);
    char RemoveLast();
    char& Last() const;
    Str StealData(Arena* a = nullptr);
    Str LendData() const;
    bool Contains(Str s);
    bool IsEmpty() const;
    ByteSlice AsByteSlice() const;
    ByteSlice StealAsByteSlice();
    bool Append(const u8* src, int size = -1);
    bool AppendSlice(const ByteSlice& d);
    void Set(Str s);
    Str CStr() const;
    char LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = char*;

    iterator begin() const { return &(els[0]); }
    iterator end() const { return &(els[len]); }
};
} // namespace str

void SeqStrNumAppend(str::Builder* b, Str s, i64 num);
void SeqStrNumFinish(str::Builder* b);

namespace wstr {
struct Builder {
    // allocator is not owned by Vec and must outlive it
    Arena* allocator = nullptr;
    WCHAR* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    WCHAR buf[32];

    static constexpr size_t kBufChars = dimof(buf);
    static constexpr size_t kElSize = sizeof(WCHAR);

    explicit Builder(int capHint = 0, Arena* allocator = nullptr);
    Builder(const Builder&);
    Builder(WStr s);
    Builder& operator=(const Builder& that);
    ~Builder();
    void Reset();
    WCHAR& operator[](int idx) const;
    bool InsertAt(int idx, const WCHAR& el);
    bool AppendChar(WCHAR);
    bool Append(WStr src);
    WCHAR RemoveAt(int idx, int count = 1);
    WCHAR RemoveLast();
    WCHAR& Last() const;
    WStr StealData();
    WStr LendData() const;
    int Find(const WCHAR& el, int startAt = 0) const;
    bool Contains(const WCHAR& el) const;
    int Remove(const WCHAR& el);
    bool IsEmpty() const;
    void Set(WStr s);
    WCHAR LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = WCHAR*;

    iterator begin() const { return &(els[0]); }
    iterator end() const { return &(els[len]); }
};
} // namespace wstr

namespace wstr {

bool Replace(Builder& s, WStr toReplace, WStr replaceWith);

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
WCHAR* CWStrTemp(Str s);

// like CWStrTemp but also reports the wide-char count (excluding the NUL) via
// cch. Use when a C/win32 API needs both the pointer and a length (e.g. a
// cbData byte count) — clearer than ToWStrTemp(...).s + .len at the call site.
WCHAR* CWStrTemp(Str s, int& cch);
WCHAR* CWStrTemp(WStr s, int& cch);

// str::Builder/wstr::Builder always keep their data NUL-terminated.
// ToStr() returns a {ptr,len} view (may contain embedded NULs).
// ToCStr() returns the NUL-terminated buffer, for passing to C/win32 code we
// don't control that expects a zero-terminated char*/WCHAR*.
Str ToStr(const str::Builder&);
char* ToCStr(const str::Builder&);
WStr ToWStr(const wstr::Builder&);
WCHAR* ToWCStr(const wstr::Builder&);

// owning temp-arena copy of the builder's content (unlike ToStr()'s view)
TempStr ToStrTemp(const str::Builder&);

int len(const str::Builder&);
int len(const wstr::Builder&);

wchar_t ToLowerW(wchar_t c);
int WStrFindSubstr(WStr str, WStr substr);
int WStrCmpNoCase(WStr a, WStr b);

WStr ToWStrTemp(Str s);
Str ToUtf8(Arena* arena, WStr wide);
Str ToUtf8Temp(WStr wide);

// Str utilities
Str FormatFileSize(Arena* arena, u64 size);
void FormatFileSizeToWstrBuf(u64 size, WStr buf);
int FormatSizeHumanIntoBuf(u64 size, Str buf);
void FormatSizeHumanIntoWBuf(u64 size, WStr wbuf);
