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
    size_t        len;
    PoolAllocator allocator;

public:

    struct Iter {
        PoolAllocator::Iter<T> iter;
        Iter(VecSegmented<T> *vec) : iter(&vec->allocator) {
        }
        T *Next() {
            return iter.Next();
        }
    };

    VecSegmented() : len(0) {
        allocator.SetAllocRounding(sizeof(T));
    }

    ~VecSegmented() {
        allocator.FreeAll();
    }

    T* MakeSpaceAtEnd(size_t count = 1) {
        void *p = allocator.Alloc(count * sizeof(T));
        return reinterpret_cast<T*>(p);
    }

    void IncreaseLen(size_t count) {
        len += count;
    }

#if 0
    // use &At() if you need a pointer to the element (e.g. if T is a struct)
    T& At(size_t idx) const {
        T *elp = allocator.GetAtPtr<T>(idx);
        return *elp;
    }
#endif
    T* AtPtr(size_t idx) const {
        T *elp = allocator.GetAtPtr<T>(idx);
        return elp;
    }

    size_t Count() const {
        return len;
    }

    size_t Size() const {
        return len;
    }

    void Append(const T& el) {
        T *els = MakeSpaceAtEnd();
        *els = el;
        len += 1;
    }

    void Append(const T* src, size_t count) {
        T *els = MakeSpaceAtEnd(count);
        memcpy(els, src, count * sizeof(T));
        len += count;
    }

    void Push(T el) {
        Append(el);
    }

    T* Last() const {
        CrashIf(0 == len);
        return AtPtr(len - 1);
    }
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
