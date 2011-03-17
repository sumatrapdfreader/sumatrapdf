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
    data = new Vec<char>(256, 1);
    if (callback)
        hThread = CreateThread(NULL, 0, HttpDownloadThread, this, 0, 0);
    else
        HttpDownloadThread(this);
}

DWORD WINAPI HttpReqCtx::HttpDownloadThread(LPVOID data)
{
    HttpReqCtx *ctx = (HttpReqCtx *)data;

    HINTERNET hFile = NULL;
    HINTERNET hInet = InternetOpen(APP_NAME_STR _T("/") CURR_VERSION_STR,
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet)
        goto DownloadError;

    hFile = InternetOpenUrl(hInet, ctx->url, NULL, 0, 0, 0);
    if (!hFile)
        goto DownloadError;

    char buffer[1024];
    DWORD dwRead;
    do {
        if (!InternetReadFile(hFile, buffer, sizeof(buffer), &dwRead))
            goto DownloadError;
        ctx->data->Append(buffer, dwRead);
    } while (dwRead > 0);

DownloadComplete:
    InternetCloseHandle(hFile);
    InternetCloseHandle(hInet);

    if (ctx->callback)
        ctx->callback->Callback(ctx);

    return 0;

DownloadError:
    ctx->error = GetLastError();
    if (ctx->error == 0)
        ctx->error = ERROR_GEN_FAILURE;
    goto DownloadComplete;
}
