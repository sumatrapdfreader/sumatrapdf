#ifndef ARTIFEX_EXTRACT_COMPAT_STRTOLL_H
#define ARTIFEX_EXTRACT_COMPAT_STRTOLL_H

#if defined(_MSC_VER) && (_MSC_VER < 1800) /* MSVC older than VS2013 */
    #define strtoll( text, end, base) (long long) _strtoi64(text, end, base)
    #define strtoull( text, end, base) (unsigned long long) _strtoi64(text, end, base)
#endif

#endif
