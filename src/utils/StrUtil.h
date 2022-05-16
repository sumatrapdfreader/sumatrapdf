/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

#define UTF8_BOM "\xEF\xBB\xBF"
#define UTF16_BOM "\xFF\xFE"
#define UTF16BE_BOM "\xFE\xFF"

bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd);
bool isLegalUTF8String(const u8** source, const u8* sourceEnd);

namespace str {

enum class TrimOpt { Left, Right, Both };

size_t Len(const WCHAR*);
size_t Len(const char* s);

void Free(const char*);
void Free(const u8*);
void Free(ByteSlice);

void Free(const WCHAR* s);

void FreePtr(const char** s);
void FreePtr(char** s);
void FreePtr(const WCHAR** s);
void FreePtr(WCHAR** s);

char* Dup(Allocator*, const char* str, size_t cch = (size_t)-1);
char* Dup(const char* s, size_t cch = (size_t)-1);
char* Dup(ByteSlice d);

WCHAR* Dup(Allocator*, const WCHAR* str, size_t cch = (size_t)-1);
WCHAR* Dup(const WCHAR* s, size_t cch = (size_t)-1);

void ReplacePtr(const char** s, const char* snew);
void ReplacePtr(char** s, const char* snew);
void ReplacePtr(const WCHAR** s, const WCHAR* snew);
void ReplaceWithCopy(const char** s, const char* snew);
void ReplaceWithCopy(char** s, const char* snew);
void ReplaceWithCopy(const WCHAR** s, const WCHAR* snew);
void ReplaceWithCopy(WCHAR** s, const WCHAR* snew);

char* Join(const char* s1, const char* s2, const char* s3 = nullptr);
WCHAR* Join(const WCHAR*, const WCHAR*, const WCHAR* s3 = nullptr);
char* Join(const char* s1, const char* s2, const char* s3, Allocator* allocator);
WCHAR* Join(const WCHAR*, const WCHAR*, const WCHAR* s3, Allocator* allocator);

bool Eq(const char* s1, const char* s2);
bool Eq(ByteSlice sp1, ByteSlice sp2);
bool EqI(const char* s1, const char* s2);
bool EqIS(const char* s1, const char* s2);
bool EqN(const char* s1, const char* s2, size_t len);
bool EqNI(const char* s1, const char* s2, size_t len);
bool IsEmpty(const char* s);
bool StartsWith(const char* str, const char* prefix);
bool StartsWith(const u8* str, const char* prefix);
ByteSlice ToSpan(const char* s);

bool Eq(const WCHAR*, const WCHAR*);
bool EqI(const WCHAR*, const WCHAR*);
bool EqIS(const WCHAR*, const WCHAR*);
bool EqN(const WCHAR*, const WCHAR*, size_t);
bool EqNI(const WCHAR*, const WCHAR*, size_t);
bool IsEmpty(const WCHAR*);
bool StartsWith(const WCHAR* str, const WCHAR* prefix);

bool StartsWithI(const char* str, const char* prefix);
bool EndsWith(const char* txt, const char* end);
bool EndsWithI(const char* txt, const char* end);
bool EqNIx(const char* s, size_t len, const char* s2);

char* ToLowerInPlace(char*);
char* ToLower(const char*);

bool StartsWithI(const WCHAR* str, const WCHAR* prefix);
bool EndsWith(const WCHAR* txt, const WCHAR* end);
bool EndsWithI(const WCHAR* txt, const WCHAR* end);
WCHAR* ToLowerInPlace(WCHAR*);
WCHAR* ToLower(const WCHAR*);
WCHAR* ToUpperInPlace(WCHAR*);

void Utf8Encode(char*& dst, int c);

bool IsDigit(char c);
bool IsWs(char c);
bool IsAlNum(char c);

const char* FindChar(const char* str, char c);
char* FindChar(char* str, char c);
const char* FindCharLast(const char* str, char c);
char* FindCharLast(char* str, char c);
const char* Find(const char* str, const char* find);
const char* FindI(const char* str, const char* find);

bool Contains(const char* s, const char* txt);
bool ContainsI(const char* s, const char* txt);

bool BufFmtV(char* buf, size_t bufCchSize, const char* fmt, va_list args);
bool BufFmt(char* buf, size_t bufCchSize, const char* fmt, ...);
char* FmtV(const char* fmt, va_list args);
char* Format(const char* fmt, ...);

const WCHAR* FindChar(const WCHAR* str, WCHAR c);
WCHAR* FindChar(WCHAR* str, WCHAR c);
const WCHAR* FindCharLast(const WCHAR* str, WCHAR c);
WCHAR* FindCharLast(WCHAR* str, WCHAR c);
const WCHAR* Find(const WCHAR* str, const WCHAR* find);

const WCHAR* FindI(const WCHAR* str, const WCHAR* find);
bool BufFmtV(WCHAR* buf, size_t bufCchSize, const WCHAR* fmt, va_list args);
WCHAR* FmtV(const WCHAR* fmt, va_list args);
WCHAR* Format(const WCHAR* fmt, ...);

bool IsWs(WCHAR c);
bool IsDigit(WCHAR c);
bool IsNonCharacter(WCHAR c);

size_t TrimWSInPlace(char* s, TrimOpt opt);
size_t TrimWSInPlace(WCHAR* s, TrimOpt opt);
void TrimWsEnd(char* s, char*& e);

size_t TransCharsInPlace(char* str, const char* oldChars, const char* newChars);
size_t TransCharsInPlace(WCHAR* str, const WCHAR* oldChars, const WCHAR* newChars);

char* Replace(const char* s, const char* toReplace, const char* replaceWith);
WCHAR* Replace(const WCHAR* s, const WCHAR* toReplace, const WCHAR* replaceWith);

size_t NormalizeWSInPlace(char* str);
size_t NormalizeWSInPlace(WCHAR* str);
size_t NormalizeNewlinesInPlace(char* s, char* e);
size_t NormalizeNewlinesInPlace(char* s);
size_t RemoveCharsInPlace(char* str, const char* toRemove);
size_t RemoveCharsInPlace(WCHAR* str, const WCHAR* toRemove);

size_t BufSet(char* dst, size_t dstCchSize, const char* src);
size_t BufSet(WCHAR* dst, size_t dstCchSize, const WCHAR* src);
size_t BufAppend(char* dst, size_t dstCchSize, const char* s);
size_t BufAppend(WCHAR* dst, size_t dstCchSize, const WCHAR* s);

char* MemToHex(const u8* buf, size_t len);
bool HexToMem(const char* s, u8* buf, size_t bufLen);

const char* Parse(const char* str, const char* fmt, ...);
const char* Parse(const char* str, size_t len, const char* fmt, ...);
const WCHAR* Parse(const WCHAR* str, const WCHAR* format, ...);

int CmpNatural(const char*, const char*);
int CmpNatural(const WCHAR*, const WCHAR*);

WCHAR* FormatFloatWithThousandSep(double number, LCID locale = LOCALE_USER_DEFAULT);
WCHAR* FormatNumWithThousandSep(i64 num, LCID locale = LOCALE_USER_DEFAULT);
WCHAR* FormatRomanNumeral(int number);

bool EmptyOrWhiteSpaceOnly(const char* sv);
} // namespace str

