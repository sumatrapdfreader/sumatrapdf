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

#define SWELL_INTERNAL_MERGESORT_IMPL
#define SWELL_INTERNAL_HTREEITEM_IMPL
#include "swell-internal.h"

#include <math.h>
#include "../mutex.h"
#include "../ptrlist.h"
#include "../assocarray.h"
#include "../queue.h"
#include "../wdlcstring.h"
#include "../wdlutf8.h"

#include "swell-dlggen.h"

bool swell_is_likely_capslock; // only used when processing dit events for a-zA-Z
SWELL_OSWINDOW SWELL_focused_oswindow; // top level window which has focus (might not map to a HWND__!)
HWND swell_captured_window;

#define STATEIMAGEMASKTOINDEX(x) (((x)>>16)&0xff)

bool swell_is_virtkey_char(int c)
{
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

bool swell_app_is_inactive;
HWND__ *SWELL_topwindows;
HWND swell_oswindow_to_hwnd(SWELL_OSWINDOW w)
{
  if (!w) return NULL;
  HWND a = SWELL_topwindows;
  while (a && a->m_oswindow != w) a=a->m_next;
  return a;
}

void swell_on_toplevel_raise(SWELL_OSWINDOW wnd) // called by swell-generic-gdk when a window is focused
{
  HWND hwnd = swell_oswindow_to_hwnd(wnd);
  if (hwnd && hwnd != SWELL_topwindows)
  {
    // implies hwnd->m_prev

    VALIDATE_HWND_LIST(SWELL_topwindows,NULL);

    // remove from list
    hwnd->m_prev->m_next = hwnd->m_next;
    if (hwnd->m_next) hwnd->m_next->m_prev = hwnd->m_prev;

    // insert at front of list
    hwnd->m_prev = NULL;
    hwnd->m_next = SWELL_topwindows;
    if (SWELL_topwindows) SWELL_topwindows->m_prev = hwnd;
    SWELL_topwindows = hwnd;
    VALIDATE_HWND_LIST(SWELL_topwindows,NULL);
  }
}

HWND__::HWND__(HWND par, int wID, RECT *wndr, const char *label, bool visible, WNDPROC wndproc, DLGPROC dlgproc, HWND ownerWindow)
{
  m_refcnt=1;
  m_private_data=0;
  m_israised=false;
  m_has_had_position=false;
  m_oswindow_private=0;
  m_oswindow_fullscreen=false;

     m_classname = "unknown";
     m_wndproc=wndproc?wndproc:dlgproc?(WNDPROC)SwellDialogDefaultWindowProc:(WNDPROC)DefWindowProc;
     m_dlgproc=dlgproc;
     m_userdata=0;
     m_style=0;
     m_exstyle=0;
     m_id=wID;
     m_owned_list=m_owner=m_owned_next=m_owned_prev=NULL;
     m_children=m_parent=m_next=m_prev=NULL;
     m_focused_child=NULL;
     if (wndr) m_position = *wndr;
     else memset(&m_position,0,sizeof(m_position));
     memset(&m_extra,0,sizeof(m_extra));
     m_visible=visible;
     m_hashaddestroy=0;
     m_enabled=true;
     m_wantfocus=true;
     m_menu=NULL;
     m_font=NULL;
     m_oswindow = NULL;

#ifdef SWELL_LICE_GDI
     m_paintctx=0;
     m_invalidated=true;
     m_child_invalidated=true;
     m_backingstore=0;
#endif

     if (label) m_title.Set(label);
     
     SetParent(this, par);
     if (!par && ownerWindow)
     {
       m_owned_next = ownerWindow->m_owned_list;
       ownerWindow->m_owned_list = this;
       if (m_owned_next) m_owned_next->m_owned_prev = this;
       m_owner = ownerWindow;
     }
}

HWND__::~HWND__()
{
  if (m_wndproc)
    m_wndproc(this,WM_NCDESTROY,0,0);
}



HWND GetParent(HWND hwnd)
{  
  if (hwnd)
  {
    return hwnd->m_parent ? hwnd->m_parent : hwnd->m_owner;
  }
  return NULL;
}

HWND GetDlgItem(HWND hwnd, int idx)
{
  if (!idx) return hwnd;
  if (hwnd) hwnd=hwnd->m_children;
  while (hwnd && hwnd->m_id != (UINT)idx) hwnd=hwnd->m_next;
  return hwnd;
}


LONG_PTR SetWindowLong(HWND hwnd, int idx, LONG_PTR val)
{
  if (!hwnd) return 0;
  if (idx==GWL_STYLE)
  {
    LONG ret = hwnd->m_style;
    hwnd->m_style=val & ~WS_VISIBLE;
    swell_oswindow_update_style(hwnd,ret);
    return ret & ~WS_VISIBLE;
  }
  if (idx==GWL_EXSTYLE)
  {
    LONG ret = hwnd->m_exstyle;
    hwnd->m_exstyle=val;
    return ret;
  }
  if (idx==GWL_USERDATA)
  {
    LONG_PTR ret = hwnd->m_userdata;
    hwnd->m_userdata=val;
    return ret;
  }
  if (idx==GWL_ID)
  {
    LONG ret = hwnd->m_id;
    hwnd->m_id=val;
    return ret;
  }
  
  if (idx==GWL_WNDPROC)
  {
    LONG_PTR ret = (LONG_PTR)hwnd->m_wndproc;
    hwnd->m_wndproc=(WNDPROC)val;
    return ret;
  }
  if (idx==DWL_DLGPROC)
  {
    LONG_PTR ret = (LONG_PTR)hwnd->m_dlgproc;
    hwnd->m_dlgproc=(DLGPROC)val;
    return ret;
  }
  
  if (idx>=0 && idx < 64*(int)sizeof(INT_PTR))
  {
    INT_PTR ret = hwnd->m_extra[idx/sizeof(INT_PTR)];
    hwnd->m_extra[idx/sizeof(INT_PTR)]=val;
    return (LONG_PTR)ret;
  }
  return 0;
}

LONG_PTR GetWindowLong(HWND hwnd, int idx)
{
  if (!hwnd) return 0;
  if (idx==GWL_STYLE)
  {
    LONG_PTR ret = hwnd->m_style;
    if (hwnd->m_visible) ret|=WS_VISIBLE;
    else ret &= ~WS_VISIBLE;
    return ret;
  }
  if (idx==GWL_EXSTYLE)
  {
    return hwnd->m_exstyle;
  }
  if (idx==GWL_USERDATA)
  {
    return hwnd->m_userdata;
  }
  if (idx==GWL_ID)
  {
    return hwnd->m_id;
  }
  
  if (idx==GWL_WNDPROC)
  {
    return (LONG_PTR)hwnd->m_wndproc;
  }
  if (idx==DWL_DLGPROC)
  {
    return (LONG_PTR)hwnd->m_dlgproc;
  }
  
  if (idx>=0 && idx < 64*(int)sizeof(INT_PTR))
  {
    return (LONG_PTR)hwnd->m_extra[idx/sizeof(INT_PTR)];
  }
  return 0;
}


static bool __isWindow(HWND hc, HWND hFind)
{
  while (hc)
  {
    if (hc == hFind || (hc->m_children && __isWindow(hc->m_children,hFind))) return true;
    hc = hc->m_next;
  }
  return false;
}

bool IsWindow(HWND hwnd)
{
  if (!hwnd) return false;
  HWND h = SWELL_topwindows;
  while (h)
  {
    if (hwnd == h || (h->m_children && __isWindow(h->m_children,hwnd))) return true;
    h=h->m_next;
  }

  return false;
}

bool IsWindowVisible(HWND hwnd)
{
  if (!hwnd) return false;
  while (hwnd->m_visible)
  {
    hwnd = hwnd->m_parent;
    if (!hwnd) return true;
  }
  return false;
}

bool IsModalDialogBox(HWND hwnd);

LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (!hwnd) return 0;
  WNDPROC wp = hwnd->m_wndproc;

  if (msg == WM_DESTROY)
  {
    if (hwnd->m_hashaddestroy) return 0;
    hwnd->m_hashaddestroy=1;

    if (GetCapture()==hwnd) ReleaseCapture(); 
    SWELL_MessageQueue_Clear(hwnd);
  }
  else if (hwnd->m_hashaddestroy == 2) return 0;
  else if (msg==WM_CAPTURECHANGED && hwnd->m_hashaddestroy) return 0;
    
  hwnd->Retain();

  LRESULT ret = wp ? wp(hwnd,msg,wParam,lParam) : 0;
 
  if (msg == WM_DESTROY)
  {
    if (GetCapture()==hwnd) ReleaseCapture(); 

    SWELL_MessageQueue_Clear(hwnd);
    // send WM_DESTROY to all children
    HWND tmp=hwnd->m_children;
    while (tmp)
    {
      HWND old = tmp;
      tmp=tmp->m_next;
      SendMessage(old,WM_DESTROY,0,0);
    }
    {
      tmp=hwnd->m_owned_list;
      while (tmp)
      {
        HWND old = tmp;
        tmp=tmp->m_owned_next;
        if (!IsModalDialogBox(old)) SendMessage(old,WM_DESTROY,0,0);
      }
    }
    if (SWELL_focused_oswindow && SWELL_focused_oswindow == hwnd->m_oswindow)
    {
      HWND h = hwnd->m_owner;
      while (h && !h->m_oswindow) h = h->m_parent ? h->m_parent : h->m_owner;
      swell_oswindow_focus(h && h->m_oswindow ? h : NULL);
    }
    hwnd->m_wndproc=NULL;
    hwnd->m_hashaddestroy=2;
    KillTimer(hwnd,-1);
  }
  hwnd->Release();
  return ret;
}

static void swell_removeWindowFromParentOrTop(HWND__ *hwnd, bool removeFromOwner)
{
  HWND par = hwnd->m_parent;
  if (hwnd->m_next) hwnd->m_next->m_prev = hwnd->m_prev;
  if (hwnd->m_prev) hwnd->m_prev->m_next = hwnd->m_next;
  if (par)
  { 
    if (par->m_focused_child == hwnd) par->m_focused_child=NULL;
    if (par->m_children == hwnd) par->m_children = hwnd->m_next;
  }
  if (hwnd == SWELL_topwindows) 
  { 
    SWELL_topwindows = hwnd->m_next;
    VALIDATE_HWND_LIST(SWELL_topwindows,NULL);
  }
  hwnd->m_next = hwnd->m_prev = hwnd->m_parent = NULL;
  if (par) VALIDATE_HWND_LIST(par->m_children,par);

  if (removeFromOwner)
  {
    if (hwnd->m_owned_next) hwnd->m_owned_next->m_owned_prev = hwnd->m_owned_prev;
    if (hwnd->m_owned_prev) hwnd->m_owned_prev->m_owned_next = hwnd->m_owned_next;
    if (hwnd->m_owner && hwnd->m_owner->m_owned_list == hwnd) hwnd->m_owner->m_owned_list = hwnd->m_owned_next;
    hwnd->m_owned_next = hwnd->m_owned_prev = hwnd->m_owner = NULL;
  }

  if (par && !par->m_hashaddestroy) InvalidateRect(par,NULL,FALSE);
}

void RecurseDestroyWindow(HWND hwnd)
{
  HWND tmp=hwnd->m_children;
  hwnd->m_children=NULL;

  while (tmp)
  {
    HWND old = tmp;
    tmp=tmp->m_next;
    if (tmp) tmp->m_prev = NULL;

    old->m_prev = old->m_next = NULL;
    RecurseDestroyWindow(old);
  }
  tmp=hwnd->m_owned_list;
  hwnd->m_owned_list = NULL;

  while (tmp)
  {
    HWND old = tmp;
    tmp=tmp->m_owned_next;
    if (tmp) tmp->m_owned_prev = NULL;

    old->m_owned_prev = old->m_owned_next = NULL;
    old->m_owner = NULL;
    if (old->m_hashaddestroy) RecurseDestroyWindow(old);
  }

  if (swell_captured_window == hwnd) swell_captured_window=NULL;

  swell_oswindow_destroy(hwnd);

  if (hwnd->m_menu) DestroyMenu(hwnd->m_menu);
  hwnd->m_menu=0;

#ifdef SWELL_LICE_GDI
  delete hwnd->m_backingstore;
  hwnd->m_backingstore=0;
#endif

  // remove from parent/global lists
  swell_removeWindowFromParentOrTop(hwnd, true);

  SWELL_MessageQueue_Clear(hwnd);
  KillTimer(hwnd,-1);
  hwnd->Release();
}


void DestroyWindow(HWND hwnd)
{
  if (!hwnd) return;
  if (hwnd->m_hashaddestroy) return; 
 
  // broadcast WM_DESTROY
  SendMessage(hwnd,WM_DESTROY,0,0);

  // safe to delete this window and all children directly
  RecurseDestroyWindow(hwnd);

}


bool IsWindowEnabled(HWND hwnd)
{
  if (!hwnd) return false;
  while (hwnd && hwnd->m_enabled) 
  {
    hwnd=hwnd->m_parent;
  }
  return !hwnd;
}

void EnableWindow(HWND hwnd, int enable)
{
  if (!hwnd) return;
  if (!!hwnd->m_enabled == !!enable) return;

  hwnd->m_enabled=!!enable;
  swell_oswindow_update_enable(hwnd);

  if (!enable)
  {
    if (hwnd->m_parent && hwnd->m_parent->m_focused_child == hwnd)
      hwnd->m_parent->m_focused_child = NULL;
  }
  InvalidateRect(hwnd,NULL,FALSE);
}

void SetForegroundWindow(HWND hwnd)
{
  if (!hwnd) return;

  // if a child window has focus, preserve that focus
  while (hwnd->m_parent && !hwnd->m_oswindow)
  {
    hwnd->m_parent->m_focused_child = hwnd;
    hwnd = hwnd->m_parent;
  }
  if (hwnd) swell_oswindow_focus(hwnd);
}

void SetFocusInternal(HWND hwnd)
{
  hwnd->m_focused_child=NULL; // make sure this window has focus, not a child
  SetForegroundWindow(hwnd);
}

void SetFocus(HWND hwnd)
{
  if (!hwnd) return;
  HWND oldfoc = GetFocus();
  SetFocusInternal(hwnd);

  if (hwnd->m_classname && oldfoc != hwnd)
  {
    if (!strcmp(hwnd->m_classname,"Edit") ||
        !strcmp(hwnd->m_classname,"combobox"))
      SendMessage(hwnd,EM_SETSEL,0,-1);
  }
}


int IsChild(HWND hwndParent, HWND hwndChild)
{
  if (!hwndParent || !hwndChild || hwndParent == hwndChild) return 0;

  while (hwndChild && hwndChild != hwndParent) hwndChild = hwndChild->m_parent;

  return hwndChild == hwndParent;
}


HWND GetFocusIncludeMenus()
{
  HWND h = swell_app_is_inactive ? NULL : swell_oswindow_to_hwnd(SWELL_focused_oswindow);
  while (h) 
  {
    HWND fc = h->m_focused_child;
    if (!fc) break;
    HWND s = h->m_children;
    while (s && s != fc) s = s->m_next;
    if (!s) break;
    h = s; // descend to focused child
  }
  return h;
}

HWND GetForegroundWindow()
{
  return GetFocus();
}

HWND GetFocus()
{
  HWND h =GetFocusIncludeMenus();
  HWND ho;
  while (h && (ho=(HWND)GetProp(h,"SWELL_MenuOwner"))) h=ho; 
  return h;
}

void ScreenToClient(HWND hwnd, POINT *pt)
{
  if (!hwnd) return;
  
  HWND tmp=hwnd;
  while (tmp)
  {
    NCCALCSIZE_PARAMS p = {{ tmp->m_position, }, };
    if (tmp->m_wndproc) tmp->m_wndproc(tmp,WM_NCCALCSIZE,0,(LPARAM)&p);

    pt->x -= p.rgrc[0].left;
    pt->y -= p.rgrc[0].top;
    tmp = tmp->m_parent;
  }
}

void ClientToScreen(HWND hwnd, POINT *pt)
{
  if (!hwnd) return;
  
  HWND tmp=hwnd;
  while (tmp)
  {
    NCCALCSIZE_PARAMS p={{tmp->m_position, }, };
    if (tmp->m_wndproc) tmp->m_wndproc(tmp,WM_NCCALCSIZE,0,(LPARAM)&p);
    pt->x += p.rgrc[0].left;
    pt->y += p.rgrc[0].top;
    tmp = tmp->m_parent;
  }
}

void GetWindowContentViewRect(HWND hwnd, RECT *r)
{
  if (hwnd && hwnd->m_oswindow) 
  {
    *r = hwnd->m_position;
    return;
  }
  GetWindowRect(hwnd,r);
}

void GetClientRect(HWND hwnd, RECT *r)
{
  r->left=r->top=r->right=r->bottom=0;
  if (!hwnd) return;
  
  r->right = hwnd->m_position.right - hwnd->m_position.left;
  r->bottom = hwnd->m_position.bottom - hwnd->m_position.top;

  NCCALCSIZE_PARAMS tr={{*r, },};
  SendMessage(hwnd,WM_NCCALCSIZE,FALSE,(LPARAM)&tr);
  r->right = r->left + (tr.rgrc[0].right-tr.rgrc[0].left);
  r->bottom=r->top + (tr.rgrc[0].bottom-tr.rgrc[0].top);
}



void SetWindowPos(HWND hwnd, HWND zorder, int x, int y, int cx, int cy, int flags)
{
  if (!hwnd) return;
 // todo: handle SWP_SHOWWINDOW
  RECT f = hwnd->m_position;
  int reposflag = 0;
  if (!(flags&SWP_NOZORDER))
  {
    if (hwnd->m_parent && zorder != hwnd)
    {
      HWND par = hwnd->m_parent;
      HWND tmp = par->m_children;
      while (tmp && tmp != hwnd) tmp=tmp->m_next;
      if (tmp) // we are in the list, so we can do a reorder
      {
        // take hwnd out of list
        if (hwnd->m_prev) hwnd->m_prev->m_next = hwnd->m_next;
        else par->m_children = hwnd->m_next;
        if (hwnd->m_next) hwnd->m_next->m_prev = hwnd->m_prev;
        hwnd->m_next=hwnd->m_prev=NULL;// leave hwnd->m_parent valid since it wont change

        // add back in
        tmp = par->m_children;
        if (zorder == HWND_BOTTOM || !tmp) // insert at front of list
        {
          if (tmp) tmp->m_prev=hwnd;
          hwnd->m_next = tmp;
          par->m_children = hwnd;
        }
        else
        {
          // zorder could be HWND_TOP here
          while (tmp && tmp != zorder && tmp->m_next) tmp=tmp->m_next;

          // tmp is either zorder or the last item in the list
          hwnd->m_next = tmp->m_next;
          tmp->m_next = hwnd;
          if (hwnd->m_next) hwnd->m_next->m_prev = hwnd;
          hwnd->m_prev = tmp;
        }
        reposflag|=4;
      }
    }
  }
  if (!(flags&SWP_NOMOVE))
  {
    int oldw = f.right-f.left;
    int oldh = f.bottom-f.top; 
    f.left=x; 
    f.right=x+oldw;
    f.top=y; 
    f.bottom=y+oldh;
    reposflag|=1;
    hwnd->m_has_had_position=true;
  }
  if (!(flags&SWP_NOSIZE))
  {
    f.right = f.left + cx;
    f.bottom = f.top + cy;
    reposflag|=2;
  }
  if (reposflag)
  {
    if (hwnd->m_oswindow && (reposflag&2))
    {
      swell_oswindow_begin_resize(hwnd->m_oswindow);
    }

    if (reposflag&3) 
    {
      hwnd->m_position = f;
    }

    if (hwnd->m_oswindow && !hwnd->m_oswindow_fullscreen)
    {
      swell_oswindow_resize(hwnd->m_oswindow,reposflag,f);
      if (reposflag&2) SendMessage(hwnd,WM_SIZE,0,0);
    }
    else
    {
      if (reposflag&2) SendMessage(hwnd,WM_SIZE,0,0);
      InvalidateRect(hwnd->m_parent ? hwnd->m_parent : hwnd,NULL,FALSE);
    }
  }
  swell_oswindow_postresize(hwnd,f);
}


BOOL EnumWindows(BOOL (*proc)(HWND, LPARAM), LPARAM lp)
{
  HWND h = SWELL_topwindows;
  if (!proc) return FALSE;
  while (h)
  {
    if (!proc(h,lp)) return FALSE;
    h = h->m_next;
  }
  return TRUE;
}

HWND GetWindow(HWND hwnd, int what)
{
  if (!hwnd) return 0;
  
  if (what == GW_CHILD) return hwnd->m_children;
  if (what == GW_OWNER) return hwnd->m_owner;
  if (what == GW_HWNDNEXT) return hwnd->m_next;
  if (what == GW_HWNDPREV) return hwnd->m_prev;
  if (what == GW_HWNDFIRST) 
  { 
    while (hwnd->m_prev) hwnd = hwnd->m_prev;
    return hwnd;
  }
  if (what == GW_HWNDLAST) 
  { 
    while (hwnd->m_next) hwnd = hwnd->m_next;
    return hwnd;
  }
  return 0;
}

HWND SetParent(HWND hwnd, HWND newPar)
{
  if (!hwnd) return NULL;

  HWND oldPar = hwnd->m_parent;

  swell_removeWindowFromParentOrTop(hwnd, newPar != NULL && newPar != oldPar);

  if (newPar)
  {
    HWND fc = newPar->m_children;
    if (!fc)
    {
      newPar->m_children = hwnd;
    }
    else
    {
      while (fc->m_next) fc = fc->m_next;
      hwnd->m_prev = fc;
      fc->m_next = hwnd;
    }
    hwnd->m_parent = newPar;
    hwnd->m_style |= WS_CHILD;

    if (newPar) VALIDATE_HWND_LIST(newPar->m_children,newPar);
  }
  else // add to top level windows
  {
    hwnd->m_next=SWELL_topwindows;
    if (hwnd->m_next) hwnd->m_next->m_prev = hwnd;
    SWELL_topwindows = hwnd;
    VALIDATE_HWND_LIST(SWELL_topwindows,NULL);
    hwnd->m_style &= ~WS_CHILD;
  }

  swell_oswindow_manage(hwnd,false);
  return oldPar;
}




// timer stuff
typedef struct TimerInfoRec
{
  UINT_PTR timerid;
  HWND hwnd;
  UINT interval;
  DWORD nextFire;
  TIMERPROC tProc;
  struct TimerInfoRec *_next;
} TimerInfoRec;

static TimerInfoRec *m_timer_list;
static WDL_Mutex m_timermutex;
static pthread_t m_pmq_mainthread;

void SWELL_RunMessageLoop()
{
  SWELL_MessageQueue_Flush();
  SWELL_RunEvents();

  DWORD now = GetTickCount();
  WDL_MutexLock lock(&m_timermutex);
  TimerInfoRec *rec = m_timer_list;
  while (rec)
  {
    if (now > rec->nextFire || now < rec->nextFire - rec->interval*4)
    {
      rec->nextFire = now + rec->interval;

      HWND h = rec->hwnd;
      TIMERPROC tProc = rec->tProc;
      UINT_PTR tid = rec->timerid;
      m_timermutex.Leave();

      if (tProc) tProc(h,WM_TIMER,tid,now);
      else if (h) SendMessage(h,WM_TIMER,tid,0);

      m_timermutex.Enter();
      TimerInfoRec *tr = m_timer_list;
      while (tr && tr != rec) tr=tr->_next;
      if (!tr) 
      {
        rec = m_timer_list;  // if no longer in the list, then abort
        continue;
      }
    }
    rec=rec->_next;
  } 
}


UINT_PTR SetTimer(HWND hwnd, UINT_PTR timerid, UINT rate, TIMERPROC tProc)
{
  if (!hwnd && !tProc) return 0; // must have either callback or hwnd
  
  if (hwnd && !timerid) return 0;

  if (hwnd && hwnd->m_hashaddestroy) return 0;

  WDL_MutexLock lock(&m_timermutex);
  TimerInfoRec *rec=NULL;
  if (hwnd||timerid)
  {
    rec = m_timer_list;
    while (rec)
    {
      if (rec->timerid == timerid && rec->hwnd == hwnd) // works for both kinds
        break;
      rec=rec->_next;
    }
  }
  
  bool recAdd=false;
  if (!rec) 
  {
    rec=(TimerInfoRec*)malloc(sizeof(TimerInfoRec));
    recAdd=true;
  }
   
  rec->tProc = tProc;
  rec->timerid=timerid;
  rec->hwnd=hwnd;
  rec->interval = rate<1?1: rate;
  rec->nextFire = GetTickCount() + rate;
  
  if (!hwnd) timerid = rec->timerid = (UINT_PTR)rec;

  if (recAdd)
  {
    rec->_next=m_timer_list;
    m_timer_list=rec;
  }
  
  return timerid;
}

BOOL KillTimer(HWND hwnd, UINT_PTR timerid)
{
  if (!hwnd && !timerid) return FALSE;

  WDL_MutexLock lock(&m_timermutex);
  BOOL rv=FALSE;

  // don't allow removing all global timers
  if (timerid!=(UINT_PTR)-1 || hwnd) 
  {
    TimerInfoRec *rec = m_timer_list, *lrec=NULL;
    while (rec)
    {
      if (rec->hwnd == hwnd && (timerid==(UINT_PTR)-1 || rec->timerid == timerid))
      {
        TimerInfoRec *nrec = rec->_next;
        
        // remove self from list
        if (lrec) lrec->_next = nrec;
        else m_timer_list = nrec;
        
        free(rec);

        rv=TRUE;
        if (timerid!=(UINT_PTR)-1) break;
        
        rec=nrec;
      }
      else 
      {
        lrec=rec;
        rec=rec->_next;
      }
    }
  }
  return rv;
}

BOOL SetDlgItemText(HWND hwnd, int idx, const char *text)
{
  hwnd =(idx ? GetDlgItem(hwnd,idx) : hwnd);
  if (!hwnd) return false;

  if (!text) text="";
 

  if (strcmp(hwnd->m_title.Get(), text))
  {
    hwnd->m_title.Set(text);
    swell_oswindow_update_text(hwnd);
  } 
  SendMessage(hwnd,WM_SETTEXT,0,(LPARAM)text);
  return true;
}

BOOL GetDlgItemText(HWND hwnd, int idx, char *text, int textlen)
{
  *text=0;
  hwnd = idx?GetDlgItem(hwnd,idx) : hwnd;
  if (!hwnd) return false;
  
  // todo: sendmessage WM_GETTEXT etc? special casing for combo boxes etc
  lstrcpyn_safe(text,hwnd->m_title.Get(), textlen);
  return true;
}

void CheckDlgButton(HWND hwnd, int idx, int check)
{
  hwnd = GetDlgItem(hwnd,idx);
  if (!hwnd) return;
  SendMessage(hwnd,BM_SETCHECK,check,0);
}


int IsDlgButtonChecked(HWND hwnd, int idx)
{
  hwnd = GetDlgItem(hwnd,idx);
  if (!hwnd) return 0;
  return SendMessage(hwnd,BM_GETCHECK,0,0);
}


BOOL SetDlgItemInt(HWND hwnd, int idx, int val, int issigned)
{
  char buf[128];
  sprintf(buf,issigned?"%d":"%u",val);
  return SetDlgItemText(hwnd,idx,buf);
}

int GetDlgItemInt(HWND hwnd, int idx, BOOL *translated, int issigned)
{
  char buf[128];
  if (!GetDlgItemText(hwnd,idx,buf,sizeof(buf)))
  {
    if (translated) *translated=0;
    return 0;
  }
  char *p=buf;
  while (*p == ' ' || *p == '\t') p++;
  int a=atoi(p);
  if ((a<0 && !issigned) || (!a && p[0] != '0')) { if (translated) *translated=0; return 0; }
  if (translated) *translated=1;
  return a;
}

void ShowWindow(HWND hwnd, int cmd)
{
  if (!hwnd) return;
 
  if (cmd==SW_SHOW||cmd==SW_SHOWNA) 
  {
    if (hwnd->m_visible) cmd = SW_SHOWNA; // do not take focus if already visible
    hwnd->m_visible=true;
  }
  else if (cmd==SW_HIDE) 
  {
    if (hwnd->m_visible)
    {
      hwnd->m_visible=false;
      if (hwnd->m_parent)
        InvalidateRect(hwnd->m_parent,&hwnd->m_position,FALSE);
    }
  }

  swell_oswindow_manage(hwnd,cmd==SW_SHOW);
  if (cmd == SW_SHOW) 
  {
    SetForegroundWindow(hwnd);
  }

  InvalidateRect(hwnd,NULL,FALSE);

}

void *SWELL_ModalWindowStart(HWND hwnd)
{
  return 0;
}

bool SWELL_ModalWindowRun(void *ctx, int *ret) // returns false and puts retval in *ret when done
{
  return false;
}

void SWELL_ModalWindowEnd(void *ctx)
{
  if (ctx) 
  {
  }
}

void SWELL_CloseWindow(HWND hwnd)
{
  DestroyWindow(hwnd);
}


static void Draw3DBox(HDC hdc, const RECT *r, int bgc, int topc, int botc, bool swap=false)
{
  RECT tr = *r;
  tr.right--;
  tr.bottom--;
  if (bgc != -1)
  {
    tr.left++;
    tr.top++;
    HBRUSH br = CreateSolidBrush(bgc);
    FillRect(hdc,&tr,br);
    DeleteObject(br);
    tr.left--;
    tr.top--;
  }

  HPEN pen = CreatePen(PS_SOLID,0,swap?botc:topc);
  HPEN pen2 = CreatePen(PS_SOLID,0,swap?topc:botc);
  HGDIOBJ oldpen = SelectObject(hdc,pen);
  MoveToEx(hdc,tr.left,tr.bottom,NULL);
  LineTo(hdc,tr.left,tr.top);
  LineTo(hdc,tr.right,tr.top);

  SelectObject(hdc,pen2);
  LineTo(hdc,tr.right,tr.bottom);
  LineTo(hdc,tr.left,tr.bottom);

  SelectObject(hdc,oldpen);
  DeleteObject(pen);
  DeleteObject(pen2);
}


#include "swell-dlggen.h"

static HWND m_make_owner;
static RECT m_transform;
static bool m_doautoright;
static RECT m_lastdoauto;
static bool m_sizetofits;

#define ACTIONTARGET (m_make_owner)

void SWELL_MakeSetCurParms(float xscale, float yscale, float xtrans, float ytrans, HWND parent, bool doauto, bool dosizetofit)
{
  if (g_swell_ui_scale != 256 && xscale != 1.0f && yscale != 1.0f)
  {
    const float m = g_swell_ui_scale/256.0f;
    xscale *= m;
    yscale *= m;
  }
  m_sizetofits=dosizetofit;
  m_lastdoauto.left = 0;
  m_lastdoauto.top = -6553600;
  m_lastdoauto.right = 0;
  m_doautoright=doauto;
  m_transform.left=(int)(xtrans*65536.0);
  m_transform.top=(int)(ytrans*65536.0);
  m_transform.right=(int)(xscale*65536.0);
  m_transform.bottom=(int)(yscale*65536.0);
  m_make_owner=parent;
}

static void UpdateAutoCoords(RECT r)
{
  m_lastdoauto.right=r.left + r.right - m_lastdoauto.left;
}


static RECT MakeCoords(int x, int y, int w, int h, bool wantauto)
{
  if (w<0&&h<0)
  {
    RECT r = { -x, -y, -x-w, -y-h};
    return r;
  }

  float ysc=m_transform.bottom/65536.0;
  int newx=(int)((x+m_transform.left/65536.0)*m_transform.right/65536.0 + 0.5);
  int newy=(int)(((((double)y+(double)m_transform.top/65536.0) )*ysc) + 0.5);
                         
  RECT  ret= { newx,  
                         newy,                  
                        (int) (newx + w*(double)m_transform.right/65536.0+0.5),
                        (int) (newy + h*fabs(ysc)+0.5)
             };
                        

  RECT oret=ret;
  if (wantauto && m_doautoright)
  {
    float dx = ret.left - m_lastdoauto.left;
    if (fabs(dx)<32 && m_lastdoauto.top >= ret.top && m_lastdoauto.top < ret.bottom)
    {
      ret.left += (int) m_lastdoauto.right;
    }
    
    m_lastdoauto.left = oret.right;
    m_lastdoauto.top = (ret.top + ret.bottom)*0.5;
    m_lastdoauto.right=0;
  }
  return ret;
}

#define TRANSFORMFONTSIZE ((m_transform.right/65536.0+1.0)*3.7)


#ifdef SWELL_LICE_GDI
//#define SWELL_ENABLE_VIRTWND_CONTROLS
#include "../wingui/virtwnd-controls.h"
#endif

