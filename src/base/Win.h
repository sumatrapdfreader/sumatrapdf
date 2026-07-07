/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define NO_COLOR (COLORREF) - 1

#define WIN_COL_WHITE RGB(0xff, 0xff, 0xff)
#define WIN_COL_BLACK RGB(0, 0, 0)

#define DRAGQUERY_NUMFILES 0xFFFFFFFF

bool ToBool(BOOL b);

UINT_PTR NextSubclassId();

RECT ClientRECT(HWND);
Rect ClientRect(HWND);
Rect WindowRect(HWND);
Rect MapRectToWindow(Rect, HWND hwndFrom, HWND hwndTo);
Rect MapLtrClientRectToScreen(HWND hwnd, Rect r);
int MapChildXForRtlParent(HWND parent, int ltrX, int childDx);

void EditSelectAll(HWND);
int EditIdealDy(HWND, bool hasBorder, int lines = 1);
void EditImplementCtrlBack(HWND hwnd);

void ListBox_AppendString_NoSort(HWND, WStr txt);
int ListBoxGetTopIndex(HWND);
bool ListBoxSetTopIndex(HWND, int);

bool IsValidHandle(HANDLE);
bool SafeCloseHandle(HANDLE*);
bool SafeFindClose(HANDLE*);
void FillWndClassEx(WNDCLASSEX& wcex, WStr clsName, WNDPROC wndproc);
void MoveWindow(HWND hwnd, Rect rect);
void MoveWindow(HWND hwnd, RECT* r);

bool IsOs64();
int CpuCoreCount();
bool IsProcess64();
bool IsProcess32();
bool IsArmBuild();
bool IsRunningInWow64();
bool IsRunningOnWine();
bool IsProcessAndOsArchSame();

bool GetOsVersion(OSVERSIONINFOEX& ver);
TempStr OsNameFromVerTemp(const OSVERSIONINFOEX& ver);
TempStr GetWindowsVerTemp();

TempStr GetEnvVariableTemp(Str name);

TempStr GetLastErrorStrTemp(DWORD& err);
void LogLastError(DWORD err = 0);
void DbgOutLastError(DWORD err = 0);

// registry
const TempStr RegKeyNameTemp(HKEY key);
bool RegKeyExists(HKEY keySub, Str keyName);
TempStr ReadRegStrTemp(HKEY keySub, Str keyName, Str valName);
TempStr LoggedReadRegStrTemp(HKEY keySub, Str keyName, Str valName);
TempStr ReadRegStr2Temp(Str keyName, Str valName);
TempStr LoggedReadRegStr2Temp(Str keyName, Str valName);
bool WriteRegStr(HKEY keySub, Str keyName, Str valName, Str value);
bool LoggedWriteRegStr(HKEY keySub, Str keyName, Str valName, Str value);
bool ReadRegDWORD(HKEY keySub, Str keyName, Str valName, DWORD& value);
bool WriteRegDWORD(HKEY keySub, Str keyName, Str valName, DWORD value);
bool LoggedWriteRegDWORD(HKEY keySub, Str keyName, Str valName, DWORD value);
bool LoggedWriteRegNone(HKEY hkey, Str key, Str valName);
bool CreateRegKey(HKEY keySub, Str keyName);
bool DeleteRegKey(HKEY keySub, Str keyName, bool resetACLFirst = false);
bool LoggedDeleteRegKey(HKEY keySub, Str keyName, bool resetACLFirst = false);
bool DeleteRegValue(HKEY keySub, Str keyName, Str val);
bool LoggedDeleteRegValue(HKEY keySub, Str keyName, Str val);
HRESULT CLSIDFromString(Str lpsz, LPCLSID pclsid);

// file and directory operations
TempStr GetSpecialFolderTemp(int csidl, bool createIfMissing = false);
TempStr GetTempDirTemp();
TempStr GetSelfExePathTemp();
TempWStr GetSelfExePathW();
TempStr GetSelfExeDirTemp();
void ChangeCurrDirToDocuments();
int FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2);
TempStr ResolveLnkTemp(Str path);
bool CreateShortcut(Str shortcutPath, Str exePath, Str args = Str(), Str description = Str(), int iconIndex = 0);
IDataObject* GetDataObjectForFile(Str filePath, HWND hwnd = nullptr);

HANDLE LaunchProcessWithCmdLine(Str exe, Str cmdLine);
HANDLE LaunchProcessInDir(Str cmdLine, Str currDir = Str(), DWORD flags = 0);
bool CreateProcessHelper(Str exe, Str args);
bool LaunchFileShell(Str path, Str params = Str(), Str verb = Str(), bool hidden = false);
bool LaunchBrowser(Str url);
void OpenPathInDefaultFileManager(Str path);
void PaintCheckerboard(HDC hdc, int x, int y, int w, int h);

