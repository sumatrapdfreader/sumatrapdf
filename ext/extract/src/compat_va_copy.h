#ifndef ARTIFEX_EXTRACT_COMPAT_VA_COPY_H
#define ARTIFEX_EXTRACT_COMPAT_VA_COPY_H

#if defined(_MSC_VER) && (_MSC_VER < 1800) /* MSVC older than VS2013 */
	#define va_copy(dst, src) ((dst) = (src))
#endif

#endif
