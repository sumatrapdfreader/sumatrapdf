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

#ifndef _WDL_SWELL_H_TYPES_DEFINED_
#define _WDL_SWELL_H_TYPES_DEFINED_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <ctype.h>

#if defined(__cplusplus)
#include <cstddef>
#endif

#include <stdint.h>
typedef intptr_t INT_PTR, *PINT_PTR, LONG_PTR, *PLONG_PTR;
typedef uintptr_t UINT_PTR, *PUINT_PTR, ULONG_PTR, *PULONG_PTR, DWORD_PTR, *PDWORD_PTR;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifndef S_OK
#define S_OK 0
#endif
#ifndef E_FAIL
#define E_FAIL (-1)
#endif


// the byte ordering of RGB() etc is different than on win32 
#define RGB(r,g,b) (((r)<<16)|((g)<<8)|(b))
#define GetRValue(x) (((x)>>16)&0xff)
#define GetGValue(x) (((x)>>8)&0xff)
#define GetBValue(x) ((x)&0xff)

// basic platform compat defines
#ifndef stricmp
#define stricmp(x,y) strcasecmp(x,y)
#endif
#ifndef strnicmp
#define strnicmp(x,y,z) strncasecmp(x,y,z)
#endif

#define DeleteFile(x) (!unlink(x))
#define MoveFile(x,y) (!rename(x,y))
#define GetCurrentDirectory(sz,buf) (!getcwd(buf,sz))
#define SetCurrentDirectory(buf) (!chdir(buf))
#define CreateDirectory(x,y) (!mkdir((x),0755))

#ifndef wsprintf
#define wsprintf sprintf
#endif

#ifndef LOWORD
#define MAKEWORD(a, b)      ((unsigned short)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b)      ((int)(((unsigned short)(a)) | ((DWORD)((unsigned short)(b))) << 16))
#define MAKEWPARAM(l, h)      (WPARAM)MAKELONG(l, h)
#define MAKELPARAM(l, h)      (LPARAM)MAKELONG(l, h)
#define MAKELRESULT(l, h)     (LRESULT)MAKELONG(l, h)
#define LOWORD(l)           ((unsigned short)(l))
#define HIWORD(l)           ((unsigned short)(((unsigned int)(l) >> 16) & 0xFFFF))
#define LOBYTE(w)           ((BYTE)(w))
#define HIBYTE(w)           ((BYTE)(((unsigned short)(w) >> 8) & 0xFF))
#endif

#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))

#define UNREFERENCED_PARAMETER(P) (P)
#define _T(T) T

#define CallWindowProc(A,B,C,D,E) ((WNDPROC)A)(B,C,D,E)
#define OffsetRect WinOffsetRect  //to avoid OSX's OffsetRect function
#define SetRect WinSetRect        //to avoid OSX's SetRect function
#define UnionRect WinUnionRect  
#define IntersectRect WinIntersectRect


#define MAX_PATH 1024


#if !defined(max) && !defined(WDL_NO_DEFINE_MINMAX) && !defined(NOMINMAX)
#define max(x,y) ((x)<(y)?(y):(x))
#define min(x,y) ((x)<(y)?(x):(y))
#endif

// SWELLAPP stuff (swellappmain.mm)
#ifdef __cplusplus
extern "C"  {
#endif
INT_PTR SWELLAppMain(int msg, INT_PTR parm1, INT_PTR parm2); // to be implemented by app (if using swellappmain.mm)
#ifdef __cplusplus
};
#endif

#define SWELLAPP_ONLOAD 0x0001 // initialization of app vars etc
#define SWELLAPP_LOADED 0x0002 // create dialogs etc
#define SWELLAPP_DESTROY 0x0003 // about to destroy (cleanup etc)
#define SWELLAPP_SHOULDDESTROY 0x0004 // return 0 to allow app to terminate, >0 to prevent

#define SWELLAPP_OPENFILE 0x0050 // parm1= (const char *)string, return >0 if allowed
#define SWELLAPP_NEWFILE 0x0051 // new file, return >0 if allowed
#define SWELLAPP_SHOULDOPENNEWFILE 0x0052 // allow opening new file? >0 if allowed

#define SWELLAPP_ONCOMMAND 0x0099 // parm1 = (int) command ID, parm2 = (id) sender 
#define SWELLAPP_PROCESSMESSAGE 0x0100 // parm1=(MSG *)msg (loosely), parm2= (NSEvent *) the event . return >0 to eat

#define SWELLAPP_ACTIVATE 0x1000  // parm1 = (bool) isactive. return nonzero to prevent WM_ACTIVATEAPP from being broadcasted
//




// basic types
typedef signed char BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef DWORD COLORREF;
typedef unsigned int UINT;
typedef int INT;

typedef ULONG_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;


typedef void *LPVOID, *PVOID;

#if defined(__APPLE__) && !defined(__LP64__)
typedef signed long HRESULT;
typedef signed long LONG;
typedef unsigned long ULONG;
#else
typedef signed int HRESULT;
typedef signed int LONG;
typedef unsigned int ULONG;
#endif

typedef short SHORT;
typedef int *LPINT;
typedef char CHAR;
typedef char *LPSTR, *LPTSTR;
typedef const char *LPCSTR, *LPCTSTR;

#define __int64 long long // define rather than typedef, for unsigned __int64 support

typedef unsigned __int64 ULONGLONG;

typedef union { 
  unsigned long long QuadPart; 
  struct {
  #ifdef __ppc__
    DWORD HighPart;
    DWORD LowPart;
  #else
    DWORD LowPart;
    DWORD HighPart;
  #endif
  };
} ULARGE_INTEGER;


typedef struct HWND__ *HWND;
typedef struct HMENU__ *HMENU;
typedef void *HANDLE, *HINSTANCE, *HDROP;
typedef void *HGLOBAL;

