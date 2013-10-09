--proc/comboboxex: standard (new, from comctl32) combobox control.
setfenv(1, require'winapi')
require'winapi.combobox'
require'winapi.comctl'
require'winapi.itemlist'

InitCommonControlsEx(ICC_USEREX_CLASSES)

--creation

WC_COMBOBOXEX = 'ComboBoxEx32'

CBES_EX_NOEDITIMAGE           = 0x00000001
CBES_EX_NOEDITIMAGEINDENT     = 0x00000002
CBES_EX_PATHWORDBREAKPROC     = 0x00000004
CBES_EX_NOSIZELIMIT           = 0x00000008
CBES_EX_CASESENSITIVE         = 0x00000010
CBES_EX_TEXTENDELLIPSIS       = 0x00000020 --vista+

--commands

CBEM_SETIMAGELIST        = (WM_USER + 2)
CBEM_GETIMAGELIST        = (WM_USER + 3)
CBEM_DELETEITEM          = CB_DELETESTRING
CBEM_GETCOMBOCONTROL     = (WM_USER + 6)
CBEM_GETEDITCONTROL      = (WM_USER + 7)
CBEM_SETEXTENDEDSTYLE    = (WM_USER + 14) -- lparam == new style, wParam (optional) == mask
CBEM_GETEXTENDEDSTYLE    = (WM_USER + 9)
CBEM_SETUNICODEFORMAT    = CCM_SETUNICODEFORMAT
CBEM_GETUNICODEFORMAT    = CCM_GETUNICODEFORMAT
CBEM_HASEDITCHANGED      = (WM_USER + 10)
CBEM_INSERTITEMW         = (WM_USER + 11)
CBEM_SETITEMW            = (WM_USER + 12)
CBEM_GETITEMW            = (WM_USER + 13)

function ComboBoxEx_SetImageList(hwnd, iml)
	return ffi.cast('HIMAGELIST', SNDMSG(hwnd, CBEM_SETIMAGELIST, 0, iml))
end

function ComboBoxEx_GetImageList(hwnd)
	return ffi.cast('HIMAGELIST', SNDMSG(hwnd, CBEM_GETIMAGELIST))
end

ffi.cdef[[
typedef struct tagCOMBOBOXEXITEMW
{
	 UINT mask;
	 INT_PTR iItem;
	 LPWSTR pszText;
	 int cchTextMax;
	 int iImage;
	 int iSelectedImage;
	 int iOverlay;
	 int iIndent;
	 LPARAM lParam;
} COMBOBOXEXITEMW, *PCOMBOBOXEXITEMW;
typedef COMBOBOXEXITEMW const *PCCOMBOEXITEMW;
]]

CBEIF_TEXT              = 0x00000001
CBEIF_IMAGE             = 0x00000002
CBEIF_SELECTEDIMAGE     = 0x00000004
CBEIF_OVERLAY           = 0x00000008
CBEIF_INDENT            = 0x00000010
CBEIF_LPARAM            = 0x00000020
CBEIF_DI_SETITEM        = 0x10000000

COMBOBOXEXITEM = struct{
	ctype = 'COMBOBOXEXITEMW', mask = 'mask',
	fields = mfields{
		'i', 'iItem', 0, countfrom0, countfrom1,
		'text', 'pszText', CBEIF_TEXT, wcs, mbs,
		'image', 'iImage', CBEIF_IMAGE, countfrom0, countfrom1,
		'selected_image', 'iSelectedImage', CBEIF_SELECTEDIMAGE, countfrom0, countfrom1,
		'overlay_image', 'iOverlay', CBEIF_OVERLAY, countfrom0, countfrom1,
		'indent', 'iIndent', CBEIF_INDENT, pass, pass,
	}
}

function ComboBoxEx_InsertItem(hwnd, item) --returns index
	item = COMBOBOXEXITEM(item)
	return countfrom1(checkpoz(SNDMSG(hwnd, CBEM_INSERTITEMW, 0,
										ffi.cast('PCOMBOBOXEXITEMW', item)))), item
end

ComboBox_DeleteItem = ComboBox_DeleteString

function ComboBoxEx_GetItem(hwnd, item)
	item = COMBOBOXEXITEM:setmask(item)
	local ws, sz
	if not ptr(item.pszText) then --user didn't supply a buffer
		ws, sz = WCS()
		item.text = ws --we set text so that ws gets pinned to item
		item.cchTextMax = sz
	else
		ws = item.pszText
		sz = item.cchTextMax
	end
	checknz(SNDMSG(hwnd, CBEM_GETITEMW, 0, ffi.cast('PCOMBOBOXEXITEMW', item)))
	--winapi can relocate pszText; if so we need to copy the string and relocate pszText back.
	--note: we go through this trouble because otherwise we would make the valididy of the
	--struct's contents dependent on the lifetime of the combobox item.
	if item.pszText ~= ws then
		wcsncpy(ws, item.pszText, sz)
		item.pszText = ws
	end
	return item
end

function ComboBoxEx_SetItem(hwnd, item)
	item = COMBOBOXEXITEM(item)
	checknz(SNDMSG(hwnd, CBEM_SETITEMW, 0, ffi.cast('PCOMBOBOXEXITEMW', item)))
	return item
end


--notifications

