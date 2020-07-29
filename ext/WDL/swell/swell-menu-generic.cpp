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
  

    This file provides basic windows menu API

  */

#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"
#include "swell-menugen.h"

#include "swell-internal.h"

#include "../ptrlist.h"
#include "../wdlcstring.h"

static bool MenuIsStringType(const MENUITEMINFO *inf)
{
  return inf->fType == MFT_STRING || inf->fType == MFT_RADIOCHECK;
}

HMENU__ *HMENU__::Duplicate()
{
  HMENU__ *p = new HMENU__;
  int x;
  for (x = 0; x < items.GetSize(); x ++)
  {
    MENUITEMINFO *s = items.Get(x);
    MENUITEMINFO *inf = (MENUITEMINFO*)calloc(sizeof(MENUITEMINFO),1);

    *inf = *s;
    if (inf->dwTypeData && MenuIsStringType(inf)) inf->dwTypeData=strdup(inf->dwTypeData);
    if (inf->hSubMenu) inf->hSubMenu = inf->hSubMenu->Duplicate();

    p->items.Add(inf);
  }
  return p;
}

void HMENU__::freeMenuItem(void *p)
{
  MENUITEMINFO *inf = (MENUITEMINFO *)p;
  if (!inf) return;
  if (inf->hSubMenu) inf->hSubMenu->Release();
  if (MenuIsStringType(inf)) free(inf->dwTypeData);
  free(inf);
}

static MENUITEMINFO *GetMenuItemByID(HMENU menu, int id, bool searchChildren=true)
{
  if (WDL_NOT_NORMALLY(!menu)) return 0;
  int x;
  for (x = 0; x < menu->items.GetSize(); x ++)
    if (menu->items.Get(x)->wID == (UINT)id) return menu->items.Get(x);

  if (searchChildren) for (x = 0; x < menu->items.GetSize(); x ++)
  { 
    if (menu->items.Get(x)->hSubMenu)
    {
      MENUITEMINFO *ret = GetMenuItemByID(menu->items.Get(x)->hSubMenu,id,true);
      if (ret) return ret;
    }
  }

  return 0;
}

bool SetMenuItemText(HMENU hMenu, int idx, int flag, const char *text)
{
  MENUITEMINFO *item = WDL_NORMALLY(hMenu) ? ((flag & MF_BYPOSITION) ? hMenu->items.Get(idx) : GetMenuItemByID(hMenu,idx)) : NULL;
  if (!item) return false;

  if (MenuIsStringType(item)) free(item->dwTypeData);
  else item->fType = MFT_STRING;
  item->dwTypeData=strdup(text?text:"");
  
  return true;
}

bool EnableMenuItem(HMENU hMenu, int idx, int en)
{
  MENUITEMINFO *item = WDL_NORMALLY(hMenu) ? ((en & MF_BYPOSITION) ? hMenu->items.Get(idx) : GetMenuItemByID(hMenu,idx)) : NULL;
  if (!item) return false;
 
  int mask = MF_ENABLED|MF_DISABLED|MF_GRAYED;
  item->fState &= ~mask;
  item->fState |= en&mask;

  return true;
}

bool CheckMenuItem(HMENU hMenu, int idx, int chk)
{
  MENUITEMINFO *item = WDL_NORMALLY(hMenu) ? ((chk & MF_BYPOSITION) ? hMenu->items.Get(idx) : GetMenuItemByID(hMenu,idx)) : NULL;
  if (!item) return false;
  
  int mask = MF_CHECKED;
  item->fState &= ~mask;
  item->fState |= chk&mask;
  
  return true;
}
HMENU SWELL_GetCurrentMenu()
{
  return NULL; // not osx
}
void SWELL_SetCurrentMenu(HMENU hmenu)
{
}

HMENU GetSubMenu(HMENU hMenu, int pos)
{
  MENUITEMINFO *item = WDL_NORMALLY(hMenu) ? hMenu->items.Get(pos) : NULL;
  if (item) return item->hSubMenu;
  return 0;
}

int GetMenuItemCount(HMENU hMenu)
{
  if (WDL_NORMALLY(hMenu)) return hMenu->items.GetSize();
  return 0;
}

int GetMenuItemID(HMENU hMenu, int pos)
{
  MENUITEMINFO *item = WDL_NORMALLY(hMenu) ? hMenu->items.Get(pos) : NULL;
  if (!item)
  {
    WDL_ASSERT(pos==0); // do not assert if GetMenuItemID(0) is called on an empty menu
    return -1;
  }
  if (item->hSubMenu) return -1;
  return item->wID;
}

bool SetMenuItemModifier(HMENU hMenu, int idx, int flag, int code, unsigned int mask)
{
  return false;
}

HMENU CreatePopupMenu()
{
  return new HMENU__;
}
HMENU CreatePopupMenuEx(const char *title)
{
  return CreatePopupMenu();
}

void DestroyMenu(HMENU hMenu)
{
  if (WDL_NORMALLY(hMenu)) hMenu->Release();
}

int AddMenuItem(HMENU hMenu, int pos, const char *name, int tagid)
{
  if (WDL_NOT_NORMALLY(!hMenu)) return -1;
  MENUITEMINFO *inf = (MENUITEMINFO*)calloc(1,sizeof(MENUITEMINFO));
  inf->wID = tagid;
  inf->fType = MFT_STRING;
  inf->dwTypeData = strdup(name?name:"");
  hMenu->items.Insert(pos,inf);
  return 0;
}

bool DeleteMenu(HMENU hMenu, int idx, int flag)
{
  if (WDL_NOT_NORMALLY(!hMenu)) return false;
  if (flag&MF_BYPOSITION)
  {
    if (hMenu->items.Get(idx))
    {
      hMenu->items.Delete(idx,true,HMENU__::freeMenuItem);
      return true;
    }
    return false;
  }
  else
  {
    int x;
    int cnt=0;
    for (x=0;x<hMenu->items.GetSize(); x ++)
    {
      if (!hMenu->items.Get(x)->hSubMenu && hMenu->items.Get(x)->wID == (UINT)idx)
      {
        hMenu->items.Delete(x--,true,HMENU__::freeMenuItem);
        cnt++;
      }
    }
    if (!cnt)
    {
      for (x=0;x<hMenu->items.GetSize(); x ++)
      {
        if (hMenu->items.Get(x)->hSubMenu) cnt += DeleteMenu(hMenu->items.Get(x)->hSubMenu,idx,flag)?1:0;
      }
    }
    return !!cnt;
  }
}


