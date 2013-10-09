--proc/window: common API for windows and standard controls.
setfenv(1, require'winapi')
require'winapi.winuser'

--creation

ffi.cdef[[
HWND CreateWindowExW(
     DWORD dwExStyle,
     LPCWSTR lpClassName,
     LPCWSTR lpWindowName,
     DWORD dwStyle,
     int X,
     int Y,
     int nWidth,
     int nHeight,
     HWND hWndParent,
     HMENU hMenu,
     HINSTANCE hInstance,
     LPVOID lpParam);

BOOL DestroyWindow(HWND hWnd);
]]

WS_OVERLAPPED = 0x00000000
WS_POPUP = 0x80000000
WS_CHILD = 0x40000000
WS_MINIMIZE = 0x20000000
WS_VISIBLE = 0x10000000
WS_DISABLED = 0x08000000
WS_CLIPSIBLINGS = 0x04000000
WS_CLIPCHILDREN = 0x02000000
WS_MAXIMIZE = 0x01000000
WS_CAPTION = 0x00C00000
WS_BORDER = 0x00800000
WS_DLGFRAME = 0x00400000
WS_VSCROLL = 0x00200000
WS_HSCROLL = 0x00100000
WS_SYSMENU = 0x00080000
WS_THICKFRAME = 0x00040000
WS_GROUP = 0x00020000
WS_TABSTOP = 0x00010000
WS_MINIMIZEBOX = 0x00020000
WS_MAXIMIZEBOX = 0x00010000
WS_TILED = WS_OVERLAPPED
WS_ICONIC = WS_MINIMIZE
WS_SIZEBOX = WS_THICKFRAME
WS_OVERLAPPEDWINDOW = bit.bor(WS_OVERLAPPED,
									  WS_CAPTION,
									  WS_SYSMENU,
									  WS_THICKFRAME,
									  WS_MINIMIZEBOX,
									  WS_MAXIMIZEBOX)
WS_TILEDWINDOW = WS_OVERLAPPEDWINDOW
WS_CHILDWINDOW = WS_CHILD

WS_EX_DLGMODALFRAME = 0x00000001
WS_EX_NOPARENTNOTIFY = 0x00000004
WS_EX_TOPMOST = 0x00000008
WS_EX_ACCEPTFILES = 0x00000010
WS_EX_TRANSPARENT = 0x00000020
WS_EX_MDICHILD = 0x00000040
WS_EX_TOOLWINDOW = 0x00000080
WS_EX_WINDOWEDGE = 0x00000100
WS_EX_CLIENTEDGE = 0x00000200
WS_EX_CONTEXTHELP = 0x00000400
WS_EX_RIGHT = 0x00001000
WS_EX_LEFT = 0x00000000
WS_EX_RTLREADING = 0x00002000
WS_EX_LTRREADING = 0x00000000
WS_EX_LEFTSCROLLBAR = 0x00004000
WS_EX_RIGHTSCROLLBAR = 0x00000000
WS_EX_CONTROLPARENT = 0x00010000
WS_EX_STATICEDGE = 0x00020000
WS_EX_APPWINDOW = 0x00040000
WS_EX_LAYERED = 0x00080000
WS_EX_NOINHERITLAYOUT = 0x00100000
WS_EX_LAYOUTRTL = 0x00400000
WS_EX_COMPOSITED = 0x02000000
WS_EX_NOACTIVATE = 0x08000000

WS_POPUPWINDOW = bit.bor(WS_POPUP, WS_BORDER, WS_SYSMENU)
WS_EX_OVERLAPPEDWINDOW = bit.bor(WS_EX_WINDOWEDGE, WS_EX_CLIENTEDGE)
WS_EX_PALETTEWINDOW = bit.bor(WS_EX_WINDOWEDGE, WS_EX_TOOLWINDOW, WS_EX_TOPMOST)

CW_USEDEFAULT = 0x80000000 --for x and y

function CreateWindow(info)
	local hwnd = checkh(C.CreateWindowExW(
								flags(info.style_ex),
								ffi.cast('LPCWSTR', wcs(MAKEINTRESOURCE(info.class))),
								wcs(info.text),
								flags(info.style),
								info.x, info.y, info.w, info.h,
								info.parent,
								nil, nil, nil))
	if not info.parent then own(hwnd, DestroyWindow) end
	return hwnd
end

