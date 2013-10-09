--proc/treeview: standard treeview control.
setfenv(1, require'winapi')
require'winapi.window'
require'winapi.comctl'

InitCommonControlsEx(ICC_TREEVIEW_CLASSES)

--creation

WC_TREEVIEW = "SysTreeView32"

TVS_HASBUTTONS           = 0x0001
TVS_HASLINES             = 0x0002
TVS_LINESATROOT          = 0x0004
TVS_EDITLABELS           = 0x0008
TVS_DISABLEDRAGDROP      = 0x0010
TVS_SHOWSELALWAYS        = 0x0020
TVS_RTLREADING           = 0x0040
TVS_NOTOOLTIPS           = 0x0080
TVS_CHECKBOXES           = 0x0100
TVS_TRACKSELECT          = 0x0200
TVS_SINGLEEXPAND         = 0x0400
TVS_INFOTIP              = 0x0800
TVS_FULLROWSELECT        = 0x1000
TVS_NOSCROLL             = 0x2000
TVS_NONEVENHEIGHT        = 0x4000
TVS_NOHSCROLL            = 0x8000  -- TVS_NOSCROLL overrides this

TVS_EX_MULTISELECT           = 0x0002
TVS_EX_DOUBLEBUFFER          = 0x0004
TVS_EX_NOINDENTSTATE         = 0x0008
TVS_EX_RICHTOOLTIP           = 0x0010
TVS_EX_AUTOHSCROLL           = 0x0020
TVS_EX_FADEINOUTEXPANDOS     = 0x0040
TVS_EX_PARTIALCHECKBOXES     = 0x0080
TVS_EX_EXCLUSIONCHECKBOXES   = 0x0100
TVS_EX_DIMMEDCHECKBOXES      = 0x0200
TVS_EX_DRAWIMAGEASYNC        = 0x0400

TREEVIEW_DEFAULTS = {
	class = WC_TREEVIEW,
	style = bit.bor(WS_CHILD, WS_VISIBLE, TVS_HASBUTTONS, TVS_HASLINES, TVS_LINESATROOT),
	style_ex = bit.bor(WS_EX_CLIENTEDGE),
	x = 10, y = 10,
	w = 200, h = 100,
}

function CreateTreeView(info)
	info = update({}, TREEVIEW_DEFAULTS, info)
	return CreateWindow(info)
end

--commands

TVIF_TEXT                = 0x0001
TVIF_IMAGE               = 0x0002
TVIF_PARAM               = 0x0004
TVIF_STATE               = 0x0008
TVIF_HANDLE              = 0x0010
TVIF_SELECTEDIMAGE       = 0x0020
TVIF_CHILDREN            = 0x0040
TVIF_INTEGRAL            = 0x0080
TVIF_STATEEX             = 0x0100
TVIF_EXPANDEDIMAGE       = 0x0200

TVIS_SELECTED            = 0x0002
TVIS_CUT                 = 0x0004
TVIS_DROPHILITED         = 0x0008
TVIS_BOLD                = 0x0010
TVIS_EXPANDED            = 0x0020
TVIS_EXPANDEDONCE        = 0x0040
TVIS_EXPANDPARTIAL       = 0x0080
TVIS_OVERLAYMASK         = 0x0F00
TVIS_STATEIMAGEMASK      = 0xF000
TVIS_USERMASK            = 0xF000

TVIS_EX_FLAT             = 0x0001
TVIS_EX_DISABLED         = 0x0002
TVIS_EX_ALL              = 0x0002

TVI_ROOT                 = ffi.cast('UINT', -0x10000)
TVI_FIRST                = ffi.cast('UINT', -0x0FFFF)
TVI_LAST                 = ffi.cast('UINT', -0x0FFFE)
TVI_SORT                 = ffi.cast('UINT', -0x0FFFD)

TV_FIRST = 0x1100

ffi.cdef[[
struct _TREEITEM;
typedef struct _TREEITEM *HTREEITEM;

typedef struct tagTVITEMEXW {
	 UINT      mask;
	 HTREEITEM hItem;
	 UINT      _state;
	 UINT      _stateMask;
	 LPWSTR    pszText;
	 int       cchTextMax;
	 int       iImage;
	 int       iSelectedImage;
	 int       cChildren;
	 LPARAM    lParam;
	 int       iIntegral;
	 UINT      uStateEx;
	 HWND      hwnd;
	 int       iExpandedImage;
	 int       iReserved;
} TVITEMEXW, *LPTVITEMEXW;

typedef struct tagTVINSERTSTRUCTW {
	 HTREEITEM hParent;
	 HTREEITEM hInsertAfter;
	 TVITEMEXW itemex;
} TVINSERTSTRUCTW, *LPTVINSERTSTRUCTW;
]]