BOOL SetMenuItemInfo(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi)
{
  if (WDL_NOT_NORMALLY(!hMenu)) return 0;
  MENUITEMINFO *item = byPos ? hMenu->items.Get(pos) : GetMenuItemByID(hMenu, pos, true);
  if (!item) return 0;
  
  if ((mi->fMask & MIIM_SUBMENU) && mi->hSubMenu != item->hSubMenu)
  {  
    if (item->hSubMenu) item->hSubMenu->Release();
    item->hSubMenu = mi->hSubMenu;
  } 
  if (mi->fMask & MIIM_TYPE)
  {
    const bool wasString = MenuIsStringType(item), isString = MenuIsStringType(mi);
    if (wasString != isString)
    {
      if (wasString) free(item->dwTypeData);
      item->dwTypeData=NULL;
    }

    if (mi->fType == MFT_BITMAP) item->dwTypeData = mi->dwTypeData;
    else if (isString && mi->dwTypeData)
    {
      free(item->dwTypeData);
      item->dwTypeData = strdup(mi->dwTypeData);
    }
    item->fType = mi->fType;
  }

  if (mi->fMask & MIIM_STATE) item->fState = mi->fState;
  if (mi->fMask & MIIM_ID) item->wID = mi->wID;
  if (mi->fMask & MIIM_DATA) item->dwItemData = mi->dwItemData;
  if ((mi->fMask & MIIM_BITMAP) && mi->cbSize >= sizeof(*mi)) item->hbmpItem = mi->hbmpItem;
  
  return true;
}

BOOL GetMenuItemInfo(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi)
{
  if (WDL_NOT_NORMALLY(!hMenu)) return 0;
  MENUITEMINFO *item = byPos ? hMenu->items.Get(pos) : GetMenuItemByID(hMenu, pos, true);
  if (!item) return 0;
  
  if (mi->fMask & MIIM_TYPE)
  {
    mi->fType = item->fType;
    if (MenuIsStringType(mi) && mi->dwTypeData && mi->cch)
    {
      lstrcpyn_safe(mi->dwTypeData,item->dwTypeData?item->dwTypeData:"",mi->cch);
    }
    else if (item->fType == MFT_BITMAP) mi->dwTypeData = item->dwTypeData;
  }
  
  if (mi->fMask & MIIM_DATA) mi->dwItemData = item->dwItemData;
  if (mi->fMask & MIIM_STATE) mi->fState = item->fState;
  if (mi->fMask & MIIM_ID) mi->wID = item->wID;
  if (mi->fMask & MIIM_SUBMENU) mi->hSubMenu = item->hSubMenu;
  if ((mi->fMask & MIIM_BITMAP) && mi->cbSize >= sizeof(*mi)) mi->hbmpItem = item->hbmpItem;
  
  return 1;
  
}

void SWELL_InsertMenu(HMENU menu, int pos, unsigned int flag, UINT_PTR idx, const char *str)
{
  MENUITEMINFO mi={sizeof(mi),MIIM_ID|MIIM_STATE|MIIM_TYPE,MFT_STRING,
    (flag & ~MF_BYPOSITION),(flag&MF_POPUP) ? 0 : (UINT)idx,NULL,NULL,NULL,0,(char *)str};
  
  if (flag&MF_POPUP) 
  {
    mi.hSubMenu = (HMENU)idx;
    mi.fMask |= MIIM_SUBMENU;
    mi.fState &= ~MF_POPUP;
  }
  
  if (flag&MF_SEPARATOR)
  {
    mi.fMask=MIIM_TYPE;
    mi.fType=MFT_SEPARATOR;
    mi.fState &= ~MF_SEPARATOR;
  }

  if (flag&MF_BITMAP)
  {
    mi.fType=MFT_BITMAP;
    mi.fState &= ~MF_BITMAP;
  }
    
  InsertMenuItem(menu,pos,(flag&MF_BYPOSITION) ?  TRUE : FALSE, &mi);
}

void InsertMenuItem(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi)
{
  if (WDL_NOT_NORMALLY(!hMenu)) return;
  int ni=hMenu->items.GetSize();
  
  if (!byPos) 
  {
    int x;
    for (x=0;x<ni && hMenu->items.Get(x)->wID != (UINT)pos; x++);
    pos = x;
  }
  if (pos < 0 || pos > ni) pos=ni; 
  
  MENUITEMINFO *inf = (MENUITEMINFO*)calloc(sizeof(MENUITEMINFO),1);
  inf->fType = mi->fType;
  if (MenuIsStringType(inf))
  {
    inf->dwTypeData = strdup(mi->dwTypeData?mi->dwTypeData:"");
  }
  else if (mi->fType == MFT_BITMAP)
  {
    inf->dwTypeData = mi->dwTypeData;
  }
  else if (mi->fType == MFT_SEPARATOR)
  {
  }
  if (mi->fMask&MIIM_SUBMENU) inf->hSubMenu = mi->hSubMenu;
  if (mi->fMask & MIIM_STATE) inf->fState = mi->fState;
  if (mi->fMask & MIIM_DATA) inf->dwItemData = mi->dwItemData;
  if (mi->fMask & MIIM_ID) inf->wID = mi->wID;
  if ((mi->fMask & MIIM_BITMAP) && mi->cbSize >= sizeof(*mi)) inf->hbmpItem = mi->hbmpItem;

  hMenu->items.Insert(pos,inf);
}


void SWELL_SetMenuDestination(HMENU menu, HWND hwnd)
{
  // only needed for Cocoa
}

