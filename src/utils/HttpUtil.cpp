/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "utils/HttpUtil.h"

#include "utils/ThreadUtil.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"

// per RFC 1945 10.15 and 3.7, a user agent product token shouldn't contain whitespace
#define USER_AGENT L"BaseHTTP"

bool HttpRspOk(const HttpRsp* rsp) {
    return (rsp->error == ERROR_SUCCESS) && (rsp->httpStatusCode == 200);
}

// returns false if failed to download or status code is not 200
// for other scenarios, check HttpRsp
bool HttpGet(const WCHAR* url, HttpRsp* rspOut) {
    logf(L"HttpGet: url: '%s'\n", url);
    HINTERNET hReq = nullptr;
    DWORD headerBuffSize = sizeof(DWORD);
    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;

    rspOut->data.allowFailure = true;

    rspOut->error = ERROR_SUCCESS;
    HINTERNET hInet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) {
        logf("HttpGet: InternetOpen failed\n");
        LogLastError();
        goto Error;
    }

    hReq = InternetOpenUrl(hInet, url, nullptr, 0, flags, 0);
    if (!hReq) {
        logf("HttpGet: InternetOpenUrl failed\n");
        LogLastError();
        goto Error;
    }

    DWORD infoLevel = HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER;
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
        bool ok = rspOut->data.Append(buf, dwRead);
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
    return HttpRspOk(rspOut);

Error:
    rspOut->error = GetLastError();
    if (0 == rspOut->error) {
        rspOut->error = ERROR_GEN_FAILURE;
    }
    goto Exit;
}

// Download content of a url to a file
bool HttpGetToFile(const WCHAR* url, const WCHAR* destFilePath) {
    logf(L"HttpGetToFile: url: '%s', file: '%s'\n", url, destFilePath);
    bool ok = false;
    HINTERNET hReq = nullptr, hInet = nullptr;
    DWORD dwRead = 0;
    DWORD headerBuffSize = sizeof(DWORD);
    DWORD statusCode = 0;
    char buf[1024];

    HANDLE hf = CreateFileW(destFilePath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
    if (INVALID_HANDLE_VALUE == hf) {
        logf(L"HttpGetToFile: CreateFileW('%s') failed\n", destFilePath);
        LogLastError();
        goto Exit;
    }

    hInet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) {
        goto Exit;
    }

    hReq = InternetOpenUrl(hInet, url, nullptr, 0, 0, 0);
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
        if (!InternetReadFile(hReq, buf, sizeof(buf), &dwRead)) {
            goto Exit;
        }
        if (dwRead == 0) {
            break;
        }
        DWORD size;
        BOOL wroteOk = WriteFile(hf, buf, (DWORD)dwRead, &size, nullptr);
        if (!wroteOk) {
            goto Exit;
        }

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
    return ok;
}

bool HttpPost(const WCHAR* server, int port, const WCHAR* url, str::Str* headers, str::Str* data) {
    str::Str resp(2048);
    bool ok = false;
    char* hdr = nullptr;
    DWORD hdrLen = 0;
    HINTERNET hConn = nullptr, hReq = nullptr;
    void* d = nullptr;
    DWORD dLen = 0;
    unsigned int timeoutMs = 15 * 1000;
    // Get the response status.
    DWORD respHttpCode = 0;
    DWORD respHttpCodeSize = sizeof(respHttpCode);
    DWORD dwRead = 0;

    HINTERNET hInet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) {
        goto Exit;
    }
    DWORD dwService = INTERNET_SERVICE_HTTP;
    hConn = InternetConnectW(hInet, server, (INTERNET_PORT)port, nullptr, nullptr, dwService, 0, 1);
    if (!hConn) {
        goto Exit;
    }

    DWORD flags = INTERNET_FLAG_NO_UI;
    if (port == 443) {
        flags |= INTERNET_FLAG_SECURE;
    }
    hReq = HttpOpenRequestW(hConn, L"POST", url, nullptr, nullptr, nullptr, flags, 0);
    if (!hReq) {
        goto Exit;
    }

    if (headers && headers->size() > 0) {
        hdr = headers->Get();
        hdrLen = (DWORD)headers->size();
    }
    if (data && data->size() > 0) {
        d = data->Get();
        dLen = (DWORD)data->size();
    }

    InternetSetOptionW(hReq, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    InternetSetOptionW(hReq, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    if (!HttpSendRequestA(hReq, hdr, hdrLen, d, dLen)) {
        goto Exit;
    }

    HttpQueryInfoW(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &respHttpCode, &respHttpCodeSize, 0);

    do {
        char buf[1024];
        if (!InternetReadFile(hReq, buf, sizeof(buf), &dwRead)) {
            goto Exit;
        }
        ok = resp.Append(buf, dwRead);
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

// callback function f is responsible for deleting HttpRsp
void HttpGetAsync(const WCHAR* url, const std::function<void(HttpRsp*)>& f) {
    RunAsync([=] {
        auto rsp = new HttpRsp;
        rsp->url.SetCopy(url);
        HttpGet(url, rsp);
        f(rsp);
    });
}

#if 0
// returns false if failed to download or status code is not 200
// for other scenarios, check HttpRsp
static bool  HttpGet(const char *url, HttpRsp *rspOut) {
    AutoFreeWstr urlW(strconv::FromUtf8(url));
    return HttpGet(urlW, rspOut);
}

void HttpGetAsync(const char *url, const std::function<void(HttpRsp *)> &f) {
    std::thread t([=] {
        auto rsp = new HttpRsp;
        HttpGet(url, rsp.get());
        f(rsp.get());
    });
    t.detach();
}
#endif
