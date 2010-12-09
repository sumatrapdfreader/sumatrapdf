#ifndef HTTP_H__
#define HTTP_H__

/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv3 */

#include "MemSegment.h"
#include "tstr_util.h"

class HttpReqCtx {
public:
    // the window to which we'll send notification about completed download
    HWND          hwndToNotify;
    // message to send when download is complete
    UINT          msg;

    TCHAR *       url;
    MemSegment    data;
    /* true for automated check, false for check triggered from menu */
    bool          notifyErrors;

    HANDLE        hThread;

    HttpReqCtx(const TCHAR *url, HWND hwnd, UINT msg, bool notifyErrors=false)
        : hwndToNotify(hwnd), msg(msg), url(NULL),
          notifyErrors(notifyErrors), hThread(NULL) {
        assert(url);
        this->url = tstr_dup(url);
    }
    ~HttpReqCtx() {
        free(url);
        if (hThread)
            CloseHandle(hThread);
    }
};

void StartHttpDownload(const TCHAR *url, HWND hwndToNotify, UINT msg, bool autoCheck);
#endif

