#ifndef MUPDF_FITZ_SYSTEM_H
#define MUPDF_FITZ_SYSTEM_H

/*
	Include the standard libc headers.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <assert.h>
#include <errno.h>
#include <limits.h> /* INT_MAX & co */
#include <float.h> /* FLT_EPSILON, FLT_MAX & co */
#include <fcntl.h> /* O_RDONLY & co */
#include <time.h>

#include <setjmp.h>

/* SumatraPDF: memento's license header can be read as being non-AGPLv3 */
#ifndef MEMENTO
#define MEMENTO_H
#define Memento_label(ptr, label) (ptr)
#else
#include "mupdf/memento.h"
#endif

#define nelem(x) (sizeof(x)/sizeof((x)[0]))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

/*
	Some differences in libc can be smoothed over
*/

#ifdef __APPLE__
#define HAVE_SIGSETJMP
#elif defined(__unix)
#define HAVE_SIGSETJMP
#endif

/*
	Where possible (i.e. on platforms on which they are provided), use
	sigsetjmp/siglongjmp in preference to setjmp/longjmp. We don't alter
	signal handlers within mupdf, so there is no need for us to
	store/restore them - hence we use the non-restoring variants. This
	makes a large speed difference on MacOSX (and probably other
	platforms too.
*/
#ifdef HAVE_SIGSETJMP
#define fz_setjmp(BUF) sigsetjmp(BUF, 0)
#define fz_longjmp(BUF,VAL) siglongjmp(BUF, VAL)
#define fz_jmp_buf sigjmp_buf
#else
#define fz_setjmp(BUF) setjmp(BUF)
#define fz_longjmp(BUF,VAL) longjmp(BUF,VAL)
#define fz_jmp_buf jmp_buf
#endif

#ifdef _MSC_VER /* Microsoft Visual C */

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef __int64 int64_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

#pragma warning( disable: 4244 ) /* conversion from X to Y, possible loss of data */
#pragma warning( disable: 4996 ) /* The POSIX name for this item is deprecated */
#pragma warning( disable: 4996 ) /* This function or variable may be unsafe */

#include <io.h>

int gettimeofday(struct timeval *tv, struct timezone *tz);

#define snprintf _snprintf
/* SumatraPDF: VS 2013 has isnan in math.h */
#if _MSC_VER < 1800
#define isnan _isnan
#endif
#define hypotf _hypotf

#define fopen fz_fopen_utf8

FILE *fz_fopen_utf8(const char *name, const char *mode);

#define fopen fz_fopen_utf8

char *fz_utf8_from_wchar(const wchar_t *s);
wchar_t *fz_wchar_from_utf8(const char *s);

FILE *fz_fopen_utf8(const char *name, const char *mode);
char **fz_argv_from_wargv(int argc, wchar_t **wargv);
void fz_free_argv(int argc, char **argv);

#else /* Unix or close enough */

#include <stdint.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0

#endif

#endif

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "libmupdf"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define LOGI(...) do {} while(0)
#define LOGE(...) do {} while(0)
#endif

/*
	Variadic macros, inline and restrict keywords

	inline is standard in C++, so don't touch the definition in this case.
	For some compilers we can enable it within C too.
*/

#ifndef __cplusplus
#if __STDC_VERSION__ == 199901L /* C99 */
#elif _MSC_VER >= 1500 /* MSVC 9 or newer */
#define inline __inline
#elif __GNUC__ >= 3 /* GCC 3 or newer */
#define inline __inline
#else /* Unknown or ancient */
#define inline
#endif
#endif

/*
	restrict is standard in C99, but not in all C++ compilers. Enable
	where possible, disable if in doubt.
 */
#if __STDC_VERSION__ == 199901L /* C99 */
#elif _MSC_VER >= 1500 /* MSVC 9 or newer */
#define restrict __restrict
#elif __GNUC__ >= 3 /* GCC 3 or newer */
#define restrict __restrict
#else /* Unknown or ancient */
#define restrict
#endif

/* noreturn is a GCC extension */
#ifdef __GNUC__
#define FZ_NORETURN __attribute__((noreturn))
#else
#ifdef _MSC_VER
#define FZ_NORETURN __declspec(noreturn)
#else
#define FZ_NORETURN
#endif
#endif

/*
	GCC can do type checking of printf strings
*/

#ifndef __printflike
#if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define __printflike(fmtarg, firstvararg)
#endif
#endif

/*
	Shut the compiler up about unused variables
*/
#define UNUSED(x) do { x = x; } while (0)

/* ARM assembly specific defines */

