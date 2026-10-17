/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's HomePage.cpp draws the start page and the About page with Gfx
// into the canvas HWND, over a tree of virtual controls. Here both are gpui
// element trees with orig's layout, order and colors: the
// [palette] SumatraPDF [?] header row, the open link / search box / view-mode
// row, the thumbnail grid (or the list view) of the file history, and the tip
// band at the bottom. The About box is a gpui Dialog with orig's rows.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Pixmap.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "FileHistory.h"
#include "FilterUtil.h"
#include "FilterHighlightDraw.h"
#include "FileThumbnails.h"
#include "Commands.h"
#include "ShortcutParse.h"
#include "TipMarkup.h"
#include "Accelerators.h"
#include "Translations.h"
#include "Theme.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "Version.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Menu.h"
#include "PagePosition.h"
#include "base/UITask.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "gui/PlatformFont.h"
#include "gui/DocCanvas.h"
#include "SvgIcons.h"
#include "gui/DialogWidgets.h"
#include "OverlayScrollbar.h"
#include "HomePage.h"
#include "HomePageCommon.h"

#include "SumatraLog.h"

// --- tips and promotions (orig's) -------------------------------------------

static Str sumatraPromos = StrL(R"promos(Try [Edna](https://edna.arslexis.io): a note taking web app for power users.
Try [MarkLexis](https://marklexis.arslexis.io): a bookmarking web application.
Try [Sumatra File Manager](https://sumatrafilemanager.com): file manager with super powers.
Try [Sumatra File Manager Online](https://online.sumatrafilemanager.com): file manager for S3, Dropbox and more.
)promos");

static Str promoFromServer;

static void EnsureTipsParsed() {
    if (gTipsParsed) {
        return;
    }
    CollectTipsFromString(sumatraTips, &gTipLines);
    CollectTipsFromString(sumatraPromos, &gPromoLines);
    if (len(promoFromServer) > 0) {
        CollectTipsFromString(promoFromServer, &gPromoLines);
    }
    gTipsParsed = true;
    PickRandomTipOrPromo();
}

void FreeHomePageTips() {
    if (gTipsParsed) {
        gTipLines.Reset();
        gPromoLines.Reset();
        gTipsParsed = false;
    }
    str::Free(promoFromServer);
    promoFromServer = {};
    HomePageInvalidateLayoutCache();
}

void PickAnotherRandomPromotion() {
    PickAnotherRandomTip();
}

void SetPromoString(Str s) {
    if (len(s) == 0) {
        return;
    }
    str::ReplaceWithCopy(&promoFromServer, s);
}

// --- the About rows (orig's) -------------------------------------------------

constexpr int kInnerPadding = 8;

static gp::El* HomeBrandEl(gp::Ctx* cx, Str name);

// one row of the About box
struct AboutRow {
    Str leftTxt;
    Str rightTxt;
    Str url;
};

static AboutRow gAboutRows[] = {
    {StrL("manual"), StrL("SumatraPDF manual"), StrL("https://www.sumatrapdfreader.org/docs/SumatraPDF-documentation")},
    {StrL("version history"), StrL("What's new"), StrL("https://www.sumatrapdfreader.org/docs/Version-history")},
    {StrL("website"), StrL("SumatraPDF website"), Str(kWebsiteURL)},
    {StrL("forums"), StrL("SumatraPDF forums"), StrL("https://github.com/sumatrapdfreader/sumatrapdf/discussions")},
    {StrL("licenses"), StrL("Various Open Source"),
     StrL("https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS")},
    {StrL("more software by me"), StrL("Sumatra File Manager"), StrL("https://sumatrafilemanager.com")},
    {{}, StrL("Sumatra File Manager Online"), StrL("https://online.sumatrafilemanager.com")},
    {{}, {}, {}}};

// the rows from kAboutSoftwareRow on are the other software, in the footer
constexpr int kAboutSoftwareEnd = dimofi(gAboutRows) - 1;
constexpr int kAboutSoftwareRow = kAboutSoftwareEnd - 2;

// the About box (orig's): the brand over the version, the link rows, a note,
// then a hairline over the copy link and the other software. Every row has a
// fixed height, so the window can be sized before it is made
constexpr int kAboutBoxDx = 380;
constexpr int kAboutPadTop = 28;
constexpr int kAboutPadSide = 32;
constexpr int kAboutPadBottom = 24;
constexpr int kAboutBrandDy = 20;
constexpr int kAboutVersionGap = 2;
constexpr int kAboutGroupGap = 22;
constexpr int kAboutRowDy = 18;
constexpr int kAboutMetaDy = 16;
constexpr int kAboutRowGap = 7;
constexpr int kAboutColGap = 20;
constexpr int kAboutFootGap = 18;
constexpr int kAboutFootRowGap = 10;
constexpr int kAboutSoftwareGap = 4;
constexpr int kAboutNoteMarkDx = 10;
constexpr int kAboutNoteMarkDy = 3;
constexpr int kAboutNoteGap = 8;
constexpr int kAboutTextFontPx = 13;
constexpr int kAboutMetaFontPx = 12;

// the line under the rows that says this isn't a release build
static Str AboutNoteText() {
#if defined(PRE_RELEASE_VER)
    return StrL("pre-release build");
#elif IS_DEBUG
    return StrL("debug build, for testing only");
#else
    return {};
#endif
}

static TempStr GetAppVersionTemp() {
    TempStr s = str::JoinTemp(StrL("v"), currentVersion);
    bool is64 = sizeof(void*) == 8;
    s = str::JoinTemp(s, is64 ? StrL(" 64-bit") : StrL(" 32-bit"));
    if (gIsDebugBuild) {
        s = str::JoinTemp(s, StrL(" (dbg)"));
    }
    return s;
}

// "26.10.14 64-bit, built Oct 14 2026 01:46"
static TempStr AboutVersionLineTemp() {
    bool is64 = sizeof(void*) == 8;
    Str bits = is64 ? StrL("64-bit") : StrL("32-bit");
    Str built = StrL(__DATE__ " " __TIME__);
    // without the seconds
    built.len = std::max(0, len(built) - 3);
    return fmt("%s %s, built %s", currentVersion, bits, built);
}

constexpr Color kCol1 = MkRgb(196, 64, 50);
constexpr Color kCol2 = MkRgb(227, 107, 35);
constexpr Color kCol3 = MkRgb(93, 160, 40);
constexpr Color kCol4 = MkRgb(69, 132, 190);
constexpr Color kCol5 = MkRgb(112, 115, 207);

// Version, OS, memory and similar facts for a bug report.
static void AppendBugReportInfo(str::Builder& s) {
    s.Append(fmt("SumatraPDF %s\n", GetAppVersionTemp()));
    s.Append(fmt("Built on: %s %s\n", StrL(__DATE__), StrL(__TIME__)));
    if (gitCommidId) {
        s.Append(fmt("Git: %s\n", gitCommidId));
    }
    Str exeType = IsDllBuild() ? StrL("dll") : StrL("static");
    Str instType = IsRunningInPortableMode() ? StrL("portable") : StrL("installed");
    s.Append(fmt("Type: %s, %s\n", exeType, instType));
    if (gIsPreReleaseBuild) {
        s.Append(StrL("Pre-release: yes\n"));
    }
    if (gIsAsanBuild) {
        s.Append(StrL("ASan: yes\n"));
    }

#if OS_WIN
    OSVERSIONINFOEX ver{};
    if (GetOsVersion(ver)) {
        TempStr os = OsNameFromVerTemp(ver);
        int buildNumber = (int)ver.dwBuildNumber & 0xFFFF;
        Str arch = StrL("64-bit");
        if (IsProcess32()) {
            arch = IsRunningInWow64() ? StrL("32-bit (Wow64)") : StrL("32-bit");
        }
        s.Append(fmt("OS: Windows %s, build %d, %s\n", os, buildNumber, arch));
    }
    if (IsOs64()) {
        s.Append(StrL("OS architecture: 64-bit\n"));
    } else {
        s.Append(StrL("OS architecture: 32-bit\n"));
    }

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        float physMemGB = (float)ms.ullTotalPhys / (float)(1024 * 1024 * 1024);
        s.Append(fmt("Physical memory: %.2f GB (%d%% in use)\n", physMemGB, (int)ms.dwMemoryLoad));
    }
#endif
    s.Append(fmt("Processors: %d\n", CpuCoreCount()));

    Str theme = ThemeGetNameAt(ThemeGetCurrentIndex());
    if (theme) {
        s.Append(fmt("Theme: %s\n", theme));
    }
}

// --- the tip markup ----------------------------------------------------------

// the parser moved to src/TipMarkup.cpp in step 14a: the notifications parse
// the same markup

// --- the layout (orig's LayoutHomePage) --------------------------------------

/* alternate static page to display when no document is loaded */

constexpr int kThumbsBorderDx = 1;
constexpr int kThumbsMarginLeft = 40;
constexpr int kThumbsMarginRight = 40;
constexpr int kThumbsSpaceBetweenX = 24;
constexpr int kThumbsSpaceBetweenY = 60;
// caption below each thumbnail: the file name over the page count
constexpr int kThumbsCaptionGapY = 10;
constexpr int kThumbsCaptionLineDy = 18;
constexpr int kThumbsCaptionMetaDy = 16;
constexpr int kThumbsCaptionDy = kThumbsCaptionLineDy + kThumbsCaptionMetaDy;
// the reading-progress line along the bottom edge of a thumbnail
constexpr int kThumbsProgressDy = 3;
// the selection ring sits this far outside the page
constexpr int kThumbsSelectionGap = 4;
constexpr int kThumbsSelectionDx = 2;
constexpr int kHomeListThumbDx = 30;
constexpr int kHomeListThumbDy = 40;
constexpr int kHomeListRowDy = 46;
constexpr int kHomeListRowGapDx = 8;
constexpr int kSearchEditDy = 28;
// gpui's Small input font. The DPI probe reports this height.
constexpr int kHomeSearchFontPx = 14;
constexpr int kSearchThumbnailsGapY = 28;
constexpr int kHomeHeaderTopY = 24;
constexpr int kHomeSearchMaxDx = 340;
constexpr int kHomeTextFontPx = 13;
constexpr int kHomeMetaFontPx = 12;
// the home page brand: five stripes in the logo colors and the name
constexpr int kHomeBrandFontPx = 15;
constexpr int kHomeBrandStripeDx = 3;
constexpr int kHomeBrandStripeDy = 16;
constexpr int kHomeBrandStripeGap = 2;
constexpr int kHomeBrandGap = 10;
constexpr int kThumbsMiddleMargin = 32;
// orig's HomePageIconSize: the icons follow ToolbarSize (dips here)
static int HomePageIconSize() {
    int sz = gSettings->toolbarSize;
    if (sz < 1) {
        sz = 16;
    }
    return RoundUp(sz, 4);
}

// light blue outline marking the keyboard-selected list row (issue #1136).
// A fixed color: it has to read as "selected" against both the light and the
// dark page background. A thumbnail gets a ring in the text color instead
constexpr Color kHomeSelectionColor = MkRgb(0x4c, 0xa6, 0xff);

// HomePageViewMode setting ("thumbnails" or "list")
bool HomePageIsListView() {
    return gSettings && str::EqI(gSettings->homePageViewMode, StrL("list"));
}

void SetHomePageListView(bool listView) {
    Str mode = listView ? StrL("list") : StrL("thumbnails");
    str::ReplaceWithCopy(&gSettings->homePageViewMode, mode);
}

// the decoded page thumbnails. A gpui RenderImage belongs to the App's one
// PaintApp, so the cache is shared by every window; it is keyed by FileState.
struct HomeThumb {
    FileState* fs = nullptr;
    gpui::RenderImage* img = nullptr;
    Str png;   // the cache file's bytes, decoded lazily on the paint thread
    Size size; // from the png header
    bool tried = false;
};

static Vec<HomeThumb*> gHomeThumbs;

struct HomeView;

// ng: orig keeps the start page's state on MainWindow; so does this, one
// HomePageUI per window (step 10b)
struct HomePageUI {
    gp::Entity<HomeView> view;
    gpui::InputState* search = nullptr;
    Str searchQuery;
    float scrollY = 0;
    // scrollY the last laid-out frame used, so a bottom scroll can measure
    // the caption against the bounds that scroll produced
    float scrollYLaidOut = 0;
    bool scrollToEnd = false;
    int selIdx = -1;
    int searchReturnCol = 0;
    int gridCols = 1;
    int entryCount = 0;
    gpui::Bounds entriesView{};
    // the entries' height is only known once they were painted: the first
    // frame asks for a second one so the scrollbar can be there
    bool askedForMeasure = false;
    // false from invalidate until the next HomePageBuild refills files
    bool filesLaidOut = false;
    Vec<FileState*> files;
    Vec<TipSpan> tipSpans;
    // orig's OnAboutContextMenu: the file the menu is for and the menu built
    // for it
    Str menuPath;
    MenuModel* menu = nullptr;
    gp::Entity<gp::PopupMenuState> menuPopup;
    // the entry the right button went down on (-1: none) and where, in the
    // window; orig's dragStart
    int rdownIdx = -1;
    bool rdownOnEntry = false;
    Point rdown;
    gpui::Bounds pageBounds{};
    gpui::Bounds searchBounds{};
    // the "Open..." link and the brand as laid out last frame: orig's
    // LayoutHomePage needs their widths, and gpui measures text only while
    // painting
    gpui::Bounds openGroupBounds{};
    gpui::Bounds brandBounds{};
    // the entries as laid out last frame, for the hover tooltip
    Vec<gpui::Bounds> entryBounds;
    // each thumbnail's selection ring, drawn for the selected one
    Vec<gpui::Bounds> ringBounds;
    // list-view size column, measured while painting (issue #5870)
    Vec<gpui::Bounds> listSizeBounds;
    // list-view directory column, same paint (issue #5870)
    Vec<gpui::Bounds> listPathBounds;
    // the "show as list" button, for the click that switches the home view
    gpui::Bounds listIconBounds{};
    // the tip band along the bottom
    gpui::Bounds tipBounds{};
};

static HomePageUI* Ui(MainWindow* win) {
    if (!win->homePage) {
        win->homePage = new HomePageUI();
    }
    return win->homePage;
}

int HomeSearchFontPx(MainWindow* win) {
    HomePageUI* h = win ? win->homePage : nullptr;
    if (!h || !h->search) {
        return 0;
    }
    return AppShellDpiPx(win, kHomeSearchFontPx);
}

void HomePageDelete(MainWindow* win) {
    HomePageUI* h = win->homePage;
    if (!h) {
        return;
    }
    delete h->search;
    str::Free(h->searchQuery);
    str::Free(h->menuPath);
    DeleteMenuModel(h->menu);
    VecReset(h->files);
    TipSpansFree(h->tipSpans);
    delete h;
    win->homePage = nullptr;
}

static HomeThumb* HomeThumbFor(FileState* fs) {
    for (HomeThumb* t : gHomeThumbs) {
        if (t->fs == fs) {
            return t;
        }
    }
    auto* t = new HomeThumb();
    t->fs = fs;
    VecAppend(gHomeThumbs, t);
    return t;
}

static void FreeHomeThumb(HomeThumb* t) {
    if (t->img) {
        gp::RenderImageRelease(t->img);
    }
    str::Free(t->png);
    delete t;
}

void HomePageThumbnailChanged(FileState* fs) {
    for (int i = 0; i < len(gHomeThumbs); i++) {
        if (gHomeThumbs[i]->fs != fs) {
            continue;
        }
        FreeHomeThumb(gHomeThumbs[i]);
        VecRemoveAt(gHomeThumbs, i);
        return;
    }
}

// The cache holds raw FileState*, owned by gSettings. Reloading settings frees
// and rebuilds those, so the cache has to be dropped first (crash 8c34d7eda).
void HomePageInvalidateLayoutCache() {
    for (HomeThumb* t : gHomeThumbs) {
        FreeHomeThumb(t);
    }
    VecReset(gHomeThumbs);
    for (MainWindow* win : gWindows) {
        HomePageUI* h = win->homePage;
        if (!h) {
            continue;
        }
        VecReset(h->files);
        h->entryCount = 0;
        h->filesLaidOut = false;
    }
}

void HomePageDestroyChrome(MainWindow* win) {
    HomePageInvalidateLayoutCache();
    HomePageUI* h = Ui(win);
    delete h->search;
    h->search = nullptr;
    str::Free(h->searchQuery);
    h->searchQuery = {};
}

// reads the cache file once, for its size at layout and its pixels at paint.
// One made for the other ThumbnailsType is as good as missing, like
// LoadThumbnail() treats it
static void HomeThumbReadPng(HomeThumb* t) {
    if (t->tried) {
        return;
    }
    t->tried = true;
    TempStr path = GetThumbnailPathTemp(t->fs->filePath);
    if (len(path) == 0) {
        return;
    }
    t->png = file::ReadFile(path);
    t->size = PngSizeFromData(t->png);
    if (len(t->png) > 0 && !ThumbnailSizeMatchesType(t->size)) {
        str::Free(t->png);
        t->png = {};
        t->size = {};
        file::Delete(path);
    }
}

// the decoded thumbnail, or null while there is none. Runs on the paint
// thread, which is where RenderImageDecode has to happen.
static gp::ImageLoadState HomeThumbLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imgOut) {
    auto* t = (HomeThumb*)user;
    HomeThumbReadPng(t);
    if (!t->img && len(t->png) > 0) {
        t->img = gp::RenderImageDecode(pa, (const u8*)t->png.s, t->png.len);
    }
    *imgOut = t->img;
    return t->img ? gp::ImageLoadState::Ready : gp::ImageLoadState::Failed;
}

static TempStr HomeSearchQueryTemp(MainWindow* win) {
    HomePageUI* h = Ui(win);
    if (h->search) {
        return str::DupTemp(FromGpui(gp::InputValue(h->search)));
    }
    return str::DupTemp(h->searchQuery);
}

// orig's LayoutHomePage's file collection
static void CollectHomePageFiles(MainWindow* win, Vec<FileState*>& fileStates, StrVec& filterWords) {
    Vec<FileState*> allFileStates;
    if (gSettings->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(allFileStates);
    } else {
        FileHistoryGetRecentlyOpenedOrder(allFileStates);
    }

    TempStr searchQuery = HomeSearchQueryTemp(win);
    bool hasFilter = len(searchQuery) > 0;
    if (hasFilter) {
        SplitFilterToWords(searchQuery, filterWords);
    }
    for (int i = 0; i < len(allFileStates); i++) {
        FileState* fs = allFileStates[i];
        // a state without a path can't be opened or thumbnailed - don't show it
        if (len(fs->filePath) == 0) {
            continue;
        }
        if (hasFilter) {
            TempStr baseName = path::GetBaseNameTemp(fs->filePath);
            if (!FilterMatches(baseName, filterWords)) {
                continue;
            }
        }
        VecAppend(fileStates, fs);
    }
}

// --- the view ----------------------------------------------------------------

struct HomeView {
    MainWindow* win = nullptr;

    static void OnEntryClick(HomeView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnEntryDown(HomeView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t idx);
    static void OnEntryHover(HomeView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx);
    static void OnPageDown(HomeView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnPageUp(HomeView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev);
    static void OnMenuAction(HomeView* self, gp::Ctx* cx, const gp::ActionEvent* ev);
    static void OnEntryForget(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnEntryPin(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnViewMode(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t listView);
    static void OnOpenDoc(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnSearch(HomeView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnSearchKey(HomeView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnScroll(HomeView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnWheel(HomeView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev);
    static void OnTipBand(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnTipLink(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t spanIdx);
    static void OnShowFreqRead(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnAboutLink(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t rowIdx);
    static void OnCopyInfo(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnAboutClose(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnAboutHover(HomeView* self, gp::Ctx* cx, const gp::HoverCardOpenChangeEvent* ev);
};

// ng: gp::Notify() wakes the entity whose listener ran, which is this view and
// not the shell that renders it, so the frame also has to be invalidated
static void HomeNotify(MainWindow* win, gp::Ctx* cx) {
    gp::Notify(cx);
    AppShellInvalidate(win);
}

static void OpenHomeEntry(MainWindow* win, int idx, bool ctrl) {
    Vec<FileState*>& files = Ui(win)->files;
    if (idx < 0 || idx >= len(files)) {
        return;
    }
    TempStr path = str::DupTemp(files[idx]->filePath);
    if (len(path) == 0) {
        return;
    }
    logf("HomePage: opening '%s'%s\n", path, Str(ctrl ? " (ctrl)" : ""));
    // orig: args.activateExisting = !isCtrl, activateExistingInWindow = true -
    // a file this window already shows is selected; ctrl forces always opening
    if (!ctrl && FindMainWindowByFile(path, true, win)) {
        AppShellInvalidate(win);
        return;
    }
    LoadDocument(win, path);
}

void HomeView::OnEntryClick(HomeView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    Ui(self->win)->selIdx = (int)idx;
    OpenHomeEntry(self->win, (int)idx, ev->modifiers.control);
    HomeNotify(self->win, cx);
}

static uint32_t ActHomeMenu() {
    static uint32_t act = gp::ActionOf(GStrL("sumatra::HomeMenu"));
    return act;
}

// orig's HomeEntriesCtrl::SetActiveEntry: the entry under the mouse becomes
// the selected one, with an infotip of its path and size
void HomeView::OnEntryHover(HomeView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx) {
    MainWindow* win = self->win;
    HomePageUI* h = Ui(win);
    int i = (int)idx;
    if (!ev->hovered || i < 0 || i >= len(h->files)) {
        HoverTooltipHide(cx);
        return;
    }
    if (h->selIdx != i) {
        h->selIdx = i;
        HomeNotify(win, cx);
    }
    FileState* fs = h->files[i];
    if (!fs || len(fs->filePath) == 0 || i >= len(h->entryBounds)) {
        return;
    }
    // Same text as the keyboard selection's: path + size (size looked up only
    // when shown)
    TempStr tip = str::DupTemp(fs->filePath);
    i64 size = file::GetSize(fs->filePath);
    if (size >= 0) {
        tip = fmt("%s  %s", tip, str::FormatSizeShortTemp(size, nullptr));
    }
    HoverTooltipShow(cx, tip, h->entryBounds[i]);
}

// the entry the right button went down on; OnPageDown, which runs after it for
// the same press, takes it from here
void HomeView::OnEntryDown(HomeView* self, gp::Ctx*, const gp::MouseDownEvent* ev, int64_t idx) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    HomePageUI* h = Ui(self->win);
    h->rdownIdx = (int)idx;
    h->rdownOnEntry = true;
}

// orig's OnMouseRightButtonDownAbout
void HomeView::OnPageDown(HomeView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    // ng: gpui's ContextMenu would open from this press; orig's opens when
    // the button comes up
    gp::WindowStopPropagation(cx);
    MainWindow* win = self->win;
    HomePageUI* h = Ui(win);
    if (!h->rdownOnEntry) {
        h->rdownIdx = -1;
    }
    h->rdownOnEntry = false;
    AppShellFocusFrame(win);
    h->rdown = Point{(int)ev->x, (int)ev->y};
}

// orig's OnAboutContextMenu: the menu of the file under the click; the
// keyboard / context-menu key falls back to the keyboard-selected home entry
static void OnAboutContextMenu(MainWindow* win, gp::Ctx* cx, int idx, float x, float y) {
    HomePageUI* h = Ui(win);
    Vec<FileState*>& files = h->files;
    bool fromClick = idx >= 0 && idx < len(files);
    if (!fromClick) {
        idx = h->selIdx;
    }
    if (idx < 0 || idx >= len(files)) {
        return;
    }
    // Keep keyboard selection in sync with the right-clicked thumbnail
    h->selIdx = idx;
    str::ReplaceWithCopy(&h->menuPath, files[idx]->filePath);
    DeleteMenuModel(h->menu);
    h->menu = BuildHomeContextMenu(win, h->menuPath);
    HomeNotify(win, cx);
    if (!h->menu) {
        return;
    }
    OpenPopupMenuAt(cx, h->menuPopup, x - h->pageBounds.x, y - h->pageBounds.y);
}

// orig's OnMouseRightButtonUpAbout
void HomeView::OnPageUp(HomeView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    MainWindow* win = self->win;
    HomePageUI* h = Ui(win);
    if (IsDragDistance((int)ev->x, h->rdown.x, (int)ev->y, h->rdown.y)) {
        return;
    }
    OnAboutContextMenu(win, cx, h->rdownIdx, ev->x, ev->y);
}

void HomePageContextMenuFromKey(MainWindow* win, gp::Ctx* cx) {
    if (!win || !win->homePage || !cx->win) {
        return;
    }
    // keyboard menu (no hit under the cursor): place at cursor
    OnAboutContextMenu(win, cx, -1, cx->win->mouseX, cx->win->mouseY);
}

// a command from a gpui popup runs while the popup still owns the mouse and
// the keyboard, so it goes through the ui task queue
struct HomeMenuCmd {
    MainWindow* win;
    Str path; // owned
    int cmdId;
};

static void RunHomeMenuCmd(HomeMenuCmd* c) {
    if (IsMainWindowValidAndNotClosing(c->win)) {
        HomeContextMenuCommand(c->win, c->path, c->cmdId);
    }
    str::Free(c->path);
    delete c;
}

void HomeView::OnMenuAction(HomeView* self, gp::Ctx* cx, const gp::ActionEvent* ev) {
    MainWindow* win = self->win;
    auto* c = new HomeMenuCmd{win, str::Dup(Ui(win)->menuPath), (int)ev->arg};
    uitask::Post(MkFunc0<HomeMenuCmd>(RunHomeMenuCmd, c), "HomeMenuCmd");
    HomeNotify(win, cx);
}

void HomeView::OnEntryForget(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idxIn) {
    int idx = (int)idxIn;
    Vec<FileState*>& files = Ui(self->win)->files;
    if (idx < 0 || idx >= len(files)) {
        return;
    }
    TempStr path = str::DupTemp(files[idx]->filePath);
    ForgetFileFromFrequentlyRead(self->win, path);
    HomeNotify(self->win, cx);
}

void HomeView::OnEntryPin(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idxIn) {
    int idx = (int)idxIn;
    Vec<FileState*>& files = Ui(self->win)->files;
    if (idx < 0 || idx >= len(files)) {
        return;
    }
    FileState* fs = files[idx];
    fs->isPinned = !fs->isPinned;
    ScheduleSaveSettings();
    HomeNotify(self->win, cx);
}

void HomeView::OnViewMode(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t listView) {
    bool wantList = listView != 0;
    if (wantList == HomePageIsListView()) {
        return;
    }
    SetHomePageListView(wantList);
    Ui(self->win)->scrollY = 0;
    ScheduleSaveSettings();
    HomeNotify(self->win, cx);
}

void HomeView::OnOpenDoc(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    ExecuteCmd(self->win, CmdOpenFile);
    HomeNotify(self->win, cx);
}

void HomeView::OnSearch(HomeView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    HomePageUI* h = Ui(self->win);
    h->scrollY = 0;
    // the filter changed the list, so select its first entry (#1136)
    HomePageSelectFirst(self->win);
    HomeNotify(self->win, cx);
}

// orig's HomeSearchEdit::WndProc: down from the search box moves into the
// file list (issue #1136), restoring the column we left from when going up;
// Esc clears the text and gives the canvas the focus. ng: a capture listener,
// so the field does not take the key first
void HomeView::OnSearchKey(HomeView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    MainWindow* win = self->win;
    HomePageUI* h = Ui(win);
    if (!h->search || !gp::FocusHandleIsFocused(cx->win, h->search->focus)) {
        return;
    }
    if (ev->vk != VK_DOWN && ev->vk != VK_ESCAPE) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    if (ev->vk == VK_DOWN) {
        int n = h->entryCount;
        h->selIdx = n > 0 ? limitValue(h->searchReturnCol, 0, n - 1) : -1;
    } else {
        gp::InputSetValue(h->search, GStrL(""));
        h->scrollY = 0;
        HomePageSelectFirst(win);
    }
    AppShellFocusFrame(win);
    HomeNotify(win, cx);
}

// orig's totalContentDy: the height of all the rows
static int HomeTotalContentDy(HomePageUI* h) {
    int thumbsRowDy = HomePageIsListView() ? kHomeListRowDy : ThumbnailFrameSize().dy + kThumbsSpaceBetweenY;
    int cols = HomePageIsListView() ? 1 : std::max(1, h->gridCols);
    int nRows = (h->entryCount + cols - 1) / cols;
    return nRows * thumbsRowDy;
}

// ng: orig's relayout clamps the offset; the rows' height is known here
static float HomeClampScrollY(HomePageUI* h, float y) {
    float maxY = std::max(0.f, (float)HomeTotalContentDy(h) - h->entriesView.h);
    return std::max(0.f, std::min(y, maxY));
}

// orig's HomePageOnMouseWheel: a third of a row per notch
void HomeView::OnWheel(HomeView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev) {
    if (ev->deltaY == 0) {
        return;
    }
    HomePageUI* h = Ui(self->win);
    int thumbsRowDy = HomePageIsListView() ? kHomeListRowDy : ThumbnailFrameSize().dy + kThumbsSpaceBetweenY;
    int scrollBy = thumbsRowDy / 3;
    if (ev->deltaY > 0) {
        scrollBy = -scrollBy;
    }
    float newScrollY = HomeClampScrollY(h, h->scrollY + (float)scrollBy);
    const_cast<gp::ScrollWheelEvent*>(ev)->propagate = false;
    if (newScrollY != h->scrollY) {
        h->scrollToEnd = false;
        h->scrollY = newScrollY;
        // orig's OverlayScrollbarSetInfo re-reveals the bar on a scroll
        OverlayScrollbarsNotifyScroll(self->win);
        HomeNotify(self->win, cx);
    }
}

void HomePageOnVScroll(MainWindow* win, ScrollMsg msg, int nTrackPos) {
    HomePageUI* h = Ui(win);
    int lineDy = HomePageIsListView() ? kHomeListRowDy : ThumbnailFrameSize().dy + kThumbsSpaceBetweenY;
    int pageDy = lineDy * 3;
    bool toEnd = msg == ScrollMsg::Bottom;
    h->scrollToEnd = toEnd;

    int newScrollY = (int)h->scrollY;
    switch (msg) {
        case ScrollMsg::LineUp:
            newScrollY -= lineDy;
            break;
        case ScrollMsg::LineDown:
            newScrollY += lineDy;
            break;
        case ScrollMsg::PageUp:
            newScrollY -= pageDy;
            break;
        case ScrollMsg::PageDown:
            newScrollY += pageDy;
            break;
        case ScrollMsg::ThumbTrack:
            newScrollY = nTrackPos;
            break;
        case ScrollMsg::Top:
            newScrollY = 0;
            break;
        case ScrollMsg::Bottom:
            newScrollY = INT_MAX / 2; // will be clamped
            break;
        default:
            break;
    }
    float y = HomeClampScrollY(h, (float)newScrollY);
    // a previous End may already sit past the row-pitch estimate
    if (toEnd && h->scrollY > y) {
        y = h->scrollY;
    }
    if (toEnd || y != h->scrollY) {
        h->scrollY = y;
        AppShellInvalidate(win);
    }
}

void HomeView::OnScroll(HomeView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    HomePageUI* h = Ui(self->win);
    h->scrollY = ev->offsetY;
    h->scrollYLaidOut = ev->offsetY;
    HomeNotify(self->win, cx);
}

void HomeView::OnTipBand(HomeView* self, gp::Ctx* cx, const gp::ClickEvent* ev) {
    if (ev->clickCount < 2) {
        return;
    }
    PickAnotherRandomPromotion();
    HomeNotify(self->win, cx);
}

void HomeView::OnTipLink(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t spanIdxIn) {
    int spanIdx = (int)spanIdxIn;
    Vec<TipSpan>& spans = Ui(self->win)->tipSpans;
    if (spanIdx < 0 || spanIdx >= len(spans)) {
        return;
    }
    Str target = spans[spanIdx].target;
    if (str::StartsWith(target, StrL("http"))) {
        SumatraLaunchBrowser(target);
        HomeNotify(self->win, cx);
        return;
    }
    int cmdId = GetCommandIdByName(target);
    if (cmdId > 0) {
        ExecuteCmd(self->win, cmdId);
    }
    HomeNotify(self->win, cx);
}

void HomeView::OnShowFreqRead(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    gSettings->showStartPage = true;
    ScheduleSaveSettings();
    logf("HomePage: ShowStartPage = true\n");
    HomeNotify(self->win, cx);
}

void HomeView::OnAboutLink(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t rowIdx) {
    Str url = gAboutRows[(int)rowIdx].url;
    if (len(url) > 0) {
        SumatraLaunchBrowser(url);
    }
    HomeNotify(self->win, cx);
}

void CopyAboutInfoToClipboard(MainWindow* win) {
    str::Builder info;
    info.Reserve(1024);
    AppendBugReportInfo(info);
    CopyTextToClipboard(win, ToStr(info));
}

void HomeView::OnCopyInfo(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    str::Builder info;
    info.Reserve(1024);
    AppendBugReportInfo(info);
    gp::ClipboardSetText(cx->win, ToGpui(ToStr(info)));
    info.Reset();
    HomeNotify(self->win, cx);
}

// --- shared pieces -----------------------------------------------------------

// a link in the text color with a muted underline; plain text when the url
// can't be opened (no disk access)
static gp::El* AboutLinkEl(MainWindow* win, gp::Ctx* cx, Str text, Str url, Str id, int row) {
    Color colText = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    bool isLink = CanAccessDisk() && len(url) > 0;
    gp::El* el = gp::TextEl(cx->a, GpuiDup(cx->a, text))
                     ->Font((float)kAboutTextFontPx)
                     ->Fg(ToGpui(colText))
                     ->H((float)kAboutRowDy)
                     ->Shrink0();
    if (isLink) {
        el->BorderB(1, ToGpui(colMuted))
            ->Cursor(gp::CursorKind::Pointer)
            ->Tip(GpuiDup(cx->a, url))
            ->PathClick(GpuiDup(cx->a, id))
            ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnAboutLink, row));
    }
    return el;
}

// the About box, the same in the About window, the home page's dropdown and
// the About page
static gp::El* AboutBoxEl(MainWindow* win, gp::Ctx* cx, bool withBorder) {
    Color colText = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    Color colEdge = ThemeEdgeColor();
    gp::El* box = gp::Div(cx->a)
                      ->FlexCol()
                      ->W((float)kAboutBoxDx)
                      ->Shrink0()
                      ->Bg(ToGpui(ThemeMainWindowBackgroundColor()))
                      ->PadT((float)kAboutPadTop)
                      ->PadX((float)kAboutPadSide)
                      ->PadB((float)kAboutPadBottom)
                      ->Gap((float)kAboutGroupGap);
    if (withBorder) {
        box->Border(1, ToGpui(colEdge));
    }

    gp::El* head = gp::Div(cx->a)->FlexCol()->Gap((float)kAboutVersionGap);
    head->Child(
        gp::Div(cx->a)->FlexRow()->ItemsCenter()->H((float)kAboutBrandDy)->Child(HomeBrandEl(cx, StrL(kAppName))));
    head->Child(gp::TextEl(cx->a, GpuiDup(cx->a, AboutVersionLineTemp()))
                    ->Font((float)kAboutMetaFontPx)
                    ->Fg(ToGpui(colMuted))
                    ->H((float)kAboutMetaDy));
    box->Child(head);

    // the rows: muted labels, right-aligned, then the links
    gp::El* labels = gp::Div(cx->a)->FlexCol()->ItemsEnd()->Gap((float)kAboutRowGap)->Shrink0();
    gp::El* values = gp::Div(cx->a)->FlexCol()->ItemsStart()->Gap((float)kAboutRowGap)->Flex1()->MinW(0);
    for (int i = 0; i < kAboutSoftwareRow; i++) {
        AboutRow* el = &gAboutRows[i];
        labels->Child(gp::TextEl(cx->a, GpuiDup(cx->a, el->leftTxt))
                          ->Font((float)kAboutTextFontPx)
                          ->Fg(ToGpui(colMuted))
                          ->H((float)kAboutRowDy));
        values->Child(AboutLinkEl(win, cx, TrimGitTemp(el->rightTxt), el->url, fmt("about-row-%d", i), i));
    }
    box->Child(gp::Div(cx->a)->FlexRow()->Gap((float)kAboutColGap)->Child(labels)->Child(values));

    Str noteTxt = AboutNoteText();
    if (len(noteTxt) > 0) {
        box->Child(
            gp::Div(cx->a)
                ->FlexRow()
                ->ItemsCenter()
                ->JustifyCenter()
                ->Gap((float)kAboutNoteGap)
                ->H((float)kAboutMetaDy)
                ->Child(gp::Div(cx->a)
                            ->W((float)kAboutNoteMarkDx)
                            ->H((float)kAboutNoteMarkDy)
                            ->Bg(ToGpui(ThemeAccentColor())))
                ->Child(
                    gp::TextEl(cx->a, GpuiDup(cx->a, noteTxt))->Font((float)kAboutMetaFontPx)->Fg(ToGpui(colMuted))));
    }

    // the footer under the hairline: the copy link, then the other software
    gp::El* foot = gp::Div(cx->a)
                       ->FlexCol()
                       ->ItemsCenter()
                       ->W(gp::kFill)
                       ->BorderT(1, ToGpui(colEdge))
                       ->PadT((float)kAboutFootGap)
                       ->Gap((float)kAboutFootRowGap);
    foot->Child(gp::TextEl(cx->a, ToGpui(Tr("Copy program and machine info")))
                    ->Font((float)kAboutTextFontPx)
                    ->Fg(ToGpui(colText))
                    ->H((float)kAboutRowDy)
                    ->BorderB(1, ToGpui(colMuted))
                    ->Cursor(gp::CursorKind::Pointer)
                    ->PathClick(GStrL("about-copy-info"))
                    ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnCopyInfo)));
    gp::El* software = gp::Div(cx->a)->FlexCol()->ItemsCenter()->Gap((float)kAboutSoftwareGap);
    software->Child(gp::TextEl(cx->a, GpuiDup(cx->a, gAboutRows[kAboutSoftwareRow].leftTxt))
                        ->Font((float)kAboutMetaFontPx)
                        ->Fg(ToGpui(colMuted))
                        ->H((float)kAboutMetaDy));
    for (int i = kAboutSoftwareRow; i < kAboutSoftwareEnd; i++) {
        AboutRow* el = &gAboutRows[i];
        software->Child(AboutLinkEl(win, cx, el->rightTxt, el->url, fmt("about-software-%d", i), i));
    }
    foot->Child(software);
    box->Child(foot);
    return box;
}

// the box's height in dips, from the fixed row heights
static int AboutBoxDy() {
    int n = kAboutSoftwareRow;
    int dy = kAboutPadTop + kAboutBrandDy + kAboutVersionGap + kAboutMetaDy;
    dy += kAboutGroupGap + (n * kAboutRowDy) + ((n - 1) * kAboutRowGap);
    if (len(AboutNoteText()) > 0) {
        dy += kAboutGroupGap + kAboutMetaDy;
    }
    int nSoftware = kAboutSoftwareEnd - kAboutSoftwareRow;
    dy += kAboutGroupGap + 1 + kAboutFootGap + kAboutRowDy + kAboutFootRowGap + kAboutMetaDy +
          (nSoftware * (kAboutSoftwareGap + kAboutRowDy)) + kAboutPadBottom;
    return dy;
}

// orig waits TooltipInitialDelayMs(), the system tooltip delay, before showing
// the About dropdown and kAboutHoverHideDelayMs before hiding it
constexpr int kAboutHoverShowDelayMs = 500;
constexpr int kAboutHoverHideDelayMs = 200;

// --- the About box (orig's ShowAboutWindow) ----------------------------------

static bool gAboutVisible = false;
static MainWindow* gAboutWin = nullptr;
// orig's SUMATRA_PDF_ABOUT window, where the platform can have one
// (gui/ToolWindow.h); null: a dialog in the frame
static ToolWindow* gAboutTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsAboutWindowVisible() {
    return gAboutVisible && !gAboutTw;
}

void CloseAboutWindow() {
    if (!gAboutVisible) {
        return;
    }
    gAboutVisible = false;
    if (gAboutTw) {
        ToolWindowClose(gAboutTw);
        gAboutTw = nullptr;
    }
    AppShellInvalidate(gAboutWin);
}

static void AboutOpenToolWindow(MainWindow* win);

void ShowAboutWindow(MainWindow* win) {
    if (gAboutVisible && gAboutTw) {
        // orig: SetActiveWindow() on the one that is open
        ToolWindowActivate(gAboutTw);
        return;
    }
    gAboutWin = win;
    gAboutVisible = true;
    AboutOpenToolWindow(win);
    AppShellInvalidate(win);
}

void HomeView::OnAboutClose(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseAboutWindow();
    HomeNotify(self->win, cx);
}

void HomeView::OnAboutHover(HomeView* self, gp::Ctx* cx, const gp::HoverCardOpenChangeEvent* ev) {
    logf("HomePage: about hover open: %d\n", (int)ev->open);
    HomeNotify(self->win, cx);
}

static void EnsureHomeView(MainWindow* win, gp::Ctx* cx) {
    HomePageUI* h = Ui(win);
    if (!h->view.IsValid()) {
        h->view = gp::EntityNewState<HomeView>(cx->app);
    }
    auto* view = (HomeView*)gp::EntityGet(cx->app, h->view.id);
    if (view) {
        view->win = win;
    }
}

// --- the About box as a window of its own (Windows) --------------------------

static Str AboutToolTitle() {
    return Tr("About SumatraPDF");
}

// orig's WndProcAbout: Esc closes, Ctrl + C copies the about info
static bool AboutToolOnKey(MainWindow* win, gp::Ctx*, const gp::KeyEvent* ev) {
    if (ev->vk == VK_ESCAPE) {
        CloseAboutWindow();
        return true;
    }
    if (ev->vk == 'C' && ev->ctrl) {
        CopyAboutInfoToClipboard(win);
        return true;
    }
    return false;
}

static void AboutToolOnClosed(MainWindow*) {
    gAboutTw = nullptr;
    CloseAboutWindow();
}

// orig's window has no owner: it stays when the main window it was opened
// from closes
static void AboutToolOnOwnerClosed(MainWindow* newOwner) {
    gAboutWin = newOwner;
}

// the client area is the box
static gp::El* AboutToolBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gAboutVisible || !gAboutTw) {
        return nullptr;
    }
    EnsureHomeView(win, cx);
    gp::El* root = gp::Div(cx->a)->W(gp::kFill)->Flex1()->MinH(0)->FlexCol()->ItemsCenter()->JustifyCenter()->Bg(
        ToGpui(ThemeMainWindowBackgroundColor()));
    root->Child(AboutBoxEl(win, cx, false));
    return root;
}

static ToolWindowDesc AboutToolDesc() {
    // orig: WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, no owner
    ToolWindowDesc desc;
    desc.name = "about";
    desc.title = AboutToolTitle;
    desc.frame = ToolWinFrame::Caption;
    desc.owner = ToolWinOwner::TopLevel;
    desc.build = AboutToolBuild;
    desc.onKey = AboutToolOnKey;
    desc.onClosed = AboutToolOnClosed;
    desc.onOwnerClosed = AboutToolOnOwnerClosed;
    return desc;
}

// orig's ShowAboutWindow: the client area is the box, the window is centered
// on the frame (HwndPositionInCenterOf)
static void AboutOpenToolWindow(MainWindow* win) {
    if (gAboutTw || !ToolWindowsAvailable()) {
        return;
    }
    ToolWindowDesc desc = AboutToolDesc();
    Size outer = ToolWindowOuterSize(desc, win, Size(kAboutBoxDx, AboutBoxDy()));
    Rect frame = AppShellWindowScreenRect(win);
    Rect r{frame.x + (frame.dx - outer.dx) / 2, frame.y + (frame.dy - outer.dy) / 2, outer.dx, outer.dy};
    gAboutTw = ToolWindowOpen(desc, win, AppShellShiftToWorkArea(r, win, true));
}

gp::El* AboutDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gAboutVisible || gAboutTw || gAboutWin != win) {
        return nullptr;
    }
    EnsureHomeView(win, cx);
    gp::El* body = gp::Div(cx->a)->FlexCol()->ItemsCenter()->W(gp::kFill)->Child(AboutBoxEl(win, cx, false));
    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(Tr("About SumatraPDF")))
        ->Body(body)
        ->W((float)kAboutBoxDx + 40)
        ->OkText(ToGpui(Tr("Close")))
        ->ShowCancel(false)
        ->OnOk(gp::ListenTo(Ui(win)->view, &HomeView::OnAboutClose))
        ->OnCancel(gp::ListenTo(Ui(win)->view, &HomeView::OnAboutClose))
        ->OnClose(gp::ListenTo(Ui(win)->view, &HomeView::OnAboutClose))
        ->IntoEl(gp::WindowSize(cx->win));
}

// --- the About page (orig's DrawAboutPage) -----------------------------------

// shown in the Home tab instead of the start page when ShowStartPage is off
static gp::El* AboutPageBuild(MainWindow* win, gp::Ctx* cx) {
    Color colBg = ThemeMainWindowBackgroundColor();
    gp::El* root = gp::Div(cx->a)->FlexCol()->SizeFull()->ItemsCenter()->JustifyCenter()->Bg(ToGpui(colBg));
    root->Child(AboutBoxEl(win, cx, false));

    bool showLink = HasPermission(Perm::SavePreferences | Perm::DiskAccess) && SettingsRememberOpenedFiles();
    if (!showLink) {
        return root;
    }
    (void)win;
    gp::El* link = gp::TextEl(cx->a, ToGpui(Tr("Show frequently read")))
                       ->Font((float)kAboutTextFontPx)
                       ->BorderB(1, ToGpui(ThemeWindowTextDisabledColor()))
                       ->Fg(ToGpui(ThemeWindowTextColor()))
                       ->Cursor(gp::CursorKind::Pointer)
                       ->Absolute()
                       ->Right((float)kInnerPadding)
                       ->Bottom((float)kInnerPadding)
                       ->PathClick(GStrL("home-show-freq-read"))
                       ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnShowFreqRead));
    root->Child(link);
    return root;
}

// --- the start page ----------------------------------------------------------

static gp::El* HomeSvgIcon(gp::Ctx* cx, const char* svg, float size, Color color) {
    return gpc::Icon::Empty(cx)->Data(ToGpui(Str(svg)))->Size(size)->Color(ToGpui(color))->IntoEl();
}

static int HomeBrandStripesDx() {
    return (5 * kHomeBrandStripeDx) + (4 * kHomeBrandStripeGap);
}

// the five logo colors as stripes before the name in the window's text color
// (orig's SumatraLogo::asBrand)
static gp::El* HomeBrandEl(gp::Ctx* cx, Str name) {
    static Color cols[] = {kCol1, kCol2, kCol3, kCol4, kCol5};
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Shrink0()->Gap((float)kHomeBrandGap);
    gp::El* stripes = gp::Div(cx->a)->FlexRow()->Shrink0()->Gap((float)kHomeBrandStripeGap);
    for (Color c : cols) {
        stripes->Child(gp::Div(cx->a)->W((float)kHomeBrandStripeDx)->H((float)kHomeBrandStripeDy)->Bg(ToGpui(c)));
    }
    row->Child(stripes);
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, name))
                   ->Font((float)kHomeBrandFontPx)
                   ->Weight(gp::FontWeight::Semibold)
                   ->Fg(ToGpui(ThemeWindowTextColor())));
    return row;
}

