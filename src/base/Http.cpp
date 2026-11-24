/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#if OS_WIN
#include "base/File.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include "base/Http.h"

HttpRsp::~HttpRsp() {
    str::Free(url);
}

bool IsHttpRspOk(const HttpRsp* rsp) {
    if (rsp->error != 0) {
        logf("HttpRspOk: rsp->error %d, should be 0\n", (int)rsp->error);
        return false;
    }
    if (rsp->httpStatusCode < 200 || rsp->httpStatusCode >= 300) {
        logf("HttpRspOk: rsp->httpStatusCode: %d\n", (int)rsp->httpStatusCode);
        return false;
    }
    return true;
}

#if OS_WIN

// All requests go through WinHTTP, not WinINet: one OS library fewer, and
// WinINet shares Internet Explorer's cookie jar, so a request would carry
// whatever cookies happen to be lying around.

// per RFC 1945 10.15 and 3.7, a user agent product token shouldn't contain whitespace
constexpr const WCHAR* kUserAgent = L"SumatraPdfHTTP";

#endif // OS_WIN

//--- URL encoding

// URL-encode s so it can be substituted into a URL, shortening it if the
// encoded form would not fit in maxEncodedLen characters.
TempStr URLEncodeMayTruncateTemp(Str s, int maxEncodedLen, bool* didTruncateOut) {
    if (maxEncodedLen <= 0) {
        maxEncodedLen = kMaxUrlEncodedLen;
    }
    return url::EncodeMayTruncateTemp(s, maxEncodedLen, didTruncateOut);
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
            b.Append(StrL("\r\n"));
        }
        b.Append(t);
    }
    return ToStrTemp(b);
}

#if OS_WIN

// POST body to url with an explicit Content-Type and optional extra headers.
// Blocking, so call it off the ui thread. Returns true on a 2xx; rspOut always
// carries the status code, the body and the win32 error.
// extraHeaders is "Name: value" per line (\n or \r\n separated)
bool HttpPostUrl(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut) {
    bool ok = false;
    DWORD sc = 0;
    HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;
    rspOut->error = ERROR_SUCCESS;

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    WCHAR* urlW = CWStrTemp(url);
    if (!WinHttpCrackUrl(urlW, 0, 0, &uc)) {
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
                hdrs.Append(StrL("\r\n"));
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

        char buf[8192];
        constexpr int kPostBodyCap = 1024 * 1024;
        for (;;) {
            int room = kPostBodyCap - len(rspOut->data);
            if (room <= 0) {
                break;
            }
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
                rspOut->error = GetLastError();
                break;
            }
            if (avail == 0) {
                break;
            }
            DWORD toRead = std::min<DWORD>(avail, (DWORD)std::min<int>(room, sizeofi(buf)));
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf, toRead, &read)) {
                rspOut->error = GetLastError();
                break;
            }
            if (read == 0) {
                break;
            }
            rspOut->data.Append(Str(buf, (int)read));
        }
        // a transfer cut short after the status line is a failure
        ok = rspOut->error == ERROR_SUCCESS && sc >= 200 && sc < 300;
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

