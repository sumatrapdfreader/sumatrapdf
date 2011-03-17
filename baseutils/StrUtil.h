/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef StrUtil_h
#define StrUtil_h

// TODO: temporary
void win32_dbg_outW(const WCHAR *format, ...);
#ifdef DEBUG
  #define DBG_OUT_W(format, ...) win32_dbg_outW(L##format, __VA_ARGS__)
#else
  #define DBG_OUT_W(format, ...) NoOp()
#endif


void    win32_dbg_out(const char *format, ...);

#ifdef DEBUG
  #define DBG_OUT win32_dbg_out
#else
  #define DBG_OUT(...) NoOp()
#endif

/* Note: this demonstrates how eventually I would like to get rid of
   tstr_* and wstr_* functions and instead rely on C++'s ability
   to use overloaded functions and only have Str::* functions */

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

char *  Format(const char *format, ...);
WCHAR * Format(const WCHAR *format, ...);

size_t  CopyTo(char *dst, size_t dstCchSize, const char *src);
size_t  CopyTo(WCHAR *dst, size_t dstCchSize, const WCHAR *src);

char *  MemToHex(const unsigned char *buf, int len);
bool    HexToMem(const char *s, unsigned char *buf, int bufLen);

}

inline bool ChrIsDigit(const WCHAR c)
{
    return '0' <= c && c <= '9';
}

#define _MemToHex(ptr) Str::MemToHex((const unsigned char *)(ptr), sizeof(*ptr))
#define _HexToMem(str, ptr) Str::HexToMem(str, (unsigned char *)(ptr), sizeof(*ptr))

#endif