function DestroyWindow(hwnd)
	if not hwnd then return end
	checknz(C.DestroyWindow(hwnd))
	disown(hwnd)
end

--commands

ffi.cdef[[
BOOL ShowWindow(HWND hWnd, int nCmdShow);
]]

SW_HIDE              = 0
SW_SHOWNORMAL        = 1
SW_NORMAL            = 1
SW_SHOWMINIMIZED     = 2
SW_SHOWMAXIMIZED     = 3
SW_MAXIMIZE          = 3
SW_SHOWNOACTIVATE    = 4
SW_SHOW              = 5
SW_MINIMIZE          = 6
SW_SHOWMINNOACTIVE   = 7
SW_SHOWNA            = 8
SW_RESTORE           = 9
SW_SHOWDEFAULT       = 10
SW_FORCEMINIMIZE     = 11
SW_MAX               = 11

function ShowWindow(hwnd, SW)
	return C.ShowWindow(hwnd, flags(SW)) ~= 0
end

ffi.cdef[[
int GetWindowTextLengthW(HWND hWnd);

int  GetWindowTextW(
     HWND hWnd,
     LPWSTR lpString,
     int nMaxCount);

BOOL SetWindowTextW(
     HWND hWnd,
     LPCWSTR lpString);
]]

function GetWindowText(hwnd, buf)
	local ws, sz = WCS(buf or checkpoz(C.GetWindowTextLengthW(hwnd)))
	C.GetWindowTextW(hwnd, ws, sz+1)
	return buf or mbs(ws)
end

function SetWindowText(hwnd, text)
	checknz(C.SetWindowTextW(hwnd, wcs(text)))
end

ffi.cdef[[
BOOL SetWindowPos(
     HWND hWnd,
     HWND hWndInsertAfter,
     int X,
     int Y,
     int cx,
     int cy,
     UINT uFlags);
]]

HWND_TOP        = ffi.cast('HWND', 0)
HWND_BOTTOM     = ffi.cast('HWND', 1)
HWND_TOPMOST    = ffi.cast('HWND', -1)
HWND_NOTOPMOST  = ffi.cast('HWND', -2)

SWP_NOSIZE           = 0x0001
SWP_NOMOVE           = 0x0002
SWP_NOZORDER         = 0x0004
SWP_NOREDRAW         = 0x0008
SWP_NOACTIVATE       = 0x0010
SWP_FRAMECHANGED     = 0x0020  --The frame changed: send WM_NCCALCSIZE
SWP_SHOWWINDOW       = 0x0040
SWP_HIDEWINDOW       = 0x0080
SWP_NOCOPYBITS       = 0x0100
SWP_NOOWNERZORDER    = 0x0200  --Don't do owner Z ordering
SWP_NOSENDCHANGING   = 0x0400  --Don't send WM_WINDOWPOSCHANGING
SWP_DRAWFRAME        = SWP_FRAMECHANGED
SWP_NOREPOSITION     = SWP_NOOWNERZORDER
SWP_DEFERERASE       = 0x2000
SWP_ASYNCWINDOWPOS   = 0x4000
SWP_FRAMECHANGED_ONLY = bit.bor(SWP_NOZORDER, SWP_NOOWNERZORDER, SWP_NOACTIVATE,
											SWP_NOSIZE, SWP_NOMOVE, SWP_FRAMECHANGED)
SWP_ZORDER_CHANGED_ONLY = bit.bor(SWP_NOMOVE, SWP_NOSIZE, SWP_NOACTIVATE)

function SetWindowPos(hwnd, back_hwnd, x, y, cx, cy, SWP)
	checknz(C.SetWindowPos(hwnd, back_hwnd, x, y, cx, cy, flags(SWP)))
end

ffi.cdef[[
BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
BOOL UpdateWindow(HWND hWnd);
BOOL EnableWindow(HWND hWnd, BOOL bEnable);
BOOL GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL GetClientRect(HWND hWnd, LPRECT lpRect);
HWND SetParent(HWND hWndChild, HWND hWndNewParent);
HWND GetParent(HWND hWnd);
BOOL IsWindowVisible(HWND hWnd);
BOOL IsWindowEnabled(HWND hWnd);
HWND GetActiveWindow();
HWND SetActiveWindow(HWND hWnd);
BOOL BringWindowToTop(HWND hWnd);
BOOL IsIconic(HWND hWnd);
BOOL IsZoomed(HWND hWnd);
HWND GetFocus();
HWND SetFocus(HWND hWnd);
HWND GetWindow(HWND hWnd, UINT uCmd);
HWND GetTopWindow(HWND hWnd);
BOOL IsWindowUnicode(HWND hWnd);
]]