static LRESULT WINAPI virtwndWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
#ifdef SWELL_ENABLE_VIRTWND_CONTROLS
  WDL_VWnd *vwnd = (WDL_VWnd *) ( msg == WM_CREATE ? (void*)lParam : GetProp(hwnd,"WDL_control_vwnd") );
  if (vwnd) switch (msg)
  {
    case WM_CREATE:
      {
        SetProp(hwnd,"WDL_control_vwnd",vwnd);
        RECT r;
        GetClientRect(hwnd,&r);
        vwnd->SetRealParent(hwnd);
        vwnd->SetPosition(&r);
        vwnd->SetID(0xf);
      }
    return 0;
    case WM_SIZE:
      {
        RECT r;
        GetClientRect(hwnd,&r);
        vwnd->SetPosition(&r);
        InvalidateRect(hwnd,NULL,FALSE);
      }
    break;
    case WM_COMMAND:
      if (LOWORD(wParam)==0xf) SendMessage(GetParent(hwnd),WM_COMMAND,(wParam&0xffff0000) | GetWindowLong(hwnd,GWL_ID),NULL);
    break;
    case WM_DESTROY:
      RemoveProp(hwnd,"WDL_control_vwnd");
      delete vwnd;
      vwnd=0;
    return 0;
    case WM_LBUTTONDOWN:
      SetCapture(hwnd);
      vwnd->OnMouseDown(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
    return 0;
    case WM_MOUSEMOVE:
      vwnd->OnMouseMove(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
    return 0;
    case WM_LBUTTONUP:
      ReleaseCapture(); 
      vwnd->OnMouseUp(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
    return 0;
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 

          HDC hdc = ps.hdc;
          if (hdc)
          {
            RECT tr = ps.rcPaint; // todo: offset by surface_offs.x/y
            vwnd->OnPaint(hdc->surface,hdc->surface_offs.x,hdc->surface_offs.y,&tr);
            vwnd->OnPaintOver(hdc->surface,hdc->surface_offs.x,hdc->surface_offs.y,&tr);
          }

          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_SETTEXT:
      if (lParam)
      {
        if (!strcmp(vwnd->GetType(),"vwnd_iconbutton")) 
        {
          WDL_VirtualIconButton *b = (WDL_VirtualIconButton *) vwnd;
          b->SetTextLabel((const char *)lParam);
        }
      }
    break;
    case BM_SETCHECK:
    case BM_GETCHECK:
      if (!strcmp(vwnd->GetType(),"vwnd_iconbutton")) 
      {
        WDL_VirtualIconButton *b = (WDL_VirtualIconButton *) vwnd;
        if (msg == BM_GETCHECK) return b->GetCheckState();

        b->SetCheckState(wParam);
      }
    return 0;
  }
#endif
  return DefWindowProc(hwnd,msg,wParam,lParam);
}

#ifdef SWELL_ENABLE_VIRTWND_CONTROLS
static HWND swell_makeButton(HWND owner, int idx, RECT *tr, const char *label, bool vis, int style)
{
  WDL_VirtualIconButton *vwnd = new WDL_VirtualIconButton;
  if (label) vwnd->SetTextLabel(label);
  vwnd->SetForceBorder(true);
  if (style & BS_AUTOCHECKBOX) vwnd->SetCheckState(0);
  HWND hwnd = new HWND__(owner,idx,tr,label,vis,virtwndWindowProc);
  hwnd->m_classname = "Button";
  hwnd->m_style = style|WS_CHILD;
  hwnd->m_wndproc(hwnd,WM_CREATE,0,(LPARAM)vwnd);
  return hwnd;
}

#endif

static void paintDialogBackground(HWND hwnd, const RECT *r, HDC hdc)
{
  HBRUSH hbrush = (HBRUSH) SendMessage(GetParent(hwnd),WM_CTLCOLORSTATIC,(WPARAM)hdc,(LPARAM)hwnd);
  if (hbrush == (HBRUSH)(INT_PTR)1) return;

  if (hbrush) 
  {
    FillRect(hdc,r,hbrush);
  }
  else
  {
    SWELL_FillDialogBackground(hdc,r,0);
  }
}

static bool fast_has_focus(HWND hwnd)
{
  if (!hwnd || !SWELL_focused_oswindow || swell_app_is_inactive) return false;
  HWND par;
  while ((par=hwnd->m_parent)!=NULL && par->m_focused_child==hwnd)
  {
    if (par->m_oswindow == SWELL_focused_oswindow) return true;
    hwnd=par;
  }
  return false;
}

static bool draw_focus_indicator(HWND hwnd, HDC hdc, const RECT *drawr)
{
  if (!fast_has_focus(hwnd)) return false;

  RECT r,tr;
  const int sz = SWELL_UI_SCALE(3);
  if (drawr) r=*drawr;
  else GetClientRect(hwnd,&r);

  HBRUSH br = CreateSolidBrushAlpha(g_swell_ctheme.focus_hilight,.75f);
  tr=r; tr.right = tr.left+sz; FillRect(hdc,&tr,br);
  tr=r; tr.left = tr.right-sz; FillRect(hdc,&tr,br);
  tr=r; tr.left+=sz; tr.right-=sz; 
  tr.bottom = tr.top+sz; FillRect(hdc,&tr,br);
  tr.bottom = r.bottom; tr.top = tr.bottom-sz; FillRect(hdc,&tr,br);

  DeleteObject(br);
  return true;
}


#ifndef SWELL_ENABLE_VIRTWND_CONTROLS
struct buttonWindowState
{
  buttonWindowState() { bitmap=0; bitmap_mode=0; state=0; }
  ~buttonWindowState() { /* if (bitmap) DeleteObject(bitmap);  */ }

  HICON bitmap;
  int bitmap_mode;
  int state;
};

static LRESULT WINAPI buttonWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_NCDESTROY:
      delete (buttonWindowState *)hwnd->m_private_data;
      hwnd->m_private_data=0;
    break;
    case WM_LBUTTONDOWN:
      SetFocusInternal(hwnd);
      SetCapture(hwnd);
      SendMessage(hwnd,WM_USER+100,0,0); // invalidate
    return 0;
    case WM_MOUSEMOVE:
    return 0;
    case WM_KEYDOWN:
      if (wParam == VK_SPACE) goto fakeButtonClick;
      if (wParam == VK_RETURN)
      {
        if (!(hwnd->m_style & 0xf))
          goto fakeButtonClick;
      }
    break;
    case WM_LBUTTONUP:
      if (GetCapture()==hwnd)
      {
fakeButtonClick:
        buttonWindowState *s = (buttonWindowState*)hwnd->m_private_data;
        ReleaseCapture(); // WM_CAPTURECHANGED will take care of the invalidate
        RECT r;
        GetClientRect(hwnd,&r);
        POINT p={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
        hwnd->Retain();
        if ((msg==WM_KEYDOWN||PtInRect(&r,p)) && hwnd->m_id && hwnd->m_parent) 
        {
          int sf = (hwnd->m_style & 0xf);
          if (sf == BS_AUTO3STATE)
          {
            int a = s->state&3;
            if (a==0) a=1;
            else if (a==1) a=2;
            else a=0;
            s->state = (a) | (s->state&~3);
          }    
          else if (sf == BS_AUTOCHECKBOX)
          {
            s->state = (!(s->state&3)) | (s->state&~3);
          }
          else if (sf == BS_AUTORADIOBUTTON)
          {
            int x;
            for (x=0;x<2;x++)
            {
              HWND nw = x ? hwnd->m_next : hwnd->m_prev;
              while (nw)
              {
                if (nw->m_classname && !strcmp(nw->m_classname,"Button"))
                {
                  if (x && (nw->m_style & WS_GROUP)) break;

                  if ((nw->m_style & 0xf) == BS_AUTORADIOBUTTON)
                  {
                    buttonWindowState *nws = (buttonWindowState*)nw->m_private_data;
                    if (nws && (nws->state&3))
                    {
                      nws->state &= ~3;
                      InvalidateRect(nw,NULL,FALSE);
                    }
                  }
  
                  if (nw->m_style & WS_GROUP) break;
                }
                else 
                {
                  break;
                }

                nw=x ? nw->m_next : nw->m_prev;
              }
            }

            s->state = 1 | (s->state&~3);
          }
          SendMessage(hwnd->m_parent,WM_COMMAND,MAKEWPARAM(hwnd->m_id,BN_CLICKED),(LPARAM)hwnd);
        }
        if (msg == WM_KEYDOWN) InvalidateRect(hwnd,NULL,FALSE);
        hwnd->Release();
      }
    return 0;
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          buttonWindowState *s = (buttonWindowState*)hwnd->m_private_data;
          RECT r; 
          GetClientRect(hwnd,&r); 

          bool pressed = GetCapture()==hwnd;

          SetBkMode(ps.hdc,TRANSPARENT);

          if (hwnd->m_enabled) 
            SetTextColor(ps.hdc, g_swell_ctheme.button_text);

          paintDialogBackground(hwnd,&r,ps.hdc);

          if (!hwnd->m_enabled) 
            SetTextColor(ps.hdc, g_swell_ctheme.button_text_disabled);

          int f=DT_VCENTER;
          int sf = (hwnd->m_style & 0xf);
          if (sf == BS_OWNERDRAW)
          {
            if (hwnd->m_parent)
            {
              DRAWITEMSTRUCT dis = { ODT_BUTTON, hwnd->m_id, 0, 0, (UINT)(pressed?ODS_SELECTED:0),hwnd,ps.hdc,r,(DWORD_PTR)hwnd->m_userdata };
              SendMessage(hwnd->m_parent,WM_DRAWITEM,(WPARAM)hwnd->m_id,(LPARAM)&dis);
            }
            EndPaint(hwnd,&ps);
            return 0;
          }

          const bool ischk = sf == BS_AUTO3STATE || sf == BS_AUTOCHECKBOX || sf == BS_AUTORADIOBUTTON;
          if (!ischk)
          {
            Draw3DBox(ps.hdc,&r,g_swell_ctheme.button_bg,
              g_swell_ctheme.button_hilight,
              g_swell_ctheme.button_shadow,pressed);

            if (hwnd->m_style & BS_LEFT)
              r.left+=2;
            else
              f|=DT_CENTER;
            if (pressed) 
            {
              const int pad = SWELL_UI_SCALE(2);
              r.left+=pad;
              r.top+=pad;
              if (s->bitmap) { r.right+=pad; r.bottom+=pad; }
            }
          }

          if (draw_focus_indicator(hwnd,ps.hdc,NULL))
          {
            KillTimer(hwnd,1);
            SetTimer(hwnd,1,100,NULL);
          }

          if (ischk)
          {
            const int chksz = SWELL_UI_SCALE(12), chki = SWELL_UI_SCALE(2);
            RECT tr={r.left+chki,(r.top+r.bottom)/2-chksz/2,r.left+chki+chksz};
            tr.bottom = tr.top+chksz;

            HPEN pen=CreatePen(PS_SOLID,0,g_swell_ctheme.checkbox_fg);
            HGDIOBJ oldPen = SelectObject(ps.hdc,pen);
            int st = (int)(s->state&3);
            if (sf == BS_AUTOCHECKBOX || sf == BS_AUTO3STATE)
            {
              if (st==3||(st==2 && (hwnd->m_style & 0xf) == BS_AUTOCHECKBOX)) st=1;
              
              Draw3DBox(ps.hdc,&tr,
                 st==2?g_swell_ctheme.checkbox_inter:
                    g_swell_ctheme.checkbox_bg,
                 g_swell_ctheme.button_shadow,
                 g_swell_ctheme.button_hilight);

              if (st == 1||pressed)
              {
                RECT ar=tr;
                ar.left+=SWELL_UI_SCALE(2);
                ar.right-=SWELL_UI_SCALE(3);
                ar.top+=SWELL_UI_SCALE(2);
                ar.bottom-=SWELL_UI_SCALE(3);
                if (pressed) 
                { 
                  const int rsz=chksz/4;
                  ar.left+=rsz;
                  ar.top+=rsz;
                  ar.right-=rsz;
                  ar.bottom-=rsz;
                }
                MoveToEx(ps.hdc,ar.left,ar.top,NULL);
                LineTo(ps.hdc,ar.right,ar.bottom);
                MoveToEx(ps.hdc,ar.right,ar.top,NULL);
                LineTo(ps.hdc,ar.left,ar.bottom);
              }
            }
            else if (sf == BS_AUTORADIOBUTTON)
            {
              HBRUSH br = CreateSolidBrush(g_swell_ctheme.checkbox_bg);
              HGDIOBJ oldBrush = SelectObject(ps.hdc,br);
              Ellipse(ps.hdc,tr.left+1,tr.top+1,tr.right-1,tr.bottom-1);
              SelectObject(ps.hdc,oldBrush);
              DeleteObject(br);
              if (st)
              {
                const int amt =  (tr.right-tr.left)/6 + SWELL_UI_SCALE(2);
                br = CreateSolidBrush(g_swell_ctheme.checkbox_fg);
                oldBrush = SelectObject(ps.hdc,br);
                Ellipse(ps.hdc,tr.left+amt,tr.top+amt,tr.right-amt,tr.bottom-amt);
                SelectObject(ps.hdc,oldBrush);
                DeleteObject(br);
              }
            }
            SelectObject(ps.hdc,oldPen);
            DeleteObject(pen);
            r.left += chksz + SWELL_UI_SCALE(5);
            SetTextColor(ps.hdc,
              hwnd->m_enabled ? g_swell_ctheme.checkbox_text :
                g_swell_ctheme.checkbox_text_disabled);
          }

          if (s->bitmap)
          {
            BITMAP inf={0,};
            GetObject(s->bitmap,sizeof(BITMAP),&inf);
            RECT cr;
            cr.left = (r.right+r.left - inf.bmWidth)/2;
            cr.top = (r.bottom+r.top - inf.bmHeight)/2;
            cr.right = cr.left+inf.bmWidth;
            cr.bottom = cr.top+inf.bmHeight;
            DrawImageInRect(ps.hdc,s->bitmap,&cr);
          }
          else
          {
            char buf[512];
            buf[0]=0;
            GetWindowText(hwnd,buf,sizeof(buf));
            if (buf[0]) DrawText(ps.hdc,buf,-1,&r,f);
          }

          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_TIMER:
      if (wParam==1)
      {
        if (!fast_has_focus(hwnd))
        {
          KillTimer(hwnd,1);
          InvalidateRect(hwnd,NULL,FALSE);
        }
      }
    break;
    case BM_GETCHECK:
      if (hwnd)
      {
        buttonWindowState *s = (buttonWindowState*)hwnd->m_private_data;
        return (s->state&3); 
      }
    return 0;
    case BM_GETIMAGE:
      if (wParam != IMAGE_BITMAP && wParam != IMAGE_ICON) return 0; // ignore unknown types
      {
        buttonWindowState *s = (buttonWindowState*)hwnd->m_private_data;
        return (LRESULT) s->bitmap;
      }
    return 0;
    case BM_SETIMAGE:
      if (wParam == IMAGE_BITMAP || wParam == IMAGE_ICON)
      {
        buttonWindowState *s = (buttonWindowState*)hwnd->m_private_data;
        LRESULT res = (LRESULT)s->bitmap;
        s->bitmap = (HICON)lParam;
        s->bitmap_mode = wParam;
        InvalidateRect(hwnd,NULL,FALSE);
        return res;
      }
    return 0;
    case BM_SETCHECK:
      if (hwnd)
      {
        buttonWindowState *s = (buttonWindowState*)hwnd->m_private_data;
        int check = (int)wParam;
        INT_PTR op = s->state;
        s->state=(check > 2 || check<0 ? 1 : (check&3)) | (s->state&~3);
        if (s->state == op) break; 
      }
      else
      {
        break;
      }
      // fall through (invalidating)
    case WM_USER+100:
    case WM_CAPTURECHANGED:
    case WM_SETTEXT:
      InvalidateRect(hwnd,NULL,FALSE);
    break;
  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}

static HWND swell_makeButton(HWND owner, int idx, RECT *tr, const char *label, bool vis, int style)
{
  HWND hwnd = new HWND__(owner,idx,tr,label,vis,buttonWindowProc);
  hwnd->m_private_data = (INT_PTR) new buttonWindowState;
  hwnd->m_classname = "Button";
  hwnd->m_style = style|WS_CHILD;
  hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
  return hwnd;
}
#endif

static LRESULT WINAPI groupWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 

          const char *buf = hwnd->m_title.Get();
          int th=SWELL_UI_SCALE(20);
          int tw=0;
          int xp=0;
          if (buf && buf[0]) 
          {
            RECT tr={0,};
            DrawText(ps.hdc,buf,-1,&tr,DT_CALCRECT);
            th=tr.bottom-tr.top;
            tw=tr.right-tr.left;
          }
          if (hwnd->m_style & SS_CENTER)
          {
            xp = r.right/2 - tw/2;
          }
          else if (hwnd->m_style & SS_RIGHT)
          {
            xp = r.right - tw;
          }
          const int sc8 = SWELL_UI_SCALE(8);
          if (xp<sc8)xp=sc8;
          if (xp+tw > r.right-sc8) tw=r.right-sc8-xp;

          HPEN pen = CreatePen(PS_SOLID,0,g_swell_ctheme.group_hilight);
          HPEN pen2 = CreatePen(PS_SOLID,0,g_swell_ctheme.group_shadow);
          HGDIOBJ oldPen=SelectObject(ps.hdc,pen);

          MoveToEx(ps.hdc,xp - (tw?sc8/2:0) + 1,th/2+1,NULL);
          LineTo(ps.hdc,1,th/2+1);
          LineTo(ps.hdc,1,r.bottom-1);
          LineTo(ps.hdc,r.right-1,r.bottom-1);
          LineTo(ps.hdc,r.right-1,th/2+1);
          LineTo(ps.hdc,xp+tw + (tw?sc8/2:0),th/2+1);

          SelectObject(ps.hdc,pen2);

          MoveToEx(ps.hdc,xp - (tw?sc8/2:0),th/2,NULL);
          LineTo(ps.hdc,0,th/2);
          LineTo(ps.hdc,0,r.bottom-2);
          LineTo(ps.hdc,r.right-2,r.bottom-2);
          LineTo(ps.hdc,r.right-2,th/2);
          LineTo(ps.hdc,xp+tw + (tw?4:0),th/2);


          SelectObject(ps.hdc,oldPen);
          DeleteObject(pen);
          DeleteObject(pen2);

          SetTextColor(ps.hdc,g_swell_ctheme.group_text);
          SetBkMode(ps.hdc,TRANSPARENT);
          r.left = xp;
          r.right = xp+tw;
          r.bottom = th;
          if (buf && buf[0]) DrawText(ps.hdc,buf,-1,&r,DT_LEFT|DT_TOP);
          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_SETTEXT:
      InvalidateRect(hwnd,NULL,TRUE);
    break;
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MOUSEMOVE:
      if (GET_Y_LPARAM(lParam) >= SWELL_UI_SCALE(20))
      {
        HWND par = GetParent(hwnd);
        if (par)
        {
          POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
          ClientToScreen(hwnd,&pt);
          ScreenToClient(par,&pt);
          return SendMessage(par,msg,wParam,MAKELPARAM(pt.x,pt.y));
        }
      }
    break;
  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}

static void calcScroll(int wh, int totalw, int scroll_x, int *thumbsz, int *thumbpos)
{
  const double isz = wh / (double) totalw;
  int sz = (int) (wh * isz + 0.5);
  if (sz < g_swell_ctheme.scrollbar_min_thumb_height) sz=g_swell_ctheme.scrollbar_min_thumb_height;

  *thumbpos = (int) (scroll_x * isz + 0.5);
  if (*thumbpos >= wh-sz) *thumbpos = wh-sz;

  *thumbsz = sz;
}

static void drawHorizontalScrollbar(HDC hdc, RECT cr, int vieww, int totalw, int scroll_x)
{
  if (totalw <= vieww) return;

  int thumbsz, thumbpos;
  calcScroll(vieww,totalw,scroll_x,&thumbsz, &thumbpos);

  HBRUSH br =  CreateSolidBrush(g_swell_ctheme.scrollbar_fg);
  HBRUSH br2 =  CreateSolidBrush(g_swell_ctheme.scrollbar_bg);
  RECT fr = { cr.left, cr.bottom - g_swell_ctheme.scrollbar_width, cr.left + thumbpos, cr.bottom };
  if (fr.right>fr.left) FillRect(hdc,&fr,br2);

  fr.left = fr.right;
  fr.right = fr.left + thumbsz;
  if (fr.right>fr.left) FillRect(hdc,&fr,br);

  fr.left = fr.right;
  fr.right = cr.right;
  if (fr.right>fr.left) FillRect(hdc,&fr,br2);

  DeleteObject(br);
  DeleteObject(br2);
}

static void drawVerticalScrollbar(HDC hdc, RECT cr, int totalh, int scroll_y)
{
  if (totalh <= cr.bottom-cr.top) return;

  int thumbsz, thumbpos;
  calcScroll(cr.bottom-cr.top,totalh,scroll_y,&thumbsz, &thumbpos);

  HBRUSH br =  CreateSolidBrush(g_swell_ctheme.scrollbar_fg);
  HBRUSH br2 =  CreateSolidBrush(g_swell_ctheme.scrollbar_bg);
  RECT fr = { cr.right - g_swell_ctheme.scrollbar_width, cr.top, cr.right,cr.top+thumbpos};
  if (fr.bottom>fr.top) FillRect(hdc,&fr,br2);

  fr.top = fr.bottom;
  fr.bottom = fr.top + thumbsz;
  if (fr.bottom>fr.top) FillRect(hdc,&fr,br);

  fr.top = fr.bottom;
  fr.bottom = cr.bottom;
  if (fr.bottom>fr.top) 
  {
    FillRect(hdc,&fr,br2);

    fr.top=fr.bottom-1; //add a little bottom border in case there is a horizontal scrollbar too
    FillRect(hdc,&fr,br2);
  }

  DeleteObject(br);
  DeleteObject(br2);
}

static int editMeasureLineLength(HDC hdc, const char *str, int str_len)
{
  RECT tmp = {0,};
  DrawText(hdc,str,str_len,&tmp,DT_NOPREFIX|DT_SINGLELINE|DT_CALCRECT|DT_RIGHT);
  return tmp.right;
}


int swell_getLineLength(const char *buf, int *post_skip, int wrap_maxwid, HDC hdc)
{
  int lb=0;
  int ps = 0;
  while (buf[lb] && buf[lb] != '\r' && buf[lb] != '\n') lb++;

  if (wrap_maxwid > g_swell_ctheme.scrollbar_width && hdc && lb>0)
  {
    wrap_maxwid -= g_swell_ctheme.scrollbar_width;
    
    // step through a word at a time and find the most that can fit
    int x=0,best_len=0,sumw=0;
    for (;;)
    {
      while (x < lb && buf[x] > 0 && isspace(buf[x])) x++;
      while (x < lb && (buf[x]<0 || !isspace(buf[x]))) x++;
      const int thisw = editMeasureLineLength(hdc,buf+best_len,x-best_len);
      if (thisw+sumw > wrap_maxwid) break;
      sumw+=thisw;
      best_len=x;
      if (x >= lb) break;
    }
    if (best_len == 0)
    {
      // todo: split individual word (ugh)
      if (x>0) lb = x;
    }
    else 
      lb = best_len;
    
    while (buf[ps+lb] == '\t' || buf[ps+lb] == ' ') ps++; // skip any trailing whitespace
  }
  if (buf[ps+lb] == '\r') ps++;
  if (buf[ps+lb] == '\n') ps++;
  *post_skip = ps;
  return lb;
}

#define EDIT_ALLOW_MULTILINE_CACHE(wwrap,hwnd,tlen) \
            ((wwrap)>0 && ((hwnd)->m_style & (ES_READONLY|ES_MULTILINE)) == (ES_READONLY|ES_MULTILINE) && (tlen)>10000)

struct __SWELL_editControlState
{
  __SWELL_editControlState()  : cache_linelen_bytes(8192)
  { 
    cursor_timer=0;  
    cursor_state=0; 
    sel1=sel2=-1; 
    cursor_pos=0;
    scroll_x=scroll_y=0;
    max_height=0;
    max_width=0;
    cache_linelen_strlen = cache_linelen_w = 0;
  }
  ~__SWELL_editControlState()  {}

  int cursor_pos, sel1,sel2; // in character pos (*not* bytepos)
  int cursor_state;
  int cursor_timer;
  int scroll_x, scroll_y;
  int max_height; // only used in multiline
  int max_width; 

  // used for caching line lengths for multiline word-wrapping edit controls
  int cache_linelen_w, cache_linelen_strlen;
  WDL_TypedBuf<int> cache_linelen_bytes;

  bool deleteSelection(WDL_FastString *fs);
  int getSelection(WDL_FastString *fs, const char **ptrOut) const;
  void moveCursor(int cp); // extends selection if shift is held, otherwise clears
  void onMouseDown(int &capmode_state, int last_cursor);
  void onMouseDrag(int &capmode_state, int p);

  void autoScrollToOffset(HWND hwnd, int charpos, bool is_multiline, bool word_wrap);
};





static bool editGetCharPos(HDC hdc, const char *str, int singleline_len, int charpos, int line_h, POINT *pt, int word_wrap,
    __SWELL_editControlState *es, HWND hwnd)
{
  int bytepos = WDL_utf8_charpos_to_bytepos(str,charpos);
  int ypos = 0;
  if (singleline_len >= 0)
  {
    if (bytepos > singleline_len) return false;
    pt->y=0;
    pt->x=editMeasureLineLength(hdc,str,bytepos);
    return true;
  }


  int *use_cache = NULL, use_cache_len = 0;
  int title_len = 0;
  const bool allow_cache = hwnd && es && EDIT_ALLOW_MULTILINE_CACHE(word_wrap,hwnd,title_len = (int)strlen(str));
  if (allow_cache && 
      es->cache_linelen_w == word_wrap && 
      es->cache_linelen_strlen == title_len)
  {
    use_cache = es->cache_linelen_bytes.Get();
    use_cache_len = es->cache_linelen_bytes.GetSize();
  } 

  while (*str)
  {
    int pskip = 0, lb;
    if (!use_cache || use_cache_len < 1)
    {
      lb = swell_getLineLength(str,&pskip,word_wrap,hdc);
    }
    else
    {
      lb = *use_cache++;
      if (WDL_NOT_NORMALLY(lb < 1)) break;
    }
    if (bytepos < lb+pskip)
    { 
      pt->x=editMeasureLineLength(hdc,str,bytepos);
      pt->y=ypos;
      return true;
    }
    str += lb+pskip;
    bytepos -= lb+pskip;
    if (*str || (pskip>0 && str[-1] == '\n')) ypos += line_h;
  }
  pt->x=0;
  pt->y=ypos;
  return true;
}


static int editHitTestLine(HDC hdc, const char *str, int str_len, int xpos, int ypos)
{
  RECT mr={0,};
  DrawText(hdc,str_len == 0 ? " " : str,wdl_max(str_len,1),&mr,DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);

  if (xpos >= mr.right) return str_len;
  if (xpos < 1) return 0;

  // could bsearch, but meh
  int x = 0;
  while (x < str_len)
  {
    memset(&mr,0,sizeof(mr));
    const int clen = wdl_utf8_parsechar(str+x,NULL); 
    DrawText(hdc,str,x+clen,&mr,DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT|DT_RIGHT/*swell-only flag*/);
    if (xpos < mr.right) break;
    x += clen;
  }
  return x;
}

static int editHitTest(HDC hdc, const char *str, int singleline_len, int xpos, int ypos, int word_wrap, 
    __SWELL_editControlState *es, HWND hwnd)
{
  if (singleline_len >= 0) return editHitTestLine(hdc,str,singleline_len,xpos,1);

  const char *buf = str;
  int bytepos = 0;
  RECT tmp={0};
  const int line_h = DrawText(hdc," ",1,&tmp,DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);

  int *use_cache = NULL, use_cache_len = 0;
  int title_len = 0;
  const bool allow_cache = hwnd && es && EDIT_ALLOW_MULTILINE_CACHE(word_wrap,hwnd,title_len = (int)strlen(str));
  if (allow_cache && 
      es->cache_linelen_w == word_wrap && 
      es->cache_linelen_strlen == title_len)
  {
    use_cache = es->cache_linelen_bytes.Get();
    use_cache_len = es->cache_linelen_bytes.GetSize();
  } 

  for (;;)
  {
    int pskip=0;
    int lb;
    
    if (!use_cache || use_cache_len < 1)
    {
      lb = swell_getLineLength(buf,&pskip,word_wrap,hdc);
    }
    else
    {
      lb = *use_cache++;
      if (WDL_NOT_NORMALLY(lb < 1)) return bytepos;
    }

    if (ypos < line_h) return bytepos + editHitTestLine(hdc,buf,lb, xpos,ypos);
    ypos -= line_h;

    if (!buf[0] || !buf[lb]) return bytepos + lb;

    bytepos += lb+pskip;
    buf += lb+pskip;
  }
}


bool __SWELL_editControlState::deleteSelection(WDL_FastString *fs)
{
    if (sel1>=0 && sel2 > sel1)
    {
      int pos1 = WDL_utf8_charpos_to_bytepos(fs->Get(),sel1);
      int pos2 = WDL_utf8_charpos_to_bytepos(fs->Get(),sel2);
      if (pos2 == pos1) return false;

      int cp = WDL_utf8_charpos_to_bytepos(fs->Get(),cursor_pos);
      fs->DeleteSub(pos1,pos2-pos1);
      if (cp >= pos2) cp -= pos2-pos1;
      else if (cp >= pos1) cp=pos1;
      cursor_pos = WDL_utf8_bytepos_to_charpos(fs->Get(),cp);

      sel1=sel2=-1;
      return true;
    }
    return false;
}

int __SWELL_editControlState::getSelection(WDL_FastString *fs, const char **ptrOut) const
{
    if (sel1>=0 && sel2>sel1)
    {
      int pos1 = WDL_utf8_charpos_to_bytepos(fs->Get(),sel1);
      int pos2 = WDL_utf8_charpos_to_bytepos(fs->Get(),sel2);
      if (ptrOut) *ptrOut = fs->Get()+pos1;
      return pos2-pos1;
    }
    return 0;
}
void __SWELL_editControlState::moveCursor(int cp) // extends selection if shift is held, otherwise clears
{
    if (GetAsyncKeyState(VK_SHIFT)&0x8000)
    {
      if (sel1>=0 && sel2>sel1 && (cursor_pos==sel1 || cursor_pos==sel2))
      {
        if (cursor_pos==sel1) sel1=cp;
        else sel2=cp;
        if (sel2<sel1)
        {
          int a = sel1;
          sel1 = sel2;
          sel2 = a;
        }
      }
      else
      {
        sel1 = wdl_min(cursor_pos,cp);
        sel2 = wdl_max(cursor_pos,cp);
      }
    }
    else 
    {
      sel1=sel2=-1;
    }
    cursor_pos = cp;
}

void __SWELL_editControlState::onMouseDown(int &capmode_state, int last_cursor)
{
    capmode_state = 4;

    if (GetAsyncKeyState(VK_SHIFT)&0x8000)
    {
      sel1=last_cursor;
      sel2=cursor_pos;
      if (sel1 > sel2)
      {
        sel1=sel2;
        sel2=last_cursor;
        capmode_state = 3;
      }
    }
    else
    {
      sel1=sel2=cursor_pos;
    }
}

void __SWELL_editControlState::onMouseDrag(int &capmode_state, int p)
{
    if (sel1 == sel2)
    {
      if (p < sel1)
      {
        sel1 = p;
        capmode_state = 3; 
      }
      else if (p > sel2)
      {
        sel2 = p;
        capmode_state = 4;
      }
    }
    else if (capmode_state == 3)
    {
      if (p < sel2) sel1 = p;
      else if (p > sel2)
      {
        sel1 = sel2;
        sel2 = p;
        capmode_state=4;
      }
    }
    else
    {
      if (p > sel1) sel2 = p;
      else if (p < sel1)
      {
        sel2 = sel1;
        sel1 = p;
        capmode_state=3;
      }
    }
}

void __SWELL_editControlState::autoScrollToOffset(HWND hwnd, int charpos, bool is_multiline, bool word_wrap)
{
    if (!hwnd) return;
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;
    RECT tmp={0,};
    const int line_h = DrawText(hdc," ",1,&tmp,DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);

    GetClientRect(hwnd,&tmp);
    if (is_multiline) 
    {
      tmp.right -= g_swell_ctheme.scrollbar_width;
      if (!word_wrap) tmp.bottom -= g_swell_ctheme.scrollbar_width;
    }

    int wwrap = word_wrap?tmp.right:0;
    POINT pt={0,};
    if (editGetCharPos(hdc, hwnd->m_title.Get(), 
         is_multiline? -1:hwnd->m_title.GetLength(), charpos, line_h, &pt,
         wwrap, is_multiline?this:NULL,hwnd))
    {
      if (!word_wrap)
      {
        const int padsz = wdl_max(tmp.right - line_h,line_h);
        if (pt.x > scroll_x+padsz) scroll_x = pt.x - padsz;
        if (pt.x < scroll_x) scroll_x=pt.x;
      }
      if (is_multiline)
      {
        if (pt.y+line_h > scroll_y+tmp.bottom) scroll_y = pt.y - tmp.bottom + line_h;
        if (pt.y < scroll_y) scroll_y=pt.y;
      }
      if (scroll_y < 0) scroll_y=0;
      if (scroll_x < 0) scroll_x=0;
    }
    ReleaseDC(hwnd,hdc);
}

static bool is_word_char(char c)
{
  return c<0/*all utf-8 chars are word chars*/ || isalnum(c) || c == '_';
}

static int scanWord(const char *buf, int bytepos, int dir)
{
  if (dir < 0 && !bytepos) return 0;
  if (dir > 0 && !buf[bytepos]) return bytepos;

  if (!buf[bytepos] && bytepos > 0) bytepos--;

  const unsigned char *bytebuf = (const unsigned char*) buf;
  if (dir < 0)
  {
    const bool cc = is_word_char(buf[--bytepos]);
    while (bytepos > 0 && is_word_char(buf[bytepos-1]) == cc) bytepos--;
    while (bytepos > 0 && bytebuf[bytepos] >= 0x80 && bytebuf[bytepos] < 0xC0) bytepos--; // skip any UTF-8 continuation bytes
  }
  else
  {
    const bool cc = is_word_char(buf[bytepos]);
    while (buf[bytepos+1] && is_word_char(buf[bytepos+1]) == cc) bytepos++;
    bytepos++;
    while (bytebuf[bytepos] >= 0x80 && bytebuf[bytepos] < 0xC0) bytepos++; // skip any UTF-8 continuation bytes
  }

  return bytepos;
}

static LRESULT OnEditKeyDown(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, 
    bool wantReturn, bool isMultiLine, __SWELL_editControlState *es, bool isEditCtl)
{
  if (lParam & (FCONTROL|FALT|FLWIN))
  {
    if (lParam == (FVIRTKEY | FCONTROL))
    {
      if (wParam == 'C' || wParam == 'X')
      {
        const char *s = NULL;
        int slen = es->getSelection(&hwnd->m_title,&s);
        if (slen > 0 && s)
        {
          if (!isEditCtl || !(hwnd->m_style & ES_PASSWORD))
          {
            OpenClipboard(hwnd);
            HANDLE h = GlobalAlloc(0,slen+1);
            if (h)
            {
              memcpy((char*)h,s,slen);
              ((char*)h)[slen] = 0;
              SetClipboardData(CF_TEXT,h);
            }
            CloseClipboard();
            if (h && wParam == 'X' && (!isEditCtl || !(hwnd->m_style & ES_READONLY)))
            {
              es->deleteSelection(&hwnd->m_title);
              return 7;
            }
          }
        }
      }
      else if (wParam == 'V' && !(hwnd->m_style & ES_READONLY))
      {
        OpenClipboard(hwnd);
        HANDLE h = GetClipboardData(CF_TEXT);
        const char *s = NULL;
        if (h)
        {
          s = (const char*)GlobalLock(h);
          if (s)
          {
            es->deleteSelection(&hwnd->m_title);
            int bytepos = WDL_utf8_charpos_to_bytepos(hwnd->m_title.Get(),es->cursor_pos);
            hwnd->m_title.Insert(s,bytepos);
            if (!(hwnd->m_style&ES_MULTILINE))
            {
              char *p = (char *)hwnd->m_title.Get() + bytepos;
              char *ep = p + strlen(s);
              while (*p && p < ep) { if (*p == '\r' || *p == '\n') *p=' '; p++; }
            }
            es->cursor_pos += WDL_utf8_get_charlen(s);
            GlobalUnlock(h);
          }
        }
        CloseClipboard();
        if (s) return 7;
      }
      else if (wParam == 'A')
      {
        es->sel1 = 0;
        es->cursor_pos = es->sel2 = WDL_utf8_get_charlen(hwnd->m_title.Get());
        return 2;
      }
    }
    if (lParam & (FALT | FLWIN)) return 0;
  }
  else
  {
    if ((lParam & FVIRTKEY) && wParam == VK_TAB) return 0; // pass through to window

    const bool is_numpad = wParam >= VK_NUMPAD0 && wParam <= VK_DIVIDE;
    if (wParam >= 32 && (!(lParam & FVIRTKEY) || swell_is_virtkey_char((int)wParam) || is_numpad))
    {
      if (lParam & FVIRTKEY)
      {
        if (wParam >= 'A' && wParam <= 'Z')
        {
          if ((lParam&FSHIFT) ^ (swell_is_likely_capslock?0:FSHIFT)) wParam += 'a' - 'A';
        }
        else if (is_numpad)
        {
          if (wParam <= VK_NUMPAD9) wParam += '0' - VK_NUMPAD0;
          else wParam += '*' - VK_MULTIPLY;
        }
      }

      if (hwnd->m_style & ES_READONLY) return 1;

      char b[8];
      WDL_MakeUTFChar(b,wParam,sizeof(b));
      es->deleteSelection(&hwnd->m_title);
      int bytepos = WDL_utf8_charpos_to_bytepos(hwnd->m_title.Get(),es->cursor_pos);
      hwnd->m_title.Insert(b,bytepos);
      es->cursor_pos++;
      return 7;
    }
  }

  if (es && (lParam & FVIRTKEY)) switch (wParam)
  {
    case VK_NEXT:
    case VK_PRIOR:
      if (!isMultiLine) break;
    case VK_UP:
    case VK_DOWN:
      if (isMultiLine)
      {
        HDC hdc=GetDC(hwnd);
        if (hdc)
        {
          RECT tmp={0};
          const int line_h = DrawText(hdc," ",1,&tmp,DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);
          POINT pt;
          GetClientRect(hwnd,&tmp);
          const int wwrap = (hwnd->m_style & ES_AUTOHSCROLL) ? 0 : tmp.right - g_swell_ctheme.scrollbar_width;
          if (editGetCharPos(hdc, hwnd->m_title.Get(), -1, es->cursor_pos, line_h, &pt, wwrap, isEditCtl ? es : NULL, hwnd))
          {
            if (wParam == VK_UP) pt.y -= line_h/2;
            else if (wParam == VK_NEXT) 
            {
              int ey = es->scroll_y + tmp.bottom - (wwrap?0:g_swell_ctheme.scrollbar_width) - line_h;
              if (pt.y < ey-line_h) pt.y = ey;
              else pt.y = ey + tmp.bottom - line_h - (wwrap?0:g_swell_ctheme.scrollbar_width);
            }
            else if (wParam == VK_PRIOR) 
            {
              if (pt.y > es->scroll_y) pt.y = es->scroll_y;
              else pt.y = es->scroll_y - (tmp.bottom-line_h/2 - g_swell_ctheme.scrollbar_width);
            }
            else pt.y += line_h + line_h/2;
            int nextpos = editHitTest(hdc, hwnd->m_title.Get(), -1,pt.x,pt.y,wwrap,es,hwnd);
            es->moveCursor(WDL_utf8_bytepos_to_charpos(hwnd->m_title.Get(),nextpos));
          }
          ReleaseDC(hwnd,hdc);
        }
        return 3;
      }
      // fall through
    case VK_HOME:
    case VK_END:
      if (isMultiLine)
      {
        const char *buf = hwnd->m_title.Get();
        int lpos = 0, wwrap=0;
        HDC hdc=NULL;
        if (!(hwnd->m_style & ES_AUTOHSCROLL))
        {
          hdc = GetDC(hwnd);
          RECT r;
          GetClientRect(hwnd,&r);
          if (hdc) wwrap = r.right - g_swell_ctheme.scrollbar_width;
        }
        const int cbytepos = WDL_utf8_charpos_to_bytepos(buf,es->cursor_pos);
        for (;;) 
        {
          int ps=0, lb = swell_getLineLength(buf+lpos, &ps,wwrap,hdc);
          if (!buf[lpos] || (cbytepos >= lpos && cbytepos < lpos+lb+ps + (buf[lpos+lb+ps]?0:1)))
          {
            if (wParam == VK_HOME) es->moveCursor(WDL_utf8_bytepos_to_charpos(buf,lpos));
            else es->moveCursor(WDL_utf8_bytepos_to_charpos(buf,lpos+lb));
            return 3;
          }
          lpos += lb+ps;
        }
        if (hdc) ReleaseDC(hwnd,hdc);
      }

      if (wParam == VK_UP || wParam == VK_HOME) es->moveCursor(0); 
      else es->moveCursor(WDL_utf8_get_charlen(hwnd->m_title.Get()));
    return 3;
    case VK_LEFT:
      { 
        int cp = es->cursor_pos;
        if (cp > 0) 
        {
          const char *buf=hwnd->m_title.Get();
          if (lParam & FCONTROL)
          {
            cp = WDL_utf8_bytepos_to_charpos(buf,
                scanWord(buf,WDL_utf8_charpos_to_bytepos(buf,cp),-1));
          }
          else
          {
            const int p = WDL_utf8_charpos_to_bytepos(buf,--cp);
            if (cp > 0 && p > 0 && buf[p] == '\n' && buf[p-1] == '\r') cp--;
          }
        }
        es->moveCursor(cp);
      }
    return 3; 
    case VK_RIGHT:
      { 
        int cp = es->cursor_pos;
        if (cp < WDL_utf8_get_charlen(hwnd->m_title.Get())) 
        {
          const char *buf=hwnd->m_title.Get();
          const int p = WDL_utf8_charpos_to_bytepos(buf,cp);

          if (lParam & FCONTROL)
            cp = WDL_utf8_bytepos_to_charpos(buf,scanWord(buf,p,1));
          else if (buf[p] == '\r' && buf[p+1] == '\n') 
            cp+=2;
          else
            cp++;
        }
        es->moveCursor(cp);
      }
    return 3;
    case VK_DELETE:
      if (hwnd->m_style & ES_READONLY) return 1;
      if (hwnd->m_title.GetLength())
      {
        if (es->deleteSelection(&hwnd->m_title)) return 7;

        const int bytepos = WDL_utf8_charpos_to_bytepos(hwnd->m_title.Get(),es->cursor_pos);
        if (bytepos < hwnd->m_title.GetLength())
        {
          const char *rd = hwnd->m_title.Get()+bytepos;
          hwnd->m_title.DeleteSub(bytepos, rd[0] == '\r' && rd[1] == '\n' ? 2 : wdl_utf8_parsechar(rd,NULL));
          return 7; 
        }
      }
    return 1;

    case VK_BACK:
      if (hwnd->m_style & ES_READONLY) return 1;
      if (hwnd->m_title.GetLength())
      {
        if (es->deleteSelection(&hwnd->m_title)) return 7;
        if (es->cursor_pos > 0)
        {
          es->cursor_pos--;
          const char *buf = hwnd->m_title.Get();
          int bytepos = WDL_utf8_charpos_to_bytepos(buf,es->cursor_pos);
          if (bytepos > 0 && buf[bytepos] == '\n' && buf[bytepos-1] == '\r') 
          {
            hwnd->m_title.DeleteSub(bytepos-1, 2);
            es->cursor_pos--;
          }
          else hwnd->m_title.DeleteSub(bytepos, wdl_utf8_parsechar(hwnd->m_title.Get()+bytepos,NULL));
          return 7; 
        }
      }
    return 1;
    case VK_RETURN:
      if (wantReturn)
      {
        if (hwnd->m_style & ES_READONLY) return 1;
        if (es->deleteSelection(&hwnd->m_title)) return 7;
        int bytepos = WDL_utf8_charpos_to_bytepos(hwnd->m_title.Get(),es->cursor_pos);
        hwnd->m_title.Insert("\r\n",bytepos);
        es->cursor_pos+=2; // skip \r and \n
        return 7;
      }
    return 0;
  }
  return 0;
}

static int editControlPaintLine(HDC hdc, const char *str, int str_len, int cursor_pos, int sel1, int sel2, const RECT *r, int dtflags)
{
  // cursor_pos, sel1, sel2 are all byte positions
  int rv = 0;
  if (str_len>0)
  {
    RECT outr = *r;
    if (sel2 < str_len || sel1 > 0)
    {
      RECT tmp={0,};
      DrawText(hdc,str,str_len,&tmp,DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);
      rv = tmp.right;
      DrawText(hdc,str,str_len,&outr,dtflags|DT_SINGLELINE|DT_NOPREFIX);
    }

    const int offs = wdl_max(sel1,0);
    const int endptr = wdl_min(sel2,str_len);
    if (endptr > offs)
    {
      SetBkMode(hdc,OPAQUE);
      SetBkColor(hdc,g_swell_ctheme.edit_bg_sel);
      const int oldc = GetTextColor(hdc);
      SetTextColor(hdc,g_swell_ctheme.edit_text_sel);

      RECT tmp={0,};
      DrawText(hdc,str,offs,&tmp,DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);
      outr.left += tmp.right;
      DrawText(hdc,str+offs,endptr-offs,&outr,dtflags|DT_SINGLELINE|DT_NOPREFIX);

      SetBkMode(hdc,TRANSPARENT);
      SetTextColor(hdc,oldc);
    }
  }

  if (cursor_pos >= 0 && cursor_pos <= str_len)
  {
    RECT mr={0,};
    if (cursor_pos>0) DrawText(hdc,str,cursor_pos,&mr,DT_CALCRECT|DT_NOPREFIX|DT_SINGLELINE);

    int oc = GetTextColor(hdc);
    SetTextColor(hdc,g_swell_ctheme.edit_cursor);
    mr.left = r->left + mr.right - 1;
    mr.right = mr.left+1;
    mr.top = r->top;
    mr.bottom = r->bottom;
    DrawText(hdc,"|",1,&mr,dtflags|DT_SINGLELINE|DT_NOPREFIX|DT_NOCLIP);
    SetTextColor(hdc,oc);
  }
  return rv;
}

static void passwordify(WDL_FastString **s)
{
  const int l = WDL_utf8_get_charlen((*s)->Get());
  if (l>0)
  {
    static WDL_FastString tmp;
    tmp.SetLen(l,false,'*');
    *s = &tmp;
  }
}

static LRESULT WINAPI editWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  __SWELL_editControlState *es = (__SWELL_editControlState*)hwnd->m_private_data;
  static int s_capmode_state /* 1=vscroll, 2=hscroll, 3=move sel1, 4=move sel2*/, s_capmode_data1;
  switch (msg)
  {
    case WM_NCDESTROY:
      delete es;
      hwnd->m_private_data=0;
    break;
    case WM_CONTEXTMENU:
      {
        HMENU menu=CreatePopupMenu();
        MENUITEMINFO mi={sizeof(mi),MIIM_ID|MIIM_TYPE,MFT_STRING, 0,
              (UINT) 100, NULL,NULL,NULL,0,(char*)"Copy"};

        if (!(hwnd->m_style & ES_PASSWORD))
          InsertMenuItem(menu,0,TRUE,&mi);
        mi.wID++;
        mi.dwTypeData = (char*)"Paste";
        InsertMenuItem(menu,0,TRUE,&mi);
        mi.wID++;
        mi.dwTypeData = (char*)"Select all";
        InsertMenuItem(menu,0,TRUE,&mi);
        POINT p;
        GetCursorPos(&p);

        const int a = TrackPopupMenu(menu,TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN,p.x,p.y,0,hwnd,0);
        DestroyMenu(menu);
        if (a==100) OnEditKeyDown(hwnd,WM_KEYDOWN,'C',FVIRTKEY|FCONTROL,false,false,es,true);
        else if (a==101) 
        {
          OnEditKeyDown(hwnd,WM_KEYDOWN,'V',FVIRTKEY|FCONTROL,false,false,es,true);
          SendMessage(GetParent(hwnd),WM_COMMAND,(EN_CHANGE<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
        }
        else if (a==102) SendMessage(hwnd,EM_SETSEL,0,-1);

        if (a) InvalidateRect(hwnd,NULL,FALSE);

      }
    return 1;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
      es->cursor_state=1; // next invalidate draws blinky

      // todo: if selection active and not focused, do not mouse hit test
      if (msg == WM_LBUTTONDOWN) 
      {
        WDL_FastString *title = &hwnd->m_title;
        if (hwnd->m_style & ES_PASSWORD) passwordify(&title);
        const bool multiline = (hwnd->m_style & ES_MULTILINE) != 0;
        RECT r;
        GetClientRect(hwnd,&r);
        if (multiline)
        {
          RECT br = r;
          br.right -= g_swell_ctheme.scrollbar_width;

          if (es->max_width > br.right && (hwnd->m_style & ES_AUTOHSCROLL))
          {
            if (GET_Y_LPARAM(lParam) >= br.bottom - g_swell_ctheme.scrollbar_width)
            {
              int xp = GET_X_LPARAM(lParam), xpos = xp;

              int thumbsz, thumbpos;
              calcScroll(br.right, es->max_width,es->scroll_x,&thumbsz,&thumbpos);

              if (xpos < thumbpos) xp = thumbpos;
              else if (xpos > thumbpos+thumbsz) xp = thumbpos + thumbsz;

              s_capmode_state = 2;
              s_capmode_data1 = xp;
              SetCapture(hwnd);
              if (xpos < thumbpos || xpos > thumbpos+thumbsz) goto forceMouseMove;
              return 0;
            }
            br.bottom -= g_swell_ctheme.scrollbar_width;
          }
          if (GET_X_LPARAM(lParam)>=br.right && es->max_height > br.bottom)
          {
            int yp = GET_Y_LPARAM(lParam), ypos = yp;

            int thumbsz, thumbpos;
            calcScroll(br.bottom, es->max_height, es->scroll_y,&thumbsz,&thumbpos);

            if (ypos < thumbpos) yp = thumbpos;
            else if (ypos > thumbpos+thumbsz) yp = thumbpos + thumbsz;

            s_capmode_state = 1;
            s_capmode_data1 = yp;
            SetCapture(hwnd);
            if (ypos < thumbpos || ypos > thumbpos+thumbsz) goto forceMouseMove;
            return 0;
          }
        }
       

        int xo=2;
        int yo = multiline ? 2 : 0;
        HDC hdc=GetDC(hwnd);
        const int last_cursor = es->cursor_pos;
        const int wwrap = (hwnd->m_style & (ES_MULTILINE|ES_AUTOHSCROLL)) == ES_MULTILINE ? 
          r.right - g_swell_ctheme.scrollbar_width : 0;
        es->cursor_pos = WDL_utf8_bytepos_to_charpos(title->Get(),
            editHitTest(hdc,title->Get(),
                        multiline?-1:title->GetLength(),
                         GET_X_LPARAM(lParam)-xo + es->scroll_x,
                         GET_Y_LPARAM(lParam)-yo + es->scroll_y, 
                         wwrap,es,hwnd)
              );

        if (msg == WM_LBUTTONDOWN) 
          es->onMouseDown(s_capmode_state,last_cursor);

        ReleaseDC(hwnd,hdc);

      }
      SetFocusInternal(hwnd);
      if (msg == WM_LBUTTONDOWN) SetCapture(hwnd);

      InvalidateRect(hwnd,NULL,FALSE);
    return 0;
    case WM_MOUSEWHEEL:
      if (es && (hwnd->m_style & ES_MULTILINE))
      {
        if ((GetAsyncKeyState(VK_CONTROL)&0x8000) || (GetAsyncKeyState(VK_MENU)&0x8000)) break; // pass modified mousewheel to parent

        RECT r;
        GetClientRect(hwnd,&r);
        r.right -= g_swell_ctheme.scrollbar_width;
        if (es->max_width > r.right && (hwnd->m_style & ES_AUTOHSCROLL))
        {
          r.bottom -= g_swell_ctheme.scrollbar_width;
        }
        const int viewsz = r.bottom;
        const int totalsz=es->max_height + g_swell_ctheme.scrollbar_width;

        const int amt = ((short)HIWORD(wParam))/-2;

        const int oldscroll = es->scroll_y;
        es->scroll_y += amt;
        if (es->scroll_y + viewsz > totalsz) es->scroll_y = totalsz-viewsz;
        if (es->scroll_y < 0) es->scroll_y=0;
        if (es->scroll_y != oldscroll)
        {
          InvalidateRect(hwnd,NULL,FALSE);
        }

        return 1;
      }
    break;
    case WM_MOUSEMOVE:
forceMouseMove:
      if (es && GetCapture()==hwnd)
      {
        RECT r;
        GetClientRect(hwnd,&r);
        const bool multiline = (hwnd->m_style & ES_MULTILINE) != 0;
        if (multiline)
        {
          r.right -= g_swell_ctheme.scrollbar_width;
          if (es->max_width > r.right && (hwnd->m_style & ES_AUTOHSCROLL))
          {
            r.bottom -= g_swell_ctheme.scrollbar_width;
          }
        }
        if (s_capmode_state == 1)
        {
          int yv = s_capmode_data1;
          int amt = GET_Y_LPARAM(lParam) - yv;

          if (amt)
          {
            const int viewsz = r.bottom;
            const int totalsz=es->max_height + g_swell_ctheme.scrollbar_width;
            amt = (int)floor(amt * (double)totalsz / (double)viewsz + 0.5);
              
            const int oldscroll = es->scroll_y;
            es->scroll_y += amt;
            if (es->scroll_y + viewsz > totalsz) es->scroll_y = totalsz-viewsz;
            if (es->scroll_y < 0) es->scroll_y=0;
            if (es->scroll_y != oldscroll)
            {
              s_capmode_data1 = GET_Y_LPARAM(lParam);
              InvalidateRect(hwnd,NULL,FALSE);
            }
          }
        }
        else if (s_capmode_state == 2)
        {
          int xv = s_capmode_data1;
          int amt = GET_X_LPARAM(lParam) - xv;

          if (amt)
          {
            const int viewsz = r.right;
            const int totalsz=es->max_width + g_swell_ctheme.scrollbar_width;
            amt = (int)floor(amt * (double)totalsz / (double)viewsz + 0.5);
              
            const int oldscroll = es->scroll_x;
            es->scroll_x += amt;
            if (es->scroll_x + viewsz > totalsz) es->scroll_x = totalsz-viewsz;
            if (es->scroll_x < 0) es->scroll_x=0;
            if (es->scroll_x != oldscroll)
            {
              s_capmode_data1 = GET_X_LPARAM(lParam);
              InvalidateRect(hwnd,NULL,FALSE);
            }
          }
        }
        else if (s_capmode_state == 3 || s_capmode_state == 4)
        {
          int wwrap=0;
          if ((hwnd->m_style & (ES_MULTILINE|ES_AUTOHSCROLL)) == ES_MULTILINE)
          {
            wwrap = r.right; // already has scrollbar size removed
          }
          int xo=2;
          int yo = multiline ? 2 : 0;
          HDC hdc=GetDC(hwnd);
          WDL_FastString *title = &hwnd->m_title;
          if (hwnd->m_style & ES_PASSWORD) passwordify(&title);
          int p = WDL_utf8_bytepos_to_charpos(title->Get(),
            editHitTest(hdc,title->Get(),
                        multiline?-1:title->GetLength(),
                         GET_X_LPARAM(lParam)-xo + es->scroll_x,
                         GET_Y_LPARAM(lParam)-yo + es->scroll_y, wwrap,es,hwnd)
              );
          ReleaseDC(hwnd,hdc);

          es->onMouseDrag(s_capmode_state,p);

          es->autoScrollToOffset(hwnd,p,
               (hwnd->m_style & ES_MULTILINE) != 0,
               (hwnd->m_style & (ES_MULTILINE|ES_AUTOHSCROLL)) == ES_MULTILINE);


          InvalidateRect(hwnd,NULL,FALSE);
        }
      }
    return 0;
    case WM_LBUTTONUP:
      ReleaseCapture();
    return 0;
    case WM_TIMER:
      if (es && wParam == 100)
      {
        if (++es->cursor_state >= 8) es->cursor_state=0;
        if (GetFocusIncludeMenus()!=hwnd || es->cursor_state<2) InvalidateRect(hwnd,NULL,FALSE);
      }
    return 0;
    case WM_LBUTTONDBLCLK:
      if (es)
      {
        // technically this should select the word rather than all
        es->sel1 = 0;
        es->cursor_pos = es->sel2 = WDL_utf8_get_charlen(hwnd->m_title.Get());
        InvalidateRect(hwnd,NULL,FALSE);
      }
    return 0;
    case WM_KEYDOWN:
      {
        const int osel1 = es && es->sel1 >= 0 && es->sel2 > es->sel1 ? es->sel1 : -1;

        int f = OnEditKeyDown(hwnd,msg,wParam,lParam, 
            (hwnd->m_style&ES_WANTRETURN) && (hwnd->m_style&ES_MULTILINE),
            !!(hwnd->m_style&ES_MULTILINE),
            es,true);
        if (f)
        {
          if (f&4) 
          {
            es->cache_linelen_w=0;
            SendMessage(GetParent(hwnd),WM_COMMAND,(EN_CHANGE<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
          }
          if (f&2) 
          {
            if ((hwnd->m_style & (ES_MULTILINE|ES_AUTOHSCROLL)) == ES_AUTOHSCROLL &&
                osel1 >= 0 && es->cursor_pos > osel1 && es->sel1 < 0)
              es->autoScrollToOffset(hwnd,osel1,false,false);

            es->autoScrollToOffset(hwnd,es->cursor_pos,
               (hwnd->m_style & ES_MULTILINE) != 0,
               (hwnd->m_style & (ES_MULTILINE|ES_AUTOHSCROLL)) == ES_MULTILINE
            );
            InvalidateRect(hwnd,NULL,FALSE);
          }
          return 0;
        }
      }
    break;
    case WM_KEYUP:
    return 0;
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;

        const bool focused = GetFocusIncludeMenus()==hwnd;
        if (es)
        {
          if (focused)
          {
            if (!es->cursor_timer) { SetTimer(hwnd,100,100,NULL); es->cursor_timer=1; }
          }
          else
          {
            if (es->cursor_timer) { KillTimer(hwnd,100); es->cursor_timer=0; }
          }
        }

        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 
          RECT orig_r = r;
          WDL_FastString *title = &hwnd->m_title;
          if (hwnd->m_style & ES_PASSWORD) passwordify(&title);

          Draw3DBox(ps.hdc,&r,
            hwnd->m_enabled ? 
              //(hwnd->m_style & ES_READONLY) ? g_swell_ctheme._3dface :
                g_swell_ctheme.edit_bg :
                g_swell_ctheme.edit_bg_disabled,
            g_swell_ctheme.edit_shadow,
            g_swell_ctheme.edit_hilight);

          SetTextColor(ps.hdc,
            hwnd->m_enabled ? 
              //(hwnd->m_style & ES_READONLY) ? g_swell_ctheme.label_text :
                   g_swell_ctheme.edit_text :
                      g_swell_ctheme.edit_text_disabled
          );
          SetBkMode(ps.hdc,TRANSPARENT);
          r.left+=2 - es->scroll_x; r.right-=2;

          const bool do_cursor = es->cursor_state!=0;
          const int cursor_pos = focused ?  WDL_utf8_charpos_to_bytepos(title->Get(),es->cursor_pos) : -1;
          const int sel1 = es->sel1>=0 && focused ? WDL_utf8_charpos_to_bytepos(title->Get(),es->sel1) : -1;
          const int sel2 = es->sel2>=0 && focused ? WDL_utf8_charpos_to_bytepos(title->Get(),es->sel2) : -1;

          const bool multiline = (hwnd->m_style & ES_MULTILINE) != 0;

          if (multiline)
          {
            r.top+=2 - es->scroll_y;
            const char *buf = title->Get(), *buf_end = buf + title->GetLength();
            int bytepos = 0;
            RECT tmp={0,};
            const int line_h = DrawText(ps.hdc," ",1,&tmp,DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);
            const int wwrap = (hwnd->m_style & ES_AUTOHSCROLL) ? 0 : orig_r.right - g_swell_ctheme.scrollbar_width;

            int *use_cache = NULL, use_cache_len = 0;
            const bool allow_cache = EDIT_ALLOW_MULTILINE_CACHE(wwrap,hwnd,title->GetLength());
            if (allow_cache && 
                es->cache_linelen_w == wwrap && 
                es->cache_linelen_strlen == title->GetLength())
            {
              use_cache = es->cache_linelen_bytes.Get();
              use_cache_len = es->cache_linelen_bytes.GetSize();
            }
            else
            {
              es->cache_linelen_w=allow_cache?wwrap:0;
              es->cache_linelen_strlen=allow_cache?title->GetLength():0;
              es->cache_linelen_bytes.Resize(0,false);
            }

            for (;;)
            {
              int pskip=0, lb;

              const bool vis = r.top >= -line_h && r.top < orig_r.bottom;
              
              if (vis || !use_cache || use_cache_len < 1)
              {
                lb = swell_getLineLength(buf,&pskip,wwrap,ps.hdc);
                if (!use_cache && allow_cache)
                {
                  int s = lb+pskip;
                  es->cache_linelen_bytes.Add(&s,1);
                }
              }
              else
              {
                lb = *use_cache;
                if (WDL_NOT_NORMALLY(lb < 1)) break; 
              }

              if (use_cache)
              {
                use_cache++;
                use_cache_len--;
              }

              if (!*buf && cursor_pos != bytepos) break;

              if (WDL_NOT_NORMALLY(buf+lb+pskip > buf_end)) break;

              if (vis)
              {
                int wid = editControlPaintLine(ps.hdc,buf,lb,
                   (do_cursor && cursor_pos >= bytepos && cursor_pos <= bytepos + lb) ? cursor_pos - bytepos : -1, 
                   sel1 >= 0 ? (sel1 - bytepos) : -1,
                   sel2 >= 0 ? (sel2 - bytepos) : -1, 
                   &r, DT_TOP);
                if (wid > es->max_width) es->max_width = wid;
              }

              r.top += line_h;

              if (!*buf || !buf[lb]) break;

              bytepos += lb+pskip;
              buf += lb+pskip;
            }
            r.top += es->scroll_y;
            es->max_height = r.top;
            if (es->max_width > r.right && (hwnd->m_style & ES_AUTOHSCROLL))
            {
              drawHorizontalScrollbar(ps.hdc,orig_r,
                  orig_r.right-orig_r.left - g_swell_ctheme.scrollbar_width,
                  es->max_width,es->scroll_x);
              orig_r.bottom -= g_swell_ctheme.scrollbar_width;
              r.bottom -= g_swell_ctheme.scrollbar_width;
            }
            if (r.top > r.bottom)
            {  
              drawVerticalScrollbar(ps.hdc,orig_r,es->max_height,es->scroll_y);
            }
          }
          else
          {
            es->max_width = editControlPaintLine(ps.hdc, title->Get(), title->GetLength(), cursor_pos, sel1, sel2, &r, DT_VCENTER);
          }

          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_SETTEXT:
      if (es) 
      {
        es->cursor_pos = WDL_utf8_get_charlen(hwnd->m_title.Get());
        es->sel1=es->sel2=-1;
        es->cache_linelen_w=es->cache_linelen_strlen=0;
        if ((hwnd->m_style & (ES_MULTILINE|ES_AUTOHSCROLL))==ES_AUTOHSCROLL &&
            GetFocus() != hwnd)
          es->autoScrollToOffset(hwnd,0,false,false);
      }
      InvalidateRect(hwnd,NULL,FALSE);
      if (hwnd->m_id && hwnd->m_parent)
        SendMessage(hwnd->m_parent,WM_COMMAND,(EN_CHANGE<<16)|hwnd->m_id,(LPARAM)hwnd);
    break;
    case EM_SETSEL:
      if (es) 
      {
        es->sel1 = (int)wParam;
        es->sel2 = (int)lParam;
        if (!es->sel1 && es->sel2 == -1) es->sel2 = WDL_utf8_get_charlen(hwnd->m_title.Get());
        InvalidateRect(hwnd,NULL,FALSE);
        if (es->sel2>=0)
          es->autoScrollToOffset(hwnd,es->sel2,
               (hwnd->m_style & ES_MULTILINE) != 0,
               (hwnd->m_style & (ES_MULTILINE|ES_AUTOHSCROLL)) == ES_MULTILINE);

      }
    return 0;
    case EM_SCROLL:
      if (es)
      {
        if (wParam == SB_TOP)
        {
          es->scroll_x=es->scroll_y=0;
          InvalidateRect(hwnd,NULL,FALSE);
        } 
        else if (wParam == SB_BOTTOM)
        {
          es->autoScrollToOffset(hwnd,hwnd->m_title.GetLength(),
               (hwnd->m_style & ES_MULTILINE) != 0,
               (hwnd->m_style & (ES_MULTILINE|ES_AUTOHSCROLL)) == ES_MULTILINE);
          InvalidateRect(hwnd,NULL,FALSE);
        }
      }
    return 0;
  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}

static LRESULT WINAPI progressWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case PBM_DELTAPOS:
      if (hwnd->m_private_data) *(int *)hwnd->m_private_data += (int) wParam; // todo: unsigned-ness conversion? unclear
      InvalidateRect(hwnd,NULL,FALSE);
    break;
    case PBM_SETPOS:
      if (hwnd->m_private_data) *(int *)hwnd->m_private_data = (int) wParam;
      InvalidateRect(hwnd,NULL,FALSE);
    break;
    case PBM_SETRANGE:
      if (hwnd->m_private_data) ((int *)hwnd->m_private_data)[1] = (int) lParam;
      InvalidateRect(hwnd,NULL,FALSE);
    break;
    case WM_NCDESTROY:
      free((int *)hwnd->m_private_data);
      hwnd->m_private_data=0;
    break;
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 

          paintDialogBackground(hwnd,&r,ps.hdc);

          if (hwnd->m_private_data)
          {
            int pos = *(int *)hwnd->m_private_data;
            const int range = ((int *)hwnd->m_private_data)[1];
            const int low = LOWORD(range), high=HIWORD(range);
            if (pos > low && high > low)
            {
              if (pos > high) pos=high;
              int dx = ((pos-low)*r.right)/(high-low);
              r.right = dx;
              HBRUSH br = CreateSolidBrush(g_swell_ctheme.progress);
              FillRect(ps.hdc,&r,br);
              DeleteObject(br);
            }
          }

          EndPaint(hwnd,&ps);
        }
      }
    break;
  }

  return DefWindowProc(hwnd,msg,wParam,lParam);
}

static LRESULT WINAPI trackbarWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  const int track_h = 10;
  static int s_cap_offs;
  switch (msg)
  {
    case WM_CREATE:
      {
        int *p = (int *)hwnd->m_private_data;
        if (p)
        {
          p[1] = (1000<<16);
          p[2] = -1;
        }
      }
    break;
    case TBM_SETPOS:
      if (hwnd->m_private_data) *(int *)hwnd->m_private_data = (int) lParam;
      if (wParam) InvalidateRect(hwnd,NULL,FALSE);
    break;
    case TBM_GETPOS:
      if (hwnd->m_private_data) return *(int *)hwnd->m_private_data;
    return 0;
    case TBM_SETRANGE:
      if (hwnd->m_private_data) ((int *)hwnd->m_private_data)[1] = (int) lParam;
      if (wParam) InvalidateRect(hwnd,NULL,FALSE);
    break;
    case TBM_SETTIC:
      if (hwnd->m_private_data) ((int *)hwnd->m_private_data)[2] = (int) lParam;
    break;
    case WM_NCDESTROY:
      free((int *)hwnd->m_private_data);
      hwnd->m_private_data=0;
    break;
    case WM_LBUTTONDBLCLK:
      if (hwnd->m_private_data) 
      {
        int *state = (int *)hwnd->m_private_data;
        const int range = state[1];
        const int low = LOWORD(range), high=HIWORD(range);
        const int to_val = state[2] >= low && state[2] <= high ? state[2] : (low+high)/2;
        if (state[0] != to_val)
        {
          state[0] = to_val;
          InvalidateRect(hwnd,NULL,FALSE);
          SendMessage(hwnd->m_parent,WM_HSCROLL,SB_ENDSCROLL,(LPARAM)hwnd);
        }
      }
    return 1;
    case WM_LBUTTONDOWN:
      SetFocusInternal(hwnd);
      SetCapture(hwnd);

      if (hwnd->m_private_data)
      {
        RECT r;
        GetClientRect(hwnd,&r);
        const int rad = wdl_min((r.bottom-r.top)/2-1,track_h);
        int *state = (int *)hwnd->m_private_data;
        const int range = state[1];
        const int low = LOWORD(range), high=HIWORD(range);
        const int dx = ((state[2]-low)*(r.right-2*rad))/(high-low) + rad;
        s_cap_offs=0;

        if (GET_X_LPARAM(lParam) >= dx-rad && GET_X_LPARAM(lParam)<=dx-rad)
        {
          s_cap_offs = GET_X_LPARAM(lParam)-dx;
          return 1;
        }
        // missed knob, so treat as a move
      }

    case WM_MOUSEMOVE:
      if (GetCapture()==hwnd && hwnd->m_private_data)
      {
        RECT r;
        GetClientRect(hwnd,&r);
        const int rad = wdl_min((r.bottom-r.top)/2-1,track_h);
        int *state = (int *)hwnd->m_private_data;
        const int range = state[1];
        const int low = LOWORD(range), high=HIWORD(range);
        int xpos = GET_X_LPARAM(lParam) - s_cap_offs;
        int use_range = (r.right-2*rad);
        if (use_range > 0)
        {
          int newval = low + (xpos - rad) * (high-low) / use_range;
          if (newval < low) newval=low;
          else if (newval > high) newval=high;
          if (newval != state[0])
          {
            state[0]=newval;
            InvalidateRect(hwnd,NULL,FALSE);
            SendMessage(hwnd->m_parent,WM_HSCROLL,0,(LPARAM)hwnd);
          }
        }
      }
    return 1;
    case WM_LBUTTONUP:
      if (GetCapture()==hwnd)
      {
        ReleaseCapture();
        SendMessage(hwnd->m_parent,WM_HSCROLL,SB_ENDSCROLL,(LPARAM)hwnd);
      }
    return 1;
    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 

          HBRUSH hbrush = (HBRUSH) SendMessage(GetParent(hwnd),WM_CTLCOLORSTATIC,(WPARAM)ps.hdc,(LPARAM)hwnd);
          if (hbrush == (HBRUSH)(INT_PTR)1) hbrush = NULL;
          else
          {
            if (hbrush) FillRect(ps.hdc,&r,hbrush);
            else
            {
              SWELL_FillDialogBackground(ps.hdc,&r,3);
            }
          }

          HBRUSH br = CreateSolidBrush(g_swell_ctheme.trackbar_track);
          const int rad = wdl_min((r.bottom-r.top)/2-1,track_h);

          RECT sr = r;
          sr.left += rad;
          sr.right -= rad;
          sr.top = (r.top+r.bottom)/2 - rad/2;
          sr.bottom = sr.top + rad;
          FillRect(ps.hdc,&sr,br);
          DeleteObject(br);

          sr.top = (r.top+r.bottom)/2 - rad;
          sr.bottom = sr.top + rad*2;

          if (hwnd->m_private_data)
          {
            const int *state = (const int *)hwnd->m_private_data;
            const int range = state[1];
            const int low = LOWORD(range), high=HIWORD(range);
            if (high > low)
            {
              if (state[2] >= low && state[2] <= high)
              {
                const int dx = ((state[2]-low)*(r.right-2*rad))/(high-low) + rad;
                HBRUSH markbr = CreateSolidBrush(g_swell_ctheme.trackbar_mark);
                RECT tr = sr;
                tr.left = dx;
                tr.right = dx+1;
                FillRect(ps.hdc,&tr,markbr);
                DeleteObject(markbr);
              }
              int pos = state[0];
              if (pos < low) pos=low;
              else if (pos > high) pos = high;

              const int dx = ((pos-low)*(r.right-2*rad))/(high-low) + rad;

              HBRUSH cbr = CreateSolidBrush(g_swell_ctheme.trackbar_knob);
              HGDIOBJ oldbr = SelectObject(ps.hdc,cbr);
              HGDIOBJ oldpen = SelectObject(ps.hdc,GetStockObject(NULL_PEN));
              Ellipse(ps.hdc, dx-rad, sr.top,  dx+rad, sr.bottom);
              SelectObject(ps.hdc,oldbr);
              SelectObject(ps.hdc,oldpen);
              DeleteObject(cbr);
            }
          }

          EndPaint(hwnd,&ps);
        }
      }
    break;
  }

  return DefWindowProc(hwnd,msg,wParam,lParam);
}

