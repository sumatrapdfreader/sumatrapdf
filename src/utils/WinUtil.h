/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#define NO_COLOR (COLORREF) - 1

#define WIN_COL_WHITE RGB(0xff, 0xff, 0xff)
#define WIN_COL_BLACK RGB(0, 0, 0)

#define DRAGQUERY_NUMFILES 0xFFFFFFFF

bool ToBool(BOOL b);

RECT ClientRECT(HWND);
Rect ClientRect(HWND);
Rect WindowRect(HWND);
Rect MapRectToWindow(Rect, HWND hwndFrom, HWND hwndTo);

void EditSelectAll(HWND);
int EditIdealDy(HWND, bool hasBorder, int lines = 1);
void EditImplementCtrlBack(HWND hwnd);

void ListBox_AppendString_NoSort(HWND, const WCHAR*);
int ListBoxGetTopIndex(HWND);
bool ListBoxSetTopIndex(HWND, int);

bool IsValidHandle(HANDLE);
bool SafeCloseHandle(HANDLE*);
void FillWndClassEx(WNDCLASSEX& wcex, const WCHAR* clsName, WNDPROC wndproc);
void MoveWindow(HWND hwnd, Rect rect);
void MoveWindow(HWND hwnd, RECT* r);

bool IsOs64();
bool IsProcess64();
bool IsArmBuild();
bool IsRunningInWow64();
bool IsProcessAndOsArchSame();

bool GetOsVersion(OSVERSIONINFOEX& ver);
TempStr OsNameFromVerTemp(const OSVERSIONINFOEX& ver);
TempStr GetWindowsVerTemp();

TempStr GetLastErrorStrTemp(DWORD err = 0);
void LogLastError(DWORD err = 0);
void DbgOutLastError(DWORD err = 0);

// registry
const char* RegKeyNameTemp(HKEY key);
const char* RegKeyNameWTemp(HKEY key);
bool RegKeyExists(HKEY keySub, const char* keyName);
char* ReadRegStrTemp(HKEY keySub, const char* keyName, const char* valName);
char* LoggedReadRegStrTemp(HKEY keySub, const char* keyName, const char* valName);
char* ReadRegStr2Temp(const char* keyName, const char* valName);
char* LoggedReadRegStr2Temp(const char* keyName, const char* valName);
bool WriteRegStr(HKEY keySub, const char* keyName, const char* valName, const char* value);
bool LoggedWriteRegStr(HKEY keySub, const char* keyName, const char* valName, const char* value);
bool ReadRegDWORD(HKEY keySub, const char* keyName, const char* valName, DWORD& value);
bool WriteRegDWORD(HKEY keySub, const char* keyName, const char* valName, DWORD value);
bool LoggedWriteRegDWORD(HKEY keySub, const char* keyName, const char* valName, DWORD value);
bool LoggedWriteRegNone(HKEY hkey, const char* key, const char* valName);
bool CreateRegKey(HKEY keySub, const char* keyName);
bool DeleteRegKey(HKEY keySub, const char* keyName, bool resetACLFirst = false);
bool LoggedDeleteRegKey(HKEY keySub, const char* keyName, bool resetACLFirst = false);
bool DeleteRegValue(HKEY keySub, const char* keyName, const char* val);
bool LoggedDeleteRegValue(HKEY keySub, const char* keyName, const char* val);
HRESULT CLSIDFromString(const char* lpsz, LPCLSID pclsid);

// file and directory operations
TempStr GetSpecialFolderTemp(int csidl, bool createIfMissing = false);
TempStr GetTempDirTemp();
TempStr GetExePathTemp();
TempStr GetExeDirTemp();
void ChangeCurrDirToDocuments();
int FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2);
char* ResolveLnkTemp(const char* path);
bool CreateShortcut(const char* shortcutPath, const char* exePath, const char* args = nullptr,
                    const char* description = nullptr, int iconIndex = 0);
IDataObject* GetDataObjectForFile(const char* filePath, HWND hwnd = nullptr);

HANDLE LaunchProces(const char* exe, const char* cmdLine);
HANDLE LaunchProcess(const char* cmdLine, const char* currDir = nullptr, DWORD flags = 0);
bool CreateProcessHelper(const char* exe, const char* args);
bool LaunchFile(const char* path, const char* params = nullptr, const char* verb = nullptr, bool hidden = false);
bool LaunchBrowser(const char* url);

bool LaunchElevated(const char* path, const char* cmdline);
bool IsProcessRunningElevated();
bool CanTalkToProcess(DWORD procId);
DWORD GetAccountType();
DWORD GetOriginalAccountType();

