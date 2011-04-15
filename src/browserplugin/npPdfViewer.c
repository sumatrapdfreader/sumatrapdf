/* (Minimal) SumatraPDF Browser Plugin - Copyright © 2010-2011  Simon Bünzli */

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <wchar.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef _WINDOWS
#define _WINDOWS
#endif
#include "npapi/npfunctions.h"

#ifndef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#else
#undef NP_END_MACRO
#define NP_END_MACRO } __pragma(warning(push)) __pragma(warning(disable:4127)) while (0) __pragma(warning(pop))

#define DLLEXPORT
#pragma comment(linker, "/EXPORT:NP_GetEntryPoints=_NP_GetEntryPoints@4,PRIVATE")
#pragma comment(linker, "/EXPORT:NP_Initialize=_NP_Initialize@4,PRIVATE")
#pragma comment(linker, "/EXPORT:NP_Shutdown=_NP_Shutdown@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=_DllRegisterServer@0,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=_DllUnregisterServer@0,PRIVATE")
#endif

/* Allow logging plugin activity with OutputDebugString(). This can be viewed
   with DebugView http://technet.microsoft.com/en-us/sysinternals/bb896647
   In debug output sp: stands for "Sumatra Plugin" (so that we can distinguish
   our logs from other apps logs) */
#if 0
#define SAZA(struct_name, n) (struct_name *)calloc((n), sizeof(struct_name))

char *FmtV(const char *fmt, va_list args)
{
    char    message[256];
    size_t  bufCchSize = sizeof(message);
    char  * buf = message;
    for (;;)
    {
        int count = _vsnprintf(buf, bufCchSize, fmt, args);
        if (0 <= count && (size_t)count < bufCchSize)
            break;
        /* we have to make the buffer bigger. The algorithm used to calculate
           the new size is arbitrary (aka. educated guess) */
        if (buf != message)
            free(buf);
        if (bufCchSize < 4*1024)
            bufCchSize += bufCchSize;
        else
            bufCchSize += 1024;
        buf = SAZA(char, bufCchSize);
        if (!buf)
            break;
    }

    if (buf == message)
        buf = _strdup(message);

    return buf;
}

void dbg(const char *format, ...)
{
    char *buf = NULL;
	va_list args;
	va_start(args, format);
	buf = FmtV(format, args);
	OutputDebugStringA(buf);
	free(buf);
	va_end(args);
}

void dbgw(const WCHAR *s)
{
	OutputDebugStringW(s);
}

const char *DllMainReason(DWORD reason)
{
	if (DLL_PROCESS_ATTACH == reason)
		return "DLL_PROCESS_ATTACH";
	if (DLL_PROCESS_DETACH == reason)
		return "DLL_PROCESS_DETACH";
	if (DLL_THREAD_ATTACH == reason)
		return "DLL_THREAD_ATTACH";
	if (DLL_THREAD_DETACH == reason)
		return "DLL_THREAD_DETACH";
	return "UNKNOWN";
}
#else
  #define dbg(format, ...) ((void)0)
  #define dbgw(s) ((void)0)
#endif

NPNetscapeFuncs gNPNFuncs;
HINSTANCE g_hInstance = NULL;
const WCHAR *g_lpRegKey = L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin";


/* ::::: DLL Exports ::::: */

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	dbg("sp: DllMain() reason: %d (%s)\n", (int)dwReason, DllMainReason(dwReason));

	g_hInstance = hInstance;
	return TRUE;

	UNREFERENCED_PARAMETER(dwReason);
	UNREFERENCED_PARAMETER(lpReserved);
}

