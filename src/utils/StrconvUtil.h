/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace strconv {

WCHAR* Utf8ToWstr(const char* s, size_t cb = (size_t)-1, Allocator* a = nullptr);
char* WstrToUtf8(const WCHAR* s, size_t cch = (size_t)-1, Allocator* a = nullptr);

char* WstrToCodePage(uint codePage, const WCHAR* s, size_t cch = (size_t)-1, Allocator* a = nullptr);
char* ToMultiByte(const char* src, uint codePageSrc, uint codePageDest);
WCHAR* StrToWstr(const char* src, uint codePage, int cbSrc = -1);
char* StrToUtf8(const char* src, uint codePage);

char* UnknownToUtf8(const char*);

char* WstrToAnsi(const WCHAR*);
char* Utf8ToAnsi(const char*);

WCHAR* AnsiToWstr(const char* src, size_t cbLen = (size_t)-1);
char* AnsiToUtf8(const char* src, size_t cbLen = (size_t)-1);

} // namespace strconv

// shorter names
// TODO: eventually we want to migrate all strconv:: to them
char* ToUtf8(const WCHAR* s, size_t cch = (size_t)-1);
WCHAR* ToWstr(const char* s, size_t cb = (size_t)-1);
