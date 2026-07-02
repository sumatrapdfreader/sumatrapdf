/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// include Base.h instead of including directly

// auto-free memory for arbitrary malloc()ed memory of type T*
template <typename T>
class ScopedMem {
  public:
    T* ptr = nullptr;

    ScopedMem() = default;
    explicit ScopedMem(T* ptr) : ptr(ptr) {}
    ~ScopedMem() { free(ptr); }
    void Set(T* newPtr) {
        free(ptr);
        ptr = newPtr;
    }
    T* Get() const { return ptr; }
    T* Take() {
        T* tmp = ptr;
        ptr = nullptr;
        return tmp;
    }
    operator T*() const { // NOLINT
        return ptr;
    }
};

// deletes an object at the end of the scope
template <typename T>
struct AutoDelete {
    T* o = nullptr;
    AutoDelete() = default;
    AutoDelete(T* p) { // NOLINT
        o = p;
    }
    ~AutoDelete() { delete o; }

    AutoDelete& operator=(AutoDelete& other) = delete;
    AutoDelete& operator=(AutoDelete&& other) = delete;
    AutoDelete& operator=(const AutoDelete& other) = delete;
    AutoDelete& operator=(const AutoDelete&& other) = delete;
    operator T*() const { // NOLINT
        return o;
    }
    T* operator->() const { // NOLINT
        return o;
    }
};

template <typename T>
struct AutoRun {
    using fnPtr = void (*)(T*);
    T* o = nullptr;
    fnPtr fn = nullptr;
    AutoRun() = default;
    AutoRun(fnPtr fn, T* o) { // NOLINT
        this->fn = fn;
        this->o = o;
    }
    ~AutoRun() {
        if (fn) {
            fn(o);
        }
    }

    AutoRun& operator=(AutoRun& other) = delete;
    AutoRun& operator=(AutoRun&& other) = delete;
    AutoRun& operator=(const AutoRun& other) = delete;
    AutoRun& operator=(const AutoRun&& other) = delete;
};
