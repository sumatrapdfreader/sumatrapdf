/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define DRAGQUERY_NUMFILES 0xFFFFFFFF

//--- bool / BOOL

bool ToBool(BOOL b);

inline BOOL toBOOL(bool b) {
    return b ? TRUE : FALSE;
}

inline bool fromBOOL(BOOL b) {
    return b != 0;
}

inline bool tobool(BOOL b) {
    return b != 0;
}

//--- subclass ids

UINT_PTR NextSubclassId();

//--- HWND: geometry

Rect HwndClientRect(HWND);
Rect HwndWindowRect(HWND);
void HwndInvalidateRect(HWND hwnd, Rect rect, bool erase);
void HwndInvalidate(HWND hwnd, bool erase = false);
Rect HwndMapRectToWindow(Rect, HWND hwndFrom, HWND hwndTo);
Rect HwndMapLtrClientRectToScreen(HWND hwnd, Rect r);
int HwndMapChildXForRtlParent(HWND parent, int ltrX, int childDx);
void HwndMoveWindow(HWND hwnd, Rect* r);
void HwndResizeClientSize(HWND, int, int);
void ResizeHwndToClientArea(HWND hwnd, int dx, int dy, bool hasMenu);
void ResizeWindow(HWND, int dx, int dy);
Rect ChildPosWithinParent(HWND);

//--- HWND: screen / work area / placement

Rect ShiftRectToWorkArea(Rect rect, HWND hwnd = nullptr, bool bFully = false);
Rect GetWorkAreaRect(Rect rect, HWND hwnd);
Size HwndLimitSizeToScreen(HWND hwnd, Size size);
void HwndEnsureOnScreen(HWND hwnd);
Rect HwndGetFullscreenRect(HWND);
Rect GetVirtualScreenRect();
void HwndPositionToTheRightOf(HWND hwnd, HWND hwndRelative);
void HwndPositionInCenterOf(HWND hwnd, HWND hwndRelative);
void HwndCenterDialog(HWND hDlg, HWND hParent = nullptr);

//--- HWND: coordinates

Point HwndMapWindowPoint(HWND, HWND, Point);
Point HwndClientToScreen(HWND, Point);
Point HwndScreenToClient(HWND, Point);
HWND HwndWindowFromPoint(Point);
Point GetCursorPosition();
Point HwndGetCursorPos(HWND hwnd);
Point& UnmirrorRtl(HWND hwnd, Point& p);
bool HwndIsCursorOverWindow(HWND);
bool HwndIsMouseOverRect(HWND hwnd, const Rect& r);

//--- HWND: focus / visibility / Z-order

HWND HwndSetFocus(HWND hwnd);
bool HwndIsFocused(HWND);
bool HwndIsVisible(HWND hwnd);
void HwndSetVisible(HWND hwnd, bool visible);
void HwndShow(HWND hwnd);
void HwndHide(HWND hwnd);
void HwndShowWithoutActivate(HWND);
void HwndToForeground(HWND hwnd);

//--- HWND: styles / RTL / chrome

bool HwndIsWindowStyleSet(HWND hwnd, DWORD flags);
bool HwndIsWindowStyleExSet(HWND hwnd, DWORD flags);
void HwndSetWindowStyle(HWND hwnd, DWORD flags, bool enable);
void HwndSetWindowExStyle(HWND hwnd, DWORD flags, bool enable);
bool HwndIsRtl(HWND hwnd);
void HwndSetRtl(HWND hwnd, bool isRtl);
bool HwndHasFrameThickness(HWND hwnd);
bool HwndHasCaption(HWND hwnd);

//--- HWND: text / font / icon / paint

int HwndGetTextLen(HWND hwnd);
TempWStr HwndGetTextWTemp(HWND hwnd);
TempStr HwndGetTextTemp(HWND hwnd);
void HwndSetText(HWND, Str s);
void HwndSetDlgItemText(HWND, int, Str s);
void HwndSetFont(HWND, HFONT);
void HwndSetFontForWindowAndItsChildren(HWND, HFONT);
void HwndSetTreeFontForDpi(HWND hwndTree, HFONT font, int dpi);
HICON HwndGetIcon(HWND);
HICON HwndSetIcon(HWND, HICON);
void HwndRepaintNow(HWND);
void HwndScheduleRepaint(HWND hwnd);

