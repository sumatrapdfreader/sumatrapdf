/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef TSTR_UTIL_H_
#define TSTR_UTIL_H_

/* currently, we always need both of these:
 * - str_util.h for DBG_OUT and
 * - wstr_util.h for multibyte_to_wstr and wstr_to_multibyte */
#include "str_util.h"
#include "wstr_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef WIN32
#define DIR_SEP_TSTR _T(DIR_SEP_STR)
#ifdef UNICODE
#define CF_T_TEXT CF_UNICODETEXT
#else
#define CF_T_TEXT CF_TEXT
#endif
#endif

#ifdef _UNICODE
  typedef wchar_t tchar_t;
  #define tchar_is_ws   wchar_is_ws
  #define tstr_len      wstr_len
  #define tstr_dup      wstr_dup
  #define tstr_find_char wstr_find_char

  #define tstr_eq       wstr_eq
  #define tstr_ieq      wstr_ieq
  #define tstr_eqn      wstr_eqn
  #define tstr_startswith wstr_startswith
  #define tstr_startswithi wstr_startswithi
  #define tstr_endswith wstr_endswith
  #define tstr_endswithi wstr_endswithi
  #define tstr_empty    wstr_empty
  #define tstr_copy     wstr_copy
  #define tstr_copyn    wstr_copyn
  #define tstr_dupn     wstr_dupn
  #define tstr_cat_s    wstr_cat_s
  #define tstr_catn_s   wstr_catn_s
  #define tstr_cat      wstr_cat
  #define tstr_cat3     wstr_cat3
  #define tstr_contains wstr_contains
  #define tstr_printf   wstr_printf
  #define tstr_printf_s wstr_printf_s
  #define tstr_dup_replace wstr_dup_replace

  #define multibyte_to_tstr(src, CodePage)  multibyte_to_wstr((src), (CodePage))
  #define tstr_to_multibyte(src, CodePage)  wstr_to_multibyte((src), (CodePage))
  #define wstr_to_tstr(src)                 wstr_dup((LPCWSTR)src);
  #define tstr_to_wstr(src)                 wstr_dup((LPCWSTR)src);
  #define DBG_OUT_T     DBG_OUT_W
#else
  typedef char tchar_t;
  #define tchar_is_ws   char_is_ws
  #define tstr_len      str_len
  #define tstr_dup      str_dup
  #define tstr_find_char  str_find_char

  #define tstr_eq       str_eq
  #define tstr_ieq      str_ieq
  #define tstr_eqn      str_eqn
  #define tstr_startswith str_startswith
  #define tstr_startswithi str_startswithi
  #define tstr_endswith str_endswith
  #define tstr_endswithi str_endswithi
  #define tstr_empty    str_empty
  #define tstr_copy     str_copy
  #define tstr_copyn    str_copyn
  #define tstr_dupn     str_dupn
  #define tstr_cat_s    str_cat_s
  #define tstr_catn_s   str_catn_s
  #define tstr_cat      str_cat
  #define tstr_cat3     str_cat3
  #define tstr_contains str_contains
  #define tstr_printf   str_printf
  #define tstr_printf_s str_printf_s
  #define tstr_dup_replace str_dup_replace

  #define multibyte_to_tstr(src, CodePage)  multibyte_to_str((src), (CodePage))
  #define tstr_to_multibyte(src, CodePage)  str_to_multibyte((src), (CodePage))
  #define wstr_to_tstr(src)                 wstr_to_multibyte((src), CP_ACP)
  #define tstr_to_wstr(src)                 multibyte_to_wstr((src), CP_ACP)
  #define DBG_OUT_T     DBG_OUT
#endif

#define   utf8_to_tstr(src) multibyte_to_tstr((src), CP_UTF8)
#define   tstr_to_utf8(src) tstr_to_multibyte((src), CP_UTF8)
#define   ansi_to_tstr(src) multibyte_to_tstr((src), CP_ACP)
#define   tstr_to_ansi(src) tstr_to_multibyte((src), CP_ACP)

int       tstr_trans_chars(tchar_t *str, const tchar_t *oldChars, const tchar_t *newChars);
tchar_t * tstr_url_encode(const tchar_t *str);
int       tstr_skip(const tchar_t **strp, const tchar_t *expect);
int       tstr_copy_skip_until(const tchar_t **strp, tchar_t *dst, size_t dst_size, tchar_t stop);
tchar_t * tstr_parse_possibly_quoted(tchar_t **txt);

#ifdef __cplusplus
}
#endif
#endif
