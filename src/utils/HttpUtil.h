/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HttpRsp {
    ScopedMem<WCHAR> url;
    str::Str<char>  data;
    DWORD           error;
    DWORD           httpStatusCode;
};

bool HttpRspOk(const HttpRsp*);

bool  HttpPost(const WCHAR *server, const WCHAR *url, str::Str<char> *headers, str::Str<char> *data);
bool  HttpGet(const WCHAR *url, HttpRsp *rspOut);
bool  HttpGetToFile(const WCHAR *url, const WCHAR *destFilePath);
//void  HttpGetAsync(const char *url, const std::function<void(HttpRsp *)> &);
void  HttpGetAsync(const WCHAR *url, const std::function<void(HttpRsp *)> &);

