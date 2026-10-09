/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's FindBar.cpp is a WS_POPUP window owned by the frame, with a
// DropDown, a VirtText status slot and six VirtIconButtons. Here the bar is an
// absolutely positioned gpui card at the top right of the canvas, built every
// frame from this state. The floating find window (orig FindWindow.cpp, the
// SearchUIFloating variant with the results list) is not ported.

namespace gpui {
struct Ctx;
struct El;
struct InputState;
} // namespace gpui

struct MainWindow;
struct FindBarWnd;
struct PlatformFont;
// the gpui view entity of this bar; defined in FindBar.cpp because this header
// is included by files that don't have gpui.h
struct FindBarUI;

struct FindBar {
    FindBarUI* ui = nullptr;
    bool visible = false;
    // ng: orig tracks "the user edited the box" with the combo box's own
    // modify flag (CbEditIsModified); gpui's InputState has none
    bool editModified = false;
    // "n / m" or "12 34" while a count scan runs; what FindBarSetStatus sets
    Str status;
    // how many matches the status slot was sized for, so the text field next
    // to it doesn't resize on every count update
    int statusTotalHits = 0;
    bool statusCapped = false;
    // the find box should take the focus on the next frame (gpui focuses by
    // handle, and the element only exists once it has been rendered)
    bool wantFocus = false;
    bool wantSelectAll = false;
    // width of the search field, set by dragging the bar's left edge; 0 for
    // the default. Per window and not saved, as orig's barDx
    int editDx = 0;

    ~FindBar();
};

gpui::El* FindBarBuild(MainWindow* win, gpui::Ctx* cx);

void DeleteFindBar(MainWindow* win);
void ShowFindBar(MainWindow* win);
void HideFindBar(MainWindow* win);
bool IsFindBarVisible(MainWindow* win);
bool IsFindUIVisible(MainWindow* win);
void FindBarReposition(MainWindow* win);
void FindBarSetStatus(MainWindow* win, Str s, int totalHits = -1);
void FocusFindEditSelectAll(MainWindow* win);
// the find text field, shared by the compact bar and the floating window
gpui::InputState* EnsureFindEdit(MainWindow* win);
// switch between the compact bar and the floating window (SearchUIFloating)
void ToggleFloatingFindUI(MainWindow* win);

// ng: orig talks to the find box through the win32 combo box helpers
// (CbGetTextLen, CbEditSetModified, ...); these are the same operations on the
// gpui text field
TempStr FindEditTextTemp(MainWindow* win);
int FindEditTextLen(MainWindow* win);
void FindEditSetText(MainWindow* win, Str s);
bool FindEditIsModified(MainWindow* win);
void FindEditSetModified(MainWindow* win, bool modified);
bool IsFindEditFocused(MainWindow* win);
#if defined(SUMATRA_NG)
TempStr FindEditTestTemp(MainWindow* win, Str action, Str arg, int* exitCodeOut = nullptr);
#endif

#if OS_WIN
FindBarWnd* CreateFindBar(MainWindow* win);
void RecreateFindBar(MainWindow* win);
void FindBarUpdateDpi(MainWindow* win);
int FindBarFontHeight(MainWindow* win);
int FindBarWindowHeight(MainWindow* win);
int FindStatusDx(PlatformFont* font, int totalHits, bool capped);
void StartPickedFindTerm(MainWindow* win, Str term);
void FindBarSetMatchCaseChecked(MainWindow* win, bool checked);
void FindBarSetMatchWholeWordChecked(MainWindow* win, bool checked);
void FindBarSyncHistory(MainWindow* win);
TempStr FindUiStateResultTemp(Str action, int* exitCodeOut = nullptr);
#endif
