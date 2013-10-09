--proc/menu: standard menu control.
setfenv(1, require'winapi')
require'winapi.winuser'

ffi.cdef[[
HMENU CreateMenu(void);
HMENU CreatePopupMenu(void);
BOOL DestroyMenu(HMENU hMenu);

HMENU GetMenu(HWND hWnd);
BOOL SetMenu(HWND hWnd, HMENU hMenu);
BOOL DrawMenuBar(HWND hWnd);

typedef struct tagMENUITEMINFOW
{
    UINT     cbSize;
    UINT     fMask;
    UINT     fType;
    UINT     fState;
    UINT     wID;
    HMENU    hSubMenu;
    HBITMAP  hbmpChecked;
    HBITMAP  hbmpUnchecked;
    ULONG_PTR dwItemData;
     LPWSTR   dwTypeData;
    UINT     cch;
    HBITMAP  hbmpItem;
}   MENUITEMINFOW,  *LPMENUITEMINFOW;

typedef MENUITEMINFOW const  *LPCMENUITEMINFOW;

BOOL InsertMenuItemW(
     HMENU hmenu,
     UINT item,
     BOOL fByPosition,
     LPCMENUITEMINFOW lpmi);

BOOL SetMenuItemInfoW(
     HMENU hmenu,
     UINT item,
     BOOL fByPositon,
     LPCMENUITEMINFOW lpmii);

BOOL GetMenuItemInfoW(
     HMENU hmenu,
     UINT item,
     BOOL fByPosition,
     LPMENUITEMINFOW lpmii);

BOOL RemoveMenu(
     HMENU hMenu,
     UINT uPosition,
     UINT uFlags
);

HMENU GetSubMenu(
     HMENU hMenu,
     int nPos
);

int GetMenuItemCount(HMENU hMenu);

typedef struct tagTPMPARAMS
{
    UINT    cbSize;
    RECT    exclude_rect;
}   TPMPARAMS;
typedef TPMPARAMS  *LPTPMPARAMS;

BOOL TrackPopupMenuEx(HMENU, UINT, int, int, HWND, LPTPMPARAMS);
]]

function CreateMenuBar()
	return own(checkh(C.CreateMenu()), DestroyMenu)
end

function CreateMenu()
	return own(checkh(C.CreatePopupMenu()), DestroyMenu)
end

function DestroyMenu(menu)
	checknz(C.DestroyMenu(menu))
	disown(menu)
end

function GetMenu(hwnd)
	return ptr(C.GetMenu(hwnd))
end

function SetMenu(hwnd, menu)
	local oldmenu = own(GetMenu(hwnd), DestroyMenu)
	checknz(C.SetMenu(hwnd, menu))
	disown(menu)
	return oldmenu
end

function DrawMenuBar(hwnd)
	checknz(C.DrawMenuBar(hwnd))
end

MF_INSERT            = 0x00000000
MF_CHANGE            = 0x00000080
MF_APPEND            = 0x00000100
MF_DELETE            = 0x00000200
MF_REMOVE            = 0x00001000
MF_BYCOMMAND         = 0x00000000
MF_BYPOSITION        = 0x00000400
MF_SEPARATOR         = 0x00000800
MF_ENABLED           = 0x00000000
MF_GRAYED            = 0x00000001
MF_DISABLED          = 0x00000002
MF_UNCHECKED         = 0x00000000
MF_CHECKED           = 0x00000008
MF_USECHECKBITMAPS   = 0x00000200
MF_STRING            = 0x00000000
MF_BITMAP            = 0x00000004
MF_OWNERDRAW         = 0x00000100
MF_POPUP             = 0x00000010
MF_MENUBARBREAK      = 0x00000020
MF_MENUBREAK         = 0x00000040
MF_UNHILITE          = 0x00000000
MF_HILITE            = 0x00000080
MF_DEFAULT           = 0x00001000
MF_SYSMENU           = 0x00002000
MF_HELP              = 0x00004000
MF_RIGHTJUSTIFY      = 0x00004000
MF_MOUSESELECT       = 0x00008000