static gp::El* HomeViewModeBtn(MainWindow* win, gp::Ctx* cx, bool listView) {
    Color colBg = ThemeMainWindowBackgroundColor();
    bool selected = listView == HomePageIsListView();
    Str tip = listView ? Tr("Show as list") : Tr("Show as thumbnails");
    const char* svg = listView ? gIconHomeList : gIconHomeThumbnails;
    gp::El* btn = gp::Div(cx->a)
                      ->W((float)HomePageIconSize() + 6)
                      ->H((float)HomePageIconSize() + 6)
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Cursor(gp::CursorKind::Pointer)
                      ->AriaLabel(ToGpui(tip))
                      ->Radius(4)
                      ->HoverBg(ToGpui(AccentColor(colBg, 14)))
                      ->PathClick(listView ? GStrL("home-view-list") : GStrL("home-view-thumbs"))
                      ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnViewMode, listView ? 1 : 0));
    if (listView) {
        btn->BoundsOut(&Ui(win)->listIconBounds);
    }
    if (selected) {
        btn->Bg(ToGpui(AccentColor(colBg, 28)));
    }
    btn->Child(HomeSvgIcon(cx, svg, (float)HomePageIconSize(), ThemeWindowTextColor()));
    return btn;
}

// one thumbnail: the page image with a hairline and a shadow, the reading
// progress along its bottom edge, the file name and page count under it and
// the ✕ that forgets the file (shown on hover, as orig does)
static gp::El* HomeThumbnailEl(MainWindow* win, gp::Ctx* cx, int idx, bool isSelected) {
    FileState* fs = Ui(win)->files[idx];
    Color colText = ThemeWindowTextColor();
    HomeThumb* t = HomeThumbFor(fs);
    Size frame = ThumbnailFrameSize();

    gp::El* cell = gp::Div(cx->a)
                       ->FlexCol()
                       ->W((float)frame.dx)
                       ->Shrink0()
                       ->Gap((float)kThumbsCaptionGapY)
                       ->Group()
                       ->Cursor(gp::CursorKind::Pointer)
                       ->PathClick(GpuiDup(cx->a, fmt("home-thumb-%d", idx)))
                       ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryClick, idx))
                       ->OnMouseDown(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryDown, idx))
                       ->OnHover(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryHover, idx))
                       ->BoundsOut(&Ui(win)->entryBounds[idx]);

    // a thumbnail smaller than the frame (a whole page, a cover) sits at the
    // bottom of its slot, centered; an empty frame stands in until there is one
    HomeThumbReadPng(t);
    Size drawn = frame;
    if (!t->size.IsEmpty() && t->size != frame) {
        drawn = ThumbnailDrawSize(t->size, frame);
    }
    gp::El* slot = gp::Div(cx->a)->FlexCol()->W((float)frame.dx)->H((float)frame.dy)->ItemsCenter()->JustifyEnd();
    // a soft shadow under the page, deeper on a dark page
    bool light = IsLightColor(ThemeMainWindowBackgroundColor());
    gp::BoxShadow shadow;
    shadow.y = 2;
    shadow.blur = 6;
    shadow.color = gp::Rgba8(0, 0, 0, light ? 40 : 120);
    gp::El* page = gp::Div(cx->a)
                       ->W((float)drawn.dx)
                       ->H((float)drawn.dy)
                       ->Shrink0()
                       ->Bg(ToGpui(ThemeControlBackgroundColor()))
                       ->Border((float)kThumbsBorderDx, ToGpui(ThemeEdgeColor()))
                       ->Shadows(&shadow, 1);
    slot->Child(page);
    gp::ImageSource src = gp::ImageSource::FromCustom(HomeThumbLoad, t);
    // no alt text: orig leaves the frame empty until the thumbnail is rendered
    page->Child(gp::Div(cx->a)->SizeFull()->ClipY()->Child(
        gp::ImageEl(cx->a, src, GStrL(""))->SizeFull()->ObjectFitMode(gp::ObjectFit::Cover)));
    // the selection ring, just outside the page; always laid out so its rect
    // is known, drawn for the selected entry
    int ringGap = kThumbsSelectionGap + kThumbsSelectionDx;
    gp::Rgba ringCol = isSelected ? ToGpui(colText) : gp::Rgba8(0, 0, 0, 0);
    page->Child(gp::Div(cx->a)
                    ->Absolute()
                    ->Left((float)-ringGap)
                    ->Top((float)-ringGap)
                    ->W((float)(drawn.dx + (2 * ringGap)))
                    ->H((float)(drawn.dy + (2 * ringGap)))
                    ->Radius(4)
                    ->Border((float)kThumbsSelectionDx, ringCol)
                    ->BoundsOut(&Ui(win)->ringBounds[idx]));
    // orig draws the ✕ in the top-right corner of the thumbnail under the mouse
    gp::El* close = gpc::Button::New(cx, GpuiDup(cx->a, fmt("home-forget-%d", idx)))
                        ->Icon(gp::IconName::Close)
                        ->WithSize(gp::UiSize::XSmall)
                        ->Compact()
                        ->Tooltip(ToGpui(Tr("Remove from Frequently Read")))
                        ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryForget, idx))
                        ->IntoEl();
    close->Absolute()->Right(5)->Top(5)->GroupHoverVisible();
    // orig's UpdateCloseBtnVisibility
    if (CanAccessDisk()) {
        page->Child(close);
    }
    float progress = 0;
    TempStr meta = HomeThumbnailMetaTemp(fs, &progress);
    if (progress > 0) {
        page->Child(gp::Div(cx->a)
                        ->Absolute()
                        ->Left(0)
                        ->Bottom(0)
                        ->W((float)drawn.dx * progress)
                        ->H((float)kThumbsProgressDy)
                        ->Bg(ToGpui(ThemeAccentColor())));
    }
    cell->Child(slot);

    // the caption: the name over the page count
    gp::El* caption = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->H((float)kThumbsCaptionDy)->Shrink0();
    // orig underlays the letters the search box matched (DrawMaybeHighlightedText)
    StrVec filterWords;
    SplitFilterToWords(HomeSearchQueryTemp(win), filterWords);
    caption->Child(gp::Div(cx->a)
                       ->FlexRow()
                       ->W(gp::kFill)
                       ->H((float)kThumbsCaptionLineDy)
                       ->ItemsCenter()
                       ->ClipX()
                       ->Child(FilterHighlightText(cx, path::GetBaseNameTemp(fs->filePath), filterWords,
                                                   ToGpui(colText), kHomeTextFontPx)
                                   ->Flex1()
                                   ->MinW(0)
                                   ->ClipX()));
    if (len(meta) > 0) {
        caption->Child(gp::TextEl(cx->a, GpuiDup(cx->a, meta))
                           ->Font((float)kHomeMetaFontPx)
                           ->Fg(ToGpui(ThemeWindowTextDisabledColor()))
                           ->H((float)kThumbsCaptionMetaDy)
                           ->Truncate());
    }
    cell->Child(caption);
    return cell;
}

