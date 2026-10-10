/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/WinDynCalls.h"
#include "base/DirScan.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/FileWatcher.h"
#include "base/GuessFileType.h"
#include "base/SquareTreeParser.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Http.h"
#include "base/Archive.h"
#include "base/Timer.h"
#include "base/CmdLineArgs.h"
#include "gui/UIModels.h"
#include "gui/BrowserView.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "PdfDarkMode.h"
#include "Annotation.h"
#include "FormFields.h"
#include "PdfTools.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "MarkdownToc.h"
#include "EmbeddedResources.h"
#include "PalmDbReader.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "PdfSync.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "AnnotPlacement.h"
#include "WindowTab.h"
#include "UpdateCheck.h"
#include "Commands.h"
#include "Flags.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "RefHover.h"
#include "ExternalViewers.h"
#include "Favorites.h"
#include "FileThumbnails.h"
#include "Menu.h"
#include "ImageReader.h"
#include "PngOptimizer.h"
#include "Print.h"
#include "SearchAndDDE.h"
#include "Selection.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "KeyboardHelp.h"
#include "SelectionToolbar.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "Screenshot.h"
#include "GlobalHotkeys.h"
#include "ImageSaveCropResize.h"
#include "StressTesting.h"
#include "HomePage.h"
#include "DocumentProperties.h"
#include "TabGroupsManage.h"
#include "TableOfContents.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "FindBar.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "SelectionTranslate.h"
#include "SelectionHandlers.h"
#include "GoogleLens.h"
#include "CommandPalette.h"
#include "SumatraDialogs.h"
#include "NavFilesInFolder.h"
#include "Installer.h"
#include "Theme.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"
#include "ExplorerQuickLook.h"
#include "PagePosition.h"
#include "Accelerators.h"
#include "ChmDump.h"
#include "ExifDump.h"
#include "HangDetector.h"
#include "PrintWin11.h"
#include "SumatraControl.h"
#include "Version.h"
#include "SumatraPDF.h"
#include "PerfLog.h"
#include "SumatraPDFCommon.h"

#include "SumatraLog.h"

constexpr const char* kRestrictionsFileName = "sumatrapdfrestrict.ini";

// in restricted mode, some features can be disabled (such as
// opening files, printing, following URLs), so that SumatraPDF
// can be used as a PDF reader on locked down systems
static Perm gPolicyRestrictions = Perm::All;

// only the listed protocols will be passed to the OS for
// opening in e.g. a browser or an email client (ignored,
// if gPolicyRestrictions doesn't contain Perm::DiskAccess)
StrVec gAllowedLinkProtocols;

// only files of the listed perceived types will be opened
// externally by LinkHandler::LaunchFile (i.e. when clicking
// on an in-document link); examples: "audio", "video", ...
StrVec gAllowedFileTypes;

// A document is usually loaded on a worker thread, where walking the file
// history (only ever touched on the UI thread) would race with it changing.
// Such loads copy the settings before leaving the UI thread and park the copy
// here for the engine to pick up.
static thread_local FileEBookUI* gLoadThreadFileEBookUI;

void SetLoadThreadFileEBookUI(FileEBookUI* v) {
    gLoadThreadFileEBookUI = v;
}

// per-document overrides of the ebook settings, null unless this document has
// an EBookUI block in FileStates (#4600)
FileEBookUI* GetFileEBookUI(Str filePath) {
    if (!uitask::IsMainUIThread()) {
        return gLoadThreadFileEBookUI;
    }
    if (!gSettings || len(filePath) == 0) {
        return nullptr;
    }
    FileState* fs = FileHistoryFindByPath(filePath);
    return fs ? fs->eBookUI : nullptr;
}

void SetCurrentLang(Str langCode) {
    if (len(langCode) == 0) {
        return;
    }
    str::ReplaceWithCopy(&gSettings->uiLanguage, langCode);
    trans::SetCurrentLangByCode(gSettings->uiLanguage);
}

void InitializePolicies(bool restrict) {
    // default configuration should be to restrict everything
    ReportIf(gPolicyRestrictions != Perm::All);
    ReportIf(len(gAllowedLinkProtocols) != 0 || len(gAllowedFileTypes) != 0);

    // the -restrict command line flag overrides any sumatrapdfrestrict.ini configuration
    if (restrict) {
        gPolicyRestrictions = Perm::RestrictedUse;
        return;
    }

    // allow to restrict SumatraPDF's functionality from an INI file in the
    // same directory as SumatraPDF.exe (see ../docs/sumatrapdfrestrict.ini)
    // (if the file isn't there, everything is allowed)
    TempStr restrictPath = GetPathInExeDirTemp(Str(kRestrictionsFileName));
    if (!file::Exists(restrictPath)) {
        Split(&gAllowedLinkProtocols, StrL(kDefaultLinkProtocols), StrL(","));
        Split(&gAllowedFileTypes, StrL(kDefaultFilePerceivedTypes), StrL(","));
        return;
    }

    Str restrictData = file::ReadFile(restrictPath);
    SquareTreeNode* root = ParseSquareTree(restrictData);
    AutoDelete delRoot(root);
    SquareTreeNode* polsec = root ? root->GetChild(StrL("Policies")) : nullptr;
    gPolicyRestrictions = Perm::RestrictedUse;
    // if the restriction file is broken, err on the side of full restriction
    if (!polsec) {
        return;
    }

    static Perm perms[] = {Perm::InternetAccess, Perm::DiskAccess,    Perm::SavePreferences, Perm::RegistryAccess,
                           Perm::PrinterAccess,  Perm::CopySelection, Perm::FullscreenAccess};
    static SeqStrings permNames =
        "InternetAccess\0DiskAccess\0SavePreferences\0RegistryAccess\0PrinterAccess\0CopySelection\0FullscreenAccess\0";

    // enable policies as indicated in sumatrapdfrestrict.ini
    for (int i = 0; i < dimofi(perms); i++) {
        Str name = SeqStrByIndex(permNames, i);
        Str val = polsec->GetValue(name);
        if (val && ParseInt(val) != 0) {
            gPolicyRestrictions = gPolicyRestrictions | perms[i];
        }
    }

    // determine the list of allowed link protocols and perceived file types
    if ((gPolicyRestrictions & Perm::DiskAccess) != (Perm)0) {
        Str value = polsec->GetValue(StrL("LinkProtocols"));
        if (value) {
            TempStr protocols = str::DupTemp(value);
            str::ToLowerInPlace(protocols);
            str::TransCharsInPlace(protocols, StrL(" :;"), StrL(",,,"));
            Split(&gAllowedLinkProtocols, protocols, StrL(","), true);
        }
        value = polsec->GetValue(StrL("SafeFileTypes"));
        if (value) {
            TempStr protocols = str::DupTemp(value);
            str::ToLowerInPlace(protocols);
            str::TransCharsInPlace(protocols, StrL(" :;"), StrL(",,,"));
            Split(&gAllowedFileTypes, protocols, StrL(","), true);
        }
    }
}

void RestrictPolicies(Perm revokePermission) {
    gPolicyRestrictions = (gPolicyRestrictions | Perm::RestrictedUse) & ~revokePermission;
}

