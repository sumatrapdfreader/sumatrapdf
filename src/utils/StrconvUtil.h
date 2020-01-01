/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace strconv {

size_t Utf8ToWcharBuf(const char* s, size_t sLen, WCHAR* bufOut, size_t cchBufOutSize);
size_t WcharToUtf8Buf(const WCHAR* s, char* bufOut, size_t cbBufOutSize);
std::string_view ToMultiByte(const char* src, UINT CodePageSrc, UINT CodePageDest);
WCHAR* ToWideChar(const char* src, UINT CodePage, int cbSrcLen = -1);

std::string_view UnknownToUtf8(const std::string_view&);

WCHAR* FromCodePage(const char* src, UINT cp);

WCHAR* Utf8ToWstr(std::string_view sv);

std::string_view WstrToUtf8(const WCHAR* src, size_t cchSrcLen = -1);

std::string_view WstrToCodePage(const WCHAR* txt, UINT codePage, int cchTxtLen = -1);
std::string_view WstrToAnsi(const WCHAR*);

WCHAR* FromAnsi(const char* src, size_t cbSrcLen = (size_t)-1);
size_t ToCodePageBuf(char* buf, int cbBufSize, const WCHAR* s, UINT cp);
size_t FromCodePageBuf(WCHAR* buf, int cchBufSize, const char* s, UINT cp);

} // namespace strconv
