/*
    WDL - Skinned/Resizing thumb scrollbar library 
    Based on the "Cool Scrollbar Library v1.2" by James Brown - http://www.catch22.net
  
    Original version Copyright(c) 2001 J Brown
    Modifications copyright (C) 2006 and later Cockos Incorporated

    Note: for a more featureful and complete, less hacked up version, you may wish to 
    download the  original from http://www.catch22.net. It has lots of added features, 
    whereas this version is very much tailored for Cockos' needs.

    License:

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
    

    Original readme from        Cool Scrollbar Library Version 1.2

	    Copyright (c) J Brown 2001
	      
	    This code is freeware, however, you may not publish
	    this code elsewhere or charge any money for it. This code
	    is supplied as-is. I make no guarantees about the suitability
	    of this code - use at your own risk.
	    
	    It would be nice if you credited me, in the event
	    that you use this code in a product.
	    
	    VERSION HISTORY:

	     V1.2: TreeView problem fixed by Diego Tartara
		       Small problem in thumbsize calculation also fixed (thanks Diego!)

	     V1.1: Added support for Right-left windows
	           Changed calling convention of APIs to WINAPI (__stdcall)
		       Completely standalone (no need for c-runtime)
		       Now supports ALL windows with appropriate USER32.DLL patching
		        (you provide!!)

	     V1.0: Apr 2001: Initial Version

      IMPORTANT:
	     This whole library is based around code for a horizontal scrollbar.
	     All "vertical" scrollbar drawing / mouse interaction uses the
	     horizontal scrollbar functions, but uses a trick to convert the vertical
	     scrollbar coordinates into horizontal equivelants. When I started this project,
	     I quickly realised that the code for horz/vert bars was IDENTICAL, apart
	     from the fact that horizontal code uses left/right coords, and vertical code
	     uses top/bottom coords. On entry to a "vertical" drawing function, for example,
	     the coordinates are "rotated" before the horizontal function is called, and
	     then rotated back once the function has completed. When something needs to
	     be drawn, the coords are converted back again before drawing.
	     
         This trick greatly reduces the amount of code required, and makes
	     maintanence much simpler. This way, only one function is needed to draw
	     a scrollbar, but this can be used for both horizontal and vertical bars
	     with careful thought.

  Notes on Cockos modifications:

    We tweaked this to support resizing thumbs, native zoom buttons, etc, and skinnability using LICE (if a skin image is supplied).
    Additionally we modified it to support OS X using SWELL (since SWELL does not provide a scrollbar API, this can be used for that purpose).
    We also shoved everything into one file, for various reasons.

*/

#ifdef _WIN32
  #include <windows.h>
  #include <windowsx.h>
  #include <commctrl.h>
  #pragma warning(disable:4244) // implicit cast int to float
#else
  #include "../../swell/swell.h"
#endif

#include "../../lice/lice.h"
#include "../../wdltypes.h"

#include "coolscroll.h"

#define ZOOMBUTTON_RESIZER_SIZE(zbs) (wdl_max(((zbs)/4),2))
#define MIN_SIZE_FOR_ZOOMBUTTONS(zbs) (6*(zbs))

//
//	SCROLLBAR datatype. There are two of these structures per window
//
typedef struct 
{
	UINT		fScrollFlags;		//flags
	BOOL		fScrollVisible;		//if this scrollbar visible?
	SCROLLINFO	scrollInfo;			//positional data (range, position, page size etc)
	
	int			nBarType;			//SB_HORZ / SB_VERT

	int			nMinThumbSize;

  LICE_SysBitmap *liceBkgnd;
  LICE_SysBitmap *liceThumb;
  int liceBkgnd_ver;
  int liceThumb_ver;

  int liceThumbState;

} SCROLLBAR;

//
//	Container structure for a cool scrollbar window.
//
typedef struct
{
	UINT bars;				//which of the scrollbars do we handle? SB_VERT / SB_HORZ / SB_BOTH
	WNDPROC oldproc;		//old window procedure to call for every message

	SCROLLBAR sbarHorz;		//one scrollbar structure each for 
	SCROLLBAR sbarVert;		//the horizontal and vertical scrollbars

	BOOL fThumbTracking;	// are we currently thumb-tracking??
	BOOL fLeftScrollbar;	// support the WS_EX_LEFTSCROLLBAR style

	//size of the window borders
	int cxLeftEdge, cxRightEdge;
	int cyTopEdge,  cyBottomEdge;

	// To prevent calling original WindowProc in response
	// to our own temporary style change (fixes TreeView problem)
	BOOL bPreventStyleChange;

  BOOL resizingHthumb;


  // internal state stuff

  UINT uScrollTimerMsg;
  
  UINT_PTR uMouseOverId;
  UINT_PTR uScrollTimerId;
  UINT_PTR uZoomTimerId;
  UINT uZoomTimerMode;

  RECT MouseOverRect;
  BOOL MouseOverRect_hasZoomButtons;

  UINT uCurrentScrollbar;
  int uCurrentScrollPortion;
  UINT uMouseOverScrollbar;
  UINT uHitTestPortion;
  UINT uLastHitTestPortion;
  UINT uScrollTimerPortion;

  UINT vscrollbarShrinkBottom,vscrollbarShrinkTop;
  void *(*getDeadAreaBitmap)(int, HWND, RECT *,int);

  int whichTheme;
} SCROLLWND;



//
//	Minimum size in pixels of a scrollbar thumb
//
#define MINTHUMBSIZE_NT4   8
#define MINTHUMBSIZE_2000  6
#define RESIZETHUMBSIZE 6


// To complement the exisiting SB_HORZ, SB_VERT, SB_BOTH
// scrollbar identifiers
#define COOLSB_NONE (-1)

// general scrollbar styles
//
// use the standard ESB_DISABLE_xxx flags to represent the
// enabled / disabled states. (defined in winuser.h)
//
#define CSBS_THUMBALWAYS		4
#define CSBS_VISIBLE			8


//define some more hittest values for our cool-scrollbar
#define HTSCROLL_LEFT		(SB_LINELEFT)
#define HTSCROLL_RIGHT		(SB_LINERIGHT)
#define HTSCROLL_UP			(SB_LINEUP)
#define HTSCROLL_DOWN		(SB_LINEDOWN)
#define HTSCROLL_THUMB		(SB_THUMBTRACK)
#define HTSCROLL_PAGEGUP	(SB_PAGEUP)
#define HTSCROLL_PAGEGDOWN	(SB_PAGEDOWN)
#define HTSCROLL_PAGELEFT	(SB_PAGELEFT)
#define HTSCROLL_PAGERIGHT	(SB_PAGERIGHT)

#define HTSCROLL_NONE		(-1)
#define HTSCROLL_NORMAL		(-1)

#define HTSCROLL_INSERTED	(128)
#define HTSCROLL_PRE		(32 | HTSCROLL_INSERTED)
#define HTSCROLL_POST		(64 | HTSCROLL_INSERTED)

#define HTSCROLL_LRESIZER	(256)
#define HTSCROLL_RRESIZER	(256+1)

#define HTSCROLL_RESIZER	(256+2)

#define HTSCROLL_ZOOMIN	(256+3)
#define HTSCROLL_ZOOMOUT	(256+4)


//
#define SM_CXVERTSB 1
#define SM_CYVERTSB 0
#define SM_CXHORZSB 0
#define SM_CYHORZSB 1
#define SM_SCROLL_WIDTH	1
#define SM_SCROLL_LENGTH 0



#define InvertCOLORREF(col) ((col) ^ RGB(255,255,255))

#define COOLSB_TIMERID1			65533		//initial timer
#define COOLSB_TIMERID2			65534		//scroll message timer
#define COOLSB_TIMERID3			-14			//mouse hover timer
#define COOLSB_TIMERID4     0xfffef110 // used for when holding a zoom button
#define COOLSB_TIMERINTERVAL1	300
#define COOLSB_TIMERINTERVAL2	55
#define COOLSB_TIMERINTERVAL3	20			//mouse hover time
#define COOLSB_TIMERINTERVAL4	150			//holding the zoom buttons

struct wdlscrollbar_themestate
{
  LICE_IBitmap **bmp;
  int hasPink;
  int thumbHV[5], thumbVV[5]; // 
  int bkghl, bkghr;
  int bkgvt, bkgvb;
  int imageVersion; // liceBkgnd_ver, liceThumb_ver
};

#ifndef MAX_SCROLLBAR_THEMES
#define MAX_SCROLLBAR_THEMES 8
#endif
static wdlscrollbar_themestate s_scrollbar_theme[MAX_SCROLLBAR_THEMES];

static wdlscrollbar_themestate *GetThemeForScrollWnd(const SCROLLWND *sw)
{
  if (!sw || sw->whichTheme < 0 || sw->whichTheme >= MAX_SCROLLBAR_THEMES)
    return &s_scrollbar_theme[0];
  return &s_scrollbar_theme[sw->whichTheme];
}

//
//	Special thumb-tracking variables
//
//

static RECT rcThumbBounds;		//area that the scroll thumb can travel in
static int  g_nThumbSize;			//(pixels)
static int  g_nThumbPos;			//(pixels)
static int  nThumbMouseOffset;	//(pixels)
static int  nLastPos = -1;		//(scrollbar units)
static int  nThumbPos0;			//(pixels) initial thumb position

//
//	Temporary state used to auto-generate timer messages
//

static HWND hwndCurCoolSB = 0;
static float m_scale = 1.0;
static int m_thumbsize = 6;

static LRESULT MouseMove(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam);

static SCROLLWND *GetScrollWndFromHwnd(HWND hwnd);
//
//	Provide this so there are NO dependencies on CRT
//


//
//	swap the rectangle's x coords with its y coords
//
static void RotateRect(RECT *rect)
{
	int temp;
	temp = rect->left;
	rect->left = rect->top;
	rect->top = temp;

	temp = rect->right;
	rect->right = rect->bottom;
	rect->bottom = temp;
}

//
//	swap the coords if the scrollbar is a SB_VERT
//
static void RotateRect0(SCROLLBAR *sb, RECT *rect)
{
	if(sb->nBarType == SB_VERT)
		RotateRect(rect);
}

//
//	Calculate if the SCROLLINFO members produce
//  an enabled or disabled scrollbar
//
static BOOL IsScrollInfoActive(SCROLLINFO *si)
{
	if((si->nPage > (UINT)si->nMax
		|| si->nMax <= si->nMin || si->nMax == 0))
		return FALSE;
	else
		return TRUE;
}

//
//	Return if the specified scrollbar is enabled or not
//
static BOOL IsScrollbarActive(SCROLLBAR *sb)
{
	SCROLLINFO *si = &sb->scrollInfo;
	if(((sb->fScrollFlags & ESB_DISABLE_BOTH) == ESB_DISABLE_BOTH) ||
		(!(sb->fScrollFlags & CSBS_THUMBALWAYS) && !IsScrollInfoActive(si)))
		return FALSE;
	else
		return TRUE;
}

#ifdef __APPLE__
static void ReleaseDCFlush(HWND hwnd, HDC hdc)
{
  ReleaseDC(hwnd,hdc);
  SWELL_FlushWindow(hwnd);
}
#define ReleaseDC(hwnd,hdc) ReleaseDCFlush(hwnd,hdc)
#endif

#ifndef _WIN32
static void GET_WINDOW_RECT(HWND hwnd, RECT *r)
{
  GetWindowContentViewRect(hwnd,r);
#ifdef __APPLE__
  if (r->top>r->bottom) 
  { 
    int tmp = r->top;
    r->top = r->bottom;
    r->bottom = tmp;
  }
#endif
}
#else
#define GET_WINDOW_RECT(hwnd, r) GetWindowRect(hwnd,r)
#endif

#ifdef __APPLE__
static void OSX_REMAP_SCREENY(HWND hwnd, LONG *y)
{
//  POINT p={0,0};
  //ClientToScreen(hwnd,&p);
  POINT p={0,*y};
  RECT tr;
  GetClientRect(hwnd,&tr);
  ScreenToClient(hwnd,&p);
  p.y-=tr.top;
  RECT r;
  GetWindowRect(hwnd,&r);
  
  *y=wdl_min(r.bottom,r.top)+p.y;
// map Y from "screen" coordinate
}

#else

#define OSX_REMAP_SCREENY(hwnd, y)
#endif

static BOOL ownDrawEdge(HWND hwnd, HDC hdc, LPRECT qrc, UINT edge, UINT grfFlags)
{
  HPEN pen1 = CreatePen(PS_SOLID, 0, CoolSB_GetSysColor(hwnd,COLOR_3DHILIGHT));
  HPEN pen2 = CreatePen(PS_SOLID, 0, CoolSB_GetSysColor(hwnd,COLOR_3DSHADOW));
  HPEN pen3 = CreatePen(PS_SOLID, 0, CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
  HPEN pen4 = CreatePen(PS_SOLID, 0, CoolSB_GetSysColor(hwnd,COLOR_3DDKSHADOW));
  HPEN oldpen = (HPEN)SelectObject(hdc,pen3);

  if(edge == EDGE_RAISED)
  {
    MoveToEx(hdc, qrc->left, qrc->top, NULL);
    LineTo(hdc, qrc->right, qrc->top);
    MoveToEx(hdc, qrc->left, qrc->top, NULL);
    LineTo(hdc, qrc->left, qrc->bottom);
    SelectObject(hdc,pen1);
    MoveToEx(hdc, qrc->left+1, qrc->top+1, NULL);
    LineTo(hdc, qrc->right-1, qrc->top+1);
    MoveToEx(hdc, qrc->left+1, qrc->top+1, NULL);
    LineTo(hdc, qrc->left+1, qrc->bottom-2);
    SelectObject(hdc,pen2);
    MoveToEx(hdc, qrc->left+1, qrc->bottom-2, NULL);
    LineTo(hdc, qrc->right-1, qrc->bottom-2);  
    MoveToEx(hdc, qrc->right-2, qrc->bottom-2, NULL);
    LineTo(hdc, qrc->right-2, qrc->top+1);  
    SelectObject(hdc,pen4);
    MoveToEx(hdc, qrc->left, qrc->bottom-1, NULL);
    LineTo(hdc, qrc->right, qrc->bottom-1);  
    MoveToEx(hdc, qrc->right-1, qrc->bottom-1, NULL);
    LineTo(hdc, qrc->right-1, qrc->top);  
  }
  else
  {
    HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
    FillRect(hdc, qrc, br);
    DeleteObject(br);
    
    SelectObject(hdc, pen2);
    MoveToEx(hdc, qrc->left, qrc->top, NULL);
    LineTo(hdc, qrc->right, qrc->top);
    MoveToEx(hdc, qrc->right-1, qrc->top, NULL);
    LineTo(hdc, qrc->right-1, qrc->bottom);
    MoveToEx(hdc, qrc->right-1, qrc->bottom-1, NULL);
    LineTo(hdc, qrc->left, qrc->bottom-1);
    MoveToEx(hdc, qrc->left, qrc->bottom-1, NULL);
    LineTo(hdc, qrc->left, qrc->top);
  }
  
  SelectObject(hdc, oldpen);

  DeleteObject(pen1);
  DeleteObject(pen2);
  DeleteObject(pen3);
  DeleteObject(pen4);
  
  if(grfFlags & BF_ADJUST)
  {
    qrc->left+=2;
    qrc->top+=2;
    qrc->bottom-=2;
    qrc->right-=2;
  }
  return 1;
}


static LRESULT CallWindowProcStyleMod(SCROLLWND *sw, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN32
  bool restore=false;
  DWORD dwStyle = GetWindowLong(hwnd,GWL_STYLE);
  if (!sw->bPreventStyleChange && ( dwStyle & (WS_VSCROLL|WS_HSCROLL) ))
  {
    sw->bPreventStyleChange = TRUE;
    SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~(WS_VSCROLL|WS_HSCROLL));
    restore = true;
  }
#endif
  
  LRESULT ret  = CallWindowProc(sw->oldproc,hwnd,msg,wParam,lParam);
#ifdef _WIN32
  if (restore)
  {
    SetWindowLong(hwnd, GWL_STYLE, dwStyle);
    sw->bPreventStyleChange = FALSE;
  }
#endif
  return ret;
}