function MoveWindow(hwnd, x, y, w, h, repaint)
	checknz(C.MoveWindow(hwnd, x, y, w, h, repaint))
end

function UpdateWindow(hwnd) --send WM_PAINT _if_ current update region is not empty
	checknz(C.UpdateWindow(hwnd))
end

function EnableWindow(hwnd, enable)
	return C.EnableWindow(hwnd, enable) ~= 0
end

function GetWindowRect(hwnd, rect)
	rect = RECT(rect)
	checknz(C.GetWindowRect(hwnd, rect))
	return rect
end

function GetClientRect(hwnd, rect)
	rect = RECT(rect)
	checknz(C.GetClientRect(hwnd, rect))
	return rect
end

GetParent = C.GetParent

function SetParent(hwnd, parent)
	local prev_parent = checkh(C.SetParent(hwnd, parent))
	if parent == nil then own(hwnd, DestroyWindow) else disown(hwnd) end
	return prev_parent
end

function IsWindowVisible(hwnd)
	return C.IsWindowVisible(hwnd) ~= 0
end

function IsWindowEnabled(hwnd)
	return C.IsWindowEnabled(hwnd) ~= 0
end

function GetActiveWindow()
	return ptr(C.GetActiveWindow())
end

SetActiveWindow = C.SetActiveWindow

function BringWindowToTop(hwnd)
	checknz(C.BringWindowToTop(hwnd))
end

IsMinimized = C.IsIconic
IsMaximized = C.IsZoomed

function GetFocus()
	return ptr(C.GetFocus())
end

SetFocus = C.SetFocus

GW_HWNDFIRST        = 0
GW_HWNDLAST         = 1
GW_HWNDNEXT         = 2
GW_HWNDPREV         = 3
GW_OWNER            = 4
GW_CHILD            = 5
GW_ENABLEDPOPUP     = 6

function GetWindow(hwnd, GW) return ptr(C.GetWindow(hwnd, flags(GW))) end
function GetOwner(hwnd)        return callh2(GetWindow, hwnd, GW_OWNER) end
function GetFirstChild(hwnd)   return callh2(GetWindow, hwnd, GW_CHILD) end
function GetFirstSibling(hwnd) return callh2(GetWindow, hwnd, GW_HWNDFIRST) end
function GetLastSibling(hwnd)  return callh2(GetWindow, hwnd, GW_HWNDLAST) end
function GetNextSibling(hwnd)  return callh2(GetWindow, hwnd, GW_HWNDNEXT) end --from top to bottom of the z-order
function GetPrevSibling(hwnd)  return callh2(GetWindow, hwnd, GW_HWNDPREV) end --from bottom to top of the z-order

local function nextchild(parent, sibling)
	if not sibling then return GetFirstChild(parent) end
	return GetNextSibling(hwnd)
end
function GetChildWindows(hwnd) --returns a stateless iterator iterating from top to bottom of the z-order
	return nextchild, hwnd
end

GetTopWindow = C.GetTopWindow --same semantics as GetFirstChild ?

function IsWindowUnicode(hwnd) --for outside windows; ours are always unicode
	return C.IsWindowUnicode(hwnd) ~= 0
end

ffi.cdef[[
typedef __stdcall BOOL (* WNDENUMPROC)(HWND, LPARAM);
BOOL EnumChildWindows(
     HWND hWndParent,
     WNDENUMPROC lpEnumFunc,
     LPARAM lParam);
]]