// one list row: thumbnail, file name, directory, size, ✕ and the pin
static gp::El* HomeListRowEl(MainWindow* win, gp::Ctx* cx, int idx, bool isSelected) {
    FileState* fs = Ui(win)->files[idx];
    Color colText = ThemeWindowTextColor();
    Color colDim = ThemeWindowTextDisabledColor();
    const gp::Theme& th = gp::ThemeNow(cx->app);
    HomeThumb* t = HomeThumbFor(fs);

    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->W(gp::kFill)
                      ->H((float)kHomeListRowDy)
                      ->ItemsCenter()
                      ->Gap((float)kHomeListRowGapDx)
                      ->Radius(4)
                      ->Group()
                      ->Cursor(gp::CursorKind::Pointer)
                      ->HoverBg(th.tokens.muted)
                      ->PathClick(GpuiDup(cx->a, fmt("home-row-%d", idx)))
                      ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryClick, idx))
                      ->OnMouseDown(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryDown, idx))
                      ->BoundsOut(&Ui(win)->entryBounds[idx])
                      ->OnHover(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryHover, idx));
    if (isSelected) {
        row->Border(2, ToGpui(kHomeSelectionColor));
    }

    gp::ImageSource src = gp::ImageSource::FromCustom(HomeThumbLoad, t);
    row->Child(gp::Div(cx->a)
                   ->W((float)kHomeListThumbDx)
                   ->H((float)kHomeListThumbDy)
                   ->Shrink0()
                   ->ClipY()
                   ->Child(gp::ImageEl(cx->a, src, GStrL(""))->SizeFull()->ObjectFitMode(gp::ObjectFit::Contain)));
    StrVec filterWords;
    SplitFilterToWords(HomeSearchQueryTemp(win), filterWords);
    row->Child(
        FilterHighlightText(cx, path::GetBaseNameTemp(fs->filePath), filterWords, ToGpui(colText), kHomeTextFontPx)
            ->Shrink0());
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, path::GetDirTemp(fs->filePath)))
                   ->Font(12)
                   ->Fg(ToGpui(colDim))
                   ->Flex1()
                   ->MinW(0)
                   ->Truncate()
                   ->BoundsOut(&Ui(win)->listPathBounds[idx]));
    i64 size = file::GetSize(fs->filePath);
    if (size >= 0) {
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, str::FormatSizeShortTemp(size)))
                       ->Font(12)
                       ->Fg(ToGpui(colText))
                       ->Shrink0()
                       ->BoundsOut(&Ui(win)->listSizeBounds[idx]));
    }
    if (gSettings->showHomePageReadingProgress) {
        TempStr progress = FormatFileStateProgressTemp(fs);
        if (len(progress) > 0) {
            row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, progress))->Font(12)->Fg(ToGpui(colText))->Shrink0());
        }
    }
    row->Child(gpc::Button::New(cx, GpuiDup(cx->a, fmt("home-row-forget-%d", idx)))
                   ->Icon(gp::IconName::Close)
                   ->Ghost()
                   ->Compact()
                   ->WithSize(gp::UiSize::XSmall)
                   ->Tooltip(ToGpui(Tr("Remove from Frequently Read")))
                   ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryForget, idx))
                   ->IntoEl());
    auto* pin = gpc::Button::New(cx, GpuiDup(cx->a, fmt("home-row-pin-%d", idx)))
                    ->Icon(gpc::ButtonIcon::New(cx, gpc::Icon::Empty(cx)->Data(ToGpui(Str(gIconPin)))))
                    ->Ghost()
                    ->Compact()
                    ->WithSize(gp::UiSize::XSmall)
                    ->Selected(fs->isPinned)
                    ->Tooltip(ToGpui(fs->isPinned ? Tr("Unpin") : Tr("Pin")))
                    ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryPin, idx));
    row->Child(pin->IntoEl());
    return row;
}

