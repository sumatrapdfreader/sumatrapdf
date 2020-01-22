#ifndef MUPDF_FITZ_STRING_H
#define MUPDF_FITZ_STRING_H

#include "mupdf/fitz/system.h"

/* The Unicode character used to incoming character whose value is unknown or unrepresentable. */
#define FZ_REPLACEMENT_CHARACTER 0xFFFD

/*
	Safe string functions
*/

size_t fz_strnlen(const char *s, size_t maxlen);

char *fz_strsep(char **stringp, const char *delim);

size_t fz_strlcpy(char *dst, const char *src, size_t n);

size_t fz_strlcat(char *dst, const char *src, size_t n);

void *fz_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

void fz_dirname(char *dir, const char *path, size_t dirsize);

char *fz_urldecode(char *url);

void fz_format_output_path(fz_context *ctx, char *path, size_t size, const char *fmt, int page);

char *fz_cleanname(char *name);

int fz_strcasecmp(const char *a, const char *b);
int fz_strncasecmp(const char *a, const char *b, size_t n);

/*
	FZ_UTFMAX: Maximum number of bytes in a decoded rune (maximum length returned by fz_chartorune).
*/
enum { FZ_UTFMAX = 4 };

int fz_chartorune(int *rune, const char *str);

int fz_runetochar(char *str, int rune);

int fz_runelen(int rune);

int fz_utflen(const char *s);

float fz_strtof(const char *s, char **es);

int fz_grisu(float f, char *s, int *exp);

int fz_is_page_range(fz_context *ctx, const char *s);
const char *fz_parse_page_range(fz_context *ctx, const char *s, int *a, int *b, int n);

#endif