extern RECT g_trackpopup_yroot;
static POINT m_trackingPt, m_trackingPt2;
static int m_trackingMouseFlag;
static int m_trackingFlags,m_trackingRet;
static HWND m_trackingPar;
static WDL_PtrList<HWND__> m_trackingMenus; // each HWND as userdata = HMENU

int swell_delegate_menu_message(HWND src, LPARAM lParam, int msg, bool screencoords)
{
  static bool _reent;
  if (_reent) return 0;

  _reent = true;

  POINT sp = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
  if (!screencoords) ClientToScreen(src,&sp);

  for (int x = m_trackingMenus.GetSize()-1; x>=0; x--)
  {
    HWND sw = m_trackingMenus.Get(x);
    if (!sw) continue;

    if (sw == src) break; // stop searching (don't delegate to parent)

    RECT r;
    GetWindowRect(sw,&r);
    if (PtInRect(&r,sp))
    {
      POINT p = sp;
      ScreenToClient(sw,&p);
      SendMessage(sw,msg,0,MAKELPARAM(p.x,p.y));
      _reent = false;
      return 1;
    }
  }

  _reent = false;
  return 0;
}

bool swell_isOSwindowmenu(SWELL_OSWINDOW osw)
{
  int x = m_trackingMenus.GetSize();
  if (osw) while (--x>=0)
  {
    HWND__ *p = m_trackingMenus.Get(x);
    if (p->m_oswindow == osw) return true;
  }
  return false;
}

int menuBarNavigate(int dir); // -1 if no menu bar active, 0 if did nothing, 1 if navigated
HWND GetFocusIncludeMenus(void);

