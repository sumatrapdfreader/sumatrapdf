/* Copyright Krzysztof Kowalczyk 2006-2011
   License: GPLv3 */

#include "SumatraPDF.h"
#include "Http.h"
#include "Version.h"
#include <Wininet.h>

static DWORD WINAPI HttpDownloadThread(LPVOID data)
{
    HttpReqCtx *ctx = (HttpReqCtx *)data;
    DWORD dwError = 0;

    HINTERNET hFile = NULL;
    HINTERNET hInet = InternetOpen(APP_NAME_STR _T("/") CURR_VERSION_STR,
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInet) {
        dwError = GetLastError();
        goto DownloadError;
    }

    hFile = InternetOpenUrl(hInet, ctx->url, NULL, 0, 0, 0);
    if (!hFile) {
        dwError = GetLastError();
        goto DownloadError;
    }

    char buffer[1024];
    DWORD dwRead;
    do {
        if (!InternetReadFile(hFile, buffer, sizeof(buffer), &dwRead)) {
            dwError = GetLastError();
            goto DownloadError;
        }
        ctx->data.add(buffer, dwRead);
    } while (dwRead > 0);

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInet);

    PostMessage(ctx->hwndToNotify, ctx->msg, (WPARAM)ctx, 0);

    return 0;

DownloadError:
    InternetCloseHandle(hFile);
    InternetCloseHandle(hInet);

    if (!ctx->silent)
        PostMessage(ctx->hwndToNotify, ctx->msg, 0, dwError);
    delete ctx;
    return 1;
}

void StartHttpDownload(const TCHAR *url, HWND hwndToNotify, UINT msg, bool silent)
{
    HttpReqCtx *ctx = new HttpReqCtx(url, hwndToNotify, msg, silent);
    ctx->hThread = CreateThread(NULL, 0, HttpDownloadThread, ctx, 0, 0);
}