DLLEXPORT NPError WINAPI NP_GetEntryPoints(NPPluginFuncs *pFuncs)
{
	dbg("sp: NP_GetEntryPoints()\n");
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
	dbg("sp: NP_Initialize()\n");

	if (!pFuncs || pFuncs->size < sizeof(NPNetscapeFuncs))
	{
		dbg("sp: NP_Initialize() error: NPERR_INVALID_FUNCTABLE_ERROR\n");
		return NPERR_INVALID_FUNCTABLE_ERROR;
	}
	if (HIBYTE(pFuncs->version) > NP_VERSION_MAJOR)
	{
		dbg("sp: NP_Initialize() error: NPERR_INCOMPATIBLE_VERSION_ERROR\n");
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	
	gNPNFuncs = *pFuncs;
	
	return NPERR_NO_ERROR;
}

DLLEXPORT NPError WINAPI NP_Shutdown(void)
{
	dbg("sp: NP_Shutdown()\n");
	return NPERR_NO_ERROR;
}

#ifndef SHREGSET_FORCE_HKCU
#define SHREGSET_FORCE_HKCU 2
#define SHREGSET_HKLM 4
#endif
bool SetRegValueW(LPWSTR lpKey, LPWSTR lpName, LPWSTR lpValue)
{
	SHRegSetUSValueW(lpKey, lpName, REG_SZ, lpValue, lstrlenW(lpValue) * sizeof(WCHAR), SHREGSET_HKLM);
	return SHRegSetUSValueW(lpKey, lpName, REG_SZ, lpValue, lstrlenW(lpValue) * sizeof(WCHAR), SHREGSET_FORCE_HKCU) == ERROR_SUCCESS;
}

DLLEXPORT STDAPI DllRegisterServer(VOID)
{
	WCHAR szString[MAX_PATH], szPath[MAX_PATH];
	HKEY hKey;
	
	lstrcpyW(szString, g_lpRegKey);
	if (RegCreateKeyExW(HKEY_CURRENT_USER, szString, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
	{
		return E_UNEXPECTED;
	}
	RegCloseKey(hKey);
	
	GetModuleFileNameW(g_hInstance, szPath, MAX_PATH);
	if (!SetRegValueW(szString, L"Description", L"SumatraPDF Browser Plugin") ||
		!SetRegValueW(szString, L"Path", szPath) || !SetRegValueW(szString, L"Version", L"0") ||
		!SetRegValueW(szString, L"ProductName", L"SumatraPDF Browser Plugin"))
	{
		return E_UNEXPECTED;
	}
	
	lstrcatW(szString, L"\\MimeTypes\\application/pdf");
	if (RegCreateKeyExW(HKEY_CURRENT_USER, szString, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
	}
	
	// Work around Mozilla bug https://bugzilla.mozilla.org/show_bug.cgi?id=581848 which
	// makes Firefox up to version 3.6.* ignore all but the first plugin for a given MIME type
	// (per http://code.google.com/p/sumatrapdf/issues/detail?id=1254#c12 Foxit does the same)
	*PathFindFileNameW(szPath) = L'\0';
	if (SHGetValueW(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH", NULL, NULL, NULL) == ERROR_FILE_NOT_FOUND)
	{
		SHSetValueW(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH", REG_SZ, szPath, (lstrlenW(szPath) + 1) * sizeof(WCHAR));
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
		if (!wcsncmp(szPluginPath, szModulePath, lstrlenW(szPluginPath)))
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

#define dimof(X)    (sizeof(X)/sizeof((X)[0]))

bool GetExePath(LPWSTR lpPath, int len)
{
	DWORD dwSize = len * sizeof(WCHAR);
	
	// Search the plugin's directory first
	GetModuleFileNameW(g_hInstance, lpPath, len - 2);
	lstrcpyW(PathFindFileNameW(lpPath), L"SumatraPDF.exe");
	if (PathFileExistsW(lpPath))
	{
		PathQuoteSpacesW(lpPath);
		return true;
	}
	
	// Try to get the path from the registry (set e.g. when making the default PDF viewer)
	if (SHRegGetUSValueW(L"Software\\Classes\\SumatraPDF\\Shell\\Open\\Command", NULL, NULL, lpPath, &dwSize, FALSE, NULL, 0) == ERROR_SUCCESS)
	{
		WCHAR *args = wcsstr(lpPath, L"\"%1\"");
		if (args)
		{
			*args = L'\0';
		}
		return true;
	}
	
	*lpPath = L'\0';
	return false;
}

static BOOL FileExists(const WCHAR *filePath)
{
	WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
	BOOL res;
	if (NULL == filePath)
		return FALSE;

	res = GetFileAttributesEx(filePath, GetFileExInfoStandard, &fileInfo);
	if (0 == res)
		return FALSE;

	if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return FALSE;
	return TRUE;
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
		dbg("sp: CreateTempFile(): GetTempPath() failed\n");
		return NULL;
	}

	uret = GetTempFileName(pathBuf, L"SPlugTmp", 0, filePathBufOut);
	if (0 == uret)
	{
		dbg("sp: CreateTempFile(): GetTempFileName() failed\n");
		return NULL;
	}

	hFile = CreateFile(filePathBufOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
						FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		dbg("sp: CreateTempFile(): CreateFile() failed\n");
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
} InstanceData;

#define COL_WINDOW_BG RGB(0xcc, 0xcc, 0xcc)

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
			SIZE msgSize;
			RECT rcProgress = rcClient;
			HBRUSH brushProgress = CreateSolidBrush(RGB(0x80, 0x80, 0xff));
			
			GetTextExtentPoint32W(hDCBuffer, data->message, lstrlenW(data->message), &msgSize);
			InflateRect(&rcProgress, -(rcProgress.right - rcProgress.left - msgSize.cx) / 2, -(rcProgress.bottom - rcProgress.top - msgSize.cy) / 2);
			OffsetRect(&rcProgress, 0, msgSize.cy + 4);
			FillRect(hDCBuffer, &rcProgress, GetStockObject(WHITE_BRUSH));
			rcProgress.right = (LONG)(rcProgress.left + data->progress * (rcProgress.right - rcProgress.left));
			FillRect(hDCBuffer, &rcProgress, brushProgress);
			
			DeleteObject(brushProgress);
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

	dbg("sp: NPP_New() mode=%d ", (int)mode);

	if (!instance)
	{
		dbg("error: NPERR_INVALID_INSTANCE_ERROR\n");
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	if (pluginType)
		dbg("pluginType: %s ", pluginType);
	if (saved)
		dbg("SavedData: len=%d", saved->len);
	dbg("\n");

	data = instance->pdata = calloc(1, sizeof(InstanceData));
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
	
	UNREFERENCED_PARAMETER(pluginType);
	UNREFERENCED_PARAMETER(mode);
	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argn);
	UNREFERENCED_PARAMETER(argv);
	UNREFERENCED_PARAMETER(saved);
}

NPError NP_LOADDS NPP_SetWindow(NPP instance, NPWindow *npwin)
{
	InstanceData *data;

	if (!instance)
	{
		dbg("sp: NPP_SetWindow() errro: NPERR_INVALID_INSTANCE_ERROR\n");
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	dbg("sp: NPP_SetWindow()\n");

	data = instance->pdata;
	if (!npwin)
	{
		data->npwin = NULL;
	}
	else if (data->npwin != npwin)
	{
		HWND hWnd = npwin->window;
		
		data->npwin = npwin;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)data);
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)PluginWndProc);
	}
	else
	{
		// The plugin's window hasn't changed, just its size
		HWND hWnd = npwin->window;
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

// To workaround FireFox bug https://bugzilla.mozilla.org/show_bug.cgi?id=644149
// (our bug: http://code.google.com/p/sumatrapdf/issues/detail?id=1328)
// if a file is big (not sure what the exact threshold should be, we use
// conservative 6MB) we save it to a file ourselves instead of relying on the
// browser to do it.
#define BIG_FILE_THRESHOLD 6*1024*1024

NPError NP_LOADDS NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
	InstanceData *data = instance->pdata;

	if (!*data->exepath)
	{
		dbg("sp: NPP_NewStream() error: NPERR_FILE_NOT_FOUND\n");
		return NPERR_FILE_NOT_FOUND;
	}

	dbg("sp: NPP_NewStream() end = %d\n", (int)stream->end);

	data->hFile = NULL;
	// TODO: this should only be done for FireFox, if we can detect we're
	// hosted inside FireFox in a non-hacky way.
	// (a hacky way could be getting a callstack and checking for FireFox.exe
	// module up in the call chain)
	if (stream->end > BIG_FILE_THRESHOLD)
	{
		data->hFile = CreateTempFile(data->filepath);
		if (data->hFile)
		{
			dbg("sp: using temporary file: ");
			dbgw(data->filepath);
			dbg("\n");
			*stype = NP_NORMAL;
			goto Exit;
		}
	}

	// either small file or failed to create a temporary file for the big file
	*stype = NP_ASFILE;

Exit:
	data->progress = stream->end > 0 ? 0.01f : 0;
	data->prevProgress = -.1f;
	RepaintOnProgressChange(data);
	
	return NPERR_NO_ERROR;
	
	UNREFERENCED_PARAMETER(type);
	UNREFERENCED_PARAMETER(seekable);
}

int32_t NP_LOADDS NPP_WriteReady(NPP instance, NPStream* stream)
{
	int32_t res = stream->end > 0 ? stream->end : INT_MAX;
	dbg("sp: NPP_WriteReady() res = %d\n", res);
	return res;
	
	UNREFERENCED_PARAMETER(instance);
	UNREFERENCED_PARAMETER(stream);
}

int32_t NP_LOADDS NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
	InstanceData *data = instance->pdata;
	DWORD bytesWritten = len;

	dbg("sp: NPP_Write() off = %d, len=%d\n", (int)offset, (int)len);

	if (data->hFile)
	{
		BOOL ok = WriteFile(data->hFile, buffer, (DWORD)len, &bytesWritten, NULL);
		if (!ok)
		{
			dbg("sp: NPP_Write() failed to write %d bytes at offset %d\n", (int)len, (int)offset);
			return -1;
		}
	}

	data->progress = stream->end > 0 ? 1.0f * (offset + len) / stream->end : 0;
	RepaintOnProgressChange(data);

	return bytesWritten;

	UNREFERENCED_PARAMETER(buffer);
}

static void LaunchWithSumatra(InstanceData *data)
{
	WCHAR cmdLine[MAX_PATH * 3];
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFOW si = { 0 };

	if (!FileExists(data->filepath))
		dbg("sp: NPP_StreamAsFile() error: file doesn't exist\n");

	wsprintfW(cmdLine, L"%s -plugin %d \"%s\"", data->exepath, data->npwin->window, data->filepath);
	
	si.cb = sizeof(si);
	if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		data->hProcess = pi.hProcess;
	}
	else
	{
		dbg("sp: NPP_StreamAsFile() error: couldn't run SumatraPDF!\n");
		data->message = L"Error: Couldn't run SumatraPDF!";
	}
}

void NP_LOADDS NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	InstanceData *data = instance->pdata;

	if (!fname)
	{
		dbg("sp: NPP_StreamAsFile() error: fname is NULL\n");
		data->message = L"Error: The PDF document couldn't be downloaded!";
		goto Exit;
	}

	dbg("sp: NPP_StreamAsFile() fname=%s\n", fname);

	if (data->hFile)
	{
		dbg("sp: NPP_StreamAsFile() error: data->hFile is != NULL (should be NULL)\n");
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
	
	UNREFERENCED_PARAMETER(stream);
}

NPError NP_LOADDS NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
	InstanceData *data;

    dbg("sp: NPP_DestroyStream() reason: %d", (int)reason);
	if (stream)
	{
		if (stream->url)
			dbg(" url: %s", stream->url);
		dbg(" end: %d", stream->end);
	}
	dbg("\n");

	if (!instance)
	{
		dbg("sp: NPP_DestroyStream() error: NPERR_INVALID_INSTANCE_ERROR\n");
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	data = instance->pdata;
	if (!data)
	{
		dbg("sp: NPP_DestroyStream() error: instance->pdata is NULL\n");
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
	
	UNREFERENCED_PARAMETER(instance);
	UNREFERENCED_PARAMETER(stream);
	UNREFERENCED_PARAMETER(reason);
}

NPError NP_LOADDS NPP_Destroy(NPP instance, NPSavedData** save)
{
	InstanceData *data;
	
	if (!instance)
	{
		dbg("sp: NPP_Destroy() error: NPERR_INVALID_INSTANCE_ERROR\n");
		return NPERR_INVALID_INSTANCE_ERROR;
	}

	dbg("sp: NPP_Destroy()\n");
	data = instance->pdata;
	if (data->hProcess)
	{
		dbg("sp: NPP_Destroy(): waiting for Sumatra to exit\n");
		TerminateProcess(data->hProcess, 99);
		WaitForSingleObject(data->hProcess, INFINITE);
		CloseHandle(data->hProcess);
	}
	if (data->hFile)
	{
		dbg("sp: NPP_Destroy(): deleting internal temporary file ");
		dbgw(data->filepath);
		dbg("\n");
		DeleteFileW(data->filepath);
		*data->filepath = 0;
	}

	if (*data->filepath)
	{
		WCHAR tempDir[MAX_PATH];
		DWORD len = GetTempPathW(MAX_PATH, tempDir);
		if (0 < len && len < MAX_PATH && !wcsncmp(data->filepath, tempDir, len))
		{
			dbg("sp: NPP_Destroy(): deleting browser temporary file ");
			dbgw(data->filepath);
			dbg("\n");
			DeleteFileW(data->filepath);
		}
	}
	free(data);
	
	return NPERR_NO_ERROR;
	
	UNREFERENCED_PARAMETER(save);
}

// TODO: NPP_Print seems to never be called by Google Chrome

#define IDM_PRINT 403

void NP_LOADDS NPP_Print(NPP instance, NPPrint* platformPrint)
{
	if (!platformPrint)
	{
		dbg("sp: NPP_Print(), platformPrint is NULL\n");
		return;
	}

	if (NP_FULL != platformPrint->mode)
	{
		dbg("sp: NPP_Print(), platformPrint->mode is %d (!= NP_FULL)\n", (int)platformPrint->mode);
	}
	else
	{
		InstanceData *data = instance->pdata;
		HWND hWnd = data->npwin->window;
		HWND hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
		
		if (hChild)
		{
			PostMessage(hChild, WM_COMMAND, IDM_PRINT, 0);
			platformPrint->print.fullPrint.pluginPrinted = true;
		}
	}
}