// A 3xx is an error; redirects are not followed.
bool HttpPostUrlNoRedirect(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut) {
    bool ok = false;
    DWORD sc = 0;
    HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;
    rspOut->error = ERROR_SUCCESS;
    rspOut->httpStatusCode = (DWORD)-1;

    if (!OnlineURLAllowed(url)) {
        rspOut->error = ERROR_INVALID_PARAMETER;
        logf("HttpPostUrlNoRedirect: url: '%s' status: %d\n", url, -1);
        return false;
    }

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    WCHAR* urlW = CWStrTemp(url);
    if (!WinHttpCrackUrl(urlW, 0, 0, &uc)) {
        rspOut->error = GetLastError();
        logf("HttpPostUrlNoRedirect: url: '%s' status: %d\n", url, -1);
        return false;
    }

    {
        int pathLen = (uc.dwUrlPathLength == (DWORD)-1) ? 0 : (int)uc.dwUrlPathLength;
        int extraLen = (uc.dwExtraInfoLength == (DWORD)-1) ? 0 : (int)uc.dwExtraInfoLength;
        TempWStr host = str::DupTemp(WStr(uc.lpszHostName, (int)uc.dwHostNameLength));
        TempWStr pathAndQuery = str::DupTemp(WStr(uc.lpszUrlPath, pathLen + extraLen));
        if (len(pathAndQuery) == 0) {
            pathAndQuery = str::DupTemp(WStrL(L"/"));
        }

        hSession = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            rspOut->error = GetLastError();
            goto ExitPost;
        }
        hConnect = WinHttpConnect(hSession, host.s, (INTERNET_PORT)uc.nPort, 0);
        if (!hConnect) {
            rspOut->error = GetLastError();
            goto ExitPost;
        }
        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, L"POST", pathAndQuery.s, nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            rspOut->error = GetLastError();
            goto ExitPost;
        }

        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy))) {
            rspOut->error = GetLastError();
            goto ExitPost;
        }

        str::Builder hdrs;
        if (!str::IsEmptyOrWhiteSpace(contentType)) {
            hdrs.Append(fmt("Content-Type: %s", contentType));
        }
        TempStr extra = HttpNormalizeHeadersTemp(extraHeaders);
        if (!str::IsEmptyOrWhiteSpace(extra)) {
            if (len(hdrs) > 0) {
                hdrs.Append(StrL("\r\n"));
            }
            hdrs.Append(extra);
        }
        TempWStr hdrsW = ToWStrTemp(ToStr(hdrs));

        BOOL sent =
            WinHttpSendRequest(hRequest, hdrsW.s, (DWORD)-1, (void*)body.s, (DWORD)len(body), (DWORD)len(body), 0);
        if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
            rspOut->error = GetLastError();
            goto ExitPost;
        }

        DWORD scSize = sizeof(sc);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &sc, &scSize, WINHTTP_NO_HEADER_INDEX);
        rspOut->httpStatusCode = sc;

        char buf[8192];
        constexpr int kPostBodyCap = 1024 * 1024;
        for (;;) {
            int room = kPostBodyCap - len(rspOut->data);
            if (room <= 0) {
                break;
            }
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
                rspOut->error = GetLastError();
                break;
            }
            if (avail == 0) {
                break;
            }
            DWORD toRead = std::min<DWORD>(avail, (DWORD)std::min<int>(room, sizeofi(buf)));
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf, toRead, &read)) {
                rspOut->error = GetLastError();
                break;
            }
            if (read == 0) {
                break;
            }
            rspOut->data.Append(Str(buf, (int)read));
        }
        // a transfer cut short after the status line is a failure
        ok = rspOut->error == ERROR_SUCCESS && sc >= 200 && sc < 300;
    }

ExitPost:
    logf("HttpPostUrlNoRedirect: url: '%s' status: %d\n", url, (int)rspOut->httpStatusCode);
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

// Scheme, host, and port. A prefix of the production host is a different host.
bool OnlineURLAllowed(Str url) {
    if (len(url) == 0) {
        return false;
    }
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUserNameLength = (DWORD)-1;
    uc.dwPasswordLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(CWStrTemp(url), 0, 0, &uc)) {
        return false;
    }
    if (!uc.lpszHostName || uc.dwHostNameLength == 0 || uc.dwHostNameLength == (DWORD)-1 || uc.dwHostNameLength > 253) {
        return false;
    }
    // length left at (DWORD)-1 means that component was absent
    bool user = uc.dwUserNameLength != 0 && uc.dwUserNameLength != (DWORD)-1;
    bool pass = uc.dwPasswordLength != 0 && uc.dwPasswordLength != (DWORD)-1;
    if (user || pass) {
        return false;
    }
    WStr host(uc.lpszHostName, (int)uc.dwHostNameLength);
    if (uc.nScheme == INTERNET_SCHEME_HTTPS && uc.nPort == 443 && wstr::EqI(host, WStrL(L"web.sumatrapdfreader.org"))) {
        return true;
    }
    if (uc.nScheme == INTERNET_SCHEME_HTTP && uc.nPort == 9332 &&
        (wstr::EqI(host, WStrL(L"localhost")) || wstr::EqI(host, WStrL(L"127.0.0.1")))) {
        return true;
    }
    return false;
}

// The list / key check. Blocks; call it off the UI thread. Keeps at most 1 MiB
// of body and does not follow redirects. Logs the URL and the status only, so
// the URL must not contain the key. A rejected URL never reaches WinHttpOpen.
constexpr int kHttpMemBodyCap = 1024 * 1024;

