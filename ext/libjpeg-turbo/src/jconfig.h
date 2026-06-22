/* jconfig.h
 *
 * SumatraPDF: hand-generated from jconfig.h.in (normally produced by CMake).
 * Emulates the libjpeg v8 API (JPEG_LIB_VERSION 80) with arithmetic coding,
 * in-memory source/destination managers, and x86/x86-64 SIMD.
 */

/* Version ID for the JPEG library.
 * Might be useful for tests like "#if JPEG_LIB_VERSION >= 60".
 */
#define JPEG_LIB_VERSION  80

/* libjpeg-turbo version */
#define LIBJPEG_TURBO_VERSION  3.1.4.1

/* libjpeg-turbo version in integer form */
#define LIBJPEG_TURBO_VERSION_NUMBER  3001004

/* Support arithmetic encoding when using 8-bit samples */
#define C_ARITH_CODING_SUPPORTED 1

/* Support arithmetic decoding when using 8-bit samples */
#define D_ARITH_CODING_SUPPORTED 1

/* Support in-memory source/destination managers */
#define MEM_SRCDST_SUPPORTED  1

/* Use accelerated SIMD routines when using 8-bit samples.
 * SumatraPDF: only available on x86/x86-64 (NASM); not on arm64.
 * Define WITHOUT_SIMD to force the pure-C build (e.g. the mingw build, which
 * does not assemble the NASM SIMD sources).
 */
#if !defined(WITHOUT_SIMD) && \
    (defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__))
#define WITH_SIMD 1
#endif

/* This version of libjpeg-turbo supports run-time selection of data precision,
 * so BITS_IN_JSAMPLE is no longer used to specify the data precision at build
 * time.  However, some downstream software expects the macro to be defined.
 * Since 12-bit data precision is an opt-in feature that requires explicitly
 * calling 12-bit-specific libjpeg API functions and using 12-bit-specific data
 * types, the unmodified portion of the libjpeg API still behaves as if it were
 * built for 8-bit precision, and JSAMPLE is still literally an 8-bit data
 * type.  Thus, it is correct to define BITS_IN_JSAMPLE to 8 here.
 */
#ifndef BITS_IN_JSAMPLE
#define BITS_IN_JSAMPLE  8
#endif

#ifdef _WIN32

#undef RIGHT_SHIFT_IS_UNSIGNED

/* Define "boolean" as unsigned char, not int, per Windows custom */
#ifndef __RPCNDR_H__            /* don't conflict if rpcndr.h already read */
typedef unsigned char boolean;
#endif
#define HAVE_BOOLEAN            /* prevent jmorecfg.h from redefining it */

/* Define "INT32" as int, not long, per Windows custom */
#if !(defined(_BASETSD_H_) || defined(_BASETSD_H))   /* don't conflict if basetsd.h already read */
typedef short INT16;
typedef signed int INT32;
#endif
#define XMD_H                   /* prevent jmorecfg.h from redefining it */

#else

/* Define if your (broken) compiler shifts signed values as if they were
   unsigned. */
/* #undef RIGHT_SHIFT_IS_UNSIGNED */

#endif