bool HasPermission(Perm permission) {
    return (permission & gPolicyRestrictions) == permission;
}

bool CanAccessDisk() {
    return HasPermission(Perm::DiskAccess);
}

// TODO: could add a setting
bool AnnotationsAreDisabled() {
    if (!CanAccessDisk()) {
        // annotations must be saved back to a file so lack of disk access
        // implies no ability to edit annotations
        return true;
    }
    return false;
}

// Find the first window that has been produced from <file>
MainWindow* FindMainWindowBySyncFile(Str path, bool focusTab) {
    for (MainWindow* win : gWindows) {
        Vec<Rect> rects;
        int page;
        auto* dm = win->AsFixed();
        if (dm && dm->pdfSync && dm->pdfSync->SourceToDoc(path, 0, 0, &page, rects) != PDFSYNCERR_UNKNOWN_SOURCEFILE) {
            return win;
        }
        bool bringFore = focusTab && win->TabCount() > 1;
        if (!bringFore) {
            continue;
        }
        // bring a background tab to the foreground
        for (WindowTab* tab : win->Tabs()) {
            if (tab != win->CurrentTab() && tab->AsFixed() && tab->AsFixed()->pdfSync &&
                tab->AsFixed()->pdfSync->SourceToDoc(path, 0, 0, &page, rects) != PDFSYNCERR_UNKNOWN_SOURCEFILE) {
                TabsSelect(win, win->GetTabIdx(tab));
                return win;
            }
        }
    }
    return nullptr;
}

bool gForceRtl = false;

bool IsUIRtl() {
    if (gForceRtl) {
        return true;
    }
    return trans::IsCurrLangRtl();
}

bool ShouldSaveThumbnail(FileState* ds) {
    // don't create thumbnails if we won't be needing them at all
    if (!HasPermission(Perm::SavePreferences)) {
        return false;
    }

    // don't materialize (hydrate) a cloud-only placeholder file just to make a
    // thumbnail. opening it would force a slow, possibly multi-minute download
    // (e.g. OneDrive "Files On-Demand" dehydrated file). issue #5756
    if (path::IsCloudPlaceholder(ds->filePath)) {
        logf("ShouldSaveThumbnail: skipping cloud placeholder '%s'\n", ds->filePath);
        return false;
    }

    // don't create thumbnails for files that won't need them anytime soon
    Vec<FileState*> list;
    if (gSettings->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(list);
    } else {
        FileHistoryGetRecentlyOpenedOrder(list);
    }
    int idx = VecFind(list, ds);
    if (idx < 0) {
        return false;
    }

    if (HasThumbnail(ds)) {
        return false;
    }
    return true;
}

bool ScrollbarsAreHidden() {
    return ScrollbarModeFromPrefs() == kScrollbarHidden;
}

bool ScrollbarsUseOverlay() {
    int mode = ScrollbarModeFromPrefs();
    return mode == kScrollbarSmart || mode == kScrollbarOverlay;
}

SeqStrings gToolbarModeNames = "show\0hide\0overlay\0";

int ToolbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarModeNames, gSettings->toolbar);
    if (idx < 0) {
        // not set / invalid: derive from the legacy showToolbar bool
        idx = gSettings->showToolbar ? kToolbarShow : kToolbarHide;
    }
    return idx;
}

bool ToolbarModeIsOverlay() {
    return ToolbarModeFromPrefs() == kToolbarOverlay;
}

bool ToolbarModeIsHidden() {
    return ToolbarModeFromPrefs() == kToolbarHide;
}

void SetToolbarMode(int mode) {
    Str name = SeqStrByIndex(gToolbarModeNames, mode);
    if (len(name) == 0) {
        name = StrL("show");
        mode = kToolbarShow;
    }
    str::ReplaceWithCopy(&gSettings->toolbar, name);
    // keep the legacy bool in sync so old versions stay sane
    gSettings->showToolbar = (mode != kToolbarHide);
}

int FullscreenToolbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarModeNames, gSettings->fullscreen.toolbar);
    if (idx < 0) {
        // not set / invalid: derive from the legacy Fullscreen.ShowToolbar bool
        idx = gSettings->fullscreen.showToolbar ? kToolbarShow : kToolbarHide;
    }
    return idx;
}

void SetFullscreenToolbarMode(int mode) {
    Str name = SeqStrByIndex(gToolbarModeNames, mode);
    if (len(name) == 0) {
        name = StrL("hide");
        mode = kToolbarHide;
    }
    str::ReplaceWithCopy(&gSettings->fullscreen.toolbar, name);
    gSettings->fullscreen.showToolbar = (mode != kToolbarHide);
}

SeqStrings gToolbarPositionNames = "top\0bottom\0";

int ToolbarPositionFromPrefs() {
    int idx = SeqStrIndexIS(gToolbarPositionNames, gSettings->toolbarPosition);
    if (idx < 0) {
        idx = kToolbarTop;
    }
    return idx;
}

bool ToolbarAtBottom() {
    return ToolbarPositionFromPrefs() == kToolbarBottom;
}

TempStr BuildZoomString(float zoomLevel) {
    TempStr zoomLevelStr = ZoomLevelStr(zoomLevel);
    Str zoomStr = Tr("Zoom");
    return fmt("%s: %s", zoomStr, zoomLevelStr);
}

// Pages shown in the page-info tip: the current page, plus its facing partner
// when that page is also visible (facing / book view with two images).
static int CollectPageInfoPages(DocController* ctrl, int pageNo, int* pagesOut, int maxPages) {
    int n = 0;
    auto add = [&](int p) {
        if (n >= maxPages || !ctrl->ValidPageNo(p)) {
            return;
        }
        for (int i = 0; i < n; i++) {
            if (pagesOut[i] == p) {
                return;
            }
        }
        pagesOut[n++] = p;
    };
    add(pageNo);
    DisplayModel* dm = ctrl->AsFixed();
    if (dm) {
        DisplayMode mode = dm->GetDisplayMode();
        if (IsFacing(mode) || IsBookView(mode)) {
            if (dm->PageVisible(pageNo + 1)) {
                add(pageNo + 1);
            } else if (dm->PageVisible(pageNo - 1)) {
                add(pageNo - 1);
            }
        }
        // stable order for multi-page rows
        if (n == 2 && pagesOut[0] > pagesOut[1]) {
            int t = pagesOut[0];
            pagesOut[0] = pagesOut[1];
            pagesOut[1] = t;
        }
    }
    return n;
}