// verb: "GET" or "DELETE"; what: the name logged
// SumatraWeb: only the SumatraPDF Web server (OnlineURLAllowed), redirects not
// followed. AnyUrl: any url, redirects followed except https -> http.
enum class HttpTarget {
    SumatraWeb,
    AnyUrl,
};

static bool InMemoryRequest(const WCHAR* verb, const char* what, Str url, Str extraHeaders, HttpRsp* rspOut,
                            HttpTarget target) {
    bool ok = false;
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    rspOut->error = ERROR_SUCCESS;
    rspOut->httpStatusCode = (DWORD)-1;

    if (target == HttpTarget::SumatraWeb && !OnlineURLAllowed(url)) {
        rspOut->error = ERROR_INVALID_PARAMETER;
        logf("%s: url: '%s' status: %d\n", Str(what), url, -1);
        return false;
    }

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(CWStrTemp(url), 0, 0, &uc)) {
        rspOut->error = GetLastError();
        logf("%s: url: '%s' status: %d\n", Str(what), url, -1);
        return false;
    }

    {
        int pathLen = (uc.dwUrlPathLength == (DWORD)-1) ? 0 : (int)uc.dwUrlPathLength;
        int extraLen = (uc.dwExtraInfoLength == (DWORD)-1) ? 0 : (int)uc.dwExtraInfoLength;
        TempWStr host = str::DupTemp(WStr(uc.lpszHostName, (int)uc.dwHostNameLength));
        TempWStr pathAndQuery = str::DupTemp(WStr(uc.lpszUrlPath, pathLen + extraLen));
        if (len(pathAndQuery) == 0) {
            pathAndQuery = str::DupTemp(WStrL(L"/"));
        }

        hSession = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            rspOut->error = GetLastError();
            goto ExitGet;
        }
        hConnect = WinHttpConnect(hSession, host.s, (INTERNET_PORT)uc.nPort, 0);
        if (!hConnect) {
            rspOut->error = GetLastError();
            goto ExitGet;
        }
        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, verb, pathAndQuery.s, nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            rspOut->error = GetLastError();
            goto ExitGet;
        }

        if (target == HttpTarget::SumatraWeb) {
            DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy))) {
                rspOut->error = GetLastError();
                goto ExitGet;
            }
        }

        TempStr extra = HttpNormalizeHeadersTemp(extraHeaders);
        TempWStr hdrsW;
        LPCWSTR hdrs = WINHTTP_NO_ADDITIONAL_HEADERS;
        if (!str::IsEmptyOrWhiteSpace(extra)) {
            hdrsW = ToWStrTemp(extra);
            hdrs = hdrsW.s;
        }

        BOOL sent = WinHttpSendRequest(hRequest, hdrs, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
            rspOut->error = GetLastError();
            goto ExitGet;
        }

        DWORD sc = 0;
        DWORD scSize = sizeof(sc);
        if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &sc, &scSize, WINHTTP_NO_HEADER_INDEX)) {
            rspOut->error = GetLastError();
            goto ExitGet;
        }
        rspOut->httpStatusCode = sc;

        char buf[8192];
        for (;;) {
            int room = kHttpMemBodyCap - len(rspOut->data);
            if (room <= 0) {
                break;
            }
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
                rspOut->error = GetLastError();
                break;
            }
            if (avail == 0) {
                break;
            }
            DWORD toRead = std::min<DWORD>(avail, (DWORD)std::min<int>(room, sizeofi(buf)));
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf, toRead, &read)) {
                rspOut->error = GetLastError();
                break;
            }
            if (read == 0) {
                break;
            }
            rspOut->data.Append(Str(buf, (int)read));
        }
        // a transfer cut short after the status line is a failure
        ok = rspOut->error == ERROR_SUCCESS && sc == 200;
    }

ExitGet:
    logf("%s: url: '%s' status: %d\n", Str(what), url, (int)rspOut->httpStatusCode);
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

// Desktop cap. The website still stores up to 200 MiB; a partial file over
// this limit is deleted so it cannot be opened as a document.
constexpr i64 kHttpDownloadMaxBytes = (i64)50 << 20;

static bool WriteAllBytes(HANDLE h, const void* data, DWORD n) {
    const char* p = (const char*)data;
    while (n > 0) {
        DWORD wrote = 0;
        if (!WriteFile(h, p, n, &wrote, nullptr) || wrote == 0) {
            return false;
        }
        p += wrote;
        n -= wrote;
    }
    return true;
}

