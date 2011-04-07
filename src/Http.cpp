/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "Http.h"
#include "Version.h"
#include <Wininet.h>

bool HttpGet(TCHAR *url, Str::Str<char> *dataOut)
{
    bool ok = false;
    HINTERNET hFile = NULL;
    HINTERNET hInet = InternetOpen(APP_NAME_STR _T("/") CURR_VERSION_STR,
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto Exit;

    hFile = InternetOpenUrl(hInet, url, NULL, 0, 0, 0);
    if (!hFile)
        goto Exit;

    DWORD dwRead;
    do {
        char *buf = dataOut->MakeSpaceAtNoLenIncrease(dataOut->Count(), 1024);
        if (!InternetReadFile(hFile, buf, 1024, &dwRead))
            goto Exit;
        dataOut->LenIncrease(dwRead);
    } while (dwRead > 0);

    ok = true;
Exit:
    InternetCloseHandle(hFile);
    InternetCloseHandle(hInet);
    return ok;
}

bool HttpPost(TCHAR *url, Str::Str<char> *headers, Str::Str<char> *data)
{
    bool ok = false;
    HINTERNET hInet = NULL, hConn = NULL, hReq = NULL;

    hInet = InternetOpen(APP_NAME_STR _T("/") CURR_VERSION_STR,
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto Exit;
    hConn = InternetConnect(hInet, url, INTERNET_DEFAULT_HTTP_PORT, NULL, NULL,
        INTERNET_SERVICE_HTTP, 0, 1);
    if (!hConn)
        goto Exit;

    DWORD flags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_FORMS_SUBMIT |
                  INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                  INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP;

    hReq = HttpOpenRequest(hConn, _T("POST"), url, NULL, NULL, NULL, flags, NULL);
    if (!hReq)
        goto Exit;
    char *hdr = NULL;
    DWORD hdrLen = 0;
    if (headers && headers->Count() > 0) {
        hdr = headers->Get();
        hdrLen = headers->Count();
    }
    void *d = NULL;
    DWORD dLen = 0;
    if (data && data->Count() > 0) {
        d = data->Get();
        dLen = data->Count();
    }

    if (!HttpSendRequestA(hReq, hdr, hdrLen, d, dLen))
        goto Exit;
    if (!HttpEndRequest(hReq, NULL, 0, 0))
        goto Exit;

    ok = true;
Exit:
    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return ok;
}

static DWORD WINAPI HttpDownloadThread(LPVOID data)
{
    HttpReqCtx *ctx = (HttpReqCtx *)data;
    if (HttpGet(ctx->url, ctx->data)) {
        if (ctx->callback)
            ctx->callback->Callback(ctx);
    } else {
        ctx->error = GetLastError();
        if (ctx->error == 0)
            ctx->error = ERROR_GEN_FAILURE;
    }
    return 0;
}

HttpReqCtx::HttpReqCtx(const TCHAR *url, CallbackFunc *callback)
    : callback(callback), error(0)
{
    assert(url);
    url = Str::Dup(url);
    data = new Str::Str<char>(256);
    if (callback)
        hThread = CreateThread(NULL, 0, HttpDownloadThread, this, 0, 0);
    else
        HttpDownloadThread(this);
}

