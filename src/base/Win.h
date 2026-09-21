/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

constexpr UINT DRAGQUERY_NUMFILES = 0xFFFFFFFF;

//--- bool / BOOL

bool ToBool(BOOL b);

inline BOOL toBOOL(bool b) {
    return b ? TRUE : FALSE;
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
Rect ChildPosWithinParent(HWND);

//--- HWND: screen / work area / placement

Rect ShiftRectToWorkArea(Rect rect, HWND hwnd = nullptr, bool bFully = false);
Rect GetWorkAreaRect(Rect rect, HWND hwnd);
Size HwndLimitSizeToScreen(HWND hwnd, Size size);
void HwndEnsureOnScreen(HWND hwnd);
Rect HwndGetFullscreenRect(HWND);
Rect GetVirtualScreenRect();
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

//--- HWND: focus / visibility / Z-order

HWND HwndSetFocus(HWND hwnd);
HWND HwndThreadFocus();
bool IsThreadInMenuMode();
bool HwndSetFocusForce(HWND hwnd);
bool HwndIsFocused(HWND);
bool HwndIsOnScreenKeyboard(HWND);
bool HwndIsVisible(HWND hwnd);
void HwndSetVisible(HWND hwnd, bool visible);
void HwndToForeground(HWND hwnd);

//--- HWND: styles / RTL / chrome

bool HwndIsWindowStyleSet(HWND hwnd, DWORD flags);
void HwndSetWindowStyle(HWND hwnd, DWORD flags, bool enable);
void HwndSetWindowExStyle(HWND hwnd, DWORD flags, bool enable);
bool HwndIsRtl(HWND hwnd);
void HwndSetRtl(HWND hwnd, bool isRtl);

//--- HWND: text / font / icon / paint

int HwndGetTextLen(HWND hwnd);
TempWStr HwndGetTextWTemp(HWND hwnd);
TempStr HwndGetTextTemp(HWND hwnd);
void HwndSetText(HWND, Str s);
void HwndSetDlgItemText(HWND, int, Str s);
void HwndSetFont(HWND, HFONT);
void HwndSetFontForWindowAndItsChildren(HWND, HFONT);
void HwndSetTreeFontForDpi(HWND hwndTree, HFONT font, int dpi);
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
// all no-op (or return a zero value) on a null hwnd. gui/win/WinGui.h overloads
// them on Edit*, which is what code holding a control should call

void EditSelectAll(HWND);
void EditSelectText(HWND hwnd, int start, int end);
void EditGetSelection(HWND hwnd, int& start, int& end);
void EditSetCursorPos(HWND hwnd, int pos);
void EditSetCursorPosAtEnd(HWND hwnd);
int EditGetTextLen(HWND hwnd);
void EditSetModified(HWND hwnd, bool);
bool EditIsModified(HWND hwnd);
void EditSetCueText(HWND hwnd, Str);
void EditSetMargins(HWND hwnd, int left, int right);
void EditSetNumbersOnly(HWND hwnd, bool);
void EditSetPasswordVisible(HWND hwnd, bool);

//--- list box

int LbAddString(HWND hwnd, WStr text);
int LbAddString(HWND hwnd, Str text);
int LbGetCurrentSelection(HWND hwnd);
bool LbSetCurrentSelection(HWND hwnd, int idx);
TempWStr LbGetTextTemp(HWND hwnd, int idx);
void LbSetItemHeight(HWND hwnd, int idx, int height);

//--- list view

//--- combo box
// all no-op (or return a zero value) on a null hwnd. gui/win/WinGui.h overloads
// them on DropDown*, which is what code holding a control should call

void CbResetContent(HWND);
void CbAddString(HWND, Str s);
int CbGetItemsCount(HWND);
void CbInsertString(HWND, int idx, Str s);
void CbDeleteString(HWND, int idx);
void CbSetCueBanner(HWND, Str);
void CbSetMinVisible(HWND, int n);
void CbSetItemHeight(HWND, int idx, int dy);
int CbGetTextLen(HWND);
bool CbIsDropped(HWND);
// which item of the drop-down list is selected, -1 for none
int CbGetCurrentSelection(HWND);
void CbSetCurrentSelection(HWND, int);

// the edit an editable (CBS_DROPDOWN) combo keeps its text in. CbEdit*Selection
// is the text selected in there, as opposed to CbGetCurrentSelection above
HWND CbEditHwnd(HWND);
void CbEditSelectAll(HWND);
void CbEditSelectText(HWND, int start, int end);
void CbEditGetSelection(HWND, int& start, int& end);
void CbEditSetModified(HWND, bool);
bool CbEditIsModified(HWND);

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
int MsgBox(HWND, Str text, Str caption, UINT flags);
HWND ShowTextInWindow(Str title, Str text, HWND* hwndPtr = nullptr);
void ShowTextInWindowDialog(Str title, Str text);

//--- GDI: draw / measure

void HdcDrawRect(HDC, const Rect&);
void HdcFillRect(HDC, const Rect&, HBRUSH);
void HdcFillRect(HDC hdc, const Rect&, Color);
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

//--- GDI: fonts

bool GetNonClientMetricsForDpi(int dpi, NONCLIENTMETRICS* ncm);

//--- GDI: handles / bitmaps / pixmaps

bool DeleteObjectSafe(HGDIOBJ*);
bool DeleteBrushSafe(HBRUSH*);

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
};

