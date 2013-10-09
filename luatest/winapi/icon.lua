--proc/icon: icon resources.
setfenv(1, require'winapi')
require'winapi.winuser'

ffi.cdef[[
HICON LoadIconW(
	  HINSTANCE hInstance,
	  LPCWSTR lpIconName);

BOOL DestroyIcon(HICON hIcon);
]]

IDI_APPLICATION   = 32512
IDI_INFORMATION   = 32516
IDI_QUESTION      = 32514
IDI_WARNING       = 32515
IDI_ERROR         = 32513
IDI_WINLOGO       = 32517 --same as IDI_APPLICATION in XP
IDI_SHIELD        = 32518 --not found in XP

function LoadIconFromInstance(hInstance, name)
	if not name then hInstance, name = nil, hInstance end
	return own(checkh(C.LoadIconW(hInstance,
							ffi.cast('LPCWSTR', wcs(MAKEINTRESOURCE(name))))), DestroyIcon)
end

function DestroyIcon(hicon)
	checknz(C.DestroyIcon(hicon))
end

if not ... then
print(LoadIconFromInstance(IDI_APPLICATION))
print(LoadIconFromInstance(IDI_INFORMATION))
end

