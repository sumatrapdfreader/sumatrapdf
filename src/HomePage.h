<<<<<<< HEAD
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* styling for About/Properties windows */

#include "utils/BaseUtil.h"

struct MainWindow;
struct StaticLink;

constexpr const char* kLeftTextFont = "Arial";
constexpr int kLeftTextFontSize = 14;
constexpr const char* kRightTextFont = "Arial Black";
constexpr int kRightTextFontSize = 14;

void ShowAboutWindow(MainWindow*);

void DrawAboutPage(MainWindow* win, HDC hdc);

constexpr const char* kLinkOpenFile = "<File,Open>";
constexpr const char* kLinkShowList = "<View,ShowList>";
constexpr const char* kLinkHideList = "<View,HideList>";
constexpr const char* kLinkNextTip = "<NextTip>";
constexpr const char* kLinkLauncherOpenPdf = "CmdOpenFile";
constexpr const char* kLinkLauncherReopenLast = "CmdReopenLastClosedFile";

void SetPromoString(const char*);

void DrawHomePage(MainWindow* win, HDC hdc);
void PickAnotherRandomPromotion();
void HomePageOnVScroll(MainWindow* win, WPARAM wp);

void HomePageOnMouseWheel(MainWindow* win, int delta);
void HomePageFocusSearch(MainWindow* win);
void HomePageDestroySearch(MainWindow* win);

// Nuevo: crear WebView para HomePage
void CreateHomePageWebView(MainWindow* win);
void HomePageHide(MainWindow* win);
void HomePageDestroy(MainWindow* win);
=======
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
>>>>>>> origin/master
