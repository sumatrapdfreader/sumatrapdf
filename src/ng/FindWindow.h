/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
struct Window;
} // namespace gpui

struct MainWindow;
struct FindWindowUI;
struct ToolWindow;

// ng: orig's FindWindowWnd is an owned WS_POPUP | WS_THICKFRAME tool window.
// Here the floating find UI is a card inside the frame, dragged by its header
// and remembered in gSettings->searchUIWindowPos, so this holds its state.
struct FindWindowWnd {
    FindWindowUI* ui = nullptr;
    // orig's window, where the platform can have one (gui/ToolWindow.h);
    // null: a card in the frame
    ToolWindow* tw = nullptr;
    bool visible = false;
    Str status;
    int statusTotalHits = -1;
    bool statusCapped = false;
    bool wantFocus = false;
    bool wantSelectAll = false;
    // selected row of the results list, and the match it stands for, so a
    // rebuilt list can restore the selection by identity (orig's savedSel*)
    int sel = -1;
    int savedSelPage = -1;
    int savedSelGlyph = -1;
    float scrollY = 0;
    // frame-relative placement; dx/dy <= 0 means "not placed yet"
    Rect pos;
    bool dragging = false;
    Point dragStart;
    Point dragOrigin;

    ~FindWindowWnd();
};

void DeleteFindWindow(MainWindow* win);
void ShowFindWindow(MainWindow* win);
void HideFindWindow(MainWindow* win);
bool IsFindWindowVisible(MainWindow* win);
void FindWindowSetStatus(MainWindow* win, Str s, int totalHits = -1);
void FindWindowRefreshResults(MainWindow* win, bool allowNavigation = true);
void FindWindowSaveSelectedMatch(MainWindow* win);
int FindWindowFontHeight(MainWindow* win);
void FindWindowApplyDpi(MainWindow* win);
// Enter / F3 / the Next-Prev buttons walk the results list when it is for the
// current term; otherwise a new search starts. Returns false if not handled.
bool FindWindowNextOrPrev(MainWindow* win, bool forward);
// orig's FindWindowWnd::OnKeyDown: F3 and the keys borrowed from the search edit
bool FindWindowOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift, bool alt);
gpui::El* FindWindowBuild(MainWindow* win, gpui::Ctx* cx);
gpui::Window* FindWindowHostGpui(MainWindow* win);
bool FindWindowHasKeyboard(MainWindow* win);
// what the scripted tests read back (orig's TestFindWindowContents)
TempStr FindWindowContentsResultTemp(int maxRows, int* exitCodeOut);
TempStr FindResultPageColumnClipResultTemp(int* exitCodeOut = nullptr);
TempStr FindResultsOrderResultTemp(Str term, int startPage, int* exitCodeOut = nullptr);
