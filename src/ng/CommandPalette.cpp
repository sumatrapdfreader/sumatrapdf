/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's CommandPalette.cpp. The model is orig's - the seven collections
// (tabs, file history, commands, toc, favorites, annotations, settings), the
// prefix that picks between them, FilterUtil scoring, the ordering rules and
// what Enter does with each kind of row. What changed is the presentation:
// orig is a WS_POPUPWINDOW with an Edit and a custom-drawn VirtListBox, this
// is a card drawn into the window with a gpui Input and one element per row.
// The Ctrl+Tab switcher (gui/TabSwitcher.cpp) runs this in orig's smart-tab
// mode instead of keeping a second copy of the tab list.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"

#include "base/File.h"
#include "base/Pixmap.h"
#include "base/SettingsUtil.h"
#include "base/UITask.h"

#include "gui/UIModels.h"

#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "AnnotSearch.h"
#include "AnnotEditToolbar.h"
#include "FilterUtil.h"
#include "FilterHighlightDraw.h"
#include "DisplayModel.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "Commands.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "CommandAvailability.h"
#include "Translations.h"
#include "Theme.h"
#include "PdfDarkMode.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AnnotPlacement.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "FileHistory.h"
#include "Favorites.h"
#include "TableOfContents.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "gui/DocCanvas.h"
#include "gui/Sidebar.h"
#include "SumatraDialogs.h"
#include "CommandPalette.h"

#include "SumatraLog.h"

// separates a setting from the value being typed for it in the "= settings"
// query, e.g. "=ZoomIncrement = 25". A setting name never contains one
constexpr const char* kPaletteSettingValueSep = "=";

constexpr float kPaletteRowDy = 22;
constexpr float kPaletteTopMargin = 42;
constexpr int kPaletteMinDx = 640;
constexpr int kPaletteMaxDx = 1024;

constexpr int kThumbDx = 120;
constexpr int kThumbDy = 170;
constexpr int kThumbGap = 16;
constexpr int kThumbMaxCols = 6;
constexpr int kThumbPadding = 16;
constexpr int kThumbScrollbarDx = 10;
constexpr int kThumbRenderScreens = 1;
constexpr int kThumbKeepScreens = 2;
constexpr int kThumbOverscanRows = 1;
constexpr int kThumbLabelInset = 4;
constexpr int kThumbLabelPadX = 6;
constexpr int kThumbLabelPadY = 2;

struct ItemDataCP {
    i32 cmdId = 0;
    // a "Debug: ..." command; those are listed after all the others
    bool isDebug = false;
    WindowTab* tab = nullptr;
    Str filePath;
    TocItem* tocItem = nullptr;
    int indent = 0;
    int pageNo = 0; // toc entry destination page (0 if none), shown in the list
    FileState* favFs = nullptr;
    Favorite* fav = nullptr;
    Annotation* annot = nullptr;
    // a "= settings" row. In the setting-picking stage the row text is the
    // setting's dotted path; in the value-picking stage it is a candidate value
    // and settingPath names the setting it belongs to.
    SettingType settingType = SettingType::Comment; // Comment: not a setting row
    int settingOffset = 0;                          // into gSettings, see SettingFieldPtr()
    intptr_t settingDefault = 0;                    // FieldInfo::value, decoded per type
    Str settingPath;
    Str settingComment; // its doc comment, from the settings metadata
};

using StrVecCP = StrVecWithData<ItemDataCP>;

static bool IsSettingRow(const ItemDataCP* d) {
    return d->settingType != SettingType::Comment;
}

static const u8* SettingRowPtr(const ItemDataCP* d) {
    return SettingFieldPtr(d->settingOffset);
}

// one page's thumbnail; the bitmap is rendered on a worker, the RenderImage is
// made on the paint thread the first time the grid draws it
struct PaletteThumb {
    int pageNo = 0;
    Pixmap* bitmap = nullptr;
    gp::RenderImage* img = nullptr;
    bool failed = false;
};

struct ThumbnailCache {
    Vec<PaletteThumb> thumbs;
    EngineBase* renderEngine = nullptr;
    AtomicInt cancelRendering = 0;
    int rotation = 0;
    bool workerRunning = false;
    bool deleteWhenWorkerFinishes = false;
};

struct CommandPaletteWnd {
    MainWindow* win = nullptr;
    bool visible = false;
    Point cursorPos;

    gpui::InputState* editQuery = nullptr;
    bool wantFocus = false;
    bool wasActive = false;
    // orig's popup window, where the platform can have one; null: an overlay
    // in the frame
    ToolWindow* tw = nullptr;
#if OS_WIN
    // tests look for a focused Edit whose root is not the frame
    HWND queryEdit = nullptr;
#endif

    StrVecCP tabs;
    StrVecCP fileHistory;
    StrVecCP commands;
    StrVecCP toc;
    StrVecCP favorites;
    StrVecCP annotations;
    StrVecCP settings;
    // what the list shows: the filtered rows
    StrVecCP items;
    int sel = -1;
    float scrollOff = 0;

    StrVec filterWords;

    int currTabIdx = 0;
    int currTocIdx = 0;
    bool tocMode = false;
    bool thumbnailMode = false;
    bool smartTabMode = false;
    bool stickyMode = false;

    ThumbnailCache* thumbCache = nullptr;
    int pageCount = 0;
    int selectedPage = 1;
    int thumbCols = 1;
    // orig's PageThumbnailsCtrl layout: rowGap spreads the rows over the height
    float thumbItemDy = (float)(kThumbDy + kThumbGap);
    float thumbViewDy = 0;
    float thumbScrollY = 0;
    // scroll the selected page's row into view at the next build
    bool thumbReveal = false;
};

static CommandPaletteWnd* gCommandPaletteWnd = nullptr;

struct PaletteView {
    static void OnInput(PaletteView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnRowClick(PaletteView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnSwitchClick(PaletteView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t prefixIdx);
    static void OnThumbClick(PaletteView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t pageNo);
    static void OnThumbMove(PaletteView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev, int64_t pageNo);
    static void OnThumbScroll(PaletteView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<PaletteView> gPaletteView;

static void FilterStringsForQuery(Str filter, StrVecCP& strings);
static void CollectStrings(MainWindow* mainWin);
static void QueryChanged();
static bool ShowsSettingHelp(CommandPaletteWnd* wnd);
static void ExecuteCurrentSelection();
static void StartThumbnailRendering();
static void ThumbSelectPage(int pageNo);
static TempStr FormatSettingValueTemp(SettingType type, const u8* p);
static bool SplitSettingValueQuery(Str query, Str& path, Str& value);
static ItemDataCP* FindSetting(Str path, Str& foundPath);

// clang-format off
static i32 gCommandsNoActivate[] = {
    CmdOptions,
    CmdSetInverseSearch,
    CmdChangeLanguage,
    CmdHelpAbout,
    CmdHelpOpenManual,
    CmdHelpOpenManualOnWebsite,
    CmdHelpOpenKeyboardShortcuts,
    CmdHelpVisitWebsite,
    CmdOpenFile,
    CmdOpenFileNoHistory,
    CmdProperties,
    CmdNewWindow,
    CmdDuplicateInNewWindow,
    CmdListPrinters,
    CmdTabGroupSave,
    CmdTabGroupRestore,
    0,
};
// clang-format on

static bool IsCmdInList(i32 cmdId, i32* ids) {
    while (*ids) {
        if (cmdId == *ids) {
            return true;
        }
        ids++;
    }
    return false;
}

static Str CommandPaletteSkipWS(Str s) {
    if (!s.s) {
        return {};
    }
    str::TrimWs(s);
    return s;
}

static TempStr QueryTemp() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->editQuery) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(wnd->editQuery)));
}

bool IsCommandPaletteVisible() {
    return gCommandPaletteWnd && gCommandPaletteWnd->visible;
}

MainWindow* CommandPaletteWindow() {
    return gCommandPaletteWnd ? gCommandPaletteWnd->win : nullptr;
}

// --- thumbnails -------------------------------------------------------------

static void FreeThumbBitmaps(ThumbnailCache* cache) {
    for (PaletteThumb& t : cache->thumbs) {
        FreePixmap(t.bitmap);
        t.bitmap = nullptr;
        if (t.img) {
            gp::RenderImageRelease(t.img);
            t.img = nullptr;
        }
    }
}

static void DeleteThumbnailCache(ThumbnailCache* cache) {
    FreeThumbBitmaps(cache);
    VecReset(cache->thumbs);
    if (cache->renderEngine) {
        cache->renderEngine->Release();
        cache->renderEngine = nullptr;
    }
    delete cache;
}

// orig's RenderPageThumbnail
static Pixmap* RenderPageThumbnail(EngineBase* engine, int pageNo, Location loc, int rotation) {
    // reflow docs share one mediabox; don't use a flat pageNo that a clone's
    // chapter layout may have already shifted
    int boxPage = loc.IsValid() && engine->isReflowable ? 1 : pageNo;
    RectF pageRect = engine->PageMediabox(boxPage);
    if (pageRect.IsEmpty()) {
        return nullptr;
    }

    pageRect = engine->Transform(pageRect, boxPage, 1.0f, rotation);
    if (pageRect.dx <= 0 || pageRect.dy <= 0) {
        return nullptr;
    }
    float zoom = (float)kThumbDx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)kThumbDy / zoom);
    pageRect = engine->Transform(pageRect, boxPage, 1.0f, rotation, true);
    RenderPageArgs args(pageNo, zoom, rotation, &pageRect, RenderTarget::View);
    args.loc = loc;
    return PixmapToBgr(engine->RenderPage(args));
}

struct ThumbRenderTask {
    ThumbnailCache* cache = nullptr;
    int pageNo = 0;
    Pixmap* bitmap = nullptr;
};

struct ThumbRenderWorker {
    ThumbnailCache* cache = nullptr;
    EngineBase* sourceEngine = nullptr;
    Vec<int> pages;
    Vec<Location> locs;
    int rotation = 0;
};

static void FinishThumbRender(ThumbRenderTask* task) {
    ThumbnailCache* cache = task->cache;
    int idx = task->pageNo - 1;
    bool isValid = !cache->deleteWhenWorkerFinishes && idx >= 0 && idx < len(cache->thumbs);
    if (isValid) {
        PaletteThumb& t = cache->thumbs[idx];
        if (task->bitmap) {
            FreePixmap(t.bitmap);
            t.bitmap = task->bitmap;
            task->bitmap = nullptr;
            if (t.img) {
                gp::RenderImageRelease(t.img);
                t.img = nullptr;
            }
        } else {
            t.failed = true;
        }
        if (gCommandPaletteWnd && gCommandPaletteWnd->thumbCache == cache) {
            AppShellInvalidate(gCommandPaletteWnd->win);
        }
    }
    FreePixmap(task->bitmap);
    delete task;
}

static void FinishThumbWorker(ThumbnailCache* cache) {
    cache->workerRunning = false;
    if (cache->deleteWhenWorkerFinishes) {
        DeleteThumbnailCache(cache);
        return;
    }
    StartThumbnailRendering();
}

