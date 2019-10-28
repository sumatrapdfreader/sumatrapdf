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

/*
   indirection layer for build and platform-specific definitions

   in general, this header should ensure that the stdint types are
   available, and that any optional compile flags are defined if
   the build system doesn't pass them directly.
*/

#ifndef _JBIG2_OS_TYPES_H
#define _JBIG2_OS_TYPES_H

#if defined(HAVE_CONFIG_H)
# include "config_types.h"
#elif defined(_WIN32)
# include "config_win32.h"
#elif defined (STD_INT_USE_SYS_TYPES_H)
# include <sys/types.h>
#elif defined (STD_INT_USE_INTTYPES_H)
# include <inttypes.h>
#elif defined (STD_INT_USE_SYS_INTTYPES_H)
# include <sys/inttypes.h>
#elif defined (STD_INT_USE_SYS_INT_TYPES_H)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

#endif /* _JBIG2_OS_TYPES_H */
