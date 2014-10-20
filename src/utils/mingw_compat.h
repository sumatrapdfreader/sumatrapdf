/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef mingw_compat_h
#define mingw_compat_h

/*
Things that are missing in mingw compiler
*/

#if defined(__MINGW32__)
#define sprintf_s snprintf
#endif

#endif
