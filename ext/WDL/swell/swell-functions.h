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

#ifndef _WDL_SWELL_H_API_DEFINED_
#define _WDL_SWELL_H_API_DEFINED_

////////////////////////////////////////////
/////////// FUNCTIONS
////////////////////////////////////////////

#ifndef SWELL_API_DEFINE


#ifdef SWELL_PROVIDED_BY_APP
  #ifdef __cplusplus
    #define SWELL_API_DEFINE(ret,func,parms) extern "C" ret (*func)parms;
  #else
    #define SWELL_API_DEFINE(ret,func,parms) extern ret (*func)parms;
  #endif
#else
#define SWELL_API_DEFINE(ret,func,parms) ret func parms ;
#endif
#endif

//   when adding APIs, add it using:
//      SWELL_API_DEFINE(void, function_name, (int parm, int parm2))
//   rather than:
//      void function_name(int parm, int parm2);

/* 
** lstrcpyn: this is provided because strncpy is braindead (filling with zeroes, and not 
** NULL terminating if the destination buffer is too short? ASKING for trouble..)
** lstrpcyn always null terminates the string and doesnt fill anything extra.
**
** note: wdlcstring.h defines lstrcpyn_safe(), which is preferred on win32 because of 
** exception handling behavior.
*/
SWELL_API_DEFINE(char *, lstrcpyn, (char *dest, const char *src, int l))


/*
** MulDiv(): (parm1*parm2)/parm3
** Implemented using long longs.
*/
SWELL_API_DEFINE(int, MulDiv, (int, int, int))


/*
** Sleep() sleeps for specified milliseconds. This maps to usleep, with a ms value of 0 
** usleeping for 100 microseconds. 
*/
SWELL_API_DEFINE(void, Sleep,(int ms))

/*
** GetTickCount() and timeGetTime() give you ms level timings via gettimeofday() or mach_getabsolutetime() or clock_gettime()
** 
** NOTE: This doesn't map to time since system start (like in win32), so a wrap around 
** is slightly more likely (i.e. even if you booted your system an hour ago it could happen).
*/
SWELL_API_DEFINE(DWORD, GetTickCount,())
#ifndef timeGetTime
#define timeGetTime() GetTickCount()
#endif

/*
** GetFileTime() gets the file time of a file (FILE *), and converts it to the Windows time.
**
** NOTE: while it returns a 64 bit time value, it is only accurate to the second since thats 
** what fstat() returns. Takes an int filedes rather than a HANDLE.
*/
SWELL_API_DEFINE(BOOL, GetFileTime,(int filedes, FILETIME *lpCreationTime, FILETIME *lpLastAccessTime, FILETIME *lpLastWriteTime))

/*
** *PrivateProfileString/Int():
** These are mostly thread-safe, mostly inter-process safe, and mostly module safe 
** (i.e. writes from other modules) should be synchronized).
**
** NOTES:
**   the filename used MUST be the full filename, unlike on Windows where files without paths go to
**   C:/Windows, here they will be opened in the current directory.
**
**   You can pass an empty string for filename to use ~/.libSwell.ini (that can be overridden by the app)
**
**   It's probably not a good idea to push your luck with simultaneous writes from multiple
**   modules in different threads/processes, but in theory it should work.
*/
SWELL_API_DEFINE(BOOL, WritePrivateProfileString, (const char *appname, const char *keyname, const char *val, const char *fn))
SWELL_API_DEFINE(DWORD, GetPrivateProfileString, (const char *appname, const char *keyname, const char *def, char *ret, int retsize, const char *fn))
SWELL_API_DEFINE(int, GetPrivateProfileInt,(const char *appname, const char *keyname, int def, const char *fn))
SWELL_API_DEFINE(BOOL, GetPrivateProfileStruct,(const char *appname, const char *keyname, void *buf, int bufsz, const char *fn))
SWELL_API_DEFINE(BOOL, WritePrivateProfileStruct,(const char *appname, const char *keyname, const void *buf, int bufsz, const char *fn))
SWELL_API_DEFINE(BOOL, WritePrivateProfileSection, (const char *appname, const char *strings, const char *fn))
SWELL_API_DEFINE(DWORD, GetPrivateProfileSection, (const char *appname, char *strout, DWORD strout_len, const char *fn))

/*
** GetModuleFileName()
** Can pass NULL (exe filename) or a hInstance from DllMain or LoadLibrary
*/
SWELL_API_DEFINE(DWORD, GetModuleFileName,(HINSTANCE hInst, char *fn, DWORD nSize))

#ifdef SWELL_TARGET_OSX
/*
** SWELL_CStringToCFString(): Creates a CFString/NSString * from a C string. This is mostly 
** used internally but you may wish to use it as well (though none of the SWELL APIs take 
** CFString/NSString.
*/
SWELL_API_DEFINE(void *,SWELL_CStringToCFString,(const char *str))
SWELL_API_DEFINE(void, SWELL_CFStringToCString, (const void *str, char *buf, int buflen))
#endif


#ifdef PtInRect
#undef PtInRect
// #define funkiness because some Mac system headers define PtInRect as well.
#endif
#define PtInRect(r,p) SWELL_PtInRect(r,p)
SWELL_API_DEFINE(BOOL, SWELL_PtInRect,(const RECT *r, POINT p))

/*
** ShellExecute(): 
** notes: 
**   action is ignored
**   content1 can be a http:// or https:// URL
**   content1 can be notepad/notepad.exe (maps to xdg-open or TextEdit.app) w/ content2 as a document
**   content1 can be explorer.exe (optionally with /select) (maps to open finder or xdg-open)
**   otherwise content1 can be an app w/ parameters in content2
*/
SWELL_API_DEFINE(BOOL, ShellExecute,(HWND hwndDlg, const char *action,  const char *content1, const char *content2, const char *content3, int blah))

SWELL_API_DEFINE(int, MessageBox,(HWND hwndParent, const char *text, const char *caption, int type))


/*
** GetOpenFileName() / GetSaveFileName() 
** These are a different API because we didnt feel like reeimplimenting the full API.
** Extlist is something similar you'd pass getopenfilename, 
** initialdir and initialfile are optional (and NULL means not set).
*/

// free() the result of this, if non-NULL.
// if allowmul is set, the multiple files are specified the same way GetOpenFileName() returns.
SWELL_API_DEFINE(char *,BrowseForFiles,(const char *text, const char *initialdir, 
					const char *initialfile, bool allowmul, const char *extlist)) 

// returns TRUE if file was chosen.         
SWELL_API_DEFINE(bool, BrowseForSaveFile,(const char *text, const char *initialdir, const char *initialfile, const char *extlist,
			char *fn, int fnsize))

// returns TRUE if path was chosen.
SWELL_API_DEFINE(bool, BrowseForDirectory,(const char *text, const char *initialdir, char *fn, int fnsize))

// can use this before calling BrowseForFiles or BrowseForSaveFile to use a template dialog
SWELL_API_DEFINE(void,BrowseFile_SetTemplate,(const char *dlgid, DLGPROC dlgProc, struct SWELL_DialogResourceIndex *reshead))


// Note that window functions are generally NOT threadsafe.
// on macOS: all of these treat HWND as NSView and/or NSWindow (usually smartish about it)

/* 
 * GetDlgItem() notes:
** macOS: GetDlgItem(hwnd,0) returns hwnd if hwnd is a NSView, or the contentview if hwnd is a NSWindow.
** macOS: note that this GetDlgItem will actually search a view hierarchy for the tagged view (win32 or -generic will only search immediate children)
*/
SWELL_API_DEFINE(HWND, GetDlgItem,(HWND, int))