static LRESULT WINAPI submenuWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static int lcol, rcol, mcol, top_margin, separator_ht, text_ht_pad, bitmap_ht_pad, scroll_margin, item_bm_pad;
  if (!lcol)
  {
    lcol=SWELL_UI_SCALE(24); rcol=SWELL_UI_SCALE(12); mcol=SWELL_UI_SCALE(10);
    top_margin=SWELL_UI_SCALE(4); separator_ht=SWELL_UI_SCALE(8); 
    text_ht_pad=SWELL_UI_SCALE(4); bitmap_ht_pad=SWELL_UI_SCALE(4);
    scroll_margin=SWELL_UI_SCALE(10);
    item_bm_pad = SWELL_UI_SCALE(4);
  }
  switch (uMsg)
  {
    case WM_CREATE:
      hwnd->m_classname = "__SWELL_MENU";
      hwnd->m_style = WS_CHILD;
      m_trackingMenus.Add(hwnd);
      SetWindowLongPtr(hwnd,GWLP_USERDATA,lParam);

      if (m_trackingPar && !(m_trackingFlags&TPM_NONOTIFY))
        SendMessage(m_trackingPar,WM_INITMENUPOPUP,(WPARAM)lParam,0);

      {
        HDC hdc = GetDC(hwnd);
        HMENU__ *menu = (HMENU__*)lParam;
        int ht = 0, wid=SWELL_UI_SCALE(100),wid2=0;
        int x;
        for (x=0; x < menu->items.GetSize(); x++)
        {
          MENUITEMINFO *inf = menu->items.Get(x);
          BITMAP bm2={0,};
          if (inf->hbmpItem)
            GetObject(inf->hbmpItem,sizeof(bm2),&bm2);

          if (MenuIsStringType(inf))
          {
            RECT r={0,};
            const char *str = inf->dwTypeData;
            if (!str || !*str) str="XXXXX";
            const char *pt2 = strstr(str,"\t");
            DrawText(hdc,str,pt2 ? (int)(pt2-str) : -1,&r,DT_CALCRECT|DT_SINGLELINE);
            if (r.bottom < bm2.bmHeight) r.bottom = bm2.bmHeight;
            if (bm2.bmWidth) r.right += bm2.bmWidth + item_bm_pad;

            if (r.right > wid) wid=r.right;
            ht += r.bottom + text_ht_pad;

            if (pt2)
            { 
              r.right=r.left;
              DrawText(hdc,pt2+1,-1,&r,DT_CALCRECT|DT_SINGLELINE);
              if (r.right > wid2) wid2=r.right;
            }
          }
          else if (inf->fType == MFT_BITMAP)
          {
            BITMAP bm={16,16};
            if (inf->dwTypeData) GetObject((HBITMAP)inf->dwTypeData,sizeof(bm),&bm);
            if (bm.bmHeight < bm2.bmHeight) bm.bmHeight = bm2.bmHeight;
            if (bm2.bmWidth) bm.bmWidth += bm2.bmWidth + item_bm_pad;
            if (bm.bmWidth > wid) wid = bm.bmWidth;

            ht += bm.bmHeight + bitmap_ht_pad;
          }
          else
          {
            // treat as separator, ignore bm2
            ht += separator_ht;
          }
        }
        wid+=lcol+rcol + (wid2?wid2+mcol:0);
        ReleaseDC(hwnd,hdc);

        const RECT ref={m_trackingPt.x, m_trackingPt.y, m_trackingPt.x, m_trackingPt.y };
        RECT vp, tr={m_trackingPt.x,m_trackingPt.y, m_trackingPt.x+wid+SWELL_UI_SCALE(4),m_trackingPt.y+ht+top_margin * 2};
        SWELL_GetViewPort(&vp,&ref,true);
        vp.bottom -= 8;
 
        if (g_trackpopup_yroot.bottom > g_trackpopup_yroot.top &&
            g_trackpopup_yroot.bottom > vp.top && 
            g_trackpopup_yroot.top < vp.bottom)
        {
          if (vp.bottom - g_trackpopup_yroot.bottom < g_trackpopup_yroot.top - vp.top)
            vp.bottom = g_trackpopup_yroot.top;
          else
            vp.top = g_trackpopup_yroot.bottom;
        }

        if (tr.bottom > vp.bottom) { tr.top += vp.bottom-tr.bottom; tr.bottom=vp.bottom; }
        if (tr.right > vp.right) 
        { 
          if ((vp.right - m_trackingPt2.x) <  (m_trackingPt2.x - vp.left))
          {
            tr.left = m_trackingPt2.x - (tr.right-tr.left);
            tr.right = m_trackingPt2.x;
          }
          else
          {
            tr.left += vp.right-tr.right; tr.right=vp.right; 
          }
        }

        if (tr.left < vp.left) { tr.right += vp.left-tr.left; tr.left=vp.left; }
        if (tr.top < vp.top) { tr.bottom += vp.top-tr.top; tr.top=vp.top; }
        if (tr.bottom > vp.bottom) tr.bottom=vp.bottom;
        if (tr.right > vp.right) tr.right=vp.right;

        SetWindowPos(hwnd,NULL,tr.left,tr.top,tr.right-tr.left,tr.bottom-tr.top,SWP_NOZORDER);

        hwnd->m_extra[0] = 0; // Y scroll offset
        hwnd->m_extra[1] = 0; // &1=allow scroll flag (set from paint), &2=force scroll down (if sel_vis is offscreen positive)
      }

      ShowWindow(hwnd,SW_SHOW);
      SetFocus(hwnd);
      SetTimer(hwnd,1,100,NULL);
      SetTimer(hwnd,2,15,NULL);
    break;
    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          RECT cr;
          GetClientRect(hwnd,&cr);
          HBRUSH br=CreateSolidBrush(g_swell_ctheme.menu_bg);
          HBRUSH br2 = CreateSolidBrushAlpha(g_swell_ctheme.menu_scroll,0.5f);
          HBRUSH br3 = CreateSolidBrush(g_swell_ctheme.menu_scroll_arrow);
          HBRUSH br_submenu_arrow = CreateSolidBrush(g_swell_ctheme.menu_submenu_arrow);
          HPEN pen=CreatePen(PS_SOLID,0,g_swell_ctheme.menu_shadow);
          HPEN pen2=CreatePen(PS_SOLID,0,g_swell_ctheme.menu_hilight);
          HGDIOBJ oldbr = SelectObject(ps.hdc,br);
          HGDIOBJ oldpen = SelectObject(ps.hdc,pen2);
          Rectangle(ps.hdc,cr.left,cr.top,cr.right,cr.bottom);
          SetBkMode(ps.hdc,TRANSPARENT);
          HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
          int x;
          int ypos = top_margin;

          MoveToEx(ps.hdc,cr.left+lcol-SWELL_UI_SCALE(4),cr.top,NULL);
          LineTo(ps.hdc,cr.left+lcol-SWELL_UI_SCALE(4),cr.bottom);
          SelectObject(ps.hdc,pen);
          MoveToEx(ps.hdc,cr.left+lcol-SWELL_UI_SCALE(5),cr.top,NULL);
          LineTo(ps.hdc,cr.left+lcol-SWELL_UI_SCALE(5),cr.bottom);

          hwnd->m_extra[1]=0;
          for (x=wdl_max(hwnd->m_extra[0],0); x < (menu->items.GetSize()); x++)
          {
            if (ypos >= cr.bottom)
            {
              hwnd->m_extra[1] = 1; // allow scrolling down
              break;
            }
            MENUITEMINFO *inf = menu->items.Get(x);
            RECT r={lcol,ypos,cr.right, };
            bool dis = !!(inf->fState & MF_GRAYED);
            BITMAP bm={16,16}, bm2={0,};
            if (inf->hbmpItem)
              GetObject(inf->hbmpItem,sizeof(bm2),&bm2);

            if (MenuIsStringType(inf))
            {
              const char *str = inf->dwTypeData;
              if (!str || !*str) str="XXXXX";
              RECT mr={0,};
              DrawText(ps.hdc,str,-1,&mr,DT_CALCRECT|DT_SINGLELINE);

              ypos += wdl_max(mr.bottom,bm2.bmHeight) + text_ht_pad;
              r.bottom = ypos;
            }
            else if (inf->fType == MFT_BITMAP)
            {
              if (inf->dwTypeData) GetObject((HBITMAP)inf->dwTypeData,sizeof(bm),&bm);

              ypos += wdl_max(bm.bmHeight,bm2.bmHeight) + bitmap_ht_pad;
              r.bottom = ypos;

            }
            else
            {
              dis=true;
              ypos += separator_ht;
              r.bottom = ypos;
            }

            if (x == menu->sel_vis && !dis)
            {
              HBRUSH brs=CreateSolidBrush(g_swell_ctheme.menu_bg_sel);
              RECT r2=r;
              FillRect(ps.hdc,&r2,brs);
              DeleteObject(brs);
              SetTextColor(ps.hdc,g_swell_ctheme.menu_text_sel);
            }
            else 
            {
              SetTextColor(ps.hdc,
                 dis ? g_swell_ctheme.menu_text_disabled : 
                 g_swell_ctheme.menu_text);
            }

            if (bm2.bmWidth)
            {
              RECT tr = r;
              tr.right = tr.left + bm2.bmWidth;
              DrawImageInRect(ps.hdc,inf->hbmpItem,&tr);

              r.left += bm2.bmWidth + item_bm_pad;
            }

            if (MenuIsStringType(inf))
            {
              const char *str = inf->dwTypeData;
              if (!str) str="";
              const char *pt2 = strstr(str,"\t");

              if (*str) 
              {
                DrawText(ps.hdc,str,pt2 ? (int)(pt2-str) : -1,&r,DT_VCENTER|DT_SINGLELINE);
                if (pt2)
                {
                  RECT tr=r; tr.right-=rcol;
                  DrawText(ps.hdc,pt2+1,-1,&tr,DT_VCENTER|DT_SINGLELINE|DT_RIGHT);
                }
              }
            }
            else if (inf->fType == MFT_BITMAP)
            {
              if (inf->dwTypeData)
              {
                RECT tr = r;
                tr.top += bitmap_ht_pad/2;
                tr.right = tr.left + bm.bmWidth;
                tr.bottom = tr.top + bm.bmHeight;
                DrawImageInRect(ps.hdc,(HBITMAP)inf->dwTypeData,&tr);
              }
            }
            else 
            {
              SelectObject(ps.hdc,pen2);
              int y = r.top/2+r.bottom/2, right = r.right-rcol*3/2;
              MoveToEx(ps.hdc,r.left,y,NULL);
              LineTo(ps.hdc,right,y);
              SelectObject(ps.hdc,pen);

              y++;
              MoveToEx(ps.hdc,r.left,y,NULL);
              LineTo(ps.hdc,right,y);
            }
            if (inf->hSubMenu) 
            {
               const int sz = (r.bottom-r.top)/4, xp = r.right - sz*2, yp = (r.top + r.bottom)/2;

               POINT pts[3] = {
                 {xp, yp-sz},
                 {xp, yp+sz},
                 {xp + sz,yp}
               };
               HGDIOBJ oldPen = SelectObject(ps.hdc,GetStockObject(NULL_PEN));
               SelectObject(ps.hdc,br_submenu_arrow);
               Polygon(ps.hdc,pts,3);

               SelectObject(ps.hdc,oldPen);
            }
            if (inf->fState&MF_CHECKED)
            {
              const int col = dis ? g_swell_ctheme.menu_text_disabled : g_swell_ctheme.menu_text;
              HPEN tpen = CreatePen(PS_SOLID,0, col);
              HBRUSH tbr = CreateSolidBrush(col);
              HGDIOBJ oldBrush = SelectObject(ps.hdc,tbr);
              HGDIOBJ oldPen = SelectObject(ps.hdc,tpen);
              const int sz = (wdl_min(lcol, r.bottom-r.top) - SWELL_UI_SCALE(6));
              const int xo = SWELL_UI_SCALE(4), yo = (r.bottom+r.top)/2 - sz/2;
              if (inf->fType&MFT_RADIOCHECK)
              {
                Ellipse(ps.hdc, xo, yo, xo+sz, yo+sz);
              }
              else
              {
                static const unsigned char coords[12] = { 128, 30, 108, 11, 48, 72, 48, 112, 0, 65, 20, 46, };
                for (int pass=0;pass<2;pass++)
                {
                  POINT pts[4];
                  for (int i=0;i<4; i ++)
                  {
                    pts[i].x = xo + ((int)coords[i*2+pass*4] * sz + 63) / 128;
                    pts[i].y = yo + ((int)coords[i*2+pass*4+1] * sz + 63) / 128;
                  }
                  Polygon(ps.hdc,pts,4);
                }
              }

              SelectObject(ps.hdc,oldPen);
              SelectObject(ps.hdc,oldBrush);
              DeleteObject(tpen);
              DeleteObject(tbr);
            }
            if ((r.top+ypos)/2 > cr.bottom)
            {
              hwnd->m_extra[1] = 1; // allow scrolling down if last item was halfway off
            }
          }
          if (x <= menu->sel_vis) hwnd->m_extra[1]|=2;


          // lower scroll indicator
          int mid=(cr.right-cr.left)/2;
          SelectObject(ps.hdc,GetStockObject(NULL_PEN));
          SelectObject(ps.hdc,br3);
          POINT pts[3];
          const int smm = SWELL_UI_SCALE(2);
          const int smh = scroll_margin-smm*2;
          if (hwnd->m_extra[1]&1)
          {
            RECT fr = {cr.left, cr.bottom-scroll_margin, cr.right,cr.bottom};
            FillRect(ps.hdc,&fr,br2);
            pts[0].x = mid; pts[0].y = cr.bottom - smm;
            pts[1].x = mid-smh; pts[1].y = pts[0].y - smh;
            pts[2].x = mid+smh; pts[2].y = pts[1].y;
            Polygon(ps.hdc,pts,3);
          }
          // upper scroll indicator
          if (hwnd->m_extra[0] > 0)
          {
            RECT fr = {cr.left, cr.top, cr.right, cr.top+scroll_margin};
            FillRect(ps.hdc,&fr,br2);

            pts[0].x = mid; pts[0].y = cr.top + smm;
            pts[1].x = mid-smh; pts[1].y = pts[0].y + smh;
            pts[2].x = mid+smh; pts[2].y = pts[1].y;
            Polygon(ps.hdc,pts,3);
          }

          SelectObject(ps.hdc,oldbr);
          SelectObject(ps.hdc,oldpen);
          DeleteObject(br);
          DeleteObject(br2);
          DeleteObject(br3);
          DeleteObject(br_submenu_arrow);
          DeleteObject(pen);
          DeleteObject(pen2);
          EndPaint(hwnd,&ps); 
        }       
      }
    break;
    case WM_TIMER:
      if (wParam==1)
      {
        HWND h = GetFocusIncludeMenus();
        if (h!=hwnd)
        {
          int a = h ? m_trackingMenus.Find(h) : -1;
          if (a<0 || a < m_trackingMenus.Find(hwnd)) 
          {
            if (m_trackingMouseFlag && m_trackingMenus.Get(0))
            {
              SetFocus(m_trackingMenus.Get(0));
              m_trackingMouseFlag=0;
            }
            else DestroyWindow(hwnd); 
          }
        }
      } 
      else if (wParam == 2)
      {
        // menu scroll
        RECT tr;
        GetWindowRect(hwnd,&tr);

        POINT curM;
        GetCursorPos(&curM);
        const bool xmatch = (curM.x >= tr.left && curM.x < tr.right);
        if (xmatch || (hwnd->m_extra[1]&3)==3)
        {
          HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
          int xFirst = wdl_max(hwnd->m_extra[0],0);
          const bool ymatch = curM.y >= tr.bottom-scroll_margin && curM.y < tr.bottom+scroll_margin;
          if ((hwnd->m_extra[1]&1) && ((hwnd->m_extra[1]&2) || ymatch))
          {
            hwnd->m_extra[0]=++xFirst;
            hwnd->m_extra[1]=0;
            if (ymatch) menu->sel_vis=-1;
            InvalidateRect(hwnd,NULL,FALSE);
          }
          else if (xFirst > 0 && curM.y >= tr.top-scroll_margin && curM.y < tr.top+scroll_margin)
          {
            hwnd->m_extra[0]=--xFirst;
            menu->sel_vis=-1;
            InvalidateRect(hwnd,NULL,FALSE);
          }
        }
      }
    break;
    case WM_KEYUP:
    return 1;
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE || wParam == VK_LEFT)
      {
        HWND l = m_trackingMenus.Get(m_trackingMenus.Find(hwnd)-1);
        if (l) SetFocus(l);
        else 
        {
          if (wParam != VK_LEFT || menuBarNavigate(-1) < 0)
            DestroyWindow(hwnd);
        }
      }
      else if (wParam == VK_RETURN || wParam == VK_RIGHT)
      {
        HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        if (wParam == VK_RIGHT)
        {
          MENUITEMINFO *inf = menu->items.Get(menu->sel_vis);
          if (!inf || !inf->hSubMenu) 
          {
            menuBarNavigate(1);
            return 1;
          }
        }
        SendMessage(hwnd,WM_USER+100,1,menu->sel_vis);
      }
      else if (wParam == VK_UP || wParam == VK_PRIOR)
      {
        HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        int l = menu->sel_vis;
        for (int i= wParam == VK_UP ? 0 : 9; i>=0; i--) 
        {
          int mc = menu->items.GetSize();
          while (mc--)
          {
            if (l<1)
            {
              if (wParam != VK_UP) break;
              l = menu->items.GetSize();
            }
            MENUITEMINFO *inf = menu->items.Get(--l);
            if (!inf) break; 
            if (!(inf->fState & MF_GRAYED) && inf->fType != MFT_SEPARATOR) 
            {
              menu->sel_vis=l;
              break;
            }
          }
        }
        if (menu->sel_vis < hwnd->m_extra[0])
          hwnd->m_extra[0] = menu->sel_vis;
        InvalidateRect(hwnd,NULL,FALSE);
      }
      else if (wParam == VK_DOWN || wParam == VK_NEXT)
      {
        HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        int l = menu->sel_vis;
        const int n =menu->items.GetSize()-1;
        for (int i = wParam == VK_DOWN ? 0 : 9; i>=0; i--) 
        {
          int mc = n+1;
          while (mc--)
          {
            if (l>=n)
            {
              if (wParam != VK_DOWN) break;
              l=-1;
              hwnd->m_extra[0]=0;
            }
            MENUITEMINFO *inf = menu->items.Get(++l);
            if (!inf) break; 
            if (!(inf->fState & MF_GRAYED) && inf->fType != MFT_SEPARATOR) 
            {
              menu->sel_vis=l;
              break;
            }
          }
        }
        InvalidateRect(hwnd,NULL,FALSE);
      }
      else if (wParam == VK_END)
      {
        HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        int l = menu->items.GetSize();
        while (l > 0)
        {
          MENUITEMINFO *inf = menu->items.Get(--l);
          if (!inf) break; 
          if (!(inf->fState & MF_GRAYED) && inf->fType != MFT_SEPARATOR) 
          {
            menu->sel_vis=l;
            break;
          }
        }
        if (menu->sel_vis < hwnd->m_extra[0])
          hwnd->m_extra[0] = menu->sel_vis;
        InvalidateRect(hwnd,NULL,FALSE);
      }
      else if (wParam == VK_HOME)
      {
        HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        int l = 0;
        while (l < menu->items.GetSize())
        {
          MENUITEMINFO *inf = menu->items.Get(l++);
          if (!inf) break; 
          if (!(inf->fState & MF_GRAYED) && inf->fType != MFT_SEPARATOR) 
          {
            menu->sel_vis=l-1;
            break;
          }
        }
        if (menu->sel_vis < hwnd->m_extra[0])
          hwnd->m_extra[0] = menu->sel_vis;
        InvalidateRect(hwnd,NULL,FALSE);
      }
      else if ((lParam & FVIRTKEY) && (
              (wParam >= 'A' && wParam <= 'Z') ||
              (wParam >= '0' && wParam <= '9')))
      {
        HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        const int n=menu->items.GetSize();
        
        int offs = menu->sel_vis+1;
        if (offs<0||offs>=n) offs=0;
        int matchcnt=0;
        for(int x=0;x<n+n;x++)
        {
          MENUITEMINFO *inf = menu->items.Get(offs);
          if (MenuIsStringType(inf) && 
              !(inf->fState & MF_GRAYED) &&
              inf->dwTypeData)
          {
            const char *p = inf->dwTypeData;
            bool is_prefix_mode = x<n;
            if (!is_prefix_mode && matchcnt) 
            {
              if (matchcnt == 1) 
              {
                // implies prefix mode, only one matching item
                SendMessage(hwnd,WM_USER+100,1,menu->sel_vis);
              }
              break;
            }

            if (is_prefix_mode) while (*p)
            {
              if (*p++ == '&')
              {
                if (*p != '&') break;
                p++;
              }
            }

            if (*p > 0 && (WPARAM)toupper(*p) == wParam)
            {
              if (!matchcnt++)
              {
                menu->sel_vis = offs;
                if (menu->sel_vis < hwnd->m_extra[0])
                  hwnd->m_extra[0] = menu->sel_vis;
                InvalidateRect(hwnd,NULL,FALSE);
              }
              if (!is_prefix_mode) break;
            }
          }
          if (++offs >= n) offs=0;
        }
      }
         
    return 1;
    case WM_DESTROY:
      {
        int a = m_trackingMenus.Find(hwnd);
        m_trackingMenus.Delete(a);
        if (m_trackingMenus.Get(a)) DestroyWindow(m_trackingMenus.Get(a));
        RemoveProp(hwnd,"SWELL_MenuOwner");
      }
    break;
    case WM_USER+100:
      if (wParam == 1 || wParam == 2 || wParam == 3 || wParam == 4)
      {
        int which = (int) lParam;
        int item_ypos = which;

        HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);

        int ht = top_margin;
        HDC hdc=GetDC(hwnd);
        if (wParam > 1) which = -1;
        else item_ypos = 0;
        for (int x=wdl_max(hwnd->m_extra[0],0); x < (menu->items.GetSize()); x++)
        {
          if (wParam == 1 && which == x) { item_ypos = ht; break; }
          MENUITEMINFO *inf = menu->items.Get(x);
          int lastht = ht;
          BITMAP bm2={0,};
          if (inf->hbmpItem)
            GetObject(inf->hbmpItem,sizeof(bm2),&bm2);

          if (MenuIsStringType(inf))
          {
            RECT r={0,};
            const char *str = inf->dwTypeData;
            if (!str || !*str) str="XXXXX";
            const char *pt2 = strstr(str,"\t");
            DrawText(hdc,str,pt2 ? (int)(pt2-str) : -1,&r,DT_CALCRECT|DT_SINGLELINE);
            ht += wdl_max(r.bottom,bm2.bmHeight) + text_ht_pad;
          }
          else if (inf->fType == MFT_BITMAP)
          {
            BITMAP bm={16,16};
            if (inf->dwTypeData) GetObject((HBITMAP)inf->dwTypeData,sizeof(bm),&bm);
            ht += wdl_max(bm.bmHeight,bm2.bmHeight) + bitmap_ht_pad;
          }
          else
          {
            ht += separator_ht;
          }
          if (wParam > 1 && item_ypos < ht) 
          { 
            item_ypos = lastht; 
            which = x; 
            if (wParam == 4 && inf->hSubMenu) 
            {
              HWND nextmenu = m_trackingMenus.Get(m_trackingMenus.Find(hwnd)+1);
              if (!nextmenu || GetWindowLongPtr(nextmenu,GWLP_USERDATA) != (LPARAM)inf->hSubMenu)
              {
                wParam = 1; // activate if not already visible
                menu->sel_vis = which;
              }
            }
            break; 
          }
        }
        ReleaseDC(hwnd,hdc);
        if (wParam == 3 || wParam == 4)
        {
          MENUITEMINFO *inf = menu->items.Get(which);
          HWND next = m_trackingMenus.Get(m_trackingMenus.Find(hwnd)+1);
          if (next && inf && (!inf->hSubMenu || (LPARAM)inf->hSubMenu != GetWindowLongPtr(next,GWLP_USERDATA))) DestroyWindow(next); 
          menu->sel_vis = which;
          return 0;
        }

        MENUITEMINFO *inf = menu->items.Get(which);

        if (inf) 
        {
          if (inf->fState&MF_GRAYED){ }
          else if (inf->hSubMenu)
          {
            const int nextidx = m_trackingMenus.Find(hwnd)+1;
            HWND hh = m_trackingMenus.Get(nextidx);

            inf->hSubMenu->sel_vis=-1;

            if (hh)
            {
              m_trackingMenus.Delete(nextidx);
              int a = m_trackingMenus.GetSize();
              while (a > nextidx) DestroyWindow(m_trackingMenus.Get(--a));
            }
            else
            {
              hh = new HWND__(NULL,0,NULL,"menu",false,submenuWndProc,NULL, hwnd);
              SetProp(hh,"SWELL_MenuOwner",GetProp(hwnd,"SWELL_MenuOwner"));
            }

            RECT r;
            GetClientRect(hwnd,&r);
            m_trackingPt.x=r.right - SWELL_UI_SCALE(3);
            m_trackingPt.y=item_ypos;
            m_trackingPt2.x=r.left + lcol/4;
            m_trackingPt2.y=item_ypos;
            ClientToScreen(hwnd,&m_trackingPt);
            ClientToScreen(hwnd,&m_trackingPt2);

            submenuWndProc(hh, WM_CREATE,0,(LPARAM)inf->hSubMenu);
            InvalidateRect(hwnd,NULL,FALSE);
          }
          else if (inf->wID) m_trackingRet = inf->wID;
        }
      }
    return 0;
    case WM_MOUSEMOVE:
      {
        if (swell_delegate_menu_message(hwnd, lParam,uMsg, false))
          return 0;

        RECT r;
        GetClientRect(hwnd,&r);
        HMENU__ *menu = (HMENU__*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        const int oldsel = menu->sel_vis;
        if (GET_X_LPARAM(lParam)>=r.left && GET_X_LPARAM(lParam)<r.right)
        {
          int mode = 4;//GET_X_LPARAM(lParam) >= r.right - rcol*2 ? 4 : 3;
          SendMessage(hwnd,WM_USER+100,mode,GET_Y_LPARAM(lParam));
        }
        else menu->sel_vis = -1;
        if (oldsel != menu->sel_vis) InvalidateRect(hwnd,NULL,FALSE);
      }
    return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
      {
        if (swell_delegate_menu_message(hwnd, lParam, uMsg, false))
          return 0;

        RECT r;
        GetClientRect(hwnd,&r);
        if (GET_X_LPARAM(lParam)>=r.left && GET_X_LPARAM(lParam)<r.right)
        {
          SendMessage(hwnd,WM_USER+100,2,GET_Y_LPARAM(lParam));
          return 0;
        }
        else DestroyWindow(hwnd);
      }
    return 0;
  }
  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

