/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "Http.h"
#include "Version.h"
#include <Wininet.h>

HttpReqCtx::HttpReqCtx(const TCHAR *url, CallbackFunc *callback)
    : callback(callback), error(0)
{
    assert(url);
    this->url = Str::Dup(url);
    data = new Str::Str<char>(256);
    if (callback)
        hThread = CreateThread(NULL, 0, HttpDownloadThread, this, 0, 0);
    else
        HttpDownloadThread(this);
}

static bool HttpGet(TCHAR *url, Str::Str<char> *dataOut)
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

    char *buf = dataOut->MakeSpaceAtNoLenIncrease(dataOut->Count(), 1024);
    DWORD dwRead;
    do {
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

DWORD WINAPI HttpReqCtx::HttpDownloadThread(LPVOID data)
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
