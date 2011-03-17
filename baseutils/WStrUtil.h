/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef WStrUtil_h
#define WStrUtil_h

#define wstr_find_char wcschr

int     wstr_copy(WCHAR *dst, size_t dst_cch_size, const WCHAR *src);
int     wstr_copyn(WCHAR *dst, size_t dst_cch_size, const WCHAR *src, size_t src_cch_size);
WCHAR * wstr_cat_s(WCHAR *dst, size_t dst_cch_size, const WCHAR *src);
WCHAR * wstr_catn_s(WCHAR *dst, size_t dst_cch_size, const WCHAR *src, size_t src_cch_size);
int     wstr_printf_s(WCHAR *out, size_t out_cch_size, const WCHAR *format, ...);

char *  wstr_to_multibyte(const WCHAR *txt, UINT CodePage);
WCHAR * multibyte_to_wstr(const char *src, UINT CodePage);
#define wstr_to_utf8(src) wstr_to_multibyte((src), CP_UTF8)
#define utf8_to_wstr(src) multibyte_to_wstr((src), CP_UTF8)

void win32_dbg_outW(const WCHAR *format, ...);
#ifdef DEBUG
  #define DBG_OUT_W(format, ...) win32_dbg_outW(L##format, __VA_ARGS__)
  void WStrUtil_test(void);
#else
  #define DBG_OUT_W(format, ...) NoOp()
#endif

#endif
