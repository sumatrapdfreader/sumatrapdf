/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class HttpRsp {
  public:
    AutoFreeStr url;
    str::Str data;
    DWORD error = (DWORD)-1;
    DWORD httpStatusCode = (DWORD)-1;

    HttpRsp() = default;
};

bool HttpRspOk(const HttpRsp*);

bool HttpPost(const char* server, int port, const char* url, str::Str* headers, str::Str* data);
bool HttpGet(const char* url, HttpRsp* rspOut);
bool HttpGetToFile(const char* url, const char* destFilePath);
