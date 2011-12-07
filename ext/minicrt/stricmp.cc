//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
//==========================================
#include "libctiny.h"
#include <windows.h>
#include <string.h>

extern "C" int __cdecl _strcmpi(const char *s1, const char *s2) {
    return lstrcmpi( s1, s2 );
}

extern "C" int __cdecl _stricmp(const char *s1, const char *s2) {
    return lstrcmpi( s1, s2 );
}