void DisableDataExecution();
bool RedirectIOToConsole();
bool RedirectIOToExistingConsole();
void HandleRedirectedConsoleOnShutdown();

bool IsKeyPressed(int key);
bool IsShiftPressed();
bool IsAltPressed();
bool IsCtrlPressed();

Rect ShiftRectToWorkArea(Rect rect, HWND hwnd = nullptr, bool bFully = false);
Rect GetWorkAreaRect(Rect rect, HWND hwnd);
void LimitWindowSizeToScreen(HWND hwnd, SIZE& size);
Rect GetFullscreenRect(HWND);
Rect GetVirtualScreenRect();

void DrawRect(HDC, const Rect&);
void FillRect(HDC, const Rect&, HBRUSH);
void FillRect(HDC hdc, const Rect&, COLORREF);
void DrawLine(HDC, const Rect&);

void DrawCenteredText(HDC hdc, Rect r, const WCHAR* txt, bool isRTL = false);
void DrawCenteredText(HDC hdc, Rect r, const char* txt, bool isRTL = false);
void DrawCenteredText(HDC, const RECT& r, const WCHAR* txt, bool isRTL = false);
Size HwndMeasureText(HWND hwnd, const char* txt, HFONT font = nullptr);

int HdcDrawText(HDC hdc, const char* s, RECT* r, uint format, HFONT font = nullptr);
int HdcDrawText(HDC hdc, const char* s, const Rect& r, uint format, HFONT font = nullptr);
int HdcDrawText(HDC hdc, const char* s, const Point& pos, uint fmt, HFONT font = nullptr);
Size HdcMeasureText(HDC hdc, const char* s, uint format, HFONT font = nullptr);
Size HdcMeasureText(HDC hdc, const char* s, HFONT font = nullptr);

bool IsFocused(HWND);
bool IsCursorOverWindow(HWND);
Point HwndGetCursorPos(HWND hwnd);
int MapWindowPoints(HWND, HWND, Point*, int);
void HwndScreenToClient(HWND, Point&);
void HwndMakeVisible(HWND);

bool IsMouseOverRect(HWND hwnd, const Rect& r);
void CenterDialog(HWND hDlg, HWND hParent = nullptr);

char* GetDefaultPrinterNameTemp();

bool CopyTextToClipboard(const char*);
bool AppendTextToClipboard(const char*);

bool CopyImageToClipboard(HBITMAP hbmp, bool appendOnly);

bool IsWindowStyleSet(HWND hwnd, DWORD flags);
bool IsWindowStyleExSet(HWND hwnd, DWORD flags);
void SetWindowStyle(HWND hwnd, DWORD flags, bool enable);
void SetWindowExStyle(HWND hwnd, DWORD flags, bool enable);

bool IsRtl(HWND hwnd);
void SetRtl(HWND hwnd, bool isRtl);

Rect ChildPosWithinParent(HWND);

HFONT GetMenuFont();
HFONT CreateSimpleFont(HDC hdc, const char* fontName, int fontSize);
HFONT GetDefaultGuiFont(bool bold = false, bool italic = false);
HFONT GetDefaultGuiFontOfSize(int size);
HFONT GetUserGuiFont(char* fontName, int size, int weightOffset);
int GetSizeOfDefaultGuiFont();
void DeleteCreatedFonts();

IStream* CreateStreamFromData(const ByteSlice&);
ByteSlice GetDataFromStream(IStream* stream, HRESULT* resOpt);
ByteSlice GetStreamOrFileData(IStream* stream, const char* filePath);
bool ReadDataFromStream(IStream* stream, void* buffer, size_t len, size_t offset = 0);
uint GuessTextCodepage(const char* data, size_t len, uint defVal = CP_ACP);
char* NormalizeString(const char* str, int /* NORM_FORM */ form);
void ResizeHwndToClientArea(HWND hwnd, int dx, int dy, bool hasMenu);
void ResizeWindow(HWND, int dx, int dy);

void MessageBoxWarningSimple(HWND hwnd, const WCHAR* msg, const WCHAR* title = nullptr);
void MessageBoxNYI(HWND hwnd);

// schedule WM_PAINT at window's leasure
void HwndScheduleRepaint(HWND hwnd);

// do WM_PAINT immediately
void RepaintNow(HWND hwnd);

bool RegisterServerDLL(const char* dllPath, const char* args = nullptr);
bool UnRegisterServerDLL(const char* dllPath, const char* args = nullptr);
bool RegisterOrUnregisterServerDLL(const char* dllPath, bool install, const char* args = nullptr);

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
void MenuSetText(HMENU m, int id, const WCHAR* s);
void MenuSetText(HMENU m, int id, const char* s);
TempStr MenuToSafeStringTemp(const char* s);

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

