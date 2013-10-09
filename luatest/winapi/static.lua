--proc/static: standard static (aka label, text) control.
setfenv(1, require'winapi')
require'winapi.window'

--creation

WC_STATIC               = 'Static'

--text styles
SS_LEFT              = 0x00000000
SS_CENTER            = 0x00000001
SS_RIGHT             = 0x00000002
SS_SIMPLE            = 0x0000000B
SS_LEFTNOWORDWRAP    = 0x0000000C
SS_NOPREFIX          = 0x00000080 -- Don't do "&" character translation

--image styles
SS_ICON              = 0x00000003
SS_BITMAP            = 0x0000000E
SS_ENHMETAFILE       = 0x0000000F

--shape styles
SS_BLACKRECT         = 0x00000004
SS_GRAYRECT          = 0x00000005
SS_WHITERECT         = 0x00000006
SS_BLACKFRAME        = 0x00000007
SS_GRAYFRAME         = 0x00000008
SS_WHITEFRAME        = 0x00000009
SS_ETCHEDHORZ        = 0x00000010
SS_ETCHEDVERT        = 0x00000011
SS_ETCHEDFRAME       = 0x00000012

--behavior styles
SS_NOTIFY            = 0x00000100

--other styles
SS_USERITEM          = 0x0000000A
SS_OWNERDRAW         = 0x0000000D
SS_TYPEMASK          = 0x0000001F
SS_REALSIZECONTROL   = 0x00000040
SS_CENTERIMAGE       = 0x00000200
SS_RIGHTJUST         = 0x00000400
SS_REALSIZEIMAGE     = 0x00000800
SS_SUNKEN            = 0x00001000
SS_EDITCONTROL       = 0x00002000
SS_ENDELLIPSIS       = 0x00004000
SS_PATHELLIPSIS      = 0x00008000
SS_WORDELLIPSIS      = 0x0000C000
SS_ELLIPSISMASK      = 0x0000C000

STATIC_DEFAULTS = {
	class = WC_STATIC,
	text = 'n/a',
	style = bit.bor(WS_CHILD, WS_VISIBLE),
	x = 10, y = 10, w = 100, h = 24,
}

function CreateStatic(info)
	info = update({}, STATIC_DEFAULTS, info)
	return CreateWindow(info)
end

--commands

STM_SETICON          = 0x0170
STM_GETICON          = 0x0171
STM_SETIMAGE         = 0x0172
STM_GETIMAGE         = 0x0173
STN_CLICKED          = 0
STN_DBLCLK           = 1
STN_ENABLE           = 2
STN_DISABLE          = 3
STM_MSGMAX           = 0x0174

Static_Enable = EnableWindow
Static_GetText = GetWindowText
Static_SetText = SetWindowText
function Static_SetIcon(st, icon)
	return SNDMSG(st, STM_SETICON, icon, 0,
							'HICON', nil, checkh, 'HICON')
end

function Static_GetIcon(st, icon)
	return SNDMSG(st, STM_GETICON, 0, 0,
							nil, nil, checkh, 'HICON')
end

--showcase

if not ... then
require'winapi.showcase'
local window = ShowcaseWindow()
local label = CreateStatic{text = 'Hi there my sweet lemon drops!', h = 100, parent = window.hwnd}
local label = CreateStatic{text = 'Hi there my sweet lemon drops!', h = 100, parent = window.hwnd}
MessageLoop()
end