static LRESULT WINAPI labelWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 

          SetTextColor(ps.hdc,
             hwnd->m_enabled ? g_swell_ctheme.label_text : 
               g_swell_ctheme.label_text_disabled);
          SetBkMode(ps.hdc,TRANSPARENT);

          paintDialogBackground(hwnd,&r,ps.hdc);

          const char *buf = hwnd->m_title.Get();
          if (buf && buf[0]) 
          {
            if ((hwnd->m_style & SS_TYPEMASK) == SS_LEFT)
            {
              RECT tmp={0,};
              const int line_h = DrawText(ps.hdc," ",1,&tmp,DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);
              if (r.bottom > line_h*5/3)
              {
                int loffs=0;
                while (buf[loffs] && r.top < r.bottom)
                {
                  int post=0, lb=swell_getLineLength(buf+loffs, &post, r.right, ps.hdc);
                  if (lb>0)
                    DrawText(ps.hdc,buf+loffs,lb,&r,DT_TOP|DT_SINGLELINE|DT_LEFT);
                  r.top += line_h;
                  loffs+=lb+post;
                } 
                buf = NULL;
              }
            }
            if (buf) DrawText(ps.hdc,buf,-1,&r,
                ((hwnd->m_style & SS_CENTER) ? DT_CENTER : 
                 (hwnd->m_style & SS_RIGHT) ? DT_RIGHT : 0)|
                DT_VCENTER);
          }
          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_SETTEXT:
       InvalidateRect(hwnd,NULL,FALSE);
    break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
       if (hwnd->m_style & SS_NOTIFY)
         SendMessage(GetParent(hwnd),WM_COMMAND,
              ((msg==WM_LBUTTONDOWN?STN_CLICKED:STN_DBLCLK)<<16)|(hwnd->m_id&0xffff),0);
    return 1;
  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}

