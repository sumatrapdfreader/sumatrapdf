/*
 * Hand-built config.h for building libarchive on macOS with clang.
 * Read-only subset matching SumatraPDF's Windows libarchive build.
 */
#define __LIBARCHIVE_CONFIG_H_INCLUDED 1

#define HAVE_CTYPE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_FCNTL 1
#define HAVE_LIMITS_H 1
#define HAVE_ARC4RANDOM_BUF 1
#define HAVE_FCHDIR 1
#define HAVE_DIRFD 1
#define HAVE_READLINK 1
#define HAVE_LSTAT 1
#define HAVE_STRUCT_STAT_ST_MTIMESPEC 1
#define HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC 1
#define HAVE_LOCALE_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE_WCHAR_H 1
#define HAVE_WCTYPE_H 1
#define HAVE_DIRENT_H 1
#define HAVE_DLFCN_H 1
#define HAVE_PWD_H 1
#define HAVE_GRP_H 1
#define HAVE_POLL_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_WAIT_H 1

#define HAVE_FSTAT 1
#define HAVE_LSTAT 1
#define HAVE_STAT 1
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
#define HAVE_CHOWN 1
#define HAVE_CHROOT 1
#define HAVE_FORK 1
#define HAVE_PIPE 1
#define HAVE_READLINK 1
#define HAVE_SYMLINK 1
#define HAVE_LINK 1
#define HAVE_OPENAT 1
#define HAVE_FUTIMES 1
#define HAVE_FUTIMENS 1
#define HAVE_UTIMENSAT 1

#define HAVE_INTTYPES_H 1
#define HAVE_INTMAX_T 1
#define HAVE_UINTMAX_T 1
#define HAVE_LONG_LONG_INT 1
#define HAVE_UNSIGNED_LONG_LONG 1
#define HAVE_UNSIGNED_LONG_LONG_INT 1
#define HAVE_WCHAR_T 1
#define HAVE_SSIZE_T 1

#define HAVE_DECL_INT32_MAX 1
#define HAVE_DECL_INT32_MIN 1
#define HAVE_DECL_INT64_MAX 1
#define HAVE_DECL_INT64_MIN 1
#define HAVE_DECL_INTMAX_MAX 1
#define HAVE_DECL_INTMAX_MIN 1
#define HAVE_DECL_SIZE_MAX 1
#define HAVE_DECL_SSIZE_MAX 1
#define HAVE_DECL_UINT32_MAX 1
#define HAVE_DECL_UINT64_MAX 1
#define HAVE_DECL_UINTMAX_MAX 1

#define SIZEOF_WCHAR_T 4
#define HAVE_EILSEQ 1

/* bundled codecs */
#define HAVE_LIBZ 1
#define HAVE_ZLIB_H 1
#define HAVE_BZLIB_H 1
#define HAVE_LZMA_H 1
#define HAVE_LIBLZMA 1

/* Apple CommonCrypto digest backend (see archive_digest_private.h) */
#define ARCHIVE_CRYPTO_MD5_LIBSYSTEM 1
#define ARCHIVE_CRYPTO_SHA1_LIBSYSTEM 1
#define ARCHIVE_CRYPTO_SHA256_LIBSYSTEM 1
#define ARCHIVE_CRYPTO_SHA384_LIBSYSTEM 1
#define ARCHIVE_CRYPTO_SHA512_LIBSYSTEM 1
