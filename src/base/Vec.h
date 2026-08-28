/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

/* Simple vector/array class that can store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c).

Storage is heap (or arena) only; starts empty with no allocation.
*/
template <typename T>
struct Vec;

// makes a template parameter non-deduced, so e.g. VecAppend(Vec<Base*>&,
// Derived*) still picks T from the vec and converts the element
template <typename T>
struct VecIdentity {
    using type = T;
};
template <typename T>
using VecIdentityT = typename VecIdentity<T>::type;

// Vec<T> with the element type erased. Vec<T>'s layout does not depend on T,
// so Vec<T>::NT() is a cast rather than a copy and the shims below cost
// nothing beyond passing elSize. The bodies live in Arena.cpp and so are
// compiled once instead of once per Vec<T>.
struct VecNonTemplated {
    int len;
    int cap;
    void* els;
};

bool VecReserveNT(Arena* arena, VecNonTemplated* v, int elSize, int wantedSize);
void* VecInsertSpaceNT(VecNonTemplated* v, int elSize, int idx, int count);
bool VecResizeNT(VecNonTemplated* v, int elSize, int newSize);
void VecRemoveAtNT(VecNonTemplated* v, int elSize, int idx, int count);
void VecRemoveAtFastNT(VecNonTemplated* v, int elSize, int idx);
void VecFreeElementsNT(VecNonTemplated* v);
void VecClearNT(VecNonTemplated* v, int elSize);
void* VecTakeNT(VecNonTemplated* v, int elSize);
void VecCopyFromNT(VecNonTemplated* v, int elSize, int srcLen, const void* srcEls, bool zeroTail);

// Ensure capacity is at least n for a vec-like {els,len,cap}. Returns the
// elements, or null if it couldn't. Growth: max(cap*2, n). arena may be null
// (heap). T is the vec, so the return type is its element pointer. Note a vec
// that has never allocated also returns null for n == 0, since there are no
// elements to point at.
template <typename T>
auto VecReserve(Arena* arena, T& v, int n) -> decltype(v.els);

// Heap Vec: same growth; returns v.els (nullptr on failure).
template <typename T>
inline T* VecReserve(Vec<T>& v, int n);

// Open a hole of `count` elements at `idx`; updates len. Returns &v.els[idx].
template <typename T>
T* VecInsertSpace(Vec<T>& v, int idx, int count);

// Vec<T>'s layout is the same for every T (see the static_asserts below), so
// the type-erased view is a cast, not a copy.
template <typename T>
VecNonTemplated* VecNT(Vec<T>& v);

template <typename T>
bool VecIsValidIndex(const Vec<T>& v, int idx);

template <typename T>
bool VecAppend(Vec<T>& v, const VecIdentityT<T>& el);

template <typename T>
bool VecAppend(Vec<T>& v, const Vec<T>& other);

// Append count elements from src.
template <typename T>
bool VecAppendN(Vec<T>& v, const T* src, int count);

// Append count blank (i.e. zeroed-out) elements at the end.
template <typename T>
T* VecAppendBlanks(Vec<T>& v, int count);

// Perf hack for using a vec as a buffer: hand the storage to the caller
// without a second allocation. Since a vec over-allocates this is likely to
// use more memory than strictly necessary, which usually doesn't matter.
template <typename T>
T* VecTake(Vec<T>& v);

// The storage, without giving it up.
template <typename T>
T* VecData(const Vec<T>& v);

// Index of the first element equal to el at or after startAt, -1 if none.
template <typename T>
int VecFind(const Vec<T>& v, const T& el, int startAt = 0);

template <typename T>
bool VecContains(const Vec<T>& v, const T& el);

// Remove the first el; returns where it was, or -1 if it wasn't there.
template <typename T>
int VecRemove(Vec<T>& v, const T& el);

// Free the storage, leaving the vec empty (len, cap and els all 0).
template <typename T>
void VecReset(Vec<T>& v);

// free() every element, then reset. Only for a vec of pointers.
template <typename T>
void VecFreeMembers(Vec<T>& v);

