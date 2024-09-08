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

template <typename T>
struct Span {
    T* d = nullptr;
    int size = 0;

    Span() = default;
    ~Span() = default;
    Span(const T* d, int size) : d((T*)d), size(size) {
    }
    T* Data() const {
        return d;
    }
    int Size() const {
        return size;
    }
};

struct StrSpan : Span<char> {
    StrSpan() = default;
    StrSpan(const char* s);
    StrSpan(const char* s, int sLen);
    char* CStr() const {
        return d;
    }
    int Len() const {
        return size;
    }
    bool IsEmpty() const {
        return !d || size == 0;
    }
};

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
    u8* data() const {
        return d;
    }
    u8* Get() const {
        return d;
    }
    size_t size() const {
        return sz;
    }
    int Size() const {
        return (int)sz;
    }
    bool empty() const {
        return !d;
    }
    bool IsEmpty() const {
        return !d;
    }
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
    operator const char*() {
        return (const char*)d;
    }
};

bool IsEqual(const ByteSlice&, const ByteSlice&);

namespace str {

enum class TrimOpt { Left, Right, Both };

size_t Len(const WCHAR*);
size_t Len(const char* s);

int Leni(const WCHAR*);
int Leni(const char* s);

void Free(const char*);
void Free(const u8*);
void Free(const StrSpan& s);

void Free(const WCHAR* s);

void FreePtr(const char** s);
void FreePtr(char** s);
void FreePtr(const WCHAR** s);
void FreePtr(WCHAR** s);

char* Dup(Allocator*, const char* str, size_t cch = (size_t)-1);
char* Dup(const char* s, size_t cch = (size_t)-1);
char* Dup(const ByteSlice&);

void ReplacePtr(const char** s, const char* snew);
void ReplacePtr(char** s, const char* snew);

void ReplaceWithCopy(const char** s, const char* snew);
void ReplaceWithCopy(const char** s, const ByteSlice&);
void ReplaceWithCopy(char** s, const char* snew);

char* Join(Allocator*, const char*, const char*, const char*);
char* Join(Allocator*, const char*, const char*, const char*, const char*, const char*);
char* Join(const char* s1, const char* s2, const char* s3 = nullptr);

bool Eq(const char* s1, const char* s2);
bool Eq(const ByteSlice& sp1, const ByteSlice& sp2);
bool EqI(const char* s1, const char* s2);
bool EqIS(const char* s1, const char* s2);
bool EqN(const char* s1, const char* s2, size_t len);
bool EqNI(const char* s1, const char* s2, size_t len);
bool IsEmpty(const char* s);
bool StartsWith(const char* str, const char* prefix);
bool StartsWith(const u8* str, const char* prefix);

bool StartsWithI(const char* str, const char* prefix);
bool EndsWith(const char* txt, const char* end);
bool EndsWithI(const char* txt, const char* end);
bool EqNIx(const char* s, size_t len, const char* s2);

char* ToLowerInPlace(char*);

char* ToLower(const char*);

char* ToUpperInPlace(char*);

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
int BufFind(const char* buf, int bufSize, const char* toFind);

bool Contains(const char* s, const char* txt);
bool ContainsI(const char* s, const char* txt);

bool BufFmtV(char* buf, size_t bufCchSize, const char* fmt, va_list args);
bool BufFmt(char* buf, size_t bufCchSize, const char* fmt, ...);
char* FmtVWithAllocator(Allocator* a, const char* fmt, va_list args);
char* FmtV(const char* fmt, va_list args);
char* Format(const char* fmt, ...);

size_t TrimWSInPlace(char* s, TrimOpt opt);
void TrimWsEnd(char* s, char*& e);

size_t TransCharsInPlace(char* str, const char* oldChars, const char* newChars);

size_t NormalizeWSInPlace(char* str);
size_t NormalizeNewlinesInPlace(char* s, char* e);
size_t NormalizeNewlinesInPlace(char* s);
size_t RemoveCharsInPlace(char* str, const char* toRemove);

int BufSet(char* dst, int dstCchSize, const char* src);
int BufAppend(char* dst, int dstCchSize, const char* s);

char* MemToHex(const u8* buf, size_t len);
bool HexToMem(const char* s, u8* buf, size_t bufLen);

const char* Parse(const char* str, const char* fmt, ...);
const char* Parse(const char* str, size_t len, const char* fmt, ...);

int CmpNatural(const char*, const char*);

TempStr FormatFloatWithThousandSepTemp(double number, LCID locale = LOCALE_USER_DEFAULT);
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale = LOCALE_USER_DEFAULT);
TempStr FormatSizeShortTemp(i64 size, const char* sizeUnits[3]);
TempStr FormatFileSizeTemp(i64);
TempStr FormatRomanNumeralTemp(int number);

