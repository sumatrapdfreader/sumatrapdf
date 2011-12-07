//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
// ==========================================

#include "libctiny.h"
#include <windows.h>
#include <malloc.h>
#include "initterm.h"

#pragma data_seg(".CRT$XCA")
_PVFV __xc_a[] = { NULL };
#pragma data_seg(".CRT$XCZ")
_PVFV __xc_z[] = { NULL };

/*
#pragma data_seg(".CRT$XIA")
_PVFV __xi_a[] = { NULL };
#pragma data_seg(".CRT$XIZ")
_PVFV __xi_z[] = { NULL };

#pragma data_seg(".CRT$XTA")
_PVFV __xt_a[] = { NULL };
#pragma data_seg(".CRT$XTZ")
_PVFV __xt_z[] = { NULL };

#pragma data_seg(".CRT$XPA")
_PVFV __xp_a[] = { NULL };
#pragma data_seg(".CRT$XPZ")
_PVFV __xp_z[] = { NULL };
*/

#pragma data_seg()  /* reset */

#pragma comment(linker, "/merge:.CRT=.data")

typedef void (__cdecl *_PVFV)();

void __cdecl _initterm(
        _PVFV * pfbegin,
        _PVFV * pfend
        ) {
    // walk the table of function pointers from the bottom up, until
    // the end is encountered.  Do not skip the first entry.  The initial
    // value of pfbegin points to the first valid entry.  Do not try to
    // execute what pfend points to.  Only entries before pfend are valid.
    while (pfbegin < pfend)
    {
        // if current table entry is non-NULL, call thru it.
        if (*pfbegin != NULL)
            (**pfbegin)();
        ++pfbegin;
    }
}

static _PVFV * pf_atexitlist = 0;
static unsigned max_atexitlist_entries = 0;
static unsigned cur_atexitlist_entries = 0;

void __cdecl _atexit_init() {
    max_atexitlist_entries = 32;
    pf_atexitlist = (_PVFV *)calloc( max_atexitlist_entries,
                                     sizeof(_PVFV*) );
}

int __cdecl atexit(_PVFV func ) {
    if (cur_atexitlist_entries < max_atexitlist_entries)
    {
        pf_atexitlist[cur_atexitlist_entries++] = func; 
        return 0;
    }

    return -1;
}

void __cdecl _DoExit() {
    if (cur_atexitlist_entries)
    {
        _initterm(  pf_atexitlist,
                    // Use ptr math to find the end of the array
                    pf_atexitlist + cur_atexitlist_entries );
    }
}

// -----------------------------------------------------

/*
static HANDLE g_hProcessHeap = NULL;

extern "C" _PVFV* __onexitbegin = NULL;
extern "C" _PVFV* __onexitend = NULL;

extern "C" _PVFV __xi_a[], __xi_z[];    // C initializers 
extern "C" _PVFV __xc_a[], __xc_z[];    // C++ initializers 
extern "C" _PVFV __xp_a[], __xp_z[];    // C pre-terminators 
extern "C" _PVFV __xt_a[], __xt_z[];    // C terminators 

// Critical section to protect initialization/exit code
static CRITICAL_SECTION g_csInit;

extern "C" void DoInitialization() {
  _PVFV* pf;

  // memset(&osi, 0, sizeof(OSVERSIONINFO));
  // osi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  // GetVersionEx(&osi);
  // _osplatform = osi.dwPlatformId;

  InitializeCriticalSection( &g_csInit );

  EnterCriticalSection( &g_csInit );

  __try
  {
    g_hProcessHeap = GetProcessHeap();

    // Call initialization routines (contructors for globals, etc.)
    for (pf = __xi_a; pf < __xi_z; pf++)
    {
      if (*pf != NULL)
      {
        (**pf)();
      }
    }

    for (pf = __xc_a; pf < __xc_z; pf++)
    {
      if (*pf != NULL)
      {
        (**pf)();
      }
    }
  }
  __finally
  {
    LeaveCriticalSection(&g_csInit);
  }
}

extern "C" void DoCleanup() {
  _PVFV* pf;

  EnterCriticalSection(&g_csInit);  // Protect access to the atexit table

  __try
  {
    // Call routines registered with atexit() from most recently registered
    // to least recently registered
    if (__onexitbegin != NULL)
    {
      for (pf = __onexitend-1; pf >= __onexitbegin; pf--)
      {
        if (*pf != NULL)
          (**pf)();
      }
    }

    free(__onexitbegin);
    __onexitbegin = NULL;
    __onexitend = NULL;

    for (pf = __xp_a; pf < __xp_z; pf++)
    {
      if (*pf != NULL)
      {
        (**pf)();
      }
    }

    for (pf = __xt_a; pf < __xt_z; pf++)
    {
      if (*pf != NULL)
      {
        (**pf)();
      }
    }
  }
  __finally
  {
    LeaveCriticalSection(&g_csInit);
    DeleteCriticalSection(&g_csInit);    
  }
}

int __cdecl atexit(_PVFV pf) {
  size_t nCurrentSize;
  int nRet = 0;  

  EnterCriticalSection(&g_csInit);

  __try
  {
    if (__onexitbegin == NULL)
    {
      __onexitbegin = (_PVFV*)malloc(16*sizeof(_PVFV));
      if (__onexitbegin == NULL)
      {
        LeaveCriticalSection(&g_csInit);
        return(-1);
      }
      __onexitend = __onexitbegin;
    }

    nCurrentSize = _msize(__onexitbegin);
    if ((nCurrentSize+sizeof(_PVFV)) < ULONG(((const byte*)__onexitend-
      (const byte*)__onexitbegin)))
    {
      _PVFV* pNew;

      pNew = (_PVFV*)realloc(__onexitbegin, 2*nCurrentSize);
      if (pNew == NULL)
      {
        LeaveCriticalSection(&g_csInit);    
        return(-1);
      }
    }

    *__onexitend = pf;
    __onexitend++;
  }
  __except (1)
  {
    nRet = -1;
  }

  LeaveCriticalSection(&g_csInit);  

  return(nRet);
}
*/
