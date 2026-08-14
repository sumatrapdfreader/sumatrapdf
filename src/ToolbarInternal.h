/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Shared between Toolbar.cpp and Toolbar_win.cpp, not meant for anyone else.
//
// Toolbar.cpp owns the toolbar: which buttons exist and in which order, whether
// they are available / enabled / checked, their tooltips and icons, the layout
// tree they live in, and when the overlay toolbar shows and hides. It reaches
// its window through VirtHost, so it names no OS windowing API.
//
// Toolbar_win.cpp owns what is left of Win32: the native page-number edit, the
// messages VirtHost doesn't model (the edit's colors, dragging the frame by the
// toolbar), eating the click that dismissed a drop-down, and the handful of
// calls that reach the frame and canvas windows, which are not hosts yet.

struct MainWindow;
struct PlatformFont;
struct VirtCtrl;
struct VirtText;
struct VirtHost;
struct Edit;

// those are not real commands but we have to refer to toolbar buttons
// is by a command. those are just background for area to be
// covered by other HWNDs. They need the right size
constexpr int PageInfoId = (int)CmdLast + 16;
constexpr int WarningMsgId = (int)CmdLast + 17;

// the overlay toolbar's delayed-hide timer, on the toolbar's own host
constexpr int kHideOverlayToolbarTimerId = 0x101;

struct ToolbarVirt {
    // the toolbar's window; owns the layout tree and the virtual controls
    VirtHost* host = nullptr;
    Vec<VirtCtrl*> items; // not owned; the layout owns them
    VirtText* pageLabel = nullptr;
    VirtText* pageTotal = nullptr;
    PlatformFont* platformFont = nullptr;
    int iconSize = 0;
    int rowDy = 0;
};

// implemented in Toolbar.cpp
Color TbTextColor();
VirtCtrl* ToolbarItemFromPoint(MainWindow*, Point);

// implemented in Toolbar_win.cpp
Edit* ToolbarCreatePageEdit(MainWindow*, PlatformFont*, int iconDy);
void ToolbarSetNativeHooks(MainWindow*, VirtHost*);
void ToolbarUpdateFindEditCursor(MainWindow*);
Rect ToolbarCanvasRectInFrame(MainWindow*);
Point ToolbarScreenToFrame(MainWindow*, Point);
void ToolbarRepaintUncovered(MainWindow*, Rect rInFrame);
void ToolbarFocusFrame(MainWindow*);
bool ToolbarFrameIsVisible(MainWindow*);
void ToolbarPostCommand(MainWindow*, int cmdId);
