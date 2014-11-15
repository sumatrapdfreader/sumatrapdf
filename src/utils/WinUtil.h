/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// the following are only defined if _WIN32_WINNT >= 0x0600 and we use 0x0500
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

#define NO_COLOR        (COLORREF)-1

#define WIN_COL_WHITE   RGB(0xff, 0xff, 0xff)
#define WIN_COL_BLACK   RGB(0, 0, 0)

#define DRAGQUERY_NUMFILES 0xFFFFFFFF

#define Edit_SelectAll(hwnd) Edit_SetSel(hwnd, 0, -1)
#define ListBox_AppendString_NoSort(hwnd, txt) ListBox_InsertString(hwnd, -1, txt)

HMODULE SafeLoadLibrary(const WCHAR *dllName);
FARPROC LoadDllFunc(const WCHAR *dllName, const char *funcName);
BOOL    SafeCloseHandle(HANDLE *h);
BOOL    SafeDestroyWindow(HWND *hwnd);
void    FillWndClassEx(WNDCLASSEX& wcex, const WCHAR *clsName, WNDPROC wndproc);
inline void MoveWindow(HWND hwnd, RectI rect) { MoveWindow(hwnd, rect.x, rect.y, rect.dx, rect.dy, TRUE); }

bool   _IsAppThemed();
bool   IsRunningInWow64();
bool   IsVistaOrGreater();

void   LogLastError(DWORD err=0);
bool   RegKeyExists(HKEY keySub, const WCHAR *keyName);
WCHAR *ReadRegStr(HKEY keySub, const WCHAR *keyName, const WCHAR *valName);
bool   WriteRegStr(HKEY keySub, const WCHAR *keyName, const WCHAR *valName, const WCHAR *value);
bool   ReadRegDWORD(HKEY keySub, const WCHAR *keyName, const WCHAR *valName, DWORD& value);
bool   WriteRegDWORD(HKEY keySub, const WCHAR *keyName, const WCHAR *valName, DWORD value);
bool   CreateRegKey(HKEY keySub, const WCHAR *keyName);
bool   DeleteRegKey(HKEY keySub, const WCHAR *keyName, bool resetACLFirst=false);
WCHAR *GetSpecialFolder(int csidl, bool createIfMissing=false);

void   DisableDataExecution();
void   RedirectIOToConsole();
WCHAR *GetExePath();
int    FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2);

WCHAR *ResolveLnk(const WCHAR *path);
bool   CreateShortcut(const WCHAR *shortcutPath, const WCHAR *exePath,
                    const WCHAR *args=NULL, const WCHAR *description=NULL,
                    int iconIndex=0);
IDataObject* GetDataObjectForFile(const WCHAR *filePath, HWND hwnd=NULL);
DWORD GetFileVersion(const WCHAR *path);

inline bool IsKeyPressed(int key) { return GetKeyState(key) & 0x8000 ? true : false; }
inline bool IsShiftPressed() { return IsKeyPressed(VK_SHIFT); }
inline bool IsAltPressed() { return IsKeyPressed(VK_MENU); }
inline bool IsCtrlPressed() { return IsKeyPressed(VK_CONTROL); }

HFONT   CreateSimpleFont(HDC hdc, const WCHAR *fontName, int fontSize);

RectI   ShiftRectToWorkArea(RectI rect, bool bFully=false);
RectI   GetWorkAreaRect(RectI rect);
RectI   GetFullscreenRect(HWND hwnd);
RectI   GetVirtualScreenRect();

bool    LaunchFile(const WCHAR *path, const WCHAR *params=NULL, const WCHAR *verb=NULL, bool hidden=false);
HANDLE  LaunchProcess(const WCHAR *cmdLine, const WCHAR *currDir=NULL, DWORD flags=0);

void    PaintRect(HDC hdc, const RectI& rect);
void    PaintLine(HDC hdc, const RectI& rect);
void    DrawCenteredText(HDC hdc, const RectI& r, const WCHAR *txt, bool isRTL=false);
void    DrawCenteredText(HDC hdc, const RECT& r, const WCHAR *txt, bool isRTL=false);
SizeI   TextSizeInHwnd(HWND hwnd, const WCHAR *txt);

