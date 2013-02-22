#ifndef Util_h
#define Util_h

#define logf(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

void log(const char *s);

class IDiaDataSource;

IDiaDataSource *LoadDia();

void BStrToString(str::Str<char>& strInOut, BSTR str, char *defString = "", bool stripWhitespace = false);

#endif