static BOOL ownDrawFrameControl(HWND hwnd, HDC hdc, LPRECT lprc, UINT uType, UINT uState, int mouseOver, const wdlscrollbar_themestate *theme)
{
  LICE_IBitmap *bmp;
  if(theme->bmp && (bmp = *theme->bmp))
  {
    static LICE_SysBitmap tmpbmp;
    int w = lprc->right-lprc->left;
    int h = lprc->bottom-lprc->top;
    int startx = 116;
    int starty = 121;
    if((uState&0xf) == DFCS_SCROLLLEFT) starty += 20;
    else if((uState&0xf) == DFCS_SCROLLRIGHT) starty += 40;
    else if((uState&0xf) == DFCS_SCROLLDOWN) starty += 60;
    if(uState & DFCS_PUSHED) startx += 34;
    else if(mouseOver) startx += 17;
    
    if (w>tmpbmp.getWidth() || h>tmpbmp.getHeight())
      tmpbmp.resize(wdl_max(w,tmpbmp.getWidth()), wdl_max(h,tmpbmp.getHeight()));

    LICE_ScaledBlit(&tmpbmp, bmp, 0, 0, w, h, startx, starty, 17, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          #ifndef _WIN32
          SWELL_SyncCtxFrameBuffer(tmpbmp.getDC());
          #endif
    BitBlt(hdc, lprc->left, lprc->top, w, h, tmpbmp.getDC(), 0, 0, SRCCOPY);
    return 1;
  }

  RECT r = *lprc;
  HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
  HBRUSH br2 = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNTEXT));
  HPEN pen = CreatePen(PS_SOLID, 0, CoolSB_GetSysColor(hwnd,COLOR_BTNTEXT));
  HPEN oldpen;
  HBRUSH oldbrush;

  ownDrawEdge(hwnd,hdc, &r, uState&DFCS_PUSHED?EDGE_ETCHED:EDGE_RAISED, BF_ADJUST);
	FillRect(hdc, &r, br);

  if(uState & DFCS_PUSHED) 
  {
    r.left++;
    r.top++;
    r.bottom++;
    r.right++;
  }
  if(!((r.bottom-r.top)&1))
  {
    if((uState&0xf) == DFCS_SCROLLRIGHT || (uState&0xf) == DFCS_SCROLLLEFT)
    {
      /*if ((uState&0xf) == DFCS_SCROLLLEFT) r.right++;
      else r.left--;
      r.top--;*/
      r.bottom--;
      if ((uState&0xf) == DFCS_SCROLLRIGHT) 
      {
        r.left--;
        r.right--;
      }
    }
    else
    {
      /*if ((uState&0xf) != DFCS_SCROLLUP) r.top--;
      else r.bottom++;
      r.left--;*/
      r.right--;
      if ((uState&0xf) != DFCS_SCROLLUP)
      {
        r.top--;
        r.bottom--;
      }
      else
      {
        r.top++;
        r.bottom++;
      }
    }
  }
  oldpen = (HPEN)SelectObject(hdc, pen);
  oldbrush = (HBRUSH)SelectObject(hdc, br2);
  if((uState&0xf) == DFCS_SCROLLRIGHT)
  {
    int h = r.bottom-r.top-6;
    POINT p[3]={{r.right-4,r.top+3+(h/2)},{r.left+5,r.top+2},{r.left+5,r.bottom-3}};
    Polygon(hdc, p, 3);
  }
  else if((uState&0xf) == DFCS_SCROLLLEFT)
  {
    int h = r.bottom-r.top-6;
    POINT p[3]={{r.left+3,r.top+3+(h/2)},{r.right-6,r.top+2},{r.right-6,r.bottom-3}};
    Polygon(hdc, p, 3);
  }
  else if((uState&0xf) == DFCS_SCROLLDOWN)
  {
    int w = r.right-r.left-6;
    POINT p[3]={{r.left+3+(w/2),r.bottom-4},{r.left+2,r.top+5},{r.right-3,r.top+5}};
    Polygon(hdc, p, 3);
  }
  else if((uState&0xf) == DFCS_SCROLLUP)
  {
    int w = r.right-r.left-6;
    POINT p[3]={{r.left+3+(w/2),r.top+3},{r.left+2,r.bottom-6},{r.right-3,r.bottom-6}};
    Polygon(hdc, p, 3);
  }

  SelectObject(hdc, oldpen);
  SelectObject(hdc, oldbrush);

  DeleteObject(br);
  DeleteObject(br2);
  DeleteObject(pen);

  return 1;
}

//
//	Draw a standard scrollbar arrow
//
static int DrawScrollArrow(HWND hwnd, SCROLLBAR *sbar, HDC hdc, RECT *rect, UINT arrow, BOOL fMouseDown, BOOL fMouseOver, const wdlscrollbar_themestate *theme)
{
	UINT ret;
	UINT flags = arrow;

	//HACKY bit so this routine can be called by vertical and horizontal code
	if(sbar->nBarType == SB_VERT)
	{
		if(flags & DFCS_SCROLLLEFT)		flags = (flags & ~DFCS_SCROLLLEFT)  | DFCS_SCROLLUP;
		if(flags & DFCS_SCROLLRIGHT)	flags = (flags & ~DFCS_SCROLLRIGHT) | DFCS_SCROLLDOWN;
	}

	if(fMouseDown) flags |= (DFCS_FLAT | DFCS_PUSHED);


	ret = ownDrawFrameControl(hwnd,hdc, rect, DFC_SCROLL, flags, fMouseOver, theme);

	return ret;
}

void CoolSB_SetScale(float scale)
{
  m_scale = scale;
  m_thumbsize = (int)(RESIZETHUMBSIZE * scale);
  if (m_thumbsize<2)m_thumbsize=2;
}

//
//	Return the size in pixels for the specified scrollbar metric,
//  for the specified scrollbar
//
static int GetScrollMetric(BOOL isVert, int metric)
{
	if(!isVert)
	{
		if(metric == SM_CXHORZSB)
		{
  		return (int)(GetSystemMetrics(SM_CXHSCROLL) * m_scale);
		}
		else
		{
		  return (int)(GetSystemMetrics(SM_CYHSCROLL) * m_scale);
		}
	}
	else
	{
		if(metric == SM_CYVERTSB)
		{
			return (int)(GetSystemMetrics(SM_CYVSCROLL) * m_scale);
		}
		else
		{
  		return (int)(GetSystemMetrics(SM_CXVSCROLL) * m_scale);
		}
	}

	return 0;
}
static int GetZoomButtonSize(BOOL isVert)
{
  return  (int)(GetSystemMetrics(isVert ? SM_CYVSCROLL : SM_CXHSCROLL) * m_scale);
}


//
//	
//
static COLORREF GetSBForeColor(HWND hwnd)
{
	COLORREF c1 = CoolSB_GetSysColor(hwnd,COLOR_3DHILIGHT);
	COLORREF c2 = CoolSB_GetSysColor(hwnd,COLOR_WINDOW);

	if(c1 != 0xffffff && c1 == c2)
	{
		return CoolSB_GetSysColor(hwnd,COLOR_BTNFACE);
	}
	else
	{
		return CoolSB_GetSysColor(hwnd,COLOR_3DHILIGHT);
	}
}

static COLORREF GetSBBackColor(HWND hwnd)
{
	return CoolSB_GetSysColor(hwnd,COLOR_SCROLLBAR);
}


