#ifndef HTTP_H__
#define HTTP_H__

/* Copyright Krzysztof Kowalczyk 2006-2011
   License: GPLv3 */

#include "tstr_util.h"
#include "Vec.h"

class HttpReqCtx {
public:
    // the window to which we'll send notification about completed download
    HWND          hwndToNotify;
    // message to send when download is complete
    UINT          msg;

    TCHAR *       url;
    Vec<char> *   data;
    /* true for automated check, false for check triggered from menu */
    bool          silent;

    HANDLE        hThread;

    HttpReqCtx(const TCHAR *url, HWND hwnd, UINT msg, bool silent=false)
        : hwndToNotify(hwnd), msg(msg), silent(silent), hThread(NULL) {
        assert(url);
        this->url = tstr_dup(url);
        data = new Vec<char>(256, 1);
    }
    ~HttpReqCtx() {
        free(url);
        delete data;
        CloseHandle(hThread);
    }
};

void StartHttpDownload(const TCHAR *url, HWND hwndToNotify, UINT msg, bool silent);
#endif

