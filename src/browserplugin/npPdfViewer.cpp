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
#ifdef _WIN64
#pragma comment(linker, "/EXPORT:NP_GetEntryPoints=NP_GetEntryPoints,PRIVATE")
#pragma comment(linker, "/EXPORT:NP_Initialize=NP_Initialize,PRIVATE")
#pragma comment(linker, "/EXPORT:NP_Shutdown=NP_Shutdown,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=DllRegisterServer,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=DllUnregisterServer,PRIVATE")
#else
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
const TCHAR *g_lpRegKey = _T("Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin");


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
        return NPERR_INVALID_FUNCTABLE_ERROR;
    
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

bool EnsureRegKey(LPCTSTR lpKey)
{
    CreateRegKey(HKEY_LOCAL_MACHINE, lpKey);
    return CreateRegKey(HKEY_CURRENT_USER, lpKey);
}

bool SetRegValue(LPCTSTR lpKey, LPCTSTR lpName, LPCTSTR lpValue)
{
    WriteRegStr(HKEY_LOCAL_MACHINE, lpKey, lpName, lpValue);
    return WriteRegStr(HKEY_CURRENT_USER, lpKey, lpName, lpValue);
}

DLLEXPORT STDAPI DllRegisterServer(VOID)
{
    if (!EnsureRegKey(g_lpRegKey))
        return E_UNEXPECTED;
    
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(g_hInstance, szPath, MAX_PATH);
    if (!SetRegValue(g_lpRegKey, _T("Description"), _T("SumatraPDF Browser Plugin")) ||
        !SetRegValue(g_lpRegKey, _T("Path"), szPath) ||
        !SetRegValue(g_lpRegKey, _T("Version"), _T("0")) ||
        !SetRegValue(g_lpRegKey, _T("ProductName"), _T("SumatraPDF Browser Plugin")))
    {
        return E_UNEXPECTED;
    }
    
    ScopedMem<TCHAR> mimeType(str::Join(g_lpRegKey, _T("\\MimeTypes\\application/pdf")));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, _T("\\MimeTypes\\application/vnd.ms-xpsdocument")));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, _T("\\MimeTypes\\application/oxps")));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, _T("\\MimeTypes\\image/vnd.djvu")));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, _T("\\MimeTypes\\image/x-djvu")));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, _T("\\MimeTypes\\image/x.djvu")));
    EnsureRegKey(mimeType);
    
    // Work around Mozilla bug https://bugzilla.mozilla.org/show_bug.cgi?id=581848 which
    // makes Firefox up to version 3.6.* ignore all but the first plugin for a given MIME type
    // (per http://code.google.com/p/sumatrapdf/issues/detail?id=1254#c12 Foxit does the same)
    *(TCHAR *)path::GetBaseName(szPath) = '\0';
    if (SHGetValue(HKEY_CURRENT_USER, _T("Environment"), _T("MOZ_PLUGIN_PATH"), NULL, NULL, NULL) == ERROR_FILE_NOT_FOUND)
    {
        WriteRegStr(HKEY_CURRENT_USER, _T("Environment"), _T("MOZ_PLUGIN_PATH"), szPath);
        SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)_T("Environment"), SMTO_ABORTIFHUNG, 5000, NULL);
    }
    
    return S_OK;
}

DLLEXPORT STDAPI DllUnregisterServer(VOID)
{
    ScopedMem<TCHAR> mozPluginPath(ReadRegStr(HKEY_CURRENT_USER, _T("Environment"), _T("MOZ_PLUGIN_PATH")));
    if (mozPluginPath)
    {
        TCHAR szModulePath[MAX_PATH];
        GetModuleFileName(g_hInstance, szModulePath, MAX_PATH);
        if (str::StartsWithI(szModulePath, mozPluginPath))
            SHDeleteValue(HKEY_CURRENT_USER, _T("Environment"), _T("MOZ_PLUGIN_PATH"));
    }
    
    DeleteRegKey(HKEY_LOCAL_MACHINE, g_lpRegKey);
    if (!DeleteRegKey(HKEY_CURRENT_USER, g_lpRegKey))
        return E_UNEXPECTED;
    
    return S_OK;
}

/* ::::: Auxiliary Methods ::::: */

bool GetExePath(LPTSTR lpPath, int len)
{
    // Search the plugin's directory first
    GetModuleFileName(g_hInstance, lpPath, len - 2);
    str::BufSet((TCHAR *)path::GetBaseName(lpPath), len - 2 - (path::GetBaseName(lpPath) - lpPath), _T("SumatraPDF.exe"));
    if (file::Exists(lpPath))
        return true;
    
    *lpPath = '\0';
    // Try to get the path from the registry (set e.g. when making the default PDF viewer)
    ScopedMem<TCHAR> path(ReadRegStr(HKEY_CURRENT_USER, _T("Software\\Classes\\SumatraPDF\\Shell\\Open\\Command"), NULL));
    if (!path)
        return false;

    CmdLineParser args(path);
    if (!file::Exists(args.At(0)))
        return false;

    str::BufSet(lpPath, len, args.At(0));
    return true;
}

