--proc/window/messages: WM names and decoders.
setfenv(1, require'winapi')
require'winapi.winuser'

--wm ranges (for filtering)
WM_KEYFIRST                      = 0x0100
WM_KEYLAST                       = 0x0109
WM_IME_KEYLAST                   = 0x010F
WM_MOUSEFIRST                    = 0x0200
WM_MOUSELAST                     = 0x020D
WM_TABLET_FIRST                  = 0x02c0
WM_TABLET_LAST                   = 0x02df
WM_HANDHELDFIRST                 = 0x0358
WM_HANDHELDLAST                  = 0x035F
WM_AFXFIRST                      = 0x0360
WM_AFXLAST                       = 0x037F
WM_PENWINFIRST                   = 0x0380
WM_PENWINLAST                    = 0x038F

WM_APP                           = 0x8000
WM_USER                          = 0x0400

WM_NAMES = constants{ --mark but keep obsolete messages so you don't see unknown messages when debugging
	WM_NULL                          = 0x0000,
	WM_CREATE                        = 0x0001,
	WM_DESTROY                       = 0x0002,
	WM_MOVE                          = 0x0003,
	WM_SIZE                          = 0x0005,
	WM_ACTIVATE                      = 0x0006,
	WM_SETFOCUS                      = 0x0007,
	WM_KILLFOCUS                     = 0x0008,
	WM_ENABLE                        = 0x000A,
	WM_SETREDRAW                     = 0x000B,
	WM_SETTEXT                       = 0x000C,
	WM_GETTEXT                       = 0x000D,
	WM_GETTEXTLENGTH                 = 0x000E,
	WM_PAINT                         = 0x000F,
	WM_CLOSE                         = 0x0010,
	WM_QUERYENDSESSION               = 0x0011,
	WM_QUERYOPEN                     = 0x0013,
	WM_ENDSESSION                    = 0x0016,
	WM_QUIT                          = 0x0012,
	WM_ERASEBKGND                    = 0x0014,
	WM_SYSCOLORCHANGE                = 0x0015,
	WM_SHOWWINDOW                    = 0x0018,
	WM_WININICHANGE                  = 0x001A, --obsolete
	WM_SETTINGCHANGE                 = 0x001A,
	WM_DEVMODECHANGE                 = 0x001B,
	WM_ACTIVATEAPP                   = 0x001C,
	WM_FONTCHANGE                    = 0x001D,
	WM_TIMECHANGE                    = 0x001E,
	WM_CANCELMODE                    = 0x001F,
	WM_SETCURSOR                     = 0x0020,
	WM_MOUSEACTIVATE                 = 0x0021,
	WM_CHILDACTIVATE                 = 0x0022,
	WM_QUEUESYNC                     = 0x0023,
	WM_GETMINMAXINFO                 = 0x0024,
	WM_PAINTICON                     = 0x0026,
	WM_ICONERASEBKGND                = 0x0027,
	WM_NEXTDLGCTL                    = 0x0028,
	WM_SPOOLERSTATUS                 = 0x002A,
	WM_DRAWITEM                      = 0x002B,
	WM_MEASUREITEM                   = 0x002C,
	WM_DELETEITEM                    = 0x002D,
	WM_VKEYTOITEM                    = 0x002E,
	WM_CHARTOITEM                    = 0x002F,
	WM_SETFONT                       = 0x0030,
	WM_GETFONT                       = 0x0031,
	WM_SETHOTKEY                     = 0x0032,
	WM_GETHOTKEY                     = 0x0033,
	WM_QUERYDRAGICON                 = 0x0037,
	WM_COMPAREITEM                   = 0x0039,
	WM_GETOBJECT                     = 0x003D,
	WM_COMPACTING                    = 0x0041,
	WM_COMMNOTIFY                    = 0x0044, --obsolete
	WM_WINDOWPOSCHANGING             = 0x0046,
	WM_WINDOWPOSCHANGED              = 0x0047,
	WM_POWER                         = 0x0048, --obsolete
	WM_COPYDATA                      = 0x004A,
	WM_CANCELJOURNAL                 = 0x004B,
	WM_NOTIFY                        = 0x004E,
	WM_INPUTLANGCHANGEREQUEST        = 0x0050,
	WM_INPUTLANGCHANGE               = 0x0051,
	WM_TCARD                         = 0x0052,
	WM_HELP                          = 0x0053,
	WM_USERCHANGED                   = 0x0054,
	WM_NOTIFYFORMAT                  = 0x0055,
	WM_CONTEXTMENU                   = 0x007B,
	WM_STYLECHANGING                 = 0x007C,
	WM_STYLECHANGED                  = 0x007D,
	WM_DISPLAYCHANGE                 = 0x007E,
	WM_GETICON                       = 0x007F,
	WM_SETICON                       = 0x0080,
	WM_NCCREATE                      = 0x0081,
	WM_NCDESTROY                     = 0x0082,
	WM_NCCALCSIZE                    = 0x0083,
	WM_NCHITTEST                     = 0x0084,
	WM_NCPAINT                       = 0x0085,
	WM_NCACTIVATE                    = 0x0086,
	WM_GETDLGCODE                    = 0x0087,
	WM_SYNCPAINT                     = 0x0088,
	WM_NCMOUSEMOVE                   = 0x00A0,
	WM_NCLBUTTONDOWN                 = 0x00A1,
	WM_NCLBUTTONUP                   = 0x00A2,
	WM_NCLBUTTONDBLCLK               = 0x00A3,
	WM_NCRBUTTONDOWN                 = 0x00A4,
	WM_NCRBUTTONUP                   = 0x00A5,
	WM_NCRBUTTONDBLCLK               = 0x00A6,
	WM_NCMBUTTONDOWN                 = 0x00A7,
	WM_NCMBUTTONUP                   = 0x00A8,
	WM_NCMBUTTONDBLCLK               = 0x00A9,
	WM_NCXBUTTONDOWN                 = 0x00AB,
	WM_NCXBUTTONUP                   = 0x00AC,
	WM_NCXBUTTONDBLCLK               = 0x00AD,
   WM_INPUT                         = 0x00FF,
	WM_KEYDOWN                       = 0x0100,
	WM_KEYUP                         = 0x0101,
	WM_CHAR                          = 0x0102,
	WM_DEADCHAR                      = 0x0103,
	WM_SYSKEYDOWN                    = 0x0104,
	WM_SYSKEYUP                      = 0x0105,
	WM_SYSCHAR                       = 0x0106,
	WM_SYSDEADCHAR                   = 0x0107,
	WM_UNICHAR                       = 0x0109,
	WM_IME_STARTCOMPOSITION          = 0x010D,
	WM_IME_ENDCOMPOSITION            = 0x010E,
	WM_IME_COMPOSITION               = 0x010F,
	WM_INITDIALOG                    = 0x0110,
	WM_COMMAND                       = 0x0111,
	WM_SYSCOMMAND                    = 0x0112,
	WM_TIMER                         = 0x0113, --id, callback
	WM_HSCROLL                       = 0x0114,
	WM_VSCROLL                       = 0x0115,
	WM_INITMENU                      = 0x0116,
	WM_INITMENUPOPUP                 = 0x0117,
	WM_MENUSELECT                    = 0x011F,
	WM_MENUCHAR                      = 0x0120,
	WM_ENTERIDLE                     = 0x0121,
	WM_MENURBUTTONUP                 = 0x0122,
	WM_MENUDRAG                      = 0x0123,
	WM_MENUGETOBJECT                 = 0x0124,
	WM_UNINITMENUPOPUP               = 0x0125,
	WM_MENUCOMMAND                   = 0x0126,
	WM_CHANGEUISTATE                 = 0x0127,
	WM_UPDATEUISTATE                 = 0x0128,
	WM_QUERYUISTATE                  = 0x0129,
	WM_CTLCOLORMSGBOX                = 0x0132,
	WM_CTLCOLOREDIT                  = 0x0133,
	WM_CTLCOLORLISTBOX               = 0x0134,
	WM_CTLCOLORBTN                   = 0x0135,
	WM_CTLCOLORDLG                   = 0x0136,
	WM_CTLCOLORSCROLLBAR             = 0x0137,
	WM_CTLCOLORSTATIC                = 0x0138,
	MN_GETHMENU                      = 0x01E1,
	WM_MOUSEMOVE                     = 0x0200,
	WM_LBUTTONDOWN                   = 0x0201,
	WM_LBUTTONUP                     = 0x0202,
	WM_LBUTTONDBLCLK                 = 0x0203,
	WM_RBUTTONDOWN                   = 0x0204,
	WM_RBUTTONUP                     = 0x0205,
	WM_RBUTTONDBLCLK                 = 0x0206,
	WM_MBUTTONDOWN                   = 0x0207,
	WM_MBUTTONUP                     = 0x0208,
	WM_MBUTTONDBLCLK                 = 0x0209,
	WM_MOUSEWHEEL                    = 0x020A,
	WM_XBUTTONDOWN                   = 0x020B,
	WM_XBUTTONUP                     = 0x020C,
	WM_XBUTTONDBLCLK                 = 0x020D,
	WM_MOUSEHWHEEL                   = 0x020E,
	WM_PARENTNOTIFY                  = 0x0210,
	WM_ENTERMENULOOP                 = 0x0211,
	WM_EXITMENULOOP                  = 0x0212,
	WM_NEXTMENU                      = 0x0213,
	WM_SIZING                        = 0x0214,
	WM_CAPTURECHANGED                = 0x0215,
	WM_MOVING                        = 0x0216,
	WM_POWERBROADCAST                = 0x0218,
	WM_DEVICECHANGE                  = 0x0219,
	WM_MDICREATE                     = 0x0220,
	WM_MDIDESTROY                    = 0x0221,
	WM_MDIACTIVATE                   = 0x0222,
	WM_MDIRESTORE                    = 0x0223,
	WM_MDINEXT                       = 0x0224,
	WM_MDIMAXIMIZE                   = 0x0225,
	WM_MDITILE                       = 0x0226,
	WM_MDICASCADE                    = 0x0227,
	WM_MDIICONARRANGE                = 0x0228,
	WM_MDIGETACTIVE                  = 0x0229,
	WM_MDISETMENU                    = 0x0230,
	WM_ENTERSIZEMOVE                 = 0x0231,
	WM_EXITSIZEMOVE                  = 0x0232,
	WM_DROPFILES                     = 0x0233,
	WM_MDIREFRESHMENU                = 0x0234,
	WM_IME_SETCONTEXT                = 0x0281,
	WM_IME_NOTIFY                    = 0x0282,
	WM_IME_CONTROL                   = 0x0283,
	WM_IME_COMPOSITIONFULL           = 0x0284,
	WM_IME_SELECT                    = 0x0285,
	WM_IME_CHAR                      = 0x0286,
	WM_IME_REQUEST                   = 0x0288,
	WM_IME_KEYDOWN                   = 0x0290,
	WM_IME_KEYUP                     = 0x0291,
	WM_MOUSEHOVER                    = 0x02A1,
	WM_MOUSELEAVE                    = 0x02A3,
	WM_NCMOUSEHOVER                  = 0x02A0,
	WM_NCMOUSELEAVE                  = 0x02A2,
	WM_WTSSESSION_CHANGE             = 0x02B1,
	WM_CUT                           = 0x0300,
	WM_COPY                          = 0x0301,
	WM_PASTE                         = 0x0302,
	WM_CLEAR                         = 0x0303,
	WM_UNDO                          = 0x0304,
	WM_RENDERFORMAT                  = 0x0305,
	WM_RENDERALLFORMATS              = 0x0306,
	WM_DESTROYCLIPBOARD              = 0x0307,
	WM_DRAWCLIPBOARD                 = 0x0308,
	WM_PAINTCLIPBOARD                = 0x0309,
	WM_VSCROLLCLIPBOARD              = 0x030A,
	WM_SIZECLIPBOARD                 = 0x030B,
	WM_ASKCBFORMATNAME               = 0x030C,
	WM_CHANGECBCHAIN                 = 0x030D,
	WM_HSCROLLCLIPBOARD              = 0x030E,
	WM_QUERYNEWPALETTE               = 0x030F,
	WM_PALETTEISCHANGING             = 0x0310,
	WM_PALETTECHANGED                = 0x0311,
	WM_HOTKEY                        = 0x0312,
	WM_PRINT                         = 0x0317,
	WM_PRINTCLIENT                   = 0x0318,
	WM_APPCOMMAND                    = 0x0319,
	WM_THEMECHANGED                  = 0x031A,
}