bool    IsCursorOverWindow(HWND hwnd);
bool    GetCursorPosInHwnd(HWND hwnd, PointI& posOut);
void    CenterDialog(HWND hDlg, HWND hParent=NULL);
WCHAR * GetDefaultPrinterName();
bool    CopyTextToClipboard(const WCHAR *text, bool appendOnly=false);
bool    CopyImageToClipboard(HBITMAP hbmp, bool appendOnly=false);
void    ToggleWindowStyle(HWND hwnd, DWORD flag, bool enable, int type=GWL_STYLE);
RectI   ChildPosWithinParent(HWND hwnd);
HFONT   GetDefaultGuiFont();

IStream*CreateStreamFromData(const void *data, size_t len);
void  * GetDataFromStream(IStream *stream, size_t *len, HRESULT *res_opt=NULL);
bool    ReadDataFromStream(IStream *stream, void *buffer, size_t len, size_t offset=0);
UINT    GuessTextCodepage(const char *data, size_t len, UINT defVal=CP_ACP);
WCHAR * NormalizeString(const WCHAR *str, int /* NORM_FORM */ form);
bool    IsRtl(HWND hwnd);
void    ResizeHwndToClientArea(HWND hwnd, int dx, int dy, bool hasMenu);

inline int RectDx(const RECT& r) { return r.right - r.left; }
inline int RectDy(const RECT& r) { return r.bottom - r.top; }

// schedule WM_PAINT at window's leasure
inline void ScheduleRepaint(HWND hwnd) { InvalidateRect(hwnd, NULL, FALSE); }

// do WM_PAINT immediately
inline void RepaintNow(HWND hwnd) {
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
}

inline BOOL toBOOL(bool b) { return b ? TRUE : FALSE; }

namespace win {

size_t GetTextLen(HWND hwnd);
WCHAR *GetText(HWND hwnd);

void SetText(HWND hwnd, const WCHAR *txt);
void SetVisibility(HWND hwnd, bool visible);
bool HasFrameThickness(HWND hwnd);
bool HasCaption(HWND hwnd);

namespace menu {
void SetChecked(HMENU m, UINT id, bool isChecked);
bool SetEnabled(HMENU m, UINT id, bool isEnabled);
void Remove(HMENU m, UINT id);
void Empty(HMENU m);
void SetText(HMENU m, UINT id, WCHAR *s);
WCHAR *ToSafeString(const WCHAR *str);

} // namespace menu

} // namespace win

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

class DeferWinPosHelper {
    HDWP hdwp;

public:
    DeferWinPosHelper() {
        hdwp = ::BeginDeferWindowPos(32);
    }

    ~DeferWinPosHelper() {
        End();
    }

    void End() {
        if (hdwp) {
            ::EndDeferWindowPos(hdwp);
            hdwp = NULL;
        }
    }

    void SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, UINT uFlags) {
        hdwp = ::DeferWindowPos(hdwp, hWnd, hWndInsertAfter, x, y, cx, cy, uFlags);
    }

    void MoveWindow(HWND hWnd, int x, int y, int cx, int cy, BOOL bRepaint=TRUE) {
        UINT uFlags = SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER;
        if (!bRepaint)
            uFlags |= SWP_NOREDRAW;
        this->SetWindowPos(hWnd, 0, x, y, cx, cy, uFlags);
    }

    void MoveWindow(HWND hWnd, RectI r) {
        this->MoveWindow(hWnd, r.x, r.y, r.dx, r.dy);
    }
};

void    InitAllCommonControls();
SizeI   GetBitmapSize(HBITMAP hbmp);
void    UpdateBitmapColors(HBITMAP hbmp, COLORREF textColor, COLORREF bgColor);
unsigned char *SerializeBitmap(HBITMAP hbmp, size_t *bmpBytesOut);
HBITMAP CreateMemoryBitmap(SizeI size, HANDLE *hDataMapping=NULL);
COLORREF AdjustLightness(COLORREF c, float factor);
float GetLightness(COLORREF c);
double  GetProcessRunningTime();

void RunNonElevated(const WCHAR *exePath);
void VariantInitBstr(VARIANT& urlVar, const WCHAR *s);
char *LoadTextResource(int resId, size_t *sizeOut=NULL);
bool DDEExecute(const WCHAR *server, const WCHAR *topic, const WCHAR *command);

void RectInflateTB(RECT& r, int top, int bottom);
void DivideRectH(const RECT&r, int y, int dy, RECT& r1, RECT& r2, RECT& r3);
void DivideRectV(const RECT&r, int x, int dx, RECT& r1, RECT& r2, RECT& r3);
