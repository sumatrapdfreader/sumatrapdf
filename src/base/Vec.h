/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

/* Simple but also optimized for small sizes vector/array class that can
store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c).

*/
template <typename T>
class Vec {
  public:
    Arena* allocator = nullptr;
    int len = 0;
    int cap = 0;
    int capacityHint = 0;
    T* els = nullptr;
    T buf[16];

    // We always pad the elements with a single 0 value. This makes
    // Vec<char> and Vec<WCHAR> a C-compatible string. Although it's
    // not useful for other types, the code is simpler if we always do it
    // (rather than have it an optional behavior).
    static constexpr int kPadding = 1;
    // byte size of a single element; kept size_t because it's used in
    // allocation-size arithmetic that must not overflow
    static constexpr size_t kElSize = sizeof(T);

  private:
    NO_INLINE bool EnsureCapSlow(int needed) {
        int newCap = cap * 2;
        if (needed > newCap) {
            newCap = needed;
        }
        if (newCap < capacityHint) {
            newCap = capacityHint;
        }

        size_t newElCount = (size_t)newCap + kPadding;
        if (newElCount >= SIZE_MAX / kElSize) {
            return false;
        }
        if (newElCount > INT_MAX) {
            // limitation of Vec::Find
            return false;
        }

        size_t allocSize = newElCount * kElSize;
        size_t newPadding = allocSize - (size_t)len * kElSize;
        T* newEls;
        if (buf == els) {
            newEls = (T*)MemDup(allocator, buf, (size_t)len * kElSize, newPadding);
        } else {
            newEls = (T*)Realloc(allocator, els, allocSize, (size_t)len * kElSize);
        }
        if (!newEls) {
            ReportIf(InterlockedExchangeAdd(&gAllowAllocFailure, 0) == 0);
            return false;
        }
        els = newEls;
        memset(els + len, 0, newPadding);
        cap = newCap;
        return true;
    }

  public:
    inline T* EnsureCap(int capNeeded) {
        // this is frequent, fast path that should be inlined
        if (cap >= capNeeded) {
            return els;
        }
        // slow path
        if (!EnsureCapSlow(capNeeded)) {
            return nullptr;
        }
        return els;
    }

