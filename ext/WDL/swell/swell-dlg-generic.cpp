/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux/OSX)
   Copyright (C) 2006 and later, Cockos, Inc.

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"
#include "swell-dlggen.h"

#include "../ptrlist.h"

static HMENU g_swell_defaultmenu,g_swell_defaultmenumodal;

void (*SWELL_DDrop_onDragLeave)();
void (*SWELL_DDrop_onDragOver)(POINT pt);
void (*SWELL_DDrop_onDragEnter)(void *hGlobal, POINT pt);
const char* (*SWELL_DDrop_getDroppedFileTargetPath)(const char* extension);

bool SWELL_owned_windows_levelincrease=false;

#include "swell-internal.h"

static SWELL_DialogResourceIndex *resById(SWELL_DialogResourceIndex *reshead, const char *resid)
{
  SWELL_DialogResourceIndex *p=reshead;
  while (p)
  {
    if (p->resid == resid) return p;
    p=p->_next;
  }
  return 0;
}

// keep list of modal dialogs
struct modalDlgRet { 
  HWND hwnd; 
  bool has_ret;
  int ret;
};


static WDL_PtrList<modalDlgRet> s_modalDialogs;

bool IsModalDialogBox(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return false;
  int a = s_modalDialogs.GetSize();
  while (a-- > 0)
  {
    modalDlgRet *r = s_modalDialogs.Get(a);
    if (r && r->hwnd == hwnd) return true;
  }
  return false;
}

HWND DialogBoxIsActive()
{
  int a = s_modalDialogs.GetSize();
  while (a-- > 0)
  {
    modalDlgRet *r = s_modalDialogs.Get(a);
    if (r && !r->has_ret && r->hwnd) return r->hwnd; 
  }
  return NULL;
}

static SWELL_OSWINDOW s_spare;
static RECT s_spare_rect;
static UINT_PTR s_spare_timer;
static int s_spare_style;

void swell_dlg_destroyspare()
{
  if (s_spare_timer)
  {
    KillTimer(NULL,s_spare_timer);
    s_spare_timer=0;
  }
  if (s_spare) 
  { 
#ifdef SWELL_TARGET_GDK
    gdk_window_destroy(s_spare);
#endif
    s_spare=NULL; 
  }
}

static void spareTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwtime)
{
  swell_dlg_destroyspare();
}

static int s_last_dlgret;

void EndDialog(HWND wnd, int ret)
{   
  if (WDL_NOT_NORMALLY(!wnd)) return;
  
  int a = s_modalDialogs.GetSize();
  while (a-->0)
  {
    modalDlgRet *r = s_modalDialogs.Get(a);
    if (r && r->hwnd == wnd)  
    {
      r->ret = ret;
      if (r->has_ret) return;

      r->has_ret=true;
    }
  }

  if (!wnd->m_hashaddestroy)
  {
    void RecurseDestroyWindow(HWND);
    SendMessage(wnd,WM_DESTROY,0,0);
    #ifndef SWELL_NO_SPARE_MODALDLG
      if (wnd->m_oswindow && wnd->m_visible)
      {
        swell_dlg_destroyspare();
        GetWindowRect(wnd,&s_spare_rect);
        s_spare_style = wnd->m_style;
        s_spare = wnd->m_oswindow;
        wnd->m_oswindow = NULL;
        s_spare_timer = SetTimer(NULL,0,
                             swell_app_is_inactive ? 500 : 100,
                             spareTimer);
      }
    #endif
    RecurseDestroyWindow(wnd);
  }
  s_last_dlgret = ret;
}

int SWELL_DialogBox(SWELL_DialogResourceIndex *reshead, const char *resid, HWND parent,  DLGPROC dlgproc, LPARAM param)
{
  SWELL_DialogResourceIndex *p=resById(reshead,resid);
  if (resid) // allow modal dialogs to be created without template
  {
    if (!p||(p->windowTypeFlags&SWELL_DLG_WS_CHILD)) return -1;
  }
  else if (parent)
  {
    resid = (const char *)(INT_PTR)(0x400002); // force non-child, force no minimize box
  }


  int ret=-1;
  s_last_dlgret = -1;
  HWND hwnd = SWELL_CreateDialog(reshead,resid,parent,dlgproc,param);
  // create dialog
  if (hwnd)
  {
    hwnd->Retain();
    ReleaseCapture(); // force end of any captures

    WDL_PtrKeyedArray<int> restwnds;
    extern HWND__ *SWELL_topwindows;
    HWND a = SWELL_topwindows;
    while (a)
    {
      if (a!=hwnd) 
      {
        int f=0;
        if (a->m_enabled) { EnableWindow(a,FALSE); f|=1; }
        if (a->m_israised) { SWELL_SetWindowLevel(a,0); f|=2; }
        if (f) restwnds.AddUnsorted((INT_PTR)a,f);
      }
      a = a->m_next;
    }
    restwnds.Resort();
    SWELL_SetWindowLevel(hwnd,1);

    modalDlgRet r = { hwnd,false, -1 };
    s_modalDialogs.Add(&r);

    if (s_spare && s_spare_style == hwnd->m_style)
    {
      if (s_spare_timer) 
      {
        KillTimer(NULL,s_spare_timer);
        s_spare_timer = 0;
      }
      SWELL_OSWINDOW w = s_spare;
      s_spare = NULL;

      int flags = 0;
      const int dw = (hwnd->m_position.right-hwnd->m_position.left) -
                      (s_spare_rect.right - s_spare_rect.left);
      const int dh = (hwnd->m_position.bottom-hwnd->m_position.top) -
                      (s_spare_rect.bottom - s_spare_rect.top);

      if (hwnd->m_has_had_position) flags |= 1;
      if (dw || dh) flags |= 2;

      if (flags == 2)
      {
        // center on the old window
        hwnd->m_position.right -= hwnd->m_position.left;
        hwnd->m_position.bottom -= hwnd->m_position.top;
        hwnd->m_position.left = s_spare_rect.left - dw/2;
        hwnd->m_position.top = s_spare_rect.top - dh/2;
        hwnd->m_position.right += hwnd->m_position.left;
        hwnd->m_position.bottom += hwnd->m_position.top;
        flags = 3;
      }
          
      if (flags)
      {
        if (flags&2) swell_oswindow_begin_resize(w);
        swell_oswindow_resize(w, flags, hwnd->m_position);
      }
      hwnd->m_oswindow = w;
      ShowWindow(hwnd,SW_SHOWNA);
    }
    else  
    {
      swell_dlg_destroyspare();
      ShowWindow(hwnd,SW_SHOW);
    }
 
    while (!r.has_ret && !hwnd->m_hashaddestroy)
    {
      void SWELL_RunMessageLoop();
      SWELL_RunMessageLoop();
      Sleep(10);
    }
    ret=r.ret;
    s_modalDialogs.DeletePtr(&r);

    a = SWELL_topwindows;
    while (a)
    {
      if (a != hwnd) 
      {
        int f = restwnds.Get((INT_PTR)a);
        if (!a->m_enabled && (f&1)) EnableWindow(a,TRUE);
        if (!a->m_israised && (f&2)) SWELL_SetWindowLevel(a,1);
      }
      a = a->m_next;
    }
    hwnd->Release();
  }
  else 
  {
    ret = s_last_dlgret; // SWELL_CreateDialog() failed, implies WM_INITDIALOG could have called EndDialog()
  }
  // while in list, do something
  return ret;
}

