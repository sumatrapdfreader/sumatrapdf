--proc/tabcontrol: standard tab control.
setfenv(1, require'winapi')
require'winapi.window'
require'winapi.comctl'

InitCommonControlsEx(ICC_TAB_CLASSES)

--creation

WC_TABCONTROL            = 'SysTabControl32'

TCS_SCROLLOPPOSITE       = 0x0001   -- assumes multiline  = tab
TCS_BOTTOM               = 0x0002
TCS_RIGHT                = 0x0002
TCS_MULTISELECT          = 0x0004  -- allow multi-select in button  = mode
TCS_FLATBUTTONS          = 0x0008
TCS_FORCEICONLEFT        = 0x0010
TCS_FORCELABELLEFT       = 0x0020
TCS_HOTTRACK             = 0x0040
TCS_VERTICAL             = 0x0080
TCS_TABS                 = 0x0000
TCS_BUTTONS              = 0x0100
TCS_SINGLELINE           = 0x0000
TCS_MULTILINE            = 0x0200
TCS_RIGHTJUSTIFY         = 0x0000
TCS_FIXEDWIDTH           = 0x0400
TCS_RAGGEDRIGHT          = 0x0800
TCS_FOCUSONBUTTONDOWN    = 0x1000
TCS_OWNERDRAWFIXED       = 0x2000
TCS_TOOLTIPS             = 0x4000
TCS_FOCUSNEVER           = 0x8000

-- EX styles for use with TCM_SETEXTENDEDSTYLE
TCS_EX_FLATSEPARATORS    = 0x00000001
TCS_EX_REGISTERDROP      = 0x00000002

--commands

TCM_FIRST               = 0x1300
TCM_INSERTITEMW         = (TCM_FIRST + 62)
TCM_GETITEMW            = (TCM_FIRST + 60)
TCM_GETITEMCOUNT        = (TCM_FIRST + 4)
TCM_SETITEMW            = (TCM_FIRST + 61)
TCM_DELETEITEM          = (TCM_FIRST + 8)
TCM_DELETEALLITEMS      = (TCM_FIRST + 9)
TCM_GETCURFOCUS         = (TCM_FIRST + 47)
TCM_SETCURFOCUS         = (TCM_FIRST + 48)
TCM_GETCURSEL           = (TCM_FIRST + 11)
TCM_SETCURSEL           = (TCM_FIRST + 12)
TCM_GETIMAGELIST        = (TCM_FIRST + 2)
TCM_SETIMAGELIST        = (TCM_FIRST + 3)

TCIF_TEXT                = 0x0001
TCIF_IMAGE               = 0x0002
TCIF_RTLREADING          = 0x0004
TCIF_PARAM               = 0x0008
TCIF_STATE               = 0x0010

TCIS_BUTTONPRESSED       = 0x0001
TCIS_HIGHLIGHTED         = 0x0002

ffi.cdef[[
typedef struct tagTCITEMW
{
    UINT mask;
    DWORD dwState;
    DWORD dwStateMask;
    LPWSTR pszText;
    int cchTextMax;
    int iImage;
    LPARAM lParam;
} TCITEMW, *LPTCITEMW;
]]

TCITEM = struct{
	ctype = 'TCITEMW', mask = 'mask',
	fields = mfields{
		'__state', 'dwState', TCIF_STATE, flags, pass,
		'__stateMask', 'dwState', TCIF_STATE, flags, pass,
		'text', 'pszText', TCIF_TEXT, wcs, mbs,
		'image', 'iImage', TCIF_IMAGE, countfrom0, countfrom1,
	},
	bitfields = {
		state = {'__state', '__stateMask', 'TCIS'},
	}
}

function TabCtrl_InsertItem(tab, i, item)
	item = TCITEM(item)
	return SNDMSG(tab, TCM_INSERTITEMW, countfrom0(i), ffi.cast('LPTCITEMW', item))
end

