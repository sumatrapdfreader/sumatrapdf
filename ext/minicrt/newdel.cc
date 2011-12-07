//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
//==========================================
#include "libctiny.h"
#include <windows.h>

void * __cdecl operator new(unsigned int s) {
  return HeapAlloc( GetProcessHeap(), 0, s );
}

void __cdecl operator delete(void* p) {
  if (p)
    HeapFree( GetProcessHeap(), 0, p ); 
}

void* __cdecl operator new[](size_t n) {
  return operator new(n);
}

void __cdecl operator delete[](void* p) {
  operator delete(p);
}
