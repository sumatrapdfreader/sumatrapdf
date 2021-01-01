/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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

int RectDx(const RECT& r);
int RectDy(const RECT& r);
POINT MakePoint(long x, long y);
SIZE MakeSize(long dx, long dy);
RECT MakeRect(long x, long y, long dx, long dy);

RECT GetClientRect(HWND);
Rect ClientRect(HWND hwnd);
Rect WindowRect(HWND hwnd);
Rect MapRectToWindow(Rect rect, HWND hwndFrom, HWND hwndTo);

void Edit_SelectAll(HWND hwnd);
void ListBox_AppendString_NoSort(HWND hwnd, WCHAR* txt);

BOOL SafeCloseHandle(HANDLE* h);
void FillWndClassEx(WNDCLASSEX& wcex, const WCHAR* clsName, WNDPROC wndproc);
void MoveWindow(HWND hwnd, Rect rect);
void MoveWindow(HWND hwnd, RECT* r);

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
bool RegKeyExists(HKEY keySub, const WCHAR* keyName);
WCHAR* ReadRegStr(HKEY keySub, const WCHAR* keyName, const WCHAR* valName);
char* ReadRegStrUtf8(HKEY keySub, const WCHAR* keyName, const WCHAR* valName);
WCHAR* ReadRegStr2(const WCHAR* keyName, const WCHAR* valName);
bool WriteRegStr(HKEY keySub, const WCHAR* keyName, const WCHAR* valName, const WCHAR* value);
bool ReadRegDWORD(HKEY keySub, const WCHAR* keyName, const WCHAR* valName, DWORD& value);
bool WriteRegDWORD(HKEY keySub, const WCHAR* keyName, const WCHAR* valName, DWORD value);
bool CreateRegKey(HKEY keySub, const WCHAR* keyName);
bool DeleteRegKey(HKEY keySub, const WCHAR* keyName, bool resetACLFirst = false);
WCHAR* GetSpecialFolder(int csidl, bool createIfMissing = false);

void DisableDataExecution();
void RedirectIOToConsole();
WCHAR* GetExePath();
WCHAR* GetExeDir();
WCHAR* GetSystem32Dir();
WCHAR* GetCurrentDir();
void ChangeCurrDirToSystem32();
int FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2);

WCHAR* ResolveLnk(const WCHAR* path);
bool CreateShortcut(const WCHAR* shortcutPath, const WCHAR* exePath, const WCHAR* args = nullptr,
                    const WCHAR* description = nullptr, int iconIndex = 0);
IDataObject* GetDataObjectForFile(const WCHAR* filePath, HWND hwnd = nullptr);
DWORD GetFileVersion(const WCHAR* path);

bool IsKeyPressed(int key);
bool IsShiftPressed();
bool IsAltPressed();
bool IsCtrlPressed();

HFONT CreateSimpleFont(HDC hdc, const WCHAR* fontName, int fontSize);

Rect ShiftRectToWorkArea(Rect rect, bool bFully = false);
Rect GetWorkAreaRect(Rect rect);
void LimitWindowSizeToScreen(HWND hwnd, SIZE& size);
Rect GetFullscreenRect(HWND);
Rect GetVirtualScreenRect();

bool LaunchFile(const WCHAR* path, const WCHAR* params = nullptr, const WCHAR* verb = nullptr, bool hidden = false);
bool LaunchBrowser(const WCHAR* url);
HANDLE LaunchProcess(const WCHAR* cmdLine, const WCHAR* currDir = nullptr, DWORD flags = 0);
bool CreateProcessHelper(const WCHAR* exe, const WCHAR* args);
bool LaunchElevated(const WCHAR* path, const WCHAR* cmdline);
bool IsProcessRunningElevated();
bool CanTalkToProcess(DWORD procId);

