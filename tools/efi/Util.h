/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Util_h
#define Util_h

#define logf(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

void log(const char *s);

class IDiaDataSource;

IDiaDataSource *LoadDia();

void BStrToString(str::Str& strInOut, BSTR str, const char *defString = "", bool stripWhitespace = false);

#endif