bool IsEmptyOrWhiteSpace(const char*);
bool Skip(const char*& s, const char* toSkip);
const char* SkipChar(const char* s, char toSkip);

WCHAR* Dup(Allocator*, const WCHAR* str, size_t cch = (size_t)-1);
WCHAR* Dup(const WCHAR* s, size_t cch = (size_t)-1);
WCHAR* Join(const WCHAR*, const WCHAR*, const WCHAR* s3 = nullptr);
WCHAR* Join(Allocator*, const WCHAR*, const WCHAR*, const WCHAR* s3);
bool Eq(const WCHAR*, const WCHAR*);
bool EqI(const WCHAR*, const WCHAR*);
bool EqN(const WCHAR*, const WCHAR*, size_t);
bool IsEmpty(const WCHAR*);
bool StartsWith(const WCHAR* str, const WCHAR* prefix);
bool StartsWithI(const WCHAR* str, const WCHAR* prefix);
bool EndsWith(const WCHAR* txt, const WCHAR* end);
bool EndsWithI(const WCHAR* txt, const WCHAR* end);
WCHAR* ToLower(const WCHAR*);
WCHAR* ToLowerInPlace(WCHAR*);
const WCHAR* Parse(const WCHAR* str, const WCHAR* format, ...);
int BufSet(WCHAR* dst, int dstCchSize, const WCHAR* src);
int BufSet(WCHAR* dst, int dstCchSize, const char* src);
size_t NormalizeWSInPlace(WCHAR* str);
size_t RemoveCharsInPlace(WCHAR* str, const WCHAR* toRemove);
const WCHAR* FindChar(const WCHAR* str, WCHAR c);
WCHAR* FindChar(WCHAR* str, WCHAR c);
const WCHAR* Find(const WCHAR* str, const WCHAR* find);
bool IsWs(WCHAR c);
bool IsDigit(WCHAR c);
bool IsNonCharacter(WCHAR c);
size_t TransCharsInPlace(WCHAR* str, const WCHAR* oldChars, const WCHAR* newChars);
WCHAR* Replace(const WCHAR* s, const WCHAR* toReplace, const WCHAR* replaceWith);

WCHAR* ToWCHAR(const char* s);
} // namespace str

namespace url {

void DecodeInPlace(char* url);
bool IsAbsolute(const char* url);
TempStr GetFullPathTemp(const char* url);
TempStr GetFileNameTemp(const char* url);

} // namespace url

using SeqStrings = const char*;

namespace seqstrings {

void Next(const char*& s);
void Next(const char*& s, int* idxInOut);
int StrToIdx(SeqStrings strs, const char* toFind);
int StrToIdxIS(SeqStrings strs, const char* toFind);
const char* IdxToStr(SeqStrings strs, int idx);
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
    Str& operator=(const Str& that);
    Str(const char*); // NOLINT

    ~Str();

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
    bool Append(const StrSpan&);
    bool Append(const Str& s);
    char RemoveAt(size_t idx, size_t count = 1);
    char RemoveLast();
    char& Last() const;
    char* StealData(Allocator* a = nullptr);
    char* LendData() const;
    bool Contains(const char* s, size_t sLen = 0);
    bool IsEmpty() const;
    ByteSlice AsByteSlice() const;
    ByteSlice StealAsByteSlice();
    bool Append(const u8* src, size_t size = -1);
    bool AppendSlice(const ByteSlice& d);
    void AppendFmt(const char* fmt, ...);
    void Set(const char*);
    char* Get() const;
    char* CStr() const;
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

// bool Replace(Str& s, const char* toReplace, const char* replaceWith);

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
    bool IsEmpty() const;
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

int ParseInt(const char* bytes);
bool IsValidProgramVersion(const char* ver);
int CompareProgramVersion(const char* ver1, const char* ver2);
