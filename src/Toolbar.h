/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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
struct VirtIconButton;
struct Edit;
struct ILayout;

void CreateToolbar(MainWindow*);
void ReCreateToolbar(MainWindow* win);
void DestroyToolbar(MainWindow* win);
void ToolbarUpdateStateForWindow(MainWindow*, bool setButtonsVisibility);
void UpdateToolbarButtonsToolTipsForWindow(MainWindow*);
void UpdateToolbarFindText(MainWindow*);
void UpdateToolbarPageText(MainWindow*, int pageCount, bool updateOnly = false);
void UpdateFindbox(MainWindow*);
void SetToolbarButtonEnableState(MainWindow*, int cmdId, bool isEnabled);
void SetToolbarButtonCheckedState(MainWindow*, int cmdId, bool isChecked);
bool ShouldShowToolbar(MainWindow*);
bool ShouldOverlayToolbar(MainWindow*);
void ShowOrHideToolbar(MainWindow*);
void PositionOverlayToolbar(MainWindow*);
void UpdateOverlayToolbarForMouse(MainWindow*);
void RevealOverlayToolbar(MainWindow*);

// delay before the overlay toolbar hides after the mouse moves away
constexpr int kDelayToolbarHide = 500;
void UpdateToolbarState(MainWindow*);
void UpdateToolbarAfterThemeChange(MainWindow*);
Rect GetToolbarButtonScreenRect(MainWindow*, int cmdId);
void ToolbarNoteDropdownClosed();
void TogglePdfAnnotationsToolbar(MainWindow*);
void EnablePdfAnnotationsToolbar(MainWindow*);
int ToolbarIconSize();

TempStr ToolbarButtonsResultTemp(int* exitCodeOut);

//--- hover drop-down

// A toolbar button can open a drop-down when the mouse rests on it, after the
// same delay a tooltip takes. The content is any layout, so a caller can put
// whatever it likes in there; NewToolbarHoverMenu() builds the menu-like rows
// most of them want.

// one item: an icon, a label and the command a click runs. NewToolbarHoverMenu()
// makes each a menu-like row, NewToolbarHoverStrip() a cell in a pyramid.
struct ToolbarHoverMenuItem {
    Str svgIcon;
    Str text;
    int cmdId = 0;
    bool enabled = true;
    // the one in use, e.g. the current zoom level: the strip boxes it
    bool isCurrent = false;
};

// Built every time the drop-down opens, so it shows the current state.
struct ToolbarHoverBuildEvent {
    MainWindow* win = nullptr;
    // out: the drop-down's content; the drop-down takes ownership
    ILayout* layout = nullptr;
    // out: optional. Hang the drop-down off the middle of the button, instead
    // of lining its left edge up with the button's
    bool centerOnButton = false;
};

// buttons armed with the same non-zero groupId share one drop-down: moving the
// mouse from one of them to another keeps it exactly where it is, instead of
// closing it and opening it again under the other button
void SetToolbarHoverDropdown(MainWindow*, int cmdId, const Func1<ToolbarHoverBuildEvent*>&, int groupId = 0);
ILayout* NewToolbarHoverMenu(MainWindow*, const Vec<ToolbarHoverMenuItem>&);
// the items as a compact pyramid of labels rather than a column of menu rows:
// the widest row on top holds the middle of the list, each row below it what
// surrounds the middle, the last one the two ends
ILayout* NewToolbarHoverStrip(MainWindow*, const Vec<ToolbarHoverMenuItem>&);
void HideToolbarHoverDropdown(MainWindow*);
bool ToolbarHoverDropdownContainsScreenPoint(MainWindow*, Point);

//--- shared between Toolbar.cpp and Toolbar_win.cpp, not meant for anyone else

// those are not real commands but we have to refer to toolbar buttons
// is by a command. those are just background for area to be
// covered by other HWNDs. They need the right size
constexpr int PageInfoId = (int)CmdLast + 16;
constexpr int WarningMsgId = (int)CmdLast + 17;

// the overlay toolbar's delayed-hide timer, on the toolbar's own host
constexpr int kHideOverlayToolbarTimerId = 0x101;
// the hover drop-down's timers, also on the toolbar's own host
constexpr int kOpenHoverDropdownTimerId = 0x102;
constexpr int kCloseHoverDropdownTimerId = 0x103;

// one row or cell of the drop-down that is up, for the -dbg-control dump
struct ToolbarHoverItemState {
    VirtCtrl* ctrl = nullptr; // not owned, the drop-down's layout owns it
    Str text;                 // not owned, the ctrl's own copy
    int cmdId = 0;
    bool isCurrent = false;
    bool isStripCell = false; // from NewToolbarHoverStrip(), so ctrl is a cell
    // in the strip pyramid's right half, i.e. past the middle of the list; that
    // half is painted on its own background
    bool isRightHalf = false;
};

// a button that opens a drop-down when the mouse rests on it
struct ToolbarHoverReg {
    int cmdId = 0;
    int groupId = 0;
    Func1<ToolbarHoverBuildEvent*> build;
};

struct ToolbarVirt {
    // the toolbar's window; owns the layout tree and the virtual controls
    VirtHost* host = nullptr;
    Vec<VirtCtrl*> items; // not owned; the layout owns them
    Vec<VirtCtrl*> annotationItems;
    ILayout* annotationRow = nullptr;
    VirtText* pageLabel = nullptr;
    VirtText* pageLabel2 = nullptr; // "Page:" before pageEdit, only for HasChapters() docs
    VirtText* pageTotal = nullptr;
    VirtText* chapterTotal = nullptr; // only for HasChapters() docs
    PlatformFont* platformFont = nullptr;
    int iconSize = 0;
    int rowDy = 0;

    // buttons with a hover drop-down, and the one currently open (if any)
    Vec<ToolbarHoverReg> hoverRegs;
    // what the drop-down that is up is showing; empty when none is
    Vec<ToolbarHoverItemState> hoverItems;
    VirtHost* hoverHost = nullptr;
    int hoverCmdId = 0;
    int hoverPendingCmdId = 0;
    // the open button's tooltip, taken away for as long as the drop-down is up
    Str hoverSavedTip;
};

// implemented in Toolbar.cpp
Color TbTextColor();
VirtCtrl* ToolbarItemFromPoint(MainWindow*, Point);

// implemented in Toolbar_win.cpp
Edit* ToolbarCreatePageEdit(MainWindow*, PlatformFont*, int iconDy);
Edit* ToolbarCreateChapterEdit(MainWindow*, PlatformFont*, int iconDy);
void ToolbarSetNativeHooks(MainWindow*, VirtHost*);
void ToolbarUpdateFindEditCursor(MainWindow*);
Rect ToolbarCanvasRectInFrame(MainWindow*);
Point ToolbarScreenToFrame(MainWindow*, Point);
void ToolbarRepaintUncovered(MainWindow*, Rect rInFrame);
void ToolbarFocusFrame(MainWindow*);
bool ToolbarFrameIsVisible(MainWindow*);
void ToolbarPostCommand(MainWindow*, int cmdId);
void ToolbarSetHeight(MainWindow*, int dy);
