//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
//==========================================
#include "libctiny.h"
#include <windows.h>
#include <string.h>

// Force the linker to include USER32.LIB
#pragma comment(linker, "/defaultlib:user32.lib")

extern "C" char *  __cdecl strupr(char *s) {
    CharUpperBuff( s, lstrlen(s) );
    return s;
}

extern "C" char *  __cdecl strlwr(char *s) {
    CharLowerBuff( s, lstrlen(s) );
    return s;
}