SWELL_API_DEFINE(void, ShowWindow,(HWND, int))

SWELL_API_DEFINE(void, DestroyWindow,(HWND hwnd)) 

SWELL_API_DEFINE(BOOL, SWELL_GetGestureInfo, (LPARAM lParam, GESTUREINFO* gi))

SWELL_API_DEFINE(void, SWELL_HideApp,())

/*
** These should all work like their Win32 versions, though if idx=0 it gets/sets the
** value for the window. 
**
** macOS: SetDlgItemText() for an edit control does NOT send a WM_COMMAND notification (win32 and -generic do)
*/
SWELL_API_DEFINE(BOOL, SetDlgItemText,(HWND, int idx, const char *text))
SWELL_API_DEFINE(BOOL, SetDlgItemInt,(HWND, int idx, int val, int issigned))
SWELL_API_DEFINE(int, GetDlgItemInt,(HWND, int idx, BOOL *translated, int issigned))
SWELL_API_DEFINE(BOOL, GetDlgItemText,(HWND, int idx, char *text, int textlen))

#ifndef GetWindowText
#define GetWindowText(hwnd,text,textlen) GetDlgItemText(hwnd,0,text,textlen)
#define SetWindowText(hwnd,text) SetDlgItemText(hwnd,0,text)
#endif


SWELL_API_DEFINE(void, CheckDlgButton,(HWND hwnd, int idx, int check))
SWELL_API_DEFINE(int, IsDlgButtonChecked,(HWND hwnd, int idx))
SWELL_API_DEFINE(void, EnableWindow,(HWND hwnd, int enable))
SWELL_API_DEFINE(void, SetFocus,(HWND hwnd))
SWELL_API_DEFINE(HWND, GetFocus,())
SWELL_API_DEFINE(void, SetForegroundWindow,(HWND hwnd))
SWELL_API_DEFINE(HWND, GetForegroundWindow,())
#ifndef GetActiveWindow
#define GetActiveWindow() GetForegroundWindow()
#endif
#ifndef SetActiveWindow
#define SetActiveWindow(x) SetForegroundWindow(x)
#endif

/*
** macOS: note that any HWND that returns YES to swellCapChangeNotify should do the following on 
** destroy or dealloc: if (GetCapture()==(HWND)self) ReleaseCapture(); Failure to do so
** can cause a dealloc'd window to get messages sent to it.
*/
SWELL_API_DEFINE(HWND, SetCapture,(HWND hwnd))
SWELL_API_DEFINE(HWND, GetCapture,())
SWELL_API_DEFINE(void, ReleaseCapture,())

/*
** IsChild()
** macOS: hwndChild must be a NSView, hwndParent can be a NSWindow or NSView.  NSWindow level ownership/children are not detected.
*/
SWELL_API_DEFINE(int, IsChild,(HWND hwndParent, HWND hwndChild))


SWELL_API_DEFINE(HWND, GetParent,(HWND hwnd))

/*
** SetParent()
** macOS: hwnd must be a NSView, newPar can be either NSView or NSWindow.
*/
SWELL_API_DEFINE(HWND, SetParent,(HWND hwnd, HWND newPar))

SWELL_API_DEFINE(HWND, GetWindow,(HWND hwnd, int what))

SWELL_API_DEFINE(BOOL, EnumWindows, (BOOL (*proc)(HWND, LPARAM), LPARAM lp))

SWELL_API_DEFINE(HWND,FindWindowEx,(HWND par, HWND lastw, const char *classname, const char *title))


/*
** macOS note: common win32 code like this:
**   RECT r;
**   GetWindowRect(hwnd,&r); 
**   ScreenToClient(otherhwnd,(LPPOINT)&r);
**   ScreenToClient(otherhwnd,((LPPOINT)&r)+1);
** does work, however be aware that in certain instances r.bottom may be less 
** than r.top, due to flipped coordinates. SetWindowPos and other functions 
** handle negative heights gracefully, and you should too.
**
** GetWindowContentViewRect gets the rectangle of the content view (pre-NCCALCSIZE etc)
*/
SWELL_API_DEFINE(void, ClientToScreen,(HWND hwnd, POINT *p))
SWELL_API_DEFINE(void, ScreenToClient,(HWND hwnd, POINT *p))
SWELL_API_DEFINE(bool, GetWindowRect,(HWND hwnd, RECT *r))
SWELL_API_DEFINE(void, GetWindowContentViewRect, (HWND hwnd, RECT *r)) 
SWELL_API_DEFINE(void, GetClientRect,(HWND hwnd, RECT *r))
SWELL_API_DEFINE(HWND, WindowFromPoint,(POINT p))
SWELL_API_DEFINE(BOOL, WinOffsetRect, (LPRECT lprc, int dx, int dy))
SWELL_API_DEFINE(BOOL, WinSetRect, (LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom))
SWELL_API_DEFINE(void,WinUnionRect,(RECT *out, const RECT *in1, const RECT *in2))
SWELL_API_DEFINE(int,WinIntersectRect,(RECT *out, const RECT *in1, const RECT *in2))

SWELL_API_DEFINE(void, SetWindowPos,(HWND hwnd, HWND unused, int x, int y, int cx, int cy, int flags))

SWELL_API_DEFINE(int, SWELL_SetWindowLevel, (HWND hwnd, int newlevel))

SWELL_API_DEFINE(BOOL,InvalidateRect,(HWND hwnd, const RECT *r, int eraseBk))

SWELL_API_DEFINE(void,UpdateWindow,(HWND hwnd))


/*
** GetWindowLong()/SetWindowLong()
**
** macOS:
**   GWL_ID is supported for all objects that support the 'tag'/'setTag' methods,
**   which would be controls and SWELL created windows/dialogs/controls.
**
**   GWL_USERDATA is supported by SWELL created windows/dialogs/controls, using 
**   (int)getSwellUserData and setSwellUserData:(int).
** 
**   GWL_WNDPROC is supported by SWELL created windows/dialogs/controls, using 
**   (int)getSwellWindowProc and setSwellWindowProc:(int).
**
**   DWL_DLGPROC is supported by SWELL-created dialogs now (it might work in windows/controls but isnt recommended)
**
**   GWL_STYLE is only supported for NSButton. Currently the only flags supported are
**   BS_AUTO3STATE (BS_AUTOCHECKBOX is returned but also ignored).
**
**   indices of >= 0 and < 128 (32 integers) are supported for SWELL created 
**   windows/dialogs/controls, via (int)getSwellExtraData:(int)idx and 
**   setSwellExtraData:(int)idx value:(int)val . 
**
** generic: indices of >= 0 && < 64*sizeof(INT_PTR) are supported
*/
SWELL_API_DEFINE(LONG_PTR, GetWindowLong,(HWND hwnd, int idx))
SWELL_API_DEFINE(LONG_PTR, SetWindowLong,(HWND hwnd, int idx, LONG_PTR val))


SWELL_API_DEFINE(BOOL, ScrollWindow, (HWND hwnd, int xamt, int yamt, const RECT *lpRect, const RECT *lpClipRect))

/* 
** GetProp() SetProp() RemoveProp() EnumPropsEx()
** Free your props otherwise they will leak.
** Restriction on what you can do in the PROPENUMPROCEX is similar to win32 
** (you can remove only the called prop, and can't add props within it).
** if the prop name is < (void *)65536 then it is treated as a short identifier.
*/
SWELL_API_DEFINE(int, EnumPropsEx,(HWND, PROPENUMPROCEX, LPARAM))
SWELL_API_DEFINE(HANDLE, GetProp, (HWND, const char *))
SWELL_API_DEFINE(BOOL, SetProp, (HWND, const char *, HANDLE))
SWELL_API_DEFINE(HANDLE, RemoveProp, (HWND, const char *))
                

