/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef Http_h
#define Http_h

#include "StrUtil.h"
#include "Vec.h"

bool  HttpPost(const TCHAR *server, const TCHAR *url, str::Str<char> *headers, str::Str<char> *data);
DWORD HttpGet(const TCHAR *url, str::Str<char> *dataOut);
bool  HttpGetToFile(const TCHAR *url, const TCHAR *destFilePath);

class HttpReqCallback;

class HttpReqCtx {
    HANDLE          hThread;

public:
    // the callback to execute when the download is complete
    HttpReqCallback *callback;

    TCHAR *         url;
    str::Str<char> *data;
    DWORD           error;

    HttpReqCtx(const TCHAR *url, HttpReqCallback *callback=NULL);
    ~HttpReqCtx() {
        free(url);
        delete data;
        CloseHandle(hThread);
    }
};

class HttpReqCallback {
public:
    virtual void Callback(HttpReqCtx *ctx=NULL) = 0;
};

#endif
