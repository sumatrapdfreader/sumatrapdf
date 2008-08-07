/*
 * Copyright (c) 1999-2000 Image Power, Inc. and the University of
 *   British Columbia.
 * Copyright (c) 2001-2003 Michael David Adams.
 * Copyright (c) 2004-2006 artofcode LLC.
 * All rights reserved.
 */

/* __START_OF_JASPER_LICENSE__
 * 
 * JasPer License Version 2.0
 * 
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 * Copyright (c) 2001-2003 Michael David Adams
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 * 
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 * 
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 * 
 * __END_OF_JASPER_LICENSE__
 */

/*
 * Primitive Types
 *
 * $Id$
 */

#ifndef JAS_TYPES_H
#define JAS_TYPES_H

#include <jasper/jas_config.h>

/* define our own boolean type -- should be bool on C++? */
typedef int	jas_bool;

#define	jas_false	0
#define	jas_true	1

#if !defined(JAS_CONFIGURE)

#if defined(WIN32) || defined(HAVE_WINDOWS_H)
/*
   We are dealing with Microsoft Windows and most likely Microsoft
   Visual C (MSVC).  (Heaven help us.)  Sadly, MSVC does not correctly
   define some of the standard types specified in ISO/IEC 9899:1999.
   In particular, it does not define the "long long" and "unsigned long
   long" types. We therefore must make our own defines.
 */
# ifdef _MSC_VER
/* 
   We use the intrinsics rather than the windows.h types because
   that header is large, slow to compile, and incompatibile with
   the MSVC /Za ANSI compliance option.
 */
#  undef longlong
#  define longlong	__int64
#  undef ulonglong
#  define ulonglong	unsigned __int64
# else
/*  Obtain the 64-bit types from the header file "windows.h".  */
#  include <windows.h>
#  undef longlong
#  define longlong	INT64
#  undef ulonglong
#  define ulonglong	UINT64
# endif /* _MSC_VER */

/* Microsoft defines some things with slightly different names */
# define O_RDWR _O_RDWR
# define O_RDONLY _O_RDONLY
# define O_WRONLY _O_WRONLY
# define O_CREAT _O_CREAT
# define O_TRUNC _O_TRUNC
# define O_APPEND _O_APPEND
# define O_EXCL _O_EXCL

#endif /* WIN32 */

#endif /* !JAS_CONFIGURE */

#if defined(HAVE_STDLIB_H)
# include <stdlib.h>
#endif
#if defined(HAVE_STDDEF_H)
# include <stddef.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
#endif

#if defined(HAVE_STDINT_H)
/*
 * The C language implementation does correctly provide the standard header
 * file "stdint.h".
 */
# include <stdint.h>
#else
/*
 * The C language implementation does not provide the standard header file
 * "stdint.h" as required by ISO/IEC 9899:1999.  Try to compensate for this
 * braindamage below.
 */
# include <limits.h>
/**********/
# if !defined(INT_FAST8_MIN)
   typedef signed char int_fast8_t;
#  define INT_FAST8_MIN	(-127)
#  define INT_FAST8_MAX	128
# endif
/**********/
# if !defined(UINT_FAST8_MAX)
   typedef unsigned char uint_fast8_t;
#  define UINT_FAST8_MAX	255
# endif
/**********/
# if !defined(INT_FAST16_MIN)
   typedef short int_fast16_t;
#  define INT_FAST16_MIN	SHRT_MIN
#  define INT_FAST16_MAX	SHRT_MAX
# endif
/**********/
# if !defined(UINT_FAST16_MAX)
   typedef unsigned short uint_fast16_t;
#  define UINT_FAST16_MAX	USHRT_MAX
# endif
/**********/
# if !defined(INT_FAST32_MIN)
   typedef int int_fast32_t;
#  define INT_FAST32_MIN	INT_MIN
#  define INT_FAST32_MAX	INT_MAX
# endif
/**********/
# if !defined(UINT_FAST32_MAX)
   typedef unsigned int uint_fast32_t;
#  define UINT_FAST32_MAX	UINT_MAX
# endif
/**********/
# if !defined(INT_FAST64_MIN)
   typedef longlong int_fast64_t;
#  define INT_FAST64_MIN	LLONG_MIN
#  define INT_FAST64_MAX	LLONG_MAX
# endif
/**********/
# if !defined(UINT_FAST64_MAX)
   typedef ulonglong uint_fast64_t;
#  define UINT_FAST64_MAX	ULLONG_MAX
# endif
/**********/
#endif /* HAVE_STDINT_H */

/* The below macro is intended to be used for type casts.  By using this
  macro, type casts can be easily located in the source code with
  tools like "grep". */
#define	JAS_CAST(t, e) \
	((t) (e))

#endif /* JAS_TYPES_H */
