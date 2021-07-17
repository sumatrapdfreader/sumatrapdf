/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace strconv {

std::wstring_view Utf8ToWstrV(const char* s, size_t cb = (size_t)-1, Allocator* a = nullptr);
std::wstring_view Utf8ToWstrV(std::string_view sv, Allocator* a = nullptr);
WCHAR* Utf8ToWstr(const char* s, size_t cb = (size_t)-1, Allocator* a = nullptr);
WCHAR* Utf8ToWstr(std::string_view sv);

std::string_view WstrToCodePageV(uint codePage, const WCHAR* s, size_t cch = (size_t)-1, Allocator* a = nullptr);
std::string_view WstrToUtf8V(const WCHAR* s, size_t cch = (size_t)-1, Allocator* a = nullptr);
std::string_view WstrToUtf8V(std::wstring_view, Allocator* a = nullptr);
char* WstrToCodePage(uint codePage, const WCHAR* s, size_t cch = (size_t)-1, Allocator* a = nullptr);
char* WstrToUtf8(const WCHAR* s, size_t cch = (size_t)-1, Allocator* a = nullptr);
char* WstrToUtf8(std::wstring_view sv, Allocator* a = nullptr);

std::string_view ToMultiByteV(const char* src, uint codePageSrc, uint codePageDest);
WCHAR* StrToWstr(const char* src, uint codePage, int cbSrc = -1);

std::string_view UnknownToUtf8V(const std::string_view&);

std::string_view WstrToAnsiV(const WCHAR*);

WCHAR* AnsiToWstr(const char* src, size_t cbSrc = (size_t)-1);

} // namespace strconv
