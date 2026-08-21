/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/Http.h"

// MinGW's winhttp.h redefines INTERNET_SCHEME as int after wininet.h (via Base.h)
// already typedef'd it as an enum, which is a hard error. MSVC headers are fine
// together. On MinGW declare only the WinHTTP bits HttpPostUrl needs and link
// -lwinhttp (see cmd/helper/mingw-build.ts).
#if defined(__MINGW32__) || defined(__MINGW64__)
#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif
#ifndef WINHTTP_NO_PROXY_NAME
#define WINHTTP_NO_PROXY_NAME ((LPCWSTR) nullptr)
#endif
#ifndef WINHTTP_NO_PROXY_BYPASS
#define WINHTTP_NO_PROXY_BYPASS ((LPCWSTR) nullptr)
#endif
#ifndef WINHTTP_NO_REFERER
#define WINHTTP_NO_REFERER ((LPCWSTR) nullptr)
#endif
#ifndef WINHTTP_DEFAULT_ACCEPT_TYPES
#define WINHTTP_DEFAULT_ACCEPT_TYPES ((LPCWSTR*)nullptr)
#endif
#ifndef WINHTTP_FLAG_SECURE
#define WINHTTP_FLAG_SECURE 0x00800000
#endif
#ifndef WINHTTP_QUERY_STATUS_CODE
#define WINHTTP_QUERY_STATUS_CODE 19
#endif
#ifndef WINHTTP_QUERY_FLAG_NUMBER
#define WINHTTP_QUERY_FLAG_NUMBER 0x20000000
#endif
#ifndef WINHTTP_HEADER_NAME_BY_INDEX
#define WINHTTP_HEADER_NAME_BY_INDEX ((LPCWSTR) nullptr)
#endif
#ifndef WINHTTP_NO_HEADER_INDEX
#define WINHTTP_NO_HEADER_INDEX ((LPDWORD) nullptr)
#endif
extern "C" {
HINTERNET WINAPI WinHttpOpen(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
HINTERNET WINAPI WinHttpConnect(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
HINTERNET WINAPI WinHttpOpenRequest(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
BOOL WINAPI WinHttpSendRequest(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
BOOL WINAPI WinHttpReceiveResponse(HINTERNET, LPVOID);
BOOL WINAPI WinHttpQueryHeaders(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
BOOL WINAPI WinHttpQueryDataAvailable(HINTERNET, LPDWORD);
BOOL WINAPI WinHttpReadData(HINTERNET, LPVOID, DWORD, LPDWORD);
BOOL WINAPI WinHttpCloseHandle(HINTERNET);
}
#else
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

// per RFC 1945 10.15 and 3.7, a user agent product token shouldn't contain whitespace
constexpr const WCHAR* kUserAgent = L"SumatraPdfHTTP";

// returns false if failed to download or status code is not 200
// for other scenarios, check HttpRsp
bool HttpGet(Str urlA, HttpRsp* rspOut) {
    logf("HttpGet: url: '%s'\n", urlA);
    HINTERNET hReq = nullptr;
    DWORD infoLevel;
    DWORD headerBuffSize = sizeof(DWORD);
    WCHAR* url = CWStrTemp(urlA);
    // NB: do NOT add INTERNET_FLAG_IGNORE_CERT_CN_INVALID here - it disables TLS
    // hostname validation, letting a network attacker with any trusted cert
    // impersonate our update-check / crash-symbol hosts (GHSA-mjwr-9w29-jp96).
    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;

    if (str::StartsWithI(urlA, StrL("https"))) {
        flags |= INTERNET_FLAG_SECURE;
    }

    rspOut->error = ERROR_SUCCESS;
    HINTERNET hInet = InternetOpenW(kUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) {
        logf("HttpGet: InternetOpen failed\n");
        LogLastError();
        goto Error;
    }

    hReq = InternetOpenUrlW(hInet, url, nullptr, 0, flags, 0);
    if (!hReq) {
        logf("HttpGet: InternetOpenUrl failed\n");
        LogLastError();
        goto Error;
    }

    infoLevel = HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER;
    if (!HttpQueryInfoW(hReq, infoLevel, &rspOut->httpStatusCode, &headerBuffSize, nullptr)) {
        logf("HttpGet: HttpQueryInfoW failed\n");
        LogLastError();
        goto Error;
    }

    for (;;) {
        char buf[1024];
        DWORD dwRead = 0;
        if (!InternetReadFile(hReq, buf, sizeof(buf), &dwRead)) {
            logf("HttpGet: InternetReadFile failed\n");
            LogLastError();
            goto Error;
        }
        if (0 == dwRead) {
            break;
        }
        AtomicIntInc(&gAllowAllocFailure);
        bool ok = rspOut->data.Append(Str(buf, (int)dwRead));
        AtomicIntDec(&gAllowAllocFailure);
        if (!ok) {
            logf("HttpGet: data.Append failed\n");
            goto Error;
        }
    }

Exit:
    if (hReq) {
        InternetCloseHandle(hReq);
    }
    if (hInet) {
        InternetCloseHandle(hInet);
    }
    return IsHttpRspOk(rspOut);

Error:
    rspOut->error = GetLastError();
    if (0 == rspOut->error) {
        rspOut->error = ERROR_GEN_FAILURE;
    }
    goto Exit;
}

constexpr const int kBufSize = 256 * 1024;

// Download content of a url to a file
bool HttpGetToFile(Str urlA, Str destFilePath, const Func1<HttpProgress*>& cbProgress, i64 maxSize) {
    logf("HttpGetToFile: url: '%s', file: '%s'\n", urlA, destFilePath);
    bool ok = false;
    HINTERNET hReq = nullptr, hInet = nullptr;
    DWORD dwRead = 0;
    DWORD headerBuffSize = sizeof(DWORD);
    DWORD statusCode = 0;
    WCHAR* url = CWStrTemp(urlA);
    char* buf = nullptr;

    HttpProgress progress{};

    WCHAR* pathW = CWStrTemp(destFilePath);
    HANDLE hf =
        CreateFileW(pathW, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (INVALID_HANDLE_VALUE == hf) {
        logf("HttpGetToFile: CreateFileW('%s') failed\n", destFilePath);
        LogLastError();
        goto Exit;
    }

    buf = AllocArray<char>(kBufSize);
    if (!buf) {
        goto Exit;
    }

    hInet = InternetOpenW(kUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) {
        goto Exit;
    }

    hReq = InternetOpenUrlW(hInet, url, nullptr, 0, 0, 0);
    if (!hReq) {
        goto Exit;
    }

    if (!HttpQueryInfoW(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &headerBuffSize, nullptr)) {
        goto Exit;
    }

    if (statusCode != 200) {
        goto Exit;
    }

    for (;;) {
        if (!InternetReadFile(hReq, buf, kBufSize, &dwRead)) {
            goto Exit;
        }
        if (dwRead == 0) {
            break;
        }
        if (maxSize >= 0 && progress.nDownloaded > maxSize - (i64)dwRead) {
            goto Exit;
        }
        DWORD size;
        BOOL wroteOk = WriteFile(hf, buf, dwRead, &size, nullptr);
        if (!wroteOk) {
            goto Exit;
        }
        progress.nDownloaded += (i64)dwRead;
        cbProgress.Call(&progress);

        if (size != dwRead) {
            goto Exit;
        }
    }

    ok = true;
Exit:
    CloseHandle(hf);
    if (hReq) {
        InternetCloseHandle(hReq);
    }
    if (hInet) {
        InternetCloseHandle(hInet);
    }
    if (!ok) {
        file::Delete(destFilePath);
    }
    free(buf);
    return ok;
}

bool HttpPost(Str serverA, int port, Str urlA, str::Builder* headers, str::Builder* data) {
    str::Builder resp(2048);
    bool ok = false;
    char* hdr = nullptr;
    DWORD hdrLen = 0;
    HINTERNET hConn = nullptr, hReq = nullptr;
    void* d = nullptr;
    DWORD dLen = 0;
    unsigned int timeoutMs = 15 * 1000;
    DWORD respHttpCode = 0;
    DWORD respHttpCodeSize = sizeof(respHttpCode);
    DWORD dwRead = 0;
    DWORD flags;
    DWORD dwService;
    WCHAR* server = CWStrTemp(serverA);
    WCHAR* url = CWStrTemp(urlA);
    DWORD infoLevel;

    DWORD accessType = INTERNET_OPEN_TYPE_PRECONFIG;
    HINTERNET hInet = InternetOpenW(kUserAgent, accessType, nullptr, nullptr, 0);
    if (!hInet) {
        goto Exit;
    }
    dwService = INTERNET_SERVICE_HTTP;
    hConn = InternetConnectW(hInet, server, (INTERNET_PORT)port, nullptr, nullptr, dwService, 0, 1);
    if (!hConn) {
        goto Exit;
    }

    flags = INTERNET_FLAG_NO_UI;
    if (port == 443) {
        flags |= INTERNET_FLAG_SECURE;
    }
    hReq = HttpOpenRequestW(hConn, L"POST", url, nullptr, nullptr, nullptr, flags, 0);
    if (!hReq) {
        goto Exit;
    }

    if (headers && len(*headers) > 0) {
        hdr = ToStr(*headers).s;
        hdrLen = (DWORD)len(*headers);
    }
    if (data && len(*data) > 0) {
        d = ToStr(*data).s;
        dLen = (DWORD)len(*data);
    }

    InternetSetOptionW(hReq, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(hReq, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    if (!HttpSendRequestA(hReq, hdr, hdrLen, d, dLen)) {
        goto Exit;
    }

    infoLevel = HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER;
    HttpQueryInfoW(hReq, infoLevel, &respHttpCode, &respHttpCodeSize, nullptr);

    do {
        char buf[1024];
        if (!InternetReadFile(hReq, buf, sizeof(buf), &dwRead)) {
            goto Exit;
        }
        ok = resp.Append(Str(buf, (int)dwRead));
        if (!ok) {
            goto Exit;
        }
    } while (dwRead > 0);

#if 0
    // it looks like I should be calling HttpEndRequest(), but it always claims
    // a timeout even though the data has been sent, received and we get HTTP 200
    if (!HttpEndRequest(hReq, nullptr, 0, 0)) {
        LogLastError();
        goto Exit;
    }
#endif
    ok = (200 == respHttpCode);
Exit:
    if (hReq) {
        InternetCloseHandle(hReq);
    }
    if (hConn) {
        InternetCloseHandle(hConn);
    }
    if (hInet) {
        InternetCloseHandle(hInet);
    }
    return ok;
}

//--- URL encoding

// url-escapes the first nChars of ws. Returns the encoded string, or empty on
// failure. URL_ESCAPE_AS_UTF8 turns one WCHAR into at most 4 utf-8 bytes and
// each byte into "%XX", so 12 chars per WCHAR is the true worst case.
TempStr UrlEscapePrefixTemp(const WCHAR* ws, int nChars) {
    if (nChars <= 0) {
        return str::DupTemp(StrL(""));
    }
    WCHAR* in = AllocArrayTemp<WCHAR>(nChars + 1);
    memcpy(in, ws, (size_t)nChars * sizeof(WCHAR));
    in[nChars] = 0;
    int bufCch = (nChars * 12) + 1;
    WCHAR* buf = AllocArrayTemp<WCHAR>(bufCch);
    auto cch = (DWORD)bufCch;
    HRESULT hr = UrlEscapeW(in, buf, &cch, URL_ESCAPE_AS_UTF8);
    if (FAILED(hr)) {
        return {};
    }
    return ToUtf8Temp(buf);
}

// URL-encode s so it can be substituted into a URL, shortening it if the
// encoded form would not fit in maxEncodedLen characters.
//
// Encoded length is wildly different from input length and depends on the
// text: an ascii letter stays 1 char, a space becomes "%20" (3), a newline
// "%0A" (3), and a CJK character is 3 utf-8 bytes so it becomes 9. That is why
// we can't just cut the input at a fixed length - we binary-search for the
// longest prefix that still fits once encoded.
//
// The cut is made on a character boundary (never inside a surrogate pair, and
// never inside a %XX escape, since we shorten the *input* and re-encode rather
// than chopping the encoded output). Callers that care pass didTruncateOut so
// they can tell the user instead of silently sending less text than asked for.
TempStr URLEncodeMayTruncateTemp(Str s, int maxEncodedLen, bool* didTruncateOut) {
    if (didTruncateOut) {
        *didTruncateOut = false;
    }
    if (maxEncodedLen <= 0) {
        maxEncodedLen = kMaxUrlEncodedLen;
    }
    TempWStr ws = ToWStrTemp(s);
    int nChars = len(ws);
    if (nChars == 0) {
        return str::DupTemp(StrL(""));
    }

    TempStr full = UrlEscapePrefixTemp(ws.s, nChars);
    if (full.s && len(full) <= maxEncodedLen) {
        return full;
    }

    // longest prefix whose encoded form fits. lo always fits, hi never does.
    int lo = 0;
    int hi = nChars;
    while (lo + 1 < hi) {
        int mid = lo + ((hi - lo) / 2);
        // don't split a surrogate pair
        if (mid > 0 && mid < nChars && (ws.s[mid] & 0xFC00) == 0xDC00) {
            mid--;
        }
        if (mid <= lo) {
            break;
        }
        TempStr enc = UrlEscapePrefixTemp(ws.s, mid);
        if (enc.s && len(enc) <= maxEncodedLen) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    if (didTruncateOut) {
        *didTruncateOut = true;
    }
    return UrlEscapePrefixTemp(ws.s, lo);
}

//--- POST with custom headers (WinHTTP)

// Splits "Name: value\nName2: value2" into the CRLF-separated form http wants.
// Also accepts a literal backslash-n: a value that came from a settings file is
// a single line, so that is how a user writes a separator there.
TempStr HttpNormalizeHeadersTemp(Str headers) {
    if (str::IsEmptyOrWhiteSpace(headers)) {
        return {};
    }
    TempStr normalized = str::ReplaceTemp(headers, StrL("\\n"), StrL("\n"));
    str::Builder b;
    StrVec lines;
    Split(&lines, normalized, StrL("\n"), true);
    for (Str line : lines) {
        Str t = str::DupTemp(line);
        str::TrimWSInPlace(t, str::TrimOpt::Both);
        if (str::IsEmptyOrWhiteSpace(t) || !str::Contains(t, StrL(":"))) {
            continue;
        }
        if (len(b) > 0) {
            b.Append("\r\n");
        }
        b.Append(t);
    }
    return ToStrTemp(b);
}

// POST body to url with an explicit Content-Type and optional extra headers.
// Blocking, so call it off the ui thread. Returns true on a 2xx; rspOut always
// carries the status code, the body and the win32 error.
//
// Uses WinHTTP rather than the WinINet the rest of this file uses: WinINet
// shares Internet Explorer's cookie jar, so a request would carry whatever
// cookies happen to be lying around to a third-party endpoint. For a call meant
// to be authenticated only by an explicit api key that's a privacy leak.
// blocking; extraHeaders is "Name: value" per line (\n or \r\n separated)
bool HttpPostUrl(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut) {
    bool ok = false;
    DWORD sc = 0;
    HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;
    rspOut->error = ERROR_SUCCESS;

    // InternetCrackUrl (WinINet) rather than WinHttpCrackUrl: same URL_COMPONENTS,
    // and avoids needing the full winhttp.h URL crack API on MinGW.
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    WCHAR* urlW = CWStrTemp(url);
    if (!InternetCrackUrlW(urlW, 0, 0, &uc)) {
        rspOut->error = GetLastError();
        return false;
    }

    {
        TempWStr host = str::DupTemp(WStr(uc.lpszHostName, (int)uc.dwHostNameLength));
        TempWStr pathAndQuery = str::DupTemp(WStr(uc.lpszUrlPath, (int)(uc.dwUrlPathLength + uc.dwExtraInfoLength)));

        hSession = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            rspOut->error = GetLastError();
            goto Exit2;
        }
        hConnect = WinHttpConnect(hSession, host.s, (INTERNET_PORT)uc.nPort, 0);
        if (!hConnect) {
            rspOut->error = GetLastError();
            goto Exit2;
        }
        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, L"POST", pathAndQuery.s, nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            rspOut->error = GetLastError();
            goto Exit2;
        }

        str::Builder hdrs;
        if (!str::IsEmptyOrWhiteSpace(contentType)) {
            hdrs.Append(fmt("Content-Type: %s", contentType));
        }
        TempStr extra = HttpNormalizeHeadersTemp(extraHeaders);
        if (!str::IsEmptyOrWhiteSpace(extra)) {
            if (len(hdrs) > 0) {
                hdrs.Append("\r\n");
            }
            hdrs.Append(extra);
        }
        TempWStr hdrsW = ToWStrTemp(ToStr(hdrs));

        BOOL sent =
            WinHttpSendRequest(hRequest, hdrsW.s, (DWORD)-1, (void*)body.s, (DWORD)len(body), (DWORD)len(body), 0);
        if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
            rspOut->error = GetLastError();
            goto Exit2;
        }

        DWORD scSize = sizeof(sc);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &sc, &scSize, WINHTTP_NO_HEADER_INDEX);
        rspOut->httpStatusCode = sc;

        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) {
                break;
            }
            char* buf = AllocArrayTemp<char>((int)avail + 1);
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf, avail, &read) || read == 0) {
                break;
            }
            rspOut->data.Append(Str(buf, (int)read));
            // a runaway response shouldn't eat memory; callers only show a snippet
            if (len(rspOut->data) > 1024 * 1024) {
                break;
            }
        }
        ok = sc >= 200 && sc < 300;
    }

Exit2:
    if (hRequest) {
        WinHttpCloseHandle(hRequest);
    }
    if (hConnect) {
        WinHttpCloseHandle(hConnect);
    }
    if (hSession) {
        WinHttpCloseHandle(hSession);
    }
    return ok;
}