--message crackers

WM_DECODERS = {} --{wm_name = function(wParam, lParam) -> <specific return values>}
local WM = WM_DECODERS

function DecodeMessage(WM, wParam, lParam) --returns decoded results...
	local decoder = WM_DECODERS[WM_NAMES[WM]] or pass
	return decoder(wParam, lParam)
end

-- window events

WA_INACTIVE     = 0
WA_ACTIVE       = 1
WA_CLICKACTIVE  = 2

function WM.WM_ACTIVATE(wParam, lParam)
	local WA, minimized = splitlong(wParam)
	return WA, minimized ~= 0, ptr(ffi.cast('HWND', lParam)) --WA_*, minimized, other_window
end

-- window sizing

function WM.WM_SIZING(wParam, lParam)
	return ffi.cast('RECT*', lParam)
end

ffi.cdef[[
typedef struct tagMINMAXINFO {
    POINT ptReserved;
    SIZE  maximized_size;
	 POINT maximized_pos;
	 SIZE  min_size;
	 SIZE  max_size;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;

typedef struct tagWINDOWPOS {
    HWND    hwnd;
    HWND    hwndInsertAfter;
    int     x;
    int     y;
	 int     w;
    int     h;
    UINT    flags;
} WINDOWPOS, *LPWINDOWPOS, *PWINDOWPOS;
]]

function WM.WM_GETMINMAXINFO(wParam, lParam)
	return ffi.cast('MINMAXINFO*', lParam)
end

function WM.WM_WINDOWPOSCHANGING(wParam, lParam)
	return ffi.cast('WINDOWPOS*', lParam)
end

WM.WM_WINDOWPOSCHANGED = WM.WM_WINDOWPOSCHANGING

-- controls/wm_*command

function WM.WM_COMMAND(wParam, lParam)
	local id, command = splitlong(wParam)
	if lParam == 0 then
		if command == 0 then --menu
			return 'menu', id
		elseif command == 1 then --accelerator
			return 'accelerator', id
		else
			assert(false)
		end
	else
		return 'control', id, command, checkh(ffi.cast('HWND', lParam))
	end
end

function WM.WM_MENUCOMMAND(wParam, lParam)
	return checkh(ffi.cast('HMENU', lParam)), countfrom1(wParam)
end

SC_SIZE          = 0xF000
SC_MOVE          = 0xF010
SC_DRAGMOVE      = 0xF012
SC_MINIMIZE      = 0xF020
SC_MAXIMIZE      = 0xF030
SC_NEXTWINDOW    = 0xF040
SC_PREVWINDOW    = 0xF050
SC_CLOSE         = 0xF060
SC_VSCROLL       = 0xF070
SC_HSCROLL       = 0xF080
SC_MOUSEMENU     = 0xF090
SC_KEYMENU       = 0xF100
SC_ARRANGE       = 0xF110
SC_RESTORE       = 0xF120
SC_TASKLIST      = 0xF130
SC_SCREENSAVE    = 0xF140
SC_HOTKEY        = 0xF150
SC_DEFAULT       = 0xF160
SC_MONITORPOWER  = 0xF170
SC_CONTEXTHELP   = 0xF180
SC_SEPARATOR     = 0xF00F
SCF_ISSECURE     = 0x00000001

function WM.WM_SYSCOMMAND(wParam, lParam)
	local SC = bit.band(wParam, 0xfff0)
	if SC == SC_KEYMENU then
		return SC, lParam --SC, char_code
	else
		return SC, splitsigned(lParam) --SC, x, y
	end
end

--controls/wm_compareitem

ffi.cdef[[
typedef struct tagCOMPAREITEMSTRUCT {
    UINT        CtlType;
    UINT        id;
    HWND        hwnd;
    UINT        i1;
    ULONG_PTR   item1_user_data;
    UINT        i2;
    ULONG_PTR   item2_user_data;
    DWORD       dwLocaleId;
} COMPAREITEMSTRUCT,  *PCOMPAREITEMSTRUCT,  *LPCOMPAREITEMSTRUCT;
]]

function WM.WM_COMPAREITEM(wParam, lParam) --must return -1 if a < b, 1 if a > b and 0 if a == b
	return checkh(ffi.cast('HWND', wParam)), checkh(ffi.cast('COMPAREITEMSTRUCT*', lParam))
end

--controls/wm_notify

NM_FIRST = 2^32

WM_NOTIFY_NAMES = constants{ --{code_number = code_name}
	NM_CUSTOMDRAW = NM_FIRST-12,
}
WM_NOTIFY_DECODERS = {} --{code_name = function(hdr, wParam) -> <specific return values>}
local WMN = WM_NOTIFY_DECODERS

ffi.cdef[[
typedef struct tagNMHDR
{
    HWND      hwndFrom;
    UINT_PTR  idFrom;
    UINT      code;
}   NMHDR;
typedef NMHDR *LPNMHDR;
]]

function WM.WM_NOTIFY(wParam, lParam) --return hwnd, code, decoded message...
	local hdr = ffi.cast('NMHDR*', lParam)
	local code_name = WM_NOTIFY_NAMES[hdr.code]
	local decoder = WM_NOTIFY_DECODERS[code_name] or pass
	return hdr.hwndFrom, hdr.code, decoder(hdr, wParam)
end

ffi.cdef[[
typedef struct tagNMCUSTOMDRAWINFO
{
    NMHDR hdr;
    DWORD stage;
    HDC hdc;
    RECT rc;
    DWORD_PTR dwItemSpec;
    UINT  uItemState;
    LPARAM lItemlParam;
} NMCUSTOMDRAW, *LPNMCUSTOMDRAW;
]]

--stage flags
CDDS_PREPAINT           = 0x00000001
CDDS_POSTPAINT          = 0x00000002
CDDS_PREERASE           = 0x00000003
CDDS_POSTERASE          = 0x00000004
CDDS_ITEM               = 0x00010000 --individual item specific
CDDS_ITEMPREPAINT       = bit.bor(CDDS_ITEM, CDDS_PREPAINT)
CDDS_ITEMPOSTPAINT      = bit.bor(CDDS_ITEM, CDDS_POSTPAINT)
CDDS_ITEMPREERASE       = bit.bor(CDDS_ITEM, CDDS_PREERASE)
CDDS_ITEMPOSTERASE      = bit.bor(CDDS_ITEM, CDDS_POSTERASE)
CDDS_SUBITEM            = 0x00020000

--return flags
CDRF_DODEFAULT          = 0x00000000
CDRF_NEWFONT            = 0x00000002
CDRF_SKIPDEFAULT        = 0x00000004
CDRF_DOERASE            = 0x00000008 -- draw the background
CDRF_SKIPPOSTPAINT      = 0x00000100 -- don't draw the focus rect
CDRF_NOTIFYPOSTPAINT    = 0x00000010
CDRF_NOTIFYITEMDRAW     = 0x00000020
CDRF_NOTIFYSUBITEMDRAW  = 0x00000020 --flags are the same, we can distinguish by context
CDRF_NOTIFYPOSTERASE    = 0x00000040


function WMN.NM_CUSTOMDRAW(hdr)
	return ffi.cast('NMCUSTOMDRAW*', hdr)
end

-- mouse input

HTERROR             = -2
HTTRANSPARENT       = -1
HTNOWHERE           = 0
HTCLIENT            = 1
HTCAPTION           = 2
HTSYSMENU           = 3
HTGROWBOX           = 4
HTSIZE              = HTGROWBOX
HTMENU              = 5
HTHSCROLL           = 6
HTVSCROLL           = 7
HTMINBUTTON         = 8
HTMAXBUTTON         = 9
HTLEFT              = 10
HTRIGHT             = 11
HTTOP               = 12
HTTOPLEFT           = 13
HTTOPRIGHT          = 14
HTBOTTOM            = 15
HTBOTTOMLEFT        = 16
HTBOTTOMRIGHT       = 17
HTBORDER            = 18
HTREDUCE            = HTMINBUTTON
HTZOOM              = HTMAXBUTTON
HTSIZEFIRST         = HTLEFT
HTSIZELAST          = HTBOTTOMRIGHT
HTOBJECT            = 19
HTCLOSE             = 20
HTHELP              = 21

function WM.WM_NCHITTEST(wParam, lParam)
	return splitsigned(lParam) --x, y; must return HT*
end

MK_LBUTTON          = 0x0001
MK_RBUTTON          = 0x0002
MK_SHIFT            = 0x0004
MK_CONTROL          = 0x0008
MK_MBUTTON          = 0x0010
MK_XBUTTON1         = 0x0020
MK_XBUTTON2         = 0x0040

local buttons_bitmask = bitmask{
	lbutton = MK_LBUTTON,
	rbutton = MK_RBUTTON,
	shift = MK_SHIFT,
	control = MK_CONTROL,
	mbutton = MK_MBUTTON,
	xbutton1 = MK_XBUTTON1,
	xbutton2 = MK_XBUTTON2,
}

function WM.WM_LBUTTONDBLCLK(wParam, lParam)
	local x, y = splitsigned(lParam)
	return x, y, buttons_bitmask:get(wParam)
end

WM.WM_LBUTTONDOWN = WM.WM_LBUTTONDBLCLK
WM.WM_LBUTTONUP = WM.WM_LBUTTONDBLCLK
WM.WM_MBUTTONDBLCLK = WM.WM_LBUTTONDBLCLK
WM.WM_MBUTTONDOWN = WM.WM_LBUTTONDBLCLK
WM.WM_MBUTTONUP = WM.WM_LBUTTONDBLCLK
WM.WM_MOUSEHOVER = WM.WM_LBUTTONDBLCLK
WM.WM_MOUSEHWHEEL = WM.WM_LBUTTONDBLCLK
WM.WM_MOUSEMOVE = WM.WM_LBUTTONDBLCLK
WM.WM_RBUTTONDBLCLK = WM.WM_LBUTTONDBLCLK
WM.WM_RBUTTONDOWN = WM.WM_LBUTTONDBLCLK
WM.WM_RBUTTONUP = WM.WM_LBUTTONDBLCLK

function WM.WM_MOUSEWHEEL(wParam, lParam)
	local x, y = splitsigned(lParam)
	local buttons, delta = splitsigned(wParam)
	return x, y, buttons_bitmask:get(buttons), delta
end

XBUTTON1 = 0x0001
XBUTTON2 = 0x0002

local xbuttons_bitmask = bitmask{
	xbutton1 = XBUTTON1,
	xbutton2 = XBUTTON2,
}

function WM.WM_XBUTTONDBLCLK(wParam, lParam)
	local x, y = splitsigned(lParam)
	local MK, XBUTTON = splitlong(wParam)
	return x, y, xbuttons_bitmask:get(XBUTTON), buttons_bitmask:get(MK)
end

WM.WM_XBUTTONDOWN = WM.WM_XBUTTONDBLCLK
WM.WM_XBUTTONUP = WM.WM_XBUTTONDBLCLK

function WM.WM_NCLBUTTONDBLCLK(wParam, lParam)
	local x, y = splitsigned(lParam)
	return x, y, wParam --x, y, HT*
end
WM.WM_NCLBUTTONDOWN = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCLBUTTONUP = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCMBUTTONDBLCLK = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCMBUTTONDOWN = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCMBUTTONUP = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCMOUSEHOVER = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCMOUSEMOVE = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCRBUTTONDBLCLK = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCRBUTTONDOWN = WM.WM_NCLBUTTONDBLCLK
WM.WM_NCRBUTTONUP = WM.WM_NCLBUTTONDBLCLK

WM.WM_NCXBUTTONDBLCLK = WM.WM_XBUTTONDBLCLK --HT*, XBUTTON*, x, y
WM.WM_NCXBUTTONDOWN = WM.WM_NCXBUTTONDBLCLK
WM.WM_NCXBUTTONUP = WM.WM_NCXBUTTONDBLCLK

MA_ACTIVATE         = 1
MA_ACTIVATEANDEAT   = 2
MA_NOACTIVATE       = 3
MA_NOACTIVATEANDEAT = 4

function WM.WM_MOUSEACTIVATE(wParam, lParam)
	local HT, MK = splitlong(lParam)
	return ffi.cast('HWND', wParam), HT, buttons_bitmask:get(MK) --must return MA_*
end


function WM.WM_SETCURSOR(wParam, lParam)
	local HT, id = splitlong(lParam)
	return ffi.cast('HWND', wParam), HT, id
end

--keyboard input

require'winapi.vkcodes'

local key_bitmask = bitmask{
	extended_key = 2^24,
	context_code = 2^29, --0 for WM_KEYDOWN
	prev_key_state = 2^30,
	transition_state = 2^31, --0 for WM_KEYDOWN
}

local function get_bitrange(from,b1,b2)
	return bit.band(bit.rshift(from, b1), 2^(b2-b1+1)-1)
end

local function key_flags(lParam)
	local t = key_bitmask:get(lParam)
	t.repeat_count = get_bitrange(lParam, 0, 15)
	t.scan_code = get_bitrange(lParam, 16, 23)
	return t
end

function WM.WM_KEYDOWN(wParam, lParam)
	return wParam, key_flags(lParam) --VK_*, flags
end

WM.WM_KEYUP = WM.WM_KEYDOWN
WM.WM_SYSKEYDOWN = WM.WM_KEYDOWN
WM.WM_SYSKEYUP = WM.WM_KEYDOWN

function WM.WM_CHAR(wParam, lParam)
	return mbs(ffi.new('WCHAR[1]', wParam)), key_flags(lParam)
end

WM.WM_UNICHAR = WM.WM_CHAR --TODO: support characters outside the BMP
WM.WM_SYSCHAR = WM.WM_CHAR
WM.WM_DEADCHAR = WM.WM_CHAR
WM.WM_SYSDEADCHAR = WM.WM_CHAR

