/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

void InitTempAllocator();
void DestroyTempAllocator();
void ResetTempAllocator();

// exists just to mark the intent of the variable
// and provide convenience operators
struct TempStr {
    std::string_view sv{};

    explicit TempStr(){
        // do nothing
    };
    explicit TempStr(char* s) {
        sv = {s, str::Len(s)};
    }
    explicit TempStr(char* s, size_t cch) {
        CrashIf((int)cch < 0);
        sv = {s, cch};
    }
    explicit TempStr(const char* s) {
        sv = {s, str::Len(s)};
    }
    explicit TempStr(const char* s, size_t cch) {
        CrashIf((int)cch < 0);
        sv = {s, cch};
    }
    explicit TempStr(std::string_view s) {
        sv = s;
    }
    explicit TempStr(const TempStr& ts) {
        sv = ts.sv;
    }
    char* Get() {
        return (char*)sv.data();
    }
    operator const char*() {
        return sv.data();
    }
    operator char*() {
        return (char*)sv.data();
    }
    operator std::string_view() {
        return sv;
    }
};

struct TempWstr {
    std::wstring_view sv{};

    explicit TempWstr() {
        // do nothing
    }
    explicit TempWstr(WCHAR* s) {
        sv = {s, str::Len(s)};
    }
    explicit TempWstr(WCHAR* s, size_t cch) {
        sv = {s, cch};
    }
    explicit TempWstr(const WCHAR* s) {
        sv = {s, str::Len(s)};
    }
    explicit TempWstr(const WCHAR* s, size_t cch) {
        sv = {s, cch};
    }
    explicit TempWstr(std::wstring_view s) {
        sv = s;
    }
    explicit TempWstr(const TempWstr& ts) {
        sv = ts.sv;
    }
    WCHAR* Get() {
        return (WCHAR*)sv.data();
    }
    operator const WCHAR*() {
        return sv.data();
    }
    operator WCHAR*() {
        return (WCHAR*)sv.data();
    }
    operator std::wstring_view() {
        return sv;
    }
};

TempStr TempStrDup(const char* s, size_t cb = (size_t)-1);
TempWstr TempWstrDup(const WCHAR* s, size_t cch = (size_t)-1);

TempStr TempToUtf8(const WCHAR* s, size_t cch = (size_t)-1);
TempStr TempToUtf8(std::wstring_view);
TempWstr TempToWstr(const char* s, size_t cb = (size_t)-1);
TempWstr TempToWstr(std::string_view sv);
