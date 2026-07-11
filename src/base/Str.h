/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

#define UTF8_BOM "\xEF\xBB\xBF"
#define UTF16_BOM "\xFF\xFE"
#define UTF16BE_BOM "\xFE\xFF"

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
bool EqI(Str s1, Str s2);
bool EqIS(Str s1, Str s2);
bool EqN(Str s1, Str s2, int n);
bool EqNI(Str s1, Str s2, int n);
bool IsNull(const Str& s);
bool StartsWith(Str str, Str prefix);

bool StartsWithI(Str str, Str prefix);
bool EndsWith(Str txt, Str end);
bool EndsWithI(Str txt, Str end);
bool EqNIx(Str s, int n, Str s2);

Str ToLowerInPlace(Str s);

Str ToLower(Str s);

Str ToUpperInPlace(Str s);

bool IsDigit(char c);
bool IsWs(char c);
bool IsAlNum(char c);

Str SliceFromChar(Str str, char c);
Str SliceFromCharLast(Str str, char c);
int IndexOfChar(Str s, char c);
int IndexOf(Str buf, Str toFind);
int IndexOfI(Str s, Str toFind);
int IndexOfAfter(Str s, Str needle);
bool Cut(Str s, Str sep, Str* before, Str* after);
bool CutChar(Str s, char c, Str* before, Str* after);
bool CutCharLast(Str s, char c, Str* before, Str* after);
bool NextLine(Str s, Str& line, Str& rest);

bool Contains(Str s, Str txt);
bool ContainsI(Str s, Str txt);
bool ContainsChar(Str s, char c);

Str TrimSuffix(Str s, Str suffix);
int LastIndexOfChar(Str s, char c);
Str TrimSuffixWhitespace(Str s); // trims trailing whitespace in place

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith);
TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith);

int TrimWSInPlace(Str s, TrimOpt opt);

void TransCharsInPlace(Str& str, Str oldChars, Str newChars);

int NormalizeWSInPlace(Str str);
TempStr NormalizeWSTemp(Str s);
int NormalizeNewlinesInPlace(Str s, Str endExclusive);
int NormalizeNewlinesInPlace(Str s);
int RemoveCharsInPlace(Str str, Str toRemove);

int BufSet(Str dst, Str src);
int BufAppend(Str dst, Str s);

TempStr MemToHexTemp(Str buf);
bool HexToMem(Str s, Str buf);

int CmpNatural(Str a, Str b);

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
bool EqN(WStr s1, WStr s2, int n);
bool EqNI(WStr s1, WStr s2, int n);
bool IsNull(const WStr& s);
bool StartsWith(WStr str, WStr prefix);
bool StartsWithI(WStr str, WStr prefix);
bool EndsWith(WStr txt, WStr end);
bool EndsWithI(WStr txt, WStr end);
WStr ToLower(WStr s);
WStr ToLowerInPlace(WStr s);
int BufSet(WStr dst, WStr src);
int NormalizeWSInPlace(WStr str);
int RemoveCharsInPlace(WStr str, WStr toRemove);
int IndexOfChar(WStr s, WCHAR c);
bool ContainsChar(WStr s, WCHAR c);
WStr SliceFromChar(WStr str, WCHAR c);
WStr FindFrom(WStr str, WStr find);
bool IsWs(WCHAR c);
bool IsDigit(WCHAR c);
bool IsNonCharacter(WCHAR c);
void TransCharsInPlace(WStr& str, WStr oldChars, WStr newChars);
WStr Replace(WStr s, WStr toReplace, WStr replaceWith);

} // namespace wstr

namespace url {

void DecodeInPlace(Str url);
bool IsAbsolute(Str url);
TempStr GetFullPathTemp(Str url);
TempStr GetFileNameTemp(Str url);

} // namespace url

using SeqStrings = const char*;

TempStr SeqStrAt(SeqStrings strs, int off);
bool SeqStrAdvance(SeqStrings strs, int& off, int* idxInOut = nullptr);
int SeqStrIndex(SeqStrings strs, Str toFind);
int SeqStrIndexIS(SeqStrings strs, Str toFind);
TempStr SeqStrByIndex(SeqStrings strs, int idx);

