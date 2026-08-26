/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

/* Simple vector/array class that can store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c).

Storage is heap (or arena) only; starts empty with no allocation.
*/
template <typename T>
struct Vec;

// Ensure capacity is at least wantedSize for a vec-like {els,len,cap}.
// Growth: max(cap*2, wantedSize). arena may be null (heap).
template <typename T>
bool VecReserve(Arena* arena, T& v, int wantedSize);

// Heap Vec: same growth; returns v.els (nullptr on failure).
template <typename T>
inline T* VecReserve(Vec<T>& v, int capNeeded);

// Open a hole of `count` elements at `idx`; updates len. Returns &v.els[idx].
template <typename T>
T* VecInsertSpace(Vec<T>& v, int idx, int count);

template <typename T>
struct Vec {
    int len = 0;
    // Negative means the elements sit in storage this vec does not own —
    // VecUseExternalBuffer put them in an array on the caller's stack — and the
    // capacity is `-cap`. Cap() is the one to read; the sign is only for
    // the two places that have to tell owned from borrowed, growing and
    // freeing. A vec that borrows leaves the borrowed block alone forever:
    // the first append past it allocates and copies, and nothing frees the
    // array.
    int cap = 0;
    T* els = nullptr;

    // We always pad heap storage with a single 0 value. This makes
    // Vec<char> and Vec<WCHAR> a C-compatible string. Although it's
    // not useful for other types, the code is simpler if we always do it
    // (rather than have it an optional behavior). Borrowed storage is not
    // padded; VecUseExternalBuffer is for POD, not C-string Vec<char>.

    int Cap() const { return cap < 0 ? -cap : cap; }

