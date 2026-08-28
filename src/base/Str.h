/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

#define kUtf8Bom "\xEF\xBB\xBF"
#define kUtf16Bom "\xFF\xFE"
#define kUtf16BeBom "\xFE\xFF"

using StrArena = u32;
StrArena StrArenaAlloc(Arena* a, int size);
StrArena StrArenaDupStr(Arena* a, Str s);
Str StrArenaToStr(Arena* a, StrArena sa);

// Singly-linked string node; AllocStrNode places the string bytes immediately
// after the node in one allocation (s.s points into that block).
struct StrNode {
    StrNode* next = nullptr;
    Str s;
};
StrNode* AllocStrNode(Arena* a, Str s);
StrNode* FindStrNode(StrNode* root, Str s);
void FreeStrNode(Arena* a, StrNode* head);

// Head/tail list of StrNodes (first→…→last via next). Does not own/free nodes.
struct StrNodeList {
    StrNode* head = nullptr;
    StrNode* tail = nullptr;
};
void StrNodeListPush(StrNodeList* list, StrNode* n);
void StrNodeListPop(StrNodeList* list);

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
// inline so callers (and the static analyzer) can see the guard
inline bool IsNull(const Str& s) {
    return !s.s;
}
bool StartsWith(Str str, Str prefix);
bool TrimPrefix(Str& s, Str prefix);

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

bool Contains(Str s, Str sub);
bool ContainsI(Str s, Str sub);
bool ContainsChar(Str s, char c);
bool ContainsCharAny(Str s, Str chars);

Str TrimSuffix(Str s, Str suffix);
int LastIndexOfChar(Str s, char c);
Str TrimSuffixWhitespace(Str s); // trims trailing whitespace in place

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith);
TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith);

int TrimWSInPlace(Str& s, TrimOpt opt);

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
int Cmp(Str a, Str b);
int CmpI(Str a, Str b);

bool IsEmptyOrWhiteSpace(Str s);
bool SkipChar(Str& s, char toSkip);
int SkipWs(Str& s);
int SkipNonWs(Str& s);
Str NextWord(Str& s);
Str TrimWs(Str s, TrimOpt opt = TrimOpt::Both);

int BufSet(WCHAR* dst, int dstCchSize, Str src);

WStr CastStrToWStr(Str s);
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
int Cmp(WStr a, WStr b);
int CmpI(WStr a, WStr b);
// inline so callers (and the static analyzer) can see the guard
inline bool IsNull(const WStr& s) {
    return !s.s;
}
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

TempStr DecodeTemp(Str url);
TempStr EncodeTemp(Str s);
TempStr EncodeMayTruncateTemp(Str s, int maxEncodedLen, bool* didTruncateOut = nullptr);
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
int SeqStrCount(SeqStrings strs);

// look up the mime type for a file extension (e.g. ".png" -> "image/png");
// returns {} for unknown extensions. If the matched type is an image and
// imgExt (the extension detected from the file's data) is given, it wins.
TempStr MimeTypeFromExtTemp(Str ext, Str imgExt = {});

// SeqStrNum: like SeqStrings but each entry is <string>\0<varint i64>, sequence ends with \0.
// Varint is unsigned LEB128 of zigzag-encoded i64 (small for non-negative values).
// Use when mapping strings to arbitrary numbers (not just sequential indices).
// In use: ShortcutParse.cpp: gVirtKeysNum (generated by cmd/gen-code.ts).
// Index-is-the-number (SeqStrings suffices today): displayModeNames, gArgNames, gToolNames,
//   permNames, gScrollbarModeNames, gFileActionNames, gAnnotationTextIcons, kPdfFilterStateStrs,
//   gLangCodes.
using SeqStrNum = const char*;

TempStr SeqStrNumAt(SeqStrNum strs, int off);
bool SeqStrNumAdvance(SeqStrNum strs, int& off, int* idxInOut = nullptr);
int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut);
int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut);
TempStr SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut);
TempStr SeqStrNumStrByNumber(SeqStrNum strs, i64 num);