static gp::El* HomeTipBandEl(MainWindow* win, gp::Ctx* cx, float padL, float padR) {
    Str line = SelectedTipLine();
    Vec<TipSpan>& spans = Ui(win)->tipSpans;
    TipSpansFree(spans);
    if (len(line) == 0) {
        return nullptr;
    }
    TipSpansParse(spans, line);

    const gp::Theme& th = gp::ThemeNow(cx->app);
    Color colText = ThemeWindowTextColor();
    gp::El* band = gp::Div(cx->a)
                       ->FlexRow()
                       ->FlexWrap()
                       ->W(gp::kFill)
                       ->Shrink0()
                       ->ItemsCenter()
                       ->PadL(padL)
                       ->PadR(padR)
                       ->PadY(8)
                       ->PathClick(GStrL("home-tip-band"))
                       ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnTipBand));
    for (int i = 0; i < len(spans); i++) {
        const TipSpan& sp = spans[i];
        gp::El* el = gp::TextEl(cx->a, GpuiDup(cx->a, sp.text))->Font((float)kHomeMetaFontPx);
        switch (sp.kind) {
            case TipSpanKind::Text:
                el->Fg(ToGpui(colText));
                break;
            case TipSpanKind::Link:
                el->Fg(ToGpui(ThemeWindowLinkColor()))
                    ->Underline()
                    ->Cursor(gp::CursorKind::Pointer)
                    // orig's VirtRichText::OnGetTooltip: the link's target
                    ->Tip(GpuiDup(cx->a, sp.target))
                    ->PathClick(GpuiDup(cx->a, fmt("home-tip-%d", i)))
                    ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnTipLink, i));
                break;
            case TipSpanKind::Kbd:
            case TipSpanKind::Code:
                el->Mono()->Font(11)->Fg(ToGpui(colText))->Bg(th.tokens.muted)->PadX(4)->Radius(3);
                break;
        }
        band->Child(el);
    }
    return band;
}