void UpdatePageInfoHelper(DocController* ctrl, NotificationWnd* wnd, int pageNo) {
    if (!ctrl->ValidPageNo(pageNo)) {
        pageNo = ctrl->CurrentPageNo();
    }
    int nPages = ctrl->PageCount();
    TempStr pageInfo;
    if (ShowChapterUi(ctrl)) {
        Location loc = ctrl->LocationFromPageNo(pageNo);
        int chapterPages = ctrl->ChapterPageCount(loc.chapter);
        pageInfo = fmt("%s %d / %d, %s %d / %d", Tr("Chapter:"), loc.chapter, ctrl->ChapterCount(), Tr("Page:"),
                       loc.page, chapterPages);
    } else if (ctrl->HasPageLabels()) {
        TempStr label = ctrl->GetPageLabeTemp(pageNo);
        pageInfo = fmt("%s %s (%d / %d)", Tr("Page:"), label, pageNo, nPages);
    } else {
        pageInfo = fmt("%s %d / %d", Tr("Page:"), pageNo, nPages);
    }
    float zoomLevel = ctrl->GetZoomVirtual();
    auto zoomStr = BuildZoomString(zoomLevel);
    pageInfo = str::JoinTemp(pageInfo, StrL(kPageInfoSep), zoomStr);

    // Image extras (issue #4456). Document file name is already on the tab.
    DisplayModel* dm = ctrl->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine && IsEngineImages(engine)) {
        if (engine->kind == kindEngineImage) {
            // Single image file (or multi-frame TIFF/GIF): resolution · size · non-default DPI
            RectF box = engine->PageMediabox(pageNo);
            int w = (int)lroundf(box.dx);
            int h = (int)lroundf(box.dy);
            i64 imgSize = -1;
            EngineImagesGetPageFileInfo(engine, pageNo, nullptr, &imgSize);
            TempStr detail{};
            if (w > 0 && h > 0) {
                detail = fmt("%d x %d", w, h);
            }
            if (imgSize >= 0) {
                TempStr sizeStr = str::FormatSizeShortTemp(imgSize);
                detail = detail ? fmt("%s%s%s", detail, StrL(kPageInfoSep), sizeStr) : sizeStr;
            }
            // fileDPI defaults to 96; only show when the image reports something else
            float dpi = engine->fileDPI;
            if (dpi > 0.5f && fabsf(dpi - 96.0f) > 0.5f) {
                TempStr dpiStr = fmt("%.0f DPI", dpi);
                detail = detail ? fmt("%s%s%s", detail, StrL(kPageInfoSep), dpiStr) : dpiStr;
            }
            if (detail) {
                pageInfo = str::JoinTemp(pageInfo, StrL(kPageInfoSep), detail);
            }
        } else {
            // Comic / image folder: per-page name · dimensions · bytes (both if facing)
            int pages[2] = {};
            int nShow = CollectPageInfoPages(ctrl, pageNo, pages, 2);
            for (int i = 0; i < nShow; i++) {
                int p = pages[i];
                TempStr imgName{};
                i64 imgSize = -1;
                if (!EngineImagesGetPageFileInfo(engine, p, &imgName, &imgSize)) {
                    continue;
                }
                RectF box = engine->PageMediabox(p);
                int w = (int)lroundf(box.dx);
                int h = (int)lroundf(box.dy);
                TempStr detail{};
                auto appendPart = [&](TempStr part) {
                    if (len(part) == 0) {
                        return;
                    }
                    detail = detail ? fmt("%s%s%s", detail, StrL(kPageInfoSep), part) : part;
                };
                appendPart(imgName);
                if (w > 0 && h > 0) {
                    appendPart(fmt("%d x %d", w, h));
                }
                if (imgSize >= 0) {
                    appendPart(str::FormatSizeShortTemp(imgSize));
                }
                if (detail) {
                    pageInfo = str::JoinTemp(pageInfo, StrL(kPageInfoSep), detail);
                }
            }
        }
    }

    NotificationUpdateMessage(wnd, pageInfo);
}

// Markdown and HTML both render in the WebView2 browser view via MarkdownModel,
// each gated by its own UseFixedPageUI opt-out (fall back to MuPDF/ebook engines).
bool ShouldUseBrowserView(FileType kind) {
    if (MarkdownModel::IsHtmlFileType(kind)) {
        return !gSettings->htmlUI.useFixedPageUI;
    }
    if (MarkdownModel::IsSupportedFileType(kind)) {
        return !gSettings->markdownUI.useFixedPageUI;
    }
    return false;
}

bool showTocByDefault(Str path, EngineBase* engine) {
    if (gSettings->alwaysShowSidebar) {
        return true;
    }
    if (!gSettings->showToc) {
        return false;
    }
    // comic book bookmarks are usually just the list of files: only show
    // ones from ComicInfo.xml (#6244)
    FileType kind = GuessFileTypeFromName(path);
    if (!IsEngineCbxSupportedFileType(kind)) {
        return true;
    }
    return EngineCbxHasComicInfoToc(engine);
}

bool IsEbookFileType(FileType ft) {
    return ft == FileType::Epub || ft == FileType::Mobi || ft == FileType::Fb2 || ft == FileType::Fb2z ||
           ft == FileType::PalmDoc || ft == FileType::HTML || ft == FileType::Txt || ft == FileType::Lit;
}

// Per-type DefaultDisplayMode (empty = inherit the global DefaultDisplayMode).
// Used only on first open when there is no remembered FileState (issue #2588).
DisplayMode DisplayModeForNewDocument(Str path, EngineBase* engine) {
    DisplayMode dm = gSettings->defaultDisplayModeEnum;
    Str modeStr;
    Kind k = engine ? engine->kind : nullptr;
    if (k == kindEngineComicBooks || k == kindEngineImageDir ||
        (path && IsEngineCbxSupportedFileType(GuessFileTypeFromName(path, true)))) {
        modeStr = gSettings->comicBookUI.defaultDisplayMode;
    } else if (k == kindEngineEpub || k == kindEngineFb2 || k == kindEngineMobi || k == kindEnginePdb ||
               k == kindEngineHtml || (path && IsEbookFileType(GuessFileTypeFromName(path, true)))) {
        modeStr = gSettings->eBookUI.defaultDisplayMode;
    }
    if (modeStr) {
        return DisplayModeFromString(modeStr, dm);
    }
    return dm;
}

// First open only: ComicBookUI.DefaultZoom when set (issue #5946). Empty
// keeps fit page, the historical comic default — not the global DefaultZoom.
// Remembered FileState wins.
float ZoomForNewDocument(Str path, EngineBase* engine, float fallback) {
    Kind k = engine ? engine->kind : nullptr;
    if (k == kindEngineComicBooks || (path && IsEngineCbxSupportedFileType(GuessFileTypeFromName(path, true)))) {
        float z = gSettings->comicBookUI.defaultZoomFloat;
        if (z != 0) {
            return z;
        }
        return kZoomFitPage;
    }
    return fallback;
}

void AutoReloadResetFileState(WindowTab* tab) {
    tab->autoReloadSize = -1;
    tab->autoReloadModTime = {};
    tab->autoReloadStartMs = 0;
}

TempStr FormatCursorPositionTemp(EngineBase* engine, PointF pt, MeasurementUnit unit) {
    pt.x = std::max(pt.x, 0.0f);
    pt.y = std::max(pt.y, 0.0f);
    pt.x /= engine->fileDPI;
    pt.y /= engine->fileDPI;

    // for MeasurementUnit::in
    float factor = 1;
    Str unitName = StrL("in");
    if (unit == MeasurementUnit::pt) {
        factor = 72;
        unitName = StrL("pt");
    } else if (unit == MeasurementUnit::mm) {
        factor = 25.4f;
        unitName = StrL("mm");
    }

    TempStr xPos = str::FormatFloatWithThousandSepTemp((double)pt.x * (double)factor);
    TempStr yPos = str::FormatFloatWithThousandSepTemp((double)pt.y * (double)factor);
    if (unit != MeasurementUnit::in) {
        // use similar precision for all units
        if (xPos.len >= 2 && str::IsDigit(xPos.s[xPos.len - 2])) {
            xPos.len--;
        }
        if (yPos.len >= 2 && str::IsDigit(yPos.s[yPos.len - 2])) {
            yPos.len--;
        }
    }
    return fmt("%s x %s %s", xPos, yPos, unitName);
}