void RunNonElevated(Str exePath);
bool LaunchElevated(Str path, Str cmdline);
bool IsProcessRunningElevated();
TempStr GetParentProcessPath(DWORD* pidOut = nullptr);
bool CanTalkToProcess(DWORD procId);
DWORD GetAccountType();
DWORD GetOriginalAccountType();

void DisableDataExecution();
bool RedirectIOToConsole();
bool RedirectIOToExistingConsole();
void HandleRedirectedConsoleOnShutdown();
void InitConsoleOutput();
void LogConsole(Str s);
void WaitForConsoleClose();
void SendEnterIfLoggedToConsole();

bool IsKeyPressed(int key);
bool IsShiftPressed();
bool IsAltPressed();
bool IsCtrlPressed();

Rect ShiftRectToWorkArea(Rect rect, HWND hwnd = nullptr, bool bFully = false);
Rect GetWorkAreaRect(Rect rect, HWND hwnd);
void LimitWindowSizeToScreen(HWND hwnd, SIZE& size);
void HwndEnsureVisible(HWND hwnd);
Rect GetFullscreenRect(HWND);
Rect GetVirtualScreenRect();

void DrawRect(HDC, const Rect&);
void FillRect(HDC, const Rect&, HBRUSH);
void FillRect(HDC hdc, const Rect&, COLORREF);
void DrawLine(HDC, const Rect&);

void DrawCenteredText(HDC hdc, Rect r, Str txt, bool isRTL = false);
Size HwndMeasureText(HWND hwnd, Str txt, HFONT font = nullptr);
int FontDyPx(HWND hwnd, HFONT hfont);

int HdcDrawText(HDC hdc, Str s, RECT* r, uint format, HFONT font = nullptr);
int HdcDrawText(HDC hdc, Str s, const Rect& r, uint format, HFONT font = nullptr);
int HdcDrawText(HDC hdc, Str s, const Point& pos, uint fmt, HFONT font = nullptr);
Size HdcMeasureText(HDC hdc, Str s, int maxDx, uint format, HFONT font);
Size HdcMeasureText(HDC hdc, Str s, uint format, HFONT font);
Size HdcMeasureText(HDC hdc, Str s, HFONT font = nullptr);

HWND HwndSetFocus(HWND hwnd);
bool HwndIsFocused(HWND);
bool IsCursorOverWindow(HWND);

HWND HwndGetParent(HWND hwnd);
TempStr HwndGetClassName(HWND hwnd);
Point HwndGetCursorPos(HWND hwnd);
Point& UnmirrorRtl(HWND hwnd, Point& p);
int MapWindowPoints(HWND, HWND, Point*, int);
void HwndScreenToClient(HWND, Point&);
void HwndMakeVisible(HWND);

bool IsMouseOverRect(HWND hwnd, const Rect& r);
void CenterDialog(HWND hDlg, HWND hParent = nullptr);
void SetDlgItemFont(HWND hDlg, int nIDDlgItem, HFONT fnt);

TempStr GetDefaultPrinterNameTemp();

bool OpenClipboardForUpdate();
void CloseClipboardAfterUpdate();

bool CopyTextToClipboard(Str s);
bool AppendTextToClipboard(Str s);

bool CopyImageToClipboard(HBITMAP hbmp, bool appendOnly);

bool IsWindowStyleSet(HWND hwnd, DWORD flags);
bool IsWindowStyleExSet(HWND hwnd, DWORD flags);
void SetWindowStyle(HWND hwnd, DWORD flags, bool enable);
void SetWindowExStyle(HWND hwnd, DWORD flags, bool enable);

bool HwndIsRtl(HWND hwnd);
void HwndSetRtl(HWND hwnd, bool isRtl);

Rect ChildPosWithinParent(HWND);

HFONT GetMenuFont();
HFONT CreateSimpleFont(HDC hdc, Str fontName, int fontSize);
HFONT GetDefaultGuiFont(bool bold = false, bool italic = false);
HFONT GetDefaultGuiFontOfSize(int size);
HFONT GetUserGuiFont(Str fontName, int size);
HFONT GetUserGuiFontEx(Str fontName, int size, bool bold, bool italic);
int GetSizeOfDefaultGuiFont();
void DeleteCreatedFonts();

