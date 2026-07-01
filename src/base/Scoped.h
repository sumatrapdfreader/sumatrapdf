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

// this is like std::unique_ptr<char> but specialized for our needs
// typical usage:
// AutoFree toFree = str::Dup("foo");
struct AutoFree {
    char* data = nullptr;

    AutoFree() = default;
    AutoFree(AutoFree& other) = delete;
    AutoFree(AutoFree&& other) = delete;

    AutoFree(char* p) { // NOLINT
        data = p;
    }

    AutoFree(const u8* p) { // NOLINT
        data = (char*)p;
    }

    void Set(char* newPtr) {
        free(data);
        data = newPtr;
    }

    void SetCopy(Str newVal) {
        free(data);
        data = nullptr;
        if (newVal) {
            data = str::Dup(newVal).s;
        }
    }

    ~AutoFree() { free(data); }

    AutoFree& operator=(AutoFree& other) = delete;
    AutoFree& operator=(AutoFree&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        free(data);
        data = other.data;
        other.data = nullptr;
        return *this;
    }
    // takes ownership of the data
    AutoFree& operator=(char* d) noexcept {
        if (data == d) {
            return *this;
        }
        free(data);
        data = d;
        return *this;
    }

    // AutoFree& operator=(const AutoFree& other) = delete;
    // AutoFree& operator=(const AutoFree&& other) = delete;

    char* Get() const { return data; }

    Str CStr() const { return Str(data); }

    operator char*() const { // NOLINT
        return data;
    }

    bool empty() const { return (!data || !*data); }

    void Reset() {
        free(data);
        data = nullptr;
    }

    char* Release() {
        char* res = data;
        data = nullptr;
        return res;
    }

    char* Take() { return this->Release(); }
};

// TODO: replace most of AutoFree with AutoFreeStr
using AutoFreeStr = AutoFree;

struct AutoFreeWStr {
    WCHAR* data = nullptr;

  protected:
    // must be accessed via size() as it might
    // not be provided initially so to avoid mistakes
    // we'll calculate it on demand if needed
    size_t len = 0;

  public:
    AutoFreeWStr() = default;
    AutoFreeWStr(AutoFreeWStr& other) = delete;
    AutoFreeWStr(AutoFreeWStr&& other) = delete;

    AutoFreeWStr(const WCHAR* p) { // NOLINT
        data = (WCHAR*)p;
    }

    AutoFreeWStr(WCHAR* p) { // NOLINT
        data = p;
    }

    ~AutoFreeWStr() { free(data); }

    AutoFreeWStr& operator=(AutoFreeWStr& other) = delete;
    AutoFreeWStr& operator=(AutoFreeWStr&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        free(data);
        data = other.data;
        len = other.len;
        other.data = nullptr;
        other.len = 0;
        return *this;
    }

#if 0
    AutoFreeWStr& operator=(const AutoFreeWStr& other) = delete;
    AutoFreeWStr& operator=(const AutoFreeWStr&& other) = delete;
#endif

    WCHAR* Get() const { return data; }

    operator WCHAR*() const { // NOLINT
        return data;
    }

    void Set(const WCHAR* newPtr) {
        free(data);
        data = (WCHAR*)newPtr;
    }

    void SetCopy(WStr newVal) {
        WStr w = wstr::Dup(newVal);
        free(data);
        data = w.s;
    }

    // for convenince, we calculate the size if wasn't provided
    // by the caller
    // this is size in characters, not bytes
    size_t size() {
        if (len == 0) {
            len = ::len(WStr(data));
        }
        return len;
    }

    bool empty() { return (data == nullptr) || (size() == 0); }

    WCHAR* Take() {
        WCHAR* res = data;
        data = nullptr;
        len = 0;
        return res;
    }

    void Reset() {
        free(data);
        data = nullptr;
    }
};
