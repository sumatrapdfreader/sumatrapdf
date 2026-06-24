/*
 * Hand-built config.h for building libarchive on Windows with MSVC.
 * Only read support is needed; zlib is available.
 */
#define __LIBARCHIVE_CONFIG_H_INCLUDED 1

/* Windows platform */
#define _CRT_SECURE_NO_WARNINGS 1

/* POSIX types not available on MSVC / provided for mingw cross compile */
#if defined(_WIN32) && !defined(__CYGWIN__)
typedef unsigned int gid_t;
typedef unsigned int uid_t;
#if defined(_MSC_VER)
typedef int id_t;
typedef int pid_t;
#if !defined(_SSIZE_T_DEFINED)
#define _SSIZE_T_DEFINED
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif
#endif
typedef unsigned short mode_t;
#endif
#endif

/* Standard C headers available on MSVC */
#define HAVE_CTYPE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_IO_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_TIME_H 1
#define HAVE_WCHAR_H 1
#define HAVE_WCTYPE_H 1
#define HAVE_WINDOWS_H 1
#define HAVE_DIRECT_H 1

/* Functions available on MSVC */
#define HAVE_FSTAT 1
#define HAVE_MEMSET 1
#define HAVE_MEMMOVE 1
#define HAVE_SETLOCALE 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRFTIME 1
#define HAVE_STRNLEN 1
#define HAVE_STRRCHR 1
#define HAVE_UTIME 1
#define HAVE_VPRINTF 1
#define HAVE_MBRTOWC 1
#define HAVE_WCRTOMB 1
#define HAVE_WCSCMP 1
#define HAVE_WCSCPY 1
#define HAVE_WCSLEN 1
#define HAVE_WCTOMB 1
#define HAVE_WMEMCMP 1
#define HAVE_WMEMCPY 1
#define HAVE_WMEMMOVE 1
#define HAVE_FTRUNCATE 1
#define HAVE_TZSET 1
#define HAVE__CTIME64_S 1
#define HAVE__FSEEKI64 1
#define HAVE__GET_TIMEZONE 1
#define HAVE__LOCALTIME64_S 1
#define HAVE__MKGMTIME64 1

/* Integer types */
#define HAVE_INTTYPES_H 1
#define HAVE_INTMAX_T 1
#define HAVE_UINTMAX_T 1
#define HAVE_LONG_LONG_INT 1
#define HAVE_UNSIGNED_LONG_LONG 1
#define HAVE_UNSIGNED_LONG_LONG_INT 1
#define HAVE_WCHAR_T 1

/* Integer limits */
#define HAVE_DECL_INT32_MAX 1
#define HAVE_DECL_INT32_MIN 1
#define HAVE_DECL_INT64_MAX 1
#define HAVE_DECL_INT64_MIN 1
#define HAVE_DECL_INTMAX_MAX 1
#define HAVE_DECL_INTMAX_MIN 1
#define HAVE_DECL_SIZE_MAX 1
#define HAVE_DECL_UINT32_MAX 1
#define HAVE_DECL_UINT64_MAX 1
#define HAVE_DECL_UINTMAX_MAX 1

/* Sizes */
#ifdef _WIN64
#define SIZEOF_WCHAR_T 2
#else
#define SIZEOF_WCHAR_T 2
#endif

/* Error codes */
#define HAVE_EILSEQ 1

/* zlib support */
#define HAVE_LIBZ 1
#define HAVE_ZLIB_H 1

/* bzip2 support */
#define HAVE_BZLIB_H 1

/* BCrypt/CNG digest support (libarchive 3.8.8 dropped legacy WinCrypt) */
#define HAVE_BCRYPT_H 1
#define ARCHIVE_CRYPTO_MD5_WIN 1
#define ARCHIVE_CRYPTO_SHA1_WIN 1
#define ARCHIVE_CRYPTO_SHA256_WIN 1
#define ARCHIVE_CRYPTO_SHA384_WIN 1
#define ARCHIVE_CRYPTO_SHA512_WIN 1

/* liblzma support for LZMA/LZMA2/XZ decompression (needed for 7zip) */
#define HAVE_LZMA_H 1
#define HAVE_LIBLZMA 1

/* No lz4, zstd, or openssl for now */
