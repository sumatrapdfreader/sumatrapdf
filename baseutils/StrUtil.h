/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef StrUtil_h
#define StrUtil_h

#ifdef _MSC_VER
#include "stdint.h"
#endif

/* DOS is 0xd 0xa */
#define DOS_NEWLINE "\x0d\x0a"

void    win32_dbg_out(const char *format, ...);
void    win32_dbg_out_hex(const char *dsc, const unsigned char *data, int dataLen);

#ifdef DEBUG
  #define DBG_OUT win32_dbg_out
  #define DBG_OUT_HEX win32_dbg_out_hex
#else
  #define DBG_OUT(...) NoOp()
  #define DBG_OUT_HEX(...) NoOp()
#endif

#define str_find_char strchr

/* Note: this demonstrates how eventually I would like to get rid of
   tstr_* and wstr_* functions and instead rely on C++'s ability
   to use overloaded functions and only have Str* functions */

static inline size_t StrLen(const char *s)
{
    return strlen(s);
}

static inline size_t StrLen(const WCHAR *s)
{
    return wcslen(s);
}

// Unfortunately can't use StrCopy() because <shlwapi.h> #defines it to
// shlwapi's StrDupA() or StrDupW() and we want C++ function overloading
// to pick up the right one
// TODO: maybe those should be Str::* instead of Str* to avoid conflicts ?

static inline char *StrCopy(const char *s)
{
    return _strdup(s);
}

static inline WCHAR *StrCopy(const WCHAR *s)
{
    return _wcsdup(s);
}

// TODO: make these return bool instead of int
int     str_eq(const char *str1, const char *str2);
int     str_ieq(const char *str1, const char *str2);
int     str_eqn(const char *str1, const char *str2, size_t len);
int     str_startswith(const char *str, const char *txt);
int     str_startswithi(const char *str, const char *txt);
int     str_endswith(const char *str, const char *end);
int     str_endswithi(const char *str, const char *end);
int     str_empty(const char *str);
int     str_copy(char *dst, size_t dst_cch_size, const char *src);
int     str_copyn(char *dst, size_t dst_cch_size, const char *src, size_t src_cch_size);
char *  str_dupn(const char *str, size_t len);
char *  str_cat_s(char *dst, size_t dst_cch_size, const char *src);
char *  str_catn_s(char *dst, size_t dst_cch_size, const char *src, size_t src_cch_size);
char *  str_cat(const char *str1, const char *str2);
char *  str_printf(const char *format, ...);
int     str_printf_s(char *out, size_t out_cch_size, const char *format, ...);

char *  mem_to_hexstr(const unsigned char *buf, int len);
BOOL    hexstr_to_mem(const char *s, unsigned char *buf, int bufLen);
#define _mem_to_hexstr(ptr) mem_to_hexstr((const unsigned char *)ptr, sizeof(*ptr))
#define _hexstr_to_mem(str, ptr) hexstr_to_mem(str, (unsigned char *)ptr, sizeof(*ptr))

char *  str_to_multibyte(const char *src, UINT CodePage);
char *  multibyte_to_str(const char *src, UINT CodePage);

#ifdef DEBUG
void StrUtil_test(void);
#endif

#endif
