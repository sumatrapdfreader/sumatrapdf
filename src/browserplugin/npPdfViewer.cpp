/* (Minimal) SumatraPDF Browser Plugin - Copyright © 2010-2011  Simon Bünzli */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "CmdLineParser.h"
#include "SimpleLog.h"
#include <shlwapi.h>

#ifndef _WINDOWS
#define _WINDOWS
#endif
#include "npapi/npfunctions.h"

#undef NP_END_MACRO
#define NP_END_MACRO } __pragma(warning(push)) __pragma(warning(disable:4127)) while (0) __pragma(warning(pop))

#define DLLEXPORT extern "C"
#pragma comment(linker, "/EXPORT:NP_GetEntryPoints=_NP_GetEntryPoints@4,PRIVATE")
#pragma comment(linker, "/EXPORT:NP_Initialize=_NP_Initialize@4,PRIVATE")
#pragma comment(linker, "/EXPORT:NP_Shutdown=_NP_Shutdown@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=_DllRegisterServer@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=_DllUnregisterServer@0,PRIVATE")

/* Allow logging plugin activity with OutputDebugString(). This can be viewed
   with DebugView http://technet.microsoft.com/en-us/sysinternals/bb896647
   In debug output sp: stands for "Sumatra Plugin" (so that we can distinguish
   our logs from other apps logs) */
#if 0
Log::DebugLogger gLogger;
#define dbg(msg, ...) gLogger.LogFmt(_T(msg), __VA_ARGS__)

const TCHAR *DllMainReason(DWORD reason)
{
	if (DLL_PROCESS_ATTACH == reason)
		return _T("DLL_PROCESS_ATTACH");
	if (DLL_PROCESS_DETACH == reason)
		return _T("DLL_PROCESS_DETACH");
	if (DLL_THREAD_ATTACH == reason)
		return _T("DLL_THREAD_ATTACH");
	if (DLL_THREAD_DETACH == reason)
		return _T("DLL_THREAD_DETACH");
	return _T("UNKNOWN");
}
#else
#define dbg(format, ...) NoOp()
#endif

NPNetscapeFuncs gNPNFuncs;
HINSTANCE g_hInstance = NULL;
const WCHAR *g_lpRegKey = L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin";


/* ::::: DLL Exports ::::: */

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	dbg("sp: DllMain() reason: %d (%s)", dwReason, DllMainReason(dwReason));

	g_hInstance = hInstance;
	return TRUE;
}

DLLEXPORT NPError WINAPI NP_GetEntryPoints(NPPluginFuncs *pFuncs)
{
	dbg("sp: NP_GetEntryPoints()");
	if (!pFuncs || pFuncs->size < sizeof(NPPluginFuncs))
	{
		return NPERR_INVALID_FUNCTABLE_ERROR;
	}
	
	pFuncs->size = sizeof(NPPluginFuncs);
	pFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
	pFuncs->newp = NPP_New;
	pFuncs->destroy = NPP_Destroy;
	pFuncs->setwindow = NPP_SetWindow;
	pFuncs->newstream = NPP_NewStream;
	pFuncs->destroystream = NPP_DestroyStream;
	pFuncs->asfile = NPP_StreamAsFile;
	pFuncs->writeready = NPP_WriteReady;
	pFuncs->write = NPP_Write;
	pFuncs->print = NPP_Print;
	pFuncs->event = NULL;
	pFuncs->urlnotify = NULL;
	pFuncs->javaClass = NULL;
	pFuncs->getvalue = NULL;
	pFuncs->setvalue = NULL;
	
	return NPERR_NO_ERROR;
}

