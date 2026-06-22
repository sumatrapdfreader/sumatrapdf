/* jconfigint.h
 *
 * SumatraPDF: hand-generated from jconfigint.h.in (normally produced by CMake).
 */

/* libjpeg-turbo build number */
#define BUILD  "0"

/* How to hide global symbols. */
#define HIDDEN

/* Compiler's inline keyword */
#undef inline

/* How to obtain function inlining. */
#ifndef INLINE
#if defined(__GNUC__)
#define INLINE  inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define INLINE  __forceinline
#else
#define INLINE  inline
#endif
#endif

/* How to obtain thread-local storage */
#if defined(_MSC_VER)
#define THREAD_LOCAL  __declspec(thread)
#else
#define THREAD_LOCAL  __thread
#endif

/* Define to the full name of this package. */
#define PACKAGE_NAME  "libjpeg-turbo"

/* Version number of package */
#define VERSION  "3.1.4.1"

/* The size of `size_t', as computed by sizeof. */
#ifdef _WIN64
#define SIZEOF_SIZE_T  8
#else
#define SIZEOF_SIZE_T  4
#endif

/* Define if your compiler has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t). */
#if defined(__GNUC__)
#define HAVE_BUILTIN_CTZL
#endif

/* Define to 1 if you have the <intrin.h> header file. */
#if defined(_MSC_VER)
#define HAVE_INTRIN_H
#endif

#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#if (SIZEOF_SIZE_T == 8)
#define HAVE_BITSCANFORWARD64
#elif (SIZEOF_SIZE_T == 4)
#define HAVE_BITSCANFORWARD
#endif
#endif

#if defined(__has_attribute)
#if __has_attribute(fallthrough)
#define FALLTHROUGH  __attribute__((fallthrough));
#else
#define FALLTHROUGH
#endif
#else
#define FALLTHROUGH
#endif

/*
 * Define BITS_IN_JSAMPLE as either
 *   8   for 8-bit sample values (the usual setting)
 *   12  for 12-bit sample values
 * Only 8 and 12 are legal data precisions for lossy JPEG according to the
 * JPEG standard, and the IJG code does not support anything else!
 */

#ifndef BITS_IN_JSAMPLE
#define BITS_IN_JSAMPLE  8      /* use 8 or 12 */
#endif

#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED
#undef WITH_SIMD

#if BITS_IN_JSAMPLE == 8

/* Support arithmetic encoding */
#define C_ARITH_CODING_SUPPORTED 1

/* Support arithmetic decoding */
#define D_ARITH_CODING_SUPPORTED 1

/* Use accelerated SIMD routines.
 * SumatraPDF: only available on x86/x86-64 (NASM); not on arm64.
 * Define WITHOUT_SIMD to force the pure-C build (e.g. the mingw build, which
 * does not assemble the NASM SIMD sources).
 */
#if !defined(WITHOUT_SIMD) && \
    (defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__))
#define WITH_SIMD 1
#endif

#endif
