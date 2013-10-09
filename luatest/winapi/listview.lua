--proc/listview: standard listview control.
setfenv(1, require'winapi')
require'winapi.window'
require'winapi.comctl'

InitCommonControlsEx(ICC_LISTVIEW_CLASSES)

--creation

WC_LISTVIEW = 'SysListView32'

LVS_ICON                 = 0x0000
LVS_REPORT               = 0x0001
LVS_SMALLICON            = 0x0002
LVS_LIST                 = 0x0003
LVS_TYPEMASK             = 0x0003
LVS_SINGLESEL            = 0x0004
LVS_SHOWSELALWAYS        = 0x0008
LVS_SORTASCENDING        = 0x0010
LVS_SORTDESCENDING       = 0x0020
LVS_SHAREIMAGELISTS      = 0x0040
LVS_NOLABELWRAP          = 0x0080
LVS_AUTOARRANGE          = 0x0100
LVS_EDITLABELS           = 0x0200
LVS_OWNERDATA            = 0x1000
LVS_NOSCROLL             = 0x2000
LVS_TYPESTYLEMASK        = 0xfc00
LVS_ALIGNTOP             = 0x0000
LVS_ALIGNLEFT            = 0x0800
LVS_ALIGNMASK            = 0x0c00
LVS_OWNERDRAWFIXED       = 0x0400
LVS_NOCOLUMNHEADER       = 0x4000
LVS_NOSORTHEADER         = 0x8000

LVS_EX_GRIDLINES         = 0x00000001
LVS_EX_SUBITEMIMAGES     = 0x00000002
LVS_EX_CHECKBOXES        = 0x00000004
LVS_EX_TRACKSELECT       = 0x00000008
LVS_EX_HEADERDRAGDROP    = 0x00000010
LVS_EX_FULLROWSELECT     = 0x00000020 -- applies to report mode only
LVS_EX_ONECLICKACTIVATE  = 0x00000040
LVS_EX_TWOCLICKACTIVATE  = 0x00000080
LVS_EX_FLATSB            = 0x00000100
LVS_EX_REGIONAL          = 0x00000200
LVS_EX_INFOTIP           = 0x00000400 -- listview does InfoTips for you
LVS_EX_UNDERLINEHOT      = 0x00000800
LVS_EX_UNDERLINECOLD     = 0x00001000
LVS_EX_MULTIWORKAREAS    = 0x00002000
LVS_EX_LABELTIP          = 0x00004000 -- listview unfolds partly hidden labels if it does not have infotip text
LVS_EX_BORDERSELECT      = 0x00008000 -- border selection style instead of highlight
LVS_EX_DOUBLEBUFFER      = 0x00010000
LVS_EX_HIDELABELS        = 0x00020000
LVS_EX_SINGLEROW         = 0x00040000
LVS_EX_SNAPTOGRID        = 0x00080000  -- Icons automatically snap to grid.
LVS_EX_SIMPLESELECT      = 0x00100000  -- Also changes overlay rendering to top right for icon mode.

LISTVIEW_DEFAULTS = {
	class = WC_LISTVIEW,
	style = bit.bor(WS_CHILD, WS_VISIBLE, LVS_REPORT),
	style_ex = bit.bor(WS_EX_CLIENTEDGE, LVS_EX_DOUBLEBUFFER),
	x = 10, y = 10,
	w = 200, h = 100,
}

function CreateListView(info)
	info = update({}, LISTVIEW_DEFAULTS, info)
	return CreateWindow(info)
end

--commands

LVM_FIRST                = 0x1000

--commands/ext. style

LVM_SETEXTENDEDLISTVIEWSTYLE  = (LVM_FIRST + 54)
function ListView_SetExtendedListViewStyle(hwnd, style_mask, style_value) --returns prev. style
	return SNDMSG(hwnd, LVM_SETEXTENDEDLISTVIEWSTYLE, style_mask, style_value)
end

LVM_GETEXTENDEDLISTVIEWSTYLE  = (LVM_FIRST + 55)
function ListView_GetExtendedListViewStyle(hwnd)
	return SNDMSG(hwnd, LVM_GETEXTENDEDLISTVIEWSTYLE)
end

LVM_SETHOVERTIME         = (LVM_FIRST + 71)
function ListView_SetHooverTime(hwmd, time) --returns prev. hoover time
	return SNDMSG(hwmd, LVM_SETHOVERTIME, 0, time)
end

--commands/columns

ffi.cdef[[
typedef struct tagLVCOLUMNW
{
	 UINT mask;
	 int fmt;
	 int cx;
	 LPWSTR pszText;
	 int cchTextMax;
	 int iSubItem;
	 int iImage;
	 int iOrder;
} LVCOLUMNW, *LPLVCOLUMNW;
]]

LVCF_FMT                 = 0x0001
LVCF_WIDTH               = 0x0002
LVCF_TEXT                = 0x0004
LVCF_SUBITEM             = 0x0008
LVCF_IMAGE               = 0x0010
LVCF_ORDER               = 0x0020

LVCFMT_LEFT              = 0x0000
LVCFMT_RIGHT             = 0x0001
LVCFMT_CENTER            = 0x0002
LVCFMT_JUSTIFYMASK       = 0x0003
LVCFMT_IMAGE             = 0x0800
LVCFMT_BITMAP_ON_RIGHT   = 0x1000
LVCFMT_COL_HAS_IMAGES    = 0x8000

LVCOLUMN = struct{
	ctype = 'LVCOLUMNW', mask = 'mask',
	fields = mfields{
		'subitem', 'iSubItem', LVCF_SUBITEM, countfrom0, countfrom1,
		'text', 'pszText', LVCF_TEXT, wcs, mbs,
		'w', 'cx', LVCF_WIDTH, pass, pass,
		'format', 'fmt', LVCF_FMT, flags, pass,
	},
	defaults = {
		text = '', --avoids flicker when resizing columns in xp
		w = 100,
	}
}

LVM_INSERTCOLUMNW        = (LVM_FIRST + 97)
function ListView_InsertColumn(hwnd, i, col)
	col = LVCOLUMN(col)
	return checkpoz(SNDMSG(hwnd, LVM_INSERTCOLUMNW, countfrom0(i), ffi.cast('LVCOLUMNW*', col))), col
end

LVM_GETCOLUMNW           = (LVM_FIRST + 95)
function ListView_GetColumn(hwnd, i, col)
	col = LVCOLUMN(col)
	checktrue(SNDMSG(hwnd, LVM_GETCOLUMN, countfrom0(i), ffi.cast('LVCOLUMNW*', col)))
	return col
end

LVM_SETCOLUMNW           = (LVM_FIRST + 96)
function ListView_SetColumn(hwnd, i, col)
	col = LVCOLUMN(col)
	checktrue(SNDMSG(hwnd, LVM_SETCOLUMN, countfrom0(i), ffi.cast('LVCOLUMNW*', col)))
	return col
end

