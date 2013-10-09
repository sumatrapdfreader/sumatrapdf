--proc/listbox: standard listbox control.
setfenv(1, require'winapi')
require'winapi.window'
require'winapi.comctl'

--creation

WC_LISTBOX             = 'ListBox'

LBS_NOTIFY             = 0x0001
LBS_SORT               = 0x0002
LBS_NOREDRAW           = 0x0004
LBS_MULTIPLESEL        = 0x0008
LBS_OWNERDRAWFIXED     = 0x0010
LBS_OWNERDRAWVARIABLE  = 0x0020
LBS_HASSTRINGS         = 0x0040
LBS_USETABSTOPS        = 0x0080
LBS_NOINTEGRALHEIGHT   = 0x0100
LBS_MULTICOLUMN        = 0x0200
LBS_WANTKEYBOARDINPUT  = 0x0400
LBS_EXTENDEDSEL        = 0x0800
LBS_DISABLENOSCROLL    = 0x1000
LBS_NODATA             = 0x2000
LBS_NOSEL              = 0x4000
LBS_COMBOBOX           = 0x8000
LBS_STANDARD           = bit.bor(LBS_NOTIFY, LBS_SORT, WS_VSCROLL, WS_BORDER)

--commands

update(WM_NAMES, constants{
	LB_ADDSTRING             = 0x0180,
	LB_INSERTSTRING          = 0x0181,
	LB_DELETESTRING          = 0x0182,
	LB_SELITEMRANGEEX        = 0x0183,
	LB_RESETCONTENT          = 0x0184,
	LB_SETSEL                = 0x0185,
	LB_SETCURSEL             = 0x0186,
	LB_GETSEL                = 0x0187,
	LB_GETCURSEL             = 0x0188,
	LB_GETTEXT               = 0x0189,
	LB_GETTEXTLEN            = 0x018A,
	LB_GETCOUNT              = 0x018B,
	LB_DIR                   = 0x018D,
	LB_GETTOPINDEX           = 0x018E,
	LB_FINDSTRING            = 0x018F,
	LB_GETSELCOUNT           = 0x0190,
	LB_GETSELITEMS           = 0x0191,
	LB_SETTABSTOPS           = 0x0192,
	LB_GETHORIZONTALEXTENT   = 0x0193,
	LB_SETHORIZONTALEXTENT   = 0x0194,
	LB_SETCOLUMNWIDTH        = 0x0195,
	LB_ADDFILE               = 0x0196,
	LB_SETTOPINDEX           = 0x0197,
	LB_GETITEMRECT           = 0x0198,
	LB_GETITEMDATA           = 0x0199,
	LB_SETITEMDATA           = 0x019A,
	LB_SELITEMRANGE          = 0x019B,
	LB_SETANCHORINDEX        = 0x019C,
	LB_GETANCHORINDEX        = 0x019D,
	LB_SETCARETINDEX         = 0x019E,
	LB_GETCARETINDEX         = 0x019F,
	LB_SETITEMHEIGHT         = 0x01A0,
	LB_GETITEMHEIGHT         = 0x01A1,
	LB_FINDSTRINGEXACT       = 0x01A2,
	LB_SETLOCALE             = 0x01A5,
	LB_GETLOCALE             = 0x01A6,
	LB_SETCOUNT              = 0x01A7,
	LB_INITSTORAGE           = 0x01A8,
	LB_ITEMFROMPOINT         = 0x01A9,
	LB_MULTIPLEADDSTRING     = 0x01B1,
	LB_GETLISTBOXINFO        = 0x01B2,
})

function ListBox_AddString(hwnd, s) --returns index
	return countfrom1(checkpoz(SNDMSG(hwnd, LB_ADDSTRING, 0, wcs(s))))
end

function ListBox_InsertString(hwnd, i, s) --returns index
	return countfrom1(checkpoz(SNDMSG(hwnd, LB_INSERTSTRING, countfrom0(i), wcs(s))))
end

function ListBox_DeleteString(hwnd, i) --returns count
	return checkpoz(SNDMSG(hwnd, LB_DELETESTRING, countfrom0(i)))
end

function ListBox_GetCount(hwnd)
	return SNDMSG(hwnd, LB_GETCOUNT)
end

function ListBox_ResetContent(hwnd)
	checknz(SNDMSG(hwnd, LB_RESETCONTENT))
end

function ListBox_GetString(hwnd, i, buf)
	local ws = WCS(buf or checkpoz(SNDMSG(hwnd, LB_GETTEXTLEN, countfrom0(i))))
	checkpoz(SNDMSG(hwnd, LB_GETTEXT, countfrom0(i), ws))
	return buf or mbs(ws)
end

function ListBox_GetItemData(hwnd, i)
	return SNDMSG(hwnd, LB_GETITEMDATA, countfrom0(i))