static void RenderThumbsInBackground(ThumbRenderWorker* worker) {
    ThumbnailCache* cache = worker->cache;
    if (worker->sourceEngine) {
        cache->renderEngine = worker->sourceEngine->Clone();
        worker->sourceEngine->Release();
        worker->sourceEngine = nullptr;
    }
    EngineBase* engine = cache->renderEngine;
    if (engine) {
        int n = len(worker->pages);
        for (int i = 0; i < n; i++) {
            if (AtomicIntGet(&cache->cancelRendering) != 0) {
                break;
            }
            Location loc = i < len(worker->locs) ? worker->locs[i] : kInvalidLocation;
            auto* task = new ThumbRenderTask;
            task->cache = cache;
            task->pageNo = worker->pages[i];
            task->bitmap = RenderPageThumbnail(engine, worker->pages[i], loc, worker->rotation);
            uitask::Post(MkFunc0<ThumbRenderTask>(FinishThumbRender, task));
        }
    }
    uitask::Post(MkFunc0<ThumbnailCache>(FinishThumbWorker, cache));
    delete worker;
}

static void FreeThumb(PaletteThumb& t) {
    FreePixmap(t.bitmap);
    t.bitmap = nullptr;
    if (t.img) {
        gp::RenderImageRelease(t.img);
        t.img = nullptr;
    }
    t.failed = false;
}

// orig's VirtListBox::UsableDy
static float ThumbUsableDy(CommandPaletteWnd* wnd) {
    float usable = (float)(int)(wnd->thumbViewDy / wnd->thumbItemDy) * wnd->thumbItemDy;
    return usable > 0 ? usable : wnd->thumbViewDy;
}

static int ThumbRows(CommandPaletteWnd* wnd) {
    int cols = std::max(wnd->thumbCols, 1);
    return (wnd->pageCount + cols - 1) / cols;
}

static float ThumbMaxScrollY(CommandPaletteWnd* wnd) {
    return std::max(0.f, (float)ThumbRows(wnd) * wnd->thumbItemDy - ThumbUsableDy(wnd));
}

// orig's PageThumbnailsCtrl::StartRendering: the rows in view first, then a
// screen on either side; thumbnails further than two more screens are freed
static void StartThumbnailRendering() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->thumbnailMode || !wnd->thumbCache || wnd->thumbCache->workerRunning) {
        return;
    }
    DisplayModel* dm = wnd->win ? wnd->win->AsFixed() : nullptr;
    if (!dm || dm->PageCount() != wnd->pageCount) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return;
    }
    ThumbnailCache* cache = wnd->thumbCache;
    if (wnd->thumbViewDy <= 0) {
        // not laid out yet; the first build starts the rendering
        return;
    }
    int cols = std::max(wnd->thumbCols, 1);
    int pageCount = wnd->pageCount;
    int visibleRows = std::max(1, (int)(ThumbUsableDy(wnd) / wnd->thumbItemDy));
    int firstVisible = ((int)(wnd->thumbScrollY / wnd->thumbItemDy) * cols) + 1;
    int perScreen = std::max(1, visibleRows * cols);
    int lastVisible = std::min(pageCount, firstVisible + perScreen - 1);
    int firstPage = std::max(1, firstVisible - (kThumbRenderScreens * perScreen));
    int lastPage = std::min(pageCount, lastVisible + (kThumbRenderScreens * perScreen));

    int keepFirst = std::max(1, firstPage - (kThumbKeepScreens * perScreen));
    int keepLast = std::min(pageCount, lastPage + (kThumbKeepScreens * perScreen));
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        if (pageNo < keepFirst || pageNo > keepLast) {
            FreeThumb(cache->thumbs[pageNo - 1]);
        }
    }

    auto* worker = new ThumbRenderWorker;
    auto queuePage = [&](int pageNo) {
        PaletteThumb& t = cache->thumbs[pageNo - 1];
        if (t.bitmap || t.failed) {
            return;
        }
        VecAppend(worker->pages, pageNo);
        VecAppend(worker->locs, engine->LocationFromPageNo(pageNo));
    };
    for (int pageNo = firstVisible; pageNo <= lastVisible; pageNo++) {
        queuePage(pageNo);
    }
    for (int pageNo = firstPage; pageNo < firstVisible; pageNo++) {
        queuePage(pageNo);
    }
    for (int pageNo = lastVisible + 1; pageNo <= lastPage; pageNo++) {
        queuePage(pageNo);
    }
    if (len(worker->pages) == 0) {
        delete worker;
        return;
    }
    worker->cache = cache;
    worker->rotation = cache->rotation;
    if (!cache->renderEngine) {
        worker->sourceEngine = engine;
        engine->AddRef();
    }
    AtomicIntSet(&cache->cancelRendering, 0);
    cache->workerRunning = true;
    RunAsync(MkFunc0<ThumbRenderWorker>(RenderThumbsInBackground, worker), StrL("CommandPaletteThumbnailRender"));
}

static void SetThumbnailMode(bool enabled) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (wnd->thumbnailMode == enabled) {
        return;
    }
    wnd->thumbnailMode = enabled;
    if (!enabled) {
        if (wnd->thumbCache) {
            AtomicIntSet(&wnd->thumbCache->cancelRendering, 1);
        }
        return;
    }
    DisplayModel* dm = wnd->win->AsFixed();
    if (!dm) {
        wnd->thumbnailMode = false;
        return;
    }
    if (!wnd->thumbCache) {
        wnd->pageCount = dm->PageCount();
        wnd->selectedPage = limitValue(dm->CurrentPageNo(), 1, std::max(wnd->pageCount, 1));
        auto* cache = new ThumbnailCache();
        cache->rotation = dm->GetRotation();
        for (int i = 0; i < wnd->pageCount; i++) {
            PaletteThumb t;
            t.pageNo = i + 1;
            VecAppend(cache->thumbs, t);
        }
        wnd->thumbCache = cache;
    }
    wnd->thumbReveal = true;
    StartThumbnailRendering();
}

static gp::ImageLoadState ThumbLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imgOut) {
    auto* t = (PaletteThumb*)user;
    if (!t->img && t->bitmap) {
        t->img = RenderImageFromPixmap(pa, t->bitmap);
    }
    *imgOut = t->img;
    return t->img ? gp::ImageLoadState::Ready : gp::ImageLoadState::Loading;
}

// --- closing ----------------------------------------------------------------

void CloseCommandPalette() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd) {
        return;
    }
    gCommandPaletteWnd = nullptr;
    MainWindow* win = wnd->win;
#if OS_WIN
    if (wnd->queryEdit) {
        HWND edit = wnd->queryEdit;
        wnd->queryEdit = nullptr;
        DestroyWindow(edit);
    }
#endif
    if (wnd->tw) {
        ToolWindowClose(wnd->tw);
        wnd->tw = nullptr;
    } else if (win && win->gpuiWin && wnd->editQuery) {
        gp::InputBlur(wnd->editQuery, win->gpuiWin->app, win->gpuiWin);
    }
    delete wnd->editQuery;
    if (wnd->thumbCache) {
        AtomicIntSet(&wnd->thumbCache->cancelRendering, 1);
        if (wnd->thumbCache->workerRunning) {
            wnd->thumbCache->deleteWhenWorkerFinishes = true;
        } else {
            DeleteThumbnailCache(wnd->thumbCache);
        }
    }
    delete wnd;
    AppShellInvalidate(win);
}

// orig's ScheduleDeleteAndExecCommand: the palette is gone before the command
// runs, so a command that opens a dialog gets the keyboard
struct PaletteExecCmd {
    MainWindow* win = nullptr;
    int cmdId = 0;
    Point cursorPos;
};

// commands that act at the mouse position (annotation create, read aloud from cursor).
// Placement-mode tools (ink, line, stamp, ...) start a mode; a point would skip
// that and create at the remembered cursor, like the context menu.
static bool CmdUsesCursorPos(int cmdId) {
    if (CommandUsesPlacementMode(cmdId)) {
        return false;
    }
    if (cmdId >= CmdCreateAnnotFirst && cmdId <= CmdCreateAnnotLast) {
        return true;
    }
    return cmdId == CmdCreateAnnotImageFromClipboard || cmdId == CmdReadAloudFromCursorPosition;
}

static void RunPaletteCmd(PaletteExecCmd* op) {
    if (IsMainWindowValidAndNotClosing(op->win)) {
        if (CmdUsesCursorPos(op->cmdId)) {
            ExecuteCmdAtPoint(op->win, op->cmdId, op->cursorPos);
        } else {
            ExecuteCmd(op->win, op->cmdId);
        }
    }
    delete op;
}

static void ScheduleDeleteAndExecCommand(i32 cmdId = 0) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd) {
        return;
    }
    MainWindow* win = wnd->win;
    Point cursorPos = wnd->cursorPos;
    CloseCommandPalette();
    if (cmdId == 0 || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    auto* op = new PaletteExecCmd{win, cmdId, cursorPos};
    uitask::Post(MkFunc0<PaletteExecCmd>(RunPaletteCmd, op), "PaletteExecuteCmd");
}

// --- selection --------------------------------------------------------------

static void SetCurrentSelection(int idx) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    wnd->sel = idx;
    AppShellInvalidate(wnd->win);
}

static bool AdvanceSelection(int dir) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (dir == 0) {
        return false;
    }
    int n = len(wnd->items);
    if (n == 0) {
        return false;
    }
    int sel = wnd->sel + dir;
    if (sel < 0) {
        sel = n - 1;
    }
    if (sel >= n) {
        sel = 0;
    }
    SetCurrentSelection(sel);
    return true;
}

// Home / End / PageUp / PageDown move the list the same way as the Find
// window: Home/End go to the first/last row, PageUp/PageDown jump a page
// (no wrap). Up/Down still wrap via AdvanceSelection.
static bool MoveSelection(int vkey, int perPage) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (vkey == VK_UP) {
        return AdvanceSelection(-1);
    }
    if (vkey == VK_DOWN) {
        return AdvanceSelection(1);
    }
    int n = len(wnd->items);
    if (n == 0) {
        return false;
    }
    int curr = wnd->sel;
    int idx = curr;
    switch (vkey) {
        case VK_HOME:
            idx = 0;
            break;
        case VK_END:
            idx = n - 1;
            break;
        case VK_NEXT:
            idx = curr < 0 ? 0 : std::min(curr + perPage, n - 1);
            break;
        case VK_PRIOR:
            idx = curr < 0 ? n - 1 : std::max(curr - perPage, 0);
            break;
        default:
            return false;
    }
    if (idx == curr) {
        return true;
    }
    SetCurrentSelection(idx);
    return true;
}

// orig's VirtListBox::EnsureVisible for the selected page's row
static void ThumbEnsureVisible(CommandPaletteWnd* wnd) {
    int cols = std::max(wnd->thumbCols, 1);
    float top = (float)((wnd->selectedPage - 1) / cols) * wnd->thumbItemDy;
    float visibleDy = ThumbUsableDy(wnd);
    if (top < wnd->thumbScrollY) {
        wnd->thumbScrollY = top;
    } else if (top + wnd->thumbItemDy > wnd->thumbScrollY + visibleDy) {
        wnd->thumbScrollY = top + wnd->thumbItemDy - visibleDy;
    }
    wnd->thumbScrollY = limitValue(wnd->thumbScrollY, 0.f, ThumbMaxScrollY(wnd));
}

// orig's PageThumbnailsCtrl::SelectPage
static void ThumbSelectPage(int pageNo) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (wnd->pageCount <= 0) {
        return;
    }
    pageNo = limitValue(pageNo, 1, wnd->pageCount);
    if (pageNo == wnd->selectedPage) {
        return;
    }
    wnd->selectedPage = pageNo;
    ThumbEnsureVisible(wnd);
    StartThumbnailRendering();
    AppShellInvalidate(wnd->win);
}

