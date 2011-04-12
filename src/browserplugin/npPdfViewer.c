/* (Minimal) SumatraPDF Browser Plugin - Copyright © 2010-2011  Simon Bünzli */

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <wchar.h>
#include <limits.h>

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

NPNetscapeFuncs gNPNFuncs;
HINSTANCE g_hInstance = NULL;
const WCHAR *g_lpRegKey = L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin";


/* ::::: DLL Exports ::::: */

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	g_hInstance = hInstance;
	return TRUE;
	
	UNREFERENCED_PARAMETER(dwReason);
	UNREFERENCED_PARAMETER(lpReserved);
}

DLLEXPORT NPError WINAPI NP_GetEntryPoints(NPPluginFuncs *pFuncs)
{
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
	if (!pFuncs || pFuncs->size < sizeof(NPNetscapeFuncs))
	{
		return NPERR_INVALID_FUNCTABLE_ERROR;
	}
	if (HIBYTE(pFuncs->version) > NP_VERSION_MAJOR)
	{
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	
	gNPNFuncs = *pFuncs;
	
	return NPERR_NO_ERROR;
}

DLLEXPORT NPError WINAPI NP_Shutdown(void)
{
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

/* ::::: Plugin Window Procedure ::::: */

typedef struct {
	NPWindow *npwin;
	LPCWSTR message;
	WCHAR filepath[MAX_PATH];
	HANDLE hProcess;
	WCHAR exepath[MAX_PATH + 2];
	FLOAT progress, prevProgress;
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
	
	if (!instance)
	{
		return NPERR_INVALID_INSTANCE_ERROR;
	}
	
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
		return NPERR_INVALID_INSTANCE_ERROR;
	}
	
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
	HWND hwnd;
	FLOAT prev, diff;

	if (!data || !data->npwin || !data->npwin->window)
		return;

	hwnd = (HWND)data->npwin->window;
	prev = data->prevProgress;
	data->prevProgress = data->progress;

	diff = data->progress - prev;
	if (diff < 0 || diff > 0.01f)
	{
		InvalidateRect(hwnd, NULL, FALSE);
		UpdateWindow(hwnd);
	}
}

NPError NP_LOADDS NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
	InstanceData *data = instance->pdata;
	
	if (!*data->exepath)
	{
		return NPERR_FILE_NOT_FOUND;
	}
	
	*stype = NP_ASFILE;
	
	data->progress = stream->end > 0 ? 0.01f : 0;
	data->prevProgress = -.1f;
	RepaintOnProgressChange(data);
	
	return NPERR_NO_ERROR;
	
	UNREFERENCED_PARAMETER(type);
	UNREFERENCED_PARAMETER(seekable);
}

int32_t NP_LOADDS NPP_WriteReady(NPP instance, NPStream* stream)
{
	return stream->end > 0 ? stream->end : INT_MAX;
	
	UNREFERENCED_PARAMETER(instance);
	UNREFERENCED_PARAMETER(stream);
}

int32_t NP_LOADDS NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
	InstanceData *data = instance->pdata;
	
	data->progress = stream->end > 0 ? 1.0f * (offset + len) / stream->end : 0;
	RepaintOnProgressChange(data);
	return len;

	UNREFERENCED_PARAMETER(buffer);
}

// TODO: NPP_StreamAsFile is never called by Firefox 4.0 for large files due to
//       Mozilla bug https://bugzilla.mozilla.org/show_bug.cgi?id=644149

void NP_LOADDS NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	InstanceData *data = instance->pdata;
	
	WCHAR cmdLine[MAX_PATH * 3];
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFOW si = { 0 };
	
	if (!fname)
	{
		data->message = L"Error: The PDF document couldn't be downloaded!";
	}
	
	data->progress = 1.0f;
	data->prevProgress = 0.0f; // force update
	RepaintOnProgressChange(data);
	
	if (!fname)
	{
		return;
	}
	
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fname, -1, data->filepath, MAX_PATH))
	{
		MultiByteToWideChar(CP_ACP, 0, fname, -1, data->filepath, MAX_PATH);
	}
	
	wsprintfW(cmdLine, L"%s -plugin %d \"%s\"", data->exepath, data->npwin->window, data->filepath);
	
	si.cb = sizeof(si);
	if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CloseHandle(pi.hThread);
		data->hProcess = pi.hProcess;
	}
	else
	{
		data->message = L"Error: Couldn't run SumatraPDF!";
	}
	if (data->npwin)
	{
		InvalidateRect((HWND)data->npwin->window, NULL, FALSE);
		UpdateWindow((HWND)data->npwin->window);
	}
	
	UNREFERENCED_PARAMETER(stream);
}

NPError NP_LOADDS NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
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
		return NPERR_INVALID_INSTANCE_ERROR;
	}
	
	data = instance->pdata;
	if (data->hProcess)
	{
		TerminateProcess(data->hProcess, 99);
		WaitForSingleObject(data->hProcess, INFINITE);
		CloseHandle(data->hProcess);
	}
	if (*data->filepath)
	{
		WCHAR tempDir[MAX_PATH];
		DWORD len = GetTempPathW(MAX_PATH, tempDir);
		if (0 < len && len < MAX_PATH && !wcsncmp(data->filepath, tempDir, len))
		{
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
	if (platformPrint && platformPrint->mode == NP_FULL)
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
