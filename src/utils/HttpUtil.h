/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class HttpRsp {
  public:
    AutoFreeW url;
    str::Str<char> data;
    DWORD error = (DWORD)-1;
    DWORD httpStatusCode = (DWORD)-1;

    HttpRsp() {}
};

bool HttpRspOk(const HttpRsp*);

bool HttpPost(const WCHAR* server, int port, const WCHAR* url, str::Str<char>* headers, str::Str<char>* data);
bool HttpGet(const WCHAR* url, HttpRsp* rspOut);
bool HttpGetToFile(const WCHAR* url, const WCHAR* destFilePath);
// void  HttpGetAsync(const char *url, const std::function<void(HttpRsp *)> &);
void HttpGetAsync(const WCHAR* url, const std::function<void(HttpRsp*)>&);