// orig's PageThumbnailsCtrl::HandleKey
static void ThumbnailHandleKey(int vkey) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (wnd->pageCount <= 0) {
        return;
    }
    int pageNo = wnd->selectedPage;
    int cols = std::max(wnd->thumbCols, 1);
    int pageStep = std::max(1, (int)(ThumbUsableDy(wnd) / wnd->thumbItemDy)) * cols;
    switch (vkey) {
        case VK_LEFT:
            pageNo--;
            break;
        case VK_RIGHT:
            pageNo++;
            break;
        case VK_UP:
            pageNo -= cols;
            break;
        case VK_DOWN:
            pageNo += cols;
            break;
        case VK_PRIOR:
            pageNo -= pageStep;
            break;
        case VK_NEXT:
            pageNo += pageStep;
            break;
        case VK_HOME:
            pageNo = 1;
            break;
        case VK_END:
            pageNo = wnd->pageCount;
            break;
        default:
            return;
    }
    ThumbSelectPage(limitValue(pageNo, 1, wnd->pageCount));
}

static void OpenSelectedThumbnailPage() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    DisplayModel* dm = wnd->win ? wnd->win->AsFixed() : nullptr;
    if (!dm) {
        return;
    }
    dm->GoToPage(wnd->selectedPage, 0, true);
    ScheduleDeleteAndExecCommand();
}

// --- switching modes --------------------------------------------------------

static void SwitchToPrefix(Str prefix) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd->editQuery) {
        return;
    }
    gp::InputSetValue(wnd->editQuery, ToGpui(prefix));
    wnd->wantFocus = true;
    QueryChanged();
}

// Second stage of "= settings": the query becomes "=<path> = <value>" and the
// list offers values instead of settings. An enum's current value is left out
// so every choice shows; a free-form value is pre-filled so it can be edited.
static void BeginEditSettingValue(Str path) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    TempStr value = {};
    if (!GetSettingsEnumValues(path)) {
        for (int i = 0; i < len(wnd->settings); i++) {
            if (str::Eq(wnd->settings[i], path)) {
                value = FormatSettingValueTemp(wnd->settings.AtData(i)->settingType,
                                               SettingRowPtr(wnd->settings.AtData(i)));
                break;
            }
        }
    }
    SwitchToPrefix(fmt("%s%s %s %s", Str(kPalettePrefixBoolSettings), path, Str(kPaletteSettingValueSep), value));
}

static void SelectSetting(Str path) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    for (int i = 0; i < len(wnd->items); i++) {
        if (str::Eq(wnd->items[i], path)) {
            SetCurrentSelection(i);
            return;
        }
    }
}

// Back to the setting-picking stage with selPath selected. Applying a value
// reloads gSettings, so the rows built from it (settings, file history,
// favorites) is rebuilt; selPath usually points into those rows, hence the copy.
static void ReturnToSettings(Str selPath) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !IsMainWindowValid(wnd->win)) {
        return;
    }
    TempStr path = str::DupTemp(selPath);
    CollectStrings(wnd->win);
    SwitchToPrefix(Str(kPalettePrefixBoolSettings));
    SelectSetting(path);
}

// the full path of the setting named in the "=<path> = <value>" query
static TempStr EditedSettingPathTemp() {
    Str filter = CommandPaletteSkipWS(QueryTemp());
    str::TrimPrefix(filter, Str(kPalettePrefixBoolSettings));
    Str path, value, foundPath;
    if (!SplitSettingValueQuery(filter, path, value) || !FindSetting(path, foundPath)) {
        return {};
    }
    return str::DupTemp(foundPath);
}

static bool IsEditingSettingValue() {
    Str query = CommandPaletteSkipWS(QueryTemp());
    if (!str::TrimPrefix(query, Str(kPalettePrefixBoolSettings))) {
        return false;
    }
    Str path;
    Str value;
    return SplitSettingValueQuery(query, path, value);
}

// --- running a row ----------------------------------------------------------

static void ExecuteCurrentSelection() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    int idx = wnd->sel;
    if (idx < 0 || idx >= len(wnd->items)) {
        return;
    }
    MainWindow* win = wnd->win;
    ItemDataCP* data = wnd->items.AtData(idx);
    i32 cmdId = data->cmdId;
    logf("CommandPalette: run row %d of %d, '%s', cmd %d\n", idx, len(wnd->items), wnd->items[idx], cmdId);
    if (cmdId == CmdToggleBoolSetting) {
        SwitchToPrefix(Str(kPalettePrefixBoolSettings));
        return;
    }
    if (cmdId != 0) {
        // ng: orig also decides which window to re-activate; the palette is
        // inside the window here, so gCommandsNoActivate only documents that
        IsCmdInList(cmdId, gCommandsNoActivate);
        ScheduleDeleteAndExecCommand(cmdId);
        return;
    }

    if (IsSettingRow(data)) {
        Str itemText = wnd->items[idx];
        if (len(data->settingPath) > 0) {
            // a value picked for a setting: the row text is the value
            logf("CommandPalette: setting '%s' = '%s'\n", data->settingPath, itemText);
            TempStr selPath = str::DupTemp(data->settingPath);
            SetSettingsValueFromStr(selPath, itemText);
            ReturnToSettings(selPath);
            return;
        }
        if (data->settingType == SettingType::Bool) {
            logf("CommandPalette: toggling setting '%s'\n", itemText);
            ToggleSettingsBool((bool*)SettingRowPtr(data));
            ReturnToSettings(itemText);
            return;
        }
        // anything else needs a value: stay open and ask for one
        BeginEditSettingValue(itemText);
        return;
    }

    WindowTab* tab = data->tab;
    if (tab != nullptr) {
        MainWindow* mainWin = FindMainWindowByTab(tab);
        if (!mainWin) {
            ScheduleDeleteAndExecCommand();
            return;
        }
        int tabIdx = mainWin->GetTabIdx(tab);
        ScheduleDeleteAndExecCommand();
        if (tabIdx >= 0) {
            TabsSelect(mainWin, tabIdx);
        }
        return;
    }

    if (data->tocItem) {
        TocItem* ti = data->tocItem;
        ScheduleDeleteAndExecCommand();
        GoToTocItem(win, ti);
        return;
    }

    if (data->fav) {
        FileState* fs = data->favFs;
        Favorite* fav = data->fav;
        ScheduleDeleteAndExecCommand();
        GoToFavorite(win, fs, fav);
        return;
    }

    if (data->annot) {
        Annotation* annot = data->annot;
        WindowTab* currTab = win->CurrentTab();
        ScheduleDeleteAndExecCommand();
        if (currTab) {
            SetSelectedAnnotation(currTab, annot);
        }
        return;
    }

    if (data->filePath) {
        TempStr path = str::DupTemp(data->filePath);
        ScheduleDeleteAndExecCommand();
        LoadDocument(win, path);
        return;
    }
    logf("CommandPalette: no match for selection '%s'\n", wnd->items[idx]);
    ScheduleDeleteAndExecCommand();
}

// Delete on a removable row: a file-history entry, an open tab or a favorite.
// Commands and TOC entries are not removable.
static bool RemoveSelectedItem() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    int currSel = wnd->sel;
    if (currSel < 0 || currSel >= len(wnd->items)) {
        return false;
    }
    ItemDataCP* d = wnd->items.AtData(currSel);
    if (d->cmdId != 0 || d->tocItem) {
        return false;
    }
    WindowTab* tab = d->tab;
    Favorite* fav = d->fav;
    FileState* favFs = d->favFs;
    TempStr filePath = str::DupTemp(d->filePath);
    if (!tab && !(fav && favFs) && len(filePath) == 0) {
        return false;
    }
    MainWindow* host = wnd->win;
    if (tab) {
        CloseTab(tab, false);
    } else if (fav && favFs) {
        DelFavorite(favFs, fav);
    } else {
        ForgetFileFromFrequentlyRead(host, filePath);
    }
    if (gCommandPaletteWnd != wnd || !IsMainWindowValid(host)) {
        return true;
    }
    CollectStrings(host);
    FilterStringsForQuery(CommandPaletteSkipWS(QueryTemp()), wnd->items);
    int n = len(wnd->items);
    SetCurrentSelection(n == 0 ? -1 : std::min(currSel, n - 1));
    return true;
}

#if OS_WIN
// orig's list-box dump. exit 2 when the palette is not open.
TempStr CommandPaletteStateTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible) {
        out.Append(StrL("NOTREADY no-palette\n"));
        return finish(2);
    }
    int sel = wnd->sel;
    int n = len(wnd->items);
    int selectedCmdId = 0;
    int annotPage = 0;
    Str selText;
    Str selValue;
    if (sel >= 0 && sel < n) {
        selText = wnd->items[sel];
        ItemDataCP* data = wnd->items.AtData(sel);
        if (data) {
            selectedCmdId = data->cmdId;
            if (data->annot) {
                annotPage = data->annot->pageNo;
            }
            if (IsSettingRow(data) && len(data->settingPath) == 0) {
                selValue = FormatSettingValueTemp(data->settingType, SettingRowPtr(data));
            }
        }
    }
    int qPos = 0;
    int qLen = 0;
    if (wnd->editQuery) {
        qLen = len(FromGpui(gp::InputValue(wnd->editQuery)));
        qPos = gp::InputCursor(wnd->editQuery);
    }
    int rendered = 0;
    if (wnd->thumbCache) {
        for (PaletteThumb& th : wnd->thumbCache->thumbs) {
            if (th.img) {
                rendered++;
            }
        }
    }
    int nAnnots = len(wnd->annotations);
    EngineBase* engine = wnd->win && wnd->win->CurrentTab() ? wnd->win->CurrentTab()->GetEngine() : nullptr;
    int annotsDone = EngineMupdfAnnotsLoadDone(engine) ? 1 : 0;
    out.Append(
        fmt("OK sel=%d items=%d querySel=%d,%d queryLen=%d cmd=%d rtl=%d thumb=%d page=%d rendered=%d annots=%d "
            "annotPage=%d annotsDone=%d ",
            sel, n, qPos, qPos, qLen, selectedCmdId, IsUIRtl() ? 1 : 0, wnd->thumbnailMode ? 1 : 0, wnd->selectedPage,
            rendered, nAnnots, annotPage, annotsDone));
    int editFocus = wnd->editQuery && wnd->editQuery->focused ? 1 : 0;
    int helpShown = (!wnd->thumbnailMode && ShowsSettingHelp(wnd)) ? 1 : 0;
    out.Append(fmt("settingHelp=%d selValue=%s selText=%s editFocus=%d\n", helpShown, selValue, selText, editFocus));
    return finish(0);
}
#endif

// --- keyboard ---------------------------------------------------------------

