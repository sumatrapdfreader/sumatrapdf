/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

namespace strconv {

WCHAR* Utf8ToWStr(const char* s, size_t cb, Allocator* a) {
    // subtle: if s is nullptr, we return nullptr. if empty string => we return empty string
    if (!s) {
        return nullptr;
    }
    if (cb == (size_t)-1) {
        cb = str::Len(s);
    }
    if (cb == 0) {
        return Allocator::AllocArray<WCHAR>(a, 1);
    }
    // ask for the size of buffer needed for converted string
    int cchNeeded = MultiByteToWideChar(CP_UTF8, 0, s, (int)cb, nullptr, 0);
    WCHAR* res = Allocator::AllocArray<WCHAR>(a, cchNeeded + 1); // +1 for terminating 0
    if (!res) {
        return nullptr;
    }
    int cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int)cb, res, (int)cchNeeded);
    ReportIf(cchConverted != cchNeeded);
    // TODO: not sure if invalid test or it's more subtle
    // triggers in Dune.epub
    // ReportIf((size_t)cchConverted != str::Len(res));
    return res;
}

char* WStrToCodePage(uint codePage, const WCHAR* s, size_t cch, Allocator* a) {
    // subtle: if s is nullptr, we return nullptr. if empty string => we return empty string
    if (!s) {
        return nullptr;
    }
    if (cch == (size_t)-1) {
        cch = str::Len(s);
    }
    if (cch == 0) {
        return Allocator::AllocArray<char>(a, 1);
    }
    // ask for the size of buffer needed for converted string
    int cbNeeded = WideCharToMultiByte(codePage, 0, s, (int)cch, nullptr, 0, nullptr, nullptr);
    if (cbNeeded == 0) {
        return nullptr;
    }
    char* res = Allocator::AllocArray<char>(a, cbNeeded + 1); // +1 for terminating 0
    if (!res) {
        return nullptr;
    }
    int cbConverted = WideCharToMultiByte(codePage, 0, s, (int)cch, res, cbNeeded, nullptr, nullptr);
    ReportIf(cbConverted != cbNeeded);
    ReportIf((size_t)cbConverted != str::Len(res));
    return res;
}

char* WStrToUtf8(const WCHAR* s, size_t cch, Allocator* a) {
    return WStrToCodePage(CP_UTF8, s, cch, a);
}

// caller needs to free() the result
WCHAR* StrCPToWStr(const char* src, uint codePage, int cbSrc) {
    ReportIf(!src);
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

TempWStr StrCPToWStrTemp(const char* src, uint codePage, int cbSrc) {
    ReportIf(!src);
    if (!src) {
        return nullptr;
    }

    int requiredBufSize = MultiByteToWideChar(codePage, 0, src, cbSrc, nullptr, 0);
    if (0 == requiredBufSize) {
        return nullptr;
    }
    WCHAR* res = AllocArrayTemp<WCHAR>((size_t)requiredBufSize + 1);
    if (!res) {
        return nullptr;
    }
    MultiByteToWideChar(codePage, 0, src, cbSrc, res, requiredBufSize);
    return res;
}

TempStr ToMultiByteTemp(const char* src, uint codePageSrc, uint codePageDest) {
    ReportIf(!src);
    if (!src) {
        return nullptr;
    }

    if (codePageSrc == codePageDest) {
        return str::DupTemp(src);
    }

    // 20127 is US-ASCII, which by definition is valid CP_UTF8
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
    // don't know what is CP_* name for it (if it exists)
    if ((codePageSrc == 20127) && (codePageDest == CP_UTF8)) {
        return str::DupTemp(src);
    }

    TempWStr tmp = StrCPToWStrTemp(src, codePageSrc);
    if (!tmp) {
        return nullptr;
    }
    size_t tmpLen = str::Len(tmp);
    Allocator* a = GetTempAllocator();
    TempStr res = (TempStr)WStrToCodePage(codePageDest, tmp, tmpLen, a);
    return res;
}

TempStr StrToUtf8Temp(const char* src, uint codePage) {
    return ToMultiByteTemp(src, codePage, CP_UTF8);
}

// tries to convert a string in unknown encoding to utf8, as best
// as it can
// caller has to free() it
char* UnknownToUtf8Temp(const char* s) {
    size_t len = str::Len(s);

    if (len < 3) {
        return str::DupTemp(s, len);
    }

    if (str::StartsWith(s, UTF8_BOM)) {
        return str::DupTemp(s + 3, len - 3);
    }

    if (str::StartsWith(s, UTF16_BOM)) {
        s += 2;
        int cch = (int)((len - 2) / 2);
        // codeql complains about char* => WCHAR* cast
        void* d = (void*)s;
        return ToUtf8Temp((const WCHAR*)d, cch);
    }

    if (str::StartsWith(s, UTF16BE_BOM)) {
        // convert from utf16 big endian to utf16
        s += 2;
        WCHAR* ws = str::ToWCHAR(s);
        int n = str::Leni(ws);
        char* tmp = (char*)s;
        for (int i = 0; i < n; i++) {
            int idx = i * 2;
            std::swap(tmp[idx], tmp[idx + 1]);
        }
        return ToUtf8Temp(ws);
    }

    // if s is valid utf8, leave it alone
    const u8* tmp = (const u8*)s;
    if (isLegalUTF8String(&tmp, tmp + len)) {
        return str::DupTemp(s, len);
    }

    TempWStr ws = strconv::AnsiToWStrTemp(s, len);
    auto res = ToUtf8Temp(ws);
    return res;
}

TempWStr AnsiToWStrTemp(const char* src, size_t cbLen) {
    return StrCPToWStrTemp(src, CP_ACP, (int)cbLen);
}

char* AnsiToUtf8(const char* src, size_t cbLen) {
    TempWStr ws = StrCPToWStrTemp(src, CP_ACP, (int)cbLen);
    char* res = ToUtf8(ws);
    return res;
}

char* WStrToAnsi(const WCHAR* src) {
    return WStrToCodePage(CP_ACP, src);
}

char* Utf8ToAnsi(const char* s) {
    TempWStr ws = ToWStrTemp(s);
    return WStrToAnsi(ws);
}

} // namespace strconv

// short names because frequently used
char* ToUtf8(const WCHAR* s, size_t cch) {
    return strconv::WStrToUtf8(s, cch);
}

WCHAR* ToWStr(const char* s, size_t cb) {
    return strconv::Utf8ToWStr(s, cb);
}
