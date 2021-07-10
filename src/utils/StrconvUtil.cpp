/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

namespace strconv {

size_t Utf8ToWcharBuf(const char* s, size_t cbLen, WCHAR* bufOut, size_t cchBufOut) {
    CrashIf(!bufOut || (0 == cchBufOut));
    int cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int)cbLen, bufOut, (int)cchBufOut);
    if (0 == cchConverted) {
        // TODO: determine ideal string length so that the conversion succeeds
        cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int)cchBufOut / 2, bufOut, (int)cchBufOut);
    } else if ((size_t)cchConverted >= cchBufOut) {
        cchConverted = (int)cchBufOut - 1;
    }
    bufOut[cchConverted] = '\0';
    return cchConverted;
}

size_t WstrToUtf8Buf(const WCHAR* s, char* bufOut, size_t cbBufOut) {
    CrashIf(!bufOut || (0 == cbBufOut));
    int cbConverted = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if ((size_t)cbConverted >= cbBufOut) {
        cbConverted = (int)cbBufOut - 1;
    }
    int res = WideCharToMultiByte(CP_UTF8, 0, s, (int)str::Len(s), bufOut, cbConverted, nullptr, nullptr);
    CrashIf(res > cbConverted);
    bufOut[res] = '\0';
    return res;
}

// a bit tricky: converts s form utf8 to unicode in provided buffer.
// if buffer is not big enough, will allocate a buffer
// It returns number of characters of converted string in cchBufInOut
// if cb is -1 we will str::Len(s)
//
// the caller must free if != bufOut
WCHAR* Utf8ToWcharBuf(const char* s, size_t cb, WCHAR* bufOut, size_t* cchBufInOut) {
    WCHAR* overflow{nullptr};
    bufOut[0] = 0;
    size_t cchBuf = *cchBufInOut - 1; // -1 for terminating zero
    *cchBufInOut = 0;
    // nuance: nullptr returns nullptr but empty string returns also empty string
    if (!s) {
        CrashIf(*cchBufInOut != 0);
        return nullptr;
    }
    if (cb == 0) {
        return bufOut;
    }
    if (cb == (size_t)-1) {
        cb = str::Len(s);
    }
    CrashIf((int)cb < 0);

    int cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int)cb, bufOut, (int)cchBuf);
    if (cchConverted > 0) {
        // did convert
        bufOut[cchConverted] = 0;
        *cchBufInOut = (size_t)cchConverted;
        // TODO: change to DebugCrashIf() because expensive
        CrashIf(*cchBufInOut != str::Len(bufOut));
        return bufOut;
    }

    // the buffer wasn't big enough, so measure how much we need and allocate
    int cchNeeded = MultiByteToWideChar(CP_UTF8, 0, s, (int)cb, nullptr, 0);
    overflow = AllocArray<WCHAR>((size_t)cchNeeded + 1); // +1 for terminating 0
    if (!overflow) {
        return nullptr;
    }
    cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int)cb, overflow, (int)cchNeeded);
    CrashIf(cchConverted != cchNeeded);
    // TODO: change to DebugCrashIf() because expensive
    CrashIf((size_t)cchConverted != str::Len(overflow));
    *cchBufInOut = (size_t)cchConverted;
    return overflow;
}

// a bit tricky: converts s form unicode to utf8 in provided buffer.
// if buffer is not big enough, will allocate a buffer
// It returns number of characters of converted string in cchBufInOut
// if cch is -1 we will str::Len(s)
//
// the caller must free if != bufOut
char* WstrToUtf8Buf(const WCHAR* s, size_t cch, char* bufOut, size_t* cbBufInOut) {
    char* overflow{nullptr};
    bufOut[0] = 0;
    size_t cbBuf = *cbBufInOut - 1; // -1 for terminating zero
    *cbBufInOut = 0;
    // nuance: nullptr returns nullptr but empty string returns also empty string
    if (!s) {
        CrashIf(*cbBufInOut != 0);
        return nullptr;
    }
    if (cch == 0) {
        return bufOut;
    }
    if (cch == (size_t)-1) {
        cch = str::Len(s);
    }
    CrashIf((int)cch < 0);

    int cbConverted = WideCharToMultiByte(CP_UTF8, 0, s, cch, bufOut, (int)cbBuf, nullptr, nullptr);
    if (cbConverted > 0) {
        // did convert
        bufOut[cbConverted] = 0;
        // TODO: change to DebugCrashIf() because expensive
        CrashIf((size_t)cbConverted != str::Len(bufOut));
        *cbBufInOut = (size_t)cbConverted;
        return bufOut;
    }

    // the buffer wasn't big enough, so measure how much we need and allocate
    int cbNeeded = WideCharToMultiByte(CP_UTF8, 0, s, cch, nullptr, 0, nullptr, nullptr);
    overflow = AllocArray<char>((size_t)cbNeeded + 1); // +1 for terminating 0
    if (!overflow) {
        return nullptr;
    }
    cbConverted = WideCharToMultiByte(CP_UTF8, 0, s, cch, overflow, cbNeeded, nullptr, nullptr);
    CrashIf(cbConverted != cbNeeded);
    // TODO: change to DebugCrashIf() because expensive
    CrashIf((size_t)cbConverted != str::Len(overflow));
    *cbBufInOut = (size_t)cbNeeded;
    return overflow;
}

