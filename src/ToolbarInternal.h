/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Shared between Toolbar.cpp and Toolbar_win.cpp, not meant for anyone else.
//
// Toolbar.cpp owns the toolbar's content and rules: which buttons exist and in
// which order, whether they are available / enabled / checked, their tooltips
// and icons, and the layout tree they are laid out in.
//
// Toolbar_win.cpp owns the toolbar's window: its window class and message loop,
// its creation and destruction, the native edit control for the page number,
// and everything that manipulates window handles (positioning the overlay
// toolbar, hit-testing against the cursor, timers, the message queue).

struct MainWindow;
struct PlatformFont;
struct VirtCtrl;
struct VirtText;
struct VirtRoot;
struct ILayout;
struct Edit;

// those are not real commands but we have to refer to toolbar buttons
// is by a command. those are just background for area to be
// covered by other HWNDs. They need the right size
constexpr int PageInfoId = (int)CmdLast + 16;
constexpr int WarningMsgId = (int)CmdLast + 17;

struct ToolbarVirt {
    ILayout* layout = nullptr;
    VirtRoot* vroot = nullptr;
    Vec<VirtCtrl*> items; // not owned; layout owns them
    VirtText* pageLabel = nullptr;
    VirtText* pageTotal = nullptr;
    PlatformFont* platformFont = nullptr;
    int iconSize = 0;
    int rowDy = 0;
};

// implemented in Toolbar.cpp
Color TbBgColor();
Color TbTextColor();
Color TbDisabledColor();
Color TbEdgeColor();
int ToolbarIconSize();
int ToolbarRowDy(int iconSize);
void BuildToolbarLayout(MainWindow*);
VirtCtrl* ToolbarItemFromPoint(MainWindow*, Point);
void ToolbarNoteDropdownClosed();

// implemented in Toolbar_win.cpp
void RelayoutToolbar(MainWindow*);
Edit* ToolbarCreatePageEdit(MainWindow*, PlatformFont*, int iconDy);