/*
** IsWindowVisible()
** macOS: 
**   if hwnd is a NSView, returns !isHiddenOrHasHiddenAncestor
**   if hwnd is a NSWindow returns isVisible
**   otherwise returns TRUE if non-null hwnd
*/
SWELL_API_DEFINE(bool, IsWindowVisible,(HWND hwnd))

// IsWindow()
// probably best avoided.
// macOS: very costly (compared to win32) -- enumerates all windows, searches for hwnd
// generic: may not be implemented 
SWELL_API_DEFINE(bool, IsWindow, (HWND hwnd)) 


/*
** SetTimer/KillTimer():
** Notes:
** The timer API may be threadsafe though it is highly untested. It is safest to only set 
** timers from the main thread.
**
** Kill all timers for a window using KillTimer(hwnd,-1);
**
** macOS: Note also that the mechanism for sending timers is SWELL_Timer:(id).
** You MUST kill all timers for a window before destroying it. Note that SWELL created 
** windows/dialogs/controls automatically do this, but if you use SetTimer() on a NSView *
** or NSWindow * directly, then you should kill all timers in -dealloc.
*/
SWELL_API_DEFINE(UINT_PTR, SetTimer,(HWND hwnd, UINT_PTR timerid, UINT rate, TIMERPROC tProc))
SWELL_API_DEFINE(BOOL, KillTimer,(HWND hwnd, UINT_PTR timerid))

#ifdef SWELL_TARGET_OSX
/*
** SendMessage can/should now be used with CB_* etc.
** macOS: Combo boxes may be implemented using a NSComboBox or NSPopUpButton depending on the style.
*/
SWELL_API_DEFINE(int, SWELL_CB_AddString,(HWND hwnd, int idx, const char *str))
SWELL_API_DEFINE(void, SWELL_CB_SetCurSel,(HWND hwnd, int idx, int sel))
SWELL_API_DEFINE(int, SWELL_CB_GetCurSel,(HWND hwnd, int idx))
SWELL_API_DEFINE(int, SWELL_CB_GetNumItems,(HWND hwnd, int idx))
SWELL_API_DEFINE(void, SWELL_CB_SetItemData,(HWND hwnd, int idx, int item, LONG_PTR data))
SWELL_API_DEFINE(LONG_PTR, SWELL_CB_GetItemData,(HWND hwnd, int idx, int item))
SWELL_API_DEFINE(void, SWELL_CB_Empty,(HWND hwnd, int idx))
SWELL_API_DEFINE(int, SWELL_CB_InsertString,(HWND hwnd, int idx, int pos, const char *str))
SWELL_API_DEFINE(int, SWELL_CB_GetItemText,(HWND hwnd, int idx, int item, char *buf, int bufsz))
SWELL_API_DEFINE(void, SWELL_CB_DeleteString,(HWND hwnd, int idx, int wh))
SWELL_API_DEFINE(int, SWELL_CB_FindString,(HWND hwnd, int idx, int startAfter, const char *str, bool exact))


/*
** Trackbar API
** You can/should now use SendMessage with TBM_* instead.
*/
SWELL_API_DEFINE(void, SWELL_TB_SetPos,(HWND hwnd, int idx, int pos))
SWELL_API_DEFINE(void, SWELL_TB_SetRange,(HWND hwnd, int idx, int low, int hi))
SWELL_API_DEFINE(int, SWELL_TB_GetPos,(HWND hwnd, int idx))
SWELL_API_DEFINE(void, SWELL_TB_SetTic,(HWND hwnd, int idx, int pos))


#endif

/*
** ListViews -- in owner data mode only LVN_GETDISPINFO is required (LVN_ODFINDITEM is never sent)
*/
SWELL_API_DEFINE(void, ListView_SetExtendedListViewStyleEx,(HWND h, int mask, int style))
SWELL_API_DEFINE(void, ListView_InsertColumn,(HWND h, int pos, const LVCOLUMN *lvc))
SWELL_API_DEFINE(bool, ListView_DeleteColumn,(HWND h, int pos))
SWELL_API_DEFINE(void, ListView_SetColumn,(HWND h, int pos, const LVCOLUMN *lvc))
SWELL_API_DEFINE(int, ListView_GetColumnWidth,(HWND h, int pos))
SWELL_API_DEFINE(int, ListView_InsertItem,(HWND h, const LVITEM *item))
SWELL_API_DEFINE(void, ListView_SetItemText,(HWND h, int ipos, int cpos, const char *txt))
SWELL_API_DEFINE(bool, ListView_SetItem,(HWND h, LVITEM *item))
SWELL_API_DEFINE(int, ListView_GetNextItem,(HWND h, int istart, int flags))
SWELL_API_DEFINE(bool, ListView_GetItem,(HWND h, LVITEM *item))
SWELL_API_DEFINE(int, ListView_GetItemState,(HWND h, int ipos, UINT mask))
SWELL_API_DEFINE(void, ListView_DeleteItem,(HWND h, int ipos))
SWELL_API_DEFINE(void, ListView_DeleteAllItems,(HWND h))
SWELL_API_DEFINE(int, ListView_GetSelectedCount,(HWND h))
SWELL_API_DEFINE(int, ListView_GetItemCount,(HWND h))
SWELL_API_DEFINE(int, ListView_GetSelectionMark,(HWND h))
SWELL_API_DEFINE(void, ListView_SetColumnWidth,(HWND h, int colpos, int wid))
SWELL_API_DEFINE(bool, ListView_SetItemState,(HWND h, int item, UINT state, UINT statemask))
SWELL_API_DEFINE(void, ListView_RedrawItems,(HWND h, int startitem, int enditem))
SWELL_API_DEFINE(void, ListView_SetItemCount,(HWND h, int cnt))
#ifdef ListView_SetItemCountEx
#undef ListView_SetItemCountEx
#endif
#define ListView_SetItemCountEx(list,cnt,flags) ListView_SetItemCount(list,cnt)

SWELL_API_DEFINE(void, ListView_EnsureVisible,(HWND h, int i, BOOL pok))
SWELL_API_DEFINE(bool, ListView_GetSubItemRect,(HWND h, int item, int subitem, int code, RECT *r))
SWELL_API_DEFINE(void, ListView_SetImageList,(HWND h, HIMAGELIST imagelist, int which)) 
SWELL_API_DEFINE(int, ListView_HitTest,(HWND h, LVHITTESTINFO *pinf))
SWELL_API_DEFINE(int, ListView_SubItemHitTest,(HWND h, LVHITTESTINFO *pinf))
SWELL_API_DEFINE(void, ListView_GetItemText,(HWND hwnd, int item, int subitem, char *text, int textmax))
SWELL_API_DEFINE(void, ListView_SortItems,(HWND hwnd, PFNLVCOMPARE compf, LPARAM parm))
SWELL_API_DEFINE(bool, ListView_GetItemRect,(HWND h, int item, RECT *r, int code))
SWELL_API_DEFINE(bool, ListView_Scroll,(HWND h, int xscroll, int yscroll))
SWELL_API_DEFINE(int, ListView_GetTopIndex,(HWND h))
SWELL_API_DEFINE(int, ListView_GetCountPerPage,(HWND h))
SWELL_API_DEFINE(BOOL, ListView_SetColumnOrderArray,(HWND h, int cnt, int* arr))
SWELL_API_DEFINE(BOOL, ListView_GetColumnOrderArray,(HWND h, int cnt, int* arr))
SWELL_API_DEFINE(HWND, ListView_GetHeader,(HWND h))
SWELL_API_DEFINE(int, Header_GetItemCount,(HWND h))
SWELL_API_DEFINE(BOOL, Header_GetItem,(HWND h, int col, HDITEM* hi))
SWELL_API_DEFINE(BOOL, Header_SetItem,(HWND h, int col, HDITEM* hi))