struct BlittableBitmap {
    Size size = {};

    BlittableBitmap(){};

    Size GetSize();

    virtual bool Blit(HDC hdc, Rect target) = 0;
    virtual bool IsValid() = 0;

    virtual ~BlittableBitmap(){};
};

struct RenderedBitmap : BlittableBitmap {
    HBITMAP hbmp = nullptr;
    HANDLE hMap = nullptr;

    RenderedBitmap(HBITMAP hbmp, Size size, HANDLE hMap = nullptr);
    ~RenderedBitmap() override;

    RenderedBitmap* Clone() const;
    HBITMAP GetBitmap() const;
    bool IsValid() override;
    bool Blit(HDC hdc, Rect target) override;
};

void InitAllCommonControls();
Size GetBitmapSize(HBITMAP hbmp);
BitmapPixels* GetBitmapPixels(HBITMAP hbmp);
void FinalizeBitmapPixels(BitmapPixels* bitmapPixels);
COLORREF GetPixel(BitmapPixels* bitmap, int x, int y);
void UpdateBitmapColors(HBITMAP hbmp, COLORREF textColor, COLORREF bgColor);
ByteSlice SerializeBitmap(HBITMAP hbmp);
HBITMAP CreateMemoryBitmap(Size size, HANDLE* hDataMapping = nullptr);
bool BlitHBITMAP(HBITMAP hbmp, HDC hdc, Rect target);
double GetProcessRunningTime();

void RunNonElevated(const char* exePath);
void VariantInitBstr(VARIANT& urlVar, const WCHAR* s);
ByteSlice LoadDataResource(int resId);
bool DDEExecute(const WCHAR* server, const WCHAR* topic, const WCHAR* command);

void RectInflateTB(RECT& r, int top, int bottom);
void DivideRectH(const RECT& r, int y, int dy, RECT& r1, RECT& r2, RECT& r3);
void DivideRectV(const RECT& r, int x, int dx, RECT& r1, RECT& r2, RECT& r3);

HCURSOR GetCachedCursor(LPWSTR id);
void SetCursorCached(LPWSTR id);
void DeleteCachedCursors();

int GetMeasurementSystem();
bool TrackMouseLeave(HWND);

HINSTANCE GetInstance();
Size ButtonGetIdealSize(HWND hwnd);
ByteSlice LockDataResource(int id);
bool IsValidDelayType(int type);

void HwndResizeClientSize(HWND, int, int);

size_t HwndGetTextLen(HWND hwnd);
TempWStr HwndGetTextWTemp(HWND hwnd);
TempStr HwndGetTextTemp(HWND hwnd);
void HwndSetText(HWND, const char* s);
void HwndSetText(HWND, const WCHAR*);
bool HwndHasFrameThickness(HWND hwnd);
bool HwndHasCaption(HWND hwnd);

HICON HwndGetIcon(HWND);
HICON HwndSetIcon(HWND, HICON);

void HwndInvalidate(HWND);

HFONT HwndGetFont(HWND);
void HwndSetFont(HWND, HFONT);

void HwndPositionToTheRightOf(HWND hwnd, HWND hwndRelative);
void HwndPositionInCenterOf(HWND hwnd, HWND hwndRelative);
void HwndSendCommand(HWND hwnd, int cmdId);
void HwndDestroyWindowSafe(HWND* hwnd);
void HwndToForeground(HWND hwnd);
void HwndSetVisibility(HWND hwnd, bool visible);

bool DeleteObjectSafe(HGDIOBJ*);
bool DestroyIconSafe(HICON*);

void TbSetButtonInfo(HWND hwnd, int buttonId, TBBUTTONINFO* info);
void TbGetPadding(HWND, int* padX, int* padY);
void TbSetPadding(HWND, int padX, int padY);
void TbGetMetrics(HWND hwnd, TBMETRICS* metrics);
void TbSetMetrics(HWND hwnd, TBMETRICS* metrics);
void TbGetRect(HWND hwnd, int buttonId, RECT* rc);

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, uint flag, bool subtree);
void AddPathToRecentDocs(const char*);

TempStr HGLOBALToStrTemp(HGLOBAL h, bool isUnicode);
HGLOBAL MemToHGLOBAL(void* src, int n, UINT flags = GMEM_MOVEABLE);
HGLOBAL StrToHGLOBAL(const char* s, UINT flags = GMEM_MOVEABLE);
TempStr AtomToStrTemp(ATOM a);
