--proc/toolbar: standard toolbar control.
setfenv(1, require'winapi')
require'winapi.window'
require'winapi.comctl'

--creation

TOOLBARCLASSNAME       = 'ToolbarWindow32'

TBSTYLE_TOOLTIPS         = 0x0100
TBSTYLE_WRAPABLE         = 0x0200
TBSTYLE_ALTDRAG          = 0x0400
TBSTYLE_FLAT             = 0x0800
TBSTYLE_LIST             = 0x1000
TBSTYLE_CUSTOMERASE      = 0x2000
TBSTYLE_REGISTERDROP     = 0x4000
TBSTYLE_TRANSPARENT      = 0x8000

BTNS_BUTTON      = 0x0000
BTNS_SEP         = 0x0001
BTNS_CHECK       = 0x0002
BTNS_GROUP       = 0x0004
BTNS_DROPDOWN    = 0x0008
BTNS_AUTOSIZE    = 0x0010 --automatically calculate the cx of the button
BTNS_NOPREFIX    = 0x0020 --this button should not have accel prefix
BTNS_SHOWTEXT    = 0x0040 -- ignored unless TBSTYLE_EX_MIXEDBUTTONS is set
BTNS_WHOLEDROPDOWN = 0x0080 -- draw drop-down arrow, but without split arrow section

TBSTYLE_EX_MIXEDBUTTONS              = 0x00000008
TBSTYLE_EX_HIDECLIPPEDBUTTONS        = 0x00000010 -- don't show partially obscured buttons
TBSTYLE_EX_DOUBLEBUFFER              = 0x00000080 -- Double Buffer the toolbar
TBSTYLE_EX_DRAWDDARROWS              = 0x00000001

--commands

TB_SETIMAGELIST          = (WM_USER + 48)
TB_GETIMAGELIST          = (WM_USER + 49)
TB_LOADIMAGES            = (WM_USER + 50)
TB_BUTTONSTRUCTSIZE      = (WM_USER + 30)
TB_ADDBUTTONS            = (WM_USER + 68)
TB_AUTOSIZE              = (WM_USER + 33)
TB_INSERTBUTTONW         = (WM_USER + 67)
TB_DELETEBUTTON          = (WM_USER + 22)
TB_BUTTONCOUNT           = (WM_USER + 24)

function Toolbar_SetImageList(tb, iml)
	return ptr(ffi.cast('HIMAGELIST', SNDMSG(tb, TB_SETIMAGELIST, 0, iml)))
end

function Toolbar_GetImageList(tb)
	return ptr(ffi.cast('HIMAGELIST', SNDMSG(tb, TB_GETIMAGELIST)))
end

IDB_STD_SMALL_COLOR      = 0
IDB_STD_LARGE_COLOR      = 1
IDB_VIEW_SMALL_COLOR     = 4
IDB_VIEW_LARGE_COLOR     = 5
IDB_HIST_SMALL_COLOR     = 8
IDB_HIST_LARGE_COLOR     = 9
IDB_HIST_NORMAL          = 12
IDB_HIST_HOT             = 13
IDB_HIST_DISABLED        = 14
IDB_HIST_PRESSED         = 15

HINST_COMMCTRL           = -1

function Toolbar_LoadImages(tb, IDB)
	return checkpoz(SNDMSG(tb, TB_LOADIMAGES, IDB, ffi.cast('HINSTANCE', HINST_COMMCTRL)))
end

--index values for IDB_HIST_LARGE_COLOR and IDB_HIST_SMALL_COLOR
HIST_BACK                = 0
HIST_FORWARD             = 1
HIST_FAVORITES           = 2
HIST_ADDTOFAVORITES      = 3
HIST_VIEWTREE            = 4
--index values for IDB_STD_LARGE_COLOR and IDB_STD_SMALL_COLOR
STD_CUT                  = 0
STD_COPY                 = 1
STD_PASTE                = 2
STD_UNDO                 = 3
STD_REDOW                = 4
STD_DELETE               = 5
STD_FILENEW              = 6
STD_FILEOPEN             = 7
STD_FILESAVE             = 8
STD_PRINTPRE             = 9
STD_PROPERTIES           = 10
STD_HELP                 = 11
STD_FIND                 = 12
STD_REPLACE              = 13
STD_PRINT                = 14
--Index values for IDB_VIEW_LARGE_COLOR and IDB_VIEW_SMALL_COLOR
VIEW_LARGEICONS          = 0
VIEW_SMALLICONS          = 1
VIEW_LIST                = 2
VIEW_DETAILS             = 3
VIEW_SORTNAME            = 4
VIEW_SORTSIZE            = 5
VIEW_SORTDATE            = 6
VIEW_SORTTYPE            = 7
VIEW_PARENTFOLDER        = 8
VIEW_NETCONNECT          = 9
VIEW_NETDISCONNECT       = 10
VIEW_NEWFOLDER           = 11
VIEW_VIEWMENU            = 12

