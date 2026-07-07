/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Http.h"

HttpRsp::~HttpRsp() {
    str::Free(url);
}

bool IsHttpRspOk(const HttpRsp* rsp) {
    if (rsp->error != 0) {
        logf("HttpRspOk: rsp->error %d, should be 0\n", (int)rsp->error);
        return false;
    }
    if (rsp->httpStatusCode >= 300) {
        logf("HttpRspOk: rsp->httpStatusCode: %d\n", (int)rsp->httpStatusCode);
        return false;
    }
    return true;
}