//--- HWND: identity / parent / lifecycle / messages

HWND HwndGetParent(HWND hwnd);
TempStr HwndGetClassName(HWND hwnd);
void HwndDestroyWindowSafe(HWND* hwnd);
void HwndSendCommand(HWND hwnd, int cmdId, LPARAM lp = 0);
void HwndPostCommand(HWND hwnd, int cmdId, LPARAM lp = 0);

//--- edit control

void EditSelectAll(HWND);

//--- list box

void ListBox_AppendString_NoSort(HWND, WStr txt);
void LbResetContent(HWND hwnd);
int LbAddString(HWND hwnd, WStr text);
int LbAddString(HWND hwnd, Str text);
int LbInsertString(HWND hwnd, int idx, WStr text);
int LbInsertString(HWND hwnd, int idx, Str text);
int LbGetCount(HWND hwnd);
int LbGetCurrentSelection(HWND hwnd);
bool LbSetCurrentSelection(HWND hwnd, int idx);
TempWStr LbGetTextTemp(HWND hwnd, int idx);
int LbGetItemHeight(HWND hwnd, int idx);
void LbSetItemHeight(HWND hwnd, int idx, int height);
Rect LbGetItemRect(HWND hwnd, int idx);
int LbItemFromPoint(HWND hwnd, Point point, bool* outside);
int LbGetTopIndex(HWND hwnd);
bool LbSetTopIndex(HWND hwnd, int idx);
void LbInitStorage(HWND hwnd, int count);

//--- list view

int LvGetItemCount(HWND hwnd);
int LvGetNextItem(HWND hwnd, int start, UINT flags);
void LvSetItemState(HWND hwnd, int i, UINT state, UINT mask);
UINT LvGetItemState(HWND hwnd, int i, UINT mask);
void LvEnsureVisible(HWND hwnd, int i, bool partialOk = false);
HWND LvGetEditControl(HWND hwnd);
int LvInsertItem(HWND hwnd, const LVITEMW* item);
bool LvEditLabel(HWND hwnd, int i);
void LvDeleteItem(HWND hwnd, int i);
void LvDeleteAllItems(HWND hwnd);
Rect LvGetItemRect(HWND hwnd, int i, int code);
Rect LvGetSubItemRect(HWND hwnd, int iItem, int iSub, int code);
void LvSetColumnWidth(HWND hwnd, int iCol, int cx);
void LvSetItemText(HWND hwnd, int i, int iSub, WStr text);
void LvSetItemText(HWND hwnd, int i, int iSub, Str text);
TempWStr LvGetItemTextTemp(HWND hwnd, int i, int iSub);
int LvHitTest(HWND hwnd, Point pt, UINT* flagsOut = nullptr);
DWORD LvSetExtendedStyle(HWND hwnd, DWORD ex);
int LvInsertColumn(HWND hwnd, int iCol, const LVCOLUMNW* col);

//--- combo box

void CbAddString(HWND, Str s);
void CbSetCurrentSelection(HWND, int);

//--- toolbar

void TbSetButtonStructSize(HWND hwnd, int size);
void TbAddButtons(HWND hwnd, int count, const TBBUTTON* buttons);
void TbAutoSize(HWND hwnd);
int TbGetButtonCount(HWND hwnd);
DWORD TbGetExtendedStyle(HWND hwnd);
void TbSetExtendedStyle(HWND hwnd, DWORD style);
Rect TbGetItemRect(HWND hwnd, int buttonIdx);

//--- tree view

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, uint flag, bool subtree);

//--- dialogs / message boxes

void MessageBoxWarningSimple(HWND hwnd, WStr msg, WStr title = WStr());
void MessageBoxNYI(HWND hwnd);
int MsgBox(HWND, Str text, Str caption, UINT flags);
HWND ShowTextInWindow(Str title, Str text, HWND* hwndPtr = nullptr);
void ShowTextInWindowDialog(Str title, Str text);

//--- GDI: draw / measure

