#ifndef MUPDF_FITZ_STRING_H
#define MUPDF_FITZ_STRING_H

#include "mupdf/fitz/system.h"

/* The Unicode character used to incoming character whose value is
 * unknown or unrepresentable. */
#define FZ_REPLACEMENT_CHARACTER 0xFFFD

/**
	Safe string functions
*/

/**
	Return strlen(s), if that is less than maxlen, or maxlen if
	there is no null byte ('\0') among the first maxlen bytes.
*/
size_t fz_strnlen(const char *s, size_t maxlen);

/**
	Given a pointer to a C string (or a pointer to NULL) break
	it at the first occurrence of a delimiter char (from a given
	set).

	stringp: Pointer to a C string pointer (or NULL). Updated on
	exit to point to the first char of the string after the
	delimiter that was found. The string pointed to by stringp will
	be corrupted by this call (as the found delimiter will be
	overwritten by 0).

	delim: A C string of acceptable delimiter characters.

	Returns a pointer to a C string containing the chars of stringp
	up to the first delimiter char (or the end of the string), or
	NULL.
*/
char *fz_strsep(char **stringp, const char *delim);

/**
	Copy at most n-1 chars of a string into a destination
	buffer with null termination, returning the real length of the
	initial string (excluding terminator).

	dst: Destination buffer, at least n bytes long.

	src: C string (non-NULL).

	n: Size of dst buffer in bytes.

	Returns the length (excluding terminator) of src.
*/
size_t fz_strlcpy(char *dst, const char *src, size_t n);

/**
	Concatenate 2 strings, with a maximum length.

	dst: pointer to first string in a buffer of n bytes.

	src: pointer to string to concatenate.

	n: Size (in bytes) of buffer that dst is in.

	Returns the real length that a concatenated dst + src would have
	been (not including terminator).
*/
size_t fz_strlcat(char *dst, const char *src, size_t n);

/**
	Find the start of the first occurrence of the substring needle in haystack.
*/
void *fz_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

/**
	extract the directory component from a path.
*/
void fz_dirname(char *dir, const char *path, size_t dirsize);

/**
	decode url escapes.
*/
char *fz_urldecode(char *url);

/**
	create output file name using a template.

	If the path contains %[0-9]*d, the first such pattern will be
	replaced with the page number. If the template does not contain
	such a pattern, the page number will be inserted before the
	filename extension. If the template does not have a filename
	extension, the page number will be added to the end.
*/
void fz_format_output_path(fz_context *ctx, char *path, size_t size, const char *fmt, int page);

/**
	rewrite path to the shortest string that names the same path.

	Eliminates multiple and trailing slashes, interprets "." and
	"..". Overwrites the string in place.
*/
char *fz_cleanname(char *name);

/**
	Resolve a path to an absolute file name.
	The resolved path buffer must be of at least PATH_MAX size.
*/
char *fz_realpath(const char *path, char *resolved_path);

/**
	Case insensitive (ASCII only) string comparison.
*/
int fz_strcasecmp(const char *a, const char *b);
int fz_strncasecmp(const char *a, const char *b, size_t n);

/**
	FZ_UTFMAX: Maximum number of bytes in a decoded rune (maximum
	length returned by fz_chartorune).
*/
enum { FZ_UTFMAX = 4 };

/**
	UTF8 decode a single rune from a sequence of chars.

	rune: Pointer to an int to assign the decoded 'rune' to.

	str: Pointer to a UTF8 encoded string.

	Returns the number of bytes consumed.
*/
int fz_chartorune(int *rune, const char *str);

/**
	UTF8 encode a rune to a sequence of chars.

	str: Pointer to a place to put the UTF8 encoded character.

	rune: Pointer to a 'rune'.

	Returns the number of bytes the rune took to output.
*/
int fz_runetochar(char *str, int rune);

/**
	Count how many chars are required to represent a rune.

	rune: The rune to encode.

	Returns the number of bytes required to represent this run in
	UTF8.
*/
int fz_runelen(int rune);

/**
	Count how many runes the UTF-8 encoded string
	consists of.

	s: The UTF-8 encoded, NUL-terminated text string.

	Returns the number of runes in the string.
*/
int fz_utflen(const char *s);

/**
	Locale-independent decimal to binary conversion. On overflow
	return (-)INFINITY and set errno to ERANGE. On underflow return
	0 and set errno to ERANGE. Special inputs (case insensitive):
	"NAN", "INF" or "INFINITY".
*/
float fz_strtof(const char *s, char **es);

int fz_grisu(float f, char *s, int *exp);

/**
	Check and parse string into page ranges:
		( ','? ([0-9]+|'N') ( '-' ([0-9]+|N) )? )+
*/
int fz_is_page_range(fz_context *ctx, const char *s);
const char *fz_parse_page_range(fz_context *ctx, const char *s, int *a, int *b, int n);

#endif