function TabCtrl_DeleteItem(tab, i)
	checknz(SNDMSG(tab, TCM_DELETEITEM, countfrom0(i)))
end

function TabCtrl_DeleteAllItems(tab)
	checknz(SNDMSG(tab, TCM_DELETEALLITEMS))
end

function TabCtrl_SetItem(tab, i, item)
	item = TCITEM(item)
	return checknz(SNDMSG(tab, TCM_SETITEMW, countfrom0(i), ffi.cast('LPTCITEMW', item)))
end

function TabCtrl_GetItem(tab, i, item)
	item = TCITEM:setmask(item)
	local ws, sz
	if not ptr(item.pszText) then --user didn't supply a buffer
		ws, sz = WCS()
		item.text = ws --we set text instead of pszText so that ws gets pinned to item
		item.cchTextMax = sz
	else
		ws = item.pszText
		sz = item.cchTextMax
	end
	checknz(SNDMSG(tab, TCM_GETITEMW, countfrom0(i), ffi.cast('LPTCITEMW', item)))
	return item
end

function TabCtrl_GetItemCount(tab)
    return checkpoz(SNDMSG(tab, TCM_GETITEMCOUNT))
end

function TabCtrl_GetCurFocus(tab)
	return checkpoz(SNDMSG(tab, TCM_GETCURFOCUS)+1)
end

function TabCtrl_SetCurFocus(tab, i)
    checkz(SNDMSG(tab, TCM_SETCURFOCUS, countfrom0(i)))
end

function TabCtrl_GetCurSel(tab, i)
	return countfrom1(SNDMSG(tab, TCM_GETCURSEL))
end

function TabCtrl_SetCurSel(tab, i) --index of prev. selection if any
	return countfrom1(SNDMSG(tab, TCM_SETCURSEL, countfrom0(i)))
end

function TabCtrl_GetImageList(tab)
	return ptr(ffi.cast('HIMAGELIST', SNDMSG(tab, TCM_GETIMAGELIST)))
end

function TabCtrl_SetImageList(tab, iml)
	return ptr(ffi.cast('HIMAGELIST', SNDMSG(tab, TCM_SETIMAGELIST, 0, iml)))
end


