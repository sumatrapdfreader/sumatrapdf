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

#if OS_WIN
#include <shellapi.h>
#include <commctrl.h>
#endif

#include "SumatraLog.h"

// --- tips and promotions (orig's) -------------------------------------------

// One tip per line; translated when displayed.
static Str sumatraTips = StrL(R"tips(You can [customize scrollbar](CmdChangeScrollbar).
You can [customize keyboard shortcuts](Help/Customize-keyboard-shortcuts).
You can [customize toolbar](Help/Customize-toolbar).
Press (Kbd/(Key/CmdCommandPalette)) to open [command palette](CmdCommandPalette).
To open file from history open [command palette](CmdCommandPalette) with (Kbd/(Key/CmdCommandPalette)) and type (Kbd/#).
You can [extract text from PDF file](Help/Tool-x-extract-text-from-pdf).
You can [toggle menu bar](CmdToggleMenuBar) with (Kbd/(Key/CmdToggleMenuBar)).
You can [toggle toolbar](CmdToggleToolbar) with (Kbd/(Key/CmdToggleToolbar)).
You can [edit PDF annotations](Help/Editing-annotations).
You can enable [citation preview on hover](Help/Citation-hover-preview).
You can [have documents read aloud](Help/Read-Aloud).
You can [sign a PDF](Help/Sign-a-PDF).
You can [fill PDF forms](Help/Fill-PDF-forms).
You can [merge PDFs](Help/Merge-PDFs) and [reorder pages](Help/Reorder-PDF-pages).
You can [split a PDF](Help/Split-a-PDF).
You can [redact a PDF](Help/Redact-a-PDF).
You can [present a PDF](Help/Present-a-PDF) full screen.
You can [use SumatraPDF with LaTeX](Help/LaTeX-integration) for forward and inverse search.
You can [read comics and manga](Help/Comics-and-manga) right to left.
You can [bookmark pages as favorites](Help/Managing-favorites).
You can [chat with AI about a document](Help/AI-Chat-with-document).
You can [customize theme colors](Help/Customize-theme-colors).
You can [save a page region as an image](Help/Save-page-region-as-image).
You can [print selected pages](Help/Print-selected-pages).
)tips");

static Str sumatraPromos = StrL(R"promos(Try [Edna](https://edna.arslexis.io): a note taking web app for power users.
Try [MarkLexis](https://marklexis.arslexis.io): a bookmarking web application.
)promos");

static Str promoFromServer;

// the tip markup, one line each; the selected one is parsed by the tip band
static StrVec gTipLines;
static StrVec gPromoLines;
static bool gTipsParsed = false;
static bool gSelectedIsPromo = false;
static int gSelectedTipIdx = -1;

static void CollectTipsFromString(Str src, StrVec* out) {
    StrVec lines;
    Split(&lines, src, StrL("\n"));
    for (int i = 0; i < len(lines); i++) {
        Str line = lines[i];
        if (str::IsEmptyOrWhiteSpace(line)) {
            continue;
        }
        out->Append(line);
    }
}

// the markup of the tip currently on show, {} when there is none
static Str SelectedTipLine() {
    if (!gSettings->showTips || gSelectedTipIdx < 0) {
        return {};
    }
    StrVec& v = gSelectedIsPromo ? gPromoLines : gTipLines;
    if (gSelectedTipIdx >= len(v)) {
        return {};
    }
    if (gSelectedIsPromo) {
        return v[gSelectedTipIdx];
    }
    return str::JoinTemp(Tr("Tip:"), StrL(" "), Tr(v[gSelectedTipIdx]));
}

static void PickRandomTipOrPromo() {
    bool pickPromo = (len(gPromoLines) > 0) && (rand() % 100 < 30);
    if (pickPromo) {
        gSelectedIsPromo = true;
        gSelectedTipIdx = rand() % len(gPromoLines);
    } else if (len(gTipLines) > 0) {
        gSelectedIsPromo = false;
        gSelectedTipIdx = rand() % len(gTipLines);
    }
}

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

static void PickAnotherRandomTip() {
    bool prevIsPromo = gSelectedIsPromo;
    int prev = gSelectedTipIdx;
    // keep picking until we get a different one
    int maxIter = 100;
    while (maxIter-- > 0) {
        PickRandomTipOrPromo();
        if (gSelectedIsPromo != prevIsPromo || gSelectedTipIdx != prev) {
            return;
        }
    }
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

constexpr int kSumatraTxtFontSize = 24;
constexpr int kInnerPadding = 8;

// one row of the About screen's two-column table
struct AboutRow {
    Str leftTxt;
    Str rightTxt;
    Str url;
};

static AboutRow gAboutRows[] = {
    // a null rightTxt means "the app version", filled in at build time because
    // it isn't known until runtime (32/64-bit, debug)
    {StrL("version"), {}, {}},
    {StrL("built on"), StrL(__DATE__ " " __TIME__), {}},
    {StrL("manual"), StrL("SumatraPDF manual"), StrL("https://www.sumatrapdfreader.org/docs/SumatraPDF-documentation")},
    {StrL("version history"), StrL("What's new"), StrL("https://www.sumatrapdfreader.org/docs/Version-history")},
    {StrL("website"), StrL("SumatraPDF website"), Str(kWebsiteURL)},
    {StrL("forums"), StrL("SumatraPDF forums"), StrL("https://github.com/sumatrapdfreader/sumatrapdf/discussions")},
    {StrL("licenses"), StrL("Various Open Source"),
     StrL("https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS")},
#ifdef GIT_COMMIT_ID_STR
    {StrL("last change"), StrL("git commit " GIT_COMMIT_ID_STR),
     StrL("https://github.com/sumatrapdfreader/sumatrapdf/commit/" GIT_COMMIT_ID_STR)},
#endif
#ifdef PRE_RELEASE_VER
    {StrL("a note"), StrL("Pre-release version, for testing only!"), {}},
#endif
#if IS_DEBUG
    {StrL("a note"), StrL("Debug version, for testing only!"), {}},
#endif
    {{}, {}, {}}};

static TempStr GetAppVersionTemp() {
    TempStr s = str::DupTemp(StrL("v" CURR_VERSION_STRA));
    bool is64 = sizeof(void*) == 8;
    s = str::JoinTemp(s, is64 ? StrL(" 64-bit") : StrL(" 32-bit"));
    if (gIsDebugBuild) {
        s = str::JoinTemp(s, StrL(" (dbg)"));
    }
    return s;
}

constexpr Color kCol1 = MkRgb(196, 64, 50);
constexpr Color kCol2 = MkRgb(227, 107, 35);
constexpr Color kCol3 = MkRgb(93, 160, 40);
constexpr Color kCol4 = MkRgb(69, 132, 190);
constexpr Color kCol5 = MkRgb(112, 115, 207);

static TempStr TrimGitTemp(Str s) {
    if (gitCommidId && str::EndsWith(s, gitCommidId)) {
        int sLen = len(s);
        int gitLen = len(gitCommidId);
        return str::DupTemp(Str(s.s, sLen - gitLen - 7));
    }
    return s;
}

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
constexpr int kThumbsSpaceBetweenX = 38;
constexpr int kThumbsSpaceBetweenY = 58;
constexpr int kHomeListThumbDx = 30;
constexpr int kHomeListThumbDy = 40;
constexpr int kHomeListRowDy = 46;
constexpr int kHomeListRowGapDx = 8;
constexpr int kSearchEditDy = 28;
constexpr int kSearchThumbnailsGapY = 12;
constexpr int kThumbsMiddleMargin = 32;
// orig's HomePageIconSize: the icons follow ToolbarSize (dips here)
static int HomePageIconSize() {
    int sz = gSettings->toolbarSize;
    if (sz < 1) {
        sz = 16;
    }
    return RoundUp(sz, 4);
}

// light blue outline marking the keyboard-selected entry (issue #1136).
// A fixed color: it has to read as "selected" against both the light and the
// dark page background
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
    Str png; // the cache file's bytes, decoded lazily on the paint thread
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
    int selIdx = -1;
    int searchReturnCol = 0;
    int gridCols = 1;
    int entryCount = 0;
    gpui::Bounds entriesView{};
    // the entries' height is only known once they were painted: the first
    // frame asks for a second one so the scrollbar can be there
    bool askedForMeasure = false;
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
    // the "Open..." link as laid out last frame: orig's LayoutHomePage needs
    // its width, and gpui measures text only while painting
    gpui::Bounds openGroupBounds{};
    // the entries as laid out last frame, for the hover tooltip
    Vec<gpui::Bounds> entryBounds;
};

static HomePageUI* Ui(MainWindow* win) {
    if (!win->homePage) {
        win->homePage = new HomePageUI();
    }
    return win->homePage;
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

// the decoded thumbnail, or null while there is none. Runs on the paint
// thread, which is where RenderImageDecode has to happen.
static gp::ImageLoadState HomeThumbLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imgOut) {
    auto* t = (HomeThumb*)user;
    if (!t->tried) {
        t->tried = true;
        TempStr path = GetThumbnailPathTemp(t->fs->filePath);
        if (len(path) > 0) {
            t->png = file::ReadFile(path);
        }
        if (len(t->png) > 0) {
            t->img = gp::RenderImageDecode(pa, (const u8*)t->png.s, t->png.len);
        }
    }
    *imgOut = t->img;
    return t->img ? gp::ImageLoadState::Ready : gp::ImageLoadState::Failed;
}

#if OS_WIN
static void GetFileStateIcon(FileState* fs) {
    if (fs->himl) {
        return;
    }
    SHFILEINFOW sfi{};
    sfi.iIcon = -1;
    uint flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
    WCHAR* filePathW = CWStrTemp(fs->filePath);
    fs->himl = (HIMAGELIST)SHGetFileInfoW(filePathW, 0, &sfi, sizeof(sfi), flags);
    fs->iconIdx = sfi.iIcon;
}

struct HomeFileIcon {
    HomeFileIcon* next = nullptr;
    HIMAGELIST imageList = nullptr;
    int iconIdx = -1;
    Pixmap* pixmap = nullptr;
    // made from the pixmap on the paint thread, the first time it is drawn
    gp::RenderImage* img = nullptr;
    bool tried = false;
};

static HomeFileIcon* gFileIcons = nullptr;

// Shell image lists are Windows drawing objects. Convert each distinct icon to
// a Pixmap once; the home page draws it as a gpui image.
static HomeFileIcon* GetFileStateIconPixmap(FileState* fs) {
    GetFileStateIcon(fs);
    if (!fs->himl || fs->iconIdx < 0) {
        return nullptr;
    }
    for (HomeFileIcon* icon = gFileIcons; icon; icon = icon->next) {
        if (icon->imageList == fs->himl && icon->iconIdx == fs->iconIdx) {
            return icon->pixmap ? icon : nullptr;
        }
    }

    HICON hicon = ImageList_GetIcon(fs->himl, fs->iconIdx, ILD_TRANSPARENT);
    Pixmap* pixmap = nullptr;
    if (hicon) {
        pixmap = PixmapFromHICON(hicon);
        DestroyIcon(hicon);
    }
    auto* icon = new HomeFileIcon();
    icon->imageList = fs->himl;
    icon->iconIdx = fs->iconIdx;
    icon->pixmap = pixmap;
    ListInsertFront(&gFileIcons, icon);
    return pixmap ? icon : nullptr;
}

// the icon's pixels are premultiplied BGRA, which is what gpui takes
static gp::ImageLoadState HomeFileIconLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imgOut) {
    auto* icon = (HomeFileIcon*)user;
    Pixmap* px = icon->pixmap;
    if (!icon->tried) {
        icon->tried = true;
        if (px->stride == px->width * 4) {
            icon->img = gp::RenderImageFromBgra(pa, px->data, px->width, px->height);
        }
    }
    *imgOut = icon->img;
    return icon->img ? gp::ImageLoadState::Ready : gp::ImageLoadState::Failed;
}
#endif

// orig draws the shell's small icon for the file's type in front of its name.
// ng: the shell is asked on Windows only; elsewhere it is gpui's document glyph
static gp::El* HomeFileIconEl(MainWindow* win, gp::Ctx* cx, FileState* fs, Color colText) {
#if OS_WIN
    HomeFileIcon* icon = GetFileStateIconPixmap(fs);
    if (icon) {
        float k = CanvasScale(win);
        gp::ImageSource src = gp::ImageSource::FromCustom(HomeFileIconLoad, icon);
        return gp::ImageEl(cx->a, src, GStrL(""))
            ->W((float)icon->pixmap->width * k)
            ->H((float)icon->pixmap->height * k)
            ->Shrink0();
    }
#endif
    return gp::IconEl(cx->a, gp::IconName::FileText, 14)->Fg(ToGpui(colText))->Shrink0();
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

// Home-list entries with a path (same set as thumbnails when search is empty).
static int CountHomePageFiles() {
    Vec<FileState*> all;
    if (gSettings && gSettings->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(all);
    } else {
        FileHistoryGetRecentlyOpenedOrder(all);
    }
    int n = 0;
    for (FileState* fs : all) {
        if (fs && len(fs->filePath) > 0) {
            n++;
        }
    }
    return n;
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
    static void OnPalette(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnHelp(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*);
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

void HomeView::OnPalette(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    ExecuteCmd(self->win, CmdCommandPalette);
    HomeNotify(self->win, cx);
}

void HomeView::OnHelp(HomeView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    ExecuteCmd(self->win, CmdToggleKeyboardHelp);
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
    int thumbsRowDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
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
    int thumbsRowDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
    int scrollBy = thumbsRowDy / 3;
    if (ev->deltaY > 0) {
        scrollBy = -scrollBy;
    }
    float newScrollY = HomeClampScrollY(h, h->scrollY + (float)scrollBy);
    const_cast<gp::ScrollWheelEvent*>(ev)->propagate = false;
    if (newScrollY != h->scrollY) {
        h->scrollY = newScrollY;
        // orig's OverlayScrollbarSetInfo re-reveals the bar on a scroll
        OverlayScrollbarsNotifyScroll(self->win);
        HomeNotify(self->win, cx);
    }
}

void HomePageOnVScroll(MainWindow* win, ScrollMsg msg, int nTrackPos) {
    HomePageUI* h = Ui(win);
    int lineDy = HomePageIsListView() ? kHomeListRowDy : kThumbnailDy + kThumbsSpaceBetweenY;
    int pageDy = lineDy * 3;

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
    if (y != h->scrollY) {
        h->scrollY = y;
        AppShellInvalidate(win);
    }
}

void HomeView::OnScroll(HomeView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    Ui(self->win)->scrollY = ev->offsetY;
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

// the app name, each letter in a different color (orig's SumatraLogo)
// fontFamily: "Arial" for orig's "Arial Black"; empty for the UI font in bold
static gp::El* SumatraLogoEl(gp::Ctx* cx, float fontSize, Str fontFamily = {}) {
    static Color cols[] = {kCol1, kCol2, kCol3, kCol4, kCol5, kCol5, kCol4, kCol3, kCol2, kCol1};
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter();
    Str name = StrL(kAppName);
    for (int i = 0; i < len(name); i++) {
        gp::El* ch = gp::TextEl(cx->a, GpuiDup(cx->a, Str(name.s + i, 1)))
                         ->Font(fontSize)
                         ->Fg(ToGpui(cols[i % (int)dimof(cols)]));
        if (len(fontFamily) > 0) {
            // DirectWrite knows "Arial Black" as Arial at its heaviest weight
            ch->FontFamily(ToGpui(fontFamily))->Weight(gp::FontWeight::Black);
        } else {
            ch->Bold();
        }
        row->Child(ch);
    }
    return row;
}

// orig's two-column About table: the left column right-aligned, the right one
// left-aligned, a vertical divider between them, links in the link color
static gp::El* AboutTableEl(MainWindow* win, gp::Ctx* cx) {
    Color colText = ThemeWindowTextColor();
    Color colLink = ThemeWindowLinkColor();
    bool canAccessDisk = CanAccessDisk();

    gp::El* left = gp::Div(cx->a)->FlexCol()->ItemsEnd()->Gap(6);
    gp::El* right = gp::Div(cx->a)->FlexCol()->ItemsStart()->Gap(6);
    for (int i = 0; gAboutRows[i].leftTxt.s; i++) {
        AboutRow* el = &gAboutRows[i];
        left->Child(
            gp::TextEl(cx->a, GpuiDup(cx->a, el->leftTxt))->Font((float)kLeftTextFontSize)->Fg(ToGpui(colText))->H(18));
        Str txt = el->rightTxt.s ? TrimGitTemp(el->rightTxt) : Str(GetAppVersionTemp());
        bool isLink = canAccessDisk && len(el->url) > 0;
        gp::El* r = gp::TextEl(cx->a, GpuiDup(cx->a, txt))
                        ->Font((float)kRightTextFontSize)
                        ->Bold()
                        ->Fg(ToGpui(isLink ? colLink : colText))
                        ->H(18);
        if (isLink) {
            r->Underline()
                ->Cursor(gp::CursorKind::Pointer)
                ->PathClick(GpuiDup(cx->a, fmt("about-row-%d", i)))
                ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnAboutLink, i));
        }
        right->Child(r);
    }

    // orig draws a vertical line in the gap between the two columns
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsStretch()->Gap(16);
    row->Child(left);
    row->Child(gp::Div(cx->a)->W(1)->Shrink0()->Bg(ToGpui(colText)));
    row->Child(right);
    return row;
}

// the framed About box: the logo band on top of the two columns
static gp::El* AboutBoxEl(MainWindow* win, gp::Ctx* cx, bool withCopyBtn) {
    Color colText = ThemeWindowTextColor();
    Color colBg = ThemeMainWindowBackgroundColor();
    gp::El* box =
        gp::Div(cx->a)->FlexCol()->ItemsCenter()->Bg(ToGpui(colBg))->Border(2, ToGpui(colText))->PadX(10)->PadB(8);
    box->Child(gp::Div(cx->a)
                   ->W(gp::kFill)
                   ->PadY(6)
                   ->ItemsCenter()
                   ->JustifyCenter()
                   ->BorderB(2, ToGpui(colText))
                   ->Child(SumatraLogoEl(cx, (float)kSumatraTxtFontSize)));
    box->Child(gp::Div(cx->a)->PadT(6)->Child(AboutTableEl(win, cx)));
    if (withCopyBtn) {
        box->Child(gp::Div(cx->a)->PadT(12)->Child(gpc::Button::New(cx, GStrL("about-copy-info"))
                                                       ->Label(ToGpui(Tr("Copy program and machine info to clipboard")))
                                                       ->Outline()
                                                       ->WithSize(gp::UiSize::Small)
                                                       ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnCopyInfo))
                                                       ->IntoEl()));
    }
    return box;
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

// orig's sizes (HomePage.cpp)
constexpr int kAboutLineOuterSize = 2;
constexpr int kAboutLineSepSize = 1;
constexpr int kAboutLeftRightSpaceDx = 8;
constexpr int kAboutMarginDx = 10;
constexpr int kAboutBoxMarginDy = 6;
constexpr int kAboutTxtDy = 6;
constexpr int kAboutRectPadding = 8;
constexpr int kAboutBtnGap = 12;
constexpr int kAboutMaxRows = 16;
// orig's themed button: the text in the 12 px app font plus 2 x 12 by 2 x 5
constexpr float kAboutBtnFontPx = 12;
constexpr int kAboutBtnPadDx = 12;
constexpr int kAboutBtnPadDy = 5;

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

// orig's AboutCtrl::UpdateLayout, in dips: the box (title band, two-column
// table, button) as GDI measures its texts in orig's fonts. ng: the window
// has to have its size before it is made
struct AboutDims {
    int nRows = 0;
    float headerDy = 0;
    float leftDx = 0;
    float rowDy[kAboutMaxRows]{};
    float tableDy = 0;
    float btnDx = 0;
    float btnDy = 0;
    // the framed box, relative to the client area
    RectF box;
};

static Str AboutRightText(int row) {
    AboutRow* el = &gAboutRows[row];
    return el->rightTxt.s ? TrimGitTemp(el->rightTxt) : Str(GetAppVersionTemp());
}

static void MeasureAbout(MainWindow* win, AboutDims& d) {
    int dpi = std::max(AppShellWindowDpi(win), 96);
    float toDip = 96.f / (float)dpi;
    auto scale = [dpi](int v) { return MulDiv(v, dpi, 96); };
    PlatformFont* fontLogo = GetUserGuiFont(StrL("Arial Black"), scale(kSumatraTxtFontSize));
    PlatformFont* fontLeft = GetUserGuiFont(Str(kLeftTextFont), scale(kLeftTextFontSize));
    PlatformFont* fontRight = GetUserGuiFont(Str(kRightTextFont), scale(kRightTextFontSize));

    Size logo = PlatformFontMeasureText(fontLogo, StrL(kAppName));
    int headerDx = logo.dx + 2 * scale(kInnerPadding);
    int headerDy = logo.dy + scale(kAboutBoxMarginDy * 2);

    int leftDx = 0;
    int rightDx = 0;
    int tableDy = 0;
    int n = 0;
    for (; gAboutRows[n].leftTxt.s && n < kAboutMaxRows; n++) {
        Size l = PlatformFontMeasureText(fontLeft, gAboutRows[n].leftTxt);
        Size r = PlatformFontMeasureText(fontRight, AboutRightText(n));
        leftDx = std::max(leftDx, l.dx);
        rightDx = std::max(rightDx, r.dx);
        int rowDy = std::max(l.dy, r.dy);
        d.rowDy[n] = (float)rowDy * toDip;
        tableDy += rowDy + (n > 0 ? scale(kAboutTxtDy) : 0);
    }
    int tableDx = leftDx + 2 * scale(kAboutLeftRightSpaceDx) + rightDx;

    Size btnText = PlatformFontMeasureText(GetDefaultGuiFont(), Tr("Copy program and machine info to clipboard"));
    int btnDx = btnText.dx + 2 * scale(kAboutBtnPadDx);
    int btnDy = btnText.dy + 2 * scale(kAboutBtnPadDy);
    int copyBlockDy = scale(kAboutBtnGap) + btnDy + scale(kAboutRectPadding);

    int marginDx = scale(kAboutMarginDx);
    int boxDx = std::max(tableDx + kAboutLineSepSize, headerDx) + (2 * kAboutLineOuterSize) + (2 * marginDx);
    boxDx = std::max(boxDx, btnDx + (2 * marginDx) + (2 * kAboutLineOuterSize));
    // one extra row gap so the last row isn't flush against the frame
    int boxDy = headerDy + tableDy + scale(kAboutTxtDy) + (2 * kAboutLineOuterSize) + 4 + copyBlockDy;

    d.nRows = n;
    d.headerDy = (float)headerDy * toDip;
    d.leftDx = (float)leftDx * toDip;
    d.tableDy = (float)tableDy * toDip;
    d.btnDx = (float)btnDx * toDip;
    d.btnDy = (float)btnDy * toDip;
    d.box = RectF((float)kAboutRectPadding, (float)kAboutRectPadding, (float)boxDx * toDip, (float)boxDy * toDip);
}

static gp::El* AboutToolBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gAboutVisible || !gAboutTw) {
        return nullptr;
    }
    EnsureHomeView(win, cx);
    AboutDims d;
    MeasureAbout(win, d);
    float fontScale = ToolWindowSetUiFontPx(cx, kAboutBtnFontPx);
    Color colText = ThemeWindowTextColor();
    Color colLink = ThemeWindowLinkColor();
    gp::Rgba line = ToGpui(colText);
    bool canAccessDisk = CanAccessDisk();
    RectF r = d.box;

    gp::El* root = gp::Div(cx->a)->W(gp::kFill)->Flex1()->MinH(0)->Bg(ToGpui(ThemeMainWindowBackgroundColor()));
    // the frame and the line under the title band
    root->Child(
        gp::Div(cx->a)->Absolute()->Left(r.x)->Top(r.y)->W(r.dx)->H(r.dy)->Border((float)kAboutLineOuterSize, line));
    root->Child(
        gp::Div(cx->a)->Absolute()->Left(r.x)->Top(r.y + d.headerDy)->W(r.dx)->H((float)kAboutLineOuterSize)->Bg(line));
    root->Child(gp::Div(cx->a)
                    ->Absolute()
                    ->Left(r.x)
                    ->Top(r.y)
                    ->W(r.dx)
                    ->H(d.headerDy)
                    ->FlexRow()
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Child(SumatraLogoEl(cx, (float)kSumatraTxtFontSize * fontScale, StrL("Arial"))));

    float x = r.x + (float)(kAboutLineOuterSize + kAboutMarginDx);
    float tableY = r.y + d.headerDy + 4;
    float rightX = x + d.leftDx + 2 * (float)kAboutLeftRightSpaceDx;
    float y = tableY;
    for (int i = 0; i < d.nRows; i++) {
        AboutRow* el = &gAboutRows[i];
        root->Child(gp::Div(cx->a)
                        ->Absolute()
                        ->Left(x)
                        ->Top(y)
                        ->W(d.leftDx)
                        ->H(d.rowDy[i])
                        ->FlexRow()
                        ->ItemsCenter()
                        ->JustifyEnd()
                        ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, el->leftTxt))
                                    ->FontFamily(GStrL("Arial"))
                                    ->Font((float)kLeftTextFontSize * fontScale)
                                    ->Fg(line)
                                    ->Shrink0()));
        bool isLink = canAccessDisk && len(el->url) > 0;
        gp::El* txt = gp::TextEl(cx->a, GpuiDup(cx->a, AboutRightText(i)))
                          ->FontFamily(GStrL("Arial"))
                          ->Weight(gp::FontWeight::Black)
                          ->Font((float)kRightTextFontSize * fontScale)
                          ->Fg(ToGpui(isLink ? colLink : colText))
                          ->Shrink0();
        if (isLink) {
            txt->Underline()
                ->Cursor(gp::CursorKind::Pointer)
                ->Tip(GpuiDup(cx->a, el->url))
                ->PathClick(GpuiDup(cx->a, fmt("about-row-%d", i)))
                ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnAboutLink, i));
        }
        root->Child(
            gp::Div(cx->a)->Absolute()->Left(rightX)->Top(y)->H(d.rowDy[i])->FlexRow()->ItemsCenter()->Child(txt));
        y += d.rowDy[i] + (float)kAboutTxtDy;
    }
    // the divider is drawn inside the gap between the two columns
    root->Child(gp::Div(cx->a)
                    ->Absolute()
                    ->Left(rightX - (float)kAboutLeftRightSpaceDx)
                    ->Top(tableY)
                    ->W((float)kAboutLineSepSize)
                    ->H(d.tableDy)
                    ->Bg(line));
    root->Child(gp::Div(cx->a)
                    ->Absolute()
                    ->Left(r.x + (r.dx - d.btnDx) / 2)
                    ->Top(r.y + r.dy - (float)kAboutRectPadding - d.btnDy)
                    ->W(d.btnDx)
                    ->H(d.btnDy)
                    ->Child(gpc::Button::New(cx, GStrL("about-copy-info"))
                                ->Label(ToGpui(Tr("Copy program and machine info to clipboard")))
                                ->Outline()
                                ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnCopyInfo))
                                ->IntoEl()
                                ->W(d.btnDx)
                                ->H(d.btnDy)
                                ->PadX(0)));
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

