/*
    jbig2dec

    Copyright (C) 2002-2003 Artifex Software, Inc.

    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.

    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id: config_win32.h 467 2008-05-17 00:08:26Z giles $
*/

/* configuration header file for compiling under Microsoft Windows */

/* update package version here */
#define PACKAGE "jbig2dec"
#define VERSION "0.3"

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

/* this doesn't work on vc2008 */
#if _MSC_VER < 1500
#  define vsnprintf _vsnprintf
#  define snprintf _snprintf
#endif

#endif /* _MSC_VER */