--[[

TCN_FIRST               = ffi.cast('UINT', -550)

TCM_GETITEMRECT         (TCM_FIRST +  = 10)
TabCtrl_GetItemRect(hwnd, i, prc)  = \
    (BOOL)SNDMSG((hwnd), TCM_GETITEMRECT, (WPARAM)(int)(i), (LPARAM)(RECT *)(prc))

TCHT_NOWHERE             = 0x0001
TCHT_ONITEMICON          = 0x0002
TCHT_ONITEMLABEL         = 0x0004
TCHT_ONITEM              = bit.bor(TCHT_ONITEMICON, TCHT_ONITEMLABEL)


typedef struct tagTCHITTESTINFO
{
    POINT pt;
    UINT flags;
} TCHITTESTINFO, *LPTCHITTESTINFO;

TCM_HITTEST             (TCM_FIRST +  = 13)
TabCtrl_HitTest(hwndTC, pinfo)  = \
    (int)SNDMSG((hwndTC), TCM_HITTEST, 0, (LPARAM)(TC_HITTESTINFO *)(pinfo))


TCM_SETITEMEXTRA        (TCM_FIRST +  = 14)
TabCtrl_SetItemExtra(hwndTC, cb)  = \
    (BOOL)SNDMSG((hwndTC), TCM_SETITEMEXTRA, (WPARAM)(cb), 0L)


TCM_ADJUSTRECT          (TCM_FIRST +  = 40)
TabCtrl_AdjustRect(hwnd, bLarger, prc)  = \
    (int)SNDMSG(hwnd, TCM_ADJUSTRECT, (WPARAM)(BOOL)(bLarger), (LPARAM)(RECT *)(prc))


TCM_SETITEMSIZE         (TCM_FIRST +  = 41)
TabCtrl_SetItemSize(hwnd, x, y)  = \
    (DWORD)SNDMSG((hwnd), TCM_SETITEMSIZE, 0, MAKELPARAM(x,y))


TCM_REMOVEIMAGE         (TCM_FIRST +  = 42)
TabCtrl_RemoveImage(hwnd, i)  = \
        (void)SNDMSG((hwnd), TCM_REMOVEIMAGE, i, 0L)


TCM_SETPADDING          (TCM_FIRST +  = 43)
TabCtrl_SetPadding(hwnd,  cx, cy)  = \
        (void)SNDMSG((hwnd), TCM_SETPADDING, 0, MAKELPARAM(cx, cy))


TCM_GETROWCOUNT         (TCM_FIRST +  = 44)
TabCtrl_GetRowCount(hwnd)  = \
        (int)SNDMSG((hwnd), TCM_GETROWCOUNT, 0, 0L)


TCM_GETTOOLTIPS         (TCM_FIRST +  = 45)
TabCtrl_GetToolTips(hwnd)  = \
        (HWND)SNDMSG((hwnd), TCM_GETTOOLTIPS, 0, 0L)


TCM_SETTOOLTIPS         (TCM_FIRST +  = 46)
TabCtrl_SetToolTips(hwnd, hwndTT)  = \
        (void)SNDMSG((hwnd), TCM_SETTOOLTIPS, (WPARAM)(hwndTT), 0L)

TCM_SETMINTABWIDTH      (TCM_FIRST +  = 49)
TabCtrl_SetMinTabWidth(hwnd, x)  = \
        (int)SNDMSG((hwnd), TCM_SETMINTABWIDTH, 0, x)

TCM_DESELECTALL         (TCM_FIRST +  = 50)
TabCtrl_DeselectAll(hwnd,  = fExcludeFocus)\
        (void)SNDMSG((hwnd), TCM_DESELECTALL, fExcludeFocus, 0)

TCM_HIGHLIGHTITEM       (TCM_FIRST +  = 51)
TabCtrl_HighlightItem(hwnd, i, fHighlight)  = \
    (BOOL)SNDMSG((hwnd), TCM_HIGHLIGHTITEM, (WPARAM)(i), (LPARAM)MAKELONG (fHighlight, 0))

TCM_SETEXTENDEDSTYLE    (TCM_FIRST + 52)  -- optional wParam ==  = mask
TabCtrl_SetExtendedStyle(hwnd,  = dw)\
        (DWORD)SNDMSG((hwnd), TCM_SETEXTENDEDSTYLE, 0, dw)

TCM_GETEXTENDEDSTYLE    (TCM_FIRST +  = 53)
#define TabCtrl_GetExtendedStyle(hwnd)\
        (DWORD)SNDMSG((hwnd), TCM_GETEXTENDEDSTYLE, 0, 0)

TCM_SETUNICODEFORMAT      = CCM_SETUNICODEFORMAT
TabCtrl_SetUnicodeFormat(hwnd, fUnicode)   = \
    (BOOL)SNDMSG((hwnd), TCM_SETUNICODEFORMAT, (WPARAM)(fUnicode), 0)

TCM_GETUNICODEFORMAT      = CCM_GETUNICODEFORMAT
TabCtrl_GetUnicodeFormat(hwnd)   = \
    (BOOL)SNDMSG((hwnd), TCM_GETUNICODEFORMAT, 0, 0)

TCN_KEYDOWN             (TCN_FIRST -  = 0)

typedef struct tagTCKEYDOWN
{
    NMHDR hdr;
    WORD wVKey;
    UINT flags;
} NMTCKEYDOWN;

TCN_SELCHANGE           (TCN_FIRST -  = 1)
TCN_SELCHANGING         (TCN_FIRST -  = 2)
TCN_GETOBJECT           (TCN_FIRST -  = 3)
TCN_FOCUSCHANGE         (TCN_FIRST -  = 4)

]]
