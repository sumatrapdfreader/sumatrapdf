/*
 * Include the basic standard libc headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include <limits.h>	/* INT_MIN, MAX ... */
#include <float.h>	/* DBL_EPSILON */
#include <math.h>

#include <errno.h>
#include <fcntl.h>	/* O_RDONLY & co */

/* Stupid macros that don't exist everywhere */

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

/* Some useful semi-standard functions */

#ifdef NEED_STRLCPY
extern int strlcpy(char *dst, const char *src, int n);
extern int strlcat(char *dst, const char *src, int n);
#endif

#ifdef NEED_STRSEP
extern char *strsep(char **stringp, const char *delim);
#endif

#ifdef NEED_GETOPT
extern int getopt(int nargc, char * const * nargv, const char *ostr);
extern int opterr, optind, optopt;
extern char *optarg;
#endif

/*
 * MSVC section
 */

#ifdef _MSC_VER

#include <io.h>

extern int gettimeofday(struct timeval *tv, struct timezone *tz);

#define FZ_FLEX 1
#define restrict

#ifdef _MSC_VER
#define inline __inline
#else
#define inline __inline__
#endif

#if _MSC_VER < 1500
#define vsnprintf _vsnprintf
#endif

#ifndef isnan
#define isnan _isnan
#endif

#ifndef va_copy
#define va_copy(a,b) (a) = (b)
#endif

#ifndef R_OK
#define R_OK 4
#endif

/*
 * C99 section
 */

#else

#include <unistd.h>
#define FZ_FLEX

#endif

