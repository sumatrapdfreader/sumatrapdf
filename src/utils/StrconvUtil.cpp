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
WCHAR* StrToWStr(const char* src, uint codePage, int cbSrc) {
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

    WCHAR* tmp = StrToWStr(src, codePageSrc);
    if (!tmp) {
        return nullptr;
    }
    size_t tmpLen = str::Len(tmp);
    Allocator* a = GetTempAllocator();
    TempStr res = (TempStr)WStrToCodePage(codePageDest, tmp, tmpLen, a);
    str::Free(tmp);
    return res;
}

TempStr StrToUtf8Temp(const char* src, uint codePage) {
    return ToMultiByteTemp(src, codePage, CP_UTF8);
}

// tries to convert a string in unknown encoding to utf8, as best
// as it can
// caller has to free() it
char* UnknownToUtf8(const char* s) {
    size_t len = str::Len(s);

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
        return ToUtf8((const WCHAR*)s, cch);
    }

    // if s is valid utf8, leave it alone
    const u8* tmp = (const u8*)s;
    if (isLegalUTF8String(&tmp, tmp + len)) {
        return str::Dup(s, len);
    }

    AutoFreeWStr uni = strconv::AnsiToWStr(s, len);
    return ToUtf8(uni.Get());
}

WCHAR* AnsiToWStr(const char* src, size_t cbLen) {
    return StrToWStr(src, CP_ACP, (int)cbLen);
}

char* AnsiToUtf8(const char* src, size_t cbLen) {
    WCHAR* ws = StrToWStr(src, CP_ACP, (int)cbLen);
    char* res = ToUtf8(ws);
    str::Free(ws);
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