LVM_DELETECOLUMN         = (LVM_FIRST + 28)
function ListView_DeleteColumn(hwnd, i)
	checktrue(SNDMSG(hwnd, LVM_DELETECOLUMN, countfrom0(i)))
end

LVM_GETCOLUMNWIDTH       = (LVM_FIRST + 29)
function ListView_GetColumnWidth(hwnd, i)
	return checkpoz(SNDMSG(hwnd, LVM_GETCOLUMNWIDTH, countfrom0(i)))
end

LVM_SETCOLUMNWIDTH       = (LVM_FIRST + 30)
LVSCW_AUTOSIZE               = -1
LVSCW_AUTOSIZE_USEHEADER     = -2
function ListView_SetColumnWidth(hwnd, i, w)
	checktrue(SNDMSG(hwnd, LVM_SETCOLUMNWIDTH, countfrom0(i), MAKELPARAM(flags(w), 0)))
end

LVM_GETHEADER            = (LVM_FIRST + 31)
function ListView_GetHeader(hwnd)
	return ffi.cast('HWND', checkh(SNDMSG(hwnd, LVM_GETHEADER)))
end

LVM_SETCOLUMNORDERARRAY  = (LVM_FIRST + 58)
function ListView_SetColumnOrderArray(hwnd, ai)
	local ai, sz = arrays.int(ai)
	checktrue(SNDMSG(hwnd, LVM_SETCOLUMNORDERARRAY, sz, ffi.cast('int*', ai)))
	return ai
end

LVM_GETCOLUMNORDERARRAY  = (LVM_FIRST + 59)
function ListView_GetColumnOrderArray(hwnd, ai)
	local ai, sz = arrays.int(ai)
	checktrue(SNDMSG(hwnd, LVM_GETCOLUMNORDERARRAY, sz, ffi.cast('int*', ai)))
	return ai
end

LVM_SETSELECTEDCOLUMN    = (LVM_FIRST + 140)
function ListView_SetSelectedColumn(hwnd, i)
	SNDMSG(hwnd, LVM_SETSELECTEDCOLUMN, countfrom0(i))
end

LVM_GETSELECTEDCOLUMN    = (LVM_FIRST + 174)
function ListView_GetSelectedColumn(hwnd)
	return countfrom1(checkpoz(SNDMSG(hwnd, LVM_GETSELECTEDCOLUMN)))
end

--commands/items

ffi.cdef[[
typedef struct tagLVITEMW
{
	 UINT mask;
	 int iItem;
	 int iSubItem;
	 UINT state;
	 UINT stateMask;
	 LPWSTR pszText;
	 int cchTextMax;
	 int iImage;
	 LPARAM lParam;
	 int iIndent;
	 int iGroupId;
	 UINT cColumns;
	 PUINT puColumns;
	 int piColFmt;
	 int iGroup;
} LVITEMW, *LPLVITEMW;
]]

LVIF_TEXT                = 0x0001
LVIF_IMAGE               = 0x0002
LVIF_PARAM               = 0x0004
LVIF_STATE               = 0x0008
LVIF_INDENT              = 0x0010
LVIF_GROUPID             = 0x0100
LVIF_COLUMNS             = 0x0200
LVIF_NORECOMPUTE         = 0x0800
LVIF_DI_SETITEM          = 0x1000
LVIF_COLFMT              = 0x00010000

LVIS_FOCUSED             = 0x0001
LVIS_SELECTED            = 0x0002
LVIS_CUT                 = 0x0004
LVIS_DROPHILITED         = 0x0008
LVIS_GLOW                = 0x0010
LVIS_ACTIVATING          = 0x0020
LVIS_OVERLAYMASK         = 0x0F00
LVIS_STATEIMAGEMASK      = 0xF000

LVITEM = struct{
	ctype = 'LVITEMW', mask = 'mask',
	fields = mfields{
		'i', 'iItem', 0, countfrom0, countfrom1,
		'subitem', 'iSubItem', 0, pass, pass, --0 is the item, 1..n are the subitems
		'columns', 'cColumns', LVIF_COLUMNS, pass, pass, --xp+
		'col_format', 'piColFmt', LVIF_COLFMT, pass, pass, --vista+
		'group_id', 'iGroupId', LVIF_GROUPID, pass, pass, --xp+
		'image', 'iImage', LVIF_IMAGE, pass, pass,
		'indent', 'iIndent', LVIF_INDENT, pass, pass,
		'state', 'state', LVIF_STATE, flags, pass,
		'state_mask', 'stateMask', LVIF_STATE, flags, pass,
		'text', 'pszText', LVIF_TEXT, wcs, mbs,
		'setitem', '', LVIF_DI_SETITEM, pass, pass,
		'norecompute', '', LVIF_NORECOMPUTE, pass, pass,
	}
}

LPSTR_TEXTCALLBACKW      = -1

LVM_INSERTITEMW          = (LVM_FIRST + 77)
function ListView_InsertItem(lv, item)
	item = LVITEM(item)
	return checkpoz(SNDMSG(lv, LVM_INSERTITEMW, 0, ffi.cast('LVITEMW*', item)))
end

LVM_GETITEMW             = (LVM_FIRST + 75)
function ListView_GetItem(lv, item)
	item = LVITEM(item)
	checkpoz(SNDMSG(lv, LVM_GETITEMW, 0, ffi.cast('LVITEMW*', item)))
	return item
end

LVM_SETITEMW             = (LVM_FIRST + 76)
function ListView_SetItem(lv, item)
	item = LVITEM(item)
	return checkpoz(SNDMSG(lv, LVM_SETITEMW, 0, ffi.cast('LVITEMW*', item)))
end

LVM_GETITEMCOUNT         = (LVM_FIRST + 4)
function ListView_GetItemCount(lv)
	return checkpoz(SNDMSG(lv, LVM_GETITEMCOUNT))
end

LVM_DELETEITEM           = (LVM_FIRST + 8)
function ListView_DeleteItem(lv, pos)
	return checkpoz(SNDMSG(lv, LVM_DELETEITEM, countfrom0(pos)))
end

LVM_DELETEALLITEMS       = (LVM_FIRST + 9)
function ListView_DeleteAllItems(lv)
	return checkpoz(SNDMSG(lv, LVM_DELETEALLITEMS))
end

--commands/owner drawing

LVIR_BOUNDS              = 0
LVIR_ICON                = 1
LVIR_LABEL               = 2
LVIR_SELECTBOUNDS        = 3

LVM_GETSUBITEMRECT       = (LVM_FIRST + 56)
function ListView_GetSubItemRect(hwnd, i, subitem, LVIR, r)
	r = RECT(r)
	r.top = subitem
	r.left = flags(LVIR)
	checknz(SNDMSG(hwnd, LVM_GETSUBITEMRECT, countfrom0(i), ffi.cast('RECT*', r)))
	return r
end