MIIM_STATE       = 0x00000001
MIIM_ID          = 0x00000002
MIIM_SUBMENU     = 0x00000004
MIIM_CHECKMARKS  = 0x00000008
MIIM_TYPE        = 0x00000010
MIIM_DATA        = 0x00000020
MIIM_STRING      = 0x00000040
MIIM_BITMAP      = 0x00000080
MIIM_FTYPE       = 0x00000100

MFT_STRING           = MF_STRING
MFT_BITMAP           = MF_BITMAP
MFT_MENUBARBREAK     = MF_MENUBARBREAK
MFT_MENUBREAK        = MF_MENUBREAK
MFT_OWNERDRAW        = MF_OWNERDRAW
MFT_RADIOCHECK       = 0x00000200
MFT_SEPARATOR        = MF_SEPARATOR
MFT_RIGHTORDER       = 0x00002000
MFT_RIGHTJUSTIFY     = MF_RIGHTJUSTIFY

MFS_GRAYED           = 0x00000003
MFS_DISABLED         = MFS_GRAYED
MFS_CHECKED          = MF_CHECKED
MFS_HILITE           = MF_HILITE
MFS_ENABLED          = MF_ENABLED
MFS_UNCHECKED        = MF_UNCHECKED
MFS_UNHILITE         = MF_UNHILITE
MFS_DEFAULT          = MF_DEFAULT

HBMMENU_CALLBACK            = -1
HBMMENU_SYSTEM              = ffi.cast('HBITMAP', 1)
HBMMENU_MBAR_RESTORE        = ffi.cast('HBITMAP', 2)
HBMMENU_MBAR_MINIMIZE       = ffi.cast('HBITMAP', 3)
HBMMENU_MBAR_CLOSE          = ffi.cast('HBITMAP', 5)
HBMMENU_MBAR_CLOSE_D        = ffi.cast('HBITMAP', 6)
HBMMENU_MBAR_MINIMIZE_D     = ffi.cast('HBITMAP', 7)
HBMMENU_POPUP_CLOSE         = ffi.cast('HBITMAP', 8)
HBMMENU_POPUP_RESTORE       = ffi.cast('HBITMAP', 9)
HBMMENU_POPUP_MAXIMIZE      = ffi.cast('HBITMAP', 10)
HBMMENU_POPUP_MINIMIZE      = ffi.cast('HBITMAP', 11)

MENUITEMINFO = struct{
	ctype = 'MENUITEMINFOW', size = 'cbSize', mask = 'fMask',
	fields = mfields{
		'id',             'wID',               MIIM_ID, pass, pass,
		'text',           'dwTypeData',        MIIM_STRING, wcs, mbs,
		'submenu',        'hSubMenu',          MIIM_SUBMENU, ptr, ptr,
		'bitmap',         'hbmpItem',          MIIM_BITMAP, ptr, ptr,
		'checked_bitmap', 'hbmpChecked',       MIIM_CHECKMARKS, ptr, ptr,
		'unchecked_bitmap','hbmpUnchecked',    MIIM_CHECKMARKS, ptr, ptr,
		'type',           'fType',             MIIM_FTYPE, flags, pass,
		'state',          'fState',            MIIM_STATE, flags, pass,
	},
}

function GetSubMenu(menu, i)
	return ptr(C.GetSubMenu(menu, countfrom0(i)))
end

function InsertMenuItem(menu, i, info, byposition)
	if not info then i,info = nil,i end --i is optional
	checknz(C.InsertMenuItemW(menu, countfrom0(i), byposition, MENUITEMINFO(info)))
	disown(info.submenu)
end

function SetMenuItem(menu, i, info, byposition)
	local oldsubmenu = GetSubMenu(menu, i)
	checknz(C.SetMenuItemInfoW(menu, countfrom0(i), byposition, MENUITEMINFO(info)))
	disown(info.submenu)
	own(oldsubmenu, DestroyMenu)
end

function GetMenuItem(menu, i, byposition, info)
	info = MENUITEMINFO:setmask(info)
	checknz(C.GetMenuItemInfoW(menu, countfrom0(i), byposition, info))
	return info
end

