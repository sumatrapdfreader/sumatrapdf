#ifndef ARTIFEX_EXTRACT_COMPAT_STDINT_H
#define ARTIFEX_EXTRACT_COMPAT_STDINT_H

/* Fake what we need from stdint.h on MSVS. */

#if defined(_MSC_VER) && (_MSC_VER < 1700) /* MSVC older than VS2012 */
	typedef signed char         int8_t;
	typedef short int           int16_t;
	typedef int                 int32_t;
	typedef __int64             int64_t;
	typedef unsigned char       uint8_t;
	typedef unsigned short int  uint16_t;
	typedef unsigned int        uint32_t;
	typedef unsigned __int64    uint64_t;
	#ifndef INT64_MAX
		#define INT64_MAX 9223372036854775807i64
	#endif
	#ifndef SIZE_MAX
		#define SIZE_MAX ((size_t) -1)
	#endif
#else
	#include <stdint.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1800) /* MSVC older than VS2013 */
	#define strtoll( text, end, base) (long long) _strtoi64(text, end, base)
	#define strtoull( text, end, base) (unsigned long long) _strtoi64(text, end, base)
#endif

#endif