function EnumChildWindows(hwnd) --for a not null hwnd use GetChildWindows (no callback, no table)
	local t = {}
	local cb = ffi.cast('WNDENUMPROC', function(hwnd, lparam)
		t[#t+1] = hwnd
		return 1 --continue
	end)
	C.EnumChildWindows(hwnd, cb, 0)
	cb:free()
	return t
end

ffi.cdef[[
typedef struct tagWINDOWPLACEMENT {
    UINT  length;
    UINT  _flags;
    UINT  command;
    POINT minimized_pos;
    POINT maximized_pos;
    RECT  normalpos;
} WINDOWPLACEMENT;
typedef WINDOWPLACEMENT *PWINDOWPLACEMENT, *LPWINDOWPLACEMENT;

BOOL GetWindowPlacement(
     HWND hWnd,
     WINDOWPLACEMENT *lpwndpl);

BOOL SetWindowPlacement(
     HWND hWnd,
     const WINDOWPLACEMENT *lpwndpl);
]]

WINDOWPLACEMENT = struct{
	ctype = 'WINDOWPLACEMENT', size = 'length',
	fields = sfields{
		'flags', '_flags', flags, pass,
	},
}

function GetWindowPlacement(hwnd, wpl)
	wpl = WINDOWPLACEMENT(wpl)
	checknz(C.GetWindowPlacement(hwnd, wpl))
	return wpl
end

function SetWindowPlacement(hwnd, wpl)
	wpl = WINDOWPLACEMENT(wpl)
	checknz(C.SetWindowPlacement(hwnd, wpl))
end

ffi.cdef[[
LRESULT DefWindowProcW(
     HWND hWnd,
     UINT Msg,
     WPARAM wParam,
     LPARAM lParam);

LRESULT CallWindowProcW(
     LONG lpPrevWndFunc,
	  HWND hWnd,
     UINT Msg,
     WPARAM wParam,
     LPARAM lParam);
]]

DefWindowProc = C.DefWindowProcW
CallWindowProc = C.CallWindowProcW

--set/get window long

GWL_WNDPROC        = -4
GWL_HINSTANCE      = -6
GWL_HWNDPARENT     = -8
GWL_STYLE          = -16
GWL_EXSTYLE        = -20
GWL_USERDATA       = -21
GWL_ID             = -12

if ffi.abi'64bit' then
	ffi.cdef[[
	LONG_PTR SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
	LONG_PTR GetWindowLongPtrW(HWND hWnd, int nIndex);
	]]
	SetWindowLongW = C.SetWindowLongPtrW
	GetWindowLongW = C.GetWindowLongPtrW
else --32bit
	ffi.cdef[[
	LONG SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong);
	LONG GetWindowLongW(HWND hWnd, int nIndex);
	]]
	SetWindowLongW = C.SetWindowLongW
	GetWindowLongW = C.GetWindowLongW
end

function SetWindowLong(hwnd, GWL, long)
	return callnz2(SetWindowLongW, hwnd, flags(GWL), ffi.cast('LONG', long))
end

function GetWindowLong(hwnd, GWL)
	return GetWindowLongW(hwnd, flags(GWL))
end

function GetWindowStyle(hwnd) return GetWindowLong(hwnd, GWL_STYLE) end
function SetWindowStyle(hwnd, style) SetWindowLong(hwnd, GWL_STYLE, flags(style)) end

function GetWindowExStyle(hwnd) return GetWindowLong(hwnd, GWL_EXSTYLE) end
function SetWindowExStyle(hwnd, style) SetWindowLong(hwnd, GWL_EXSTYLE, flags(style)) end

function GetWindowInstance(hwnd) return ffi.cast('HMODULE', GetWindowLong(hwnd, GWL_HINSTANCE)) end
function SetWindowInstance(hwnd, hinst) SetWindowLong(hwnd, GWL_HINSTANCE, hinst) end

function IsRestored(hwnd) return bit.band(GetWindowStyle(hwnd), WS_MINIMIZE + WS_MAXIMIZE) == 0 end
function IsVisible(hwnd) return bit.band(GetWindowStyle(hwnd), WS_VISIBLE) == WS_VISIBLE end

--window geometry

ffi.cdef[[
int MapWindowPoints(
     HWND hWndFrom,
     HWND hWndTo,
     LPPOINT lpPoints,
     UINT cPoints);

HWND WindowFromPoint(POINT Point);
HWND ChildWindowFromPoint(HWND hWndParent, POINT Point);
HWND RealChildWindowFromPoint(HWND hWndParent, POINT Point);

BOOL AdjustWindowRectEx(
     LPRECT lpRect,
     DWORD dwStyle,
     BOOL bMenu,
     DWORD dwExStyle);
]]

function MapWindowPoints(hwndFrom, hwndTo, points)
	local points, sz = arrays.POINT(points)
	callnz2(C.MapWindowPoints, hwndFrom, hwndTo, points, sz)
	return points
end

function MapWindowPoint(hwndFrom, hwndTo, ...) --changes and returns the same passed point
	local p = POINT(...)
	callnz2(C.MapWindowPoints, hwndFrom, hwndTo, ffi.cast('POINT*', p), 1)
	return p
