#ifndef TR_COMMON_MINICRT_INITTERM_H_
#define TR_COMMON_MINICRT_INITTERM_H_

#include "libctiny.h"

typedef void (__cdecl *_PVFV)();
extern _PVFV __xc_a[], __xc_z[];    /* C++ initializers */

void __cdecl _initterm(_PVFV * pfbegin, _PVFV * pfend);
void __cdecl _atexit_init();
void __cdecl _DoExit();

#endif  // TR_COMMON_MINICRT_INITTERM_H_