static gpc::PopupMenu* HomePopupFromModel(gp::Ctx* cx, MenuModel* model, Str id) {
    gpc::PopupMenu* menu = gpc::PopupMenu::New(cx, GpuiDup(cx->a, id))->MinW(200);
    if (!model) {
        return menu;
    }
    for (const MenuItemModel& it : model->items) {
        if (it.separator) {
            menu->Separator();
            continue;
        }
        menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(it.title).display), ActHomeMenu(),
                             (intptr_t)it.cmdId);
        if (len(it.accel) > 0) {
            menu->Kbd(GpuiDup(cx->a, it.accel));
        }
        menu->Disabled(it.disabled);
        menu->Checked(it.checked);
    }
    return menu;
}

gp::El* HomePageBuild(MainWindow* win, gp::Ctx* cx) {
    EnsureHomeView(win, cx);
    bool showStartPage = HasPermission(Perm::SavePreferences | Perm::DiskAccess) && gSettings &&
                         SettingsRememberOpenedFiles() && gSettings->showStartPage;
    if (!showStartPage) {
        Ui(win)->filesLaidOut = true;
        return AboutPageBuild(win, cx);
    }
    EnsureTipsParsed();

    HomePageUI* h = Ui(win);
    VecReset(h->files);
    StrVec filterWords;
    CollectHomePageFiles(win, h->files, filterWords);
    h->filesLaidOut = true;
    int nFiles = len(h->files);
    h->entryCount = nFiles;
    // gpui writes each entry's rect back through BoundsOut, so the slots have
    // to exist (and keep their address) before the entries are built
    while (len(h->entryBounds) < nFiles) {
        VecAppend(h->entryBounds, gpui::Bounds{});
    }
    while (len(h->ringBounds) < nFiles) {
        VecAppend(h->ringBounds, gpui::Bounds{});
    }
    while (len(h->listSizeBounds) < nFiles) {
        VecAppend(h->listSizeBounds, gpui::Bounds{});
    }
    while (len(h->listPathBounds) < nFiles) {
        VecAppend(h->listPathBounds, gpui::Bounds{});
    }
    // orig's homePageSelIdx starts at 0, so the first thumbnail is outlined
    // before any key is pressed
    if (nFiles > 0 && (h->selIdx < 0 || h->selIdx >= nFiles)) {
        h->selIdx = 0;
    }

    Color colBg = ThemeMainWindowBackgroundColor();
    Color colText = ThemeWindowTextColor();
    float frameDx = (float)win->canvasRc.dx;
    bool listView = HomePageIsListView();

    // header rows share the thumbnail grid's x, including when it is empty
    Size frame = ThumbnailFrameSize();
    int cols = (int)((frameDx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) /
                     (frame.dx + kThumbsSpaceBetweenX));
    cols = std::max(cols, 1);
    h->gridCols = cols;
    // End: the row pitch stops short of the last caption, so step from the
    // caption's laid-out bottom (issue #6234)
    if (h->scrollToEnd) {
        h->scrollToEnd = false;
        if (!listView && nFiles > 0 && h->entriesView.h > 0) {
            gp::Bounds last = h->entryBounds[nFiles - 1];
            if (last.h > 1.f) {
                float viewBottom = h->entriesView.y + h->entriesView.h;
                float maxScroll = last.y + last.h + h->scrollYLaidOut - viewBottom;
                if (maxScroll > h->scrollY + 0.5f) {
                    h->scrollY = maxScroll;
                }
            }
        }
    }
    int contentDx = (cols * frame.dx) + ((cols - 1) * kThumbsSpaceBetweenX);
    int thumbsStartX = kThumbsMarginLeft + (((int)frameDx - contentDx - kThumbsMarginLeft - kThumbsMarginRight) / 2);
    if (thumbsStartX < kInnerPadding) {
        thumbsStartX = kInnerPadding;
    }
    float startX = (float)thumbsStartX;
    float endX = std::max(0.f, frameDx - startX - (float)contentDx);

    // orig scrolls the entries from a wheel anywhere on the page
    gp::El* root = gp::Div(cx->a)
                       ->FlexCol()
                       ->SizeFull()
                       ->Bg(ToGpui(colBg))
                       ->OnScrollWheel(gp::ListenTo(Ui(win)->view, &HomeView::OnWheel));

    // one header row. The brand ends on the first column's right edge, the
    // Open link and view icons end on the last column's, and the search field
    // sits on the center line between them
    // orig's chrome-less About dropdown under the brand: shown after the
    // tooltip delay, it stays up while the cursor is over the brand or the
    // box, so its links stay clickable
#if OS_WIN
    Str logoName = StrL("SumatraPDF NG");
#else
    Str logoName = StrL(kAppName);
#endif
    gp::El* brand = gpc::HoverCard::New(cx, GStrL("home-about-hover"))
                        ->Trigger(HomeBrandEl(cx, logoName))
                        ->Content(AboutBoxEl(win, cx, true))
                        ->OpenDelay(kAboutHoverShowDelayMs)
                        ->CloseDelay(kAboutHoverHideDelayMs)
                        ->OnOpenChange(gp::ListenTo(Ui(win)->view, &HomeView::OnAboutHover))
                        ->IntoEl();
    brand->BoundsOut(&h->brandBounds);
    int brandDx = (int)h->brandBounds.w;
    if (brandDx <= 0) {
        // first frame: not measured yet
        brandDx = HomeBrandStripesDx() + kHomeBrandGap + (len(logoName) * 8);
    }

    // [open] "Open..."  [thumbnails] [list]
    Str openTxt = Tr("Open...");
    int openTextGap = 6;
    gp::El* openGroup = gp::Div(cx->a)
                            ->FlexRow()
                            ->ItemsCenter()
                            ->Gap((float)openTextGap)
                            ->Shrink0()
                            ->BoundsOut(&h->openGroupBounds)
                            ->Cursor(gp::CursorKind::Pointer)
                            ->PathClick(GStrL("home-open-doc"))
                            ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnOpenDoc));
    openGroup->Child(HomeSvgIcon(cx, gIconFileOpen, (float)HomePageIconSize(), colText));
    openGroup->Child(gp::TextEl(cx->a, GpuiDup(cx->a, openTxt))->Font((float)kHomeTextFontPx)->Fg(ToGpui(colText)));

    if (!h->search) {
        h->search = new gp::InputState();
        h->search->focus = gp::FocusHandleNew(cx->app);
        if (len(h->searchQuery) > 0) {
            gp::InputSetValue(h->search, ToGpui(h->searchQuery));
        }
    }
    TempStr cue = fmt(Tr("Search %d files (Ctrl + F)").s, CountHomePageFiles());
    gp::InputSetPlaceholder(h->search, ToGpui(cue));
    h->search->onChange = gp::ListenTo(Ui(win)->view, &HomeView::OnSearch);

    // the search box is centered; the brand and the Open group bound its width
    int iconGap = 4;
    int viewBtnDx = HomePageIconSize() + 6;
    int viewIconsDx = (2 * viewBtnDx) + iconGap;
    int openGroupDx = (int)h->openGroupBounds.w;
    if (openGroupDx <= 0) {
        // first frame: not measured yet
        openGroupDx = HomePageIconSize() + openTextGap + (len(openTxt) * 7);
    }
    int rowGapX = 16;
    int leftDx = std::max(brandDx, frame.dx) + rowGapX;
    int rightDx = openGroupDx + rowGapX + viewIconsDx + rowGapX;
    int flankDx = std::max(leftDx, rightDx);
    int borderDx = std::min(contentDx - (2 * flankDx), kHomeSearchMaxDx);
    borderDx = std::max(borderDx, 200);
    int borderDy = AppShellDpiPx(win, kSearchEditDy) + 2; // 1px border on each side
    int rowDy = std::max({viewBtnDx, borderDy, kHomeBrandStripeDy});

    // every row item (brand, link, search box, view icons) is centered on the
    // row's vertical centerline. Both flanks are as wide, so the middle is
    // centered over the grid
    gp::El* toolRow = gp::Div(cx->a)->FlexRow()->W((float)contentDx)->H((float)rowDy)->Shrink0()->ItemsCenter();
    gp::El* brandBox = gp::Div(cx->a)
                           ->FlexRow()
                           ->W((float)std::max(frame.dx, brandDx))
                           ->Shrink0()
                           ->ItemsCenter()
                           ->JustifyEnd()
                           ->Child(brand);
    toolRow->Child(gp::Div(cx->a)->FlexRow()->W((float)flankDx)->Shrink0()->ItemsCenter()->Child(brandBox));
    gp::El* searchBox = gp::Div(cx->a)
                            ->W((float)borderDx)
                            ->Shrink0()
                            ->H((float)borderDy)
                            ->BoundsOut(&h->searchBounds)
                            ->Child(gpc::Input::New(cx, GStrL("home-search"), h->search)
                                        ->WithSize(gp::UiSize::Small)
                                        ->W(gp::kFill)
                                        ->IntoEl()
                                        ->Font((float)AppShellDpiPx(win, kHomeSearchFontPx))
                                        ->H((float)borderDy));
    toolRow->Child(gp::Div(cx->a)->FlexRow()->Flex1()->MinW(0)->ItemsCenter()->JustifyCenter()->Child(searchBox));
    gp::El* viewBtns = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap((float)iconGap)->Shrink0();
    viewBtns->Child(HomeViewModeBtn(win, cx, false));
    viewBtns->Child(HomeViewModeBtn(win, cx, true));
    toolRow->Child(gp::Div(cx->a)
                       ->FlexRow()
                       ->W((float)flankDx)
                       ->Shrink0()
                       ->ItemsCenter()
                       ->JustifyEnd()
                       ->Gap((float)rowGapX)
                       ->Child(openGroup)
                       ->Child(viewBtns));
    root->Child(gp::Div(cx->a)
                    ->FlexRow()
                    ->W(gp::kFill)
                    ->Shrink0()
                    ->PadL(startX)
                    ->PadT((float)kHomeHeaderTopY)
                    ->PadB((float)kSearchThumbnailsGapY)
                    ->Child(toolRow));

    // the entries, scrolled as one block
    // Leave room above the first row so the selection ring's top edge
    // isn't clipped by the scrolled area (orig's thumbsContentPadTop)
    float contentPadTop = listView ? 2.f : (float)(kThumbsSelectionGap + kThumbsSelectionDx + 2);
    gp::El* content =
        gp::Div(cx->a)->FlexCol()->W(startX + (float)contentDx)->PadL(startX)->PadT(contentPadTop)->PadB(12);
    if (listView) {
        content->Gap(0);
        for (int i = 0; i < nFiles; i++) {
            content->Child(HomeListRowEl(win, cx, i, i == h->selIdx));
        }
    } else {
        // the row pitch is the frame plus kThumbsSpaceBetweenY; the caption
        // takes its share of the space
        content->Gap((float)(kThumbsSpaceBetweenY - kThumbsCaptionGapY - kThumbsCaptionDy));
        for (int row = 0; row * cols < nFiles; row++) {
            gp::El* rowEl = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->Gap((float)kThumbsSpaceBetweenX);
            for (int col = 0; col < cols; col++) {
                int idx = (row * cols) + col;
                if (idx >= nFiles) {
                    break;
                }
                rowEl->Child(HomeThumbnailEl(win, cx, idx, idx == h->selIdx));
            }
            content->Child(rowEl);
        }
    }
    h->scrollYLaidOut = h->scrollY;
    root->Child(gp::Div(cx->a)
                    ->Id(GStrL("home-entries"))
                    ->FlexCol()
                    ->Flex1()
                    ->W(gp::kFill)
                    ->MinH(0)
                    ->ScrollY(h->scrollY)
                    ->ScrollFromPath()
                    // orig has no bar here but its overlay one (below)
                    ->HideScrollbar()
                    ->OnScroll(gp::ListenTo(Ui(win)->view, &HomeView::OnScroll))
                    ->OnScrollWheel(gp::ListenTo(Ui(win)->view, &HomeView::OnWheel))
                    ->BoundsOut(&h->entriesView)
                    ->Child(content));

    // orig: thumbsBottomY = rc.dy - tipHeight - kThumbsMiddleMargin
    root->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)kThumbsMiddleMargin)->Shrink0());
    h->tipBounds = {};
    gp::El* tip = HomeTipBandEl(win, cx, startX, endX);
    if (tip) {
        tip->BoundsOut(&h->tipBounds);
        root->Child(tip);
    }
    gp::El* wrap = gp::Div(cx->a)
                       ->SizeFull()
                       ->CaptureKeyDown(gp::ListenTo(Ui(win)->view, &HomeView::OnSearchKey))
                       ->BoundsOut(&h->pageBounds)
                       ->OnMouseDown(gp::ListenTo(Ui(win)->view, &HomeView::OnPageDown))
                       ->OnMouseUp(gp::ListenTo(Ui(win)->view, &HomeView::OnPageUp))
                       ->Child(root);
    // orig's UpdateHomeOverlayScrollbar: the canvas' overlay bar, only in the
    // smart / overlay modes, along the whole right edge of the canvas
    int totalContentDy = HomeTotalContentDy(h);
    int thumbsVisibleDy = (int)h->entriesView.h;
    if (thumbsVisibleDy > 0) {
        h->askedForMeasure = false;
    } else if (!h->askedForMeasure) {
        h->askedForMeasure = true;
        AppShellInvalidate(win);
    }
    bool show = ScrollbarsUseOverlay() && thumbsVisibleDy > 0 && totalContentDy > thumbsVisibleDy;
    if (show) {
        OverlayScrollbarsSyncMode(win);
        show = IsOverlayScrollbarVisible(win->overlayScrollV);
    }
    if (show) {
        OverlayScrollbar* sb = win->overlayScrollV;
        OverlayScrollbarSetInfo(sb, 0, totalContentDy - 1, thumbsVisibleDy, (int)h->scrollY);
        gp::Listener onWheel = gp::ListenTo(Ui(win)->view, &HomeView::OnWheel);
        float k = CanvasScale(win);
        int barLen = (int)((float)win->canvasRc.dy / k);
        gp::El* bar = OverlayScrollbarBuild(cx, sb, StrL("home-scroll-v"), barLen, k, &onWheel);
        wrap->Child(bar->Absolute()->Right(0)->Top(0));
    }
    gpc::PopupMenu* popup = TrackPopup(cx, HomePopupFromModel(cx, h->menu, StrL("home-ctx-menu")));
    h->menuPopup = popup->state;
    // ng: ContextMenu::IntoEl() puts its own mouse-down listener on the child
    // it is given, in place of the child's (an element has one), and opens the
    // menu from the press. So the page goes inside a box: OnPageDown then runs
    // first and keeps the press from it, and the menu opens on release
    gp::El* wrapBox = gp::Div(cx->a)->SizeFull()->Child(wrap);
    gp::El* body = gpc::ContextMenu::New(cx, GStrL("home-ctx"))->Child(wrapBox)->Menu(popup)->IntoEl();
    body->OnAction(ActHomeMenu(), gp::ListenTo(Ui(win)->view, &HomeView::OnMenuAction));
    return body;
}

