/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Http_h
#define Http_h

#include "StrUtil.h"
#include "Vec.h"

bool HttpPost(char *server, char *url, Str::Str<char> *headers, Str::Str<char> *data);
// returns ERROR_SUCCESS or an error code
DWORD HttpGet(TCHAR *url, Str::Str<char> *dataOut);

class HttpReqCtx {
    HANDLE          hThread;

public:
    // the callback to execute when the download is complete
    CallbackFunc *  callback;

    TCHAR *         url;
    Str::Str<char> *data;
    DWORD           error;

    HttpReqCtx(const TCHAR *url, CallbackFunc *callback=NULL);
    ~HttpReqCtx() {
        free(url);
        delete data;
        CloseHandle(hThread);
    }
};

#endif