void HdcDrawRect(HDC, const Rect&);
void HdcFillRect(HDC, const Rect&, HBRUSH);
void HdcFillRect(HDC hdc, const Rect&, Color);
void HdcFillRectWithBkColor(HDC hdc, const Rect& rect);
void HdcDrawLine(HDC, const Rect&);
int HdcDrawText(HDC hdc, Str s, const Rect& r, uint format, HFONT font = nullptr);
int HdcDrawText(HDC hdc, WStr s, const Rect& r, uint format, HFONT font = nullptr);
int HdcDrawText(HDC hdc, Str s, const Point& pos, uint format, HFONT font = nullptr);
int HdcDrawText(HDC hdc, WStr s, const Point& pos, uint format, HFONT font = nullptr);
bool HdcExTextOut(HDC hdc, Point pos, uint options, const Rect& rect, Str text);
bool HdcExTextOut(HDC hdc, Point pos, uint options, const Rect& rect, WStr text);
Size HdcMeasureText(HDC hdc, Str s, int maxDx, uint format, HFONT font);
void HdcDrawCenteredText(HDC hdc, Rect r, Str txt, bool isRTL = false);
Size HdcGetTextExtentPoint32(HDC hdc, Str str);
Size HdcGetTextExtentPoint32(HDC hdc, WStr str);
void HdcPaintCheckerboard(HDC hdc, int x, int y, int w, int h);
int HdcMeasureStringWidth(HDC hdc, WStr str);

//--- GDI: fonts

int GetSizeOfDefaultGuiFont();
bool GetNonClientMetricsForDpi(int dpi, NONCLIENTMETRICS* ncm);

//--- GDI: handles / bitmaps / pixmaps

bool DeleteObjectSafe(HGDIOBJ*);
bool DeleteBrushSafe(HBRUSH*);
bool DestroyIconSafe(HICON*);

struct RenderedBitmap;

