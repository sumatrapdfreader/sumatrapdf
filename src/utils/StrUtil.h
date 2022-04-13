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
void Free(std::string_view);
void Free(ByteSlice);

void Free(const WCHAR* s);
void Free(std::wstring_view);

void FreePtr(const char** s);
void FreePtr(char** s);
void FreePtr(const WCHAR** s);
void FreePtr(WCHAR** s);

char* Dup(Allocator*, const char* str, size_t cch = (size_t)-1);
char* Dup(const char* s, size_t cch = (size_t)-1);
char* Dup(Allocator*, std::string_view);
char* Dup(std::string_view);
char* Dup(ByteSlice d);

WCHAR* Dup(Allocator*, const WCHAR* str, size_t cch = (size_t)-1);
WCHAR* Dup(const WCHAR* s, size_t cch = (size_t)-1);
WCHAR* Dup(std::wstring_view);

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
bool Eq(std::string_view s1, const char* s2);
bool Eq(ByteSlice sp1, ByteSlice sp2);
bool EqI(const char* s1, const char* s2);
bool EqI(std::string_view s1, const char* s2);
bool EqIS(const char* s1, const char* s2);
bool EqN(const char* s1, const char* s2, size_t len);
bool EqNI(const char* s1, const char* s2, size_t len);
bool IsEmpty(const char* s);
bool StartsWith(const char* str, const char* prefix);
bool StartsWith(const u8* str, const char* prefix);
bool StartsWith(std::string_view s, const char* prefix);
ByteSlice ToSpan(const char* s);

bool Eq(const WCHAR*, const WCHAR*);
bool Eq(std::wstring_view s1, const WCHAR* s2);
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
WCHAR* ToLowerInPlace(WCHAR* s);
WCHAR* ToLower(const WCHAR* s);

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

bool Contains(std::string_view s, const char* txt);
bool ContainsI(std::string_view s, const char* txt);

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

bool EmptyOrWhiteSpaceOnly(std::string_view sv);
} // namespace str

namespace url {

void DecodeInPlace(char* urlA);

bool IsAbsolute(const WCHAR* url);
void DecodeInPlace(WCHAR* url);
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
    Str(std::string_view s); // NOLINT
    Str(const char*);        // NOLINT

    Str& operator=(const Str& that);

    ~Str();

    void Reset();
    [[nodiscard]] char& at(size_t idx) const;
    [[nodiscard]] char& at(int idx) const;
    [[nodiscard]] char& operator[](size_t idx) const;
    [[nodiscard]] char& operator[](long idx) const;
    [[nodiscard]] char& operator[](ULONG idx) const;
    [[nodiscard]] char& operator[](int idx) const;
#if defined(_WIN64)
    [[nodiscard]] char& at(u32 idx) const;
    [[nodiscard]] char& operator[](u32 idx) const;
#endif
    [[nodiscard]] size_t size() const;
    [[nodiscard]] int isize() const;
    bool InsertAt(size_t idx, char el);
    bool Append(char el);
    bool Append(const char* src, size_t count = -1);
    char RemoveAt(size_t idx, size_t count = 1);
    char RemoveLast();
    [[nodiscard]] char& Last() const;
    [[nodiscard]] char* StealData();
    [[nodiscard]] char* LendData() const;
    [[nodiscard]] int Find(char el, size_t startAt = 0) const;
    [[nodiscard]] bool Contains(char el) const;
    int Remove(char el);
    void Reverse() const;
    char& FindEl(const std::function<bool(char&)>& check) const;
    [[nodiscard]] bool IsEmpty() const;
    [[nodiscard]] std::string_view AsView() const;
    [[nodiscard]] ByteSlice AsSpan() const;
    [[nodiscard]] ByteSlice AsByteSlice() const;
    std::string_view StealAsView();
    bool AppendChar(char c);
    bool Append(const u8* src, size_t size = -1);
    bool AppendView(std::string_view sv);
    bool AppendSpan(ByteSlice d);
    void AppendFmt(const char* fmt, ...);
    bool AppendAndFree(const char* s);
    void Set(std::string_view sv);
    [[nodiscard]] char* Get() const;
    [[nodiscard]] char LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = char*;

