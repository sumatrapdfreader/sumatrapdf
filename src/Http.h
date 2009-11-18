#ifndef HTTP_H__
#define HTTP_H__

/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv2 */

#include <Wininet.h>
#include "MemSegment.h"

// based on information in http://www.codeproject.com/KB/IP/asyncwininet.aspx
class HttpReqCtx {
public:
    // the window to which we'll send notification about completed download
    HWND          hwndToNotify;
    // message to send when download is complete
    UINT          msg;
    // handle for connection during request processing
    HINTERNET     httpFile;

    TCHAR *       url;
    MemSegment    data;
    /* true for automated check, false for check triggered from menu */
    bool          autoCheck;

    HttpReqCtx(const TCHAR *_url, HWND _hwnd, UINT _msg) {
        assert(_url);
        hwndToNotify = _hwnd;
        url = tstr_dup(_url);
        msg = _msg;
        autoCheck = false;
        httpFile = 0;
    }
    ~HttpReqCtx() {
        free(url);
        data.freeAll();
    }
};

bool WininetInit();
void WininetDeinit();

void StartHttpDownload(const TCHAR *url, HWND hwndToNotify, UINT msg, bool autoCheck);
#endif

