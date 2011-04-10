/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see ./COPYING) */

#ifndef Http_h
#define Http_h

#include "StrUtil.h"
#include "Vec.h"

bool  HttpPost(const TCHAR *server, const TCHAR *url, Str::Str<char> *headers, Str::Str<char> *data);
DWORD HttpGet(const TCHAR *url, Str::Str<char> *dataOut);
bool  HttpGetToFile(const TCHAR *url, const TCHAR *destFilePath);

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
