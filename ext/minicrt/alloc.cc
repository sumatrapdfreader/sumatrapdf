//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
//==========================================
#include "libctiny.h"
#include <windows.h>
#include <malloc.h>

extern "C"
#if _MSC_VER >= 1400
__declspec(noalias restrict)
#endif
void * __cdecl malloc(size_t size) {
    return HeapAlloc( GetProcessHeap(), 0, size );
}

extern "C"
#if _MSC_VER >= 1400
__declspec(noalias)
#endif
void __cdecl free(void * p) {
    HeapFree( GetProcessHeap(), 0, p );
}