bool HttpGetWithHeaders(Str url, Str extraHeaders, HttpRsp* rspOut) {
    return InMemoryRequest(L"GET", "HttpGetWithHeaders", url, extraHeaders, rspOut, HttpTarget::SumatraWeb);
}

// Ok: 200
bool HttpDeleteWithHeaders(Str url, Str extraHeaders, HttpRsp* rspOut) {
    return InMemoryRequest(L"DELETE", "HttpDeleteWithHeaders", url, extraHeaders, rspOut, HttpTarget::SumatraWeb);
}

// Writes url's body to destPath through destPath.part, so a failed transfer
// keeps the last good copy. A body over maxBytes deletes the partial file.
static bool DownloadToFile(const char* what, Str url, Str extraHeaders, Str destPath, HttpRsp* rspOut,
                           HttpTarget target, i64 maxBytes, const Func1<HttpProgress*>* progressCb) {
    bool ok = false;
    bool created = false;
    TempStr partPath;
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    rspOut->error = ERROR_SUCCESS;
    rspOut->httpStatusCode = (DWORD)-1;

    if (target == HttpTarget::SumatraWeb && !OnlineURLAllowed(url)) {
        rspOut->error = ERROR_INVALID_PARAMETER;
        logf("%s: url: '%s' status: %d\n", Str(what), url, -1);
        return false;
    }

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(CWStrTemp(url), 0, 0, &uc)) {
        rspOut->error = GetLastError();
        logf("%s: url: '%s' status: %d\n", Str(what), url, -1);
        return false;
    }

    {
        int pathLen = (uc.dwUrlPathLength == (DWORD)-1) ? 0 : (int)uc.dwUrlPathLength;
        int extraLen = (uc.dwExtraInfoLength == (DWORD)-1) ? 0 : (int)uc.dwExtraInfoLength;
        TempWStr host = str::DupTemp(WStr(uc.lpszHostName, (int)uc.dwHostNameLength));
        TempWStr pathAndQuery = str::DupTemp(WStr(uc.lpszUrlPath, pathLen + extraLen));
        if (len(pathAndQuery) == 0) {
            pathAndQuery = str::DupTemp(WStrL(L"/"));
        }

        hSession = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            rspOut->error = GetLastError();
            goto ExitDl;
        }
        hConnect = WinHttpConnect(hSession, host.s, (INTERNET_PORT)uc.nPort, 0);
        if (!hConnect) {
            rspOut->error = GetLastError();
            goto ExitDl;
        }
        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, L"GET", pathAndQuery.s, nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            rspOut->error = GetLastError();
            goto ExitDl;
        }

        if (target == HttpTarget::SumatraWeb) {
            DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy))) {
                rspOut->error = GetLastError();
                goto ExitDl;
            }
        }

        TempStr extra = HttpNormalizeHeadersTemp(extraHeaders);
        TempWStr hdrsW;
        LPCWSTR hdrs = WINHTTP_NO_ADDITIONAL_HEADERS;
        if (!str::IsEmptyOrWhiteSpace(extra)) {
            hdrsW = ToWStrTemp(extra);
            hdrs = hdrsW.s;
        }

        BOOL sent = WinHttpSendRequest(hRequest, hdrs, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
            rspOut->error = GetLastError();
            goto ExitDl;
        }

        DWORD sc = 0;
        DWORD scSize = sizeof(sc);
        if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &sc, &scSize, WINHTTP_NO_HEADER_INDEX)) {
            rspOut->error = GetLastError();
            goto ExitDl;
        }
        rspOut->httpStatusCode = sc;
        // a 3xx is not followed; it is not a file
        if (sc != 200) {
            goto ExitDl;
        }

        DWORD contentLen = 0;
        DWORD clSize = sizeof(contentLen);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &contentLen, &clSize, WINHTTP_NO_HEADER_INDEX)) {
            if ((i64)contentLen > maxBytes) {
                rspOut->error = ERROR_FILE_TOO_LARGE;
                goto ExitDl;
            }
        }

        // A failed transfer must not truncate the last good copy of destPath.
        partPath = str::JoinTemp(destPath, StrL(".part"));
        hFile = CreateFileW(CWStrTemp(partPath), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            rspOut->error = GetLastError();
            goto ExitDl;
        }
        created = true;

        i64 got = 0;
        char buf[32 * 1024];
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
                rspOut->error = GetLastError();
                goto ExitDl;
            }
            if (avail == 0) {
                break;
            }
            DWORD want = avail;
            if (want > sizeof(buf)) {
                want = (DWORD)sizeof(buf);
            }
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf, want, &read)) {
                rspOut->error = GetLastError();
                goto ExitDl;
            }
            if (read == 0) {
                break;
            }
            if (got + (i64)read > maxBytes) {
                rspOut->error = ERROR_FILE_TOO_LARGE;
                goto ExitDl;
            }
            if (!WriteAllBytes(hFile, buf, read)) {
                rspOut->error = GetLastError();
                goto ExitDl;
            }
            got += (i64)read;
            if (progressCb) {
                HttpProgress progress{got};
                progressCb->Call(&progress);
            }
        }
        ok = true;
    }

