/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef WinUtil_h
#define WinUtil_h

#include "BaseUtil.h"
#include <WindowsX.h>
#include <CommCtrl.h>
#include "StrUtil.h"
#include "GeomUtil.h"

#ifndef USER_DEFAULT_SCREEN_DPI
// the following is only defined if _WIN32_WINNT >= 0x0600 and we use 0x0500
#define USER_DEFAULT_SCREEN_DPI 96
#endif

HMODULE SafeLoadLibrary(const TCHAR *dllName);
FARPROC LoadDllFunc(TCHAR *dllName, const char *funcName);

class ScopedCom {
public:
    ScopedCom() { CoInitialize(NULL); }
    ~ScopedCom() { CoUninitialize(); }
};

class ScopedOle {
public:
    ScopedOle() { OleInitialize(NULL); }
    ~ScopedOle() { OleUninitialize(); }
};

class ScopedHandle {
    HANDLE handle;
public:
    ScopedHandle(HANDLE handle) : handle(handle) { }
    ~ScopedHandle() { CloseHandle(handle); }
    operator HANDLE() const { return handle; }
};

class MillisecondTimer {
    LARGE_INTEGER   start;
    LARGE_INTEGER   end;
public:
    void Start() { QueryPerformanceCounter(&start); }
    void Stop() { QueryPerformanceCounter(&end); }

    double GetCurrTimeInMs()
    {
        LARGE_INTEGER curr;
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&curr);
        double timeInSecs = (double)(curr.QuadPart-start.QuadPart)/(double)freq.QuadPart;
        return timeInSecs * 1000.0;
    }

    double GetTimeInMs()
    {
        LARGE_INTEGER   freq;
        QueryPerformanceFrequency(&freq);
        double timeInSecs = (double)(end.QuadPart-start.QuadPart)/(double)freq.QuadPart;
        return timeInSecs * 1000.0;
    }
};

class ScopedGdiPlus {
protected:
    Gdiplus::GdiplusStartupInput si;
    Gdiplus::GdiplusStartupOutput so;
    ULONG_PTR token, hookToken;
    bool noBgThread;

public:
    // suppress the GDI+ background thread when initiating in WinMain,
    // as that thread causes DDE messages to be sent too early and
    // thus unexpected timeouts
    ScopedGdiPlus(bool inWinMain=false) : noBgThread(inWinMain) {
        si.SuppressBackgroundThread = noBgThread;
        Gdiplus::GdiplusStartup(&token, &si, &so);
        if (noBgThread)
            so.NotificationHook(&hookToken);
    }
    ~ScopedGdiPlus() {
        if (noBgThread)
            so.NotificationUnhook(hookToken);
        Gdiplus::GdiplusShutdown(token);
    }
};

class DoubleBuffer {
    HWND hTarget;
    HDC hdcCanvas, hdcBuffer;
    HBITMAP doubleBuffer;
    RectI rect;

public:
    DoubleBuffer(HWND hwnd, RectI rect);
    ~DoubleBuffer();

    HDC GetDC() const { return hdcBuffer ? hdcBuffer : hdcCanvas; }
    void Flush(HDC hdc);
};

inline void InitAllCommonControls()
{
    INITCOMMONCONTROLSEX cex = {0};
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);
}

inline void FillWndClassEx(WNDCLASSEX &wcex, HINSTANCE hInstance) 
{
    ZeroMemory(&wcex, sizeof(WNDCLASSEX));
    wcex.cbSize     = sizeof(WNDCLASSEX);
    wcex.style      = CS_HREDRAW | CS_VREDRAW;
    wcex.hInstance  = hInstance;
    wcex.hCursor    = LoadCursor(NULL, IDC_ARROW);
}

bool IsAppThemed();
bool WindowsVerVistaOrGreater();
bool IsRunningInWow64();

void SeeLastError(DWORD err=0);
TCHAR *ReadRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName);
bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value);
bool WriteRegDWORD(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, DWORD value);
bool CreateRegKey(HKEY keySub, const TCHAR *keyName);
bool DeleteRegKey(HKEY keySub, const TCHAR *keyName, bool resetACLFirst=false);

void EnableNx();
void RedirectIOToConsole();
TCHAR *GetExePath();
int FileTimeDiffInSecs(FILETIME& ft1, FILETIME& ft2);
TCHAR *MenuSafeString(const TCHAR *str);

TCHAR *ResolveLnk(const TCHAR *path);
bool CreateShortcut(const TCHAR *shortcutPath, const TCHAR *exePath,
                    const TCHAR *args=NULL, const TCHAR *description=NULL,
                    int iconIndex=0);
IDataObject* GetDataObjectForFile(LPCTSTR filePath, HWND hwnd=NULL);
DWORD GetFileVersion(TCHAR *path);

inline bool IsKeyPressed(int key)
{
    return GetKeyState(key) & 0x8000 ? true : false;
}
inline bool IsShiftPressed() { return IsKeyPressed(VK_SHIFT); }
inline bool IsAltPressed() { return IsKeyPressed(VK_MENU); }
inline bool IsCtrlPressed() { return IsKeyPressed(VK_CONTROL); }

namespace win {

inline size_t GetTextLen(HWND hwnd)
{
    return (size_t)SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
}

/* return a text in edit control represented by hwnd
   return NULL in case of error (couldn't allocate memory)
   caller needs to free() the text */
inline TCHAR *GetText(HWND hwnd)
{
    size_t  cchTxtLen = GetTextLen(hwnd);
    TCHAR * txt = (TCHAR*)calloc(cchTxtLen + 1, sizeof(TCHAR));
    if (NULL == txt)
        return NULL;
    SendMessage(hwnd, WM_GETTEXT, cchTxtLen + 1, (LPARAM)txt);
    txt[cchTxtLen] = 0;
    return txt;
}

inline void SetText(HWND hwnd, const TCHAR *txt)
{
    SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)txt);
}