TVITEMEXW = struct{
	ctype = 'TVITEMEXW', mask = 'mask',
	fields = mfields{
		'item', 'hItem', 0, pass,
		'__state', '_state', TVIF_STATE, flags,
		'__stateMask', '_stateMask', TVIF_STATE, flags,
		'text', 'pszText', TVIF_TEXT, wcs,
		'image', 'iImage', TVIF_IMAGE, pass,
		'selected_image', 'iSelectedImage', TVIF_SELECTEDIMAGE, pass,
		'children_count', 'cChildren', TVIF_CHILDREN, pass,
		'integral', 'iIntegral', TVIF_INTEGRAL, pass,
		'state_ex', 'uStateEx', TVIF_STATEEX, pass,
		'handle', 'hwnd', TVIF_HANDLE, pass,
		'expanded_image', 'iExpandedImage', TVIF_EXPANDEDIMAGE, pass,
	},
	bitfields = {
		state = {'__state', '__stateMask', 'TVIS'},
	}
}

TVINSERTSTRUCTW = struct{
	ctype = 'TVINSERTSTRUCTW',
	fields = sfields{
		'parent', 'hParent', 'HTREEITEM',
		'insert_after', 'hInsertAfter', 'HTREEITEM',
		'item', 'itemex', TVITEMEXW,
	}
}

TVM_INSERTITEMW          = (TV_FIRST + 50)
TVM_GETITEMW             = (TV_FIRST + 62)
TVM_SETITEMW             = (TV_FIRST + 63)
TVM_DELETEITEM           = (TV_FIRST + 1)
TVM_EXPAND               = (TV_FIRST + 2)
TVM_GETIMAGELIST         = (TV_FIRST + 8)
TVM_SETIMAGELIST         = (TV_FIRST + 9)

function TreeView_InsertItem(tv, item)
	 item = TVINSERTSTRUCTW(item)
	 return ffi.cast('HTREEITEM', checkh(SNDMSG(tv, TVM_INSERTITEMW, 0,
							ffi.cast('LPTVINSERTSTRUCTW', item))))
end

function TreeView_GetItem(tv, item)
	item = TVITEMEXW(item)
	checknz(SNDMSG(TVM_GETITEM, 0, ffi.cast('LPTVITEMEXW', item)))
	return item
end

function TreeView_SetItem(hwnd, item)
	checknz(SNDMSG(TVM_SETITEM, 0, ffi.cast('LPTVITEMEXW', TVITEMEXW(item))))
end

function TreeView_DeleteItem(tv, item)
	return checkpoz(SNDMSG(tv, TVM_DELETEITEM, 0, item))
end

function TreeView_DeleteAllItems(tv)
	return TreeView_DeleteItem(tv, TVI_ROOT)
end

TVE_COLLAPSE             = 0x0001
TVE_EXPAND               = 0x0002
TVE_TOGGLE               = 0x0003
TVE_EXPANDPARTIAL        = 0x4000
TVE_COLLAPSERESET        = 0x8000

function TreeView_Expand(tv, item, code)
	return checkpoz(SNDMSG(tv, TVM_EXPAND, code, item))
end

TVSIL_NORMAL = 0
TVSIL_STATE  = 2

function TreeView_GetImageList(tv, TVSIL)
	return checkpoz(SNDMSG(tv, TVM_GETIMAGELIST, TVSIL))
end

function TreeView_SetImageList(tv, TVSIL, iml)
	return checkh(SNDMSG(tv, TVM_SETIMAGELIST, TVSIL, iml))
end

--showcase

if not ... then
require'winapi.showcase'
local window = ShowcaseWindow()

--make a tree view with some items
local tv = CreateTreeView{parent = window}
local root = TreeView_InsertItem(tv, {parent = TVI_ROOT, item = {state_BOLD = true, text = 'I am ROOT'}})
local weasel = TreeView_InsertItem(tv, {parent = root, item = {state = {BOLD = true}, text = 'I am Weasel'}})
local baboon = TreeView_InsertItem(tv, {parent = weasel, item = {text = 'And I \xc5\x98 baboon'}})