    [[nodiscard]] iterator begin() const {
        return &(els[0]);
    }
    [[nodiscard]] iterator end() const {
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
    explicit WStr(std::wstring_view);
    WStr(const WCHAR*); // NOLINT
    WStr& operator=(const WStr& that);
    ~WStr();
    void Reset();
    [[nodiscard]] WCHAR& at(size_t idx) const;
    [[nodiscard]] WCHAR& at(int idx) const;
    [[nodiscard]] WCHAR& operator[](size_t idx) const;
    [[nodiscard]] WCHAR& operator[](long idx) const;
    [[nodiscard]] WCHAR& operator[](ULONG idx) const;
    [[nodiscard]] WCHAR& operator[](int idx) const;
#if defined(_WIN64)
    [[nodiscard]] WCHAR& at(u32 idx) const;
    [[nodiscard]] WCHAR& operator[](u32 idx) const;
#endif
    [[nodiscard]] size_t size() const;
    [[nodiscard]] int isize() const;
    bool InsertAt(size_t idx, const WCHAR& el);
    bool Append(const WCHAR& el);
    bool Append(const WCHAR* src, size_t count = -1);
    WCHAR RemoveAt(size_t idx, size_t count = 1);
    WCHAR RemoveLast();
    [[nodiscard]] WCHAR& Last() const;
    [[nodiscard]] WCHAR* StealData();
    [[nodiscard]] WCHAR* LendData() const;
    [[nodiscard]] int Find(const WCHAR& el, size_t startAt = 0) const;
    [[nodiscard]] bool Contains(const WCHAR& el) const;
    int Remove(const WCHAR& el);
    void Reverse() const;
    WCHAR& FindEl(const std::function<bool(WCHAR&)>& check) const;
    [[nodiscard]] bool IsEmpty() const;
    [[nodiscard]] std::wstring_view AsView() const;
    [[nodiscard]] std::span<WCHAR> AsSpan() const;
    std::wstring_view StealAsView();
    bool AppendChar(WCHAR c);
    bool AppendSpan(std::span<WCHAR> d);
    bool AppendView(std::wstring_view sv);
    void AppendFmt(const WCHAR* fmt, ...);
    bool AppendAndFree(const WCHAR* s);
    void Set(std::wstring_view sv);
    [[nodiscard]] WCHAR* Get() const;
    [[nodiscard]] WCHAR LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = WCHAR*;

    [[nodiscard]] iterator begin() const {
        return &(els[0]);
    }
    [[nodiscard]] iterator end() const {
        return &(els[len]);
    }
};

bool Replace(WStr& s, const WCHAR* toReplace, const WCHAR* replaceWith);

} // namespace str

ByteSlice ToSpanU8(std::string_view sv);

// TOOD: smarter WStrVec class which uses str::Wstr for the buffer
// and stores str::wstring_view in an array

// WStrVec owns the strings in the list
class WStrVec : public Vec<WCHAR*> {
  public:
    WStrVec() = default;
    WStrVec(const WStrVec& other) : Vec(other) {
        // make sure not to share string pointers between StrVecs
        for (size_t i = 0; i < len; i++) {
            if (at(i)) {
                at(i) = str::Dup(at(i));
            }
        }
    }
    ~WStrVec() {
        FreeMembers();
    }

    WStrVec& operator=(const WStrVec& other) {
        if (this == &other) {
            return *this;
        }

        FreeMembers();
        Vec::operator=(other);
        for (size_t i = 0; i < other.len; i++) {
            if (at(i)) {
                at(i) = str::Dup(at(i));
            }
        }
        return *this;
    }

    void Reset() {
        FreeMembers();
    }

    WCHAR* Join(const WCHAR* joint = nullptr) {
        str::WStr tmp(256);
        size_t jointLen = str::Len(joint);
        for (size_t i = 0; i < len; i++) {
            WCHAR* s = at(i);
            if (i > 0 && jointLen > 0) {
                tmp.Append(joint, jointLen);
            }
            tmp.Append(s);
        }
        return tmp.StealData();
    }

    int Find(const WCHAR* s, int startAt = 0) const {
        for (int i = startAt; i < (int)len; i++) {
            WCHAR* item = at(i);
            if (str::Eq(s, item)) {
                return i;
            }
        }
        return -1;
    }

    bool Contains(const WCHAR* s) const {
        return -1 != Find(s);
    }

    int FindI(const WCHAR* s, size_t startAt = 0) const {
        for (size_t i = startAt; i < len; i++) {
            WCHAR* item = at(i);
            if (str::EqI(s, item)) {
                return (int)i;
            }
        }
        return -1;
    }

