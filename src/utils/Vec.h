/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

/* Simple but also optimized for small sizes vector/array class that can
store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c).

We always pad the elements with a single 0 value. This makes
Vec<char> and Vec<WCHAR> a C-compatible string. Although it's
not useful for other types, the code is simpler if we always do it
(rather than have it an optional behavior).
*/
template <typename T>
class Vec {
  public:
    Allocator* allocator{nullptr};
    size_t len{0};
    size_t cap{0};
    size_t capacityHint{0};
    T* els{nullptr};
    T buf[16];

    static constexpr size_t kPadding = 1;
    static constexpr size_t kBufSize = sizeof(buf);
    static constexpr size_t kElSize = sizeof(T);

  protected:
    bool EnsureCap(size_t needed) {
        if (cap >= needed) {
            return true;
        }

        size_t newCap = cap * 2;
        if (needed > newCap) {
            newCap = needed;
        }
        if (newCap < capacityHint) {
            newCap = capacityHint;
        }

        size_t newElCount = newCap + kPadding;
        if (newElCount >= SIZE_MAX / kElSize) {
            return false;
        }
        if (newElCount > INT_MAX) {
            // limitation of Vec::Find
            return false;
        }

        size_t allocSize = newElCount * kElSize;
        size_t newPadding = allocSize - len * kElSize;
        T* newEls;
        if (buf == els) {
            newEls = (T*)Allocator::MemDup(allocator, buf, len * kElSize, newPadding);
        } else {
            newEls = (T*)Allocator::Realloc(allocator, els, allocSize);
        }
        if (!newEls) {
            CrashAlwaysIf(gAllowAllocFailure.load() == 0);
            return false;
        }
        els = newEls;
        memset(els + len, 0, newPadding);
        cap = newCap;
        return true;
    }

    T* MakeSpaceAt(size_t idx, size_t count) {
        size_t newLen = std::max(len, idx) + count;
        bool ok = EnsureCap(newLen);
        if (!ok) {
            return nullptr;
        }
        T* res = &(els[idx]);
        if (len > idx) {
            T* src = els + idx;
            T* dst = els + idx + count;
            memmove(dst, src, (len - idx) * kElSize);
        }
        len = newLen;
        return res;
    }

    void FreeEls() {
        if (els != buf) {
            Allocator::Free(allocator, els);
        }
    }

  public:
    // allocator is not owned by Vec and must outlive it
    explicit Vec(size_t capHint = 0, Allocator* allocator = nullptr) : allocator(allocator), capacityHint(capHint) {
        els = buf;
        Reset();
    }

    ~Vec() {
        FreeEls();
    }

    // ensure that a Vec never shares its els buffer with another after a clone/copy
    // note: we don't inherit allocator as it's not needed for our use cases
    Vec(const Vec& orig) {
        els = buf;
        Reset();
        EnsureCap(orig.cap);
        len = orig.len;
        // using memcpy, as Vec only supports POD types
        memcpy(els, orig.els, kElSize * (orig.len));
    }

    // this frees all elements and clears the array.
    // only applicable where T is a pointer. Otherwise will fail to compile
    void FreeMembers() {
        for (size_t i = 0; i < len; i++) {
            auto s = els[i];
            free(s);
        }
        Reset();
    }

    Vec& operator=(const Vec& that) {
        if (this != &that) {
            EnsureCap(that.cap);
            // using memcpy, as Vec only supports POD types
            memcpy(els, that.els, kElSize * (len = that.len));
            memset(els + len, 0, kElSize * (cap - len));
        }
        return *this;
    }

    [[nodiscard]] T& operator[](size_t idx) const {
        CrashIf(idx >= len);
        return els[idx];
    }

    [[nodiscard]] T& operator[](long idx) const {
        CrashIf(idx < 0);
        CrashIf((size_t)idx >= len);
        return els[idx];
    }

    [[nodiscard]] T& operator[](ULONG idx) const {
        CrashIf((size_t)idx >= len);
        return els[idx];
    }

    [[nodiscard]] T& operator[](int idx) const {
        CrashIf(idx < 0);
        CrashIf((size_t)idx >= len);
        return els[idx];
    }

    void Reset() {
        len = 0;
        cap = dimof(buf) - kPadding;
        FreeEls();
        els = buf;
        memset(buf, 0, kBufSize);
    }