LVM_GETITEMRECT          = (LVM_FIRST + 14)
function ListView_GetItemRect(hwnd, i, LVIR, r)
	r = RECT(r)
	r.left = flags(LVIR)
	checktrue(SNDMSG(hwnd, LVM_GETITEMRECT, countfrom0(i), ffi.cast('RECT*', r)))
	return r
end

--[[
LVM_SUBITEMHITTEST       = (LVM_FIRST + 57)
function ListView_SubItemHitTest(hwnd, plvhti) \
		  (int)SNDMSG((hwnd), LVM_SUBITEMHITTEST, 0, (LPARAM)(LPLVHITTESTINFO)(plvhti))

LVM_SETITEMPOSITION      = (LVM_FIRST + 15)
ListView_SetItemPosition(hwndLV,  = i, x, y) \
	 (BOOL)SNDMSG((hwndLV), LVM_SETITEMPOSITION, (WPARAM)(int)(i), MAKELPARAM((x), (y)))


LVM_GETITEMPOSITION      = (LVM_FIRST + 16)
ListView_GetItemPosition(hwndLV,  = i, ppt) \
	 (BOOL)SNDMSG((hwndLV), LVM_GETITEMPOSITION, (WPARAM)(int)(i), (LPARAM)(POINT *)(ppt))

LVM_GETSTRINGWIDTHW      = (LVM_FIRST + 87)
ListView_GetStringWidth(hwndLV,  = psz) \
	 (int)SNDMSG((hwndLV), LVM_GETSTRINGWIDTH, 0, (LPARAM)(LPCTSTR)(psz))
]]

--notifications

LVN_FIRST                   = 2^32 -100
update(WM_NOTIFY_NAMES, constants{
	LVN_ITEMCHANGING         = LVN_FIRST-0,
	LVN_ITEMCHANGED          = LVN_FIRST-1,
	LVN_INSERTITEM           = LVN_FIRST-2,
	LVN_DELETEITEM           = LVN_FIRST-3,
	LVN_DELETEALLITEMS       = LVN_FIRST-4,
	LVN_BEGINLABELEDITW      = LVN_FIRST-75,
	LVN_ENDLABELEDITW        = LVN_FIRST-76,
	LVN_COLUMNCLICK          = LVN_FIRST-8,
	LVN_BEGINDRAG            = LVN_FIRST-9,
	LVN_BEGINRDRAG           = LVN_FIRST-11,
	LVN_ODCACHEHINT          = LVN_FIRST-13,
	LVN_ODFINDITEMW          = LVN_FIRST-79,
	LVN_ITEMACTIVATE         = LVN_FIRST-14,
	LVN_ODSTATECHANGED       = LVN_FIRST-15,
	LVN_HOTTRACK             = LVN_FIRST-21,
	LVN_GETDISPINFOW         = LVN_FIRST-77,
	LVN_SETDISPINFOW         = LVN_FIRST-78,
	LVN_KEYDOWN              = LVN_FIRST-55,
	LVN_MARQUEEBEGIN         = LVN_FIRST-56,
	LVN_GETINFOTIPW          = LVN_FIRST-58,
	LVN_INCREMENTALSEARCHW   = LVN_FIRST-63,
	LVN_COLUMNDROPDOWN       = LVN_FIRST-64,
	LVN_COLUMNOVERFLOWCLICK  = LVN_FIRST-66,
	LVN_BEGINSCROLL          = LVN_FIRST-80,
	LVN_ENDSCROLL            = LVN_FIRST-81,
	LVN_LINKCLICK            = LVN_FIRST-84,
	LVN_GETEMPTYMARKUP       = LVN_FIRST-87,
})

ffi.cdef[[
typedef struct tagNMLISTVIEW
{
	 NMHDR   hdr;
	 int     iItem;
	 int     iSubItem;
	 UINT    uNewState;
	 UINT    uOldState;
	 UINT    uChanged;
	 POINT   ptAction;
	 LPARAM  lParam;
} NMLISTVIEW, *LPNMLISTVIEW;
]]

function WM_NOTIFY_DECODERS.LVN_ITEMCHANGING(hdr) --return item, subitem, new_LVIS, old_LVIS, changed_LVIF
	local t = ffi.cast('NMLISTVIEW*', hdr)
	return countfrom1(t.iItem), t.iSubItem, t.uNewState, t.uOldState, t.uChanged
end

WM_NOTIFY_DECODERS.LVN_ITEMCHANGED = WM_NOTIFY_DECODERS.LVN_ITEMCHANGING

ffi.cdef[[
typedef struct tagNMLVODSTATECHANGE
{
	 NMHDR hdr;
	 int iFrom;
	 int iTo;
	 UINT uNewState;
	 UINT uOldState;
} NMLVODSTATECHANGE, *LPNMLVODSTATECHANGE;
]]

function WM_NOTIFY_DECODERS.LVN_ODSTATECHANGED(hdr)
	return ffi.cast('NMLVODSTATECHANGE*', hdr)
end

-- notifications/custom draw

ffi.cdef[[
typedef struct tagNMLVCUSTOMDRAW
{
    NMCUSTOMDRAW nmcd;
    COLORREF text_color;
    COLORREF bk_color;
    int subitem;
    DWORD dwItemType;
    COLORREF clrFace;
    int iIconEffect;
    int iIconPhase;
    int iPartId;
    int iStateId;
    RECT rcText;
    UINT uAlign;
} NMLVCUSTOMDRAW, *LPNMLVCUSTOMDRAW;
]]