end

function ListBox_SetItemData(hwnd, i, data)
	checkpoz(SNDMSG(hwnd, LB_SETITEMDATA, countfrom0(i), data))
end

function ListBox_FindString(hwnd, s, indexStart) --returns index
	return countfrom1(checkpoz(SNDMSG(hwnd, LB_FINDSTRING, countfrom0(indexStart or -1), wcs(s))))
end

function ListBox_FindItemData(hwnd, data, indexStart) --returns index
	return countfrom1(checkpoz(SNDMSG(hwnd, LB_FINDSTRING, countfrom0(indexStart or -1), data)))
end

function ListBox_FindStringExact(hwnd, s, istart) --istart is optional
	return countfrom1(checkpoz(SNDMSG(hwnd, LB_FINDSTRINGEXACT, countfrom0(istart), wcs(s))))
end

function ListBox_SetSel(hwnd, yes, i)
	checkpoz(SNDMSG(hwnd, LB_SETSEL, yes, countfrom0(i)))
end

function ListBox_GetSel(hwnd, i)
	return checkpoz(SNDMSG(hwnd, LB_GETSEL, countfrom0(i))) ~= 0
end

function ListBox_SelItemRange(hwnd, fSelect, first, last)
   checkpoz(SNDMSG(hwnd, LB_SELITEMRANGE, fSelect, MAKELPARAM(countfrom0(first), countfrom0(last))))
end

function ListBox_GetCurSel(hwnd)
	return countfrom1(SNDMSG(hwnd, LB_GETCURSEL))
end

function ListBox_SetCurSel(hwnd, i) --a nil i means no selection
	local check = i == nil and pass or checkpoz
	check(SNDMSG(hwnd, LB_SETCURSEL, countfrom0(i)))
end

function ListBox_GetSelCount(hwnd)
	return checkpoz(SNDMSG(hwnd, LB_GETSELCOUNT))
end

function ListBox_GetSelItems(hwnd)
	local n = GetSelCount(hwnd)
	local items = ffi.new('int[?]', n)
	n = checkpoz(SNDMSG(hwnd, LB_GETSELITEMS, n, items))
	--TODO: should we return the zero-based cdata array and its length instead?
	local t={}
	for i=1,n do
		t[i] = items[i-1]+1
	end
	return t
end

function ListBox_GetTopIndex(hwnd)
	return checkpoz(SNDMSG(hwnd, LB_GETTOPINDEX))+1
end

function ListBox_SetTopIndex(hwnd, indexTop)
	checkpoz(SNDMSG(hwnd, LB_SETTOPINDEX, countfrom0(indexTop)))
end

function ListBox_SetColumnWidth(hwnd, cxColumn)
	SNDMSG(hwnd, LB_SETCOLUMNWIDTH, cxColumn)
end

function ListBox_SetTabStops(hwnd, tabs)
	local tabs, n = array('int', tabs)
	checknz(SNDMSG(hwnd, LB_SETTABSTOPS, n, tabs))
end

function ListBox_GetHorizontalExtent(hwnd)
	SNDMSG(hwnd, LB_GETHORIZONTALEXTENT)
end

function ListBox_SetHorizontalExtent(hwnd, cxExtent)
	SNDMSG(hwnd, LB_SETHORIZONTALEXTENT, cxExtent)
end

function ListBox_GetItemRect(hwnd, i, rect)
	rect = RECT(rect)
	checkpoz(SNDMSG(hwnd, LB_GETITEMRECT, countfrom0(i), ffi.cast('RECT*', rect)))
	return rect
end

function ListBox_SetCaretIndex(hwnd, i)
	checkz(SNDMSG(hwnd, LB_SETCARETINDEX, countfrom0(i)))
end

function ListBox_GetCaretIndex(hwnd)
	return checkpoz(SNDMSG(hwnd, LB_GETCARETINDEX))+1
end

function ListBox_SetItemHeight(hwnd, i, h)
   SNDMSG(hwnd, LB_SETITEMHEIGHT, countfrom0(i), MAKELPARAM(h, 0))
end

function ListBox_GetItemHeight(hwnd, i)
	return checkpoz(SNDMSG(hwnd, LB_GETITEMHEIGHT, countfrom0(i)))
end

function ListBox_Dir(hwnd, attrs, lpszFileSpec)
	return checkpoz(SNDMSG(hwnd, LB_DIR, attrs, ffi.cast('LPCTSTR', wcs(lpszFileSpec))))
end

--notifications

LBN_ERRSPACE         = -2
LBN_SELCHANGE        = 1
LBN_DBLCLK           = 2
LBN_SELCANCEL        = 3
LBN_SETFOCUS         = 4
LBN_KILLFOCUS        = 5

