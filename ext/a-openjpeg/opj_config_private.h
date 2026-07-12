#define OPJ_PACKAGE_VERSION "2.5.4"

#if !defined(_POSIX_C_SOURCE)
#if defined(OPJ_HAVE_FSEEKO) || defined(OPJ_HAVE_POSIX_MEMALIGN)

#define _POSIX_C_SOURCE 200112L
#endif
#endif

#if !defined(__APPLE__)

#elif defined(__BIG_ENDIAN__)
# define OPJ_BIG_ENDIAN
#endif