#ifdef ARCH_ARM
#ifdef NDK_PROFILER
extern void __gnu_mcount_nc(void);
#define ENTER_PG "push {lr}\nbl __gnu_mcount_nc\n"
#else
#define ENTER_PG
#endif

/* If we're compiling as thumb code, then we need to tell the compiler
 * to enter and exit ARM mode around our assembly sections. If we move
 * the ARM functions to a separate file and arrange for it to be compiled
 * without thumb mode, we can save some time on entry.
 */
/* This is slightly suboptimal; __thumb__ and __thumb2__ become defined
 * and undefined by #pragma arm/#pragma thumb - but we can't define a
 * macro to track that. */
#if defined(__thumb__) || defined(__thumb2__)
#define ENTER_ARM ".balign 4\nmov r12,pc\nbx r12\n0:.arm\n" ENTER_PG
#define ENTER_THUMB "9:.thumb\n" ENTER_PG
#else
#define ENTER_ARM
#define ENTER_THUMB
#endif
#endif

#ifdef CLUSTER
#define LOCAL_TRIG_FNS
#endif

#ifdef LOCAL_TRIG_FNS
/*
 * Trig functions
 */
static float
my_atan_table[258] =
{
0.0000000000f, 0.00390623013f,0.00781234106f,0.0117182136f,
0.0156237286f, 0.0195287670f, 0.0234332099f, 0.0273369383f,
0.0312398334f, 0.0351417768f, 0.0390426500f, 0.0429423347f,
0.0468407129f, 0.0507376669f, 0.0546330792f, 0.0585268326f,
0.0624188100f, 0.0663088949f, 0.0701969711f, 0.0740829225f,
0.0779666338f, 0.0818479898f, 0.0857268758f, 0.0896031775f,
0.0934767812f, 0.0973475735f, 0.1012154420f, 0.1050802730f,
0.1089419570f, 0.1128003810f, 0.1166554350f, 0.1205070100f,
0.1243549950f, 0.1281992810f, 0.1320397620f, 0.1358763280f,
0.1397088740f, 0.1435372940f, 0.1473614810f, 0.1511813320f,
0.1549967420f, 0.1588076080f, 0.1626138290f, 0.1664153010f,
0.1702119250f, 0.1740036010f, 0.1777902290f, 0.1815717110f,
0.1853479500f, 0.1891188490f, 0.1928843120f, 0.1966442450f,
0.2003985540f, 0.2041471450f, 0.2078899270f, 0.2116268090f,
0.2153577000f, 0.2190825110f, 0.2228011540f, 0.2265135410f,
0.2302195870f, 0.2339192060f, 0.2376123140f, 0.2412988270f,
0.2449786630f, 0.2486517410f, 0.2523179810f, 0.2559773030f,
0.2596296290f, 0.2632748830f, 0.2669129880f, 0.2705438680f,
0.2741674510f, 0.2777836630f, 0.2813924330f, 0.2849936890f,
0.2885873620f, 0.2921733830f, 0.2957516860f, 0.2993222020f,
0.3028848680f, 0.3064396190f, 0.3099863910f, 0.3135251230f,
0.3170557530f, 0.3205782220f, 0.3240924700f, 0.3275984410f,
0.3310960770f, 0.3345853220f, 0.3380661230f, 0.3415384250f,
0.3450021770f, 0.3484573270f, 0.3519038250f, 0.3553416220f,
0.3587706700f, 0.3621909220f, 0.3656023320f, 0.3690048540f,
0.3723984470f, 0.3757830650f, 0.3791586690f, 0.3825252170f,
0.3858826690f, 0.3892309880f, 0.3925701350f, 0.3959000740f,
0.3992207700f, 0.4025321870f, 0.4058342930f, 0.4091270550f,
0.4124104420f, 0.4156844220f, 0.4189489670f, 0.4222040480f,
0.4254496370f, 0.4286857080f, 0.4319122350f, 0.4351291940f,
0.4383365600f, 0.4415343100f, 0.4447224240f, 0.4479008790f,
0.4510696560f, 0.4542287350f, 0.4573780990f, 0.4605177290f,
0.4636476090f, 0.4667677240f, 0.4698780580f, 0.4729785980f,
0.4760693300f, 0.4791502430f, 0.4822213240f, 0.4852825630f,
0.4883339510f, 0.4913754780f, 0.4944071350f, 0.4974289160f,
0.5004408130f, 0.5034428210f, 0.5064349340f, 0.5094171490f,
0.5123894600f, 0.5153518660f, 0.5183043630f, 0.5212469510f,
0.5241796290f, 0.5271023950f, 0.5300152510f, 0.5329181980f,
0.5358112380f, 0.5386943730f, 0.5415676050f, 0.5444309400f,
0.5472843810f, 0.5501279330f, 0.5529616020f, 0.5557853940f,
0.5585993150f, 0.5614033740f, 0.5641975770f, 0.5669819340f,
0.5697564530f, 0.5725211450f, 0.5752760180f, 0.5780210840f,
0.5807563530f, 0.5834818390f, 0.5861975510f, 0.5889035040f,
0.5915997100f, 0.5942861830f, 0.5969629370f, 0.5996299860f,
0.6022873460f, 0.6049350310f, 0.6075730580f, 0.6102014430f,
0.6128202020f, 0.6154293530f, 0.6180289120f, 0.6206188990f,
0.6231993300f, 0.6257702250f, 0.6283316020f, 0.6308834820f,
0.6334258830f, 0.6359588250f, 0.6384823300f, 0.6409964180f,
0.6435011090f, 0.6459964250f, 0.6484823880f, 0.6509590190f,
0.6534263410f, 0.6558843770f, 0.6583331480f, 0.6607726790f,
0.6632029930f, 0.6656241120f, 0.6680360620f, 0.6704388650f,
0.6728325470f, 0.6752171330f, 0.6775926450f, 0.6799591110f,
0.6823165550f, 0.6846650020f, 0.6870044780f, 0.6893350100f,
0.6916566220f, 0.6939693410f, 0.6962731940f, 0.6985682070f,
0.7008544080f, 0.7031318220f, 0.7054004770f, 0.7076604000f,
0.7099116190f, 0.7121541600f, 0.7143880520f, 0.7166133230f,
0.7188300000f, 0.7210381110f, 0.7232376840f, 0.7254287490f,
0.7276113330f, 0.7297854640f, 0.7319511710f, 0.7341084830f,
0.7362574290f, 0.7383980370f, 0.7405303370f, 0.7426543560f,
0.7447701260f, 0.7468776740f, 0.7489770290f, 0.7510682220f,
0.7531512810f, 0.7552262360f, 0.7572931160f, 0.7593519510f,
0.7614027700f, 0.7634456020f, 0.7654804790f, 0.7675074280f,
0.7695264800f, 0.7715376650f, 0.7735410110f, 0.7755365500f,
0.7775243100f, 0.7795043220f, 0.7814766150f, 0.7834412190f,
0.7853981630f, 0.7853981630f /* Extended by 1 for interpolation */
};

