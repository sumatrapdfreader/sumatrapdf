/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef StrUtil_h
#define StrUtil_h

namespace Str {

inline size_t Len(const char *s) { return strlen(s); }
inline size_t Len(const WCHAR *s) { return wcslen(s); }

inline char *  Dup(const char *s) { return _strdup(s); }
inline WCHAR * Dup(const WCHAR *s) { return _wcsdup(s); }

char *  Join(const char *s1, const char *s2, const char *s3=NULL);
WCHAR * Join(const WCHAR *s1, const WCHAR *s2, const WCHAR *s3=NULL);

bool Eq(const char *s1, const char *s2);
bool Eq(const WCHAR *s1, const WCHAR *s2);
bool EqI(const char *s1, const char *s2);
bool EqI(const WCHAR *s1, const WCHAR *s2);
bool EqN(const char *s1, const char *s2, size_t len);
bool EqN(const WCHAR *s1, const WCHAR *s2, size_t len);

template <typename T>
inline bool IsEmpty(T *s) {
    return !s || (0 == *s);
}

template <typename T>
inline bool StartsWith(const T* str, const T* txt) {
    return EqN(str, txt, Len(txt));
}

bool StartsWithI(const char *str, const char *txt);
bool StartsWithI(const WCHAR *str, const WCHAR *txt);
bool EndsWith(const char *txt, const char *end);
bool EndsWith(const WCHAR *txt, const WCHAR *end);
bool EndsWithI(const char *txt, const char *end);
bool EndsWithI(const WCHAR *txt, const WCHAR *end);

char *  DupN(const char *s, size_t lenCch);
WCHAR * DupN(const WCHAR *s, size_t lenCch);

char *  ToMultiByte(const WCHAR *txt, UINT CodePage);
char *  ToMultiByte(const char *src, UINT CodePageSrc, UINT CodePageDest);
WCHAR * ToWideChar(const char *src, UINT CodePage);

inline const char * FindChar(const char *str, const char c) {
    return strchr(str, c);
}
inline const WCHAR * FindChar(const WCHAR *str, const WCHAR c) {
    return wcschr(str, c);
}

char *  FmtV(const char *fmt, va_list args);
char *  Format(const char *fmt, ...);
WCHAR * FmtV(const WCHAR *fmt, va_list args);
WCHAR * Format(const WCHAR *fmt, ...);

size_t  TransChars(char *str, const char *oldChars, const char *newChars);
size_t  TransChars(WCHAR *str, const WCHAR *oldChars, const WCHAR *newChars);

size_t  BufSet(char *dst, size_t dstCchSize, const char *src);
size_t  BufSet(WCHAR *dst, size_t dstCchSize, const WCHAR *src);

char *  MemToHex(const unsigned char *buf, int len);
bool    HexToMem(const char *s, unsigned char *buf, int bufLen);

#ifdef DEBUG
void    DbgOut(const TCHAR *format, ...);
#endif

class Parser {
    const TCHAR *pos;

public:
    Parser() : pos(NULL) { }

    bool            Init(const TCHAR *pos);
    bool            Skip(const TCHAR *str, const TCHAR *alt=NULL);
    bool            CopyUntil(TCHAR c, TCHAR *buffer, size_t bufSize);
    TCHAR *         ExtractUntil(TCHAR c);
    bool            Scan(const TCHAR *format, ...);
    const TCHAR *   Peek() { return pos; }
};

namespace Conv {

#ifdef _UNICODE
inline TCHAR *  FromUtf8(const char *src) { return ToWideChar(src, CP_UTF8); }
inline char *   ToUtf8(const TCHAR *src) { return ToMultiByte(src, CP_UTF8); }
inline TCHAR *  FromAnsi(const char *src) { return ToWideChar(src, CP_ACP); }
inline char *   ToAnsi(const TCHAR *src) { return ToMultiByte(src, CP_ACP); }
inline TCHAR *  FromWStr(const WCHAR *src) { return Dup(src); }
inline WCHAR *  ToWStr(const TCHAR *src) { return Dup(src); }
inline TCHAR *  FromWStrQ(WCHAR *src) { return src; }
inline WCHAR *  ToWStrQ(TCHAR *src) { return src; }
#else
inline TCHAR *  FromUtf8(const char *src) { return ToMultiByte(src, CP_UTF8, CP_ACP); }
inline char *   ToUtf8(const TCHAR *src) { return ToMultiByte(src, CP_ACP, CP_UTF8); }
inline TCHAR *  FromAnsi(const char *src) { return Dup(src); }
inline char *   ToAnsi(const TCHAR *src) { return Dup(src); }
inline TCHAR *  FromWStr(const WCHAR *src) { return ToMultiByte(src, CP_ACP); }
inline WCHAR *  ToWStr(const TCHAR *src) { return ToWideChar(src, CP_ACP); }
inline TCHAR *FromWStrQ(WCHAR *src) {
    if (!src) return NULL;
    char *str = FromWStr(src);
    free(src);
    return str;
}
inline WCHAR *ToWStrQ(TCHAR *src) {
    if (!src) return NULL;
    WCHAR *str = ToWStr(src);
    free(src);
    return str;
}
#endif

}

}

inline bool ChrIsDigit(const WCHAR c)
{
    return '0' <= c && c <= '9';
}

#define _MemToHex(ptr) Str::MemToHex((const unsigned char *)(ptr), sizeof(*ptr))
#define _HexToMem(str, ptr) Str::HexToMem(str, (unsigned char *)(ptr), sizeof(*ptr))

#ifndef DEBUG
  #define DBG_OUT(format, ...) NoOp()
#elif UNICODE
  #define DBG_OUT(format, ...) Str::DbgOut(L##format, __VA_ARGS__)
#else
  #define DBG_OUT(format, ...) Str::DbgOut(format, __VA_ARGS__)
#endif

#ifdef _UNICODE
  #define CF_T_TEXT CF_UNICODETEXT
#else
  #define CF_T_TEXT CF_TEXT
#endif

#endif