    /* splits a string into several substrings, separated by the separator
       (optionally collapsing several consecutive separators into one);
       e.g. splitting "a,b,,c," by "," results in the list "a", "b", "", "c", ""
       (resp. "a", "b", "c" if separators are collapsed) */
    size_t Split(const WCHAR* s, const WCHAR* separator, bool collapse = false) {
        size_t start = len;
        const WCHAR* next;

        while ((next = str::Find(s, separator)) != nullptr) {
            if (!collapse || next > s) {
                Append(str::Dup(s, next - s));
            }
            s = next + str::Len(separator);
        }
        if (!collapse || *s) {
            Append(str::Dup(s));
        }

        return len - start;
    }

    void Sort() {
        Vec::Sort(cmpAscii);
    }
    void SortNatural() {
        Vec::Sort(cmpNatural);
    }

  private:
    static int cmpNatural(const void* a, const void* b) {
        return str::CmpNatural(*(const WCHAR**)a, *(const WCHAR**)b);
    }

    static int cmpAscii(const void* a, const void* b) {
        return wcscmp(*(const WCHAR**)a, *(const WCHAR**)b);
    }
};

// WStrList is a subset of WStrVec that's optimized for appending and searching
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
    explicit WStrList(size_t capHint = 0, Allocator* allocator = nullptr) : items(capHint, allocator) {
        this->allocator = allocator;
    }

    ~WStrList() {
        for (Item& item : items) {
            Allocator::Free(allocator, item.string);
        }
    }

    [[nodiscard]] const WCHAR* at(size_t idx) const {
        return items.at(idx).string;
    }

    [[nodiscard]] const WCHAR* Last() const {
        return items.Last().string;
    }

    [[nodiscard]] size_t size() const {
        return count;
    }

    // str must have been allocated by allocator and is owned by StrList
    void Append(WCHAR* str) {
        u32 hash = MurmurHashWStrI(str);
        items.Append(Item{str, hash});
        count++;
    }

    int Find(const WCHAR* str, size_t startAt = 0) const {
        u32 hash = MurmurHashWStrI(str);
        Item* item = items.LendData();
        for (size_t i = startAt; i < count; i++) {
            if (item[i].hash == hash && str::Eq(item[i].string, str)) {
                return (int)i;
            }
        }
        return -1;
    }

    int FindI(const WCHAR* str, size_t startAt = 0) const {
        u32 hash = MurmurHashWStrI(str);
        Item* item = items.LendData();
        for (size_t i = startAt; i < count; i++) {
            if (item[i].hash == hash && str::EqI(item[i].string, str)) {
                return (int)i;
            }
        }
        return -1;
    }

    bool Contains(const WCHAR* str) const {
        return -1 != Find(str);
    }
};

typedef bool (*StrLessFunc)(std::string_view s1, std::string_view s2);

struct StrVec;

struct StrVecSortedView {
    StrVec* v; // not owned
    Vec<u32> sortedIndex;
    int Size() const;
    std::string_view at(int) const;

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

    int Size() const;
    std::string_view at(int) const;

    int Append(const char*);
    int Find(std::string_view sv, int startAt = 0) const;
    bool Exists(std::string_view) const;
    int AppendIfNotExists(std::string_view);

    bool GetSortedView(StrVecSortedView&, StrLessFunc lessFn = nullptr) const;
    bool GetSortedViewNoCase(StrVecSortedView&) const;
};

typedef bool (*WStrLessFunc)(std::wstring_view s1, std::wstring_view s2);

struct WStrVec2;

struct WStrVecSortedView {
    WStrVec2* v; // not owned
    Vec<u32> sortedIndex;
    int Size() const;
    std::wstring_view at(int) const;
};

// same design as StrVec
struct WStrVec2 {
    str::WStr strings;
    Vec<u32> index;

    WStrVec2() = default;
    ~WStrVec2() = default;
    void Reset();

    int Size() const;
    std::wstring_view at(int) const;
    int Append(const WCHAR*);
    int Find(const WCHAR* s, int startAt = 0) const;
    bool Exists(std::wstring_view) const;
    int AppendIfNotExists(std::wstring_view);

    bool GetSortedView(WStrVecSortedView&, WStrLessFunc lessFn = nullptr) const;
    bool GetSortedViewNoCase(WStrVecSortedView&) const;

    // TODO: remove, only for compat
    size_t size() const;
    // TODO: rename to Index()
};
