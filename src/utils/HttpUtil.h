/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef HttpUtil_h
#define HttpUtil_h

bool  HttpPost(const WCHAR *server, const WCHAR *url, str::Str<char> *headers, str::Str<char> *data);
DWORD HttpGet(const WCHAR *url, str::Str<char> *dataOut);
bool  HttpGetToFile(const WCHAR *url, const WCHAR *destFilePath);

class HttpReqCallback;

class HttpReq {
    HANDLE          thread;
    // the callback to execute when the download is complete
    HttpReqCallback*callback;

    static DWORD WINAPI DownloadThread(LPVOID data);

public:
    WCHAR *         url;
    str::Str<char> *data;
    DWORD           error;

    explicit HttpReq(const WCHAR *url, HttpReqCallback *callback=NULL);
    ~HttpReq();
};

class HttpReqCallback {
public:
    virtual void Callback(HttpReq *ctx=NULL) = 0;
};

#endif
