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
TempStr DupTemp(Str s);
TempWStr DupTemp(WStr s);

TempStr JoinTemp(Str s1, Str s2, Str s3 = {});
TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4);
TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4, Str s5);
TempWStr JoinTemp(WStr s1, WStr s2, WStr s3 = {});

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith);
TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith);

TempStr FormatTemp(Str fmt, ...);
} // namespace str

// Temporary, guaranteed zero-terminated copy of s (lives in the temp arena).
// Use when passing a Str/WStr to a C or win32 API that requires a
// NUL-terminated string; the name documents that intent at the call site.
// Returns non-const so it implicitly converts to both char* and const char*
// (some C/win32 APIs take non-const), avoiding casts at the call site.
char* CStrTemp(Str s);
WCHAR* CWStrTemp(WStr s);

TempWStr ToWStrTempFromBuilder(const StrBuilder& s);