SWELL_API_DEFINE(int, SWELL_GetListViewHeaderHeight, (HWND h))

#ifndef ImageList_Create
#define ImageList_Create(x,y,a,b,c) ImageList_CreateEx();
#endif
SWELL_API_DEFINE(HIMAGELIST, ImageList_CreateEx,())
SWELL_API_DEFINE(BOOL, ImageList_Remove, (HIMAGELIST list, int idx))
SWELL_API_DEFINE(int, ImageList_ReplaceIcon,(HIMAGELIST list, int offset, HICON image))
SWELL_API_DEFINE(int, ImageList_Add,(HIMAGELIST list, HBITMAP image, HBITMAP mask))
SWELL_API_DEFINE(void, ImageList_Destroy, (HIMAGELIST))
/*
** TabCtrl api. 
*/
SWELL_API_DEFINE(int, TabCtrl_GetItemCount,(HWND hwnd))
SWELL_API_DEFINE(BOOL, TabCtrl_DeleteItem,(HWND hwnd, int idx))
SWELL_API_DEFINE(int, TabCtrl_InsertItem,(HWND hwnd, int idx, TCITEM *item))
SWELL_API_DEFINE(int, TabCtrl_SetCurSel,(HWND hwnd, int idx))
SWELL_API_DEFINE(int, TabCtrl_GetCurSel,(HWND hwnd))
SWELL_API_DEFINE(BOOL, TabCtrl_AdjustRect, (HWND hwnd, BOOL fLarger, RECT *r))

/*
** TreeView
*/

SWELL_API_DEFINE(HTREEITEM, TreeView_InsertItem, (HWND hwnd, TV_INSERTSTRUCT *ins)) 
SWELL_API_DEFINE(BOOL, TreeView_Expand,(HWND hwnd, HTREEITEM item, UINT flag))
SWELL_API_DEFINE(HTREEITEM, TreeView_GetSelection,(HWND hwnd))
SWELL_API_DEFINE(void, TreeView_DeleteItem,(HWND hwnd, HTREEITEM item))
SWELL_API_DEFINE(void, TreeView_DeleteAllItems,(HWND hwnd))
SWELL_API_DEFINE(void, TreeView_SelectItem,(HWND hwnd, HTREEITEM item))
SWELL_API_DEFINE(BOOL, TreeView_GetItem,(HWND hwnd, LPTVITEM pitem))
SWELL_API_DEFINE(BOOL, TreeView_SetItem,(HWND hwnd, LPTVITEM pitem))
SWELL_API_DEFINE(HTREEITEM, TreeView_HitTest, (HWND hwnd, TVHITTESTINFO *hti))
SWELL_API_DEFINE(BOOL, TreeView_SetIndent,(HWND hwnd, int indent))

SWELL_API_DEFINE(HTREEITEM, TreeView_GetChild, (HWND hwnd, HTREEITEM item))
SWELL_API_DEFINE(HTREEITEM, TreeView_GetNextSibling, (HWND hwnd, HTREEITEM item))
SWELL_API_DEFINE(HTREEITEM, TreeView_GetRoot, (HWND hwnd))

SWELL_API_DEFINE(void,TreeView_SetBkColor,(HWND hwnd, int color))
SWELL_API_DEFINE(void,TreeView_SetTextColor,(HWND hwnd, int color))
SWELL_API_DEFINE(void,ListView_SetBkColor,(HWND hwnd, int color))
SWELL_API_DEFINE(void,ListView_SetTextBkColor,(HWND hwnd, int color))
SWELL_API_DEFINE(void,ListView_SetTextColor,(HWND hwnd, int color))
SWELL_API_DEFINE(void,ListView_SetGridColor,(HWND hwnd, int color))
SWELL_API_DEFINE(void,ListView_SetSelColors,(HWND hwnd, int *colors, int ncolors))

/*
** These are deprecated macOS-only functions for launching a modal window but still running
** your own code. In general use DialogBox with a timer if needed instead.
*/
SWELL_API_DEFINE(void *, SWELL_ModalWindowStart,(HWND hwnd))
SWELL_API_DEFINE(bool, SWELL_ModalWindowRun,(void *ctx, int *ret)) // returns false and puts retval in *ret when done
SWELL_API_DEFINE(void, SWELL_ModalWindowEnd,(void *ctx))
SWELL_API_DEFINE(void, SWELL_CloseWindow,(HWND hwnd))

/*
** Menu functions
** macOS: HMENU is a NSMenu *.
*/
SWELL_API_DEFINE(HMENU, CreatePopupMenu,())
SWELL_API_DEFINE(HMENU, CreatePopupMenuEx,(const char *title))
SWELL_API_DEFINE(void, DestroyMenu,(HMENU hMenu))
SWELL_API_DEFINE(int, AddMenuItem,(HMENU hMenu, int pos, const char *name, int tagid))
SWELL_API_DEFINE(HMENU, GetSubMenu,(HMENU hMenu, int pos))
SWELL_API_DEFINE(int, GetMenuItemCount,(HMENU hMenu))
SWELL_API_DEFINE(int, GetMenuItemID,(HMENU hMenu, int pos))
SWELL_API_DEFINE(bool, SetMenuItemModifier,(HMENU hMenu, int idx, int flag, int code, unsigned int mask))
SWELL_API_DEFINE(bool, SetMenuItemText,(HMENU hMenu, int idx, int flag, const char *text))
SWELL_API_DEFINE(bool, EnableMenuItem,(HMENU hMenu, int idx, int en))
SWELL_API_DEFINE(bool, DeleteMenu,(HMENU hMenu, int idx, int flag))
SWELL_API_DEFINE(bool, CheckMenuItem,(HMENU hMenu, int idx, int chk))
SWELL_API_DEFINE(void, InsertMenuItem,(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi))
SWELL_API_DEFINE(void,SWELL_InsertMenu,(HMENU menu, int pos, unsigned int flag, UINT_PTR idx, const char *str))
#ifdef InsertMenu
#undef InsertMenu
#endif
#define InsertMenu SWELL_InsertMenu

SWELL_API_DEFINE(BOOL, GetMenuItemInfo,(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi))
SWELL_API_DEFINE(BOOL, SetMenuItemInfo,(HMENU hMenu, int pos, BOOL byPos, MENUITEMINFO *mi))
SWELL_API_DEFINE(void, DrawMenuBar,(HWND))



/*
** LoadMenu()
**  Loads a menu created with SWELL_DEFINE_MENU_RESOURCE_BEGIN(), see swell-menugen.h
**  Notes: the hinst parameter is ignored, the menu must have been defined in the same 
**  module (executable or shared library) as the LoadMenu call. If you wish to load a 
**  menu from another module, get its SWELL_curmodule_menuresource_head and pass it to 
**  SWELL_LoadMenu directly.
*/
#ifndef LoadMenu
#define LoadMenu(hinst,resid) SWELL_LoadMenu(SWELL_curmodule_menuresource_head,(resid))
#endif
SWELL_API_DEFINE(HMENU, SWELL_LoadMenu,(struct SWELL_MenuResourceIndex *head, const char *resid))

