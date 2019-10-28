/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

/* configuration header file for compiling under Microsoft Windows */

#ifdef _MSC_VER

/* VS 2012 and later have stdint.h */
# if _MSC_VER >= 1700
#  include <stdint.h>
# else
typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef __int64 int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
#ifndef SIZE_MAX
#define SIZE_MAX (~((size_t) 0))
#endif
# endif

/* VS 2008 and later have vsnprintf */
# if _MSC_VER < 1500
#  define vsnprintf _vsnprintf
# endif

/* VS 2014 and later have (finally) snprintf */
# if _MSC_VER >= 1900
#  define STDC99
# else
#  define snprintf _snprintf
# endif

#else /* _MSC_VER */

/* Not VS -- it had best behave */
# include <stdint.h>

#endif /* _MSC_VER */