struct __SWELL_ComboBoxInternalState_rec 
{ 
  __SWELL_ComboBoxInternalState_rec(const char *_desc=NULL, LPARAM _parm=0) { desc=_desc?strdup(_desc):NULL; parm=_parm; } 
  ~__SWELL_ComboBoxInternalState_rec() { free(desc); } 
  char *desc; 
  LPARAM parm; 
  static int cmp(const __SWELL_ComboBoxInternalState_rec **a, const __SWELL_ComboBoxInternalState_rec **b) { return strcmp((*a)->desc, (*b)->desc); }
};

class __SWELL_ComboBoxInternalState
{
  public:
    __SWELL_ComboBoxInternalState() { selidx=-1; }
    ~__SWELL_ComboBoxInternalState() { }

    int selidx;
    WDL_PtrList_DeleteOnDestroy<__SWELL_ComboBoxInternalState_rec> items;
    __SWELL_editControlState editstate;
};

static LRESULT WINAPI comboWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  static const int buttonwid = 16; // used in edit combobox
  static int s_capmode_state;
  __SWELL_ComboBoxInternalState *s = (__SWELL_ComboBoxInternalState*)hwnd->m_private_data;
  if (msg >= CB_ADDSTRING && msg <= CB_INITSTORAGE)
  {
    if (s)
    {
      switch (msg)
      {
        case CB_ADDSTRING:
          
          if (!(hwnd->m_style & CBS_SORT))
          {
            s->items.Add(new __SWELL_ComboBoxInternalState_rec((const char *)lParam));
            return s->items.GetSize() - 1;
          }
          else
          {
            __SWELL_ComboBoxInternalState_rec *r=new __SWELL_ComboBoxInternalState_rec((const char *)lParam);
            // find position of insert for wParam
            bool m;
            int idx = s->items.LowerBound(r,&m,__SWELL_ComboBoxInternalState_rec::cmp);
            if (s->selidx >= idx) s->selidx++;
            s->items.Insert(idx,r);
            return idx;
          }

        case CB_INSERTSTRING:
          if (wParam > (WPARAM)s->items.GetSize()) wParam=(WPARAM)s->items.GetSize();
          s->items.Insert(wParam,new __SWELL_ComboBoxInternalState_rec((const char *)lParam));
          if (s->selidx >= (int)wParam) s->selidx++;
        return wParam;

        case CB_DELETESTRING:
          if (wParam >= (WPARAM)s->items.GetSize()) return CB_ERR;

          s->items.Delete(wParam,true);

          if (wParam == (WPARAM)s->selidx || s->selidx >= s->items.GetSize()) { s->selidx=-1; InvalidateRect(hwnd,NULL,FALSE); }
          else if ((int)wParam < s->selidx) s->selidx--;

        return s->items.GetSize();

        case CB_GETCOUNT: return s->items.GetSize();
        case CB_GETCURSEL: return s->selidx >=0 && s->selidx < s->items.GetSize() ? s->selidx : -1;

        case CB_GETLBTEXTLEN: 
        case CB_GETLBTEXT: 
          if (wParam < (WPARAM)s->items.GetSize()) 
          {
            __SWELL_ComboBoxInternalState_rec *rec = s->items.Get(wParam);
            if (!rec) return CB_ERR;
            const char *ptr=rec->desc;
            int l = ptr ? strlen(ptr) : 0;
            if (msg == CB_GETLBTEXT && lParam) memcpy((char *)lParam,ptr?ptr:"",l+1);
            return l;
          }
        return CB_ERR;
        case CB_RESETCONTENT:
          s->selidx=-1;
          s->items.Empty(true);
        return 0;
        case CB_SETCURSEL:
          if (wParam >= (WPARAM)s->items.GetSize())
          {
            if (s->selidx!=-1)
            {
              s->selidx = -1;
              SetWindowText(hwnd,"");
              InvalidateRect(hwnd,NULL,FALSE);
            }
            return CB_ERR;
          }
          else
          {
            if (s->selidx != (int)wParam)
            {
              s->selidx=(int)wParam;
              char *ptr=s->items.Get(wParam)->desc;
              SetWindowText(hwnd,ptr);
              InvalidateRect(hwnd,NULL,FALSE);
            }
          }
        return s->selidx;

        case CB_GETITEMDATA:
          if (wParam < (WPARAM)s->items.GetSize()) 
          {
            return s->items.Get(wParam)->parm;
          }
        return CB_ERR;
        case CB_SETITEMDATA:
          if (wParam < (WPARAM)s->items.GetSize()) 
          {
            s->items.Get(wParam)->parm=lParam;
            return 0;
          }
        return CB_ERR;
        case CB_INITSTORAGE:
        return 0;

        case CB_FINDSTRINGEXACT:
        case CB_FINDSTRING:
          {
            int x;
            int a = (int)wParam;
            a++;
            for (x=a;x<s->items.GetSize();x++) 
              if (msg == CB_FINDSTRING ? 
                  !stricmp(s->items.Get(x)->desc,(char *)lParam)  :
                  !strcmp(s->items.Get(x)->desc,(char *)lParam)) return x;

            for (x=0;x<a;x++)
              if (msg == CB_FINDSTRING ? 
                  !stricmp(s->items.Get(x)->desc,(char *)lParam)  :
                  !strcmp(s->items.Get(x)->desc,(char *)lParam)) return x;
          }
        return CB_ERR;
      }
    }
  }

  switch (msg)
  {
    case WM_NCDESTROY:
      hwnd->m_private_data=0;
      delete s;
    break;
    case WM_TIMER:
      if (wParam == 100)
      {
        if (++s->editstate.cursor_state >= 8) s->editstate.cursor_state=0;
        if (GetFocusIncludeMenus()!=hwnd || s->editstate.cursor_state<2) InvalidateRect(hwnd,NULL,FALSE);
      }
      else if (wParam==1)
      {
        if (!fast_has_focus(hwnd))
        {
          KillTimer(hwnd,1);
          InvalidateRect(hwnd,NULL,FALSE);
        }
      }
    return 0;

    case WM_LBUTTONDOWN:
      {
        RECT r;
        GetClientRect(hwnd,&r);
        if ((hwnd->m_style & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST || GET_X_LPARAM(lParam) >= r.right-SWELL_UI_SCALE(buttonwid))
        {
          s_capmode_state=5;
        }
        else
        {
          int xo=3;
          HDC hdc=GetDC(hwnd);
          const int last_cursor = s->editstate.cursor_pos;
          s->editstate.cursor_pos = 
            WDL_utf8_bytepos_to_charpos(hwnd->m_title.Get(),
              editHitTest(hdc,hwnd->m_title.Get(),
                          hwnd->m_title.GetLength(),
                          GET_X_LPARAM(lParam)-xo + s->editstate.scroll_x,
                          GET_Y_LPARAM(lParam),0,NULL,NULL)
            );
           
          s->editstate.onMouseDown(s_capmode_state,last_cursor);

          ReleaseDC(hwnd,hdc);

          SetFocusInternal(hwnd);
        }
        SetCapture(hwnd);
      }
      InvalidateRect(hwnd,NULL,FALSE);
    return 0;
    case WM_MOUSEMOVE:
      if (GetCapture()==hwnd)
      {
        if (s_capmode_state == 3 || s_capmode_state == 4)
        {
          const bool multiline = (hwnd->m_style & ES_MULTILINE) != 0;
          int xo=3;
          HDC hdc=GetDC(hwnd);
          int p = WDL_utf8_bytepos_to_charpos(hwnd->m_title.Get(),
            editHitTest(hdc,hwnd->m_title.Get(),
                        multiline?-1:hwnd->m_title.GetLength(),
                         GET_X_LPARAM(lParam)-xo + s->editstate.scroll_x,
                         GET_Y_LPARAM(lParam),0,NULL,NULL)
              );
          ReleaseDC(hwnd,hdc);

          s->editstate.onMouseDrag(s_capmode_state,p);
          s->editstate.autoScrollToOffset(hwnd,p,false,false);

          InvalidateRect(hwnd,NULL,FALSE);
        }
      }
    return 0;
    case WM_LBUTTONUP:
      if (GetCapture()==hwnd)
      {
        ReleaseCapture(); 
popupMenu:
        if (s && s->items.GetSize() && s_capmode_state == 5)
        {
          int x;
          HMENU menu = CreatePopupMenu();
          for (x=0;x<s->items.GetSize();x++)
          {
            MENUITEMINFO mi={sizeof(mi),MIIM_ID|MIIM_STATE|MIIM_TYPE,MFT_STRING,
              (UINT) (x == s->selidx?MFS_CHECKED:0),
              (UINT) (100+x), NULL,NULL,NULL,0,s->items.Get(x)->desc};
            InsertMenuItem(menu,x,TRUE,&mi);
          }

          hwnd->Retain();
          RECT r;
          GetWindowRect(hwnd,&r);
          int a = TrackPopupMenu(menu,TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN,r.left,r.bottom,0,hwnd,0);
          DestroyMenu(menu);
          if (hwnd->m_private_data && a>=100 && a < s->items.GetSize()+100)
          {
            s->selidx = a-100;
            char *ptr=s->items.Get(s->selidx)->desc;
            SetWindowText(hwnd,ptr);
            InvalidateRect(hwnd,NULL,FALSE);
            SendMessage(GetParent(hwnd),WM_COMMAND,(GetWindowLong(hwnd,GWL_ID)&0xffff) | (CBN_SELCHANGE<<16),(LPARAM)hwnd);
          }
          hwnd->Release();
        }
      }
      s_capmode_state=0;
    return 0;
    case WM_LBUTTONDBLCLK:
      if (s)
      {
        // technically this should select the word rather than all
        s->editstate.sel1 = 0;
        s->editstate.cursor_pos = s->editstate.sel2 = WDL_utf8_get_charlen(hwnd->m_title.Get());
        InvalidateRect(hwnd,NULL,FALSE);
      }
    return 0;
    case WM_KEYDOWN:
      if ((lParam&FVIRTKEY) && wParam == VK_DOWN) { s_capmode_state=5; goto popupMenu; }
      if ((hwnd->m_style & CBS_DROPDOWNLIST) != CBS_DROPDOWNLIST && 
          OnEditKeyDown(hwnd,msg,wParam,lParam,false,false,&s->editstate,false))
      {
        if (s) s->selidx=-1; // lookup from text?
        SendMessage(GetParent(hwnd),WM_COMMAND,(CBN_EDITCHANGE<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
        s->editstate.autoScrollToOffset(hwnd,s->editstate.cursor_pos,false,false);
        InvalidateRect(hwnd,NULL,FALSE);
        return 0;
      }
      if (wParam == VK_SPACE) { s_capmode_state=5; goto popupMenu; }
    break;
    case WM_KEYUP:
    return 0;
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 
          bool pressed = s_capmode_state == 5 && GetCapture()==hwnd;

          SetTextColor(ps.hdc,
            hwnd->m_enabled ? g_swell_ctheme.combo_text : 
              g_swell_ctheme.combo_text_disabled);
          SetBkMode(ps.hdc,TRANSPARENT);

          Draw3DBox(ps.hdc,&r,g_swell_ctheme.combo_bg,
              g_swell_ctheme.combo_hilight,
              g_swell_ctheme.combo_shadow,pressed);

          int cursor_pos = -1;
          bool focused = false;
          if ((hwnd->m_style & CBS_DROPDOWNLIST) != CBS_DROPDOWNLIST)
          {
            focused = GetFocusIncludeMenus()==hwnd;
            if (focused)
            {
              if (!s->editstate.cursor_timer) { SetTimer(hwnd,100,100,NULL); s->editstate.cursor_timer=1; }
            }
            else
            {
              if (s->editstate.cursor_timer) { KillTimer(hwnd,100); s->editstate.cursor_timer=0; }
            }

            HBRUSH br = CreateSolidBrush(g_swell_ctheme.combo_bg2);
            RECT tr=r; 
            const int pad = SWELL_UI_SCALE(2);
            tr.left+=pad; tr.top+=pad; tr.bottom-=pad; tr.right -= SWELL_UI_SCALE(buttonwid+2);
            FillRect(ps.hdc,&tr,br);
            DeleteObject(br);

            if (focused && s->editstate.cursor_state)
            {
              cursor_pos = WDL_utf8_charpos_to_bytepos(hwnd->m_title.Get(),s->editstate.cursor_pos);
            }
          }

          HBRUSH br = CreateSolidBrush(pressed?g_swell_ctheme.combo_arrow_press : g_swell_ctheme.combo_arrow);
          HGDIOBJ oldbr=SelectObject(ps.hdc,br);
          HGDIOBJ oldpen=SelectObject(ps.hdc,GetStockObject(NULL_PEN));
          const int dw = SWELL_UI_SCALE(8);
          const int dh = SWELL_UI_SCALE(4);
          const int cx = r.right-dw/2-SWELL_UI_SCALE(4);
          const int cy = (r.bottom+r.top)/2;

          POINT pts[3] = {
            { cx-dw/2,cy-dh/2 },
            { cx,cy+dh/2 },
            { cx+dw/2,cy-dh/2 }
          };
          Polygon(ps.hdc,pts,3);

          SelectObject(ps.hdc,oldpen);
          SelectObject(ps.hdc,oldbr);
          DeleteObject(br);

         
          if (pressed) 
          {
            r.left+=SWELL_UI_SCALE(2);
            r.top+=SWELL_UI_SCALE(2);
          }

          r.left+=SWELL_UI_SCALE(3);
          r.right-=SWELL_UI_SCALE(3);

          if ((hwnd->m_style & CBS_DROPDOWNLIST) != CBS_DROPDOWNLIST)
          {
            r.right -= SWELL_UI_SCALE(buttonwid+2);
            editControlPaintLine(ps.hdc, hwnd->m_title.Get(), hwnd->m_title.GetLength(), cursor_pos, 
                focused ? s->editstate.sel1 : -1, focused ? s->editstate.sel2 : -1, &r, DT_VCENTER);
          }
          else
          {
            char buf[512];
            buf[0]=0;
            GetWindowText(hwnd,buf,sizeof(buf));
            if (buf[0]) DrawText(ps.hdc,buf,-1,&r,DT_VCENTER);
          }

          if (draw_focus_indicator(hwnd,ps.hdc,NULL))
          {
            KillTimer(hwnd,1);
            SetTimer(hwnd,1,100,NULL);
          }
          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_SETTEXT:
      s->editstate.cursor_pos = WDL_utf8_get_charlen(hwnd->m_title.Get());
      s->editstate.sel1 = s->editstate.sel2 = -1;
    case WM_CAPTURECHANGED:
      InvalidateRect(hwnd,NULL,FALSE);
    break;
    case EM_SETSEL:
      if (s && (hwnd->m_style & CBS_DROPDOWNLIST) != CBS_DROPDOWNLIST)
      {
        s->editstate.sel1 = (int)wParam;
        s->editstate.sel2 = (int)lParam;
        if (!s->editstate.sel1 && s->editstate.sel2 == -1)
          s->editstate.sel2 = WDL_utf8_get_charlen(hwnd->m_title.Get());
        if (s->editstate.sel2>=0)
          s->editstate.autoScrollToOffset(hwnd,s->editstate.sel2, false, false);
        InvalidateRect(hwnd,NULL,FALSE);
      }
    return 0;
  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}


/// these are for swell-dlggen.h
HWND SWELL_MakeButton(int def, const char *label, int idx, int x, int y, int w, int h, int flags)
{  
  UINT_PTR a=(UINT_PTR)label;
  if (a < 65536) label = "ICONTEMP";
  
  RECT tr=MakeCoords(x,y,w,h,true);
  HWND hwnd = swell_makeButton(m_make_owner,idx,&tr,label,!(flags&SWELL_NOT_WS_VISIBLE),(def ? BS_DEFPUSHBUTTON : 0) | (flags&BS_LEFT));

  if (m_doautoright) UpdateAutoCoords(tr);
  if (def) { }
  return hwnd;
}


HWND SWELL_MakeLabel( int align, const char *label, int idx, int x, int y, int w, int h, int flags)
{
  RECT tr=MakeCoords(x,y,w,h,true);
  HWND hwnd = new HWND__(m_make_owner,idx,&tr,label, !(flags&SWELL_NOT_WS_VISIBLE),labelWindowProc);
  hwnd->m_classname = "static";
  if (align > 0) flags |= SS_RIGHT;
  else if (align == 0) flags |= SS_CENTER;
  hwnd->m_style = (flags & ~SWELL_NOT_WS_VISIBLE)|WS_CHILD;
  hwnd->m_wantfocus = false;
  hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
  if (m_doautoright) UpdateAutoCoords(tr);
  return hwnd;
}
HWND SWELL_MakeEditField(int idx, int x, int y, int w, int h, int flags)
{  
  RECT tr=MakeCoords(x,y,w,h,true);
  HWND hwnd = new HWND__(m_make_owner,idx,&tr,NULL, !(flags&SWELL_NOT_WS_VISIBLE),editWindowProc);
  hwnd->m_private_data = (INT_PTR) new __SWELL_editControlState;
  hwnd->m_style = WS_CHILD | (flags & ~SWELL_NOT_WS_VISIBLE);
  hwnd->m_classname = "Edit";
  hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
  if (m_doautoright) UpdateAutoCoords(tr);
  return hwnd;
}


HWND SWELL_MakeCheckBox(const char *name, int idx, int x, int y, int w, int h, int flags=0)
{
  return SWELL_MakeControl(name,idx,"Button",BS_AUTOCHECKBOX|flags,x,y,w,h,0);
}

struct SWELL_ListView_Col
{
  char *name;
  int xwid;
  int sortindicator;
};

enum { LISTVIEW_HDR_YMARGIN = 2 };

struct listViewState
{
  listViewState(bool ownerData, bool isMultiSel, bool isListBox)
  {
    m_selitem=-1;
    m_is_multisel = isMultiSel;
    m_is_listbox = isListBox;
    m_owner_data_size = ownerData ? 0 : -1;
    m_last_row_height = 0;
    m_scroll_x=m_scroll_y=0;
    m_capmode_state=0;
    m_capmode_data1=0;
    m_capmode_data2=0;
    m_status_imagelist = NULL;
    m_status_imagelist_type = 0;
    m_extended_style=0;

    m_color_bg = g_swell_ctheme.listview_bg;
    m_color_bg_sel = g_swell_ctheme.listview_bg_sel;
    m_color_text = g_swell_ctheme.listview_text;
    m_color_text_sel = g_swell_ctheme.listview_text_sel;
    m_color_grid = g_swell_ctheme.listview_grid;
    memset(m_color_extras,0xff,sizeof(m_color_extras)); // if !=-1, overrides bg/fg for (focus?0:2)
  } 
  ~listViewState()
  { 
    m_data.Empty(true);
    const int n=m_cols.GetSize();
    for (int x=0;x<n;x++) free(m_cols.Get()[x].name);
  }
  WDL_PtrList<SWELL_ListView_Row> m_data;
  WDL_TypedBuf<SWELL_ListView_Col> m_cols;
  
  int GetNumItems() const { return m_owner_data_size>=0 ? m_owner_data_size : m_data.GetSize(); }
  bool IsOwnerData() const { return m_owner_data_size>=0; }
  bool HasColumnHeaders(HWND hwnd) const
  { 
     if (m_is_listbox || !m_cols.GetSize()) return false;
     return !(hwnd->m_style & LVS_NOCOLUMNHEADER) && (hwnd->m_style & LVS_REPORT);
  }
  int GetColumnHeaderHeight(HWND hwnd) const { return HasColumnHeaders(hwnd) ? m_last_row_height+LISTVIEW_HDR_YMARGIN : 0; }

  int m_owner_data_size; // -1 if m_data valid, otherwise size
  int m_last_row_height;
  int m_selitem; // for single sel, or used for focus for multisel

  int m_scroll_x,m_scroll_y,m_capmode_state, m_capmode_data1,m_capmode_data2;
  int m_extended_style;

  int m_color_bg, m_color_bg_sel, m_color_text, m_color_text_sel, m_color_grid;
  int m_color_extras[4];

  int getTotalWidth() const
  {
    int s = 0;
    const SWELL_ListView_Col *col = m_cols.Get();
    const int n = m_cols.GetSize();
    for (int x=0; x < n; x ++) s += col[x].xwid;
    return s;
  }

  void sanitizeScroll(HWND h)
  {
    RECT r;
    GetClientRect(h,&r);
    r.right -= g_swell_ctheme.scrollbar_width;

    const int mx = getTotalWidth() - r.right;
    if (m_scroll_x > mx) m_scroll_x = mx;
    if (m_scroll_x < 0) m_scroll_x = 0;

    if (m_last_row_height > 0)
    {
      r.bottom -= GetColumnHeaderHeight(h);
      if (mx>0) r.bottom -= g_swell_ctheme.scrollbar_width;

      const int vh = m_last_row_height * GetNumItems();
      if (m_scroll_y < 0 || vh <= r.bottom) m_scroll_y=0;
      else if (m_scroll_y > vh - r.bottom) m_scroll_y = vh - r.bottom;
    }
  }

  WDL_TypedBuf<unsigned int> m_owner_multisel_state;

  bool get_sel(int idx)
  {
    if (!m_is_multisel) return idx>=0 && idx == m_selitem;
    if (m_owner_data_size<0)
    {
      SWELL_ListView_Row *p = m_data.Get(idx);
      return p && (p->m_tmp&1);
    }
    const unsigned int mask = 1<<(idx&31);
    const int szn = idx/32;
    const unsigned int *p=m_owner_multisel_state.Get();
    return p && idx>=0 && szn < m_owner_multisel_state.GetSize() && (p[szn]&mask);
  }
  bool set_sel(int idx, bool v) // returns true if value changed
  {
    if (!m_is_multisel) 
    { 
      const int oldsel = m_selitem;
      if (v) m_selitem = idx;
      else if (oldsel == idx) m_selitem = -1;

      return oldsel != m_selitem;
    }
    else if (m_owner_data_size<0)
    {
      SWELL_ListView_Row *p = m_data.Get(idx);
      if (p) 
      {
        const int oldtmp = p->m_tmp;
        return (p->m_tmp = (v ? (oldtmp|1) : (oldtmp&~1))) != oldtmp;
      }
    }
    else 
    {
      if (idx>=0 && idx < m_owner_data_size)
      {
        const int szn = idx/32;
        const int oldsz=m_owner_multisel_state.GetSize();
        unsigned int *p = m_owner_multisel_state.Get();
        if (oldsz<szn+1) 
        {
          p = m_owner_multisel_state.ResizeOK(szn+1,false);
          if (p) memset(p+oldsz,0,(szn+1-oldsz)*sizeof(*p));
        }
        const unsigned int mask = 1<<(idx&31);
        if (p) 
        {
          const unsigned int oldval = p[szn];
          return oldval != (p[szn] = v ? (oldval|mask) : (oldval&~mask));
        }
      }
    }
    return false;
  }
  bool clear_sel()
  {
    if (!m_is_multisel) 
    {
      if (m_selitem != -1) { m_selitem = -1; return true; }
      return false;
    }

    if (m_owner_data_size<0)
    {
      bool rv=false;
      const int n=m_data.GetSize();
      for (int x=0;x<n;x++) 
      {
        int *a = &m_data.Get(x)->m_tmp;
        if (*a&1)
        {
          *a&=~1;
          rv=true;
        }
      }
      return rv;
    }

    bool rv=false;
    int n = m_owner_multisel_state.GetSize();
    if (n > m_owner_data_size) n=m_owner_data_size;
    for(int x=0;x<n;x++) if (m_owner_multisel_state.Get()[x]) { rv=true; break; }

    m_owner_multisel_state.Resize(0,false);
    return rv;
  }
  bool hasStatusImage() const
  {
    return m_status_imagelist && m_status_imagelist_type == LVSIL_STATE;
  }
  bool hasAnyImage() const
  {
    return m_status_imagelist && 
           (m_status_imagelist_type == LVSIL_SMALL || m_status_imagelist_type == LVSIL_STATE);
  }
  

  bool m_is_multisel, m_is_listbox;
  WDL_PtrList<HGDIOBJ__> *m_status_imagelist;
  int m_status_imagelist_type;
};

// returns non-NULL if a searching string occurred
static const char *stateStringOnKey(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (lParam & (FCONTROL|FALT|FLWIN)) return NULL;
  if (uMsg != WM_KEYDOWN) return NULL;
  static WDL_FastString str;
  static DWORD last_t;
  DWORD now = GetTickCount();
  if (now > last_t + 500 || now < last_t - 500) str.Set("");
  last_t = now;

  const bool is_numpad = wParam >= VK_NUMPAD0 && wParam <= VK_DIVIDE;
  if ((lParam & FVIRTKEY) && wParam == VK_BACK)
  {
    str.SetLen(WDL_utf8_charpos_to_bytepos(str.Get(),WDL_utf8_get_charlen(str.Get())-1));
  }
  else if (wParam >= 32 && (!(lParam & FVIRTKEY) || swell_is_virtkey_char((int)wParam) || is_numpad))
  {
    if (lParam & FVIRTKEY)
    {
      if (wParam >= 'A' && wParam <= 'Z')
      {
        if ((lParam&FSHIFT) ^ (swell_is_likely_capslock?0:FSHIFT)) wParam += 'a' - 'A';
      }
      else if (is_numpad)
      {
        if (wParam <= VK_NUMPAD9) wParam += '0' - VK_NUMPAD0;
        else wParam += '*' - VK_MULTIPLY;
      }
    }

    char b[8];
    WDL_MakeUTFChar(b,wParam,sizeof(b));
    str.Append(b);
    return str.Get();
  }

  return NULL;
}


static LRESULT listViewWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  enum { col_resize_sz = 3 };
  listViewState *lvs = (listViewState *)hwnd->m_private_data;
  static POINT s_clickpt;
  switch (msg)
  {
    case WM_MOUSEWHEEL:
      if ((GetAsyncKeyState(VK_CONTROL)&0x8000) || (GetAsyncKeyState(VK_MENU)&0x8000)) break; // pass modified mousewheel to parent

      {
        const int amt = ((short)HIWORD(wParam))/40;
        if (amt && lvs)
        {
          if (GetAsyncKeyState(VK_SHIFT)&0x8000)
          {
            const int oldscroll = lvs->m_scroll_x;
            lvs->m_scroll_x -= amt*4;
            lvs->sanitizeScroll(hwnd);
            if (lvs->m_scroll_x != oldscroll)
              InvalidateRect(hwnd,NULL,FALSE);
          }  
          else
          {
            const int oldscroll = lvs->m_scroll_y;
            lvs->m_scroll_y -= amt*lvs->m_last_row_height;
            lvs->sanitizeScroll(hwnd);
            if (lvs->m_scroll_y != oldscroll)
              InvalidateRect(hwnd,NULL,FALSE);
          }
        }
      }
    return 1;
    case WM_RBUTTONDOWN:
      if (lvs && lvs->m_last_row_height>0 && !lvs->m_is_listbox)
      {
        LVHITTESTINFO inf = { { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }, };
        const int row = ListView_SubItemHitTest(hwnd, &inf);
        const int n = ListView_GetItemCount(hwnd);
        if (row>=0 && row<n && !ListView_GetItemState(hwnd,row,LVIS_SELECTED))
        {
          for (int x=0;x<n;x++)
          {
            ListView_SetItemState(hwnd,x,(x==row)?(LVIS_SELECTED|LVIS_FOCUSED):0,LVIS_SELECTED|LVIS_FOCUSED);
          }
        }

        NMLISTVIEW nm={{hwnd,hwnd->m_id,NM_RCLICK},row,inf.iSubItem,0,0,0, {inf.pt.x,inf.pt.y}};
        SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
      }
    return 1;
    case WM_SETCURSOR:
      if (lvs)
      {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(hwnd,&p);
        const int hdr_size = lvs->GetColumnHeaderHeight(hwnd);
        if (p.y >= 0 && p.y < hdr_size)
        {
          const SWELL_ListView_Col *col = lvs->m_cols.Get();
          p.x += lvs->m_scroll_x;
          if (lvs->hasStatusImage()) p.x -= lvs->m_last_row_height;

          for (int x=0; x < lvs->m_cols.GetSize(); x ++)
          {
            const int minw = wdl_max(col_resize_sz+1,col[x].xwid);
            if (p.x >= minw-col_resize_sz && p.x < minw)
            {
              SetCursor(SWELL_LoadCursor(IDC_SIZEWE));
              return 1;
            }
            p.x -= col[x].xwid;
          }
        }
      }
    break;
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
      SetFocusInternal(hwnd);
      if (msg == WM_LBUTTONDOWN) SetCapture(hwnd);
      else ReleaseCapture();

      if (lvs && lvs->m_last_row_height>0)
      {
        s_clickpt.x = GET_X_LPARAM(lParam);
        s_clickpt.y = GET_Y_LPARAM(lParam);
        RECT r;
        GetClientRect(hwnd,&r);
        const int hdr_size = lvs->GetColumnHeaderHeight(hwnd);
        const int hdr_size_nomargin = hdr_size>0 ? hdr_size-LISTVIEW_HDR_YMARGIN : 0;
        const int n=lvs->GetNumItems();
        const int row_height = lvs->m_last_row_height;
        const int totalw = lvs->getTotalWidth();

        if (hdr_size + n * row_height > r.bottom - g_swell_ctheme.scrollbar_width)
          r.right -= g_swell_ctheme.scrollbar_width;

        if (GET_Y_LPARAM(lParam) >= 0 && GET_Y_LPARAM(lParam) < hdr_size)
        {
          const SWELL_ListView_Col *col = lvs->m_cols.Get();
          int px = GET_X_LPARAM(lParam) + lvs->m_scroll_x;
          if (lvs->hasStatusImage()) px -= lvs->m_last_row_height;
          for (int x=0; x < lvs->m_cols.GetSize(); x ++)
          {
            const int minw = wdl_max(col_resize_sz+1,col[x].xwid);
            if (px >= minw-col_resize_sz && px < minw)
            {
              lvs->m_capmode_state = 3;
              lvs->m_capmode_data1 = x;
              lvs->m_capmode_data2 = minw-px;
              return 0;
            }

            if (px >= 0 && px <col[x].xwid)
            {
              HWND par = hwnd->m_parent;
              if (par)
              {
                NMLISTVIEW hdr={{hwnd,(UINT_PTR)hwnd->m_id,LVN_COLUMNCLICK},-1,x};
                if (par->m_wndproc&&!par->m_hashaddestroy) par->m_wndproc(par,WM_NOTIFY,hwnd->m_id, (LPARAM) &hdr);
              }
              ReleaseCapture();
              return 0;
            }
            px -= col[x].xwid;
          }
        }
        else if (totalw > r.right && 
                 GET_Y_LPARAM(lParam) >= r.bottom - g_swell_ctheme.scrollbar_width)
        {
          const int xpos = GET_X_LPARAM(lParam);
          int xp = xpos;

          int thumbsz, thumbpos;
          calcScroll(r.right,totalw,lvs->m_scroll_x,&thumbsz,&thumbpos);

          if (xpos < thumbpos) xp = thumbpos; // jump on first mouse move
          else if (xpos > thumbpos+thumbsz) xp = thumbpos + thumbsz;

          lvs->m_capmode_state = 4;
          lvs->m_capmode_data1 = xp;
          if (xpos < thumbpos || xpos > thumbpos+thumbsz) goto forceMouseMove;
          return 0;
        }

        lvs->m_capmode_state=0;
        const int ypos = GET_Y_LPARAM(lParam) - hdr_size_nomargin;

        if (totalw > r.right) r.bottom -= g_swell_ctheme.scrollbar_width;
        if (n * row_height > r.bottom - hdr_size_nomargin && 
            GET_X_LPARAM(lParam) >= r.right)
        {
          int yp = GET_Y_LPARAM(lParam);

          int thumbsz, thumbpos;
          calcScroll(r.bottom - hdr_size_nomargin, n*row_height,lvs->m_scroll_y,&thumbsz,&thumbpos);

          if (ypos < thumbpos) yp = thumbpos + hdr_size_nomargin; // jump on first mouse move
          else if (ypos > thumbpos+thumbsz) yp = thumbpos + hdr_size_nomargin + thumbsz;

          lvs->m_capmode_state = 1;
          lvs->m_capmode_data1 = yp;
          if (ypos < thumbpos || ypos > thumbpos+thumbsz) goto forceMouseMove;
          return 0;
        }

        const int hit = ypos >= 0 ? ((ypos+lvs->m_scroll_y) / row_height) : -1;
        if (hit < 0) return 1;

        int subitem = 0;

        {
          const int ncol=lvs->m_cols.GetSize();
          const bool has_image = lvs->hasStatusImage();
          int xpos=0, xpt = GET_X_LPARAM(lParam) + lvs->m_scroll_x;
          if (has_image) xpos += lvs->m_last_row_height;
          for (int x=0;x<ncol;x++)
          {
            const int xwid = lvs->m_cols.Get()[x].xwid;
            if (xpt >= xpos && xpt < xpos+xwid) { subitem = x; break; }
            xpos += xwid;
          }
        }


        if (!lvs->m_is_multisel)
        {
          const int oldsel = lvs->m_selitem;
          if (hit >= 0 && hit < n) lvs->m_selitem = hit;
          else lvs->m_selitem = -1;

          if (lvs->m_is_listbox)
          {
            //if (oldsel != lvs->m_selitem) 
            SendMessage(GetParent(hwnd),WM_COMMAND,(LBN_SELCHANGE<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
            if (msg == WM_LBUTTONDBLCLK)
              SendMessage(GetParent(hwnd),WM_COMMAND,(LBN_DBLCLK<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
          }
          else
          {
            if (hit >= 0) 
            {
              lvs->m_capmode_state = 2;
              lvs->m_capmode_data1 = hit;
              lvs->m_capmode_data2 = subitem;
            }

            if(hit < n)
            {
              NMLISTVIEW nm={{hwnd,hwnd->m_id,msg == WM_LBUTTONDBLCLK ? NM_DBLCLK : NM_CLICK},hit,subitem,0,0,0, {s_clickpt.x, s_clickpt.y }};
              SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
            }
            if (oldsel != lvs->m_selitem) 
            {
              NMLISTVIEW nm={{hwnd,hwnd->m_id,LVN_ITEMCHANGED},lvs->m_selitem,1,LVIS_SELECTED,};
              SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
            }
          }
          InvalidateRect(hwnd,NULL,FALSE);
        }
        else 
        {
          bool changed = false;
          if (hit >= n)
          {
            changed |= lvs->clear_sel();
            lvs->m_selitem = -1;
          }
          else
          {
            const bool ctrl = (GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
            if (!ctrl) 
            {
              if (!lvs->get_sel(hit))
                changed |= lvs->clear_sel();
            }
            if ((GetAsyncKeyState(VK_SHIFT)&0x8000) && lvs->m_selitem >= 0)
            {
              int a=lvs->m_selitem;
              int b = hit;
              if (a>b) { b=a; a=hit; }
              while (a<=b) changed |= lvs->set_sel(a++,true);
            }
            else
            {
              lvs->m_selitem = hit;
              changed |= lvs->set_sel(hit,!ctrl || !lvs->get_sel(hit));
            }
          }

          if (lvs->m_is_listbox)
          {
            if (changed) SendMessage(GetParent(hwnd),WM_COMMAND,(LBN_SELCHANGE<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
            if (msg == WM_LBUTTONDBLCLK)
              SendMessage(GetParent(hwnd),WM_COMMAND,(LBN_DBLCLK<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
          }
          else 
          {
            if (hit >=0 && hit < n)
            {
              lvs->m_capmode_state = 2;
              lvs->m_capmode_data1 = hit;
              lvs->m_capmode_data2 = subitem;
              NMLISTVIEW nm={{hwnd,hwnd->m_id,msg == WM_LBUTTONDBLCLK ? NM_DBLCLK : NM_CLICK},hit,subitem,LVIS_SELECTED,};
              SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
            }
            if (changed)
            {
              NMLISTVIEW nm={{hwnd,hwnd->m_id,LVN_ITEMCHANGED},hit,0,LVIS_SELECTED,};
              if (nm.iItem < 0 || nm.iItem >= n) nm.iItem=0;
              SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
            }
          }

          InvalidateRect(hwnd,NULL,FALSE);
        }
      }
    return 1;
    case WM_MOUSEMOVE:
      if (GetCapture()==hwnd && lvs)
      {
forceMouseMove:
        RECT r;
        GetClientRect(hwnd,&r);
        r.right -= g_swell_ctheme.scrollbar_width;
        switch (lvs->m_capmode_state)
        {
          case 3:
            {
              int x = lvs->m_capmode_data1;
              int xp = GET_X_LPARAM(lParam) + lvs->m_scroll_x + lvs->m_capmode_data2;
              if (lvs->hasStatusImage()) xp -= lvs->m_last_row_height;

              SWELL_ListView_Col *col = lvs->m_cols.Get();
              if (x < lvs->m_cols.GetSize())
              {
                for (int i = 0; i < x; i ++) xp -= col[i].xwid;
                if (xp<0) xp=0;
                if (col[x].xwid != xp)
                {
                  col[x].xwid = xp;
                  if (lvs->m_scroll_x > 0 && GET_X_LPARAM(lParam) < 0)
                    lvs->m_scroll_x--;
                  else if (GET_X_LPARAM(lParam) > r.right+12)
                    lvs->m_scroll_x+=16; // additional check might not be necessary?

                  InvalidateRect(hwnd,NULL,FALSE);
                }
              }
            }
          break;
          case 2:
            if (!lvs->m_is_listbox)
            {
              const int dx = GET_X_LPARAM(lParam) - s_clickpt.x, dy = GET_Y_LPARAM(lParam) - s_clickpt.y;
              if (dx*dx+dy*dy > 32)
              {
                NMLISTVIEW nm={{hwnd,hwnd->m_id,LVN_BEGINDRAG},lvs->m_capmode_data1,lvs->m_capmode_data2};
                lvs->m_capmode_state=0;
                SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
              }
            }
          break;
          case 1:
            {
              int yv = lvs->m_capmode_data1;
              int amt = GET_Y_LPARAM(lParam) - yv;

              if (amt)
              {
                const int totalw = lvs->getTotalWidth();
                if (totalw > r.right) r.bottom -= lvs->m_last_row_height;


                const int viewsz = r.bottom-lvs->GetColumnHeaderHeight(hwnd);
                const double totalsz=(double)lvs->GetNumItems() * (double)lvs->m_last_row_height;
                amt = (int)floor(amt * totalsz / (double)viewsz + 0.5);
              
                const int oldscroll = lvs->m_scroll_y;
                lvs->m_scroll_y += amt;
                lvs->sanitizeScroll(hwnd);
                if (lvs->m_scroll_y != oldscroll)
                {
                  lvs->m_capmode_data1 = GET_Y_LPARAM(lParam);
                  InvalidateRect(hwnd,NULL,FALSE);
                }
              }
            }
          break;
          case 4:
            {
              int xv = lvs->m_capmode_data1;
              int amt = GET_X_LPARAM(lParam) - xv;

              if (amt)
              {
                const int viewsz = r.right;
                const double totalsz=(double)lvs->getTotalWidth();
                amt = (int)floor(amt * totalsz / (double)viewsz + 0.5);
              
                const int oldscroll = lvs->m_scroll_x;
                lvs->m_scroll_x += amt;
                lvs->sanitizeScroll(hwnd);
                if (lvs->m_scroll_x != oldscroll)
                {
                  lvs->m_capmode_data1 = GET_X_LPARAM(lParam);
                  InvalidateRect(hwnd,NULL,FALSE);
                }
              }
            }
          break;
        }
      }
    return 1;
    case WM_LBUTTONUP:
      if (GetCapture()==hwnd)
      {
        ReleaseCapture(); // WM_CAPTURECHANGED will take care of the invalidate
      }
    return 1;
    case WM_KEYDOWN:
      if (lvs)
      {
        const char *s = stateStringOnKey(msg,wParam,lParam);
        if (s)
        {
          int col = 0;
          if (!lvs->m_is_listbox)
          {
            for (int x=0;x<lvs->m_cols.GetSize();x++)
            {
              if (lvs->m_cols.Get()[x].sortindicator)
              {
                col = x;
                break;
              }
            }
          }

          const int n = lvs->GetNumItems();
          int selitem = lvs->m_selitem;
          if (selitem < 0 || selitem >= n) selitem=0;
          for (int x=0;x<n;x++)
          {
            int offs = (selitem + x) % n;
            if (offs < 0) offs+=n;

            const char *v=NULL;
            char buf[1024];
            if (!lvs->IsOwnerData())
            {
              SWELL_ListView_Row *row = lvs->m_data.Get(offs);
              if (row) v = row->m_vals.Get(col);
            }
            else
            {
              buf[0]=0;
              NMLVDISPINFO nm={{hwnd,hwnd->m_id,LVN_GETDISPINFO},{LVIF_TEXT, offs,col, 0,0, buf, sizeof(buf), -1 }};
              SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
              v = buf;
            }

            if (v && !strnicmp(v,s,strlen(s))) 
            {
              if (!lvs->m_is_multisel)
              {
                const int oldsel = lvs->m_selitem;
                lvs->m_selitem = offs;

                if (lvs->m_is_listbox)
                {
                  SendMessage(GetParent(hwnd),WM_COMMAND,(LBN_SELCHANGE<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
                }
                else
                {
                  if (oldsel != lvs->m_selitem) 
                  {
                    NMLISTVIEW nm={{hwnd,hwnd->m_id,LVN_ITEMCHANGED},lvs->m_selitem,1,LVIS_SELECTED,};
                    SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
                  }
                }
              }
              else 
              {
                bool changed = lvs->clear_sel() | lvs->set_sel(offs,true);
                lvs->m_selitem = offs;

                if (lvs->m_is_listbox)
                {
                  if (changed) SendMessage(GetParent(hwnd),WM_COMMAND,(LBN_SELCHANGE<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
                }
                else 
                {
                  if (changed)
                  {
                    NMLISTVIEW nm={{hwnd,hwnd->m_id,LVN_ITEMCHANGED},offs,0,LVIS_SELECTED,};
                    if (nm.iItem < 0 || nm.iItem >= n) nm.iItem=0;
                    SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
                  }
                }
              }

              ListView_EnsureVisible(hwnd,offs,FALSE);
              InvalidateRect(hwnd,NULL,FALSE);
              break;
            }
          }
        }
      }
      if (lvs && (lParam & FVIRTKEY)) 
      {
        int flag=0;
        int ni;
        const int oldsel = lvs->m_selitem;

        switch (wParam)
        {
          case VK_NEXT:
          case VK_PRIOR:
          case VK_UP:
          case VK_DOWN:
            {
              RECT r;
              GetClientRect(hwnd,&r);
              const int page = lvs->m_last_row_height ? 
                (r.bottom - g_swell_ctheme.scrollbar_width)/lvs->m_last_row_height : 4;
              const int cnt = lvs->GetNumItems();

              ni = lvs->m_selitem + (wParam == VK_UP ? -1 :
                                     wParam == VK_PRIOR ? 2-wdl_max(page,3) :
                                     wParam == VK_NEXT ? wdl_max(page,3)-2 : 
                                     1);

              if (ni<0) ni=0;
              if (ni>cnt-1) ni=cnt-1;
              const bool shift = (GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;
              
              if (ni>=0 && (ni!=lvs->m_selitem || !shift))
              {
                if (lvs->m_is_multisel) 
                {
                  if (!shift)
                  {
                    lvs->clear_sel();
                    lvs->set_sel(ni,true);
                  }
                  else
                  {
                    if (lvs->get_sel(ni)) lvs->set_sel(lvs->m_selitem,false);
                    else lvs->set_sel(ni,true);
                  }
                }
                lvs->m_selitem=ni;
                flag|=3;
              }
            }
            flag|=4;
          break;
          case VK_HOME:
          case VK_END:
            ni = wParam == VK_HOME ? 0 : lvs->GetNumItems()-1;
            if (ni != lvs->m_selitem)
            {
              if (lvs->m_is_multisel) 
              {
                if (!(GetAsyncKeyState(VK_SHIFT)&0x8000)) 
                {
                  lvs->clear_sel();
                  lvs->set_sel(ni,true);
                }
                else
                {
                  if (wParam == VK_HOME)
                  {
                    for (ni = lvs->m_selitem; ni >= 0; ni--) 
                      lvs->set_sel(ni,true);
                  }
                  else
                  {
                    for (int x=wdl_max(0,lvs->m_selitem); x <= ni; x++)
                      lvs->set_sel(x,true);
                  }
                }
              }
              lvs->m_selitem=ni;
              flag|=3;
            }
            flag|=4;
          break;
        }
        if (flag)
        {
          if (flag & 2)
          {
            if (lvs->m_is_listbox)
            {
              if (oldsel != lvs->m_selitem) 
                SendMessage(GetParent(hwnd),WM_COMMAND,(LBN_SELCHANGE<<16) | (hwnd->m_id&0xffff),(LPARAM)hwnd);
            } 
            else
            {
              NMLISTVIEW nm={{hwnd,hwnd->m_id,LVN_ITEMCHANGED},lvs->m_selitem,0,LVIS_SELECTED,};
              SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
            }
            ListView_EnsureVisible(hwnd,lvs->m_selitem,FALSE);
          }
          if (flag&1) InvalidateRect(hwnd,NULL,FALSE);

          return 0;
        }
      }
    break;
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT cr; 
          GetClientRect(hwnd,&cr); 
          HBRUSH bgbr = CreateSolidBrush(lvs->m_color_bg);
          FillRect(ps.hdc,&cr,bgbr);
          DeleteObject(bgbr);
          const bool focused = GetFocus() == hwnd;
          const int bgsel = lvs->m_color_extras[focused ? 0 : 2 ];
          bgbr=CreateSolidBrush(bgsel == -1 ? lvs->m_color_bg_sel : bgsel);
          if (lvs) 
          {
            TEXTMETRIC tm; 
            GetTextMetrics(ps.hdc,&tm);
            const int row_height = tm.tmHeight + 4;
            lvs->m_last_row_height = row_height;
            lvs->sanitizeScroll(hwnd);

            const bool owner_data = lvs->IsOwnerData();
            const int nrows = owner_data ? lvs->m_owner_data_size : lvs->m_data.GetSize();
            const int hdr_size = lvs->GetColumnHeaderHeight(hwnd);
            const int hdr_size_nomargin = hdr_size>0 ? hdr_size-LISTVIEW_HDR_YMARGIN : 0;
            int ypos = hdr_size - lvs->m_scroll_y;

            SetBkMode(ps.hdc,TRANSPARENT);
            const int ncols = lvs->m_cols.GetSize();
            const int nc = wdl_max(ncols,1);
            SWELL_ListView_Col *cols = lvs->m_cols.Get();

            const bool has_image = lvs->hasAnyImage();
            const bool has_status_image = lvs->hasStatusImage();
            const int xo = lvs->m_scroll_x;

            const int totalw = lvs->getTotalWidth();

            const bool vscroll_area = hdr_size + nrows * row_height > cr.bottom - g_swell_ctheme.scrollbar_width;
            const bool hscroll = totalw > cr.right - (vscroll_area ? g_swell_ctheme.scrollbar_width : 0);
            if (hscroll)
              cr.bottom -= g_swell_ctheme.scrollbar_width;

            HPEN gridpen = NULL;
            HGDIOBJ oldpen = NULL;
            if (!lvs->m_is_listbox && (hwnd->m_style & LVS_REPORT) && (lvs->m_extended_style&LVS_EX_GRIDLINES))
            {
              gridpen = CreatePen(PS_SOLID,0,lvs->m_color_grid);
              oldpen = SelectObject(ps.hdc,gridpen);
            }

            for (int rowidx = 0; rowidx < nrows && ypos < cr.bottom; rowidx ++)
            {
              const char *str = NULL;
              char buf[4096];

              bool sel;
              {
                RECT tr={cr.left,ypos,cr.right,ypos + row_height};
                if (tr.bottom < hdr_size) 
                {
                  ypos += row_height;
                  continue;
                }

                sel = lvs->get_sel(rowidx);
                if (sel)
                {
                  FillRect(ps.hdc,&tr,bgbr);
                }
              }

              if (sel) 
              {
                int c = lvs->m_color_extras[focused ? 1 : 3 ];
                SetTextColor(ps.hdc, c == -1 ? lvs->m_color_text_sel : c);
              }
              else SetTextColor(ps.hdc, lvs->m_color_text);

              SWELL_ListView_Row *row = lvs->m_data.Get(rowidx);
              int xpos=-xo;
              for (int col = 0; col < nc && xpos < cr.right; col ++)
              {
                int image_idx = 0;
                if (owner_data)
                {
                  NMLVDISPINFO nm={{hwnd,hwnd->m_id,LVN_GETDISPINFO},{LVIF_TEXT, rowidx,col, 0,0, buf, sizeof(buf), -1 }};
                  if (!col && has_image)
                  {
                    if (lvs->m_status_imagelist_type == LVSIL_STATE) nm.item.mask |= LVIF_STATE;
                    else if (lvs->m_status_imagelist_type == LVSIL_SMALL) nm.item.mask |= LVIF_IMAGE;

                  }
                  buf[0]=0;
                  SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
                  str=buf;
                  if (!col && has_image)
                  {
                    if (lvs->m_status_imagelist_type == LVSIL_STATE) image_idx=STATEIMAGEMASKTOINDEX(nm.item.state);
                    else if (lvs->m_status_imagelist_type == LVSIL_SMALL) image_idx = nm.item.iImage + 1;
                  }
                }
                else
                {
                  if (!col && has_image) image_idx = row->m_imageidx;
                  if (row) str = row->m_vals.Get(col);
                }

                RECT ar = { xpos,ypos, cr.right, ypos + row_height };
                if (!col && has_image)
                {
                  if (image_idx>0) 
                  {
                    HICON icon = lvs->m_status_imagelist->Get(image_idx-1);      
                    if (icon)
                    {
                      if (has_status_image || col >= ncols)
                        ar.right = ar.left + row_height;
                      else
                        ar.right = ar.left + wdl_min(row_height,cols[col].xwid);
                      DrawImageInRect(ps.hdc,icon,&ar);
                    }
                  }

                  if (has_status_image) 
                  {
                    xpos += row_height;
                  }
                  ar.left += row_height;
                }
  
                if (lvs->m_is_listbox && (hwnd->m_style & LBS_OWNERDRAWFIXED))
                {
                  if (hwnd->m_parent)
                  {
                    DRAWITEMSTRUCT dis = { ODT_LISTBOX, hwnd->m_id, (UINT)rowidx, 0, 
                      (UINT)(sel?ODS_SELECTED:0),hwnd,ps.hdc,ar,row?(DWORD_PTR)row->m_param:0 };
                    dis.rcItem.left++;
                    if (cr.bottom-cr.top < nrows*row_height)
                      dis.rcItem.right -= g_swell_ctheme.scrollbar_width;
                    SendMessage(hwnd->m_parent,WM_DRAWITEM,(WPARAM)hwnd->m_id,(LPARAM)&dis);
                  }
                }
                else
                {
                  if (ncols > 0)
                  {
                    ar.right = xpos + cols[col].xwid - SWELL_UI_SCALE(3);
                    xpos += cols[col].xwid;
                  }
                  else ar.right = cr.right;

                  if (ar.right > ar.left)
                  {
                    const int adj = (ar.right-ar.left)/16;
                    const int maxadj = SWELL_UI_SCALE(4);
                    ar.left += wdl_min(adj,maxadj);

                    if(str)
                      DrawText(ps.hdc,str,-1,&ar,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                  }
                }
              }
              ypos += row_height;
              if (gridpen)  
              {
                MoveToEx(ps.hdc,0,ypos-1,NULL);
                LineTo(ps.hdc,cr.right,ypos-1);
              }
            }
            if (gridpen)
            {
              if (row_height>0) for (;;)
              {
                ypos += row_height;
                if (ypos >= cr.bottom) break;
                MoveToEx(ps.hdc,0,ypos-1,NULL);
                LineTo(ps.hdc,cr.right,ypos-1);
              }
              int xpos=(has_status_image ? row_height : 0) - xo;
              for (int col=0; col < ncols; col ++)
              {
                xpos += cols[col].xwid;
                if (xpos > cr.right) break;
                MoveToEx(ps.hdc,xpos-1,hdr_size_nomargin,NULL);
                LineTo(ps.hdc,xpos-1,cr.bottom);
              }
            }
            if (hdr_size_nomargin>0)
            {
              HBRUSH br = CreateSolidBrush(g_swell_ctheme.listview_hdr_bg);
              int xpos=(has_status_image ? row_height : 0) - xo;
              ypos=0;
              SetTextColor(ps.hdc,g_swell_ctheme.listview_hdr_text);

              if (xpos>0) 
              {
                RECT tr={0,ypos,xpos,ypos+hdr_size_nomargin };
                FillRect(ps.hdc,&tr,br);
              }
              for (int col=0; col < ncols; col ++)
              {
                RECT tr={xpos,ypos,0,ypos + hdr_size_nomargin };
                xpos += cols[col].xwid;
                tr.right = xpos;
               
                if (tr.right > tr.left) 
                {
                  Draw3DBox(ps.hdc,&tr, 
                      g_swell_ctheme.listview_hdr_bg,
                      g_swell_ctheme.listview_hdr_hilight,
                      g_swell_ctheme.listview_hdr_shadow);

                  if (cols[col].sortindicator != 0)
                  {
                    const int tsz = (tr.bottom-tr.top)/4;
                    if (tr.right > tr.left + 2*tsz)
                    {
                      const int x1 = tr.left + 2;
                      int y2 = (tr.bottom+tr.top)/2 - tsz/2 - tsz/4;
                      int y1 = y2 + tsz;
                      if (cols[col].sortindicator >= 0)
                      {
                        int tmp=y1; 
                        y1=y2;
                        y2=tmp;
                      }
                      HBRUSH hdrbr = CreateSolidBrush(g_swell_ctheme.listview_hdr_arrow);
                      HGDIOBJ oldBrush = SelectObject(ps.hdc,hdrbr);
                      HGDIOBJ oldPen = SelectObject(ps.hdc,GetStockObject(NULL_PEN));

                      POINT pts[3] = {{x1,y1}, {x1+tsz*2,y1}, {x1+tsz,y2}};
                      Polygon(ps.hdc,pts,3);
                      SelectObject(ps.hdc,oldBrush);
                      SelectObject(ps.hdc,oldPen);
                      DeleteObject(hdrbr);
                      tr.left = x1 + tsz*2;
                    }
                  }

                  if (cols[col].name) 
                  {
                    tr.left += wdl_min((tr.right-tr.left)/4,4);
                    DrawText(ps.hdc,cols[col].name,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                  }
                }
                if (xpos >= cr.right) break;
              }
              if (xpos < cr.right)
              {
                RECT tr={xpos,ypos,cr.right,ypos+hdr_size_nomargin };
                FillRect(ps.hdc,&tr,br);
              }
              DeleteObject(br);
            }
            if (gridpen) 
            {
              SelectObject(ps.hdc,oldpen);
              DeleteObject(gridpen);
            }

            cr.top += hdr_size_nomargin;
            drawVerticalScrollbar(ps.hdc,cr,nrows*row_height,lvs->m_scroll_y);

            if (hscroll)
            {
              cr.bottom += g_swell_ctheme.scrollbar_width;
              drawHorizontalScrollbar(ps.hdc,cr,
                  cr.right-cr.left - (vscroll_area ? g_swell_ctheme.scrollbar_width : 0),
                  totalw,lvs->m_scroll_x);
            }
          }
          DeleteObject(bgbr);

          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_NCDESTROY:
      hwnd->m_private_data = 0;
      delete lvs;
    break;
    case LB_ADDSTRING:
      if (lvs && !lvs->IsOwnerData())
      {
         // todo: optional sort
        int rv=lvs->m_data.GetSize();
        SWELL_ListView_Row *row=new SWELL_ListView_Row;
        row->m_vals.Add(strdup((const char *)lParam));
        lvs->m_data.Add(row); 
        InvalidateRect(hwnd,NULL,FALSE);
        return rv;
      }
    return LB_ERR;
     
    case LB_INSERTSTRING:
      if (lvs && !lvs->IsOwnerData())
      {
        int idx =  (int) wParam;
        if (idx<0 || idx>lvs->m_data.GetSize()) idx=lvs->m_data.GetSize();
        SWELL_ListView_Row *row=new SWELL_ListView_Row;
        row->m_vals.Add(strdup((const char *)lParam));
        lvs->m_data.Insert(idx,row); 
        InvalidateRect(hwnd,NULL,FALSE);
        return idx;
      }
    return LB_ERR;
    case LB_DELETESTRING:
      if (lvs && !lvs->IsOwnerData())
      {
        int idx =  (int) wParam;
        if (idx<0 || idx>=lvs->m_data.GetSize()) return LB_ERR;
        lvs->m_data.Delete(idx,true);
        InvalidateRect(hwnd,NULL,FALSE);
        return lvs->m_data.GetSize();
      }
    return LB_ERR;
    case LB_GETTEXT:
      if (!lParam) return LB_ERR;
      *(char *)lParam = 0;
      if (lvs && !lvs->IsOwnerData())
      {
        SWELL_ListView_Row *row = lvs->m_data.Get(wParam);
        if (row && row->m_vals.Get(0))
        {
          strcpy((char *)lParam, row->m_vals.Get(0));
          return (LRESULT)strlen(row->m_vals.Get(0));
        }
      }
    return LB_ERR;
    case LB_GETTEXTLEN:
        {
          SWELL_ListView_Row *row=lvs->m_data.Get(wParam);
          if (row) 
          {
            const char *p=row->m_vals.Get(0);
            return p?strlen(p):0;
          }
        }
    return LB_ERR;
    case LB_RESETCONTENT:
      if (lvs && !lvs->IsOwnerData())
      {
        lvs->m_data.Empty(true,free);
      }
      InvalidateRect(hwnd,NULL,FALSE);
    return 0;
    case LB_SETSEL:
      if (lvs && lvs->m_is_multisel)
      {
        if (lvs->IsOwnerData())
        {
        }
        else
        {
          if ((int)lParam == -1)
          {
            int x;
            const int n=lvs->m_data.GetSize();
            for(x=0;x<n;x++) 
            {
              SWELL_ListView_Row *row=lvs->m_data.Get(x);
              if (row) row->m_tmp = (row->m_tmp&~1) | (wParam?1:0);
            }
            InvalidateRect(hwnd,NULL,FALSE);
          }
          else
          {
            SWELL_ListView_Row *row=lvs->m_data.Get((int)lParam);
            if (!row) return LB_ERR;
            const int otmp = row->m_tmp;
            row->m_tmp = (row->m_tmp&~1) | (wParam?1:0);
            if (row->m_tmp != otmp) InvalidateRect(hwnd,NULL,FALSE);
            return 0;
          }
        }
      }
    return LB_ERR;
    case LB_SETCURSEL:
      if (lvs && !lvs->IsOwnerData() && !lvs->m_is_multisel)
      {
        lvs->m_selitem = (int)wParam;
        InvalidateRect(hwnd,NULL,FALSE);
      }
    return LB_ERR;
    case LB_GETSEL:
      if (lvs && lvs->m_is_multisel)
      {
        if (lvs->IsOwnerData())
        {
        }
        else
        {
          SWELL_ListView_Row *row=lvs->m_data.Get((int)wParam);
          if (!row) return LB_ERR;
          return row->m_tmp&1;
        }
      }
    return LB_ERR;
    case LB_GETCURSEL:
      if (lvs)
      {
        return (LRESULT)lvs->m_selitem;
      }
    return LB_ERR;
    case LB_GETCOUNT:
      if (lvs) return lvs->GetNumItems();
    return LB_ERR;
    case LB_GETSELCOUNT:
      if (lvs && lvs->m_is_multisel)
      {
        int cnt=0;
        if (lvs->IsOwnerData())
        {
        }
        else
        {
          int x;
          const int n=lvs->m_data.GetSize();
          for(x=0;x<n;x++) 
          {
            SWELL_ListView_Row *row=lvs->m_data.Get(x);
            if (row && (row->m_tmp&1)) cnt++;
          }
        }
        return cnt;
      }
    return LB_ERR;
    case LB_GETITEMDATA:
      if (lvs && !lvs->IsOwnerData())
      {
        SWELL_ListView_Row *row = lvs->m_data.Get(wParam);
        return row ? row->m_param : LB_ERR;
      }
    return LB_ERR;
    case LB_SETITEMDATA:
      if (lvs && !lvs->IsOwnerData())
      {
        SWELL_ListView_Row *row = lvs->m_data.Get(wParam);
        if (row) row->m_param = lParam;
        return row ? 0 : LB_ERR;
      }
    return LB_ERR;
  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}

struct treeViewState 
{
  treeViewState() 
  { 
    m_sel=NULL;
    m_last_row_height=0;
    m_scroll_x=m_scroll_y=m_capmode=0;
    m_root.m_state = TVIS_EXPANDED;
    m_root.m_haschildren=true;
  }
  ~treeViewState() 
  {
  }
  bool findItem(HTREEITEM item, HTREEITEM *parOut, int *idxOut)
  {
    if (!m_root.FindItem(item,parOut,idxOut)) return false;
    if (parOut && *parOut == &m_root) *parOut = NULL;
    return true;
  }

  int navigateSel(int key, int pagesize) // returns 2 force invalidate, 1 if ate key 
  {
    HTREEITEM par=NULL;
    int idx=0,tmp=1;

    switch (key)
    {
      case VK_LEFT:
        if (m_sel && findItem(m_sel,&par,NULL))
        {
          if (m_sel->m_haschildren && (m_sel->m_state & TVIS_EXPANDED))
          {
            m_sel->m_state &= ~TVIS_EXPANDED;
            return 2;
          }
          if (par) m_sel=par;
        }
      return 1;
      case VK_HOME:
        m_sel=m_root.m_children.Get(0);
      return 1;
      case VK_END:
        par = &m_root;
        while (par && par->m_haschildren && 
               par->m_children.GetSize() && (par->m_state & TVIS_EXPANDED))
          par = par->m_children.Get(par->m_children.GetSize()-1);
        if (par && par != &m_root) m_sel=par;
      return 1;
      case VK_RIGHT:
        if (m_sel && findItem(m_sel,NULL,NULL) && m_sel->m_haschildren)
        {
          if (!(m_sel->m_state&TVIS_EXPANDED))
          {
            m_sel->m_state |= TVIS_EXPANDED;
            return 2;
          }
          par = m_sel->m_children.Get(0);
          if (par) m_sel=par;
        }
      return 1;
      case VK_PRIOR:
        tmp = wdl_max(pagesize,2) - 1;
      case VK_UP:
        while (tmp-- > 0)
        {
          if (m_sel && findItem(m_sel,&par,&idx))
          {
            if (idx>0)
            {
              par = (par?par:&m_root)->m_children.Get(idx-1);
              while (par && (par->m_state & TVIS_EXPANDED) && 
                     par->m_haschildren && par->m_children.GetSize())
                par = par->m_children.Get(par->m_children.GetSize()-1);
            }
            if (par) m_sel=par;
          }
        }
      return 1;
      case VK_NEXT:
        tmp = wdl_max(pagesize,2) - 1;
      case VK_DOWN:
        while (tmp-- > 0)
        {
          if (m_sel && findItem(m_sel,&par,&idx))
          {
            if (m_sel->m_haschildren && 
                m_sel->m_children.GetSize() &&
                (m_sel->m_state & TVIS_EXPANDED))
            {
              par = m_sel->m_children.Get(0);
              if (par) m_sel=par;
              continue;
            }

next_item_in_parent:
            if (par)
            {
              if (idx+1 < par->m_children.GetSize()) 
              {
                par = par->m_children.Get(idx+1);
                if (par) m_sel=par;
              }
              else if (findItem(par,&par,&idx)) goto next_item_in_parent;
            }
            else
            {
              par = m_root.m_children.Get(idx+1);
              if (par) m_sel=par;
            }
          }
        }
      return 1;
    }
    return 0;
  }

  void doDrawItem(HTREEITEM item, HDC hdc, RECT *rect) // draws any subitems too, updates rect->top
  {
#ifdef SWELL_LICE_GDI
    if (!item) return;

    if (item != &m_root)
    {
      const int ob = rect->bottom;
      rect->bottom = rect->top + m_last_row_height;
      if (rect->right > rect->left)
      {
        int oc=0;
        if (item == m_sel) 
        {
          SetBkMode(hdc,OPAQUE);
          SetBkColor(hdc,g_swell_ctheme.treeview_bg_sel);
          oc = GetTextColor(hdc);
          SetTextColor(hdc,g_swell_ctheme.treeview_text_sel);
        }
       
        RECT dr = *rect;
        const int sz = m_last_row_height/4;
        if (item->m_haschildren)
        {
          bool exp = (item->m_state&TVIS_EXPANDED);
          POINT pts[3];
          if (exp)
          {
            const int yo = dr.top + sz+sz/2,xo=dr.left+1;
            pts[0].x=xo; pts[0].y=yo;
            pts[1].x=xo+sz*2; pts[1].y=yo;
            pts[2].x=xo+sz; pts[2].y=yo+sz;
          }
          else
          {
            const int yo = dr.top + sz, xo = dr.left+sz*3/4+1;
            pts[0].x=xo; pts[0].y=yo;
            pts[1].x=xo+sz; pts[1].y=yo+sz;
            pts[2].x=xo; pts[2].y=yo+sz*2;
          }
          Polygon(hdc,pts,3);
        }
        dr.left += sz*2+3;

        DrawText(hdc,item->m_value ? item->m_value : "",-1,&dr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        if (item == m_sel) 
        {
          SetBkMode(hdc,TRANSPARENT);
          SetTextColor(hdc,oc);
        }
      }
      rect->top = rect->bottom;
      rect->bottom = ob;
    }

    if ((item->m_state & TVIS_EXPANDED) && item->m_haschildren && item->m_children.GetSize())
    {
      const int n = item->m_children.GetSize();
      rect->left += m_last_row_height;
      for (int x=0;x<n && rect->top < rect->bottom;x++)
      {
        doDrawItem(item->m_children.Get(x),hdc,rect);
      }
      rect->left -= m_last_row_height;
    } 
#endif
  }
  HTREEITEM hitTestItem(HTREEITEM item, int *y, int *xo) 
  {
    *y -= m_last_row_height;
    if (*y < 0) return item;
    if ((item->m_state & TVIS_EXPANDED) && item->m_haschildren && item->m_children.GetSize())
    {
      int x;
      const int n = item->m_children.GetSize();
      for (x=0;x<n;x++)
      {
        HTREEITEM t=hitTestItem(item->m_children.Get(x),y,xo);
        if (t) 
        {
          if (xo) *xo += m_last_row_height;
          return t;
        }
      }
    } 
    return NULL;
  }
  int CalculateItemHeight(HTREEITEM__ *item, HTREEITEM stopAt, bool *done)
  {
    int h = m_last_row_height;
    if (item == stopAt) { *done=true; return 0; }

    if ((item->m_state & TVIS_EXPANDED) && 
        item->m_haschildren && 
        item->m_children.GetSize())
    {
      const int n = item->m_children.GetSize();
      for (int x=0;x<n;x++) 
      {
        h += CalculateItemHeight(item->m_children.Get(x),stopAt,done);
        if (*done) break;
      }
    }
    return h;
  }

  int calculateContentsHeight(HTREEITEM item=NULL) 
  { 
    bool done=false;
    const int rv = CalculateItemHeight(&m_root,item,&done);
    if (item && !done) return 0;
    return rv - m_last_row_height; 
  }

  int sanitizeScroll(HWND h)
  {
    RECT r;
    GetClientRect(h,&r);
    if (m_last_row_height > 0)
    {
      const int vh = calculateContentsHeight();
      if (m_scroll_y < 0 || vh <= r.bottom) m_scroll_y=0;
      else if (m_scroll_y > vh - r.bottom) m_scroll_y = vh - r.bottom;
      return vh;
    }
    return 0;
  }

  void ensureItemVisible(HWND hwnd, HTREEITEM item)
  {
    if (m_last_row_height<1) return;
    const int x = item ? calculateContentsHeight(item) : 0;
    RECT r;
    GetClientRect(hwnd,&r);
    if (x < m_scroll_y) m_scroll_y = x;
    else if (x+m_last_row_height > m_scroll_y+r.bottom) 
      m_scroll_y = x+m_last_row_height - r.bottom;
  }

  HTREEITEM__ m_root;
  HTREEITEM m_sel;
  int m_last_row_height;
  int m_scroll_x,m_scroll_y,m_capmode;

};

static LRESULT treeViewWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  treeViewState *tvs = (treeViewState *)hwnd->m_private_data;
  switch (msg)
  {
    case WM_MOUSEWHEEL:
      if ((GetAsyncKeyState(VK_CONTROL)&0x8000) || (GetAsyncKeyState(VK_MENU)&0x8000)) break; // pass modified mousewheel to parent

      {
        const int amt = ((short)HIWORD(wParam))/40;
        if (amt && tvs)
        {
          const int oldscroll = tvs->m_scroll_y;
          tvs->m_scroll_y -= amt*tvs->m_last_row_height;
          tvs->sanitizeScroll(hwnd);
          if (tvs->m_scroll_y != oldscroll)
            InvalidateRect(hwnd,NULL,FALSE);

        }
      }
    return 1;
    case WM_KEYDOWN:
      if (tvs && (lParam & FVIRTKEY)) 
      {
        HTREEITEM oldSel = tvs->m_sel;
        RECT r;
        GetClientRect(hwnd,&r);
        int flag = tvs->navigateSel((int)wParam,tvs->m_last_row_height ? r.bottom / tvs->m_last_row_height : 4); 
        if (oldSel != tvs->m_sel)
        {
          if (tvs->m_sel) tvs->ensureItemVisible(hwnd,tvs->m_sel);
          InvalidateRect(hwnd,NULL,FALSE);
          NMTREEVIEW nm={{(HWND)hwnd,(UINT_PTR)hwnd->m_id,TVN_SELCHANGED},};
          SendMessage(GetParent(hwnd),WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
        }
        else if (flag&2) InvalidateRect(hwnd,NULL,FALSE);

        if (flag) return 0;
      }
    break;
    case WM_LBUTTONDOWN:
      SetFocusInternal(hwnd);
      SetCapture(hwnd);
      if (tvs)
      {
        tvs->m_capmode=0;
        RECT cr;
        GetClientRect(hwnd,&cr);
        int total_h;
        if (GET_X_LPARAM(lParam) >= cr.right-g_swell_ctheme.scrollbar_width && 
             (total_h=tvs->sanitizeScroll(hwnd)) > cr.bottom)
        {
          int ypos = GET_Y_LPARAM(lParam);
          int yp = ypos;

          int thumbsz, thumbpos;
          calcScroll(cr.bottom, total_h,tvs->m_scroll_y,&thumbsz,&thumbpos);

          if (ypos < thumbpos) yp = thumbpos; // jump on first mouse move
          else if (ypos > thumbpos+thumbsz) yp = thumbpos + thumbsz;

          tvs->m_capmode = (1<<16) | (yp&0xffff); 
          if (ypos < thumbpos || ypos > thumbpos+thumbsz) goto forceMouseMove;
          return 0;
        }

        if (tvs->m_last_row_height)
        {
          int y = GET_Y_LPARAM(lParam) + tvs->m_scroll_y + tvs->m_last_row_height;
          int xo=-tvs->m_last_row_height;
          HTREEITEM hit = tvs->hitTestItem(&tvs->m_root,&y,&xo);
          if (hit && GET_X_LPARAM(lParam) >= xo) 
          {
            if (hit->m_haschildren)
            {
              if (GET_X_LPARAM(lParam) < xo + (tvs->m_last_row_height/4)*2+3)
              {
                hit->m_state ^= TVIS_EXPANDED;
                InvalidateRect(hwnd,NULL,FALSE);
                return 0;
              }
            }
            if (tvs->m_sel != hit)
            {
              tvs->m_sel = hit;
              InvalidateRect(hwnd,NULL,FALSE);
              NMTREEVIEW nm={{(HWND)hwnd,(UINT_PTR)hwnd->m_id,TVN_SELCHANGED},};
              SendMessage(GetParent(hwnd),WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
            }
          }
        }
      }
    return 0;
    case WM_MOUSEMOVE:
      if (GetCapture()==hwnd && tvs)
      {
forceMouseMove:
        switch (HIWORD(tvs->m_capmode))
        {
          case 1:
            {
              int yv = (short)LOWORD(tvs->m_capmode);
              int amt = GET_Y_LPARAM(lParam) - yv;

              if (amt)
              {
                RECT r;
                GetClientRect(hwnd,&r);

                const int viewsz = r.bottom;
                const double totalsz=tvs->calculateContentsHeight();
                amt = (int)floor(amt * totalsz / (double)viewsz + 0.5);
              
                const int oldscroll = tvs->m_scroll_y;
                tvs->m_scroll_y += amt;
                tvs->sanitizeScroll(hwnd);
                if (tvs->m_scroll_y != oldscroll)
                {
                  tvs->m_capmode = (GET_Y_LPARAM(lParam)&0xffff) | (1<<16);
                  InvalidateRect(hwnd,NULL,FALSE);
                }
              }
            }
          break;
        }
      }
    return 1;
    case WM_LBUTTONUP:
      if (GetCapture() == hwnd)
      {
        ReleaseCapture();
      }
    return 1;
    case WM_RBUTTONDOWN:
      if (tvs && tvs->m_last_row_height>0)
      {
        NMLISTVIEW nm={{hwnd,hwnd->m_id,NM_RCLICK},0,0,0,};
        SendMessage(GetParent(hwnd),WM_NOTIFY,hwnd->m_id,(LPARAM)&nm);
      }
    return 1;
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 
          {
            HBRUSH br = CreateSolidBrush(g_swell_ctheme.treeview_bg);
            FillRect(ps.hdc,&r,br);
            DeleteObject(br);
          }
          if (tvs)
          {
            RECT cr=r;
            SetTextColor(ps.hdc,g_swell_ctheme.treeview_text);

            const int lrh = tvs->m_last_row_height;
            TEXTMETRIC tm; 
            GetTextMetrics(ps.hdc,&tm);
            const int row_height = tm.tmHeight;
            tvs->m_last_row_height = row_height;
            const int total_h = tvs->sanitizeScroll(hwnd);
            if (!lrh && tvs->m_sel) 
              tvs->ensureItemVisible(hwnd,tvs->m_sel);

            SetBkMode(ps.hdc,TRANSPARENT);

            r.top -= tvs->m_scroll_y;

            HBRUSH br = CreateSolidBrush(g_swell_ctheme.treeview_arrow);
            HGDIOBJ oldpen = SelectObject(ps.hdc,GetStockObject(NULL_PEN));
            HGDIOBJ oldbr = SelectObject(ps.hdc,br);

            r.left -= tvs->m_last_row_height;
            tvs->doDrawItem(&tvs->m_root,ps.hdc,&r);

            SelectObject(ps.hdc,oldbr);
            SelectObject(ps.hdc,oldpen);
            DeleteObject(br);

            drawVerticalScrollbar(ps.hdc,cr,total_h,tvs->m_scroll_y);
          }

          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_NCDESTROY:
      hwnd->m_private_data = 0;
      delete tvs;
    break;
  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}

struct tabControlState
{
  tabControlState() { m_curtab=0; }
  ~tabControlState() { m_tabs.Empty(true,free); }
  int m_curtab;
  WDL_PtrList<char> m_tabs;
};

#define TABCONTROL_HEIGHT SWELL_UI_SCALE(20)

static LRESULT tabControlWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  const int xdiv = 6,xpad=4;

  tabControlState *s = (tabControlState *)hwnd->m_private_data;
  switch (msg)
  {
    case WM_NCDESTROY:
      hwnd->m_private_data = 0;
      delete s;
    break;
    case WM_TIMER:
      if (wParam==1)
      {
        if (!fast_has_focus(hwnd))
        {
          KillTimer(hwnd,1);
          InvalidateRect(hwnd,NULL,FALSE);
        }
      }
    break;
    case WM_LBUTTONUP:
      if (GET_Y_LPARAM(lParam) < TABCONTROL_HEIGHT)
      {
        return 1;
      }
    break;
    case WM_KEYDOWN:
      if (lParam==FVIRTKEY && 
           (wParam==VK_LEFT || 
            wParam==VK_RIGHT || 
            wParam==VK_HOME || 
            wParam == VK_END))
      {
        int ct = s->m_curtab;
        if (wParam==VK_LEFT)ct--;
        else ct++;
        if (ct>=s->m_tabs.GetSize()||wParam == VK_END) ct=s->m_tabs.GetSize()-1;
        if (ct<0||wParam==VK_HOME) ct=0;
        if (ct != s->m_curtab)
        {
          s->m_curtab = ct;
          NMHDR nm={hwnd,(UINT_PTR)hwnd->m_id,TCN_SELCHANGE};
          InvalidateRect(hwnd,NULL,FALSE);
          SendMessage(GetParent(hwnd),WM_NOTIFY,nm.idFrom,(LPARAM)&nm);
        }

        return 0;
      }
    break;
    case WM_LBUTTONDOWN:
      if (GET_Y_LPARAM(lParam) < TABCONTROL_HEIGHT)
      {
        SetFocusInternal(hwnd);
        int xp=GET_X_LPARAM(lParam),tab;
        HDC dc = GetDC(hwnd);
        int tabchg = -1;
        for (tab = 0; tab < s->m_tabs.GetSize(); tab ++)
        {
          const char *buf = s->m_tabs.Get(tab);
          RECT tr={0,};
          DrawText(dc,buf,-1,&tr,DT_CALCRECT|DT_NOPREFIX|DT_SINGLELINE);
          xp -= tr.right - tr.left + 2*SWELL_UI_SCALE(xpad) + SWELL_UI_SCALE(xdiv);
          if (xp < 0)
          {
            if (s->m_curtab != tab)
            {
              tabchg = tab;
            }
            break;
          }
        }
        if (tabchg >=0)
        {
          s->m_curtab = tabchg;
          NMHDR nm={hwnd,(UINT_PTR)hwnd->m_id,TCN_SELCHANGE};
          InvalidateRect(hwnd,NULL,FALSE);
          SendMessage(GetParent(hwnd),WM_NOTIFY,nm.idFrom,(LPARAM)&nm);
        }
        else
          InvalidateRect(hwnd,NULL,FALSE);
 
        ReleaseDC(hwnd,dc);
        return 1;
      }
    break;
    case WM_PAINT:
      { 
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r; 
          GetClientRect(hwnd,&r); 

          int tab;
          int xp=0;
          HPEN pen = CreatePen(PS_SOLID,0,g_swell_ctheme.tab_hilight);
          HPEN pen2 = CreatePen(PS_SOLID,0,g_swell_ctheme.tab_shadow);

          SetBkMode(ps.hdc,TRANSPARENT);
          SetTextColor(ps.hdc,g_swell_ctheme.tab_text);
          HGDIOBJ oldPen=SelectObject(ps.hdc,pen);
          const int th = TABCONTROL_HEIGHT;

          {
            RECT bgr={0,0,r.right,th};
            IntersectRect(&bgr,&bgr,&ps.rcPaint);
            HBRUSH hbrush = (HBRUSH) SendMessage(hwnd,WM_CTLCOLORDLG,(WPARAM)ps.hdc,(LPARAM)hwnd);
            if (hbrush && hbrush != (HBRUSH)1) FillRect(ps.hdc,&bgr,hbrush);
            else SWELL_FillDialogBackground(ps.hdc,&bgr,0);
          }

          int lx=0;
          RECT fr={0,};
          for (tab = 0; tab < s->m_tabs.GetSize() && xp < r.right; tab ++)
          {
            const char *buf = s->m_tabs.Get(tab);
            RECT tr={0,};
            DrawText(ps.hdc,buf,-1,&tr,DT_CALCRECT|DT_NOPREFIX|DT_SINGLELINE);
            int tw=tr.right-tr.left + 2*SWELL_UI_SCALE(xpad);

            const int olx=lx;
            lx=xp + tw+SWELL_UI_SCALE(xdiv);
 
            MoveToEx(ps.hdc,xp,th-1,NULL);
            LineTo(ps.hdc,xp,0);
            LineTo(ps.hdc,xp+tw,0);
            SelectObject(ps.hdc,pen2);
            LineTo(ps.hdc,xp+tw,th-1);

            if (tab==s->m_curtab)
            {
              fr.left=xp;
              fr.right=xp+tw;
              fr.top=0;
              fr.bottom=th-2;
            }

            MoveToEx(ps.hdc, tab == s->m_curtab ? lx-SWELL_UI_SCALE(xdiv) : olx,th-1,NULL);
            LineTo(ps.hdc,lx,th-1);

            SelectObject(ps.hdc,pen);

            tr.left = xp+SWELL_UI_SCALE(xpad);
            tr.top=0;
            tr.right = xp+tw-SWELL_UI_SCALE(xpad);
            tr.bottom = th;

            DrawText(ps.hdc,buf,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            xp = lx;
          }
          if (draw_focus_indicator(hwnd,ps.hdc,&fr))
          {
            KillTimer(hwnd,1);
            SetTimer(hwnd,1,100,NULL);
          }
          SelectObject(ps.hdc,pen2);
          MoveToEx(ps.hdc,lx,th-1,NULL);
          LineTo(ps.hdc,r.right,th-1);

          SelectObject(ps.hdc,oldPen);

          EndPaint(hwnd,&ps);
          DeleteObject(pen);
          DeleteObject(pen2);
        }
      }
      return 0;
  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}



HWND SWELL_MakeListBox(int idx, int x, int y, int w, int h, int styles)
{
  RECT tr=MakeCoords(x,y,w,h,true);
  HWND hwnd = new HWND__(m_make_owner,idx,&tr,NULL, !(styles&SWELL_NOT_WS_VISIBLE), listViewWindowProc);
  hwnd->m_style = WS_CHILD | (styles & ~SWELL_NOT_WS_VISIBLE);
  hwnd->m_classname = "ListBox";
  hwnd->m_private_data = (INT_PTR) new listViewState(false, !!(styles & LBS_EXTENDEDSEL), true);
  hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
  if (m_doautoright) UpdateAutoCoords(tr);
  return hwnd;
}

typedef struct ccprocrec
{
  SWELL_ControlCreatorProc proc;
  int cnt;
  struct ccprocrec *next;
} ccprocrec;

static ccprocrec *m_ccprocs;

void SWELL_RegisterCustomControlCreator(SWELL_ControlCreatorProc proc)
{
  if (!proc) return;
  
  ccprocrec *p=m_ccprocs;
  while (p && p->next)
  {
    if (p->proc == proc)
    {
      p->cnt++;
      return;
    }
    p=p->next;
  }
  ccprocrec *ent = (ccprocrec*)malloc(sizeof(ccprocrec));
  ent->proc=proc;
  ent->cnt=1;
  ent->next=0;
  
  if (p) p->next=ent;
  else m_ccprocs=ent;
}

void SWELL_UnregisterCustomControlCreator(SWELL_ControlCreatorProc proc)
{
  if (!proc) return;
  
  ccprocrec *lp=NULL;
  ccprocrec *p=m_ccprocs;
  while (p)
  {
    if (p->proc == proc)
    {
      if (--p->cnt <= 0)
      {
        if (lp) lp->next=p->next;
        else m_ccprocs=p->next;
        free(p);
      }
      return;
    }
    lp=p;
    p=p->next;
  }
}



HWND SWELL_MakeControl(const char *cname, int idx, const char *classname, int style, int x, int y, int w, int h, int exstyle)
{
  if (m_ccprocs)
  {
    RECT poo=MakeCoords(x,y,w,h,false);
    ccprocrec *p=m_ccprocs;
    while (p)
    {
      HWND hhh=p->proc((HWND)m_make_owner,cname,idx,classname,style,poo.left,poo.top,poo.right-poo.left,poo.bottom-poo.top);
      if (hhh) 
      {
        if (exstyle) SetWindowLong(hhh,GWL_EXSTYLE,exstyle);
        return hhh;
      }
      p=p->next;
    }
  }
  if (!stricmp(classname,"SysTabControl32"))
  {
    RECT tr=MakeCoords(x,y,w,h,false);
    HWND hwnd = new HWND__(m_make_owner,idx,&tr,NULL, !(style&SWELL_NOT_WS_VISIBLE), tabControlWindowProc);
    hwnd->m_style = WS_CHILD | (style & ~SWELL_NOT_WS_VISIBLE);
    hwnd->m_classname = "SysTabControl32";
    hwnd->m_private_data = (INT_PTR) new tabControlState;
    hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
    SetWindowPos(hwnd,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE); 
    return hwnd;
  }
  else if (!stricmp(classname, "SysListView32")||!stricmp(classname, "SysListView32_LB"))
  {
    RECT tr=MakeCoords(x,y,w,h,false);
    HWND hwnd = new HWND__(m_make_owner,idx,&tr,NULL, !(style&SWELL_NOT_WS_VISIBLE), listViewWindowProc);
    hwnd->m_style = WS_CHILD | (style & ~SWELL_NOT_WS_VISIBLE);
    hwnd->m_classname = "SysListView32";
    if (!stricmp(classname, "SysListView32"))
      hwnd->m_private_data = (INT_PTR) new listViewState(!!(style & LVS_OWNERDATA), !(style & LVS_SINGLESEL), false);
    else
      hwnd->m_private_data = (INT_PTR) new listViewState(false,false, true);

    hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
    return hwnd;
  }
  else if (!stricmp(classname, "SysTreeView32"))
  {
    RECT tr=MakeCoords(x,y,w,h,false);
    HWND hwnd = new HWND__(m_make_owner,idx,&tr,NULL, !(style&SWELL_NOT_WS_VISIBLE), treeViewWindowProc);
    hwnd->m_style = WS_CHILD | (style & ~SWELL_NOT_WS_VISIBLE);
    hwnd->m_classname = "SysTreeView32";
    hwnd->m_private_data = (INT_PTR) new treeViewState;
    hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
    return hwnd;
  }
  else if (!stricmp(classname, "msctls_progress32"))
  {
    RECT tr=MakeCoords(x,y,w,h,false);
    HWND hwnd = new HWND__(m_make_owner,idx,&tr,NULL, !(style&SWELL_NOT_WS_VISIBLE), progressWindowProc);
    hwnd->m_wantfocus = false;
    hwnd->m_style = WS_CHILD | (style & ~SWELL_NOT_WS_VISIBLE);
    hwnd->m_classname = "msctls_progress32";
    int *state = (int *)calloc(2,sizeof(int)); // pos, range
    if (state) state[1] = 100<<16;
    hwnd->m_private_data = (INT_PTR) state;
    hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
    return hwnd;
  }
  else if (!stricmp(classname,"Edit"))
  {
    return SWELL_MakeEditField(idx,x,y,w,h,style);
  }
  else if (!stricmp(classname, "static"))
  {
    RECT tr=MakeCoords(x,y,w,h,false);
    HWND hwnd = new HWND__(m_make_owner,idx,&tr,cname, !(style&SWELL_NOT_WS_VISIBLE),labelWindowProc);
    hwnd->m_wantfocus = false;
    hwnd->m_style = WS_CHILD | (style & ~SWELL_NOT_WS_VISIBLE);
    hwnd->m_classname = "static";
    hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
    if (m_doautoright) UpdateAutoCoords(tr);
    return hwnd;
  }
  else if (!stricmp(classname,"Button"))
  {
    RECT tr=MakeCoords(x,y,w,h,true);
    HWND hwnd = swell_makeButton(m_make_owner,idx,&tr,cname,!(style&SWELL_NOT_WS_VISIBLE),(style&~SWELL_NOT_WS_VISIBLE)|WS_CHILD);
    if (m_doautoright) UpdateAutoCoords(tr);
    return hwnd;
  }
  else if (!stricmp(classname,"REAPERhfader")||!stricmp(classname,"msctls_trackbar32"))
  {
    RECT tr=MakeCoords(x,y,w,h,true);
    HWND hwnd = new HWND__(m_make_owner,idx,&tr,cname, !(style&SWELL_NOT_WS_VISIBLE),trackbarWindowProc);
    hwnd->m_style = WS_CHILD | (style & ~SWELL_NOT_WS_VISIBLE);
    hwnd->m_classname = !stricmp(classname,"REAPERhfader") ? "REAPERhfader" : "msctls_trackbar32";
    hwnd->m_private_data = (INT_PTR) calloc(3,sizeof(int)); // pos, range, tic
    hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
    return hwnd;
  }
  else if (!stricmp(classname,"COMBOBOX"))
  {
    return SWELL_MakeCombo(idx, x, y, w, h, style);
  }
  return 0;
}

HWND SWELL_MakeCombo(int idx, int x, int y, int w, int h, int flags)
{
  RECT tr=MakeCoords(x,y,w,h,true);
  const int maxh = g_swell_ctheme.combo_height;
  if (tr.bottom > tr.top + maxh) tr.bottom=tr.top+maxh;
  HWND hwnd = new HWND__(m_make_owner,idx,&tr,NULL, !(flags&SWELL_NOT_WS_VISIBLE),comboWindowProc);
  hwnd->m_private_data = (INT_PTR) new __SWELL_ComboBoxInternalState;
  hwnd->m_style = (flags & ~SWELL_NOT_WS_VISIBLE)|WS_CHILD;
  hwnd->m_classname = "combobox";
  hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
  if (m_doautoright) UpdateAutoCoords(tr);
  return hwnd;
}

HWND SWELL_MakeGroupBox(const char *name, int idx, int x, int y, int w, int h, int style)
{
  RECT tr=MakeCoords(x,y,w,h,false);
  HWND hwnd = new HWND__(m_make_owner,idx,&tr,name, !(style&SWELL_NOT_WS_VISIBLE),groupWindowProc);
  hwnd->m_wantfocus = false;
  hwnd->m_style = WS_CHILD | (style & ~SWELL_NOT_WS_VISIBLE);
  hwnd->m_classname = "groupbox";
  hwnd->m_wndproc(hwnd,WM_CREATE,0,0);
  SetWindowPos(hwnd,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE); 
  return hwnd;
}


int TabCtrl_GetItemCount(HWND hwnd)
{
   tabControlState *s = hwnd ? (tabControlState*) hwnd->m_private_data : NULL;
   return s ? s->m_tabs.GetSize() : 0;
}

BOOL TabCtrl_AdjustRect(HWND hwnd, BOOL fLarger, RECT *r)
{
  if (!r || !hwnd) return FALSE;
 
  r->top += TABCONTROL_HEIGHT;
  
  return TRUE;
}


BOOL TabCtrl_DeleteItem(HWND hwnd, int idx)
{
  tabControlState *s = hwnd ? (tabControlState*) hwnd->m_private_data : NULL;
  if (!s || !s->m_tabs.Get(idx)) return FALSE;
  
  s->m_tabs.Delete(idx,true);
  if (s->m_curtab>0) s->m_curtab--;
  InvalidateRect(hwnd,NULL,FALSE);
  // todo: send notification?

  return TRUE;
}

int TabCtrl_InsertItem(HWND hwnd, int idx, TCITEM *item)
{
  tabControlState *s = hwnd ? (tabControlState*) hwnd->m_private_data : NULL;
  if (!item || !s) return -1;
  if (!(item->mask & TCIF_TEXT) || !item->pszText) return -1;

  s->m_tabs.Insert(idx, strdup(item->pszText));

  InvalidateRect(hwnd,NULL,FALSE);
  // todo: send notification if s->m_tabs.GetSize()==1 ?

  return TRUE;
}

int TabCtrl_SetCurSel(HWND hwnd, int idx)
{
  tabControlState *s = hwnd ? (tabControlState*) hwnd->m_private_data : NULL;
  if (!s || !s->m_tabs.Get(idx)) return -1;
  const int lt =s->m_curtab;
  s->m_curtab = idx;
  InvalidateRect(hwnd,NULL,FALSE);
  
  return lt; 
}

int TabCtrl_GetCurSel(HWND hwnd)
{
  tabControlState *s = hwnd ? (tabControlState*) hwnd->m_private_data : NULL;
  return s ? s->m_curtab : -1;
}

void ListView_SetExtendedListViewStyleEx(HWND h, int flag, int mask)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return;
  lvs->m_extended_style = (lvs->m_extended_style & ~mask) | (flag&mask);
}

void SWELL_SetListViewFastClickMask(HWND hList, int mask)
{
}

void ListView_SetImageList(HWND h, HIMAGELIST imagelist, int which)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return;
  lvs->m_status_imagelist= (WDL_PtrList<HGDIOBJ__> *)imagelist;
  lvs->m_status_imagelist_type = which;
}

int ListView_GetColumnWidth(HWND h, int pos)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return 0;
  if (pos < 0 || pos >= lvs->m_cols.GetSize()) return 0;

  return lvs->m_cols.Get()[pos].xwid;
}

void ListView_InsertColumn(HWND h, int pos, const LVCOLUMN *lvc)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !lvc) return;
  SWELL_ListView_Col col = { 0, 100 };
  if (lvc->mask & LVCF_WIDTH) col.xwid = lvc->cx;
  if (lvc->mask & LVCF_TEXT) col.name = lvc->pszText ? strdup(lvc->pszText) : NULL;
  if (pos<0)pos=0;
  else if (pos>lvs->m_cols.GetSize()) pos=lvs->m_cols.GetSize();
  lvs->m_cols.Insert(col,pos);
}

void ListView_SetColumn(HWND h, int pos, const LVCOLUMN *lvc)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !lvc) return;
  SWELL_ListView_Col *col = pos>=0&&pos < lvs->m_cols.GetSize() ? lvs->m_cols.Get()+pos : NULL;
  if (!col) return;
  if (lvc->mask & LVCF_WIDTH) col->xwid = lvc->cx;
  if (lvc->mask & LVCF_TEXT) 
  {
    free(col->name);
    col->name = lvc->pszText ? strdup(lvc->pszText) : NULL;
  }
}

void ListView_GetItemText(HWND hwnd, int item, int subitem, char *text, int textmax)
{
  LVITEM it={LVIF_TEXT,item,subitem,0,0,text,textmax,};
  ListView_GetItem(hwnd,&it);
}

int ListView_InsertItem(HWND h, const LVITEM *item)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || lvs->IsOwnerData() || !item || item->iSubItem) return 0;

  int idx =  (int) item->iItem;
  if (idx<0 || idx>lvs->m_data.GetSize()) idx=lvs->m_data.GetSize();
  SWELL_ListView_Row *row=new SWELL_ListView_Row;
  row->m_vals.Add((item->mask&LVIF_TEXT) && item->pszText ? strdup(item->pszText) : NULL);
  row->m_param = (item->mask&LVIF_PARAM) ? item->lParam : 0;
  row->m_tmp = ((item->mask & LVIF_STATE) && (item->state & LVIS_SELECTED)) ? 1:0;
  if ((item->mask&LVIF_STATE) && (item->stateMask & LVIS_STATEIMAGEMASK)) row->m_imageidx=STATEIMAGEMASKTOINDEX(item->state);
  lvs->m_data.Insert(idx,row); 
  InvalidateRect(h,NULL,FALSE);
  return idx;
}

void ListView_SetItemText(HWND h, int ipos, int cpos, const char *txt)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || lvs->IsOwnerData() || cpos < 0) return;
  const int ncol = wdl_max(lvs->m_cols.GetSize(),1);
  if (cpos >= ncol) return;

  SWELL_ListView_Row *row=lvs->m_data.Get(ipos);
  if (!row) return;
  while (row->m_vals.GetSize()<=cpos) row->m_vals.Add(NULL);
  free(row->m_vals.Get(cpos));
  row->m_vals.Set(cpos,txt?strdup(txt):NULL);
  InvalidateRect(h,NULL,FALSE);
}

int ListView_GetNextItem(HWND h, int istart, int flags)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return -1;
  const int n = lvs->GetNumItems();
  for (int x = wdl_max(0,istart+1); x < n; x ++)
  {
    if (flags&LVNI_SELECTED) if (lvs->get_sel(x)) return x;
    if (flags&LVNI_FOCUSED) if (lvs->m_selitem==x) return x;
  }
  return -1;
}

bool ListView_SetItem(HWND h, LVITEM *item)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !item) return false;

  const bool ownerData = lvs->IsOwnerData();
  if (!ownerData)
  {
    SWELL_ListView_Row *row=lvs->m_data.Get(item->iItem);
    if (!row) return false;

    const int ncol = wdl_max(lvs->m_cols.GetSize(),1);
    if (item->iSubItem >= 0 && item->iSubItem < ncol)
    {
      while (row->m_vals.GetSize()<=item->iSubItem) row->m_vals.Add(NULL);
      if (item->mask&LVIF_TEXT) 
      {
        free(row->m_vals.Get(item->iSubItem));
        row->m_vals.Set(item->iSubItem,item->pszText?strdup(item->pszText):NULL);
      }
    }
    if (item->mask & LVIF_PARAM) 
    {
      row->m_param = item->lParam;
    }
    if (item->mask&LVIF_IMAGE)
    {
      row->m_imageidx=item->iImage+1;
    }
  }
  else 
  {
    if (item->iItem < 0 || item->iItem >= lvs->GetNumItems()) return false;
  }
  if (item->mask & LVIF_STATE)
  {
    ListView_SetItemState(h,item->iItem,item->state,item->stateMask);
  }

  InvalidateRect(h,NULL,FALSE);

  return true;
}

bool ListView_GetItem(HWND h, LVITEM *item)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !item) return false;
  if (!lvs->IsOwnerData())
  {
    SWELL_ListView_Row *row=lvs->m_data.Get(item->iItem);
    if (!row) return false;
    if ((item->mask&LVIF_TEXT)&&item->pszText && item->cchTextMax > 0) 
    {
      const char *v=row->m_vals.Get(item->iSubItem);
      lstrcpyn_safe(item->pszText, v?v:"",item->cchTextMax);
    }
    if (item->mask & LVIF_PARAM) item->lParam = row->m_param;
  }
  else 
  {
    if (item->iItem < 0 || item->iItem >= lvs->GetNumItems()) return false;
  }

  if (item->mask & LVIF_STATE) 
  {
    item->state = 0;
    if ((item->stateMask & LVIS_SELECTED) && lvs->get_sel(item->iItem)) item->state |= LVIS_SELECTED;
    if ((item->stateMask & LVIS_FOCUSED) && lvs->m_selitem == item->iItem) item->state |= LVIS_FOCUSED;
    if (item->stateMask & 0xff0000)
    {
      SWELL_ListView_Row *row = lvs->m_data.Get(item->iItem);
      if (row)
        item->state |= INDEXTOSTATEIMAGEMASK(row->m_imageidx);
    }
  }

  return true;
}
int ListView_GetItemState(HWND h, int ipos, UINT mask)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return 0;
  int ret  = 0;
  if (mask & LVIS_SELECTED) ret |= (lvs->get_sel(ipos) ? LVIS_SELECTED : 0 );
  if ((mask & LVIS_FOCUSED) && lvs->m_selitem == ipos) ret |= LVIS_FOCUSED;
  if ((mask & LVIS_STATEIMAGEMASK) && lvs->m_status_imagelist_type == LVSIL_STATE) 
  {
    SWELL_ListView_Row *row = lvs->m_data.Get(ipos);
    if (row)
      ret |= INDEXTOSTATEIMAGEMASK(row->m_imageidx);
  }
  return ret;
}

bool ListView_SetItemState(HWND h, int ipos, UINT state, UINT statemask)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return false;

  static int _is_doing_all;
  
  if (ipos == -1)
  {
    int x;
    int n=ListView_GetItemCount(h);
    _is_doing_all++;
    if ((statemask & LVIS_SELECTED) && (state & LVIS_SELECTED) && !lvs->m_is_multisel)
      statemask &= ~LVIS_SELECTED;
    for (x = 0; x < n; x ++)
      ListView_SetItemState(h,x,state,statemask);
    _is_doing_all--;
    ListView_RedrawItems(h,0,n-1);
    return true;
  }
  bool changed=false;

  if (statemask & LVIS_SELECTED) changed |= lvs->set_sel(ipos,!!(state&LVIS_SELECTED));
  if (statemask & LVIS_FOCUSED)
  {
    if (state&LVIS_FOCUSED) 
    {
      if (lvs->m_selitem != ipos)
      {
        changed=true;
        lvs->m_selitem = ipos;
      }
    }
  }
  if ((statemask & LVIS_STATEIMAGEMASK) && lvs->m_status_imagelist_type == LVSIL_STATE) 
  {
    SWELL_ListView_Row *row = lvs->m_data.Get(ipos);
    if (row)
    {
      const int idx= row->m_imageidx;
      row->m_imageidx=STATEIMAGEMASKTOINDEX(state);
      if (!changed && idx != row->m_imageidx) ListView_RedrawItems(h,ipos,ipos);
    }
  }

  if (changed)
  {
    static int __rent;
    if (!__rent)
    {
      __rent++;
      NMLISTVIEW nm={{(HWND)h,(unsigned short)h->m_id,LVN_ITEMCHANGED},ipos,0,state,};
      SendMessage(GetParent(h),WM_NOTIFY,h->m_id,(LPARAM)&nm);      
      __rent--;
    }
    if (!_is_doing_all) ListView_RedrawItems(h,ipos,ipos);
  }
  return true;
}
void ListView_RedrawItems(HWND h, int startitem, int enditem)
{
  if (!h) return;
  InvalidateRect(h,NULL,FALSE);
}

void ListView_DeleteItem(HWND h, int ipos)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || lvs->IsOwnerData()) return;
  lvs->m_data.Delete(ipos,true);
  InvalidateRect(h,NULL,FALSE);
}

void ListView_DeleteAllItems(HWND h)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || lvs->IsOwnerData()) return;
  lvs->m_data.Empty(true);
  InvalidateRect(h,NULL,FALSE);
}