bool CommandPaletteOnKeyDown(MainWindow* win, int vkey, bool ctrl, bool shift) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible || wnd->win != win) {
        return false;
    }
    if (vkey == VK_ESCAPE) {
        if (IsEditingSettingValue()) {
            ReturnToSettings(EditedSettingPathTemp());
            return true;
        }
        ScheduleDeleteAndExecCommand();
        return true;
    }
    if (vkey == VK_RETURN) {
        if (wnd->thumbnailMode) {
            OpenSelectedThumbnailPage();
            return true;
        }
        ExecuteCurrentSelection();
        return true;
    }
    if (vkey == VK_DELETE) {
        // not a removable list item: let the edit control process Delete
        return RemoveSelectedItem();
    }
    if (vkey == VK_TAB) {
        if (ctrl) {
            return AdvanceSelection(shift ? -1 : 1);
        }
        return false;
    }
    bool isArrow = vkey == VK_LEFT || vkey == VK_RIGHT || vkey == VK_UP || vkey == VK_DOWN || vkey == VK_NEXT ||
                   vkey == VK_PRIOR || vkey == VK_HOME || vkey == VK_END;
    if (!isArrow) {
        return false;
    }
    if (wnd->thumbnailMode) {
        ThumbnailHandleKey(vkey);
        return true;
    }
    if (vkey == VK_LEFT || vkey == VK_RIGHT) {
        return false;
    }
    if (vkey == VK_HOME || vkey == VK_END) {
        // Home / End: if the caret is already at the start/end of the query,
        // move the list; otherwise let the Input move the caret
        if (!ctrl && wnd->editQuery) {
            int cursor = gp::InputCursor(wnd->editQuery);
            int textLen = len(FromGpui(gp::InputValue(wnd->editQuery)));
            bool caretAtBound = (vkey == VK_END) ? cursor == textLen : cursor == 0;
            if (!caretAtBound) {
                return false;
            }
        }
    }
    // rows that fit in the list box; the card is sized in CommandPaletteBuild
    int perPage = std::max((int)((float)win->frameRc.dy * 0.6f / kPaletteRowDy), 1);
    return MoveSelection(vkey, perPage);
}

// smart-tab: orig watches for the Ctrl key going up in its PreTranslate
bool CommandPaletteOnKeyUp(MainWindow* win, int vkey) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible || wnd->win != win || !wnd->smartTabMode || wnd->stickyMode) {
        return false;
    }
    if (vkey != VK_CONTROL) {
        return false;
    }
    ExecuteCurrentSelection();
    return true;
}

WindowTab* CommandPaletteHighlightedTab(MainWindow* win) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible || wnd->win != win || !wnd->smartTabMode) {
        return nullptr;
    }
    if (wnd->sel < 0 || wnd->sel >= len(wnd->items)) {
        return nullptr;
    }
    return wnd->items.AtData(wnd->sel)->tab;
}

// --- collecting -------------------------------------------------------------

static bool AllowCommand(const AppCommandCtx& ctx, i32 cmdId) {
    return CommandShouldShow(GetCommandVisibility(cmdId, ctx, CommandSurface::Palette));
}

static TempStr ConvertPathForDisplayTemp(Str s) {
    return path::GetBaseNameTemp(s);
}

static TempStr RemovePrefixFromString(Str s) {
    return str::ReplaceTemp(s, StrL("&"), StrL(""));
}

// orig's UpdateCommandNameTemp, with the toggles this port has
static TempStr UpdateCommandNameTemp(MainWindow* win, int cmdId, Str s) {
    bool isToggle = false;
    bool newIsOn = false;
    switch (cmdId) {
        case CmdToggleFullscreen: {
            isToggle = true;
            newIsOn = !(win->isFullScreen || win->presentation);
        } break;
        case CmdToggleMenuBar: {
            isToggle = true;
            bool visible = SettingsUseTabs() ? gSettings->showMenubarWithTabs : gSettings->showMenubar;
            newIsOn = !visible;
        } break;
        case CmdToggleBookmarks:
        case CmdToggleTableOfContents: {
            isToggle = true;
            newIsOn = !SidebarContentVisible(win, SidebarContent::Bookmarks);
        } break;
        case CmdToggleThumbnails: {
            isToggle = true;
            newIsOn = !SidebarContentVisible(win, SidebarContent::Thumbnails);
        } break;
        case CmdTogglePresentationMode: {
            isToggle = true;
            newIsOn = !win->presentation;
        } break;
        case CmdToggleLinks: {
            isToggle = true;
            newIsOn = !gSettings->showLinks;
        } break;
        case CmdToggleHighlightFormFields: {
            isToggle = true;
            newIsOn = !gSettings->highlightFormFields;
        } break;
        case CmdToggleDisableLinks: {
            isToggle = true;
            newIsOn = !gSettings->disableLinks;
        } break;
        case CmdTogglePageGrid: {
            isToggle = true;
            newIsOn = !ShowPageGrid();
        } break;
        case CmdToggleShowAnnotations: {
            WindowTab* tab = win->CurrentTab();
            if (tab) {
                isToggle = true;
                newIsOn = tab->hideAnnotations;
            }
        } break;
        case CmdToggleContinuousView: {
            if (win->ctrl) {
                isToggle = true;
                newIsOn = !IsContinuous(win->ctrl->GetDisplayMode());
            }
        } break;
        case CmdToggleMangaMode: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !dm->GetDisplayR2L();
            }
        } break;
        case CmdToggleUniformPageWidth: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !dm->GetUniformPageWidth();
            }
        } break;
        case CmdToggleTrimEmptyMargins: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !dm->GetTrimEmptyMargins();
            }
        } break;
        case CmdToggleFreePan: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !dm->GetFreePan();
            }
        } break;
        case CmdFindToggleMatchCase: {
            isToggle = true;
            newIsOn = !win->findMatchCase;
        } break;
        case CmdFindToggleMatchWholeWord: {
            isToggle = true;
            newIsOn = !win->findMatchWholeWord;
        } break;
        case CmdFavoriteToggle: {
            isToggle = true;
            newIsOn = !gSettings->showFavorites;
        } break;
        case CmdTogglePageInfo: {
            isToggle = true;
            newIsOn = !win->pageInfoWanted;
        } break;
        case CmdTogglePageBoxes: {
            isToggle = true;
            newIsOn = !win->showPageBoxes;
        } break;
        case CmdToggleImages: {
            isToggle = true;
            newIsOn = !ShowImageOutlines();
        } break;
        case CmdToggleTransparencyGrid: {
            isToggle = true;
            newIsOn = !ShowTransparencyGrid();
        } break;
        case CmdDebugShowFitContentArea: {
            isToggle = true;
            newIsOn = !ShowFitContentArea();
        } break;
        case CmdTogglePreservePdfImages: {
            isToggle = true;
            newIsOn = !GetPreservePdfImagesInDarkMode();
        } break;
        case CmdDebugTogglePredictiveRender: {
            isToggle = true;
            newIsOn = !gPredictiveRender;
        } break;
        case CmdToggleEngineeringDrawingEnhance: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !EngineMupdfCadEnhanceActive(dm->GetEngine());
            }
        } break;
    }

    if (isToggle) {
        return str::JoinTemp(s, newIsOn ? StrL(": set to true") : StrL(": set to false"));
    }

    // these cycle through values rather than on and off, so they name what
    // comes next instead of saying set to true / false
    if (cmdId == CmdToggleZoom) {
        WindowTab* tab = win->CurrentTab();
        if (tab && tab->IsDocLoaded()) {
            Str zoomName;
            ZoomToString(&zoomName, tab->NextToggleZoom(), nullptr);
            TempStr res = str::JoinTemp(s, StrL(": switch to "), zoomName);
            str::Free(zoomName);
            return res;
        }
    }

    // the cursor-position tip cycles pt -> mm -> in -> off
    if (cmdId == CmdToggleCursorPosition) {
        Str unit = NextCursorPositionUnitName(win);
        if (len(unit) > 0) {
            return str::JoinTemp(s, StrL(": switch to "), unit);
        }
    }

    if (cmdId == CmdToggleLightDarkTheme) {
        Str target = ToggleLightDarkThemeTargetName();
        if (target) {
            return str::JoinTemp(s, StrL(": switch to "), target);
        }
    }

    return s;
}

static void AppendTab(StrVecCP& tabs, WindowTab* tab, WindowTab* currTab, int& currTabIdx) {
    ItemDataCP data;
    data.tab = tab;
    if (tab->IsAboutTab()) {
        tabs.Append(Tr("Home"), data);
    } else {
        auto name = path::GetBaseNameTemp(tab->filePath);
        if (len(name) == 0) {
            return;
        }
        tabs.Append(name, data);
    }
    if (tab == currTab) {
        currTabIdx = len(tabs) - 1;
    }
}

// orig's CollectTabsRegular / CollectTabsMru, as a list of tabs so the Ctrl+Tab
// switcher can use the same order
void PaletteCollectTabs(MainWindow* win, bool mru, Vec<WindowTab*>& out, int& currTabIdx) {
    VecReset(out);
    currTabIdx = 0;
    WindowTab* currTab = win->CurrentTab();
    if (mru) {
        if (currTab) {
            VecAppend(out, currTab);
        }
        Vec<WindowTab*>* history = win->tabSelectionHistory;
        if (history) {
            for (int i = len(*history) - 1; i >= 0; i--) {
                WindowTab* tab = (*history)[i];
                if (tab != currTab && !VecContains(out, tab)) {
                    VecAppend(out, tab);
                }
            }
        }
    }
    for (MainWindow* w : gWindows) {
        for (WindowTab* tab : w->Tabs()) {
            if (!VecContains(out, tab)) {
                VecAppend(out, tab);
            }
        }
    }
    int idx = VecFind(out, currTab);
    currTabIdx = std::max(idx, 0);
}

static void CollectTabs(MainWindow* mainWin, bool mru) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    wnd->tabs.Reset();
    Vec<WindowTab*> ordered;
    int currIdx = 0;
    PaletteCollectTabs(mainWin, mru, ordered, currIdx);
    WindowTab* currTab = mainWin->CurrentTab();
    wnd->currTabIdx = 0;
    for (WindowTab* tab : ordered) {
        AppendTab(wnd->tabs, tab, currTab, wnd->currTabIdx);
    }
}

static void CollectTocRec(StrVecCP& toc, TocItem* ti, int indent, int currPageNo, int& bestIdx, int& bestPageNo) {
    while (ti) {
        Str title = ti->title ? ti->title : StrL("");
        ItemDataCP data;
        data.tocItem = ti;
        data.indent = indent;
        data.pageNo = ti->pageNo;
        if (len(title) > 0) {
            toc.Append(title, data);
        }
        int pageNo = ti->pageNo;
        if (len(title) > 0 && pageNo > 0 && pageNo <= currPageNo && pageNo > bestPageNo) {
            bestPageNo = pageNo;
            bestIdx = len(toc) - 1;
        }
        if (ti->child) {
            CollectTocRec(toc, ti->child, indent + 1, currPageNo, bestIdx, bestPageNo);
        }
        ti = ti->next;
    }
}

static void CollectToc(MainWindow* mainWin) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    wnd->toc.Reset();
    wnd->currTocIdx = 0;
    if (!mainWin->ctrl) {
        return;
    }
    TocTree* tree = mainWin->ctrl->GetToc();
    if (!tree || !tree->root) {
        return;
    }
    int currPageNo = mainWin->ctrl->CurrentPageNo();
    int bestIdx = 0;
    int bestPageNo = 0;
    CollectTocRec(wnd->toc, tree->root->child, 0, currPageNo, bestIdx, bestPageNo);
    wnd->currTocIdx = bestIdx;
}

static void AppendFavoritesForFile(StrVecCP& favorites, FileState* fs, bool isCurrent) {
    if (!fs || !fs->favorites) {
        return;
    }
    for (Favorite* fav : *fs->favorites) {
        TempStr rn = FavReadableNameTemp(fav);
        TempStr disp;
        if (isCurrent) {
            disp = rn;
        } else {
            TempStr base = path::GetBaseNameTemp(fs->filePath);
            disp = fmt("%s : %s", base, rn);
        }
        if (len(disp) == 0) {
            continue;
        }
        ItemDataCP data;
        data.favFs = fs;
        data.fav = fav;
        favorites.Append(disp, data);
    }
}

