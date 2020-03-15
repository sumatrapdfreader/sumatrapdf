#ifndef _COOLSBLIB_INCLUDED
#define _COOLSBLIB_INCLUDED

/*
    WDL - Skinned/Resizing thumb scrollbar library 
    Based on the "Cool Scrollbar Library v1.2" by James Brown - http://www.catch22.net
  
    Original version Copyright(c) 2001 J Brown
    Modifications copyright (C) 2006 and later Cockos Incorporated

    Note: for a more featureful, less hacked up version, you may wish to download the 
    original from catch22.net. It has lots of added features, whereas this version is 
    very much tailored for Cockos' needs.

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
    
*/

#ifdef __cplusplus
extern "C"{
#endif

/*

	Public interface to the Cool Scrollbar library


*/

// notifications sent on user actions
#define WM_SB_RESIZE (WM_USER+511)
#define WM_SB_ZOOM (WM_USER+512)
#define WM_SB_TRESIZE_HL (WM_USER+513)
#define WM_SB_TRESIZE_HR (WM_USER+514)
#define WM_SB_TRESIZE_VT (WM_USER+515)
#define WM_SB_TRESIZE_VB (WM_USER+516)
#define WM_SB_TRESIZE_START (WM_USER+517)
#define WM_SB_DBLCLK (WM_USER+518) // wParam has SB_HORZ or SB_VERT


#ifndef COOLSB_NO_FUNC_DEFS
BOOL	WINAPI InitializeCoolSB(HWND hwnd);
HRESULT WINAPI UninitializeCoolSB(HWND hwnd); // call in WM_DESTROY -- not strictly required, but recommended

BOOL WINAPI CoolSB_SetMinThumbSize(HWND hwnd, UINT wBar, UINT size);
BOOL WINAPI CoolSB_IsThumbTracking(HWND hwnd);
BOOL WINAPI CoolSB_IsCoolScrollEnabled(HWND hwnd);
void CoolSB_SetVScrollPad(HWND hwnd, UINT topamt, UINT botamt, void *(*getDeadAreaBitmap)(int which, HWND hwnd, RECT *, int defcol));
//
BOOL WINAPI CoolSB_GetScrollInfo(HWND hwnd, int fnBar, LPSCROLLINFO lpsi);
int	 WINAPI CoolSB_GetScrollPos(HWND hwnd, int nBar);
BOOL WINAPI CoolSB_GetScrollRange(HWND hwnd, int nBar, LPINT lpMinPos, LPINT lpMaxPos);

//
int	 WINAPI CoolSB_SetScrollInfo	(HWND hwnd, int fnBar, LPSCROLLINFO lpsi, BOOL fRedraw);
int  WINAPI CoolSB_SetScrollPos	(HWND hwnd, int nBar, int nPos, BOOL fRedraw);
int  WINAPI CoolSB_SetScrollRange	(HWND hwnd, int nBar, int nMinPos, int nMaxPos, BOOL fRedraw);
BOOL WINAPI CoolSB_ShowScrollBar	(HWND hwnd, int wBar, BOOL fShow);

BOOL WINAPI CoolSB_SetResizingThumb(HWND hwnd, BOOL active);
BOOL WINAPI CoolSB_SetThemeIndex(HWND hwnd, int idx);
void CoolSB_SetScale(float scale); // sets scale to use for scrollbars (does not refresh, though -- set this at startup/etc)
void CoolSB_OnColorThemeChange(); // refreshes all



// TO BE IMPLEMENTED BY APP:
void *GetIconThemePointer(const char *name); // implemented by calling app, can return a LICE_IBitmap **img for "scrollbar"
int CoolSB_GetSysColor(HWND hwnd, int val); // can be a passthrough to GetSysColor()

#endif


#ifdef __cplusplus
}
#endif

#endif
