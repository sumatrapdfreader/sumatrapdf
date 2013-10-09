--proc/winuser: winuser types and macros from multiple headers.
setfenv(1, require'winapi')
require'winapi.winusertypes'

--constants

IMAGE_BITMAP        = 0
IMAGE_ICON          = 1
IMAGE_CURSOR        = 2
IMAGE_ENHMETAFILE   = 3

DLGC_WANTARROWS     = 0x0001      -- Control wants arrow keys
DLGC_WANTTAB        = 0x0002      -- Control wants tab keys
DLGC_WANTALLKEYS    = 0x0004      -- Control wants all keys
DLGC_WANTMESSAGE    = 0x0004      -- Pass message to control
DLGC_HASSETSEL      = 0x0008      -- Understands EM_SETSEL message
DLGC_DEFPUSHBUTTON  = 0x0010      -- Default pushbutton
DLGC_UNDEFPUSHBUTTON= 0x0020      -- Non-default pushbutton
DLGC_RADIOBUTTON    = 0x0040      -- Radio button
DLGC_WANTCHARS      = 0x0080      -- Want WM_CHAR messages
DLGC_STATIC         = 0x0100      -- Static item: don't include
DLGC_BUTTON         = 0x2000      -- Button item: can be checked

--macros

function MAKELONG(lo,hi)
	return bit.bor(bit.band(lo, 0xffff), bit.lshift(bit.band(hi, 0xffff), 16))
end

MAKEWPARAM = MAKELONG
MAKELPARAM = MAKELONG
MAKELRESULT = MAKELONG

function MAKEINTRESOURCE(i)
	if type(i) == 'number' then
		return ffi.cast('LPWSTR', ffi.cast('WORD', i))
	end
	return i
end

--types

SIZE = types.SIZE
POINT = types.POINT
RECT = types.RECT

local function struct_tostring(fields)
	return function(t)
		local s = fields[1]..'{'..t[fields[2]]
		for i=3,#fields do
			s = s..','..t[fields[i]]
		end
		return s..'}'
	end
end

ffi.metatype('SIZE', {__tostring = struct_tostring{'SIZE','w','h'}})
ffi.metatype('POINT', {__tostring = struct_tostring{'POINT','x','y'}})
ffi.metatype('RECT', {
	__tostring = struct_tostring{'RECT','x1','y1','x2','y2'},
	__index = function(r,k)
		if k == 'w' then return r.x2 - r.x1 end
		if k == 'h' then return r.y2 - r.y1 end
	end,
})