static inline float my_sinf(float x)
{
	float x2, xn;
	int i;
	/* Map x into the -PI to PI range. We could do this using:
	 * x = fmodf(x, (float)(2.0 * M_PI));
	 * but that's C99, and seems to misbehave with negative numbers
	 * on some platforms. */
	x -= (float)M_PI;
	i = x / (float)(2.0f * M_PI);
	x -= i * (float)(2.0f * M_PI);
	if (x < 0.0f)
		x += (float)(2.0f * M_PI);
	x -= (float)M_PI;
	if (x <= (float)(-M_PI/2.0))
		x = -(float)M_PI-x;
	else if (x >= (float)(M_PI/2.0))
		x = (float)M_PI-x;
	x2 = x*x;
	xn = x*x2/6.0f;
	x -= xn;
	xn *= x2/20.0f;
	x += xn;
	xn *= x2/42.0f;
	x -= xn;
	xn *= x2/72.0f;
	x += xn;
	return x;
}

static inline float my_atan2f(float o, float a)
{
	int negate = 0, flip = 0, i;
	float r, s;
	if (o == 0.0f)
	{
		if (a > 0)
			return 0.0f;
		else
			return (float)M_PI;
	}
	if (o < 0)
		o = -o, negate = 1;
	if (a < 0)
		a = -a, flip = 1;
	if (o < a)
		i = (int)(65536.0f*o/a + 0.5f);
	else
		i = (int)(65536.0f*a/o + 0.5f);
	r = my_atan_table[i>>8];
	s = my_atan_table[(i>>8)+1];
	r += (s-r)*(i&255)/256.0f;
	if (o >= a)
		r = (float)(M_PI/2.0f) - r;
	if (flip)
		r = (float)M_PI - r;
	if (negate)
		r = -r;
	return r;
}

#define sinf(x) my_sinf(x)
#define cosf(x) my_sinf(((float)(M_PI/2.0f)) + (x))
#define atan2f(x,y) my_atan2f((x),(y))
#endif

/* SumatraPDF: basic global synchronizing */
#ifdef _WIN32
void fz_synchronize_begin();
void fz_synchronize_end();
#endif

#endif