/*
** TrackPopupMenu
** Notes: the rectangle is ignored, and resvd should always be 0.
*/
SWELL_API_DEFINE(int, TrackPopupMenu,(HMENU hMenu, int flags, int xpos, int ypos, int resvd, HWND hwnd, const RECT *r))

/* 
** SWELL_SetMenuDestination: set the action destination for all items and subitems in a menu 
** macOS only, TrackPopupMenu and SetMenu use this internally, but it may be useful.
*/
SWELL_API_DEFINE(void, SWELL_SetMenuDestination,(HMENU menu, HWND hwnd))

/*
** SWELL_DuplicateMenu:
** Copies an entire menu.
*/
SWELL_API_DEFINE(HMENU, SWELL_DuplicateMenu,(HMENU menu))  

/*
** SetMenu()/GetMenu()
** macOS: These work on SWELL created NSWindows, or objective C objects that respond to
**   swellSetMenu:(HMENU) and swellGetMenu. SWELL windows will automatically set the 
**   application menu via NSApp setMainMenu: when activated.
*/
SWELL_API_DEFINE(BOOL, SetMenu,(HWND hwnd, HMENU menu))
SWELL_API_DEFINE(HMENU, GetMenu,(HWND hwnd))

/*
** SWELL_SetDefaultWindowMenu()/SWELL_GetDefaultWindowMenu()
** macOS: these set an internal flag for the default window menu, which will be set 
** when switching to a window that has no menu set. Set this to your application's
** main menu.
**
** generic: these set the internal state, which is currently unused
*/
SWELL_API_DEFINE(HMENU, SWELL_GetDefaultWindowMenu,())
SWELL_API_DEFINE(void, SWELL_SetDefaultWindowMenu,(HMENU))
SWELL_API_DEFINE(HMENU, SWELL_GetDefaultModalWindowMenu,())
SWELL_API_DEFINE(void, SWELL_SetDefaultModalWindowMenu,(HMENU))
SWELL_API_DEFINE(HMENU, SWELL_GetCurrentMenu,())
SWELL_API_DEFINE(void, SWELL_SetCurrentMenu,(HMENU))



/* 
** SWELL dialog box/control/window/child dialog/etc creation
** DialogBox(), DialogBoxParam(), CreateDialog(), and CreateDialogParam()
**
** Notes:
** hInstance parameters are ignored. If you wish to load a dialog resource from another
** module (shared library/executable), you should get its SWELL_curmodule_dialogresource_head 
** via your own mechanism and pass it as the first parameter of SWELL_DialogBox or whichever API.
** 
** If you are using CreateDialog() and creating a child window, you can use a resource ID of 
** 0, which creates an opaque child window. Instead of passing a DLGPROC, you should pass a 
** (WNDPROC) routine that retuns LRESULT (and cast it to DLGPROC).
** 
*/

#ifndef DialogBox
#define DialogBox(hinst, resid, par, dlgproc) SWELL_DialogBox(SWELL_curmodule_dialogresource_head,(resid),par,dlgproc,0)
#define DialogBoxParam(hinst, resid, par, dlgproc, param) SWELL_DialogBox(SWELL_curmodule_dialogresource_head,(resid),par,dlgproc,param)
#define CreateDialog(hinst,resid,par,dlgproc) SWELL_CreateDialog(SWELL_curmodule_dialogresource_head,(resid),par,dlgproc,0)
#define CreateDialogParam(hinst,resid,par,dlgproc,param) SWELL_CreateDialog(SWELL_curmodule_dialogresource_head,(resid),par,dlgproc,param)
#endif
SWELL_API_DEFINE(int, SWELL_DialogBox,(struct SWELL_DialogResourceIndex *reshead, const char *resid, HWND parent,  DLGPROC dlgproc, LPARAM param))  
SWELL_API_DEFINE(HWND, SWELL_CreateDialog,(struct SWELL_DialogResourceIndex *reshead, const char *resid, HWND parent, DLGPROC dlgproc, LPARAM param))


/*
** SWELL_RegisterCustomControlCreator(), SWELL_UnregisterCustomControlCreator()
** Notes:
** Pass these a callback function that can create custom controls based on classname.
*/

SWELL_API_DEFINE(void, SWELL_RegisterCustomControlCreator,(SWELL_ControlCreatorProc proc))
SWELL_API_DEFINE(void, SWELL_UnregisterCustomControlCreator,(SWELL_ControlCreatorProc proc))

SWELL_API_DEFINE(LRESULT, DefWindowProc,(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam))
                 
SWELL_API_DEFINE(void, EndDialog,(HWND, int))            


SWELL_API_DEFINE(int,SWELL_GetDefaultButtonID,(HWND hwndDlg, bool onlyIfEnabled))


/*
** SendMessage()
** Notes:
**   LIMITATION - SendMessage should only be used from the same thread that the window/view
**   was created in. Cross-thread use SHOULD BE AVOIDED. It may work, but it may blow up.
**   PostMessage  (below) can be used in certain instances for asynchronous notifications.
*/
SWELL_API_DEFINE(LRESULT, SendMessage,(HWND, UINT, WPARAM, LPARAM))  

#ifndef SendDlgItemMessage                                       
#define SendDlgItemMessage(hwnd,idx,msg,wparam,lparam) SendMessage(GetDlgItem(hwnd,idx),msg,wparam,lparam)
#endif

/*
** SWELL_BroadcastMessage()
**  sends a message to all top-level windows
*/
SWELL_API_DEFINE(void,SWELL_BroadcastMessage,(UINT, WPARAM, LPARAM))

/*
** PostMessage()
** Notes:
**   Queues a message into the application message queue. Note that you should only ever
**   send messages to destinations that were created from the main thread. They will be 
**   processed later from a timer (in the main thread).
** When a window is destroyed any outstanding messages will be discarded for it.
*/
SWELL_API_DEFINE(BOOL, PostMessage,(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam))

/*
** SWELL_MessageQueue_Flush():
** Notes:
** Processes all messages in the message queue. ONLY call from the main thread.
*/
SWELL_API_DEFINE(void, SWELL_MessageQueue_Flush,())

/*
** SWELL_MessageQueue_Clear():
** Notes:
** Discards all messages from the message queue if h is NULL, otherwise discards all messages
** to h.
*/
SWELL_API_DEFINE(void, SWELL_MessageQueue_Clear,(HWND h))



/*
** keyboard/mouse support
*/

/*
** SWELL_MacKeyToWindowsKey()
** Pass a keyboard NSEvent *, and it will return a windows VK_ keycode (or ascii), and set flags, 
** including (possibly) FSHIFT, FCONTROL (apple key), FALT, and FVIRTKEY. The ctrl key is not checked,
** as SWELL generally encourages this to be used soley for a right mouse button (as modifier).
*/
#ifdef SWELL_TARGET_OSX
SWELL_API_DEFINE(int, SWELL_MacKeyToWindowsKey,(void *nsevent, int *flags))

  // ex is the same as normal, except if mode=1 it does more processing of raw keys w/ modifiers
  // and also if nsevent==NULL current event is used
SWELL_API_DEFINE(int, SWELL_MacKeyToWindowsKeyEx,(void *nsevent, int *flags, int mode))
#endif
SWELL_API_DEFINE(int,SWELL_KeyToASCII,(int wParam, int lParam, int *newflags))


/*
** GetAsyncKeyState()
** macOS: only supports VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_SHIFT, VK_MENU, VK_CONTROL (apple/command key), VK_LWIN (control key)
** GDK: only supports VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_SHIFT, VK_MENU, VK_CONTROL, VK_LWIN
*/
SWELL_API_DEFINE(WORD, GetAsyncKeyState,(int key))