void PaintRect(HDC, const Rect);
void PaintLine(HDC, const Rect);
void DrawCenteredText(HDC hdc, const Rect r, const WCHAR* txt, bool isRTL = false);
void DrawCenteredText(HDC, const RECT& r, const WCHAR* txt, bool isRTL = false);
Size TextSizeInHwnd(HWND, const WCHAR*, HFONT = nullptr);
SIZE TextSizeInHwnd2(HWND, const WCHAR*, HFONT);
Size TextSizeInDC(HDC, const WCHAR*);

bool IsFocused(HWND);
bool IsCursorOverWindow(HWND);
bool GetCursorPosInHwnd(HWND, Point&);
POINT GetCursorPosInHwnd(HWND);
void CenterDialog(HWND hDlg, HWND hParent = nullptr);
WCHAR* GetDefaultPrinterName();
bool CopyTextToClipboard(const WCHAR* text, bool appendOnly = false);
bool CopyImageToClipboard(HBITMAP hbmp, bool appendOnly);

bool IsWindowStyleSet(HWND hwnd, DWORD flags);
bool IsWindowStyleExSet(HWND hwnd, DWORD flags);
void SetWindowStyle(HWND hwnd, DWORD flags, bool enable);
void SetWindowExStyle(HWND hwnd, DWORD flags, bool enable);

bool IsRtl(HWND hwnd);
void SetRtl(HWND hwnd, bool isRtl);

Rect ChildPosWithinParent(HWND);

int GetSizeOfDefaultGuiFont();
HFONT GetDefaultGuiFont();
HFONT GetDefaultGuiFont(bool bold, bool italic);
HFONT GetDefaultGuiFontOfSize(int size);

IStream* CreateStreamFromData(std::span<u8>);
std::span<u8> GetDataFromStream(IStream* stream, HRESULT* resOpt);
std::span<u8> GetStreamOrFileData(IStream* stream, const WCHAR* filePath);
bool ReadDataFromStream(IStream* stream, void* buffer, size_t len, size_t offset = 0);
uint GuessTextCodepage(const char* data, size_t len, uint defVal = CP_ACP);
WCHAR* NormalizeString(const WCHAR* str, int /* NORM_FORM */ form);
void ResizeHwndToClientArea(HWND hwnd, int dx, int dy, bool hasMenu);
void ResizeWindow(HWND, int dx, int dy);

void MessageBoxWarningSimple(HWND hwnd, const WCHAR* msg, const WCHAR* title = nullptr);
void MessageBoxNYI(HWND hwnd);

// schedule WM_PAINT at window's leasure
void ScheduleRepaint(HWND hwnd);

// do WM_PAINT immediately
void RepaintNow(HWND hwnd);

bool RegisterServerDLL(const WCHAR* dllPath, const WCHAR* args = nullptr);
bool UnRegisterServerDLL(const WCHAR* dllPath, const WCHAR* args = nullptr);
bool RegisterOrUnregisterServerDLL(const WCHAR* dllPath, bool install, const WCHAR* args = nullptr);

inline BOOL toBOOL(bool b) {
    return b ? TRUE : FALSE;
}

inline bool fromBOOL(BOOL b) {
    return b ? true : false;
}

inline bool tobool(BOOL b) {
    return b ? true : false;
}

namespace win {

void ToForeground(HWND hwnd);

size_t GetTextLen(HWND hwnd);
WCHAR* GetText(HWND hwnd);
str::Str GetTextUtf8(HWND hwnd);

void SetText(HWND hwnd, const WCHAR* txt);
void SetVisibility(HWND hwnd, bool visible);
bool HasFrameThickness(HWND hwnd);
bool HasCaption(HWND hwnd);

namespace menu {
void SetChecked(HMENU m, int id, bool isChecked);
bool SetEnabled(HMENU m, int id, bool isEnabled);
void Remove(HMENU m, int id);
// TODO: this doesn't recognize enum Cmd, why?
// void Remove(HMENU m, enum Cmd id);
void Empty(HMENU m);
void SetText(HMENU m, int id, const WCHAR* s);
const WCHAR* ToSafeString(AutoFreeWstr& s);

} // namespace menu

} // namespace win