    T* MakeSpaceAt(int idx, int count) {
        int newLen = std::max(len, idx) + count;
        T* ok = EnsureCap(newLen);
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
            Free(allocator, els);
            els = nullptr;
        }
    }

  public:
    // resets to initial state, freeing memory
    void Reset() {
        FreeEls();
        len = 0;
        cap = dimofi(buf) - kPadding;
        els = buf;
        memset(buf, 0, sizeof(buf));
    }

    // use to empty but don't free els
    // for efficient reuse
    void Clear() {
        len = 0;
        memset(els, 0, cap * kElSize);
    }

    bool SetSize(int newSize) {
        if (newSize <= cap) {
            len = newSize;
            memset(els + len, 0, (cap - len) * kElSize);
            return true;
        }
        auto res = MakeSpaceAt(0, newSize);
        return res != nullptr;
    }

    // allocator is not owned by Vec and must outlive it
    explicit Vec(int capHint = 0, Arena* a = nullptr) {
        allocator = a;
        capacityHint = capHint;
        els = buf;
        Reset();
    }

    // ensure that a Vec never shares its els buffer with another after a clone/copy
    // note: we don't inherit allocator as it's not needed for our use cases
    Vec(const Vec& other) {
        els = buf;
        Reset();

        EnsureCap(other.len);
        len = other.len;
        // using memcpy, as Vec only supports POD types
        memcpy(els, other.els, kElSize * (other.len));
    }

    // TODO: write Vec(const Vec&& other)

    Vec& operator=(const Vec& other) {
        if (this == &other) {
            return *this;
        }

        els = buf;
        Reset();
        EnsureCap(other.len);
        // using memcpy, as Vec only supports POD types
        len = other.len;
        capacityHint = other.capacityHint;
        memcpy(els, other.els, kElSize * len);
        memset(els + len, 0, kElSize * (cap - len));
        return *this;
    }

    ~Vec() { FreeEls(); }

    // this frees all elements and clears the array.
    // only applicable where T is a pointer. Otherwise will fail to compile
    void FreeMembers() {
        for (int i = 0; i < len; i++) {
            auto s = els[i];
            free(s);
        }
        Reset();
    }

    T& operator[](int idx) const {
        ReportIf(idx < 0);
        ReportIf(idx >= len);
        return els[idx];
    }

    bool isValidIndex(int idx) const { return (idx >= 0) && (idx < len); }

    bool InsertAt(int idx, const T& el) {
        T* p = MakeSpaceAt(idx, 1);
        if (!p) {
            return false;
        }
        p[0] = el;
        return true;
    }

    bool Append(const T& el) { return InsertAt(len, el); }

    bool Append(const T* src, int count) {
        if (0 == count) {
            return true;
        }
        T* dst = MakeSpaceAt(len, count);
        if (!dst) {
            return false;
        }
        memcpy(dst, src, (size_t)count * kElSize);
        return true;
    }

    bool Append(const Vec& other) {
        int n = other.len;
        const T* data = other.LendData();
        return this->Append(data, n);
    }

    // appends count blank (i.e. zeroed-out) elements at the end
    T* AppendBlanks(int count) { return MakeSpaceAt(len, count); }

    void RemoveAt(int idx, int count = 1) {
        if (len > idx + count) {
            T* dst = els + idx;
            T* src = els + idx + count;
            memmove(dst, src, (size_t)(len - idx - count) * kElSize);
        }
        len -= count;
        memset(els + len, 0, (size_t)count * kElSize);
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
    void RemoveAtFast(int idx) {
        ReportIf(idx >= len);
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
        ReportIf(0 == len);
        T el = els[len - 1];
        RemoveAtFast(len - 1);
        return el;
    }

    T PopAt(int idx) {
        ReportIf(idx >= len);
        T el = els[idx];
        RemoveAt(idx);
        return el;
    }

    T& Last() const {
        ReportIf(0 == len);
        return els[len - 1];
    }

    // perf hack for using as a buffer: client can get accumulated data
    // without duplicate allocation. Note: since Vec over-allocates, this
    // is likely to use more memory than strictly necessary, but in most cases
    // it doesn't matter
    T* Take() {
        T* res = els;
        if (els == buf) {
            res = (T*)MemDup(allocator, buf, (len + kPadding) * kElSize);
        }
        els = buf;
        Reset();
        return res;
    }

    T* LendData() const { return els; }

    int Find(const T& el, int startAt = 0) const {
        for (int i = startAt; i < len; i++) {
            if (els[i] == el) {
                return i;
            }
        }
        return -1;
    }

    bool Contains(const T& el) const { return -1 != Find(el); }

    // returns position of removed element or -1 if not removed
    int Remove(const T& el) {
        int i = Find(el);
        if (i >= 0) {
            RemoveAt(i);
        }
        return i;
    }

    // returns position of removed element or -1 if not removed
    int RemoveFast(const T& el) {
        int i = Find(el);
        if (i >= 0) {
            RemoveAtFast(i);
        }
        return i;
    }

    void Sort(int (*cmpFunc)(const void* a, const void* b)) { qsort(els, len, kElSize, cmpFunc); }

    void SortTyped(int (*cmpFunc)(const T* a, const T* b)) {
        auto cmpFunc2 = (int (*)(const void* a, const void* b))cmpFunc;
        qsort(els, len, kElSize, cmpFunc2);
    }

    void Reverse() {
        for (int i = 0; i < len / 2; i++) {
            std::swap(els[i], els[len - i - 1]);
        }
    }

    bool IsEmpty() const { return len == 0; }

    // TOOD: replace with IsEmpty()
    bool empty() const { return len == 0; }

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() { return &(els[0]); }
    const_iterator begin() const { return &(els[0]); }
    iterator end() { return &(els[len]); }
    const_iterator end() const { return &(els[len]); }
};

// number of elements, as int (matches len() for Str / WStr)
template <typename T>
inline int len(const Vec<T>& v) {
    return v.len;
}

// only suitable for T that are pointers to C++ objects
template <typename T>
inline void DeleteVecMembers(Vec<T>& v) {
    for (T& el : v) {
        delete el;
    }
    v.Clear();
}

template <typename T>
void VecExpandTo(Arena* arena, T& v, int wantedSize) {
    if (wantedSize <= v.cap) {
        return;
    }
    v.els = (decltype(v.els))ReallocToWantedSize(arena, v.els, &v.cap, wantedSize, (int)sizeof(*v.els));
}

template <typename T>
void VecExpand(Arena* arena, T& v, int n) {
    int wantedSize = v.len + n;
    VecExpandTo(arena, v, wantedSize);
}

template <typename T, typename E>
void VecPush(Arena* arena, T& v, E el) {
    VecExpand(arena, v, 1);
    v.els[v.len] = el;
    v.len++;
}

// Iterator wrapper for range-based for loops over Vec types (structs with len/els)
template <typename Vec>
class VecIterator {
    Vec* vec;

  public:
    VecIterator(Vec* v) : vec(v) {}
    auto begin() { return vec ? vec->els : nullptr; }
    auto end() { return vec ? vec->els + vec->len : nullptr; }
};

// Helper functions for type deduction (works with both Vec& and Vec*)
template <typename Vec>
VecIterator<Vec> VecIter(Vec& v) {
    return VecIterator<Vec>(&v);
}
template <typename Vec>
VecIterator<Vec> VecIter(Vec* v) {
    return VecIterator<Vec>(v);
}
