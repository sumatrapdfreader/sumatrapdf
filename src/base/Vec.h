/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

/* Simple vector/array class that can store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c).

Storage is heap (or arena) only; starts empty with no allocation.
*/
template <typename T>
struct Vec {
    int len = 0;
    int cap = 0;
    T* els = nullptr;

    // We always pad the elements with a single 0 value. This makes
    // Vec<char> and Vec<WCHAR> a C-compatible string. Although it's
    // not useful for other types, the code is simpler if we always do it
    // (rather than have it an optional behavior).

    inline T* EnsureCap(int capNeeded) {
        // this is frequent, fast path that should be inlined
        if (cap >= capNeeded) {
            return els;
        }
        // slow path: growth policy + untyped realloc (NO_INLINE VecRealloc)
        int newCap = cap * 2;
        if (capNeeded > newCap) {
            newCap = capNeeded;
        }
        if (!VecRealloc(nullptr, (void**)&els, len, &cap, newCap, (int)sizeof(T))) {
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
            memmove(dst, src, (size_t)(len - idx) * sizeof(T));
        }
        len = newLen;
        return res;
    }

    void FreeEls() {
        if (els) {
            Free(nullptr, els);
            els = nullptr;
        }
    }

    // resets to initial state, freeing memory
    void Reset() {
        FreeEls();
        len = 0;
        cap = 0;
    }

    // use to empty but don't free els
    // for efficient reuse
    void Clear() {
        len = 0;
        if (els && cap > 0) {
            memset(els, 0, (size_t)cap * sizeof(T));
        }
    }

    bool SetSize(int newSize) {
        if (newSize <= cap) {
            len = newSize;
            if (els) {
                memset(els + len, 0, (size_t)(cap - len) * sizeof(T));
            }
            return true;
        }
        auto res = MakeSpaceAt(0, newSize);
        return res != nullptr;
    }

    explicit Vec() = default;

    // ensure that a Vec never shares its els buffer with another after a clone/copy
    // note: we don't inherit allocator as it's not needed for our use cases
    Vec(const Vec& other) {
        EnsureCap(other.len);
        len = other.len;
        // using memcpy, as Vec only supports POD types
        if (other.len > 0 && other.els && els) {
            memcpy(els, other.els, sizeof(T) * (size_t)other.len);
        }
    }

    // TODO: write Vec(const Vec&& other)

    Vec& operator=(const Vec& other) {
        if (this == &other) {
            return *this;
        }

        Reset();
        EnsureCap(other.len);
        // using memcpy, as Vec only supports POD types
        len = other.len;
        if (other.len > 0) {
            memcpy(els, other.els, sizeof(T) * (size_t)len);
            memset(els + len, 0, sizeof(T) * (size_t)(cap - len));
        }
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
        memcpy(dst, src, (size_t)count * sizeof(T));
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
            memmove(dst, src, (size_t)(len - idx - count) * sizeof(T));
        }
        len -= count;
        memset(els + len, 0, (size_t)count * sizeof(T));
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
            memcpy(toRemove, last, sizeof(T));
        }
        memset(last, 0, sizeof(T));
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
        els = nullptr;
        len = 0;
        cap = 0;
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

    void Sort(int (*cmpFunc)(const void* a, const void* b)) {
        if (len > 0) {
            qsort(els, len, sizeof(T), cmpFunc);
        }
    }

    void SortTyped(int (*cmpFunc)(const T* a, const T* b)) {
        if (len > 0) {
            auto cmpFunc2 = (int (*)(const void* a, const void* b))cmpFunc;
            qsort(els, len, sizeof(T), cmpFunc2);
        }
    }

    void Reverse() {
        for (int i = 0; i < len / 2; i++) {
            std::swap(els[i], els[len - i - 1]);
        }
    }

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() { return els; }
    const_iterator begin() const { return els; }
    iterator end() { return els ? els + len : nullptr; }
    const_iterator end() const { return els ? els + len : nullptr; }
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

// Grow a vec-like {els, len, cap} to hold at least wantedSize elements.
// Growth policy: (max(cap*2, wantedSize)).
template <typename T>
bool VecExpandTo(Arena* arena, T& v, int wantedSize) {
    if (wantedSize <= v.cap) {
        return true;
    }
    int newCap = v.cap * 2;
    if (wantedSize > newCap) {
        newCap = wantedSize;
    }
    return VecRealloc(arena, (void**)&v.els, v.len, &v.cap, newCap, (int)sizeof(*v.els));
}

template <typename T>
bool VecExpand(Arena* arena, T& v, int n) {
    return VecExpandTo(arena, v, v.len + n);
}

template <typename T, typename E>
bool VecPush(Arena* arena, T& v, E el) {
    bool ok = VecExpand(arena, v, 1);
    if (!ok) {
        return false;
    }
    v.els[v.len] = el;
    v.len++;
    return true;
}

// Iterator wrapper for range-based for loops over Vec types (structs with len/els)
template <typename Vec>
class VecIterator {
    Vec* vec;

  public:
    VecIterator(Vec* v) : vec(v) {}
    auto begin() { return vec ? vec->els : nullptr; }
    auto end() { return vec && vec->els ? vec->els + vec->len : nullptr; }
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
