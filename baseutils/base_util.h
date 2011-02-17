/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef BASE_UTIL_H_
#define BASE_UTIL_H_

#ifdef _UNICODE
#ifndef UNICODE
#define UNICODE
#endif
#endif

#ifdef UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif
#endif

/* It seems that Visual C defines WIN32 for Windows code but _WINDOWS for WINCE projects,
   so I'll make sure to set WIN32 always*/
#ifdef _WINDOWS
 #ifndef WIN32
  #define WIN32
 #endif
 #ifndef _WIN32
  #define _WIN32
 #endif
#endif

#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#endif
#include <stdlib.h>
#ifdef DEBUG
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include <windows.h>
#include <tchar.h>

/* Few most common includes for C stdlib */
#include <assert.h>
#include <stdio.h>

#ifndef _UNICODE
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <wchar.h>
#include <string.h>

#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>
#include <inttypes.h>

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef _T
#define _T TEXT
#endif

/* compile-time assert */
#ifndef CASSERT
  #define CASSERT(exp, name) typedef int dummy##name [ (exp ) ? 1 : -1 ];
#endif

/* Ugly names but the whole point is to make things shorter.
   SA = Struct Allocate
   SAZ = Struct Allocate and Zero memory
   SAZA = Struct Allocate and Zero memory for Array */
#define SA(struct_name) (struct_name *)malloc(sizeof(struct_name))
#define SAZA(struct_name, n) (struct_name *)calloc((n), sizeof(struct_name))
#define SAZ(struct_name) SAZA(struct_name, 1)

#define dimof(X)    (sizeof(X)/sizeof((X)[0]))

#ifdef __cplusplus
extern "C"
{
#endif

/* TODO: consider using standard C macros for SWAP */
void        swap_int(int *one, int *two);
void        swap_double(double *one, double *two);

void *      memdup(void *data, size_t len);
#define     _memdup(ptr) memdup(ptr, sizeof(*(ptr)))

#ifdef __cplusplus
}
#endif

#endif