SavedAnnotSel CaptureSelectedAnnotation(WindowTab* tab) {
    SavedAnnotSel key;
    Annotation* a = tab ? tab->selectedAnnotation : nullptr;
    if (!a) {
        return key;
    }
    key.valid = true;
    key.pageNo = a->pageNo;
    key.type = a->type;
    key.bounds = a->bounds;
    return key;
}

Annotation* FindMatchingAnnotation(WindowTab* tab, const SavedAnnotSel& key) {
    if (!key.valid || !tab) {
        return nullptr;
    }
    EngineBase* engine = tab->GetEngine();
    if (!engine) {
        return nullptr;
    }
    Vec<Annotation*> annots;
    EngineMupdfGetAnnotations(engine, annots);
    for (Annotation* a : annots) {
        if (a->pageNo == key.pageNo && a->type == key.type && a->bounds == key.bounds) {
            return a;
        }
    }
    return nullptr;
}

// Returns the current engine only after proving that tab still belongs to a live window.
// Modal UI can dispatch messages that close the tab or replace its engine.
EngineBase* GetLiveTabEngine(WindowTab* tab, MainWindow** winOut) {
    if (winOut) {
        *winOut = nullptr;
    }
    MainWindow* win = FindMainWindowByTab(tab);
    if (!win) {
        return nullptr;
    }
    DisplayModel* dm = tab->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine && winOut) {
        *winOut = win;
    }
    return engine;
}

// After a message pump, a nested DDE CloseAllTabs / CloseWindow may have
// already removed this tab. GetTabIdx does not dereference `tab`, so a freed
// pointer just comes back as -1. Do not delete it again.
bool TabStillInWindow(MainWindow* win, WindowTab* tab) {
    return IsMainWindowValid(win) && !win->isBeingClosed && win->GetTabIdx(tab) >= 0;
}

// returns false if no filter has been appended
bool AppendFileFilterForDoc(DocController* ctrl, str::Builder& fileFilter) {
    Kind type = nullptr;
    if (ctrl->AsFixed()) {
        type = ctrl->AsFixed()->engineType;
    } else if (ctrl->AsChm()) {
        type = kindEngineChm;
    }
    // markdown has no engine kind; it falls through to the default filter below.
    // Prefer GetDefaultFileExt() where it distinguishes formats (xps, epub, …);
    // fall back to engine kind for the rest.
    auto ext = ctrl->GetDefaultFileExt();
    if (str::EqI(ext, StrL(".xps"))) {
        fileFilter.Append(Tr("XPS documents"));
    } else if (str::EqI(ext, StrL(".docx"))) {
        fileFilter.Append(Tr("Word documents"));
    } else if (str::EqI(ext, StrL(".xlsx"))) {
        fileFilter.Append(Tr("Excel workbooks"));
    } else if (str::EqI(ext, StrL(".pptx"))) {
        fileFilter.Append(Tr("PowerPoint presentations"));
    } else if (str::EqI(ext, StrL(".epub"))) { // NOLINT(bugprone-branch-clone): see kindEngineEpub below
        // .epub can be handled by kindEngineMupdf
        fileFilter.Append(Tr("EPUB ebooks"));
    } else if (type == kindEngineDjVu) {
        fileFilter.Append(Tr("DjVu documents"));
    } else if (type == kindEngineComicBooks) {
        fileFilter.Append(Tr("Comic books"));
    } else if (type == kindEngineImage) {
        Str imgDefExt = ctrl->GetDefaultFileExt();
        if (len(imgDefExt) > 0 && imgDefExt.s[0] == '.') {
            imgDefExt = Str(imgDefExt.s + 1, imgDefExt.len - 1);
        }
        fileFilter.Append(fmt(Tr("Image files (*.%s)").s, imgDefExt));
    } else if (type == kindEngineImageDir) {
        return false; // only show "All files"
    } else if (type == kindEnginePostScript || type == kindEngineDvi) {
        // also offer the PDF the converter produced (SaveFileAs writes it)
        if (type == kindEngineDvi) {
            fileFilter.Append(Tr("DVI documents"));
        } else {
            fileFilter.Append(Tr("PostScript documents"));
        }
        fileFilter.Append(fmt("\1*%s\1", ctrl->GetDefaultFileExt()));
        fileFilter.Append(Tr("PDF documents"));
        fileFilter.Append(StrL("\1*.pdf\1"));
        return false;
    } else if (type == kindEngineChm) {
        fileFilter.Append(Tr("CHM documents"));
    } else if (type == kindEngineEpub) {
        fileFilter.Append(Tr("EPUB ebooks"));
    } else if (type == kindEngineMobi) {
        fileFilter.Append(Tr("Mobi documents"));
    } else if (type == kindEngineFb2) {
        fileFilter.Append(Tr("FictionBook documents"));
    } else if (type == kindEnginePdb) {
        fileFilter.Append(Tr("PalmDoc documents"));
    } else {
        fileFilter.Append(Tr("PDF documents"));
    }
    return true;
}

// FilePicker: empty/os = Windows dialog; sumatrapdf = Navigate Files in Folder.
bool FilePickerIsSumatraPDF() {
    return gSettings && str::EqI(gSettings->filePicker, StrL("sumatrapdf"));
}

// move to the recycle bin and forget it in the file history / thumbnail cache
void DeleteFileFromDiskAndHistory(Str path) {
    file::DeleteFileToTrash(path);
    DeleteThumbnailForFile(path);
    FileState* fs = FileHistoryFindByPath(path);
    if (fs) {
        FileHistoryRemove(fs);
        DeleteFileState(fs);
    }
    ScheduleSaveSettings();
}

bool IsOpenableNextPrevFile(Str path) {
    FileType kind = GuessFileTypeFromName(path, true);
    return IsSupportedFileType(kind, true);
}

// File history is UI-thread only, so snapshot paths in this dir before the
// worker runs and merge them there (unsupported types the user has opened).
void CollectHistoryFilesInDir(Str dir, StrVec& out) {
    Vec<FileState*>* states = FileHistoryStates();
    if (!states) {
        return;
    }
    for (FileState* fs : *states) {
        if (!fs || len(fs->filePath) == 0) {
            continue;
        }
        TempStr d = path::GetDirTemp(fs->filePath);
        if (!str::EqI(d, dir)) {
            continue;
        }
        // DirIter already keeps openable names; only extra is unsupported
        // types the user has opened (so we don't Contains() each history
        // path against tens of thousands of listed files).
        if (IsOpenableNextPrevFile(fs->filePath)) {
            continue;
        }
        out.Append(fs->filePath);
    }
}