IStream* CreateStreamFromData(const Str&);
Str ReadIStream(IStream* stream);
uint GuessTextCodepage(Str data, uint defVal = CP_ACP);
TempStr NormalizeString(Str str, int /* NORM_FORM */ form);
void ResizeHwndToClientArea(HWND hwnd, int dx, int dy, bool hasMenu);
void ResizeWindow(HWND, int dx, int dy);

void MessageBoxWarningSimple(HWND hwnd, WStr msg, WStr title = WStr());
void MessageBoxNYI(HWND hwnd);

bool RegisterServerDLL(Str dllPath, Str args = Str());
bool UnRegisterServerDLL(Str dllPath, Str args = Str());
bool RegisterOrUnregisterServerDLL(Str dllPath, bool install, Str args = Str());

inline BOOL toBOOL(bool b) {
    return b ? TRUE : FALSE;
}

inline bool fromBOOL(BOOL b) {
    return b != 0;
}

inline bool tobool(BOOL b) {
    return b != 0;
}

void MenuSetChecked(HMENU m, int id, bool isChecked);
bool MenuSetEnabled(HMENU m, int id, bool isEnabled);
void MenuRemove(HMENU m, int id);
// TODO: this doesn't recognize enum Cmd, why?
// void Remove(HMENU m, enum Cmd id);
void MenuEmpty(HMENU m);
void MenuSetText(HMENU m, int id, WStr s);
void MenuSetText(HMENU m, int id, Str s);
TempStr MenuToSafeStringTemp(Str s);

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
    void Flush(HDC hdc) const;
};

class DeferWinPosHelper {
    HDWP hdwp;

  public:
    DeferWinPosHelper();
    ~DeferWinPosHelper();
    void End();
    void SetWindowPos(HWND hwnd, Rect rc);
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

struct Pixmap;
struct RenderedBitmap;
Pixmap* AllocPixmapDIB(int w, int h);
bool BlitPixmap(Pixmap* p, HDC hdc, Rect target);
bool BlitPixmapRegion(Pixmap* p, HDC hdc, Rect target, Rect source);
Pixmap* PixmapFromHBITMAP(HBITMAP hbmp, Size size, HANDLE hMap = nullptr);
Pixmap* PixmapFromRenderedBitmap(RenderedBitmap* rb);
RenderedBitmap* RenderedBitmapFromPixmap(Pixmap* px);

// A Windows present-layer bitmap handle: an HBITMAP (+ optional file mapping) that can be
// blitted to an HDC. Concrete and Windows-only by design - portable pixel data lives in
// Pixmap; this is just the GDI handle the UI paints. Built from a Pixmap or an HBITMAP.
struct RenderedBitmap {
    Size size = {};
    HBITMAP hbmp = nullptr;
    HANDLE hMap = nullptr;

    RenderedBitmap(HBITMAP hbmp, Size size, HANDLE hMap = nullptr);
    ~RenderedBitmap();

