/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// include BaseUtil.h instead of including directly

// auto-free memory for arbitrary malloc()ed memory of type T*
template <typename T>
class ScopedMem {
  public:
    T* ptr = nullptr;

    ScopedMem() = default;
    explicit ScopedMem(T* ptr) : ptr(ptr) {
    }
    ~ScopedMem() {
        free(ptr);
    }
    void Set(T* newPtr) {
        free(ptr);
        ptr = newPtr;
    }
    T* Get() const {
        return ptr;
    }
    T* get() const {
        return ptr;
    }
    T* StealData() {
        T* tmp = ptr;
        ptr = nullptr;
        return tmp;
    }
    operator T*() const {
        return ptr;
    }
};

// deletes any object at the end of the scope
template <class T>
class ScopedPtr {
    T* obj = nullptr;

  public:
    ScopedPtr() = default;
    explicit ScopedPtr(T* obj) : obj(obj) {
    }
    ~ScopedPtr() {
        delete obj;
    }
    T* Detach() {
        T* tmp = obj;
        obj = nullptr;
        return tmp;
    }
    operator T*() const {
        return obj;
    }
    T* operator->() const {
        return obj;
    }
    T* operator=(T* newObj) {
        delete obj;
        obj = newObj;
        return obj;
    }
};

template <typename T>
class AutoFreeStrBasic {
  public:
    T* ptr = nullptr;

    AutoFreeStrBasic() = default;
    explicit AutoFreeStrBasic(T* ptr) {
        this->ptr = ptr;
    }
    ~AutoFreeStrBasic() {
        free(this->ptr);
    }
    void Set(T* newPtr) {
        free(this->ptr);
        this->ptr = newPtr;
    }
    void Set(const T* newPtr) {
        free(this->ptr);
        this->ptr = const_cast<T*>(newPtr);
    }
    void SetCopy(const T* newPtr) {
        free(this->ptr);
        this->ptr = nullptr;
        if (newPtr) {
            this->ptr = str::Dup(newPtr);
        }
    }
    operator T*() const {
        return this->ptr;
    }
#if 0
    // TODO: I want AutoStrW v = str::Dup(L"foo"); to work
    // can't figure out the right incantation
    AutoFreeStr& operator=(T* newPtr) {
        free(ptr);
        ptr = newPtr;
        return *this;
    }
#endif
    T* Get() const {
        return this->ptr;
    }
    T* get() const {
        return this->ptr;
    }
    T* StealData() {
        T* tmp = this->ptr;
        this->ptr = nullptr;
        return tmp;
    }
    void Reset() {
        free(this->ptr);
        this->ptr = nullptr;
    }

    // TODO: only valid for T = char
    std::string_view AsView() const {
        return {this->ptr, str::Len(this->ptr)};
    }
};

typedef AutoFreeStrBasic<char> AutoFreeStr;

#if OS_WIN
typedef AutoFreeStrBasic<WCHAR> AutoFreeW;
#endif

template <typename T>
struct AutoDelete {
    T* o = nullptr;
    AutoDelete() = default;
    AutoDelete(T* p) {
        o = p;
    }
    ~AutoDelete() {
        delete o;
    }

    AutoDelete& operator=(AutoDelete& other) = delete;
    AutoDelete& operator=(AutoDelete&& other) = delete;
    AutoDelete& operator=(const AutoDelete& other) = delete;
    AutoDelete& operator=(const AutoDelete&& other) = delete;

    T* get() const {
        return o;
    }
};

// this is like std::unique_ptr<char> but specialized for our needs
// typical usage:
// AutoFree toFree = str::Dup("foo");
// We also allow assignment from std::tuple<char*, size_t> (because we
// return it from many functions) and remember length
struct AutoFree {
    char* data = nullptr;

  protected:
    // must be accessed via size() as it might
    // not be provided initially so to avoid mistakes
    // we'll calculate it on demand if needed
    size_t len = 0;

  public:
    AutoFree() = default;

    AutoFree(const char* p) {
        data = (char*)p;
    }

    // I often return allocated strings as a tuple so this
    // allows easy freeing of them
    AutoFree(std::tuple<char*, size_t> s) {
        data = std::get<0>(s);
        len = std::get<1>(s);
    }

    ~AutoFree() {
        str::Free(data);
    }

    AutoFree& operator=(AutoFree& other) = delete;
    AutoFree& operator=(AutoFree&& other) = delete;
    AutoFree& operator=(const AutoFree& other) = delete;
    AutoFree& operator=(const AutoFree&& other) = delete;

    char* get() const {
        return data;
    }

    // for convenince, we calculate the size if wasn't provided
    // by the caller
    size_t size() {
        if (len == 0) {
            len = str::Len(data);
        }
        return len;
    }

    std::string_view as_view() {
        size_t sz = size();
        return {data, sz};
    }
};

#if 0
struct AutoFreeWstr {
    WCHAR* d = nullptr;

    AutoFreeWstr() = default;

    AutoFreeWstr(const WCHAR* p) {
        d = (WCHAR*)p;
    }

    ~AutoFreeWstr() {
        str::Free(d);
    }

    AutoFreeWstr& operator=(AutoFreeWstr& other) = delete;
    AutoFreeWstr& operator=(AutoFreeWstr&& other) = delete;
    AutoFreeWstr& operator=(const AutoFreeWstr& other) = delete;
    AutoFreeWstr& operator=(const AutoFreeWstr&& other) = delete;

    WCHAR* get() const {
        return d;
    }

    std::wstring_view as_view() const {
        return {d, str::Len(d)};
    }
};
#endif
