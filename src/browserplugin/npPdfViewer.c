/* (Minimal) SumatraPDF Browser Plugin - Copyright (C) 2010-2011  Simon Bünzli */

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <wchar.h>

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
	pFuncs->print = NPP_Print;
	
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
	
	return S_OK;
}

DLLEXPORT STDAPI DllUnregisterServer(VOID)
{
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
		wchar_t *args = wcsstr(lpPath, L"\"%1\"");
		if (args)
		{
			*args = '\0';
		}
		return true;
	}
	
	*lpPath = '\0';
	return false;
}

/* ::::: Plugin Window Procedure ::::: */

typedef struct {
	NPWindow *npwin;
	LPCWSTR message;
	LPWSTR filepath;
	HANDLE hProcess;
	WCHAR exepath[MAX_PATH + 2];
} InstanceData;

LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	InstanceData *data = (InstanceData *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	
	if (uiMsg == WM_PAINT)
	{
		PAINTSTRUCT ps;
		RECT rcClient;
		
		HDC hDC = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rcClient);
		FillRect(hDC, &rcClient, GetStockObject(GRAY_BRUSH));
		DrawTextW(hDC, data->message, -1, &rcClient, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		EndPaint(hWnd, &ps);
		
		return 0;
	}
	
	return DefWindowProc(hWnd, uiMsg, wParam, lParam);
}

/* ::::: Plugin Methods ::::: */

NPError NP_LOADDS NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
	InstanceData *data = instance->pdata = calloc(1, sizeof(InstanceData));
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

NPError NPP_SetWindow(NPP instance, NPWindow *npwin)
{
	InstanceData *data = instance->pdata;
	
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

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
	InstanceData *data = instance->pdata;
	
	if (!*data->exepath)
	{
		return NPERR_FILE_NOT_FOUND;
	}
	
	*stype = NP_ASFILEONLY;
	
	return NPERR_NO_ERROR;
	
	UNREFERENCED_PARAMETER(instance);
	UNREFERENCED_PARAMETER(type);
	UNREFERENCED_PARAMETER(stream);
	UNREFERENCED_PARAMETER(seekable);
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	InstanceData *data = instance->pdata;
	
	WCHAR cmdLine[MAX_PATH * 3];
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFOW si = { 0 };
	
	if (!fname)
	{
		data->message = L"Error: The PDF document couldn't be downloaded!";
		return;
	}
	
	data->filepath = malloc(MAX_PATH * sizeof(WCHAR));
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
		data->message = L"";
	}
	else
	{
		data->message = L"Error: Couldn't run SumatraPDF!";
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
	InstanceData *data = instance->pdata;
	if (data->hProcess)
	{
		TerminateProcess(data->hProcess, 99);
		CloseHandle(data->hProcess);
	}
	if (data->filepath)
	{
		DeleteFileW(data->filepath);
		free(data->filepath);
	}
	free(data);
	*save = NULL;
	
	return NPERR_NO_ERROR;
}

// TODO: NPP_Print seems to never be called

#define IDM_PRINT 403

void NP_LOADDS NPP_Print(NPP instance, NPPrint* platformPrint)
{
	InstanceData *data = instance->pdata;
	
	if (platformPrint->mode == NP_FULL)
	{
		HWND hWnd = data->npwin->window;
		HWND hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
		
		if (hChild)
		{
			PostMessage(hChild, WM_COMMAND, IDM_PRINT, 0);
			platformPrint->print.fullPrint.pluginPrinted = true;
		}
	}
}