namespace url {

void DecodeInPlace(char* urlA);
void DecodeInPlace(WCHAR* url);

bool IsAbsolute(const WCHAR* url);
WCHAR* GetFullPath(const WCHAR* url);
WCHAR* GetFileName(const WCHAR* url);

} // namespace url

using SeqStrings = const char*;

namespace seqstrings {

void Next(const char*& s);
void Next(const char*& s, int& idx);
int StrToIdx(SeqStrings strs, const char* toFind);
int StrToIdxIS(SeqStrings strs, const char* toFind);
const char* IdxToStr(SeqStrings strs, int idx);

int StrToIdx(SeqStrings strs, const WCHAR* toFind);
const WCHAR* IdxToStr(const WCHAR* strs, int idx);
} // namespace seqstrings

#define _MemToHex(ptr) str::MemToHex((const u8*)(ptr), sizeof(*ptr))
#define _HexToMem(txt, ptr) str::HexToMem(txt, (u8*)(ptr), sizeof(*ptr))

namespace str {
struct Str {
    // allocator is not owned by Vec and must outlive it
    Allocator* allocator = nullptr;
    // TODO: to save space (8 bytes), combine els and buf?
    char* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    char buf[32];

    int nReallocs = 0;

    static constexpr size_t kBufChars = dimof(buf);

