/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* styling for About/Properties windows */

struct MainWindow;
struct Gfx;

constexpr const char* kLeftTextFont = "Arial";
constexpr int kLeftTextFontSize = 14;
constexpr const char* kRightTextFont = "Arial Black";
constexpr int kRightTextFontSize = 14;

void ShowAboutWindow(MainWindow*);

void DrawAboutPage(MainWindow* win, Gfx* gfx);

bool HomePageIsListView();
void SetHomePageListView(bool listView);

void SetPromoString(Str s);
void FreeHomePageTips();
void HomePageInvalidateLayoutCache();

void DrawHomePage(MainWindow* win, Gfx* gfx);
void HomePageCreate(MainWindow* win);
void HomePageRelayout(MainWindow* win);
void HomePageHideSearch(MainWindow* win);
void PickAnotherRandomPromotion();
void HomePageOnVScroll(MainWindow* win, WPARAM wp);
void HomePageOnMouseWheel(MainWindow* win, int delta);
void HomePageFocusSearch(MainWindow* win);
void HomePageUpdateSearchColors(MainWindow* win);
void HomePageOnDpiChanged(MainWindow* win, int dpi);
void HomePageDestroySearch(MainWindow* win);
void HomePageDestroyChrome(MainWindow* win);
bool HomePageOnCanvasMessage(MainWindow* win, UINT msg, WPARAM wp, LPARAM lp, LRESULT& res);

void HomePageMoveSelection(MainWindow* win, int dCol, int dRow);
Str HomePageSelectedFilePathTemp(MainWindow* win);
void HomePageSelectFirst(MainWindow* win);
void HomePageOnWindowActivate(MainWindow* win, bool active);
bool HomePageOnHover(MainWindow* win, int x, int y);
Str HomePageFilePathAtTemp(MainWindow* win, int x, int y);

void HomePageClearActiveEntry(MainWindow* win);

TempStr HomeListRowsResultTemp(int* exitCodeOut);
TempStr HomeSelectionResultTemp(int* exitCodeOut);