// Keep the cached list naturally sorted without a full SortNatural on the UI thread.
void InsertSortedNatural(StrVec* v, Str s) {
    if (v->Contains(s)) {
        return;
    }
    int lo = 0;
    int hi = len(*v);
    while (lo < hi) {
        int mid = lo + ((hi - lo) / 2);
        if (StrLessNatural(v->At(mid), s)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    v->InsertAt(lo, s);
}

PendingNextPrevNav* gPendingNextPrevNav = nullptr;

void ClearPendingNextPrevNav() {
    delete gPendingNextPrevNav;
    gPendingNextPrevNav = nullptr;
}

bool IsAtDocumentBottom(MainWindow* win) {
    DocController* ctrl = win->ctrl;
    if (!ctrl) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        return dm->IsAtDocumentEnd();
    }
    // CHM / Markdown render in a browser control that scrolls itself, so we
    // can't tell how far down it is. Comparing page numbers instead said "at
    // the end" for a document the user had barely started reading, which put
    // the open-next-file tip on screen on the first scroll down.
    return false;
}

void OnNextFileHintClosed(NotificationClosedEvent* ev) {
    RemoveNotification(ev->wnd);
    if (ev->reason != NotifCloseReason::User) {
        return;
    }
    gSettings->showFileNavigateHint = false;
    ScheduleSaveSettings();
}

void ToggleMangaMode(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    dm->SetDisplayR2L(!dm->GetDisplayR2L());
    ScrollState state = dm->GetScrollState();
    dm->Relayout(dm->GetZoomVirtual(), dm->GetRotation());
    dm->SetScrollState(state);
}

void OnMenuZoom(MainWindow* win, int menuId) {
    if (!win->IsDocLoaded()) {
        return;
    }

    float zoom = ZoomMenuItemToZoom(menuId);
    SmartZoom(win, zoom, nullptr, true);
}

void TogglePresentationMode(MainWindow* win) {
    // only DisplayModel currently supports an actual presentation mode
    ToggleFullScreen(win, win->AsFixed() != nullptr);
}

// make sure that idx falls within <0, max-1> inclusive range
// negative numbers wrap from the end
int wrapIdx(int idx, int max) {
    for (; idx < 0; idx += max) {
        idx += max;
    }
    return idx % max;
}

// first entry is value in gLangCodes, second is ISO 639 lang code
// I made it manually by looking at trans_lang.go and
// https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes
// but not fully and it might be incorrect anyway wrt. to other translation websites
static const char* gLangsMap = "am\0hy\0by\0be\0ca-xv\0ca\0cz\0cs\0kr\0ko\0vn\0vi\0cn\0zh-CN\0tw\0zh-TW\0";

TempStr GetISO639LangCodeFromLangTemp(Str lang) {
    int idx = SeqStrIndex(gLangsMap, lang);
    if (idx < 0 || idx % 2 != 0) {
        return lang;
    }
    return SeqStrByIndex(gLangsMap, idx + 1);
}

static void AppendLayoutFloats(str::Builder& b, Vec<float>* vals) {
    if (!vals) {
        return;
    }
    for (float v : *vals) {
        b.Append(fmt("%g,", v));
    }
}

// font, page size, spacing and CSS: what a reload has to re-paginate
Str EbookLayoutSnapshot() {
    str::Builder b;
    if (!gSettings) {
        return b.TakeStr();
    }
    b.Append(fmt("dpi=%d\n", gSettings->customScreenDPI));
    EBookUI* g = &gSettings->eBookUI;
    b.Append(fmt("g|%s|%g|%g|%g|%d|%g|", g->fontName, g->fontSize, g->layoutDx, g->layoutDy,
                 g->ignoreDocumentCSS ? 1 : 0, g->lineSpacing));
    AppendLayoutFloats(b, g->margin);
    b.Append(fmt("|%s\n", g->customCSS));
    if (gSettings->fileStates) {
        for (FileState* fs : *gSettings->fileStates) {
            FileEBookUI* f = fs->eBookUI;
            if (!f) {
                continue;
            }
            b.Append(fmt("f|%s|%s|%g|%g|%g|%s|%g|", fs->filePath, f->fontName, f->fontSize, f->layoutDx, f->layoutDy,
                         f->ignoreDocumentCSS, f->lineSpacing));
            AppendLayoutFloats(b, f->margin);
            b.Append(fmt("|%s\n", f->customCSS));
        }
    }
    return b.TakeStr();
}

// reflowable docs, and anything with chapters (MOBI): their page count follows
// the ebook font / page size / CSS
static bool LayoutFollowsEbookSettings(EngineBase* engine) {
    if (!engine) {
        return false;
    }
    if (engine->isReflowable || engine->HasChapters()) {
        return true;
    }
    Kind k = engine->kind;
    return k == kindEngineMobi || k == kindEngineFb2 || k == kindEnginePdb || k == kindEngineHtml ||
           k == kindEngineEpub;
}

void ReloadEbookLayoutDocs() {
    for (MainWindow* w : gWindows) {
        Vec<WindowTab*> tabs;
        for (WindowTab* tab : w->Tabs()) {
            VecAppend(tabs, tab);
        }
        for (WindowTab* tab : tabs) {
            DisplayModel* dm = tab->AsFixed();
            EngineBase* engine = dm ? dm->GetEngine() : nullptr;
            if (!LayoutFollowsEbookSettings(engine)) {
                continue;
            }
            if (tab == w->CurrentTab()) {
                ReloadDocument(w, false);
            } else {
                tab->reloadOnFocus = true;
            }
        }
    }
}

// a -zoom value the cmd-line parser understands: a fit mode name or a percentage
TempStr ZoomArgTemp(DocController* ctrl) {
    float zoom = ctrl->GetZoomVirtual();
    if (kZoomFitPage == zoom) {
        return StrL("fit page");
    }
    if (kZoomFitWidth == zoom) {
        return StrL("fit width");
    }
    if (kZoomFitHeight == zoom) {
        return StrL("fit height");
    }
    if (kZoomFitContent == zoom) {
        return StrL("fit content");
    }
    if (kZoomFitVisible == zoom) {
        return StrL("fit visible");
    }
    return fmt("%g%%", ctrl->GetZoomVirtual(true));
}

static const char* kHelpThemeValues[] = {"app", "light", "dark"};

// HelpTheme setting, "app" unless it holds one of the known values
Str HelpThemePref() {
    for (const char* v : kHelpThemeValues) {
        if (str::EqI(gSettings->helpTheme, Str(v))) {
            return Str(v);
        }
    }
    return StrL("app");
}

// theme.js reports a click on the manual's switch via
// window.__sumatra__.notify("manualTheme", "<system|light|dark>")
void ManualOnJsNotify(void*, Str method, Str paramsJson) {
    if (!str::Eq(method, StrL("manualTheme"))) {
        return;
    }
    // params is a JSON array with one string, e.g. ["dark"]; "system" is what
    // theme.js calls the follow-the-app option
    Str v{};
    if (str::Contains(paramsJson, StrL("\"system\""))) {
        v = StrL("app");
    }
    for (const char* known : kHelpThemeValues) {
        if (str::Contains(paramsJson, fmt("\"%s\"", Str(known)))) {
            v = Str(known);
        }
    }
    if (len(v) == 0 || str::Eq(gSettings->helpTheme, v)) {
        return;
    }
    str::ReplaceWithCopy(&gSettings->helpTheme, v);
    ScheduleSaveSettings();
}

TempStr DocURIToLocalManualUrlTemp(Str docURI) {
    if (len(docURI) == 0) {
        docURI = Str(kManualDefaultDocURI);
    }

    Str fragment = str::SliceFromChar(docURI, '#');
    Str pathStart = docURI;
    if (len(pathStart) > 0 && pathStart.s[0] == '/') {
        pathStart = Str(pathStart.s + 1, pathStart.len - 1);
    }
    int pathLen = fragment ? (int)(fragment.s - pathStart.s) : pathStart.len;
    if (pathLen <= 0) {
        pathStart = Str(kManualDefaultDocURI + 1);
        pathLen = pathStart.len;
        fragment = {};
    }

    TempStr htmlFile = str::DupTemp(Str(pathStart.s, pathLen));
    if (!str::EndsWithI(htmlFile, StrL(".html"))) {
        htmlFile = str::JoinTemp(htmlFile, StrL(".html"));
    }

    TempStr url = str::JoinTemp(Str(kManualVirtualHost), htmlFile);
    if (fragment) {
        url = str::JoinTemp(url, fragment);
    }
    return url;
}

static void SetAnnotCreateArgsFromCommand(AnnotCreateArgs& args, CustomCommand* cmd) {
    args.copyToClipboard = GetCommandBoolArg(cmd, kCmdArgCopyToClipboard, false);
    args.setContentToSelection = GetCommandBoolArg(cmd, kCmdArgSetContent, false);

    auto* col = GetCommandArg(cmd, kCmdArgColor);
    if (col && col->colorVal.parsedOk) {
        args.col = col->colorVal;
    }

    auto* bgCol = GetCommandArg(cmd, kCmdArgBgColor);
    if (bgCol && bgCol->colorVal.parsedOk) {
        args.bgCol = bgCol->colorVal;
    }

    auto* interiorCol = GetCommandArg(cmd, kCmdArgInteriorColor);
    if (interiorCol && interiorCol->colorVal.parsedOk) {
        args.interiorCol = interiorCol->colorVal;
    }

    if (GetCommandArg(cmd, kCmdArgOpacity)) {
        args.opacity = GetCommandIntArg(cmd, kCmdArgOpacity, 100);
        setMinMax(args.opacity, 0, 100);
    }

    int textSize = GetCommandIntArg(cmd, kCmdArgTextSize, -1);
    if (textSize >= 0) {
        // set some reasonable limits
        setMinMax(textSize, 5, 128);
        args.textSize = textSize;
    }

    int borderWidth = GetCommandIntArg(cmd, kCmdArgBorderWidth, -1);
    if (borderWidth >= 0) {
        // set some reasonable limits
        setMinMax(borderWidth, 0, 128);
        args.borderWidth = borderWidth;
    }

    int quadding = QuaddingFromName(GetCommandStringArg(cmd, kCmdArgAlignment, {}));
    if (quadding >= 0) {
        args.quadding = quadding;
    }
}

void SetAnnotCreateArgs(AnnotCreateArgs& args, CustomCommand* cmd) {
    auto& a = gSettings->annotations;
    ParsedColor* col = nullptr;
    ParsedColor* bgCol = nullptr;
    auto typ = args.annotType;
    if (typ == AnnotationType::Text) {
        col = GetParsedColor(a.textIconColor);
    } else if (typ == AnnotationType::Underline) {
        col = GetParsedColor(a.underlineColor);
    } else if (typ == AnnotationType::Highlight) {
        col = GetParsedColor(a.highlightColor);
    } else if (typ == AnnotationType::Squiggly) {
        col = GetParsedColor(a.squigglyColor);
    } else if (typ == AnnotationType::StrikeOut) {
        col = GetParsedColor(a.strikeOutColor);
    } else if (typ == AnnotationType::FreeText) {
        col = GetParsedColor(a.freeTextColor);
        bgCol = GetParsedColor(a.freeTextBackgroundColor);
        if (bgCol && bgCol->parsedOk) {
            args.bgCol = *bgCol;
        }
        args.opacity = a.freeTextOpacity;
        args.textSize = a.freeTextSize;
        args.borderWidth = a.freeTextBorderWidth;
        args.quadding = QuaddingFromName(a.freeTextAlignment);
    } else if (typ == AnnotationType::Line) {
        col = GetParsedColor(a.lineColor);
    } else if (typ == AnnotationType::PolyLine) {
        col = GetParsedColor(a.polyLineColor);
    } else if (typ == AnnotationType::Square) {
        col = GetParsedColor(a.squareColor);
    } else if (typ == AnnotationType::Circle) {
        col = GetParsedColor(a.circleColor);
    } else if (typ == AnnotationType::Polygon) {
        col = GetParsedColor(a.polygonColor);
    } else if (typ == AnnotationType::Ink) {
        col = GetParsedColor(a.inkColor);
        args.borderWidth = a.inkBorderWidth;
    } else if (typ == AnnotationType::Stamp) {
        col = GetParsedColor(a.stampColor);
    } else if (typ == AnnotationType::Caret) {
        col = GetParsedColor(a.caretColor);
    } else if (typ == AnnotationType::FileAttachment) {
        col = GetParsedColor(a.fileAttachmentColor);
    } else if (typ == AnnotationType::Redact) {
        // a redaction mark has no color to pick: it's the black box that
        // replaces the text. MuPDF's default is what we want
    } else {
        logf("SetAnnotCreateArgs: unexpected type %d for default prefs color\n", (int)typ);
        // ReportIf(true);
    }
    if (col && col->parsedOk) {
        args.col = *col;
    }

    // a command's arguments (e.g. Shift+A's "openedit", or a color) override
    // the settings; ones it doesn't give keep them (#6197). Test the arguments,
    // not `cmd->id != cmd->origId`: a colliding Shortcuts entry gets a generated
    // id even without arguments (#5869).
    if (cmd && cmd->firstArg) {
        SetAnnotCreateArgsFromCommand(args, cmd);
    }
}

void TocItemToText(str::Builder& s, TocItem* item, int level) {
    while (item) {
        if (item->title) {
            for (int i = 0; i < level; i++) {
                s.AppendChar('\t');
            }
            s.Append(item->title);
            s.AppendChar('\n');
        }
        if (item->child) {
            int nextLevel = item->title ? level + 1 : level;
            TocItemToText(s, item->child, nextLevel);
        }
        item = item->next;
    }
}

// for toggle commands that accept an optional "state" bool arg (issue #5067):
// returns false if the command asked for a state that already matches the
// current one (so the toggle should be skipped); true otherwise (no explicit
// state given, or the requested state differs and a flip is needed)
bool ShouldToggle(CustomCommand* cmd, bool curState) {
    if (!GetCommandArg(cmd, kCmdArgState)) {
        return true; // no explicit state: always toggle
    }
    return GetCommandBoolArg(cmd, kCmdArgState, !curState) != curState;
}

// The image file the current tab is showing, or empty when it isn't showing
// one. The image editor takes a path rather than reaching into the tab itself.
Str CurrentImageTabPathTemp(MainWindow* win) {
    if (!win) {
        return {};
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return {};
    }
    if (tab->GetEngineType() != kindEngineImage) {
        return {};
    }
    return tab->filePath;
}

// Run the print dialog from the message loop rather than from whatever loop
// delivered the WM_COMMAND. TranslateAcceleratorW sends it from inside a win32k
// user-mode callback, and menu commands come from the menu's modal loop, so
// calling PrintCurrentFile() directly nests the modal dialog inside one of
// those. That matters more than it used to: on Windows 11 PrintDlgExW shows the
// out-of-process unified print dialog and blocks on it with
// CoWaitForMultipleHandles, and it has been seen to never return - the dialog
// vanishes and the app hangs with PrintDlgExW still on the stack.
void PrintCurrentFileDeferred(MainWindow* win) {
    // the window can be closed between posting this and running it
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    PrintCurrentFile(win);
}

void PrintSelectionDeferred(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    PrintCurrentFile(win, false, true);
}

// A gesture that writes to the document as it goes (a resize drag writes the
// annotation on every mouse move) should still be a single undo step, so it
// holds one journal operation open from start to end. Both calls are safe to
// make when there is nothing open, which keeps the many ways a gesture can end
// (mouse up, Esc, the annotation deleted, the document closed) from leaking it.
void BeginPdfEditOperation(MainWindow* win, const char* name) {
    if (!win || win->pdfEditOperationActive) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !EngineSupportsAnnotations(engine)) {
        return;
    }
    EngineMupdfBeginOperation(engine, name);
    win->pdfEditOperationActive = true;
}

