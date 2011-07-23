/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "BaseUtil.h"
#include <Wininet.h>

#include "Http.h"
#include "FileUtil.h"
#include "WinUtil.h"

// per RFC 1945 10.15 and 3.7, a user agent product token shouldn't contain whitespace
#define USER_AGENT _T("BaseHTTP")

// returns ERROR_SUCCESS or an error code
DWORD HttpGet(const TCHAR *url, str::Str<char> *dataOut)
{
    DWORD error = ERROR_SUCCESS;

    HINTERNET hFile = NULL;
    HINTERNET hInet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto Error;

    DWORD flags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    hFile = InternetOpenUrl(hInet, url, NULL, 0, flags, 0);
    if (!hFile)
        goto Error;

    DWORD dwRead;
    do {
        char *buf = dataOut->EnsureEndPadding(1024);
        if (!InternetReadFile(hFile, buf, 1024, &dwRead))
            goto Error;
        dataOut->LenIncrease(dwRead);
    } while (dwRead > 0);

Exit:
    InternetCloseHandle(hFile);
    InternetCloseHandle(hInet);
    return error;

Error:
    error = GetLastError();
    if (!error)
        error = ERROR_GEN_FAILURE;
    goto Exit;
}

// Download content of a url to a file
bool HttpGetToFile(const TCHAR *url, const TCHAR *destFilePath)
{
    bool ok = false;
    char buf[1024];
    HINTERNET hFile = NULL, hInet = NULL;

    HANDLE hf = CreateFile(destFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,  
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (INVALID_HANDLE_VALUE == hf) {
        SeeLastError();
        goto Exit;
    }

    hInet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto Exit;

    hFile = InternetOpenUrl(hInet, url, NULL, 0, 0, 0);
    if (!hFile)
        goto Exit;

    DWORD dwRead;
    for (;;) {
        if (!InternetReadFile(hFile, buf, sizeof(buf), &dwRead))
            goto Exit;
        if (dwRead == 0)
            break;

        DWORD size;
        BOOL ok = WriteFile(hf, buf, (DWORD)dwRead, &size, NULL);
        if (!ok) {
            SeeLastError();
            goto Exit;
        }
        if (size != dwRead)
            goto Exit;
    }

    ok = true;
Exit:
    CloseHandle(hf);
    InternetCloseHandle(hFile);
    InternetCloseHandle(hInet);
    if (!ok)
        file::Delete(destFilePath);
    return ok;
}

bool HttpPost(const TCHAR *server, const TCHAR *url, str::Str<char> *headers, str::Str<char> *data)
{
    str::Str<char> resp(2048);
    bool ok = false;
    HINTERNET hInet = NULL, hConn = NULL, hReq = NULL;
    hInet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto Exit;
    hConn = InternetConnect(hInet, server, INTERNET_DEFAULT_HTTP_PORT,
                            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
    if (!hConn)
        goto Exit;

    DWORD flags = INTERNET_FLAG_KEEP_CONNECTION;
    hReq = HttpOpenRequest(hConn, _T("POST"), url, NULL, NULL, NULL, flags, NULL);
    if (!hReq)
        goto Exit;
    char *hdr = NULL;
    DWORD hdrLen = 0;
    if (headers && headers->Count() > 0) {
        hdr = headers->Get();
        hdrLen = (DWORD)headers->Count();
    }
    void *d = NULL;
    DWORD dLen = 0;
    if (data && data->Count() > 0) {
        d = data->Get();
        dLen = (DWORD)data->Count();
    }

    unsigned int timeoutMs = 15 * 1000;
    InternetSetOption(hReq, INTERNET_OPTION_SEND_TIMEOUT,
                      &timeoutMs, sizeof(timeoutMs));

    InternetSetOption(hReq, INTERNET_OPTION_RECEIVE_TIMEOUT,
                      &timeoutMs, sizeof(timeoutMs));

    if (!HttpSendRequestA(hReq, hdr, hdrLen, d, dLen)) {
        SeeLastError();
        goto Exit;
    }

    // Get the response status.
    DWORD respHttpCode = 0;
    DWORD respHttpCodeSize = sizeof(respHttpCode);
    if (!HttpQueryInfo(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &respHttpCode, &respHttpCodeSize, 0)) {
        SeeLastError();
    }

    DWORD dwRead;
    do {
        char *buf = resp.EnsureEndPadding(1024);
        if (!InternetReadFile(hReq, buf, 1024, &dwRead))
            goto Exit;
        resp.LenIncrease(dwRead);
    } while (dwRead > 0);

#if 0
    // it looks like I should be calling HttpEndRequest(), but it always claims
    // a timeout even though the data has been sent, received and we get HTTP 200
    if (!HttpEndRequest(hReq, NULL, 0, 0)) {
        SeeLastError();
        goto Exit;
    }
#endif
    ok = (200 == respHttpCode);
Exit:
    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return ok;
}

static DWORD WINAPI HttpDownloadThread(LPVOID data)
{
    HttpReqCtx *ctx = (HttpReqCtx *)data;
    ctx->error = HttpGet(ctx->url, ctx->data);
    if (ctx->callback)
        ctx->callback->Callback(ctx);
    return 0;
}

HttpReqCtx::HttpReqCtx(const TCHAR *url, HttpReqCallback *callback)
    : callback(callback), error(0)
{
    assert(url);
    this->url = str::Dup(url);
    data = new str::Str<char>(256);
    if (callback)
        hThread = CreateThread(NULL, 0, HttpDownloadThread, this, 0, 0);
    else
        HttpDownloadThread(this);
}
