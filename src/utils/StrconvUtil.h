/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace strconv {

std::string_view UnknownToUtf8(const std::string_view&);

#if OS_WIN
WCHAR* FromCodePage(const char* src, UINT cp);
std::string_view WstrToCodePage(const WCHAR* src, UINT cp);

// TODO: replace with Utf8ToWchar
WCHAR* FromUtf8(const char* src);
WCHAR* FromUtf8(const char* src, size_t cbSrcLen);

WCHAR* Utf8ToWchar(const char* src);
WCHAR* Utf8ToWchar(const char* src, size_t cbSrcLen);
WCHAR* Utf8ToWchar(std::string_view sv);

std::string_view WstrToUtf8(const WCHAR* src);

std::string_view WstrToUtf8(const WCHAR* src, size_t cchSrcLen);
std::string_view WstrToUtf8(const WCHAR* src);
WCHAR* FromAnsi(const char* src, size_t cbSrcLen = (size_t)-1);
std::string_view WstrToAnsi(const WCHAR*);
size_t ToCodePageBuf(char* buf, int cbBufSize, const WCHAR* s, UINT cp);
size_t FromCodePageBuf(WCHAR* buf, int cchBufSize, const char* s, UINT cp);

#endif

} // namespace strconv