// orig's ShowAboutWindow: the client area is the box plus kAboutRectPadding
// around, the window is centered on the frame (HwndPositionInCenterOf)
static void AboutOpenToolWindow(MainWindow* win) {
    if (gAboutTw || !ToolWindowsAvailable()) {
        return;
    }
    AboutDims d;
    MeasureAbout(win, d);
    int dx = (int)(d.box.dx + 0.5f) + 2 * kAboutRectPadding;
    int dy = (int)(d.box.dy + 0.5f) + 2 * kAboutRectPadding;
    ToolWindowDesc desc = AboutToolDesc();
    Size outer = ToolWindowOuterSize(desc, win, Size(dx, dy));
    Rect frame = AppShellWindowScreenRect(win);
    Rect r{frame.x + (frame.dx - outer.dx) / 2, frame.y + (frame.dy - outer.dy) / 2, outer.dx, outer.dy};
    gAboutTw = ToolWindowOpen(desc, win, AppShellShiftToWorkArea(r, win, true));
}

gp::El* AboutDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gAboutVisible || gAboutTw || gAboutWin != win) {
        return nullptr;
    }
    EnsureHomeView(win, cx);
    gp::El* body = gp::Div(cx->a)->FlexCol()->ItemsCenter()->W(gp::kFill)->Child(AboutBoxEl(win, cx, true));
    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(Tr("About SumatraPDF")))
        ->Body(body)
        ->W(520)
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
                       ->Font(16)
                       ->Underline()
                       ->Fg(ToGpui(ThemeWindowLinkColor()))
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

