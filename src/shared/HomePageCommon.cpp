/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dict.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Launch.h"
#include "base/Pixmap.h"
#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "Annotation.h"
#include "AppSettings.h"
#include "Commands.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "PagePosition.h"
#include "Theme.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "FileHistory.h"
#include "FilterUtil.h"
#include "Translations.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Notifications.h"
#include "ServerFiles.h"
#include "TextSelection.h"
#include "SumatraPDFCommon.h"
#include "HomePage.h"
#include "HomePageCommon.h"

#include "SumatraLog.h"

static int FindHomePath(const StrVec& paths, Str path) {
#if OS_WIN
    return paths.FindI(path);
#else
    return paths.Find(path);
#endif
}

bool HomeSelection::Contains(Str path) const {
    return FindHomePath(paths, path) >= 0;
}

void HomeSelection::Toggle(Str path) {
    explicitSelection = true;
    int idx = FindHomePath(paths, path);
    if (idx >= 0) {
        paths.RemoveAt(idx);
    } else {
        paths.AppendNonEmpty(path);
    }
}

void HomeSelection::Clear() {
    paths.Reset();
    EndDrag();
    explicitSelection = true;
}

void HomeSelection::Reset() {
    Clear();
    explicitSelection = false;
}

void HomeSelection::BeginDrag(Point pt, HomeDragMode mode) {
    if (mode == HomeDragMode::Add) {
        dragBase = paths;
    }
    dragging = true;
    anchor = pt;
}

void HomeSelection::UpdateDrag(Point pt, Rect viewport) {
    explicitSelection = true;
    paths = dragBase;
    marquee = Rect::FromXY(pt, anchor).Intersect(viewport);
}

void HomeSelection::AddDragHit(Str path, Rect bounds) {
    if (!marquee.Intersect(bounds).IsEmpty() && !Contains(path)) {
        paths.AppendNonEmpty(path);
    }
}

void HomeSelection::EndDrag() {
    dragging = false;
    marquee = {};
    dragBase.Reset();
}

void HomeSelection::MenuPaths(Str target, const Vec<FileState*>& visible, StrVec& out) const {
    if (!paths.IsEmpty() && (len(target) == 0 || Contains(target))) {
        for (FileState* fs : visible) {
            if (Contains(fs->filePath)) {
                out.Append(fs->filePath);
            }
        }
        if (!out.IsEmpty()) {
            return;
        }
    }
    out.AppendNonEmpty(target);
}

bool HomeFilesAllPinned(const StrVec& paths) {
    for (Str path : paths) {
        FileState* fs = FileHistoryFindByPath(path);
        if (!fs || !fs->isPinned) {
            return false;
        }
    }
    return !paths.IsEmpty();
}

static void ToggleHomeFilesPinned(MainWindow* win, const StrVec& paths) {
    bool pin = !HomeFilesAllPinned(paths);
    for (Str path : paths) {
        FileState* fs = FileHistoryFindByPath(path);
        if (fs) {
            fs->isPinned = pin;
        }
    }
    ScheduleSaveSettings();
    win->RedrawAll(true);
}

static void ShowHomeFilesInFolder(MainWindow* win, const StrVec& paths) {
    if (paths.IsEmpty() || !CanAccessDisk() || gPluginMode) {
        return;
    }
    if (len(paths) == 1) {
        ShowFileInFolder(win, paths[0]);
        return;
    }

    StrVec dirs;
    Vec<int> dirIndexes;
    for (Str path : paths) {
        TempStr dir = path::GetDirTemp(path::NormalizeTemp(path));
        int idx = FindHomePath(dirs, dir);
        if (idx < 0) {
            idx = len(dirs);
            dirs.Append(dir);
        }
        VecAppend(dirIndexes, idx);
    }
    for (int i = 0; i < len(dirs); i++) {
        StrVec files;
        for (int j = 0; j < len(paths); j++) {
            if (dirIndexes[j] == i) {
                files.Append(paths[j]);
            }
        }
        if (FilePickerIsSumatraPDF() || OS_WASM || len(files) == 1) {
            ShowFileInFolder(win, files[0]);
            continue;
        }
        OpenFilesInDefaultManager(dirs[i], files);
    }
}

