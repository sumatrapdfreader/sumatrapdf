/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* VecSegmented has (mostly) the same API as Vec but allocates
   using PoolAllocator. This means it's append only (we have no
   easy way to remove an item). The upside is that we can retain
   pointers to elements within the vector because we never
   reallocate memory, so once the element has been added to the
   array, it forever occupies the same piece of memory.
   It's also safe to read allocated elements in any thread.
*/
template <typename T>
class VecSegmented {
protected:
    size_t        len;
    PoolAllocator allocator;

public:

    T *IterStart() {
        return allocator.IterStart<T>();
    }
    T *IterNext() {
        return allocator.IterNext<T>();
    }

    VecSegmented() : len(0) {
        allocator.SetAllocRounding(sizeof(T));
    }

    ~VecSegmented() {
        allocator.FreeAll();
    }

    T* AllocAtEnd(size_t count = 1) {
        void *p = allocator.Alloc(count * sizeof(T));
        len += count;
        return reinterpret_cast<T*>(p);
    }

#if 0
    // use &At() if you need a pointer to the element (e.g. if T is a struct)
    T& At(size_t idx) const {
        T *elp = allocator.GetAtPtr<T>(idx);
        return *elp;
    }

    T* AtPtr(size_t idx) const {
        return allocator.GetAtPtr<T>(idx);
    }
#endif

    size_t Count() const {
        return len;
    }

    size_t Size() const {
        return len;
    }

    T* Append(const T& el) {
        T *elPtr = AllocAtEnd();
        *elPtr = el;
        return elPtr;
    }

#if 0
    void Append(const T* src, size_t count) {
        T *els = AllocAtEnd(count);
        memcpy(els, src, count * sizeof(T));
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
