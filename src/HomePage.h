/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* styling for About/Properties windows */

struct MainWindow;

constexpr const char* kLeftTextFont = "Arial";
constexpr int kLeftTextFontSize = 14;
constexpr const char* kRightTextFont = "Arial Black";
constexpr int kRightTextFontSize = 14;

void ShowAboutWindow(MainWindow*);

void DrawAboutPage(MainWindow* win, HDC hdc);

TempStr GetStaticLinkAtTemp(Vec<StaticLink*>& linkInfo, int x, int y, StaticLink** info);

constexpr const char* kLinkOpenFile = "<File,Open>";
constexpr const char* kLinkShowList = "<View,ShowList>";
constexpr const char* kLinkHideList = "<View,HideList>";
constexpr const char* kLinkNextTip = "<NextTip>";

void SetPromoString(const char*);

void DrawHomePage(MainWindow* win, HDC hdc);
void PickAnotherRandomPromotion();
void HomePageOnVScroll(MainWindow* win, WPARAM wp);
void HomePageOnMouseWheel(MainWindow* win, int delta);
void HomePageFocusSearch(MainWindow* win);
void HomePageDestroySearch(MainWindow* win);

// per-thumbnail floating ✕ close button (issue #283)
void HomePageUpdateCloseButton(MainWindow* win, int x, int y);
void HomePageHideCloseButton();
void HomePageOnCanvasMouseLeave();