bool RunHomeFilesCommand(MainWindow* win, const StrVec& paths, int cmd) {
    if (len(paths) < 2) {
        return false;
    }
    switch (cmd) {
        case CmdShowInFolder:
            ShowHomeFilesInFolder(win, paths);
            return true;
        case CmdPinSelectedDocument:
            ToggleHomeFilesPinned(win, paths);
            return true;
        case CmdUploadToServer:
            RunServerFilesBatch(win, paths, ServerFileAction::Upload);
            return true;
        case CmdDeleteFromServer:
            RunServerFilesBatch(win, paths, ServerFileAction::Delete);
            return true;
    }
    return false;
}

// Home page state that orig and ng keep the same way: the tip of the day and
// promo lines, and how many files the page lists.

// the tip markup, one line each; the selected one is parsed by the tip band
StrVec gTipLines;

StrVec gPromoLines;

bool gTipsParsed = false;

bool gSelectedIsPromo = false;

int gSelectedTipIdx = -1;

void CollectTipsFromString(Str src, StrVec* out) {
    StrVec lines;
    Split(&lines, src, StrL("\n"));
    for (int i = 0; i < len(lines); i++) {
        Str line = lines[i];
        if (str::IsEmptyOrWhiteSpace(line)) {
            continue;
        }
        // a browser tab has no LaTeX integration
        if (OS_WASM && str::Contains(line, StrL("(Help/LaTeX-integration)"))) {
            continue;
        }
        out->Append(line);
    }
}

// the markup of the tip currently on show, {} when there is none
Str SelectedTipLine() {
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
    // translated when shown, so a language change applies without re-parsing
    return str::JoinTemp(Tr("Tip:"), StrL(" "), Tr(v[gSelectedTipIdx]));
}

void PickRandomTipOrPromo() {
    bool pickPromo = (len(gPromoLines) > 0) && (rand() % 100 < 30);
    if (pickPromo) {
        gSelectedIsPromo = true;
        gSelectedTipIdx = rand() % len(gPromoLines);
    } else if (len(gTipLines) > 0) {
        gSelectedIsPromo = false;
        gSelectedTipIdx = rand() % len(gTipLines);
    }
}

