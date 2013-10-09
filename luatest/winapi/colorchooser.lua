--proc/comdlg/colorchooser: color chooser dialog.
setfenv(1, require'winapi')
require'winapi.comdlg'

ffi.cdef[[
typedef UINT_PTR (__stdcall *LPCCHOOKPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagCHOOSECOLORW {
   DWORD        lStructSize;
   HWND         hwndOwner;
   HWND         hInstance;
   COLORREF     rgbResult;
   COLORREF*    lpCustColors;
   DWORD        Flags;
   LPARAM       lCustData;
   LPCCHOOKPROC lpfnHook;
   LPCWSTR      lpTemplateName;
} CHOOSECOLORW, *LPCHOOSECOLORW;

BOOL  ChooseColorW(LPCHOOSECOLORW);
]]

CC_RGBINIT                = 0x00000001
CC_FULLOPEN               = 0x00000002
CC_PREVENTFULLOPEN        = 0x00000004
CC_SHOWHELP               = 0x00000008
CC_ENABLEHOOK             = 0x00000010
CC_ENABLETEMPLATE         = 0x00000020
CC_ENABLETEMPLATEHANDLE   = 0x00000040
CC_SOLIDCOLOR             = 0x00000080
CC_ANYCOLOR               = 0x00000100

COLORREF16 = function() return ffi.new('COLORREF[16]') end

CHOOSECOLOR = struct{
	ctype = 'CHOOSECOLORW', size = 'lStructSize',
	fields = sfields{
		'result', 'rgbResult', pass, pass,
		'custom_colors', 'lpCustColors', pass, pass,
		'flags', 'Flags', flags, pass,
	},
}

function ChooseColor(cc, cust) --reusing the cc preserves the custom colors
	cc = CHOOSECOLOR(cc)
	cust = cc.custom_colors ~= nil and cc.custom_colors or COLORREF16(cust)
	cc.custom_colors = cust
	checkcomdlg(comdlg.ChooseColorW(ffi.cast('LPCHOOSECOLORW', cc)))
	return cc, cust
end


if not ... then
require'winapi.showcase'
local window = ShowcaseWindow()
local cc, cust = CHOOSECOLOR{}
cc = ChooseColor(cc)
end