DLLEXPORT NPError WINAPI NP_Initialize(NPNetscapeFuncs *pFuncs)
{
	dbg("sp: NP_Initialize()");

	if (!pFuncs || pFuncs->size < sizeof(NPNetscapeFuncs))
	{
		dbg("sp: NP_Initialize() error: NPERR_INVALID_FUNCTABLE_ERROR");
		return NPERR_INVALID_FUNCTABLE_ERROR;
	}
	if (HIBYTE(pFuncs->version) > NP_VERSION_MAJOR)
	{
		dbg("sp: NP_Initialize() error: NPERR_INCOMPATIBLE_VERSION_ERROR");
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	
	gNPNFuncs = *pFuncs;
	
	return NPERR_NO_ERROR;
}

DLLEXPORT NPError WINAPI NP_Shutdown(void)
{
	dbg("sp: NP_Shutdown()");
	return NPERR_NO_ERROR;
}

#ifndef SHREGSET_FORCE_HKCU
#define SHREGSET_FORCE_HKCU 2
#define SHREGSET_HKLM 4
#endif
bool SetRegValueW(LPCWSTR lpKey, LPCWSTR lpName, LPCWSTR lpValue)
{
	SHRegSetUSValueW(lpKey, lpName, REG_SZ, lpValue, Str::Len(lpValue) * sizeof(WCHAR), SHREGSET_HKLM);
	return SHRegSetUSValueW(lpKey, lpName, REG_SZ, lpValue, Str::Len(lpValue) * sizeof(WCHAR), SHREGSET_FORCE_HKCU) == ERROR_SUCCESS;
}

DLLEXPORT STDAPI DllRegisterServer(VOID)
{
	WCHAR szPath[MAX_PATH];
	HKEY hKey;
	
	if (RegCreateKeyExW(HKEY_CURRENT_USER, g_lpRegKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
	{
		return E_UNEXPECTED;
	}
	RegCloseKey(hKey);
	
	GetModuleFileNameW(g_hInstance, szPath, MAX_PATH);
	if (!SetRegValueW(g_lpRegKey, L"Description", L"SumatraPDF Browser Plugin") ||
		!SetRegValueW(g_lpRegKey, L"Path", szPath) || !SetRegValueW(g_lpRegKey, L"Version", L"0") ||
		!SetRegValueW(g_lpRegKey, L"ProductName", L"SumatraPDF Browser Plugin"))
	{
		return E_UNEXPECTED;
	}
	
	ScopedMem<WCHAR> mimeType(Str::Join(g_lpRegKey, L"\\MimeTypes\\application/pdf"));
	if (RegCreateKeyExW(HKEY_CURRENT_USER, mimeType, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
	}
	mimeType.Set(Str::Join(g_lpRegKey, L"\\MimeTypes\\application/vnd.ms-xpsdocument"));
	if (RegCreateKeyExW(HKEY_CURRENT_USER, mimeType, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
	}
	
	// Work around Mozilla bug https://bugzilla.mozilla.org/show_bug.cgi?id=581848 which
	// makes Firefox up to version 3.6.* ignore all but the first plugin for a given MIME type
	// (per http://code.google.com/p/sumatrapdf/issues/detail?id=1254#c12 Foxit does the same)
	*PathFindFileNameW(szPath) = L'\0';
	if (SHGetValueW(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH", NULL, NULL, NULL) == ERROR_FILE_NOT_FOUND)
	{
		SHSetValueW(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH", REG_SZ, szPath, (Str::Len(szPath) + 1) * sizeof(WCHAR));
		SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
	}
	
	return S_OK;
}

DLLEXPORT STDAPI DllUnregisterServer(VOID)
{
	WCHAR szPluginPath[MAX_PATH];
	DWORD dwSize = MAX_PATH * sizeof(WCHAR);
	
	if (SHGetValueW(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH", NULL, szPluginPath, &dwSize) == ERROR_SUCCESS)
	{
		WCHAR szModulePath[MAX_PATH];
		GetModuleFileNameW(g_hInstance, szModulePath, MAX_PATH);
		if (Str::StartsWithI(szModulePath, szPluginPath))
		{
			SHDeleteValueW(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH");
		}
	}
	
	SHDeleteKeyW(HKEY_LOCAL_MACHINE, g_lpRegKey);
	if (SHDeleteKeyW(HKEY_CURRENT_USER, g_lpRegKey) != ERROR_SUCCESS)
	{
		return E_UNEXPECTED;
	}
	
	return S_OK;
}

/* ::::: Auxiliary Methods ::::: */

bool GetExePath(LPWSTR lpPath, int len)
{
	// Search the plugin's directory first
	GetModuleFileNameW(g_hInstance, lpPath, len - 2);
	lstrcpyW(PathFindFileNameW(lpPath), L"SumatraPDF.exe");
	if (PathFileExistsW(lpPath))
	{
		PathQuoteSpacesW(lpPath);
		return true;
	}
	
	*lpPath = L'\0';
	// Try to get the path from the registry (set e.g. when making the default PDF viewer)
	ScopedMem<TCHAR> path(ReadRegStr(HKEY_CURRENT_USER, _T("Software\\Classes\\SumatraPDF\\Shell\\Open\\Command"), NULL));
	if (!path)
		return false;

	CmdLineParser args(path);
	if (!File::Exists(args[0]))
		return false;

	ScopedMem<WCHAR> pathw(Str::Conv::ToWStr(args[0]));
	Str::BufSet(lpPath, len, pathw);
	return true;
}

// filePathBuf must be MAX_PATH in size
HANDLE CreateTempFile(WCHAR *filePathBufOut)
{
	DWORD		ret;
	UINT		uret;
	HANDLE		hFile;
	WCHAR		pathBuf[MAX_PATH];

	ret = GetTempPath(dimof(pathBuf), pathBuf);
	if (0 == ret || ret > dimof(pathBuf))
	{
		dbg("sp: CreateTempFile(): GetTempPath() failed");
		return NULL;
	}

	uret = GetTempFileName(pathBuf, L"SPlugTmp", 0, filePathBufOut);
	if (0 == uret)
	{
		dbg("sp: CreateTempFile(): GetTempFileName() failed");
		return NULL;
	}

	hFile = CreateFile(filePathBufOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
						FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		dbg("sp: CreateTempFile(): CreateFile() failed");
		return NULL;
	}
	return hFile;
}

/* ::::: Plugin Window Procedure ::::: */

typedef struct {
	NPWindow *	npwin;
	LPCWSTR		message;
	WCHAR		filepath[MAX_PATH];
	HANDLE      hFile;
	HANDLE		hProcess;
	WCHAR		exepath[MAX_PATH + 2];
	FLOAT		progress, prevProgress;
	uint32_t	totalSize, currSize;
} InstanceData;

#define COL_WINDOW_BG RGB(0xcc, 0xcc, 0xcc)

enum Magnitudes { KB = 1024, MB = 1024 * KB, GB = 1024 * MB };

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// Caller needs to free the result.
static TCHAR *FormatSizeSuccint(size_t size) {
	const TCHAR *unit = NULL;
	double s = (double)size;

	if (size > GB)
	{
		s /= GB;
		unit = _T("GB");
	}
	else if (size > MB)
	{
		s /= MB;
		unit = _T("MB");
	}
	else
	{
		s /= KB;
		unit = _T("KB");
	}
	
	return Str::FormatFloatWithThousandSep(s, unit);
}

LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	InstanceData *data = (InstanceData *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	
	if (uiMsg == WM_PAINT)
	{
		PAINTSTRUCT ps;
		RECT rcClient;
		
		HDC hDC = BeginPaint(hWnd, &ps);
		HBRUSH brushBg = CreateSolidBrush(COL_WINDOW_BG);
		HFONT hFont = CreateFontW(-MulDiv(14, GetDeviceCaps(hDC, LOGPIXELSY), 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"MS Shell Dlg");
		HDC hDCBuffer = CreateCompatibleDC(hDC);
		HBITMAP hDoubleBuffer;
		HWND hChild;
		
		// set up double buffering
		GetClientRect(hWnd, &rcClient);
		hDoubleBuffer = CreateCompatibleBitmap(hDC, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
		SelectObject(hDCBuffer, hDoubleBuffer);
		
		// display message centered in the window
		FillRect(hDCBuffer, &rcClient, brushBg);
		hFont = (HFONT)SelectObject(hDCBuffer, hFont);
		SetTextColor(hDCBuffer, RGB(0, 0, 0));
		SetBkMode(hDCBuffer, TRANSPARENT);
		DrawTextW(hDCBuffer, data->message, -1, &rcClient, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		
		// draw a progress bar, if a download is in progress
		if (0 < data->progress && data->progress <= 1)
		{
			ScopedMem<TCHAR> currSize(FormatSizeSuccint(data->currSize));
			ScopedMem<TCHAR> totalSize(FormatSizeSuccint(data->totalSize));
			ScopedMem<TCHAR> s(Str::Format(_T("%s of %s"), currSize, totalSize));
			SIZE msgSize;
			RECT rcProgress = rcClient;
			RECT rcProgressAll;
			HBRUSH brushProgress = CreateSolidBrush(RGB(0x80, 0x80, 0xff));
			
			GetTextExtentPoint32W(hDCBuffer, data->message, Str::Len(data->message), &msgSize);
			InflateRect(&rcProgress, -(rcProgress.right - rcProgress.left - msgSize.cx) / 2, (-(rcProgress.bottom - rcProgress.top - msgSize.cy) / 2) + 2);
			OffsetRect(&rcProgress, 0, msgSize.cy + 4 + 2);
			FillRect(hDCBuffer, &rcProgress, (HBRUSH)GetStockObject(WHITE_BRUSH));
			rcProgressAll = rcProgress;
			rcProgress.right = (LONG)(rcProgress.left + data->progress * (rcProgress.right - rcProgress.left));
			FillRect(hDCBuffer, &rcProgress, brushProgress);
			
			DeleteObject(brushProgress);
			
			// don't display weird values (especially when totalSize is unknown)
			if (data->currSize <= data->totalSize && 0 < data->totalSize)
			{
				DrawTextW(hDCBuffer, s, -1, &rcProgressAll, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
			}
		}
		
		// draw the buffer on screen
		BitBlt(hDC, 0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, hDCBuffer, 0, 0, SRCCOPY);
		
		DeleteObject(SelectObject(hDCBuffer, hFont));
		DeleteObject(brushBg);
		DeleteObject(hDoubleBuffer);
		DeleteDC(hDCBuffer);
		EndPaint(hWnd, &ps);
		
		hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
		if (hChild)
		{
			InvalidateRect(hChild, NULL, FALSE);
		}
	}
	else if (uiMsg == WM_SIZE)
	{
		HWND hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
		if (hChild)
		{
			RECT rcClient;
			GetClientRect(hWnd, &rcClient);
			MoveWindow(hChild, rcClient.left, rcClient.top, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, FALSE);
		}
		
	}
	
	return DefWindowProc(hWnd, uiMsg, wParam, lParam);
}

/* ::::: Plugin Methods ::::: */

NPError NP_LOADDS NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
	InstanceData *data;

	dbg("sp: NPP_New() mode=%d ", mode);

	if (!instance)
	{
		dbg("error: NPERR_INVALID_INSTANCE_ERROR");
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	if (pluginType)
		dbg("sp:   pluginType: %s ", pluginType);
	if (saved)
		dbg("sp:   SavedData: len=%d", saved->len);

	instance->pdata = calloc(1, sizeof(InstanceData));
	data = (InstanceData *)instance->pdata;
	gNPNFuncs.setvalue(instance, NPPVpluginWindowBool, (void *)true);
	
	if (GetExePath(data->exepath, MAX_PATH + 2))
	{
		data->message = L"Opening PDF document in SumatraPDF...";
	}
	else
	{
		data->message = L"Error: SumatraPDF hasn't been found!";
	}
	
	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPP_SetWindow(NPP instance, NPWindow *npwin)
{
	InstanceData *data;

	if (!instance)
	{
		dbg("sp: NPP_SetWindow() errro: NPERR_INVALID_INSTANCE_ERROR");
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	dbg("sp: NPP_SetWindow()");

	data = (InstanceData *)instance->pdata;
	if (!npwin)
	{
		data->npwin = NULL;
	}
	else if (data->npwin != npwin)
	{
		HWND hWnd = (HWND)npwin->window;
		
		data->npwin = npwin;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)data);
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)PluginWndProc);
	}
	else
	{
		// The plugin's window hasn't changed, just its size
		HWND hWnd = (HWND)npwin->window;
		HWND hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
		
		if (hChild)
		{
			RECT rcClient;
			GetClientRect(hWnd, &rcClient);
			MoveWindow(hChild, rcClient.left, rcClient.top, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, FALSE);
		}
	}
	
	return NPERR_NO_ERROR;
}

static void RepaintOnProgressChange(InstanceData *data)
{
	FLOAT diff = data->progress - data->prevProgress;

	if (!data || !data->npwin || !data->npwin->window)
		return;

	if (diff < 0 || diff > 0.01f)
	{
		HWND hwnd = (HWND)data->npwin->window;
		InvalidateRect(hwnd, NULL, FALSE);
		UpdateWindow(hwnd);
		data->prevProgress = data->progress;
	}
}

// To workaround Firefox bug https://bugzilla.mozilla.org/show_bug.cgi?id=644149
// if a file is big (not sure what the exact threshold should be, we use
// conservative 3MB) or of undetermined size, we save it to a temporary file
// ourselves instead of relying on the browser to do it.
// cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1328
#define BIG_FILE_THRESHOLD (3 * 1024 * 1024)

NPError NP_LOADDS NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
	InstanceData *data = (InstanceData *)instance->pdata;

	if (!*data->exepath)
	{
		dbg("sp: NPP_NewStream() error: NPERR_FILE_NOT_FOUND");
		return NPERR_FILE_NOT_FOUND;
	}

	dbg("sp: NPP_NewStream() end = %d", stream->end);

	// default to asking the browser to create the temporary file for us
	*stype = NP_ASFILE;

	data->hFile = NULL;
	// create a temporary file only for Gecko based browsers such as Firefox
	const char *userAgent = gNPNFuncs.uagent(instance);
	if (strstr(userAgent, "Gecko/") && (stream->end > BIG_FILE_THRESHOLD || !stream->end))
	{
		data->hFile = CreateTempFile(data->filepath);
		if (data->hFile)
		{
			dbg("sp: using temporary file: %s", data->filepath);
			*stype = NP_NORMAL;
		}
	}
	data->totalSize = stream->end;
	data->currSize = 0;
	data->progress = stream->end > 0 ? 0.01f : 0;
	data->prevProgress = -.1f;
	RepaintOnProgressChange(data);
	
	return NPERR_NO_ERROR;
}

int32_t NP_LOADDS NPP_WriteReady(NPP instance, NPStream* stream)
{
	int32_t res = stream->end > 0 ? stream->end : INT_MAX;
	dbg("sp: NPP_WriteReady() res = %d", res);
	return res;
}

int32_t NP_LOADDS NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
	InstanceData *data = (InstanceData *)instance->pdata;
	DWORD bytesWritten = len;

	dbg("sp: NPP_Write() off = %d, len=%d", offset, len);

	if (data->hFile)
	{
		// Note: we optimistically assume that data comes in sequentially
		// (i.e. next offset will be current offset + bytesWritten)
		BOOL ok = WriteFile(data->hFile, buffer, (DWORD)len, &bytesWritten, NULL);
		if (!ok)
		{
			dbg("sp: NPP_Write() failed to write %d bytes at offset %d", len, offset);
			return -1;
		}
	}

	data->currSize = offset + bytesWritten;
	data->progress = stream->end > 0 ? 1.0f * (offset + len) / stream->end : 0;
	RepaintOnProgressChange(data);

	return bytesWritten;
}

static void LaunchWithSumatra(InstanceData *data)
{
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFOW si = { 0 };

	if (!PathFileExistsW(data->filepath))
		dbg("sp: NPP_StreamAsFile() error: file doesn't exist");

	ScopedMem<WCHAR> cmdLine(Str::Format(L"%s -plugin %d \"%s\"", data->exepath, (HWND)data->npwin->window, data->filepath));
	
	si.cb = sizeof(si);
	if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		data->hProcess = pi.hProcess;
	}
	else
	{
		dbg("sp: NPP_StreamAsFile() error: couldn't run SumatraPDF!");
		data->message = L"Error: Couldn't run SumatraPDF!";
	}
}

void NP_LOADDS NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	InstanceData *data = (InstanceData *)instance->pdata;

	if (!fname)
	{
		dbg("sp: NPP_StreamAsFile() error: fname is NULL");
		data->message = L"Error: The PDF document couldn't be downloaded!";
		goto Exit;
	}

	dbg("sp: NPP_StreamAsFile() fname=%s", ScopedMem<TCHAR>(Str::Conv::FromAnsi(fname)));

	if (data->hFile)
	{
		dbg("sp: NPP_StreamAsFile() error: data->hFile is != NULL (should be NULL)");
	}

	data->progress = 1.0f;
	data->prevProgress = 0.0f; // force update
	RepaintOnProgressChange(data);

	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fname, -1, data->filepath, MAX_PATH))
	{
		MultiByteToWideChar(CP_ACP, 0, fname, -1, data->filepath, MAX_PATH);
	}

	LaunchWithSumatra(data);

Exit:
	if (data->npwin)
	{
		InvalidateRect((HWND)data->npwin->window, NULL, FALSE);
		UpdateWindow((HWND)data->npwin->window);
	}
}

NPError NP_LOADDS NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
	InstanceData *data;

	dbg("sp: NPP_DestroyStream() reason: %d", reason);
	if (stream)
	{
		if (stream->url)
			dbg("sp:   url: %s", stream->url);
		dbg("sp:   end: %d", stream->end);
	}

	if (!instance)
	{
		dbg("sp: NPP_DestroyStream() error: NPERR_INVALID_INSTANCE_ERROR");
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	data = (InstanceData *)instance->pdata;
	if (!data)
	{
		dbg("sp: NPP_DestroyStream() error: instance->pdata is NULL");
		return NPERR_NO_ERROR;
	}

	if (!data->hFile)
		goto Exit;

	CloseHandle(data->hFile);
	LaunchWithSumatra(data);

Exit:
	if (data->npwin)
	{
		InvalidateRect((HWND)data->npwin->window, NULL, FALSE);
		UpdateWindow((HWND)data->npwin->window);
	}

	return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPP_Destroy(NPP instance, NPSavedData** save)
{
	InstanceData *data;
	
	if (!instance)
	{
		dbg("sp: NPP_Destroy() error: NPERR_INVALID_INSTANCE_ERROR");
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	dbg("sp: NPP_Destroy()");
	data = (InstanceData *)instance->pdata;
	if (data->hProcess)
	{
		dbg("sp: NPP_Destroy(): waiting for Sumatra to exit");
		TerminateProcess(data->hProcess, 99);
		WaitForSingleObject(data->hProcess, INFINITE);
		CloseHandle(data->hProcess);
	}
	if (data->hFile)
	{
		dbg("sp: NPP_Destroy(): deleting internal temporary file %s",
			ScopedMem<TCHAR>(Str::Conv::FromWStr(data->filepath)));
		DeleteFileW(data->filepath);
		*data->filepath = 0;
	}

	if (*data->filepath)
	{
		WCHAR tempDir[MAX_PATH];
		DWORD len = GetTempPathW(MAX_PATH, tempDir);
		if (0 < len && len < MAX_PATH && Str::StartsWithI(data->filepath, tempDir))
		{
			dbg("sp: NPP_Destroy(): deleting browser temporary file %s",
				ScopedMem<TCHAR>(Str::Conv::FromWStr(data->filepath)));
			DeleteFileW(data->filepath);
		}
	}
	free(data);
	
	return NPERR_NO_ERROR;
}

// TODO: NPP_Print seems to never be called by Google Chrome

#define IDM_PRINT 403

void NP_LOADDS NPP_Print(NPP instance, NPPrint* platformPrint)
{
	if (!platformPrint)
	{
		dbg("sp: NPP_Print(), platformPrint is NULL");
		return;
	}

	if (NP_FULL != platformPrint->mode)
	{
		dbg("sp: NPP_Print(), platformPrint->mode is %d (!= NP_FULL)", platformPrint->mode);
	}
	else
	{
		InstanceData *data = (InstanceData *)instance->pdata;
		HWND hWnd = (HWND)data->npwin->window;
		HWND hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
		
		if (hChild)
		{
			PostMessage(hChild, WM_COMMAND, IDM_PRINT, 0);
			platformPrint->print.fullPrint.pluginPrinted = true;
		}
	}
}
