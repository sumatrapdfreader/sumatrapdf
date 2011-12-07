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
void * __cdecl realloc(void * p, size_t size) {
    if (p)
        return HeapReAlloc( GetProcessHeap(), 0, p, size );
    else    // 'p' is 0, and HeapReAlloc doesn't act like realloc() here
        return HeapAlloc( GetProcessHeap(), 0, size );
}

extern "C"
#if _MSC_VER >= 1400
__declspec(noalias restrict)
#endif
void * __cdecl calloc(size_t nitems, size_t size) {
    return HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, nitems * size );
}

extern "C"
#if _MSC_VER >= 1400
  __declspec(noalias restrict)
#endif
void * __cdecl _recalloc(void * p, size_t nitems, size_t size) {
    return realloc(p, nitems * size);
}

