/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef WSTR_UTIL_H_
#define WSTR_UTIL_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define wstr_len wcslen
#define wstrlen wcslen
int     wstr_eq(const WCHAR *str1, const WCHAR *str2);
int     wstr_ieq(const WCHAR *str1, const WCHAR *str2);
int     wstr_startswith(const WCHAR *str, const WCHAR *txt);
int     wstr_startswithi(const WCHAR *str, const WCHAR *txt);
int     wstr_endswithi(const WCHAR *str, const WCHAR *end);
int     wstr_empty(const WCHAR *str);
int     wstr_copy(WCHAR *dst, int dst_cch_size, const WCHAR *src);
int     wstr_copyn(WCHAR *dst, int dst_cch_size, const WCHAR *src, int src_cch_size);
int     wstr_skip(const WCHAR **strp, const WCHAR *expect);
int     wstr_copy_skip_until(const WCHAR **strp, WCHAR *dst, size_t dst_size, WCHAR stop);
WCHAR * wstr_dup(const WCHAR *str);
WCHAR * wstr_dupn(const WCHAR *str, int str_len_cch);
WCHAR * wstr_cat_s(WCHAR *dst, size_t dst_cch_size, const WCHAR *src);
WCHAR * wstr_catn_s(WCHAR *dst, size_t dst_cch_size, const WCHAR *src, size_t src_cch_size);
WCHAR * wstr_cat(const WCHAR *str1, const WCHAR *str2);
WCHAR * wstr_cat3(const WCHAR *str1, const WCHAR *str2, const WCHAR *str3);
WCHAR * wstr_cat4(const WCHAR *str1, const WCHAR *str2, const WCHAR *str3, const WCHAR *str4);
WCHAR * wstr_url_encode(const WCHAR *str);
WCHAR   wchar_needs_url_escape(WCHAR c);
int     wstr_contains(const WCHAR *str, WCHAR c);
WCHAR  *wstr_escape(const WCHAR *txt);
WCHAR * wstr_printf(const WCHAR *format, ...);
const WCHAR *wstr_find_char(const WCHAR *txt, WCHAR c);
BOOL wstr_dup_replace(WCHAR **dst, const WCHAR *src);

char *wstr_to_multibyte(const WCHAR *txt,  UINT CodePage);
char *wstr_to_utf8(const WCHAR *txt);
WCHAR *multibyte_to_wstr(const char *src, UINT CodePage);
WCHAR *utf8_to_wstr(const char *utf8);
WCHAR *str_to_wstr_simplistic(const char *s);


WCHAR *wstr_parse_possibly_quoted(WCHAR **txt);
int hex_wstr_decode_byte(const WCHAR **txt);

#ifdef DEBUG
void wstr_util_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