class HdcScopedSelectFont {
    HGDIOBJ prevFont;
    HDC hdc;
public:
    HdcScopedSelectFont(HDC hdc, HFONT font) : hdc(hdc)
    {
        prevFont = SelectObject(hdc, font);
    }
    ~HdcScopedSelectFont()
    {
        SelectObject(hdc, prevFont);
    } 
};

inline int GetHwndDpi(HWND hwnd, float *uiDPIFactor)
{
    HDC dc = GetDC(hwnd);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    // round untypical resolutions up to the nearest quarter
    if (uiDPIFactor)
        *uiDPIFactor = ceil(dpi * 4.0f / USER_DEFAULT_SCREEN_DPI) / 4.0f;
    ReleaseDC(hwnd, dc);
    return dpi;
}

namespace menu {

inline void SetChecked(HMENU m, UINT id, bool isChecked)
{
    CheckMenuItem(m, id, MF_BYCOMMAND | (isChecked ? MF_CHECKED : MF_UNCHECKED));
}

inline bool SetEnabled(HMENU m, UINT id, bool isEnabled)
{
    BOOL ret = EnableMenuItem(m, id, MF_BYCOMMAND | (isEnabled ? MF_ENABLED : MF_GRAYED));
    return ret != -1;
}

inline void Remove(HMENU m, UINT id)
{
    RemoveMenu(m, id, MF_BYCOMMAND);
}

inline void Empty(HMENU m)
{
    while (RemoveMenu(m, 0, MF_BYPOSITION));
}

inline void SetText(HMENU m, UINT id, TCHAR *s)
{
    MENUITEMINFO mii = { 0 };
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STRING;
    mii.fType = MFT_STRING;
    mii.dwTypeData = s;
    mii.cch = (UINT)str::Len(s);
    SetMenuItemInfo(m, id, FALSE, &mii);
}

} // namespace Menu

namespace font {
    
HFONT GetSimple(HDC hdc, TCHAR *fontName, int fontSize);

inline void Delete(HFONT font)
{
    DeleteObject(font);
}

class ScopedFont {
    HFONT font;
public:
    ScopedFont(HDC hdc, TCHAR *fontName, int fontSize) {
        font = GetSimple(hdc, fontName, fontSize);
    }
    ~ScopedFont() {
        DeleteObject(font);
    }
    operator HFONT() const { return font; }
};

}// namespace font

} // namespace win

/* Utilities to help in common windows programming tasks */

/* constant to make it easier to return proper LRESULT values when handling
   various windows messages */
#define WM_KILLFOCUS_HANDLED 0
#define WM_SETFOCUS_HANDLED 0
#define WM_KEYDOWN_HANDLED 0
#define WM_KEYUP_HANDLED 0
#define WM_LBUTTONDOWN_HANDLED 0
#define WM_LBUTTONUP_HANDLED 0
#define WM_PAINT_HANDLED 0
#define WM_DRAWITEM_HANDLED TRUE
#define WM_MEASUREITEM_HANDLED TRUE
#define WM_SIZE_HANDLED 0
#define LVN_ITEMACTIVATE_HANDLED 0
#define WM_VKEYTOITEM_HANDLED_FULLY -2
#define WM_VKEYTOITEM_NOT_HANDLED -1
#define WM_NCPAINT_HANDLED 0
#define WM_VSCROLL_HANDLED 0
#define WM_HSCROLL_HANDLED 0
#define WM_CREATE_FAILED -1
#define WM_CREATE_OK 0

#define WIN_COL_RED     RGB(255,0,0)
#define WIN_COL_WHITE   RGB(255,255,255)
#define WIN_COL_BLACK   RGB(0,0,0)
#define WIN_COL_BLUE    RGB(0,0,255)
#define WIN_COL_GREEN   RGB(0,255,0)
#define WIN_COL_GRAY    RGB(215,215,215)

#define DRAGQUERY_NUMFILES 0xFFFFFFFF

#define Edit_SelectAll(hwnd) Edit_SetSel(hwnd, 0, -1)
#define ListBox_AppendString_NoSort(hwnd, txt) ListBox_InsertString(hwnd, -1, txt)

RectI   ShiftRectToWorkArea(RectI rect, bool bFully=false);

void    LaunchFile(const TCHAR *path, const TCHAR *params=NULL, const TCHAR *verb=NULL, bool hidden=false);
HANDLE  LaunchProcess(TCHAR *cmdLine, DWORD flags=0);

void    PaintRect(HDC hdc, RectI& rect);
void    PaintLine(HDC hdc, RectI& rect);
void    DrawCenteredText(HDC hdc, RectI& r, const TCHAR *txt, bool isRTL=false);
SIZE    TextSizeInHwnd(HWND hwnd, const TCHAR *txt);

bool    IsCursorOverWindow(HWND hwnd);
void    CenterDialog(HWND hDlg, HWND hParent=NULL);
TCHAR * GetDefaultPrinterName();
bool    CopyTextToClipboard(const TCHAR *text, bool appendOnly=false);
void    ToggleWindowStyle(HWND hwnd, DWORD flag, bool enable, int type=GWL_STYLE);

IStream*CreateStreamFromData(void *data, size_t len);
HRESULT GetDataFromStream(IStream *stream, void **data, size_t *len);

#endif