// Insert el at idx, moving the rest up.
template <typename T>
bool VecInsertAt(Vec<T>& v, int idx, const VecIdentityT<T>& el);

// Remove count elements at idx, moving the rest down.
template <typename T>
void VecRemoveAtN(Vec<T>& v, int idx, int count);

// Remove the element at idx.
template <typename T>
void VecRemoveAt(Vec<T>& v, int idx);

// Same, but hands the element back. Both exist because most removals throw the
// element away, and for those VecRemoveAt() avoids copying it out first --
// which for a big element type is the whole cost of the call.
template <typename T>
T VecPopAt(Vec<T>& v, int idx);

// Cheaper RemoveAt: fills the hole with the last element, so the order changes.
// Only for elements that can be moved with memcpy().
template <typename T>
void VecRemoveAtFast(Vec<T>& v, int idx);

// Empty the vec but keep the storage, for efficient reuse.
template <typename T>
void VecClear(Vec<T>& v);

// The last element; the vec must not be empty.
template <typename T>
T& VecLast(const Vec<T>& v);

// Drop the last element, a no-op on an empty vec.
template <typename T>
void VecRemoveLast(Vec<T>& v);

// Remove and return the last element.
template <typename T>
T VecPop(Vec<T>& v);

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

    explicit Vec() = default;

    // ensure that a Vec never shares its els buffer with another after a clone/copy
    // note: we don't inherit allocator as it's not needed for our use cases
    Vec(const Vec& other) { VecCopyFromNT(VecNT(*this), (int)sizeof(T), other.len, (const void*)other.els, false); }

    Vec& operator=(const Vec& other) {
        if (this == &other) {
            return *this;
        }

        VecReset(*this);
        VecCopyFromNT(VecNT(*this), (int)sizeof(T), other.len, (const void*)other.els, true);
        return *this;
    }

    ~Vec() { VecReset(*this); }

    T& operator[](int idx) const {
        ReportIf(idx < 0);
        ReportIf(idx >= len);
        return els[idx];
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

// v is a Vec<T> or another vec-shaped struct (VecStr, str::Builder); they all
// lead with {len, cap, els}, which the static_asserts hold them to, so the
// erased view is a cast and this compiles to just the call
template <typename T>
auto VecReserve(Arena* arena, T& v, int n) -> decltype(v.els) {
    static_assert(offsetof(T, len) == offsetof(VecNonTemplated, len));
    static_assert(offsetof(T, cap) == offsetof(VecNonTemplated, cap));
    static_assert(offsetof(T, els) == offsetof(VecNonTemplated, els));
    if (!VecReserveNT(arena, (VecNonTemplated*)&v, (int)sizeof(*v.els), n)) {
        return nullptr;
    }
    return v.els;
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
// Vec<T>::NT() casts, so the layouts must match. Vec<T> is standard-layout and
// its field offsets do not depend on T; check both a small and a large T.
static_assert(sizeof(Vec<char>) == sizeof(VecNonTemplated));
static_assert(offsetof(Vec<char>, len) == offsetof(VecNonTemplated, len));
static_assert(offsetof(Vec<char>, cap) == offsetof(VecNonTemplated, cap));
static_assert(offsetof(Vec<char>, els) == offsetof(VecNonTemplated, els));
static_assert(sizeof(Vec<double>) == sizeof(VecNonTemplated));
static_assert(offsetof(Vec<double>, els) == offsetof(VecNonTemplated, els));

template <typename T, int N>
inline void VecUseExternalBuffer(Vec<T>& v, T (&buf)[N]) {
    v.els = buf;
    v.cap = -N;
    v.len = 0;
}

template <typename T>
inline T* VecReserve(Vec<T>& v, int n) {
    return VecReserve(nullptr, v, n);
}

template <typename T>
T* VecInsertSpace(Vec<T>& v, int idx, int count) {
    return (T*)VecInsertSpaceNT(VecNT(v), (int)sizeof(T), idx, count);
}

template <typename T>
void VecReset(Vec<T>& v) {
    VecFreeElementsNT(VecNT(v));
}

template <typename T>
void VecFreeMembers(Vec<T>& v) {
    for (int i = 0; i < v.len; i++) {
        free(v.els[i]);
    }
    VecReset(v);
}

template <typename T>
VecNonTemplated* VecNT(Vec<T>& v) {
    return (VecNonTemplated*)&v;
}

template <typename T>
bool VecIsValidIndex(const Vec<T>& v, int idx) {
    return (idx >= 0) && (idx < v.len);
}

template <typename T>
bool VecAppend(Vec<T>& v, const VecIdentityT<T>& el) {
    return VecInsertAt(v, v.len, el);
}

template <typename T>
bool VecAppend(Vec<T>& v, const Vec<T>& other) {
    return VecAppendN(v, other.els, other.len);
}

template <typename T>
T* VecAppendBlanks(Vec<T>& v, int count) {
    return VecInsertSpace(v, v.len, count);
}

template <typename T>
T* VecTake(Vec<T>& v) {
    return (T*)VecTakeNT(VecNT(v), (int)sizeof(T));
}

template <typename T>
T* VecData(const Vec<T>& v) {
    return v.els;
}

template <typename T>
void VecRemoveAtN(Vec<T>& v, int idx, int count) {
    VecRemoveAtNT(VecNT(v), (int)sizeof(T), idx, count);
}

template <typename T>
void VecRemoveAt(Vec<T>& v, int idx) {
    VecRemoveAtN(v, idx, 1);
}

template <typename T>
T VecPopAt(Vec<T>& v, int idx) {
    ReportIf(idx >= v.len);
    T el = v.els[idx];
    VecRemoveAtN(v, idx, 1);
    return el;
}

template <typename T>
void VecRemoveAtFast(Vec<T>& v, int idx) {
    VecRemoveAtFastNT(VecNT(v), (int)sizeof(T), idx);
}

template <typename T>
void VecClear(Vec<T>& v) {
    VecClearNT(VecNT(v), (int)sizeof(T));
}

template <typename T>
T& VecLast(const Vec<T>& v) {
    ReportIf(0 == v.len);
    return v.els[v.len - 1];
}

template <typename T>
void VecRemoveLast(Vec<T>& v) {
    if (v.len == 0) {
        return;
    }
    VecRemoveAt(v, v.len - 1);
}

template <typename T>
T VecPop(Vec<T>& v) {
    ReportIf(0 == v.len);
    T el = v.els[v.len - 1];
    VecRemoveAtFast(v, v.len - 1);
    return el;
}

template <typename T>
bool VecInsertAt(Vec<T>& v, int idx, const VecIdentityT<T>& el) {
    T* p = VecInsertSpace(v, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

template <typename T>
int VecFind(const Vec<T>& v, const T& el, int startAt) {
    for (int i = startAt; i < v.len; i++) {
        if (v.els[i] == el) {
            return i;
        }
    }
    return -1;
}

template <typename T>
bool VecContains(const Vec<T>& v, const T& el) {
    return -1 != VecFind(v, el);
}

template <typename T>
int VecRemove(Vec<T>& v, const T& el) {
    int i = VecFind(v, el);
    if (i >= 0) {
        VecRemoveAt(v, i);
    }
    return i;
}

template <typename T>
bool VecAppendN(Vec<T>& v, const T* src, int count) {
    if (0 == count) {
        return true;
    }
    T* dst = VecInsertSpace(v, v.len, count);
    if (!dst) {
        return false;
    }
    memcpy((void*)dst, (const void*)src, (size_t)count * sizeof(T));
    return true;
}

// Set logical length to newSize (std::vector::resize). Grows capacity if needed;
// zeros unused capacity beyond the new length.
template <typename T>
bool VecResize(Vec<T>& v, int newSize) {
    return VecResizeNT(VecNT(v), (int)sizeof(T), newSize);
}

// only suitable for T that are pointers to C++ objects
template <typename T>
inline void DeleteVecMembers(Vec<T>& v) {
    for (T& el : v) {
        delete el;
    }
    VecClear(v);
}

template <typename T, typename E>
bool VecPush(Arena* arena, T& v, E el) {
    if (!VecReserve(arena, v, v.len + 1)) {
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