int ListView_GetSelectedCount(HWND h)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return 0;
  const int n = lvs->GetNumItems();
  int sum=0,x;
  for (x=0;x<n;x++) if (lvs->get_sel(x)) sum++;
  return sum;
}

int ListView_GetItemCount(HWND h)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return 0;
  return lvs->GetNumItems();
}

int ListView_GetSelectionMark(HWND h)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return 0;
  const int n = lvs->GetNumItems();
  int x;
  for (x=0;x<n;x++) if (lvs->get_sel(x)) return x;
  return -1;
}
int SWELL_GetListViewHeaderHeight(HWND h)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  return lvs ? lvs->GetColumnHeaderHeight(h) : 0;
}

void ListView_SetColumnWidth(HWND h, int pos, int wid)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return;
  SWELL_ListView_Col *col = pos>=0&&pos < lvs->m_cols.GetSize() ? lvs->m_cols.Get()+pos : NULL;
  if (col) 
  {
    col->xwid = wid;
    InvalidateRect(h,NULL,FALSE);
  }
}

int ListView_HitTest(HWND h, LVHITTESTINFO *pinf)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !pinf) return -1;

  pinf->flags=0;
  pinf->iItem=-1;

  int x=pinf->pt.x;
  int y=pinf->pt.y;

  RECT r;
  GetClientRect(h,&r);

  if (x < 0) pinf->flags |= LVHT_TOLEFT;
  if (x >= r.right) pinf->flags |= LVHT_TORIGHT;
  if (y < 0) pinf->flags |= LVHT_ABOVE;
  if (y >= r.bottom) pinf->flags |= LVHT_BELOW;

  if (!pinf->flags && lvs->m_last_row_height)
  {
    const int ypos = y - lvs->GetColumnHeaderHeight(h);
    const int hit = ypos >= 0 ? ((ypos + lvs->m_scroll_y) / lvs->m_last_row_height) : -1;
    if (hit < 0) pinf->flags |= LVHT_ABOVE;
    pinf->iItem=hit < 0 || hit >= lvs->GetNumItems() ? -1 : hit;
    if (pinf->iItem >= 0)
    {
      if (lvs->m_status_imagelist && x < lvs->m_last_row_height)
      {
        pinf->flags=LVHT_ONITEMSTATEICON;
      }
      else 
      {
        pinf->flags=LVHT_ONITEMLABEL;
      }
    }
    else 
    {
      pinf->flags=LVHT_NOWHERE;
    }
  }

  return pinf->iItem;
}
int ListView_SubItemHitTest(HWND h, LVHITTESTINFO *pinf)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !pinf) return -1;

  const int row = ListView_HitTest(h, pinf);
  int x,xpos=-lvs->m_scroll_x,idx=0;
  const int n=lvs->m_cols.GetSize();
  const bool has_image = lvs->hasStatusImage();
  if (has_image) xpos += lvs->m_last_row_height;
  for (x=0;x<n;x++)
  {
    const int xwid = lvs->m_cols.Get()[x].xwid;
    if (pinf->pt.x >= xpos && pinf->pt.x < xpos+xwid) { idx = x; break; }
    xpos += xwid;
  }
  pinf->iSubItem = idx;
  return row;
}