end

function MapWindowRect(hwndFrom, hwndTo, ...) --changes and returns the same passed rect
	local r = RECT(...)
	callnz2(C.MapWindowPoints, hwndFrom, hwndTo, ffi.cast('POINT*', r), 2)
	return r
end

function WindowFromPoint(...)
	return ptr(C.WindowFromPoint(POINT(...)))
end

function ChildWindowFromPoint(hwnd, ...)
	return ptr(C.ChildWindowFromPoint(hwnd, POINT(...)))
end

function RealChildWindowFromPoint(hwnd, ...)
	return ptr(C.RealChildWindowFromPoint(hwnd, POINT(...)))
end

function AdjustWindowRect(cr, wstyle, wstylex, hasmenu, rect)
	rect = RECT(rect)
	rect.x1 = cr.x1
	rect.y1 = cr.y1
	rect.x2 = cr.x2
	rect.y2 = cr.y2
	checknz(C.AdjustWindowRectEx(rect))
	return rect
end

-- messages

ffi.cdef[[
typedef struct tagMSG {
	 HWND        hwnd;
	 UINT        message;
	 union {
		WPARAM      wParam;
		int         signed_wParam;
	 };
	 LPARAM      lParam;
	 DWORD       time;
	 POINT       pt;
} MSG, *PMSG, *NPMSG, *LPMSG;

BOOL GetMessageW(
	  LPMSG lpMsg,
	  HWND hWnd,
	  UINT wMsgFilterMin,
	  UINT wMsgFilterMax);

BOOL TranslateMessage(const MSG *lpMsg);

int TranslateAcceleratorW(
     HWND hWnd,
     HACCEL hAccTable,
     LPMSG lpMsg);

LRESULT DispatchMessageW(const MSG *lpMsg);

BOOL IsDialogMessageW(
     HWND hDlg,
     LPMSG lpMsg);

void PostQuitMessage(int nExitCode);

LRESULT SendMessageW(
	  HWND hWnd,
	  UINT Msg,
	  WPARAM wParam,
	  LPARAM lParam);

BOOL PeekMessageW(
     LPMSG lpMsg,
     HWND hWnd,
     UINT wMsgFilterMin,
     UINT wMsgFilterMax,
     UINT wRemoveMsg);

BOOL PostMessageW(
     HWND hWnd,
     UINT Msg,
     WPARAM wParam,
     LPARAM lParam);

HWND GetCapture(void);
HWND SetCapture(HWND hWnd);
BOOL ReleaseCapture(void);

typedef void (* TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
UINT_PTR SetTimer(
     HWND hWnd,
     UINT_PTR nIDEvent,
     UINT uElapse,
     TIMERPROC lpTimerFunc);

BOOL KillTimer(
     HWND hWnd,
     UINT_PTR uIDEvent);
]]

function GetMessage(hwnd, WMmin, WMmax, msg)
	return checkpoz(C.GetMessageW(types.MSG(msg), hwnd, flags(WMmin), flags(WMmax)))
end

function DispatchMessage(msg)
	return C.DispatchMessageW(msg)
end

function TranslateAccelerator(hwnd, haccel, msg)
	return C.TranslateAcceleratorW(hwnd, haccel, msg) ~= 0
end

function TranslateMessage(msg)
	return C.TranslateMessage(msg)
end

function IsDialogMessage(hwnd, msg)
	return C.IsDialogMessageW(hwnd, msg) ~= 0
end

--[[
Mike Sez:
> The problem is that a FFI callback cannot safely be called from a
> C function which is itself called via the FFI from JIT-compiled
> code.
>
> I've put in a lot of heuristics to detect this, and it usually
> succeeds in disabling compilation for such a function. However in
> your case the loop is compiled before the callback is ever called,
> so the detection fails.
>
> The straighforward solution is to put the message loop into an
> extra Lua function and use jit.off(func).
]]
jit.off(GetMessage)
jit.off(DispatchMessage)
jit.off(TranslateAccelerator)
jit.off(TranslateMessage)
jit.off(IsDialogMessage)


HWND_BROADCAST = ffi.cast('HWND', 0xffff)
HWND_MESSAGE   = ffi.cast('HWND', -3)

function PostQuitMessage(exitcode)
	C.PostQuitMessage(exitcode or 0)
end

function SendMessage(hwnd, WM, wparam, lparam)
	if wparam == nil then wparam = 0 end
	if type(lparam) == 'nil' then lparam = 0 end
	return C.SendMessageW(hwnd, WM,
										ffi.cast('WPARAM', wparam),
										ffi.cast('LPARAM', lparam))
end

SNDMSG = SendMessage --less typing on those tedious macros

function PostMessage(hwnd, WM, wparam, lparam)
	if wparam == nil then wparam = 0 end
	if lparam == nil then lparam = 0 end
	return C.PostMessageW(hwnd, WM,
										ffi.cast('WPARAM', wparam),
										ffi.cast('LPARAM', lparam))
end

-- Queue status flags for GetQueueStatus() and MsgWaitForMultipleObjects()
QS_KEY              = 0x0001
QS_MOUSEMOVE        = 0x0002
QS_MOUSEBUTTON      = 0x0004
QS_POSTMESSAGE      = 0x0008
QS_TIMER            = 0x0010
QS_PAINT            = 0x0020
QS_SENDMESSAGE      = 0x0040
QS_HOTKEY           = 0x0080
QS_ALLPOSTMESSAGE   = 0x0100
QS_RAWINPUT         = 0x0400
QS_MOUSE            = bit.bor(QS_MOUSEMOVE, QS_MOUSEBUTTON)
QS_INPUT            = bit.bor(QS_MOUSE, QS_KEY, QS_RAWINPUT)
QS_ALLEVENTS        = bit.bor(QS_INPUT, QS_POSTMESSAGE, QS_TIMER, QS_PAINT, QS_HOTKEY)
QS_ALLINPUT         = bit.bor(QS_INPUT, QS_POSTMESSAGE, QS_TIMER, QS_PAINT, QS_HOTKEY, QS_SENDMESSAGE)

PM_NOREMOVE         = 0x0000
PM_REMOVE           = 0x0001
PM_NOYIELD          = 0x0002
PM_QS_INPUT         = bit.lshift(QS_INPUT, 16)
PM_QS_POSTMESSAGE   = bit.lshift(bit.bor(QS_POSTMESSAGE, QS_HOTKEY, QS_TIMER), 16)
PM_QS_PAINT         = bit.lshift(QS_PAINT, 16)
PM_QS_SENDMESSAGE   = bit.lshift(QS_SENDMESSAGE, 16)

function PeekMessage(hwnd, WMmin, WMmax, PM, msg)
	msg = types.MSG(msg)
	return C.PeekMessageW(msg, hwnd, flags(WMmin), flags(WMmax), flags(PM)) ~= 0, msg
end

function GetCapture()
	return ptr(C.GetCapture())
end

function SetCapture(hwnd)
	return ptr(C.SetCapture(hwnd))
end

function ReleaseCapture()
	return checknz(C.ReleaseCapture())
end

function SetTimer(hwnd, id, timeout, callback)
	return checknz(C.SetTimer(hwnd, id, timeout, callback))
end

function KillTimer(hwnd, id)
	checknz(C.KillTimer(hwnd, id))
end

--message-based commands

function SetWindowFont(hwnd, font)
	SNDMSG(hwnd, WM_SETFONT, font, true) --no result
end

function GetWindowFont(hwnd)
	return ptr(ffi.cast('HFONT', SNDMSG(hwnd, WM_GETFONT)))
end

function CloseWindow(hwnd) --the winapi CloseWindow() has nothing to do with closing the window
	checkz(SNDMSG(hwnd, WM_CLOSE))
end

function SetRedraw(hwnd, allow) --adds WS_VISIBLE to the window!
	SNDMSG(hwnd, WM_SETREDRAW, allow)
end

UIS_SET                         = 1
UIS_CLEAR                       = 2
UIS_INITIALIZE                  = 3

UISF_HIDEFOCUS                  = 0x1
UISF_HIDEACCEL                  = 0x2
UISF_ACTIVE                     = 0x4

function ChangeUIState(hwnd, UIS, UISF)
	SNDMSG(hwnd, WM_CHANGEUISTATE, MAKEWPARAM(flags(UIS), flags(UISF)))
end

--mouse input

ffi.cdef[[
BOOL DragDetect(HWND hwnd, POINT pt);
]]

function DragDetect(hwnd, point)
	return C.DragDetect(hwnd, POINT(point)) ~= 0
end

--message crackers

require'winapi.windowmessages'