    Size GetSize();
    RenderedBitmap* Clone() const;
    HBITMAP GetBitmap() const;
    bool IsValid();
    bool Blit(HDC hdc, Rect target);
};

i64 RenderedBitmapByteSize(RenderedBitmap*);

void InitAllCommonControls();
Size GetBitmapSize(HBITMAP hbmp);
BitmapPixels* GetBitmapPixels(HBITMAP hbmp);
void FinalizeBitmapPixels(BitmapPixels* bitmapPixels);
COLORREF GetPixel(BitmapPixels* bitmap, int x, int y);
void UpdateBitmapColors(HBITMAP hbmp, COLORREF textColor, COLORREF bgColor);
Str HBITMAPToBmpFormat(HBITMAP hbmp);
Str GetClipboardImageBmp();
HBITMAP CreateMemoryBitmap(Size size, HANDLE* hDataMapping = nullptr);
bool BlitHBITMAP(HBITMAP hbmp, HDC hdc, Rect target);
double GetProcessRunningTime();

void VariantInitBstr(VARIANT& urlVar, WStr s);
bool DDEExecute(WStr server, WStr topic, WStr command);

void RectInflateTB(RECT& r, int top, int bottom);
void DivideRectH(const RECT& r, int y, int dy, RECT& r1, RECT& r2, RECT& r3);
void DivideRectV(const RECT& r, int x, int dx, RECT& r1, RECT& r2, RECT& r3);

HCURSOR GetCachedCursor(LPWSTR id);
void SetCursorCached(LPWSTR id);
void DeleteCachedCursors();

int GetMeasurementSystem();
bool TrackMouseLeave(HWND);

struct LoadedDataResource {
    const u8* data = nullptr;
    int dataSize = 0;
};
bool LockDataResource(int resId, LoadedDataResource*);

HINSTANCE GetInstance();
Size ButtonGetIdealSize(HWND hwnd);
bool IsValidDelayType(int type);

void HwndResizeClientSize(HWND, int, int);

int HwndGetTextLen(HWND hwnd);
TempWStr HwndGetTextWTemp(HWND hwnd);
TempStr HwndGetTextTemp(HWND hwnd);
void HwndSetText(HWND, Str s);
bool HwndHasFrameThickness(HWND hwnd);
bool HwndHasCaption(HWND hwnd);

void HwndSetDlgItemText(HWND, int, Str s);

void CbAddString(HWND, Str s);
void CbSetCurrentSelection(HWND, int);

HICON HwndGetIcon(HWND);
HICON HwndSetIcon(HWND, HICON);

void HwndRepaintNow(HWND);
void HwndScheduleRepaint(HWND hwnd);

HFONT HwndGetFont(HWND);
void HwndSetFont(HWND, HFONT);

void HwndPositionToTheRightOf(HWND hwnd, HWND hwndRelative);
void HwndPositionInCenterOf(HWND hwnd, HWND hwndRelative);
void HwndSendCommand(HWND hwnd, int cmdId, LPARAM lp = 0);
void HwndPostCommand(HWND hwnd, int cmdId, LPARAM lp = 0);
void HwndDestroyWindowSafe(HWND* hwnd);
void HwndToForeground(HWND hwnd);
void HwndSetVisibility(HWND hwnd, bool visible);

bool DeleteObjectSafe(HGDIOBJ*);
bool DeleteBrushSafe(HBRUSH*);
bool DestroyIconSafe(HICON*);

void TbSetButtonInfoById(HWND hwnd, int buttonId, TBBUTTONINFO* info);
void TbGetPadding(HWND, int* padX, int* padY);
void TbSetPadding(HWND, int padX, int padY);
void TbGetMetrics(HWND hwnd, TBMETRICS* metrics);
void TbSetMetrics(HWND hwnd, TBMETRICS* metrics);
void TbGetRectById(HWND hwnd, int buttonId, RECT* rc);
void TbGetRectByIdx(HWND hwnd, int buttonIdx, RECT* rc);

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, uint flag, bool subtree);
void AddPathToRecentDocs(Str path);

TempStr HGLOBALToStrTemp(HGLOBAL h, bool isUnicode);
HGLOBAL MemToHGLOBAL(void* src, int n, UINT flags = GMEM_MOVEABLE);
HGLOBAL StrToHGLOBAL(Str s, UINT flags = GMEM_MOVEABLE);
TempStr AtomToStrTemp(ATOM a);
int MsgBox(HWND, Str text, Str caption, UINT flags);
void MaskFpExceptions();
HWND ShowTextInWindow(Str title, Str text, HWND* hwndPtr = nullptr);
void ShowTextInWindowDialog(Str title, Str text);

constexpr u32 kCpuMMX = 1 << 1;
constexpr u32 kCpuSSE = 1 << 2;
constexpr u32 kCpuSSE2 = 1 << 2;
constexpr u32 kCpuSSE3 = 1 << 3;
constexpr u32 kCpuSSE41 = 1 << 4;
constexpr u32 kCpuSSE42 = 1 << 5;
constexpr u32 kCpuAVX = 1 << 6;
constexpr u32 kCpuAVX2 = 1 << 7;
// ARM
constexpr u32 kCpuNEON = 1 << 8;
constexpr u32 kCpuArmCrypto = 1 << 9;
constexpr u32 kCpuArmAtomics = 1 << 10;
constexpr u32 kCpuArmDotProd = 1 << 11;

u32 CpuID();
Str LatestSupportedSIMD();

LARGE_INTEGER TimeNow();
double TimeDiffSecs(const LARGE_INTEGER& start, const LARGE_INTEGER& end);
double TimeDiffMs(const LARGE_INTEGER& start, const LARGE_INTEGER& end);
bool IsPEFileSigned(Str filePath);
TempStr GetExecutableSignerTemp(Str exePath);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DC state management
struct SavedDCState {
    HWND hwnd;
    HDC hdc;
    HFONT oldFont;
};

SavedDCState SaveDCState(HWND hwnd);
void RestoreDCState(SavedDCState* state);
int MeasureStringWidth(HDC hdc, WStr str);
Str GetLastErrorAsStr(Arena* arena);
bool WasLaunchedByPowershellWithPipeRedirect();
Str GetAppLocalDataDirTemp();
