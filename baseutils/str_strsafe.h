/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef __STR_STRSAFE_H
#define __STR_STRSAFE_H

/* When using MSVC, use <strsafe.h>, emulate it on other compiler (e.g. mingw) */

#ifndef DISABLE_STRSAFE
  #include <strsafe.h>
#else
  #include <stdio.h>
  #include <string.h>
  #include <windows.h>
  #define	STRSAFE_E_INSUFFICIENT_BUFFER   -1
  #define	_vsnprintf_s(p,s,z,f,a)		vsnprintf(p,s,f,a)
  
  /* WARNING: the return values of these two pairs of functions aren't
  compatible. The strsafe functions return an error code and the standard C
  functions return a character count */
  #define	StringCchVPrintfA			vsnprintf
  #define	StringCchPrintfA			snprintf
  
  #define	_stricmp					strcasecmp
  #define	_strnicmp					strncasecmp
#endif

#endif