static void CollectFavorites(MainWindow* mainWin) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    wnd->favorites.Reset();
    WindowTab* currTab = mainWin->CurrentTab();
    Str currFilePath = currTab ? currTab->filePath : Str();

    FileState* currFs = nullptr;
    if (currFilePath) {
        for (FileState* fs : *gSettings->fileStates) {
            if (str::Eq(fs->filePath, currFilePath)) {
                currFs = fs;
                break;
            }
        }
    }
    if (currFs) {
        AppendFavoritesForFile(wnd->favorites, currFs, true);
    }
    for (FileState* fs : *gSettings->fileStates) {
        if (fs == currFs) {
            continue;
        }
        AppendFavoritesForFile(wnd->favorites, fs, false);
    }
}

static void CollectAnnotations(MainWindow* mainWin) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    wnd->annotations.Reset();
    WindowTab* tab = mainWin ? mainWin->CurrentTab() : nullptr;
    if (!tab) {
        return;
    }
    EngineBase* engine = tab->GetEngine();
    if (!EngineSupportsAnnotations(engine)) {
        return;
    }
    // orig kicks the background loader off when the palette opens, so the list
    // is not limited to the pages that happen to be laid out
    StartLoadingAnnotationsForUi(tab);
    Vec<Annotation*> annots;
    EngineMupdfGetLoadedAnnotations(engine, annots);
    for (Annotation* a : annots) {
        if (!a) {
            continue;
        }
        ItemDataCP data;
        data.annot = a;
        data.pageNo = a->pageNo;
        wnd->annotations.Append(AnnotationListRowTextTemp(a), data);
    }
}

// the scalar settings the palette can edit; arrays and compact structs need the
// advanced settings dialog or the settings file
static bool IsPaletteSettingType(SettingType t) {
    switch (t) {
        case SettingType::Bool:
        case SettingType::Int:
        case SettingType::Float:
        case SettingType::String:
        case SettingType::Color:
            return true;
        default:
            return false;
    }
}

// field.value holds the default: the value itself for Bool/Int, a string
// pointer for Float/String/Color. It is NOT a pointer for Bool/Int, so only
// deref it for the string-backed types.
static TempStr FormatSettingDefaultTemp(SettingType type, intptr_t def) {
    switch (type) {
        case SettingType::Bool:
            return str::DupTemp(def != 0 ? StrL("true") : StrL("false"));
        case SettingType::Int:
            return fmt("%d", (int)def);
        default:
            return str::DupTemp(Str((const char*)def));
    }
}

static TempStr FormatSettingValueTemp(SettingType type, const u8* p) {
    switch (type) {
        case SettingType::Bool:
            return str::DupTemp(*(const bool*)p ? StrL("true") : StrL("false"));
        case SettingType::Int:
            return fmt("%d", *(const int*)p);
        case SettingType::Float:
            return fmt("%g", *(const float*)p);
        default:
            // Color is a ParsedColor whose first member is the text
            return str::DupTemp(*(const Str*)p);
    }
}

static bool SettingDiffersFromDefault(const ItemDataCP* d) {
    if (d->settingType == SettingType::Float) {
        float def = 0;
        str::Parse(Str((const char*)d->settingDefault), "%f", &def);
        return *(const float*)SettingRowPtr(d) != def;
    }
    TempStr val = FormatSettingValueTemp(d->settingType, SettingRowPtr(d));
    return !str::Eq(val, FormatSettingDefaultTemp(d->settingType, d->settingDefault));
}

// one "= settings" row per scalar setting; compact structs and arrays need
// the advanced settings dialog
static void CollectSettingRows(StrVecCP& out) {
    Vec<SettingField> fields;
    CollectSettingFields(fields);
    for (const SettingField& sf : fields) {
        if (!IsPaletteSettingType(sf.field->type) || len(sf.path) == 0) {
            continue;
        }
        ItemDataCP data;
        data.settingType = sf.field->type;
        data.settingOffset = sf.offset;
        data.settingDefault = sf.field->value;
        data.settingComment = sf.comment;
        out.Append(sf.path, data);
    }
}

static void CollectSettings() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    wnd->settings.Reset();
    CollectSettingRows(wnd->settings);
    SortNoCase(&wnd->settings);

    // changed values first, then the rest; both groups stay alphabetical
    StrVecCP ordered;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < len(wnd->settings); i++) {
            bool changed = SettingDiffersFromDefault(wnd->settings.AtData(i));
            if (changed == (pass == 0)) {
                ordered.AppendFrom(&wnd->settings, i);
            }
        }
    }
    wnd->settings = ordered;
}

static void CollectStrings(MainWindow* mainWin) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    AppCommandCtx ctx = NewAppCommandCtx(mainWin);

    CollectTabs(mainWin, wnd->smartTabMode && gSettings->tabsMru);
    CollectToc(mainWin);
    CollectFavorites(mainWin);
    CollectAnnotations(mainWin);
    CollectSettings();

    wnd->fileHistory.Reset();
    for (FileState* fs : *gSettings->fileStates) {
        TempStr s = ConvertPathForDisplayTemp(fs->filePath);
        if (len(s) == 0) {
            continue;
        }
        ItemDataCP data;
        data.filePath = fs->filePath;
        wnd->fileHistory.Append(s, data);
    }

    StrVecCP tempCommands;
    int cmdId = 0;
    for (int i = 0; i < gCommandsCount; i++) {
        Str name = gCommands[i].description;
        cmdId = gCommands[i].id;
        if (!AllowCommand(ctx, (i32)cmdId)) {
            continue;
        }
        ItemDataCP data;
        data.cmdId = (i32)cmdId;
        // test against the English name: a translation may not carry the prefix
        data.isDebug = str::StartsWith(name, StrL("Debug: "));
        auto nameTranslated = trans::GetTranslation(name);
        auto nameUpdated = UpdateCommandNameTemp(mainWin, cmdId, nameTranslated);
        tempCommands.Append(nameUpdated, data);
    }

    // the same command under another wording a user may search for
    for (int i = 0; i < gCommandAltDescsCount; i++) {
        Str name = gCommandAltDescs[i].description;
        cmdId = gCommandAltDescs[i].id;
        if (!AllowCommand(ctx, (i32)cmdId)) {
            continue;
        }
        ItemDataCP data;
        data.cmdId = (i32)cmdId;
        auto nameTranslated = trans::GetTranslation(name);
        auto nameUpdated = UpdateCommandNameTemp(mainWin, cmdId, nameTranslated);
        tempCommands.Append(nameUpdated, data);
    }

    auto* curr = gFirstCustomCommand;
    while (curr) {
        TempStr name = curr->name;
        cmdId = curr->id;
        if (cmdId > 0 && !str::IsEmptyOrWhiteSpace(name)) {
            if (AllowCommand(ctx, cmdId)) {
                ItemDataCP data;
                data.cmdId = cmdId;
                name = RemovePrefixFromString(name);
                tempCommands.Append(name, data);
            }
        }
        curr = curr->next;
    }

    SortNoCase(&tempCommands);
    int n = len(tempCommands);
    wnd->commands.Reset();
    // dev-only commands go last instead of sitting in the middle of the list
    // under "D"; each group keeps its alphabetical order
    for (int pass = 0; pass < 2; pass++) {
        bool wantDebug = (pass == 1);
        for (int i = 0; i < n; i++) {
            if (tempCommands.AtData(i)->isDebug == wantDebug) {
                wnd->commands.AppendFrom(&tempCommands, i);
            }
        }
    }
}

// --- filtering --------------------------------------------------------------

// Return the same effective shortcut text that is painted on the right side
// of a command row, without the menu separator tab.
static TempStr CommandPaletteShortcutTemp(i32 cmdId) {
    if (cmdId == 0) {
        return {};
    }
    TempStr withAccel = AppendAccelKeyToMenuStringTemp(StrL(""), cmdId);
    if (len(withAccel) == 0 || withAccel.s[0] != '\t') {
        return {};
    }
    return Str(withAccel.s + 1, len(withAccel) - 1);
}

static void FilterStrings(StrVecCP& strs, const StrVec& words, StrVecCP& matchedOut) {
    int n = len(strs);
    for (int i = 0; i < n; i++) {
        Str s = strs[i];
        if (len(s) == 0) {
            continue;
        }
        bool matches = FilterMatches(s, words);
        ItemDataCP* data = strs.AtData(i);
        if (!matches && data && data->cmdId != 0) {
            TempStr shortcut = CommandPaletteShortcutTemp(data->cmdId);
            matches = FilterMatches(shortcut, words);
        }
        if (!matches && data && IsSettingRow(data)) {
            TempStr val = FormatSettingValueTemp(data->settingType, SettingRowPtr(data));
            matches = FilterMatches(val, words);
        }
        if (!matches) {
            continue;
        }
        matchedOut.AppendFrom(&strs, i);
    }
}

// "ZoomIncrement = 25" -> path "ZoomIncrement", value "25". False when the
// query is still naming a setting, so the list keeps filtering settings.
static bool SplitSettingValueQuery(Str query, Str& path, Str& value) {
    int at = str::IndexOfChar(query, kPaletteSettingValueSep[0]);
    if (at < 0) {
        return false;
    }
    path = Str(query.s, at);
    value = Str(query.s + at + 1, query.len - at - 1);
    str::TrimWsBoth(path);
    str::TrimWsBoth(value);
    return len(path) > 0;
}

// The setting at a full dotted path, or an unambiguous leaf ("Units" for
// "FixedPageUI.PageGrid.Units") so the name can be typed by hand
static ItemDataCP* FindSetting(Str path, Str& foundPath) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    ItemDataCP* found = nullptr;
    int nLeaf = 0;
    for (int i = 0; i < len(wnd->settings); i++) {
        Str s = wnd->settings[i];
        if (str::EqI(s, path)) {
            found = wnd->settings.AtData(i);
            foundPath = s;
            nLeaf = 1;
            break;
        }
        Str leaf = str::SliceFromCharLast(s, '.');
        if (len(leaf) > 1 && str::EqI(Str(leaf.s + 1, leaf.len - 1), path)) {
            nLeaf++;
            found = wnd->settings.AtData(i);
            foundPath = s;
        }
    }
    if (nLeaf != 1) {
        return nullptr;
    }
    return found;
}

// Rows for the value stage: an enum offers its allowed values, anything else
// offers the one value being typed. Enter on a row applies it (see
// ExecuteCurrentSelection).
static void FillSettingValueRows(Str path, Str value, StrVecCP& out) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    Str foundPath;
    ItemDataCP* found = FindSetting(path, foundPath);
    if (!found || found->settingType == SettingType::Bool) {
        return;
    }
    ItemDataCP data = *found;
    data.settingPath = foundPath;
    const char** enumValues = GetSettingsEnumValues(foundPath);
    if (!enumValues) {
        // clearing a string is meaningful, an empty number is not
        bool isStr = found->settingType != SettingType::Int && found->settingType != SettingType::Float;
        if (len(value) > 0 || isStr) {
            out.Append(value, data);
        }
        return;
    }
    for (const char** v = enumValues; *v; v++) {
        Str s(*v);
        // the empty choice means "unset"; it can't be a row you pick, so leave
        // it to the advanced settings dialog
        if (len(s) == 0 || (len(value) > 0 && !FilterMatches(s, wnd->filterWords))) {
            continue;
        }
        out.Append(s, data);
    }
}

