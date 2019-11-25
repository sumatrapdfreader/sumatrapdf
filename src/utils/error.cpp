/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

error* NewError(const char* s) {
    auto e = new error();
    e->err.Set(s);
    return e;
}

error::~error() {
}

char* error::Error() {
    return err.Get();
}