    bool SetSize(size_t newSize) {
        Reset();
        return MakeSpaceAt(0, newSize);
    }

    [[nodiscard]] T& at(size_t idx) const {
        CrashIf(idx >= len);
        return els[idx];
    }

    [[nodiscard]] T& at(int idx) const {
        CrashIf(idx < 0);
        CrashIf((size_t)idx >= len);
        return els[idx];
    }

    [[nodiscard]] size_t size() const {
        return len;
    }
    [[nodiscard]] int isize() const {
        return (int)len;
    }

    bool InsertAt(size_t idx, const T& el) {
        T* p = MakeSpaceAt(idx, 1);
        if (!p) {
            return false;
        }
        p[0] = el;
        return true;
    }

    bool Append(const T& el) {
        return InsertAt(len, el);
    }

    bool Append(const T* src, size_t count) {
        if (0 == count) {
            return true;
        }
        T* dst = MakeSpaceAt(len, count);
        if (!dst) {
            return false;
        }
        memcpy(dst, src, count * kElSize);
        return true;
    }

    // appends count blank (i.e. zeroed-out) elements at the end
    T* AppendBlanks(size_t count) {
        return MakeSpaceAt(len, count);
    }

    void RemoveAt(size_t idx, size_t count = 1) {
        if (len > idx + count) {
            T* dst = els + idx;
            T* src = els + idx + count;
            memmove(dst, src, (len - idx - count) * kElSize);
        }
        len -= count;
        memset(els + len, 0, count * kElSize);
    }

    void RemoveLast() {
        if (len == 0) {
            return;
        }
        RemoveAt(len - 1);
    }

    // This is a fast version of RemoveAt() which replaces the element we're
    // removing with the last element, copying less memory.
    // It can only be used if order of elements doesn't matter and elements
    // can be copied via memcpy()
    // TODO: could be extend to take number of elements to remove
    void RemoveAtFast(size_t idx) {
        CrashIf(idx >= len);
        if (idx >= len) {
            return;
        }
        T* toRemove = els + idx;
        T* last = els + len - 1;
        if (toRemove != last) {
            memcpy(toRemove, last, kElSize);
        }
        memset(last, 0, kElSize);
        --len;
    }

    T Pop() {
        CrashIf(0 == len);
        T el = at(len - 1);
        RemoveAtFast(len - 1);
        return el;
    }

    T PopAt(size_t idx) {
        CrashIf(idx >= len);
        T el = at(idx);
        RemoveAt(idx);
        return el;
    }

    [[nodiscard]] T& Last() const {
        CrashIf(0 == len);
        return at(len - 1);
    }

    // perf hack for using as a buffer: client can get accumulated data
    // without duplicate allocation. Note: since Vec over-allocates, this
    // is likely to use more memory than strictly necessary, but in most cases
    // it doesn't matter
    [[nodiscard]] T* StealData() {
        T* res = els;
        if (els == buf) {
            res = (T*)Allocator::MemDup(allocator, buf, (len + kPadding) * kElSize);
        }
        els = buf;
        Reset();
        return res;
    }

    [[nodiscard]] T* LendData() const {
        return els;
    }

    [[nodiscard]] int Find(const T& el, size_t startAt = 0) const {
        for (size_t i = startAt; i < len; i++) {
            if (els[i] == el) {
                return (int)i;
            }
        }
        return -1;
    }

    [[nodiscard]] bool Contains(const T& el) const {
        return -1 != Find(el);
    }

    // returns position of removed element or -1 if not removed
    int Remove(const T& el) {
        int i = Find(el);
        if (-1 == i) {
            return -1;
        }
        RemoveAt(i);
        return i;
    }

    void Sort(int (*cmpFunc)(const void* a, const void* b)) {
        qsort(els, len, kElSize, cmpFunc);
    }

    void SortTyped(int (*cmpFunc)(const T* a, const T* b)) {
        auto cmpFunc2 = (int (*)(const void* a, const void* b))cmpFunc;
        qsort(els, len, kElSize, cmpFunc2);
    }

    void Reverse() {
        for (size_t i = 0; i < len / 2; i++) {
            std::swap(els[i], els[len - i - 1]);
        }
    }

    T& FindEl(const std::function<bool(T&)>& check) {
        for (size_t i = 0; i < len; i++) {
            if (check(els[i])) {
                return els[i];
            }
        }
        return els[len]; // nullptr-sentinel
    }

