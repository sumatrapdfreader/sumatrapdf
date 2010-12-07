/* Version ID for the JPEG library.
 * Might be useful for tests like "#if JPEG_LIB_VERSION >= 60".
 */
#define JPEG_LIB_VERSION  80
 
#define C_ARITH_CODING_SUPPORTED  1
#define D_ARITH_CODING_SUPPORTED  1

#define HAVE_PROTOTYPES           1
#define HAVE_UNSIGNED_CHAR        1
#define HAVE_UNSIGNED_SHORT       1
#undef __CHAR_UNSIGNED__
#define HAVE_STDDEF_H             1
#define HAVE_STDLIB_H             1

#undef NEED_BSD_STRINGS
#undef NEED_SYS_TYPES_H
#undef NEED_SHORT_EXTERNAL_NAMES

#undef INCOMPLETE_TYPES_BROKEN

#define WITH_SIMD                 1

/* Define if shift is unsigned */
#undef RIGHT_SHIFT_IS_UNSIGNED

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#undef inline
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t
