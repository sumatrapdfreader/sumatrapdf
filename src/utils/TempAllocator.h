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
TempWStr DupTemp(WStr s, size_t cch = (size_t)-1);

TempStr JoinTemp(Str s1, Str s2, Str s3 = {});
TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4);
TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4, Str s5);
TempWStr JoinTemp(WStr s1, WStr s2, WStr s3 = {});

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith);
TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith);

TempStr FormatTemp(const char* fmt, ...);
} // namespace str

FORCEINLINE TempStr ToUtf8Temp(const WCHAR* s, size_t cch = (size_t)-1) {
    if (!s) {
        ReportIf((int)cch > 0);
        return {};
    }
    if (cch == (size_t)-1) {
        return ToUtf8Temp(WStr(s));
    }
    return ToUtf8Temp(WStr((wchar_t*)s, (int)cch));
}
TempWStr ToWStrTempFromBuilder(const StrBuilder& s);