struct DoubleBuffer {
    HWND hTarget = nullptr;
    HDC hdcCanvas = nullptr;
    HDC hdcBuffer = nullptr;
    HBITMAP doubleBuffer = nullptr;
    Rect rect{};

    DoubleBuffer(HWND hwnd, Rect rect);
    DoubleBuffer(const DoubleBuffer&) = delete;
    DoubleBuffer& operator=(const DoubleBuffer&) = delete;
    ~DoubleBuffer();

    HDC GetDC() const;
    void Flush(HDC hdc);
};

class DeferWinPosHelper {
    HDWP hdwp;

  public:
    DeferWinPosHelper();
    ~DeferWinPosHelper();
    void End();
    void SetWindowPos(HWND hwnd, const Rect rc);
    void SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, uint uFlags);
    void MoveWindow(HWND hWnd, int x, int y, int cx, int cy, BOOL bRepaint = TRUE);
    void MoveWindow(HWND hWnd, Rect r);
};

struct BitmapPixels {
    u8* pixels;
    Size size;
    int nBytes;
    int nBytesPerPixel;
    int nBytesPerRow;

    HBITMAP hbmp;
    BITMAPINFO bmi;
    HDC hdc;
};

void InitAllCommonControls();
Size GetBitmapSize(HBITMAP hbmp);
BitmapPixels* GetBitmapPixels(HBITMAP hbmp);
void FinalizeBitmapPixels(BitmapPixels* bitmapPixels);
COLORREF GetPixel(BitmapPixels* bitmap, int x, int y);
void UpdateBitmapColors(HBITMAP hbmp, COLORREF textColor, COLORREF bgColor);
std::span<u8> SerializeBitmap(HBITMAP hbmp);
HBITMAP CreateMemoryBitmap(Size size, HANDLE* hDataMapping = nullptr);
bool BlitHBITMAP(HBITMAP hbmp, HDC hdc, Rect target);
double GetProcessRunningTime();

void RunNonElevated(const WCHAR* exePath);
void VariantInitBstr(VARIANT& urlVar, const WCHAR* s);
std::span<u8> LoadDataResource(int resId);
bool DDEExecute(const WCHAR* server, const WCHAR* topic, const WCHAR* command);

void RectInflateTB(RECT& r, int top, int bottom);
void DivideRectH(const RECT& r, int y, int dy, RECT& r1, RECT& r2, RECT& r3);
void DivideRectV(const RECT& r, int x, int dx, RECT& r1, RECT& r2, RECT& r3);

HCURSOR GetCachedCursor(LPWSTR id);
void SetCursorCached(LPWSTR id);
void DeleteCachedCursors();

int GetMeasurementSystem();
bool TrackMouseLeave(HWND);

void TriggerRepaint(HWND);
HINSTANCE GetInstance();
Size ButtonGetIdealSize(HWND hwnd);
std::tuple<const u8*, DWORD, HGLOBAL> LockDataResource(int id);
bool IsValidDelayType(int type);

void HwndDpiAdjust(HWND, float* x, float* y);
void HwndSetText(HWND, std::string_view s);
HICON HwndSetIcon(HWND, HICON);
HICON HwndGetIcon(HWND);
void HwndInvalidate(HWND);
void HwndSetFont(HWND, HFONT);
HFONT HwndGetFont(HWND);
Size HwndMeasureText(HWND hwnd, const WCHAR* txt, HFONT font);
void HwndPositionToTheRightOf(HWND hwnd, HWND hwndRelative);
void HwndSendCommand(HWND hwnd, int cmdId);

void TbSetButtonInfo(HWND hwnd, int buttonId, TBBUTTONINFO* info);
void TbGetPadding(HWND, int* padX, int* padY);
void TbSetPadding(HWND, int padX, int padY);
void TbGetMetrics(HWND hwnd, TBMETRICS* metrics);
void TbSetMetrics(HWND hwnd, TBMETRICS* metrics);
void TbGetRect(HWND hwnd, int buttonId, RECT* rc);