ExitDl:
    logf("%s: url: '%s' status: %d\n", Str(what), url, (int)rspOut->httpStatusCode);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }
    if (ok && !file::RenameReplace(destPath, partPath)) {
        DWORD err = GetLastError();
        rspOut->error = err != 0 ? err : (DWORD)ERROR_GEN_FAILURE;
        ok = false;
    }
    if (created && !ok) {
        file::Delete(partPath);
    }
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

// Does not follow redirects. A body over 50 MiB deletes the partial file.
bool HttpDownloadWithHeaders(Str url, Str extraHeaders, Str destPath, HttpRsp* rspOut) {
    return DownloadToFile("HttpDownloadWithHeaders", url, extraHeaders, destPath, rspOut, HttpTarget::SumatraWeb,
                          kHttpDownloadMaxBytes, nullptr);
}

// returns false if failed to download or status code is not 200
bool HttpGet(Str url, HttpRsp* rspOut) {
    return InMemoryRequest(L"GET", "HttpGet", url, Str(), rspOut, HttpTarget::AnyUrl);
}

// Download content of a url to a file; maxSize < 0: no limit
bool HttpGetToFile(Str url, Str destFilePath, const Func1<HttpProgress*>& cbProgress, i64 maxSize) {
    HttpRsp rsp;
    i64 maxBytes = maxSize < 0 ? INT64_MAX : maxSize;
    return DownloadToFile("HttpGetToFile", url, Str(), destFilePath, &rsp, HttpTarget::AnyUrl, maxBytes, &cbProgress);
}

#endif

#if OS_WIN
// One streamed request on WinHTTP's async API. Each call completes on a
// WinHTTP thread through StreamCallback, which signals `done`. Cancel() only
// signals an event, so HttpPostStream alone uses and closes the request: a
// handle closed by another thread can't be used, or reused, mid-call.
struct AsyncStreamReq {
    HANDLE done = nullptr;   // auto-reset: the pending call completed
    HANDLE closed = nullptr; // manual-reset: WinHTTP is done with the request
    DWORD status = 0;        // callback status of the completion
    DWORD bytes = 0;         // available / read
};

static void CALLBACK StreamCallback(HINTERNET, DWORD_PTR ctx, DWORD status, void* info, DWORD infoLen) {
    auto* r = (AsyncStreamReq*)ctx;
    if (!r) {
        return;
    }
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
        SetEvent(r->closed);
        return;
    }
    if (status == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE) {
        r->bytes = *(DWORD*)info;
    } else if (status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) {
        r->bytes = infoLen;
    } else if (status != WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE &&
               status != WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE && status != WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
        return;
    }
    r->status = status;
    SetEvent(r->done);
}

// waits for the pending call to complete with `want`; false on an error or a cancel
static bool WaitStream(AsyncStreamReq* r, HttpStreamSink* sink, HANDLE cancelEvent, DWORD want) {
    // the sink can be canceled without Cancel(): look at it now and then
    constexpr DWORD kPollMs = 200;
    HANDLE events[2] = {r->done, cancelEvent};
    for (;;) {
        DWORD res = WaitForMultipleObjects(2, events, FALSE, kPollMs);
        if (res == WAIT_OBJECT_0) {
            return r->status == want;
        }
        if (res != WAIT_TIMEOUT || sink->IsCanceled()) {
            return false;
        }
    }
}