static void FilterStringsForQuery(Str filter, StrVecCP& strings) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    strings.Reset();
    if (len(filter) == 0) {
        filter = StrL("");
    }

    PaletteMode mode = ParsePaletteMode(filter);
    if (mode == PaletteMode::Thumbnails) {
        return;
    }
    bool searchTabs = mode == PaletteMode::Tabs || mode == PaletteMode::Everything;
    bool searchHistory = mode == PaletteMode::FileHistory || mode == PaletteMode::Everything;
    bool searchCommands = mode == PaletteMode::Commands || mode == PaletteMode::Everything;
    bool searchToc = mode == PaletteMode::Toc;
    bool searchFavorites = mode == PaletteMode::Favorites;
    bool searchAnnotations = mode == PaletteMode::Annotations;
    bool searchSettings = mode == PaletteMode::Settings;

    wnd->filterWords.Reset();
    if (searchSettings) {
        Str path, value;
        if (SplitSettingValueQuery(filter, path, value)) {
            SplitFilterToWords(value, wnd->filterWords);
            FillSettingValueRows(path, value, strings);
            // the rows are the values themselves: nothing to highlight
            wnd->filterWords.Reset();
            return;
        }
    }
    if (searchAnnotations) {
        AnnotMatchOpts opts;
        if (!ParseAnnotSearch(filter, opts)) {
            opts.Reset();
            StrVec words;
            SplitFilterToWords(filter, words);
            for (Str w : words) {
                AnnotSearchAddContentWord(opts, w);
            }
        }
        AnnotSearchContentWords(opts, wnd->filterWords);
        int n = len(wnd->annotations);
        for (int i = 0; i < n; i++) {
            ItemDataCP* data = wnd->annotations.AtData(i);
            if (data && AnnotMatches(data->annot, opts)) {
                strings.AppendFrom(&wnd->annotations, i);
            }
        }
        return;
    }

    SplitFilterToWords(filter, wnd->filterWords);

    if (searchTabs) {
        FilterStrings(wnd->tabs, wnd->filterWords, strings);
    }
    if (searchHistory) {
        FilterStrings(wnd->fileHistory, wnd->filterWords, strings);
    }
    if (searchCommands) {
        FilterStrings(wnd->commands, wnd->filterWords, strings);
    }
    if (searchToc) {
        FilterStrings(wnd->toc, wnd->filterWords, strings);
    }
    if (searchFavorites) {
        FilterStrings(wnd->favorites, wnd->filterWords, strings);
    }
    if (searchSettings) {
        FilterStrings(wnd->settings, wnd->filterWords, strings);
    }
}

// the document's annotations changed while the palette is up: re-collect them
// and re-filter, but only when the query is still in annotation mode
void CommandPaletteOnAnnotationsChanged() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->win) {
        return;
    }
    CollectAnnotations(wnd->win);
    TempStr filter = CommandPaletteSkipWS(QueryTemp());
    if (!str::StartsWith(filter, Str(kPalettePrefixAnnotations))) {
        return;
    }
    FilterStringsForQuery(filter, wnd->items);
    AppShellInvalidate(wnd->win);
}

// ng: ReloadSettings() freed the FileState / Favorite objects the favorites and
// file history rows point at: collect them again from the new gSettings
void CommandPaletteOnSettingsReloaded() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible || !IsMainWindowValid(wnd->win)) {
        return;
    }
    int currSel = wnd->sel;
    CollectStrings(wnd->win);
    FilterStringsForQuery(CommandPaletteSkipWS(QueryTemp()), wnd->items);
    int n = len(wnd->items);
    SetCurrentSelection(n == 0 ? -1 : std::min(currSel, n - 1));
    AppShellInvalidate(wnd->win);
}

#if OS_WIN
// Tests replace the query with WM_SETTEXT on the frame. The box is a gpui
// input, so the message has to land here.
bool CommandPaletteSetText(MainWindow* win, const WCHAR* text) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible || wnd->win != win || !wnd->editQuery || !text) {
        return false;
    }
    gp::InputSetValue(wnd->editQuery, ToGpui(ToUtf8Temp(text)));
    QueryChanged();
    AppShellInvalidate(win);
    return true;
}
#endif

static void QueryChanged() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    TempStr filter = CommandPaletteSkipWS(QueryTemp());
    if (wnd->win->AsFixed() && str::StartsWith(filter, Str(kPalettePrefixThumbnails))) {
        SetThumbnailMode(true);
        AppShellInvalidate(wnd->win);
        return;
    }
    SetThumbnailMode(false);
    int currSelIdx = 0;
    int nItemsPrev = len(wnd->items);
    if (wnd->smartTabMode && !wnd->stickyMode && len(filter) > 1) {
        wnd->stickyMode = true;
        currSelIdx = wnd->sel;
    }
    FilterStringsForQuery(filter, wnd->items);
    int nItems = len(wnd->items);
    if (nItems == 0) {
        SetCurrentSelection(-1);
        return;
    }
    if (wnd->stickyMode && nItemsPrev == nItems) {
        SetCurrentSelection(currSelIdx);
        return;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixTOC)) && len(wnd->filterWords) == 0) {
        int idx = (wnd->currTocIdx >= 0 && wnd->currTocIdx < nItems) ? wnd->currTocIdx : 0;
        SetCurrentSelection(idx);
        return;
    }
    SetCurrentSelection(0);
}

// --- opening ----------------------------------------------------------------

static void PaletteOpenToolWindow(MainWindow* win, CommandPaletteWnd* wnd);

void RunCommandPalette(MainWindow* win, Str prefix, int smartTabAdvance) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    CloseCommandPalette();
    auto* wnd = new CommandPaletteWnd();
    gCommandPaletteWnd = wnd;
    wnd->win = win;
    wnd->visible = true;
    wnd->cursorPos = win->dragPrevPos;
    wnd->smartTabMode = str::Eq(prefix, Str(kPalettePrefixTabs)) && smartTabAdvance != 0;
    wnd->tocMode = str::Eq(prefix, Str(kPalettePrefixTOC));

    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gp::InputSetPlaceholder(s, GStrL("enter search term"));
    gp::InputSetValue(s, ToGpui(prefix));
    wnd->editQuery = s;
    wnd->wantFocus = true;

    CollectStrings(win);
    FilterStringsForQuery(prefix, wnd->items);
    if (str::StartsWith(prefix, Str(kPalettePrefixThumbnails))) {
        SetThumbnailMode(true);
    }

    int nItems = len(wnd->items);
    wnd->sel = nItems > 0 ? 0 : -1;
    if (wnd->smartTabMode && nItems > 0) {
        wnd->sel = (wnd->currTabIdx + nItems + smartTabAdvance) % nItems;
    } else if (wnd->tocMode && wnd->currTocIdx >= 0 && wnd->currTocIdx < nItems) {
        wnd->sel = wnd->currTocIdx;
    }
    logf("RunCommandPalette: prefix '%s', %d items, selected %d\n", prefix, nItems, wnd->sel);
    PaletteOpenToolWindow(win, wnd);
    AppShellInvalidate(win);
}

// --- the view ---------------------------------------------------------------

void PaletteView::OnInput(PaletteView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (!gCommandPaletteWnd) {
        return;
    }
    if (ev->kind == gp::InputEventKind::PressEnter) {
        ExecuteCurrentSelection();
        gp::Notify(cx);
        return;
    }
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    QueryChanged();
    gp::Notify(cx);
}

void PaletteView::OnRowClick(PaletteView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    if (!gCommandPaletteWnd) {
        return;
    }
    SetCurrentSelection((int)idx);
    if (ev->clickCount >= 2) {
        ExecuteCurrentSelection();
    }
    gp::Notify(cx);
}

static const char* gSwitchPrefixes[] = {
    kPalettePrefixCommands, kPalettePrefixTabs,       kPalettePrefixFileHistory, kPalettePrefixFavorites,
    kPalettePrefixTOC,      kPalettePrefixThumbnails, kPalettePrefixAnnotations, kPalettePrefixBoolSettings,
};

void PaletteView::OnSwitchClick(PaletteView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t prefixIdx) {
    if (!gCommandPaletteWnd || prefixIdx < 0 || prefixIdx >= dimofi(gSwitchPrefixes)) {
        return;
    }
    SwitchToPrefix(Str(gSwitchPrefixes[prefixIdx]));
    gp::Notify(cx);
}

void PaletteView::OnThumbClick(PaletteView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t pageNo) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd) {
        return;
    }
    ThumbSelectPage((int)pageNo);
    if (ev->clickCount >= 2) {
        OpenSelectedThumbnailPage();
        return;
    }
    gp::Notify(cx);
}

// orig's OnThumbMouseMove: in the palette the thumbnail under the mouse is the
// selected one
void PaletteView::OnThumbMove(PaletteView*, gp::Ctx* cx, const gp::MouseMoveEvent*, int64_t pageNo) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->thumbnailMode || wnd->selectedPage == (int)pageNo) {
        return;
    }
    ThumbSelectPage((int)pageNo);
    gp::Notify(cx);
}

void PaletteView::OnThumbScroll(PaletteView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd) {
        return;
    }
    wnd->thumbScrollY = ev->offsetY;
    StartThumbnailRendering();
    AppShellInvalidate(wnd->win);
    gp::Notify(cx);
}

static TempStr RightColumnTemp(ItemDataCP* data) {
    if (data->cmdId != 0) {
        return CommandPaletteShortcutTemp(data->cmdId);
    }
    if (IsSettingRow(data) && len(data->settingPath) == 0) {
        return FormatSettingValueTemp(data->settingType, SettingRowPtr(data));
    }
    if (data->annot) {
        return fmt("p%d", data->pageNo);
    }
    if (data->pageNo > 0) {
        // toc entry: show the destination page number on the right, e.g. "p33"
        return fmt("p%d", data->pageNo);
    }
    if (data->tocItem && data->tocItem->loc.chapter >= 1) {
        // chaptered doc: destination unresolved until clicked, show the chapter
        return fmt("ch%d", data->tocItem->loc.chapter);
    }
    if (data->filePath) {
        return path::GetDirTemp(data->filePath);
    }
    return {};
}

static gp::El* BuildList(CommandPaletteWnd* wnd, gp::Ctx* cx, float listDy) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    // keep the selected row visible; gpui has no EnsureVisible for a plain box
    int visibleRows = std::max((int)(listDy / kPaletteRowDy), 1);
    if (wnd->sel >= 0) {
        float selTop = (float)wnd->sel * kPaletteRowDy;
        float selBottom = selTop + kPaletteRowDy;
        if (selTop < wnd->scrollOff) {
            wnd->scrollOff = selTop;
        } else if (selBottom > wnd->scrollOff + listDy) {
            wnd->scrollOff = selBottom - listDy;
        }
    }
    float maxOff = std::max(0.f, (float)len(wnd->items) * kPaletteRowDy - listDy);
    wnd->scrollOff = limitValue(wnd->scrollOff, 0.f, maxOff);
    (void)visibleRows;

    gp::El* list = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->H(listDy)->ScrollY(wnd->scrollOff);
    int n = len(wnd->items);
    for (int i = 0; i < n; i++) {
        ItemDataCP* data = wnd->items.AtData(i);
        Str itemText = wnd->items[i];
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kPaletteRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(6)
                          ->PathClick(GpuiDup(cx->a, fmt("palette-row-%d", i)))
                          ->OnClick(gp::ListenTo(gPaletteView, &PaletteView::OnRowClick, (intptr_t)i));
        if (i == wnd->sel) {
            row->Bg(th.selection);
        }
        if (data->indent > 0) {
            row->Child(gp::Div(cx->a)->W((float)(data->indent * 16))->Shrink0());
        }
        gp::El* left = gp::Div(cx->a)->FlexRow()->Flex1()->MinW(0)->ItemsCenter()->ClipX();
        left->Child(FilterHighlightText(cx, itemText, wnd->filterWords, th.foreground, 13));
        if (data->annot) {
            Str contents = Contents(data->annot);
            if (len(contents) > 0) {
                left->Child(gp::TextEl(cx->a, GpuiDup(cx->a, str::JoinTemp(StrL("  "), contents)))
                                ->Font(13)
                                ->Fg(th.mutedFg)
                                ->Truncate());
            }
        }
        row->Child(left);
        TempStr right = RightColumnTemp(data);
        if (len(right) > 0) {
            bool emphasized = IsSettingRow(data) && len(data->settingPath) == 0 && SettingDiffersFromDefault(data);
            gp::Rgba rightFg = emphasized ? th.foreground : th.mutedFg;
            int boldLen = emphasized ? len(right) : 0;
            gp::El* rightEl = FilterHighlightText(cx, right, wnd->filterWords, rightFg, 12, 0, boldLen)->Shrink0();
            row->Child(gp::Div(cx->a)->W(8)->Shrink0());
            row->Child(rightEl);
        }
        list->Child(row);
    }
    if (n == 0) {
        list->Child(gp::Div(cx->a)
                        ->W(gp::kFill)
                        ->H(kPaletteRowDy)
                        ->PadX(6)
                        ->Child(gp::TextEl(cx->a, ToGpui(Tr("No result found")))->Font(13)->Fg(th.mutedFg)));
    }
    return list;
}