void DestroyPopupMenus()
{
  if (m_trackingMenus.GetSize()) DestroyWindow(m_trackingMenus.Get(0));
}

SWELL_OSWINDOW swell_ignore_focus_oswindow;
DWORD swell_ignore_focus_oswindow_until;

int TrackPopupMenu(HMENU hMenu, int flags, int xpos, int ypos, int resvd, HWND hwnd, const RECT *r)
{
  if (WDL_NOT_NORMALLY(!hMenu) || m_trackingMenus.GetSize()) return 0;

  ReleaseCapture();

  hMenu->Retain();
  m_trackingPar=hwnd;
  m_trackingFlags=flags;
  m_trackingRet=-1;
  m_trackingPt2.x=m_trackingPt.x=xpos;
  m_trackingPt2.y=m_trackingPt.y=ypos;
  m_trackingMouseFlag = 0;
  if (GetAsyncKeyState(VK_LBUTTON)) m_trackingMouseFlag |= 1;
  if (GetAsyncKeyState(VK_RBUTTON)) m_trackingMouseFlag |= 2;
  if (GetAsyncKeyState(VK_MBUTTON)) m_trackingMouseFlag |= 4;

//  HWND oldFoc = GetFocus();
 // bool oldFoc_child = oldFoc && (IsChild(hwnd,oldFoc) || oldFoc == hwnd || oldFoc==GetParent(hwnd));

  if (hwnd) 
  {
    hwnd->Retain();
    swell_ignore_focus_oswindow = hwnd->m_oswindow;
    swell_ignore_focus_oswindow_until = GetTickCount()+500;
  }


  hMenu->sel_vis=-1;
  HWND hh=new HWND__(NULL,0,NULL,"menu",false,submenuWndProc,NULL, hwnd);

  submenuWndProc(hh,WM_CREATE,0,(LPARAM)hMenu);

  SetProp(hh,"SWELL_MenuOwner",(HANDLE)hwnd);

  while (m_trackingRet<0 && m_trackingMenus.GetSize())
  {
    void SWELL_RunMessageLoop();
    SWELL_RunMessageLoop();
    Sleep(10);
  }

  int x=m_trackingMenus.GetSize()-1;
  while (x>=0)
  {
    HWND h = m_trackingMenus.Get(x);
    m_trackingMenus.Delete(x);
    if (h) DestroyWindow(h);
    x--;
  }

//  if (oldFoc_child) SetFocus(oldFoc);

  if (!(flags&TPM_RETURNCMD) && m_trackingRet>0) 
    SendMessage(hwnd,WM_COMMAND,m_trackingRet,0);
  
  if (hwnd) hwnd->Release();

  swell_ignore_focus_oswindow = NULL;
  hMenu->Release();

  if (flags & TPM_RETURNCMD) return m_trackingRet>0?m_trackingRet:0;

  return resvd!=0xbeef || m_trackingRet>0;
}