--[===[

--[[
LVM_SETUNICODEFORMAT      = CCM_SETUNICODEFORMAT
ListView_SetUnicodeFormat(hwnd,  = fUnicode)  \
	 (BOOL)SNDMSG((hwnd), LVM_SETUNICODEFORMAT, (WPARAM)(fUnicode), 0)

LVM_GETUNICODEFORMAT      = CCM_GETUNICODEFORMAT
ListView_GetUnicodeFormat(hwnd)   = \
	 (BOOL)SNDMSG((hwnd), LVM_GETUNICODEFORMAT, 0, 0)


LVM_GETBKCOLOR           = (LVM_FIRST + 0)
ListView_GetBkColor(hwnd)   = \
	 (COLORREF)SNDMSG((hwnd), LVM_GETBKCOLOR, 0, 0L)

LVM_SETBKCOLOR           = (LVM_FIRST + 1)
ListView_SetBkColor(hwnd,  = clrBk) \
	 (BOOL)SNDMSG((hwnd), LVM_SETBKCOLOR, 0, (LPARAM)(COLORREF)(clrBk))

LVM_GETIMAGELIST         = (LVM_FIRST + 2)
ListView_GetImageList(hwnd,  = iImageList) \
	 (HIMAGELIST)SNDMSG((hwnd), LVM_GETIMAGELIST, (WPARAM)(INT)(iImageList), 0L)

LVSIL_NORMAL             = 0
LVSIL_SMALL              = 1
LVSIL_STATE              = 2

LVM_SETIMAGELIST         = (LVM_FIRST + 3)
ListView_SetImageList(hwnd,  = himl, iImageList) \
	 (HIMAGELIST)SNDMSG((hwnd), LVM_SETIMAGELIST, (WPARAM)(iImageList), (LPARAM)(HIMAGELIST)(himl))

]]

--INDEXTOSTATEIMAGEMASK(i)  = ((i) << 12)

I_INDENTCALLBACK         = -1

I_GROUPIDCALLBACK    = -1
I_GROUPIDNONE        = -2

-- For tileview
I_COLUMNSCALLBACK        = -1

LVM_GETCALLBACKMASK      = (LVM_FIRST + 10)
ListView_GetCallbackMask(hwnd)  = \
	 (BOOL)SNDMSG((hwnd), LVM_GETCALLBACKMASK, 0, 0)


LVM_SETCALLBACKMASK      = (LVM_FIRST + 11)
ListView_SetCallbackMask(hwnd,  = mask) \
	 (BOOL)SNDMSG((hwnd), LVM_SETCALLBACKMASK, (WPARAM)(UINT)(mask), 0)

LVNI_ALL                 = 0x0000
LVNI_FOCUSED             = 0x0001
LVNI_SELECTED            = 0x0002
LVNI_CUT                 = 0x0004
LVNI_DROPHILITED         = 0x0008
LVNI_ABOVE               = 0x0100
LVNI_BELOW               = 0x0200
LVNI_TOLEFT              = 0x0400
LVNI_TORIGHT             = 0x0800

--[[
LVM_GETNEXTITEM          = (LVM_FIRST + 12)
ListView_GetNextItem(hwnd,  = i, flags) \
	 (int)SNDMSG((hwnd), LVM_GETNEXTITEM, (WPARAM)(int)(i), MAKELPARAM((flags), 0))
]]

LVFI_PARAM               = 0x0001
LVFI_STRING              = 0x0002
LVFI_PARTIAL             = 0x0008
LVFI_WRAP                = 0x0020
LVFI_NEARESTXY           = 0x0040

LVM_FINDITEMW            = (LVM_FIRST + 83)

--[[
ListView_FindItem(hwnd,  = iStart, plvfi) \
	 (int)SNDMSG((hwnd), LVM_FINDITEM, (WPARAM)(int)(iStart), (LPARAM)(const LV_FINDINFO *)(plvfi))
]]

LVHT_NOWHERE             = 0x0001
LVHT_ONITEMICON          = 0x0002
LVHT_ONITEMLABEL         = 0x0004
LVHT_ONITEMSTATEICON     = 0x0008
LVHT_ONITEM              = bit.bor(LVHT_ONITEMICON, LVHT_ONITEMLABEL, LVHT_ONITEMSTATEICON)

LVHT_ABOVE               = 0x0008
LVHT_BELOW               = 0x0010
LVHT_TORIGHT             = 0x0020
LVHT_TOLEFT              = 0x0040

--LVHITTESTINFO_V1_SIZE  = CCSIZEOF_STRUCT(LVHITTESTINFO, iItem)

--[[
LVM_HITTEST              = (LVM_FIRST + 18)
ListView_HitTest(hwndLV,  = pinfo) \
	 (int)SNDMSG((hwndLV), LVM_HITTEST, 0, (LPARAM)(LV_HITTESTINFO *)(pinfo))


LVM_ENSUREVISIBLE        = (LVM_FIRST + 19)
ListView_EnsureVisible(hwndLV,  = i, fPartialOK) \
	 (BOOL)SNDMSG((hwndLV), LVM_ENSUREVISIBLE, (WPARAM)(int)(i), MAKELPARAM((fPartialOK), 0))


LVM_SCROLL               = (LVM_FIRST + 20)
ListView_Scroll(hwndLV,  = dx, dy) \
	 (BOOL)SNDMSG((hwndLV), LVM_SCROLL, (WPARAM)(int)(dx), (LPARAM)(int)(dy))


LVM_REDRAWITEMS          = (LVM_FIRST + 21)
ListView_RedrawItems(hwndLV,  = iFirst, iLast) \
	 (BOOL)SNDMSG((hwndLV), LVM_REDRAWITEMS, (WPARAM)(int)(iFirst), (LPARAM)(int)(iLast))
]]

LVA_DEFAULT              = 0x0000
LVA_ALIGNLEFT            = 0x0001
LVA_ALIGNTOP             = 0x0002
LVA_SNAPTOGRID           = 0x0005

--[[
LVM_ARRANGE              = (LVM_FIRST + 22)
ListView_Arrange(hwndLV,  = code) \
	 (BOOL)SNDMSG((hwndLV), LVM_ARRANGE, (WPARAM)(UINT)(code), 0L)

LVM_EDITLABELW           = (LVM_FIRST + 118)

ListView_EditLabel(hwndLV,  = i) \
	 (HWND)SNDMSG((hwndLV), LVM_EDITLABEL, (WPARAM)(int)(i), 0L)


LVM_GETEDITCONTROL       = (LVM_FIRST + 24)
ListView_GetEditControl(hwndLV)  = \
	 (HWND)SNDMSG((hwndLV), LVM_GETEDITCONTROL, 0, 0L)

]]



LVM_CREATEDRAGIMAGE      = (LVM_FIRST + 33)
ListView_CreateDragImage(hwnd,  = i, lpptUpLeft) \
	 (HIMAGELIST)SNDMSG((hwnd), LVM_CREATEDRAGIMAGE, (WPARAM)(int)(i), (LPARAM)(LPPOINT)(lpptUpLeft))


LVM_GETVIEWRECT          = (LVM_FIRST + 34)
ListView_GetViewRect(hwnd,  = prc) \
	 (BOOL)SNDMSG((hwnd), LVM_GETVIEWRECT, 0, (LPARAM)(RECT *)(prc))


LVM_GETTEXTCOLOR         = (LVM_FIRST + 35)
ListView_GetTextColor(hwnd)   = \
	 (COLORREF)SNDMSG((hwnd), LVM_GETTEXTCOLOR, 0, 0L)


LVM_SETTEXTCOLOR         = (LVM_FIRST + 36)
ListView_SetTextColor(hwnd,  = clrText) \
	 (BOOL)SNDMSG((hwnd), LVM_SETTEXTCOLOR, 0, (LPARAM)(COLORREF)(clrText))


LVM_GETTEXTBKCOLOR       = (LVM_FIRST + 37)
ListView_GetTextBkColor(hwnd)   = \
	 (COLORREF)SNDMSG((hwnd), LVM_GETTEXTBKCOLOR, 0, 0L)


LVM_SETTEXTBKCOLOR       = (LVM_FIRST + 38)
ListView_SetTextBkColor(hwnd,  = clrTextBk) \
	 (BOOL)SNDMSG((hwnd), LVM_SETTEXTBKCOLOR, 0, (LPARAM)(COLORREF)(clrTextBk))


LVM_GETTOPINDEX          = (LVM_FIRST + 39)
ListView_GetTopIndex(hwndLV)  = \
	 (int)SNDMSG((hwndLV), LVM_GETTOPINDEX, 0, 0)


LVM_GETCOUNTPERPAGE      = (LVM_FIRST + 40)
ListView_GetCountPerPage(hwndLV)  = \
	 (int)SNDMSG((hwndLV), LVM_GETCOUNTPERPAGE, 0, 0)


LVM_GETORIGIN            = (LVM_FIRST + 41)
ListView_GetOrigin(hwndLV,  = ppt) \
	 (BOOL)SNDMSG((hwndLV), LVM_GETORIGIN, (WPARAM)0, (LPARAM)(POINT *)(ppt))

LVM_UPDATE               = (LVM_FIRST + 42)
ListView_Update(hwndLV,  = i) \
	 (BOOL)SNDMSG((hwndLV), LVM_UPDATE, (WPARAM)(i), 0L)

LVM_SETITEMSTATE         = (LVM_FIRST + 43)
ListView_SetItemState(hwndLV,  = i, data, mask) \
{ LV_ITEM _ms_lvi;\
  _ms_lvi.stateMask = mask;\
  _ms_lvi.state = data;\
  SNDMSG((hwndLV), LVM_SETITEMSTATE, (WPARAM)(i), (LPARAM)(LV_ITEM *)&_ms_lvi);\
}

ListView_SetCheckState(hwndLV,  = i, fCheck) \
  ListView_SetItemState(hwndLV, i, INDEXTOSTATEIMAGEMASK((fCheck)?2:1), LVIS_STATEIMAGEMASK)

LVM_GETITEMSTATE         = (LVM_FIRST + 44)
ListView_GetItemState(hwndLV,  = i, mask) \
	(UINT)SNDMSG((hwndLV), LVM_GETITEMSTATE, (WPARAM)(i), (LPARAM)(mask))

ListView_GetCheckState(hwndLV,  = i) \
	((((UINT)(SNDMSG((hwndLV), LVM_GETITEMSTATE, (WPARAM)(i), LVIS_STATEIMAGEMASK))) >> 12) -1)

LVM_GETITEMTEXTW         = (LVM_FIRST + 115)

ListView_GetItemText(hwndLV,  = i, iSubItem_, pszText_, cchTextMax_) \
{ LV_ITEM _ms_lvi;\
  _ms_lvi.iSubItem = iSubItem_;\
  _ms_lvi.cchTextMax = cchTextMax_;\
  _ms_lvi.pszText = pszText_;\
  SNDMSG((hwndLV), LVM_GETITEMTEXT, (WPARAM)(i), (LPARAM)(LV_ITEM *)&_ms_lvi);\
}

-- these flags only apply to LVS_OWNERDATA listviews in report or list mode
LVSICF_NOINVALIDATEALL   = 0x00000001
LVSICF_NOSCROLL          = 0x00000002

LVM_SETITEMCOUNT         = (LVM_FIRST + 47)
ListView_SetItemCount(hwndLV,  = cItems) \
  SNDMSG((hwndLV), LVM_SETITEMCOUNT, (WPARAM)(cItems), 0)

ListView_SetItemCountEx(hwndLV,  = cItems, dwFlags) \
  SNDMSG((hwndLV), LVM_SETITEMCOUNT, (WPARAM)(cItems), (LPARAM)(dwFlags))

typedef int (__stdcall *PFNLVCOMPARE)(LPARAM, LPARAM, LPARAM);

LVM_SORTITEMS            = (LVM_FIRST + 48)
ListView_SortItems(hwndLV,  = _pfnCompare, _lPrm) \
  (BOOL)SNDMSG((hwndLV), LVM_SORTITEMS, (WPARAM)(LPARAM)(_lPrm), \
  (LPARAM)(PFNLVCOMPARE)(_pfnCompare))

LVM_SETITEMPOSITION32    = (LVM_FIRST + 49)
ListView_SetItemPosition32(hwndLV,  = i, x0, y0) \
{   POINT ptNewPos; \
	 ptNewPos.x = x0; ptNewPos.y = y0; \
	 SNDMSG((hwndLV), LVM_SETITEMPOSITION32, (WPARAM)(int)(i), (LPARAM)&ptNewPos); \
}

LVM_GETSELECTEDCOUNT     = (LVM_FIRST + 50)
ListView_GetSelectedCount(hwndLV)  = \
	 (UINT)SNDMSG((hwndLV), LVM_GETSELECTEDCOUNT, 0, 0L)

LVM_GETITEMSPACING       = (LVM_FIRST + 51)
ListView_GetItemSpacing(hwndLV,  = fSmall) \
		  (DWORD)SNDMSG((hwndLV), LVM_GETITEMSPACING, fSmall, 0L)

LVM_GETISEARCHSTRINGW    = (LVM_FIRST + 117)

ListView_GetISearchString(hwndLV,  = lpsz) \
		  (BOOL)SNDMSG((hwndLV), LVM_GETISEARCHSTRING, 0, (LPARAM)(LPTSTR)(lpsz))

LVM_SETICONSPACING       = (LVM_FIRST + 53)
-- -1 for cx and cy means we'll use the default (system settings)
-- 0 for cx or cy means use the current setting (allows you to change just one param)
ListView_SetIconSpacing(hwndLV,  = cx, cy) \
		  (DWORD)SNDMSG((hwndLV), LVM_SETICONSPACING, 0, MAKELONG(cx,cy))
]]