void DrawAdHocVScrollbarEx(LICE_IBitmap* dest, RECT* r, int pos, int page, int max, int wtheme)
{
  const wdlscrollbar_themestate *theme = &s_scrollbar_theme[wtheme < 0 || wtheme >= MAX_SCROLLBAR_THEMES ? 0 : wtheme];
  LICE_IBitmap* src=*theme->bmp;
  if (!src) return;

  int x=r->left;
  int y=r->top;
  int w=r->right-r->left;
  int h=r->bottom-r->top;

  int range=h-17*2;
  int thumb=range*page/max;
  int tpos=(range*pos)/max;

  if (theme->hasPink) 
  { 
    LICE_ScaledBlit(dest, src, 
                    x, y, w, theme->bkgvt, 
                    170, 37, 17, theme->bkgvt, 
                    1.0f, LICE_BLIT_FILTER_BILINEAR);
    LICE_ScaledBlit(dest, src, 
                    x, y+theme->bkgvt, w, h-theme->bkgvt-theme->bkgvb, 
                    170, 37+theme->bkgvt, 17, 238-37-theme->bkgvt-theme->bkgvb, 
                    1.0f, LICE_BLIT_FILTER_BILINEAR);
    LICE_ScaledBlit(dest, src, x, y+h-theme->bkgvb, w, theme->bkgvb, 
                    170, 238-theme->bkgvb, 17, theme->bkgvb,
                    1.0f, LICE_BLIT_FILTER_BILINEAR);

    int th=(thumb-theme->thumbVV[0]-theme->thumbVV[2]-theme->thumbVV[4])/2;
    if (th < 0) th=0;

    LICE_ScaledBlit(dest, src,
                    x, y+17+tpos, w, theme->thumbVV[0], 
                    0, 91, 17, theme->thumbVV[0], 
                    1.0f, LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
    LICE_ScaledBlit(dest, src,
                    x, y+17+tpos+theme->thumbVV[0], w, th, 
                    0, 91+theme->thumbVV[0], 17, theme->thumbVV[1], 
                    1.0f, LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
    LICE_ScaledBlit(dest, src,
                    x, y+17+tpos+theme->thumbVV[0]+th, w, theme->thumbVV[2], 
                    0, 91+theme->thumbVV[0]+theme->thumbVV[1], 17, theme->thumbVV[2], 
                    1.0f, LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
    LICE_ScaledBlit(dest, src,
                    x, y+17+tpos+theme->thumbVV[0]+th+theme->thumbVV[2], w, th, 
                    0, 91+theme->thumbVV[0]+theme->thumbVV[1]+theme->thumbVV[2], 17, theme->thumbVV[3], 
                    1.0f, LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
    LICE_ScaledBlit(dest, src,
                    x, y+17+tpos+theme->thumbVV[0]+th+theme->thumbVV[2]+th, w, theme->thumbVV[4], 
                    0, 91+theme->thumbVV[0]+theme->thumbVV[1]+theme->thumbVV[2]+theme->thumbVV[3], 17, theme->thumbVV[4], 
                    1.0f, LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
  }
  else
  {
    LICE_ScaledBlit(dest, src,
                    x, y, w, h, 
                    170, 34, 17, 238-34, 
                    1.0f, LICE_BLIT_FILTER_BILINEAR);
 
    LICE_ScaledBlit(dest, src,
                    x, y+17+tpos, w, thumb, 
                    0, 90, 17, 238-90, 1.0f, 
                    LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
  }

  LICE_ScaledBlit(dest, src,
                  x, y, w, w,
                  116, 121, 17, 17, 
                  1.0f, LICE_BLIT_FILTER_BILINEAR);
  LICE_ScaledBlit(dest, src,
                  x, y+h-w, w, w,
                  116, 181, 17, 17, 
                  1.0f, LICE_BLIT_FILTER_BILINEAR);
}
void DrawAdHocVScrollbar(LICE_IBitmap* dest, RECT* r, int pos, int page, int max)
{
  DrawAdHocVScrollbarEx(dest,r,pos,page,max,0);
}


//
//	Paint a checkered rectangle, with each alternate
//	pixel being assigned a different colour
//
static void DrawCheckedRect(const wdlscrollbar_themestate *theme, LICE_IBitmap *bmOut, HDC hdc, RECT *rect, COLORREF fg, COLORREF bg, SCROLLBAR *sb, const RECT *wndrect, int on, int offsx=0, int offsy=0)
{
  int isvert = sb->nBarType==SB_VERT;

  LICE_IBitmap *bmp;
  if(theme->bmp && (bmp = *theme->bmp))
  {
    int w = rect->right-rect->left;
    int h = rect->bottom-rect->top;
    int ww = wndrect->right - wndrect->left;
    int wh = wndrect->bottom - wndrect->top;
    if(isvert)
    {
      ww = wndrect->bottom - wndrect->top;
      wh = wndrect->right - wndrect->left;
    }
    int nw = ww;
    int nh = wh;
    if(!isvert) nh *= 2;
    else nw *= 2;

    if(!sb->liceBkgnd || sb->liceBkgnd->getWidth()!=nw || sb->liceBkgnd->getHeight()!=nh || sb->liceBkgnd_ver!=theme->imageVersion)
    {
      sb->liceBkgnd_ver=theme->imageVersion;
      if(!sb->liceBkgnd) sb->liceBkgnd = new LICE_SysBitmap;
      sb->liceBkgnd->resize(nw, nh);
      if(!isvert)
      {
        int desth = nh/2;
        if(theme->hasPink)
        {
          LICE_ScaledBlit(sb->liceBkgnd, bmp, 0, 0, theme->bkghl, desth, 
                                     0, 0, theme->bkghl, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, theme->bkghl, 0, ww-theme->bkghl-theme->bkghr, desth,
                                     theme->bkghl, 0, 204-theme->bkghl-theme->bkghr, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, ww-theme->bkghr, 0, theme->bkghr, desth, 
                                     204-theme->bkghr, 0, theme->bkghr, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, 0, desth, theme->bkghl, desth, 
                                     0, 17, theme->bkghl, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, theme->bkghl, desth, ww-theme->bkghl-theme->bkghr, desth, 
                                     theme->bkghl, 17, 204-theme->bkghl-theme->bkghr, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, ww-theme->bkghr, desth, theme->bkghr, desth, 204-theme->bkghr, 
                                     17, theme->bkghr, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        }
        else
        {
          LICE_ScaledBlit(sb->liceBkgnd, bmp, 0, 0, ww, desth, 0, 0, 204, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, 0, desth, ww, desth, 0, 17, 204, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        }
      }
      else
      {
        int destw = nw/2;
        int starty = 34;
        if(theme->hasPink) 
        {
          starty = 37;
          LICE_ScaledBlit(sb->liceBkgnd, bmp, 0, 0, destw, theme->bkgvt, 
                                     170, starty, 17, theme->bkgvt, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, 0, theme->bkgvt, destw, wh-theme->bkgvt-theme->bkgvb, 
                                     170, starty+theme->bkgvt, 17, 238-starty-theme->bkgvt-theme->bkgvb, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, 0, wh-theme->bkgvb, destw, theme->bkgvb, 
                                     170, 238-theme->bkgvb, 17, theme->bkgvb, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, destw, 0, destw, theme->bkgvt, 
                                     187, starty, 17, theme->bkgvt, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, destw, theme->bkgvt, destw, wh-theme->bkgvt-theme->bkgvb, 
                                     187, starty+theme->bkgvt, 17, 238-starty-theme->bkgvt-theme->bkgvb, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, destw, wh-theme->bkgvb, destw, theme->bkgvb, 
                                     187, 238-theme->bkgvb, 17, theme->bkgvb, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        }
        else
        {
          LICE_ScaledBlit(sb->liceBkgnd, bmp, 0, 0, destw, wh, 170, starty, 17, 238-starty, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(sb->liceBkgnd, bmp, destw, 0, destw, wh, 187, starty, 17, 238-starty, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        }
      }
    }
    if(!isvert)
    {
//      if (nh/2 != h) OutputDebugString("blah\n");
      if (bmOut)
        LICE_ScaledBlit(bmOut, sb->liceBkgnd, rect->left,rect->top, w, h,  
                      rect->left+offsx,on?nh/2:0, w, nh/2, 1.0f, LICE_BLIT_MODE_COPY);
      else
      {
        BitBlt(hdc, rect->left, rect->top, w, h, sb->liceBkgnd->getDC(), rect->left+offsx, on?nh/2:0, SRCCOPY);
      }
    }
    else
    {
//      if (nw/2 != w) OutputDebugString("blah2\n");
      if (bmOut)
        LICE_ScaledBlit(bmOut, sb->liceBkgnd, rect->left,rect->top, w, h,  on?nw/2:0, rect->top+offsy, nw/2, h, 1.0f, LICE_BLIT_MODE_COPY);
      else
      {
        BitBlt(hdc, rect->left, rect->top, w, h, sb->liceBkgnd->getDC(), on?nw/2:0, rect->top+offsy, SRCCOPY);
      }
    }
    return;
  }


#ifdef _WIN32
	static WORD wCheckPat[8] =
	{
		0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555
	};

	HBITMAP hbmp;
	HBRUSH  hbr, hbrold;
	COLORREF fgold, bgold;

	hbmp = CreateBitmap(8, 8, 1, 1, wCheckPat);
	hbr  = CreatePatternBrush(hbmp);

	UnrealizeObject(hbr);
	SetBrushOrgEx(hdc, rect->left, rect->top, 0);

	hbrold = (HBRUSH)SelectObject(hdc, hbr);

	fgold = SetTextColor(hdc, fg);
	bgold = SetBkColor(hdc, bg);
	
	PatBlt(hdc, rect->left, rect->top, 
				rect->right - rect->left, 
				rect->bottom - rect->top, 
				PATCOPY);
	
	SetBkColor(hdc, bgold);
	SetTextColor(hdc, fgold);
	
	SelectObject(hdc, hbrold);
	DeleteObject(hbr);
	DeleteObject(hbmp);
#else
  HBRUSH br=CreateSolidBrush(RGB(100,100,100));
  FillRect(hdc,rect,br);
  DeleteObject(br);
  //FUCKO> osx version!
#endif

}

//
//	Fill the specifed rectangle using a solid colour
//
static void PaintRect(HDC hdc, RECT *rect, COLORREF color)
{
#ifdef _WIN32
	COLORREF oldcol = SetBkColor(hdc, color);
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, rect, "", 0, 0);
	SetBkColor(hdc, oldcol);
#else
  HBRUSH br=CreateSolidBrush(color);
  FillRect(hdc,rect,br);
  DeleteObject(br);
#endif
}

//
//	Draw a simple blank scrollbar push-button. Can be used
//	to draw a push button, or the scrollbar thumb
//	drawflag - could set to BF_FLAT to make flat scrollbars
//
static void DrawBlankButton(HWND hwnd, HDC hdc, const RECT *rect)
{
	RECT rc = *rect;
  HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
			
	ownDrawEdge(hwnd,hdc, &rc, EDGE_RAISED, BF_RECT | BF_ADJUST);
	FillRect(hdc, &rc, br);
  DeleteObject(br);
}

//
//	Send a WM_VSCROLL or WM_HSCROLL message
//
static void SendScrollMessage(HWND hwnd, UINT scrMsg, UINT scrId, UINT pos)
{
	SendMessage(hwnd, scrMsg, MAKEWPARAM(scrId, pos), 0);
}

//
//	Calculate the screen coordinates of the area taken by
//  the horizontal scrollbar. Take into account the size
//  of the window borders
//
static BOOL GetHScrollRect(SCROLLWND *sw, HWND hwnd, RECT *rect, BOOL *hasZoomButtons)
{
	GET_WINDOW_RECT(hwnd, rect);
  
	if(sw->fLeftScrollbar)
	{
		rect->left  += sw->cxLeftEdge + (sw->sbarVert.fScrollVisible ? 
					GetScrollMetric(TRUE, SM_CXVERTSB) : 0);
		rect->right -= sw->cxRightEdge;
	}
	else
	{
		rect->left   += sw->cxLeftEdge;					//left window edge
	
		rect->right  -= sw->cxRightEdge +				//right window edge
					(sw->sbarVert.fScrollVisible ? 
					GetScrollMetric(TRUE, SM_CXVERTSB) : 0);
	}
	
	rect->bottom -= sw->cyBottomEdge;				//bottom window edge
	
	rect->top	  = rect->bottom -
					(sw->sbarHorz.fScrollVisible ?
					GetScrollMetric(FALSE, SM_CYHORZSB) : 0);

  if (hasZoomButtons) *hasZoomButtons=0;
  if(sw->resizingHthumb)
  {
    int zbs = GetZoomButtonSize(FALSE);
    if (rect->right - rect->left >= MIN_SIZE_FOR_ZOOMBUTTONS(zbs))
    {
      if (hasZoomButtons) *hasZoomButtons=1;
      rect->right -= zbs*2 + ZOOMBUTTON_RESIZER_SIZE(zbs);
    }
  }
	//printf("ry=%d,%d\n",rect->top,rect->bottom);

	return TRUE;
}

//
//	Calculate the screen coordinates of the area taken by the
//  vertical scrollbar
//
static BOOL GetVScrollRect(SCROLLWND *sw, HWND hwnd, RECT *rect, BOOL *hasZoomButtons)
{
	GET_WINDOW_RECT(hwnd, rect);
	rect->top	 += sw->cyTopEdge + sw->vscrollbarShrinkTop;						//top window edge
	
	rect->bottom -= sw->cyBottomEdge + sw->vscrollbarShrinkBottom + 
					(sw->sbarHorz.fScrollVisible ?		//bottom window edge
					GetScrollMetric(FALSE, SM_CYHORZSB) : 0);

	if(sw->fLeftScrollbar)
	{
		rect->left	+= sw->cxLeftEdge;
		rect->right = rect->left + (sw->sbarVert.fScrollVisible ?
					GetScrollMetric(TRUE, SM_CXVERTSB) : 0);
	}
	else
	{
		rect->right  -= sw->cxRightEdge;
		rect->left    = rect->right - (sw->sbarVert.fScrollVisible ?	
					GetScrollMetric(TRUE, SM_CXVERTSB) : 0);
	}

  if (hasZoomButtons) *hasZoomButtons=0;
  if(sw->resizingHthumb)
  {
    int zbs = GetZoomButtonSize(TRUE);
    if (rect->bottom - rect->top >= MIN_SIZE_FOR_ZOOMBUTTONS(zbs))
    {
      if (hasZoomButtons) *hasZoomButtons=1;   
      rect->bottom -= zbs*2 + ZOOMBUTTON_RESIZER_SIZE(zbs);
    }
  }

	return TRUE;
}

//	Depending on what type of scrollbar nBar refers to, call the
//  appropriate Get?ScrollRect function
//
static BOOL GetScrollRect(SCROLLWND *sw, UINT nBar, HWND hwnd, RECT *rect, BOOL *hasZoomButtons)
{
	if(nBar == SB_HORZ)
		return GetHScrollRect(sw, hwnd, rect,hasZoomButtons);
	else if(nBar == SB_VERT)
		return GetVScrollRect(sw, hwnd, rect,hasZoomButtons);
	else
  {
    if (hasZoomButtons) *hasZoomButtons=0;
		return FALSE;
  }
}



//
//	Work out the scrollbar width/height for either type of scrollbar (SB_HORZ/SB_VERT)
//	rect - coords of the scrollbar.
//	store results into *thumbsize and *thumbpos
//
static int CalcThumbSize(SCROLLBAR *sbar, const RECT *rect, int *pthumbsize, int *pthumbpos)
{
	SCROLLINFO *si;
	int scrollsize;			//total size of the scrollbar including arrow buttons
	int workingsize;		//working area (where the thumb can slide)
	int siMaxMin;
	int butsize;
	int startcoord;
	int thumbpos = 0, thumbsize = 0;


	//work out the width (for a horizontal) or the height (for a vertical)
	//of a standard scrollbar button
	butsize = GetScrollMetric(sbar->nBarType == SB_VERT, SM_SCROLL_LENGTH);

	if(1) //sbar->nBarType == SB_HORZ)
	{
		scrollsize = rect->right - rect->left;
		startcoord = rect->left;
	}
	/*else if(sbar->nBarType == SB_VERT)
	{
		scrollsize = rect->bottom - rect->top;
		startcoord = rect->top;
	}
	else
	{
		return 0;
	}*/

	si = &sbar->scrollInfo;
	siMaxMin = si->nMax - si->nMin + 1;
	workingsize = scrollsize - butsize * 2;

	//
	// Work out the scrollbar thumb SIZE
	//
	if(si->nPage == 0)
	{
		thumbsize = butsize;
	}
	else if(siMaxMin > 0)
	{
		thumbsize = MulDiv(si->nPage, workingsize, siMaxMin);

		if(thumbsize < sbar->nMinThumbSize)
			thumbsize = sbar->nMinThumbSize;
	}

	//
	// Work out the scrollbar thumb position
	//
	if(siMaxMin > 0)
	{
		int pagesize = wdl_max(1, si->nPage);
		thumbpos = MulDiv(si->nPos - si->nMin, workingsize-thumbsize, siMaxMin - pagesize);
		
		if(thumbpos < 0)						
			thumbpos = 0;

		if(thumbpos >= workingsize-thumbsize)	
			thumbpos = workingsize-thumbsize;
	}

	thumbpos += startcoord + butsize;

	*pthumbpos  = thumbpos;
	*pthumbsize = thumbsize;

	return 1;
}

//
//	return a hit-test value for whatever part of the scrollbar x,y is located in
//	rect, x, y: SCREEN coordinates
//	the rectangle must not include space for any inserted buttons 
//	(i.e, JUST the scrollbar area)
//
static UINT GetHorzScrollPortion(SCROLLBAR *sbar, HWND hwnd, const RECT *rect, int x, int y, BOOL hasZoomButtons)
{
	int thumbwidth, thumbpos;
	int butwidth = GetScrollMetric(sbar->nBarType == SB_VERT, SM_SCROLL_LENGTH);
	int scrollwidth  = rect->right-rect->left;
	int workingwidth = scrollwidth - butwidth*2;
  SCROLLWND *sw = GetScrollWndFromHwnd(hwnd);

	if(y < rect->top || y >= rect->bottom)
		return HTSCROLL_NONE;

	CalcThumbSize(sbar, rect, &thumbwidth, &thumbpos);

	//if we have had to scale the buttons to fit in the rect,
	//then adjust the button width accordingly
	if(scrollwidth <= butwidth * 2)
	{
		butwidth = scrollwidth / 2;	
	}

  if(sw->resizingHthumb&&hasZoomButtons)
  {
    //check for resizer
    if(x>=rect->right)
    {
      const int zbs = GetZoomButtonSize(sbar->nBarType==SB_VERT);
      const int zrs = ZOOMBUTTON_RESIZER_SIZE(zbs);
      if (x < rect->right+zbs) return HTSCROLL_ZOOMIN;
      if (x < rect->right+zbs+zrs) return HTSCROLL_RESIZER;
      if (x < rect->right+zbs*2+zrs) return HTSCROLL_ZOOMOUT;
    }
  }
  
	//check for left button click
	if(x >= rect->left && x < rect->left + butwidth)
	{
		return HTSCROLL_LEFT;	
	}
	//check for right button click
	else if(x >= rect->right-butwidth && x < rect->right)
	{
		return HTSCROLL_RIGHT;
	}
	
	//if the thumb is too big to fit (i.e. it isn't visible)
	//then return a NULL scrollbar area
	if(thumbwidth >= workingwidth)
		return HTSCROLL_NONE;

	//check for point in the thumbbar
	if(x >= thumbpos && x < thumbpos+thumbwidth)
	{
    if(sw->resizingHthumb)
    {
      if(sbar->nBarType == SB_HORZ) //only for horizontal
      {
        if(x<=thumbpos+m_thumbsize)
          return HTSCROLL_LRESIZER;
        if(x>=(thumbpos+thumbwidth-m_thumbsize))
          return HTSCROLL_RRESIZER;
      }
    }
		return HTSCROLL_THUMB;
	}	
	//check for left margin
	else if(x >= rect->left+butwidth && x < thumbpos)
	{
		return HTSCROLL_PAGELEFT;
	}
	else if(x >= thumbpos+thumbwidth && x < rect->right-butwidth)
	{
		return HTSCROLL_PAGERIGHT;
	}
	
	return HTSCROLL_NONE;
}

//
//	For vertical scrollbars, rotate all coordinates by -90 degrees
//	so that we can use the horizontal version of this function
//
static UINT GetVertScrollPortion(SCROLLBAR *sb, HWND hwnd, RECT *rect, int x, int y, BOOL hasZoomButtons)
{
	UINT r;
	
	RotateRect(rect);
	r = GetHorzScrollPortion(sb, hwnd, rect, y, x,hasZoomButtons);
	RotateRect(rect);
	return r;
}



static void drawSkinThumb(HDC hdc, RECT r, int fBarHot, int pressed, int vert, const RECT *wndrect, SCROLLBAR *sb, SCROLLWND *sw, const wdlscrollbar_themestate *theme)
{
  LICE_IBitmap *bmp;
  if(theme->bmp && (bmp = *theme->bmp))
  {
    int w = r.right-r.left;
    int h = r.bottom-r.top;
    int startx = 0;
    int starty = 187;
    if(theme->hasPink) starty = 37;
    if(!vert)
    {
      static LICE_SysBitmap tmpbmp;
      if (w>tmpbmp.getWidth() || h>tmpbmp.getHeight())
        tmpbmp.resize(wdl_max(w,tmpbmp.getWidth()), wdl_max(h,tmpbmp.getHeight()));

      //draw background first so alpha channel thumbs work
      {
        RECT bgr = {0,0,w,h};
        DrawCheckedRect(theme,&tmpbmp,tmpbmp.getDC(), &bgr, 0, 0, sb, wndrect, 0, r.left);
      }

      int st = (fBarHot?1:0) + (pressed?2:0);
      int neww = w;
      int part1 = 16, part2 = 10, part3 = 14, part4 = 10, part5 = 16;
      if(theme->hasPink)
      {
        part1 = theme->thumbHV[0]; part2 = theme->thumbHV[1]; part3 = theme->thumbHV[2]; part4 = theme->thumbHV[3]; part5 = theme->thumbHV[4];
      }

      double sc = h==16||h==17 ? 1.0 : h / 17.0;

      int part1_s = (int)(part1*sc+0.5);
      int part3_s = (int)(part3*sc+0.5);
      int part5_s = (int)(part5*sc+0.5);
      int tl = part1_s+part3_s+part5_s;
      if(w<tl) w = tl;

      if(!sb->liceThumb || sb->liceThumb->getWidth()!=w || sb->liceThumb->getHeight()!=h || sb->liceThumbState!=st || sb->liceThumb_ver!=theme->imageVersion)
      {
        sb->liceThumb_ver=theme->imageVersion;
        if(!sb->liceThumb) sb->liceThumb = new LICE_SysBitmap;
        sb->liceThumb->resize(w, h);
        sb->liceThumbState = st;

        if(fBarHot) starty += 17;
        else if(pressed) starty += 17*2;

        int mid = (w-part3_s)/2;
        LICE_ScaledBlit(sb->liceThumb, bmp, 0, 0, part1_s, h, startx, starty, part1, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        LICE_ScaledBlit(sb->liceThumb, bmp, part1_s, 0, mid-part1_s, h, startx+part1, starty, part2, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        LICE_ScaledBlit(sb->liceThumb, bmp, mid, 0, part3_s, h, startx+part1+part2, starty, part3, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        LICE_ScaledBlit(sb->liceThumb, bmp, mid+part3_s, 0, w-(mid+part3_s)-part5, h, startx+part1+part2+part3, starty, part4, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        LICE_ScaledBlit(sb->liceThumb, bmp, w-part5_s, 0, part5_s, h, startx+part1+part2+part3+part4, starty, part5, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        
      }

      LICE_ScaledBlit(&tmpbmp, sb->liceThumb, 0, 0, neww, h, 0, 0, w, h, 1.0f, LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
      BitBlt(hdc, r.left, r.top, neww, h, tmpbmp.getDC(), 0, 0, SRCCOPY);
    }
    else
    {
      static LICE_SysBitmap tmpbmp;
      if (w>tmpbmp.getWidth() || h>tmpbmp.getHeight())
        tmpbmp.resize(wdl_max(w,tmpbmp.getWidth()), wdl_max(h,tmpbmp.getHeight()));
      starty = 116;
      if(theme->hasPink) starty = 91;

      //draw background first so alpha channel thumbs work
      {
        RECT bgr = {0,0,w,h};
        DrawCheckedRect(theme,&tmpbmp,tmpbmp.getDC(), &bgr, 0, 0, sb, wndrect, 0, 0, r.top - sw->vscrollbarShrinkTop);
      }

      int st = (fBarHot?1:0) + (pressed?2:0);

      int newh = h;
      int part1 = 8, part2 = 16, part3 = 18, part4 = 16, part5 = 8;
      if(theme->hasPink)
      {
        part1 = theme->thumbVV[0]; part2 = theme->thumbVV[1]; part3 = theme->thumbVV[2]; part4 = theme->thumbVV[3]; part5 = theme->thumbVV[4];
      }

      double sc = w==16||w==17 ? 1.0 : w / 17.0;

      int part1_s = (int)(part1*sc+0.5);
      int part3_s = (int)(part3*sc+0.5);
      int part5_s = (int)(part5*sc+0.5);
      int tl = part1_s+part3_s+part5_s;
      if(h<tl) h = tl;

      if(!sb->liceThumb || sb->liceThumb->getWidth()!=w || sb->liceThumb->getHeight()!=h || sb->liceThumbState!=st || sb->liceThumb_ver!=theme->imageVersion)
      {
        sb->liceThumb_ver = theme->imageVersion;
        if(!sb->liceThumb) sb->liceThumb = new LICE_SysBitmap;
        sb->liceThumb->resize(w, h);

        sb->liceThumbState = st;

        if(fBarHot) startx += 17;
        else if(pressed) startx += 17*2;
      
        int mid = (h-part3)/2;
        LICE_ScaledBlit(sb->liceThumb, bmp, 0, 0, w, part1_s, startx, starty, 17, part1, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        LICE_ScaledBlit(sb->liceThumb, bmp, 0, part1_s, w, mid-part1_s, startx, starty+part1, 17, part2, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        LICE_ScaledBlit(sb->liceThumb, bmp, 0, mid, w, part3_s, startx, starty+part1+part2, 17, part3, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        LICE_ScaledBlit(sb->liceThumb, bmp, 0, mid+part3_s, w, h-(mid+part3_s)-part5_s, startx, starty+part1+part2+part3, 17, part4, 1.0f, LICE_BLIT_FILTER_BILINEAR);
        LICE_ScaledBlit(sb->liceThumb, bmp, 0, h-part5_s, w, part5_s, startx, starty+part1+part2+part3+part4, 17, part5, 1.0f, LICE_BLIT_FILTER_BILINEAR);
      }

      LICE_ScaledBlit(&tmpbmp, sb->liceThumb, 0, 0, w, newh, 0, 0, w, h, 1.0f, LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
      BitBlt(hdc, r.left, r.top, w, newh, tmpbmp.getDC(), 0, 0, SRCCOPY);
    }
  }
}


//
//	Draw a complete HORIZONTAL scrollbar in the given rectangle
//	Don't draw any inserted buttons in this procedure
//	
//	uDrawFlags - hittest code, to say if to draw the
//  specified portion in an active state or not.
//
//
static LRESULT NCDrawHScrollbar(SCROLLBAR *sb, HWND hwnd, HDC hdc, const RECT *rect, UINT uDrawFlags, BOOL hasZoomButtons, const wdlscrollbar_themestate *theme)
{
	SCROLLINFO *si;
	RECT ctrl, thumb;
	RECT sbm;
	int butwidth	 = GetScrollMetric(sb->nBarType == SB_VERT, SM_SCROLL_LENGTH);
	int scrollwidth  = rect->right-rect->left;
	int workingwidth = scrollwidth - butwidth*2;
	int thumbwidth   = 0, thumbpos = 0;

	BOOL fMouseDownL = 0, fMouseOverL = 0, fBarHot = 0;
	BOOL fMouseDownR = 0, fMouseOverR = 0;

	COLORREF crCheck1   = GetSBForeColor(hwnd);
	COLORREF crCheck2   = GetSBBackColor(hwnd);
	COLORREF crInverse1 = InvertCOLORREF(crCheck1);
	COLORREF crInverse2 = InvertCOLORREF(crCheck2);

	//drawing flags to modify the appearance of the scrollbar buttons
	UINT uLeftButFlags  = DFCS_SCROLLLEFT;
	UINT uRightButFlags = DFCS_SCROLLRIGHT;

  SCROLLWND *sw = GetScrollWndFromHwnd(hwnd);

	if(scrollwidth <= 0)
		return 0;

	si = &sb->scrollInfo;

  int sbYoffs=-(int)sw->vscrollbarShrinkTop,sbXoffs=0;
#ifdef _WIN32
    // this is a stupid fix for now . this needs a ton of overhauling
  {
    RECT r;
    POINT p={0,0};
    ClientToScreen(hwnd,&p);
    GetWindowRect(hwnd,&r);

    if (sb == &sw->sbarVert) sbYoffs =  r.top - p.y - sw->vscrollbarShrinkTop;
    if (sb == &sw->sbarHorz) sbXoffs =  r.left - p.x;
  }
#endif


	if(hwnd != hwndCurCoolSB)
		uDrawFlags = HTSCROLL_NONE;
	//
	// work out the thumb size and position
	//
	CalcThumbSize(sb, rect, &thumbwidth, &thumbpos);
	
	if(sb->fScrollFlags & ESB_DISABLE_LEFT)		uLeftButFlags  |= DFCS_INACTIVE;
	if(sb->fScrollFlags & ESB_DISABLE_RIGHT)	uRightButFlags |= DFCS_INACTIVE;

	//if we need to grey the arrows because there is no data to scroll
	if(!IsScrollInfoActive(si) && !(sb->fScrollFlags & CSBS_THUMBALWAYS))
	{
		uLeftButFlags  |= DFCS_INACTIVE;
		uRightButFlags |= DFCS_INACTIVE;
	}

	if(hwnd == hwndCurCoolSB)
	{
		fMouseDownL = (uDrawFlags == HTSCROLL_LEFT);
		fMouseDownR = (uDrawFlags == HTSCROLL_RIGHT);
	}


  int fMouseOverPlus, fMouseOverMinus;
  {
    BOOL ldis = !(uLeftButFlags & DFCS_INACTIVE);
    BOOL rdis = !(uRightButFlags & DFCS_INACTIVE);
    
    fBarHot = sb->nBarType == (int)sw->uMouseOverScrollbar;
    
    fMouseOverL = sw->uHitTestPortion == HTSCROLL_LEFT && fBarHot && ldis;		
    fMouseOverR = sw->uHitTestPortion == HTSCROLL_RIGHT && fBarHot && rdis;
    fMouseOverPlus = sw->uHitTestPortion == HTSCROLL_ZOOMIN && fBarHot && ldis;
    fMouseOverMinus = sw->uHitTestPortion == HTSCROLL_ZOOMOUT && fBarHot && ldis;
  }

	//
	// Draw the scrollbar now
	//
	if(scrollwidth > butwidth*2)
	{
		//LEFT ARROW
		SetRect(&ctrl, rect->left, rect->top, rect->left + butwidth, rect->bottom);

		RotateRect0(sb, &ctrl);

  	DrawScrollArrow(hwnd,sb, hdc, &ctrl, uLeftButFlags, fMouseDownL, fMouseOverL, theme);

		RotateRect0(sb, &ctrl);

		//MIDDLE PORTION
		//if we can fit the thumbbar in, then draw it
		if(thumbwidth > 0 && thumbwidth <= workingwidth
			&& IsScrollInfoActive(si) && ((sb->fScrollFlags & ESB_DISABLE_BOTH) != ESB_DISABLE_BOTH))
		{	
			//Draw the scrollbar margin above the thumb
			SetRect(&sbm, rect->left + butwidth, rect->top, thumbpos, rect->bottom);
			
			RotateRect0(sb, &sbm);
			
			if(uDrawFlags == HTSCROLL_PAGELEFT)
				DrawCheckedRect(theme,NULL,hdc, &sbm, crInverse1, crInverse2, sb, rect, 1,sbXoffs,sbYoffs);
			else
				DrawCheckedRect(theme,NULL,hdc, &sbm, crCheck1, crCheck2, sb, rect, 0,sbXoffs,sbYoffs);

			RotateRect0(sb, &sbm);			
			
			//Draw the margin below the thumb
			sbm.left = thumbpos+thumbwidth;
			sbm.right = rect->right - butwidth;
			
			RotateRect0(sb, &sbm);
			if(uDrawFlags == HTSCROLL_PAGERIGHT)
				DrawCheckedRect(theme,NULL,hdc, &sbm, crInverse1, crInverse2, sb, rect, 1,sbXoffs,sbYoffs);
			else
				DrawCheckedRect(theme,NULL,hdc, &sbm, crCheck1, crCheck2, sb, rect, 0,sbXoffs,sbYoffs);
			RotateRect0(sb, &sbm);
			
			//Draw the THUMB finally
			SetRect(&thumb, thumbpos, rect->top, thumbpos+thumbwidth, rect->bottom);

			RotateRect0(sb, &thumb);			

      if(theme->bmp && *theme->bmp)
      {
        int is_tracking = sw->fThumbTracking &&
                          sw->uCurrentScrollbar == sb->nBarType &&
                          GetCapture()==hwnd;

        drawSkinThumb(hdc, thumb, 
            !is_tracking && sw->uHitTestPortion == HTSCROLL_THUMB, is_tracking, 
            sb->nBarType == SB_VERT, rect, sb,sw,theme);
      }
      else
      {
        //no skinning

        {
          RECT r = thumb;
          if(sw->resizingHthumb)
          {
            if(sb->nBarType == SB_HORZ)
            {
              r.left += m_thumbsize;
              r.right -= m_thumbsize;
            }
            else
            {
              //disabled for now
              /*r.top += m_thumbsize;
              r.bottom -= m_thumbsize;*/
            }
          }
          DrawBlankButton(hwnd,hdc, &r);
        }
        
        if(sw->resizingHthumb)
        {
          //draw left and right resizers
          if(sb->nBarType == SB_HORZ)
          {
            HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
            {
              RECT r={thumb.left, thumb.top, thumb.left+m_thumbsize, thumb.bottom};
              ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
              FillRect(hdc, &r, br);
            }
            {
              RECT r={thumb.right-m_thumbsize, thumb.top, thumb.right, thumb.bottom};
              ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
              FillRect(hdc, &r, br);
            }
            DeleteObject(br);
          }
          else
          {
            //disabled for now
            /*HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
            {
            RECT r={thumb.left, thumb.top, thumb.right, thumb.top+m_thumbsize};
            ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
            FillRect(hdc, &r, br);
            }
            {
            RECT r={thumb.left, thumb.bottom - m_thumbsizeE, thumb.right, thumb.bottom};
            ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
            FillRect(hdc, &r, br);
            }
            DeleteObject(br);*/
          }
        }
      }
      RotateRect0(sb, &thumb);

		}
		//otherwise, just leave that whole area blank
		else
		{
			OffsetRect(&ctrl, butwidth, 0);
			ctrl.right = rect->right - butwidth;

			//if we always show the thumb covering the whole scrollbar,
			//then draw it that way
			if(!IsScrollInfoActive(si)	&& (sb->fScrollFlags & CSBS_THUMBALWAYS) 
				&& ctrl.right - ctrl.left > sb->nMinThumbSize)
			{
				//leave a 1-pixel gap between the thumb + right button
				ctrl.right --;
				RotateRect0(sb, &ctrl);

				DrawBlankButton(hwnd,hdc, &ctrl);

				RotateRect0(sb, &ctrl);

				//draw the single-line gap
				ctrl.left = ctrl.right;
				ctrl.right += 1;
				
        RECT r2 = ctrl;
        r2.right -= 1;
				RotateRect0(sb, &ctrl);
        RotateRect0(sb, &r2);
				
  			PaintRect(hdc, &r2, CoolSB_GetSysColor(hwnd,COLOR_SCROLLBAR));

				RotateRect0(sb, &ctrl);
			}
			//otherwise, paint a blank if the thumb doesn't fit in
			else
			{
				RotateRect0(sb, &ctrl);
	
  			DrawCheckedRect(theme,NULL,hdc, &ctrl, crCheck1, crCheck2, sb, rect, 0, sbXoffs,sbYoffs);
				
				RotateRect0(sb, &ctrl);
			}
		}

		//RIGHT ARROW
		SetRect(&ctrl, rect->right - butwidth, rect->top, rect->right, rect->bottom);

    RECT r2 = ctrl;
   // r2.right -= 1;
    RotateRect0(sb, &ctrl);
    RotateRect0(sb, &r2);

		DrawScrollArrow(hwnd,sb, hdc, &r2, uRightButFlags, fMouseDownR, fMouseOverR,theme);

    if(sw->resizingHthumb && hasZoomButtons)
    {
    //zoom/resize buttons
    {
      SetBkMode(hdc, TRANSPARENT);
      if(sb->nBarType == SB_HORZ)
      {
        int zbs = GetZoomButtonSize(FALSE);
        if(theme->bmp && *theme->bmp)
        {
          LICE_IBitmap *bmp = *theme->bmp;
          static LICE_SysBitmap tmpbmp;

          int w = zbs;
          int h = ctrl.bottom-ctrl.top;
          int startx = 116;
          int starty = 201;
          if(fMouseOverPlus) startx += 17;
          if(uDrawFlags == HTSCROLL_ZOOMIN) startx = 116+17+17;
          if (w>tmpbmp.getWidth() || h>tmpbmp.getHeight())
            tmpbmp.resize(wdl_max(w,tmpbmp.getWidth()), wdl_max(h,tmpbmp.getHeight()));
          LICE_ScaledBlit(&tmpbmp, bmp, 0, 0, w, h, startx, starty, 17, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          
          BitBlt(hdc, ctrl.right, ctrl.top, w, h, tmpbmp.getDC(), 0, 0, SRCCOPY);

          LICE_ScaledBlit(&tmpbmp, bmp, 0, 0, ZOOMBUTTON_RESIZER_SIZE(zbs), h, 163, 101, 4, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          BitBlt(hdc, ctrl.right+zbs, ctrl.top, ZOOMBUTTON_RESIZER_SIZE(zbs), h, tmpbmp.getDC(), 0, 0, SRCCOPY);

          startx = 116;
          starty = 221;
          if(fMouseOverMinus) startx += 17;
          if(uDrawFlags == HTSCROLL_ZOOMOUT) startx = 116+17+17;

          LICE_ScaledBlit(&tmpbmp, bmp, 0, 0, w, h, startx, starty, 17, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          BitBlt(hdc, ctrl.right+zbs+ZOOMBUTTON_RESIZER_SIZE(zbs), ctrl.top, w, h, tmpbmp.getDC(), 0, 0, SRCCOPY);
        }
        else
        {
          HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
          HPEN pen=CreatePen(PS_SOLID, 0, CoolSB_GetSysColor(hwnd,COLOR_3DDKSHADOW));
          HGDIOBJ oldPen=SelectObject(hdc,pen);
          // +
          {
            int pressed = (uDrawFlags == HTSCROLL_ZOOMIN);
            RECT r = {ctrl.right+pressed, ctrl.top+pressed, ctrl.right + zbs, ctrl.bottom};
            ownDrawEdge(hwnd,hdc, &r, pressed?0:EDGE_RAISED, BF_RECT | BF_ADJUST);
            FillRect(hdc, &r, br);


            int cy=(ctrl.top+ctrl.bottom)/2+pressed,
                    cx=ctrl.right+zbs/2+pressed;
            int sz=wdl_min(14,ctrl.bottom-ctrl.top)/4;
            
            MoveToEx(hdc,cx-sz,cy,NULL);
            LineTo(hdc,cx+sz+1,cy);            
            MoveToEx(hdc,cx,cy-sz,NULL);
            LineTo(hdc,cx,cy+sz+1);
          }
          // resize thumb
          {
            RECT r = {ctrl.right + zbs, ctrl.top, ctrl.right + zbs + ZOOMBUTTON_RESIZER_SIZE(zbs), ctrl.bottom};
            ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
            FillRect(hdc, &r, br);
          }
          // -
          {
            int pressed = (uDrawFlags == HTSCROLL_ZOOMOUT);
            RECT r = {ctrl.right + zbs + ZOOMBUTTON_RESIZER_SIZE(zbs) +pressed, ctrl.top+pressed, 
              ctrl.right + zbs*2 + ZOOMBUTTON_RESIZER_SIZE(zbs), ctrl.bottom};
            ownDrawEdge(hwnd,hdc, &r, pressed?0:EDGE_RAISED, BF_RECT | BF_ADJUST);
            FillRect(hdc, &r, br);
            int cy=(ctrl.top+ctrl.bottom)/2+pressed,
                cx=ctrl.right+zbs+ZOOMBUTTON_RESIZER_SIZE(zbs)+zbs/2+pressed;
            int sz=wdl_min(14,ctrl.bottom-ctrl.top)/4;
            
            MoveToEx(hdc,cx-sz,cy,NULL);
            LineTo(hdc,cx+sz+1,cy);            
          }
          SelectObject(hdc,oldPen);
          DeleteObject(pen);
          DeleteObject(br);
        }
      }
      else
      {
        int zbs = GetZoomButtonSize(TRUE);
        if(theme->bmp && *theme->bmp)
        {
          LICE_IBitmap *bmp = *theme->bmp;
          static LICE_SysBitmap tmpbmp;
          int w = ctrl.right - ctrl.left;
          int h = zbs;
          int startx = 116;
          int starty = 201;
          if(fMouseOverPlus) startx += 17;
          if(uDrawFlags == HTSCROLL_ZOOMIN) startx = 116+17+17;
          if (w>tmpbmp.getWidth() || h>tmpbmp.getHeight())
            tmpbmp.resize(wdl_max(w,tmpbmp.getWidth()), wdl_max(h,tmpbmp.getHeight()));
          LICE_ScaledBlit(&tmpbmp, bmp, 0, 0, w, h, startx, starty, 17, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          BitBlt(hdc, ctrl.left, ctrl.bottom, w, h, tmpbmp.getDC(), 0, 0, SRCCOPY);

          LICE_ScaledBlit(&tmpbmp, bmp, 0, 0, w, ZOOMBUTTON_RESIZER_SIZE(zbs), 143, 114, 17, 4, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          BitBlt(hdc, ctrl.left, ctrl.bottom+zbs, w, ZOOMBUTTON_RESIZER_SIZE(zbs), tmpbmp.getDC(), 0, 0, SRCCOPY);

          startx = 116;
          starty = 221;
          if(fMouseOverMinus) startx += 17;
          if(uDrawFlags == HTSCROLL_ZOOMOUT) startx = 116+17+17;

          LICE_ScaledBlit(&tmpbmp, bmp, 0, 0, w, h, startx, starty, 17, 17, 1.0f, LICE_BLIT_FILTER_BILINEAR);
          BitBlt(hdc, ctrl.left, ctrl.bottom+zbs+ZOOMBUTTON_RESIZER_SIZE(zbs), w, h, tmpbmp.getDC(), 0, 0, SRCCOPY);
        }
        else
        {
          HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
          HPEN pen=CreatePen(PS_SOLID, 0, CoolSB_GetSysColor(hwnd,COLOR_3DDKSHADOW));
          HGDIOBJ oldPen=SelectObject(hdc,pen);
          // +
          {
            int pressed = (uDrawFlags == HTSCROLL_ZOOMIN);
            RECT r = {ctrl.left+pressed, ctrl.bottom+pressed, ctrl.right, ctrl.bottom + zbs};
            ownDrawEdge(hwnd,hdc, &r, pressed?0:EDGE_RAISED, BF_RECT | BF_ADJUST);
            FillRect(hdc, &r, br);

            int cx=(ctrl.left+ctrl.right)/2+pressed,cy=ctrl.bottom+zbs/2+pressed;
            int sz=wdl_min(14,ctrl.right-ctrl.left)/4;
            
            MoveToEx(hdc,cx-sz,cy,NULL);
            LineTo(hdc,cx+sz+1,cy);            
            MoveToEx(hdc,cx,cy-sz,NULL);
            LineTo(hdc,cx,cy+sz+1);
          }
          // resize thumb
          {
            RECT r = {ctrl.left, ctrl.bottom + zbs, ctrl.right, ctrl.bottom + zbs + ZOOMBUTTON_RESIZER_SIZE(zbs)};
            ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
            FillRect(hdc, &r, br);
          }
          // -
          {
            int pressed = (uDrawFlags == HTSCROLL_ZOOMOUT);
            RECT r = {ctrl.left+pressed, ctrl.bottom + zbs  + ZOOMBUTTON_RESIZER_SIZE(zbs) + pressed, 
                      ctrl.right, ctrl.bottom + ZOOMBUTTON_RESIZER_SIZE(zbs) + zbs*2};
            ownDrawEdge(hwnd,hdc, &r, pressed?0:EDGE_RAISED, BF_RECT | BF_ADJUST);
            FillRect(hdc, &r, br);

            int cx=(ctrl.left+ctrl.right)/2+pressed,cy=ctrl.bottom+zbs+ZOOMBUTTON_RESIZER_SIZE(zbs)+zbs/2+pressed;
            int sz=wdl_min(14,ctrl.right-ctrl.left)/4;
            
            MoveToEx(hdc,cx-sz,cy,NULL);
            LineTo(hdc,cx+sz+1,cy);
          }
          SelectObject(hdc,oldPen);
          DeleteObject(pen);
          DeleteObject(br);
        }
      }
    }
    }
		RotateRect0(sb, &ctrl);
	}
	//not enough room for the scrollbar, so just draw the buttons (scaled in size to fit)
	else
	{
		butwidth = scrollwidth / 2;

		//LEFT ARROW
		SetRect(&ctrl, rect->left, rect->top, rect->left + butwidth, rect->bottom);

		RotateRect0(sb, &ctrl);
		DrawScrollArrow(hwnd,sb, hdc, &ctrl, uLeftButFlags, fMouseDownL, fMouseOverL,theme);
		RotateRect0(sb, &ctrl);

		//RIGHT ARROW
		OffsetRect(&ctrl, scrollwidth - butwidth, 0);
		
		RotateRect0(sb, &ctrl);
		DrawScrollArrow(hwnd,sb, hdc, &ctrl, uRightButFlags, fMouseDownR, fMouseOverR,theme);		
		RotateRect0(sb, &ctrl);

		//if there is a gap between the buttons, fill it with a solid color
		//if(butwidth & 0x0001)
		if(ctrl.left != rect->left + butwidth)
		{
			ctrl.left --;
			ctrl.right -= butwidth;
			RotateRect0(sb, &ctrl);
			
  		DrawCheckedRect(theme,NULL,hdc, &ctrl, crCheck1, crCheck2, sb, rect, 0, sbXoffs, sbYoffs);

			RotateRect0(sb, &ctrl);
		}
			
	}

	return FALSE;
}

//
//	Draw a vertical scrollbar using the horizontal draw routine, but
//	with the coordinates adjusted accordingly
//
static LRESULT NCDrawVScrollbar(SCROLLBAR *sb, HWND hwnd, HDC hdc, const RECT *rect, UINT uDrawFlags, BOOL hasZoomButtons, const wdlscrollbar_themestate *theme)
{
	LRESULT ret;
	RECT rc;

	rc = *rect;
	RotateRect(&rc);
	ret = NCDrawHScrollbar(sb, hwnd, hdc, &rc, uDrawFlags,hasZoomButtons,theme);
	RotateRect(&rc);
	
	return ret;
}

//
//	Generic wrapper function for the scrollbar drawing
//
static LRESULT NCDrawScrollbar(SCROLLBAR *sb, HWND hwnd, HDC hdc, const RECT *rect, UINT uDrawFlags, BOOL hasZoomButtons, const wdlscrollbar_themestate *theme)
{
	if(sb->nBarType == SB_HORZ)
		return NCDrawHScrollbar(sb, hwnd, hdc, rect, uDrawFlags,hasZoomButtons,theme);
	else
		return NCDrawVScrollbar(sb, hwnd, hdc, rect, uDrawFlags,hasZoomButtons,theme);
}



static int getPink(const wdlscrollbar_themestate *ts, int x, int y, int vert, int np=0, int add=1)
{
  LICE_IBitmap *bmp = ts->bmp ? *ts->bmp : NULL;
  if(!bmp) return 0;

  const int w = bmp->getWidth();
  const int h = bmp->getHeight();
  const int rs=bmp->getRowSpan();
  const LICE_pixel *p = bmp->getBits();
  if(x < 0 || y < 0 || x>=w || y>=h) return 0;
  p += rs * y + x;
  if(!vert)
  {
    int l;
    for(l=0;;l++)
    {
      if(np && *p==LICE_RGBA(255,0,255,255)) break;
      if(!np && *p!=LICE_RGBA(255,0,255,255)) break;
      p += add;
      x += add;
      if(x>=w || x<0) break;
    }
    return l;
  }

  int l;
  for(l=0;;l++)
  {
    if(np && *p==LICE_RGBA(255,0,255,255)) break;
    if(!np && *p!=LICE_RGBA(255,0,255,255)) break;
    p += rs*add;
    y += add;
    if(y>=h || y<0) break;
  }
  return l;
}

static void initLiceBmp(wdlscrollbar_themestate *ts, LICE_IBitmap **bmpIn)
{
  ts->bmp = bmpIn;
  if(!bmpIn) return;

  ts->hasPink = getPink(ts,0,35,0)>0;
  if(ts->hasPink)
  {
//    LICE_IBitmap *bmp = *ts->bmp;
    memset(&ts->thumbHV, 0, sizeof(ts->thumbHV));
    memset(&ts->thumbVV, 0, sizeof(ts->thumbVV));
//    int w = bmp->getWidth();
 //   int h = bmp->getHeight();
    {
      int l = getPink(ts,0,89,0);
      ts->thumbHV[0] = l;
      int x = l;
      l = getPink(ts,x, 89, 0, 1);
      ts->thumbHV[1] = l;
      x += l;
      l = getPink(ts,x, 89, 0);
      ts->thumbHV[2] = l;
      x += l;
      l = getPink(ts,x, 89, 0, 1);
      ts->thumbHV[3] = l;
      x += l;
      l = getPink(ts,x, 89, 0);
      ts->thumbHV[4] = l;

      int y = 91;
      l = getPink(ts,52, y, 1);
      ts->thumbVV[0] = l;
      y += l;
      l = getPink(ts,52, y, 1, 1);
      ts->thumbVV[1] = l;
      y += l;
      l = getPink(ts,52, y, 1);
      ts->thumbVV[2] = l;
      y += l;
      l = getPink(ts,52, y, 1, 1);
      ts->thumbVV[3] = l;
      y += l;
      l = getPink(ts,52, y, 1);
      ts->thumbVV[4] = l;

      ts->bkghl = getPink(ts,0, 35, 0);
      ts->bkghr = getPink(ts,203, 35, 0, 0, -1);

      ts->bkgvt = getPink(ts,168, 37, 1);
      ts->bkgvb = getPink(ts,168, 237, 1, 0, -1);
    }
  }
}

static LRESULT NCPaint(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam, HDC hdcParam=NULL)
{
	SCROLLBAR *sb;
	HDC hdc;
	HRGN hrgn;
	RECT winrect, rect;
//	BOOL fUpdateAll = (wParam == 1);
	UINT ret;

  wdlscrollbar_themestate *theme = GetThemeForScrollWnd(sw);
  if(!theme->bmp)
  {
    char tmp[512];
    if (!sw->whichTheme) strcpy(tmp,"scrollbar");
    else wsprintf(tmp,"scrollbar_%d",sw->whichTheme+1);
    initLiceBmp(theme,(LICE_IBitmap **)GetIconThemePointer(tmp));
  }
  
	GET_WINDOW_RECT(hwnd, &winrect);
	
	//if entire region needs painting, then make a region to cover the entire window
/*	if(fUpdateAll)
		hrgn = (HRGN)wParam;
	else
  */
  hrgn = (HRGN)wParam;
	
	//hdc = GetWindowDC(hwnd);
  if(hdcParam != NULL) 
    hdc = hdcParam;
  else
	  hdc = GetWindowDC(hwnd);

//  printf("wndrect: %d,%d,%d,%d hdc=%d hv=%d\n",winrect.left,winrect.top,winrect.right,winrect.bottom,hdc,sw->sbarHorz.fScrollVisible);

	//
	//	Only draw the horizontal scrollbar if the window is tall enough
	//
	sb = &sw->sbarHorz;
	if(sb->fScrollVisible)
	{
		//get the screen coordinates of the whole horizontal scrollbar area
    BOOL hasZoomButtons;
		GetHScrollRect(sw, hwnd, &rect, &hasZoomButtons);

		//make the coordinates relative to the window for drawing
		OffsetRect(&rect, -winrect.left, -winrect.top);


		if(sw->uCurrentScrollbar == SB_HORZ)
			NCDrawHScrollbar(sb, hwnd, hdc, &rect, sw->uScrollTimerPortion,hasZoomButtons,theme);
		else
			NCDrawHScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_NONE,hasZoomButtons,theme);
	}

	//
	// Only draw the vertical scrollbar if the window is wide enough to accomodate it
	//
	sb = &sw->sbarVert;
	if(sb->fScrollVisible)
	{
		//get the screen cooridinates of the whole horizontal scrollbar area
    BOOL hasZoomButtons;
		GetVScrollRect(sw, hwnd, &rect,&hasZoomButtons);

		//make the coordinates relative to the window for drawing
		OffsetRect(&rect, -winrect.left, -winrect.top);


		if(sw->uCurrentScrollbar == SB_VERT)
    {
			NCDrawVScrollbar(sb, hwnd, hdc, &rect, sw->uScrollTimerPortion,hasZoomButtons,theme);
		}
    else
			NCDrawVScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_NONE,hasZoomButtons,theme);
	}

	//Call the default window procedure for WM_NCPAINT, with the
	//new window region. ** region must be in SCREEN coordinates **

    // If the window has WS_(H-V)SCROLL bits set, we should reset them
    // to avoid windows taking the scrollbars into account.
    // We temporarily set a flag preventing the subsecuent 
    // WM_STYLECHANGING/WM_STYLECHANGED to be forwarded to 
    // the original window procedure
	
	ret = CallWindowProcStyleMod(sw, hwnd, WM_NCPAINT, (WPARAM)hrgn, lParam);

	


	// DRAW THE DEAD AREA
	// only do this if the horizontal and vertical bars are visible
  if (sw->sbarVert.fScrollVisible && (sw->vscrollbarShrinkTop || sw->vscrollbarShrinkBottom || sw->sbarHorz.fScrollVisible))
  {
    int col=CoolSB_GetSysColor(hwnd,COLOR_3DFACE);
		GET_WINDOW_RECT(hwnd, &rect);
		OffsetRect(&rect, -winrect.left, -winrect.top);

		if(sw->fLeftScrollbar)
		{
			rect.left += sw->cxLeftEdge;
			rect.right = rect.left + GetScrollMetric(TRUE, SM_CXVERTSB);
		}
		else
		{
			rect.right -= sw->cxRightEdge;
			rect.left = rect.right  - GetScrollMetric(TRUE, SM_CXVERTSB);
		}

    if (sw->vscrollbarShrinkTop)
    {
      RECT r2=rect;
      r2.top += sw->cyTopEdge;
	  	r2.bottom = r2.top + sw->vscrollbarShrinkTop;

      LICE_IBitmap *bm=NULL;
      if (sw->getDeadAreaBitmap) bm = (LICE_IBitmap *)sw->getDeadAreaBitmap(1,hwnd,&r2,col);
      
      if (bm)
      {
        BitBlt(hdc,r2.left,r2.top,r2.right-r2.left,r2.bottom-r2.top,bm->getDC(),0,0,SRCCOPY);
      }
      else
      {
  		  PaintRect(hdc, &r2, col);
      }
    }

    if (sw->sbarHorz.fScrollVisible||sw->vscrollbarShrinkBottom)
    {
		  rect.bottom -= sw->cyBottomEdge;
		  rect.top  = rect.bottom - GetScrollMetric(FALSE, SM_CYHORZSB) - sw->vscrollbarShrinkBottom;
      LICE_IBitmap *bm=NULL;
      if (sw->getDeadAreaBitmap) bm = (LICE_IBitmap *)sw->getDeadAreaBitmap(0,hwnd,&rect,col);

      if (bm)
      {
        BitBlt(hdc,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,bm->getDC(),0,0,SRCCOPY);
      }
      else
      {
    		PaintRect(hdc, &rect, col);
      }
    }
  }

//	UNREFERENCED_PARAMETER(clip);

  if(!hdcParam) ReleaseDC(hwnd, hdc);
	return ret;
}

//
// Need to detect if we have clicked in the scrollbar region or not
//
static LRESULT NCHitTest(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  RECT hrect;
  RECT vrect;
  POINT pt;

  pt.x = GET_X_LPARAM(lParam); 
  pt.y = GET_Y_LPARAM(lParam);
  OSX_REMAP_SCREENY(hwnd,&pt.y);
	
	//work out exactly where the Horizontal and Vertical scrollbars are
  BOOL hasZoomButtons;
	GetHScrollRect(sw, hwnd, &hrect, &hasZoomButtons);
  if (hasZoomButtons && sw->resizingHthumb) 
  {
    int zbs = GetZoomButtonSize(FALSE);
    hrect.right += zbs*2+ZOOMBUTTON_RESIZER_SIZE(zbs);
  }
	GetVScrollRect(sw, hwnd, &vrect,&hasZoomButtons);
  if (hasZoomButtons && sw->resizingHthumb) 
  {
    int zbs = GetZoomButtonSize(TRUE);
    vrect.bottom += zbs*2+ZOOMBUTTON_RESIZER_SIZE(zbs);
  }
	  
    
	//Clicked in the horizontal scrollbar area
	if(sw->sbarHorz.fScrollVisible && PtInRect(&hrect, pt))
	{
		return HTHSCROLL;
	}
	//Clicked in the vertical scrollbar area
	else if(sw->sbarVert.fScrollVisible && PtInRect(&vrect, pt))
	{

		return HTVSCROLL;
	}
	//clicked somewhere else
	else
	{
		return CallWindowProc(sw->oldproc, hwnd, WM_NCHITTEST, wParam, lParam);
	}
}

//
//	Return a HT* value indicating what part of the scrollbar was clicked
//	Rectangle is not adjusted
//
static UINT GetHorzPortion(SCROLLBAR *sb, HWND hwnd, RECT *rect, int x, int y, BOOL hasZoomButtons)
{
	RECT rc = *rect;

	if(y < rc.top || y >= rc.bottom) return HTSCROLL_NONE;


	//Now we have the rectangle for the scrollbar itself, so work out
	//what part we clicked on.
	return GetHorzScrollPortion(sb, hwnd, &rc, x, y,hasZoomButtons);
}

//
//	Just call the horizontal version, with adjusted coordinates
//
static UINT GetVertPortion(SCROLLBAR *sb, HWND hwnd, RECT *rect, int x, int y, BOOL hasZoomButtons)
{
	UINT ret;
	RotateRect(rect);
	ret = GetHorzPortion(sb, hwnd, rect, y, x,hasZoomButtons);
	RotateRect(rect);
	return ret;
}

//
//	Wrapper function for GetHorzPortion and GetVertPortion
//
static UINT GetPortion(SCROLLBAR *sb, HWND hwnd, RECT *rect, int x, int y, BOOL hasZoomButtons)
{
	if(sb->nBarType == SB_HORZ)
		return GetHorzPortion(sb, hwnd, rect, x, y,hasZoomButtons);
	else if(sb->nBarType == SB_VERT)
		return GetVertPortion(sb, hwnd, rect, x, y,hasZoomButtons);
	else
		return HTSCROLL_NONE;
}


//
//	Left button click in the non-client area
//
static LRESULT NCLButtonDown(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam, BOOL isDouble)
{
	RECT rect, winrect;
	HDC hdc;
	SCROLLBAR *sb;
	POINT pt;

	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);
  OSX_REMAP_SCREENY(hwnd,&pt.y);

	hwndCurCoolSB = hwnd;

	//
	//	HORIZONTAL SCROLLBAR PROCESSING
	//
  BOOL hasZoomButtons;
	if(wParam == HTHSCROLL)
	{
		sw->uScrollTimerMsg = WM_HSCROLL;
		sw->uCurrentScrollbar = SB_HORZ;
		sb = &sw->sbarHorz;

		//get the total area of the normal Horz scrollbar area
		GetHScrollRect(sw, hwnd, &rect,&hasZoomButtons);
		sw->uCurrentScrollPortion = GetHorzPortion(sb, hwnd, &rect, pt.x,pt.y,hasZoomButtons);
	}
	//
	//	VERTICAL SCROLLBAR PROCESSING
	//
	else if(wParam == HTVSCROLL)
	{
		sw->uScrollTimerMsg = WM_VSCROLL;
		sw->uCurrentScrollbar = SB_VERT;
		sb = &sw->sbarVert;

		//get the total area of the normal Horz scrollbar area
		GetVScrollRect(sw, hwnd, &rect,&hasZoomButtons);
		sw->uCurrentScrollPortion = GetVertPortion(sb, hwnd, &rect, pt.x,pt.y,hasZoomButtons);
	}
	//
	//	NORMAL PROCESSING
	//
	else
	{
		sw->uCurrentScrollPortion = HTSCROLL_NONE;
		return CallWindowProc(sw->oldproc, hwnd, WM_NCLBUTTONDOWN, wParam, lParam);
	}

	//
	// we can now share the same code for vertical
	// and horizontal scrollbars
	//
  const wdlscrollbar_themestate *theme = GetThemeForScrollWnd(sw);

	switch(sw->uCurrentScrollPortion)
	{
	//inserted buttons to the left/right

	case HTSCROLL_THUMB: 

		//if the scrollbar is disabled, then do no further processing
		if(!IsScrollbarActive(sb))
			return 0;
		
    if (isDouble)
    {
      SendMessage(hwnd,WM_SB_DBLCLK,wParam == HTVSCROLL ? SB_VERT : SB_HORZ,0);
    }
    else
    {

		  RotateRect0(sb, &rect);
		  CalcThumbSize(sb, &rect, &g_nThumbSize, &g_nThumbPos);
		  RotateRect0(sb, &rect);
		  
		  //remember the bounding rectangle of the scrollbar work area
		  rcThumbBounds = rect;
		  
		  sw->fThumbTracking = TRUE;
		  sb->scrollInfo.nTrackPos = sb->scrollInfo.nPos;
		  
		  if(wParam == HTVSCROLL) 
			  nThumbMouseOffset = pt.y - g_nThumbPos;
		  else
			  nThumbMouseOffset = pt.x - g_nThumbPos;

		  nLastPos = sb->scrollInfo.nPos;
		  nThumbPos0 = g_nThumbPos;
	  
#if 0
		  //if(sb->fFlatScrollbar)
		  //{
			  GetWindowRect(hwnd, &winrect);
        FIXWINDOWRECT(&winrect);
			  OffsetRect(&rect, -winrect.left, -winrect.top);
			  hdc = GetWindowDC(hwnd);
			  NCDrawScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_THUMB);
			  ReleaseDC(hwnd, hdc);
		  //}
#endif

        MouseMove(sw, hwnd, 0, 0);
    }

		break;

		//Any part of the scrollbar
	case HTSCROLL_LEFT:  
		if(sb->fScrollFlags & ESB_DISABLE_LEFT)		return 0;
		else										goto target1;
	
	case HTSCROLL_RIGHT: 
		if(sb->fScrollFlags & ESB_DISABLE_RIGHT)	return 0;
		else										goto target1;

		goto target1;	

	case HTSCROLL_PAGELEFT:  case HTSCROLL_PAGERIGHT:

		target1:

		//if the scrollbar is disabled, then do no further processing
		if(!IsScrollbarActive(sb))
			break;

		//ajust the horizontal rectangle to NOT include
		//any inserted buttons

		SendScrollMessage(hwnd, sw->uScrollTimerMsg, sw->uCurrentScrollPortion, 0);

		// Check what area the mouse is now over :
		// If the scroll thumb has moved under the mouse in response to 
		// a call to SetScrollPos etc, then we don't hilight the scrollbar margin
		if(sw->uCurrentScrollbar == SB_HORZ)
			sw->uScrollTimerPortion = GetHorzScrollPortion(sb, hwnd, &rect, pt.x, pt.y,hasZoomButtons);
		else
			sw->uScrollTimerPortion = GetVertScrollPortion(sb, hwnd, &rect, pt.x, pt.y,hasZoomButtons);

		GET_WINDOW_RECT(hwnd, &winrect);
		OffsetRect(&rect, -winrect.left, -winrect.top);
		hdc = GetWindowDC(hwnd);
			
		NCDrawScrollbar(sb, hwnd, hdc, &rect, sw->uScrollTimerPortion,hasZoomButtons,theme);
		ReleaseDC(hwnd, hdc);

		//Post the scroll message!!!!
		sw->uScrollTimerPortion = sw->uCurrentScrollPortion;

		//set a timer going on the first click.
		//if this one expires, then we can start off a more regular timer
		//to generate the auto-scroll behaviour
		sw->uScrollTimerId = SetTimer(hwnd, COOLSB_TIMERID1, COOLSB_TIMERINTERVAL1, 0);
		break;
  case HTSCROLL_LRESIZER:
  case HTSCROLL_RRESIZER:
		if(wParam == HTVSCROLL) 
			nThumbMouseOffset = pt.y - g_nThumbPos;
		else
			nThumbMouseOffset = pt.x;
    if(wParam == HTHSCROLL)
    {
      int nThumbSize, nThumbPos;
      GetHScrollRect(sw, hwnd, &rect,NULL);
      CalcThumbSize(sb, &rect, &nThumbSize, &nThumbPos);
      SendMessage(hwnd, WM_SB_TRESIZE_START, nThumbSize, nThumbPos);
    }
    else
    {
      int nThumbSize, nThumbPos;
      GetVScrollRect(sw, hwnd, &rect,NULL);
      RotateRect0(sb, &rect);
      CalcThumbSize(sb, &rect, &nThumbSize, &nThumbPos);
      SendMessage(hwnd, WM_SB_TRESIZE_START, nThumbSize, nThumbPos);
    }
    break;
  case HTSCROLL_RESIZER:
		if(wParam == HTVSCROLL) 
			nThumbMouseOffset = pt.y;
		else
			nThumbMouseOffset = pt.x;
    break;
  case HTSCROLL_ZOOMIN:
    SendMessage(hwnd, WM_SB_ZOOM, 0, (wParam == HTVSCROLL));
    if (sw->uZoomTimerId) KillTimer(hwnd,sw->uZoomTimerId);
    sw->uZoomTimerId=SetTimer(hwnd,COOLSB_TIMERID4,COOLSB_TIMERINTERVAL4,NULL);
    sw->uZoomTimerMode=wParam == HTVSCROLL;
    sw->uScrollTimerPortion = HTSCROLL_ZOOMIN;
    {
      GET_WINDOW_RECT(hwnd, &winrect);
      OffsetRect(&rect, -winrect.left, -winrect.top);
      hdc = GetWindowDC(hwnd);
      NCDrawScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_ZOOMIN,hasZoomButtons,theme);
      ReleaseDC(hwnd, hdc);
    }
    break;
  case HTSCROLL_ZOOMOUT:
    SendMessage(hwnd, WM_SB_ZOOM, 1, (wParam == HTVSCROLL));
    if (sw->uZoomTimerId) KillTimer(hwnd,sw->uZoomTimerId);
    sw->uZoomTimerId=SetTimer(hwnd,COOLSB_TIMERID4,COOLSB_TIMERINTERVAL4,NULL);
    sw->uZoomTimerMode=2 + (wParam == HTVSCROLL);
    sw->uScrollTimerPortion = HTSCROLL_ZOOMOUT;
    {
      GET_WINDOW_RECT(hwnd, &winrect);
      OffsetRect(&rect, -winrect.left, -winrect.top);
      hdc = GetWindowDC(hwnd);
      NCDrawScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_ZOOMOUT,hasZoomButtons,theme);
      ReleaseDC(hwnd, hdc);
    }
    break;
	default:
		return CallWindowProc(sw->oldproc, hwnd, WM_NCLBUTTONDOWN, wParam, lParam);
		//return 0;
	}
		
	SetCapture(hwnd);
#ifndef _WIN32
        sw->uLastHitTestPortion = sw->uHitTestPortion     = HTSCROLL_NONE;
#endif
	return 0;
}

//
//	Left button released
//
static LRESULT LButtonUp(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	RECT rect={0,};
	//UINT thisportion;
	HDC hdc;
	RECT winrect;

  if (sw->uZoomTimerId) KillTimer(hwnd,sw->uZoomTimerId);
  sw->uZoomTimerId=0;

	//current scrollportion is the button that we clicked down on
	if(sw->uCurrentScrollPortion != HTSCROLL_NONE)
	{
		SCROLLBAR *sb = &sw->sbarHorz;
		lParam = GetMessagePos();
		ReleaseCapture();
#ifndef _WIN32
        SetCursor(LoadCursor(NULL,IDC_ARROW)); // OS X seems to like this
#endif
		GET_WINDOW_RECT(hwnd, &winrect);
    BOOL hasZoomButtons=0;


		//emulate the mouse input on a scrollbar here...
		if(sw->uCurrentScrollbar == SB_HORZ)
		{
			//get the total area of the normal Horz scrollbar area
			sb = &sw->sbarHorz;
			GetHScrollRect(sw, hwnd, &rect,&hasZoomButtons);
		}
		else if(sw->uCurrentScrollbar == SB_VERT)
		{
			//get the total area of the normal Horz scrollbar area
			sb = &sw->sbarVert;
			GetVScrollRect(sw, hwnd, &rect,&hasZoomButtons);
		}

		//we need to do different things depending on if the
		//user is activating the scrollbar itself, or one of
		//the inserted buttons
		switch(sw->uCurrentScrollPortion)
		{

		//The scrollbar is active
		case HTSCROLL_LEFT:  case HTSCROLL_RIGHT: 
		case HTSCROLL_PAGELEFT:  case HTSCROLL_PAGERIGHT: 
		case HTSCROLL_NONE:
	  case HTSCROLL_ZOOMIN:
	  case HTSCROLL_ZOOMOUT:
			
			KillTimer(hwnd, sw->uScrollTimerId);

		case HTSCROLL_THUMB: 

			//In case we were thumb tracking, make sure we stop NOW
			if(sw->fThumbTracking == TRUE)
			{
				SendScrollMessage(hwnd, sw->uScrollTimerMsg, SB_THUMBPOSITION, nLastPos);
				sw->fThumbTracking = FALSE;
			}

			//send the SB_ENDSCROLL message now that scrolling has finished
			SendScrollMessage(hwnd, sw->uScrollTimerMsg, SB_ENDSCROLL, 0);

			//adjust the total scroll area to become where the scrollbar
			//really is (take into account the inserted buttons)
			OffsetRect(&rect, -winrect.left, -winrect.top);
			hdc = GetWindowDC(hwnd);
			
			//draw whichever scrollbar sb is
			NCDrawScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_NORMAL,hasZoomButtons,GetThemeForScrollWnd(sw));

			ReleaseDC(hwnd, hdc);
			break;
		}

		//reset our state to default
		sw->uCurrentScrollPortion = HTSCROLL_NONE;
		sw->uScrollTimerPortion	  = HTSCROLL_NONE;
		sw->uScrollTimerId		  = 0;

		sw->uScrollTimerMsg       = 0;
		sw->uCurrentScrollbar     = COOLSB_NONE;

		return 0;
	}
	else
	{
		/*
		// Can't remember why I did this!
		if(GetCapture() == hwnd)
		{
			ReleaseCapture();
		}*/
	}

	return CallWindowProc(sw->oldproc, hwnd, WM_LBUTTONUP, wParam, lParam);
}

//
//	This function is called whenever the mouse is moved and 
//  we are dragging the scrollbar thumb about.
//
static LRESULT ThumbTrackHorz(SCROLLBAR *sbar, HWND hwnd, int x, int y, const wdlscrollbar_themestate *theme)
{
	POINT pt;
	RECT rc, winrect, rc2;
	COLORREF crCheck1 = GetSBForeColor(hwnd);
	COLORREF crCheck2 = GetSBBackColor(hwnd);
	HDC hdc;
	int thumbpos = g_nThumbPos;
	int pos;
	int siMaxMin = 0;
  SCROLLWND *sw = GetScrollWndFromHwnd(hwnd);

	SCROLLINFO *si;
	si = &sbar->scrollInfo;

	pt.x = x;
	pt.y = y;

  int sbYoffs=-(int)sw->vscrollbarShrinkTop,sbXoffs=0;
#ifdef _WIN32
  // this is a stupid fix for now . this needs a ton of overhauling
  {
    RECT r;
    POINT p={0,0};
    ClientToScreen(hwnd,&p);
    GetWindowRect(hwnd,&r);

    if (sbar == &sw->sbarVert) sbYoffs =  r.top - p.y - sw->vscrollbarShrinkTop;
    if (sbar == &sw->sbarHorz) sbXoffs =  r.left - p.x;
  }
#endif

	//draw the thumb at whatever position
	rc = rcThumbBounds;

#define THUMBTRACK_SNAPDIST 24

	SetRect(&rc2, rc.left -  THUMBTRACK_SNAPDIST*2, rc.top -    THUMBTRACK_SNAPDIST, 
				  rc.right + THUMBTRACK_SNAPDIST*2, rc.bottom + THUMBTRACK_SNAPDIST);

  int adj = GetScrollMetric(sbar->nBarType == SB_VERT, SM_CXHORZSB);
	rc.left += adj;
	rc.right -= adj;

	//keep the thumb within the scrollbar limits
	thumbpos = pt.x - nThumbMouseOffset;
	if(thumbpos < rc.left) thumbpos = rc.left;
	if(thumbpos > rc.right - g_nThumbSize) thumbpos = rc.right - g_nThumbSize;

	GET_WINDOW_RECT(hwnd, &winrect);

	if(sbar->nBarType == SB_VERT)
		RotateRect(&winrect);
	
	hdc = GetWindowDC(hwnd);
		

	OffsetRect(&rc, -winrect.left, -winrect.top);
	thumbpos -= winrect.left;

	//draw the margin before the thumb
	SetRect(&rc2, rc.left, rc.top, thumbpos, rc.bottom);
	RotateRect0(sbar, &rc2);

	DrawCheckedRect(theme,NULL,hdc, &rc2, crCheck1, crCheck2, sbar, &rcThumbBounds, 0,sbXoffs, sbYoffs );
	
	RotateRect0(sbar, &rc2);

	//draw the margin after the thumb 
	SetRect(&rc2, thumbpos+g_nThumbSize, rc.top, rc.right, rc.bottom);
	
	RotateRect0(sbar, &rc2);
	
	DrawCheckedRect(theme,NULL,hdc, &rc2, crCheck1, crCheck2, sbar, &rcThumbBounds, 0, sbXoffs, sbYoffs );
  
	RotateRect0(sbar, &rc2);
	
	//finally draw the thumb itelf. This is how it looks on win2000, anyway
	SetRect(&rc2, thumbpos, rc.top, thumbpos+g_nThumbSize, rc.bottom);
	
	RotateRect0(sbar, &rc2);

  if(theme->bmp && *theme->bmp)
  {
    drawSkinThumb(hdc, rc2, 0, 1, sbar->nBarType == SB_VERT, &rcThumbBounds, sbar,sw,theme);
  }
  else
  {
    // no skinning
    {
      RECT r = rc2;
      if(sw->resizingHthumb)
      {
        if(sbar->nBarType == SB_HORZ)
        {
          r.left += m_thumbsize;
          r.right -= m_thumbsize;
        }
        else
        {
          //disabled for now
          /*r.top += m_thumbsize;
          r.bottom -= m_thumbsize;*/
        }
      }
      DrawBlankButton(hwnd,hdc, &r);
    }
    
    if(sw->resizingHthumb)
    {
      //draw left and right resizers
      if(sbar->nBarType == SB_HORZ)
      {
        RECT thumb = rc2;
        HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
        {
          RECT r={thumb.left, thumb.top, thumb.left+m_thumbsize, thumb.bottom};
          ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
          FillRect(hdc, &r, br);
        }
        {
          RECT r={thumb.right-m_thumbsize, thumb.top, thumb.right, thumb.bottom};
          ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
          FillRect(hdc, &r, br);
        }
        DeleteObject(br);
      }
      else
      {
        //disabled for now
        /*RECT thumb = rc2;
        HBRUSH br = CreateSolidBrush(CoolSB_GetSysColor(hwnd,COLOR_BTNFACE));
        {
        RECT r={thumb.left, thumb.top, thumb.right, thumb.top+m_thumbsize};
        ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
        FillRect(hdc, &r, br);
        }
        {
        RECT r={thumb.left, thumb.bottom - m_thumbsize, thumb.right, thumb.bottom};
        ownDrawEdge(hwnd,hdc, &r, EDGE_RAISED, BF_RECT | BF_ADJUST);
        FillRect(hdc, &r, br);
        }
        DeleteObject(br);*/
      }
    }
  }

	RotateRect0(sbar, &rc2);
	ReleaseDC(hwnd, hdc);

	//post a SB_TRACKPOS message!!!
	siMaxMin = si->nMax - si->nMin;

	if(siMaxMin > 0)
  {
		pos = MulDiv(thumbpos-rc.left, siMaxMin-si->nPage + 1, rc.right-rc.left-g_nThumbSize);
    /*this +1 should probably not be here, todo someday remove, allows thumb tracking messages to exceed expected bounds*/
  }
	else
		pos = thumbpos - rc.left;

	if(pos != nLastPos)
	{
		si->nTrackPos = pos;	
		SendScrollMessage(hwnd, sw->uScrollTimerMsg, SB_THUMBTRACK, pos);
	}

	nLastPos = pos;

	
	return 0;
}

//
//	remember to rotate the thumb bounds rectangle!!
//
static LRESULT ThumbTrackVert(SCROLLBAR *sb, HWND hwnd, int x, int y, const wdlscrollbar_themestate *theme)
{
	//sw->swapcoords = TRUE;
	RotateRect(&rcThumbBounds);
	ThumbTrackHorz(sb, hwnd, y, x,theme);
	RotateRect(&rcThumbBounds);
	//sw->swapcoords = FALSE;

	return 0;
}

//
//	Called when we have set the capture from the NCLButtonDown(...)
//	
static LRESULT MouseMove(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	RECT rect;
	UINT thisportion;
	HDC hdc;
	static UINT lastportion = 0;
	POINT pt;
	RECT winrect;

	if(sw->fThumbTracking == TRUE)
	{
		LONG x, y;
		lParam = GetMessagePos();
		x = GET_X_LPARAM(lParam);
		y = GET_Y_LPARAM(lParam);
    OSX_REMAP_SCREENY(hwnd,&y);

		if(sw->uCurrentScrollbar == SB_HORZ)
			return ThumbTrackHorz(&sw->sbarHorz, hwnd, x,y,GetThemeForScrollWnd(sw));


		else if(sw->uCurrentScrollbar == SB_VERT)
			return ThumbTrackVert(&sw->sbarVert, hwnd, x,y,GetThemeForScrollWnd(sw));
	}

	if(sw->uCurrentScrollPortion == HTSCROLL_NONE)
	{
		return CallWindowProc(sw->oldproc, hwnd, WM_MOUSEMOVE, wParam, lParam);
	}
	else
	{
		LPARAM nlParam;
		SCROLLBAR *sb = &sw->sbarHorz;

		nlParam = GetMessagePos();

		GET_WINDOW_RECT(hwnd, &winrect);

		pt.x = GET_X_LPARAM(nlParam);
		pt.y = GET_Y_LPARAM(nlParam);
    OSX_REMAP_SCREENY(hwnd,&pt.y);

		//emulate the mouse input on a scrollbar here...
		if(sw->uCurrentScrollbar == SB_HORZ)
		{
			sb = &sw->sbarHorz;
		}
		else if(sw->uCurrentScrollbar == SB_VERT)
		{
			sb = &sw->sbarVert;
		}

		//get the total area of the normal scrollbar area
    BOOL hasZoomButtons;
		GetScrollRect(sw, sb->nBarType, hwnd, &rect,&hasZoomButtons);
		
		//see if we clicked in the inserted buttons / normal scrollbar
		//thisportion = GetPortion(sb, hwnd, &rect, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		thisportion = GetPortion(sb, hwnd, &rect, pt.x, pt.y,hasZoomButtons);
		
    const wdlscrollbar_themestate *theme = GetThemeForScrollWnd(sw);

		//we need to do different things depending on if the
		//user is activating the scrollbar itself, or one of
		//the inserted buttons
		switch(sw->uCurrentScrollPortion)
		{


		//The scrollbar is active
		case HTSCROLL_LEFT:		 case HTSCROLL_RIGHT:case HTSCROLL_THUMB: 
		case HTSCROLL_PAGELEFT:  case HTSCROLL_PAGERIGHT: 
		case HTSCROLL_NONE:
			
			//adjust the total scroll area to become where the scrollbar
			//really is (take into account the inserted buttons)

			OffsetRect(&rect, -winrect.left, -winrect.top);
			hdc = GetWindowDC(hwnd);
		
			if(thisportion != sw->uCurrentScrollPortion)
			{
				sw->uScrollTimerPortion = HTSCROLL_NONE;

				if(lastportion != thisportion)
					NCDrawScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_NORMAL,hasZoomButtons,theme);
			}
			//otherwise, draw the button in its depressed / clicked state
			else
			{
				sw->uScrollTimerPortion = sw->uCurrentScrollPortion;

				if(lastportion != thisportion)
					NCDrawScrollbar(sb, hwnd, hdc, &rect, thisportion,hasZoomButtons,theme);
			}

			ReleaseDC(hwnd, hdc);

			break;
    case HTSCROLL_LRESIZER:
    case HTSCROLL_RRESIZER:
      {
        int nThumbSize, nThumbPos;
        int offs = pt.x - nThumbMouseOffset;
        if(sw->uCurrentScrollbar == SB_VERT) offs = pt.y - nThumbMouseOffset;
        if(sw->uCurrentScrollPortion == HTSCROLL_RRESIZER) offs = -offs;

        if(sw->uCurrentScrollbar == SB_VERT)
        {
          GetVScrollRect(sw, hwnd, &rect,NULL);
		      RotateRect0(sb, &rect);
          CalcThumbSize(sb, &rect, &nThumbSize, &nThumbPos);
          SendMessage(hwnd, (sw->uCurrentScrollPortion == HTSCROLL_RRESIZER?WM_SB_TRESIZE_VB:WM_SB_TRESIZE_VT), offs, ((nThumbSize&0xffff)<<16) + ((rect.right-rect.left)&0xffff));
        }
        else
        {
		      GetHScrollRect(sw, hwnd, &rect,NULL);
		      CalcThumbSize(sb, &rect, &nThumbSize, &nThumbPos);
          SendMessage(hwnd, (sw->uCurrentScrollPortion == HTSCROLL_RRESIZER?WM_SB_TRESIZE_HR:WM_SB_TRESIZE_HL), offs, ((nThumbSize&0xffff)<<16) + ((rect.right-rect.left)&0xffff));
        }

        if(sw->uCurrentScrollbar == SB_VERT) 
        {
          //nThumbMouseOffset = pt.y;
          SetCursor(LoadCursor(NULL,IDC_SIZENS));
        }
        else
        {
          //nThumbMouseOffset = pt.x;
          SetCursor(LoadCursor(NULL,IDC_SIZEWE));
        }
      }
      break;
    case HTSCROLL_RESIZER:
      {
        int offs = pt.x - nThumbMouseOffset;
        if(sw->uCurrentScrollbar == SB_VERT) offs = pt.y - nThumbMouseOffset;
        SendMessage(hwnd, WM_SB_RESIZE, offs, sw->uCurrentScrollbar == SB_VERT);
        if(sw->uCurrentScrollbar == SB_VERT) 
        {
          nThumbMouseOffset = pt.y;
          SetCursor(LoadCursor(NULL,IDC_SIZENS));
        }
        else
        {
          nThumbMouseOffset = pt.x;
          SetCursor(LoadCursor(NULL,IDC_SIZEWE));
        }
      }
      break;
		}


		lastportion = thisportion;

		//must return zero here, because we might get cursor anomilies
		//CallWindowProc(sw->oldproc, hwnd, WM_MOUSEMOVE, wParam, lParam);
		return 0;
		
	}
}


//
//	We must allocate from in the non-client area for our scrollbars
//	Call the default window procedure first, to get the borders (if any)
//	allocated some space, then allocate the space for the scrollbars
//	if they fit
//
static LRESULT NCCalcSize(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	NCCALCSIZE_PARAMS *nccsp;
	RECT *rect;
	RECT oldrect;
//	BOOL fCalcValidRects = (wParam == TRUE);
	SCROLLBAR *sb;
	UINT ret;

	//Regardless of the value of fCalcValidRects, the first rectangle 
	//in the array specified by the rgrc structure member of the 
	//NCCALCSIZE_PARAMS structure contains the coordinates of the window,
	//so we can use the exact same code to modify this rectangle, when
	//wParam is TRUE and when it is FALSE.
	nccsp = (NCCALCSIZE_PARAMS *)lParam;
	rect = &nccsp->rgrc[0];
	oldrect = *rect;


	//call the default procedure to get the borders allocated
	ret = CallWindowProcStyleMod(sw,hwnd, WM_NCCALCSIZE, wParam, lParam);

  // calculate what the size of each window border is,
	sw->cxLeftEdge   = rect->left     - oldrect.left;
	sw->cxRightEdge  = oldrect.right  - rect->right;
	sw->cyTopEdge    = rect->top      - oldrect.top;
	sw->cyBottomEdge = oldrect.bottom - rect->bottom;

	sb = &sw->sbarHorz;

	//if there is room, allocate some space for the horizontal scrollbar
	//NOTE: Change the ">" to a ">=" to make the horz bar totally fill the
	//window before disappearing
  
	if((sb->fScrollFlags & CSBS_VISIBLE) && 
		rect->bottom - rect->top > GetScrollMetric(FALSE, SM_CYHORZSB))
	{
		rect->bottom -= GetScrollMetric(FALSE, SM_CYHORZSB);
		sb->fScrollVisible = TRUE;
	}
	else
  {
		sb->fScrollVisible = FALSE;
  }

	sb = &sw->sbarVert;

	//if there is room, allocate some space for the vertical scrollbar
	if((sb->fScrollFlags & CSBS_VISIBLE) && 
		rect->right - rect->left >= GetScrollMetric(TRUE, SM_CXVERTSB))
	{
		if(sw->fLeftScrollbar)
			rect->left  += GetScrollMetric(TRUE, SM_CXVERTSB);
		else
			rect->right -= GetScrollMetric(TRUE, SM_CXVERTSB);

		sb->fScrollVisible = TRUE;
	}
	else
		sb->fScrollVisible = FALSE;

//printf("nccalcsize, %d,%d,%d,%d -> %d,%d,%d,%d\n",oldrect.left,oldrect.top,oldrect.right,oldrect.bottom,
//       rect->left,rect->top,rect->right,rect->bottom);

	//don't return a value unless we actually modify the other rectangles
	//in the NCCALCSIZE_PARAMS structure. In this case, we return 0
	//no matter what the value of fCalcValidRects is
	return ret;//FALSE;
}

//
//	used for hot-tracking over the scroll buttons
//
static LRESULT NCMouseMove(SCROLLWND *sw, HWND hwnd, WPARAM wHitTest, LPARAM lParam)
{
  {
    LONG x, y;
    int p;
    RECT hr, vr;
    lParam = GetMessagePos();
    x = GET_X_LPARAM(lParam);
    y = GET_Y_LPARAM(lParam);
    OSX_REMAP_SCREENY(hwnd,&y);
    BOOL hasZoomButtons;
    GetHScrollRect(sw, hwnd, &hr,&hasZoomButtons);
    p = GetHorzPortion(&sw->sbarHorz, hwnd, &hr, x, y,hasZoomButtons);
    if(p == HTSCROLL_NONE)
    {
      GetVScrollRect(sw, hwnd, &vr,&hasZoomButtons);
      p = GetVertPortion(&sw->sbarVert, hwnd, &vr, x, y,hasZoomButtons);
    }
    if(p == HTSCROLL_RESIZER || p == HTSCROLL_LRESIZER || p == HTSCROLL_RRESIZER)
    {
      if(wHitTest == HTHSCROLL)
        SetCursor(LoadCursor(NULL,IDC_SIZEWE));
      else
        SetCursor(LoadCursor(NULL,IDC_SIZENS));
    }
#ifndef _WIN32
    else SetCursor(LoadCursor(NULL,IDC_ARROW)); 
#endif
  }

	//install a timer for the mouse-over events, if the mouse moves
	//over one of the scrollbars
	hwndCurCoolSB = hwnd;
	if(wHitTest == HTHSCROLL)
	{
		if(sw->uMouseOverScrollbar == SB_HORZ)
			return CallWindowProc(sw->oldproc, hwnd, WM_NCMOUSEMOVE, wHitTest, lParam);

		sw->uLastHitTestPortion = HTSCROLL_NONE;
		sw->uHitTestPortion     = HTSCROLL_NONE;
		GetScrollRect(sw, SB_HORZ, hwnd, &sw->MouseOverRect, &sw->MouseOverRect_hasZoomButtons);
		sw->uMouseOverScrollbar = SB_HORZ;
		sw->uMouseOverId = SetTimer(hwnd, COOLSB_TIMERID3, COOLSB_TIMERINTERVAL3, 0);

		NCPaint(sw, hwnd, 1, 0);
	}
	else if(wHitTest == HTVSCROLL)
	{
		if(sw->uMouseOverScrollbar == SB_VERT)
			return CallWindowProc(sw->oldproc, hwnd, WM_NCMOUSEMOVE, wHitTest, lParam);

		sw->uLastHitTestPortion = HTSCROLL_NONE;
		sw->uHitTestPortion     = HTSCROLL_NONE;
		GetScrollRect(sw, SB_VERT, hwnd, &sw->MouseOverRect, &sw->MouseOverRect_hasZoomButtons);
		sw->uMouseOverScrollbar = SB_VERT;
		sw->uMouseOverId = SetTimer(hwnd, COOLSB_TIMERID3, COOLSB_TIMERINTERVAL3, 0);

		NCPaint(sw, hwnd, 1, 0);
	}


	return CallWindowProc(sw->oldproc, hwnd, WM_NCMOUSEMOVE, wHitTest, lParam);
}

//
//	Timer routine to generate scrollbar messages
//
static LRESULT CoolSB_Timer(SCROLLWND *swnd, HWND hwnd, WPARAM wTimerId, LPARAM lParam)
{
	//let all timer messages go past if we don't have a timer installed ourselves
	if(swnd->uScrollTimerId == 0 && swnd->uMouseOverId == 0 && swnd->uZoomTimerId == 0)
	{
		return CallWindowProc(swnd->oldproc, hwnd, WM_TIMER, wTimerId, lParam);
	}

  if (wTimerId == COOLSB_TIMERID4)
  {
    SendMessage(hwnd, WM_SB_ZOOM, swnd->uZoomTimerMode/2, swnd->uZoomTimerMode&1);
    return 0;
  }

	//mouse-over timer
	if(wTimerId == COOLSB_TIMERID3)
	{
		POINT pt;
		RECT rect, winrect;
		HDC hdc;
		SCROLLBAR *sbar;

		if(swnd->fThumbTracking)
			return 0;

		//if the mouse moves outside the current scrollbar,
		//then kill the timer..
		GetCursorPos(&pt);
    POINT pt_orig = pt;
    OSX_REMAP_SCREENY(hwnd,&pt.y);

    RECT mor = swnd->MouseOverRect;
    BOOL hasZoomButtons = swnd->MouseOverRect_hasZoomButtons;

    if (hasZoomButtons && swnd->resizingHthumb)
    {
      int zbs = GetZoomButtonSize(swnd->uMouseOverScrollbar==SB_VERT);
      int extrasz=zbs*2+ZOOMBUTTON_RESIZER_SIZE(zbs);

      if(swnd->uMouseOverScrollbar == SB_VERT)
        mor.bottom += extrasz;
      else
        mor.right += extrasz;
    }

		if(!PtInRect(&mor, pt)||WindowFromPoint(pt_orig)!=hwnd)
		{
			KillTimer(hwnd, swnd->uMouseOverId);
			swnd->uMouseOverId = 0;
			swnd->uMouseOverScrollbar = COOLSB_NONE;
			swnd->uLastHitTestPortion = HTSCROLL_NONE;

			swnd->uHitTestPortion = HTSCROLL_NONE;
			NCPaint(swnd, hwnd, 1, 0);
		}
		else
		{
			if(swnd->uMouseOverScrollbar == SB_HORZ)
			{
				sbar = &swnd->sbarHorz;
				swnd->uHitTestPortion = GetHorzPortion(sbar, hwnd, &swnd->MouseOverRect, pt.x, pt.y,hasZoomButtons);
			}
			else
			{
				sbar = &swnd->sbarVert;
				swnd->uHitTestPortion = GetVertPortion(sbar, hwnd, &swnd->MouseOverRect, pt.x, pt.y,hasZoomButtons);
			}
      
			if(swnd->uLastHitTestPortion != swnd->uHitTestPortion)
			{
				rect = swnd->MouseOverRect;

				GET_WINDOW_RECT(hwnd, &winrect);
				OffsetRect(&rect, -winrect.left, -winrect.top);

				hdc = GetWindowDC(hwnd);
				NCDrawScrollbar(sbar, hwnd, hdc, &rect, HTSCROLL_NONE,hasZoomButtons,GetThemeForScrollWnd(swnd));
				ReleaseDC(hwnd, hdc);
			}
			
			swnd->uLastHitTestPortion = swnd->uHitTestPortion;
		}

		return 0;
	}

	//if the first timer goes off, then we can start a more
	//regular timer interval to auto-generate scroll messages
	//this gives a slight pause between first pressing the scroll arrow, and the
	//actual scroll starting
	if(wTimerId == COOLSB_TIMERID1)
	{
		KillTimer(hwnd, swnd->uScrollTimerId);
		swnd->uScrollTimerId = SetTimer(hwnd, COOLSB_TIMERID2, COOLSB_TIMERINTERVAL2, 0);
		return 0;
	}
	//send the scrollbar message repeatedly
	else if(wTimerId == COOLSB_TIMERID2)
	{
		//need to process a spoof WM_MOUSEMOVE, so that
		//we know where the mouse is each time the scroll timer goes off.
		//This is so we can stop sending scroll messages if the thumb moves
		//under the mouse.
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(hwnd, &pt);
		
		MouseMove(swnd, hwnd, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));

		if(swnd->uScrollTimerPortion != HTSCROLL_NONE)
			SendScrollMessage(hwnd, swnd->uScrollTimerMsg, swnd->uScrollTimerPortion, 0);
		
		return 0;
	}
	else
	{
		return CallWindowProc(swnd->oldproc, hwnd, WM_TIMER, wTimerId, lParam);
	}
}

//
//	We must intercept any calls to SetWindowLong, to check if
//  left-scrollbars are taking effect or not
//
static LRESULT CoolSB_StyleChange(SCROLLWND *swnd, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	STYLESTRUCT *ss = (STYLESTRUCT *)lParam;

	if(wParam == GWL_EXSTYLE)
	{
		if(ss->styleNew & WS_EX_LEFTSCROLLBAR)
			swnd->fLeftScrollbar = TRUE;
		else
			swnd->fLeftScrollbar = FALSE;
	}

	return CallWindowProc(swnd->oldproc, hwnd, msg, wParam, lParam);
}



//
//  CoolScrollbar subclass procedure.
//	Handle all messages needed to mimick normal windows scrollbars
//
static LRESULT CALLBACK CoolSBWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WNDPROC oldproc;
	SCROLLWND *swnd = GetScrollWndFromHwnd(hwnd);
	
  switch(message)
	{
	case WM_NCDESTROY:
		//this should NEVER be called, because the user
		//should have called Uninitialize() themselves.

		//However, if the user tries to call Uninitialize().. 
		//after this window is destroyed, this window's entry in the lookup
		//table will not be there, and the call will fail
		oldproc = swnd->oldproc;
		UninitializeCoolSB(hwnd);
		
		//we must call the original window procedure, otherwise it
		//will never get the WM_NCDESTROY message, and it wouldn't
		//be able to clean up etc.
		return CallWindowProc(oldproc, hwnd, message, wParam, lParam);

	case WM_NCCALCSIZE:
		return NCCalcSize(swnd, hwnd, wParam, lParam);

	case WM_NCPAINT:
		return NCPaint(swnd, hwnd, wParam, lParam);	

	case WM_NCHITTEST:
		return NCHitTest(swnd, hwnd, wParam, lParam);

  #ifdef _WIN32
	  case WM_NCRBUTTONDOWN: case WM_NCRBUTTONUP: 
	  case WM_NCMBUTTONDOWN: case WM_NCMBUTTONUP: 
  		  if(wParam == HTHSCROLL || wParam == HTVSCROLL) 
	  		  return 0;
    break;
  #endif

	case WM_NCLBUTTONDBLCLK:
		if(wParam == HTHSCROLL || wParam == HTVSCROLL)
			return NCLButtonDown(swnd, hwnd, wParam, lParam,TRUE);
		else
			break;

	case WM_NCLBUTTONDOWN:
		return NCLButtonDown(swnd, hwnd, wParam, lParam,FALSE);


	case WM_LBUTTONUP:
		return LButtonUp(swnd, hwnd, wParam, lParam);

	case WM_MOUSEMOVE: 
		return MouseMove(swnd, hwnd, wParam, lParam);
	
	case WM_TIMER:
		return CoolSB_Timer(swnd, hwnd, wParam, lParam);

	//case WM_STYLECHANGING:
	//	return CoolSB_StyleChange(swnd, hwnd, WM_STYLECHANGING, wParam, lParam);
	case WM_STYLECHANGED:

		if(swnd->bPreventStyleChange)
		{
			// the NCPAINT handler has told us to eat this message!
			return 0;
		}
		else
		{
            if (message == WM_STYLECHANGED) 
				return CoolSB_StyleChange(swnd, hwnd, WM_STYLECHANGED, wParam, lParam);
			else
				break;
		}

	case WM_NCMOUSEMOVE: 
		return NCMouseMove(swnd, hwnd, wParam, lParam);


	case WM_CAPTURECHANGED:
		break;

#ifdef _WIN32
  case WM_NCACTIVATE: // fix for floating windows etc on XPsp2 etc
  case WM_SYSCOMMAND: // fix for MIDI editor when fully zoomed out on XPsp2

  return CallWindowProcStyleMod(swnd,hwnd,message,wParam,lParam);

  case /*WM_MOUSEHWHEEL:*/ 0x20E:
   {
     const LRESULT rv = CallWindowProc(swnd->oldproc,hwnd,message,wParam,lParam);
     return rv  ? rv : 1; // always return nonzero from WM_MOUSEHWHEEL for Logitech drivers (which will otherwise convert it to a scroll)
   }

#endif
	default:
#if 0
    if (message)
    {
      static int msgs[512]={0,};
      int x;
      for(x=0;x<512&&msgs[x] && msgs[x]!=message;x++);
      if (x<512 && !msgs[x])
      {
        msgs[x]=message;
        FILE *fp = fopen("C:/log.txt","a+");
        if (fp) { fprintf(fp,"%d\n",message); fclose(fp); }

      }
    }
#endif
		break;
	}


	return CallWindowProc(swnd->oldproc, hwnd, message, wParam, lParam);
}

void CoolSB_OnColorThemeChange()
{
  int x;
  for (x=0;x<MAX_SCROLLBAR_THEMES;x++) 
  {
    s_scrollbar_theme[x].bmp = NULL;
    s_scrollbar_theme[x].imageVersion++;
  }
}




#ifndef _WIN32 // SWELL does not yet emulate these, so we have some default behaviors here

static BOOL GetScrollInfo(HWND hwnd, int sb, SCROLLINFO *si)
{
  si->nMin=0; si->nMax=1000; si->nPage=10; si->nPos=si->nTrackPos=0;
  return FALSE;
}
static int GetScrollPos(HWND hwnd, int sb)
{
  return 0;
}

static BOOL GetScrollRange(HWND hwnd, int sb, int *minpos, int *maxpos)
{
  if (minpos) *minpos=0;
  if (maxpos) *maxpos=1000;
  return 0;
}
static BOOL SetScrollInfo(HWND hwnd, int sb, SCROLLINFO *si, BOOL redraw)
{
  return 0;
}
static BOOL SetScrollRange(HWND hwnd, int nBar, int minv, int maxv, BOOL fRedraw)
{
return 0;
}
static int SetScrollPos(HWND hwnd, int nBar, int nPos, BOOL fRedraw)
{
  return 0;
}
static BOOL ShowScrollBar(HWND hwnd, int nBar, BOOL vis)
{
  return 0;
}


#endif

static const char *szPropStr = "CoolSBSubclassPtr";

SCROLLWND *GetScrollWndFromHwnd(HWND hwnd)
{
	return (SCROLLWND *)GetProp(hwnd, szPropStr);
}

SCROLLBAR *GetScrollBarFromHwnd(HWND hwnd, UINT nBar)
{
	SCROLLWND *sw = GetScrollWndFromHwnd(hwnd);
	
	if(!sw) return 0;
	
	if(nBar == SB_HORZ)
		return &sw->sbarHorz;
	else if(nBar == SB_VERT)
		return &sw->sbarVert;
	else
		return 0;
}

BOOL WINAPI CoolSB_IsCoolScrollEnabled(HWND hwnd)
{
	if(GetScrollWndFromHwnd(hwnd))
		return TRUE;
	else
		return FALSE;
}

//
//	Special support for USER32.DLL patching (using Detours library)
//	The only place we call a real scrollbar API is in InitializeCoolSB,
//	where we call EnableScrollbar.
//	
//	We HAVE to call the origial EnableScrollbar function, 
//	so we need to be able to set a pointer to this func when using
//	using Detours (or any other LIB??)
//


static void RedrawNonClient(HWND hwnd, BOOL fFrameChanged)
{
	if(fFrameChanged == FALSE)
	{
		SendMessage(hwnd, WM_NCPAINT, (WPARAM)1, 0);
	}
	else
	{
#ifdef _WIN32
		SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
			| SWP_FRAMECHANGED | SWP_DRAWFRAME);
#else
                InvalidateRect(hwnd,NULL,FALSE);
#endif
	}
}

void CoolSB_SetVScrollPad(HWND hwnd, UINT topamt, UINT botamt, void *(*getDeadAreaBitmap)(int which, HWND hwnd, RECT *r, int col))
{
	SCROLLWND *sw = GetScrollWndFromHwnd(hwnd);
  if (sw && (botamt != sw->vscrollbarShrinkBottom||topamt != sw->vscrollbarShrinkTop||sw->getDeadAreaBitmap != getDeadAreaBitmap)) 
  {
    sw->getDeadAreaBitmap=getDeadAreaBitmap;
    sw->vscrollbarShrinkBottom = botamt;
    sw->vscrollbarShrinkTop = topamt;
#ifdef _WIN32
    RedrawNonClient(hwnd,FALSE);
#endif
  }
}


//
//	return the default minimum size of a scrollbar thumb
//
int WINAPI CoolSB_GetDefaultMinThumbSize(void)
{
#ifdef _WIN32
	DWORD dwVersion = GetVersion();

	// set the minimum thumb size for a scrollbar. This
	// differs between NT4 and 2000, so need to check to see
	// which platform we are running under
	if(dwVersion < 0x80000000)              // Windows NT/2000
	{
		if(LOBYTE(LOWORD(dwVersion)) >= 5)
			return MINTHUMBSIZE_2000;
		else
			return MINTHUMBSIZE_NT4;
	}
	else
	{
		return MINTHUMBSIZE_NT4;
	}
  #else
			return MINTHUMBSIZE_2000;
  #endif
}


static SCROLLINFO *GetScrollInfoFromHwnd(HWND hwnd, int fnBar)
{
	SCROLLBAR *sb = GetScrollBarFromHwnd(hwnd, fnBar);

	if(sb == 0)
		return FALSE;

	if(fnBar == SB_HORZ)
	{
		return &sb->scrollInfo;
	}
	else if(fnBar == SB_VERT)
	{
		return &sb->scrollInfo;
	}
	else
		return NULL;
}
//
//	Initialize the cool scrollbars for a window by subclassing it
//	and using the coolsb window procedure instead
//
BOOL WINAPI InitializeCoolSB(HWND hwnd)
{
	SCROLLWND *sw;
	SCROLLINFO *si;
	RECT rect;
	DWORD dwCurStyle;
	//BOOL fDisabled;


	GetClientRect(hwnd, &rect);

	//if we have already initialized Cool Scrollbars for this window,
	//then stop the user from doing it again
	if(GetScrollWndFromHwnd(hwnd) != 0)
	{
		return FALSE;
	}

	//allocate a private scrollbar structure which we 
	//will use to keep track of the scrollbar data
#ifdef _WIN32
	sw = (SCROLLWND *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SCROLLWND));
#else
  sw = (SCROLLWND *)calloc(1,sizeof(SCROLLWND));
#endif

  sw->uCurrentScrollbar = COOLSB_NONE;	//SB_HORZ / SB_VERT
  sw->uCurrentScrollPortion = HTSCROLL_NONE;
  sw->uMouseOverScrollbar = COOLSB_NONE;
  sw->uHitTestPortion = HTSCROLL_NONE;
  sw->uLastHitTestPortion = HTSCROLL_NONE;
  sw->uScrollTimerPortion = HTSCROLL_NONE;

	si = &sw->sbarHorz.scrollInfo;
	si->cbSize = sizeof(SCROLLINFO);
	si->fMask  = SIF_ALL;
	GetScrollInfo(hwnd, SB_HORZ, si);

	si = &sw->sbarVert.scrollInfo;
	si->cbSize = sizeof(SCROLLINFO);
	si->fMask  = SIF_ALL;
	GetScrollInfo(hwnd, SB_VERT, si);

	//check to see if the window has left-aligned scrollbars
	if(GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_LEFTSCROLLBAR)
		sw->fLeftScrollbar = TRUE;
	else
		sw->fLeftScrollbar = FALSE;

	dwCurStyle = GetWindowLong(hwnd, GWL_STYLE);

	SetProp(hwnd, szPropStr, (HANDLE)sw);


	//scrollbars will automatically get enabled, even if
	//they aren't to start with....sorry, but there isn't an
	//easy alternative.
	if(dwCurStyle & WS_HSCROLL)
		sw->sbarHorz.fScrollFlags = CSBS_VISIBLE;

	if(dwCurStyle & WS_VSCROLL)
		sw->sbarVert.fScrollFlags = CSBS_VISIBLE;

	//need to be able to distinguish between horizontal and vertical
	//scrollbars in some instances
	sw->sbarHorz.nBarType	     = SB_HORZ;
	sw->sbarVert.nBarType	     = SB_VERT;

	sw->bPreventStyleChange		 = FALSE;

  sw->resizingHthumb = FALSE;
	
	sw->oldproc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (INT_PTR)CoolSBWndProc);

	CoolSB_SetMinThumbSize(hwnd, SB_BOTH, CoolSB_GetDefaultMinThumbSize());

	
	//send the window a frame changed message to update the scrollbars
	RedrawNonClient(hwnd, TRUE);

	return TRUE;
}


BOOL WINAPI CoolSB_IsThumbTracking(HWND hwnd)	
{ 
	SCROLLWND *sw;

	if((sw = GetScrollWndFromHwnd(hwnd)) == NULL)
		return FALSE;
	else
		return sw->fThumbTracking; 
}


BOOL WINAPI CoolSB_GetScrollInfo (HWND hwnd, int fnBar, LPSCROLLINFO lpsi)
{
	SCROLLINFO *mysi;
	BOOL copied = FALSE;
	
	if(!lpsi)
		return FALSE;

	if(!(mysi = GetScrollInfoFromHwnd(hwnd, fnBar)))
	{
		return GetScrollInfo(hwnd, fnBar, lpsi);
	}
	
	if(lpsi->fMask & SIF_PAGE)
	{
		lpsi->nPage = mysi->nPage;
		copied = TRUE;
	}

	if(lpsi->fMask & SIF_POS)
	{
		lpsi->nPos = mysi->nPos;
		copied = TRUE;
	}

	if(lpsi->fMask & SIF_TRACKPOS)
	{
		lpsi->nTrackPos = mysi->nTrackPos;
		copied = TRUE;
	}

	if(lpsi->fMask & SIF_RANGE)
	{
		lpsi->nMin = mysi->nMin;
		lpsi->nMax = mysi->nMax;
		copied = TRUE;
	}

	return copied;
}

int	WINAPI CoolSB_GetScrollPos (HWND hwnd, int nBar)
{
	SCROLLINFO *mysi;
	
	if(!(mysi = GetScrollInfoFromHwnd(hwnd, nBar)))
		return GetScrollPos(hwnd, nBar);

	return mysi->nPos;
}

BOOL WINAPI CoolSB_GetScrollRange (HWND hwnd, int nBar, LPINT lpMinPos, LPINT lpMaxPos)
{
	SCROLLINFO *mysi;
	
	if(!lpMinPos || !lpMaxPos)
		return FALSE;

	if(!(mysi = GetScrollInfoFromHwnd(hwnd, nBar)))
		return GetScrollRange(hwnd, nBar, lpMinPos, lpMaxPos);

	*lpMinPos = mysi->nMin;
	*lpMaxPos = mysi->nMax;

	return TRUE;
}

int	WINAPI CoolSB_SetScrollInfo (HWND hwnd, int fnBar, LPSCROLLINFO lpsi, BOOL fRedraw)
{
	SCROLLINFO *mysi;
	SCROLLBAR *sbar;
	BOOL       fRecalcFrame = FALSE;

	if(!lpsi)
		return FALSE;

	if(!(mysi = GetScrollInfoFromHwnd(hwnd, fnBar)))
		return SetScrollInfo(hwnd, fnBar, lpsi, fRedraw);

	//if(CoolSB_IsThumbTracking(hwnd))
	//	return mysi->nPos;

	if(lpsi->fMask & SIF_RANGE)
	{
		mysi->nMin = lpsi->nMin;
		mysi->nMax = lpsi->nMax;
	}

	//The nPage member must specify a value from 0 to nMax - nMin +1. 
	if(lpsi->fMask & SIF_PAGE)
	{
		UINT t = (UINT)(mysi->nMax - mysi->nMin + 1);
		mysi->nPage = wdl_min(wdl_max(0, lpsi->nPage), t);
	}

	//The nPos member must specify a value between nMin and nMax - wdl_max(nPage - 1, 0).
	if(lpsi->fMask & SIF_POS)
	{
		mysi->nPos = wdl_max(lpsi->nPos, mysi->nMin);
		mysi->nPos = wdl_min((UINT)mysi->nPos, mysi->nMax - wdl_max(mysi->nPage - 1, 0));
	}

	sbar = GetScrollBarFromHwnd(hwnd, fnBar);

	if((lpsi->fMask & SIF_DISABLENOSCROLL) || (sbar->fScrollFlags & CSBS_THUMBALWAYS))
	{
		if(!sbar->fScrollVisible)
		{
			CoolSB_ShowScrollBar(hwnd, fnBar, TRUE);
			fRecalcFrame = TRUE;
		}
	}
	else
	{
		if(mysi->nPage >  (UINT)mysi->nMax
			|| (mysi->nPage == (UINT)mysi->nMax && mysi->nMax == 0)
			|| mysi->nMax  <= mysi->nMin)
		{
			if(sbar->fScrollVisible)
			{
				CoolSB_ShowScrollBar(hwnd, fnBar, FALSE);
				fRecalcFrame = TRUE;
			}
		}
		else
		{
			if(!sbar->fScrollVisible)
			{
				CoolSB_ShowScrollBar(hwnd, fnBar, TRUE);
				fRecalcFrame = TRUE;
			}

		}

	}

	if(fRedraw && !CoolSB_IsThumbTracking(hwnd))
		RedrawNonClient(hwnd, fRecalcFrame);
	
	return mysi->nPos;
}


int WINAPI CoolSB_SetScrollPos(HWND hwnd, int nBar, int nPos, BOOL fRedraw)
{
	SCROLLINFO *mysi;
	int oldpos;
	
	if(!(mysi = GetScrollInfoFromHwnd(hwnd, nBar)))
	{
		return SetScrollPos(hwnd, nBar, nPos, fRedraw);
	}

	//this is what should happen, but real scrollbars don't work like this..
	//if(CoolSB_IsThumbTracking(hwnd))
	//	return mysi->nPos;

	//validate and set the scollbar position
	oldpos = mysi->nPos;
	mysi->nPos = wdl_max(nPos, mysi->nMin);
	mysi->nPos = wdl_min((UINT)mysi->nPos, mysi->nMax - wdl_max(mysi->nPage - 1, 0));

	if(fRedraw && !CoolSB_IsThumbTracking(hwnd))
		RedrawNonClient(hwnd, FALSE);

	return oldpos;
}

int WINAPI CoolSB_SetScrollRange (HWND hwnd, int nBar, int nMinPos, int nMaxPos, BOOL fRedraw)
{
	SCROLLINFO *mysi;
	
	if(!(mysi = GetScrollInfoFromHwnd(hwnd, nBar)))
		return SetScrollRange(hwnd, nBar, nMinPos, nMaxPos, fRedraw);

	if(CoolSB_IsThumbTracking(hwnd))
		return mysi->nPos;

	//hide the scrollbar if nMin == nMax
	//nMax-nMin must not be greater than MAXLONG
	mysi->nMin = nMinPos;
	mysi->nMax = nMaxPos;
	
	if(fRedraw)
		RedrawNonClient(hwnd, FALSE);

	return TRUE;
}

//
//	Show or hide the specified scrollbars
//
BOOL WINAPI CoolSB_ShowScrollBar (HWND hwnd, int wBar, BOOL fShow)
{
	SCROLLBAR *sbar;
	BOOL bFailed = FALSE;
	DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

	if(!CoolSB_IsCoolScrollEnabled(hwnd))
  {
		return ShowScrollBar(hwnd, wBar, fShow);
  }
	if((wBar == SB_HORZ || wBar == SB_BOTH) && 
	   (sbar = GetScrollBarFromHwnd(hwnd, SB_HORZ)))
	{
		sbar->fScrollFlags  =  sbar->fScrollFlags & ~CSBS_VISIBLE;
		sbar->fScrollFlags |= (fShow == TRUE ? CSBS_VISIBLE : 0);
		//bFailed = TRUE;
    
		if(fShow)	SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_HSCROLL);
		else		SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_HSCROLL);
	}

	if((wBar == SB_VERT || wBar == SB_BOTH) && 
	   (sbar = GetScrollBarFromHwnd(hwnd, SB_VERT)))
	{
		sbar->fScrollFlags  =  sbar->fScrollFlags & ~CSBS_VISIBLE;
		sbar->fScrollFlags |= (fShow == TRUE ? CSBS_VISIBLE : 0);
		//bFailed = TRUE;

		if(fShow)	SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_VSCROLL);
		else		SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_VSCROLL);
	}

	if(bFailed)
	{
		return FALSE;
	}
	else
	{
		//DWORD style = GetWindowLong(hwnd, GWL_STYLE);
		//style |= WS_VSCROLL;
		
		//if(s
		//SetWindowLong(hwnd, GWL_STYLE, style);

		SetWindowPos(hwnd, 0, 0, 0, 0, 0, 
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | 
			SWP_NOACTIVATE | SWP_FRAMECHANGED);
		
		return TRUE;
	}
}

//
//	Remove cool scrollbars from the specified window.
//
HRESULT WINAPI UninitializeCoolSB(HWND hwnd)
{
	SCROLLWND *sw = GetScrollWndFromHwnd(hwnd);
	if(!sw) return E_FAIL;

	//restore the window procedure with the original one
	SetWindowLongPtr(hwnd, GWLP_WNDPROC, (INT_PTR)sw->oldproc);

	RemoveProp(hwnd, szPropStr);
	//SetWindowLong(hwnd, GWL_USERDATA, 0);

  delete sw->sbarHorz.liceBkgnd;
  delete sw->sbarHorz.liceThumb;
  delete sw->sbarVert.liceBkgnd;
  delete sw->sbarVert.liceThumb;
  sw->sbarHorz.liceBkgnd = NULL;
  sw->sbarHorz.liceThumb = NULL;
  sw->sbarVert.liceBkgnd = NULL;
  sw->sbarVert.liceThumb = NULL;

      //finally, release the memory needed for the cool scrollbars
#ifdef _WIN32
	HeapFree(GetProcessHeap(), 0, sw);
#else
  free(sw);
#endif

  //Force WM_NCCALCSIZE and WM_NCPAINT so the original scrollbars can kick in
  RedrawNonClient(hwnd, TRUE);
  
	return S_OK;
}


//
//	Set the minimum size, in pixels, that the thumb box will shrink to.
//
BOOL WINAPI CoolSB_SetMinThumbSize(HWND hwnd, UINT wBar, UINT size)
{
	SCROLLBAR *sbar;

	if(!GetScrollWndFromHwnd(hwnd))
		return FALSE;

	if(size == -1)
		size = CoolSB_GetDefaultMinThumbSize();

	if((wBar == SB_HORZ || wBar == SB_BOTH) && 
	   (sbar = GetScrollBarFromHwnd(hwnd, SB_HORZ)))
	{
		sbar->nMinThumbSize = size;
	}

	if((wBar == SB_VERT || wBar == SB_BOTH) && 
	   (sbar = GetScrollBarFromHwnd(hwnd, SB_VERT)))
	{
		sbar->nMinThumbSize = size;
	}

	return TRUE;
}

BOOL WINAPI CoolSB_SetResizingThumb(HWND hwnd, BOOL active)
{
	SCROLLWND *swnd;

	if(!(swnd = GetScrollWndFromHwnd(hwnd)))
		return FALSE;

  swnd->resizingHthumb = active;

  return TRUE;
}
BOOL WINAPI CoolSB_SetThemeIndex(HWND hwnd, int idx)
{
	SCROLLWND *swnd;

	if(!(swnd = GetScrollWndFromHwnd(hwnd)))
		return FALSE;

  swnd->whichTheme = idx;
  swnd->sbarHorz.liceBkgnd_ver += 0x800;
  swnd->sbarVert.liceBkgnd_ver += 0x800;
  swnd->sbarHorz.liceThumb_ver += 0x800;
  swnd->sbarVert.liceThumb_ver += 0x800;

  return TRUE;
}
