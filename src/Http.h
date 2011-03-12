/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Http_h
#define Http_h

#include "TStrUtil.h"
#include "Vec.h"

class HttpReqCtx {
    HANDLE          hThread;
    static DWORD WINAPI HttpDownloadThread(LPVOID data);

public:
    // the callback to execute when the download is complete
    CallbackFunc *  callback;

    TCHAR *         url;
    Vec<char> *     data;
    DWORD           error;

    HttpReqCtx(const TCHAR *url, CallbackFunc *callback=NULL);
    ~HttpReqCtx() {
        free(url);
        delete data;
        CloseHandle(hThread);
    }
};

#endif
