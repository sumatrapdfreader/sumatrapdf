/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef StrUtil_h
#define StrUtil_h

// TODO: temporary
WCHAR * wstr_printf(const WCHAR *format, ...);

void    win32_dbg_out(const char *format, ...);
void    win32_dbg_out_hex(const char *dsc, const unsigned char *data, int dataLen);

#ifdef DEBUG
  #define DBG_OUT win32_dbg_out
  #define DBG_OUT_HEX win32_dbg_out_hex
#else
  #define DBG_OUT(...) NoOp()
  #define DBG_OUT_HEX(...) NoOp()
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

inline const char * FindChar(const char *str, const char c) {
    return strchr(str, c);
}
inline const WCHAR * FindChar(const WCHAR *str, const WCHAR c) {
    return wcschr(str, c);
}

}

static inline bool ChrIsDigit(const WCHAR c)
{
    return '0' <= c && c <= '9';
}

// I would like to remove the usage of *str_copy* and *str_cat* completely,
// using either Str class or Str::Join() etc.
// Using fixed size buffers is a known receipt for buffer overruns
int     str_copy(char *dst, size_t dst_cch_size, const char *src);
char *  str_cat_s(char *dst, size_t dst_cch_size, const char *src);


char *  str_printf(const char *format, ...);
int     str_printf_s(char *out, size_t out_cch_size, const char *format, ...);

char *  mem_to_hexstr(const unsigned char *buf, int len);
bool    hexstr_to_mem(const char *s, unsigned char *buf, int bufLen);
#define _mem_to_hexstr(ptr) mem_to_hexstr((const unsigned char *)ptr, sizeof(*ptr))
#define _hexstr_to_mem(str, ptr) hexstr_to_mem(str, (unsigned char *)ptr, sizeof(*ptr))

char *  str_to_multibyte(const char *src, UINT CodePage);
char *  multibyte_to_str(const char *src, UINT CodePage);
#define str_to_utf8(src) str_to_multibyte((src), CP_UTF8)
#define utf8_to_str(src) multibyte_to_str((src), CP_UTF8)

#ifdef DEBUG
void StrUtil_test(void);
#endif

#endif