// TODO: write WstrToCodePageBuf, rewrite WstrToUtf8Buf using it, optimize this
// by using WstrToCodePageBuf (avoid one call WideCharToMultiByte in common case
// where the string can be converted with a buffer)
std::string_view WstrToCodePage(const WCHAR* txt, uint codePage, int cchTxt) {
    CrashIf(!txt);
    if (!txt) {
        return {};
    }

    int bufSize = WideCharToMultiByte(codePage, 0, txt, cchTxt, nullptr, 0, nullptr, nullptr);
    if (0 == bufSize) {
        return {};
    }
    char* res = AllocArray<char>(size_t(bufSize) + 1); // +1 for terminating 0
    if (!res) {
        return {};
    }
    WideCharToMultiByte(codePage, 0, txt, cchTxt, res, bufSize, nullptr, nullptr);
    size_t resLen = str::Len(res);
    return {res, resLen};
}

/* Caller needs to free() the result */
WCHAR* ToWideChar(const char* src, uint codePage, int cbSrc) {
    CrashIf(!src);
    if (!src) {
        return nullptr;
    }

    int requiredBufSize = MultiByteToWideChar(codePage, 0, src, cbSrc, nullptr, 0);
    if (0 == requiredBufSize) {
        return str::Dup(L"");
    }
    WCHAR* res = AllocArray<WCHAR>(requiredBufSize + 1);
    if (!res) {
        return nullptr;
    }
    MultiByteToWideChar(codePage, 0, src, cbSrc, res, requiredBufSize);
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
        return str::Dup(s, len);
    }

    if (str::StartsWith(s, UTF8_BOM)) {
        return str::Dup(s + 3, len - 3);
    }

    // TODO: UTF16BE_BOM

    if (str::StartsWith(s, UTF16_BOM)) {
        s += 2;
        int cch = (int)((len - 2) / 2);
        return strconv::WstrToUtf8((const WCHAR*)s, cch);
    }

    // if s is valid utf8, leave it alone
    const u8* tmp = (const u8*)s;
    if (isLegalUTF8String(&tmp, tmp + len)) {
        return str::Dup(s, len);
    }

    AutoFreeWstr uni = strconv::FromAnsi(s, len);
    return strconv::WstrToUtf8(uni.Get());
}

size_t ToCodePageBuf(char* buf, int cbBuf, const WCHAR* s, uint cp) {
    return WideCharToMultiByte(cp, 0, s, -1, buf, cbBuf, nullptr, nullptr);
}
size_t FromCodePageBuf(WCHAR* buf, int cchBuf, const char* s, uint cp) {
    return MultiByteToWideChar(cp, 0, s, -1, buf, cchBuf);
}

WCHAR* FromCodePage(const char* src, uint cp) {
    return ToWideChar(src, cp);
}

WCHAR* Utf8ToWstr(std::string_view sv) {
    return ToWideChar(sv.data(), CP_UTF8, (int)sv.size());
}

std::string_view WstrToUtf8(const WCHAR* src, size_t cch) {
    return WstrToCodePage(src, CP_UTF8, (int)cch);
}

std::string_view WstrToUtf8(std::wstring_view sv) {
    return WstrToCodePage(sv.data(), CP_UTF8, (int)sv.size());
}

WCHAR* FromAnsi(const char* src, size_t cbLen) {
    return ToWideChar(src, CP_ACP, (int)cbLen);
}

std::string_view WstrToAnsi(const WCHAR* src) {
    return WstrToCodePage(src, CP_ACP);
}

StackWstrToUtf8::StackWstrToUtf8(const WCHAR* s, size_t cch) {
    cbConverted = dimof(buf);
    overflow = WstrToUtf8Buf(s, cch, buf, &cbConverted);
}

StackWstrToUtf8::StackWstrToUtf8(std::wstring_view sv) {
    int cch = (int)sv.size();
    cbConverted = dimof(buf);
    overflow = WstrToUtf8Buf(sv.data(), cch, buf, &cbConverted);
}

size_t StackWstrToUtf8::size() const {
    CrashIf((int)cbConverted < 0);
    return cbConverted;
}

char* StackWstrToUtf8::Get() const {
    CrashIf((int)cbConverted < 0);
    if (overflow) {
        return overflow;
    }
    return (char*)buf;
}

std::string_view StackWstrToUtf8::AsView() const {
    char* d = Get();
    return {d, cbConverted};
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
    if (overflow != buf) {
        str::Free(overflow);
    }
}

StackUtf8ToWstr::StackUtf8ToWstr(const char* s, size_t cb) {
    cchConverted = dimof(buf);
    overflow = Utf8ToWcharBuf(s, cb, buf, &cchConverted);
}

StackUtf8ToWstr::StackUtf8ToWstr(std::string_view sv) {
    int cb = (int)sv.size();
    cchConverted = dimof(buf);
    overflow = Utf8ToWcharBuf(sv.data(), cb, buf, &cchConverted);
}

size_t StackUtf8ToWstr::size() const {
    CrashIf((int)cchConverted < 0);
    return cchConverted;
}

WCHAR* StackUtf8ToWstr::Get() const {
    CrashIf((int)cchConverted < 0);
    if (overflow) {
        return overflow;
    }
    return (WCHAR*)buf;
}

std::wstring_view StackUtf8ToWstr::AsView() const {
    WCHAR* d = Get();
    return {d, cchConverted};
}

#if 0
StackUtf8ToWstr& StackUtf8ToWstr::operator=(const StackUtf8ToWstr& other) {
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

StackUtf8ToWstr::operator WCHAR*() const {
    return Get();
}

StackUtf8ToWstr::~StackUtf8ToWstr() {
    if (overflow != buf) {
        str::Free(overflow);
    }
}

} // namespace strconv