void SWELL_Menu_AddMenuItem(HMENU hMenu, const char *name, int idx, unsigned int flags)
{
  MENUITEMINFO mi={sizeof(mi),MIIM_ID|MIIM_STATE|MIIM_TYPE,MFT_STRING,
    (UINT)((flags)?MFS_GRAYED:0),(UINT)idx,NULL,NULL,NULL,0,(char *)name};
  if (!name)
  {
    mi.fType = MFT_SEPARATOR;
    mi.fMask&=~(MIIM_STATE|MIIM_ID);
  }
  InsertMenuItem(hMenu,GetMenuItemCount(hMenu),TRUE,&mi);
}


SWELL_MenuResourceIndex *SWELL_curmodule_menuresource_head; // todo: move to per-module thingy

static SWELL_MenuResourceIndex *resById(SWELL_MenuResourceIndex *head, const char *resid)
{
  SWELL_MenuResourceIndex *p=head;
  while (p)
  {
    if (p->resid == resid) return p;
    p=p->_next;
  }
  return 0;
}

HMENU SWELL_LoadMenu(SWELL_MenuResourceIndex *head, const char *resid)
{
  SWELL_MenuResourceIndex *p;
  
  if (!(p=resById(head,resid))) return 0;
  HMENU hMenu=CreatePopupMenu();
  if (hMenu) p->createFunc(hMenu);
  return hMenu;
}

