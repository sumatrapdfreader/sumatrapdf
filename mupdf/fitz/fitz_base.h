/*
 * Include the basic standard libc headers.
 */

#ifndef _FITZ_BASE_H_
#define _FITZ_BASE_H_

#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

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

#ifdef _MSC_VER /* stupid stone-age compiler */

#pragma warning( disable: 4244 ) /* conversion from X to Y, possible loss of data */
#pragma warning( disable: 4996 ) /* The POSIX name for this item is deprecated */
#pragma warning( disable: 4996 ) /* This function or variable may be unsafe */

#include <io.h>

extern int gettimeofday(struct timeval *tv, struct timezone *tz);

#define inline __inline

#define __func__ __FUNCTION__

#define snprintf _snprintf
#define hypotf _hypotf
#define strtoll _strtoi64

#if _MSC_VER < 1500
#define vsnprintf _vsnprintf
#endif

#else /* unix or close enough */

#include <unistd.h>

#endif

#ifndef _C99
#ifdef __GNUC__
#define restrict __restrict
#else
#define restrict
#endif
#endif

/*
 * CPU detection and flags
 */

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#define HAVE_CPUDEP
#define HAVE_MMX        (1<<0)
#define HAVE_MMXEXT     (1<<1)
#define HAVE_SSE        (1<<2)
#define HAVE_SSE2       (1<<3)
#define HAVE_SSE3       (1<<4)
#define HAVE_3DNOW      (1<<5)
#define HAVE_AMD64      (1<<6)
#endif

#ifdef ARCH_ARM
#define HAVE_CPUDEP
#endif

/* call this before using fitz */
extern void fz_cpudetect(void);

/* treat as constant! */
extern unsigned fz_cpuflags;

int fz_isbigendian(void);

/*
 * Base Fitz runtime.
 */

#ifndef __printflike
#if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define __printflike(fmtarg, firstvararg)
#endif
#endif

#ifndef nil
#define nil ((void*)0)
#endif

#ifndef offsetof
#define offsetof(s, m) (unsigned long)(&(((s*)0)->m))
#endif

#ifndef nelem
#define nelem(x) (sizeof(x)/sizeof((x)[0]))
#endif

#ifndef ABS
#define ABS(x) ( (x) < 0 ? -(x) : (x) )
#endif

#ifndef MAX
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#endif

#ifndef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif

#ifndef CLAMP
#define CLAMP(x,a,b) ( (x) > (b) ? (b) : ( (x) < (a) ? (a) : (x) ) )
#endif

/* utf-8 encoding and decoding */
int chartorune(int *rune, char *str);
int runetochar(char *str, int *rune);
int runelen(int c);

/* useful string functions */
extern char *fz_strsep(char **stringp, const char *delim);
extern int fz_strlcpy(char *dst, const char *src, int n);
extern int fz_strlcat(char *dst, const char *src, int n);

/* getopt */
extern int fz_getopt(int nargc, char * const * nargv, const char *ostr);
extern int fz_optind;
extern char *fz_optarg;

/* memory allocation */
void *fz_malloc(int n);
void *fz_realloc(void *p, int n);
void fz_free(void *p);
char *fz_strdup(char *s);

/*
 * Error handling.
 */

typedef int fz_error;

extern char fz_errorbuf[];

void fz_warn(char *fmt, ...) __printflike(1,2);
fz_error fz_throwimp(const char *file, int line, const char *func, char *fmt, ...) __printflike(4, 5);
fz_error fz_rethrowimp(fz_error cause, const char *file, int line, const char *func, char *fmt, ...) __printflike(5, 6);
fz_error fz_catchimp(fz_error cause, const char *file, int line, const char *func, char *fmt, ...) __printflike(5, 6);

#define fz_throw(...) fz_throwimp(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define fz_rethrow(cause, ...) fz_rethrowimp(cause, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define fz_catch(cause, ...) fz_catchimp(cause, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define fz_okay ((fz_error)0)

/*
 * Generic hash-table with fixed-length keys.
 */

typedef struct fz_hashtable_s fz_hashtable;

fz_hashtable * fz_newhash(int initialsize, int keylen);
void fz_debughash(fz_hashtable *table);
void fz_emptyhash(fz_hashtable *table);
void fz_freehash(fz_hashtable *table);

void *fz_hashfind(fz_hashtable *table, void *key);
void fz_hashinsert(fz_hashtable *table, void *key, void *val);
void fz_hashremove(fz_hashtable *table, void *key);

int fz_hashlen(fz_hashtable *table);
void *fz_hashgetkey(fz_hashtable *table, int idx);
void *fz_hashgetval(fz_hashtable *table, int idx);

/*
 * Math and geometry
 */

/* multiply 8-bit fixpoint (0..1) so that 0*0==0 and 255*255==255 */
static inline int fz_mul255(int a, int b)
{
	int x = a * b + 0x80;
	x += x >> 8;
	return x >> 8;
}

typedef struct fz_matrix_s fz_matrix;
typedef struct fz_point_s fz_point;
typedef struct fz_rect_s fz_rect;
typedef struct fz_bbox_s fz_bbox;

extern const fz_rect fz_unitrect;
extern const fz_rect fz_emptyrect;
extern const fz_rect fz_infiniterect;

extern const fz_bbox fz_unitbbox;
extern const fz_bbox fz_emptybbox;
extern const fz_bbox fz_infinitebbox;

#define fz_isemptyrect(r) ((r).x0 == (r).x1)
#define fz_isinfiniterect(r) ((r).x0 > (r).x1)

/*
	/ a b 0 \
	| c d 0 |
	\ e f 1 /
*/
struct fz_matrix_s
{
	float a, b, c, d, e, f;
};

struct fz_point_s
{
	float x, y;
};

struct fz_rect_s
{
	float x0, y0;
	float x1, y1;
};

struct fz_bbox_s
{
	int x0, y0;
	int x1, y1;
};

void fz_invert3x3(float *dst, float *m);

fz_matrix fz_concat(fz_matrix one, fz_matrix two);
fz_matrix fz_identity(void);
fz_matrix fz_scale(float sx, float sy);
fz_matrix fz_rotate(float theta);
fz_matrix fz_translate(float tx, float ty);
fz_matrix fz_invertmatrix(fz_matrix m);
int fz_isrectilinear(fz_matrix m);
float fz_matrixexpansion(fz_matrix m);

fz_bbox fz_roundrect(fz_rect r);
fz_bbox fz_intersectbbox(fz_bbox a, fz_bbox b);
fz_bbox fz_unionbbox(fz_bbox a, fz_bbox b);

fz_point fz_transformpoint(fz_matrix m, fz_point p);
fz_point fz_transformvector(fz_matrix m, fz_point p);
fz_rect fz_transformrect(fz_matrix m, fz_rect r);

#endif