/*
** GetCursorPos(), GetMessagePos()
** GetMessagePos() currently returns the same coordinates as GetCursorPos()
*/
SWELL_API_DEFINE(void, GetCursorPos,(POINT *pt))
SWELL_API_DEFINE(DWORD, GetMessagePos,())

/*
** LoadCursor(). 
** Notes: hinstance parameter ignored, supports loading some of the predefined values (e.g. IDC_SIZEALL)
** and cursors registered into the main module
** macOS: HCURSOR = NSCursor *
** see also: SWELL_LoadCursorFromFile
*/
SWELL_API_DEFINE(HCURSOR, SWELL_LoadCursor,(const char *idx))
#ifndef LoadCursor
#define LoadCursor(a,x) SWELL_LoadCursor(x)
#endif

/*
** SetCursor()
** macOS: can cast a NSCursor* to HCURSOR if desired
*/
#ifdef SetCursor
#undef SetCursor
#endif
#define SetCursor(x) SWELL_SetCursor(x)
SWELL_API_DEFINE(void, SWELL_SetCursor,(HCURSOR curs))


#ifdef GetCursor
#undef GetCursor
#endif
#define GetCursor SWELL_GetCursor

#ifdef ShowCursor
#undef ShowCursor
#endif
#define ShowCursor SWELL_ShowCursor

#ifdef SetCursorPos
#undef SetCursorPos
#endif
#define SetCursorPos SWELL_SetCursorPos

#ifdef ScrollWindowEx
#undef ScrollWindowEx
#endif
#define ScrollWindowEx(a,b,c,d,e,f,g,h) ScrollWindow(a,b,c,d,e)


/*
** SWELL_EnableRightClickEmulate()
** macOS only
** Globally enable or disable emulating mouse right-click using control+left-click
*/
SWELL_API_DEFINE(void, SWELL_EnableRightClickEmulate, (BOOL enable))


/*
** GetCursor() gets the actual system cursor,
** SWELL_GetLastSetCursor() gets the last cursor set via SWELL (if they differ than some other window must have changed the cursor)
*/
SWELL_API_DEFINE(HCURSOR, SWELL_GetCursor,())
SWELL_API_DEFINE(HCURSOR, SWELL_GetLastSetCursor,())

SWELL_API_DEFINE(bool, SWELL_IsCursorVisible, ())
SWELL_API_DEFINE(int, SWELL_ShowCursor, (BOOL bShow))
SWELL_API_DEFINE(BOOL, SWELL_SetCursorPos, (int X, int Y))

/*
** SWELL_GetViewPort
** Gets screen information, for the screen that contains sourcerect. if wantWork is set
** it excluses the menu bar etc.
*/
SWELL_API_DEFINE(void, SWELL_GetViewPort,(RECT *r, const RECT *sourcerect, bool wantWork))

/*
** Clipboard API
** macOS: setting multiple types may not be supported
** GDK: only CF_TEXT is shared with system, other types are stored internally
*/
SWELL_API_DEFINE(bool, OpenClipboard,(HWND hwndDlg))
SWELL_API_DEFINE(void, CloseClipboard,())
SWELL_API_DEFINE(HANDLE, GetClipboardData,(UINT type))

SWELL_API_DEFINE(void, EmptyClipboard,())
SWELL_API_DEFINE(void, SetClipboardData,(UINT type, HANDLE h))
SWELL_API_DEFINE(UINT, RegisterClipboardFormat,(const char *desc))
SWELL_API_DEFINE(UINT, EnumClipboardFormats,(UINT lastfmt))

#ifndef CF_TEXT
  // do not use 'static int globalvalue = CF_TEXT' as this will cause problems (RegisterClipboardFormat() being called too soon!).
#define CF_TEXT (RegisterClipboardFormat("SWELL__CF_TEXT"))
#endif

SWELL_API_DEFINE(HANDLE, GlobalAlloc,(int flags, int sz))
SWELL_API_DEFINE(void *, GlobalLock,(HANDLE h))
SWELL_API_DEFINE(int, GlobalSize,(HANDLE h))
SWELL_API_DEFINE(void, GlobalUnlock,(HANDLE h))
SWELL_API_DEFINE(void, GlobalFree,(HANDLE h))


SWELL_API_DEFINE(HANDLE,CreateThread,(void *TA, DWORD stackSize, DWORD (*ThreadProc)(LPVOID), LPVOID parm, DWORD cf, DWORD *tidOut))
SWELL_API_DEFINE(HANDLE,CreateEvent,(void *SA, BOOL manualReset, BOOL initialSig, const char *ignored))
SWELL_API_DEFINE(HANDLE,CreateEventAsSocket,(void *SA, BOOL manualReset, BOOL initialSig, const char *ignored))


#ifdef _beginthreadex
#undef _beginthreadex
#endif
#define _beginthreadex(a,b,c,d,e,f) ((UINT_PTR)CreateThread(a,b,(unsigned (*)(LPVOID))(c),d,e,(DWORD*)(f)))

SWELL_API_DEFINE(DWORD,GetCurrentThreadId,())
SWELL_API_DEFINE(DWORD,WaitForSingleObject,(HANDLE hand, DWORD msTO))
SWELL_API_DEFINE(DWORD,WaitForAnySocketObject,(int numObjs, HANDLE *objs, DWORD msTO))
SWELL_API_DEFINE(BOOL,CloseHandle,(HANDLE hand))
SWELL_API_DEFINE(BOOL,SetThreadPriority,(HANDLE evt, int prio))
SWELL_API_DEFINE(BOOL,SetEvent,(HANDLE evt))
SWELL_API_DEFINE(BOOL,ResetEvent,(HANDLE evt))

#ifdef SWELL_TARGET_OSX
SWELL_API_DEFINE(void,SWELL_EnsureMultithreadedCocoa,())
SWELL_API_DEFINE(void *, SWELL_InitAutoRelease,())
SWELL_API_DEFINE(void, SWELL_QuitAutoRelease,(void *p))
SWELL_API_DEFINE(int,SWELL_TerminateProcess,(HANDLE hand))
SWELL_API_DEFINE(HANDLE,SWELL_CreateProcessIO,(const char *exe, int nparams, const char **params, bool redirectIO))
SWELL_API_DEFINE(int,SWELL_ReadWriteProcessIO,(HANDLE, int w/*stdin,stdout,stderr*/, char *buf, int bufsz))
#else
SWELL_API_DEFINE(HANDLE,SWELL_CreateProcessFromPID,(int pid))
#endif

SWELL_API_DEFINE(HANDLE,SWELL_CreateProcess,(const char *exe, int nparams, const char **params))
SWELL_API_DEFINE(int,SWELL_GetProcessExitCode,(HANDLE hand))


SWELL_API_DEFINE(HINSTANCE,LoadLibraryGlobals,(const char *fileName, bool symbolsAsGlobals))
SWELL_API_DEFINE(HINSTANCE,LoadLibrary,(const char *fileName))
SWELL_API_DEFINE(void *,GetProcAddress,(HINSTANCE hInst, const char *procName))
SWELL_API_DEFINE(BOOL,FreeLibrary,(HINSTANCE hInst))

SWELL_API_DEFINE(void*,SWELL_GetBundle,(HINSTANCE hInst))

/*
** SWELL_CreateMemContext()
** Creates a memory context (that you can get the bits for, below)
** hdc is ignored
*/
SWELL_API_DEFINE(HDC, SWELL_CreateMemContext,(HDC hdc, int w, int h))

/*
** SWELL_DeleteGfxContext()
** Deletes a context created with SWELL_CreateMemContext() (or the internal SWELL_CreateGfxContext)
*/
SWELL_API_DEFINE(void, SWELL_DeleteGfxContext,(HDC))

