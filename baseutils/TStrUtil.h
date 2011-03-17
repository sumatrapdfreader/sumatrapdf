/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef TStrUtil_h
#define TStrUtil_h

// TODO: integrate TStrUtil into StrUtil
#include "StrUtil.h"

#ifdef UNICODE
#define CF_T_TEXT CF_UNICODETEXT
#else
#define CF_T_TEXT CF_TEXT
#endif

#ifdef _UNICODE
  #define utf8_to_tstr(src) Str::ToWideChar((src), CP_UTF8)
  #define tstr_to_utf8(src) Str::ToMultiByte((src), CP_UTF8)
  #define ansi_to_tstr(src) Str::ToWideChar((src), CP_ACP)
  #define tstr_to_ansi(src) Str::ToMultiByte((src), CP_ACP)
  #define wstr_to_tstr(src) Str::Dup(src)
  #define tstr_to_wstr(src) Str::Dup(src)
  #define DBG_OUT_T         DBG_OUT_W

  #define wstr_to_tstr_q(src)   (src)
  #define tstr_to_wstr_q(src)   (src)
#else
  #define utf8_to_tstr(src) Str::ToMultiByte((src), CP_UTF8, CP_ACP)
  #define tstr_to_utf8(src) Str::ToMultiByte((src), CP_ACP, CP_UTF8)
  #define ansi_to_tstr(src) Str::ToMultiByte((src), CP_ACP, CP_ACP)
  #define tstr_to_ansi(src) Str::ToMultiByte((src), CP_ACP, CP_ACP)
  #define wstr_to_tstr(src) Str::ToMultiByte((src), CP_ACP)
  #define tstr_to_wstr(src) Str::ToWideChar((src), CP_ACP)
  #define DBG_OUT_T         DBG_OUT

static inline char *wstr_to_tstr_q(WCHAR *src)
{
    if (!src) return NULL;
    char *str = wstr_to_tstr(src);
    free(src);
    return str;
}
static inline WCHAR *tstr_to_wstr_q(char *src)
{
    if (!src) return NULL;
    WCHAR *str = tstr_to_wstr(src);
    free(src);
    return str;
}
#endif

int       tstr_trans_chars(TCHAR *str, const TCHAR *oldChars, const TCHAR *newChars);
TCHAR *   tstr_url_encode(const TCHAR *str);
int       tstr_skip(const TCHAR **strp, const TCHAR *expect);
int       tstr_copy_skip_until(const TCHAR **strp, TCHAR *dst, size_t dst_size, TCHAR stop);
TCHAR *   tstr_parse_possibly_quoted(TCHAR **txt);

#endif
