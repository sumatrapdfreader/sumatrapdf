/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// the following are only defined if _WIN32_WINNT >= 0x0600 and we use 0x0500
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

#define NO_COLOR (COLORREF) - 1

#define WIN_COL_WHITE RGB(0xff, 0xff, 0xff)
#define WIN_COL_BLACK RGB(0, 0, 0)

#define DRAGQUERY_NUMFILES 0xFFFFFFFF

int RectDx(const RECT &r);
int RectDy(const RECT &r);
POINT MakePoint(long x, long y);
SIZE MakeSize(long dx, long dy);
RECT MakeRect(long x, long y, long dx, long dy);

RECT GetClientRect(HWND);

void Edit_SelectAll(HWND hwnd);
void ListBox_AppendString_NoSort(HWND hwnd, WCHAR *txt);

BOOL SafeCloseHandle(HANDLE *h);
BOOL SafeDestroyWindow(HWND *hwnd);
void FillWndClassEx(WNDCLASSEX &wcex, const WCHAR *clsName, WNDPROC wndproc);
void MoveWindow(HWND hwnd, RectI rect);
void MoveWindow(HWND hwnd, RECT *r);

bool IsOs64();
bool IsProcess64();
bool IsRunningInWow64();
bool IsProcessAndOsArchSame();
bool IsVistaOrGreater();
void GetOsVersion(OSVERSIONINFOEX& ver);
bool IsWin10();
bool IsWin7();

void LogLastError(DWORD err = 0);
void DbgOutLastError(DWORD err = 0);
bool RegKeyExists(HKEY keySub, const WCHAR *keyName);
WCHAR *ReadRegStr(HKEY keySub, const WCHAR *keyName, const WCHAR *valName);
bool WriteRegStr(HKEY keySub, const WCHAR *keyName, const WCHAR *valName, const WCHAR *value);
bool ReadRegDWORD(HKEY keySub, const WCHAR *keyName, const WCHAR *valName, DWORD &value);
bool WriteRegDWORD(HKEY keySub, const WCHAR *keyName, const WCHAR *valName, DWORD value);
bool CreateRegKey(HKEY keySub, const WCHAR *keyName);
bool DeleteRegKey(HKEY keySub, const WCHAR *keyName, bool resetACLFirst = false);
WCHAR *GetSpecialFolder(int csidl, bool createIfMissing = false);

void DisableDataExecution();
void RedirectIOToConsole();
WCHAR *GetExePath();
WCHAR *GetExeDir();
int FileTimeDiffInSecs(const FILETIME &ft1, const FILETIME &ft2);

WCHAR *ResolveLnk(const WCHAR *path);
bool CreateShortcut(const WCHAR *shortcutPath, const WCHAR *exePath, const WCHAR *args = nullptr,
                    const WCHAR *description = nullptr, int iconIndex = 0);
IDataObject *GetDataObjectForFile(const WCHAR *filePath, HWND hwnd = nullptr);
DWORD GetFileVersion(const WCHAR *path);

inline bool IsKeyPressed(int key) { return GetKeyState(key) & 0x8000 ? true : false; }
inline bool IsShiftPressed() { return IsKeyPressed(VK_SHIFT); }
inline bool IsAltPressed() { return IsKeyPressed(VK_MENU); }
inline bool IsCtrlPressed() { return IsKeyPressed(VK_CONTROL); }

HFONT CreateSimpleFont(HDC hdc, const WCHAR *fontName, int fontSize);

RectI ShiftRectToWorkArea(RectI rect, bool bFully = false);
RectI GetWorkAreaRect(RectI rect);
RectI GetFullscreenRect(HWND hwnd);
RectI GetVirtualScreenRect();

bool LaunchFile(const WCHAR *path, const WCHAR *params = nullptr, const WCHAR *verb = nullptr,
                bool hidden = false);
HANDLE LaunchProcess(const WCHAR *cmdLine, const WCHAR *currDir = nullptr, DWORD flags = 0);