    [[nodiscard]] bool IsEmpty() const {
        return len == 0;
    }

    // TOOD: replace with IsEmpty()
    [[nodiscard]] bool empty() const {
        return len == 0;
    }

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    typedef T* iterator;
    typedef const T* const_iterator;

    iterator begin() {
        return &(els[0]);
    }
    const_iterator begin() const {
        return &(els[0]);
    }
    iterator end() {
        return &(els[len]);
    }
    const_iterator end() const {
        return &(els[len]);
    }
};

// only suitable for T that are pointers to C++ objects
template <typename T>
inline void DeleteVecMembers(Vec<T>& v) {
    for (T& el : v) {
        delete el;
    }
    v.Reset();
}

#if OS_WIN
// WStrVec owns the strings in the list
class WStrVec : public Vec<WCHAR*> {
  public:
    WStrVec() : Vec() {
    }
    WStrVec(const WStrVec& orig) : Vec(orig) {
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

    WStrVec& operator=(const WStrVec& that) {
        if (this != &that) {
            FreeMembers();
            Vec::operator=(that);
            for (size_t i = 0; i < that.len; i++) {
                if (at(i)) {
                    at(i) = str::Dup(at(i));
                }
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
                Append(str::DupN(s, next - s));
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
#endif

#if OS_WIN
// WStrList is a subset of WStrVec that's optimized for appending and searching
// WStrList owns the strings it contains and frees them at destruction
class WStrList {
    struct Item {
        WCHAR* string;
        u32 hash;

        explicit Item(WCHAR* string = nullptr, u32 hash = 0) : string(string), hash(hash) {
        }
    };

    Vec<Item> items;
    size_t count = 0;
    Allocator* allocator;

    // variation of MurmurHash2 which deals with strings that are
    // mostly ASCII and should be treated case independently
    // TODO: I'm guessing would be much faster when done as MurmuserHash2I()
    // with lower-casing done in-line, without the need to allocate memory for the copy
    static u32 GetQuickHashI(const WCHAR* str) {
        size_t len = str::Len(str);
        AutoFree data(AllocArray<char>(len));
        WCHAR c;
        for (char* dst = data; (c = *str++) != 0; dst++) {
            *dst = (c & 0xFF80) ? 0x80 : 'A' <= c && c <= 'Z' ? (char)(c + 'a' - 'A') : (char)c;
        }
        return MurmurHash2(data, len);
    }

  public:
    explicit WStrList(size_t capHint = 0, Allocator* allocator = nullptr) : items(capHint, allocator) {
        this->allocator = allocator;
    }

    ~WStrList() {
        for (Item& item : items) {
            Allocator::Free(allocator, item.string);
        }
    }

    const WCHAR* at(size_t idx) const {
        return items.at(idx).string;
    }

    const WCHAR* Last() const {
        return items.Last().string;
    }

    size_t size() const {
        return count;
    }

    // str must have been allocated by allocator and is owned by StrList
    void Append(WCHAR* str) {
        items.Append(Item(str, GetQuickHashI(str)));
        count++;
    }

    int Find(const WCHAR* str, size_t startAt = 0) const {
        u32 hash = GetQuickHashI(str);
        Item* item = items.LendData();
        for (size_t i = startAt; i < count; i++) {
            if (item[i].hash == hash && str::Eq(item[i].string, str)) {
                return (int)i;
            }
        }
        return -1;
    }

    int FindI(const WCHAR* str, size_t startAt = 0) const {
        u32 hash = GetQuickHashI(str);
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
#endif

// TODO: could increase kVecStrIndexSize when expanding array
constexpr int kVecStrIndexSize = 128;
struct VecStrIndex {
    VecStrIndex* next;
    int nStrings;
    char* offsets[kVecStrIndexSize];
    i32 sizes[kVecStrIndexSize];
    int ItemsLeft();
};

// Append-only, optimized vector of strings. Allocates from pool allocator, so
// strings are close together and freed in bulk
// implemented in BaseUtil.cpp
struct VecStr {
    PoolAllocator allocator;
    VecStrIndex* firstIndex = nullptr;
    VecStrIndex* currIndex = nullptr;

    VecStr() = default;
    ~VecStr() = default;
    int Size();
    void Reset();
    bool Append(std::string_view sv);

    std::string_view at(int);
};