bool HttpPostStream(Str url, Str headers, Str body, HttpStreamSink* sink, HttpStreamCancel* cancel) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = (DWORD)-1;
    TempWStr wide = ToWStrTemp(url);
    if (!WinHttpCrackUrl(wide.s, 0, 0, &parts) || sink->IsCanceled()) return false;
    TempWStr host = str::DupTemp(WStr(parts.lpszHostName, (int)parts.dwHostNameLength));
    TempWStr path = str::DupTemp(WStr(parts.lpszUrlPath, (int)(parts.dwUrlPathLength + parts.dwExtraInfoLength)));
    HINTERNET session = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
    if (!session) return false;
    HINTERNET connect = WinHttpConnect(session, host.s, parts.nPort, 0);
    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"POST", path.s, nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags)
                                : nullptr;
    HANDLE cancelEvent = (HANDLE)cancel->event;
    // heap: a WinHTTP thread can still call back after a wait gives up
    auto* r = new AsyncStreamReq();
    r->done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    r->closed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    bool ok = false;
    bool callbackSet = false;
    if (request && r->done && r->closed) {
        constexpr int kConnectTimeoutMs = 30000;
        constexpr int kStreamTimeoutMs = 180000;
        WinHttpSetTimeouts(request, kConnectTimeoutMs, kConnectTimeoutMs, kConnectTimeoutMs, kStreamTimeoutMs);
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        DWORD_PTR ctx = (DWORD_PTR)r;
        DWORD notify = WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES;
        callbackSet = WinHttpSetStatusCallback(request, StreamCallback, notify, 0) != WINHTTP_INVALID_STATUS_CALLBACK;
        bool setOk = callbackSet && WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &ctx, sizeof(ctx)) &&
                     WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
        TempWStr wideHeaders = ToWStrTemp(headers);
        if (setOk && !cancel->IsCanceled() &&
            WinHttpSendRequest(request, wideHeaders.s, (DWORD)len(wideHeaders), (void*)body.s, (DWORD)len(body),
                               (DWORD)len(body), ctx) &&
            WaitStream(r, sink, cancelEvent, WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE) &&
            WinHttpReceiveResponse(request, nullptr) &&
            WaitStream(r, sink, cancelEvent, WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE)) {
            DWORD status = 0, size = sizeof(status);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
                sink->OnStatus((int)status);
                // ask what has arrived, then read that: a read for a full buffer
                // would hold back a streamed reply until the buffer fills
                char bytes[4096];
                while (!sink->IsCanceled()) {
                    if (!WinHttpQueryDataAvailable(request, nullptr) ||
                        !WaitStream(r, sink, cancelEvent, WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE)) {
                        break;
                    }
                    if (r->bytes == 0) {
                        ok = true;
                        break;
                    }
                    DWORD toRead = std::min(r->bytes, (DWORD)sizeof(bytes));
                    if (!WinHttpReadData(request, bytes, toRead, nullptr) ||
                        !WaitStream(r, sink, cancelEvent, WINHTTP_CALLBACK_STATUS_READ_COMPLETE)) {
                        break;
                    }
                    if (r->bytes == 0) {
                        ok = true;
                        break;
                    }
                    if (!sink->OnData(Str(bytes, (int)r->bytes))) break;
                }
            }
        }
    }
    // closing aborts a pending call; WinHTTP may still call back until HANDLE_CLOSING
    bool released = true;
    if (request) {
        WinHttpCloseHandle(request);
        if (callbackSet) {
            constexpr DWORD kCloseWaitMs = 10000;
            released = WaitForSingleObject(r->closed, kCloseWaitMs) == WAIT_OBJECT_0;
        }
    }
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    if (released) {
        if (r->done) CloseHandle(r->done);
        if (r->closed) CloseHandle(r->closed);
        delete r;
    }
    return ok && !sink->IsCanceled();
}

HttpStreamCancel::HttpStreamCancel() {
    event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

HttpStreamCancel::~HttpStreamCancel() {
    if (event) {
        CloseHandle((HANDLE)event);
    }
}

void HttpStreamCancel::Cancel() {
    AtomicBoolSet(&canceled, true);
    if (event) {
        SetEvent((HANDLE)event);
    }
}
#else
HttpStreamCancel::HttpStreamCancel() = default;
HttpStreamCancel::~HttpStreamCancel() = default;

void HttpStreamCancel::Cancel() {
    AtomicBoolSet(&canceled, true);
}
#endif

bool HttpStreamCancel::IsCanceled() {
    return AtomicBoolGet(&canceled);
}