--load some standard icons and assign them to the treeview
local il = ShowcaseImageList()
TreeView_SetImageList(tv, TVSIL_NORMAL, il)

TreeView_Expand(tv, root, TVE_EXPAND)

MessageLoop()
end

--[[
TVN_FIRST               = ffi.cast('UINT', -400)

typedef struct tagNMTVSTATEIMAGECHANGING
{
	 NMHDR hdr;
	 HTREEITEM hti;
	 int iOldStateImageIndex;
	 int iNewStateImageIndex;
} NMTVSTATEIMAGECHANGING, *LPNMTVSTATEIMAGECHANGING;


I_CHILDRENCALLBACK   = (-1)


TVM_GETITEMRECT          = (TV_FIRST + 4)
TreeView_GetItemRect(hwnd,  = hitem, prc, code) \
	 (*(HTREEITEM *)(prc) = (hitem), (BOOL)SNDMSG((hwnd), TVM_GETITEMRECT, (WPARAM)(code), (LPARAM)(RECT *)(prc)))


TVM_GETCOUNT             = (TV_FIRST + 5)
TreeView_GetCount(hwnd)  = \
	 (UINT)SNDMSG((hwnd), TVM_GETCOUNT, 0, 0)


TVM_GETINDENT            = (TV_FIRST + 6)
TreeView_GetIndent(hwnd)  = \
	 (UINT)SNDMSG((hwnd), TVM_GETINDENT, 0, 0)


TVM_SETINDENT            = (TV_FIRST + 7)
TreeView_SetIndent(hwnd,  = indent) \
	 (BOOL)SNDMSG((hwnd), TVM_SETINDENT, (WPARAM)(indent), 0)


TVM_GETNEXTITEM          = (TV_FIRST + 10)
TreeView_GetNextItem(hwnd,  = hitem, code) \
	 (HTREEITEM)SNDMSG((hwnd), TVM_GETNEXTITEM, (WPARAM)(code), (LPARAM)(HTREEITEM)(hitem))


TVGN_ROOT                = 0x0000
TVGN_NEXT                = 0x0001
TVGN_PREVIOUS            = 0x0002
TVGN_PARENT              = 0x0003
TVGN_CHILD               = 0x0004
TVGN_FIRSTVISIBLE        = 0x0005
TVGN_NEXTVISIBLE         = 0x0006
TVGN_PREVIOUSVISIBLE     = 0x0007
TVGN_DROPHILITE          = 0x0008
TVGN_CARET               = 0x0009
TVGN_LASTVISIBLE         = 0x000A
TVGN_NEXTSELECTED        = 0x000B

TVSI_NOSINGLEEXPAND     = 0x8000 -- Should not conflict with TVGN flags.

TreeView_GetChild(hwnd,  = hitem)        TreeView_GetNextItem(hwnd, hitem, TVGN_CHILD)
TreeView_GetNextSibling(hwnd,  = hitem)  TreeView_GetNextItem(hwnd, hitem, TVGN_NEXT)
TreeView_GetPrevSibling(hwnd,  = hitem)  TreeView_GetNextItem(hwnd, hitem, TVGN_PREVIOUS)
TreeView_GetParent(hwnd,  = hitem)       TreeView_GetNextItem(hwnd, hitem, TVGN_PARENT)
TreeView_GetFirstVisible(hwnd)           TreeView_GetNextItem(hwnd, NULL,  TVGN_FIRSTVISIBLE)
TreeView_GetNextVisible(hwnd,  = hitem)  TreeView_GetNextItem(hwnd, hitem, TVGN_NEXTVISIBLE)
TreeView_GetPrevVisible(hwnd,  = hitem)  TreeView_GetNextItem(hwnd, hitem, TVGN_PREVIOUSVISIBLE)
TreeView_GetSelection(hwnd)              TreeView_GetNextItem(hwnd, NULL,  TVGN_CARET)
TreeView_GetDropHilight(hwnd)            TreeView_GetNextItem(hwnd, NULL,  TVGN_DROPHILITE)
TreeView_GetRoot(hwnd)                   TreeView_GetNextItem(hwnd, NULL,  TVGN_ROOT)
TreeView_GetLastVisible(hwnd)            TreeView_GetNextItem(hwnd, NULL,  TVGN_LASTVISIBLE)
TreeView_GetNextSelected(hwnd, hitem)    TreeView_GetNextItem(hwnd, hitem,  TVGN_NEXTSELECTED)

TVM_SELECTITEM           = (TV_FIRST + 11)
TreeView_Select(hwnd,  = hitem, code) \
	 (BOOL)SNDMSG((hwnd), TVM_SELECTITEM, (WPARAM)(code), (LPARAM)(HTREEITEM)(hitem))

TreeView_SelectItem(hwnd,  = hitem)            TreeView_Select(hwnd, hitem, TVGN_CARET)
TreeView_SelectDropTarget(hwnd,  = hitem)      TreeView_Select(hwnd, hitem, TVGN_DROPHILITE)
TreeView_SelectSetFirstVisible(hwnd,  = hitem) TreeView_Select(hwnd, hitem, TVGN_FIRSTVISIBLE)

TVM_EDITLABELW           = (TV_FIRST + 65)
TreeView_EditLabel(hwnd,  = hitem) \
	 (HWND)SNDMSG((hwnd), TVM_EDITLABEL, 0, (LPARAM)(HTREEITEM)(hitem))

TVM_GETEDITCONTROL       = (TV_FIRST + 15)
TreeView_GetEditControl(hwnd)  = \
	 (HWND)SNDMSG((hwnd), TVM_GETEDITCONTROL, 0, 0)


TVM_GETVISIBLECOUNT      = (TV_FIRST + 16)
TreeView_GetVisibleCount(hwnd)  = \
	 (UINT)SNDMSG((hwnd), TVM_GETVISIBLECOUNT, 0, 0)


TVM_HITTEST              = (TV_FIRST + 17)
TreeView_HitTest(hwnd,  = lpht) \
	 (HTREEITEM)SNDMSG((hwnd), TVM_HITTEST, 0, (LPARAM)(LPTV_HITTESTINFO)(lpht))

typedef struct tagTVHITTESTINFO {
	 POINT       pt;
	 UINT        flags;
	 HTREEITEM   hItem;
} TVHITTESTINFO, *LPTVHITTESTINFO;

TVHT_NOWHERE             = 0x0001
TVHT_ONITEMICON          = 0x0002
TVHT_ONITEMLABEL         = 0x0004
TVHT_ONITEM              = bit.bor(TVHT_ONITEMICON, TVHT_ONITEMLABEL, TVHT_ONITEMSTATEICON)
TVHT_ONITEMINDENT        = 0x0008
TVHT_ONITEMBUTTON        = 0x0010
TVHT_ONITEMRIGHT         = 0x0020
TVHT_ONITEMSTATEICON     = 0x0040
TVHT_ABOVE               = 0x0100
TVHT_BELOW               = 0x0200
TVHT_TORIGHT             = 0x0400
TVHT_TOLEFT              = 0x0800


TVM_CREATEDRAGIMAGE      = (TV_FIRST + 18)
TreeView_CreateDragImage(hwnd,  = hitem) \
	 (HIMAGELIST)SNDMSG((hwnd), TVM_CREATEDRAGIMAGE, 0, (LPARAM)(HTREEITEM)(hitem))


TVM_SORTCHILDREN         = (TV_FIRST + 19)
TreeView_SortChildren(hwnd,  = hitem, recurse) \
	 (BOOL)SNDMSG((hwnd), TVM_SORTCHILDREN, (WPARAM)(recurse), (LPARAM)(HTREEITEM)(hitem))


TVM_ENSUREVISIBLE        = (TV_FIRST + 20)
TreeView_EnsureVisible(hwnd,  = hitem) \
	 (BOOL)SNDMSG((hwnd), TVM_ENSUREVISIBLE, 0, (LPARAM)(HTREEITEM)(hitem))


TVM_SORTCHILDRENCB       = (TV_FIRST + 21)
TreeView_SortChildrenCB(hwnd,  = psort, recurse) \
	 (BOOL)SNDMSG((hwnd), TVM_SORTCHILDRENCB, (WPARAM)(recurse), \
	 (LPARAM)(LPTV_SORTCB)(psort))


TVM_ENDEDITLABELNOW      = (TV_FIRST + 22)
TreeView_EndEditLabelNow(hwnd,  = fCancel) \
	 (BOOL)SNDMSG((hwnd), TVM_ENDEDITLABELNOW, (WPARAM)(fCancel), 0)


TVM_GETISEARCHSTRINGW    = (TV_FIRST + 64)

TVM_SETTOOLTIPS          = (TV_FIRST + 24)
TreeView_SetToolTips(hwnd,   = hwndTT) \
	 (HWND)SNDMSG((hwnd), TVM_SETTOOLTIPS, (WPARAM)(hwndTT), 0)
TVM_GETTOOLTIPS          = (TV_FIRST + 25)
TreeView_GetToolTips(hwnd)  = \
	 (HWND)SNDMSG((hwnd), TVM_GETTOOLTIPS, 0, 0)


TreeView_GetISearchString(hwndTV,  = lpsz) \
		  (BOOL)SNDMSG((hwndTV), TVM_GETISEARCHSTRING, 0, (LPARAM)(LPTSTR)(lpsz))


TVM_SETINSERTMARK        = (TV_FIRST + 26)
TreeView_SetInsertMark(hwnd,  = hItem, fAfter) \
		  (BOOL)SNDMSG((hwnd), TVM_SETINSERTMARK, (WPARAM) (fAfter), (LPARAM) (hItem))

TVM_SETUNICODEFORMAT      = CCM_SETUNICODEFORMAT
TreeView_SetUnicodeFormat(hwnd,  = fUnicode)  \
	 (BOOL)SNDMSG((hwnd), TVM_SETUNICODEFORMAT, (WPARAM)(fUnicode), 0)

TVM_GETUNICODEFORMAT      = CCM_GETUNICODEFORMAT
TreeView_GetUnicodeFormat(hwnd)   = \
	 (BOOL)SNDMSG((hwnd), TVM_GETUNICODEFORMAT, 0, 0)


TVM_SETITEMHEIGHT          = (TV_FIRST + 27)
TreeView_SetItemHeight(hwnd,   = iHeight) \
	 (int)SNDMSG((hwnd), TVM_SETITEMHEIGHT, (WPARAM)(iHeight), 0)
TVM_GETITEMHEIGHT          = (TV_FIRST + 28)
TreeView_GetItemHeight(hwnd)  = \
	 (int)SNDMSG((hwnd), TVM_GETITEMHEIGHT, 0, 0)

TVM_SETBKCOLOR               = (TV_FIRST + 29)
TreeView_SetBkColor(hwnd,  = clr) \
	 (COLORREF)SNDMSG((hwnd), TVM_SETBKCOLOR, 0, (LPARAM)(clr))

TVM_SETTEXTCOLOR               = (TV_FIRST + 30)
TreeView_SetTextColor(hwnd,  = clr) \
	 (COLORREF)SNDMSG((hwnd), TVM_SETTEXTCOLOR, 0, (LPARAM)(clr))

TVM_GETBKCOLOR               = (TV_FIRST + 31)
TreeView_GetBkColor(hwnd)  = \
	 (COLORREF)SNDMSG((hwnd), TVM_GETBKCOLOR, 0, 0)

TVM_GETTEXTCOLOR               = (TV_FIRST + 32)
TreeView_GetTextColor(hwnd)  = \
	 (COLORREF)SNDMSG((hwnd), TVM_GETTEXTCOLOR, 0, 0)

TVM_SETSCROLLTIME               = (TV_FIRST + 33)
TreeView_SetScrollTime(hwnd,  = uTime) \
	 (UINT)SNDMSG((hwnd), TVM_SETSCROLLTIME, uTime, 0)

TVM_GETSCROLLTIME               = (TV_FIRST + 34)
TreeView_GetScrollTime(hwnd)  = \
	 (UINT)SNDMSG((hwnd), TVM_GETSCROLLTIME, 0, 0)


TVM_SETINSERTMARKCOLOR               = (TV_FIRST + 37)
TreeView_SetInsertMarkColor(hwnd,  = clr) \
	 (COLORREF)SNDMSG((hwnd), TVM_SETINSERTMARKCOLOR, 0, (LPARAM)(clr))
TVM_GETINSERTMARKCOLOR               = (TV_FIRST + 38)
TreeView_GetInsertMarkColor(hwnd)  = \
	 (COLORREF)SNDMSG((hwnd), TVM_GETINSERTMARKCOLOR, 0, 0)

TreeView_SetCheckState(hwndTV,  = hti, fCheck) \
  TreeView_SetItemState(hwndTV, hti, INDEXTOSTATEIMAGEMASK((fCheck)?2:1), TVIS_STATEIMAGEMASK)

TVM_GETITEMSTATE         = (TV_FIRST + 39)
TreeView_GetItemState(hwndTV,  = hti, mask) \
	(UINT)SNDMSG((hwndTV), TVM_GETITEMSTATE, (WPARAM)(hti), (LPARAM)(mask))

TreeView_GetCheckState(hwndTV,  = hti) \
	((((UINT)(SNDMSG((hwndTV), TVM_GETITEMSTATE, (WPARAM)(hti), TVIS_STATEIMAGEMASK))) >> 12) -1)


TVM_SETLINECOLOR             = (TV_FIRST + 40)
TreeView_SetLineColor(hwnd,  = clr) \
	 (COLORREF)SNDMSG((hwnd), TVM_SETLINECOLOR, 0, (LPARAM)(clr))

TVM_GETLINECOLOR             = (TV_FIRST + 41)
TreeView_GetLineColor(hwnd)  = \
	 (COLORREF)SNDMSG((hwnd), TVM_GETLINECOLOR, 0, 0)




TVM_MAPACCIDTOHTREEITEM      = (TV_FIRST + 42)
TreeView_MapAccIDToHTREEITEM(hwnd,  = id) \
	 (HTREEITEM)SNDMSG((hwnd), TVM_MAPACCIDTOHTREEITEM, id, 0)

TVM_MAPHTREEITEMTOACCID      = (TV_FIRST + 43)
TreeView_MapHTREEITEMToAccID(hwnd,  = htreeitem) \
	 (UINT)SNDMSG((hwnd), TVM_MAPHTREEITEMTOACCID, (WPARAM)(htreeitem), 0)

TVM_SETEXTENDEDSTYLE       = (TV_FIRST + 44)
TreeView_SetExtendedStyle(hwnd,  = dw, mask) \
	 (DWORD)SNDMSG((hwnd), TVM_SETEXTENDEDSTYLE, mask, dw)

TVM_GETEXTENDEDSTYLE       = (TV_FIRST + 45)
TreeView_GetExtendedStyle(hwnd)  = \
	 (DWORD)SNDMSG((hwnd), TVM_GETEXTENDEDSTYLE, 0, 0)


TVM_SETAUTOSCROLLINFO    = (TV_FIRST + 59)
TreeView_SetAutoScrollInfo(hwnd,  = uPixPerSec, uUpdateTime) \
	 SNDMSG((hwnd), TVM_SETAUTOSCROLLINFO, (WPARAM)(uPixPerSec), (LPARAM)(uUpdateTime))



TVM_GETSELECTEDCOUNT        = (TV_FIRST + 70)
TreeView_GetSelectedCount(hwnd)  = \
	 (DWORD)SNDMSG((hwnd), TVM_GETSELECTEDCOUNT, 0, 0)

TVM_SHOWINFOTIP             = (TV_FIRST + 71)
TreeView_ShowInfoTip(hwnd,  = hitem) \
	 (DWORD)SNDMSG((hwnd), TVM_SHOWINFOTIP, 0, (LPARAM)(hitem))

typedef enum _TVITEMPART
{
	 TVGIPR_BUTTON  = 0x0001,
} TVITEMPART;

typedef struct tagTVGETITEMPARTRECTINFO {
	 HTREEITEM hti;
	 RECT*     prc;
	 TVITEMPART partID;
} TVGETITEMPARTRECTINFO;

TVM_GETITEMPARTRECT          = (TV_FIRST + 72)
TreeView_GetItemPartRect(hwnd,  = hitem, prc, partid) \
{ TVGETITEMPARTRECTINFO info; \
  info.hti = (hitem); \
  info.prc = (prc); \
  info.partID = (partid); \
  (BOOL)SNDMSG((hwnd), TVM_GETITEMPARTRECT, 0, (LPARAM)&info); \
}

typedef int (CALLBACK *PFNTVCOMPARE)(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

typedef struct tagTVSORTCB
{
	 HTREEITEM       hParent;
	 PFNTVCOMPARE    lpfnCompare;
	 LPARAM          lParam;
} TVSORTCB, *LPTVSORTCB;

typedef struct tagNMTREEVIEWA {
	 NMHDR       hdr;
	 UINT        action;
	 TVITEMA    itemOld;
	 TVITEMA    itemNew;
	 POINT       ptDrag;
} NMTREEVIEWA, *LPNMTREEVIEWA;

typedef struct tagNMTREEVIEWW {
	 NMHDR       hdr;
	 UINT        action;
	 TVITEMW    itemOld;
	 TVITEMW    itemNew;
	 POINT       ptDrag;
} NMTREEVIEWW, *LPNMTREEVIEWW;

TVN_SELCHANGINGW         = (TVN_FIRST-50)
TVN_SELCHANGEDW          = (TVN_FIRST-51)
TVN_GETDISPINFOW         = (TVN_FIRST-52)
TVN_SETDISPINFOW         = (TVN_FIRST-53)

TVC_UNKNOWN              = 0x0000
TVC_BYMOUSE              = 0x0001
TVC_BYKEYBOARD           = 0x0002

TVIF_DI_SETITEM          = 0x1000

typedef struct tagTVDISPINFOW {
	 NMHDR hdr;
	 TVITEMW item;
} NMTVDISPINFOW, *LPNMTVDISPINFOW;

typedef struct tagTVDISPINFOEXW {
	 NMHDR hdr;
	 TVITEMEXW item;
} NMTVDISPINFOEXW, *LPNMTVDISPINFOEXW;

TVN_ITEMEXPANDINGW       = (TVN_FIRST-54)
TVN_ITEMEXPANDEDW        = (TVN_FIRST-55)
TVN_BEGINDRAGW           = (TVN_FIRST-56)
TVN_BEGINRDRAGW          = (TVN_FIRST-57)
TVN_DELETEITEMW          = (TVN_FIRST-58)
TVN_BEGINLABELEDITW      = (TVN_FIRST-59)
TVN_ENDLABELEDITW        = (TVN_FIRST-60)
TVN_KEYDOWN              = (TVN_FIRST-12)
TVN_GETINFOTIPW          = (TVN_FIRST-14)
TVN_SINGLEEXPAND         = (TVN_FIRST-15)

TVNRET_DEFAULT           = 0
TVNRET_SKIPOLD           = 1
TVNRET_SKIPNEW           = 2

TVN_ITEMCHANGINGW        = (TVN_FIRST-17)
TVN_ITEMCHANGEDW         = (TVN_FIRST-19)
TVN_ASYNCDRAW            = (TVN_FIRST-20)

--#include <pshpack1.h>

typedef struct tagTVKEYDOWN {
	 NMHDR hdr;
	 WORD wVKey;
	 UINT flags;
} NMTVKEYDOWN, *LPNMTVKEYDOWN;

--#include <poppack.h>

typedef struct tagNMTVCUSTOMDRAW
{
	 NMCUSTOMDRAW nmcd;
	 COLORREF     clrText;
	 COLORREF     clrTextBk;
	 int iLevel;
} NMTVCUSTOMDRAW, *LPNMTVCUSTOMDRAW;

-- for tooltips

typedef struct tagNMTVGETINFOTIPW
{
	 NMHDR hdr;
	 LPWSTR pszText;
	 int cchTextMax;
	 HTREEITEM hItem;
	 LPARAM lParam;
} NMTVGETINFOTIPW, *LPNMTVGETINFOTIPW;

-- treeview's customdraw return meaning don't draw images.  valid on CDRF_NOTIFYITEMPREPAINT
TVCDRF_NOIMAGES          = 0x00010000

typedef struct tagTVITEMCHANGE {
	 NMHDR hdr;
	 UINT uChanged;
	 HTREEITEM hItem;
	 UINT uStateNew;
	 UINT uStateOld;
	 LPARAM lParam;
} NMTVITEMCHANGE;

typedef struct tagNMTVASYNCDRAW
{
	 NMHDR     hdr;
	 IMAGELISTDRAWPARAMS *pimldp;    -- the draw that failed
	 HRESULT   hr;                   -- why it failed
	 HTREEITEM hItem;                -- item that failed to draw icon
	 LPARAM    lParam;               -- its data
	 -- Out Params
	 DWORD     dwRetFlags;           -- What listview should do on return
	 int       iRetImageIndex;       -- used if ADRF_DRAWIMAGE is returned
} NMTVASYNCDRAW;

]]