// filePathBuf must be MAX_PATH in size
HANDLE CreateTempFile(TCHAR *filePathBufOut)
{
    TCHAR pathBuf[MAX_PATH];
    DWORD ret = GetTempPath(dimof(pathBuf), pathBuf);
    if (0 == ret || ret > dimof(pathBuf))
    {
        dbg("sp: CreateTempFile(): GetTempPath() failed");
        return NULL;
    }

    UINT uret = GetTempFileName(pathBuf, _T("SPT"), 0, filePathBufOut);
    if (0 == uret)
    {
        dbg("sp: CreateTempFile(): GetTempFileName() failed");
        return NULL;
    }

    HANDLE hFile = CreateFile(filePathBufOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        dbg("sp: CreateTempFile(): CreateFile() failed");
        return NULL;
    }
    return hFile;
}

/* ::::: Plugin Window Procedure ::::: */

struct InstanceData {
    NPWindow *  npwin;
    LPCTSTR     message;
    TCHAR       filepath[MAX_PATH];
    HANDLE      hFile;
    HANDLE      hProcess;
    TCHAR       exepath[MAX_PATH];
    float       progress, prevProgress;
    uint32_t    totalSize, currSize;
};

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
    
    ScopedMem<TCHAR> sizestr(str::FormatFloatWithThousandSep(s));
    if (!unit)
        return sizestr.StealData();
    
    return str::Format(_T("%s %s"), sizestr, unit);
}

LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    InstanceData *data = (InstanceData *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    if (uiMsg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hWnd, &ps);
        HBRUSH brushBg = CreateSolidBrush(COL_WINDOW_BG);
        HFONT hFont = GetSimpleFont(hDC, _T("MS Shell Dlg"), 14);
        
        // set up double buffering
        RectI rcClient = ClientRect(hWnd);
        DoubleBuffer buffer(hWnd, rcClient);
        HDC hDCBuffer = buffer.GetDC();
        
        // display message centered in the window
        FillRect(hDCBuffer, &rcClient.ToRECT(), brushBg);
        hFont = (HFONT)SelectObject(hDCBuffer, hFont);
        SetTextColor(hDCBuffer, RGB(0, 0, 0));
        SetBkMode(hDCBuffer, TRANSPARENT);
        DrawCenteredText(hDCBuffer, rcClient, data->message);
        
        // draw a progress bar, if a download is in progress
        if (0 < data->progress && data->progress <= 1)
        {
            SIZE msgSize;
            RectI rcProgress = rcClient;
            
            HBRUSH brushProgress = CreateSolidBrush(RGB(0x80, 0x80, 0xff));
            GetTextExtentPoint32(hDCBuffer, data->message, (int)str::Len(data->message), &msgSize);
            rcProgress.Inflate(-(rcProgress.dx - msgSize.cx) / 2, -(rcProgress.dy - msgSize.cy) / 2 + 2);
            rcProgress.Offset(0, msgSize.cy + 4 + 2);
            FillRect(hDCBuffer, &rcProgress.ToRECT(), GetStockBrush(WHITE_BRUSH));
            RectI rcProgressAll = rcProgress;
            rcProgress.dx = (int)(data->progress * rcProgress.dx);
            FillRect(hDCBuffer, &rcProgress.ToRECT(), brushProgress);
            DeleteObject(brushProgress);
            
            ScopedMem<TCHAR> currSize(FormatSizeSuccint(data->currSize));
            if (0 == data->totalSize || data->currSize > data->totalSize)
            {
                // total size unknown or bogus => show just the current size
                DrawCenteredText(hDCBuffer, rcProgressAll, currSize);
            }
            else
            {
                ScopedMem<TCHAR> totalSize(FormatSizeSuccint(data->totalSize));
                ScopedMem<TCHAR> s(str::Format(_T("%s of %s"), currSize, totalSize));
                DrawCenteredText(hDCBuffer, rcProgressAll, s);
            }
        }
        
        // draw the buffer on screen
        buffer.Flush(hDC);
        
        DeleteObject(SelectObject(hDCBuffer, hFont));
        DeleteObject(brushBg);
        EndPaint(hWnd, &ps);
        
        HWND hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
        if (hChild)
            InvalidateRect(hChild, NULL, FALSE);
    }
    else if (uiMsg == WM_SIZE)
    {
        HWND hChild = FindWindowEx(hWnd, NULL, NULL, NULL);
        if (hChild)
        {
            ClientRect rcClient(hWnd);
            MoveWindow(hChild, rcClient.x, rcClient.y, rcClient.dx, rcClient.dy, FALSE);
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
    
    if (GetExePath(data->exepath, dimof(data->exepath)))
        data->message = _T("Opening document in SumatraPDF...");
    else
        data->message = _T("Error: SumatraPDF hasn't been found!");
    
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
            ClientRect rcClient(hWnd);
            MoveWindow(hChild, rcClient.x, rcClient.y, rcClient.dx, rcClient.dy, FALSE);
        }
    }
    
    return NPERR_NO_ERROR;
}

