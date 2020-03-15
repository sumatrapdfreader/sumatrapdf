/*
    WDL - virtwnd.h
    Copyright (C) 2006 and later Cockos Incorporated

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
      

    This file provides interfaces for the WDL Virtual Windows layer, a system that allows
    creating many controls within one system device context.

    The base class is a WDL_VWnd.

    If you create a WDL_VWnd, you should send it (or its parent) mouse messages etc.

    To paint a WDL_VWnd, use a WDL_VWnd_Painter in WM_PAINT etc.


    More documentation should follow...
*/



#ifndef _WDL_VIRTWND_H_
#define _WDL_VIRTWND_H_

#ifdef _WIN32
#include <windows.h>
#else
#include "../swell/swell.h"
#endif
#include "../ptrlist.h"
#include "../wdlstring.h"



class WDL_VWnd_Painter;
class LICE_IBitmap;

#define WDL_VWND_SCALEBASE 256 // .8 fixed point scale

// deprecated
#define WDL_VirtualWnd_ChildList WDL_VWnd
#define WDL_VirtualWnd WDL_VWnd
#define WDL_VirtualWnd_Painter WDL_VWnd_Painter

class WDL_VWnd;

class WDL_VWnd_IAccessibleBridge
{
public:
  virtual void Release()=0;
  virtual void ClearCaches(){}
  virtual void OnFocused() {} 
  virtual void OnStateChange() {}
protected:
  virtual ~WDL_VWnd_IAccessibleBridge(){}
};


#include "../destroycheck.h"
#define WDL_VWND_DCHK(n) WDL_DestroyCheck n(&m_destroystate)

class WDL_VWnd
{
public:
  WDL_VWnd();
  virtual ~WDL_VWnd();
  virtual const char *GetType() { return "vwnd_unknown"; }

  virtual void SetID(int id) { m_id=id; }
  virtual int GetID() { return m_id; }
  virtual INT_PTR GetUserData() { return m_userdata; }
  virtual INT_PTR SetUserData(INT_PTR ud) { INT_PTR od=m_userdata; m_userdata=ud; return od; }
  virtual void SetPosition(const RECT *r) { m_position=*r; }
  virtual void GetPosition(RECT *r) { *r=m_position; }
  virtual void GetPositionPaintExtent(RECT *r, int rscale) { *r=m_position; ScaleRect(r,rscale); }
  virtual void GetPositionPaintOverExtent(RECT *r, int rscale) { *r=m_position; ScaleRect(r,rscale); }
  virtual void SetVisible(bool vis) { m_visible=vis; }
  virtual bool IsVisible() { return m_visible; }
  virtual bool WantsPaintOver() { return m_children && m_children->GetSize(); }
  virtual WDL_VWnd *GetParent() { return m_parent; }
  virtual void SetParent(WDL_VWnd *par) { m_parent=par; }

  virtual void RequestRedraw(RECT *r); 
  virtual void OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale);
  virtual void OnPaintOver(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale);

  virtual int OnMouseDown(int xpos, int ypos); // return -1 to eat, >0 to capture
  virtual bool OnMouseDblClick(int xpos, int ypos);
  virtual bool OnMouseWheel(int xpos, int ypos, int amt);

  virtual void OnMouseMove(int xpos, int ypos);
  virtual void OnMouseUp(int xpos, int ypos);

  // child windows
  virtual WDL_VWnd *EnumChildren(int x);
  virtual int GetNumChildren();
  virtual WDL_VWnd *GetChildByID(int id);
  virtual void AddChild(WDL_VWnd *wnd, int pos=-1);
  virtual void RemoveChild(WDL_VWnd *wnd, bool dodel=false);
  virtual void RemoveAllChildren(bool dodel=true);
  virtual WDL_VWnd *GetCaptureWnd() { return m_children ? m_children->Get(m_captureidx) : 0; }
  virtual WDL_VWnd *VirtWndFromPoint(int xpos, int ypos, int maxdepth=-1); // maxdepth=0 only direct children, etc, -1 is unlimited

  // OS access
  virtual HWND GetRealParent() { if (m_realparent) return m_realparent; if (GetParent()) return GetParent()->GetRealParent(); return 0; }
  virtual void SetRealParent(HWND par) { m_realparent=par; }

  virtual INT_PTR SendCommand(int command, INT_PTR parm1, INT_PTR parm2, WDL_VWnd *src);

  // request if window has cursor
  virtual int UpdateCursor(int xpos, int ypos); // >0 if set, 0 if cursor wasnt set , <0 if cursor should be default...
  virtual bool GetToolTipString(int xpos, int ypos, char *bufOut, int bufOutSz); // true if handled

  virtual void GetPositionInTopVWnd(RECT *r);

  // these do not store a copy, usually you set them to static strings etc, but a control can override, too...
  virtual void SetAccessDesc(const char *desc) { m__iaccess_desc=desc; }
  virtual const char *GetAccessDesc() { return m__iaccess_desc; }

  virtual WDL_VWnd_IAccessibleBridge *GetAccessibilityBridge() { return m__iaccess; }
  virtual void SetAccessibilityBridge(WDL_VWnd_IAccessibleBridge *br) { m__iaccess=br; }

  virtual void SetChildPosition(WDL_VWnd *ch, int pos);
  
  virtual void SetCurPainter(WDL_VWnd_Painter *p) { m_curPainter=p; }
  virtual bool IsDescendent(WDL_VWnd *w);

  virtual void OnCaptureLost();

  virtual bool GetAccessValueDesc(char *buf, int bufsz) { return false; } // allow control to format value string

  static void ScaleRect(RECT *r, int sc)
  {
    if (sc != WDL_VWND_SCALEBASE)
    {
      r->left = r->left * sc / WDL_VWND_SCALEBASE;
      r->top = r->top * sc / WDL_VWND_SCALEBASE;
      r->right = r->right * sc / WDL_VWND_SCALEBASE;
      r->bottom = r->bottom * sc / WDL_VWND_SCALEBASE;
    }
  }