// orig's white circle with a glyph: the command palette and the keyboard help
static gp::El* HomeSvgIcon(gp::Ctx* cx, const char* svg, float size, Color color) {
    return gpc::Icon::Empty(cx)->Data(ToGpui(Str(svg)))->Size(size)->Color(ToGpui(color))->IntoEl();
}

static gp::El* HomeCircleBtn(gp::Ctx* cx, Str id, Str glyph, const char* svg, Str tooltip, gp::Listener onClick) {
    gp::El* btn = gp::Div(cx->a)
                      ->W(30)
                      ->H(30)
                      ->Radius(15)
                      ->Bg(ToGpui(kColWhite))
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Cursor(gp::CursorKind::Pointer)
                      ->AriaLabel(GpuiDup(cx->a, tooltip))
                      ->PathClick(GpuiDup(cx->a, id))
                      ->OnClick(onClick);
    if (svg) {
        btn->Child(HomeSvgIcon(cx, svg, 16, kColBlack));
    } else {
        btn->Child(gp::TextEl(cx->a, GpuiDup(cx->a, glyph))->Font(14)->Bold()->Fg(ToGpui(kColBlack)));
    }
    return btn;
}

// e.g. "Command Palette (Ctrl + K)"
static TempStr AppendCmdAccel(Str base, int cmd) {
    TempStr accel = AppendAccelKeyToMenuStringTemp({}, cmd);
    if (len(accel) == 0) {
        return str::DupTemp(base);
    }
    return str::JoinTemp(base, fmt(" (%s)", Str(accel.s + 1, len(accel) - 1))); // +1 skips the leading \t
}