TBSTATE_CHECKED          = 0x01
TBSTATE_PRESSED          = 0x02
TBSTATE_ENABLED          = 0x04
TBSTATE_HIDDEN           = 0x08
TBSTATE_INDETERMINATE    = 0x10
TBSTATE_WRAP             = 0x20
TBSTATE_ELLIPSES         = 0x40
TBSTATE_MARKED           = 0x80

ffi.cdef([[
typedef struct _TBBUTTON {
    int iBitmap;
    int idCommand;
    BYTE fsState;
    BYTE fsStyle;
    BYTE bReserved[%d];
    DWORD_PTR dwData;
    INT_PTR iString;
} TBBUTTON, *PTBBUTTON, *LPTBBUTTON;
typedef const TBBUTTON *LPCTBBUTTON;
]] % (ffi.abi('64bit') and 6 or 2))

TBBUTTON = struct{
	ctype = 'TBBUTTON',
	fields = sfields{
		'i', 'iBitmap', countfrom0, countfrom1,
		'command', 'idCommand', pass, pass,
		'state', 'fsState', flags, pass, --TBSTATE_*
		'style', 'fsStyle', flags, pass, --TBSTYLE_*
	},
	defaults = {
		state = TBSTATE_ENABLED,
	}
}

function Toolbar_AddButton(tb, button) --TODO: support an array of buttons
	button = TBBUTTON(button)
	SNDMSG(tb, TB_BUTTONSTRUCTSIZE, ffi.sizeof(button))
	checknz(SNDMSG(tb, TB_ADDBUTTONS, 1, ffi.cast('TBBUTTON*', button)))
	SNDMSG(tb, TB_AUTOSIZE)
end

function Toolbar_InsertButton(tb, i, button)
	button = TBBUTTON(button)
	SNDMSG(tb, TB_BUTTONSTRUCTSIZE, ffi.sizeof(button))
	checktrue(SNDMSG(tb, TB_INSERTBUTTON, countfrom0(i), ffi.cast('BBUTTON*', button)))
end

function Tooldbar_DeleteButton(tb, i)
	return checktrue(SNDMSG(tb, countfrom0(i)))
end

function Toolbar_GetButtonCount(tb)
	return checkpoz(SNDMSG(tb, TB_BUTTONCOUNT))
end

TBIF_IMAGE               = 0x00000001
TBIF_TEXT                = 0x00000002
TBIF_STATE               = 0x00000004
TBIF_STYLE               = 0x00000008
TBIF_LPARAM              = 0x00000010
TBIF_COMMAND             = 0x00000020
TBIF_SIZE                = 0x00000040
TBIF_BYINDEX             = 0x80000000 -- wParam in Get/SetButtonInfo is an index, not id

ffi.cdef[[
typedef struct {
    UINT cbSize;
    DWORD dwMask;
    int idCommand;
    int iImage;
    BYTE fsState;
    BYTE fsStyle;
    WORD cx;
    DWORD_PTR lParam;
    LPWSTR pszText;
    int cchText;
} TBBUTTONINFOW, *LPTBBUTTONINFOW;
]]

TBBUTTONINFO = struct{
	ctype = 'TBBUTTONINFOW', size = 'cbSize', mask = 'dwMask',
	fields = mfields{
		'id', 'idCommand', TBIF_COMMAND, pass, pass,
		'i', 'iImage', TBIF_IMAGE, countfrom0, countfrom1,
		'text', 'pszText', TBIF_TEXT, wcs, mbs,
		'state', 'fsState', TBIF_STATE, flags, pass,
		'style','fsStyle', TBIF_STYLE, flags, pass,
		'w', 'cx', TBIF_SIZE, pass, pass,
		'user_data', 'lParam', TBIF_LPARAM, pass, pass,
		'by_index', '', TBIF_BYINDEX, pass, pass,
	},
}

