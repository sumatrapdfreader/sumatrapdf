/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* styling for About/Properties windows */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct Gfx;
enum class ScrollMsg;

constexpr const char* kLeftTextFont = "Arial";
constexpr int kLeftTextFontSize = 14;
constexpr const char* kRightTextFont = "Arial Black";
constexpr int kRightTextFontSize = 14;

// the About box (CmdHelpAbout); orig opens a window of its own
void ShowAboutWindow(MainWindow*);
void CloseAboutWindow();
bool IsAboutWindowVisible();
gpui::El* AboutDialogBuild(MainWindow* win, gpui::Ctx* cx);

// the Home tab's canvas: orig's start page, or orig's About page when the
// start page is switched off (ShowStartPage = false)
gpui::El* HomePageBuild(MainWindow* win, gpui::Ctx* cx);
void HomePageDestroyChrome(MainWindow* win);
void HomePageDelete(MainWindow* win);

bool HomePageIsListView();
void SetHomePageListView(bool listView);

void SetPromoString(Str s);
void FreeHomePageTips();
void HomePageInvalidateLayoutCache();
void PickAnotherRandomPromotion();

void HomePageFocusSearch(MainWindow* win);
// the start page's overlay scrollbar (`nTrackPos` for ScrollMsg::ThumbTrack)
void HomePageOnVScroll(MainWindow* win, ScrollMsg msg, int nTrackPos);
void HomePageMoveSelection(MainWindow* win, int dCol, int dRow);
Str HomePageSelectedFilePathTemp(MainWindow* win);
void HomePageSelectFirst(MainWindow* win);
bool HomePageOnKeyDown(MainWindow* win, int vk, bool ctrl);
// orig's WM_CONTEXTMENU from the keyboard: the selected entry's menu
void HomePageContextMenuFromKey(MainWindow* win, gpui::Ctx* cx);
void CopyAboutInfoToClipboard(MainWindow* win);

#if OS_WIN
void DrawAboutPage(MainWindow* win, Gfx* gfx);
void DrawHomePage(MainWindow* win, Gfx* gfx);
void HomePageCreate(MainWindow* win);
void HomePageRelayout(MainWindow* win);
void HomePageHideSearch(MainWindow* win);
void HomePageOnVScroll(MainWindow* win, WPARAM wp);
void HomePageOnMouseWheel(MainWindow* win, int delta);
void HomePageUpdateSearchColors(MainWindow* win);
void HomePageOnDpiChanged(MainWindow* win, int dpi);
void HomePageDestroySearch(MainWindow* win);
bool HomePageOnCanvasMessage(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp, LRESULT& res);
void HomePageOnWindowActivate(MainWindow* win, bool active);
bool HomePageOnHover(MainWindow* win, int x, int y);
Str HomePageFilePathAtTemp(MainWindow* win, int x, int y);
void HomePageClearActiveEntry(MainWindow* win);
TempStr HomeListRowsResultTemp(int* exitCodeOut);
TempStr HomeSelectionResultTemp(int* exitCodeOut);
#endif
