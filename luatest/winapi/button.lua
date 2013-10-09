--proc/button: button and button-like controls.
setfenv(1, require'winapi')
require'winapi.window'
require'winapi.comctl'

--creation

WC_BUTTON            = 'Button'

BS_PUSHBUTTON        = 0x00000000
BS_DEFPUSHBUTTON     = 0x00000001
BS_CHECKBOX          = 0x00000002
BS_AUTOCHECKBOX      = 0x00000003
BS_RADIOBUTTON       = 0x00000004
BS_3STATE            = 0x00000005
BS_AUTO3STATE        = 0x00000006
BS_GROUPBOX          = 0x00000007
BS_AUTORADIOBUTTON   = 0x00000009
BS_PUSHBOX           = 0x0000000A --does not display a button face or frame; only the text appears
BS_OWNERDRAW         = 0x0000000B
BS_LEFTTEXT          = 0x00000020
BS_TEXT              = 0x00000000
BS_ICON              = 0x00000040
BS_BITMAP            = 0x00000080
BS_LEFT              = 0x00000100
BS_RIGHT             = 0x00000200
BS_CENTER            = 0x00000300
BS_TOP               = 0x00000400
BS_BOTTOM            = 0x00000800
BS_VCENTER           = 0x00000C00
BS_PUSHLIKE          = 0x00001000
BS_MULTILINE         = 0x00002000
BS_NOTIFY            = 0x00004000
BS_FLAT              = 0x00008000
BS_SPLITBUTTON       = 0x0000000C --vista+
BS_DEFSPLITBUTTON    = 0x0000000D --vista+
BS_COMMANDLINK       = 0x0000000E --vista+
BS_DEFCOMMANDLINK    = 0x0000000F --vista+

--commands

BM_GETCHECK         = 0x00F0
BM_SETCHECK         = 0x00F1
BM_GETSTATE         = 0x00F2
BM_SETSTATE         = 0x00F3
BM_SETSTYLE         = 0x00F4
BM_CLICK            = 0x00F5
BM_GETIMAGE         = 0x00F6
BM_SETIMAGE         = 0x00F7
BM_SETDONTCLICK     = 0x00F8 --vista+

Button_Enable = EnableWindow
Button_GetText = GetWindowText
Button_SetText = SetWindowText

BST_UNCHECKED       = 0x0000
BST_CHECKED         = 0x0001
BST_INDETERMINATE   = 0x0002
BST_PUSHED          = 0x0004
BST_FOCUS           = 0x0008

function Button_GetCheck(hwndCtl)
	return SNDMSG(hwndCtl, BM_GETCHECK)
end

function Button_SetCheck(hwndCtl, check)
	return SNDMSG(hwndCtl, BM_SETCHECK, flags(check))
end

function Button_GetState(hwndCtl) --pushed state
	return SNDMSG(hwndCtl, BM_GETSTATE) ~= 0
end

function Button_SetState(hwndCtl, state) --pushed state
	checkz(SNDMSG(hwndCtl, BM_SETSTATE, state))
end

function Button_SetStyle(hwndCtl, style, fRedraw)
	checknz(SNDMSG(hwndCtl, BM_SETSTYLE, flags(style), MAKELPARAM(fRedraw and 1 or 0, 0)))
end

function Button_Click(hwndCtl)
	SNDMSG(hwndCtl, BM_CLICK)
end

function Button_GetBitmap(hwndCtl)
	return ptr(ffi.cast('HBITMAP', SNDMSG(hwndCtl, BM_GETIMAGE, IMAGE_BITMAP)))
end

function Button_GetIcon(hwndCtl)
	return ptr(ffi.cast('HICON', SNDMSG(hwndCtl, BM_GETIMAGE, IMAGE_ICON)))
end

function Button_SetBitmap(hwndCtl, bitmap) --must set BS_BITMAP
	return ptr(ffi.cast('HBITMAP', SNDMSG(hwndCtl, BM_SETIMAGE, IMAGE_BITMAP, bitmap)))