HMENU SWELL_DuplicateMenu(HMENU menu)
{
  if (WDL_NOT_NORMALLY(!menu)) return 0;
  return menu->Duplicate();
}

BOOL  SetMenu(HWND hwnd, HMENU menu)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;
  HMENU oldmenu = hwnd->m_menu;

  hwnd->m_menu = menu;
  
  if (!hwnd->m_parent && !!hwnd->m_menu != !!oldmenu)
  {
    WNDPROC oldwc = hwnd->m_wndproc;
    hwnd->m_wndproc = DefWindowProc;
    RECT r;
    GetWindowRect(hwnd,&r);

    if (oldmenu) r.bottom -= g_swell_ctheme.menubar_height; // hack: we should WM_NCCALCSIZE before and after, really
    else r.bottom += g_swell_ctheme.menubar_height;

    SetWindowPos(hwnd,NULL,0,0,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
    hwnd->m_wndproc = oldwc;
    // resize
  }

  return TRUE;
}

HMENU GetMenu(HWND hwnd)
{
  if (WDL_NOT_NORMALLY(!hwnd)) return 0;
  return hwnd->m_menu;
}

void DrawMenuBar(HWND hwnd)
{
  if (WDL_NORMALLY(hwnd) && hwnd->m_menu)
  {
    RECT r;
    GetClientRect(hwnd,&r);
    r.top = - g_swell_ctheme.menubar_height;
    r.bottom=0;
    InvalidateRect(hwnd,&r,FALSE);
  }
}


