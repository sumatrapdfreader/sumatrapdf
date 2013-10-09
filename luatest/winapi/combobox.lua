--proc/combobox: standard (old, not comctl-based) combobox control.
setfenv(1, require'winapi')
require'winapi.window'

--creation

CB_OKAY             = 0
CB_ERR              = -1
CB_ERRSPACE         = -2

CBS_SIMPLE             = 0x0001
CBS_DROPDOWN           = 0x0002
CBS_DROPDOWNLIST       = 0x0003
CBS_OWNERDRAWFIXED     = 0x0010
CBS_OWNERDRAWVARIABLE  = 0x0020
CBS_AUTOHSCROLL        = 0x0040
CBS_OEMCONVERT         = 0x0080
CBS_SORT               = 0x0100
CBS_HASSTRINGS         = 0x0200
CBS_NOINTEGRALHEIGHT   = 0x0400
CBS_DISABLENOSCROLL    = 0x0800
CBS_UPPERCASE          = 0x2000
CBS_LOWERCASE          = 0x4000

--commands

CB_GETEDITSEL                = 0x0140
CB_LIMITTEXT                 = 0x0141
CB_SETEDITSEL                = 0x0142
CB_ADDSTRING                 = 0x0143
CB_DELETESTRING              = 0x0144
CB_DIR                       = 0x0145
CB_GETCOUNT                  = 0x0146
CB_GETCURSEL                 = 0x0147
CB_GETLBTEXT                 = 0x0148 --don't look for CB_SETLBTEXT :)
CB_GETLBTEXTLEN              = 0x0149
CB_INSERTSTRING              = 0x014A
CB_RESETCONTENT              = 0x014B
CB_FINDSTRING                = 0x014C
CB_SELECTSTRING              = 0x014D
CB_SETCURSEL                 = 0x014E
CB_SHOWDROPDOWN              = 0x014F
CB_GETITEMDATA               = 0x0150
CB_SETITEMDATA               = 0x0151
CB_GETDROPPEDCONTROLRECT     = 0x0152
CB_SETITEMHEIGHT             = 0x0153
CB_GETITEMHEIGHT             = 0x0154
CB_SETEXTENDEDUI             = 0x0155
CB_GETEXTENDEDUI             = 0x0156
CB_GETDROPPEDSTATE           = 0x0157
CB_FINDSTRINGEXACT           = 0x0158
CB_SETLOCALE                 = 0x0159
CB_GETLOCALE                 = 0x015A
CB_GETTOPINDEX               = 0x015b
CB_SETTOPINDEX               = 0x015c
CB_GETHORIZONTALEXTENT       = 0x015d
CB_SETHORIZONTALEXTENT       = 0x015e
CB_GETDROPPEDWIDTH           = 0x015f
CB_SETDROPPEDWIDTH           = 0x0160
CB_INITSTORAGE               = 0x0161
CB_MULTIPLEADDSTRING         = 0x0163
CB_GETCOMBOBOXINFO           = 0x0164
CB_MSGMAX                    = 0x0165

function ComboBox_AddString(hwnd, s) --returns index
	return countfrom1(checkpoz(SNDMSG(hwnd, CB_ADDSTRING, 0, wcs(s))))
end

function ComboBox_InsertString(hwnd, i, s) --returns index
	return countfrom1(checkpoz(SNDMSG(hwnd, CB_INSERTSTRING, countfrom0(i), wcs(s))))
end

function ComboBox_DeleteString(hwnd, i) --returns count
	return checkpoz(SNDMSG(hwnd, CB_DELETESTRING, countfrom0(i)))
end

function ComboBox_GetString(hwnd, i, buf) --there's no SetString
	local ws = WCS(buf or checkpoz(SNDMSG(hwnd, CB_GETLBTEXTLEN, countfrom0(i))))
	checkpoz(SNDMSG(hwnd, CB_GETLBTEXT, countfrom0(i), ws))
	return buf or mbs(ws)
end

function ComboBox_GetCount(hwnd)
	return checkpoz(SNDMSG(hwnd, CB_GETCOUNT))
end

function ComboBox_Reset(hwnd) --clears the listbox and the edit box
	return checkz(SNDMSG(hwnd, CB_RESETCONTENT))
end

function ComboBox_SetCurSel(hwnd, i) --returns index
	return countfrom1(checkpoz(SNDMSG(hwnd, CB_SETCURSEL, countfrom0(i))))
end

function ComboBox_GetCurSel(hwnd)
	return countfrom1(SNDMSG(hwnd, CB_GETCURSEL))
end

function ComboBox_SetExtendedUI(hwnd, extended)
	return checkz(SNDMSG(hwnd, CB_SETEXTENDEDUI, extended))
end

function ComboBox_LimitText(hwnd, cchMax)
	return checktrue(SNDMSG(hwnd, CB_LIMITTEXT, cchMax))
end

function ComboBox_SetItemHeight(hwnd, item, height)
	item = (item == 'edit' and -1)  or (item == 'list' and 0) or countfrom0(item)
	checkpoz(SNDMSG(hwnd, CB_SETITEMHEIGHT, item, height)) --all we know is that -1 is an error
end

function ComboBox_GetItemHeight(hwnd, item)
	item = (item == 'edit' and -1)  or (item == 'list' and 0) or countfrom0(item)
	return checkpoz(SNDMSG(hwnd, CB_GETITEMHEIGHT, item))
end

function ComboBox_GetEditSel(hwnd)
	local p1, p2 = ffi.new'DWORD[1]', ffi.new'DWORD[1]'
	SNDMSG(hwnd, CB_GETEDITSEL, p1, p2)
	return countfrom1(p1[0]), countfrom1(p2[0])
end

function CheckBox_SetEditSel(hwnd, i, j)
	checktrue(SNDMSG(hwnd, CB_SETEDITSEL, 0, MAKELPARAM(countfrom0(i), countfrom0(j))))
end

function ComboBox_ShowDropdown(hwnd, show)
	checktrue(SNDMSG(hwnd, CB_SHOWDROPDOWN, show))
end

function ComboBox_DroppedDown(hwnd)
	return SNDMSG(hwnd, CB_GETDROPPEDSTATE) == 1
end

function ComboBox_SetDroppedWidth(hwnd, w) --returns width
	checkpoz(SNDMSG(hwnd, CB_SETDROPPEDWIDTH, w))
end

function ComboBox_GetDroppedWidth(hwnd)
	return checkpoz(SNDMSG(hwnd, CB_GETDROPPEDWIDTH))
end

--notifications

CBN_ERRSPACE         = -1
CBN_SELCHANGE        = 1
CBN_DBLCLK           = 2
CBN_SETFOCUS         = 3
CBN_KILLFOCUS        = 4
CBN_EDITCHANGE       = 5
CBN_EDITUPDATE       = 6
CBN_DROPDOWN         = 7
CBN_CLOSEUP          = 8
CBN_SELENDOK         = 9
CBN_SELENDCANCEL     = 10

--TODO
--[[
WM_COMPAREITEM
WM_DRAWITEM
WM_MEASUREITEM
]]

