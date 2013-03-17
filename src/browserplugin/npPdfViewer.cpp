/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"

#include "BencUtil.h"
#include "CmdLineParser.h"
#include "FileUtil.h"
#include "WinUtil.h"
#ifndef _WINDOWS
#define _WINDOWS
#endif
#include "npapi/npfunctions.h"

#include "DebugLog.h"

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

#if NOLOG == 0
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
#endif

NPNetscapeFuncs gNPNFuncs;
HINSTANCE g_hInstance = NULL;
#ifndef _WIN64
const WCHAR *g_lpRegKey = L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin";
#else
const WCHAR *g_lpRegKey = L"Software\\MozillaPlugins\\@mozilla.zeniko.ch/SumatraPDF_Browser_Plugin_x64";
#endif
int gTranslationIdx = 0;

/* ::::: DLL Exports ::::: */

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    plogf("sp: DllMain() reason: %d (%s)", dwReason, DllMainReason(dwReason));

    g_hInstance = hInstance;
    return TRUE;
}

DLLEXPORT NPError WINAPI NP_GetEntryPoints(NPPluginFuncs *pFuncs)
{
    plogf("sp: NP_GetEntryPoints()");
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
    plogf("sp: NP_Initialize()");

    if (!pFuncs || pFuncs->size < sizeof(NPNetscapeFuncs))
    {
        plogf("sp: NP_Initialize() error: NPERR_INVALID_FUNCTABLE_ERROR");
        return NPERR_INVALID_FUNCTABLE_ERROR;
    }
    if (HIBYTE(pFuncs->version) > NP_VERSION_MAJOR)
    {
        plogf("sp: NP_Initialize() error: NPERR_INCOMPATIBLE_VERSION_ERROR");
        return NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    
    gNPNFuncs = *pFuncs;
    
    return NPERR_NO_ERROR;
}

DLLEXPORT NPError WINAPI NP_Shutdown(void)
{
    plogf("sp: NP_Shutdown()");
    return NPERR_NO_ERROR;
}

bool EnsureRegKey(const WCHAR *lpKey)
{
    CreateRegKey(HKEY_LOCAL_MACHINE, lpKey);
    return CreateRegKey(HKEY_CURRENT_USER, lpKey);
}

bool SetRegValue(const WCHAR *lpKey, const WCHAR *lpName, const WCHAR *lpValue)
{
    WriteRegStr(HKEY_LOCAL_MACHINE, lpKey, lpName, lpValue);
    return WriteRegStr(HKEY_CURRENT_USER, lpKey, lpName, lpValue);
}

DLLEXPORT STDAPI DllRegisterServer(VOID)
{
    if (!EnsureRegKey(g_lpRegKey))
        return E_UNEXPECTED;
    
    WCHAR szPath[MAX_PATH];
    GetModuleFileName(g_hInstance, szPath, MAX_PATH);
    if (!SetRegValue(g_lpRegKey, L"Description", L"SumatraPDF Browser Plugin") ||
        !SetRegValue(g_lpRegKey, L"Path", szPath) ||
        !SetRegValue(g_lpRegKey, L"Version", L"0") ||
        !SetRegValue(g_lpRegKey, L"ProductName", L"SumatraPDF Browser Plugin"))
    {
        return E_UNEXPECTED;
    }
    
    ScopedMem<WCHAR> mimeType(str::Join(g_lpRegKey, L"\\MimeTypes\\application/pdf"));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, L"\\MimeTypes\\application/vnd.ms-xpsdocument"));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, L"\\MimeTypes\\application/oxps"));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, L"\\MimeTypes\\image/vnd.djvu"));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, L"\\MimeTypes\\image/x-djvu"));
    EnsureRegKey(mimeType);
    mimeType.Set(str::Join(g_lpRegKey, L"\\MimeTypes\\image/x.djvu"));
    EnsureRegKey(mimeType);
    
    // Work around Mozilla bug https://bugzilla.mozilla.org/show_bug.cgi?id=581848 which
    // makes Firefox up to version 3.6.* ignore all but the first plugin for a given MIME type
    // (per http://code.google.com/p/sumatrapdf/issues/detail?id=1254#c12 Foxit does the same)
    *(WCHAR *)path::GetBaseName(szPath) = '\0';
    if (SHGetValue(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH", NULL, NULL, NULL) == ERROR_FILE_NOT_FOUND)
    {
        WriteRegStr(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH", szPath);
        SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, NULL);
    }
    
    return S_OK;
}

DLLEXPORT STDAPI DllUnregisterServer(VOID)
{
    ScopedMem<WCHAR> mozPluginPath(ReadRegStr(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH"));
    if (mozPluginPath)
    {
        WCHAR szModulePath[MAX_PATH];
        GetModuleFileName(g_hInstance, szModulePath, MAX_PATH);
        if (str::StartsWithI(szModulePath, mozPluginPath))
            SHDeleteValue(HKEY_CURRENT_USER, L"Environment", L"MOZ_PLUGIN_PATH");
    }
    
    DeleteRegKey(HKEY_LOCAL_MACHINE, g_lpRegKey);
    if (!DeleteRegKey(HKEY_CURRENT_USER, g_lpRegKey))
        return E_UNEXPECTED;
    
    return S_OK;
}

/* ::::: Auxiliary Methods ::::: */

bool GetExePath(WCHAR *lpPath, size_t len)
{
    // Search the plugin's directory first
    GetModuleFileName(g_hInstance, lpPath, len - 2);
    str::BufSet((WCHAR *)path::GetBaseName(lpPath), len - 2 - (path::GetBaseName(lpPath) - lpPath), L"SumatraPDF.exe");
    if (file::Exists(lpPath))
        return true;
    
    *lpPath = '\0';
    // Try to get the path from the registry (set e.g. when making the default PDF viewer)
    ScopedMem<WCHAR> path(ReadRegStr(HKEY_CURRENT_USER, L"Software\\Classes\\SumatraPDF\\Shell\\Open\\Command", NULL));
    if (!path)
        return false;

    WStrVec args;
    ParseCmdLine(path, args);
    if (!file::Exists(args.At(0)))
        return false;

    str::BufSet(lpPath, len, args.At(0));
    return true;
}

HANDLE CreateTempFile(WCHAR *filePathBufOut, size_t bufSize)
{
    ScopedMem<WCHAR> tmpPath(path::GetTempPath(L"nPV"));
    if (!tmpPath)
    {
        plogf("sp: CreateTempFile(): GetTempPath() failed");
        return NULL;
    }
    str::BufSet(filePathBufOut, bufSize, tmpPath);

    HANDLE hFile = CreateFile(filePathBufOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        plogf("sp: CreateTempFile(): CreateFile() failed");
        return NULL;
    }
    return hFile;
}

#include "Trans_browserplugin_txt.cpp"

void SelectTranslation(const WCHAR *exePath=NULL)
{
    LANGID langId = GetUserDefaultUILanguage();
    int idx = GetLanguageIndex(langId);
    if (-1 == idx) {
        // try a neutral language if the specific sublanguage isn't available
        langId = MAKELANGID(PRIMARYLANGID(langId), SUBLANG_NEUTRAL);
        idx = GetLanguageIndex(langId);
    }
    if (-1 != idx) {
        gTranslationIdx = idx;
        plogf("sp: Detected language %s (%d)", gLanguages[idx / gTranslationsCount], idx);
    }

    // try to extract the language used by SumatraPDF
    ScopedMem<WCHAR> path;
    if (exePath) {
        path.Set(path::GetDir(exePath));
        path.Set(path::Join(path, L"sumatrapdfprefs.dat"));
    }
    if (!file::Exists(path)) {
        path.Set(GetSpecialFolder(CSIDL_APPDATA));
        path.Set(path::Join(path, L"SumatraPDF\\sumatrapdfprefs.dat"));
    }
    if (!file::Exists(path))
        return;
    plogf("sp: Found preferences at %S", path);
    ScopedMem<char> data(file::ReadAll(path, NULL));
    if (data) {
        BencObj *root = BencObj::Decode(data);
        if (root && root->Type() == BT_DICT) {
            BencDict *global = static_cast<BencDict *>(root)->GetDict("gp");
            BencString *string = global ? global->GetString("UILanguage") : NULL;
            if (string) {
                plogf("sp: UILanguage from preferences: %s", string->RawValue());
                for (int i = 0; gLanguages[i]; i++) {
                    if (str::Eq(gLanguages[i], string->RawValue())) {
                        gTranslationIdx = i * gTranslationsCount;
                        break;
                    }
                }
            }
        }
        delete root;
    }
}

int cmpWcharPtrs(const void *a, const void *b)
{
    return wcscmp(*(const WCHAR **)a, *(const WCHAR **)b);
}

const WCHAR *Translate(const WCHAR *s)
{
    const WCHAR **res = (const WCHAR **)bsearch(&s, gTranslations, gTranslationsCount, sizeof(s), cmpWcharPtrs);
    int idx = gTranslationIdx + res - gTranslations;
    return res && gTranslations[idx] ? gTranslations[idx] : s;
}

#define _TR(x) Translate(TEXT(x))

/* ::::: Plugin Window Procedure ::::: */

struct InstanceData {
    NPWindow *  npwin;
    LPCWSTR     message;
    WCHAR       filepath[MAX_PATH];
    HANDLE      hFile;
    HANDLE      hProcess;
    WCHAR       exepath[MAX_PATH];
    float       progress, prevProgress;
    uint32_t    totalSize, currSize;
};

#define COL_WINDOW_BG RGB(0xcc, 0xcc, 0xcc)

enum Magnitudes { KB = 1024, MB = 1024 * KB, GB = 1024 * MB };

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// Caller needs to free the result.
WCHAR *FormatSizeSuccint(size_t size) {
    const WCHAR *unit = NULL;
    double s = (double)size;

    if (size > GB)
    {
        s /= GB;
        unit = _TR("GB");
    }
    else if (size > MB)
    {
        s /= MB;
        unit = _TR("MB");
    }
    else
    {
        s /= KB;
        unit = _TR("KB");
    }
    
    ScopedMem<WCHAR> sizestr(str::FormatFloatWithThousandSep(s));
    if (!unit)
        return sizestr.StealData();
    
    return str::Format(L"%s %s", sizestr, unit);
}

LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    NPP instance = (NPP)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    if (uiMsg == WM_PAINT)
    {
        InstanceData *data = (InstanceData *)instance->pdata;
        
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hWnd, &ps);
        HBRUSH brushBg = CreateSolidBrush(COL_WINDOW_BG);
        HFONT hFont = GetSimpleFont(hDC, L"MS Shell Dlg", 14);
        bool isRtL = IsLanguageRtL(gTranslationIdx);
        
        // set up double buffering
        RectI rcClient = ClientRect(hWnd);
        DoubleBuffer buffer(hWnd, rcClient);
        HDC hDCBuffer = buffer.GetDC();
        
        // display message centered in the window
        FillRect(hDCBuffer, &rcClient.ToRECT(), brushBg);
        hFont = (HFONT)SelectObject(hDCBuffer, hFont);
        SetTextColor(hDCBuffer, RGB(0, 0, 0));
        SetBkMode(hDCBuffer, TRANSPARENT);
        DrawCenteredText(hDCBuffer, rcClient, data->message, isRtL);
        
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
            
            ScopedMem<WCHAR> currSize(FormatSizeSuccint(data->currSize));
            if (0 == data->totalSize || data->currSize > data->totalSize)
            {
                // total size unknown or bogus => show just the current size
                DrawCenteredText(hDCBuffer, rcProgressAll, currSize, isRtL);
            }
            else
            {
                ScopedMem<WCHAR> totalSize(FormatSizeSuccint(data->totalSize));
                ScopedMem<WCHAR> s(str::Format(_TR("%s of %s"), currSize, totalSize));
                DrawCenteredText(hDCBuffer, rcProgressAll, s, isRtL);
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
    else if (uiMsg == WM_COPYDATA)
    {
        COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lParam;
        if (cds && 0x4C5255 /* URL */ == cds->dwData)
        {
            plogf("sp: NPN_GetURL %s", cds->dwData, (const char *)cds->lpData);
            gNPNFuncs.geturl(instance, (const char *)cds->lpData, "_blank");
            return TRUE;
        }
    }
    
    return DefWindowProc(hWnd, uiMsg, wParam, lParam);
}

/* ::::: Plugin Methods ::::: */

NPError NP_LOADDS NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
    plogf("sp: NPP_New() mode=%d ", mode);

    if (!instance)
    {
        plogf("sp: error: NPERR_INVALID_INSTANCE_ERROR");
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    if (pluginType)
        plogf("sp:   pluginType: %s ", pluginType);
    if (saved)
        plogf("sp:   SavedData: len=%d", saved->len);

    instance->pdata = AllocStruct<InstanceData>();
    if (!instance->pdata)
    {
        plogf("sp: error: NPERR_OUT_OF_MEMORY_ERROR");
        return NPERR_OUT_OF_MEMORY_ERROR;
    }

    gNPNFuncs.setvalue(instance, NPPVpluginWindowBool, (void *)true);
    
    InstanceData *data = (InstanceData *)instance->pdata;
    bool ok = GetExePath(data->exepath, dimof(data->exepath));
    SelectTranslation(ok ? data->exepath : NULL);
    if (ok)
        data->message = _TR("Opening document in SumatraPDF...");
    else
        data->message = _TR("Error: SumatraPDF hasn't been found!");
    
    return NPERR_NO_ERROR;
}

NPError NP_LOADDS NPP_SetWindow(NPP instance, NPWindow *npwin)
{
    if (!instance)
    {
        plogf("sp: NPP_SetWindow() errro: NPERR_INVALID_INSTANCE_ERROR");
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    plogf("sp: NPP_SetWindow()");

    InstanceData *data = (InstanceData *)instance->pdata;
    if (!npwin)
    {
        data->npwin = NULL;
    }
    else if (data->npwin != npwin)
    {
        HWND hWnd = (HWND)npwin->window;
        
        data->npwin = npwin;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)instance);
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

void TriggerRepaintOnProgressChange(InstanceData *data)
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
        plogf("sp: NPP_NewStream() error: NPERR_FILE_NOT_FOUND");
        return NPERR_FILE_NOT_FOUND;
    }

    plogf("sp: NPP_NewStream() end=%d", stream->end);

    // if we can create a temporary file ourselfes, we manage the download
    // process. The reason for that is that NP_ASFILE (where browser manages
    // file downloading) is not reliable and has been broken in almost every
    // browser at some point

    *stype = NP_ASFILE;
    data->hFile = CreateTempFile(data->filepath, dimof(data->filepath));
    if (data->hFile)
    {
        plogf("sp: using temporary file: %S", data->filepath);
        *stype = NP_NORMAL;
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
    plogf("sp: NPP_WriteReady() res=%d", res);
    return res;
}

int32_t NP_LOADDS NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
    InstanceData *data = (InstanceData *)instance->pdata;
    DWORD bytesWritten = len;

    plogf("sp: NPP_Write() off=%d, len=%d", offset, len);

    if (data->hFile)
    {
        // Note: we optimistically assume that data comes in sequentially
        // (i.e. next offset will be current offset + bytesWritten)
        BOOL ok = WriteFile(data->hFile, buffer, (DWORD)len, &bytesWritten, NULL);
        if (!ok)
        {
            plogf("sp: NPP_Write() failed to write %d bytes at offset %d", len, offset);
            return -1;
        }
    }

    data->currSize = offset + bytesWritten;
    data->progress = stream->end > 0 ? 1.0f * (offset + len) / stream->end : 0;
    TriggerRepaintOnProgressChange(data);

    return bytesWritten;
}

void LaunchWithSumatra(InstanceData *data, const char *url_utf8)
{
    if (!file::Exists(data->filepath))
        plogf("sp: NPP_StreamAsFile() error: file doesn't exist");

    ScopedMem<WCHAR> url(str::conv::FromUtf8(url_utf8));
    // escape quotation marks and backslashes for CmdLineParser.cpp's ParseQuoted
    if (str::FindChar(url, '"')) {
        WStrVec parts;
        parts.Split(url, L"\"");
        url.Set(parts.Join(L"%22"));
    }
    if (str::EndsWith(url, L"\\")) {
        url[str::Len(url) - 1] = '\0';
        url.Set(str::Join(url, L"%5c"));
    }

    ScopedMem<WCHAR> cmdLine(str::Format(L"\"%s\" -plugin \"%s\" %d \"%s\"",
        data->exepath, url ? url : L"", (HWND)data->npwin->window, data->filepath));
    data->hProcess = LaunchProcess(cmdLine);
    if (!data->hProcess)
    {
        plogf("sp: NPP_StreamAsFile() error: couldn't run SumatraPDF!");
        data->message = _TR("Error: Couldn't run SumatraPDF!");
    }
}

void NP_LOADDS NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
    InstanceData *data = (InstanceData *)instance->pdata;

    if (!fname)
    {
        plogf("sp: NPP_StreamAsFile() error: fname is NULL");
        data->message = _TR("Error: The document couldn't be downloaded!");
        goto Exit;
    }

    plogf("sp: NPP_StreamAsFile() fname=%s", fname);

    if (data->hFile)
        plogf("sp: NPP_StreamAsFile() error: data->hFile is != NULL (should be NULL)");

    data->progress = 1.0f;
    data->prevProgress = 0.0f; // force update
    TriggerRepaintOnProgressChange(data);

    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, fname, -1, data->filepath, MAX_PATH))
        MultiByteToWideChar(CP_ACP, 0, fname, -1, data->filepath, MAX_PATH);

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

    plogf("sp: NPP_DestroyStream() reason: %d", reason);
    if (stream)
    {
        if (stream->url)
            plogf("sp:   url: %s", stream->url);
        plogf("sp:   end: %d", stream->end);
    }

    if (!instance)
    {
        plogf("sp: NPP_DestroyStream() error: NPERR_INVALID_INSTANCE_ERROR");
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    data = (InstanceData *)instance->pdata;
    if (!data)
    {
        plogf("sp: NPP_DestroyStream() error: instance->pdata is NULL");
        return NPERR_NO_ERROR;
    }

    if (!data->hFile)
        goto Exit;

    CloseHandle(data->hFile);
    if (stream)
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
    if (!instance)
    {
        plogf("sp: NPP_Destroy() error: NPERR_INVALID_INSTANCE_ERROR");
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    plogf("sp: NPP_Destroy()");
    InstanceData *data = (InstanceData *)instance->pdata;
    if (data->hProcess)
    {
        plogf("sp: NPP_Destroy(): waiting for Sumatra to exit");
        TerminateProcess(data->hProcess, 99);
        WaitForSingleObject(data->hProcess, INFINITE);
        CloseHandle(data->hProcess);
    }
    if (data->hFile)
    {
        plogf("sp: NPP_Destroy(): deleting internal temporary file %S", data->filepath);
        DeleteFile(data->filepath);
        *data->filepath = '\0';
    }

    if (*data->filepath)
    {
        WCHAR tempDir[MAX_PATH];
        DWORD len = GetTempPath(MAX_PATH, tempDir);
        if (0 < len && len < MAX_PATH && str::StartsWithI(data->filepath, tempDir))
        {
            plogf("sp: NPP_Destroy(): deleting browser temporary file %S", data->filepath);
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
        plogf("sp: NPP_Print(), platformPrint is NULL");
        return;
    }

    if (NP_FULL != platformPrint->mode)
    {
        plogf("sp: NPP_Print(), platformPrint->mode is %d (!= NP_FULL)", platformPrint->mode);
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
