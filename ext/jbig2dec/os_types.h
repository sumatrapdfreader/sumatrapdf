/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

/*
   indirection layer for build and platform-specific definitions

   in general, this header should ensure that the stdint types are
   available, and that any optional compile flags are defined if
   the build system doesn't pass them directly.
*/

#ifndef _JBIG2_OS_TYPES_H
#define _JBIG2_OS_TYPES_H

#if defined(__CYGWIN__) && !defined(HAVE_STDINT_H)
# include <sys/types.h>
# if defined(OLD_CYGWIN_SYS_TYPES)
  /*
   * Old versions of Cygwin have no stdint.h but define "MS types". Some of
   * them conflict with a standard type emulation provided by config_types.h
   * so we do a fixup here.
   */
   typedef u_int8_t uint8_t;
   typedef u_int16_t uint16_t;
   typedef u_int32_t uint32_t;
#endif
#elif defined(HAVE_CONFIG_H)
# include "config_types.h"
#elif defined(_WIN32) || defined(__WIN32__)
# include "config_win32.h"
#elif defined (STD_INT_USE_SYS_TYPES_H)
# include <sys/types.h>
#elif defined (STD_INT_USE_INTTYPES_H)
# include <inttypes.h>
#elif defined (STD_INT_USE_SYS_INTTYPES_H)
# include <sys/inttypes.h>
#elif defined (STD_INT_USE_SYS_INT_TYPES_H)
# include <sys/int_types.h>
#elif !defined(HAVE_STDINT_H)
   typedef unsigned char  uint8_t;
   typedef unsigned short uint16_t;
   typedef unsigned int   uint32_t;
   typedef signed char    int8_t;
   typedef signed short   int16_t;
   typedef signed int     int32_t;
#endif

#if defined(HAVE_STDINT_H) || defined(__MACOS__)
# include <stdint.h>
#elif defined(__VMS) || defined(__osf__)
# include <inttypes.h>
#endif

#ifdef __hpux
#include <sys/_inttypes.h>
#endif

#endif /* _JBIG2_OS_TYPES_H */