-- BUTTONINFO APIs do NOT support the string pool.
TB_GETBUTTONINFO         = (WM_USER + 63)
TB_SETBUTTONINFO         = (WM_USER + 64)

function Toolbar_GetButtonInfo(tb, i, info)
	info = TBBUTTONINFO:setmask(info)
	if not ptr(item.pszText) then --user didn't supply a buffer
		local ws, sz = WCS()
		item.text = ws --ws gets pinned and the field mask gets set
		item.cchTextMax = sz
	end
	if bit.band(info.fsState, TBIF_BYINDEX) ~= 0 then i = countfrom0(i) end --default
	checkpoz(SNDMSG(tb, TB_GETBUTTONINFO, i, info))
	return info
end

function Toolbar_SetButtonInfo(tb, i, info)
	info = TBBUTTONINFO(info)
	if bit.band(info.fsState, TBIF_BYINDEX) ~= 0 then i = countfrom0(i) end
	checkpoz(SNDMSG(tb, TB_SETBUTTONINFO, i, info))
end


--[[

typedef struct tagCOLORSCHEME {
   DWORD            dwSize;
   COLORREF         clrBtnHighlight;
   COLORREF         clrBtnShadow;
} COLORSCHEME, *LPCOLORSCHEME;

typedef struct _COLORMAP {
    COLORREF from;
    COLORREF to;
} COLORMAP, *LPCOLORMAP;

WINCOMMCTRLAPI HBITMAP WINAPI CreateMappedBitmap(HINSTANCE hInstance, INT_PTR idBitmap,
                                  UINT wFlags, __in_opt LPCOLORMAP lpColorMap,
                                  int iNumMaps);

CMB_MASKED               = 0x02

TB_ENABLEBUTTON          = (WM_USER + 1)
TB_CHECKBUTTON           = (WM_USER + 2)
TB_PRESSBUTTON           = (WM_USER + 3)
TB_HIDEBUTTON            = (WM_USER + 4)
TB_INDETERMINATE         = (WM_USER + 5)
TB_MARKBUTTON            = (WM_USER + 6)
TB_ISBUTTONENABLED       = (WM_USER + 9)
TB_ISBUTTONCHECKED       = (WM_USER + 10)
TB_ISBUTTONPRESSED       = (WM_USER + 11)
TB_ISBUTTONHIDDEN        = (WM_USER + 12)
TB_ISBUTTONINDETERMINATE = (WM_USER + 13)
TB_ISBUTTONHIGHLIGHTED   = (WM_USER + 14)
TB_SETSTATE              = (WM_USER + 17)
TB_GETSTATE              = (WM_USER + 18)
TB_ADDBITMAP             = (WM_USER + 19)
TB_GETBUTTON             = (WM_USER + 23)
TB_COMMANDTOINDEX        = (WM_USER + 25)
TB_SAVERESTOREW          = (WM_USER + 76)
TB_CUSTOMIZE             = (WM_USER + 27)
TB_ADDSTRINGW            = (WM_USER + 77)
TB_GETITEMRECT           = (WM_USER + 29)
TB_SETBUTTONSIZE         = (WM_USER + 31)
TB_SETBITMAPSIZE         = (WM_USER + 32)
TB_GETTOOLTIPS           = (WM_USER + 35)
TB_SETTOOLTIPS           = (WM_USER + 36)
TB_SETPARENT             = (WM_USER + 37)
TB_SETROWS               = (WM_USER + 39)
TB_GETROWS               = (WM_USER + 40)
TB_SETCMDID              = (WM_USER + 42)
TB_CHANGEBITMAP          = (WM_USER + 43)
TB_GETBITMAP             = (WM_USER + 44)
TB_REPLACEBITMAP         = (WM_USER + 46)
TB_SETINDENT             = (WM_USER + 47)
TB_GETRECT               = (WM_USER + 51) -- wParam is the Cmd instead of index
TB_SETHOTIMAGELIST       = (WM_USER + 52)
TB_GETHOTIMAGELIST       = (WM_USER + 53)
TB_SETDISABLEDIMAGELIST  = (WM_USER + 54)
TB_GETDISABLEDIMAGELIST  = (WM_USER + 55)
TB_SETSTYLE              = (WM_USER + 56)
TB_GETSTYLE              = (WM_USER + 57)
TB_GETBUTTONSIZE         = (WM_USER + 58)
TB_SETBUTTONWIDTH        = (WM_USER + 59)
TB_SETMAXTEXTROWS        = (WM_USER + 60)
TB_GETTEXTROWS           = (WM_USER + 61)
TB_GETOBJECT             = (WM_USER + 62)  -- wParam == IID, lParam void **ppv
TB_GETHOTITEM            = (WM_USER + 71)
TB_SETHOTITEM            = (WM_USER + 72)  -- wParam == iHotItem
TB_SETANCHORHIGHLIGHT    = (WM_USER + 73)  -- wParam == TRUE/FALSE
TB_GETANCHORHIGHLIGHT    = (WM_USER + 74)
TB_GETBUTTONTEXTW        = (WM_USER + 75)
TB_GETINSERTMARK         = (WM_USER + 79)  -- lParam == LPTBINSERTMARK
TB_SETINSERTMARK         = (WM_USER + 80)  -- lParam == LPTBINSERTMARK
TB_INSERTMARKHITTEST     = (WM_USER + 81)  -- wParam == LPPOINT lParam == LPTBINSERTMARK
TB_MOVEBUTTON            = (WM_USER + 82)
TB_GETMAXSIZE            = (WM_USER + 83)  -- lParam == LPSIZE
TB_SETEXTENDEDSTYLE      = (WM_USER + 84)  -- For TBSTYLE_EX_*
TB_GETEXTENDEDSTYLE      = (WM_USER + 85)  -- For TBSTYLE_EX_*
TB_GETPADDING            = (WM_USER + 86)
TB_SETPADDING            = (WM_USER + 87)
TB_SETINSERTMARKCOLOR    = (WM_USER + 88)
TB_GETINSERTMARKCOLOR    = (WM_USER + 89)
TB_SETCOLORSCHEME        = CCM_SETCOLORSCHEME  -- lParam is color scheme
TB_GETCOLORSCHEME        = CCM_GETCOLORSCHEME      -- fills in COLORSCHEME pointed to by lParam
TB_SETUNICODEFORMAT      = CCM_SETUNICODEFORMAT
TB_GETUNICODEFORMAT      = CCM_GETUNICODEFORMAT
TB_MAPACCELERATORW       = (WM_USER + 90)  -- wParam == ch, lParam int * pidBtn

-- Custom Draw Structure
typedef struct _NMTBCUSTOMDRAW {
    NMCUSTOMDRAW nmcd;
    HBRUSH hbrMonoDither;
    HBRUSH hbrLines;                -- For drawing lines on buttons
    HPEN hpenLines;                 -- For drawing lines on buttons
    COLORREF clrText;               -- Color of text
    COLORREF clrMark;               -- Color of text bk when marked. (only if TBSTATE_MARKED)
    COLORREF clrTextHighlight;      -- Color of text when highlighted
    COLORREF clrBtnFace;            -- Background of the button
    COLORREF clrBtnHighlight;       -- 3D highlight
    COLORREF clrHighlightHotTrack;  -- In conjunction with fHighlightHotTrack will cause button to highlight like a menu
    RECT rcText;                    -- Rect for text
    int nStringBkMode;
    int nHLStringBkMode;
    int iListGap;
} NMTBCUSTOMDRAW, * LPNMTBCUSTOMDRAW;

-- Toolbar custom draw return flags
TBCDRF_NOEDGES               = 0x00010000  -- Don't draw button edges
TBCDRF_HILITEHOTTRACK        = 0x00020000  -- Use color of the button bk when hottracked
TBCDRF_NOOFFSET              = 0x00040000  -- Don't offset button if pressed
TBCDRF_NOMARK                = 0x00080000  -- Don't draw default highlight of image/text for TBSTATE_MARKED
TBCDRF_NOETCHEDEFFECT        = 0x00100000  -- Don't draw etched effect for disabled items
TBCDRF_BLENDICON             = 0x00200000  -- Use ILD_BLEND50 on the icon image
TBCDRF_NOBACKGROUND          = 0x00400000  -- Use ILD_BLEND50 on the icon image
TBCDRF_USECDCOLORS           = 0x00800000  -- Use CustomDrawColors to RenderText regardless of VisualStyle

typedef struct tagTBADDBITMAP {
        HINSTANCE       hInst;
        UINT_PTR        nID;
} TBADDBITMAP, *LPTBADDBITMAP;

typedef struct tagTBSAVEPARAMSW {
    HKEY hkr;
    LPCWSTR pszSubKey;
    LPCWSTR pszValueName;
} TBSAVEPARAMSW, *LPTBSAVEPARAMW;

typedef struct {
    int   iButton;
    DWORD dwFlags;
} TBINSERTMARK, * LPTBINSERTMARK;
TBIMHT_AFTER       = 0x00000001 -- TRUE = insert After iButton, otherwise before
TBIMHT_BACKGROUND  = 0x00000002 -- TRUE iff missed buttons completely

typedef struct {
    HINSTANCE       hInstOld;
    UINT_PTR        nIDOld;
    HINSTANCE       hInstNew;
    UINT_PTR        nIDNew;
    int             nButtons;
} TBREPLACEBITMAP, *LPTBREPLACEBITMAP;

TBBF_LARGE               = 0x0001

TB_GETBITMAPFLAGS        = (WM_USER + 41)

TB_HITTEST               = (WM_USER + 69)

-- New post Win95/NT4 for InsertButton and AddButton.  if iString member
-- is a pointer to a string, it will be handled as a string like listview
-- (although LPSTR_TEXTCALLBACK is not supported).

TB_SETDRAWTEXTFLAGS      = (WM_USER + 70)  -- wParam == mask lParam == bit values
TB_GETSTRINGW            = (WM_USER + 91)
TB_SETHOTITEM2           = (WM_USER + 94)  -- wParam == iHotItem,  lParam = dwFlags
TB_SETLISTGAP            = (WM_USER + 96)
TB_GETIMAGELISTCOUNT     = (WM_USER + 98)
TB_GETIDEALSIZE          = (WM_USER + 99)  -- wParam == fHeight, lParam = psize
-- before using WM_USER + 103, recycle old space above (WM_USER + 97)
TB_TRANSLATEACCELERATOR      = CCM_TRANSLATEACCELERATOR

TBMF_PAD                 = 0x00000001
TBMF_BARPAD              = 0x00000002
TBMF_BUTTONSPACING       = 0x00000004

typedef struct {
    UINT cbSize;
    DWORD dwMask;

    int cxPad;        -- PAD
    int cyPad;
    int cxBarPad;     -- BARPAD
    int cyBarPad;
    int cxButtonSpacing;   -- BUTTONSPACING
    int cyButtonSpacing;
} TBMETRICS, * LPTBMETRICS;

TB_GETMETRICS            = (WM_USER + 101)
TB_SETMETRICS            = (WM_USER + 102)
TB_GETITEMDROPDOWNRECT   = (WM_USER + 103)  -- Rect of item's drop down button
TB_SETPRESSEDIMAGELIST   = (WM_USER + 104)
TB_GETPRESSEDIMAGELIST   = (WM_USER + 105)
TB_SETWINDOWTHEME        = CCM_SETWINDOWTHEME

TBN_FIRST               = ffi.cast('UINT', -700)
TBN_GETBUTTONINFOA       = (TBN_FIRST-0)
TBN_BEGINDRAG            = (TBN_FIRST-1)
TBN_ENDDRAG              = (TBN_FIRST-2)
TBN_BEGINADJUST          = (TBN_FIRST-3)
TBN_ENDADJUST            = (TBN_FIRST-4)
TBN_RESET                = (TBN_FIRST-5)
TBN_QUERYINSERT          = (TBN_FIRST-6)
TBN_QUERYDELETE          = (TBN_FIRST-7)
TBN_TOOLBARCHANGE        = (TBN_FIRST-8)
TBN_CUSTHELP             = (TBN_FIRST-9)
TBN_DROPDOWN             = (TBN_FIRST - 10)
TBN_GETOBJECT            = (TBN_FIRST - 12)

-- Structure for TBN_HOTITEMCHANGE notification
typedef struct tagNMTBHOTITEM
{
    NMHDR   hdr;
    int     idOld;
    int     idNew;
    DWORD   dwFlags;           -- HICF_*
} NMTBHOTITEM, * LPNMTBHOTITEM;

-- Hot item change flags
HICF_OTHER           = 0x00000000
HICF_MOUSE           = 0x00000001          -- Triggered by mouse
HICF_ARROWKEYS       = 0x00000002          -- Triggered by arrow keys
HICF_ACCELERATOR     = 0x00000004          -- Triggered by accelerator
HICF_DUPACCEL        = 0x00000008          -- This accelerator is not unique
HICF_ENTERING        = 0x00000010          -- idOld is invalid
HICF_LEAVING         = 0x00000020          -- idNew is invalid
HICF_RESELECT        = 0x00000040          -- hot item reselected
HICF_LMOUSE          = 0x00000080          -- left mouse button selected
HICF_TOGGLEDROPDOWN  = 0x00000100          -- Toggle button's dropdown state

TBN_HOTITEMCHANGE        = (TBN_FIRST - 13)
TBN_DRAGOUT              = (TBN_FIRST - 14) -- this is sent when the user clicks down on a button then drags off the button
TBN_DELETINGBUTTON       = (TBN_FIRST - 15) -- uses TBNOTIFY
TBN_GETDISPINFOA         = (TBN_FIRST - 16) -- This is sent when the  toolbar needs  some display information
TBN_GETDISPINFOW         = (TBN_FIRST - 17) -- This is sent when the  toolbar needs  some display information
TBN_GETINFOTIPA          = (TBN_FIRST - 18)
TBN_GETINFOTIPW          = (TBN_FIRST - 19)
TBN_GETBUTTONINFOW       = (TBN_FIRST - 20)
TBN_RESTORE              = (TBN_FIRST - 21)
TBN_SAVE                 = (TBN_FIRST - 22)
TBN_INITCUSTOMIZE        = (TBN_FIRST - 23)
TBNRF_HIDEHELP           = 0x00000001
TBNRF_ENDCUSTOMIZE       = 0x00000002
TBN_WRAPHOTITEM          = (TBN_FIRST - 24)
TBN_DUPACCELERATOR       = (TBN_FIRST - 25)
TBN_WRAPACCELERATOR      = (TBN_FIRST - 26)
TBN_DRAGOVER             = (TBN_FIRST - 27)
TBN_MAPACCELERATOR       = (TBN_FIRST - 28)

typedef struct tagNMTBSAVE
{
    NMHDR hdr;
    DWORD* pData;
    DWORD* pCurrent;
    UINT cbData;
    int iItem;
    int cButtons;
    TBBUTTON tbButton;
} NMTBSAVE, *LPNMTBSAVE;

typedef struct tagNMTBRESTORE
{
    NMHDR hdr;
    DWORD* pData;
    DWORD* pCurrent;
    UINT cbData;
    int iItem;
    int cButtons;
    int cbBytesPerRecord;
    TBBUTTON tbButton;
} NMTBRESTORE, *LPNMTBRESTORE;

typedef struct tagNMTBGETINFOTIPW
{
    NMHDR hdr;
    LPWSTR pszText;
    int cchTextMax;
    int iItem;
    LPARAM lParam;
} NMTBGETINFOTIPW, *LPNMTBGETINFOTIPW;

TBNF_IMAGE               = 0x00000001
TBNF_TEXT                = 0x00000002
TBNF_DI_SETITEM          = 0x10000000

typedef struct {
    NMHDR hdr;
    DWORD dwMask;      --[in] Specifies the values requested .[out] Client ask the data to be set for future use
    int idCommand;    -- [in] id of button we're requesting info for
    DWORD_PTR lParam;  -- [in] lParam of button
    int iImage;       -- [out] image index
    LPWSTR pszText;   -- [out] new text for item
    int cchText;      -- [in] size of buffer pointed to by pszText
} NMTBDISPINFOW, *LPNMTBDISPINFOW;

-- Return codes for TBN_DROPDOWN
TBDDRET_DEFAULT          = 0
TBDDRET_NODEFAULT        = 1
TBDDRET_TREATPRESSED     = 2       -- Treat as a standard press button

TBNOTIFY        = NMTOOLBAR
LPTBNOTIFY      = LPNMTOOLBAR

typedef struct tagNMTOOLBARW {
    NMHDR   hdr;
    int     iItem;
    TBBUTTON tbButton;
    int     cchText;
    LPWSTR   pszText;

    RECT    rcButton;

} NMTOOLBARW, *LPNMTOOLBARW;

]]
