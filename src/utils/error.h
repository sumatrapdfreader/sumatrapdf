/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* emulating Go's error handling */
struct error {
    str::Str err;

    virtual ~error();
    virtual char* Error();
};

error* NewError(const char* s);