// look up the mime type for a file extension (e.g. ".png" -> "image/png");
// returns {} for unknown extensions. If the matched type is an image and
// imgExt (the extension detected from the file's data) is given, it wins.
TempStr MimeTypeFromExtTemp(Str ext, Str imgExt = {});

// SeqStrNum: like SeqStrings but each entry is <string>\0<varint i64>, sequence ends with \0.
// Varint is unsigned LEB128 of zigzag-encoded i64 (small for non-negative values).
// Use when mapping strings to arbitrary numbers (not just sequential indices).
// Parallel string[] + number[] tables elsewhere in the codebase (candidates for SeqStrNum):
//   Accelerators.cpp: gVirtKeysNum (generated by cmd/gen-code.ts)
// Index-is-the-number (SeqStrings suffices today): displayModeNames, gArgNames, gToolNames,
//   permNames, gScrollbarModeNames, gFileActionNames, gAnnotNames, kPdfFilterStateStrs, gLangCodes.
using SeqStrNum = const char*;

TempStr SeqStrNumAt(SeqStrNum strs, int off);
bool SeqStrNumAdvance(SeqStrNum strs, int& off, int* idxInOut = nullptr);
int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut);
int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut);
TempStr SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut);
TempStr SeqStrNumStrByNumber(SeqStrNum strs, i64 num);

namespace str {
struct Builder {
    // arena is not owned by Builder and must outlive it
    Arena* a = nullptr;
    // TODO: to save space (8 bytes), combine els and buf?
    char* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    char buf[32];

    int nReallocs = 0;

    static constexpr size_t kBufChars = dimof(buf);

    explicit Builder(int capHint = 0, Arena* a = nullptr);
    Builder(Str s);
    // the implicit memberwise copy would alias els and double-free it
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    void Reset(Str s = {});
    char& operator[](int idx) const;
    bool InsertAt(int idx, char el);
    bool AppendChar(char c);
    bool Append(Str src);
    char RemoveAt(int idx, int count = 1);
    char RemoveLast();
    char& Last() const;
    Str TakeStr();
    bool IsEmpty() const;
    char LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = char*;

    iterator begin() const { return &(els[0]); }
    iterator end() const { return &(els[len]); }
};

bool Contains(const Builder& b, Str s);
} // namespace str

void SeqStrNumAppend(str::Builder* b, Str s, i64 num);
void SeqStrNumFinish(str::Builder* b);

namespace wstr {
struct Builder {
    // arena is not owned by Builder and must outlive it
    Arena* a = nullptr;
    WCHAR* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    WCHAR buf[32];

    static constexpr size_t kBufChars = dimof(buf);
    static constexpr size_t kElSize = sizeof(WCHAR);

    explicit Builder(int capHint = 0, Arena* a = nullptr);
    Builder(const Builder&);
    Builder(WStr s);
    Builder& operator=(const Builder& that);
    ~Builder();
    void Reset(WStr s = {});
    WCHAR& operator[](int idx) const;
    bool InsertAt(int idx, const WCHAR& el);
    bool AppendChar(WCHAR);
    bool Append(WStr src);
    WCHAR RemoveAt(int idx, int count = 1);
    WCHAR RemoveLast();
    WStr TakeWStr();
    bool IsEmpty() const;
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
bool ContainsChar(const Builder& b, WCHAR el);

} // namespace wstr

int ParseInt(Str s);
i64 ParseInt64(Str s);
bool IsValidProgramVersion(Str ver);
int CompareProgramVersion(Str ver1, Str ver2);
bool IsTextRtl(WStr s);
bool IsTextRtl(Str s);

// Temporary, guaranteed zero-terminated copy of s (lives in the temp arena).
// Use when passing a Str/WStr to a C or win32 API that requires a
// NUL-terminated string; the name documents that intent at the call site.
// Returns non-const so it implicitly converts to both char* and const char*
// (some C/win32 APIs take non-const), avoiding casts at the call site.
char* CStrTemp(Str s);
WCHAR* CWStrTemp(WStr s);

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

// Str utilities
Str FormatFileSize(Arena* arena, u64 size);
void FormatFileSizeToWstrBuf(u64 size, WStr buf);
int FormatSizeHumanIntoBuf(u64 size, Str buf);
void FormatSizeHumanIntoWBuf(u64 size, WStr wbuf);
