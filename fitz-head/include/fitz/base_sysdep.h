/*
 * Include the basic standard libc headers.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include <limits.h>	/* INT_MIN, MAX ... */
#include <float.h>	/* DBL_EPSILON */
#include <math.h>

#include <errno.h>
#include <fcntl.h>	/* O_RDONLY & co */

#ifdef _WIN32
#	define vsnprintf _vsnprintf
#	include <io.h>
#else
#	include <unistd.h>
#endif

#ifdef HAVE_C99
#	define FZ_FLEX
#else
#	define FZ_FLEX 1
#	define restrict
#ifdef _MSC_VER
#       define inline __inline
#       define FORCEINLINE __forceinline
#else
#	define inline __inline__
#       define FORCEINLINE __inline__
#endif
#endif

#ifndef va_copy
#define va_copy(a,b) (a) = (b)
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 * Extras! Extras! Get them while they're hot!
 */

/* not supposed to be here, but printf debugging sorta needs it */
#include <stdio.h>

#ifdef NEED_MATH
#define M_E 2.71828182845904523536
#define M_LOG2E 1.44269504088896340736
#define M_LOG10E 0.434294481903251827651
#define M_LN2 0.693147180559945309417
#define M_LN10 2.30258509299404568402
#define M_PI 3.14159265358979323846f
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.785398163397448309616
#define M_1_PI 0.318309886183790671538
#define M_2_PI 0.636619772367581343076
#define M_1_SQRTPI 0.564189583547756286948
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2 1.41421356237309504880
#define M_SQRT_2 0.707106781186547524401
#endif

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