// orig's PageThumbnailsCtrl::SetBounds + DrawRow for ThumbnailsHost::Palette
static gp::El* BuildThumbnails(CommandPaletteWnd* wnd, gp::Ctx* cx, float listDx, float listDy) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    int availableDx = (int)listDx - (2 * kThumbPadding) - kThumbScrollbarDx;
    int cols = limitValue((availableDx + kThumbGap) / (kThumbDx + kThumbGap), 1, kThumbMaxCols);
    bool relayout = cols != wnd->thumbCols || listDy != wnd->thumbViewDy;
    wnd->thumbCols = cols;
    wnd->thumbViewDy = listDy;
    int rows = ThumbRows(wnd);

    // the palette spreads the rows over its height
    int visibleRows = std::max(1, ((int)listDy - kThumbGap) / (kThumbDy + kThumbGap));
    visibleRows = std::min(visibleRows, rows);
    float rowGap = (float)kThumbGap;
    if (visibleRows > 0) {
        float freeDy = listDy - (float)(visibleRows * kThumbDy);
        rowGap = std::max(rowGap, (float)(int)(freeDy / (float)(visibleRows + 1)));
    }
    float itemDy = (float)kThumbDy + rowGap;
    wnd->thumbItemDy = itemDy;
    if (wnd->thumbReveal || relayout) {
        ThumbEnsureVisible(wnd);
        wnd->thumbReveal = false;
    }
    wnd->thumbScrollY = limitValue(wnd->thumbScrollY, 0.f, ThumbMaxScrollY(wnd));
    if (relayout) {
        StartThumbnailRendering();
    }

    gp::El* grid = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    ThumbnailCache* cache = wnd->thumbCache;
    if (!cache) {
        return grid->H(listDy);
    }
    // ng: only the rows in view are built, between two spacers that stand in
    // for the rest. Row i is at rowGap + i * itemDy, as orig's padding.top
    int firstRow = std::max((int)(wnd->thumbScrollY / itemDy) - kThumbOverscanRows, 0);
    int endRow = std::min((int)((wnd->thumbScrollY + listDy) / itemDy) + 1 + kThumbOverscanRows, rows);
    grid->Child(gp::Div(cx->a)->W(gp::kFill)->H(rowGap + (float)firstRow * itemDy)->Shrink0());
    for (int r = firstRow; r < endRow; r++) {
        gp::El* rowEl = gp::Div(cx->a)
                            ->FlexRow()
                            ->W(gp::kFill)
                            ->H(itemDy)
                            ->Gap((float)kThumbGap)
                            ->JustifyCenter()
                            ->ItemsStart()
                            ->Shrink0();
        for (int c = 0; c < cols; c++) {
            int pageNo = (r * cols) + c + 1;
            if (pageNo > wnd->pageCount) {
                // keeps a short last row aligned with the columns above it
                rowEl->Child(gp::Div(cx->a)->W((float)kThumbDx)->H((float)kThumbDy)->Shrink0());
                continue;
            }
            PaletteThumb* t = &cache->thumbs[pageNo - 1];
            gp::El* cell = gp::Div(cx->a)
                               ->FlexCol()
                               ->W((float)kThumbDx)
                               ->H((float)kThumbDy)
                               ->Shrink0()
                               ->ItemsCenter()
                               ->JustifyCenter()
                               ->Bg(gp::Rgba{0xff, 0xff, 0xff, 0xff})
                               ->PathClick(GpuiDup(cx->a, fmt("palette-thumb-%d", pageNo)))
                               ->OnClick(gp::ListenTo(gPaletteView, &PaletteView::OnThumbClick, (intptr_t)pageNo))
                               ->OnMouseMove(gp::ListenTo(gPaletteView, &PaletteView::OnThumbMove, (intptr_t)pageNo));
            if (t->bitmap) {
                gp::ImageSource src = gp::ImageSource::FromCustom(ThumbLoad, t);
                cell->Child(gp::ImageEl(cx->a, src, GStrL(""))->SizeFull()->ObjectFitMode(gp::ObjectFit::Contain));
            }
            if (pageNo == wnd->selectedPage) {
                // over the image, as orig's DrawRect after DrawPixmap
                gp::El* frame = gp::Div(cx->a)->SizeFull()->Border(3, gp::Rgba{0, 120, 215, 0xff});
                frame->Absolute()->Left(0)->Top(0);
                cell->Child(frame);
            }
            // a pill inside the thumbnail, at its bottom
            gp::El* label = gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d", pageNo)))
                                ->Font(12)
                                ->Fg(th.foreground)
                                ->Bg(th.tokens.background)
                                ->Radius(999)
                                ->PadX((float)kThumbLabelPadX)
                                ->PadY((float)kThumbLabelPadY);
            label->Absolute()->Bottom((float)kThumbLabelInset);
            cell->Child(label);
            rowEl->Child(cell);
        }
        grid->Child(rowEl);
    }
    // the content is as tall as orig's scroll range: rows * itemDy - UsableDy
    float bottomDy = (float)(rows - endRow) * itemDy + (listDy - ThumbUsableDy(wnd)) - rowGap;
    if (bottomDy > 0) {
        grid->Child(gp::Div(cx->a)->W(gp::kFill)->H(bottomDy)->Shrink0());
    }
    return gp::Div(cx->a)
        ->Id(GStrL("palette-thumbs"))
        ->FlexCol()
        ->W(gp::kFill)
        ->H(listDy)
        ->Shrink0()
        ->ScrollY(wnd->thumbScrollY)
        ->ScrollFromPath()
        ->OnScroll(gp::ListenTo(gPaletteView, &PaletteView::OnThumbScroll))
        ->Child(grid);
}

// the "# History" / "> Commands" switches in the top row
static gp::El* BuildSwitchRow(CommandPaletteWnd* wnd, gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* row = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->JustifyCenter()->ItemsCenter()->Gap(4)->Shrink0();
    auto addSwitch = [&](Str label, int prefixIdx) {
        gp::El* el = gp::Div(cx->a)
                         ->FlexRow()
                         ->ItemsCenter()
                         ->PadX(8)
                         ->H(20)
                         ->Radius(4)
                         ->Cursor(gp::CursorKind::Pointer)
                         ->PathClick(GpuiDup(cx->a, fmt("palette-switch-%d", prefixIdx)))
                         ->OnClick(gp::ListenTo(gPaletteView, &PaletteView::OnSwitchClick, (intptr_t)prefixIdx));
        // the leading character is the prefix: draw it as a key cap, as orig does
        el->Child(gp::TextEl(cx->a, GpuiDup(cx->a, Str(label.s, 1)))
                      ->Font(12)
                      ->Fg(th.foreground)
                      ->Bg(th.tokens.muted)
                      ->Radius(3)
                      ->PadX(4));
        el->Child(gp::TextEl(cx->a, GpuiDup(cx->a, Str(label.s + 1, len(label) - 1)))->Font(12)->Fg(th.mutedFg));
        row->Child(el);
    };
    addSwitch(Tr("> Commands"), 0);
    addSwitch(Tr("@ Tabs"), 1);
    addSwitch(Tr("# History"), 2);
    if (len(wnd->favorites) > 0) {
        addSwitch(Tr("$ Favorites"), 3);
    }
    if (len(wnd->toc) > 0) {
        addSwitch(Tr("% TOC"), 4);
    }
    if (wnd->win->AsFixed()) {
        addSwitch(Tr("& Thumbnails"), 5);
    }
    if (len(wnd->annotations) > 0) {
        addSwitch(Tr("* Annotations"), 6);
    }
    addSwitch(Tr("= Settings"), 7);
    return row;
}

enum {
    kHelpSmartTab,
    kHelpCommands,
    kHelpHistory,
    kHelpTabs,
    kHelpFavorites,
    kHelpAnnotations,
    kHelpSettings,
    kHelpSettingValue,
    kHelpToc,
    kHelpEverything,
    kHelpThumbnails,
};

static int PaletteHelpKind(Str filter, bool smartTab) {
    if (smartTab) {
        return kHelpSmartTab;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixEverything))) {
        return kHelpEverything;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixTabs))) {
        return kHelpTabs;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixFileHistory))) {
        return kHelpHistory;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixTOC))) {
        return kHelpToc;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixFavorites))) {
        return kHelpFavorites;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixAnnotations))) {
        return kHelpAnnotations;
    }
    if (str::TrimPrefix(filter, Str(kPalettePrefixBoolSettings))) {
        Str path, value;
        return SplitSettingValueQuery(filter, path, value) ? kHelpSettingValue : kHelpSettings;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixThumbnails))) {
        return kHelpThumbnails;
    }
    return kHelpCommands;
}

static gp::El* BuildHelpRow(CommandPaletteWnd* wnd, gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    TempStr filter = CommandPaletteSkipWS(QueryTemp());
    int kind = PaletteHelpKind(filter, wnd->smartTabMode);
    Str strings[4];
    int nHelp = 0;
    switch (kind) {
        case kHelpSmartTab:
            strings[nHelp++] = Tr("Ctrl+Tab navigate");
            strings[nHelp++] = Tr("Release Ctrl select");
            strings[nHelp++] = Tr("Space for sticky mode");
            strings[nHelp++] = Tr("Del close tab");
            break;
        case kHelpHistory:
            strings[nHelp++] = Tr("Enter open file");
            strings[nHelp++] = Tr("Del remove from history");
            strings[nHelp++] = Tr("Esc close");
            break;
        case kHelpTabs:
            strings[nHelp++] = Tr("Enter switch to tab");
            strings[nHelp++] = Tr("Del close tab");
            strings[nHelp++] = Tr("Esc close");
            break;
        case kHelpFavorites:
            strings[nHelp++] = Tr("Enter go to favorite");
            strings[nHelp++] = Tr("Del remove favorite");
            strings[nHelp++] = Tr("Esc close");
            break;
        case kHelpAnnotations:
        case kHelpThumbnails:
        case kHelpToc:
            strings[nHelp++] = Tr("Enter go to");
            strings[nHelp++] = Tr("Esc close");
            break;
        case kHelpSettings:
            strings[nHelp++] = Tr("Enter change");
            strings[nHelp++] = Tr("Esc close");
            break;
        case kHelpSettingValue:
            strings[nHelp++] = Tr("Enter apply");
            strings[nHelp++] = Tr("Esc go back");
            break;
        case kHelpEverything:
            strings[nHelp++] = Tr("Enter select");
            strings[nHelp++] = Tr("Esc close");
            break;
        default:
            strings[nHelp++] = Tr("Enter run command");
            strings[nHelp++] = Tr("Esc close");
            break;
    }
    gp::El* row = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->JustifyCenter()->ItemsCenter()->Gap(12)->Shrink0();
    for (int i = 0; i < nHelp; i++) {
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, strings[i]))->Font(11)->Fg(th.mutedFg));
    }
    return row;
}

