/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* VecSegmented has (mostly) the same API as std::vector but allocates
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

    size_t size() const {
        return allocator.nAllocs;
    }

    // TODO: push_back() should return void
    T* push_back(const T& el) {
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