void EndPdfEditOperation(MainWindow* win) {
    if (!win || !win->pdfEditOperationActive) {
        return;
    }
    win->pdfEditOperationActive = false;
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (engine) {
        EngineMupdfEndOperation(engine);
    }
}

void ReplaceColor(ParsedColor& col, Str maybeColor) {
    ParsedColor c;
    ParseColor(c, maybeColor);
    if (c.parsedOk) {
        SetColorText(col, SerializeColorTemp(c.col));
    }
}

EBookUI* GetEBookUI() {
    if (!gSettings) return nullptr;
    return &gSettings->eBookUI;
}

// ok for tab to be null
void SelectTabInWindow(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    auto* win = tab->win;
    if (tab == win->CurrentTab()) {
        return;
    }
    TabsSelect(win, win->GetTabIdx(tab));
}

// True while a tab is mid-load (async open). Used so we don't treat a plain
// home/empty window as "still loading" for WindowState bookkeeping.
bool WindowHasDocumentLoading(MainWindow* win) {
    if (!win) {
        return false;
    }
    for (WindowTab* tab : win->Tabs()) {
        if (tab->loadState == WindowTab::LoadState::Loading || tab->loadState == WindowTab::LoadState::LoadedPending) {
            return true;
        }
    }
    return false;
}