// orig's VirtFixedLinesText: a fixed number of lines, so the list doesn't
// jump as comments of different lengths come and go
constexpr float kSettingHelpDy = 84;
constexpr float kPaletteGapDy = 6;

static bool ShowsSettingHelp(CommandPaletteWnd* wnd) {
    TempStr filter = CommandPaletteSkipWS(QueryTemp());
    int kind = PaletteHelpKind(filter, wnd->smartTabMode);
    return kind == kHelpSettings || kind == kHelpSettingValue;
}

// the doc comment of the selected setting, like the advanced settings dialog
static gp::El* BuildSettingHelp(CommandPaletteWnd* wnd, gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    Str comment;
    int idx = wnd->sel;
    if (idx >= 0 && idx < len(wnd->items)) {
        comment = wnd->items.AtData(idx)->settingComment;
    }
    return gp::Div(cx->a)
        ->W(gp::kFill)
        ->H(kSettingHelpDy)
        ->Shrink0()
        ->Pad(6)
        ->Border(1, th.border)
        ->ClipY()
        ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, comment))->Font(12)->Fg(th.foreground)->Wrap()->W(gp::kFill));
}

static RectF PaletteBounds(MainWindow* win, CommandPaletteWnd* wnd) {
    // orig sizes the palette to the frame: 640..1024 wide, at least 480 tall,
    // and puts it 42 px below the frame's top edge
    float dx = (float)limitValue(win->frameRc.dx - 256, kPaletteMinDx, kPaletteMaxDx);
    dx = std::min(dx, (float)win->frameRc.dx - 32);
    float dy = std::max((float)win->frameRc.dy - kPaletteTopMargin - 24, 160.f);
    if (wnd->smartTabMode) {
        // size the window to the number of tabs instead of using a fixed height
        dy = std::min(dy, (float)len(wnd->items) * kPaletteRowDy + 100);
    }
    return RectF(((float)win->frameRc.dx - dx) / 2, kPaletteTopMargin, dx, dy);
}

static gp::El* PaletteCard(CommandPaletteWnd* wnd, gp::Ctx* cx, RectF bounds);

bool CommandPaletteOnMouseDown(MainWindow* win, float x, float y) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    // in a window of its own a click in the frame deactivates it
    if (wnd && wnd->tw) {
        return false;
    }
    if (!wnd || !wnd->visible || wnd->win != win || PaletteBounds(win, wnd).Contains(PointF{x, y})) {
        return false;
    }
    CloseCommandPalette();
    return true;
}

// --- a window of its own (Windows) ------------------------------------------

// orig's size: the frame's client width less 256, between 640 and 1024 - wider
// than a narrow frame -, and its height less 72, at least 480
static Size PaletteWindowSize(MainWindow* win, CommandPaletteWnd* wnd) {
    constexpr int kPaletteFrameDx = 256;
    constexpr int kPaletteFrameDy = 72;
    constexpr int kPaletteMinDy = 480;
    int dx = limitValue(win->frameRc.dx - kPaletteFrameDx, kPaletteMinDx, kPaletteMaxDx);
    int dy = std::max(win->frameRc.dy - kPaletteFrameDy, kPaletteMinDy);
    if (wnd->smartTabMode) {
        dy = std::min(dy, (int)((float)len(wnd->items) * kPaletteRowDy) + 100);
    }
    return Size(dx, dy);
}

#if OS_WIN
constexpr UINT_PTR kPaletteQuerySubclassId = 7;

// WM_SETTEXT and the keys tests post at the query Edit. The box they see is
// this control; the text they filter is the gpui input.
static LRESULT CALLBACK PaletteQueryProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dw) {
    auto* win = (MainWindow*)dw;
    if (msg == WM_SETTEXT && lp) {
        CommandPaletteSetText(win, (const WCHAR*)lp);
    }
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (CommandPaletteOnKeyDown(win, (int)wp, ctrl, shift)) {
            return 0;
        }
    }
    if (msg == WM_KEYUP) {
        CommandPaletteOnKeyUp(win, (int)wp);
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, PaletteQueryProc, kPaletteQuerySubclassId);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void PaletteAttachQueryEdit(CommandPaletteWnd* wnd) {
    if (!wnd || wnd->queryEdit) {
        return;
    }
    HWND parent = wnd->tw ? ToolWindowHwnd(wnd->tw) : nullptr;
    if (!parent) {
        return;
    }
    HWND edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 1, 1, parent, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (!edit) {
        return;
    }
    SetWindowSubclass(edit, PaletteQueryProc, kPaletteQuerySubclassId, (DWORD_PTR)wnd->win);
    wnd->queryEdit = edit;
}

static void PaletteFocusQueryEdit(CommandPaletteWnd* wnd) {
    if (!wnd || !wnd->queryEdit || GetFocus() == wnd->queryEdit) {
        return;
    }
    SetFocus(wnd->queryEdit);
}
#endif

static gp::El* PaletteToolBuild(MainWindow* win, gp::Ctx* cx) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible || wnd->win != win || !wnd->tw || ToolWindowGpui(wnd->tw) != cx->win) {
        return nullptr;
    }
    // orig's WA_INACTIVE: the palette goes when another window is activated
    bool active = gp::WindowIsActive(cx);
    if (wnd->wasActive && !active) {
        CloseCommandPalette();
        return nullptr;
    }
    wnd->wasActive = active;
    gp::WinSize ws = gp::WindowSize(cx->win);
    gp::El* card = PaletteCard(wnd, cx, RectF(0, 0, ws.dipW, ws.dipH));
#if OS_WIN
    PaletteAttachQueryEdit(wnd);
    PaletteFocusQueryEdit(wnd);
#endif
    return card;
}

static bool PaletteToolOnKey(MainWindow* win, gp::Ctx*, const gp::KeyEvent* ev) {
    return CommandPaletteOnKeyDown(win, (int)ev->vk, ev->ctrl, ev->shift);
}

// the smart tab switcher picks its tab when Ctrl goes up
static void PaletteToolOnKeyUp(MainWindow* win, gp::Ctx*, const gp::KeyEvent* ev) {
    CommandPaletteOnKeyUp(win, (int)ev->vk);
}

// the owner closed it, or Alt + F4
static void PaletteToolOnClosed(MainWindow*) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (wnd && wnd->tw && !ToolWindowIsLive(wnd->tw)) {
        wnd->tw = nullptr;
        if (!IsMainWindowValid(wnd->win)) {
            wnd->win = nullptr;
        }
        CloseCommandPalette();
    }
}

static Str PaletteToolTitle() {
    return Tr("Command Palette");
}

// orig's window: WS_POPUPWINDOW, WS_EX_TOOLWINDOW, owned by the frame;
// centered on the frame's width and shifted into the work area, 42 under the
// frame's top (PositionCommandPalette)
static void PaletteOpenToolWindow(MainWindow* win, CommandPaletteWnd* wnd) {
    if (!ToolWindowsAvailable()) {
        return;
    }
    ToolWindowDesc desc;
    desc.name = "palette";
    desc.title = PaletteToolTitle;
    desc.frame = ToolWinFrame::None;
    desc.style = ToolWinStyle::Tool;
    desc.owner = ToolWinOwner::Owned;
    desc.build = PaletteToolBuild;
    desc.onKey = PaletteToolOnKey;
    desc.onCaptureKey = PaletteToolOnKey;
    desc.onKeyUp = PaletteToolOnKeyUp;
    desc.onClosed = PaletteToolOnClosed;
    Size client = PaletteWindowSize(win, wnd);
    Size outer = ToolWindowOuterSize(desc, win, client);
    // orig's WS_BORDER: the card's own border is drawn in its place
    constexpr int kPaletteBorder = 1;
    outer.dx += 2 * kPaletteBorder;
    outer.dy += 2 * kPaletteBorder;
    Rect r = ToolWindowCenteredOuter(win, outer);
    Rect frame = AppShellWindowScreenRect(win);
    if (!frame.IsEmpty()) {
        r.y = frame.y + (int)kPaletteTopMargin;
    }
    wnd->tw = ToolWindowOpen(desc, win, r);
}

gpui::Window* CommandPaletteInputWindow(MainWindow* win) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible || wnd->win != win || !wnd->tw || !ToolWindowIsLive(wnd->tw)) {
        return nullptr;
    }
    return ToolWindowGpui(wnd->tw);
}

gp::El* CommandPaletteBuild(MainWindow* win, gp::Ctx* cx) {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->visible || wnd->win != win || wnd->tw) {
        return nullptr;
    }
    bool active = gp::WindowIsActive(cx);
    if (wnd->wasActive && !active) {
        CloseCommandPalette();
        return nullptr;
    }
    wnd->wasActive = active;
    return PaletteCard(wnd, cx, PaletteBounds(win, wnd));
}

// the palette itself, `bounds` in the window it is drawn in
static gp::El* PaletteCard(CommandPaletteWnd* wnd, gp::Ctx* cx, RectF bounds) {
    if (!gPaletteView.IsValid()) {
        gPaletteView = gp::EntityNewState<PaletteView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    wnd->editQuery->onChange = gp::ListenTo(gPaletteView, &PaletteView::OnInput);

    gp::El* card = gp::Div(cx->a)
                       ->FlexCol()
                       ->W(bounds.dx)
                       ->H(bounds.dy)
                       ->Gap(6)
                       ->Pad(8)
                       ->Bg(th.tokens.popover)
                       ->Border(1, th.border);
    card->Child(gpc::Input::New(cx, GStrL("palette-query"), wnd->editQuery)
                    ->WithSize(gp::UiSize::Small)
                    ->W(gp::kFill)
                    ->IntoEl());
    if (!wnd->smartTabMode) {
        card->Child(BuildSwitchRow(wnd, cx));
    }
    float listDy = bounds.dy - (wnd->smartTabMode ? 70.f : 96.f);
    bool settingHelp = !wnd->thumbnailMode && ShowsSettingHelp(wnd);
    if (settingHelp) {
        listDy -= kSettingHelpDy + kPaletteGapDy;
    }
    listDy = std::max(listDy, kPaletteRowDy);
    if (wnd->thumbnailMode) {
        card->Child(BuildThumbnails(wnd, cx, bounds.dx - 16, listDy));
    } else {
        card->Child(BuildList(wnd, cx, listDy));
    }
    if (settingHelp) {
        card->Child(BuildSettingHelp(wnd, cx));
    }
    card->Child(BuildHelpRow(wnd, cx));

    card->Absolute()->Left(bounds.x)->Top(bounds.y);
    if (wnd->wantFocus) {
        gp::InputFocus(wnd->editQuery, cx->app, cx->win);
        wnd->wantFocus = cx->win->input != wnd->editQuery;
    }
    return card;
}
