/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace strconv {

// TODO: remove use of this and replace with the improved versions below
size_t Utf8ToWcharBuf(const char* s, size_t sLen, WCHAR* bufOut, size_t cchBufOut);
size_t WstrToUtf8Buf(const WCHAR* s, char* bufOut, size_t cbBufOut);

WCHAR* Utf8ToWcharBuf(const char* s, size_t cb, WCHAR* bufOut, size_t* cchBufInOut);
char* WstrToUtf8Buf(const WCHAR* s, size_t cch, char* bufOut, size_t* cbBufInOut);

std::string_view ToMultiByte(const char* src, uint codePageSrc, uint codePageDest);
WCHAR* ToWideChar(const char* src, uint codePage, int cbSrc = -1);

std::string_view UnknownToUtf8(const std::string_view&);

WCHAR* FromCodePage(const char* src, uint cp);

WCHAR* Utf8ToWstr(std::string_view sv);

std::string_view WstrToUtf8(const WCHAR* src, size_t cch = (size_t)-1);
std::string_view WstrToUtf8(std::wstring_view);

std::string_view WstrToCodePage(const WCHAR* txt, uint codePage, int cchTxt = -1);
std::string_view WstrToAnsi(const WCHAR*);

WCHAR* FromAnsi(const char* src, size_t cbSrc = (size_t)-1);
size_t ToCodePageBuf(char* buf, int cbBuf, const WCHAR* s, uint cp);
size_t FromCodePageBuf(WCHAR* buf, int cchBuf, const char* s, uint cp);

struct StackWstrToUtf8 {
    char buf[128];
    char* overflow{nullptr};
    size_t cbConverted{(size_t)-1};

    StackWstrToUtf8(std::wstring_view);
    StackWstrToUtf8(const WCHAR*, size_t cch = (size_t)-1);
    StackWstrToUtf8& operator=(const StackWstrToUtf8&) = delete;
    ~StackWstrToUtf8();
    char* Get() const;
    size_t size() const;
    operator char*() const;
    std::string_view AsView() const;
};

struct StackUtf8ToWstr {
    WCHAR buf[128];
    WCHAR* overflow{nullptr};
    size_t cchConverted{(size_t)-1};

    StackUtf8ToWstr(std::string_view);
    StackUtf8ToWstr(const char*, size_t cb = (size_t)-1);
    StackUtf8ToWstr& operator=(const StackUtf8ToWstr&) = delete;
    ~StackUtf8ToWstr();
    WCHAR* Get() const;
    size_t size() const;
    operator WCHAR*() const;
    std::wstring_view AsView() const;
};

} // namespace strconv