LVM_SETHOTITEM   = (LVM_FIRST + 60)
ListView_SetHotItem(hwnd,  = i) \
		  (int)SNDMSG((hwnd), LVM_SETHOTITEM, (WPARAM)(i), 0)

LVM_GETHOTITEM   = (LVM_FIRST + 61)
ListView_GetHotItem(hwnd)  = \
		  (int)SNDMSG((hwnd), LVM_GETHOTITEM, 0, 0)

LVM_SETHOTCURSOR   = (LVM_FIRST + 62)
ListView_SetHotCursor(hwnd,  = hcur) \
		  (HCURSOR)SNDMSG((hwnd), LVM_SETHOTCURSOR, 0, (LPARAM)(hcur))

LVM_GETHOTCURSOR   = (LVM_FIRST + 63)
ListView_GetHotCursor(hwnd)  = \
		  (HCURSOR)SNDMSG((hwnd), LVM_GETHOTCURSOR, 0, 0)

LVM_APPROXIMATEVIEWRECT  = (LVM_FIRST + 64)
ListView_ApproximateViewRect(hwnd,  = iWidth, iHeight, iCount) \
		  (DWORD)SNDMSG((hwnd), LVM_APPROXIMATEVIEWRECT, iCount, MAKELPARAM(iWidth, iHeight))

LV_MAX_WORKAREAS          = 16
LVM_SETWORKAREAS          = (LVM_FIRST + 65)
ListView_SetWorkAreas(hwnd,  = nWorkAreas, prc) \
	 (BOOL)SNDMSG((hwnd), LVM_SETWORKAREAS, (WPARAM)(int)(nWorkAreas), (LPARAM)(RECT *)(prc))