// A Windows present-layer bitmap handle: an HBITMAP (+ optional file mapping) that can be
// blitted to an HDC. Concrete and Windows-only by design - portable pixel data lives in
// Pixmap; this is just the GDI handle the UI paints. Built from a Pixmap or an HBITMAP.
struct RenderedBitmap {
    Size size;
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

void UpdateBitmapColors(HBITMAP hbmp, Color textColor, Color bgColor, Color linkColor = 0,
                        Vec<Rect>* skipRects = nullptr);
HBITMAP CreateMemoryBitmap(Size size, HANDLE* hDataMapping = nullptr);
bool BlitHBITMAP(HBITMAP hbmp, HDC hdc, Rect target);

//--- double-buffer / deferred window positioning

struct DoubleBuffer {
    HWND hTarget = nullptr;
    HDC hdcCanvas = nullptr;
    HDC hdcBuffer = nullptr;
    HBITMAP doubleBuffer = nullptr;
    Rect rect;

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

//--- DC state

struct SavedDCState {
    HWND hwnd;
    HDC hdc;
    HFONT oldFont;
};

SavedDCState SaveDCState(HWND hwnd);
void RestoreDCState(SavedDCState* state);

//--- clipboard

bool OpenClipboardForUpdate();
void CloseClipboardAfterUpdate();
bool CopyTextToClipboard(Str s);
bool AppendTextToClipboard(Str s);
bool CopyImageToClipboard(HBITMAP hbmp, bool appendOnly);

//--- menus

void MenuSetChecked(HMENU m, int id, bool isChecked);
bool MenuSetEnabled(HMENU m, int id, bool isEnabled);
void MenuRemove(HMENU m, int id);
void MenuEmpty(HMENU m);
void MenuSetText(HMENU m, int id, WStr s);
void MenuSetText(HMENU m, int id, Str s);
TempStr MenuToSafeStringTemp(Str s);

//--- keyboard state

bool IsKeyPressed(int key);
bool IsShiftPressed();
bool IsAltPressed();
bool IsCtrlPressed();

//--- cursors / mouse tracking

HCURSOR GetCachedCursor(LPWSTR id);
void SetCursorCached(LPWSTR id);
void DeleteCachedCursors();
bool TrackMouseLeave(HWND);

//--- handles

bool IsValidHandle(HANDLE);
bool SafeCloseHandle(HANDLE*);
bool SafeFindClose(HANDLE*);

//--- OS / process / CPU

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
double GetProcessRunningTime();
DWORD GetAccountType();
DWORD GetOriginalAccountType();
bool IsProcessRunningElevated();
TempStr GetParentProcessPath(DWORD* pidOut = nullptr);
bool CanTalkToProcess(DWORD procId);
void DisableDataExecution();
void MaskFpExceptions();

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

//--- environment / errors / paths

TempStr GetEnvVariableTemp(Str name);
TempStr GetLastErrorStrTemp(DWORD& err);
void LogLastError(DWORD err = 0);
void DbgOutLastError(DWORD err = 0);
Str GetLastErrorAsStr(Arena* arena);
TempStr GetSpecialFolderTemp(int csidl, bool createIfMissing = false);
TempStr GetTempDirTemp();
Str GetAppLocalDataDirTemp();
void ChangeCurrDirToDocuments();
TempStr ResolveLnkTemp(Str path);
bool CreateShortcut(Str shortcutPath, Str exePath, Str args = Str(), Str description = Str(), int iconIndex = 0);
IDataObject* GetDataObjectForFile(Str filePath, HWND hwnd = nullptr);
void AddPathToRecentDocs(Str path);

//--- process launch / shell

HANDLE LaunchProcessWithCmdLine(Str exe, Str cmdLine);
HANDLE LaunchProcessInDir(Str cmdLine, Str currDir = Str(), DWORD flags = 0);
bool CreateProcessHelper(Str exe, Str args);
bool LaunchFileShell(Str path, Str params = Str(), Str verb = Str(), bool hidden = false);
bool LaunchBrowser(Str url);
void OpenPathInDefaultFileManager(Str path);
void RunNonElevated(Str exePath);
bool LaunchElevated(Str path, Str cmdline);

//--- console

bool RedirectIOToConsole();
bool RedirectIOToExistingConsole();
void HandleRedirectedConsoleOnShutdown();
void InitConsoleOutput();
void LogConsole(Str s);
void WaitForConsoleClose();
void SendEnterIfLoggedToConsole();
bool WasLaunchedByPowershellWithPipeRedirect();

//--- registry

TempStr RegKeyNameTemp(HKEY key);
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

//--- COM / streams / DDE / DLL servers

IStream* CreateStreamFromData(const Str&);
Str ReadIStream(IStream* stream);
uint GuessTextCodepage(Str data, uint defVal = CP_ACP);
TempStr NormalizeString(Str str, int /* NORM_FORM */ form);
void VariantInitBstr(VARIANT& urlVar, WStr s);
bool DDEExecute(WStr server, WStr topic, WStr command);
bool RegisterServerDLL(Str dllPath, Str args = Str());
bool UnRegisterServerDLL(Str dllPath, Str args = Str());
bool RegisterOrUnregisterServerDLL(Str dllPath, bool install, Str args = Str());

//--- resources / instance / common controls

void InitAllCommonControls();
void FillWndClassEx(WNDCLASSEX& wcex, WStr clsName, WNDPROC wndproc);
HINSTANCE GetInstance();
Size ButtonGetIdealSize(HWND hwnd);
bool IsValidDelayType(int type);

struct LoadedDataResource {
    const u8* data = nullptr;
    int dataSize = 0;
};
bool LockDataResource(int resId, LoadedDataResource*);

//--- HGLOBAL / atoms

TempStr HGLOBALToStrTemp(HGLOBAL h, bool isUnicode);
HGLOBAL MemToHGLOBAL(void* src, int n, UINT flags = GMEM_MOVEABLE);
HGLOBAL StrToHGLOBAL(Str s, UINT flags = GMEM_MOVEABLE);
TempStr AtomToStrTemp(ATOM a);

//--- timing

LARGE_INTEGER TimeNow();
double TimeDiffSecs(const LARGE_INTEGER& start, const LARGE_INTEGER& end);
double TimeDiffMs(const LARGE_INTEGER& start, const LARGE_INTEGER& end);

//--- misc

TempStr GetDefaultPrinterNameTemp();
int GetMeasurementSystem();
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