    explicit Str(size_t capHint = 0, Allocator* allocator = nullptr);
    Str(const Str& that);
    Str(const char*); // NOLINT

    Str& operator=(const Str& that);

    ~Str();

    void Reset();
    char& at(size_t idx) const;
    char& at(int idx) const;
    char& operator[](size_t idx) const;
    char& operator[](long idx) const;
    char& operator[](ULONG idx) const;
    char& operator[](int idx) const;
#if defined(_WIN64)
    char& at(u32 idx) const;
    char& operator[](u32 idx) const;
#endif
    size_t size() const;
    int isize() const;
    bool InsertAt(size_t idx, char el);
    bool AppendChar(char c);
    bool Append(const char* src, size_t count = -1);
    bool Append(const Str& s);
    char RemoveAt(size_t idx, size_t count = 1);
    char RemoveLast();
    char& Last() const;
    char* StealData();
    char* LendData() const;
    bool Contains(const char* s, size_t sLen = 0);
    bool IsEmpty() const;
    ByteSlice AsByteSlice() const;
    ByteSlice StealAsByteSlice();
    bool Append(const u8* src, size_t size = -1);
    bool AppendSlice(ByteSlice d);
    void AppendFmt(const char* fmt, ...);
    bool AppendAndFree(const char* s);
    void Set(const char*);
    char* Get() const;
    char LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = char*;

    iterator begin() const {
        return &(els[0]);
    }
    iterator end() const {
        return &(els[len]);
    }
};

bool Replace(Str& s, const char* toReplace, const char* replaceWith);

struct WStr {
    // allocator is not owned by Vec and must outlive it
    Allocator* allocator = nullptr;
    WCHAR* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    WCHAR buf[32];

    static constexpr size_t kBufChars = dimof(buf);
    static constexpr size_t kElSize = sizeof(WCHAR);

    explicit WStr(size_t capHint = 0, Allocator* allocator = nullptr);
    WStr(const WStr&);
    WStr(const WCHAR*); // NOLINT
    WStr& operator=(const WStr& that);
    ~WStr();
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
    bool Append(const WCHAR* src, size_t count = -1);
    WCHAR RemoveAt(size_t idx, size_t count = 1);
    WCHAR RemoveLast();
    WCHAR& Last() const;
    WCHAR* StealData();
    WCHAR* LendData() const;
    int Find(const WCHAR& el, size_t startAt = 0) const;
    bool Contains(const WCHAR& el) const;
    int Remove(const WCHAR& el);
    void Reverse() const;
    WCHAR& FindEl(const std::function<bool(WCHAR&)>& check) const;
    bool IsEmpty() const;
    void AppendFmt(const WCHAR* fmt, ...);
    bool AppendAndFree(const WCHAR*);
    void Set(const WCHAR*);
    WCHAR* Get() const;
    WCHAR LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = WCHAR*;

    iterator begin() const {
        return &(els[0]);
    }
    iterator end() const {
        return &(els[len]);
    }
};

bool Replace(WStr& s, const WCHAR* toReplace, const WCHAR* replaceWith);

} // namespace str

// WStrVecOld owns the strings in the list
class WStrVecOld : public Vec<WCHAR*> {
  public:
    WStrVecOld() = default;

