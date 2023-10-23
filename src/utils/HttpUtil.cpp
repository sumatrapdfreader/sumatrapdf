/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/HttpUtil.h"

#include "utils/Log.h"

// per RFC 1945 10.15 and 3.7, a user agent product token shouldn't contain whitespace
constexpr const WCHAR* kUserAgent = L"SumatraPdfHTTP";

bool HttpRspOk(const HttpRsp* rsp) {
    return (rsp->error == ERROR_SUCCESS) && (rsp->httpStatusCode == 200);
}

// returns false if failed to download or status code is not 200
// for other scenarios, check HttpRsp
bool HttpGet(const char* urlA, HttpRsp* rspOut) {
    logf("HttpGet: url: '%s'\n", urlA);
    HINTERNET hReq = nullptr;
    DWORD infoLevel;
    DWORD headerBuffSize = sizeof(DWORD);
    WCHAR* url = ToWStrTemp(urlA);
    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

    if (str::StartsWithI(urlA, "https")) {
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
        InterlockedIncrement(&gAllowAllocFailure);
        bool ok = rspOut->data.Append(buf, dwRead);
        InterlockedDecrement(&gAllowAllocFailure);
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
bool HttpGetToFile(const char* urlA, const char* destFilePath) {
    logf("HttpGetToFile: url: '%s', file: '%s'\n", urlA, destFilePath);
    bool ok = false;
    HINTERNET hReq = nullptr, hInet = nullptr;
    DWORD dwRead = 0;
    DWORD headerBuffSize = sizeof(DWORD);
    DWORD statusCode = 0;
    WCHAR* url = ToWStrTemp(urlA);
    char buf[1024];

    WCHAR* pathW = ToWStrTemp(destFilePath);
    HANDLE hf =
        CreateFileW(pathW, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (INVALID_HANDLE_VALUE == hf) {
        logf("HttpGetToFile: CreateFileW('%s') failed\n", destFilePath);
        LogLastError();
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

bool HttpPost(const char* serverA, int port, const char* urlA, str::Str* headers, str::Str* data) {
    str::Str resp(2048);
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
    WCHAR* server = ToWStrTemp(serverA);
    WCHAR* url = ToWStrTemp(urlA);
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

    infoLevel = HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER;
    HttpQueryInfoW(hReq, infoLevel, &respHttpCode, &respHttpCodeSize, nullptr);

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
