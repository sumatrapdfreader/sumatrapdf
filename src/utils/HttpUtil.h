/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Http_h
#define Http_h

bool  HttpPost(const TCHAR *server, const TCHAR *url, str::Str<char> *headers, str::Str<char> *data);
DWORD HttpGet(const TCHAR *url, str::Str<char> *dataOut);
bool  HttpGetToFile(const TCHAR *url, const TCHAR *destFilePath);

class HttpReqCallback;

class HttpReq {
    HANDLE          thread;
    // the callback to execute when the download is complete
    HttpReqCallback*callback;

    static DWORD WINAPI DownloadThread(LPVOID data);

public:
    TCHAR *         url;
    str::Str<char> *data;
    DWORD           error;

    HttpReq(const TCHAR *url, HttpReqCallback *callback=NULL);
    ~HttpReq();
};

class HttpReqCallback {
public:
    virtual void Callback(HttpReq *ctx=NULL) = 0;
};

#endif
