/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef TSTR_UTIL_H_
#define TSTR_UTIL_H_

/* currently, we always need both of these:
 * - str_util.h for DBG_OUT and
 * - wstr_util.h for multibyte_to_wstr and wstr_to_multibyte */
#include "str_util.h"
#include "wstr_util.h"

#ifdef _UNICODE
  #define tstr_len      wcslen
  #define tstr_dup      wstr_dup
  #define tstr_dupn     wstr_dupn
  #define tstr_cat_s    wstr_cat_s
  #define tstr_catn_s   wstr_catn_s
  #define tstr_cat      wstr_cat
  #define tstr_cat3     wstr_cat3
  #define tstr_cat4     wstr_cat4
  #define tstr_copy     wstr_copy
  #define tstr_copyn    wstr_copyn
  #define tstr_startswith wstr_startswith
  #define tstr_endswithi wstr_endswithi
  #define tstr_startswithi wstr_startswithi
  #define tstr_url_encode wstr_url_encode
  #define tchar_needs_url_escape wchar_needs_url_escape
  #define tstr_contains wstr_contains
  #define tstr_printf   wstr_printf
  #define tstr_eq       wstr_eq
  #define tstr_ieq      wstr_ieq
  #define tstr_empty    wstr_empty
  #define tstr_find_char wstr_find_char
  #define tstr_skip     wstr_skip
  #define tstr_copy_skip_until     wstr_copy_skip_until
  #define tstr_parse_possibly_quoted  wstr_parse_possibly_quoted
  #define tstr_trans_chars wstr_trans_chars
  #define tstr_dup_replace wstr_dup_replace
  #define multibyte_to_tstr(src,CodePage)             multibyte_to_wstr((src), (CodePage))
  #define tstr_to_multibyte(src,CodePage)             wstr_to_multibyte((src), (CodePage))
  #define wstr_to_tstr(src)                           wstr_dup((LPCWSTR)src);
  #define tstr_to_wstr(src)                           wstr_dup((LPCWSTR)src);
  #define hex_tstr_decode_byte                        hex_wstr_decode_byte
#else
  #define tstr_len      strlen
  #define tstr_dup      str_dup
  #define tstr_dupn     str_dupn
  #define tstr_cat_s    str_cat_s
  #define tstr_catn_s   str_catn_s
  #define tstr_cat      str_cat
  #define tstr_cat3     str_cat3
  #define tstr_cat4     str_cat4
  #define tstr_copy     str_copy
  #define tstr_copyn    str_copyn
  #define tstr_startswith str_startswith
  #define tstr_endswithi str_endswithi
  #define tstr_startswithi str_startswithi
  #define tstr_url_encode str_url_encode
  #define tchar_needs_url_escape char_needs_url_escape
  #define tstr_contains str_contains
  #define tstr_printf   str_printf
  #define tstr_eq       str_eq
  #define tstr_ieq      str_ieq
  #define tstr_empty    str_empty
  #define tstr_find_char  str_find_char
  #define tstr_skip     str_skip
  #define tstr_copy_skip_until     str_copy_skip_until
  #define tstr_parse_possibly_quoted  str_parse_possibly_quoted
  #define tstr_trans_chars str_trans_chars
  #define tstr_dup_replace str_dup_replace  
  #define multibyte_to_tstr(src,CodePage)             multibyte_to_str((src), (CodePage))
  #define tstr_to_multibyte(src,CodePage)             str_to_multibyte((src), (CodePage))
  #define wstr_to_tstr(src)                           wstr_to_multibyte((src), CP_ACP)
  #define tstr_to_wstr(src)                           multibyte_to_wstr((src), CP_ACP)
  #define hex_tstr_decode_byte                        hex_str_decode_byte
#endif

#define utf8_to_tstr(src) multibyte_to_tstr((src), CP_UTF8)
#define tstr_to_utf8(src) tstr_to_multibyte((src), CP_UTF8)

#endif
