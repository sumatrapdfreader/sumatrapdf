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
  

    SWELL provides _EXTREMELY BASIC_ win32 wrapping for OS X and maybe other platforms.

  */


#ifndef _WIN32


#ifndef _WDL_SWELL_H_ // here purely for apps/other libraries (dirscan.h uses it), each section actually has its own define
#define _WDL_SWELL_H_


#if defined(__APPLE__) && !defined(SWELL_FORCE_GENERIC)
#define SWELL_TARGET_OSX
#define SWELL_TARGET_OSX_COCOA
#endif

// for swell*generic
// #define SWELL_TARGET_GDK
// #define SWELL_LICE_GDI

#endif

#ifdef __APPLE__
// go ahead and get this included before we define FSHIFT in swell-types.h
#include <sys/param.h>
#endif

// IF YOU ADD TO SWELL:
// Adding types, defines, etc: add to swell-types.h 
// Adding functions: put them in swell-functions.h


#include "swell-types.h"
#include "swell-functions.h"


#ifndef SWELL_PROVIDED_BY_APP
#ifndef _WDL_SWELL_H_UTIL_DEFINED_
#define _WDL_SWELL_H_UTIL_DEFINED_

// these should never be called directly!!! put SWELL_POSTMESSAGE_DELEGATE_IMPL in your nsapp delegate, and call SWELL_POSTMESSAGE_INIT at some point from there too
                 
#define SWELL_POSTMESSAGE_INIT SWELL_Internal_PostMessage_Init();
#define SWELL_POSTMESSAGE_DELEGATE_IMPL \
                 -(bool)swellPostMessage:(HWND)dest msg:(int)message wp:(WPARAM)wParam lp:(LPARAM)lParam { \
                   return SWELL_Internal_PostMessage(dest,message,wParam,lParam); \
                 } \
                 -(void)swellPostMessageClearQ:(HWND)dest { \
                   SWELL_Internal_PMQ_ClearAllMessages(dest); \
                 } \
                 -(void)swellPostMessageTick:(id)sender { \
                   SWELL_MessageQueue_Flush(); \
                 } 
                 
BOOL SWELL_Internal_PostMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void SWELL_Internal_PMQ_ClearAllMessages(HWND hwnd);


// if you use this then include swell-appstub.mm in your project
#define SWELL_APPAPI_DELEGATE_IMPL \
                -(void *)swellGetAPPAPIFunc { \
                    void *SWELLAPI_GetFunc(const char *name); \
                    return (void*)SWELLAPI_GetFunc; \
                }

#endif // _WDL_SWELL_H_UTIL_DEFINED_
#endif // !SWELL_PROVIDED_BY_APP

#endif // !_WIN32


#ifndef SWELL_TARGET_OSX

#ifndef SWELL_CB_InsertString

#define SWELL_CB_InsertString(hwnd, idx, pos, str) ((int)SendDlgItemMessage(hwnd,idx,CB_INSERTSTRING,(pos),(LPARAM)(str)))
#define SWELL_CB_AddString(hwnd, idx, str) ((int)SendDlgItemMessage(hwnd,idx,CB_ADDSTRING,0,(LPARAM)(str)))
#define SWELL_CB_SetCurSel(hwnd,idx,val) ((int)SendDlgItemMessage(hwnd,idx,CB_SETCURSEL,(WPARAM)(val),0))
#define SWELL_CB_GetNumItems(hwnd,idx) ((int)SendDlgItemMessage(hwnd,idx,CB_GETCOUNT,0,0))
#define SWELL_CB_GetCurSel(hwnd,idx) ((int)SendDlgItemMessage(hwnd,idx,CB_GETCURSEL,0,0))
#define SWELL_CB_SetItemData(hwnd,idx,item,val) SendDlgItemMessage(hwnd,idx,CB_SETITEMDATA,(item),(val))
#define SWELL_CB_GetItemData(hwnd,idx,item) SendDlgItemMessage(hwnd,idx,CB_GETITEMDATA,(item),0)
#define SWELL_CB_GetItemText(hwnd,idx,item,buf,bufsz) SendDlgItemMessage(hwnd,idx,CB_GETLBTEXT,(item),(LPARAM)(buf))
#define SWELL_CB_Empty(hwnd,idx) SendDlgItemMessage(hwnd,idx,CB_RESETCONTENT,0,0)
#define SWELL_CB_DeleteString(hwnd,idx,str) SendDlgItemMessage(hwnd,idx,CB_DELETESTRING,str,0)