// --- keyboard navigation of the file list (issue #1136) ----------------------

static Rect BoundsRect(const gpui::Bounds& b) {
    return Rect{(int)b.x, (int)b.y, (int)b.w, (int)b.h};
}

static TempStr RectCsvTemp(const Rect& r) {
    return fmt("%d,%d,%d,%d", r.x, r.y, r.dx, r.dy);
}

// search / outline rects are window coordinates. outline is the part inside
// the entries viewport, so a thumbnail scrolled under the search field does
// not report a painted outline on top of it (issue #5978).
TempStr HomeSelectionForWindowTemp(int* exitCodeOut, int winIdx) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    MainWindow* win = (winIdx >= 0 && winIdx < len(gWindows)) ? gWindows[winIdx] : nullptr;
    if (!win) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    HomePageUI* h = Ui(win);
    if (!win->IsCurrentTabAbout()) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-layout")));
    }
    if (!h->filesLaidOut) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-layout")));
    }
    int sel = h->selIdx;
    Str path;
    if (sel >= 0 && sel < len(h->files)) {
        path = h->files[sel]->filePath;
    }
    bool searchFocus = h->search && win->gpuiWin && gp::FocusHandleIsFocused(win->gpuiWin, h->search->focus);
    int searchBox = h->search ? 1 : 0;
    Rect search = BoundsRect(h->searchBounds);
    Rect outlineFull;
    Rect outline;
    bool showSel = !HomePageIsListView() && !searchFocus && sel >= 0 && sel < len(h->ringBounds);
    if (showSel) {
        outlineFull = BoundsRect(h->ringBounds[sel]);
        outline = outlineFull.Intersect(BoundsRect(h->entriesView));
    }
    Rect lastCaption;
    if (!HomePageIsListView() && len(h->entryBounds) > 0) {
        lastCaption = BoundsRect(h->entryBounds[len(h->entryBounds) - 1]);
    }
    Rect tipRect = BoundsRect(h->tipBounds);
    return finish(0, fmt("OK sel=%d entries=%d searchFocus=%d searchBox=%d search=%s outline=%s outlineFull=%s path=%s "
                         "listView=%d listIcon=%s thumbsArea=%s lastCaption=%s tip=%d,%d tipRect=%s",
                         sel, h->entryCount, searchFocus ? 1 : 0, searchBox, RectCsvTemp(search), RectCsvTemp(outline),
                         RectCsvTemp(outlineFull), path, HomePageIsListView() ? 1 : 0,
                         RectCsvTemp(BoundsRect(h->listIconBounds)), RectCsvTemp(BoundsRect(h->entriesView)),
                         RectCsvTemp(lastCaption), gSelectedIsPromo ? 1 : 0, gSelectedTipIdx, RectCsvTemp(tipRect)));
}

