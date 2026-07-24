/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

namespace strconv {

#if !OS_WIN
static bool IsSupportedCodePage(uint codePage) {
    return codePage == CP_UTF8 || codePage == CP_ACP || codePage == 20127;
}
#endif

#if OS_WIN
static WStr WrapAllocatedWStr(WCHAR* s, int n) {
    if (!s) {
        return {};
    }
    return WStr(s, n);
}

static Str WrapAllocatedStr(char* s, int n) {
    if (!s) {
        return {};
    }
    return Str(s, n);
}
#endif

WStr Utf8ToWStr(Str s, Arena* a) {
    // subtle: if s.s is nullptr, we return empty. if empty string => we return empty string
    if (str::IsNull(s)) {
        return {};
    }
#if OS_WIN
    if (s.len == 0) {
        WCHAR* res = AllocArray<WCHAR>(a, 1);
        return WrapAllocatedWStr(res, 0);
    }
    // ask for the size of buffer needed for converted string
    int cchNeeded = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, nullptr, 0);
    WCHAR* res = AllocArray<WCHAR>(a, cchNeeded + 1);
    if (!res) {
        return {};
    }
    int cchConverted = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, res, cchNeeded);
    ReportIf(cchConverted != cchNeeded);
    // TODO: not sure if invalid test or it's more subtle
    // triggers in Dune.epub
    // ReportIf(cchConverted != s.len);
    return WrapAllocatedWStr(res, cchConverted);
#else
    TempWStr res = ToWStrTemp(s);
    return wstr::Dup(a, res);
#endif
}

Str WStrToCodePage(uint codePage, WStr s, Arena* a) {
    // subtle: if s.s is nullptr, we return empty. if empty string => we return empty string
    if (wstr::IsNull(s)) {
        return {};
    }
#if OS_WIN
    if (s.len == 0) {
        char* res = AllocArray<char>(a, 1);
        return WrapAllocatedStr(res, 0);
    }
    // ask for the size of buffer needed for converted string
    int cbNeeded = WideCharToMultiByte(codePage, 0, s.s, s.len, nullptr, 0, nullptr, nullptr);
    if (cbNeeded == 0) {
        return {};
    }
    char* res = AllocArray<char>(a, cbNeeded + 1);
    if (!res) {
        return {};
    }
    int cbConverted = WideCharToMultiByte(codePage, 0, s.s, s.len, res, cbNeeded, nullptr, nullptr);
    ReportIf(cbConverted != cbNeeded);
    return WrapAllocatedStr(res, cbConverted);
#else
    if (!IsSupportedCodePage(codePage)) {
        return {};
    }
    TempStr res = ToUtf8Temp(s);
    return str::Dup(a, res);
#endif
}

Str WStrToUtf8(WStr s, Arena* a) {
    return WStrToCodePage(CP_UTF8, s, a);
}

// caller needs to free() the result
WStr StrCPToWStr(Str src, uint codePage) {
    ReportIf(str::IsNull(src));
    if (str::IsNull(src)) {
        return {};
    }

#if OS_WIN
    int requiredBufSize = MultiByteToWideChar(codePage, 0, src.s, src.len, nullptr, 0);
    if (0 == requiredBufSize) {
        return {};
    }
    WCHAR* res = AllocArray<WCHAR>(requiredBufSize + 1);
    if (!res) {
        return {};
    }
    MultiByteToWideChar(codePage, 0, src.s, src.len, res, requiredBufSize);
    return WrapAllocatedWStr(res, requiredBufSize);
#else
    if (!IsSupportedCodePage(codePage)) {
        return {};
    }
    TempWStr res = ToWStrTemp(src);
    return wstr::Dup(nullptr, res);
#endif
}

TempWStr StrCPToWStrTemp(Str src, uint codePage) {
    ReportIf(str::IsNull(src));
    if (str::IsNull(src)) {
        return {};
    }

#if OS_WIN
    int requiredBufSize = MultiByteToWideChar(codePage, 0, src.s, src.len, nullptr, 0);
    if (0 == requiredBufSize) {
        return {};
    }
    WCHAR* res = AllocArrayTemp<WCHAR>(requiredBufSize + 1);
    if (!res) {
        return {};
    }
    MultiByteToWideChar(codePage, 0, src.s, src.len, res, requiredBufSize);
    return WrapAllocatedWStr(res, requiredBufSize);
#else
    if (!IsSupportedCodePage(codePage)) {
        return {};
    }
    return ToWStrTemp(src);
#endif
}

TempStr ToMultiByteTemp(Str src, uint codePageSrc, uint codePageDest) {
    ReportIf(str::IsNull(src));
    if (str::IsNull(src)) {
        return {};
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
        return {};
    }
    Arena* a = GetTempArena();
    TempStr res = WStrToCodePage(codePageDest, tmp, a);
    return res;
}

TempStr StrToUtf8Temp(Str src, uint codePage) {
    return ToMultiByteTemp(src, codePage, CP_UTF8);
}

// tries to convert a string in unknown encoding to utf8, as best
// as it can
// caller has to free() it
TempStr UnknownToUtf8Temp(Str s) {
    if (s.len < 3) {
        return str::DupTemp(s);
    }

    if (str::StartsWith(s, Str(UTF8_BOM))) {
        return str::DupTemp(Str(s.s + 3, s.len - 3));
    }

    if (str::StartsWith(s, Str(UTF16_BOM))) {
        int bomOff = 2;
        int cch = (s.len - bomOff) / 2;
        return ToUtf8Temp(WStr((wchar_t*)(s.s + bomOff), cch));
    }

    if (str::StartsWith(s, Str(UTF16BE_BOM))) {
        // convert from utf16 big endian to utf16
        int bomOff = 2;
        int n = (s.len - bomOff) / 2;
        TempWStr tmpW = str::DupTemp(WStr((wchar_t*)(s.s + bomOff), n));
        u8* bytes = (u8*)tmpW.s;
        for (int i = 0; i < n; i++) {
            int idx = i * (int)sizeof(WCHAR);
            std::swap(bytes[idx], bytes[idx + 1]);
        }
        return ToUtf8Temp(WStr(tmpW.s, n));
    }

    // if s is valid utf8, leave it alone
    const u8* scan = (const u8*)s.s;
    const u8* end = scan + s.len;
    if (isLegalUTF8String(&scan, end)) {
        return str::DupTemp(s);
    }

    TempWStr ws = strconv::AnsiToWStrTemp(s);
    auto res = ToUtf8Temp(ws);
    return res;
}

TempWStr AnsiToWStrTemp(Str src) {
    return StrCPToWStrTemp(src, CP_ACP);
}

TempStr AnsiToUtf8Temp(Str src) {
    TempWStr ws = StrCPToWStrTemp(src, CP_ACP);
    TempStr res = ToUtf8Temp(ws);
    return res;
}

Str AnsiToUtf8(Str src) {
    TempWStr ws = StrCPToWStrTemp(src, CP_ACP);
    Str res = ToUtf8(ws);
    return res;
}

Str WStrToAnsi(WStr src) {
    return WStrToCodePage(CP_ACP, src);
}

Str Utf8ToAnsi(Str s) {
    TempWStr ws = ToWStrTemp(s);
    return WStrToAnsi(ws);
}

} // namespace strconv

// short names because frequently used
Str ToUtf8(WStr s, Arena* a) {
    return strconv::WStrToUtf8(s, a);
}

WStr ToWStr(Str s, Arena* a) {
    return strconv::Utf8ToWStr(s, a);
}
