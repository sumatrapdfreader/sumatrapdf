/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

Allocator* GetTempAllocator();
void DestroyTempAllocator();
void ResetTempAllocator();

template <typename T>
FORCEINLINE T* AllocArrayTemp(size_t n) {
    auto a = GetTempAllocator();
    return (T*)Allocator::AllocZero(a, n * sizeof(T));
}

// exists just to mark the intentA
using TempStr = char*;
using TempWStr = WCHAR*;

namespace str {
TempStr DupTemp(const char* s, size_t cb = (size_t)-1);
TempWStr DupTemp(const WCHAR* s, size_t cch = (size_t)-1);

TempStr JoinTemp(const char* s1, const char* s2, const char* s3 = nullptr);
TempWStr JoinTemp(const WCHAR* s1, const WCHAR* s2, const WCHAR* s3 = nullptr);

TempStr ReplaceTemp(const char* s, const char* toReplace, const char* replaceWith);

TempStr FormatTemp(const char* fmt, ...);
} // namespace str

TempStr ToUtf8Temp(const WCHAR* s, size_t cch = (size_t)-1);
TempWStr ToWStrTemp(const char* s, size_t cb = (size_t)-1);