/*
** SWELL_GetCtxGC()
** macOS: Returns the CGContextRef of a HDC
** GDK: returns NULL
*/
SWELL_API_DEFINE(void *, SWELL_GetCtxGC,(HDC ctx))


/*
** SWELL_GetCtxFrameBuffer()
** Gets the framebuffer of a memory context. NULL if none available.
*/
SWELL_API_DEFINE(void *, SWELL_GetCtxFrameBuffer,(HDC ctx))



/* 
** Some utility functions for pushing, setting, and popping the clip region. 
** macOS-only
*/
SWELL_API_DEFINE(void, SWELL_PushClipRegion,(HDC ctx))
SWELL_API_DEFINE(void, SWELL_SetClipRegion,(HDC ctx, const RECT *r))
SWELL_API_DEFINE(void, SWELL_PopClipRegion,(HDC ctx))



SWELL_API_DEFINE(HFONT, CreateFontIndirect,(LOGFONT *))
SWELL_API_DEFINE(HFONT, CreateFont,(int lfHeight, int lfWidth, int lfEscapement, int lfOrientation, int lfWeight, char lfItalic, 
  char lfUnderline, char lfStrikeOut, char lfCharSet, char lfOutPrecision, char lfClipPrecision, 
         char lfQuality, char lfPitchAndFamily, const char *lfFaceName))

SWELL_API_DEFINE(HPEN, CreatePen,(int attr, int wid, int col))
SWELL_API_DEFINE(HBRUSH, CreateSolidBrush,(int col))
SWELL_API_DEFINE(HPEN, CreatePenAlpha,(int attr, int wid, int col, float alpha))
SWELL_API_DEFINE(HBRUSH, CreateSolidBrushAlpha,(int col, float alpha))
SWELL_API_DEFINE(HGDIOBJ, SelectObject,(HDC ctx, HGDIOBJ pen))
SWELL_API_DEFINE(HGDIOBJ, GetStockObject,(int wh))
SWELL_API_DEFINE(void, DeleteObject,(HGDIOBJ))
#ifndef DestroyIcon
#define DestroyIcon(x) DeleteObject(x)
#endif

#ifdef LineTo
#undef LineTo
#endif
#ifdef SetPixel
#undef SetPixel
#endif
#ifdef FillRect
#undef FillRect
#endif
#ifdef DrawText
#undef DrawText
#endif
#ifdef Polygon
#undef Polygon
#endif

#define DrawText SWELL_DrawText
#define FillRect SWELL_FillRect
#define LineTo SWELL_LineTo
#define SetPixel SWELL_SetPixel
#define Polygon(a,b,c) SWELL_Polygon(a,b,c)

SWELL_API_DEFINE(void, SWELL_FillRect,(HDC ctx, const RECT *r, HBRUSH br))
SWELL_API_DEFINE(void, Rectangle,(HDC ctx, int l, int t, int r, int b))
SWELL_API_DEFINE(void, Ellipse,(HDC ctx, int l, int t, int r, int b))
SWELL_API_DEFINE(void, SWELL_Polygon,(HDC ctx, POINT *pts, int npts))
SWELL_API_DEFINE(void, MoveToEx,(HDC ctx, int x, int y, POINT *op))
SWELL_API_DEFINE(void, LineTo,(HDC ctx, int x, int y))
SWELL_API_DEFINE(void, SetPixel,(HDC ctx, int x, int y, int c))
SWELL_API_DEFINE(void, PolyBezierTo,(HDC ctx, POINT *pts, int np))
SWELL_API_DEFINE(int, SWELL_DrawText,(HDC ctx, const char *buf, int len, RECT *r, int align))
SWELL_API_DEFINE(void, SetTextColor,(HDC ctx, int col))
SWELL_API_DEFINE(int, GetTextColor,(HDC ctx))
SWELL_API_DEFINE(void, SetBkColor,(HDC ctx, int col))
SWELL_API_DEFINE(void, SetBkMode,(HDC ctx, int col))
SWELL_API_DEFINE(int, GetGlyphIndicesW, (HDC ctx, wchar_t *buf, int len, unsigned short *indices, int flags))

SWELL_API_DEFINE(void, RoundRect,(HDC ctx, int x, int y, int x2, int y2, int xrnd, int yrnd))
SWELL_API_DEFINE(void, PolyPolyline,(HDC ctx, POINT *pts, DWORD *cnts, int nseg))
SWELL_API_DEFINE(BOOL, GetTextMetrics,(HDC ctx, TEXTMETRIC *tm))
SWELL_API_DEFINE(int, GetTextFace,(HDC ctx, int nCount, LPTSTR lpFaceName))
#ifdef SWELL_TARGET_OSX
SWELL_API_DEFINE(void *, GetNSImageFromHICON,(HICON))
#endif
SWELL_API_DEFINE(BOOL, GetObject, (HICON icon, int bmsz, void *_bm))
SWELL_API_DEFINE(HICON, CreateIconIndirect, (ICONINFO* iconinfo))
SWELL_API_DEFINE(HICON, LoadNamedImage,(const char *name, bool alphaFromMask))
SWELL_API_DEFINE(void, DrawImageInRect,(HDC ctx, HICON img, const RECT *r))
SWELL_API_DEFINE(void, BitBlt,(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int mode))
SWELL_API_DEFINE(void, StretchBlt,(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int srcw, int srch, int mode))
#ifndef SWELL_TARGET_OSX
SWELL_API_DEFINE(void, StretchBltFromMem,(HDC hdcOut, int x, int y, int w, int h, const void *bits, int srcw, int srch, int srcspan))
SWELL_API_DEFINE(int, SWELL_GetScaling256, (void))
#endif

SWELL_API_DEFINE(void*, SWELL_ExtendedAPI, (const char *key, void *v))

SWELL_API_DEFINE(int, GetSysColor,(int idx))
SWELL_API_DEFINE(HBITMAP, CreateBitmap,(int width, int height, int numplanes, int bitsperpixel, unsigned char* bits))

SWELL_API_DEFINE(void, SetOpaque, (HWND h, bool isopaque))
SWELL_API_DEFINE(void, SetAllowNoMiddleManRendering, (HWND h, bool allow)) // defaults to allow, use this to disable
#ifdef SWELL_TARGET_OSX
SWELL_API_DEFINE(int, SWELL_IsRetinaDC, (HDC hdc)) // returns 1 if DC is a retina DC (2x res possible)
SWELL_API_DEFINE(int, SWELL_IsRetinaHWND, (HWND h)) // returns 1 if HWND is a retina HWND
SWELL_API_DEFINE(void, SWELL_SetViewGL, (HWND h, bool wantGL))
SWELL_API_DEFINE(bool, SWELL_GetViewGL, (HWND h))
SWELL_API_DEFINE(bool, SWELL_SetGLContextToView, (HWND h)) // sets GL context to that view, returns TRUE if successs (use NULL to clear GL context)
#endif

#if defined(SWELL_TARGET_OSX) && !defined(SWELL_NO_METAL)
SWELL_API_DEFINE(int, SWELL_EnableMetal,(HWND h, int mode)) // can only call once per window. calling with 0 does nothing. 1=metal enabled, 2=metal enabled and support GetDC()/ReleaseDC() for drawing (more overhead). returns metal setting. mode=-1 for non-metal async layered mode. mode=-2 for non-metal non-async layered mode
  // NOTE: if using SWELL_EnableMetal(-1), any BitBlt()/StretchBlt() __MUST__ have the source bitmap persist. If it is resized after Blit it could cause crashes, too. So really this method is unsafe for practical use.
