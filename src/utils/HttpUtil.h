/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HttpRsp {
    str::Str<char>  data;
    DWORD           error;
    DWORD           httpStatusCode;
};

bool HttpRspOk(const HttpRsp*);

bool  HttpPost(const WCHAR *server, const WCHAR *url, str::Str<char> *headers, str::Str<char> *data);
bool  HttpGet(const WCHAR *url, HttpRsp *rspOut);
bool  HttpGetToFile(const WCHAR *url, const WCHAR *destFilePath);

class HttpReqCallback;

class HttpReq {
    HANDLE          thread;
    // the callback to execute when the download is complete
    HttpReqCallback*callback;

    static DWORD WINAPI DownloadThread(LPVOID data);

public:
    WCHAR *         url;
    HttpRsp         rsp;

    explicit HttpReq(const WCHAR *url, HttpReqCallback *callback=NULL);
    ~HttpReq();
};

class HttpReqCallback {
public:
    virtual void Callback(HttpReq *ctx=NULL) = 0;
    virtual ~HttpReqCallback() { }
};