void ListView_SetItemCount(HWND h, int cnt)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !lvs->IsOwnerData()) return;
  lvs->m_owner_data_size = cnt > 0 ? cnt : 0;
  if (lvs->m_owner_multisel_state.GetSize() > lvs->m_owner_data_size) lvs->m_owner_multisel_state.Resize(lvs->m_owner_data_size);
  if (lvs->m_selitem >= lvs->m_owner_data_size) lvs->m_selitem = -1;
}

void ListView_EnsureVisible(HWND h, int i, BOOL pok)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !lvs->m_last_row_height) return;
  const int n = lvs->GetNumItems();
  if (i>=0 && i < n)
  {
    const int row_height = lvs->m_last_row_height;
    RECT r;
    GetClientRect(h,&r);
    r.bottom -= lvs->GetColumnHeaderHeight(h);
    if (lvs->getTotalWidth() > r.right) r.bottom -= row_height;

    const int oldy = lvs->m_scroll_y;
    if (i*row_height < lvs->m_scroll_y) lvs->m_scroll_y = i*row_height;
    else if ((i+1)*row_height > lvs->m_scroll_y + r.bottom) lvs->m_scroll_y = (i+1)*row_height-r.bottom;
    lvs->sanitizeScroll(h);
    if (oldy != lvs->m_scroll_y)
    {
      InvalidateRect(h,NULL,FALSE);
    }
  }

}
bool ListView_GetSubItemRect(HWND h, int item, int subitem, int code, RECT *r)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !r) return false;

  r->top = lvs->m_last_row_height * item - lvs->m_scroll_y;
  r->top += lvs->GetColumnHeaderHeight(h);
  RECT cr;
  GetClientRect(h,&cr);
  r->left=cr.left;
  r->right=cr.right;

  if (subitem>0)
  {
    int x,xpos=-lvs->m_scroll_x;
    const int n=lvs->m_cols.GetSize();
    for (x = 0; x < n; x ++)
    {
      int xwid = lvs->m_cols.Get()[x].xwid;
      if (!x && lvs->hasStatusImage()) xwid += lvs->m_last_row_height;
      if (x == subitem)
      {
        r->left=xpos;
        r->right=xpos+xwid;
        break;
      }
      xpos += xwid;
    }
  }

  if (r->top < -64-lvs->m_last_row_height) r->top = -64 - lvs->m_last_row_height;
  if (r->top > cr.bottom+64) r->top = cr.bottom+64;

  r->bottom = r->top + lvs->m_last_row_height;

  return true;
}

