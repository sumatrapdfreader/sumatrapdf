--proc/window/class: window classes in the winapi sense.
--this is unrelated to the oo API for windows (see msdn for what a window class means).
setfenv(1, require'winapi')
require'winapi.winuser'

--register/unregister

ffi.cdef[[
typedef struct tagWNDCLASSEXW {
	 UINT        cbSize;
	 UINT        _style;
	 WNDPROC     proc;
	 int         cbClsExtra;
	 int         cbWndExtra;
	 HINSTANCE   hInstance;
	 HICON       icon;
	 HCURSOR     cursor;
	 HBRUSH      background;
	 LPCWSTR     lpszMenuName;
	 LPCWSTR     lpszClassName;
	 HICON       small_icon;
} WNDCLASSEXW, *PWNDCLASSEXW,  *NPWNDCLASSEXW,  *LPWNDCLASSEXW;

ATOM RegisterClassExW(const WNDCLASSEXW *);

BOOL UnregisterClassW(
     LPCWSTR lpClassName,
     HINSTANCE hInstance);

]]

CS_VREDRAW = 0x0001
CS_HREDRAW = 0x0002
CS_DBLCLKS = 0x0008
CS_OWNDC = 0x0020
CS_CLASSDC = 0x0040
CS_PARENTDC = 0x0080
CS_NOCLOSE = 0x0200
CS_SAVEBITS = 0x0800
CS_BYTEALIGNCLIENT = 0x1000
CS_BYTEALIGNWINDOW = 0x2000
CS_GLOBALCLASS = 0x4000
CS_IME = 0x00010000
CS_DROPSHADOW = 0x00020000

WNDCLASSEXW = struct{
	ctype = 'WNDCLASSEXW', size = 'cbSize',
	fields = sfields{
		'name', 'lpszClassName', wcs, mbs,
		'style', '_style', flags, pass,
	}
}

function RegisterClass(info)
	return checknz(C.RegisterClassExW(WNDCLASSEXW(info)))
end

function UnregisterClass(class)
	if not class then return end
	checknz(C.UnregisterClassW(ffi.cast('LPCWSTR', wcs(MAKEINTRESOURCE(class))), nil))
end

--set/get class long

GCL_MENUNAME        = -8
GCL_HBRBACKGROUND   = -10
GCL_HCURSOR         = -12
GCL_HICON           = -14
GCL_HMODULE         = -16
GCL_CBWNDEXTRA      = -18
GCL_CBCLSEXTRA      = -20
GCL_WNDPROC         = -24
GCL_STYLE           = -26
GCW_ATOM            = -32
GCL_HICONSM         = -34

if ffi.abi'64bit' then
	ffi.cdef[[
	LONG_PTR SetClassLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
	LONG_PTR GetClassLongPtrW(HWND hWnd, int nIndex);
	]]
	SetClassLongW = C.SetClassLongPtrW
	GetClassLongW = C.GetClassLongPtrW
else --32bit
	ffi.cdef[[
	LONG SetClassLongW(HWND hWnd, int nIndex, LONG dwNewLong);
	LONG GetClassLongW(HWND hWnd, int nIndex);
	]]
	SetClassLongW = C.SetClassLongW
	GetClassLongW = C.GetClassLongW
end

function SetClassLong(hwnd, GCL, long)
	callnz2(SetClassLongW, hwnd, flags(GCL), ffi.cast('LONG', long))
end

function GetClassLong(hwnd, GCL) return GetClassLongW(hwnd, flags(GCL)) end

function GetClassStyle(hwnd) return GetClassLong(hwnd, GCL_STYLE) end
function SetClassStyle(hwnd, style) SetClassLong(hwnd, GCL_STYLE, flags(style)) end

function GetClassIcon(hwnd) return ffi.cast('HICON', GetClassLong(hwnd, GCL_HICON)) end
function SetClassIcon(hwnd, icon) SetClassLong(hwnd, GCL_HICON, icon) end

function GetClassSmallIcon(hwnd) return ffi.cast('HICON', GetClassLong(hwnd, GCL_HICONSM)) end
function SetClassSmallIcon(hwnd, icon) SetClassLong(hwnd, GCL_HICONSM, icon) end

function GetClassCursor(hwnd) return ffi.cast('HCURSOR', GetClassLong(hwnd, GCL_HCURSOR)) end
function SetClassCursor(hwnd, cursor) SetClassLong(hwnd, GCL_HCURSOR, cursor) end

function GetClassBackground(hwnd) return ffi.cast('HBRUSH', GetClassLong(hwnd, GCL_HBRBACKGROUND)) end
function SetClassBackground(hwnd, bg) SetClassLong(hwnd, GCL_HBRBACKGROUND, bg) end

--showcase

if not ... then
require'winapi.color'
require'winapi.cursor'
require'winapi.window' --for DefWindowProc
local class = assert(print(RegisterClass{
	name='MyClass',
	style = bit.bor(CS_HREDRAW, CS_VREDRAW),
	background = COLOR_WINDOW,
	cursor = LoadCursor(IDC_ARROW),
	proc = DefWindowProc,
}))
UnregisterClass(class)
end

