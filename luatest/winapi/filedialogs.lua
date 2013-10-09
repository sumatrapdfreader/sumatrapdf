--proc/comdlg/filedialogs: standard open and save file dialogs.
setfenv(1, require'winapi')
require'winapi.comdlg'

OFN_READONLY                 = 0x00000001
OFN_OVERWRITEPROMPT          = 0x00000002
OFN_HIDEREADONLY             = 0x00000004
OFN_NOCHANGEDIR              = 0x00000008
OFN_SHOWHELP                 = 0x00000010
OFN_ENABLEHOOK               = 0x00000020
OFN_ENABLETEMPLATE           = 0x00000040
OFN_ENABLETEMPLATEHANDLE     = 0x00000080
OFN_NOVALIDATE               = 0x00000100
OFN_ALLOWMULTISELECT         = 0x00000200
OFN_EXTENSIONDIFFERENT       = 0x00000400
OFN_PATHMUSTEXIST            = 0x00000800
OFN_FILEMUSTEXIST            = 0x00001000
OFN_CREATEPROMPT             = 0x00002000
OFN_SHAREAWARE               = 0x00004000
OFN_NOREADONLYRETURN         = 0x00008000
OFN_NOTESTFILECREATE         = 0x00010000
OFN_NONETWORKBUTTON          = 0x00020000
OFN_NOLONGNAMES              = 0x00040000     -- force no long names for 4.x modules
OFN_EXPLORER                 = 0x00080000     -- new look commdlg
OFN_NODEREFERENCELINKS       = 0x00100000
OFN_LONGNAMES                = 0x00200000     -- force long names for 3.x modules
-- OFN_ENABLEINCLUDENOTIFY and OFN_ENABLESIZING require
-- Windows 2000 or higher to have any effect.
OFN_ENABLEINCLUDENOTIFY      = 0x00400000     -- send include message to callback
OFN_ENABLESIZING             = 0x00800000
OFN_DONTADDTORECENT          = 0x02000000
OFN_FORCESHOWHIDDEN          = 0x10000000    -- Show All files including System and hidden files

--FlagsEx Values
OFN_EX_NOPLACESBAR = 0x00000001

-- Return values for the registered message sent to the hook function
-- when a sharing violation occurs.  OFN_SHAREFALLTHROUGH allows the
-- filename to be accepted, OFN_SHARENOWARN rejects the name but puts
-- up no warning (returned when the app has already put up a warning
-- message), and OFN_SHAREWARN puts up the default warning message
-- for sharing violations.
--
-- Note:  Undefined return values map to OFN_SHAREWARN, but are
--        reserved for future use.
OFN_SHAREFALLTHROUGH     = 2
OFN_SHARENOWARN          = 1
OFN_SHAREWARN            = 0

ffi.cdef[[
typedef UINT_PTR (*LPOFNHOOKPROC) (HWND, UINT, WPARAM, LPARAM);

typedef struct tagOFNW {
   DWORD        lStructSize;
   HWND         hwndOwner;
   HINSTANCE    hInstance;
   LPCWSTR      lpstrFilter;
   LPWSTR       lpstrCustomFilter;
   DWORD        nMaxCustFilter;
   DWORD        nFilterIndex;
   LPWSTR       lpstrFile;
   DWORD        nMaxFile;
   LPWSTR       lpstrFileTitle;
   DWORD        nMaxFileTitle;
   LPCWSTR      lpstrInitialDir;
   LPCWSTR      lpstrTitle;
   DWORD        Flags;
   WORD         nFileOffset;
   WORD         nFileExtension;
   LPCWSTR      lpstrDefExt;
   LPARAM       lCustData;
   LPOFNHOOKPROC lpfnHook;
   LPCWSTR      lpTemplateName;
   void *        pvReserved;
   DWORD        dwReserved;
   DWORD        FlagsEx;
} OPENFILENAMEW, *LPOPENFILENAMEW;

BOOL GetSaveFileNameW(LPOPENFILENAMEW);
BOOL GetOpenFileNameW(LPOPENFILENAMEW);
]]

local function wcsmax(maxfield)
	return function(s, cdata)
		local s, sz = wcs_sz(s)
		cdata[maxfield] = sz
		return s
	end
end

local function set_filter(s)
	if type(s) == 'table' then s = table.concat(s,'\0') end
	if type(s) == 'string' then s = s..'\0' end
	return wcs(s)
end

OPENFILENAME = struct{
	ctype = 'OPENFILENAMEW', size = 'lStructSize',
	fields = sfields{
		'filepath', 'lpstrFile', pass, mbs, --out
		'filename', 'lpstrFileTitle', pass, mbs, --out
		'filter', 'lpstrFilter', set_filter, mbs,
		'custom_filter', 'lpstrCustomFilter', pass, mbs, --out
		'filter_index', 'nFilterIndex', pass, pass,
		'initial_dir', 'lpstrInitialDir', wcs, mbs,
		'title', 'lpstrTitle', wcs, mbs,
		'flags', 'Flags', flags, pass, --OFN_*
		'default_ext', 'lpstrDefExt', wcs, mbs,
		'flags_ex', 'FlagsEx', flags, pass, --OFN_EX_*
	},
}

function GetSaveFileName(info)
	info = OPENFILENAME(info)
	info.lpstrFile, info.nMaxFile = WCS()
	info.lpstrFileTitle, info.nMaxFileTitle = WCS()
	info.lpstrCustomFilter, info.nMaxCustFilter = WCS()
	return checkcomdlg(comdlg.GetSaveFileNameW(info)), info
end

function GetOpenFileName(info)
	info = OPENFILENAME(info)
	info.lpstrFile, info.nMaxFile = WCS()
	info.lpstrFileTitle, info.nMaxFileTitle = WCS()
	info.lpstrCustomFilter, info.nMaxCustFilter = WCS()
	return checkcomdlg(comdlg.GetOpenFileNameW(info)), info
end


--showcase
if not ... then
	local ok, info = GetSaveFileName{
		title = 'Save this thing',
		filter = {'All Files','*.*','Text Files','*.txt'},
		filter_index = 1,
		flags = 'OFN_SHOWHELP',
	}
	print(ok, info.filepath, info.filename, info.filter_index, info.custom_filter)
end

