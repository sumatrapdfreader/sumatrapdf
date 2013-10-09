--proc/comctl: common controls API.
setfenv(1, require'winapi')
require'winapi.winuser'

comctl = ffi.load'comctl32'

--common types

ffi.cdef[[
struct IStream;
typedef struct _HIMAGELIST;
typedef struct _HIMAGELIST* HIMAGELIST;
]]

--initialization

ffi.cdef[[
typedef struct tagINITCOMMONCONTROLSEX {
    DWORD dwSize;
    DWORD dwICC;
} INITCOMMONCONTROLSEX, *LPINITCOMMONCONTROLSEX;

BOOL InitCommonControlsEx(LPINITCOMMONCONTROLSEX);
]]

ICC_LISTVIEW_CLASSES    = 0x00000001 -- listview, header
ICC_TREEVIEW_CLASSES    = 0x00000002 -- treeview, tooltips
ICC_BAR_CLASSES         = 0x00000004 -- toolbar, statusbar, trackbar, tooltips
ICC_TAB_CLASSES         = 0x00000008 -- tab, tooltips
ICC_UPDOWN_CLASS        = 0x00000010 -- updown
ICC_PROGRESS_CLASS      = 0x00000020 -- progress
ICC_HOTKEY_CLASS        = 0x00000040 -- hotkey
ICC_ANIMATE_CLASS       = 0x00000080 -- animate
ICC_WIN95_CLASSES       = 0x000000FF
ICC_DATE_CLASSES        = 0x00000100 -- month picker, date picker, time picker, updown
ICC_USEREX_CLASSES      = 0x00000200 -- comboex
ICC_COOL_CLASSES        = 0x00000400 -- rebar (coolbar) control
ICC_INTERNET_CLASSES    = 0x00000800
ICC_PAGESCROLLER_CLASS  = 0x00001000 -- page scroller
ICC_NATIVEFNTCTL_CLASS  = 0x00002000 -- native font control
ICC_STANDARD_CLASSES    = 0x00004000
ICC_LINK_CLASS          = 0x00008000

function InitCommonControlsEx(ICC)
	local icex = types.INITCOMMONCONTROLSEX()
	icex.dwSize = ffi.sizeof(icex)
	icex.dwICC = flags(ICC)
	checknz(comctl.InitCommonControlsEx(icex))
end

InitCommonControlsEx()

--common styles for rebar controls, toolbar controls, and status windows

CCS_TOP                 = 0x00000001
CCS_BOTTOM              = 0x00000003 --default on statusbar
CCS_VERT                = 0x00000080
CCS_LEFT                = bit.bor(CCS_VERT, CCS_TOP)
CCS_RIGHT               = bit.bor(CCS_VERT, CCS_BOTTOM)
CCS_NOMOVEY             = 0x00000002
CCS_NOMOVEX             = bit.bor(CCS_VERT, CCS_NOMOVEY)
CCS_NORESIZE            = 0x00000004
CCS_NOPARENTALIGN       = 0x00000008
CCS_NODIVIDER           = 0x00000040 --remove the top highlight line
CCS_ADJUSTABLE          = 0x00000020 --customizable

--commands

CCM_FIRST                = 0x2000
CCM_SETBKCOLOR           = (CCM_FIRST + 1) -- lParam is bkColor
CCM_SETCOLORSCHEME       = (CCM_FIRST + 2) -- lParam is color scheme
CCM_GETCOLORSCHEME       = (CCM_FIRST + 3) -- fills in COLORSCHEME pointed to by lParam
CCM_GETDROPTARGET        = (CCM_FIRST + 4)
CCM_SETUNICODEFORMAT     = (CCM_FIRST + 5)
CCM_GETUNICODEFORMAT     = (CCM_FIRST + 6)
CCM_SETVERSION           = (CCM_FIRST + 0x7)
CCM_GETVERSION           = (CCM_FIRST + 0x8)
CCM_SETNOTIFYWINDOW      = (CCM_FIRST + 0x9) -- wParam == hwndParent.
CCM_SETWINDOWTHEME       = (CCM_FIRST + 0xb)
CCM_DPISCALE             = (CCM_FIRST + 0xc) -- wParam == Awareness

function Comctl_SetVersion(hctl, version)
	return checkpoz(SNDMSG(hctl, version))
end

--assorted consants

I_IMAGECALLBACK          = -1
I_IMAGENONE              = -2

ODT_MENU        = 1
ODT_LISTBOX     = 2
ODT_COMBOBOX    = 3
ODT_BUTTON      = 4
ODT_STATIC      = 5

--showcase

if not ... then
InitCommonControlsEx(0xFFFF) --init all
end

