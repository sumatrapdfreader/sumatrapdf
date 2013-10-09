--proc/cursor: cursor resources.
setfenv(1, require'winapi')
require'winapi.winuser'

ffi.cdef[[
HCURSOR LoadCursorW(
     HINSTANCE hInstance,
     LPCWSTR lpCursorName);
int ShowCursor(BOOL bShow);
BOOL SetCursorPos(int X, int Y);
BOOL SetPhysicalCursorPos(int X, int Y);
HCURSOR SetCursor(HCURSOR hCursor);
BOOL GetCursorPos(LPPOINT lpPoint);
BOOL GetPhysicalCursorPos(LPPOINT lpPoint);
BOOL ClipCursor(const RECT *lpRect);
BOOL GetClipCursor(LPRECT lpRect);
HCURSOR GetCursor(void);
]]

IDC_ARROW       = 32512
IDC_IBEAM       = 32513
IDC_WAIT        = 32514
IDC_CROSS       = 32515
IDC_UPARROW     = 32516
IDC_SIZE        = 32640
IDC_ICON        = 32641
IDC_SIZENWSE    = 32642
IDC_SIZENESW    = 32643
IDC_SIZEWE      = 32644
IDC_SIZENS      = 32645
IDC_SIZEALL     = 32646
IDC_NO          = 32648
IDC_HAND        = 32649
IDC_APPSTARTING = 32650
IDC_HELP        = 32651

function LoadCursor(hInstance, name)
	if not name then hInstance, name = nil, hInstance end
   return checkh(C.LoadCursorW(hInstance,
						ffi.cast('LPCWSTR', wcs(MAKEINTRESOURCE(name)))))
end

function SetCursor(cursor)
	return ptr(C.SetCursor(cursor))
end

function GetCursorPos(p)
	p = POINT(p)
	checknz(C.GetCursorPos(p))
	return p
end


if not ... then
print(LoadCursor(IDC_ARROW))
assert(LoadCursor(IDC_ARROW) == LoadCursor(IDC_ARROW)) --same handle every time, no worry about freeing these
print(LoadCursor(IDC_HELP))
end

