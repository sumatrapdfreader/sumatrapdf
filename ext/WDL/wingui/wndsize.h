/*
    WDL - wndsize.h
    Copyright (C) 2004 and later Cockos Incorporated

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

/*

  This file provides the interface for a simple class that allows one to easily 
  make resizeable dialogs and have controls that move according to ratios of the 
  new size.

  Usually, one does a 

  static WDL_WndSizer resize;

  in their DlgProc, and in WM_INITDIALOG:

        resize.init(hwndDlg);

        // add a list of items
        resize.init_item(IDC_MASTERVOL,  // dialog id
                              0.0,  // left position, 0=left anchor, 1=right anchor, etc
                              0.0, // top position, 0=anchored to its initial top position, 1=anchored to distance from bottom, etc
                              0.7f,  // right position
                              0.0);  // bottom position
        ...


  then, in WM_SIZE,
      if (wParam != SIZE_MINIMIZED) 
      {
        resize.onResize(); // don't actually resize, just compute the rects
      }


  is about all that's needed.

   
*/

/*
 * Additional note from late-2019
 * If on Windows and Multimonitor v2 DPI awareness mode is enabled
 * The coordinates for WDL_WndSizer__rec will be based on the DPI-scaling at init() time, which is m_base_dpi.
 *
 * One can use dpi_to_sizer() and sizer_to_dpi() to convert to/from (if DPI-scaling is disabled, then these functions no-op)
 *
 * If m_base_dpi is set to <=0 this functionality is disabled (disable if you want to manage this all at the caller level)
 */

#ifndef _WNDSIZE_H_
#define _WNDSIZE_H_


#include "../heapbuf.h"

class WDL_VWnd;

struct WDL_WndSizer__rec
{
  HWND hwnd;
  RECT orig;
  RECT real_orig;
  RECT last;
  float scales[4];
  WDL_VWnd *vwnd;
};

class WDL_WndSizer
{
public:
  WDL_WndSizer() 
  { 
    m_hwnd=NULL; 
    m_base_dpi=256;
    memset(&m_min_size,0,sizeof(m_min_size));
    memset(&m_orig_size,0,sizeof(m_orig_size));
    memset(&m_margins,0,sizeof(m_margins));
  }
  ~WDL_WndSizer() { }

  void init(HWND hwndDlg, RECT *initr=NULL);

  // 1.0 means it moves completely with the point, 0.0 = not at all
  void init_item(int dlg_id, float left_scale=0.0, float top_scale=0.0, float right_scale=1.0, float bottom_scale=1.0, RECT *initr=NULL);
  void init_itemvirt(WDL_VWnd *vwnd, float left_scale=0.0, float top_scale=0.0, float right_scale=1.0, float bottom_scale=1.0);
  void init_item(int dlg_id, float *scales, RECT *initr=NULL);
  void init_itemvirt(WDL_VWnd *vwnd, float *scales);
  void init_itemhwnd(HWND h, float left_scale=0.0, float top_scale=0.0, float right_scale=1.0, float bottom_scale=1.0, RECT *srcr=NULL);
  void remove_item(int dlg_id);
  void remove_itemvirt(WDL_VWnd *vwnd);
  void remove_itemhwnd(HWND h);

  WDL_WndSizer__rec *get_item(int dlg_id) const;
  WDL_WndSizer__rec *get_itembyindex(int idx) const;
  WDL_WndSizer__rec *get_itembywnd(HWND h) const;
  WDL_WndSizer__rec *get_itembyvirt(WDL_VWnd *vwnd) const;
  
  RECT get_orig_rect() const { RECT r={0,0,m_orig_size.x,m_orig_size.y}; return r; }
  RECT get_orig_rect_dpi(int dpi=0) const {
    RECT r = get_orig_rect();
    sizer_to_dpi_rect(&r,dpi);
    return r;
  }
  void set_orig_rect(const RECT *r, const POINT *minSize=NULL) 
  {
    if (r) { m_orig_size.x = r->right; m_orig_size.y = r->bottom; } 
    if (minSize) m_min_size = *minSize;
    else m_min_size.x=m_min_size.y=0;
  }
  POINT get_min_size(bool applyMargins=false) const;

  void onResize(HWND only=0, int notouch=0, int xtranslate=0, int ytranslate=0);
  void onResizeVirtual(int width, int height);


  void set_margins(int left, int top, int right, int bottom) { m_margins.left=left; m_margins.top=top; m_margins.right=right; m_margins.bottom=bottom; }
  void get_margins(int *left, int *top, int *right, int *bottom) const 
  {
    if (left) *left=m_margins.left;
    if (top) *top=m_margins.top;
    if (right) *right=m_margins.right;
    if (bottom) *bottom=m_margins.bottom;
  }

  void transformRect(RECT *r, const float *scales, const RECT *wndSize) const;

  int m_base_dpi; // if set <=0, all DPI functionality will be skipped. 256=normal
  static int calc_dpi(HWND hwnd); // returns 0 if not windows or not in multimonitorv2 awareness mode
  bool sizer_to_dpi_rect(RECT *r, int dpi=0) const; // returns true if modified, converts to current DPI from sizer base DPI

  int dpi_to_sizer(int x, int dpi=0) const // convert dpi-specific coordinate to sizer coordinate (orig.left etc) if in DPI mode
  {
    if (m_base_dpi<=0 || !x) return x;
    if (dpi<=0) dpi=calc_dpi(m_hwnd);
    return dpi>0 && dpi != m_base_dpi ? x * m_base_dpi / dpi : x;
  }
  int sizer_to_dpi(int x, int dpi=0) const // convert sizer coordinate (orig.left etc) to dpi-specific coordinate, if in DPI mode
  {
    if (m_base_dpi<=0 || !x) return x;
    if (dpi<=0) dpi=calc_dpi(m_hwnd);
    return dpi>0 && dpi != m_base_dpi ? x * dpi / m_base_dpi : x;
  }

private:
#ifdef _WIN32
  static BOOL CALLBACK enum_RegionRemove(HWND hwnd,LPARAM lParam);
  HRGN m_enum_rgn;
#endif
  HWND m_hwnd;
  POINT m_orig_size,m_min_size;
  RECT m_margins;

  WDL_TypedBuf<WDL_WndSizer__rec> m_list;

};

#endif//_WNDSIZE_H_