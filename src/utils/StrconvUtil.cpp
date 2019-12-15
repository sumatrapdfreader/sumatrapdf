/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

namespace strconv {

// tries to convert a string in unknown encoding to utf8, as best
// as it can
// caller has to free() it
std::string_view UnknownToUtf8(const std::string_view& txt) {
    size_t len = txt.size();
    const char* s = txt.data();

    if (len < 3) {
        return str::DupN(s, len);
    }

    if (str::StartsWith(s, UTF8_BOM)) {
        return str::DupN(s + 3, len - 3);
    }

    // TODO: UTF16BE_BOM

    if (str::StartsWith(s, UTF16_BOM)) {
        s += 2;
        int cchLen = (int)((len - 2) / 2);
        return strconv::WstrToUtf8((const WCHAR*)s, cchLen);
    }

    // if s is valid utf8, leave it alone
    const u8* tmp = (const u8*)s;
    if (isLegalUTF8String(&tmp, tmp + len)) {
        return str::DupN(s, len);
    }

    AutoFreeWstr uni(strconv::FromAnsi(s, len));
    return strconv::WstrToUtf8(uni.Get());
}

size_t ToCodePageBuf(char* buf, int cbBufSize, const WCHAR* s, UINT cp) {
    return WideCharToMultiByte(cp, 0, s, -1, buf, cbBufSize, nullptr, nullptr);
}
size_t FromCodePageBuf(WCHAR* buf, int cchBufSize, const char* s, UINT cp) {
    return MultiByteToWideChar(cp, 0, s, -1, buf, cchBufSize);
}

WCHAR* FromCodePage(const char* src, UINT cp) {
    return str::ToWideChar(src, cp);
}

WCHAR* FromUtf8(const char* src) {
    return str::ToWideChar(src, CP_UTF8);
}

WCHAR* FromUtf8(const char* src, size_t cbSrcLen) {
    return str::ToWideChar(src, CP_UTF8, (int)cbSrcLen);
}

WCHAR* Utf8ToWchar(const char* src) {
    return str::ToWideChar(src, CP_UTF8);
}

WCHAR* Utf8ToWchar(const char* src, size_t cbSrcLen) {
    return str::ToWideChar(src, CP_UTF8, (int)cbSrcLen);
}

WCHAR* Utf8ToWchar(std::string_view sv) {
    return str::ToWideChar(sv.data(), CP_UTF8, (int)sv.size());
}

std::string_view WstrToUtf8(const WCHAR* src, size_t cchSrcLen) {
    return str::WstrToCodePage(src, CP_UTF8, (int)cchSrcLen);
}

std::string_view WstrToUtf8(const WCHAR* src) {
    return str::WstrToCodePage(src, CP_UTF8);
}

WCHAR* FromAnsi(const char* src, size_t cbSrcLen) {
    return str::ToWideChar(src, CP_ACP, (int)cbSrcLen);
}

std::string_view WstrToAnsi(const WCHAR* src) {
    return str::WstrToCodePage(src, CP_ACP);
}

// TODO: redundant with strconv::
std::string_view WstrToCodePage(const WCHAR* src, UINT cp) {
    return str::WstrToCodePage(src, cp);
}

} // namespace strconv