bool ListView_GetItemRect(HWND h, int item, RECT *r, int code)
{
  return ListView_GetSubItemRect(h, item, -1, code, r);
}

bool ListView_Scroll(HWND h, int xscroll, int yscroll)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !lvs->m_last_row_height) return false;
  const int oldy = lvs->m_scroll_y, oldx = lvs->m_scroll_x;
  lvs->m_scroll_x += xscroll;
  lvs->m_scroll_y += yscroll;
  lvs->sanitizeScroll(h);
  if (oldy != lvs->m_scroll_y || oldx != lvs->m_scroll_x)
      InvalidateRect(h,NULL,FALSE);
  return true;
}

void ListView_SortItems(HWND hwnd, PFNLVCOMPARE compf, LPARAM parm)
{
  listViewState *lvs = hwnd ? (listViewState *)hwnd->m_private_data : NULL;
  if (!lvs || 
      lvs->m_is_listbox ||
      lvs->m_owner_data_size >= 0 || !compf) return;

  WDL_HeapBuf tmp;
  char *b = (char*)tmp.ResizeOK(lvs->m_data.GetSize()*sizeof(void *));
  if (b) 
    __listview_mergesort_internal(lvs->m_data.GetList(),lvs->m_data.GetSize(),
       sizeof(void *),compf,parm,(char*)b);
  InvalidateRect(hwnd,NULL,FALSE);
}

bool ListView_DeleteColumn(HWND h, int pos)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || pos < 0 || pos >= lvs->m_cols.GetSize()) return false;

  free(lvs->m_cols.Get()[pos].name);
  lvs->m_cols.Delete(pos);
  InvalidateRect(h,NULL,FALSE);
  return true;
}

int ListView_GetCountPerPage(HWND h)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !lvs->m_last_row_height) return 0;

  RECT cr;
  GetClientRect(h,&cr);
  cr.bottom -= lvs->GetColumnHeaderHeight(h);
  return (cr.bottom-cr.top) / lvs->m_last_row_height;
}

HWND ChildWindowFromPoint(HWND h, POINT p)
{
  if (!h) return 0;

  RECT r={0,};

  for(;;)
  {
    HWND h2=h->m_children;
    RECT sr;

    NCCALCSIZE_PARAMS tr={{h->m_position,},};
    if (h->m_wndproc) h->m_wndproc(h,WM_NCCALCSIZE,0,(LPARAM)&tr);
    r.left += tr.rgrc[0].left - h->m_position.left;
    r.top += tr.rgrc[0].top - h->m_position.top;

    HWND best=NULL;
    RECT bestsr = { 0, };
    while (h2)
    {
      sr = h2->m_position;
      sr.left += r.left;
      sr.right += r.left;
      sr.top += r.top;
      sr.bottom += r.top;

      if (h2->m_visible && PtInRect(&sr,p)) 
      {
        bestsr = sr;
        best=h2;
      }

      h2 = h2->m_next;
    }
    if (!best) break; // h is the window we care about

    h=best; // descend to best
    r=bestsr;
  }

  return h;
}

static HWND recurseOwnedWindowHitTest(HWND h, POINT p, int maxdepth)
{
  RECT r;
  GetWindowContentViewRect(h,&r);
  if (!PtInRect(&r,p)) return NULL;

  // check any owned windows first, as they are always above our window
  if (h->m_owned_list && maxdepth > 0)
  {
    HWND owned = h->m_owned_list;
    while (owned)
    {
      if (owned->m_visible)
      {
        HWND hit = recurseOwnedWindowHitTest(owned,p,maxdepth-1);
        if (hit) return hit;
      }
      owned = owned->m_owned_next;
    }
  }
  p.x -= r.left;
  p.y -= r.top;
  return ChildWindowFromPoint(h,p);
}

HWND WindowFromPoint(POINT p)
{
  HWND h = SWELL_topwindows;
  while (h)
  {
    if (h->m_visible)
    {
      HWND hit = recurseOwnedWindowHitTest(h,p,20);
      if (hit) return hit;
    }
    h = h->m_next;
  }
  return NULL;
}

BOOL InvalidateRect(HWND hwnd, const RECT *r, int eraseBk)
{ 
  if (!hwnd || hwnd->m_hashaddestroy) return FALSE;

#ifdef SWELL_LICE_GDI
  RECT rect;
  if (r) 
  {
    rect = *r;
  }
  else
  {
    rect = hwnd->m_position;
    WinOffsetRect(&rect, -rect.left, -rect.top);
  }

  // rect is in client coordinates of h
  HWND h = hwnd;
  for (;;)
  {
    if (!h->m_visible || h->m_hashaddestroy) return FALSE;

    RECT ncrect = h->m_position;
    if (h->m_oswindow) WinOffsetRect(&ncrect, -ncrect.left, -ncrect.top);

    NCCALCSIZE_PARAMS tr;
    memset(&tr,0,sizeof(tr));
    tr.rgrc[0] = ncrect;
    if (h->m_wndproc) h->m_wndproc(h,WM_NCCALCSIZE,0,(LPARAM)&tr);

    WinOffsetRect(&rect,tr.rgrc[0].left, tr.rgrc[0].top);

    if (!IntersectRect(&rect,&rect,&ncrect)) return FALSE;

    if (h->m_oswindow) break;

    h=h->m_parent;
    if (!h) return FALSE;
  }

  {
    hwnd->m_invalidated=true;
    HWND t=hwnd->m_parent;
    if (t && (t->m_style & WS_CLIPSIBLINGS))
    {
      // child window, invalidate any later children that intersect us (redraw them on top of us)
      HWND nw = hwnd->m_next;
      while (nw)
      {
        RECT tmp;
        if (nw->m_visible && !nw->m_invalidated && WinIntersectRect(&tmp,&hwnd->m_position,&nw->m_position))
          nw->m_invalidated=true;
        nw=nw->m_next;
      }
    }

    while (t)
    { 
      if (eraseBk)
      {
        t->m_invalidated=true;
        eraseBk--;
      }
      t->m_child_invalidated=true;
      t=t->m_parent; 
    }
  }
  swell_oswindow_invalidate(h, (hwnd!=h || r) ? &rect : NULL);
#endif
  return TRUE;
}


HWND GetCapture()
{
  return swell_captured_window;
}

HWND SetCapture(HWND hwnd)
{
  HWND oc = swell_captured_window;
  if (oc != hwnd)
  {
    swell_captured_window=hwnd;
    if (oc) SendMessage(oc,WM_CAPTURECHANGED,0,(LPARAM)hwnd);
  } 
  return oc;
}

void ReleaseCapture()
{
  if (swell_captured_window) 
  {
    SendMessage(swell_captured_window,WM_CAPTURECHANGED,0,0);
    swell_captured_window=0;
  }
}

static HWND getNextFocusWindow(HWND hwnd, bool rev, HWND foc_child)
{
  HWND ch = NULL;
  if (foc_child)
  {
    ch = hwnd->m_children;
    while (ch && ch != foc_child) ch = ch->m_next;
  }

  int pass=0;
  if (ch) 
  {
    ch = rev ? ch->m_prev : ch->m_next;
  }
  else
  {
    ch = hwnd->m_children;
    if (ch && rev) while (ch->m_next) ch=ch->m_next;
    pass++;
  }

  for (;;)
  {
    while (ch)
    {
      // scan to find next matching control
      if (ch->m_wantfocus && ch->m_visible && ch->m_enabled) break;
      ch = rev ? ch->m_prev : ch->m_next;
    }
    if (ch || ++pass>1 || hwnd->m_parent) break;

    // continue searching
    ch = hwnd->m_children;
    if (ch && rev) while (ch->m_next) ch=ch->m_next;
  }
  if (ch && ch->m_children)
  {
    HWND sub = getNextFocusWindow(ch,rev,NULL);
    if (sub) return sub;
  }
  return ch;
}


LRESULT SwellDialogDefaultWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  DLGPROC d=(DLGPROC)GetWindowLong(hwnd,DWL_DLGPROC);
  if (d) 
  {
    if (uMsg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          HBRUSH hbrush = (HBRUSH) d(hwnd,WM_CTLCOLORDLG,(WPARAM)ps.hdc,(LPARAM)hwnd);
          if (hbrush && hbrush != (HBRUSH)1)
          {
            FillRect(ps.hdc,&ps.rcPaint,hbrush);
          }
          else if (1) 
          {
            SWELL_FillDialogBackground(ps.hdc,&ps.rcPaint,0);
          }
          
          EndPaint(hwnd,&ps);
        }
    }
    
    LRESULT r=(LRESULT) d(hwnd,uMsg,wParam,lParam);
    
   
    if (r) return r; 

    if (uMsg == WM_KEYDOWN)
    {
      if (!hwnd->m_parent)
      {
        if (wParam == VK_ESCAPE)
        {
          if (IsWindowEnabled(hwnd) && !SendMessage(hwnd,WM_CLOSE,0,0))
            SendMessage(hwnd,WM_COMMAND,IDCANCEL,0);
          return 0;
        }
        else if (wParam == VK_RETURN)
        {
          HWND c = GetWindow(hwnd,GW_CHILD);
          while (c)
          {
            if (c->m_id && (c->m_style&BS_DEFPUSHBUTTON) && 
                c->m_classname && !strcmp(c->m_classname,"Button"))
            {
              SendMessage(hwnd,WM_COMMAND,c->m_id,0);
              return 0;
            }
            c = GetWindow(c,GW_HWNDNEXT);
          }
          c = GetDlgItem(hwnd,IDOK);
          if (c) SendMessage(hwnd,WM_COMMAND,IDOK,0);
          return 0;
        }
      }
      int navdir = 0;

      if (wParam == VK_TAB && (lParam&~FSHIFT) == FVIRTKEY) navdir = (lParam & FSHIFT) ? -1 : 1;
      else if (lParam == FVIRTKEY)
      {
        if (wParam == VK_LEFT || wParam == VK_UP) navdir = -1;
        else if (wParam == VK_RIGHT || wParam == VK_DOWN) navdir = 1;
      }

      if (navdir)
      {
        HWND ch = getNextFocusWindow(hwnd,navdir<0,hwnd->m_focused_child);
        if (ch)
        {
          SetFocus(ch);

          InvalidateRect(ch,NULL,FALSE);
          return 0;
        }
      }
    }
  }
  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

BOOL EndPaint(HWND hwnd, PAINTSTRUCT *ps)
{
  return TRUE;
}


static HFONT menubar_font;

static bool wantRightAlignedMenuBarItem(const char *p)
{
  char c = *p;
  return c > 0 && c != '&' && !isalnum(c);
}

static int menuBarHitTest(HWND hwnd, int mousex, int mousey, RECT *rOut, int forceItem)
{
  int rv=-1;
  RECT r;
  GetWindowContentViewRect(hwnd,&r);
  if (forceItem >= 0 || (mousey>=r.top && mousey < r.top+g_swell_ctheme.menubar_height))
  {
    HDC dc = GetWindowDC(hwnd);

    int x,xpos=r.left + g_swell_ctheme.menubar_margin_width;
    HMENU__ *menu = (HMENU__*)hwnd->m_menu;
    HGDIOBJ oldfont = dc ? SelectObject(dc,menubar_font) : NULL;
    const int n=menu->items.GetSize();
    for(x=0;x<n;x++)
    {
      MENUITEMINFO *inf = menu->items.Get(x);
      if (inf->fType == MFT_STRING && inf->dwTypeData)
      {
        bool dis = !!(inf->fState & MF_GRAYED);
        RECT cr={0,}; 
        DrawText(dc,inf->dwTypeData,-1,&cr,DT_CALCRECT);
        if (x == n-1 && wantRightAlignedMenuBarItem(inf->dwTypeData))
        {
          xpos = wdl_max(xpos,r.right - g_swell_ctheme.menubar_margin_width - cr.right);
          cr.right = r.right - g_swell_ctheme.menubar_margin_width - xpos;
        }

        if (forceItem>=0 ? forceItem == x : (mousex >=xpos && mousex< xpos + cr.right + g_swell_ctheme.menubar_spacing_width))
        {
          if (!dis) 
          {
            rOut->left = xpos;
            rOut->right = xpos + cr.right;
            rOut->top = r.top;
            rOut->bottom = r.top + g_swell_ctheme.menubar_height;
            rv=x;
          }
          break;
        }

        xpos+=cr.right+g_swell_ctheme.menubar_spacing_width;
      }
    }
    
    if (dc) 
    {
      SelectObject(dc,oldfont);
      ReleaseDC(hwnd,dc);
    }
  }
  return rv;
}

static RECT g_menubar_lastrect;
static HWND g_menubar_active;
static POINT g_menubar_startpt;
static bool g_menubar_active_drag;

HWND swell_window_wants_all_input()
{
  return g_menubar_active_drag ? g_menubar_active : NULL;
}

int menuBarNavigate(int dir) // -1 if no menu bar active, 0 if did nothing, 1 if navigated
{
  if (!g_menubar_active || !g_menubar_active->m_menu) return -1;
  HMENU__ *menu = (HMENU__*)g_menubar_active->m_menu;
  RECT r;
  const int x = menuBarHitTest(g_menubar_active,0,0,&r,menu->sel_vis + dir);
  if (x>=0)
  {
    MENUITEMINFO *inf = menu->items.Get(x);
    if (inf && inf->hSubMenu)
    {
      menu->sel_vis = x;
      g_menubar_lastrect = r;

      DestroyPopupMenus();
      return 1;
    }
  }
  return 0;
}

RECT g_trackpopup_yroot;
static void runMenuBar(HWND hwnd, HMENU__ *menu, int x, const RECT *use_r)
{
  menu->Retain();
  MENUITEMINFO *inf = menu->items.Get(x);
  RECT r = *use_r;
  g_trackpopup_yroot = r;

  RECT mbr;
  GetWindowContentViewRect(hwnd,&mbr);
  mbr.right -= mbr.left;
  mbr.left=0;
  mbr.bottom = 0;
  mbr.top = -g_swell_ctheme.menubar_height;
  menu->sel_vis = x;
  GetCursorPos(&g_menubar_startpt);
  g_menubar_active = hwnd;
  g_menubar_active_drag=true;
  for (;;)
  {
    InvalidateRect(hwnd,&mbr,FALSE);
    if (TrackPopupMenu(inf->hSubMenu,0,r.left,r.bottom,0xbeef,hwnd,NULL) || menu->sel_vis == x) break;

    x = menu->sel_vis;
    inf = menu->items.Get(x);
    if (!inf || !inf->hSubMenu) break;

    r = g_menubar_lastrect;
  }
  menu->sel_vis=-1;
  InvalidateRect(hwnd,&mbr,FALSE);
  g_menubar_active = NULL;
  g_trackpopup_yroot.top = g_trackpopup_yroot.bottom = 0;
  menu->Release();
}

