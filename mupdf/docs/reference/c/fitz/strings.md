# Strings

All text strings in MuPDF use the `UTF-8` encoding.

## Unicode

The following functions encode and decode `UTF-8` characters, and return the
number of bytes used by the `UTF-8` character (at most `FZ_UTFMAX`).

	int fz_chartorune(int *rune, const char *str);
	int fz_runetochar(char *str, int rune);

## Locale Independent

Since many of the C string functions are locale dependent, we also provide our
own locale independent versions of these functions. We also have a couple of
semi-standard functions like `strsep` and `strlcpy` that we can't rely on the
system providing. These should be pretty self explanatory:

	char *fz_strdup(fz_context *ctx, const char *s);
	float fz_strtof(const char *s, char **es);
	char *fz_strsep(char **stringp, const char *delim);
	size_t fz_strlcpy(char *dst, const char *src, size_t n);
	size_t fz_strlcat(char *dst, const char *src, size_t n);
	void *fz_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
	int fz_strcasecmp(const char *a, const char *b);

There are also a couple of functions to process filenames and URLs:

`char *fz_cleanname(char *path);`
:	Rewrite path in-place to the shortest string that names the same path.
	Eliminates multiple and trailing slashes, and interprets "." and "..".

`void fz_dirname(char *dir, const char *path, size_t dir_size);`
:	Extract the directory component from a path.

`char *fz_urldecode(char *url);`
:	Decode URL escapes in-place.

## Formatting

Our `printf` family handles the common `printf` formatting characters, with a
few minor differences. We also support several non-standard formatting
characters. The same `printf` syntax is used in the `printf` functions in the
I/O module as well.

	size_t fz_vsnprintf(char *buffer, size_t space, const char *fmt, va_list args);
	size_t fz_snprintf(char *buffer, size_t space, const char *fmt, ...);
	char *fz_asprintf(fz_context *ctx, const char *fmt, ...);

`%%`, `%c`, `%e`, `%f`, `%p`, `%x`, `%d`, `%u`, `%s`
:	These behave as usual, but only take padding (+,0,space), width, and precision arguments.

`%g float`
:	Prints the `float` in the shortest possible format that won't lose precision, except `NaN` to `0`, `+Inf` to `FLT_MAX`, `-Inf` to `-FLT_MAX`.

`%M fz_matrix*`
:	Prints all 6 coefficients in the matrix as `%g` separated by spaces.

`%R fz_rect*`
:	Prints all `x0`, `y0`, `x1`, `y1` in the rectangle as `%g` separated by spaces.

`%P fz_point*`
:	Prints `x`, `y` in the point as `%g` separated by spaces.

`%C int`
:	Formats character as `UTF-8`. Useful to print unicode text.

`%q char*`
:	Formats string using double quotes and C escapes.

`%( char*`
:	Formats string using parenthesis quotes and Postscript escapes.

`%n char*`
:	Formats string using prefix `/` and PDF name hex-escapes.