static void TriggerRepaintOnProgressChange(InstanceData *data)
{
    if (!data || !data->npwin || !data->npwin->window)
        return;

    float diff = data->progress - data->prevProgress;
    if (diff < 0 || diff > 0.01f)
    {
        HWND hwnd = (HWND)data->npwin->window;
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
        data->prevProgress = data->progress;
    }
}

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

    // Firefox has a bug https://bugzilla.mozilla.org/show_bug.cgi?id=644149
    // where it's internal caching doesn't work for large files and is flaky
    // for small files. As a workaround, we do file downloading ourselves.
    // cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1328
    // and http://fofou.appspot.com/sumatrapdf/topic?id=2067366&comments=16
    // Opera disabled the cache for private tabs, so we have to do the caching ourselves, too
    const char *userAgent = gNPNFuncs.uagent(instance);
    if (str::Find(userAgent, "Gecko/") || str::StartsWith(userAgent, "Opera/"))
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
    TriggerRepaintOnProgressChange(data);
    
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
    TriggerRepaintOnProgressChange(data);

    return bytesWritten;
}

static void LaunchWithSumatra(InstanceData *data, const char *url_utf8)
{
    if (!file::Exists(data->filepath))
        dbg("sp: NPP_StreamAsFile() error: file doesn't exist");

    ScopedMem<TCHAR> url(str::conv::FromUtf8(url_utf8));
    ScopedMem<TCHAR> cmdLine(str::Format(_T("\"%s\" -plugin \"%s\" %d \"%s\""),
        data->exepath, url ? url : _T(""), (HWND)data->npwin->window, data->filepath));

    data->hProcess = LaunchProcess(cmdLine);
    if (!data->hProcess)
    {
        dbg("sp: NPP_StreamAsFile() error: couldn't run SumatraPDF!");
        data->message = _T("Error: Couldn't run SumatraPDF!");
    }
}

void NP_LOADDS NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
    InstanceData *data = (InstanceData *)instance->pdata;

    if (!fname)
    {
        dbg("sp: NPP_StreamAsFile() error: fname is NULL");
        data->message = _T("Error: The document couldn't be downloaded!");
        goto Exit;
    }

    dbg("sp: NPP_StreamAsFile() fname=%s", ScopedMem<TCHAR>(str::conv::FromAnsi(fname)));

    if (data->hFile)
    {
        dbg("sp: NPP_StreamAsFile() error: data->hFile is != NULL (should be NULL)");
    }

    data->progress = 1.0f;
    data->prevProgress = 0.0f; // force update
    TriggerRepaintOnProgressChange(data);

#ifdef UNICODE
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fname, -1, data->filepath, MAX_PATH))
        MultiByteToWideChar(CP_ACP, 0, fname, -1, data->filepath, MAX_PATH);
#else
    str::BufSet(data->filepath, dimof(data->filepath), fname);
#endif

    LaunchWithSumatra(data, stream->url);

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
    LaunchWithSumatra(data, stream->url);

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
        dbg("sp: NPP_Destroy(): deleting internal temporary file %s", data->filepath);
        DeleteFile(data->filepath);
        *data->filepath = '\0';
    }

    if (*data->filepath)
    {
        TCHAR tempDir[MAX_PATH];
        DWORD len = GetTempPath(MAX_PATH, tempDir);
        if (0 < len && len < MAX_PATH && str::StartsWithI(data->filepath, tempDir))
        {
            dbg("sp: NPP_Destroy(): deleting browser temporary file %s", data->filepath);
            DeleteFile(data->filepath);
        }
    }
    free(data);
    
    return NPERR_NO_ERROR;
}

// Note: NPP_Print is never called by Google Chrome or Firefox 4
//       cf. http://code.google.com/p/chromium/issues/detail?id=83341
//       and https://bugzilla.mozilla.org/show_bug.cgi?id=638796

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