    void FreeEls() {
        if (els) {
            if (cap > 0) {
                Free(nullptr, (void*)els);
            } else {
                cap = 0;
            }
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
        if (els && Cap() > 0) {
            memset((void*)els, 0, (size_t)Cap() * sizeof(T));
        }
    }

    explicit Vec() = default;

    // ensure that a Vec never shares its els buffer with another after a clone/copy
    // note: we don't inherit allocator as it's not needed for our use cases
    Vec(const Vec& other) {
        VecReserve(*this, other.len);
        len = other.len;
        // using memcpy, as Vec only supports POD types
        if (other.len > 0 && other.els && els) {
            memcpy((void*)els, (const void*)other.els, sizeof(T) * (size_t)other.len);
        }
    }

    // TODO: write Vec(const Vec&& other)

    Vec& operator=(const Vec& other) {
        if (this == &other) {
            return *this;
        }

        Reset();
        VecReserve(*this, other.len);
        // using memcpy, as Vec only supports POD types
        len = other.len;
        if (other.len > 0) {
            memcpy((void*)els, (const void*)other.els, sizeof(T) * (size_t)len);
            memset((void*)(els + len), 0, sizeof(T) * (size_t)(Cap() - len));
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
        T* p = VecInsertSpace(*this, idx, 1);
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
        T* dst = VecInsertSpace(*this, len, count);
        if (!dst) {
            return false;
        }
        memcpy((void*)dst, (const void*)src, (size_t)count * sizeof(T));
        return true;
    }

    bool Append(const Vec& other) {
        int n = other.len;
        const T* data = other.LendData();
        return this->Append(data, n);
    }

    // appends count blank (i.e. zeroed-out) elements at the end
    T* AppendBlanks(int count) { return VecInsertSpace(*this, len, count); }

    void RemoveAt(int idx, int count = 1) {
        if (len > idx + count) {
            T* dst = els + idx;
            T* src = els + idx + count;
            memmove((void*)dst, (const void*)src, (size_t)(len - idx - count) * sizeof(T));
        }
        len -= count;
        memset((void*)(els + len), 0, (size_t)count * sizeof(T));
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
            memcpy((void*)toRemove, (const void*)last, sizeof(T));
        }
        memset((void*)last, 0, sizeof(T));
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
        if (cap < 0) {
            int n = len;
            T* borrowed = els;
            els = nullptr;
            cap = 0;
            len = 0;
            if (n <= 0) {
                return nullptr;
            }
            if (!VecRealloc(nullptr, (void**)&els, 0, &cap, n, (int)sizeof(T))) {
                return nullptr;
            }
            memcpy((void*)els, (const void*)borrowed, (size_t)n * sizeof(T));
            T* res = els;
            els = nullptr;
            cap = 0;
            return res;
        }
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

// cmp is non-deduced (via nested type) so lambdas convert after T is known from v.
template <typename T>
struct VecSortCmp {
    using Fn = int (*)(const T* a, const T* b);
};

template <typename T>
void VecSort(Vec<T>& v, typename VecSortCmp<T>::Fn cmpFunc) {
    if (v.len > 0) {
        auto cmp = (int (*)(const void* a, const void* b))cmpFunc;
        qsort((void*)v.els, v.len, sizeof(T), cmp);
    }
}

template <typename T>
void VecReverse(Vec<T>& v) {
    for (int i = 0; i < v.len / 2; i++) {
        std::swap(v.els[i], v.els[v.len - i - 1]);
    }
}

// Doubling, but never from a first capacity of one. max(cap * 2, wanted)
// out of an empty vec hands back 1, so a vec that ends up holding four
// elements reallocates and memcpys three times on the way there. The floor
// is in bytes rather than in elements (Rust's RawVec::MIN_NON_ZERO_CAP):
// four 192-byte items is a sensible first block and four 4 KB ones is not.
inline int VecNextCap(int cap, int wanted, int elSize) {
    if (cap == 0) {
        int floorCap = elSize == 1 ? 8 : elSize <= 1024 ? 4 : 1;
        return std::max(floorCap, wanted);
    }
    return std::max(cap * 2, wanted);
}

template <typename T>
bool VecReserve(Arena* arena, T& v, int wantedSize) {
    // v is a Vec<T> or a vec-shaped struct (str::Builder), so the borrowed-
    // storage sign is spelled out here rather than read off Cap() those do
    // not have. A positive cap in those is the only case they ever reach.
    int elSize = (int)sizeof(*v.els);
    int curCap = v.cap < 0 ? -v.cap : v.cap;
    if (wantedSize <= curCap) {
        return true;
    }
    int newCap = VecNextCap(curCap, wantedSize, elSize);
    if (v.cap < 0) {
        auto* borrowed = v.els;
        v.els = nullptr;
        v.cap = 0;
        if (!VecRealloc(arena, (void**)&v.els, 0, &v.cap, newCap, elSize)) {
            v.els = borrowed;
            v.cap = -curCap;
            return false;
        }
        if (v.len > 0) {
            memcpy((void*)v.els, (const void*)borrowed, (size_t)v.len * (size_t)elSize);
        }
        return true;
    }
    return VecRealloc(arena, (void**)&v.els, v.len, &v.cap, newCap, elSize);
}

// Lend v an array to start in, instead of its first allocation. The vec
// must be empty and must not have storage yet — this is for the line right
// after it is declared:
//
//     int buf[4];
//     Vec<int> v;
//     VecUseExternalBuffer(v, buf);
//
// It appends into buf until buf is full, and the append past that allocates
// and copies, leaving buf alone. Nothing frees buf, so it must outlive the
// vec. The vec must not then have its storage taken over by hand
// (other.els = v.els), since the sign is what says the block is not the heap's.
template <typename T, int N>
inline void VecUseExternalBuffer(Vec<T>& v, T (&buf)[N]) {
    v.els = buf;
    v.cap = -N;
    v.len = 0;
}

template <typename T>
inline T* VecReserve(Vec<T>& v, int capNeeded) {
    if (!VecReserve(nullptr, v, capNeeded)) {
        return nullptr;
    }
    return v.els;
}

template <typename T>
T* VecInsertSpace(Vec<T>& v, int idx, int count) {
    int newLen = std::max(v.len, idx) + count;
    T* ok = VecReserve(v, newLen);
    if (!ok) {
        return nullptr;
    }
    T* res = &(v.els[idx]);
    if (v.len > idx) {
        T* src = v.els + idx;
        T* dst = v.els + idx + count;
        memmove((void*)dst, (const void*)src, (size_t)(v.len - idx) * sizeof(T));
    }
    v.len = newLen;
    return res;
}

// Set logical length to newSize (std::vector::resize). Grows capacity if needed;
// zeros unused capacity beyond the new length.
template <typename T>
bool VecResize(Vec<T>& v, int newSize) {
    if (newSize < 0) {
        return false;
    }
    if (newSize > v.Cap()) {
        if (!VecReserve(v, newSize)) {
            return false;
        }
    }
    v.len = newSize;
    if (v.els && v.Cap() > v.len) {
        memset((void*)(v.els + v.len), 0, (size_t)(v.Cap() - v.len) * sizeof(T));
    }
    return true;
}

// only suitable for T that are pointers to C++ objects
template <typename T>
inline void DeleteVecMembers(Vec<T>& v) {
    for (T& el : v) {
        delete el;
    }
    v.Clear();
}

template <typename T, typename E>
bool VecPush(Arena* arena, T& v, E el) {
    bool ok = VecReserve(arena, v, v.len + 1);
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
