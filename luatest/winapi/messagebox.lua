--proc/messagebox: standard message box dialog.
setfenv(1, require'winapi')
require'winapi.winuser'
require'winapi.comctl'

ffi.cdef[[
int MessageBoxExW(
	  HWND hWnd,
	  LPCWSTR lpText,
	  LPCWSTR lpCaption,
	  UINT uType,
	  WORD wLanguageId);

LONG GetDialogBaseUnits();
]]

MB_OK                        = 0x00000000
MB_OKCANCEL                  = 0x00000001
MB_ABORTRETRYIGNORE          = 0x00000002
MB_YESNOCANCEL               = 0x00000003
MB_YESNO                     = 0x00000004
MB_RETRYCANCEL               = 0x00000005
MB_CANCELTRYCONTINUE         = 0x00000006
MB_ICONHAND                  = 0x00000010
MB_ICONQUESTION              = 0x00000020
MB_ICONEXCLAMATION           = 0x00000030
MB_ICONASTERISK              = 0x00000040
MB_USERICON                  = 0x00000080
MB_ICONWARNING               = MB_ICONEXCLAMATION
MB_ICONERROR                 = MB_ICONHAND
MB_ICONINFORMATION           = MB_ICONASTERISK
MB_ICONSTOP                  = MB_ICONHAND
MB_DEFBUTTON1                = 0x00000000
MB_DEFBUTTON2                = 0x00000100
MB_DEFBUTTON3                = 0x00000200
MB_DEFBUTTON4                = 0x00000300
MB_APPLMODAL                 = 0x00000000
MB_SYSTEMMODAL               = 0x00001000
MB_TASKMODAL                 = 0x00002000
MB_HELP                      = 0x00004000
MB_NOFOCUS                   = 0x00008000
MB_SETFOREGROUND             = 0x00010000
MB_DEFAULT_DESKTOP_ONLY      = 0x00020000
MB_TOPMOST                   = 0x00040000
MB_RIGHT                     = 0x00080000
MB_RTLREADING                = 0x00100000
MB_SERVICE_NOTIFICATION      = 0x00200000
MB_SERVICE_NOTIFICATION_NT3X = 0x00040000
MB_TYPEMASK                  = 0x0000000F
MB_ICONMASK                  = 0x000000F0
MB_DEFMASK                   = 0x00000F00
MB_MODEMASK                  = 0x00003000
MB_MISCMASK                  = 0x0000C000

IDOK                 = 1
IDCANCEL             = 2
IDABORT              = 3
IDRETRY              = 4
IDIGNORE             = 5
IDYES                = 6
IDNO                 = 7
IDCLOSE              = 8
IDHELP               = 9
IDTRYAGAIN           = 10
IDCONTINUE           = 11
IDTIMEOUT            = 32000

function MessageBox(text, title, MB, parent, langid)
	return checknz(C.MessageBoxExW(parent, wcs(text), wcs(title), flags(MB), langid or 0))
end

function GetDialogBaseUnits()
	local long = C.GetDialogBaseUnits()
	return bit.band(long, 0xFFFF), bit.rshift(long, 16)
end

if not ... then
MessageBox('Noisy whippersnappers', 'Alert', 'MB_ICONEXCLAMATION | MB_CANCELTRYCONTINUE')
end
