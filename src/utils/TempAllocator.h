/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

// GetTempArena()/ResetTempArena()/DestroyTempArena() are declared in common.h

// arena for allocations that live until the program exits (see TempAllocator.cpp)
extern Arena* gLifetimeArena;
Arena* GetLifetimeArena();
void DestroyLifetimeArena();

template <typename T>
FORCEINLINE T* AllocArrayTemp(size_t n) {
    if (!mulSafe<size_t>(&n, sizeof(T))) {
        return nullptr;
    }
    auto a = GetTempArena();
    return (T*)AllocZero(a, n);
}

namespace str {
TempStr DupTemp(Str s, size_t cb = (size_t)-1);
FORCEINLINE TempStr DupTemp(const char* s, size_t cb = (size_t)-1) {
    return DupTemp(Str(s), cb);
}
TempWStr DupTemp(const WCHAR* s, size_t cch = (size_t)-1);

TempStr JoinTemp(const char* s1, const char* s2, const char* s3 = nullptr);
TempStr JoinTemp(const char* s1, const char* s2, const char* s3, const char* s4);
TempStr JoinTemp(const char* s1, const char* s2, const char* s3, const char* s4, const char* s5);
TempStr JoinTemp(Str s1, const char* s2, const char* s3 = nullptr);
TempStr JoinTemp(const char* s1, Str s2, const char* s3 = nullptr);
TempStr JoinTemp(Str s1, Str s2, const char* s3 = nullptr);
TempWStr JoinTemp(const WCHAR* s1, const WCHAR* s2, const WCHAR* s3 = nullptr);
TempWStr JoinTemp(WStr s1, const WCHAR* s2, const WCHAR* s3 = nullptr);
TempWStr JoinTemp(const WCHAR* s1, WStr s2, const WCHAR* s3 = nullptr);
TempWStr JoinTemp(WStr s1, WStr s2, const WCHAR* s3 = nullptr);

TempStr ReplaceTemp(const char* s, const char* toReplace, const char* replaceWith);
TempStr ReplaceNoCaseTemp(const char* s, const char* toReplace, const char* replaceWith);

TempStr FormatTemp(const char* fmt, ...);
} // namespace str

TempStr ToUtf8Temp(const WCHAR* s, size_t cch = (size_t)-1);
TempWStr ToWStrTemp(const char* s, size_t cb = (size_t)-1);
TempWStr ToWStrTemp(const StrBuilder& s);