MF_BYCOMMAND  = 0x00000000
MF_BYPOSITION = 0x00000400

function RemoveMenuItem(menu, i, byposition)
	local oldsubmenu = GetSubMenu(menu, i)
	checknz(C.RemoveMenu(menu, countfrom0(i), byposition and MF_BYPOSITION or MF_BYCOMMAND))
	return own(oldsubmenu, DestroyMenu)
end

function GetMenuItemCount(menu)
	return checkpoz(C.GetMenuItemCount(menu))
end

TPM_LEFTBUTTON      = 0x0000
TPM_RIGHTBUTTON     = 0x0002
TPM_LEFTALIGN       = 0x0000
TPM_CENTERALIGN     = 0x0004
TPM_RIGHTALIGN      = 0x0008
TPM_TOPALIGN        = 0x0000
TPM_VCENTERALIGN    = 0x0010
TPM_BOTTOMALIGN     = 0x0020
TPM_HORIZONTAL      = 0x0000 -- horz alignment matters more
TPM_VERTICAL        = 0x0040 -- vert alignment matters more
TPM_NONOTIFY        = 0x0080 -- don't send any notification msgs
TPM_RETURNCMD       = 0x0100
TPM_RECURSE         = 0x0001
TPM_HORPOSANIMATION = 0x0400
TPM_HORNEGANIMATION = 0x0800
TPM_VERPOSANIMATION = 0x1000
TPM_VERNEGANIMATION = 0x2000
TPM_NOANIMATION     = 0x4000
TPM_LAYOUTRTL       = 0x8000
TPM_WORKAREA        = 0x10000

TPMPARAMS = struct{
	ctype = 'TPMPARAMS', size = 'cbSize', fields = {}
}

function TrackPopupMenu(menu, hwnd, x, y, TPM, tpmp)
	if tpmp then tpmp = TPMPARAMS(tpmp) end
	return C.TrackPopupMenuEx(menu, flags(TPM), x, y, hwnd, tpmp)
end

--get/set menu info

ffi.cdef[[
typedef struct tagMENUINFO
{
    DWORD   cbSize;
    DWORD   fMask;
    DWORD   dwStyle;
    UINT    cyMax;
    HBRUSH  hbrBack;
    DWORD   dwContextHelpID;
    ULONG_PTR dwMenuData;
}   MENUINFO,  *LPMENUINFO;
typedef MENUINFO const  *LPCMENUINFO;

BOOL GetMenuInfo(HMENU, LPMENUINFO);
BOOL SetMenuInfo(HMENU, LPCMENUINFO);
]]

MNS_NOCHECK         = 0x80000000
MNS_MODELESS        = 0x40000000
MNS_DRAGDROP        = 0x20000000
MNS_AUTODISMISS     = 0x10000000
MNS_NOTIFYBYPOS     = 0x08000000
MNS_CHECKORBMP      = 0x04000000

MIM_MAXHEIGHT               = 0x00000001
MIM_BACKGROUND              = 0x00000002
MIM_HELPID                  = 0x00000004
MIM_MENUDATA                = 0x00000008
MIM_STYLE                   = 0x00000010
MIM_APPLYTOSUBMENUS         = 0x80000000

MENUINFO = struct{
	ctype = 'MENUINFO', size = 'cbSize', mask = 'fMask',
	fields = mfields{
		'max_height', 'cyMax', MIM_MAXHEIGHT, pass, pass,
		'background', 'hbrBack', MIM_BACKGROUND, pass, ptr,
		'help_id', 'dwContextHelpID', MIM_HELPID, pass, pass,
		'user_data', 'dwMenuData', MIM_MENUDATA, pass, pass,
		'style', 'dwStyle', MIM_STYLE, flags, pass, --MNS_*
		'apply_to_submenus', '', MIM_APPLYTOSUBMENUS, pass, pass,
	},
}

function GetMenuInfo(menu, info)
	info = MENUINFO:setmask(info)
	checknz(C.GetMenuInfo(menu, info))
	return info
end

function SetMenuInfo(menu, info)
	checknz(C.SetMenuInfo(menu, MENUINFO(info)))
end

