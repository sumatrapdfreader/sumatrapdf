#ifndef ARTIFEX_EXTRACT_COMPAT_INLINE
#define ARTIFEX_EXTRACT_COMPAT_INLINE

#if !defined __cplusplus && defined(_MSC_VER)
    #if (_MSC_VER < 1500)
        /* inline and inline__ not available so remove all mention of
        inline. This may result in warnings about unused static functions. */
        #define inline
    #else
        /* __inline is always available. */
        #define inline __inline
    #endif
#endif

#endif
