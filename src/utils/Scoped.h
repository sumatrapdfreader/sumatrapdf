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
    explicit operator T*() const {
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
    size_t len = 0;

    AutoFree() = default;
    AutoFree(AutoFree& other) = delete;
    AutoFree(AutoFree&& other) = delete;

    AutoFree(const char* p) { // NOLINT
        data = (char*)p;
        len = str::Len(data);
    }

    AutoFree(const u8* p) { // NOLINT
        data = (char*)p;
        len = str::Len(data);
    }

    AutoFree(ByteSlice s) { // NOLINT
        data = (char*)s.data();
        len = s.size();
    }

    void Set(const char* newPtr) {
        free(data);
        data = (char*)newPtr;
        len = str::Len(data);
    }

    void Set(ByteSlice d) {
        free(data);
        data = (char*)d.data();
        len = d.size();
    }

    void SetCopy(const char* newPtr) {
        free(data);
        data = nullptr;
        if (newPtr) {
            data = str::Dup(newPtr);
            len = str::Len(data);
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
        len = other.len;
        other.data = nullptr;
        other.len = 0;
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

    // for convenince, we calculate the size if wasn't provided
    // by the caller
    size_t size() const {
        return len;
    }

    bool empty() const {
        return (data == nullptr) || (len == 0);
    }

    ByteSlice AsByteSlice() const {
        return {(u8*)data, len};
    }

    void Reset() {
        free(data);
        data = nullptr;
        len = 0;
    }

    char* Release() {
        char* res = data;
        data = nullptr;
        len = 0;
        return res;
    }

    char* StealData() {
        return this->Release();
    }

    void TakeOwnershipOf(const char* s, size_t size = 0) {
        free(data);
        data = (char*)s;
        if (size == 0) {
            len = str::Len(s);
        } else {
            len = size;
        }
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