typedef void (*TIMERPROC)(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

typedef struct 
{
  LONG x,y;
} POINT, *LPPOINT;


typedef struct
{
  SHORT x;
  SHORT y;
} POINTS;


typedef struct 
{
  LONG left,top, right, bottom;
} RECT, *LPRECT;


typedef struct {
  unsigned char fVirt;
  unsigned short key,cmd;
} ACCEL, *LPACCEL;


typedef struct {
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME;

typedef struct _GUID {
  unsigned int Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char  Data4[8];
} GUID;

typedef struct {
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
} MSG, *LPMSG;

typedef struct HDC__ *HDC;
typedef struct HCURSOR__ *HCURSOR;
typedef struct HRGN__ *HRGN;

typedef struct HGDIOBJ__ *HBITMAP;
typedef struct HGDIOBJ__ *HICON;
typedef struct HGDIOBJ__ *HGDIOBJ;
typedef struct HGDIOBJ__ *HBRUSH;
typedef struct HGDIOBJ__ *HPEN;
typedef struct HGDIOBJ__ *HFONT;


typedef struct
{
  HWND  hwndFrom;
  UINT_PTR  idFrom;
  UINT  code;
} NMHDR, *LPNMHDR;


typedef struct {
  NMHDR   hdr;
  DWORD_PTR   dwItemSpec;
  DWORD_PTR   dwItemData;
  POINT   pt;
  DWORD   dwHitInfo;
} NMMOUSE, *LPNMMOUSE;
typedef NMMOUSE NMCLICK;
typedef LPNMMOUSE LPNMCLICK;

typedef struct 
{ 
  int mask, fmt,cx; 
  char *pszText; 
  int cchTextMax, iSubItem;
} LVCOLUMN;
typedef struct 
{ 
  int mask, iItem, iSubItem, state, stateMask; 
  char *pszText; 
  int cchTextMax, iImage;
  LPARAM lParam;
} LVITEM;

typedef int (*PFNLVCOMPARE)(LPARAM, LPARAM, LPARAM);

typedef struct HIMAGELIST__ *HIMAGELIST;

typedef struct
{
  POINT pt;
  UINT flags;
  int iItem;
  int iSubItem;    // this is was NOT in win95.  valid only for LVM_SUBITEMHITTEST
} LVHITTESTINFO, *LPLVHITTESTINFO;


typedef struct
{
  NMHDR   hdr;
  int     iItem;
  int     iSubItem;
  UINT    uNewState;
  UINT    uOldState;
  UINT    uChanged;
  POINT   ptAction;
  LPARAM  lParam;
} NMLISTVIEW, *LPNMLISTVIEW;

typedef struct
{
  NMHDR hdr;
  LVITEM item;
} NMLVDISPINFO, *LPNMLVDISPINFO;

typedef struct
{
  UINT    mask;
  int     cxy;
  char*   pszText;
  HBITMAP hbm;
  int     cchTextMax;
  int     fmt;
  LPARAM  lParam;
  int     iImage;
  int     iOrder;
  UINT    type;
  void    *pvFilter;
  UINT    state;
} HDITEM, *LPHDITEM;  

typedef struct TCITEM
{
  UINT mask;
  DWORD dwState;
  DWORD dwStateMask;
  char *pszText;
  int cchTextMax;
  int iImage;
  
  LPARAM lParam;
} TCITEM, *LPTCITEM;

typedef struct tagDRAWITEMSTRUCT {
    UINT        CtlType;
    UINT        CtlID;
    UINT        itemID;
    UINT        itemAction;
    UINT        itemState;
    HWND        hwndItem;
    HDC         hDC;
    RECT        rcItem;
    DWORD_PTR   itemData;
} DRAWITEMSTRUCT, *PDRAWITEMSTRUCT, *LPDRAWITEMSTRUCT;

typedef struct tagBITMAP {
  LONG bmWidth;
  LONG bmHeight;
  LONG bmWidthBytes;
  WORD bmPlanes;
  WORD bmBitsPixel;
  LPVOID bmBits;
} BITMAP, *PBITMAP, *LPBITMAP;
#define ODT_MENU        1
#define ODT_LISTBOX     2
#define ODT_COMBOBOX    3
#define ODT_BUTTON      4

#define ODS_SELECTED    0x0001




typedef struct 
{  
        DWORD cbSize;
        HWND hWnd;
        UINT uID;
        UINT uFlags;
        UINT uCallbackMessage;
        HICON hIcon;      
        CHAR   szTip[64];
} NOTIFYICONDATA,*PNOTIFYICONDATA, *LPNOTIFYICONDATA;


#define NIM_ADD         0x00000000
#define NIM_MODIFY      0x00000001
#define NIM_DELETE      0x00000002

#define NIF_MESSAGE     0x00000001
#define NIF_ICON        0x00000002
#define NIF_TIP         0x00000004



typedef struct HTREEITEM__ *HTREEITEM;

#define TVIF_TEXT               0x0001
#define TVIF_IMAGE              0x0002
#define TVIF_PARAM              0x0004
#define TVIF_STATE              0x0008
#define TVIF_HANDLE             0x0010
#define TVIF_SELECTEDIMAGE      0x0020
#define TVIF_CHILDREN           0x0040

#define TVIS_SELECTED           0x0002
#define TVIS_DROPHILITED        0x0008
#define TVIS_BOLD               0x0010
#define TVIS_EXPANDED           0x0020

#define TVE_COLLAPSE            0x0001
#define TVE_EXPAND              0x0002
#define TVE_TOGGLE              0x0003

#define TVN_FIRST               (0U-400U)       // treeview
#define TVN_SELCHANGED          (TVN_FIRST-2)

// swell-extension: WM_MOUSEMOVE set via capture in TVN_BEGINDRAG can return:
//   -1 = drag not possible
//   -2 = destination at end of list
//   (HTREEITEM) = will end up before this item
#define TVN_BEGINDRAG           (TVN_FIRST-7) 

#define TVI_ROOT                ((HTREEITEM)0xFFFF0000)
#define TVI_FIRST               ((HTREEITEM)0xFFFF0001)
#define TVI_LAST                ((HTREEITEM)0xFFFF0002)
#define TVI_SORT                ((HTREEITEM)0xFFFF0003)

#define TVHT_NOWHERE            0x0001
#define TVHT_ONITEMICON         0x0002
#define TVHT_ONITEMLABEL        0x0004
#define TVHT_ONITEM             (TVHT_ONITEMICON | TVHT_ONITEMLABEL | TVHT_ONITEMSTATEICON)
#define TVHT_ONITEMINDENT       0x0008
#define TVHT_ONITEMBUTTON       0x0010
#define TVHT_ONITEMRIGHT        0x0020
#define TVHT_ONITEMSTATEICON    0x0040

#define TVHT_ABOVE              0x0100
#define TVHT_BELOW              0x0200
#define TVHT_TORIGHT            0x0400
#define TVHT_TOLEFT             0x0800

typedef struct {
  UINT      mask;
  HTREEITEM hItem;
  UINT      state;
  UINT      stateMask;
  char *pszText;
  int       cchTextMax;
  int       iImage;
  int       iSelectedImage;
  int       cChildren;
  LPARAM    lParam;
} TVITEM, TV_ITEM, *LPTVITEM, *LPTV_ITEM;

typedef struct {
  HTREEITEM hParent;
  HTREEITEM hInsertAfter;
  TVITEM item;
} TVINSERTSTRUCT, *LPTVINSERTSTRUCT, TV_INSERTSTRUCT, *LPTV_INSERTSTRUCT;

typedef struct {
  POINT       pt;
  UINT        flags;
  HTREEITEM   hItem;
} TVHITTESTINFO, *LPTVHITTESTINFO;

typedef struct {
  NMHDR       hdr;
  UINT        action;
  TVITEM    itemOld;
  TVITEM    itemNew;
  POINT       ptDrag;
} NMTREEVIEW, *LPNMTREEVIEW;


typedef struct
{
  unsigned int cbSize, fMask, fType, fState, wID;
  HMENU hSubMenu;
  HICON hbmpChecked,hbmpUnchecked;
  DWORD_PTR dwItemData;
  char *dwTypeData;
  int cch;
  HBITMAP hbmpItem;
} MENUITEMINFO;

#define SetMenuDefaultItem(a,b,c) do { if ((a)||(b)||(c)) { } } while(0)

typedef struct {
  POINT ptReserved, ptMaxSize, ptMaxPosition, ptMinTrackSize, ptMaxTrackSize;
} MINMAXINFO, *LPMINMAXINFO;


typedef struct
{
  int lfHeight, lfWidth, lfEscapement,lfOrientation, lfWeight;
  char lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, 
    lfQuality, lfPitchAndFamily;
  char lfFaceName[32];
} LOGFONT;
typedef struct
{
  LONG tmHeight; 
  LONG tmAscent; 
  LONG tmDescent; 
  LONG tmInternalLeading; 
  LONG tmAveCharWidth;
  // todo: implement rest
} TEXTMETRIC;

typedef struct {
  HDC         hdc;
  BOOL        fErase;
  RECT        rcPaint;
} PAINTSTRUCT;

typedef struct
{
  UINT    cbSize;
  UINT    fMask;
  int     nMin;
  int     nMax;
  UINT    nPage;
  int     nPos;
  int     nTrackPos;
} SCROLLINFO, *LPSCROLLINFO;

typedef struct
{
  DWORD   styleOld;
  DWORD   styleNew;
} STYLESTRUCT, *LPSTYLESTRUCT;

typedef struct _DROPFILES {
   DWORD pFiles;                       // offset of file list
   POINT pt;                           // drop point (client coords)
   BOOL fNC;                           // is it on NonClient area
                                       // and pt is in screen coords
   BOOL fWide;                         // WIDE character switch
} DROPFILES, *LPDROPFILES;


typedef struct
{
  HWND    hwnd;
  HWND    hwndInsertAfter;
  int     x;
  int     y;
  int     cx;
  int     cy;
  UINT    flags;
} WINDOWPOS, *LPWINDOWPOS, *PWINDOWPOS;

typedef struct  
{
  RECT       rgrc[3];
  PWINDOWPOS lppos;
} NCCALCSIZE_PARAMS, *LPNCCALCSIZE_PARAMS;



typedef INT_PTR (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);



#define GF_BEGIN 1
#define GF_INERTIA 2
#define GF_END 4

#define GID_BEGIN 1
#define GID_END   2
#define GID_ZOOM  3
#define GID_PAN   4
#define GID_ROTATE  5
#define GID_TWOFINGERTAP  6
#define GID_ROLLOVER      7

typedef struct tagGESTUREINFO
{
  UINT cbSize;
  DWORD dwFlags;
  DWORD dwID;
  HWND hwndTarget;
  POINTS ptsLocation;
  DWORD dwInstanceID;
  DWORD dwSequenceID;
  ULONGLONG ullArguments;
  UINT cbExtraArgs;
} GESTUREINFO;

// not using this stuff yet
#define GC_PAN 1
#define GC_PAN_WITH_SINGLE_FINGER_VERTICALLY 2
#define GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY 4

typedef struct tagGESTURECONFIG
{
  DWORD dwID;
  DWORD dwWant;
  DWORD dwBlock;
} GESTURECONFIG;



#ifndef WINAPI
#define WINAPI
#endif

#ifndef CALLBACK
#define CALLBACK
#endif


typedef BOOL (*PROPENUMPROCEX)(HWND hwnd, const char *lpszString, HANDLE hData, LPARAM lParam);

// swell specific type
typedef HWND (*SWELL_ControlCreatorProc)(HWND parent, const char *cname, int idx, const char *classname, int style, int x, int y, int w, int h);                                           

#define DLL_PROCESS_DETACH   0    
#define DLL_PROCESS_ATTACH   1    

// if the user implements this (and links with swell-modstub[-generic], this will get called for DLL_PROCESS_[AT|DE]TACH
#ifdef __cplusplus
extern "C"  {
#endif
__attribute__ ((visibility ("default"))) BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved);
#ifdef __cplusplus
};
#endif

/*
 ** win32 specific constants
 */
#define MB_OK 0
#define MB_OKCANCEL 1
#define MB_ABORTRETRYIGNORE 2
#define MB_YESNOCANCEL 3
#define MB_YESNO 4
#define MB_RETRYCANCEL 5

#define MB_DEFBUTTON1 0
#define MB_DEFBUTTON2 0x00000100
#define MB_DEFBUTTON3 0x00000200

#define MB_ICONERROR 0
#define MB_ICONSTOP 0
#define MB_ICONINFORMATION 0
#define MB_ICONWARNING 0
#define MB_ICONQUESTION 0
#define MB_TOPMOST 0
#define MB_ICONEXCLAMATION 0

#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7

#define GW_HWNDFIRST        0
#define GW_HWNDLAST         1
#define GW_HWNDNEXT         2
#define GW_HWNDPREV         3
#define GW_OWNER            4
#define GW_CHILD            5

#define GWL_HWNDPARENT      (-25)
#define GWL_USERDATA        (-21)
#define GWL_ID              (-12)
#define GWL_STYLE           (-16) // only supported for BS_ for now I think
#define GWL_EXSTYLE         (-20)
#define GWL_WNDPROC         (-4)
#define DWL_DLGPROC         (-8)

#define SWELL_NOT_WS_VISIBLE ((int)0x80000000)
// oops these don't match real windows
#define WS_CHILDWINDOW (WS_CHILD)
#define WS_CHILD        0x40000000L
#define WS_DISABLED     0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_VISIBLE      0x02000000L // only used by GetWindowLong(GWL_STYLE) -- not settable
#define WS_CAPTION      0x00C00000L
#define WS_VSCROLL      0x00200000L
#define WS_HSCROLL      0x00100000L
#define WS_SYSMENU      0x00080000L
#define WS_THICKFRAME   0x00040000L
#define WS_GROUP        0x00020000L
#define WS_TABSTOP      0x00010000L

#define TVS_DISABLEDRAGDROP 0x10

#define WS_BORDER 0 // ignored for now

#define WM_CTLCOLORMSGBOX 0x0132
#define WM_CTLCOLOREDIT 0x0133
#define WM_CTLCOLORLISTBOX 0x0134
#define WM_CTLCOLORBTN 0x0135
#define WM_CTLCOLORDLG 0x0136
#define WM_CTLCOLORSCROLLBAR 0x0137
#define WM_CTLCOLORSTATIC 0x0138

#define CB_ADDSTRING                0x0143
#define CB_DELETESTRING             0x0144
#define CB_GETCOUNT                 0x0146
#define CB_GETCURSEL                0x0147
#define CB_GETLBTEXT                0x0148
#define CB_GETLBTEXTLEN             0x0149
#define CB_INSERTSTRING             0x014A
#define CB_RESETCONTENT             0x014B
#define CB_FINDSTRING               0x014C
#define CB_SETCURSEL                0x014E
#define CB_GETITEMDATA              0x0150
#define CB_SETITEMDATA              0x0151
#define CB_FINDSTRINGEXACT          0x0158
#define CB_INITSTORAGE              0x0161

#define LB_ADDSTRING            0x0180 // oops these don't all match real windows, todo fix (maybe)
#define LB_INSERTSTRING         0x0181
#define LB_DELETESTRING         0x0182
#define LB_GETTEXT              0x0183
#define LB_RESETCONTENT         0x0184
#define LB_SETSEL               0x0185
#define LB_SETCURSEL            0x0186
#define LB_GETSEL               0x0187
#define LB_GETCURSEL            0x0188
#define LB_GETTEXTLEN           0x018A
#define LB_GETCOUNT             0x018B
#define LB_GETSELCOUNT          0x0190
#define LB_GETITEMDATA          0x0199
#define LB_SETITEMDATA          0x019A
#define LB_FINDSTRINGEXACT      0x01A2

#define TBM_GETPOS              (WM_USER)
#define TBM_SETTIC              (WM_USER+4)
#define TBM_SETPOS              (WM_USER+5)
#define TBM_SETRANGE            (WM_USER+6)
#define TBM_SETSEL              (WM_USER+10)

#define PBM_SETRANGE            (WM_USER+1)
#define PBM_SETPOS              (WM_USER+2)
#define PBM_DELTAPOS            (WM_USER+3)

#define BM_GETCHECK        0x00F0
#define BM_SETCHECK        0x00F1
#define BM_GETIMAGE        0x00F6
#define BM_SETIMAGE        0x00F7
#define IMAGE_BITMAP 0
#define IMAGE_ICON 1

#define NM_FIRST                (0U-  0U)       // generic to all controls
#define NM_LAST                 (0U- 99U)
#define NM_CLICK                (NM_FIRST-2)    // uses NMCLICK struct
#define NM_DBLCLK               (NM_FIRST-3)
#define NM_RCLICK               (NM_FIRST-5)    // uses NMCLICK struct


#define LVSIL_STATE 1
#define LVSIL_SMALL 2

#define LVIR_BOUNDS             0
#define LVIR_ICON               1
#define LVIR_LABEL              2
#define LVIR_SELECTBOUNDS       3


#define LVHT_NOWHERE            0x0001
#define LVHT_ONITEMICON         0x0002
#define LVHT_ONITEMLABEL        0x0004
#define LVHT_ONITEMSTATEICON    0x0008
#define LVHT_ONITEM             (LVHT_ONITEMICON | LVHT_ONITEMLABEL | LVHT_ONITEMSTATEICON)

#define LVHT_ABOVE              0x0010
#define LVHT_BELOW              0x0020
#define LVHT_TORIGHT            0x0040
#define LVHT_TOLEFT             0x0080

#define LVCF_FMT  1
#define LVCF_WIDTH 2
#define LVCF_TEXT 4

#define LVCFMT_LEFT 0
#define LVCFMT_RIGHT 1
#define LVCFMT_CENTER 2

#define LVIF_TEXT 1
#define LVIF_IMAGE 2
#define LVIF_PARAM 4
#define LVIF_STATE 8

#define LVIS_SELECTED 1
#define LVIS_FOCUSED 2
#define LVNI_SELECTED 1
#define LVNI_FOCUSED 2
#define INDEXTOSTATEIMAGEMASK(x) ((x)<<16)
#define LVIS_STATEIMAGEMASK (255<<16)

#define LVN_FIRST               (0U-100U)       // listview
#define LVN_LAST                (0U-199U)
#define LVN_BEGINDRAG           (LVN_FIRST-9)
#define LVN_COLUMNCLICK         (LVN_FIRST-8)
#define LVN_ITEMCHANGED         (LVN_FIRST-1)
#define LVN_ODFINDITEM          (LVN_FIRST-52)
#define LVN_GETDISPINFO         (LVN_FIRST-50)

#define LVS_EX_GRIDLINES 0x01
#define LVS_EX_HEADERDRAGDROP 0x10
#define LVS_EX_FULLROWSELECT 0x20 // ignored for now (enabled by default on OSX)

#define HDI_FORMAT 0x4
#define HDF_SORTUP 0x0400
#define HDF_SORTDOWN 0x0200

#define TCIF_TEXT               0x0001
#define TCIF_IMAGE              0x0002
#define TCIF_PARAM              0x0008
//#define TCIF_STATE              0x0010



#define TCN_FIRST               (0U-550U)       // tab control
#define TCN_LAST                (0U-580U)
#define TCN_SELCHANGE           (TCN_FIRST - 1)


#define BS_AUTOCHECKBOX    0x00000003L
#define BS_AUTO3STATE      0x00000006L
#define BS_AUTORADIOBUTTON 0x00000009L
#define BS_OWNERDRAW       0x0000000BL
#define BS_BITMAP          0x00000080L



#define BST_CHECKED 1
#define BST_UNCHECKED 0
#define BST_INDETERMINATE 2

// note: these differ in values from their win32 counterparts, because we got them
// wrong to begin with, and we'd like to keep backwards compatability for things compiled
// against an old swell.h (and using the SWELL API via an exported mechanism, i.e. third party
// plug-ins). 
#define SW_HIDE 0
#define SW_SHOWNA 1        // 8 on win32
#define SW_SHOW 2          // 1 on win32
#define SW_SHOWMINIMIZED 3 // 2 on win32

// aliases (todo implement these as needed)
#define SW_SHOWNOACTIVATE SW_SHOWNA 
#define SW_NORMAL SW_SHOW 
#define SW_SHOWNORMAL SW_SHOW
#define SW_SHOWMAXIMIZED SW_SHOW
#define SW_SHOWDEFAULT SW_SHOWNORMAL
#define SW_RESTORE SW_SHOWNA

#define SWP_NOMOVE 1
#define SWP_NOSIZE 2
#define SWP_NOZORDER 4
#define SWP_NOACTIVATE 8
#define SWP_SHOWWINDOW 16
#define SWP_FRAMECHANGED 32
#define SWP_NOCOPYBITS 0
#define HWND_TOP        ((HWND)0)
#define HWND_BOTTOM     ((HWND)1)
#define HWND_TOPMOST    ((HWND)-1)
#define HWND_NOTOPMOST  ((HWND)-2)

// most of these are ignored, actually, but TPM_NONOTIFY and TPM_RETURNCMD are now used
#define TPM_LEFTBUTTON  0x0000L
#define TPM_RIGHTBUTTON 0x0002L
#define TPM_LEFTALIGN   0x0000L
#define TPM_CENTERALIGN 0x0004L
#define TPM_RIGHTALIGN  0x0008L
#define TPM_TOPALIGN        0x0000L
#define TPM_VCENTERALIGN    0x0010L
#define TPM_BOTTOMALIGN     0x0020L
#define TPM_HORIZONTAL      0x0000L     /* Horz alignment matters more */
#define TPM_VERTICAL        0x0040L     /* Vert alignment matters more */
#define TPM_NONOTIFY        0x0080L     /* Don't send any notification msgs */
#define TPM_RETURNCMD       0x0100L

#define MIIM_ID 1
#define MIIM_STATE 2
#define MIIM_TYPE 4
#define MIIM_SUBMENU 8
#define MIIM_DATA 16
#define MIIM_BITMAP 0x80

#define MF_ENABLED 0
#define MF_GRAYED 1
#define MF_DISABLED 2
#define MF_STRING 0
#define MF_BITMAP 4
#define MF_UNCHECKED 0
#define MF_CHECKED 8
#define MF_POPUP 0x10
#define MF_BYCOMMAND 0
#define MF_BYPOSITION 0x400
#define MF_SEPARATOR 0x800

#define MFT_STRING MF_STRING
#define MFT_BITMAP MF_BITMAP
#define MFT_SEPARATOR MF_SEPARATOR
#define MFT_RADIOCHECK 0x200

#define MFS_GRAYED (MF_GRAYED|MF_DISABLED)
#define MFS_DISABLED MFS_GRAYED
#define MFS_CHECKED MF_CHECKED
#define MFS_ENABLED MF_ENABLED
#define MFS_UNCHECKED MF_UNCHECKED

#define EN_SETFOCUS         0x0100
#define EN_KILLFOCUS        0x0200
#define EN_CHANGE           0x0300
#define STN_CLICKED         0
#define STN_DBLCLK          1
#define WM_CREATE                       0x0001
#define WM_DESTROY                      0x0002
#define WM_MOVE                         0x0003
#define WM_SIZE                         0x0005
#define WM_ACTIVATE                     0x0006
#define WM_SETREDRAW                    0x000B // implemented on macOS NSTableViews, maybe elsewhere?
#define WM_SETTEXT			0x000C // not implemented on OSX, used internally on Linux
#define WM_PAINT                        0x000F
#define WM_CLOSE                        0x0010
#define WM_ERASEBKGND                   0x0014
#define WM_SHOWWINDOW                   0x0018
#define WM_ACTIVATEAPP                  0x001C
#define WM_SETCURSOR                    0x0020
#define WM_MOUSEACTIVATE                0x0021
#define WM_GETMINMAXINFO                0x0024
#define WM_DRAWITEM                     0x002B
#define WM_SETFONT                      0x0030
#define WM_GETFONT                      0x0031
#define WM_GETOBJECT 			0x003D // implemented differently than win32 -- see virtwnd/virtwnd-nsaccessibility.mm
#define WM_COPYDATA                     0x004A
#define WM_NOTIFY                       0x004E
#define WM_CONTEXTMENU                  0x007B
#define WM_STYLECHANGED                 0x007D
#define WM_DISPLAYCHANGE                0x007E
#define WM_NCDESTROY                    0x0082
#define WM_NCCALCSIZE                   0x0083
#define WM_NCHITTEST                    0x0084
#define WM_NCPAINT                      0x0085
#define WM_NCMOUSEMOVE                  0x00A0
#define WM_NCLBUTTONDOWN                0x00A1
#define WM_NCLBUTTONUP                  0x00A2
#define WM_NCLBUTTONDBLCLK              0x00A3
#define WM_NCRBUTTONDOWN                0x00A4
#define WM_NCRBUTTONUP                  0x00A5
#define WM_NCRBUTTONDBLCLK              0x00A6
#define WM_NCMBUTTONDOWN                0x00A7
#define WM_NCMBUTTONUP                  0x00A8
#define WM_NCMBUTTONDBLCLK              0x00A9
#define WM_KEYFIRST                     0x0100
#define WM_KEYDOWN                      0x0100
#define WM_KEYUP                        0x0101
#define WM_CHAR                         0x0102
#define WM_DEADCHAR                     0x0103
#define WM_SYSKEYDOWN                   0x0104
#define WM_SYSKEYUP                     0x0105
#define WM_SYSCHAR                      0x0106
#define WM_SYSDEADCHAR                  0x0107
#define WM_KEYLAST                      0x0108
#define WM_INITDIALOG                   0x0110
#define WM_COMMAND                      0x0111
#define WM_SYSCOMMAND                   0x0112
#define SC_CLOSE        0xF060
#define WM_TIMER                        0x0113
#define WM_HSCROLL                      0x0114
#define WM_VSCROLL                      0x0115
#define WM_INITMENUPOPUP                0x0117
#define WM_GESTURE 			0x0119
#define WM_MOUSEFIRST                   0x0200
#define WM_MOUSEMOVE                    0x0200
#define WM_LBUTTONDOWN                  0x0201
#define WM_LBUTTONUP                    0x0202
#define WM_LBUTTONDBLCLK                0x0203
#define WM_RBUTTONDOWN                  0x0204
#define WM_RBUTTONUP                    0x0205
#define WM_RBUTTONDBLCLK                0x0206
#define WM_MBUTTONDOWN                  0x0207
#define WM_MBUTTONUP                    0x0208
#define WM_MBUTTONDBLCLK                0x0209
#define WM_MOUSEWHEEL                   0x020A
#define WM_MOUSEHWHEEL                  0x020E
#define WM_MOUSELAST                    0x020A
#define WM_CAPTURECHANGED               0x0215
#define WM_DROPFILES                    0x0233
#define WM_USER                         0x0400

#define HTCAPTION 2
#define HTBOTTOMRIGHT 17

#define     WA_INACTIVE     0
#define     WA_ACTIVE       1
#define     WA_CLICKACTIVE  2

#define BN_CLICKED 0

#define LBN_SELCHANGE       1
#define LBN_DBLCLK          2
#define LB_ERR (-1)

#define CBN_SELCHANGE       1
#define CBN_EDITCHANGE      5
#define CBN_DROPDOWN        7
#define CBN_CLOSEUP         8
#define CB_ERR (-1)

#define EM_GETSEL               0xF0B0
#define EM_SETSEL               0xF0B1
#define EM_SCROLL               0xF0B5
#define EM_REPLACESEL           0xF0C2
#define EM_SETPASSWORDCHAR      0xF0CC

#define SB_HORZ             0
#define SB_VERT             1
#define SB_CTL              2
#define SB_BOTH             3

#define SB_LINEUP           0
#define SB_LINELEFT         0
#define SB_LINEDOWN         1
#define SB_LINERIGHT        1
#define SB_PAGEUP           2
#define SB_PAGELEFT         2
#define SB_PAGEDOWN         3
#define SB_PAGERIGHT        3
#define SB_THUMBPOSITION    4
#define SB_THUMBTRACK       5
#define SB_TOP              6
#define SB_LEFT             6
#define SB_BOTTOM           7
#define SB_RIGHT            7
#define SB_ENDSCROLL        8

#define DFCS_SCROLLUP           0x0000
#define DFCS_SCROLLDOWN         0x0001
#define DFCS_SCROLLLEFT         0x0002
#define DFCS_SCROLLRIGHT        0x0003
#define DFCS_SCROLLCOMBOBOX     0x0005
#define DFCS_SCROLLSIZEGRIP     0x0008
#define DFCS_SCROLLSIZEGRIPRIGHT 0x0010

#define DFCS_INACTIVE           0x0100
#define DFCS_PUSHED             0x0200
#define DFCS_CHECKED            0x0400
#define DFCS_FLAT               0x4000

#define DFCS_BUTTONPUSH         0x0010

#define DFC_SCROLL              3
#define DFC_BUTTON              4

#define ESB_ENABLE_BOTH     0x0000
#define ESB_DISABLE_BOTH    0x0003

#define ESB_DISABLE_LEFT    0x0001
#define ESB_DISABLE_RIGHT   0x0002

#define ESB_DISABLE_UP      0x0001
#define ESB_DISABLE_DOWN    0x0002

#define BDR_RAISEDOUTER 0x0001
#define BDR_SUNKENOUTER 0x0002
#define BDR_RAISEDINNER 0x0004
#define BDR_SUNKENINNER 0x0008

#define BDR_OUTER       0x0003
#define BDR_INNER       0x000c

#define EDGE_RAISED     (BDR_RAISEDOUTER | BDR_RAISEDINNER)
#define EDGE_SUNKEN     (BDR_SUNKENOUTER | BDR_SUNKENINNER)
#define EDGE_ETCHED     (BDR_SUNKENOUTER | BDR_RAISEDINNER)
#define EDGE_BUMP       (BDR_RAISEDOUTER | BDR_SUNKENINNER)

#define BF_ADJUST       0x2000
#define BF_FLAT         0x4000
#define BF_LEFT         0x0001
#define BF_TOP          0x0002
#define BF_RIGHT        0x0004
#define BF_BOTTOM       0x0008
#define BF_RECT         (BF_LEFT | BF_TOP | BF_RIGHT | BF_BOTTOM)

#define PATCOPY             (DWORD)0x00F00021

#define HTHSCROLL           6
#define HTVSCROLL           7

#define WS_EX_LEFTSCROLLBAR     0x00004000L
#define WS_EX_ACCEPTFILES       0x00000010L

#define SIF_RANGE           0x0001
#define SIF_PAGE            0x0002
#define SIF_POS             0x0004
#define SIF_DISABLENOSCROLL 0x0008
#define SIF_TRACKPOS        0x0010
#define SIF_ALL             (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)

#define SIZE_RESTORED       0
#define SIZE_MINIMIZED      1
#define SIZE_MAXIMIZED      2
#define SIZE_MAXSHOW        3
#define SIZE_MAXHIDE        4


#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE(x) ((const char *)(UINT_PTR)(x))         
#endif                

#ifdef FSHIFT
#undef FSHIFT
#endif

#define FVIRTKEY  1
#define FSHIFT    0x04
#define FCONTROL  0x08
#define FALT      0x10
#define FLWIN     0x20


#define VK_LBUTTON        0x01
#define VK_RBUTTON        0x02
#define VK_MBUTTON        0x04

#define VK_BACK           0x08
#define VK_TAB            0x09

#define VK_CLEAR          0x0C
#define VK_RETURN         0x0D

#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_MENU           0x12
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14

#define VK_ESCAPE         0x1B

#define VK_SPACE          0x20
#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SELECT         0x29
#define VK_PRINT          0x2A
#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_HELP           0x2F

#define VK_LWIN           0x5B

#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B
#define VK_SEPARATOR      0x6C
#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B
#define VK_F13            0x7C
#define VK_F14            0x7D
#define VK_F15            0x7E
#define VK_F16            0x7F
#define VK_F17            0x80
#define VK_F18            0x81
#define VK_F19            0x82
#define VK_F20            0x83
#define VK_F21            0x84
#define VK_F22            0x85
#define VK_F23            0x86
#define VK_F24            0x87

#define VK_NUMLOCK        0x90
#define VK_SCROLL         0x91

// these should probably not be used (wParam is not set in WM_LBUTTONDOWN/WM_MOUSEMOVE etc)
#define MK_LBUTTON        0x01 
#define MK_RBUTTON        0x02
#define MK_MBUTTON        0x10

#define IDC_SIZENESW MAKEINTRESOURCE(32643)
#define IDC_SIZENWSE MAKEINTRESOURCE(32642)
#define IDC_IBEAM MAKEINTRESOURCE(32513)
#define IDC_UPARROW MAKEINTRESOURCE(32516)
#define IDC_NO MAKEINTRESOURCE(32648)
#define IDC_SIZEALL MAKEINTRESOURCE(32646)
#define IDC_SIZENS MAKEINTRESOURCE(32645)
#define IDC_SIZEWE MAKEINTRESOURCE(32644)
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_HAND MAKEINTRESOURCE(32649)



#define COLOR_3DSHADOW 0
#define COLOR_3DHILIGHT 1
#define COLOR_3DFACE 2
#define COLOR_BTNTEXT 3
#define COLOR_WINDOW 4
#define COLOR_SCROLLBAR 5
#define COLOR_3DDKSHADOW 6
#define COLOR_BTNFACE 7
#define COLOR_INFOBK 8
#define COLOR_INFOTEXT 9

#define SRCCOPY 0
#define SRCCOPY_USEALPHACHAN 0xdeadbeef
#define PS_SOLID 0

#define DT_TOP 0
#define DT_LEFT 0
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_BOTTOM 8
#define DT_WORDBREAK 0x10
#define DT_SINGLELINE 0x20
#define DT_NOCLIP 0x100
#define DT_CALCRECT 0x400
#define DT_NOPREFIX 0x800
#define DT_END_ELLIPSIS 0x8000

#define FW_DONTCARE         0
#define FW_THIN             100
#define FW_EXTRALIGHT       200
#define FW_LIGHT            300
#define FW_NORMAL           400
#define FW_MEDIUM           500
#define FW_SEMIBOLD         600
#define FW_BOLD             700
#define FW_EXTRABOLD        800
#define FW_HEAVY            900

#define FW_ULTRALIGHT       FW_EXTRALIGHT
#define FW_REGULAR          FW_NORMAL
#define FW_DEMIBOLD         FW_SEMIBOLD
#define FW_ULTRABOLD        FW_EXTRABOLD
#define FW_BLACK            FW_HEAVY

#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define DRAFT_QUALITY 1
#define PROOF_QUALITY 2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY 4
#define DEFAULT_PITCH 0
#define DEFAULT_CHARSET 0
#define ANSI_CHARSET 0
#define TRANSPARENT 0
#define OPAQUE 1

#define NULL_PEN 1
#define NULL_BRUSH 2

#define GGI_MARK_NONEXISTING_GLYPHS 1

#define GMEM_ZEROINIT 1
#define GMEM_FIXED 0
#define GMEM_MOVEABLE 0
#define GMEM_DDESHARE 0
#define GMEM_DISCARDABLE 0
#define GMEM_SHARE 0
#define GMEM_LOWER 0
#define GHND (GMEM_MOVEABLE|GM_ZEROINIT)
#define GPTR (GMEM_FIXED|GMEM_ZEROINIT)

#define CF_TEXT (1)
#define CF_HDROP (2)

#define _MCW_RC         0x00000300              /* Rounding Control */
#define _RC_NEAR        0x00000000              /*   near */
#define _RC_DOWN        0x00000100              /*   down */
#define _RC_UP          0x00000200              /*   up */
#define _RC_CHOP        0x00000300              /*   chop */


extern struct SWELL_DialogResourceIndex *SWELL_curmodule_dialogresource_head;
extern struct SWELL_MenuResourceIndex *SWELL_curmodule_menuresource_head;

#define HTNOWHERE           0
#define HTCLIENT            1
#define HTMENU              5
#define HTHSCROLL           6
#define HTVSCROLL           7

#define SM_CXSCREEN             0
#define SM_CYSCREEN             1
#define SM_CXVSCROLL            2
#define SM_CYHSCROLL            3
#define SM_CYVSCROLL            20
#define SM_CXHSCROLL            21


#if 0 // these are disabled until implemented

#define SM_CYCAPTION            4
#define SM_CXBORDER             5
#define SM_CYBORDER             6
#define SM_CXDLGFRAME           7
#define SM_CYDLGFRAME           8
#define SM_CYVTHUMB             9
#define SM_CXHTHUMB             10
#define SM_CXICON               11
#define SM_CYICON               12
#define SM_CXCURSOR             13
#define SM_CYCURSOR             14
#define SM_CYMENU               15
#define SM_CXFULLSCREEN         16
#define SM_CYFULLSCREEN         17
#define SM_CYKANJIWINDOW        18
#define SM_MOUSEPRESENT         19
#define SM_DEBUG                22
#define SM_SWAPBUTTON           23
#define SM_CXMIN                28
#define SM_CYMIN                29
#define SM_CXSIZE               30
#define SM_CYSIZE               31
#define SM_CXFRAME              32
#define SM_CYFRAME              33
#define SM_CXMINTRACK           34
#define SM_CYMINTRACK           35
#define SM_CXDOUBLECLK          36
#define SM_CYDOUBLECLK          37
#define SM_CXICONSPACING        38
#define SM_CYICONSPACING        39

#endif // unimplemented system metrics


#define THREAD_BASE_PRIORITY_LOWRT  15
#define THREAD_BASE_PRIORITY_MAX    2
#define THREAD_BASE_PRIORITY_MIN    -2
#define THREAD_BASE_PRIORITY_IDLE   -15
#define THREAD_PRIORITY_LOWEST          THREAD_BASE_PRIORITY_MIN
#define THREAD_PRIORITY_BELOW_NORMAL    (THREAD_PRIORITY_LOWEST+1)
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_HIGHEST         THREAD_BASE_PRIORITY_MAX
#define THREAD_PRIORITY_ABOVE_NORMAL    (THREAD_PRIORITY_HIGHEST-1)
#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT
#define THREAD_PRIORITY_IDLE            THREAD_BASE_PRIORITY_IDLE



#define WAIT_OBJECT_0       (0 )
#define WAIT_TIMEOUT                        (0x00000102L)
#define WAIT_FAILED (DWORD)0xFFFFFFFF
#define INFINITE            0xFFFFFFFF


#define FR_PRIVATE 1 // AddFontResourceEx()

typedef struct _ICONINFO
{
  BOOL fIcon;
  DWORD xHotspot;
  DWORD yHotspot;
  HBITMAP hbmMask;
  HBITMAP hbmColor;
} ICONINFO, *PICONINFO;

typedef struct _COPYDATASTRUCT
{
  ULONG_PTR dwData;
  DWORD     cbData;
  PVOID     lpData;
} COPYDATASTRUCT, *PCOPYDATASTRUCT;


#endif //_WDL_SWELL_H_TYPES_DEFINED_