// copied from swell-menu.mm, can have a common impl someday
int SWELL_GenerateMenuFromList(HMENU hMenu, const void *_list, int listsz)
{
  SWELL_MenuGen_Entry *list = (SWELL_MenuGen_Entry *)_list;
  const int l1=strlen(SWELL_MENUGEN_POPUP_PREFIX);
  while (listsz>0)
  {
    int cnt=1;
    if (!list->name) SWELL_Menu_AddMenuItem(hMenu,NULL,-1,0);
    else if (!strcmp(list->name,SWELL_MENUGEN_ENDPOPUP)) return list + 1 - (SWELL_MenuGen_Entry *)_list;
    else if (!strncmp(list->name,SWELL_MENUGEN_POPUP_PREFIX,l1)) 
    { 
      MENUITEMINFO mi={sizeof(mi),MIIM_SUBMENU|MIIM_STATE|MIIM_TYPE,MFT_STRING,0,0,CreatePopupMenuEx(list->name+l1),NULL,NULL,0,(char *)list->name+l1};
      cnt += SWELL_GenerateMenuFromList(mi.hSubMenu,list+1,listsz-1);
      InsertMenuItem(hMenu,GetMenuItemCount(hMenu),TRUE,&mi);
    }
    else SWELL_Menu_AddMenuItem(hMenu,list->name,list->idx,list->flags);

    list+=cnt;
    listsz -= cnt;
  }
  return list + 1 - (SWELL_MenuGen_Entry *)_list;
}
#endif
