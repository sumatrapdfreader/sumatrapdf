#ifndef Util_h
#define Util_h

#define logf(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

void log(const char *s);

class IDiaDataSource;

IDiaDataSource *LoadDia();

const char* BStrToString(str::Str<char>& strInOut, BSTR str, char *defString = "", bool stripWhitespace = false);

class StringInterner {
    dict::MapStrToInt   strToInt;
    Vec<const char *>   intToStr;
    int                 nTotalStrings; // just so we know how effective interning is

public:
    StringInterner() : nTotalStrings(0) {}
    int             Intern(const char *s);
    const char *    GetByIndex(int n) const { return intToStr.At(n); }
};

#endif