// an image next to the document with the same base name (Calibre puts a
// "Title.jpg" cover next to "Title.epub") beats page 1 as the thumbnail
TempStr FindCoverImageTemp(Str docPath) {
    static const char* kCoverExts[] = {".jpg", ".jpeg", ".png"};
    TempStr noExt = path::GetPathNoExtTemp(docPath);
    for (const char* ext : kCoverExts) {
        TempStr cover = str::JoinTemp(noExt, Str(ext));
        if (str::EqI(cover, docPath)) {
            continue;
        }
        if (file::Exists(cover)) {
            return cover;
        }
    }
    return {};
}

void TogglePageInfoHelper(MainWindow* win) {
    if (!win) {
        return;
    }
    if (win->pageInfoWanted) {
        win->pageInfoWanted = false;
        RemoveNotificationsForGroup(win, kNotifPageInfo);
        return;
    }
    win->pageInfoWanted = true;
    ShowPageInfoIfWanted(win);
}

void RenameFileInHistory(Str oldPath, Str newPath) {
    logf("RenameFileInHistory: oldPath: '%s', newPath: '%s'\n", oldPath, newPath);
    if (path::IsSame(oldPath, newPath)) {
        return;
    }
    FileState* fs = FileHistoryFindByPath(newPath);
    bool oldIsPinned = false;
    int oldOpenCount = 0;
    if (fs) {
        oldIsPinned = fs->isPinned;
        oldOpenCount = fs->openCount;
        FileHistoryRemove(fs);
        // TODO: merge favorites as well?
        if (len(*fs->favorites) > 0) {
            UpdateFavoritesTreeForAllWindows();
        }
        DeleteFileState(fs);
    }
    fs = FileHistoryFindByPath(oldPath);
    if (fs) {
        SetFileStatePath(fs, newPath);
        // merge Frequently Read data, so that a file
        // doesn't accidentally vanish from there
        fs->isPinned = fs->isPinned || oldIsPinned;
        fs->openCount += oldOpenCount;
        // the thumbnail is recreated by LoadDocument
        FreePixmap(fs->thumbnail);
        fs->thumbnail = nullptr;
    }
}

MeasurementUnit cursorPosUnit = MeasurementUnit::pt;

// end-of-document hint for "open next file in folder" discoverability
Kind kNotifNextFileHint = "nextFileHint";

void DismissNextFileScrollHint(MainWindow* win) {
    if (!win) {
        return;
    }
    RemoveNotificationsForGroup(win, kNotifNextFileHint);
}

void ToggleContinuousView(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    DisplayMode newMode = win->ctrl->GetDisplayMode();
    switch (newMode) {
        case DisplayMode::SinglePage:
        case DisplayMode::Continuous:
            newMode = IsContinuous(newMode) ? DisplayMode::SinglePage : DisplayMode::Continuous;
            break;
        case DisplayMode::Facing:
        case DisplayMode::ContinuousFacing:
            newMode = IsContinuous(newMode) ? DisplayMode::Facing : DisplayMode::ContinuousFacing;
            break;
        case DisplayMode::BookView:
        case DisplayMode::ContinuousBookView:
            newMode = IsContinuous(newMode) ? DisplayMode::BookView : DisplayMode::ContinuousBookView;
            break;
        default:
            break;
    }
    SwitchToDisplayMode(win, newMode);
}

void ShowZoomNotification(MainWindow* win, float zoomLevel) {
    // don't show zoom info if showing page info
    NotificationWnd* wnd = GetNotificationForGroup(win, kNotifPageInfo);
    if (wnd) {
        return;
    }
    NotificationCreateArgs args;
    args.groupId = kNotifZoomOrView;
    args.timeoutMs = 2000;
    args.win = win;
    args.msg = BuildZoomString(zoomLevel);
    ShowNotification(args);
}

