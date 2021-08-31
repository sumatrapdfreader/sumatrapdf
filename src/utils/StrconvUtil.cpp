/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

namespace strconv {

std::wstring_view Utf8ToWstrV(const char* s, size_t cb, Allocator* a) {
    // subtle: if s is nullptr, we return nullptr. if empty string => we return empty string
    if (!s) {
        return {};
    }
    if (cb == (size_t)-1) {
        cb = str::Len(s);
    }
    if (cb == 0) {
        return {(const WCHAR*)Allocator::AllocZero(a, sizeof(WCHAR)), 0};
    }
    // ask for the size of buffer needed for converted string
    int cchNeeded = MultiByteToWideChar(CP_UTF8, 0, s, (int)cb, nullptr, 0);
    size_t cbAlloc = ((size_t)cchNeeded * sizeof(WCHAR)) + sizeof(WCHAR); // +1 for terminating 0
    WCHAR* res = (WCHAR*)Allocator::AllocZero(a, cbAlloc);
    if (!res) {
        return {nullptr, 0};
    }
    int cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int)cb, res, (int)cchNeeded);
    ReportIf(cchConverted != cchNeeded);
    // TODO: not sure if invalid test or it's more subtle
    // triggers in Dune.epub
    // ReportIf((size_t)cchConverted != str::Len(res));
    return {res, (size_t)cchConverted};
}

std::wstring_view Utf8ToWstrV(std::string_view sv, Allocator* a) {
    return Utf8ToWstrV(sv.data(), sv.size(), a);
}

WCHAR* Utf8ToWstr(const char* s, size_t cb, Allocator* a) {
    auto v = Utf8ToWstrV(s, cb, a);
    return (WCHAR*)v.data();
}

WCHAR* Utf8ToWstr(std::string_view sv) {
    return Utf8ToWstr(sv.data(), sv.size(), nullptr);
}

std::string_view WstrToCodePageV(uint codePage, const WCHAR* s, size_t cch, Allocator* a) {
    // subtle: if s is nullptr, we return nullptr. if empty string => we return empty string
    if (!s) {
        return {};
    }
    if (cch == (size_t)-1) {
        cch = str::Len(s);
    }
    if (cch == 0) {
        return {(const char*)Allocator::AllocZero(a, sizeof(char)), 0};
    }
    // ask for the size of buffer needed for converted string
    int cbNeeded = WideCharToMultiByte(codePage, 0, s, (int)cch, nullptr, 0, nullptr, nullptr);
    if (cbNeeded == 0) {
        return {};
    }
    size_t cbAlloc = cbNeeded + sizeof(char); // +1 for terminating 0
    char* res = (char*)Allocator::AllocZero(a, cbAlloc);
    if (!res) {
        return {nullptr, 0};
    }
    int cbConverted = WideCharToMultiByte(codePage, 0, s, (int)cch, res, cbNeeded, nullptr, nullptr);
    ReportIf(cbConverted != cbNeeded);
    ReportIf((size_t)cbConverted != str::Len(res));
    return {res, (size_t)cbConverted};
}

std::string_view WstrToUtf8V(const WCHAR* s, size_t cch, Allocator* a) {
    return WstrToCodePageV(CP_UTF8, s, cch, a);
}

std::string_view WstrToUtf8V(std::wstring_view sv, Allocator* a) {
    return WstrToCodePageV(CP_UTF8, sv.data(), sv.size(), a);
}

char* WstrToCodePage(uint codePage, const WCHAR* s, size_t cch, Allocator* a) {
    auto v = WstrToCodePageV(codePage, s, cch, a);
    return (char*)v.data();
}

char* WstrToUtf8(const WCHAR* s, size_t cch, Allocator* a) {
    auto v = WstrToUtf8V(s, cch, a);
    return (char*)v.data();
}

char* WstrToUtf8(std::wstring_view sv, Allocator* a) {
    auto v = WstrToUtf8V(sv.data(), sv.size(), a);
    return (char*)v.data();
}

// caller needs to free() the result
WCHAR* StrToWstr(const char* src, uint codePage, int cbSrc) {
    CrashIf(!src);
    if (!src) {
        return nullptr;
    }

    int requiredBufSize = MultiByteToWideChar(codePage, 0, src, cbSrc, nullptr, 0);
    if (0 == requiredBufSize) {
        return nullptr;
    }
    WCHAR* res = AllocArray<WCHAR>((size_t)requiredBufSize + 1);
    if (!res) {
        return nullptr;
    }
    MultiByteToWideChar(codePage, 0, src, cbSrc, res, requiredBufSize);
    return res;
}

// caller needs to free() the result
std::string_view ToMultiByteV(const char* src, uint codePageSrc, uint codePageDest) {
    CrashIf(!src);
    if (!src) {
        return {};
    }

    if (codePageSrc == codePageDest) {
        return std::string_view(str::Dup(src));
    }

    // 20127 is US-ASCII, which by definition is valid CP_UTF8
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
    // don't know what is CP_* name for it (if it exists)
    if ((codePageSrc == 20127) && (codePageDest == CP_UTF8)) {
        return std::string_view(str::Dup(src));
    }

    AutoFreeWstr tmp(StrToWstr(src, codePageSrc));
    if (!tmp) {
        return {};
    }

    return WstrToCodePageV(codePageDest, tmp.Get(), tmp.size());
}

// tries to convert a string in unknown encoding to utf8, as best
// as it can
// caller has to free() it
std::string_view UnknownToUtf8V(const std::string_view& txt) {
    size_t len = txt.size();
    const char* s = txt.data();

    if (len < 3) {
        return str::Dup(s, len);
    }

    if (str::StartsWith(s, UTF8_BOM)) {
        return str::Dup(s + 3, len - 3);
    }

    // TODO: UTF16BE_BOM

    if (str::StartsWith(s, UTF16_BOM)) {
        s += 2;
        int cch = (int)((len - 2) / 2);
        return strconv::WstrToUtf8V((const WCHAR*)s, cch);
    }

    // if s is valid utf8, leave it alone
    const u8* tmp = (const u8*)s;
    if (isLegalUTF8String(&tmp, tmp + len)) {
        return str::Dup(s, len);
    }

    AutoFreeWstr uni = strconv::AnsiToWstr(s, len);
    return strconv::WstrToUtf8V(uni.Get());
}

WCHAR* AnsiToWstr(const char* src, size_t cbLen) {
    return StrToWstr(src, CP_ACP, (int)cbLen);
}

std::string_view WstrToAnsiV(const WCHAR* src) {
    return WstrToCodePageV(CP_ACP, src);
}

} // namespace strconv