    WStrVecOld(const WStrVecOld& other);
    ~WStrVecOld();
    WStrVecOld& operator=(const WStrVecOld& other);
    void Reset();
    int Find(const WCHAR* s, int startAt = 0) const;
    bool Contains(const WCHAR* s) const;
    int FindI(const WCHAR* s, size_t startAt = 0) const;
    void Sort();
    void SortNatural();
};

size_t Split(WStrVecOld& v, const WCHAR* s, const WCHAR* separator, bool collapse = false);
WCHAR* Join(const WStrVecOld& v, const WCHAR* joint = nullptr);

typedef bool (*WStrLessFunc)(const WCHAR* s1, const WCHAR* s2);

struct WStrVec;

struct WStrVecSortedView {
    WStrVec* v; // not owned
    Vec<u32> sortedIndex;
    int Size() const;
    WCHAR* at(int) const;
};

// same design as StrVec
struct WStrVec {
    str::WStr strings;
    Vec<u32> index;

    WStrVec() = default;
    ~WStrVec() = default;
    void Reset();

    int Size() const;
    WCHAR* at(int) const;
    WCHAR* at(size_t idx) const;

    int Append(const WCHAR*, size_t sLen = 0);
    int Find(const WCHAR* s, int startAt = 0) const;
    int FindI(const WCHAR* s, int startAt = 0) const;
    bool Exists(const WCHAR* s) const;
    int AppendIfNotExists(const WCHAR* s);

    void Sort(WStrLessFunc lessFn = nullptr);
    void SortI();
    void SortNatural();

    bool GetSortedView(WStrVecSortedView&, WStrLessFunc lessFn = nullptr) const;
    bool GetSortedViewNoCase(WStrVecSortedView&) const;

    // TODO: remove, only for compat
    size_t size() const;
};

size_t Split(WStrVec& v, const WCHAR* s, const WCHAR* separator, bool collapse);
WCHAR* Join(const WStrVec& v, const WCHAR* joint = nullptr);

// WStrList is a subset of WStrVecOld that's optimized for appending and searching
// WStrList owns the strings it contains and frees them at destruction
class WStrList {
    struct Item {
        WCHAR* string = nullptr;
        u32 hash = 0;
    };

    Vec<Item> items;
    size_t count = 0;
    Allocator* allocator;

  public:
    explicit WStrList(size_t capHint = 0, Allocator* allocator = nullptr);
    ~WStrList();
    const WCHAR* at(size_t idx) const;
    const WCHAR* Last() const;
    size_t size() const;
    void Append(WCHAR* str);
    int Find(const WCHAR* str, size_t startAt = 0) const;
    int FindI(const WCHAR* str, size_t startAt = 0) const;
    bool Contains(const WCHAR* str) const;
};

typedef bool (*StrLessFunc)(const char* s1, const char* s2);

struct StrVec;

struct StrVecSortedView {
    StrVec* v; // not owned
    Vec<u32> sortedIndex;
    int Size() const;
    char* at(int) const;
    char* operator[](int) const;

    StrVecSortedView() = default;
    ~StrVecSortedView() = default;
};

// strings are stored linearly in strings, separated by 0
// index is an array of indexes i.e. strings[index[2]] is
// beginning of string at index 2
struct StrVec {
    str::Str strings;
    Vec<u32> index;

    StrVec() = default;
    ~StrVec() = default;
    void Reset();

    size_t size() const;
    int Size() const;
    char* at(int) const;
    char* operator[](int) const;

    int Append(const char*, size_t len = 0);
    int Find(const char*, int startAt = 0) const;
    bool Exists(const char*) const;
    int AppendIfNotExists(const char*);

    bool GetSortedView(StrVecSortedView&, StrLessFunc lessFn = nullptr) const;
    bool GetSortedViewNoCase(StrVecSortedView&) const;

    void Sort(StrLessFunc lessFn = nullptr);
    void SortI();
    void SortNatural();
};

size_t Split(StrVec& v, const char* s, const char* separator, bool collapse);
char* Join(const StrVec& v, const char* joint = nullptr);