end

function Button_SetIcon(hwndCtl, icon) --must set BS_ICON
	return ptr(ffi.cast('HICON', SNDMSG(hwndCtl, BM_SETIMAGE, IMAGE_ICON, icon)))
end

function Button_SetDontClick(hwndCtl, dontclick)
	SNDMSG(hwndCtl, BM_SETDONTCLICK, dontclick)
end

BCM_FIRST               = 0x1600
BCM_GETIDEALSIZE        = (BCM_FIRST + 0x0001)
BCM_SETIMAGELIST        = (BCM_FIRST + 0x0002)
BCM_GETIMAGELIST        = (BCM_FIRST + 0x0003)
BCM_SETTEXTMARGIN       = (BCM_FIRST + 0x0004)
BCM_GETTEXTMARGIN       = (BCM_FIRST + 0x0005)
BCM_SETDROPDOWNSTATE    = (BCM_FIRST + 0x0006)
BCM_SETSPLITINFO        = (BCM_FIRST + 0x0007)
BCM_GETSPLITINFO        = (BCM_FIRST + 0x0008)
BCM_SETNOTE             = (BCM_FIRST + 0x0009)
BCM_GETNOTE             = (BCM_FIRST + 0x000A)
BCM_GETNOTELENGTH       = (BCM_FIRST + 0x000B)
BCM_SETSHIELD           = (BCM_FIRST + 0x000C)

function Button_GetIdealSize(hwnd, size)
	size = SIZE(size)
	checknz(SNDMSG(hwnd, BCM_GETIDEALSIZE, 0, ffi.cast('PSIZE', size)))
	return size
end

BUTTON_IMAGELIST_ALIGN_LEFT     = 0
BUTTON_IMAGELIST_ALIGN_RIGHT    = 1
BUTTON_IMAGELIST_ALIGN_TOP      = 2
BUTTON_IMAGELIST_ALIGN_BOTTOM   = 3
BUTTON_IMAGELIST_ALIGN_CENTER   = 4

ffi.cdef[[
enum PUSHBUTTONSTATES {
	PBS_NORMAL = 1,
	PBS_HOT = 2,
	PBS_PRESSED = 3,
	PBS_DISABLED = 4,
	PBS_DEFAULTED = 5,
	PBS_DEFAULTED_ANIMATING = 6,
};

typedef struct
{
	 HIMAGELIST  imagelist;
	 RECT        margin;
	 UINT        uAlign;
} BUTTON_IMAGELIST, *PBUTTON_IMAGELIST;
]]

BUTTON_IMAGELIST = struct{
	ctype = 'BUTTON_IMAGELIST',
	fields = sfields{
		'align', 'uAlign', flags,
	}
}

BCCL_NOGLYPH = ffi.cast('HIMAGELIST', -1) --flag to indicate no glyph to SetImageList

function Button_SetImageList(hwnd, bimlist)
	bimlist = BUTTON_IMAGELIST(bimlist)
	checknz(SNDMSG(hwnd, BCM_SETIMAGELIST, 0, ffi.cast('PBUTTON_IMAGELIST', bimlist)))
end

function Button_GetImageList(hwnd, bimlist)
	bimlist = BUTTON_IMAGELIST(bimlist)
	checknz(SNDMSG(hwnd, BCM_GETIMAGELIST, 0, ffi.cast('PBUTTON_IMAGELIST', bimlist)))
	return bimlist
end

function Button_SetTextMargin(hwnd, margin)
	margin = RECT(margin)
	checknz(SNDMSG(hwnd, BCM_SETTEXTMARGIN, 0, ffi.cast('RECT*', margin)))
end

function Button_GetTextMargin(hwnd, margin)
	margin = RECT(margin)
	checknz(SNDMSG(hwnd, BCM_GETTEXTMARGIN, 0, ffi.cast('RECT*', margin)))
	return margin
end

