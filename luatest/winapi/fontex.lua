--proc/fontex: font resources (new API).
setfenv(1, require'winapi')
require'winapi.font'

ffi.cdef[[
typedef struct tagENUMLOGFONTEXW
{
    LOGFONTW    elfLogFont;
    WCHAR       elfFullName[64];
    WCHAR       elfStyle[32];
    WCHAR       elfScript[32];
} ENUMLOGFONTEXW,  *LPENUMLOGFONTEXW;

typedef struct tagDESIGNVECTOR
{
    DWORD  dvReserved;
    DWORD  dvNumAxes;
    LONG   dvValues[16];
} DESIGNVECTOR, *PDESIGNVECTOR,  *LPDESIGNVECTOR;

typedef struct tagENUMLOGFONTEXDVW
{
    ENUMLOGFONTEXW elfEnumLogfontEx;
    DESIGNVECTOR   elfDesignVector;
} ENUMLOGFONTEXDVW, *PENUMLOGFONTEXDVW,  *LPENUMLOGFONTEXDVW;

HFONT   CreateFontIndirectExW(const ENUMLOGFONTEXDVW *);
]]

ENUMLOGFONTEXDVW = struct{
	ctype = 'ENUMLOGFONTEXDVW',
}

function CreateFontEx(lfex)
	lfex = ENUMLOGFONTEXDVW(lfex)
	return own(checkh(C.CreateFontIndirectExW(lfex)), DeleteObject)
end

if not ... then
local font = print(CreateFontEx{

})
end