#define SWELL_TB_SetPos(hwnd, idx, pos) SendDlgItemMessage(hwnd,idx, TBM_SETPOS,TRUE,(pos))
#define SWELL_TB_SetRange(hwnd, idx, low, hi) SendDlgItemMessage(hwnd,idx,TBM_SETRANGE,TRUE,(LPARAM)MAKELONG((low),(hi)))
#define SWELL_TB_GetPos(hwnd, idx) ((int)SendDlgItemMessage(hwnd,idx,TBM_GETPOS,0,0))
#define SWELL_TB_SetTic(hwnd, idx, pos) SendDlgItemMessage(hwnd,idx,TBM_SETTIC,0,(pos))

#endif

#endif// !SWELL_TARGET_OSX




#ifndef WDL_GDP_CTX                // stupid GDP compatibility layer, deprecated


#define WDL_GDP_CTX HDC
#define WDL_GDP_PEN HPEN
#define WDL_GDP_BRUSH HBRUSH
#define WDL_GDP_CreatePen(col, wid) (WDL_GDP_PEN)CreatePen(PS_SOLID,(wid),(col))
#define WDL_GDP_DeletePen(pen) DeleteObject((HGDIOBJ)(pen))
#define WDL_GDP_SetPen(ctx, pen) ((WDL_GDP_PEN)SelectObject(ctx,(HGDIOBJ)(pen)))
#define WDL_GDP_SetBrush(ctx, brush) ((WDL_GDP_BRUSH)SelectObject(ctx,(HGDIOBJ)(brush)))
#define WDL_GDP_CreateBrush(col) (WDL_GDP_BRUSH)CreateSolidBrush(col)
#define WDL_GDP_DeleteBrush(brush) DeleteObject((HGDIOBJ)(brush))
#define WDL_GDP_FillRectWithBrush(hdc,r,br) FillRect(hdc,r,(HBRUSH)(br))
#define WDL_GDP_Rectangle(hdc,l,t,r,b) Rectangle(hdc,l,t,r,b)
#define WDL_GDP_Polygon(hdc,pts,n) Polygon(hdc,pts,n)
#define WDL_GDP_MoveToEx(hdc,x,y,op) MoveToEx(hdc,x,y,op)
#define WDL_GDP_LineTo(hdc,x,y) LineTo(hdc,x,y)
#define WDL_GDP_PutPixel(hdc,x,y,c) SetPixel(hdc,x,y,c)
#define WDL_GDP_PolyBezierTo(hdc,p,np) PolyBezierTo(hdc,p,np)

#define SWELL_SyncCtxFrameBuffer(x) // no longer used

#endif

#if !defined(SWELL_AUTORELEASE_HELPER_DEFINED) && defined(__cplusplus) && (!defined(SWELL_TARGET_OSX) || defined(SWELL_API_DEFINE))
#define SWELL_AUTORELEASE_HELPER_DEFINED

class SWELL_AutoReleaseHelper  // no-op on non-apple
{
#ifdef SWELL_TARGET_OSX
    void *m_arp;
#endif
  public:
    SWELL_AutoReleaseHelper() 
    {
#ifdef SWELL_TARGET_OSX
      m_arp = SWELL_InitAutoRelease();
#endif
    }
    ~SWELL_AutoReleaseHelper() 
    { 
#ifdef SWELL_TARGET_OSX
      release(); 
#endif
    }

    void release()
    {
#ifdef SWELL_TARGET_OSX
      if (m_arp) { SWELL_QuitAutoRelease(m_arp); m_arp=NULL; }
#endif
    }

};

#endif

#if defined(_WIN32) && !defined(LoadLibraryGlobals)
#define LoadLibraryGlobals(a,b) LoadLibrary(a)
#endif