HWND SWELL_CreateDialog(SWELL_DialogResourceIndex *reshead, const char *resid, HWND parent, DLGPROC dlgproc, LPARAM param)
{
  int forceStyles=0; // 1=resizable, 2=no minimize, 4=no close
  bool forceNonChild=false;
  if ((((INT_PTR)resid)&~0xf)==0x400000)
  {
    forceStyles = (int) (((INT_PTR)resid)&0xf);
    if (forceStyles) forceNonChild=true;
    resid=0;
  }
  SWELL_DialogResourceIndex *p=resById(reshead,resid);
  if (!p&&resid) return 0;
  
  RECT r={0,0,SWELL_UI_SCALE(p ? p->width : 300), SWELL_UI_SCALE(p ? p->height : 200) };
  HWND owner=NULL;

  if (!forceNonChild && parent && (!p || (p->windowTypeFlags&SWELL_DLG_WS_CHILD)))
  {
  } 
  else 
  {
    owner = parent;
    parent = NULL; // top level window
  }

  HWND__ *h = new HWND__(parent,0,&r,NULL,false,NULL,NULL, owner);
  if (forceNonChild || (p && !(p->windowTypeFlags&SWELL_DLG_WS_CHILD)))
  {
    if ((forceStyles&1) || (p && (p->windowTypeFlags&SWELL_DLG_WS_RESIZABLE))) 
      h->m_style |= WS_THICKFRAME|WS_CAPTION;
    else h->m_style |= WS_CAPTION;
  }
  else if (!p && !parent) h->m_style |= WS_CAPTION;
  else if (parent && (!p || (p->windowTypeFlags&SWELL_DLG_WS_CHILD))) h->m_style |= WS_CHILD;

  if (p)
  {
    h->m_style |= p->windowTypeFlags & (WS_CLIPSIBLINGS);
    if (p->windowTypeFlags&SWELL_DLG_WS_DROPTARGET)
      h->m_exstyle|=WS_EX_ACCEPTFILES;
  }

  h->Retain();

  if (p)
  {
    p->createFunc(h,p->windowTypeFlags);
    if (p->title) SetWindowText(h,p->title);

    h->m_dlgproc = dlgproc;
    h->m_wndproc = SwellDialogDefaultWindowProc;

    HWND hFoc=h->m_children;
    while (hFoc)
    {
      if (hFoc->m_wantfocus && hFoc->m_visible && hFoc->m_enabled) 
      {
        h->m_focused_child = hFoc; // default focus to hFoc, but set focus more aggressively after WM_INITDIALOG if the dlgproc returns 1
        break;
      }
      hFoc=hFoc->m_next;
    }

    if (hFoc) hFoc->Retain();

    if (h->m_dlgproc(h,WM_INITDIALOG,(WPARAM)hFoc,param))
    {
      if (hFoc && hFoc->m_wantfocus && hFoc->m_visible && hFoc->m_enabled)
      {
        if (!h->m_hashaddestroy && !hFoc->m_hashaddestroy)
          SetFocus(hFoc);
      }
    }

    if (hFoc) hFoc->Release();
  }
  else
  {
    h->m_wndproc = (WNDPROC)dlgproc;
    h->m_wndproc(h,WM_CREATE,0,param);
  }

  HWND rv = h->m_hashaddestroy ? NULL : h;
  h->Release();
  return rv;
}


HMENU SWELL_GetDefaultWindowMenu() { return g_swell_defaultmenu; }
void SWELL_SetDefaultWindowMenu(HMENU menu)
{
  g_swell_defaultmenu=menu;
}
HMENU SWELL_GetDefaultModalWindowMenu() 
{ 
  return g_swell_defaultmenumodal; 
}
void SWELL_SetDefaultModalWindowMenu(HMENU menu)
{
  g_swell_defaultmenumodal=menu;
}



SWELL_DialogResourceIndex *SWELL_curmodule_dialogresource_head; // this eventually will go into a per-module stub file

#endif