#else
  #ifndef SWELL_EnableMetal
  #define SWELL_EnableMetal(hwnd,x) (void)(x)
  #endif
#endif

SWELL_API_DEFINE(HDC, BeginPaint,(HWND, PAINTSTRUCT *))
SWELL_API_DEFINE(BOOL, EndPaint,(HWND, PAINTSTRUCT *))

SWELL_API_DEFINE(HDC, GetDC,(HWND)) // use these sparingly! they kinda work but shouldnt be overused!!
SWELL_API_DEFINE(HDC, GetWindowDC,(HWND)) 
SWELL_API_DEFINE(void, ReleaseDC,(HWND, HDC))

#ifdef SWELL_TARGET_OSX
SWELL_API_DEFINE(void, SWELL_FlushWindow,(HWND))
#endif
            
SWELL_API_DEFINE(void, SWELL_FillDialogBackground,(HDC hdc, const RECT *r, int level))

SWELL_API_DEFINE(HGDIOBJ,SWELL_CloneGDIObject,(HGDIOBJ a))

SWELL_API_DEFINE(int, GetSystemMetrics, (int))

SWELL_API_DEFINE(BOOL, DragQueryPoint,(HDROP,LPPOINT))
SWELL_API_DEFINE(void, DragFinish,(HDROP))
SWELL_API_DEFINE(UINT, DragQueryFile,(HDROP,UINT,char *,UINT))

// source drag/drop - callback is source implementing "create dropped files at droppath"
SWELL_API_DEFINE(void, SWELL_InitiateDragDrop, (HWND, RECT* srcrect, const char* srcfn, void (*callback)(const char* droppath)))
SWELL_API_DEFINE(void,SWELL_InitiateDragDropOfFileList,(HWND, RECT *srcrect, const char **srclist, int srccount, HICON icon))
SWELL_API_DEFINE(void, SWELL_FinishDragDrop, ())  // cancels any outstanding InitiateDragDrop



// focus rects aren't implemented as XOR as on win32, might be a straight blit or a separate window
// rct=NULL to "free" handle
// otherwise rct is in hwndPar coordinates
SWELL_API_DEFINE(void,SWELL_DrawFocusRect,(HWND hwndPar, RECT *rct, void **handle))


#ifdef SWELL_TARGET_OSX
SWELL_API_DEFINE(void,SWELL_SetWindowRepre,(HWND hwnd, const char *fn, bool isDirty)) // sets the represented file and edited state
SWELL_API_DEFINE(void,SWELL_PostQuitMessage,(void *sender))
SWELL_API_DEFINE(bool,SWELL_osx_is_dark_mode,(int mode)) // mode=0 for dark mode enabled enabled, 1=dark mode allowed (Breaks various things)
#endif

/*
** Functions used by swell-dlggen.h and swell-menugen.h
** No need to really dig into these unless you're working on swell or debugging..
*/

SWELL_API_DEFINE(void, SWELL_MakeSetCurParms,(float xscale, float yscale, float xtrans, float ytrans, HWND parent, bool doauto, bool dosizetofit))

SWELL_API_DEFINE(HWND, SWELL_MakeButton,(int def, const char *label, int idx, int x, int y, int w, int h, int flags))
SWELL_API_DEFINE(HWND, SWELL_MakeEditField,(int idx, int x, int y, int w, int h, int flags))
SWELL_API_DEFINE(HWND, SWELL_MakeLabel,(int align, const char *label, int idx, int x, int y, int w, int h, int flags))
SWELL_API_DEFINE(HWND, SWELL_MakeControl,(const char *cname, int idx, const char *classname, int style, int x, int y, int w, int h, int exstyle))
SWELL_API_DEFINE(HWND, SWELL_MakeCombo,(int idx, int x, int y, int w, int h, int flags))
SWELL_API_DEFINE(HWND, SWELL_MakeGroupBox,(const char *name, int idx, int x, int y, int w, int h, int style))
SWELL_API_DEFINE(HWND, SWELL_MakeCheckBox,(const char *name, int idx, int x, int y, int w, int h, int flags))
SWELL_API_DEFINE(HWND, SWELL_MakeListBox,(int idx, int x, int y, int w, int h, int styles))

SWELL_API_DEFINE(void, SWELL_Menu_AddMenuItem,(HMENU hMenu, const char *name, int idx, unsigned int flags))
SWELL_API_DEFINE(int, SWELL_GenerateMenuFromList,(HMENU hMenu, const void *list, int listsz)) // list is SWELL_MenuGen_Entry

SWELL_API_DEFINE(void, SWELL_GenerateDialogFromList, (const void *list, int listsz))


SWELL_API_DEFINE(unsigned int, _controlfp,(unsigned int flag, unsigned int mask))

SWELL_API_DEFINE(void,SWELL_Internal_PostMessage_Init,())


SWELL_API_DEFINE(HCURSOR,SWELL_LoadCursorFromFile,(const char *fn))
SWELL_API_DEFINE(void,SWELL_SetWindowWantRaiseAmt,(HWND h, int  amt))
SWELL_API_DEFINE(int,SWELL_GetWindowWantRaiseAmt,(HWND))

SWELL_API_DEFINE(void,SWELL_SetListViewFastClickMask,(HWND hList, int mask))


SWELL_API_DEFINE(void,GetTempPath,(int sz, char *buf))

#ifndef SWELL_TARGET_OSX
SWELL_API_DEFINE(void,SWELL_initargs,(int *argc, char ***argv))
SWELL_API_DEFINE(void,SWELL_RunMessageLoop,())
SWELL_API_DEFINE(HWND,SWELL_CreateXBridgeWindow,(HWND viewpar, void **wref, RECT*))
#endif

SWELL_API_DEFINE(bool,SWELL_GenerateGUID,(void *g))

SWELL_API_DEFINE(BOOL,EnumChildWindows,(HWND hwnd, BOOL (*cwEnumFunc)(HWND,LPARAM),LPARAM lParam))


SWELL_API_DEFINE(BOOL,SWELL_IsGroupBox,(HWND))
SWELL_API_DEFINE(BOOL,SWELL_IsButton,(HWND))
SWELL_API_DEFINE(BOOL,SWELL_IsStaticText,(HWND))
SWELL_API_DEFINE(void,SWELL_GetDesiredControlSize,(HWND hwnd, RECT *r))

SWELL_API_DEFINE(int,AddFontResourceEx,(LPCTSTR str, DWORD fl, void *pdv))

#ifdef SWELL_TARGET_OSX
SWELL_API_DEFINE(void,SWELL_DisableAppNap,(int disable))
SWELL_API_DEFINE(int,SWELL_GetOSXVersion,())
#endif

SWELL_API_DEFINE(void,SWELL_Register_Cursor_Resource,(const char *idx, const char *name, int hotspot_x, int hotspot_y))

#ifndef SWELL_TARGET_OSX
SWELL_API_DEFINE(bool, SWELL_ChooseColor, (HWND, int *, int ncustom, int *custom))
SWELL_API_DEFINE(bool, SWELL_ChooseFont, (HWND, LOGFONT*))
#endif

SWELL_API_DEFINE(bool, IsWindowEnabled, (HWND))

SWELL_API_DEFINE(int, GetClassName, (HWND, char *, int)) // only partially implemented, if using custom control creators they should call SWELL_SetClassName() to set the class name (reading class name is desired)
SWELL_API_DEFINE(void, SWELL_SetClassName, (HWND, const char*)) // must pass a static string!

#endif // _WDL_SWELL_H_API_DEFINED_