void PickAnotherRandomTip() {
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

// how many digits of the commit the About box shows
constexpr int kAboutCommitDigits = 6;

// The About box's "source" row: the commit the build is made from ("a1b2c3"),
// linking to it on GitHub. Local builds aren't given one.
void AboutSourceRow(Str* text, Str* url) {
    static Str gText;
    static Str gUrl;
    if (str::IsNull(gText)) {
        if (len(gitCommitId) > 0) {
            gText = str::Dup(Str(gitCommitId.s, std::min(len(gitCommitId), kAboutCommitDigits)));
            gUrl = str::Dup(fmt("https://github.com/sumatrapdfreader/sumatrapdf/commit/%s", gitCommitId));
        } else {
            gText = str::Dup(StrL("local build"));
            gUrl = str::Dup(StrL("https://github.com/sumatrapdfreader/sumatrapdf"));
        }
    }
    *text = gText;
    *url = gUrl;
}

// Home-list entries with a path (same set as thumbnails when search is empty).
// the history files the home page shows, in its sort order, matching searchQuery
void CollectHomePageFiles(Str searchQuery, Vec<FileState*>& fileStates, StrVec& filterWords) {
    Vec<FileState*> allFileStates;
    if (gSettings->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(allFileStates);
    } else {
        FileHistoryGetRecentlyOpenedOrder(allFileStates);
    }

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

int CountHomePageFiles() {
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

// lays the header out in a row, the brand at its start: false if the search
// box would be narrower than searchMinDx (unless `force`)
static bool PlaceHomeHeader(const HomeHeaderIn& in, int rowX, int rowDx, HomeHeaderOut& out, bool force) {
    int leftDx = out.showBrand ? in.brandDx + in.gapX : 0;
    int openDx = in.openIconDx + (out.showOpenText ? in.openTextGap + in.openTextDx : 0);
    int rightDx = in.gapX + openDx + in.gapX + in.viewIconsDx;
    int availDx = rowDx - leftDx - rightDx;
    if (availDx < in.searchMinDx && !force) {
        return false;
    }
    out.rowX = rowX;
    out.rowDx = rowDx;
    out.brandX = rowX;
    out.viewIconsX = rowX + rowDx - in.viewIconsDx;
    out.openX = out.viewIconsX - in.gapX - openDx;
    out.searchDx = std::min(std::max(availDx, in.searchMinDx / 2), in.searchMaxDx);
    // centered on the row unless a flank is in the way
    int x = rowX + ((rowDx - out.searchDx) / 2);
    x = std::min(x, out.openX - in.gapX - out.searchDx);
    out.searchX = std::max(x, rowX + leftDx);
    return true;
}

// The brand ends on the first column's right edge, the Open link and view
// icons on the last column's, the search box is centered. When the grid is too
// narrow for that (one or two columns, issue #6367), the row spans the page
// and drops the "Open..." text, then the brand, before anything overlaps.
void LayoutHomeHeader(const HomeHeaderIn& in, HomeHeaderOut& out) {
    out = {};
    int openDx = in.openIconDx + in.openTextGap + in.openTextDx;
    int leftDx = std::max(in.brandDx, in.firstColDx) + in.gapX;
    int rightDx = in.gapX + openDx + in.gapX + in.viewIconsDx;
    int flankDx = std::max(leftDx, rightDx);
    int searchDx = std::min(in.gridDx - (2 * flankDx), in.searchMaxDx);
    if (searchDx >= in.searchMinDx) {
        out.rowX = in.gridX;
        out.rowDx = in.gridDx;
        out.brandX = in.gridX + std::max(in.firstColDx - in.brandDx, 0);
        out.viewIconsX = in.gridX + in.gridDx - in.viewIconsDx;
        out.openX = out.viewIconsX - in.gapX - openDx;
        out.searchDx = searchDx;
        out.searchX = in.gridX + ((in.gridDx - searchDx) / 2);
        return;
    }
    if (PlaceHomeHeader(in, in.gridX, in.gridDx, out, false)) {
        return;
    }
    if (PlaceHomeHeader(in, in.pageX, in.pageDx, out, false)) {
        return;
    }
    out.showOpenText = false;
    if (PlaceHomeHeader(in, in.pageX, in.pageDx, out, false)) {
        return;
    }
    out.showBrand = false;
    PlaceHomeHeader(in, in.pageX, in.pageDx, out, true);
}

// "Page 24 of 47" while a document is being read, otherwise its page count;
// a chapter position ("3:12") is shown as stored
TempStr HomeThumbnailMetaTemp(FileState* fs, float* progressOut) {
    *progressOut = 0;
    bool showProgress = gSettings && gSettings->showHomePageReadingProgress;
    StoredPagePos pos = ParseStoredPagePos(fs->pageNo);
    if (showProgress && len(pos.bookmark) > 0) {
        return FormatFileStateProgressTemp(fs);
    }
    if (fs->pageCount <= 0) {
        return {};
    }
    if (showProgress && pos.pageNo > 1 && pos.pageNo <= fs->pageCount) {
        *progressOut = (float)pos.pageNo / (float)fs->pageCount;
        return fmt(Tr("Page %d of %d").s, pos.pageNo, fs->pageCount);
    }
    if (fs->pageCount == 1) {
        return str::DupTemp(Tr("1 page"));
    }
    return fmt(Tr("%d pages").s, fs->pageCount);
}

// Sizes of files whose lookup can block, found on a thread: a file on a share
// that's away waits out a network timeout (#6474). UI thread only
struct HomeFileSizeEntry {
    Str path;
    i64 size = kHomeFileSizePending;
    u64 lookedUpMs = 0;
    bool lookingUp = false;
};
static Vec<HomeFileSizeEntry> gHomeFileSizes;
static dict::StrToInt* gHomeFileSizeIdx = nullptr; // path -> index in gHomeFileSizes
// re-looked up after this, so a file that changed shows its new size
constexpr u64 kHomeFileSizeTtlMs = 60 * 1000;
// a local lookup is cheap, but ng redraws every frame and would repeat it per frame
constexpr u64 kLocalFileSizeTtlMs = 2 * 1000;

struct HomeFileSizeLookup {
    Str path;
    i64 size = -1;
};

static HomeFileSizeEntry* FindHomeFileSize(Str path) {
    int idx = -1;
    if (!gHomeFileSizeIdx || !gHomeFileSizeIdx->Get(path, &idx)) {
        return nullptr;
    }
    return &gHomeFileSizes[idx];
}

static HomeFileSizeEntry* AddHomeFileSize(Str path) {
    if (!gHomeFileSizeIdx) {
        gHomeFileSizeIdx = new dict::StrToInt();
    }
    gHomeFileSizeIdx->Insert(path, len(gHomeFileSizes));
    HomeFileSizeEntry* e = VecAppendBlanks(gHomeFileSizes, 1);
    e->path = str::Dup(path);
    e->size = kHomeFileSizePending;
    return e;
}

static void HomeFileSizeDone(HomeFileSizeLookup* res) {
    HomeFileSizeEntry* e = FindHomeFileSize(res->path);
    if (e) {
        e->size = res->size;
        e->lookedUpMs = GetTickCount64();
        e->lookingUp = false;
    }
    str::Free(res->path);
    delete res;
    MaybeRedrawHomePage();
}

static void HomeFileSizeThread(HomeFileSizeLookup* res) {
    res->size = file::GetSize(res->path);
    uitask::Post(MkFunc0<HomeFileSizeLookup>(HomeFileSizeDone, res), "HomeFileSizeDone");
}

static bool SizeLookupMayBlock(Str path) {
#if OS_WIN
    // decided from the path shape and a cached drive type, without I/O
    return path::IsOnNetworkDrive(path);
#elif OS_WASM
    (void)path;
    return false;
#else
    // telling a network mount apart takes statfs, which a dead mount blocks too
    (void)path;
    return true;
#endif
}

// size of a file the Home page lists: -1 if unknown, kHomeFileSizePending while
// being looked up
i64 GetHomeFileSize(Str path) {
    HomeFileSizeEntry* e = FindHomeFileSize(path);
    if (!e) {
        e = AddHomeFileSize(path);
    }
    u64 now = GetTickCount64();
    if (!SizeLookupMayBlock(path)) {
        if (e->size == kHomeFileSizePending || now - e->lookedUpMs > kLocalFileSizeTtlMs) {
            e->size = file::GetSize(path);
            e->lookedUpMs = now;
        }
        return e->size;
    }
    bool stale = e->size == kHomeFileSizePending || now - e->lookedUpMs > kHomeFileSizeTtlMs;
    if (stale && !e->lookingUp) {
        e->lookingUp = true;
        auto* res = new HomeFileSizeLookup{str::Dup(path), -1};
        RunAsync(MkFunc0<HomeFileSizeLookup>(HomeFileSizeThread, res), StrL("HomeFileSize"));
    }
    return e->size;
}

// one tip per line; cmd/trans-dl.ts extracts each line for translation
Str sumatraTips = StrL(R"tips(You can [customize scrollbar](CmdChangeScrollbar).
You can [customize keyboard shortcuts](Help/Customize-keyboard-shortcuts).
You can [customize toolbar](Help/Customize-toolbar).
Press (Kbd/(Key/CmdCommandPalette)) to open [command palette](CmdCommandPalette).
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
Press (Kbd/@) to quickly switch between tabs.
Press (Kbd/#) to navigate file history.
Press (Kbd/$) to navigate via favorites.
Press (Kbd/%) to navigate via table of contents.
Press (Kbd/&) to navigate via thumbnails.
Press (Kbd/(Key/CmdToggleAutomaticallyScroll)) to [scroll automatically](Help/Scrolling-and-zooming#scroll-automatically).
You can keep your place with the [reading bar](Help/Scrolling-and-zooming#keep-your-place-with-a-reading-bar).
Press (Kbd/=) to [change a setting](CmdQuickEditSettings) without opening Advanced Settings.
You can [browse files in the current folder](CmdNavigateFilesInFolder). [Learn more](Help/Scrolling-and-zooming#browse-files-in-the-folder)
You can overlay a [page grid](Help/Scrolling-and-zooming#overlay-a-page-grid) for measurement and calligraphy.
You can [zoom to fit the page content](Help/Scrolling-and-zooming#fit-the-page-content).
You can [paste an image from the clipboard](Help/Add-an-image-to-a-PDF#insert-an-image-from-the-clipboard).
You can [translate selected text](Help/Customize-search-translation-services#search-or-translate-the-selection).
)tips");

static Str sumatraPromos = StrL(R"promos(Try [Edna](https://edna.arslexis.io): a note taking web app for power users.
Try [MarkLexis](https://marklexis.arslexis.io): a bookmarking web application.
Try [Sumatra File Manager](https://sumatrafilemanager.com): file manager with super powers.
Try [Sumatra File Manager Web](https://web.sumatrafilemanager.com): file manager for S3, Dropbox and more.
)promos");

// promotions from the update-check file, shown with the built-in ones
static Str promoFromServer;

// built-in promos, then the server's except those linking to a built-in's url
static void CollectPromoLines() {
    gPromoLines.Reset();
    CollectTipsFromString(sumatraPromos, &gPromoLines);
    int nBuiltIn = len(gPromoLines);
    StrVec server;
    CollectTipsFromString(promoFromServer, &server);
    for (Str line : server) {
        Str url = str::SliceFromChar(line, '(');
        int end = str::IndexOfChar(url, ')');
        url = end < 0 ? Str{} : Str(url.s, end + 1);
        bool dup = false;
        for (int i = 0; i < nBuiltIn && len(url) > 0; i++) {
            dup = dup || str::Contains(gPromoLines[i], url);
        }
        if (!dup) {
            gPromoLines.Append(line);
        }
    }
}

void EnsureTipsParsed() {
    if (gTipsParsed) {
        return;
    }
    CollectTipsFromString(sumatraTips, &gTipLines);
    CollectPromoLines();
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

void SetPromoString(Str s) {
    if (len(s) == 0) {
        return;
    }
    str::ReplaceWithCopy(&promoFromServer, s);
    // the update check usually ends after the home page parsed its tips
    if (gTipsParsed) {
        CollectPromoLines();
    }
}

static Str BitnessStr() {
    return sizeof(void*) == 8 ? StrL("64-bit") : StrL("32-bit");
}

// "v3.7 64-bit (dbg)", for bug reports
TempStr GetAppVersionTemp() {
    TempStr s = fmt("v%s %s", currentVersion, BitnessStr());
    if (gIsDebugBuild) {
        s = str::JoinTemp(s, StrL(" (dbg)"));
    }
    return s;
}

// "3.7 64-bit, built Oct 14 2026 01:46"
TempStr AboutVersionLineTemp() {
    Str built = StrL(__DATE__ " " __TIME__);
    // without the seconds
    built.len = std::max(0, len(built) - 3);
    return fmt("%s %s, built %s", currentVersion, BitnessStr(), built);
}

// the line under the rows that says this isn't a release build
Str AboutNoteText() {
#if defined(PRE_RELEASE_VER)
    return StrL("pre-release build");
#elif IS_DEBUG
    return StrL("debug build, for testing only");
#else
    return {};
#endif
}