TempStr HomeSelectionResultTemp(int* exitCodeOut) {
    return HomeSelectionForWindowTemp(exitCodeOut, 0);
}

// What the list view drew for each row. NOTREADY until the size column has
// been measured, which is the paint after the row is built.
TempStr HomeListRowsResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        out.Append(StrL("NOTREADY no-window\n"));
        return finish(2);
    }
    if (!win->IsCurrentTabAbout()) {
        out.Append(StrL("NOTREADY no-layout\n"));
        return finish(2);
    }
    if (!HomePageIsListView()) {
        out.Append(StrL("ERROR not-list-view\n"));
        return finish(1);
    }
    HomePageUI* h = Ui(win);
    if (!h->filesLaidOut) {
        out.Append(StrL("NOTREADY no-layout\n"));
        return finish(2);
    }
    int n = len(h->files);
    if (n > 0 && len(h->listSizeBounds) < n) {
        out.Append(StrL("NOTREADY no-layout\n"));
        return finish(2);
    }
    for (int i = 0; i < n; i++) {
        i64 size = file::GetSize(h->files[i]->filePath);
        gpui::Bounds& b = h->listSizeBounds[i];
        if (size >= 0 && (b.w <= 0 || b.h <= 0)) {
            out.Append(StrL("NOTREADY no-layout\n"));
            return finish(2);
        }
    }
    out.Append(fmt("OK rows=%d\n", n));
    for (int i = 0; i < n; i++) {
        FileState* fs = h->files[i];
        i64 size = file::GetSize(fs->filePath);
        TempStr sizeText = size < 0 ? str::DupTemp(StrL("")) : str::FormatSizeShortTemp(size);
        Rect r = size < 0 ? Rect{} : BoundsRect(h->listSizeBounds[i]);
        TempStr progress;
        if (gSettings && gSettings->showHomePageReadingProgress) {
            progress = FormatFileStateProgressTemp(fs);
        }
        if (str::IsNull(progress)) {
            progress = StrL("");
        }
        out.Append(fmt("row=%d size='%s' sizeRect=%d,%d,%d,%d progress='%s' path=%s\n", i, sizeText, r.x, r.y, r.dx,
                       r.dy, progress, fs->filePath));
        Rect pathRc = BoundsRect(h->listPathBounds[i]);
        out.Append(fmt("pathRect=%d,%d,%d,%d\n", pathRc.x, pathRc.y, pathRc.dx, pathRc.dy));
    }
    return finish(0);
}

