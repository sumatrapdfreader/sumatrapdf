#include "wdltypes.h"

#ifdef _WIN32
  #ifndef SWP__NOMOVETHENSIZE
  #define SWP__NOMOVETHENSIZE (1<<30)
  #endif

#ifdef WDL_WIN32_HIDPI_IMPL
#ifndef _WDL_WIN32_HIDPI_H_IMPL
#define _WDL_WIN32_HIDPI_H_IMPL

WDL_WIN32_HIDPI_IMPL void *_WDL_dpi_func(void *p)
{
  static void *(WINAPI *__SetThreadDpiAwarenessContext)(void *);
  if (!__SetThreadDpiAwarenessContext)
  {
    HINSTANCE huser32 = LoadLibrary("user32.dll");
    if (huser32) *(void **)&__SetThreadDpiAwarenessContext = GetProcAddress(huser32,"SetThreadDpiAwarenessContext");
    if (!__SetThreadDpiAwarenessContext) *(UINT_PTR *)&__SetThreadDpiAwarenessContext = 1;
  }
  return (UINT_PTR)__SetThreadDpiAwarenessContext > 1 ? __SetThreadDpiAwarenessContext(p) : NULL;
}

WDL_WIN32_HIDPI_IMPL void WDL_mmSetWindowPos(HWND hwnd, HWND hwndAfter, int x, int y, int w, int h, UINT f)
{
#ifdef SetWindowPos
#undef SetWindowPos
#endif
  static char init;

  if (!init)
  {
    init = 1;
    HINSTANCE h = GetModuleHandle("user32.dll");
    if (h)
    {
      BOOL (WINAPI *__AreDpiAwarenessContextsEqual)(void *, void *);
      void * (WINAPI *__GetThreadDpiAwarenessContext )();
      *(void **)&__GetThreadDpiAwarenessContext = GetProcAddress(h,"GetThreadDpiAwarenessContext");
      *(void **)&__AreDpiAwarenessContextsEqual = GetProcAddress(h,"AreDpiAwarenessContextsEqual");
      if (__GetThreadDpiAwarenessContext && __AreDpiAwarenessContextsEqual)
      {
        if (__AreDpiAwarenessContextsEqual(__GetThreadDpiAwarenessContext(),(void*)(INT_PTR)-4))
          init = 2;
      }
    }
  }

  if (init == 2 && hwnd &&
    !(f&(SWP_NOMOVE|SWP_NOSIZE|SWP__NOMOVETHENSIZE|SWP_ASYNCWINDOWPOS)) &&
    !(GetWindowLong(hwnd,GWL_STYLE)&WS_CHILD))
  {
    SetWindowPos(hwnd,NULL,x,y,0,0,SWP_NOREDRAW|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_DEFERERASE);
    f |= SWP_NOMOVE;
  }
  SetWindowPos(hwnd,hwndAfter,x,y,w,h,f&~SWP__NOMOVETHENSIZE);
#define SetWindowPos WDL_mmSetWindowPos
}


#endif // end of _WDL_WIN32_HIDPI_H_IMPL
#else // if !WDL_WIN32_HIDPI_IMPL
void WDL_mmSetWindowPos(HWND hwnd, HWND hwndAfter, int x, int y, int w, int h, UINT f);
void *_WDL_dpi_func(void *p);
#ifdef SetWindowPos
#undef SetWindowPos
#endif
#define SetWindowPos WDL_mmSetWindowPos
#endif

#else // !_WIN32
  #ifndef SWP__NOMOVETHENSIZE
  #define SWP__NOMOVETHENSIZE 0
  #endif
#endif

#ifndef _WDL_WIN32_HIDPI_H_
#define _WDL_WIN32_HIDPI_H_

static WDL_STATICFUNC_UNUSED void *WDL_dpi_enter_aware(int mode) // -1 = DPI_AWARENESS_CONTEXT_UNAWARE, -2=aware, -3=mm aware, -4=mmaware2, -5=gdiscaled
{
#ifdef _WIN32
  return _WDL_dpi_func((void *)(INT_PTR)mode);
#else
  return NULL;
#endif
}
static WDL_STATICFUNC_UNUSED void WDL_dpi_leave_aware(void **p)
{
#ifdef _WIN32
  if (p)
  {
    if (*p) _WDL_dpi_func(*p);
    *p = NULL;
  }
#endif
}

#ifdef __cplusplus
struct WDL_dpi_aware_scope {
#ifdef _WIN32
  WDL_dpi_aware_scope(int mode=-1) { c = mode ? WDL_dpi_enter_aware(mode) : NULL; }
  ~WDL_dpi_aware_scope() { if (c) WDL_dpi_leave_aware(&c); }
  void *c;
#else
  WDL_dpi_aware_scope() { }
#endif
};
#endif

#endif
