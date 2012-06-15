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


/* configuration header file for compiling under Microsoft Windows */

/* update package version here */
#define PACKAGE "jbig2dec"
#define VERSION "0.11"

#if defined(_MSC_VER) || (defined(__BORLANDC__) && defined(__WIN32__))
  /* Microsoft Visual C++ or Borland C++ */
  typedef signed char             int8_t;
  typedef short int               int16_t;
  typedef int                     int32_t;
  typedef __int64                 int64_t;

  typedef unsigned char             uint8_t;
  typedef unsigned short int        uint16_t;
  typedef unsigned int              uint32_t;
  /* no uint64_t */

#  if defined(_MSC_VER)
#   if _MSC_VER < 1500	/* VS 2008 has vsnprintf */
#    define vsnprintf _vsnprintf
#   endif
#  endif
#  define snprintf _snprintf

#endif /* _MSC_VER */