LVM_GETWORKAREAS         = (LVM_FIRST + 70)
ListView_GetWorkAreas(hwnd,  = nWorkAreas, prc) \
	 (BOOL)SNDMSG((hwnd), LVM_GETWORKAREAS, (WPARAM)(int)(nWorkAreas), (LPARAM)(RECT *)(prc))


LVM_GETNUMBEROFWORKAREAS   = (LVM_FIRST + 73)
ListView_GetNumberOfWorkAreas(hwnd,  = pnWorkAreas) \
	 (BOOL)SNDMSG((hwnd), LVM_GETNUMBEROFWORKAREAS, 0, (LPARAM)(UINT *)(pnWorkAreas))


LVM_GETSELECTIONMARK     = (LVM_FIRST + 66)
ListView_GetSelectionMark(hwnd)  = \
	 (int)SNDMSG((hwnd), LVM_GETSELECTIONMARK, 0, 0)

LVM_SETSELECTIONMARK     = (LVM_FIRST + 67)
ListView_SetSelectionMark(hwnd,  = i) \
	 (int)SNDMSG((hwnd), LVM_SETSELECTIONMARK, 0, (LPARAM)(i))

LVM_GETHOVERTIME         = (LVM_FIRST + 72)
#define ListView_GetHoverTime(hwndLV)\
		  (DWORD)SNDMSG((hwndLV), LVM_GETHOVERTIME, 0, 0)

LVM_SETTOOLTIPS        = (LVM_FIRST + 74)
ListView_SetToolTips(hwndLV,  = hwndNewHwnd)\
		  (HWND)SNDMSG((hwndLV), LVM_SETTOOLTIPS, (WPARAM)(hwndNewHwnd), 0)

LVM_GETTOOLTIPS        = (LVM_FIRST + 78)
#define ListView_GetToolTips(hwndLV)\
		  (HWND)SNDMSG((hwndLV), LVM_GETTOOLTIPS, 0, 0)


LVM_SORTITEMSEX           = (LVM_FIRST + 81)
ListView_SortItemsEx(hwndLV,  = _pfnCompare, _lPrm) \
  (BOOL)SNDMSG((hwndLV), LVM_SORTITEMSEX, (WPARAM)(LPARAM)(_lPrm), (LPARAM)(PFNLVCOMPARE)(_pfnCompare))
]]

LVBKIF_SOURCE_NONE       = 0x00000000
LVBKIF_SOURCE_HBITMAP    = 0x00000001
LVBKIF_SOURCE_URL        = 0x00000002
LVBKIF_SOURCE_MASK       = 0x00000003
LVBKIF_STYLE_NORMAL      = 0x00000000
LVBKIF_STYLE_TILE        = 0x00000010
LVBKIF_STYLE_MASK        = 0x00000010

LVBKIF_FLAG_TILEOFFSET   = 0x00000100
LVBKIF_TYPE_WATERMARK    = 0x10000000


LVM_SETBKIMAGEA          = (LVM_FIRST + 68)
LVM_SETBKIMAGEW          = (LVM_FIRST + 138)
LVM_GETBKIMAGEA          = (LVM_FIRST + 69)
LVM_GETBKIMAGEW          = (LVM_FIRST + 139)

--[[

LVM_SETTILEWIDTH          = (LVM_FIRST + 141)
ListView_SetTileWidth(hwnd,  = cpWidth) \
	 SNDMSG((hwnd), LVM_SETTILEWIDTH, (WPARAM)cpWidth, 0)
]]

LV_VIEW_ICON         = 0x0000
LV_VIEW_DETAILS      = 0x0001
LV_VIEW_SMALLICON    = 0x0002
LV_VIEW_LIST         = 0x0003
LV_VIEW_TILE         = 0x0004
LV_VIEW_MAX          = 0x0004

--[[
LVM_SETVIEW          = (LVM_FIRST + 142)
ListView_SetView(hwnd,  = iView) \
	 (DWORD)SNDMSG((hwnd), LVM_SETVIEW, (WPARAM)(DWORD)iView, 0)

LVM_GETVIEW          = (LVM_FIRST + 143)
ListView_GetView(hwnd)  = \
	 (DWORD)SNDMSG((hwnd), LVM_GETVIEW, 0, 0)
]]

LVGF_NONE            = 0x00000000
LVGF_HEADER          = 0x00000001
LVGF_FOOTER          = 0x00000002
LVGF_STATE           = 0x00000004
LVGF_ALIGN           = 0x00000008
LVGF_GROUPID         = 0x00000010

LVGS_NORMAL          = 0x00000000
LVGS_COLLAPSED       = 0x00000001
LVGS_HIDDEN          = 0x00000002

LVGA_HEADER_LEFT     = 0x00000001
LVGA_HEADER_CENTER   = 0x00000002
LVGA_HEADER_RIGHT    = 0x00000004  -- Don't forget to validate exclusivity
LVGA_FOOTER_LEFT     = 0x00000008
LVGA_FOOTER_CENTER   = 0x00000010
LVGA_FOOTER_RIGHT    = 0x00000020  -- Don't forget to validate exclusivity

--[[
LVM_INSERTGROUP          = (LVM_FIRST + 145)
ListView_InsertGroup(hwnd,  = index, pgrp) \
	 SNDMSG((hwnd), LVM_INSERTGROUP, (WPARAM)index, (LPARAM)pgrp)


LVM_SETGROUPINFO          = (LVM_FIRST + 147)
ListView_SetGroupInfo(hwnd,  = iGroupId, pgrp) \
	 SNDMSG((hwnd), LVM_SETGROUPINFO, (WPARAM)iGroupId, (LPARAM)pgrp)


LVM_GETGROUPINFO          = (LVM_FIRST + 149)
ListView_GetGroupInfo(hwnd,  = iGroupId, pgrp) \
	 SNDMSG((hwnd), LVM_GETGROUPINFO, (WPARAM)iGroupId, (LPARAM)pgrp)

LVM_REMOVEGROUP          = (LVM_FIRST + 150)
ListView_RemoveGroup(hwnd,  = iGroupId) \
	 SNDMSG((hwnd), LVM_REMOVEGROUP, (WPARAM)iGroupId, 0)

LVM_MOVEGROUP          = (LVM_FIRST + 151)
ListView_MoveGroup(hwnd,  = iGroupId, toIndex) \
	 SNDMSG((hwnd), LVM_MOVEGROUP, (WPARAM)iGroupId, (LPARAM)toIndex)

LVM_MOVEITEMTOGROUP             = (LVM_FIRST + 154)
ListView_MoveItemToGroup(hwnd,  = idItemFrom, idGroupTo) \
	 SNDMSG((hwnd), LVM_MOVEITEMTOGROUP, (WPARAM)idItemFrom, (LPARAM)idGroupTo)
]]