namespace str {
struct Builder {
    // len/cap/els come first, in that order, so Builder has the same layout as
    // Vec<T> and can be handed to the VecNonTemplated helpers
    int len = 0;
    // Negative means the chars sit in storage this Builder does not own and the
    // capacity is `-cap`, exactly like Vec<T>: the buffer lent by
    // BuilderUseExternalBuffer(), or a block allocated from an arena. Growing
    // allocates a fresh block and copies; nothing frees it.
    int cap = 0;
    char* els = nullptr;

    Builder() = default;
    // the implicit memberwise copy would alias els and double-free it
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    void Reset(Str s = {});
    char& operator[](int idx) const;
    // these grow on the heap; to grow from an arena use the BuilderAppend*()
    // free functions below, which take the allocator like VecPush() does
    bool AppendChar(char c);
    bool Append(Str src);
    char RemoveAt(int idx, int count = 1);
    char RemoveLast();
    Str TakeStr();
    char LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = char*;

    iterator begin() const { return els ? &(els[0]) : nullptr; }
};

bool Contains(const Builder& b, Str sub);

// Builder does not hold an allocator; like Vec, the arena is passed to the calls
// that can grow. a == nullptr means the heap. Storage that came from an arena is
// never freed by the Builder (the arena owns it).
// Lend b a buffer to start in, instead of its first allocation, the way
// VecUseExternalBuffer() does. b must be empty and have no storage yet. It
// appends into buf until buf is full; the append past that allocates and
// copies, leaving buf alone. Nothing frees buf, so it must outlive b.
void BuilderUseExternalBuffer(Builder& b, Str buf);

// allocate storage for cap chars up front, instead of on the first append
bool BuilderReserve(Arena* a, Builder& b, int cap);

bool BuilderAppendChar(Arena* a, Builder& b, char c);
bool BuilderAppend(Arena* a, Builder& b, Str s);
Str BuilderTakeStr(Arena* a, Builder& b);
} // namespace str

void SeqStrNumAppend(str::Builder* b, Str s, i64 num);
void SeqStrNumFinish(str::Builder* b);

namespace wstr {
struct Builder {
    // len/cap/els come first, in that order, so Builder has the same layout as
    // Vec<T> and can be handed to the VecNonTemplated helpers
    int len = 0;
    // negative cap means storage we don't own, of capacity -cap; see str::Builder
    int cap = 0;
    WCHAR* els = nullptr;

    static constexpr int kElSize = sizeofi(WCHAR);

    Builder() = default;
    // the implicit memberwise copy would alias els and double-free it
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    bool AppendChar(WCHAR);
    bool Append(WStr src);
    WCHAR RemoveLast();
    WCHAR LastChar() const;
    WStr TakeWStr();
};
} // namespace wstr

namespace wstr {

// see str::BuilderUseExternalBuffer()
void BuilderUseExternalBuffer(Builder& b, WStr buf);

// see str::BuilderReserve()
bool BuilderReserve(Builder& b, int cap);

} // namespace wstr

int ParseInt(Str s);
i64 ParseInt64(Str s);
bool IsValidProgramVersion(Str ver);
int CompareProgramVersion(Str ver1, Str ver2);
bool IsTextRtl(WStr s);
bool IsTextRtl(Str s);

char* CStrTemp(Str s);
WCHAR* CWStrTemp(WStr s);

WCHAR* CWStrTemp(WStr s, int& cch);

Str ToStr(const str::Builder&);
WStr ToWStr(const wstr::Builder&);

TempStr ToStrTemp(const str::Builder&);

int len(const str::Builder&);
int len(const wstr::Builder&);

wchar_t ToLowerW(wchar_t c);
int WStrFindSubstr(WStr str, WStr substr);
int WStrCmpNoCase(WStr a, WStr b);

// human readable size, e.g. "1.23 GB", "456 KB", "17 B"
TempStr FormatFileSizeTemp(u64 size);
