/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include <Wininet.h>
#include "HttpUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"

// per RFC 1945 10.15 and 3.7, a user agent product token shouldn't contain whitespace
#define USER_AGENT L"BaseHTTP"

bool HttpRspOk(const HttpRsp* rsp)
{
    return (rsp->error == ERROR_SUCCESS) && (rsp->httpStatusCode == 200);
}

// returns false if failed to download or status code is not 200
// for other scenarios, check HttpRsp
bool  HttpGet(const WCHAR *url, HttpRsp *rspOut)
{
    HINTERNET hReq = NULL;
    DWORD headerBuffSize = sizeof(DWORD);
    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;

    rspOut->error = ERROR_SUCCESS;
    HINTERNET hInet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto Error;

    hReq = InternetOpenUrl(hInet, url, NULL, 0, flags, 0);
    if (!hReq)
        goto Error;

    if (!HttpQueryInfoW(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &rspOut->httpStatusCode, &headerBuffSize, NULL)) {
        goto Error;
    }

    for (;;) {
        char buf[1024];
        DWORD dwRead = 0;
        if (!InternetReadFile(hReq, buf, sizeof(buf), &dwRead)) {
            goto Error;
        }
        if (0 == dwRead) {
            break;
        }
        bool ok = rspOut->data.AppendChecked(buf, dwRead);
        if (!ok)
            goto Error;
    }

Exit:
    if (hReq)
        InternetCloseHandle(hReq);
    if (hInet)
        InternetCloseHandle(hInet);
    return HttpRspOk(rspOut);

Error:
    rspOut->error = GetLastError();
    if (0 == rspOut->error)
        rspOut->error = ERROR_GEN_FAILURE;
    goto Exit;
}

// Download content of a url to a file
bool HttpGetToFile(const WCHAR *url, const WCHAR *destFilePath)
{
    bool ok = false;
    HINTERNET  hReq = NULL, hInet = NULL;
    DWORD dwRead = 0;
    DWORD headerBuffSize = sizeof(DWORD);
    DWORD statusCode = 0;
    char buf[1024];

    HANDLE hf = CreateFile(destFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,  NULL);
    if (INVALID_HANDLE_VALUE == hf)
        goto Exit;

    hInet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto Exit;

    hReq = InternetOpenUrl(hInet, url, NULL, 0, 0, 0);
    if (!hReq)
        goto Exit;

    if (!HttpQueryInfoW(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &headerBuffSize, NULL)) {
        goto Exit;
    }
    if (statusCode != 200) {
        goto Exit;
    }

    for (;;) {
        if (!InternetReadFile(hReq, buf, sizeof(buf), &dwRead))
            goto Exit;
        if (dwRead == 0)
            break;
        DWORD size;
        BOOL wroteOk = WriteFile(hf, buf, (DWORD)dwRead, &size, NULL);
        if (!wroteOk)
            goto Exit;

        if (size != dwRead)
            goto Exit;
    }

    ok = true;
Exit:
    CloseHandle(hf);
    if (hReq)
        InternetCloseHandle(hReq);
    if (hInet)
        InternetCloseHandle(hInet);
    if (!ok)
        file::Delete(destFilePath);
    return ok;
}

bool HttpPost(const WCHAR *server, const WCHAR *url, str::Str<char> *headers, str::Str<char> *data)
{
    str::Str<char> resp(2048);
    bool ok = false;
    DWORD flags = 0;
    char *hdr = NULL;
    DWORD hdrLen = 0;
    HINTERNET hConn = NULL, hReq = NULL;
    void *d = NULL;
    DWORD dLen = 0;
    unsigned int timeoutMs = 15 * 1000;
    // Get the response status.
    DWORD respHttpCode = 0;
    DWORD respHttpCodeSize = sizeof(respHttpCode);
    DWORD dwRead = 0;

    HINTERNET hInet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto Exit;
    hConn = InternetConnect(hInet, server, INTERNET_DEFAULT_HTTP_PORT,
                            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
    if (!hConn)
        goto Exit;

    hReq = HttpOpenRequest(hConn, L"POST", url, NULL, NULL, NULL, flags, NULL);
    if (!hReq)
        goto Exit;
    if (headers && headers->Count() > 0) {
        hdr = headers->Get();
        hdrLen = (DWORD)headers->Count();
    }
    if (data && data->Count() > 0) {
        d = data->Get();
        dLen = (DWORD)data->Count();
    }

    InternetSetOption(hReq, INTERNET_OPTION_SEND_TIMEOUT,
                      &timeoutMs, sizeof(timeoutMs));

    InternetSetOption(hReq, INTERNET_OPTION_RECEIVE_TIMEOUT,
                      &timeoutMs, sizeof(timeoutMs));

    if (!HttpSendRequestA(hReq, hdr, hdrLen, d, dLen))
        goto Exit;

    HttpQueryInfo(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &respHttpCode, &respHttpCodeSize, 0);

    do {
        char buf[1024];
        if (!InternetReadFile(hReq, buf, sizeof(buf), &dwRead))
            goto Exit;
        ok = resp.AppendChecked(buf, dwRead);
        if (!ok)
            goto Exit;
    } while (dwRead > 0);

#if 0
    // it looks like I should be calling HttpEndRequest(), but it always claims
    // a timeout even though the data has been sent, received and we get HTTP 200
    if (!HttpEndRequest(hReq, NULL, 0, 0)) {
        LogLastError();
        goto Exit;
    }
#endif
    ok = (200 == respHttpCode);
Exit:
    if (hReq)
        InternetCloseHandle(hReq);
    if (hConn)
        InternetCloseHandle(hConn);
    if (hInet)
        InternetCloseHandle(hInet);
    return ok;
}

DWORD WINAPI HttpReq::DownloadThread(LPVOID data)
{
    HttpReq *req = (HttpReq *)data;
    HttpGet(req->url, &req->rsp);
    req->callback->Callback(req);
    return 0;
}

HttpReq::HttpReq(const WCHAR *url, HttpReqCallback *callback) :
    thread(NULL), callback(callback),
    url(str::Dup(url))
{
    assert(url);

    if (callback)
        thread = CreateThread(NULL, 0, DownloadThread, this, 0, 0);
    else
        HttpGet(url, &rsp);
}

HttpReq::~HttpReq()
{
    CloseHandle(thread);
    free(url);
}