LVGMF_NONE           = 0x00000000
LVGMF_BORDERSIZE     = 0x00000001
LVGMF_BORDERCOLOR    = 0x00000002
LVGMF_TEXTCOLOR      = 0x00000004

--[[
LVM_SETGROUPMETRICS          = (LVM_FIRST + 155)
ListView_SetGroupMetrics(hwnd,  = pGroupMetrics) \
	 SNDMSG((hwnd), LVM_SETGROUPMETRICS, 0, (LPARAM)pGroupMetrics)

LVM_GETGROUPMETRICS          = (LVM_FIRST + 156)
ListView_GetGroupMetrics(hwnd,  = pGroupMetrics) \
	 SNDMSG((hwnd), LVM_GETGROUPMETRICS, 0, (LPARAM)pGroupMetrics)

LVM_ENABLEGROUPVIEW          = (LVM_FIRST + 157)
ListView_EnableGroupView(hwnd,  = fEnable) \
	 SNDMSG((hwnd), LVM_ENABLEGROUPVIEW, (WPARAM)fEnable, 0)

LVM_SORTGROUPS          = (LVM_FIRST + 158)
ListView_SortGroups(hwnd,  = _pfnGroupCompate, _plv) \
	 SNDMSG((hwnd), LVM_SORTGROUPS, (WPARAM)_pfnGroupCompate, (LPARAM)_plv)

LVM_INSERTGROUPSORTED            = (LVM_FIRST + 159)
ListView_InsertGroupSorted(hwnd,  = structInsert) \
	 SNDMSG((hwnd), LVM_INSERTGROUPSORTED, (WPARAM)structInsert, 0)

LVM_REMOVEALLGROUPS              = (LVM_FIRST + 160)
ListView_RemoveAllGroups(hwnd)  = \
	 SNDMSG((hwnd), LVM_REMOVEALLGROUPS, 0, 0)

LVM_HASGROUP                     = (LVM_FIRST + 161)
ListView_HasGroup(hwnd,  = dwGroupId) \
	 SNDMSG((hwnd), LVM_HASGROUP, dwGroupId, 0)

LVTVIF_AUTOSIZE        = 0x00000000
LVTVIF_FIXEDWIDTH      = 0x00000001
LVTVIF_FIXEDHEIGHT     = 0x00000002
LVTVIF_FIXEDSIZE       = 0x00000003

LVTVIM_TILESIZE        = 0x00000001
LVTVIM_COLUMNS         = 0x00000002
LVTVIM_LABELMARGIN     = 0x00000004

LVM_SETTILEVIEWINFO                  = (LVM_FIRST + 162)
ListView_SetTileViewInfo(hwnd,  = ptvi) \
	 SNDMSG((hwnd), LVM_SETTILEVIEWINFO, 0, (LPARAM)ptvi)

LVM_GETTILEVIEWINFO                  = (LVM_FIRST + 163)
ListView_GetTileViewInfo(hwnd,  = ptvi) \
	 SNDMSG((hwnd), LVM_GETTILEVIEWINFO, 0, (LPARAM)ptvi)

LVM_SETTILEINFO                      = (LVM_FIRST + 164)
ListView_SetTileInfo(hwnd,  = pti) \
	 SNDMSG((hwnd), LVM_SETTILEINFO, 0, (LPARAM)pti)

LVM_GETTILEINFO                      = (LVM_FIRST + 165)
ListView_GetTileInfo(hwnd,  = pti) \
	 SNDMSG((hwnd), LVM_GETTILEINFO, 0, (LPARAM)pti)

LVIM_AFTER       = 0x00000001 -- TRUE = insert After iItem, otherwise before

LVM_SETINSERTMARK                    = (LVM_FIRST + 166)
ListView_SetInsertMark(hwnd,  = lvim) \
	 (BOOL)SNDMSG((hwnd), LVM_SETINSERTMARK, (WPARAM) 0, (LPARAM) (lvim))

LVM_GETINSERTMARK                    = (LVM_FIRST + 167)
ListView_GetInsertMark(hwnd,  = lvim) \
	 (BOOL)SNDMSG((hwnd), LVM_GETINSERTMARK, (WPARAM) 0, (LPARAM) (lvim))

LVM_INSERTMARKHITTEST                = (LVM_FIRST + 168)
ListView_InsertMarkHitTest(hwnd,  = point, lvim) \
	 (int)SNDMSG((hwnd), LVM_INSERTMARKHITTEST, (WPARAM)(LPPOINT)(point), (LPARAM)(LPLVINSERTMARK)(lvim))

LVM_GETINSERTMARKRECT                = (LVM_FIRST + 169)
ListView_GetInsertMarkRect(hwnd,  = rc) \
	 (int)SNDMSG((hwnd), LVM_GETINSERTMARKRECT, (WPARAM)0, (LPARAM)(LPRECT)(rc))

LVM_SETINSERTMARKCOLOR                  = (LVM_FIRST + 170)
ListView_SetInsertMarkColor(hwnd,  = color) \
	 (COLORREF)SNDMSG((hwnd), LVM_SETINSERTMARKCOLOR, (WPARAM)0, (LPARAM)(COLORREF)(color))

LVM_GETINSERTMARKCOLOR                  = (LVM_FIRST + 171)
ListView_GetInsertMarkColor(hwnd)  = \
	 (COLORREF)SNDMSG((hwnd), LVM_GETINSERTMARKCOLOR, (WPARAM)0, (LPARAM)0)

LVM_SETINFOTIP         = (LVM_FIRST + 173)

ListView_SetInfoTip(hwndLV,  = plvInfoTip)\
		  (BOOL)SNDMSG((hwndLV), LVM_SETINFOTIP, (WPARAM)0, (LPARAM)plvInfoTip)

LVM_ISGROUPVIEWENABLED   = (LVM_FIRST + 175)
ListView_IsGroupViewEnabled(hwnd)  = \
	 (BOOL)SNDMSG((hwnd), LVM_ISGROUPVIEWENABLED, 0, 0)

LVM_GETOUTLINECOLOR      = (LVM_FIRST + 176)
ListView_GetOutlineColor(hwnd)  = \
	 (COLORREF)SNDMSG((hwnd), LVM_GETOUTLINECOLOR, 0, 0)

LVM_SETOUTLINECOLOR      = (LVM_FIRST + 177)
ListView_SetOutlineColor(hwnd,  = color) \
	 (COLORREF)SNDMSG((hwnd), LVM_SETOUTLINECOLOR, (WPARAM)0, (LPARAM)(COLORREF)(color))

LVM_CANCELEDITLABEL      = (LVM_FIRST + 179)
ListView_CancelEditLabel(hwnd)  = \
	 (VOID)SNDMSG((hwnd), LVM_CANCELEDITLABEL, (WPARAM)0, (LPARAM)0)

-- These next to methods make it easy to identify an item that can be repositioned
-- within listview. For example: Many developers use the lParam to store an identifier that is
-- unique. Unfortunatly, in order to find this item, they have to iterate through all of the items
-- in the listview. Listview will maintain a unique identifier.  The upper bound is the size of a DWORD.
LVM_MAPINDEXTOID      = (LVM_FIRST + 180)
ListView_MapIndexToID(hwnd,  = index) \
	 (UINT)SNDMSG((hwnd), LVM_MAPINDEXTOID, (WPARAM)index, (LPARAM)0)

LVM_MAPIDTOINDEX      = (LVM_FIRST + 181)
ListView_MapIDToIndex(hwnd,  = id) \
	 (UINT)SNDMSG((hwnd), LVM_MAPIDTOINDEX, (WPARAM)id, (LPARAM)0)

ListView_SetBkImage(hwnd,  = plvbki) \
	 (BOOL)SNDMSG((hwnd), LVM_SETBKIMAGE, 0, (LPARAM)(plvbki))

ListView_GetBkImage(hwnd,  = plvbki) \
	 (BOOL)SNDMSG((hwnd), LVM_GETBKIMAGE, 0, (LPARAM)(plvbki))
]]