void HomePageSelectFirst(MainWindow* win) {
    HomePageUI* h = Ui(win);
    h->selIdx = 0;
    h->searchReturnCol = 0;
}

Str HomePageSelectedFilePathTemp(MainWindow* win) {
    HomePageUI* h = Ui(win);
    int idx = h->selIdx;
    if (idx < 0 || idx >= len(h->files)) {
        return {};
    }
    return str::DupTemp(h->files[idx]->filePath);
}

void HomePageFocusSearch(MainWindow* win) {
    HomePageUI* h = Ui(win);
    if (!h->search || !win->gpuiWin) {
        return;
    }
    gp::InputFocus(h->search, win->gpuiWin->app, win->gpuiWin);
    gp::InputSelectAll(h->search, win->gpuiWin->app, win->gpuiWin);
}

// dCol/dRow are in grid steps; in list view only dRow matters
void HomePageMoveSelection(MainWindow* win, int dCol, int dRow) {
    HomePageUI* h = Ui(win);
    int n = h->entryCount;
    if (n == 0) {
        return;
    }
    int idx = h->selIdx;
    if (idx < 0 || idx >= n) {
        h->selIdx = 0;
        AppShellInvalidate(win);
        return;
    }
    int nCols = HomePageIsListView() ? 1 : h->gridCols;
    int delta = HomePageIsListView() ? dRow : dCol + (dRow * nCols);
    if (delta == 0) {
        return;
    }
    int newIdx = idx + delta;
    if (newIdx < 0) {
        // above the first row: hand focus to the search box, remember column
        if (dRow < 0 && h->search) {
            h->searchReturnCol = HomePageIsListView() ? 0 : (idx % nCols);
            HomePageFocusSearch(win);
            AppShellInvalidate(win);
            return;
        }
        newIdx = 0;
    }
    if (newIdx >= n) {
        newIdx = n - 1;
    }
    if (newIdx == idx) {
        return;
    }
    h->selIdx = newIdx;
    AppShellInvalidate(win);
}

// orig routes these through WndProcCanvasAbout; the shell's key handler calls
// this while the Home tab is the current one
bool HomePageOnKeyDown(MainWindow* win, int vk, bool ctrl) {
    if (!win->IsCurrentTabAbout()) {
        return false;
    }
    switch (vk) {
        case VK_LEFT:
            HomePageMoveSelection(win, -1, 0);
            return true;
        case VK_RIGHT:
            HomePageMoveSelection(win, 1, 0);
            return true;
        case VK_UP:
            HomePageMoveSelection(win, 0, -1);
            return true;
        case VK_DOWN:
            HomePageMoveSelection(win, 0, 1);
            return true;
        case VK_RETURN:
            OpenHomeEntry(win, Ui(win)->selIdx, ctrl);
            return true;
        case VK_DELETE: {
            // remove the keyboard-selected entry from file history (not from disk)
            Str path = HomePageSelectedFilePathTemp(win);
            if (len(path) > 0) {
                ForgetFileFromFrequentlyRead(win, path);
            }
            return true;
        }
    }
    return false;
}