--[[

typedef struct {
    NMHDR hdr;
    COMBOBOXEXITEMA ceItem;
} NMCOMBOBOXEXA, *PNMCOMBOBOXEXA;

typedef struct {
    NMHDR hdr;
    COMBOBOXEXITEMW ceItem;
} NMCOMBOBOXEXW, *PNMCOMBOBOXEXW;

#ifdef UNICODE
#define NMCOMBOBOXEX            NMCOMBOBOXEXW
#define PNMCOMBOBOXEX           PNMCOMBOBOXEXW
#define CBEN_GETDISPINFO        CBEN_GETDISPINFOW
#else
#define NMCOMBOBOXEX            NMCOMBOBOXEXA
#define PNMCOMBOBOXEX           PNMCOMBOBOXEXA
#define CBEN_GETDISPINFO        CBEN_GETDISPINFOA
#endif

#else
typedef struct {
    NMHDR hdr;
    COMBOBOXEXITEM ceItem;
} NMCOMBOBOXEX, *PNMCOMBOBOXEX;

#define CBEN_GETDISPINFO         (CBEN_FIRST - 0)

#endif      // _WIN32_IE >= 0x0400

#if (_WIN32_IE >= 0x0400)
#define CBEN_GETDISPINFOA        (CBEN_FIRST - 0)
#endif
#define CBEN_INSERTITEM          (CBEN_FIRST - 1)
#define CBEN_DELETEITEM          (CBEN_FIRST - 2)
#define CBEN_BEGINEDIT           (CBEN_FIRST - 4)
#define CBEN_ENDEDITA            (CBEN_FIRST - 5)
#define CBEN_ENDEDITW            (CBEN_FIRST - 6)

#if (_WIN32_IE >= 0x0400)
#define CBEN_GETDISPINFOW        (CBEN_FIRST - 7)
#endif

#if (_WIN32_IE >= 0x0400)
#define CBEN_DRAGBEGINA                  (CBEN_FIRST - 8)
#define CBEN_DRAGBEGINW                  (CBEN_FIRST - 9)

#ifdef UNICODE
#define CBEN_DRAGBEGIN CBEN_DRAGBEGINW
#else
#define CBEN_DRAGBEGIN CBEN_DRAGBEGINA
#endif

#endif  //(_WIN32_IE >= 0x0400)

// lParam specifies why the endedit is happening
#ifdef UNICODE
#define CBEN_ENDEDIT CBEN_ENDEDITW
#else
#define CBEN_ENDEDIT CBEN_ENDEDITA
#endif

#define CBENF_KILLFOCUS         1
#define CBENF_RETURN            2
#define CBENF_ESCAPE            3
#define CBENF_DROPDOWN          4

#define CBEMAXSTRLEN 260

#if (_WIN32_IE >= 0x0400)
// CBEN_DRAGBEGIN sends this information ...

typedef struct {
    NMHDR hdr;
    int   iItemid;
    WCHAR szText[CBEMAXSTRLEN];
}NMCBEDRAGBEGINW, *LPNMCBEDRAGBEGINW, *PNMCBEDRAGBEGINW;


typedef struct {
    NMHDR hdr;
    int   iItemid;
    char szText[CBEMAXSTRLEN];
}NMCBEDRAGBEGINA, *LPNMCBEDRAGBEGINA, *PNMCBEDRAGBEGINA;

#ifdef UNICODE
#define  NMCBEDRAGBEGIN NMCBEDRAGBEGINW
#define  LPNMCBEDRAGBEGIN LPNMCBEDRAGBEGINW
#define  PNMCBEDRAGBEGIN PNMCBEDRAGBEGINW
#else
#define  NMCBEDRAGBEGIN NMCBEDRAGBEGINA
#define  LPNMCBEDRAGBEGIN LPNMCBEDRAGBEGINA
#define  PNMCBEDRAGBEGIN PNMCBEDRAGBEGINA
#endif
#endif      // _WIN32_IE >= 0x0400

// CBEN_ENDEDIT sends this information...
// fChanged if the user actually did anything
// iNewSelection gives what would be the new selection unless the notify is failed
//                      iNewSelection may be CB_ERR if there's no match
typedef struct {
        NMHDR hdr;
        BOOL fChanged;
        int iNewSelection;
        WCHAR szText[CBEMAXSTRLEN];
        int iWhy;
} NMCBEENDEDITW, *LPNMCBEENDEDITW, *PNMCBEENDEDITW;

typedef struct {
        NMHDR hdr;
        BOOL fChanged;
        int iNewSelection;
        char szText[CBEMAXSTRLEN];
        int iWhy;
} NMCBEENDEDITA, *LPNMCBEENDEDITA,*PNMCBEENDEDITA;

#ifdef UNICODE
#define  NMCBEENDEDIT NMCBEENDEDITW
#define  LPNMCBEENDEDIT LPNMCBEENDEDITW
#define  PNMCBEENDEDIT PNMCBEENDEDITW
#else
#define  NMCBEENDEDIT NMCBEENDEDITA
#define  LPNMCBEENDEDIT LPNMCBEENDEDITA
#define  PNMCBEENDEDIT PNMCBEENDEDITA
#endif

#endif

#endif      // _WIN32_IE >= 0x0300
]]

