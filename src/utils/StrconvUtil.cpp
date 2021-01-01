/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

namespace strconv {

size_t Utf8ToWcharBuf(const char* s, size_t cbLen, WCHAR* bufOut, size_t cchBufOutSize) {
    CrashIf(!bufOut || (0 == cchBufOutSize));
    int cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int)cbLen, bufOut, (int)cchBufOutSize);
    if (0 == cchConverted) {
        // TODO: determine ideal string length so that the conversion succeeds
        cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int)cchBufOutSize / 2, bufOut, (int)cchBufOutSize);
    } else if ((size_t)cchConverted >= cchBufOutSize) {
        cchConverted = (int)cchBufOutSize - 1;
    }
    bufOut[cchConverted] = '\0';
    return cchConverted;
}

size_t WcharToUtf8Buf(const WCHAR* s, char* bufOut, size_t cbBufOutSize) {
    CrashIf(!bufOut || (0 == cbBufOutSize));
    int cbConverted = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if ((size_t)cbConverted >= cbBufOutSize) {
        cbConverted = (int)cbBufOutSize - 1;
    }
    int res = WideCharToMultiByte(CP_UTF8, 0, s, (int)str::Len(s), bufOut, cbConverted, nullptr, nullptr);
    CrashIf(res > cbConverted);
    bufOut[res] = '\0';
    return res;
}

std::string_view WstrToCodePage(const WCHAR* txt, uint codePage, int cchTxtLen) {
    CrashIf(!txt);
    if (!txt) {
        return {};
    }

    int bufSize = WideCharToMultiByte(codePage, 0, txt, cchTxtLen, nullptr, 0, nullptr, nullptr);
    if (0 == bufSize) {
        return {};
    }
    char* res = AllocArray<char>(size_t(bufSize) + 1); // +1 for terminating 0
    if (!res) {
        return {};
    }
    WideCharToMultiByte(codePage, 0, txt, cchTxtLen, res, bufSize, nullptr, nullptr);
    size_t resLen = str::Len(res);
    return {res, resLen};
}

/* Caller needs to free() the result */
WCHAR* ToWideChar(const char* src, uint codePage, int cbSrcLen) {
    CrashIf(!src);
    if (!src) {
        return nullptr;
    }

    int requiredBufSize = MultiByteToWideChar(codePage, 0, src, cbSrcLen, nullptr, 0);
    if (0 == requiredBufSize) {
        return str::Dup(L"");
    }
    WCHAR* res = AllocArray<WCHAR>(requiredBufSize + 1);
    if (!res) {
        return nullptr;
    }
    MultiByteToWideChar(codePage, 0, src, cbSrcLen, res, requiredBufSize);
    return res;
}

std::string_view ToMultiByte(const char* src, uint codePageSrc, uint codePageDest) {
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

    AutoFreeWstr tmp(ToWideChar(src, codePageSrc));
    if (!tmp) {
        return {};
    }

    return WstrToCodePage(tmp.Get(), codePageDest);
}

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

    AutoFreeWstr uni = strconv::FromAnsi(s, len);
    return strconv::WstrToUtf8(uni.Get());
}

size_t ToCodePageBuf(char* buf, int cbBufSize, const WCHAR* s, uint cp) {
    return WideCharToMultiByte(cp, 0, s, -1, buf, cbBufSize, nullptr, nullptr);
}
size_t FromCodePageBuf(WCHAR* buf, int cchBufSize, const char* s, uint cp) {
    return MultiByteToWideChar(cp, 0, s, -1, buf, cchBufSize);
}

WCHAR* FromCodePage(const char* src, uint cp) {
    return ToWideChar(src, cp);
}

WCHAR* Utf8ToWstr(std::string_view sv) {
    return ToWideChar(sv.data(), CP_UTF8, (int)sv.size());
}

std::string_view WstrToUtf8(const WCHAR* src, size_t cchSrcLen) {
    return WstrToCodePage(src, CP_UTF8, (int)cchSrcLen);
}

std::string_view WstrToUtf8(std::wstring_view sv) {
    return WstrToCodePage(sv.data(), CP_UTF8, (int)sv.size());
}

WCHAR* FromAnsi(const char* src, size_t cbSrcLen) {
    return ToWideChar(src, CP_ACP, (int)cbSrcLen);
}

std::string_view WstrToAnsi(const WCHAR* src) {
    return WstrToCodePage(src, CP_ACP);
}

static void Set(StackWstrToUtf8* o, const WCHAR* s, int cch) {
    o->buf[0] = 0;
    if (!s || cch == 0) {
        return;
    }

    int cbBufSize = (int)sizeof(o->buf) - 1; // -1 for terminating zero
    int res = WideCharToMultiByte(CP_UTF8, 0, s, cch, o->buf, cbBufSize, nullptr, nullptr);
    if (res > 0) {
        o->buf[res] = 0;
        return;
    }

    // the buffer wasn't big enough, so measure how much we need and allocate
    int cbNeeded = WideCharToMultiByte(CP_UTF8, 0, s, cch, nullptr, 0, nullptr, nullptr);
    o->overflow = AllocArray<char>((size_t)cbNeeded + 1); // +1 for terminating 0
    if (!o->overflow) {
        return;
    }
    res = WideCharToMultiByte(CP_UTF8, 0, s, cch, o->overflow, cbNeeded, nullptr, nullptr);
    CrashIf(res != cbNeeded);
}

StackWstrToUtf8::StackWstrToUtf8(const WCHAR* s) {
    int n = (int)str::Len(s);
    Set(this, s, n);
}

StackWstrToUtf8::StackWstrToUtf8(std::wstring_view sv) {
    int cch = (int)sv.size();
    Set(this, sv.data(), cch);
}

char* StackWstrToUtf8::Get() const {
    if (overflow) {
        return overflow;
    }
    return (char*)buf;
}

#if 0
StackWstrToUtf8& StackWstrToUtf8::operator=(const StackWstrToUtf8& other) {
    if (this == &other) {
        return *this;
    }
    str::Free(overflow);
    overflow = nullptr;
    memcpy(this->buf, other.buf, sizeof(this->buf));
    this->overflow = str::Dup(other.overflow);
    return *this;
}
#endif

StackWstrToUtf8::operator char*() const {
    return Get();
}

StackWstrToUtf8::~StackWstrToUtf8() {
    str::Free(overflow);
}

} // namespace strconv