void PaintRect(HDC, const RectI &);
void PaintLine(HDC, const RectI &);
void DrawCenteredText(HDC hdc, const RectI &r, const WCHAR *txt, bool isRTL = false);
void DrawCenteredText(HDC , const RECT &r, const WCHAR *txt, bool isRTL = false);
SizeI TextSizeInHwnd(HWND, const WCHAR*, HFONT=nullptr);
SIZE TextSizeInHwnd2(HWND, const WCHAR *, HFONT);
SizeI TextSizeInDC(HDC, const WCHAR *);

bool IsCursorOverWindow(HWND);
bool GetCursorPosInHwnd(HWND, PointI&);
void CenterDialog(HWND hDlg, HWND hParent = nullptr);
WCHAR *GetDefaultPrinterName();
bool CopyTextToClipboard(const WCHAR *text, bool appendOnly = false);
bool CopyImageToClipboard(HBITMAP hbmp, bool appendOnly);
void ToggleWindowStyle(HWND hwnd, DWORD flag, bool enable, int type = GWL_STYLE);
RectI ChildPosWithinParent(HWND);
HFONT GetDefaultGuiFont();
long GetDefaultGuiFontSize();

IStream *CreateStreamFromData(const void *data, size_t len);
void *GetDataFromStream(IStream *stream, size_t *len, HRESULT *res_opt = nullptr);
bool ReadDataFromStream(IStream *stream, void *buffer, size_t len, size_t offset = 0);
UINT GuessTextCodepage(const char *data, size_t len, UINT defVal = CP_ACP);
WCHAR *NormalizeString(const WCHAR *str, int /* NORM_FORM */ form);
bool IsRtl(HWND hwnd);
void ResizeHwndToClientArea(HWND hwnd, int dx, int dy, bool hasMenu);
void ResizeWindow(HWND, int dx, int dy);

// schedule WM_PAINT at window's leasure
void ScheduleRepaint(HWND hwnd);

// do WM_PAINT immediately
void RepaintNow(HWND hwnd);

inline BOOL toBOOL(bool b) { return b ? TRUE : FALSE; }

namespace win {

void ToForeground(HWND hwnd);

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
const WCHAR *ToSafeString(const WCHAR *str, ScopedMem<WCHAR> &newStr);

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
    DeferWinPosHelper();
    ~DeferWinPosHelper();
    void End();
    void SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, UINT uFlags);
    void MoveWindow(HWND hWnd, int x, int y, int cx, int cy, BOOL bRepaint = TRUE);
    void MoveWindow(HWND hWnd, RectI r);
};

struct BitmapPixels {
    uint8*      pixels;
    SizeI       size;
    int         nBytes;
    int         nBytesPerPixel;
    int         nBytesPerRow;

    HBITMAP     hbmp;
    BITMAPINFO  bmi;
    HDC         hdc;
};

void InitAllCommonControls();
SizeI GetBitmapSize(HBITMAP hbmp);
BitmapPixels *GetBitmapPixels(HBITMAP hbmp);
void FinalizeBitmapPixels(BitmapPixels* bitmapPixels);
COLORREF GetPixel(BitmapPixels *bitmap, int x, int y);
void UpdateBitmapColors(HBITMAP hbmp, COLORREF textColor, COLORREF bgColor);
unsigned char *SerializeBitmap(HBITMAP hbmp, size_t *bmpBytesOut);
HBITMAP CreateMemoryBitmap(SizeI size, HANDLE *hDataMapping = nullptr);
double GetProcessRunningTime();

void RunNonElevated(const WCHAR *exePath);
void VariantInitBstr(VARIANT &urlVar, const WCHAR *s);
char *LoadTextResource(int resId, size_t *sizeOut = nullptr);
bool DDEExecute(const WCHAR *server, const WCHAR *topic, const WCHAR *command);

void RectInflateTB(RECT &r, int top, int bottom);
void DivideRectH(const RECT &r, int y, int dy, RECT &r1, RECT &r2, RECT &r3);
void DivideRectV(const RECT &r, int x, int dx, RECT &r1, RECT &r2, RECT &r3);

HCURSOR GetCursor(LPWSTR id);
void SetCursor(LPWSTR id);
void DeleteCachedCursors();

int GetMeasurementSystem();
bool TrackMouseLeave(HWND);

void TriggerRepaint(HWND);
POINT GetCursorPosInHwnd(HWND);
