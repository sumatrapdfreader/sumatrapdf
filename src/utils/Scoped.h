/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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
    T* StealData() {
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
    ~AutoDelete() {
        delete o;
    }

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

    T* Get() const {
        return o;
    }
};

// this is like std::unique_ptr<char> but specialized for our needs
// typical usage:
// AutoFree toFree = str::Dup("foo");
struct AutoFree {
    char* data = nullptr;

    AutoFree() = default;
    AutoFree(AutoFree& other) = delete;
    AutoFree(AutoFree&& other) = delete;

    AutoFree(const char* p) { // NOLINT
        data = (char*)p;
    }

    AutoFree(const u8* p) { // NOLINT
        data = (char*)p;
    }

    void Set(const char* newPtr) {
        free(data);
        data = (char*)newPtr;
    }

    void SetCopy(const char* newPtr) {
        free(data);
        data = nullptr;
        if (newPtr) {
            data = str::Dup(newPtr);
        }
    }

    ~AutoFree() {
        str::Free(data);
    }

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
    AutoFree& operator=(const char* d) noexcept {
        if (data == d) {
            return *this;
        }
        free(data);
        data = (char*)d;
        return *this;
    }

    // AutoFree& operator=(const AutoFree& other) = delete;
    // AutoFree& operator=(const AutoFree&& other) = delete;

    char* Get() const {
        return data;
    }

    operator char*() const { // NOLINT
        return data;
    }

    bool empty() const {
        return (!data || !*data);
    }

    void Reset() {
        free(data);
        data = nullptr;
    }

    char* Release() {
        char* res = data;
        data = nullptr;
        return res;
    }

    char* StealData() {
        return this->Release();
    }

    void TakeOwnershipOf(const char* s) {
        free(data);
        data = (char*)s;
    }
};

// TODO: replace most of AutoFree with AutoFreeStr
using AutoFreeStr = AutoFree;

struct AutoFreeWstr {
    WCHAR* data = nullptr;

  protected:
    // must be accessed via size() as it might
    // not be provided initially so to avoid mistakes
    // we'll calculate it on demand if needed
    size_t len = 0;

  public:
    AutoFreeWstr() = default;
    AutoFreeWstr(AutoFreeWstr& other) = delete;
    AutoFreeWstr(AutoFreeWstr&& other) = delete;

    AutoFreeWstr(const WCHAR* p) { // NOLINT
        data = (WCHAR*)p;
    }

    AutoFreeWstr(WCHAR* p) { // NOLINT
        data = p;
    }

    ~AutoFreeWstr() {
        str::Free(data);
    }

    AutoFreeWstr& operator=(AutoFreeWstr& other) = delete;
    AutoFreeWstr& operator=(AutoFreeWstr&& other) noexcept {
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
    AutoFreeWstr& operator=(const AutoFreeWstr& other) = delete;
    AutoFreeWstr& operator=(const AutoFreeWstr&& other) = delete;
#endif

    WCHAR* Get() const {
        return data;
    }

    operator WCHAR*() const { // NOLINT
        return data;
    }

    void Set(const WCHAR* newPtr) {
        str::Free(data);
        data = (WCHAR*)newPtr;
    }

    void SetCopy(const WCHAR* newVal) {
        str::FreePtr(&data);
        data = str::Dup(newVal);
    }

    // for convenince, we calculate the size if wasn't provided
    // by the caller
    // this is size in characters, not bytes
    size_t size() {
        if (len == 0) {
            len = str::Len(data);
        }
        return len;
    }

    bool empty() {
        return (data == nullptr) || (size() == 0);
    }

    WCHAR* StealData() {
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
