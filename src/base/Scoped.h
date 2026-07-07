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

template <typename Fn>
struct AutoCall;

template <typename Result>
struct AutoCall<Result (*)()> {
    using Fn = Result (*)();
    Fn fn = nullptr;
    AutoCall() = default;
    AutoCall(Fn fn) { this->fn = fn; } // NOLINT
    AutoCall(AutoCall& other) = delete;
    AutoCall(AutoCall&& other) = delete;
    AutoCall(const AutoCall& other) = delete;
    AutoCall(const AutoCall&& other) = delete;
    ~AutoCall() {
        if (fn) {
            fn();
        }
    }

    AutoCall& operator=(AutoCall& other) = delete;
    AutoCall& operator=(AutoCall&& other) = delete;
    AutoCall& operator=(const AutoCall& other) = delete;
    AutoCall& operator=(const AutoCall&& other) = delete;
};

template <typename Result, typename Arg>
struct AutoCall<Result (*)(Arg)> {
    using Fn = Result (*)(Arg);
    Fn fn = nullptr;
    Arg arg{};
    AutoCall() = default;
    AutoCall(Fn fn, Arg arg) { // NOLINT
        this->fn = fn;
        this->arg = arg;
    }
    AutoCall(AutoCall& other) = delete;
    AutoCall(AutoCall&& other) = delete;
    AutoCall(const AutoCall& other) = delete;
    AutoCall(const AutoCall&& other) = delete;
    ~AutoCall() {
        if (fn) {
            fn(arg);
        }
    }

    AutoCall& operator=(AutoCall& other) = delete;
    AutoCall& operator=(AutoCall&& other) = delete;
    AutoCall& operator=(const AutoCall& other) = delete;
    AutoCall& operator=(const AutoCall&& other) = delete;
};

template <typename Result, typename Arg1, typename Arg2>
struct AutoCall<Result (*)(Arg1, Arg2)> {
    using Fn = Result (*)(Arg1, Arg2);
    Fn fn = nullptr;
    Arg1 arg1{};
    Arg2 arg2{};
    AutoCall() = default;
    AutoCall(Fn fn, Arg1 arg1, Arg2 arg2) { // NOLINT
        this->fn = fn;
        this->arg1 = arg1;
        this->arg2 = arg2;
    }
    AutoCall(AutoCall& other) = delete;
    AutoCall(AutoCall&& other) = delete;
    AutoCall(const AutoCall& other) = delete;
    AutoCall(const AutoCall&& other) = delete;
    ~AutoCall() {
        if (fn) {
            fn(arg1, arg2);
        }
    }

    AutoCall& operator=(AutoCall& other) = delete;
    AutoCall& operator=(AutoCall&& other) = delete;
    AutoCall& operator=(const AutoCall& other) = delete;
    AutoCall& operator=(const AutoCall&& other) = delete;
};

template <typename Result>
AutoCall(Result (*)()) -> AutoCall<Result (*)()>;
template <typename Result, typename Arg>
AutoCall(Result (*)(Arg), Arg) -> AutoCall<Result (*)(Arg)>;
template <typename Result, typename Arg1, typename Arg2>
AutoCall(Result (*)(Arg1, Arg2), Arg1, Arg2) -> AutoCall<Result (*)(Arg1, Arg2)>;
