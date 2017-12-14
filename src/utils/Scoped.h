/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// include BaseUtil.h instead of including directly

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
    T* StealData() {
        T* tmp = ptr;
        ptr = nullptr;
        return tmp;
    }
    operator T*() const { return ptr; }
};

// deletes any object at the end of the scope
template <class T>
class ScopedPtr {
    T* obj = nullptr;

  public:
    ScopedPtr() = default;
    explicit ScopedPtr(T* obj) : obj(obj) {}
    ~ScopedPtr() { delete obj; }
    T* Detach() {
        T* tmp = obj;
        obj = nullptr;
        return tmp;
    }
    operator T*() const { return obj; }
    T* operator->() const { return obj; }
    T* operator=(T* newObj) {
        delete obj;
        return (obj = newObj);
    }
};

template <typename T>
class AutoFreeStr : public ScopedMem<T> {
  public:
    AutoFreeStr() = default;
    explicit AutoFreeStr(T* ptr) { this->ptr = ptr; }
    void SetCopy(const T* newPtr) {
        free(this->ptr);
        this->ptr = nullptr;
        if (newPtr) {
            this->ptr = str::Dup(newPtr);
        }
    }
    // only valid for T = char
    std::string_view AsView() const { return {this->ptr, str::Len(this->ptr)}; }
};

typedef AutoFreeStr<char> AutoFree;

#if OS_WIN
typedef AutoFreeStr<WCHAR> AutoFreeW;
#endif
