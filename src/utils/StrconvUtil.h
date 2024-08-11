/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace strconv {

WCHAR* Utf8ToWStr(const char* s, size_t cb = (size_t)-1, Allocator* a = nullptr);
char* WStrToUtf8(const WCHAR* s, size_t cch = (size_t)-1, Allocator* a = nullptr);

char* WStrToCodePage(uint codePage, const WCHAR* s, size_t cch = (size_t)-1, Allocator* a = nullptr);
TempStr ToMultiByteTemp(const char* src, uint codePageSrc, uint codePageDest);
WCHAR* StrCPToWStr(const char* src, uint codePage, int cbSrc = -1);
TempWStr StrCPToWStrTemp(const char* src, uint codePage, int cbSrc = -1);
TempStr StrToUtf8Temp(const char* src, uint codePage);

char* UnknownToUtf8Temp(const char*);

char* WStrToAnsi(const WCHAR*);
char* Utf8ToAnsi(const char*);

TempWStr AnsiToWStrTemp(const char* src, size_t cbLen = (size_t)-1);
char* AnsiToUtf8(const char* src, size_t cbLen = (size_t)-1);

} // namespace strconv

// shorter names
// TODO: eventually we want to migrate all strconv:: to them
char* ToUtf8(const WCHAR* s, size_t cch = (size_t)-1);
WCHAR* ToWStr(const char* s, size_t cb = (size_t)-1);
