/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: Simplified BSD (see ./COPYING) */

#ifndef WinUtil_h
#define WinUtil_h

#include "BaseUtil.h"
#include <WindowsX.h>
#include <CommCtrl.h>
#include "GeomUtil.h"

class WinLibrary {
public:
    WinLibrary(const TCHAR *libName, bool dontFree=false) : dontFree(dontFree) {
        hlib = LoadSystemLibrary(libName);
    }
    ~WinLibrary() { if (!dontFree) FreeLibrary(hlib); }
    FARPROC GetProcAddr(const char *procName) {
        if (!hlib) return NULL;
        return GetProcAddress(hlib, procName);
    }
private:
    bool    dontFree;
    HMODULE hlib;
    HMODULE LoadSystemLibrary(const TCHAR *libName);
};

class ScopedCom {
public:
    ScopedCom() { CoInitialize(NULL); }
    ~ScopedCom() { CoUninitialize(); }
};

class MillisecondTimer {
    LARGE_INTEGER   start;
    LARGE_INTEGER   end;
public:
    void Start() { QueryPerformanceCounter(&start); }
    void Stop() { QueryPerformanceCounter(&end); }

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
    ULONG_PTR           token;
public:
    ScopedGdiPlus()  { Gdiplus::GdiplusStartup(&token, &si, NULL); }
    ~ScopedGdiPlus() { Gdiplus::GdiplusShutdown(token); }
};

class ClientRect : public RectI {
public:
    ClientRect(HWND hwnd) {
        RECT rc;
        if (GetClientRect(hwnd, &rc)) {
            x = rc.left; dx = rc.right - rc.left;
            y = rc.top; dy = rc.bottom - rc.top;
        }
    }
};

class WindowRect : public RectI {
public:
    WindowRect(HWND hwnd) {
        RECT rc;
        if (GetWindowRect(hwnd, &rc)) {
            x = rc.left; dx = rc.right - rc.left;
            y = rc.top; dy = rc.bottom - rc.top;
        }
    }
};

static inline void InitAllCommonControls()
{
    INITCOMMONCONTROLSEX cex = {0};
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES ;
    InitCommonControlsEx(&cex);
}

static inline void FillWndClassEx(WNDCLASSEX &wcex, HINSTANCE hInstance) 
{
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.hIconSm        = 0;
}

static inline int RectDx(const RECT *r)
{
    return r->right - r->left;
}

static inline int RectDy(const RECT *r)
{
    return r->bottom - r->top;
}

bool IsAppThemed();
bool WindowsVerVistaOrGreater();

void SeeLastError(DWORD err=0);
TCHAR *ReadRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName);
bool WriteRegStr(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, const TCHAR *value);
bool WriteRegDWORD(HKEY keySub, const TCHAR *keyName, const TCHAR *valName, DWORD value);
bool DeleteRegKey(HKEY keySub, const TCHAR *keyName, bool resetACLFirst=false);

void EnableNx();
void RedirectIOToConsole();
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

namespace Win {

inline int GetTextLen(HWND hwnd)
{
    return (int)SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
}

/* return a text in edit control represented by hwnd
   return NULL in case of error (couldn't allocate memory)
   caller needs to free() the text */
inline TCHAR *GetText(HWND hwnd)
{
    int     cchTxtLen = GetTextLen(hwnd);
    TCHAR * txt = (TCHAR*)calloc((size_t)cchTxtLen + 1, sizeof(TCHAR));
    if (NULL == txt)
        return NULL;
    SendMessage(hwnd, WM_GETTEXT, cchTxtLen + 1, (LPARAM)txt);
    txt[cchTxtLen] = 0;
    return txt;
}

inline void SetText(HWND hwnd, const TCHAR *txt)
{
    SendMessage(hwnd, WM_SETTEXT, (WPARAM)0, (LPARAM)txt);
}

inline void SetFont(HWND hwnd, HFONT font)
{
	SetWindowFont(hwnd, font, TRUE);
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

namespace Menu {

inline void Check(HMENU m, UINT id, bool check)
{
    CheckMenuItem(m, id, MF_BYCOMMAND | (check ? MF_CHECKED : MF_UNCHECKED));
}

inline void Enable(HMENU m, UINT id, bool enable)
{
    EnableMenuItem(m, id, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
}

inline void Hide(HMENU m, UINT id)
{
    RemoveMenu(m, id, MF_BYCOMMAND);
}

inline void Empty(HMENU m)
{
    while (RemoveMenu(m, 0, MF_BYPOSITION));
}

} // namespace Menu

namespace Font {
    
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

}// namespace Font

} // namespace Win

// used to be in win_util.h

#include <commctrl.h>
#include <windowsx.h>

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

int     screen_get_dx();
int     screen_get_dy();
int     screen_get_menu_dy();
int     screen_get_caption_dy();
void    rect_shift_to_work_area(RECT *rect, bool bFully);

void    launch_url(const TCHAR *url);
void    exec_with_params(const TCHAR *exe, const TCHAR *params, bool hidden);

void    paint_round_rect_around_hwnd(HDC hdc, HWND hwnd_edit_parent, HWND hwnd_edit, COLORREF col);
void    paint_rect(HDC hdc, RECT * rect);
void    DrawCenteredText(HDC hdc, RectI r, const TCHAR *txt);

bool    IsCursorOverWindow(HWND hwnd);
void    CenterDialog(HWND hDlg);
TCHAR * GetDefaultPrinterName();
bool    CopyTextToClipboard(const TCHAR *text, bool appendOnly=false);

#endif