LRESULT DefWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_DESTROY:
      if (g_menubar_active == hwnd) g_menubar_active=NULL;
    break;
    case WM_NCMOUSEMOVE:
      if (g_menubar_active == hwnd && hwnd->m_menu)
      {
        swell_delegate_menu_message(hwnd,lParam,WM_MOUSEMOVE,true);

        HMENU__ *menu = (HMENU__*)hwnd->m_menu;
        RECT r;
        const int x = menuBarHitTest(hwnd,GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam),&r,-1);
        if (x>=0 && x != menu->sel_vis)
        {
          MENUITEMINFO *inf = menu->items.Get(x);
          if (inf && inf->hSubMenu)
          {
            menu->sel_vis = x;
            g_menubar_lastrect = r;

            DestroyPopupMenus(); // cause new menu to be popped up
          }
        }
      }
    break;

    case WM_NCCALCSIZE:
      if (!hwnd->m_parent && hwnd->m_menu)
      {
        RECT *r = (RECT*)lParam;
        r->top += g_swell_ctheme.menubar_height;
      }
    break;
    case WM_NCPAINT:
      if (!hwnd->m_parent && hwnd->m_menu)
      {
        HDC dc = GetWindowDC(hwnd);
        if (dc)
        {
          if (!menubar_font) 
            menubar_font = CreateFont(g_swell_ctheme.menubar_font_size,0,0,0,FW_NORMAL,0,0,0,0,0,0,0,0,g_swell_deffont_face);

          RECT r;
          GetWindowContentViewRect(hwnd,&r);
          r.right -= r.left; r.left=0;
          r.bottom -= r.top; r.top=0;
          if (r.bottom>g_swell_ctheme.menubar_height) r.bottom=g_swell_ctheme.menubar_height;

          {
            HBRUSH br=CreateSolidBrush(g_swell_ctheme.menubar_bg);
            FillRect(dc,&r,br);
            DeleteObject(br);
          }

          HGDIOBJ oldfont = SelectObject(dc,menubar_font);
          SetBkMode(dc,TRANSPARENT);

          int x,xpos=g_swell_ctheme.menubar_margin_width;
          HMENU__ *menu = (HMENU__*)hwnd->m_menu;
          const int n = menu->items.GetSize();
          for(x=0;x<n;x++)
          {
            MENUITEMINFO *inf = menu->items.Get(x);
            if (inf->fType == MFT_STRING && inf->dwTypeData)
            {
              bool dis = !!(inf->fState & MF_GRAYED);
              RECT cr={0};
              DrawText(dc,inf->dwTypeData,-1,&cr,DT_CALCRECT);

              if (x == n-1 && wantRightAlignedMenuBarItem(inf->dwTypeData))
              {
                cr.left = wdl_max(xpos,r.right - g_swell_ctheme.menubar_margin_width - cr.right);
                cr.right = r.right - g_swell_ctheme.menubar_margin_width;
              }
              else
              {
                cr.left = xpos;
                cr.right += xpos;
              }
              cr.top = r.top;
              cr.bottom = r.bottom;
              if (!dis && menu->sel_vis == x)
              {
                HBRUSH br = CreateSolidBrush(g_swell_ctheme.menubar_bg_sel);
                FillRect(dc,&cr,br);
                DeleteObject(br);
                SetTextColor(dc,g_swell_ctheme.menubar_text_sel);
              }
              else SetTextColor(dc,
                 dis ? g_swell_ctheme.menubar_text_disabled :
                   g_swell_ctheme.menubar_text);

              DrawText(dc,inf->dwTypeData,-1,&cr,DT_VCENTER|DT_LEFT);
              xpos=cr.right+g_swell_ctheme.menubar_spacing_width;
            }
          }

          SelectObject(dc,oldfont);
          ReleaseDC(hwnd,dc);
        }
      }
    break;
    case WM_RBUTTONUP:
    case WM_NCRBUTTONUP:
      {  
        POINT p={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
        HWND hwndDest=hwnd;
        if (msg==WM_RBUTTONUP)
        {
          ClientToScreen(hwnd,&p);
          HWND h=WindowFromPoint(p);
          if (h && IsChild(hwnd,h)) hwndDest=h;
        }
        SendMessage(hwnd,WM_CONTEXTMENU,(WPARAM)hwndDest,(p.x&0xffff)|(p.y<<16));
      }
    return 1;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
      if (!hwnd->m_parent && hwnd->m_menu)
      {
        if (msg == WM_NCLBUTTONUP && g_menubar_active_drag) 
        {
          g_menubar_active_drag=false;
          if (swell_delegate_menu_message(hwnd,lParam,WM_LBUTTONUP,true)) return 0;

          POINT pt;
          GetCursorPos(&pt);
          pt.x -= g_menubar_startpt.x;
          pt.y -= g_menubar_startpt.y;
          if (pt.x*pt.x+ pt.y*pt.y > 4*4) 
          {
            DestroyPopupMenus();
            return 0;
          }
        }
        RECT r;
        const int x = menuBarHitTest(hwnd,GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam),&r,-1);
        if (x>=0)
        {
          HMENU__ *menu = (HMENU__*)hwnd->m_menu;
          MENUITEMINFO *inf = menu->items.Get(x);
          if (inf) 
          {
            if (inf->hSubMenu)
            { 
              if (msg == WM_NCLBUTTONDOWN) 
              {
                runMenuBar(hwnd,menu,x,&r);
              }
            }
            else if (msg == WM_NCLBUTTONUP)
            { 
              if (inf->wID) SendMessage(hwnd,WM_COMMAND,inf->wID,0);
            }
          }
        }
      }
    break;
    case WM_NCHITTEST: 
      if (!hwnd->m_parent && hwnd->m_menu)
      {
        RECT r;
        GetWindowContentViewRect(hwnd,&r);
        if (GET_Y_LPARAM(lParam)>=r.top && GET_Y_LPARAM(lParam) < r.top+g_swell_ctheme.menubar_height) return HTMENU;
      }
      // todo: WM_NCCALCSIZE etc
    return HTCLIENT;
    case WM_KEYDOWN:
    case WM_KEYUP: 
        if (hwnd->m_parent) return SendMessage(hwnd->m_parent,msg,wParam,lParam);

        if (msg == WM_KEYDOWN && hwnd->m_menu && 
            lParam == (FVIRTKEY | FALT) && (
              (wParam >= 'A' && wParam <= 'Z') ||
              (wParam >= '0' && wParam <= '9')
            ) 
           )
        {
          HMENU__ *menu = (HMENU__*)hwnd->m_menu;
          const int n=menu->items.GetSize();
          for(int x=0;x<n;x++)
          {
            MENUITEMINFO *inf = menu->items.Get(x);
            if (inf->fType == MFT_STRING && 
                !(inf->fState & MF_GRAYED) &&
                inf->dwTypeData)
            {
              const char *p = inf->dwTypeData;
              while (*p)
              {
                if (*p++ == '&')
                {
                  if (*p != '&') break;
                  p++;
                }
              }
              if (*p > 0 && (WPARAM)toupper(*p) == wParam)
              {
                if (inf->hSubMenu)
                {
                  RECT r;
                  if (menuBarHitTest(hwnd,0,0,&r,x)>=0)
                  {
                    runMenuBar(hwnd,menu,x,&r);
                  }
                }
                else
                {
                  if (inf->wID) SendMessage(hwnd,WM_COMMAND,inf->wID,0);
                }

                return 1;
              }
            }
          }
        }
    return 69;

    case WM_CONTEXTMENU:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_SETCURSOR:
        return hwnd->m_parent ? SendMessage(hwnd->m_parent,msg,wParam,lParam) : 0;

    case WM_SETFONT:
      hwnd->m_font = (HFONT)wParam;
    return 0;

    case WM_GETFONT:
        if (hwnd->m_font) return (LRESULT) hwnd->m_font;
#ifdef SWELL_FREETYPE
        {
          HFONT SWELL_GetDefaultFont(void);
          return (LRESULT)SWELL_GetDefaultFont();
        }
#endif

        return 0;
    case WM_DROPFILES:
        if (hwnd->m_parent && wParam)
        {
          DROPFILES *df=(DROPFILES*)wParam;
          ClientToScreen(hwnd,&df->pt);
          ScreenToClient(hwnd->m_parent,&df->pt);

          return SendMessage(hwnd->m_parent,msg,wParam,lParam);
        }
        return 0;

  }
  return 0;
}


















///////////////// clipboard compatability (NOT THREAD SAFE CURRENTLY)


BOOL DragQueryPoint(HDROP hDrop,LPPOINT pt)
{
  if (!hDrop) return 0;
  DROPFILES *df=(DROPFILES*)GlobalLock(hDrop);
  BOOL rv=!df->fNC;
  *pt=df->pt;
  GlobalUnlock(hDrop);
  return rv;
}

void DragFinish(HDROP hDrop)
{
//do nothing for now (caller will free hdrops)
}

UINT DragQueryFile(HDROP hDrop, UINT wf, char *buf, UINT bufsz)
{
  if (!hDrop) return 0;
  DROPFILES *df=(DROPFILES*)GlobalLock(hDrop);

  UINT rv=0;
  char *p=(char*)df + df->pFiles;
  if (wf == 0xFFFFFFFF)
  {
    while (*p)
    {
      rv++;
      p+=strlen(p)+1;
    }
  }
  else
  {
    while (*p)
    {
      if (!wf--)
      {
        if (buf)
        {
          lstrcpyn_safe(buf,p,bufsz);
          rv=strlen(buf);
        }
        else rv=strlen(p);
          
        break;
      }
      p+=strlen(p)+1;
    }
  }
  GlobalUnlock(hDrop);
  return rv;
}


///////// PostMessage emulation

BOOL PostMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  return SWELL_Internal_PostMessage(hwnd,message,wParam,lParam);
}

void SWELL_MessageQueue_Clear(HWND h)
{
  SWELL_Internal_PMQ_ClearAllMessages(h);
}



// implementation of postmessage stuff




typedef struct PMQ_rec
{
  HWND hwnd;
  UINT msg;
  WPARAM wParam;
  LPARAM lParam;

  struct PMQ_rec *next;
} PMQ_rec;

static WDL_Mutex *m_pmq_mutex;
static PMQ_rec *m_pmq, *m_pmq_empty, *m_pmq_tail;
static int m_pmq_size;

#define MAX_POSTMESSAGE_SIZE 1024


void SWELL_Internal_PostMessage_Init()
{
  if (m_pmq_mutex) return;
  
  m_pmq_mainthread=pthread_self();
  m_pmq_mutex = new WDL_Mutex;
}

void SWELL_MessageQueue_Flush()
{
  if (!m_pmq_mutex) return;
  
  m_pmq_mutex->Enter();
  int max_amt = m_pmq_size;
  PMQ_rec *p=m_pmq;
  if (p)
  {
    m_pmq = p->next;
    if (m_pmq_tail == p) m_pmq_tail=NULL;
    m_pmq_size--;
  }
  m_pmq_mutex->Leave();
  
  // process out up to max_amt of queue
  while (p)
  {
    SendMessage(p->hwnd,p->msg,p->wParam,p->lParam); 

    m_pmq_mutex->Enter();
    // move this message to empty list
    p->next=m_pmq_empty;
    m_pmq_empty = p;

    // get next queued message (if within limits)
    p = (--max_amt > 0) ? m_pmq : NULL;
    if (p)
    {
      m_pmq = p->next;
      if (m_pmq_tail == p) m_pmq_tail=NULL;
      m_pmq_size--;
    }
    m_pmq_mutex->Leave();
  }
}

void SWELL_Internal_PMQ_ClearAllMessages(HWND hwnd)
{
  if (!m_pmq_mutex) return;
  
  m_pmq_mutex->Enter();
  PMQ_rec *p=m_pmq;
  PMQ_rec *lastrec=NULL;
  while (p)
  {
    if (hwnd && p->hwnd != hwnd) { lastrec=p; p=p->next; }
    else
    {
      PMQ_rec *next=p->next; 
      
      p->next=m_pmq_empty; // add p to empty list
      m_pmq_empty=p;
      m_pmq_size--;
      
      
      if (p==m_pmq_tail) m_pmq_tail=lastrec; // update tail
      
      if (lastrec)  p = lastrec->next = next;
      else p = m_pmq = next;
    }
  }
  m_pmq_mutex->Leave();
}

BOOL SWELL_Internal_PostMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (!hwnd||hwnd->m_hashaddestroy||!m_pmq_mutex) return FALSE;

  BOOL ret=FALSE;
  m_pmq_mutex->Enter();

  if (m_pmq_empty||m_pmq_size<MAX_POSTMESSAGE_SIZE)
  {
    PMQ_rec *rec=m_pmq_empty;
    if (rec) m_pmq_empty=rec->next;
    else rec=(PMQ_rec*)malloc(sizeof(PMQ_rec));
    rec->next=0;
    rec->hwnd=hwnd;
    rec->msg=msg;
    rec->wParam=wParam;
    rec->lParam=lParam;

    if (m_pmq_tail) m_pmq_tail->next=rec;
    else 
    {
      PMQ_rec *p=m_pmq;
      while (p && p->next) p=p->next; // shouldnt happen unless m_pmq is NULL As well but why not for safety
      if (p) p->next=rec;
      else m_pmq=rec;
    }
    m_pmq_tail=rec;
    m_pmq_size++;

    ret=TRUE;
  }

  m_pmq_mutex->Leave();

  return ret;
}


int EnumPropsEx(HWND hwnd, PROPENUMPROCEX proc, LPARAM lParam)
{
  if (!hwnd) return -1;
  int x;
  for (x =0 ; x < hwnd->m_props.GetSize(); x ++)
  {
    const char *k="";
    void *p = hwnd->m_props.Enumerate(x,&k);
    if (!proc(hwnd,k,p,lParam)) return 0;
  }
  return 1;
}

HANDLE GetProp(HWND hwnd, const char *name)
{
  if (!hwnd) return NULL;
  return hwnd->m_props.Get(name);
}

BOOL SetProp(HWND hwnd, const char *name, HANDLE val)
{
  if (!hwnd) return false;
  hwnd->m_props.Insert(name,(void *)val);
  return TRUE;
}

HANDLE RemoveProp(HWND hwnd, const char *name)
{
  HANDLE h =GetProp(hwnd,name);
  hwnd->m_props.Delete(name);
  return h;
}


int GetSystemMetrics(int p)
{
  switch (p)
  {
    case SM_CXSCREEN:
    case SM_CYSCREEN:
      {
         RECT r;
         SWELL_GetViewPort(&r, NULL, false);
         return p==SM_CXSCREEN ? r.right-r.left : r.bottom-r.top; 
      }
    case SM_CXHSCROLL:
    case SM_CYHSCROLL:
    case SM_CXVSCROLL:
    case SM_CYVSCROLL: return g_swell_ctheme.smscrollbar_width;
  }
  return 0;
}

BOOL ScrollWindow(HWND hwnd, int xamt, int yamt, const RECT *lpRect, const RECT *lpClipRect)
{
  if (!hwnd || (!xamt && !yamt)) return FALSE;
  InvalidateRect(hwnd,NULL,FALSE);
  
  // move child windows only
  hwnd = hwnd->m_children;
  while (hwnd)
  {
    hwnd->m_position.left += xamt;
    hwnd->m_position.right += xamt;
    hwnd->m_position.top += yamt;
    hwnd->m_position.bottom += yamt;

    hwnd=hwnd->m_next;
  }
  return TRUE;
}

HWND FindWindowEx(HWND par, HWND lastw, const char *classname, const char *title)
{
  HWND h=lastw?GetWindow(lastw,GW_HWNDNEXT):par?GetWindow(par,GW_CHILD):SWELL_topwindows;
  while (h)
  {
    bool isOk=true;
    if (title && strcmp(title,h->m_title.Get())) isOk=false;
    else if (classname)
    {
      if (!h->m_classname || strcmp(classname,h->m_classname)) isOk=false;
    }
    
    if (isOk) return h;
    h=GetWindow(h,GW_HWNDNEXT);
  }
  return NULL;
}


HTREEITEM TreeView_InsertItem(HWND hwnd, TV_INSERTSTRUCT *ins)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs || !ins) return NULL;

  HTREEITEM__ *par=NULL;
  int inspos=0;
  
  if (ins->hParent && ins->hParent != TVI_ROOT && ins->hParent != TVI_FIRST && ins->hParent != TVI_LAST && ins->hParent != TVI_SORT)
  {
    if (tvs->findItem(ins->hParent,&par,&inspos))
    {
      par = ins->hParent; 
    }
    else return 0;
  }
  
  if (ins->hInsertAfter == TVI_FIRST) inspos=0;
  else if (ins->hInsertAfter == TVI_LAST || ins->hInsertAfter == TVI_SORT || !ins->hInsertAfter) 
    inspos=(par ? par : &tvs->m_root)->m_children.GetSize();
  else inspos = (par ? par : &tvs->m_root)->m_children.Find(ins->hInsertAfter)+1;
  
  HTREEITEM__ *item=new HTREEITEM__;
  if (ins->item.mask & TVIF_CHILDREN) item->m_haschildren = !!ins->item.cChildren;
  if (ins->item.mask & TVIF_PARAM) item->m_param = ins->item.lParam;
  if (ins->item.mask & TVIF_TEXT) item->m_value = strdup(ins->item.pszText);

  (par ? par : &tvs->m_root)->m_children.Insert(inspos,item);
  
  InvalidateRect(hwnd,NULL,FALSE);
  return item;
}

BOOL TreeView_Expand(HWND hwnd, HTREEITEM item, UINT flag)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs || !tvs->findItem(item,NULL,NULL)) return FALSE;
 
  const int os = item->m_state;
  if (flag == TVE_EXPAND) item->m_state |= TVIS_EXPANDED;
  else if (flag == TVE_COLLAPSE) item->m_state &= ~TVIS_EXPANDED;
  else if (flag == TVE_TOGGLE) item->m_state ^= TVIS_EXPANDED;
  
  if (item->m_state != os) InvalidateRect(hwnd,NULL,FALSE);
  return TRUE;
}

HTREEITEM TreeView_GetSelection(HWND hwnd)
{ 
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs || !tvs->m_sel || !tvs->findItem(tvs->m_sel,NULL,NULL)) return NULL;
  return tvs->m_sel;
}

void TreeView_DeleteItem(HWND hwnd, HTREEITEM item)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs) return;
  HTREEITEM par=NULL;
  int idx=0;
  if (!tvs->findItem(item,&par,&idx)) return;

  if (tvs->m_sel && (item == tvs->m_sel || item->FindItem(tvs->m_sel,NULL,NULL))) tvs->m_sel=NULL;

  (par ? par : &tvs->m_root)->m_children.Delete(idx,true);
  InvalidateRect(hwnd,NULL,FALSE);
}

void TreeView_DeleteAllItems(HWND hwnd)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs) return;
  tvs->m_root.m_children.Empty(true);
  tvs->m_sel=NULL;
  InvalidateRect(hwnd,NULL,FALSE);
}

void TreeView_SelectItem(HWND hwnd, HTREEITEM item)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs) return;

  if (tvs->m_sel == item || (item && !tvs->findItem(item,NULL,NULL))) return;

  tvs->m_sel = item;

  static int __rent;
  if (!__rent)
  {
    __rent++;
    NMTREEVIEW nm={{(HWND)hwnd,(UINT_PTR)hwnd->m_id,TVN_SELCHANGED},};
    nm.itemNew.hItem = item;
    SendMessage(GetParent(hwnd),WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
    __rent--;
  }
  tvs->ensureItemVisible(hwnd,tvs->m_sel);
  InvalidateRect(hwnd,NULL,FALSE);
}

BOOL TreeView_GetItem(HWND hwnd, LPTVITEM pitem)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs || !pitem || !(pitem->mask & TVIF_HANDLE) || !(pitem->hItem)) return FALSE;
  
  HTREEITEM ti = pitem->hItem;
  pitem->cChildren = ti->m_haschildren ? 1:0;
  pitem->lParam = ti->m_param;
  if ((pitem->mask&TVIF_TEXT)&&pitem->pszText&&pitem->cchTextMax>0)
  {
    lstrcpyn_safe(pitem->pszText,ti->m_value?ti->m_value:"",pitem->cchTextMax);
  }
  pitem->state=(ti == tvs->m_sel ? TVIS_SELECTED : 0) | (ti->m_state & TVIS_EXPANDED);
  
  return TRUE;
}

BOOL TreeView_SetItem(HWND hwnd, LPTVITEM pitem)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs || !pitem || !(pitem->mask & TVIF_HANDLE) || !(pitem->hItem)) return FALSE;

  if (!tvs->findItem(pitem->hItem,NULL,NULL)) return FALSE;
  
  HTREEITEM__ *ti = (HTREEITEM__*)pitem->hItem;
  
  if (pitem->mask & TVIF_CHILDREN) ti->m_haschildren = pitem->cChildren?1:0;
  if (pitem->mask & TVIF_PARAM)  ti->m_param =  pitem->lParam;
  
  if ((pitem->mask&TVIF_TEXT)&&pitem->pszText)
  {
    free(ti->m_value);
    ti->m_value=strdup(pitem->pszText);
    InvalidateRect(hwnd, 0, FALSE);
  }
 
  ti->m_state = (ti->m_state & ~pitem->stateMask) | (pitem->state & pitem->stateMask &~ TVIS_SELECTED);

  if (pitem->stateMask & pitem->state & TVIS_SELECTED)
  {
    tvs->m_sel = ti;
    static int __rent;
    if (!__rent)
    {
      __rent++;
      NMTREEVIEW nm={{hwnd,(UINT_PTR)hwnd->m_id,TVN_SELCHANGED},};
      SendMessage(GetParent(hwnd),WM_NOTIFY,nm.hdr.idFrom,(LPARAM)&nm);
      __rent--;
    }
  }

  InvalidateRect(hwnd,NULL,FALSE);
    
  return TRUE;
}

HTREEITEM TreeView_HitTest(HWND hwnd, TVHITTESTINFO *hti)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs || !hti || !tvs->m_last_row_height) return NULL;

  RECT r;
  GetClientRect(hwnd,&r);
  if (!PtInRect(&r,hti->pt)) return NULL;

  int y = hti->pt.y + tvs->m_scroll_y + tvs->m_last_row_height;
  return tvs->hitTestItem(&tvs->m_root,&y,NULL);
}

HTREEITEM TreeView_GetRoot(HWND hwnd)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs) return NULL;
  return tvs->m_root.m_children.Get(0);
}

HTREEITEM TreeView_GetChild(HWND hwnd, HTREEITEM item)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  if (!tvs) return NULL;
  return (item && item != TVI_ROOT ? item : &tvs->m_root)->m_children.Get(0);
}

HTREEITEM TreeView_GetNextSibling(HWND hwnd, HTREEITEM item)
{
  treeViewState *tvs = hwnd ? (treeViewState *)hwnd->m_private_data : NULL;
  
  HTREEITEM par=NULL;
  int idx=0;
  if (!tvs || !tvs->findItem(item,&par,&idx)) return NULL;

  return (par ? par : &tvs->m_root)->m_children.Get(idx+1);
}
BOOL TreeView_SetIndent(HWND hwnd, int indent)
{
  return FALSE;
}

void TreeView_SetBkColor(HWND hwnd, int color)
{
}
void TreeView_SetTextColor(HWND hwnd, int color)
{
}

void ListView_SetBkColor(HWND h, int color)
{
  if (h && h->m_private_data && h->m_classname && !strcmp(h->m_classname,"SysListView32"))
  {
    listViewState *lvs = (listViewState *)h->m_private_data;
    if (lvs) lvs->m_color_bg = color;
  }
}
void ListView_SetTextBkColor(HWND h, int color)
{
}
void ListView_SetTextColor(HWND h, int color)
{
  if (h && h->m_private_data && h->m_classname && !strcmp(h->m_classname,"SysListView32"))
  {
    listViewState *lvs = (listViewState *)h->m_private_data;
    lvs->m_color_text = color;
  }
}
void ListView_SetGridColor(HWND h, int color)
{
  if (h && h->m_private_data && h->m_classname && !strcmp(h->m_classname,"SysListView32"))
  {
    listViewState *lvs = (listViewState *)h->m_private_data;
    lvs->m_color_grid = color;
  }
}
void ListView_SetSelColors(HWND h, int *colors, int ncolors)
{
  if (h && h->m_private_data && h->m_classname && !strcmp(h->m_classname,"SysListView32"))
  {
    listViewState *lvs = (listViewState *)h->m_private_data;
    if (colors && ncolors > 0) 
      memcpy(lvs->m_color_extras,colors,wdl_min(ncolors*sizeof(int),sizeof(lvs->m_color_extras)));
  }
}
int ListView_GetTopIndex(HWND h)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs || !lvs->m_last_row_height) return 0;
  return lvs->m_scroll_y / lvs->m_last_row_height;
}
BOOL ListView_GetColumnOrderArray(HWND h, int cnt, int* arr)
{
  if (arr) for (int x=0;x<cnt;x++) arr[x]=x; // todo
  return FALSE;
}
BOOL ListView_SetColumnOrderArray(HWND h, int cnt, int* arr)
{
  return FALSE;
}
HWND ListView_GetHeader(HWND h)
{
  return h;
}

int Header_GetItemCount(HWND h)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  return lvs ? lvs->m_cols.GetSize() : 0;
}

BOOL Header_GetItem(HWND h, int col, HDITEM* hi)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return FALSE;
  if (col < 0 || col >= lvs->m_cols.GetSize()) return FALSE;

  const SWELL_ListView_Col *c = lvs->m_cols.Get() + col;
  if (hi->mask&HDI_FORMAT)
  {
    if (c->sortindicator<0) hi->fmt = HDF_SORTUP;
    else if (c->sortindicator>0) hi->fmt = HDF_SORTDOWN;
    else hi->fmt=0;
  }

  return TRUE;
}

BOOL Header_SetItem(HWND h, int col, HDITEM* hi)
{
  listViewState *lvs = h ? (listViewState *)h->m_private_data : NULL;
  if (!lvs) return FALSE;
  if (col < 0 || col >= lvs->m_cols.GetSize()) return FALSE;

  SWELL_ListView_Col *c = lvs->m_cols.Get() + col;
  if (hi->mask&HDI_FORMAT)
  {
    if (hi->fmt & HDF_SORTUP) c->sortindicator=-1;
    else if (hi->fmt & HDF_SORTDOWN) c->sortindicator=1;
    else c->sortindicator=0;
  }

  return TRUE;
}


BOOL EnumChildWindows(HWND hwnd, BOOL (*cwEnumFunc)(HWND,LPARAM),LPARAM lParam)
{
  if (hwnd && hwnd->m_children)
  {
    HWND n=hwnd->m_children;
    while (n)
    {
      if (!cwEnumFunc(n,lParam) || !EnumChildWindows(n,cwEnumFunc,lParam)) return FALSE;
      n = n->m_next;
    }
  }
  return TRUE;
}
void SWELL_GetDesiredControlSize(HWND hwnd, RECT *r)
{
}

BOOL SWELL_IsGroupBox(HWND hwnd)
{
  //todo
  return FALSE;
}
BOOL SWELL_IsButton(HWND hwnd)
{
  //todo
  return FALSE;
}
BOOL SWELL_IsStaticText(HWND hwnd)
{
  //todo
  return FALSE;
}


BOOL ShellExecute(HWND hwndDlg, const char *action,  const char *content1, const char *content2, const char *content3, int blah)
{
  const char *xdg = "/usr/bin/xdg-open";
  const char *argv[3] = { NULL };
  char *tmp=NULL;

  if (!content1 || !*content1) return FALSE;

  if (!strnicmp(content1,"http://",7) || !strnicmp(content1,"https://",8))
  {
    argv[0] = xdg;
    argv[1] = content1;
  }
  else if (!stricmp(content1,"explorer.exe")) 
  {
    const char *fn = content2;
    if (fn && !strnicmp(fn, "/select,\"", 9))
    {
      tmp = strdup(fn+9);
      if (*tmp && tmp[strlen(tmp)-1]=='\"') tmp[strlen(tmp)-1]='\0';
      WDL_remove_filepart(tmp);
      fn = tmp;
    }
    if (!fn || !*fn) return FALSE;

    argv[0] = xdg;
    argv[1] = fn;
  }
  else if (!stricmp(content1,"notepad.exe")||!stricmp(content1,"notepad"))
  { 
    if (!content2 || !*content2) return FALSE;
    argv[0] = xdg;
    argv[1] = content2;
  }
  else
  {
    if (content2 && *content2)
    {
      argv[0] = content1;
      argv[1] = content2;
    }
    else
    {
      argv[0] = xdg;
      argv[1] = content1; // default to xdg-open for whatever else
    }
  }

  if (fork() == 0) 
  {
    for (int x=0;argv[x];x++) argv[x] = strdup(argv[x]);
    execv(argv[0],(char *const*)argv);
    exit(0); // if execv fails for some reason
  }
  free(tmp);
  return TRUE;
}



static LRESULT WINAPI focusRectWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT r;
          GetClientRect(hwnd,&r);
          HBRUSH br = CreateSolidBrushAlpha(g_swell_ctheme.focusrect,0.5f);
          HPEN pen = CreatePen(0,PS_SOLID,g_swell_ctheme.focusrect);
          HGDIOBJ oldbr = SelectObject(ps.hdc,br);
          HGDIOBJ oldpen = SelectObject(ps.hdc,pen);
          Rectangle(ps.hdc,0,0,r.right,r.bottom);
          SelectObject(ps.hdc,oldbr);
          SelectObject(ps.hdc,oldpen);
          DeleteObject(br);
          DeleteObject(pen);
          EndPaint(hwnd,&ps);
        }
      }
    return 0;

  }
  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

// r=NULL to "free" handle
// otherwise r is in hwndPar coordinates
void SWELL_DrawFocusRect(HWND hwndPar, RECT *rct, void **handle)
{
  if (!handle) return;

  HWND h = (HWND) *handle;
  if (h && (!rct || h->m_parent != hwndPar))
  {
    DestroyWindow(h);
    h->Release();
    *handle = NULL;
    h = NULL;
  }

  if (rct)
  {
    if (!h)
    {
      h = new HWND__(hwndPar,0,rct,"",false,focusRectWndProc);
      h->m_style = WS_CHILD; // using this for top-level will also keep it out of the window list
      h->Retain();
      *handle = h;
      ShowWindow(h,SW_SHOWNA);
    }
    SetWindowPos(h,HWND_TOP,rct->left,rct->top,rct->right-rct->left,rct->bottom-rct->top,SWP_NOACTIVATE);
    InvalidateRect(h,NULL,FALSE);
  }

  if (hwndPar)
  {
    InvalidateRect(hwndPar,NULL,FALSE);
    UpdateWindow(hwndPar);
  } 
}

void SWELL_BroadcastMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  HWND h = SWELL_topwindows;
  while (h) 
  { 
    SendMessage(h,uMsg,wParam,lParam);
    if (uMsg == WM_DISPLAYCHANGE)
      InvalidateRect(h,NULL,FALSE);
    h = h->m_next;
  }
}

void SetOpaque(HWND h, bool opaque)
{
}
void SetAllowNoMiddleManRendering(HWND h, bool allow)
{
}
int SWELL_GetDefaultButtonID(HWND hwndDlg, bool onlyIfEnabled)
{
  return 0;
}

void SWELL_HideApp()
{
}

BOOL SWELL_GetGestureInfo(LPARAM lParam, GESTUREINFO* gi)
{
  return FALSE;
}

void SWELL_SetWindowWantRaiseAmt(HWND h, int  amt)
{
}
int SWELL_GetWindowWantRaiseAmt(HWND h)
{
  return 0;
}

// copied from swell-wnd.mm, can maybe have a common impl instead
void SWELL_GenerateDialogFromList(const void *_list, int listsz)
{
#define SIXFROMLIST list->p1,list->p2,list->p3, list->p4, list->p5, list->p6
  SWELL_DlgResourceEntry *list = (SWELL_DlgResourceEntry*)_list;
  while (listsz>0)
  {
    if (!strcmp(list->str1,"__SWELL_BUTTON"))
    {
      SWELL_MakeButton(list->flag1,list->str2, SIXFROMLIST);
    } 
    else if (!strcmp(list->str1,"__SWELL_EDIT"))
    {
      SWELL_MakeEditField(SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_COMBO"))
    {
      SWELL_MakeCombo(SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_LISTBOX"))
    {
      SWELL_MakeListBox(SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_GROUP"))
    {
      SWELL_MakeGroupBox(list->str2,SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_CHECKBOX"))
    {
      SWELL_MakeCheckBox(list->str2,SIXFROMLIST);
    }
    else if (!strcmp(list->str1,"__SWELL_LABEL"))
    {
      SWELL_MakeLabel(list->flag1, list->str2, SIXFROMLIST);
    }
    else if (*list->str2)
    {
      SWELL_MakeControl(list->str1, list->flag1, list->str2, SIXFROMLIST);
    }
    listsz--;
    list++;
  }
}

int swell_fullscreenWindow(HWND hwnd, BOOL fs)
{
  if (hwnd)
  {
    hwnd->m_oswindow_fullscreen = fs;
    return 1;
  }
  return 0;
}

void SWELL_SetClassName(HWND hwnd, const char *p)
{
  if (hwnd)
    hwnd->m_classname=p;
}

int GetClassName(HWND hwnd, char *buf, int bufsz)
{
  if (!hwnd || !hwnd->m_classname || !buf || bufsz<1) return 0;
  lstrcpyn_safe(buf,hwnd->m_classname,bufsz);
  return (int)strlen(buf);
}

#ifdef _DEBUG
void VALIDATE_HWND_LIST(HWND listHead, HWND par)
{
  if (!listHead) return;
  WDL_ASSERT(!listHead->m_prev);
  WDL_ASSERT(listHead->m_parent == par);
  WDL_ASSERT(listHead->m_next != listHead);
  HWND last = listHead;
  HWND list = listHead->m_next;
  while (list)
  {
    WDL_ASSERT(list->m_prev == last);
    WDL_ASSERT(list->m_parent == par);
    last = list;
    list = list->m_next;
  }
}
#endif



#endif