function Button_SetDropDownState(hwnd, fDropDown)
	checknz(SNDMSG(hwnd, BCM_SETDROPDOWNSTATE, fDropDown))
end

function Button_SetSplitInfo(hwnd, pInfo)
	checknz(SNDMSG(hwnd, BCM_SETSPLITINFO, 0, pInfo))
end

function Button_GetSplitInfo(hwnd, pInfo)
	checknz(SNDMSG(hwnd, BCM_GETSPLITINFO, 0, pInfo))
end

function Button_SetNote(hwnd, psz)
	checknz(SNDMSG(hwnd, BCM_SETNOTE, 0, ffi.cast('LPCWSTR', wcs(psz))))
end

function Button_GetNote(hwnd, buf)
	local ws, sz = WCS(buf or checkpoz(SNDMSG(hwnd, BCM_GETNOTELENGTH)))
	checknz(SNDMSG(hwnd, BCM_GETNOTE, ws, ffi.cast('LPCWSTR', sz)))
	return buf or mbs(ws)
end

-- Macro to use on a button or command link to display an elevated icon
function Button_SetElevationRequiredState(hwnd, fRequired)
	 return checkpoz(SNDMSG(hwnd, BCM_SETSHIELD, 0, fRequired))
end

--notifications

BN_CLICKED           = 0
BN_DOUBLECLICKED     = 5 --only if BS_NOTIFY
BN_SETFOCUS          = 6 --only if BS_NOTIFY damn it
BN_KILLFOCUS         = 7 --only if BS_NOTIFY damn it

ffi.cdef[[
typedef struct tagNMBCDROPDOWN
{
	 NMHDR   hdr;
	 RECT    rcButton;
} NMBCDROPDOWN, *LPNMBCDROPDOWN;
]]

BCN_FIRST            = ffi.cast('UINT', -1250)
BCN_HOTITEMCHANGE    = (BCN_FIRST + 0x0001)
BCN_DROPDOWN         = (BCN_FIRST + 0x0002)


--[[

-- SPLIT BUTTON INFO mask flags
BCSIF_GLYPH             = 0x0001
BCSIF_IMAGE             = 0x0002
BCSIF_STYLE             = 0x0004
BCSIF_SIZE              = 0x0008

-- SPLIT BUTTON STYLE flags
BCSS_NOSPLIT            = 0x0001
BCSS_STRETCH            = 0x0002
BCSS_ALIGNLEFT          = 0x0004
BCSS_IMAGE              = 0x0008


-- Hot item change flags
#define HICF_OTHER          0x00000000
#define HICF_MOUSE          0x00000001          -- Triggered by mouse
#define HICF_ARROWKEYS      0x00000002          -- Triggered by arrow keys
#define HICF_ACCELERATOR    0x00000004          -- Triggered by accelerator
#define HICF_DUPACCEL       0x00000008          -- This accelerator is not unique
#define HICF_ENTERING       0x00000010          -- idOld is invalid
#define HICF_LEAVING        0x00000020          -- idNew is invalid
#define HICF_RESELECT       0x00000040          -- hot item reselected
#define HICF_LMOUSE         0x00000080          -- left mouse button selected
#define HICF_TOGGLEDROPDOWN 0x00000100          -- Toggle button's dropdown state

typedef struct tagNMBCHOTITEM
{
	 NMHDR   hdr;
	 DWORD   dwFlags;           -- HICF_*
} NMBCHOTITEM, *LPNMBCHOTITEM;


#define BST_HOT            0x0200

-- BUTTON STATE FLAGS
#define BST_DROPDOWNPUSHED      0x0400

-- BUTTON STRUCTURES
typedef struct tagBUTTON_SPLITINFO
{
	 UINT        mask;
	 HIMAGELIST  himlGlyph;         -- interpreted as WCHAR if BCSIF_GLYPH is set
	 UINT        uSplitStyle;
	 SIZE        size;
} BUTTON_SPLITINFO, *PBUTTON_SPLITINFO;


]]
