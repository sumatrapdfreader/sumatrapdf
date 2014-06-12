#ifndef Config_Win32_h
#define Config_Win32_h

#ifdef _WIN64
#define ssize_t __int64
#else
#define ssize_t long
#endif
#define pid_t int
#define uid_t unsigned int
#define gid_t unsigned int
// cf. archive_entry.h
#define mode_t unsigned short

#define ARCHIVE_CRYPTO_MD5_WIN
#define ARCHIVE_CRYPTO_SHA1_WIN
/* TODO: only on Vista and above
#define ARCHIVE_CRYPTO_SHA256_WIN
#define ARCHIVE_CRYPTO_SHA384_WIN
#define ARCHIVE_CRYPTO_SHA512_WIN
*/

#define HAVE_BZLIB_H 1
#define HAVE_CTYPE_H 1
#define HAVE_DECL_INT64_MAX 1
#define HAVE_DECL_INT64_MIN 1
#define HAVE_DECL_SIZE_MAX 1
#define HAVE_DECL_UINT32_MAX 1
#define HAVE_DECL_UINT64_MAX 1

#define HAVE_ERRNO_H 1
#define HAVE_FCNTL 1
#define HAVE_FCNTL_H 1

#define HAVE_INTTYPES_H 1
#define HAVE_IO_H 1

#define HAVE_LIBBZ2 1
#define HAVE_LIBLZMADEC 1

#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1

#define HAVE_LZMADEC_H 1

#define HAVE_MEMMOVE 1
#define HAVE_MEMSET 1
#define HAVE_MKDIR 1

#undef HAVE_SETENV
#undef HAVE_SETLOCALE

#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRFTIME 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRNCPY_S 1
#define HAVE_STRRCHR 1
#define HAVE_TIME_H 1

#define HAVE_VPRINTF 1
#define HAVE_WCHAR_H 1
#define HAVE_WCHAR_T 1
#define HAVE_WCRTOMB 1
#define HAVE_WCSCMP 1
#define HAVE_WCSCPY 1
#define HAVE_WCSLEN 1
#define HAVE_WCTOMB 1
#define HAVE_WCTYPE_H 1

#define HAVE_WINCRYPT_H 1
#define HAVE_WINDOWS_H 1
#define HAVE_WINIOCTL_H 1
#define HAVE_WMEMCMP 1
#define HAVE_WMEMCPY 1

#define HAVE_ZLIB_H 1

#define HAVE__CTIME64_S 1
#define HAVE__FSEEKI64 1
#define HAVE__GET_TIMEZONE 1
#define HAVE__LOCALTIME64_S 1

#define STDC_HEADERS 1

#define _WIN32_WINNT 0x0500

#endif