-- key flags stored in uKeyFlags
LVKF_ALT        = 0x0001
LVKF_CONTROL    = 0x0002
LVKF_SHIFT      = 0x0004

--NMLVCUSTOMDRAW_V3_SIZE  = CCSIZEOF_STRUCT(NMLVCUSTOMDRW, clrTextBk)

-- dwItemType
LVCDI_ITEM       = 0x00000000
LVCDI_GROUP      = 0x00000001

-- ListView custom draw return values
LVCDRF_NOSELECT              = 0x00010000
LVCDRF_NOGROUPFRAME          = 0x00020000


--#include <pshpack1.h>
--#include <poppack.h>


-- NMLVGETINFOTIPA.dwFlag values

LVGIT_UNFOLDED   = 0x0001


]===]





--[=[



ffi.cdef[[
typedef struct tagLVFINDINFOW
{
	 UINT flags;
	 LPCWSTR psz;
	 LPARAM lParam;
	 POINT pt;
	 UINT vkDirection;
} LVFINDINFOW, *LPFINDINFOW;

typedef struct tagLVHITTESTINFO
{
	 POINT pt;
	 UINT flags;
	 int iItem;
	 int iSubItem;
} LVHITTESTINFO, *LPLVHITTESTINFO;

typedef struct tagLVBKIMAGEW
{
	 ULONG ulFlags;
	 HBITMAP hbm;
	 LPWSTR pszImage;
	 UINT cchImageMax;
	 int xOffsetPercent;
	 int yOffsetPercent;
} LVBKIMAGEW, *LPLVBKIMAGEW;

typedef struct tagLVGROUP
{
	 UINT    cbSize;
	 UINT    mask;
	 LPWSTR  pszHeader;
	 int     cchHeader;
	 LPWSTR  pszFooter;
	 int     cchFooter;
	 int     iGroupId;
	 UINT    stateMask;
	 UINT    state;
	 UINT    uAlign;
} LVGROUP, *PLVGROUP;

typedef struct tagLVGROUPMETRICS
{
	 UINT cbSize;
	 UINT mask;
	 UINT Left;
	 UINT Top;
	 UINT Right;
	 UINT Bottom;
	 COLORREF crLeft;
	 COLORREF crTop;
	 COLORREF crRight;
	 COLORREF crBottom;
	 COLORREF crHeader;
	 COLORREF crFooter;
} LVGROUPMETRICS, *PLVGROUPMETRICS;

typedef int (__stdcall *PFNLVGROUPCOMPARE)(int, int, void *);

typedef struct tagLVINSERTGROUPSORTED
{
	 PFNLVGROUPCOMPARE pfnGroupCompare;
	 void *pvData;
	 LVGROUP lvGroup;
}LVINSERTGROUPSORTED, *PLVINSERTGROUPSORTED;

typedef struct tagLVTILEVIEWINFO
{
	 UINT cbSize;
	 DWORD dwMask;
	 DWORD dwFlags;
	 SIZE sizeTile;
	 int cLines;
	 RECT rcLabelMargin;
} LVTILEVIEWINFO, *PLVTILEVIEWINFO;

typedef struct tagLVTILEINFO
{
	 UINT cbSize;
	 int iItem;
	 UINT cColumns;
	 PUINT puColumns;
} LVTILEINFO, *PLVTILEINFO;

typedef struct
{
	 UINT cbSize;
	 DWORD dwFlags;
	 int iItem;
	 DWORD dwReserved;
} LVINSERTMARK, * LPLVINSERTMARK;

typedef struct tagLVSETINFOTIP
{
	 UINT cbSize;
	 DWORD dwFlags;
	 LPWSTR pszText;
	 int iItem;
	 int iSubItem;
} LVSETINFOTIP, *PLVSETINFOTIP;

typedef struct tagNMITEMACTIVATE
{
	 NMHDR   hdr;
	 int     iItem;
	 int     iSubItem;
	 UINT    uNewState;
	 UINT    uOldState;
	 UINT    uChanged;
	 POINT   ptAction;
	 LPARAM  lParam;
	 UINT    uKeyFlags;
} NMITEMACTIVATE, *LPNMITEMACTIVATE;

typedef struct tagNMLVCUSTOMDRAW
{
	 NMCUSTOMDRAW nmcd;
	 COLORREF clrText;
	 COLORREF clrTextBk;
	 int iSubItem;
	 DWORD dwItemType;
	 COLORREF clrFace;
	 int iIconEffect;
	 int iIconPhase;
	 int iPartId;
	 int iStateId;
	 RECT rcText;
	 UINT uAlign;

} NMLVCUSTOMDRAW, *LPNMLVCUSTOMDRAW;

typedef struct tagNMLVCACHEHINT
{
	 NMHDR   hdr;
	 int     iFrom;
	 int     iTo;
} NMLVCACHEHINT, *LPNMLVCACHEHINT;

typedef struct tagNMLVFINDITEMW
{
	 NMHDR   hdr;
	 int     iStart;
	 LVFINDINFOW lvfi;
} NMLVFINDITEMW, *LPNMLVFINDITEMW;


typedef struct tagLVDISPINFOW {
	 NMHDR hdr;
	 LVITEMW item;
} NMLVDISPINFOW, *LPNMLVDISPINFOW;

typedef struct tagLVKEYDOWN
{
	 NMHDR hdr;
	 WORD wVKey;
	 UINT flags;
} NMLVKEYDOWN, *LPNMLVKEYDOWN;

typedef struct tagNMLVGETINFOTIPW
{
	 NMHDR hdr;
	 DWORD dwFlags;
	 LPWSTR pszText;
	 int cchTextMax;
	 int iItem;
	 int iSubItem;
	 LPARAM lParam;
} NMLVGETINFOTIPW, *LPNMLVGETINFOTIPW;

typedef struct tagNMLVSCROLL
{
	 NMHDR   hdr;
	 int     dx;
	 int     dy;
} NMLVSCROLL, *LPNMLVSCROLL;

]]

]=]

