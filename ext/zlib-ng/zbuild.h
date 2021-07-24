#ifndef _ZBUILD_H
#define _ZBUILD_H

/* Determine compiler version of C Standard */
#ifdef __STDC_VERSION__
#  if __STDC_VERSION__ >= 199901L
#    ifndef STDC99
#      define STDC99
#    endif
#  endif
#  if __STDC_VERSION__ >= 201112L
#    ifndef STDC11
#      define STDC11
#    endif
#  endif
#endif

/* Determine compiler support for TLS */
#ifndef Z_TLS
#  if defined(STDC11) && !defined(__STDC_NO_THREADS__)
#    define Z_TLS _Thread_local
#  elif defined(__GNUC__) || defined(__SUNPRO_C)
#    define Z_TLS __thread
#  elif defined(_WIN32) && (defined(_MSC_VER) || defined(__ICL))
#    define Z_TLS __declspec(thread)
#  else
#    warning Unable to detect Thread Local Storage support.
#    define Z_TLS
#  endif
#endif

/* This has to be first include that defines any types */
#if defined(_MSC_VER)
#  if defined(_WIN64)
    typedef __int64 ssize_t;
#  else
    typedef long ssize_t;
#  endif
#endif

#if defined(ZLIB_COMPAT)
#  define PREFIX(x) x
#  define PREFIX2(x) ZLIB_ ## x
#  define PREFIX3(x) z_ ## x
#  define PREFIX4(x) x ## 64
#  define zVersion zlibVersion
#  define z_size_t unsigned long
#else
#  define PREFIX(x) zng_ ## x
#  define PREFIX2(x) ZLIBNG_ ## x
#  define PREFIX3(x) zng_ ## x
#  define PREFIX4(x) zng_ ## x
#  define zVersion zlibng_version
#  define z_size_t size_t
#endif

/* Minimum of a and b. */
#define MIN(a, b) ((a) > (b) ? (b) : (a))
/* Maximum of a and b. */
#define MAX(a, b) ((a) < (b) ? (b) : (a))
/* Ignore unused variable warning */
#define Z_UNUSED(var) (void)(var)

#endif
