--proc/color: standard color brushes
setfenv(1, require'winapi')
require'winapi.winuser'

CTLCOLOR_MSGBOX          = ffi.cast('HBRUSH', 0)
CTLCOLOR_EDIT            = ffi.cast('HBRUSH', 1)
CTLCOLOR_LISTBOX         = ffi.cast('HBRUSH', 2)
CTLCOLOR_BTN             = ffi.cast('HBRUSH', 3)
CTLCOLOR_DLG             = ffi.cast('HBRUSH', 4)
CTLCOLOR_SCROLLBAR       = ffi.cast('HBRUSH', 5)
CTLCOLOR_STATIC          = ffi.cast('HBRUSH', 6)
CTLCOLOR_MAX             = ffi.cast('HBRUSH', 7)

COLOR_SCROLLBAR          = ffi.cast('HBRUSH', 0)
COLOR_BACKGROUND         = ffi.cast('HBRUSH', 1)
COLOR_ACTIVECAPTION      = ffi.cast('HBRUSH', 2)
COLOR_INACTIVECAPTION    = ffi.cast('HBRUSH', 3)
COLOR_MENU               = ffi.cast('HBRUSH', 4)
COLOR_WINDOW             = ffi.cast('HBRUSH', 5)
COLOR_WINDOWFRAME        = ffi.cast('HBRUSH', 6)
COLOR_MENUTEXT           = ffi.cast('HBRUSH', 7)
COLOR_WINDOWTEXT         = ffi.cast('HBRUSH', 8)
COLOR_CAPTIONTEXT        = ffi.cast('HBRUSH', 9)
COLOR_ACTIVEBORDER       = ffi.cast('HBRUSH', 10)
COLOR_INACTIVEBORDER     = ffi.cast('HBRUSH', 11)
COLOR_APPWORKSPACE       = ffi.cast('HBRUSH', 12)
COLOR_HIGHLIGHT          = ffi.cast('HBRUSH', 13)
COLOR_HIGHLIGHTTEXT      = ffi.cast('HBRUSH', 14)
COLOR_BTNFACE            = ffi.cast('HBRUSH', 15)
COLOR_BTNSHADOW          = ffi.cast('HBRUSH', 16)
COLOR_GRAYTEXT           = ffi.cast('HBRUSH', 17)
COLOR_BTNTEXT            = ffi.cast('HBRUSH', 18)
COLOR_INACTIVECAPTIONTEXT= ffi.cast('HBRUSH', 19)
COLOR_BTNHIGHLIGHT       = ffi.cast('HBRUSH', 20)

COLOR_3DDKSHADOW         = ffi.cast('HBRUSH', 21)
COLOR_3DLIGHT            = ffi.cast('HBRUSH', 22)
COLOR_INFOTEXT           = ffi.cast('HBRUSH', 23)
COLOR_INFOBK             = ffi.cast('HBRUSH', 24)

COLOR_HOTLIGHT           = ffi.cast('HBRUSH', 26)
COLOR_GRADIENTACTIVECAPTION = ffi.cast('HBRUSH', 27)
COLOR_GRADIENTINACTIVECAPTION = ffi.cast('HBRUSH', 28)

COLOR_MENUHILIGHT        = ffi.cast('HBRUSH', 29)
COLOR_MENUBAR            = ffi.cast('HBRUSH', 30)

COLOR_DESKTOP            = COLOR_BACKGROUND
COLOR_3DFACE             = COLOR_BTNFACE
COLOR_3DSHADOW           = COLOR_BTNSHADOW
COLOR_3DHIGHLIGHT        = COLOR_BTNHIGHLIGHT
COLOR_3DHILIGHT          = COLOR_BTNHIGHLIGHT
COLOR_BTNHILIGHT         = COLOR_BTNHIGHLIGHT

