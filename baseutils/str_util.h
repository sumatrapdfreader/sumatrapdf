/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef STR_UTIL_H_
#define STR_UTIL_H_

#ifdef _MSC_VER
#include "stdint.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* DOS is 0xd 0xa */
#define DOS_NEWLINE "\x0d\x0a"
/* Mac is single 0xd */
#define MAC_NEWLINE "\x0d"
/* Unix is single 0xa (10) */
#define UNIX_NEWLINE "\x0a"
#define UNIX_NEWLINE_C 0xa

#ifdef _WIN32
  #define DIR_SEP_CHAR '\\'
  #define DIR_SEP_STR  "\\"
  /* Warning: ordinary forward slashes (/) are often valid on Windows too */
#else
  #define DIR_SEP_CHAR '/'
  #define DIR_SEP_STR  "/"
#endif

#define no_op() ((void)0)

#ifdef _WIN32
  void    win32_dbg_out(const char *format, ...);
  void    win32_dbg_out_hex(const char *dsc, const unsigned char *data, int dataLen);
#endif

#ifdef DEBUG
  #ifdef _WIN32
  #define DBG_OUT win32_dbg_out
  #define DBG_OUT_HEX win32_dbg_out_hex
  #else
  #define DBG_OUT printf
  #define DBG_OUT_HEX(...) no_op()
  #endif
#else
  #define DBG_OUT(...) no_op()
  #define DBG_OUT_HEX(...) no_op()
#endif

int char_is_ws_or_zero(char c);
int char_is_ws(char c);
int char_is_digit(char c);
int char_is_dir_sep(char c);

#define str_len strlen
int     str_eq(const char *str1, const char *str2);
int     str_ieq(const char *str1, const char *str2);
#define str_eq_no_case str_ieq
int     str_eqn(const char *str1, const char *str2, int len);
int     str_startswith(const char *str, const char *txt);
int     str_startswithi(const char *str, const char *txt);
int     str_endswith(const char *str, const char *end);
int     str_endswithi(const char *str, const char *end);
int     str_endswith_char(const char *str, char c);
int     str_empty(const char *str);
int     str_copy(char *dst, size_t dst_cch_size, const char *src);
int     str_copyn(char *dst, size_t dst_cch_size, const char *src, size_t src_cch_size);
char *  str_dup(const char *str);
char *  str_dupn(const char *str, size_t len);
char *  str_cat_s(char *dst, size_t dst_cch_size, const char *src);
char *  str_catn_s(char *dst, size_t dst_cch_size, const char *src, size_t src_cch_size);
char *  str_cat(const char *str1, const char *str2);
char *  str_cat3(const char *str1, const char *str2, const char *str3);
char *  str_cat4(const char *str1, const char *str2, const char *str3, const char *str4);
char *  str_url_encode(const char *str);
int     char_needs_url_escape(char c);
int     str_contains(const char *str, char c);
char *  str_printf_args(const char *format, va_list args);
char *  str_printf(const char *format, ...);
int     str_printf_s(char *out, size_t out_cch_size, const char *format, ...);
const char *str_find_char(const char *txt, char c);
char *  str_split_iter(char **txt, char c);
char *  str_normalize_newline(const char *txt, const char *replace);
void    str_strip_left(char *txt, const char *to_strip);
void    str_strip_ws_left(char *txt);
void    str_strip_right(char *txt, const char *to_strip);
void    str_strip_ws_right(char *txt);
void    str_strip_both(char *txt, const char *to_strip);
void    str_strip_ws_both(char *txt);
char *  str_escape(const char *txt);
int     str_skip(const char **strp, const char *expect);
int     str_copy_skip_until(const char **strp, char *dst, size_t dst_size, char stop);
char *  str_parse_possibly_quoted(char **txt);
BOOL    str_to_double(const char *txt, double *resOut);
char *  mem_to_hexstr(const unsigned char *buf, int len);
BOOL    hexstr_to_mem(const char *s, unsigned char *buf, int bufLen);
int     hex_str_decode_byte(const char **txt);
BOOL    str_dup_replace(char **dst, const char *src);
int     str_trans_chars(char *str, const char *oldChars, const char *newChars);

char *  str_to_multibyte(const char *src, UINT CodePage);
char *  multibyte_to_str(const char *src, UINT CodePage);

char *  str_parse_possibly_quoted(char  **txt);
int     hex_str_decode_byte(const char **txt);

size_t  digits_for_number(int64_t num);

#ifdef DEBUG
void str_util_test(void);
#endif

typedef struct str_item str_item;
typedef struct str_array str_array;

struct str_item {
    void *  opaque;   /* opaque data that the user can use */
    char    str[1];
};

struct str_array {
    int          items_allocated;
    int          items_count;
    str_item **  items;
};

void      str_array_init(str_array *str_arr);
void      str_array_free(str_array *str_arr);
void      str_array_delete(str_array *str_arr);
str_item *str_array_set(str_array *str_arr, int index, const char *str);
str_item *str_array_add(str_array *str_arr, const char *str);
str_item *str_array_get(str_array *str_arr, int index);
int       str_array_get_count(str_array *str_arr);
int       str_array_exists_no_case(str_array *str_arr, const char *str);
str_item *str_array_add_no_dups(str_array *str_arr, const char *str);

#ifdef __cplusplus
}
#endif
#endif
