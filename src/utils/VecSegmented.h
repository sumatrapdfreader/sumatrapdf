/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
struct VecSegmented {
    PoolAllocator allocator;

    VecSegmented() = default;

    ~VecSegmented() {
        allocator.FreeAll();
    }

    T* AllocAtEnd() {
        void* p = allocator.Alloc(sizeof(T));
        return reinterpret_cast<T*>(p);
    }

    size_t Size() const {
        return allocator.nAllocs;
    }

    T* AtPtr(int i) {
        void* p = allocator.At(i);
        return (T*)p;
    }

    T* Append(const T& el) {
        T* elPtr = AllocAtEnd();
        *elPtr = el;
        return elPtr;
    }

    PoolAllocator::Iter<T> begin() {
        return allocator.begin<T>();
    }
    PoolAllocator::Iter<T> end() {
        return allocator.end<T>();
    }
};
