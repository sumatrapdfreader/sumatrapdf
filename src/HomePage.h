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

// HomePageViewMode setting ("thumbnails" or "list")
bool HomePageIsListView();
void SetHomePageListView(bool listView);

TempStr GetStaticLinkAtTemp(Vec<StaticLink*>& linkInfo, int x, int y, StaticLink** info);
TempStr LinkTooltipTemp(StaticLink* link);

constexpr const char* kLinkOpenFile = "<File,Open>";
constexpr const char* kLinkShowList = "<View,ShowList>";
constexpr const char* kLinkHideList = "<View,HideList>";
constexpr const char* kLinkNextTip = "<NextTip>";
constexpr const char* kLinkHomeListView = "<HomePage,ListView>";
constexpr const char* kLinkHomeThumbnailView = "<HomePage,ThumbnailView>";
constexpr const char* kLinkHomeRemoveFilePrefix = "<HomePage,RemoveFile>";
constexpr const char* kLinkHomePinFilePrefix = "<HomePage,PinFile>";

void SetPromoString(Str s);
void FreeHomePageTips();

void DrawHomePage(MainWindow* win, HDC hdc);
void PickAnotherRandomPromotion();
void HomePageOnVScroll(MainWindow* win, WPARAM wp);
void HomePageOnMouseWheel(MainWindow* win, int delta);
void HomePageFocusSearch(MainWindow* win);
void HomePageDestroySearch(MainWindow* win);

// keyboard navigation of the file list (issue #1136). dCol/dRow are in grid
// steps; in list view only dRow matters. Moving up past the first row puts
// focus in the search box
void HomePageMoveSelection(MainWindow* win, int dCol, int dRow);
// file of the keyboard-selected entry, empty if there's no selection
Str HomePageSelectedFilePathTemp(MainWindow* win);
// select the first entry, e.g. after the filter changed the list
void HomePageSelectFirst(MainWindow* win);
// hide keyboard-selection tip on deactivate; restore it when the frame is active
void HomePageOnWindowActivate(MainWindow* win, bool active);
// mouse over a file entry: update homePageSelIdx and show tip at that entry
// (not at the cursor). Returns true if (x,y) is over a file thumbnail/list row
bool HomePageOnHover(MainWindow* win, int x, int y);

// per-thumbnail ✕ close button (issue #283)
void HomePageUpdateCloseButton(MainWindow* win, int x, int y);
void HomePageHideCloseButton();
bool HomePageOnCloseButtonClick(MainWindow* win, int x, int y);
void HomePageOnCanvasMouseLeave();