protected:
  WDL_VWnd *m_parent;
  WDL_VWnd_IAccessibleBridge *m__iaccess;
  bool m_visible;
  int m_id;
  RECT m_position;
  INT_PTR m_userdata;

  HWND m_realparent;
  int m_captureidx;
  int m_lastmouseidx;
  WDL_PtrList<WDL_VWnd> *m_children;

  const char *m__iaccess_desc;

  WDL_VWnd_Painter *m_curPainter;
  virtual int GSC(int a);

  WDL_DestroyState m_destroystate;
};


// painting object (can be per window or per thread or however you like)
#define WDL_VWP_SUNKENBORDER 0x00010000
#define WDL_VWP_SUNKENBORDER_NOTOP 0x00020000
#define WDL_VWP_DIVIDER_VERT 0x00030000
#define WDL_VWP_DIVIDER_HORZ 0x00040000


#include "virtwnd-skin.h"

class WDL_VWnd_Painter
{
public:
  WDL_VWnd_Painter();
  ~WDL_VWnd_Painter();


  void SetGSC(int (*GSC)(int));
  void PaintBegin(HWND hwnd, int bgcolor=-1, const RECT *limitBGrect=NULL, const RECT *windowRect=NULL, HDC hdcOut=NULL, const RECT *clip_r=NULL); // if hwnd is NULL, windowRect/hdcOut/clip_r must be set
  void SetBGImage(WDL_VirtualWnd_BGCfg *bitmap, int tint=-1, WDL_VirtualWnd_BGCfgCache *cacheObj=NULL, bool tintUnderMode=false) // call before every paintbegin (resets if you dont)
  { 
    m_bgbm=bitmap; 
    m_bgbmtintcolor=tint; 
    m_bgcache=cacheObj; 
    m_bgbmtintUnderMode = tintUnderMode;
  } 
  void SetBGGradient(int wantGradient, double start, double slope); // wantg < 0 to use system defaults

  void PaintBGCfg(WDL_VirtualWnd_BGCfg *bitmap, const RECT *coords, bool allowTint=true, float alpha=1.0, int mode=0);
  void PaintVirtWnd(WDL_VWnd *vwnd, int borderflags=0, int x_xlate=0, int y_xlate=0);
  void PaintBorderForHWND(HWND hwnd, int borderflags);
  void PaintBorderForRect(const RECT *r, int borderflags);

  void GetPaintInfo(RECT *rclip, int *xoffsdraw, int *yoffsdraw);
  void SetRenderScale(int render_scale, int advisory_scale=WDL_VWND_SCALEBASE) { m_render_scale = render_scale; m_advisory_scale = advisory_scale; }
  int GetRenderScale() const { return m_render_scale; }

  void RenderScaleRect(RECT *r) const
  {
    WDL_VWnd::ScaleRect(r,m_render_scale);
  }

  LICE_IBitmap *GetBuffer(int *xo, int *yo) 
  { 
    *xo = -m_paint_xorig;
    *yo = -m_paint_yorig;
    return m_bm; 
  }

  void PaintEnd(int xoffs=0, int yoffs=0);

  int GSC(int a);
private:

  double m_gradstart,m_gradslope;

  int m_wantg;
  int (*m_GSC)(int);
  void DoPaintBackground(LICE_IBitmap *bmOut, int bgcolor, const RECT *clipr, int wnd_w, int wnd_h, int xoffs, int yoffs);
  void tintRect(LICE_IBitmap *bmOut, const RECT *clipr, int xoffs, int yoffs, bool isCopy);
  LICE_IBitmap *m_bm;
  WDL_VirtualWnd_BGCfg *m_bgbm;
  int m_bgbmtintcolor;
  bool m_bgbmtintUnderMode;
  int m_render_scale, m_advisory_scale;

  WDL_VirtualWnd_BGCfgCache *m_bgcache;
  HWND m_cur_hwnd;
public:
  PAINTSTRUCT m_ps;
  int m_paint_xorig, m_paint_yorig;

};

void WDL_VWnd_regHelperClass(const char *classname, void *icon=NULL, void *iconsm=NULL); // register this class if you wish to make your dialogs use it (better paint behavior)

// in virtwnd-iaccessible.cpp
LRESULT WDL_AccessibilityHandleForVWnd(bool isDialog, HWND hwnd, WDL_VWnd *vw, WPARAM wParam, LPARAM lParam);

extern bool wdl_virtwnd_nosetcursorpos; // set to true to prevent SetCursorPos() from being called in sliders/etc (for pen/tablet mode)

#endif