HBITMAP CreateMemoryBitmap(Size size, HANDLE* hDataMapping = nullptr);

inline bool IsPrinterDC(HDC hdc) {
    int tech = GetDeviceCaps(hdc, TECHNOLOGY);
    return tech == DT_RASPRINTER || tech == DT_PLOTTER;
}

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
    void MoveWindow(HWND hWnd, Rect r);
    // A transparent WebView canvas growing into a sibling's old rectangle must
    // discard those screen bits or the sibling remains visible until composition.
    void MoveWindowNoCopyBits(HWND hWnd, Rect r);
};

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
bool IsRightButtonPressed();
int ReleaseThreadKeyState();

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
bool IsProcessRunningElevated();
bool CanTalkToProcess(DWORD procId);
void DisableDataExecution();
void MaskFpExceptions();

constexpr u32 kCpuMMX = 1 << 0;
constexpr u32 kCpuSSE = 1 << 1;
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
TempStr CpuFeaturesTemp();

//--- environment / errors / paths

TempStr GetEnvVariableTemp(Str name);
TempStr GetLastErrorStrTemp(DWORD& err);
void LogLastError(DWORD err = 0);
Str GetLastErrorAsStr(Arena* arena);
TempStr GetSpecialFolderTemp(int csidl, bool createIfMissing = false);
// initialCch is only a starting guess; tests pass a tiny value to force the retry
TempStr GetTempDirTemp(int initialCch = MAX_PATH);
void ChangeCurrDirToDocuments();
TempStr ResolveLnkTemp(Str path);
bool CreateShortcut(Str shortcutPath, Str exePath, Str args = Str(), Str description = Str(), int iconIndex = 0);
IDataObject* GetDataObjectForFile(Str filePath, HWND hwnd = nullptr);
void AddPathToRecentDocs(Str path);
void ListDriveRoots(StrVec& out);
bool ListShellQuickAccess(StrVec& dirsOut, StrVec& filesOut);

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
void LogConsole(Str s);
bool WasLaunchedByPowershellWithPipeRedirect();

//--- registry

extern bool gLogRegistryCalls;

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
bool WriteRegNone(HKEY hkey, Str key, Str valName);
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
bool LockDataResource(int resId, LoadedDataResource*, HMODULE mod = nullptr);

//--- HGLOBAL / atoms

TempStr HGLOBALToStrTemp(HGLOBAL h, bool isUnicode);
HGLOBAL MemToHGLOBAL(void* src, int n, UINT flags = GMEM_MOVEABLE);
TempStr AtomToStrTemp(ATOM a);

//--- timing

//--- misc

TempStr GetDefaultPrinterNameTemp();
int GetMeasurementSystem();
