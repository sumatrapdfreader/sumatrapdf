/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

Allocator* GetTempAllocator();
void DestroyTempAllocator();
void ResetTempAllocator();

// exists just to mark the intent of the variable
// and provide convenience operators
struct TempStr {
    std::string_view sv{};

    TempStr() = default;
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
    TempStr(const TempStr& ts) {
        sv = ts.sv;
    }
    bool empty() const {
        return sv.empty();
    }
    size_t size() const {
        return sv.size();
    }
    char* Get() const {
        return (char*)sv.data();
    }
    const char* data() const {
        return sv.data();
    }
    std::string_view AsView() const {
        return sv;
    }
    operator const char*() const { // NOLINT
        return sv.data();
    }
    operator char*() const { // NOLINT
        return (char*)sv.data();
    }
};

struct TempWstr {
    const WCHAR* s;
    int sLen;

    TempWstr() {
        s = nullptr;
        sLen = 0;
    };
    explicit TempWstr(WCHAR* sIn) {
        s = sIn;
        sLen = -1; // net yet known
    }
    explicit TempWstr(WCHAR* sIn, size_t cch) {
        s = sIn;
        sLen = (int)cch;
    }
    explicit TempWstr(const WCHAR* sIn) {
        s = sIn;
        sLen = -1; // net yet known
    }
    explicit TempWstr(const WCHAR* sIn, size_t cch) {
        s = sIn;
        sLen = (int)cch;
    }
    TempWstr(const TempWstr& o) {
        s = o.s;
        sLen = o.sLen;
    }
    bool empty() const {
        if (s == nullptr) {
            return true;
        }
        // TOOD: remve const and update sLen?
        int n = sLen;
        if (n < 0) {
            n = (int)str::Len(s);
        }
        return n == 0;
    }
    size_t size() const {
        if (s == nullptr) {
            return 0;
        }
        if (sLen < 0) {
            return (int)str::Len(s);
        }
        return sLen;
    }
    WCHAR* Get() const {
        return (WCHAR*)s;
    }
    operator const WCHAR*() const { // NOLINT
        return s;
    }
    operator WCHAR*() const { // NOLINT
        return (WCHAR*)s;
    }
};

namespace str {
TempStr DupTemp(const char* s, size_t cb = (size_t)-1);
TempWstr DupTemp(const WCHAR* s, size_t cch = (size_t)-1);

TempStr DupTemp(std::string_view);

TempStr JoinTemp(const char* s1, const char* s2, const char* s3 = nullptr);
TempWstr JoinTemp(const WCHAR* s1, const WCHAR* s2, const WCHAR* s3 = nullptr);
} // namespace str

TempStr ToUtf8Temp(const WCHAR* s, size_t cch = (size_t)-1);
TempWstr ToWstrTemp(const char* s, size_t cb = (size_t)-1);
TempWstr ToWstrTemp(std::string_view sv);
