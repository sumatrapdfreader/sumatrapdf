/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv2 */

#include "SumatraPDF.h"

#include "Http.h"

static HINTERNET g_hOpen = NULL;

bool WininetInit()
{
    if (!g_hOpen)
        g_hOpen = InternetOpen(APP_NAME_STR, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);
    if (NULL == g_hOpen) {
        DBG_OUT("InternetOpen() failed\n");
        return false;
    }
    return true;
}

void WininetDeinit()
{
    if (g_hOpen)
        InternetCloseHandle(g_hOpen);
}

static void __stdcall InternetCallbackProc(HINTERNET hInternet,
                        DWORD_PTR dwContext,
                        DWORD dwInternetStatus,
                        LPVOID statusInfo,
                        DWORD statusLen)
{
    char buf[256];
    INTERNET_ASYNC_RESULT* res;
    HttpReqCtx *ctx = (HttpReqCtx*)dwContext;

    switch (dwInternetStatus)
    {
        case INTERNET_STATUS_HANDLE_CREATED:
            res = (INTERNET_ASYNC_RESULT*)statusInfo;
            ctx->httpFile = (HINTERNET)(res->dwResult);

            _snprintf(buf, 256, "HANDLE_CREATED (%d)", statusLen );
            break;

        case INTERNET_STATUS_REQUEST_COMPLETE:
        {
            // Check for errors.
            if (LPINTERNET_ASYNC_RESULT(statusInfo)->dwError != 0)
            {
                _snprintf(buf, 256, "REQUEST_COMPLETE (%d) Error (%d) encountered", statusLen, GetLastError());
                break;
            }

            // Set the resource handle to the HINTERNET handle returned in the callback.
            HINTERNET hInt = HINTERNET(LPINTERNET_ASYNC_RESULT(statusInfo)->dwResult);
            assert(hInt == ctx->httpFile);

            _snprintf(buf, 256, "REQUEST_COMPLETE (%d)", statusLen);

            INTERNET_BUFFERS ib = {0};
            ib.dwStructSize = sizeof(ib);
            ib.lpvBuffer = malloc(1024);

            // This is not exactly async, but we're assuming it'll complete quickly
            // because the update file is small and we now that connection is working
            // since we already got headers back
            BOOL ok;
            while (TRUE) {
                ib.dwBufferLength = 1024;
                ok = InternetReadFileEx(ctx->httpFile, &ib, IRF_ASYNC, (LPARAM)ctx);
                if (ok || (!ok && GetLastError()==ERROR_IO_PENDING)) {
                    DWORD readSize = ib.dwBufferLength;
                    if (readSize > 0) {
                        ctx->data.add(ib.lpvBuffer, readSize);
                    }
                }
                if (ok || GetLastError()!=ERROR_IO_PENDING)
                    break; // read the whole file or error
            }
            free(ib.lpvBuffer);
            InternetCloseHandle(ctx->httpFile);
            ctx->httpFile = 0;
            if (ok) {
                // read the whole file
                PostMessage(ctx->hwndToNotify, ctx->msg, (WPARAM) ctx, 0);
            } else {
                delete ctx;
            }
        }
        break;

#if 0
        case INTERNET_STATUS_CLOSING_CONNECTION:
            _snprintf(buf, 256, "CLOSING_CONNECTION (%d)", statusLen);
            break;

        case INTERNET_STATUS_CONNECTED_TO_SERVER:
            _snprintf(buf, 256, "CONNECTED_TO_SERVER (%d)", statusLen);
            break;

        case INTERNET_STATUS_CONNECTING_TO_SERVER:
            _snprintf(buf, 256, "CONNECTING_TO_SERVER (%d)", statusLen);
            break;

        case INTERNET_STATUS_CONNECTION_CLOSED:
            _snprintf(buf, 256, "CONNECTION_CLOSED (%d)", statusLen);
            break;

        case INTERNET_STATUS_HANDLE_CLOSING:
            _snprintf(buf, 256, "HANDLE_CLOSING (%d)", statusLen);
            break;

        case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
            _snprintf(buf, 256, "INTERMEDIATE_RESPONSE (%d)", statusLen );
            break;

        case INTERNET_STATUS_NAME_RESOLVED:
            _snprintf(buf, 256, "NAME_RESOLVED (%d)", statusLen);
            break;

        case INTERNET_STATUS_RECEIVING_RESPONSE:
            _snprintf(buf, 256, "RECEIVING_RESPONSE (%d)",statusLen);
            break;

        case INTERNET_STATUS_RESPONSE_RECEIVED:
            _snprintf(buf, 256, "RESPONSE_RECEIVED (%d)", statusLen);
            break;

        case INTERNET_STATUS_REDIRECT:
            _snprintf(buf, 256, "REDIRECT (%d)", statusLen);
            break;

        case INTERNET_STATUS_REQUEST_SENT:
            _snprintf(buf, 256, "REQUEST_SENT (%d)", statusLen);
            break;

        case INTERNET_STATUS_RESOLVING_NAME:
            _snprintf(buf, 256, "RESOLVING_NAME (%d)", statusLen);
            break;

        case INTERNET_STATUS_SENDING_REQUEST:
            _snprintf(buf, 256, "SENDING_REQUEST (%d)", statusLen);
            break;

        case INTERNET_STATUS_STATE_CHANGE:
            _snprintf(buf, 256, "STATE_CHANGE (%d)", statusLen);
            break;
#else
        case INTERNET_STATUS_CLOSING_CONNECTION:
        case INTERNET_STATUS_CONNECTED_TO_SERVER:
        case INTERNET_STATUS_CONNECTING_TO_SERVER:
        case INTERNET_STATUS_CONNECTION_CLOSED:
        case INTERNET_STATUS_HANDLE_CLOSING:
        case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
        case INTERNET_STATUS_NAME_RESOLVED:
        case INTERNET_STATUS_RECEIVING_RESPONSE:
        case INTERNET_STATUS_RESPONSE_RECEIVED:
        case INTERNET_STATUS_REDIRECT:
        case INTERNET_STATUS_REQUEST_SENT:
        case INTERNET_STATUS_RESOLVING_NAME:
        case INTERNET_STATUS_SENDING_REQUEST:
        case INTERNET_STATUS_STATE_CHANGE:
            return;
#endif
        default:
            _snprintf(buf, 256, "Unknown: Status %d Given", dwInternetStatus);
            break;
    }

    DBG_OUT(buf);
    DBG_OUT("\n");
}

void StartHttpDownload(const TCHAR *url, HWND hwndToNotify, UINT msg, bool autoCheck)
{
    HttpReqCtx *ctx = new HttpReqCtx(url, hwndToNotify, msg);
    ctx->autoCheck = autoCheck;

    InternetSetStatusCallback(g_hOpen, (INTERNET_STATUS_CALLBACK)InternetCallbackProc);
    HINTERNET urlHandle = InternetOpenUrl(g_hOpen, url, NULL, 0, 
      INTERNET_FLAG_RELOAD | INTERNET_FLAG_PRAGMA_NOCACHE | 
      INTERNET_FLAG_NO_CACHE_WRITE, (LPARAM)ctx);
    /* MSDN says NULL result from InternetOpenUrl() means an error, but in my testing
       in async mode InternetOpenUrl() returns NULL and error is ERROR_IO_PENDING */
    if (!urlHandle && (GetLastError() != ERROR_IO_PENDING)) {
        DBG_OUT("InternetOpenUrl() failed\n");
        delete ctx;
    }
}
