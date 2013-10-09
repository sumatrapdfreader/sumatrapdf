--proc/resource: part of winuser dealing with resources.
setfenv(1, require'winapi')
require'winapi.winuser'

ffi.cdef[[
HANDLE LoadImageW(
     HINSTANCE hInst,
     LPCWSTR name,
     UINT type,
     int cx,
     int cy,
     UINT fuLoad);
]]

LR_DEFAULTCOLOR     = 0x00000000
LR_MONOCHROME       = 0x00000001
LR_COLOR            = 0x00000002
LR_COPYRETURNORG    = 0x00000004
LR_COPYDELETEORG    = 0x00000008
LR_LOADFROMFILE     = 0x00000010 --will only load from the current working directory!
LR_LOADTRANSPARENT  = 0x00000020
LR_DEFAULTSIZE      = 0x00000040
LR_VGACOLOR         = 0x00000080
LR_LOADMAP3DCOLORS  = 0x00001000
LR_CREATEDIBSECTION = 0x00002000
LR_COPYFROMRESOURCE = 0x00004000
LR_SHARED           = 0x00008000

function LoadImage(hinst, name, IMAGE, w, h, LR)
	return checkh(C.LoadImageW(hinst, wcs(name), flags(IMAGE), w, h, flags(LR)))
end

function LoadIconFromFile(filename, w, h, LR)
	return LoadImage(nil, filename, IMAGE_ICON, w or 0, h or 0, bit.bor(LR_LOADFROMFILE, flags(LR)))
end

function LoadBitmapFromFile(filename, w, h, LR)
	return LoadImage(nil, filename, IMAGE_BITMAP, w or 0, h or 0, bit.bor(LR_LOADFROMFILE, flags(LR)))
end
