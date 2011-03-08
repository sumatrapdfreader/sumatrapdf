/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef HTTP_H__
#define HTTP_H__

#include "TStrUtil.h"
#include "Vec.h"

class HttpReqCtx {
    HANDLE          hThread;

public:
    // the callback to execute when the download is complete
    CallbackFunc *  callback;

    TCHAR *         url;
    Vec<char> *     data;
    DWORD           error;

    HttpReqCtx(const TCHAR *url, CallbackFunc *callback);
    ~HttpReqCtx() {
        free(url);
        delete data;
        CloseHandle(hThread);
    }
};

#endif