static gp::El* HomeViewModeBtn(MainWindow* win, gp::Ctx* cx, bool listView) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
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
                      ->HoverBg(th.tokens.muted)
                      ->PathClick(listView ? GStrL("home-view-list") : GStrL("home-view-thumbs"))
                      ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnViewMode, listView ? 1 : 0));
    if (selected) {
        btn->Bg(th.tokens.accent)->Border(1, th.border);
    }
    btn->Child(HomeSvgIcon(cx, svg, (float)HomePageIconSize(), ThemeWindowTextColor()));
    return btn;
}

// one thumbnail: the page image with a rounded outline, the file name under it
// and the ✕ that forgets the file (shown on hover, as orig does)
static gp::El* HomeThumbnailEl(MainWindow* win, gp::Ctx* cx, int idx, bool isSelected) {
    FileState* fs = Ui(win)->files[idx];
    Color colText = ThemeWindowTextColor();
    HomeThumb* t = HomeThumbFor(fs);

    gp::El* cell = gp::Div(cx->a)
                       ->FlexCol()
                       ->W((float)kThumbnailDx)
                       ->Shrink0()
                       ->Gap(3)
                       ->Group()
                       ->Cursor(gp::CursorKind::Pointer)
                       ->PathClick(GpuiDup(cx->a, fmt("home-thumb-%d", idx)))
                       ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryClick, idx))
                       ->OnMouseDown(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryDown, idx))
                       ->BoundsOut(&Ui(win)->entryBounds[idx])
                       ->OnHover(gp::ListenTo(Ui(win)->view, &HomeView::OnEntryHover, idx));

    gp::El* page = gp::Div(cx->a)
                       ->W((float)kThumbnailDx)
                       ->H((float)kThumbnailDy)
                       ->Radius(10)
                       ->ClipY()
                       ->Border((float)kThumbsBorderDx, ToGpui(colText));
    gp::ImageSource src = gp::ImageSource::FromCustom(HomeThumbLoad, t);
    // no alt text: orig leaves the frame empty until the thumbnail is rendered
    page->Child(gp::ImageEl(cx->a, src, GStrL(""))->SizeFull()->ObjectFitMode(gp::ObjectFit::Cover));
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
    if (gSettings->showHomePageReadingProgress) {
        TempStr progress = FormatFileStateProgressTemp(fs);
        if (len(progress) > 0) {
            page->Child(gp::TextEl(cx->a, GpuiDup(cx->a, progress))
                            ->Font(11)
                            ->Fg(ToGpui(kColWhite))
                            ->Bg(ToGpui(kColBlack))
                            ->PadX(5)
                            ->PadY(2)
                            ->Radius(3)
                            ->Absolute()
                            ->Right(5)
                            ->Bottom(5));
        }
    }
    cell->Child(page);

    gp::El* nameRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(4);
    nameRow->Child(HomeFileIconEl(win, cx, fs, colText));
    // orig underlays the letters the search box matched (DrawMaybeHighlightedText)
    StrVec filterWords;
    SplitFilterToWords(HomeSearchQueryTemp(win), filterWords);
    nameRow->Child(FilterHighlightText(cx, path::GetBaseNameTemp(fs->filePath), filterWords, ToGpui(colText), 14)
                       ->Flex1()
                       ->MinW(0)
                       ->ClipX());
    cell->Child(nameRow);

    gp::El* outer = gp::Div(cx->a)->FlexCol()->Shrink0()->Pad(3)->Radius(10)->Child(cell);
    if (isSelected) {
        outer->Border(2, ToGpui(kHomeSelectionColor));
    }
    return outer;
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
        FilterHighlightText(cx, path::GetBaseNameTemp(fs->filePath), filterWords, ToGpui(colText), 14)->Shrink0());
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, path::GetDirTemp(fs->filePath)))
                   ->Font(12)
                   ->Fg(ToGpui(colDim))
                   ->Flex1()
                   ->MinW(0)
                   ->Truncate());
    i64 size = file::GetSize(fs->filePath);
    if (size >= 0) {
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, str::FormatSizeShortTemp(size)))
                       ->Font(12)
                       ->Fg(ToGpui(colText))
                       ->Shrink0());
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
                       ->Bg(ToGpui(ThemeControlBackgroundColor()))
                       ->PathClick(GStrL("home-tip-band"))
                       ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnTipBand));
    for (int i = 0; i < len(spans); i++) {
        const TipSpan& sp = spans[i];
        gp::El* el = gp::TextEl(cx->a, GpuiDup(cx->a, sp.text))->Font(16);
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
                el->Mono()->Font(14)->Fg(ToGpui(colText))->Bg(th.tokens.muted)->PadX(4)->Radius(3);
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
        return AboutPageBuild(win, cx);
    }
    EnsureTipsParsed();

    HomePageUI* h = Ui(win);
    VecReset(h->files);
    StrVec filterWords;
    CollectHomePageFiles(win, h->files, filterWords);
    int nFiles = len(h->files);
    h->entryCount = nFiles;
    // gpui writes each entry's rect back through BoundsOut, so the slots have
    // to exist (and keep their address) before the entries are built
    while (len(h->entryBounds) < nFiles) {
        VecAppend(h->entryBounds, gpui::Bounds{});
    }
    if (h->selIdx >= nFiles) {
        h->selIdx = nFiles - 1;
    }

    Color colBg = ThemeMainWindowBackgroundColor();
    Color colText = ThemeWindowTextColor();
    float frameDx = (float)win->canvasRc.dx;
    bool listView = HomePageIsListView();

    // orig's LayoutHomePage: the thumbnail grid is centered in the canvas and
    // the header rows align with it; with no files at all it sits at the left
    // margin. The unfiltered count is used so the layout stays stable when a
    // search filters the results
    int nFilesForLayout = CountHomePageFiles();
    int cols = (int)((frameDx - kThumbsMarginLeft - kThumbsMarginRight + kThumbsSpaceBetweenX) /
                     (kThumbnailDx + kThumbsSpaceBetweenX));
    cols = std::max(cols, 1);
    h->gridCols = cols;
    int contentDx = (cols * kThumbnailDx) + ((cols - 1) * kThumbsSpaceBetweenX);
    int thumbsStartX = kThumbsMarginLeft + (((int)frameDx - contentDx - kThumbsMarginLeft - kThumbsMarginRight) / 2);
    if (thumbsStartX < kInnerPadding) {
        thumbsStartX = kInnerPadding;
    } else if (nFilesForLayout == 0) {
        thumbsStartX = kThumbsMarginLeft;
    }
    float startX = (float)thumbsStartX;
    float endX = std::max(0.f, frameDx - startX - (float)contentDx);

    gp::El* root = gp::Div(cx->a)->FlexCol()->SizeFull()->Bg(ToGpui(colBg));

    // [command palette] SumatraPDF [keyboard shortcuts]
    // centered over the grid, not over the canvas
    gp::El* logoRow =
        gp::Div(cx->a)->FlexRow()->W((float)contentDx)->Shrink0()->ItemsCenter()->JustifyCenter()->Gap(10);
    logoRow->Child(HomeCircleBtn(cx, StrL("home-palette"), {}, gIconCommandPalette,
                                 AppendCmdAccel(Tr("Command Palette"), CmdCommandPalette),
                                 gp::ListenTo(Ui(win)->view, &HomeView::OnPalette)));
    // orig's chrome-less About dropdown under the logo: shown after the tooltip
    // delay, it stays up while the cursor is over the logo or the box, so its
    // links stay clickable
    logoRow->Child(gpc::HoverCard::New(cx, GStrL("home-about-hover"))
                       ->Trigger(SumatraLogoEl(cx, (float)kSumatraTxtFontSize))
                       ->Content(AboutBoxEl(win, cx, false))
                       ->OpenDelay(kAboutHoverShowDelayMs)
                       ->CloseDelay(kAboutHoverHideDelayMs)
                       ->OnOpenChange(gp::ListenTo(Ui(win)->view, &HomeView::OnAboutHover))
                       ->IntoEl());
    logoRow->Child(HomeCircleBtn(cx, StrL("home-help"), StrL("?"), nullptr,
                                 AppendCmdAccel(Tr("Keyboard Shortcuts"), CmdToggleKeyboardHelp),
                                 gp::ListenTo(Ui(win)->view, &HomeView::OnHelp)));
    root->Child(gp::Div(cx->a)->FlexRow()->W(gp::kFill)->Shrink0()->PadL(startX)->PadT(8)->Child(logoRow));

    // [open] "Open..."      search box      [thumbnails] [list]
    Str openTxt = Tr("Open...");
    gp::El* openGroup = gp::Div(cx->a)
                            ->FlexRow()
                            ->ItemsCenter()
                            ->Gap(3)
                            ->Shrink0()
                            ->BoundsOut(&h->openGroupBounds)
                            ->Cursor(gp::CursorKind::Pointer)
                            ->PathClick(GStrL("home-open-doc"))
                            ->OnClick(gp::ListenTo(Ui(win)->view, &HomeView::OnOpenDoc));
    openGroup->Child(HomeSvgIcon(cx, gIconFileOpen, (float)HomePageIconSize(), colText));
    openGroup->Child(
        gp::TextEl(cx->a, GpuiDup(cx->a, openTxt))->Font(14)->Underline()->Fg(ToGpui(ThemeWindowLinkColor())));

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

    // the search box takes what is left between the link and the icons; both
    // sides are padded to the wider of the two, so the box sits centered
    int iconGap = 4;
    int viewIconsDx = (2 * HomePageIconSize()) + iconGap;
    int openGroupDx = (int)h->openGroupBounds.w;
    if (openGroupDx <= 0) {
        // first frame: not measured yet
        openGroupDx = HomePageIconSize() + 3 + (len(openTxt) * 7);
    }
    int rowGapX = 16;
    int flankDx = std::max(viewIconsDx, openGroupDx) + rowGapX;
    int borderDx = contentDx - (2 * flankDx);
    borderDx = std::max(borderDx, 200);
    int borderX = (contentDx - borderDx) / 2;
    int borderDy = kSearchEditDy + 2; // 1px border on each side
    int rowDy = std::max(HomePageIconSize(), borderDy);

    // every row item (link, search box, view icons) is centered on the row's
    // vertical centerline
    gp::El* toolRow = gp::Div(cx->a)->FlexRow()->W((float)contentDx)->H((float)rowDy)->Shrink0()->ItemsCenter();
    toolRow->Child(
        gp::Div(cx->a)->FlexRow()->W((float)std::max(0, borderX))->Shrink0()->ItemsCenter()->Child(openGroup));
    toolRow->Child(gp::Div(cx->a)
                       ->W((float)borderDx)
                       ->Shrink0()
                       ->H((float)borderDy)
                       ->Child(gpc::Input::New(cx, GStrL("home-search"), h->search)
                                   ->WithSize(gp::UiSize::Small)
                                   ->W(gp::kFill)
                                   ->IntoEl()
                                   ->H((float)borderDy)));
    gp::El* viewBtns = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap((float)iconGap)->Shrink0();
    viewBtns->Child(HomeViewModeBtn(win, cx, false));
    viewBtns->Child(HomeViewModeBtn(win, cx, true));
    toolRow->Child(gp::Div(cx->a)->FlexRow()->Flex1()->MinW(0)->ItemsCenter()->JustifyEnd()->Child(viewBtns));
    root->Child(gp::Div(cx->a)
                    ->FlexRow()
                    ->W(gp::kFill)
                    ->Shrink0()
                    ->PadL(startX)
                    ->PadT(8)
                    ->PadB((float)kSearchThumbnailsGapY)
                    ->Child(toolRow));

    // the entries, scrolled as one block
    // Leave room above the first row so the selection outline's top edge
    // isn't clipped by the scrolled area (orig's thumbsContentPadTop)
    float contentPadTop = listView ? 2.f : 5.f;
    gp::El* content =
        gp::Div(cx->a)->FlexCol()->W(startX + (float)contentDx)->PadL(startX)->PadT(contentPadTop)->PadB(12);
    if (listView) {
        content->Gap(0);
        for (int i = 0; i < nFiles; i++) {
            content->Child(HomeListRowEl(win, cx, i, i == h->selIdx));
        }
    } else {
        content->Gap((float)kThumbsSpaceBetweenY - 24);
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
    gp::El* tip = HomeTipBandEl(win, cx, startX, endX);
    if (tip) {
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
