/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef VecSegmented_h
#define VecSegmented_h

#include "BaseUtil.h"
#include "StrUtil.h"
#include "Allocator.h"

/* VecSegmented has (mostly) the same API as Vec but it allocates
   using PoolAllocator. This means it's append only (we have no
   easy way to remove an item). The upside is that we can retain
   pointers to elements within the vector because we never
   reallocate memory, so once the element has been added to the
   array, it forever occupies the same piece of memory.
*/
template <typename T>
class VecSegmented {
protected:
    PoolAllocator allocator;
    size_t        len;

    T* MakeSpaceAt(size_t idx, size_t count) {
#if 0
        EnsureCap(len + count);
        T* res = &(els[idx]);
        if (len > idx) {
            T* src = els + idx;
            T* dst = els + idx + count;
            memmove(dst, src, (len - idx) * sizeof(T));
        }
        IncreaseLen(count);
        return res;
#endif
    }

public:
    VecSegmented() : len(0) {
    }

    ~VecSegmented() {
        allocator.FreeAll();
    }

    void EnsureCap(size_t needed) {
    }

    // ensures empty space at the end of the list where items can
    // be appended through ReadFile or memcpy (don't forget to call
    // IncreaseLen once you know how many items have been added)
    // and returns a pointer to the first empty spot
    // Note: use AppendBlanks if you know the number of items in advance
    T *EnsureEndPadding(size_t count) {
        EnsureCap(len + count);
        return &els[len];
    }

    void IncreaseLen(size_t count) {
        len += count;
    }

    // use &At() if you need a pointer to the element (e.g. if T is a struct)
    T& At(size_t idx) const {
        return els[idx];
    }

    size_t Count() const {
        return len;
    }

    size_t Size() const {
        return len;
    }

    void InsertAt(size_t idx, const T& el) {
        MakeSpaceAt(idx, 1)[0] = el;
    }

    void Append(const T& el) {
        InsertAt(len, el);
    }

    void Append(const T* src, size_t count) {
        T* dst = AppendBlanks(count);
        memcpy(dst, src, count * sizeof(T));
    }

    // TODO: bad name, it doesn't append anything; AllocateAtEnd()?
    // like EnsureEndPadding() but also increases the length
    T* AppendBlanks(size_t count) {
        return MakeSpaceAt(len, count);
    }

    void Push(T el) {
        Append(el);
    }

    T& Pop() {
        assert(len > 0);
        if (len > 0)
            len--;
        return At(len);
    }

    T& Last() const {
        assert(len > 0);
        return At(len - 1);
    }

#if 0
    int Find(T el, size_t startAt=0) const {
        for (size_t i = startAt; i < len; i++) {
            if (els[i] == el)
                return (int)i;
        }
        return -1;
    }

    void Sort(int (*cmpFunc)(const void *a, const void *b)) {
        qsort(els, len, sizeof(T), cmpFunc);
    }

    void Reverse() {
        for (size_t i = 0; i < len / 2; i++) {
            swap(els[i], els[len - i - 1]);
        }
    }
#endif

};

#if 0
// only suitable for T that are pointers that were malloc()ed
template <typename T>
inline void FreeVecMembers(VecSegmented<T>& v)
{
    T *data = v.LendData();
    for (size_t i = 0; i < v.Count(); i++) {
        free(data[i]);
    }
    v.Reset();
}

// only suitable for T that are pointers to C++ objects
template <typename T>
inline void DeleteVecMembers(VecSegmented<T>& v)
{
    T *data = v.LendData();
    for (size_t i = 0; i < v.Count(); i++) {
        delete data[i];
    }
    v.Reset();
}
#endif

#endif
