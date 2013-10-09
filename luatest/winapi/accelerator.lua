--proc/accelerator: creating accelerator tables
setfenv(1, require'winapi')
require'winapi.winuser'

ffi.cdef[[
typedef struct tagACCEL {
    BYTE   fVirt;
    WORD   key;
    WORD   id;
} ACCEL, *LPACCEL;

HACCEL CreateAcceleratorTableW(LPACCEL paccel, int cAccel);
BOOL DestroyAcceleratorTable(HACCEL hAccel);
int CopyAcceleratorTableW(HACCEL hAccelSrc, LPACCEL lpAccelDst, int cAccelEntries);
]]

FVIRTKEY  = 0x01 --without this flag, FCONTROL doesn't work; with this flag, lowercase letters don't work.
FSHIFT    = 0x04
FCONTROL  = 0x08
FALT      = 0x10

function ACCEL(t)
	if type(t) == 'table' then
		local is_char = type(t.key) == 'string' and not t.key:find'^VK_'
		t = {
			key = is_char and wcs(t.key:upper())[0] or flags(t.key),
			fVirt = bit.bor(flags(t.modifiers), FVIRTKEY),
			id = t.id,
		}
	end
	return types.ACCEL(t)
end

function CreateAcceleratorTable(accel)
	local accel, sz = arrays.ACCEL(accel)
	return own(checkh(C.CreateAcceleratorTableW(accel, sz)), DestroyAcceleratorTable)
end

function DestroyAcceleratorTable(haccel)
	checknz(C.DestroyAcceleratorTable(haccel))
	disown(haccel)
end

function AcceleratorTableSize(haccel)
	return C.CopyAcceleratorTableW(haccel, nil, 0)
end

function CopyAcceleratorTable(haccel, buf)
	local buf, sz = arrays.ACCEL(buf or AcceleratorTableSize(haccel))
	C.CopyAcceleratorTableW(haccel, buf, sz)
	return buf
end