void ShowViewModeNotification(MainWindow* win, int cmdId) {
    NotificationWnd* wnd = GetNotificationForGroup(win, kNotifPageInfo);
    if (wnd) {
        return;
    }
    Str viewName;
    if (cmdId == CmdSinglePageView) {
        viewName = Tr("Single Page");
    } else if (cmdId == CmdFacingView) {
        viewName = Tr("Facing");
    } else if (cmdId == CmdBookView) {
        viewName = Tr("Book View");
    } else {
        return;
    }
    TempStr msg = fmt("%s: %s", Tr("View"), viewName);
    NotificationCreateArgs args;
    args.groupId = kNotifZoomOrView;
    args.timeoutMs = 2000;
    args.win = win;
    args.msg = msg;
    ShowNotification(args);
}

// Zoom so that the current selection (Ctrl + drag rectangle or selected text)
// fills the window, and centre it. The selection itself is left alone so it can
// still be copied afterwards, and a navigation point is added first so Back
// returns to the view you zoomed from (issue #1699).
void ZoomToSelection(MainWindow* win) {
    DisplayModel* dm = win->AsFixed();
    WindowTab* tab = win->CurrentTab();
    if (!dm || !tab || !win->showSelection || !tab->selectionOnPage) {
        return;
    }

    // the selection doesn't move in page coordinates while we zoom, so remember
    // it there and map it back to the screen once the new zoom is applied
    int pageNo = 0;
    RectF selPage;
    Rect selScreen;
    bool isFirst = true;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        Rect rc = sel.GetRect(dm);
        if (rc.IsEmpty()) {
            continue;
        }
        if (isFirst) {
            pageNo = sel.pageNo;
            selPage = sel.rect;
            selScreen = rc;
            isFirst = false;
            continue;
        }
        selScreen = selScreen.Union(rc);
        if (sel.pageNo == pageNo) {
            selPage = selPage.Union(sel.rect);
        }
    }
    Rect viewPort = dm->GetViewPort();
    if (isFirst || selScreen.dx <= 0 || selScreen.dy <= 0 || viewPort.dx <= 0 || viewPort.dy <= 0) {
        return;
    }

    float fx = (float)viewPort.dx / (float)selScreen.dx;
    float fy = (float)viewPort.dy / (float)selScreen.dy;
    float newZoom = dm->GetZoomVirtual(true) * std::min(fx, fy);
    newZoom = limitValue(newZoom, kZoomMin, kZoomMax);

    // remember the zoom too, so Back undoes the whole "zoom to selection"
    dm->AddNavPoint(true);
    SmartZoom(win, newZoom, nullptr, false);

    // put the middle of the selection in the middle of the window
    Rect rc = dm->CvtToScreen(pageNo, selPage);
    viewPort = dm->GetViewPort();
    // the selection can already be centered on either axis, in which case
    // there's nothing to scroll (ScrollYBy asserts on a 0 delta)
    int dx = rc.x + (rc.dx / 2) - (viewPort.dx / 2);
    int dy = rc.y + (rc.dy / 2) - (viewPort.dy / 2);
    if (0 != dx) {
        dm->ScrollXBy(dx);
    }
    if (0 != dy) {
        dm->ScrollYBy(dy, false);
    }
}

// what CmdToggleCursorPosition would switch to. The tip cycles pt -> mm -> in
// and then closes, so the command palette can't say true / false; naming the
// next unit here keeps it in step with ToggleCursorPositionInDoc() below
// next state of the cursor-position tip, for the command palette
Str NextCursorPositionUnitName(MainWindow* win) {
    if (!win || !win->AsFixed()) {
        return {};
    }
    if (!GetNotificationForGroup(win, kNotifCursorPos)) {
        return StrL("pt");
    }
    if (cursorPosUnit == MeasurementUnit::pt) {
        return StrL("mm");
    }
    if (cursorPosUnit == MeasurementUnit::mm) {
        return StrL("in");
    }
    return StrL("off");
}

bool IsManualDocHtmlPage(Str path) {
    if (len(path) == 0 || !str::EndsWithI(path, StrL(".html"))) {
        return false;
    }
    if (str::EqI(path, StrL("manual.shell.html"))) {
        return false;
    }
    return true;
}

// The manual's theme switch (docs/theme.js) has a third option that follows the
// app: announce the app's scheme and the HelpTheme setting before the script
// runs, and hand the exact window colors to manual.css so "app" mode matches
// the native window.
Str ManualInjectThemeCss(Str html) {
    TempStr bg = SerializeColorTemp(ThemeWindowBackgroundColor());
    TempStr fg = SerializeColorTemp(ThemeWindowTextColor());
    Str scheme = IsLightColor(ThemeWindowBackgroundColor()) ? StrL("light") : StrL("dark");
    // theme.js calls the follow-the-app option "system"
    Str pref = HelpThemePref();
    if (str::Eq(pref, StrL("app"))) {
        pref = StrL("system");
    }
    TempStr script =
        fmt("<script>window.SumatraAppTheme=\"%s\";window.SumatraManualTheme=\"%s\"</script>", scheme, pref);
    TempStr css =
        fmt("<style id=\"sumatra-manual-theme\">"
            "html[data-theme-pref=\"system\"]{--bg-primary:%s;--bg-elevated:%s;--text-primary:%s;--link-color:%s}"
            "</style>",
            bg, bg, fg, fg);

    int scriptAt = str::IndexOfI(html, StrL("<head>"));
    scriptAt = scriptAt < 0 ? 0 : scriptAt + len(StrL("<head>"));
    int cssAt = str::IndexOfI(html, StrL("</head>"));
    if (cssAt < scriptAt) {
        cssAt = scriptAt;
    }
    str::Builder result;
    result.Reserve(len(html) + len(script) + len(css));
    result.Append(Str(html.s, scriptAt));
    result.Append(script);
    result.Append(Str(html.s + scriptAt, cssAt - scriptAt));
    result.Append(css);
    result.Append(Str(html.s + cssAt, len(html) - cssAt));
    return result.TakeStr();
}

TempStr DocURIToWebUrlTemp(Str docURI) {
    if (len(docURI) == 0) {
        docURI = Str(kManualDefaultDocURI);
    }
    if (len(docURI) > 0 && docURI.s[0] == '/') {
        return fmt("https://www.sumatrapdfreader.org/docs%s", docURI);
    }
    return fmt("https://www.sumatrapdfreader.org/docs/%s", docURI);
}

// Pick the center of the visible part of the current page when a command has
// no usable canvas point, as happens after clicking an annotation-toolbar button.
bool SetPointToVisiblePage(DisplayModel* dm, Point& pt, int& pageNo) {
    pageNo = dm->FirstVisiblePageNo();
    if (!dm->ValidPageNo(pageNo)) {
        pageNo = dm->CurrentPageNo();
    }
    if (!dm->ValidPageNo(pageNo)) {
        return false;
    }
    PageInfo* pi = dm->GetPageInfo(pageNo);
    Size viewport = dm->GetViewPort().Size();
    Rect visible = pi->pageOnScreen.Intersect(Rect{0, 0, viewport.dx, viewport.dy});
    if (visible.IsEmpty()) {
        visible = pi->pageOnScreen;
    }
    if (visible.IsEmpty()) {
        return false;
    }
    pt = Point(visible.x + (visible.dx / 2), visible.y + (visible.dy / 2));
    return true;
}
