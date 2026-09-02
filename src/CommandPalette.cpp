/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Theme.h"
#include "WindowTab.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "TableOfContents.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "Menu.h"
#include "Translations.h"
#include "CommandPaletteInternal.h"
#include "CommandPalette.h"

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
    CmdPdShowInfo,
    CmdDocumentShowOutline,
    CmdListPrinters,
    CmdCropImage,
    CmdResizeImage,
    CmdConvertImageToPdf,
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

// UI language (and the debug RTL toggle), not the palette hwnd: that window
// stays LTR so virtual-control coords and clicks are not mirrored (#5956).
bool CommandPaletteUiRtl() {
    return IsUIRtl();
}

Str CommandPaletteSkipWS(Str s) {
    if (!s.s) {
        return {};
    }
    str::SkipWs(s);
    return s;
}

CommandPaletteWnd* gCommandPaletteWnd = nullptr;
static HWND gHwndToActivateOnClose = nullptr;
static WindowTab* gTabToSelectOnClose = nullptr;
static i32 gCmdIdToExecOnClose = 0;
static FileState* gFavFsToGoToOnClose = nullptr;
static Favorite* gFavToGoToOnClose = nullptr;

constexpr int kPaletteThumbnailDx = 120;
constexpr int kPaletteThumbnailDy = 170;
constexpr int kPaletteThumbnailGap = 16;
constexpr int kPaletteThumbnailPadding = 16;
constexpr int kPaletteThumbnailMaxCols = 6;
constexpr int kPaletteThumbnailRenderScreens = 1;
constexpr int kPaletteThumbnailKeepScreens = 2;

static Pixmap* const kThumbnailRenderFailed = (Pixmap*)(intptr_t)-1;

struct ThumbnailPaletteCtrl;

struct ThumbnailPaletteCache {
    Vec<Pixmap*> thumbnails;
    ThumbnailPaletteCtrl* ctrl = nullptr;
    EngineBase* renderEngine = nullptr;
    AtomicInt cancelRendering = 0;
    int rotation = 0;
    int thumbDx = 0;
    int thumbDy = 0;
    bool workerRunning = false;
    bool deleteWhenWorkerFinishes = false;
};

struct ThumbnailRowsModel : ListBoxModel {
    int rows = 0;

    int ItemsCount() override { return rows; }
    Str Item(int) override { return {}; }
};

struct ThumbnailRenderTask {
    ThumbnailPaletteCache* cache = nullptr;
    int pageNo = 0;
    Pixmap* bitmap = nullptr;
};

struct ThumbnailRenderWorker {
    ThumbnailPaletteCache* cache = nullptr;
    EngineBase* sourceEngine = nullptr;
    Vec<int> pages;
    int rotation = 0;
    int thumbDx = 0;
    int thumbDy = 0;
};

struct ThumbnailPaletteCtrl : VirtListBox {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    ThumbnailRowsModel* rowsModel = nullptr;
    ThumbnailPaletteCache* cache = nullptr;
    int pageCount = 0;
    int selectedPage = 1;
    int cols = 1;
    int thumbDx = 0;
    int thumbDy = 0;
    int gap = 0;
    int labelDy = 0;
    bool active = false;

    ThumbnailPaletteCtrl(MainWindow*, PlatformFont*, int dpi);
    ~ThumbnailPaletteCtrl() override;

    void SetBounds(Rect) override;
    void DrawRow(DrawItemEvent*);
    void OnThumbMouseDown(VirtMouseEvent*);
    void OnThumbMouseMove(VirtMouseEvent*);
    void OnThumbMouseUp(VirtMouseEvent*);
    void OnThumbMouseWheel(VirtMouseEvent*);
    void OnThumbDoubleClick(VirtMouseEvent*);
    void Activate();
    void Deactivate();
    void HandleKey(int vkey);
    void StartRendering();
    int RenderedCount() const;

  private:
    int PageAtPoint(Point);
    void SelectPage(int);
    void OpenSelectedPage();
};

static void FreeThumbnail(Pixmap* thumbnail) {
    if (thumbnail != kThumbnailRenderFailed) {
        FreePixmap(thumbnail);
    }
}

static Pixmap* ThumbnailToDraw(ThumbnailPaletteCache* cache, int idx) {
    Pixmap* thumbnail = cache->thumbnails[idx];
    return thumbnail == kThumbnailRenderFailed ? nullptr : thumbnail;
}

static void FreeThumbnailRenderEngine(ThumbnailPaletteCache* cache) {
    ReportIf(cache->workerRunning);
    if (cache->renderEngine) {
        cache->renderEngine->Release();
        cache->renderEngine = nullptr;
    }
}

static void DeleteThumbnailCache(ThumbnailPaletteCache* cache) {
    for (Pixmap* thumbnail : cache->thumbnails) {
        FreeThumbnail(thumbnail);
    }
    VecReset(cache->thumbnails);
    FreeThumbnailRenderEngine(cache);
    delete cache;
}

static Pixmap* RenderPageThumbnail(EngineBase* engine, int pageNo, int rotation, int thumbDx, int thumbDy) {
    RectF pageRect = engine->PageMediabox(pageNo);
    if (pageRect.IsEmpty()) {
        return nullptr;
    }

    pageRect = engine->Transform(pageRect, pageNo, 1.0f, rotation);
    if (pageRect.dx <= 0 || pageRect.dy <= 0) {
        return nullptr;
    }
    float zoom = (float)thumbDx / pageRect.dx;
    pageRect.dy = std::min(pageRect.dy, (float)thumbDy / zoom);
    pageRect = engine->Transform(pageRect, pageNo, 1.0f, rotation, true);
    RenderPageArgs args(pageNo, zoom, rotation, &pageRect, RenderTarget::View);
    return engine->RenderPage(args);
}

static void FinishThumbnailRender(ThumbnailRenderTask* task) {
    ThumbnailPaletteCache* cache = task->cache;
    int idx = task->pageNo - 1;
    bool isValid = !cache->deleteWhenWorkerFinishes && idx >= 0 && idx < len(cache->thumbnails);
    if (isValid) {
        if (task->bitmap) {
            FreeThumbnail(cache->thumbnails[idx]);
            cache->thumbnails[idx] = task->bitmap;
            task->bitmap = nullptr;
            if (cache->ctrl && cache->ctrl->active) {
                cache->ctrl->Invalidate();
            }
        } else if (!cache->thumbnails[idx]) {
            cache->thumbnails[idx] = kThumbnailRenderFailed;
        }
    }
    FreePixmap(task->bitmap);
    delete task;
}

static void FinishThumbnailWorker(ThumbnailPaletteCache* cache) {
    cache->workerRunning = false;
    if (cache->deleteWhenWorkerFinishes) {
        DeleteThumbnailCache(cache);
        return;
    }
    if (!cache->ctrl || !cache->ctrl->active) {
        FreeThumbnailRenderEngine(cache);
        return;
    }
    cache->ctrl->StartRendering();
}

static void RenderAndPostThumbnail(ThumbnailRenderWorker* worker, EngineBase* engine, int pageNo) {
    auto* task = new ThumbnailRenderTask;
    task->cache = worker->cache;
    task->pageNo = pageNo;
    task->bitmap = RenderPageThumbnail(engine, pageNo, worker->rotation, worker->thumbDx, worker->thumbDy);
    uitask::Post(MkFunc0<ThumbnailRenderTask>(FinishThumbnailRender, task));
}

static void RenderThumbnailsInBackground(ThumbnailRenderWorker* worker) {
    ThumbnailPaletteCache* cache = worker->cache;
    if (worker->sourceEngine) {
        cache->renderEngine = worker->sourceEngine->Clone();
        worker->sourceEngine->Release();
        worker->sourceEngine = nullptr;
    }
    EngineBase* engine = cache->renderEngine;
    if (engine) {
        for (int pageNo : worker->pages) {
            if (AtomicIntGet(&cache->cancelRendering) != 0) {
                break;
            }
            RenderAndPostThumbnail(worker, engine, pageNo);
        }
    }
    uitask::Post(MkFunc0<ThumbnailPaletteCache>(FinishThumbnailWorker, cache));
    delete worker;
}

ThumbnailPaletteCtrl::ThumbnailPaletteCtrl(MainWindow* win, PlatformFont* font, int dpi) {
    this->win = win;
    this->tab = win->CurrentTab();
    this->font = font;
    this->dpi = dpi;
    SetFlag(vwfFocusable, false);
    SetColor(kColListText, ThemeWindowTextColor());
    SetColor(kColListBg, ThemeWindowControlBackgroundColor());

    thumbDx = DpiScaleByDpi(dpi, kPaletteThumbnailDx);
    thumbDy = DpiScaleByDpi(dpi, kPaletteThumbnailDy);
    gap = DpiScaleByDpi(dpi, kPaletteThumbnailGap);
    labelDy = PlatformFontMeasureText(font, StrL("0")).dy + DpiScaleByDpi(dpi, 4);
    itemDy = thumbDy + labelDy + gap;
    padding = DpiScaledInsets(kPaletteThumbnailPadding, kPaletteThumbnailPadding);

    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    pageCount = dm ? dm->PageCount() : 0;
    selectedPage = dm ? Clamp(dm->CurrentPageNo(), 1, std::max(pageCount, 1)) : 1;

    rowsModel = new ThumbnailRowsModel();
    rowsModel->rows = pageCount;
    SetModel(rowsModel);

    cache = new ThumbnailPaletteCache();
    cache->ctrl = this;
    cache->rotation = dm ? dm->GetRotation() : 0;
    cache->thumbDx = thumbDx;
    cache->thumbDy = thumbDy;
    VecAppendBlanks(cache->thumbnails, pageCount);

    onDrawItem = MkMethod1<ThumbnailPaletteCtrl, DrawItemEvent*, &ThumbnailPaletteCtrl::DrawRow>(this);
    VirtCtrl::onMouseDown =
        MkMethod1<ThumbnailPaletteCtrl, VirtMouseEvent*, &ThumbnailPaletteCtrl::OnThumbMouseDown>(this);
    VirtCtrl::onMouseMove =
        MkMethod1<ThumbnailPaletteCtrl, VirtMouseEvent*, &ThumbnailPaletteCtrl::OnThumbMouseMove>(this);
    VirtCtrl::onMouseUp = MkMethod1<ThumbnailPaletteCtrl, VirtMouseEvent*, &ThumbnailPaletteCtrl::OnThumbMouseUp>(this);
    VirtCtrl::onMouseWheel =
        MkMethod1<ThumbnailPaletteCtrl, VirtMouseEvent*, &ThumbnailPaletteCtrl::OnThumbMouseWheel>(this);
    VirtCtrl::onDoubleClick =
        MkMethod1<ThumbnailPaletteCtrl, VirtMouseEvent*, &ThumbnailPaletteCtrl::OnThumbDoubleClick>(this);
}

ThumbnailPaletteCtrl::~ThumbnailPaletteCtrl() {
    cache->ctrl = nullptr;
    AtomicIntSet(&cache->cancelRendering, 1);
    if (cache->workerRunning) {
        cache->deleteWhenWorkerFinishes = true;
    } else {
        DeleteThumbnailCache(cache);
    }
    cache = nullptr;
}

void ThumbnailPaletteCtrl::SetBounds(Rect r) {
    int reservedScrollbarDx = DpiScaleByDpi(dpi, 10);
    int availableDx = r.dx - padding.left - padding.right - reservedScrollbarDx;
    int newCols = (availableDx + gap) / (thumbDx + gap);
    newCols = Clamp(newCols, 1, kPaletteThumbnailMaxCols);
    if (newCols != cols) {
        cols = newCols;
        rowsModel->rows = (pageCount + cols - 1) / cols;
        SetModel(rowsModel);
    }

    VirtListBox::SetBounds(r);
    EnsureVisible((selectedPage - 1) / cols);
    if (active) {
        StartRendering();
    }
}

void ThumbnailPaletteCtrl::DrawRow(DrawItemEvent* ev) {
    int gridDx = cols * thumbDx + (cols - 1) * gap;
    int left = ev->itemRect.x + std::max(0, (ev->itemRect.dx - gridDx) / 2);
    int firstPage = ev->itemIndex * cols + 1;
    int lastPage = std::min(pageCount, firstPage + cols - 1);
    Color textColor = ThemeWindowTextColor();
    for (int pageNo = firstPage; pageNo <= lastPage; pageNo++) {
        int col = pageNo - firstPage;
        int x = left + col * (thumbDx + gap);
        Rect pageRect{x, ev->itemRect.y, thumbDx, thumbDy};
        ev->gfx->FillRect(pageRect, kColWhite);

        Pixmap* thumbnail = ThumbnailToDraw(cache, pageNo - 1);
        if (thumbnail) {
            int drawDx = std::min(thumbnail->width, thumbDx);
            int drawDy = std::min(thumbnail->height, thumbDy);
            Rect target{x + (thumbDx - drawDx) / 2, pageRect.y + (thumbDy - drawDy) / 2, drawDx, drawDy};
            ev->gfx->DrawPixmap(thumbnail, target);
        }

        Color border = pageNo == selectedPage ? MkRgb(0, 120, 215) : MkGray(150);
        ev->gfx->DrawRect(pageRect, border, pageNo == selectedPage ? 3 : 1);
        Rect label{x, pageRect.Bottom(), thumbDx, labelDy};
        ev->gfx->DrawText(fmt("%d", pageNo), label, gfxTextCenter | gfxTextVCenter, font, textColor);
    }
}

int ThumbnailPaletteCtrl::PageAtPoint(Point pt) {
    int row = ItemFromPoint(pt);
    if (row < 0) {
        return -1;
    }
    Rect rowRect = ItemRect(row);
    if (rowRect.IsEmpty()) {
        return -1;
    }
    Point origin = OriginInWindow();
    rowRect.Offset(-origin.x, -origin.y);
    int gridDx = cols * thumbDx + (cols - 1) * gap;
    int left = rowRect.x + std::max(0, (rowRect.dx - gridDx) / 2);
    if (pt.x < left || pt.y < rowRect.y || pt.y >= rowRect.y + thumbDy + labelDy) {
        return -1;
    }
    int col = (pt.x - left) / (thumbDx + gap);
    if (col < 0 || col >= cols) {
        return -1;
    }
    int cellX = left + col * (thumbDx + gap);
    if (pt.x >= cellX + thumbDx) {
        return -1;
    }
    int pageNo = row * cols + col + 1;
    return pageNo <= pageCount ? pageNo : -1;
}

void ThumbnailPaletteCtrl::SelectPage(int pageNo) {
    if (pageCount <= 0) {
        return;
    }
    pageNo = Clamp(pageNo, 1, pageCount);
    if (pageNo == selectedPage) {
        return;
    }
    selectedPage = pageNo;
    EnsureVisible((selectedPage - 1) / cols);
    Invalidate();
    StartRendering();
}

void ThumbnailPaletteCtrl::OpenSelectedPage() {
    if (!win || !tab || win->CurrentTab() != tab) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return;
    }
    dm->GoToPage(selectedPage, 0, true);
    ScheduleDeleteAndExecCommand();
}

void ThumbnailPaletteCtrl::OnThumbMouseDown(VirtMouseEvent* ev) {
    int pageNo = PageAtPoint(ev->pt);
    if (pageNo > 0) {
        SelectPage(pageNo);
        ev->didHandle = true;
        return;
    }
    int oldScrollY = scrollY;
    VirtListBox::OnMouseDown(ev);
    if (scrollY != oldScrollY) {
        StartRendering();
    }
}

void ThumbnailPaletteCtrl::OnThumbMouseMove(VirtMouseEvent* ev) {
    int oldScrollY = scrollY;
    VirtListBox::OnMouseMove(ev);
    if (scrollY != oldScrollY) {
        StartRendering();
    }
    if (ev->didHandle) {
        return;
    }
    int pageNo = PageAtPoint(ev->pt);
    if (pageNo > 0) {
        SelectPage(pageNo);
        ev->didHandle = true;
    }
}

void ThumbnailPaletteCtrl::OnThumbMouseUp(VirtMouseEvent* ev) {
    VirtListBox::OnMouseUp(ev);
}

void ThumbnailPaletteCtrl::OnThumbMouseWheel(VirtMouseEvent* ev) {
    int oldScrollY = scrollY;
    VirtListBox::OnMouseWheel(ev);
    if (scrollY != oldScrollY) {
        StartRendering();
    }
}

void ThumbnailPaletteCtrl::OnThumbDoubleClick(VirtMouseEvent* ev) {
    int pageNo = PageAtPoint(ev->pt);
    if (pageNo <= 0) {
        return;
    }
    SelectPage(pageNo);
    OpenSelectedPage();
    ev->didHandle = true;
}

void ThumbnailPaletteCtrl::HandleKey(int vkey) {
    if (pageCount <= 0) {
        return;
    }
    int pageNo = selectedPage;
    int pageStep = std::max(1, UsableDy() / itemDy) * cols;
    switch (vkey) {
        case VK_RETURN:
            OpenSelectedPage();
            return;
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
            pageNo = pageCount;
            break;
        default:
            return;
    }
    SelectPage(Clamp(pageNo, 1, pageCount));
}

void ThumbnailPaletteCtrl::Activate() {
    if (active) {
        return;
    }
    active = true;
    AtomicIntSet(&cache->cancelRendering, 0);
    EnsureVisible((selectedPage - 1) / cols);
    StartRendering();
    Invalidate();
}

void ThumbnailPaletteCtrl::Deactivate() {
    if (!active) {
        return;
    }
    active = false;
    AtomicIntSet(&cache->cancelRendering, 1);
    if (!cache->workerRunning) {
        FreeThumbnailRenderEngine(cache);
    }
}

int ThumbnailPaletteCtrl::RenderedCount() const {
    int n = 0;
    for (Pixmap* thumbnail : cache->thumbnails) {
        if (thumbnail && thumbnail != kThumbnailRenderFailed) {
            n++;
        }
    }
    return n;
}

void ThumbnailPaletteCtrl::StartRendering() {
    if (!active || cache->workerRunning || pageCount <= 0) {
        return;
    }
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm || win->CurrentTab() != tab || dm->PageCount() != pageCount || dm->GetRotation() != cache->rotation) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return;
    }

    int visibleRows = std::max(1, UsableDy() / itemDy);
    int firstVisible = (scrollY / itemDy) * cols + 1;
    int perScreen = std::max(1, visibleRows * cols);
    int lastVisible = std::min(pageCount, firstVisible + perScreen - 1);
    int firstPage = std::max(1, firstVisible - kPaletteThumbnailRenderScreens * perScreen);
    int lastPage = std::min(pageCount, lastVisible + kPaletteThumbnailRenderScreens * perScreen);

    int keepFirst = std::max(1, firstPage - kPaletteThumbnailKeepScreens * perScreen);
    int keepLast = std::min(pageCount, lastPage + kPaletteThumbnailKeepScreens * perScreen);
    for (int idx = 0; idx < len(cache->thumbnails); idx++) {
        int pageNo = idx + 1;
        if (cache->thumbnails[idx] && (pageNo < keepFirst || pageNo > keepLast)) {
            FreeThumbnail(cache->thumbnails[idx]);
            cache->thumbnails[idx] = nullptr;
        }
    }

    auto* worker = new ThumbnailRenderWorker;
    for (int pageNo = firstVisible; pageNo <= lastVisible; pageNo++) {
        if (!cache->thumbnails[pageNo - 1]) {
            VecAppend(worker->pages, pageNo);
        }
    }
    for (int pageNo = firstPage; pageNo < firstVisible; pageNo++) {
        if (!cache->thumbnails[pageNo - 1]) {
            VecAppend(worker->pages, pageNo);
        }
    }
    for (int pageNo = lastVisible + 1; pageNo <= lastPage; pageNo++) {
        if (!cache->thumbnails[pageNo - 1]) {
            VecAppend(worker->pages, pageNo);
        }
    }
    if (len(worker->pages) == 0) {
        delete worker;
        return;
    }

    worker->cache = cache;
    worker->rotation = cache->rotation;
    worker->thumbDx = cache->thumbDx;
    worker->thumbDy = cache->thumbDy;
    if (!cache->renderEngine) {
        worker->sourceEngine = engine;
        engine->AddRef();
    }
    AtomicIntSet(&cache->cancelRendering, 0);
    cache->workerRunning = true;
    RunAsync(MkFunc0<ThumbnailRenderWorker>(RenderThumbnailsInBackground, worker),
             StrL("CommandPaletteThumbnailRender"));
}

void CommandPaletteWnd::SetThumbnailMode(ThumbnailMode mode) {
    bool enabled = mode == ThumbnailMode::Enabled;
    if (!thumbnailCtrl || thumbnailMode == enabled) {
        return;
    }
    if (enabled) {
        listBox->SetIsVisible(false);
        thumbnailCtrl->SetIsVisible(true);
        thumbnailMode = true;
        DoLayout(HwndClientRect(hwnd).Size());
        thumbnailCtrl->Activate();
    } else {
        thumbnailCtrl->Deactivate();
        thumbnailCtrl->SetIsVisible(false);
        listBox->SetIsVisible(true);
        thumbnailMode = false;
        DoLayout(HwndClientRect(hwnd).Size());
    }
    EditSetFocus(editQuery);
}

void SafeDeleteCommandPaletteWnd() {
    if (!gCommandPaletteWnd) {
        return;
    }

    MainWindow* win = gCommandPaletteWnd->win;
    auto* tmp = gCommandPaletteWnd;
    gCommandPaletteWnd = nullptr;
    delete tmp;
    if (gHwndToActivateOnClose) {
        HWND fg = GetForegroundWindow();
        if (!fg || fg == gHwndToActivateOnClose) {
            SetActiveWindow(gHwndToActivateOnClose);
        }
        gHwndToActivateOnClose = nullptr;
    }
    if (gTabToSelectOnClose) {
        WindowTab* tab = gTabToSelectOnClose;
        gTabToSelectOnClose = nullptr;
        if (IsMainWindowValidAndNotClosing(tab->win) && tab->win->GetTabIdx(tab) >= 0) {
            SelectTabInWindow(tab);
        }
    }
    if (gCmdIdToExecOnClose != 0) {
        i32 cmdId = gCmdIdToExecOnClose;
        gCmdIdToExecOnClose = 0;
        if (IsMainWindowValidAndNotClosing(win)) {
            HwndPostCommand(win->hwndFrame, cmdId);
        }
    }
    if (gFavToGoToOnClose) {
        FileState* fs = gFavFsToGoToOnClose;
        Favorite* fav = gFavToGoToOnClose;
        gFavFsToGoToOnClose = nullptr;
        gFavToGoToOnClose = nullptr;
        if (IsMainWindowValidAndNotClosing(win)) {
            GoToFavorite(win, fs, fav);
        }
    }
}

void ScheduleDeleteAndExecCommand(i32 cmdId) {
    if (!gCommandPaletteWnd) {
        return;
    }
    gCmdIdToExecOnClose = cmdId;
    if (IsMainWindowValidAndNotClosing(gCommandPaletteWnd->win)) {
        HighlightTab(gCommandPaletteWnd->win, nullptr);
    }
    auto fn = MkFunc0Void(SafeDeleteCommandPaletteWnd);
    uitask::Post(fn, "SafeDeleteCommandPaletteWnd");
}

void CommandPaletteSetCurrentSelection(CommandPaletteWnd* wnd, int idx) {
    wnd->listBox->SetCurrentSelection(idx);
    wnd->OnSelectionChange();
}

static void EditSetTextAndFocus(Edit* e, Str s) {
    e->SetText(s);
    EditSetCursorPosAtEnd(e);
    EditSetFocus(e);
}

void CommandPaletteWnd::SwitchToPrefix(Str prefix) {
    EditSetTextAndFocus(editQuery, prefix);
}

void CommandPaletteWnd::SwitchToCommands() {
    SwitchToPrefix(Str(kPalettePrefixCommands));
}

void CommandPaletteWnd::SwitchToTabs() {
    SwitchToPrefix(Str(kPalettePrefixTabs));
}

void CommandPaletteWnd::SwitchToEverything() {
    SwitchToPrefix(Str(kPalettePrefixEverything));
}

void CommandPaletteWnd::SwitchToFileHistory() {
    SwitchToPrefix(Str(kPalettePrefixFileHistory));
}

void CommandPaletteWnd::SwitchToTOC() {
    SwitchToPrefix(Str(kPalettePrefixTOC));
}

void CommandPaletteWnd::SwitchToFavorites() {
    SwitchToPrefix(Str(kPalettePrefixFavorites));
}

void CommandPaletteWnd::SwitchToBoolSettings() {
    SwitchToPrefix(Str(kPalettePrefixBoolSettings));
}

void CommandPaletteWnd::OnActivate(WindowBase::ActivateEvent* ev) {
    if (ev->state == WA_INACTIVE) {
        // -for-testing runs in the background, so this popup never stays
        // foreground. Closing on WA_INACTIVE would destroy it between
        // sequential WM_SETTEXT queries (image-only-palette-items).
        if (!gForTesting) {
            ScheduleDeleteAndExecCommand();
        }
        ev->didHandle = true;
    }
}

void CommandPaletteWnd::OnCommand(WindowBase::CommandEvent* ev) {
    int cmdId = LOWORD(ev->wparam);
    CustomCommand* cmd = FindCustomCommand(cmdId);
    if (cmd != nullptr) {
        cmdId = cmd->origId;
    }
    switch (cmdId) {
        case CmdNextTabSmart:
        case CmdPrevTabSmart: {
            int dir = cmdId == CmdNextTabSmart ? 1 : -1;
            AdvanceSelection(dir);
            ev->didHandle = true;
            return;
        }
        case CmdSelectAll:
            // Ctrl+A is an accelerator (CmdSelectAll) sent to this window;
            // select the query instead of the document (issue #5972).
            EditSelectAll(editQuery);
            ev->didHandle = true;
            return;
        case CmdCopySelection:
            // Ctrl+C is an accelerator (CmdCopySelection) sent to this window;
            // copy the query instead of the document (issue #5972).
            if (editQuery && editQuery->hwnd) {
                SendMessageW(editQuery->hwnd, WM_COPY, 0, 0);
            }
            ev->didHandle = true;
            return;
    }
}

void CommandPaletteWnd::OnSelectionChange() {
    int idx = listBox->GetCurrentSelection();
    if (!smartTabMode) {
        return;
    }
    auto* m = (ListBoxModelCP*)listBox->model;
    ItemDataCP* data = m->strings.AtData(idx);
    HighlightTab(win, data->tab);
}

bool CommandPaletteWnd::AdvanceSelection(int dir) {
    if (dir == 0) {
        return false;
    }
    int n = listBox->ItemsCount();
    if (n == 0) {
        return false;
    }
    int currSel = listBox->GetCurrentSelection();
    int sel = currSel + dir;
    if (sel < 0) {
        sel = n - 1;
    }
    if (sel >= n) {
        sel = 0;
    }
    CommandPaletteSetCurrentSelection(this, sel);
    return true;
}

// Delete selected list item when it is removable: file-history entry, open tab,
// or favorite. Commands and TOC entries are not removable (caller should let
// the edit control handle Delete). After removal, refilter and keep selection
// on the same index (or the new last item if we deleted the last row).
// remove selected history / tab / favorite; keeps selection index stable
bool CommandPaletteWnd::RemoveSelectedItem() {
    if (!listBox || !listBox->model) {
        return false;
    }
    int currSel = listBox->GetCurrentSelection();
    if (currSel < 0) {
        return false;
    }
    auto* m = (ListBoxModelCP*)listBox->model;
    int n = m->ItemsCount();
    if (currSel >= n) {
        return false;
    }
    ItemDataCP* d = m->Data(currSel);
    if (!d) {
        return false;
    }

    // Commands and TOC: not removable from the palette
    if (d->cmdId != 0 || d->tocItem) {
        return false;
    }

    if (d->tab) {
        WindowTab* tab = d->tab;
        CloseTab(tab, false);
        if (!IsMainWindowValid(win)) {
            // closing the last tab closed the host window
            ScheduleDeleteAndExecCommand();
            return true;
        }
    } else if (d->fav && d->favFs) {
        // copy before DelFavorite frees the Favorite*
        Str path = d->favFs->filePath;
        int pageNo = d->fav->pageNo;
        DelFavorite(path, pageNo);
    } else if (d->filePath) {
        ForgetFileFromFrequentlyRead(win, d->filePath);
    } else {
        return false;
    }

    CollectStrings(win);
    Str filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    FilterStringsForQuery(filter, m->strings);
    listBox->SetModel(m);

    n = m->ItemsCount();
    if (n == 0) {
        listBox->SetCurrentSelection(-1);
        return true;
    }
    int sel = currSel;
    if (sel >= n) {
        sel = n - 1;
    }
    CommandPaletteSetCurrentSelection(this, sel);
    return true;
}

void CommandPaletteWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_ESCAPE) {
        ScheduleDeleteAndExecCommand();
        ev->didHandle = true;
        return;
    }

    if (ev->vkey == VK_RETURN) {
        if (thumbnailMode) {
            thumbnailCtrl->HandleKey(ev->vkey);
            ev->didHandle = true;
            return;
        }
        ExecuteCurrentSelection();
        ev->didHandle = true;
        return;
    }

    if (ev->vkey == VK_DELETE) {
        if (RemoveSelectedItem()) {
            ev->didHandle = true;
        }
        // not a removable list item: let the edit control process Delete
        return;
    }

    if (ev->vkey == VK_TAB) {
        if (ev->isCtrl) {
            ev->didHandle = AdvanceSelection(ev->isShift ? -1 : 1);
        }
        return;
    }

    if (ev->vkey == VK_LEFT || ev->vkey == VK_RIGHT || ev->vkey == VK_UP || ev->vkey == VK_DOWN ||
        ev->vkey == VK_NEXT || ev->vkey == VK_PRIOR) {
        if (thumbnailMode) {
            thumbnailCtrl->HandleKey(ev->vkey);
            ev->didHandle = true;
            return;
        }
        ev->didHandle = MoveSelection(ev->vkey);
        return;
    }

    if (ev->vkey == VK_HOME || ev->vkey == VK_END) {
        if (thumbnailMode) {
            thumbnailCtrl->HandleKey(ev->vkey);
            ev->didHandle = true;
            return;
        }
        // Ctrl+Home / Ctrl+End: always first / last row
        if (ev->isCtrl) {
            ev->didHandle = MoveSelection(ev->vkey);
            return;
        }
        if (!editQuery || ev->hwnd != editQuery->hwnd) {
            ev->didHandle = MoveSelection(ev->vkey);
            return;
        }
        // Home / End: if the caret is already at the start/end of the query,
        // move the list; otherwise let the Edit control move the caret.
        int selStart = 0, selEnd = 0;
        EditGetSelection(editQuery, selStart, selEnd);
        int textLen = EditGetTextLen(editQuery);
        bool toEnd = (ev->vkey == VK_END);
        bool caretAtBound = (selStart == selEnd) && (toEnd ? selEnd == textLen : selStart == 0);
        if (caretAtBound) {
            ev->didHandle = MoveSelection(ev->vkey);
        }
    }
}

// Home / End / PageUp / PageDown move the list the same way as the Find
// window: Home/End go to the first/last row, PageUp/PageDown jump a page
// (no wrap). Up/Down still wrap via AdvanceSelection.
bool CommandPaletteWnd::MoveSelection(int vkey) {
    if (vkey == VK_UP) {
        return AdvanceSelection(-1);
    }
    if (vkey == VK_DOWN) {
        return AdvanceSelection(1);
    }
    if (!listBox) {
        return false;
    }
    int n = listBox->ItemsCount();
    if (n == 0) {
        return false;
    }
    int curr = listBox->GetCurrentSelection();
    int perPage = std::max(listBox->UsableDy() / listBox->GetItemHeight(), 1);
    int idx = curr;
    switch (vkey) {
        case VK_HOME:
            idx = 0;
            break;
        case VK_END:
            idx = n - 1;
            break;
        case VK_NEXT:
            if (curr < 0) {
                idx = 0;
            } else {
                idx = std::min(curr + perPage, n - 1);
            }
            break;
        case VK_PRIOR:
            if (curr < 0) {
                idx = n - 1;
            } else {
                idx = std::max(curr - perPage, 0);
            }
            break;
        default:
            return false;
    }
    if (idx == curr) {
        return true;
    }
    CommandPaletteSetCurrentSelection(this, idx);
    return true;
}

// smart-tab releases Ctrl after the palette is open; key-downs go via onKeyDown
void CommandPaletteWnd::PreTranslate(WindowBase::PreTranslateEvent* ev) {
    MSG& msg = *ev->msg;
    if (smartTabMode && msg.message == WM_KEYUP && msg.wParam == VK_CONTROL) {
        if (!stickyMode) {
            ExecuteCurrentSelection();
        }
        ev->didHandle = true;
    }
}

void CommandPaletteWnd::ExecuteCurrentSelection() {
    int idx = listBox->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    auto* m = (ListBoxModelCP*)listBox->model;
    ItemDataCP* data = m->strings.AtData(idx);
    i32 cmdId = data->cmdId;
    if (cmdId == CmdToggleBoolSetting) {
        SwitchToBoolSettings();
        return;
    }
    if (cmdId != 0) {
        bool noActivate = IsCmdInList(cmdId, gCommandsNoActivate);
        if (noActivate) {
            gHwndToActivateOnClose = nullptr;
        }
        ScheduleDeleteAndExecCommand(cmdId);
        return;
    }

    if (data->boolSetting) {
        ToggleSettingsBool(data->boolSetting);
        ScheduleDeleteAndExecCommand();
        return;
    }

    WindowTab* tab = data->tab;
    if (tab != nullptr) {
        MainWindow* mainWin = FindMainWindowByTab(tab);
        if (!mainWin) {
            ScheduleDeleteAndExecCommand();
            return;
        }
        gTabToSelectOnClose = tab;
        gHwndToActivateOnClose = mainWin->hwndFrame;
        ScheduleDeleteAndExecCommand();
        return;
    }

    if (data->tocItem) {
        gHwndToActivateOnClose = win->hwndFrame;
        GoToTocItem(win, data->tocItem);
        ScheduleDeleteAndExecCommand();
        return;
    }

    if (data->fav) {
        gHwndToActivateOnClose = win->hwndFrame;
        gFavFsToGoToOnClose = data->favFs;
        gFavToGoToOnClose = data->fav;
        ScheduleDeleteAndExecCommand();
        return;
    }
    auto filePath = data->filePath;
    if (filePath) {
        LoadArgs args(filePath, win);
        args.activateExisting = true;
        args.activateExistingInWindow = true;
        args.forceReuse = false;
        StartLoadDocument(&args);
        ScheduleDeleteAndExecCommand();
        return;
    }
    logf("CommandPaletteWnd::ExecuteCurrentSelection: no match for selection '%s'\n", m->strings[idx]);
    ReportIf(true);
    ScheduleDeleteAndExecCommand();
}

void CommandPaletteWnd::OnListDoubleClick() {
    ExecuteCurrentSelection();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    ScheduleDeleteAndExecCommand();
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    ScheduleDeleteAndExecCommand();
}

// The help lines name keys, and a key reads better as a key-cap than as a word
// in the sentence. The strings are translated, so instead of putting markup in
// them - which would invalidate every existing translation - the key names are
// wrapped wherever they ended up in the sentence. Translators leave key names
// in English, so matching on them works in every language
static const char* kHelpKeys[] = {
    "Ctrl+Tab", "Ctrl", "Enter", "Space", "Del", "Esc", "PgUp", "PgDn", "\u2191", "\u2193",
};

// s[at..] is tok and isn't part of a longer word
static bool IsTokenAt(Str s, int at, Str tok) {
    int n = len(tok);
    if (at + n > len(s)) {
        return false;
    }
    if (!str::EqN(Str(s.s + at, n), tok, n)) {
        return false;
    }
    char before = (at > 0) ? s.s[at - 1] : ' ';
    char after = (at + n < len(s)) ? s.s[at + n] : ' ';
    bool okBefore = (before == ' ') || (before == '(');
    bool okAfter = (after == ' ') || (after == ',') || (after == ')') || (after == '.');
    return okBefore && okAfter;
}

static TempStr WithKbdMarkupTemp(Str s) {
    str::Builder out;
    int i = 0;
    while (i < len(s)) {
        Str match{};
        for (const char* k : kHelpKeys) {
            if (IsTokenAt(s, i, Str(k))) {
                match = Str(k);
                break;
            }
        }
        if (!match) {
            out.AppendChar(s.s[i]);
            i++;
            continue;
        }
        out.Append(StrL("(Kbd/"));
        out.Append(match);
        out.Append(StrL(")"));
        i += len(match);
    }
    return ToStrTemp(out);
}

// one of the "# File History" / "> Commands" switches in the top row; it
// carries the prefix it switches to so they can share one click handler
struct PaletteSwitch : VirtRichText {
    CommandPaletteWnd* wnd = nullptr;
    Str prefix;
};

static void OnPaletteSwitchClicked(VirtMouseEvent* ev) {
    auto* t = (PaletteSwitch*)ev->target;
    t->wnd->SwitchToPrefix(t->prefix);
}

// what the two help rows have in common
struct HelpStyle {
    HWND hwnd = nullptr;
    PlatformFont* font = nullptr;
    Color colTxt = kColorUnset;
    Color colBg = kColorUnset;
};

static void InitHelpText(const HelpStyle& st, VirtRichText* t, Str markup) {
    ParseTipInto(t, markup);
    t->font = st.font;
    // the help rows are not links, so they take the plain text color
    t->SetColor(kColRichText, st.colTxt);
    t->SetColor(kColRichLink, st.colTxt);
    t->SetColor(kColRichBg, st.colBg);
    int padX = DpiScale(8);
    t->padding = Insets{0, padX, 0, padX};
}

static VirtRichText* NewHelpText(const HelpStyle& st, Str markup) {
    auto* t = new VirtRichText();
    InitHelpText(st, t, markup);
    return t;
}

// a row of help items: virtual controls sitting in the palette's layout next
// to the real edit and list
static HBox* NewHelpRow(HBox* box) {
    box->alignMain = MainAxisAlign::MainCenter;
    box->alignCross = CrossAxisAlign::CrossCenter;
    return box;
}

enum {
    kHelpNone = -1,
    kHelpSmartTab,
    kHelpCommands,
    kHelpHistory,
    kHelpTabs,
    kHelpFavorites,
    kHelpSettings,
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
    if (str::StartsWith(filter, Str(kPalettePrefixTOC)) || str::StartsWith(filter, Str(kPalettePrefixTOCLegacy))) {
        return kHelpToc;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixFavorites))) {
        return kHelpFavorites;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixBoolSettings))) {
        return kHelpSettings;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixThumbnails))) {
        return kHelpThumbnails;
    }
    return kHelpCommands;
}

void CommandPaletteWnd::UpdateHelpRow() {
    if (!helpRow) {
        return;
    }
    Str filter{};
    if (editQuery) {
        filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    }
    int kind = PaletteHelpKind(filter, smartTabMode);
    if (helpRow->ChildrenCount() > 0 && kind == helpKind) {
        return;
    }
    helpKind = kind;

    for (auto& c : helpRow->children) {
        delete c.layout;
    }
    VecReset(helpRow->children);

    Str strings[4];
    int nHelp = 0;
    switch (kind) {
        case kHelpSmartTab:
            strings[nHelp++] = _TRA("Ctrl+Tab navigate");
            strings[nHelp++] = _TRA("Release Ctrl select");
            strings[nHelp++] = _TRA("Space for sticky mode");
            strings[nHelp++] = _TRA("Del close tab");
            break;
        case kHelpHistory:
            strings[nHelp++] = _TRA("Enter open file");
            strings[nHelp++] = _TRA("Del remove from history");
            strings[nHelp++] = _TRA("Esc close");
            break;
        case kHelpTabs:
            strings[nHelp++] = _TRA("Enter switch to tab");
            strings[nHelp++] = _TRA("Del close tab");
            strings[nHelp++] = _TRA("Esc close");
            break;
        case kHelpFavorites:
            strings[nHelp++] = _TRA("Enter go to favorite");
            strings[nHelp++] = _TRA("Del remove favorite");
            strings[nHelp++] = _TRA("Esc close");
            break;
        case kHelpSettings:
            strings[nHelp++] = _TRA("Enter change");
            strings[nHelp++] = _TRA("Esc close");
            break;
        case kHelpThumbnails:
            strings[nHelp++] = _TRA("Enter go to");
            strings[nHelp++] = _TRA("Esc close");
            break;
        case kHelpToc:
            strings[nHelp++] = _TRA("Enter go to");
            strings[nHelp++] = _TRA("Esc close");
            break;
        case kHelpEverything:
            strings[nHelp++] = _TRA("Enter select");
            strings[nHelp++] = _TRA("Esc close");
            break;
        default:
            strings[nHelp++] = _TRA("Enter run command");
            strings[nHelp++] = _TRA("Esc close");
            break;
    }
    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    HelpStyle st{hwnd, GetAppFont(), colTxt, colBg};
    for (int i = 0; i < nHelp; i++) {
        helpRow->AddChild(NewHelpText(st, WithKbdMarkupTemp(strings[i])));
    }
    if (layout) {
        DoLayout();
    }
}

bool CommandPaletteWnd::Create(MainWindow* win, Str prefix, int smartTabAdvance) {
    if (str::Eq(prefix, Str(kPalettePrefixTabs))) {
        smartTabMode = smartTabAdvance != 0;
    }
    tocMode = str::Eq(prefix, Str(kPalettePrefixTOC)) || str::Eq(prefix, Str(kPalettePrefixTOCLegacy));
    CollectStrings(win);
    {
        CreateCustomArgs args;
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        // owned tool window: stays above the frame, off the taskbar (#6126)
        args.owner = win->hwndFrame;
        args.exStyle = WS_EX_TOOLWINDOW;
        args.font = GetFont();
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = true;
        args.cueText = StrL("enter search term");
        args.text = prefix;
        args.font = GetFont();
        args.isRtl = IsUIRtl();
        auto* c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::QueryChanged>(this);
        editQuery = c;
        vbox->AddChild(c);
    }

    if (!smartTabMode) {
        vbox->AddChild(new Spacer(0, DpiScale(4)));
        auto* box = new HBox();
        box->rtl = CommandPaletteUiRtl();
        // same smaller app font as the bottom hint row
        HelpStyle st{hwnd, GetAppFont(), colTxt, colBg};
        // in "# File History" and friends the first character is what you type
        // to get there, so it becomes a key-cap
        auto addSwitch = [this, box, &st](Str s, Str switchTo) {
            TempStr markup = str::JoinTemp(StrL("(Kbd/"), Str(s.s, 1), StrL(")"), Str(s.s + 1, len(s) - 1));
            auto* t = new PaletteSwitch();
            InitHelpText(st, t, markup);
            t->wnd = this;
            t->prefix = switchTo;
            t->onClick = MkFunc1Void(OnPaletteSwitchClicked);
            box->AddChild(t);
        };
        addSwitch(_TRA("# File History"), Str(kPalettePrefixFileHistory));
        addSwitch(_TRA("> Commands"), Str(kPalettePrefixCommands));
        addSwitch(_TRA("@ Tabs"), Str(kPalettePrefixTabs));
        addSwitch(_TRA(": Everything"), Str(kPalettePrefixEverything));
        if (len(toc) > 0) {
            addSwitch(_TRA("% TOC"), Str(kPalettePrefixTOC));
        }
        if (len(favorites) > 0) {
            addSwitch(_TRA("$ Favorites"), Str(kPalettePrefixFavorites));
        }
        addSwitch(_TRA("= Settings"), Str(kPalettePrefixBoolSettings));
        if (win->AsFixed()) {
            addSwitch(_TRA("& Thumbnails"), Str(kPalettePrefixThumbnails));
        }
        vbox->AddChild(NewHelpRow(box));
    }

    {
        auto* overlay = new Overlay();
        auto* c = new VirtListBox();
        // the query edit owns the keyboard here (the palette turns the arrow
        // keys into selection changes itself), so the list doesn't take the
        // focus and doesn't show a focus ring
        c->SetFlag(vwfFocusable, false);
        c->dpi = GetDpi();
        c->font = font;
        c->SetColor(kColListText, colTxt);
        c->SetColor(kColListBg, colBg);
        c->padding = DpiScaledInsets(4, 0);
        c->onDoubleClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::OnListDoubleClick>(this);
        c->onDrawItem =
            MkMethod1<CommandPaletteWnd, VirtListBox::DrawItemEvent*, &CommandPaletteWnd::DrawListBoxItem>(this);
        c->onSelectionChanged = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::OnSelectionChange>(this);
        auto* m = new ListBoxModelCP();
        FilterStringsForQuery(prefix, m->strings);
        c->SetModel(m);
        listBox = c;
        overlay->AddChild(c);

        if (win->AsFixed()) {
            thumbnailCtrl = new ThumbnailPaletteCtrl(win, font, GetDpi());
            thumbnailCtrl->SetIsVisible(false);
            overlay->AddChild(thumbnailCtrl);
        }
        vbox->AddChild(overlay, 1);
    }

    {
        auto* box = new HBox();
        box->rtl = CommandPaletteUiRtl();
        helpRow = NewHelpRow(box);
        vbox->AddChild(helpRow);
        UpdateHelpRow();
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    auto rc = HwndClientRect(win->hwndFrame);
    int dy = rc.dy - 72;
    dy = std::max(dy, 480);
    int dx = rc.dx - 256;
    dx = limitValue(dx, 640, 1024);
    if (smartTabMode) {
        // size the window to the number of tabs instead of using a fixed height
        int itemDy = listBox->GetItemHeight();
        int maxLines = 16;
        if (itemDy > 0) {
            maxLines = std::max((rc.dy - DpiScale(160)) / itemDy, 3);
        }
        listBox->idealSizeLines = std::min(listBox->model->ItemsCount(), maxLines);
        dy = 0;
    }
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    // the help rows are virtual controls: pick them up so we paint them and
    // they get their input
    DoLayout(HwndClientRect(hwnd).Size());
    if (str::StartsWith(prefix, Str(kPalettePrefixThumbnails))) {
        SetThumbnailMode(ThumbnailMode::Enabled);
    }
    PositionCommandPalette(hwnd, win->hwndFrame);

    EditSetCursorPosAtEnd(editQuery);
    if (smartTabMode) {
        int nItems = listBox->model->ItemsCount();
        int tabToSelect = (currTabIdx + nItems + smartTabAdvance) % nItems;
        CommandPaletteSetCurrentSelection(this, tabToSelect);
    } else if (tocMode) {
        int nItems = listBox->model->ItemsCount();
        if (currTocIdx >= 0 && currTocIdx < nItems) {
            CommandPaletteSetCurrentSelection(this, currTocIdx);
        }
    }

    SetIsVisible(true);
    EditSetFocus(editQuery);
    return true;
}

void RunCommandPalette(MainWindow* win, Str prefix, int smartTabAdvance) {
    if (gCommandPaletteWnd) {
        if (gCommandPaletteWnd->hwnd && IsWindow(gCommandPaletteWnd->hwnd)) {
            HwndSetFocus(gCommandPaletteWnd->hwnd);
            return;
        }
        ScheduleDeleteAndExecCommand();
    }

    auto* wnd = new CommandPaletteWnd();
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onActivate = MkMethod1<CommandPaletteWnd, WindowBase::ActivateEvent*, &CommandPaletteWnd::OnActivate>(wnd);
    wnd->onCommand = MkMethod1<CommandPaletteWnd, WindowBase::CommandEvent*, &CommandPaletteWnd::OnCommand>(wnd);
    wnd->onKeyDown = MkMethod1<CommandPaletteWnd, KeyEvent*, &CommandPaletteWnd::OnKeyDown>(wnd);
    wnd->onPreTranslate =
        MkMethod1<CommandPaletteWnd, WindowBase::PreTranslateEvent*, &CommandPaletteWnd::PreTranslate>(wnd);
    wnd->SetFont(GetAppBiggerFont());
    wnd->win = win;
    bool ok = wnd->Create(win, prefix, smartTabAdvance);
    ReportIf(!ok);
    gCommandPaletteWnd = wnd;
    gHwndToActivateOnClose = win->hwndFrame;
}

HWND CommandPaletteHwndForAccelerator(HWND hwnd) {
    if (!gCommandPaletteWnd) {
        return nullptr;
    }
    auto* wnd = gCommandPaletteWnd;
    HWND wHwnd = wnd->hwnd;
    if (hwnd == wHwnd) {
        return wHwnd;
    }
    if (wnd->editQuery && wnd->editQuery->hwnd == hwnd) {
        return wHwnd;
    }
    return nullptr;
}

// Selected list row and query-edit selection, for -dbg-control tests.
TempStr CommandPaletteStateTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    if (!gCommandPaletteWnd || !gCommandPaletteWnd->hwnd) {
        out.Append(StrL("NOTREADY no-palette\n"));
        return finish(2);
    }
    auto* wnd = gCommandPaletteWnd;
    int sel = wnd->listBox ? wnd->listBox->GetCurrentSelection() : -1;
    int n = wnd->listBox ? wnd->listBox->ItemsCount() : 0;
    int selectedCmdId = 0;
    if (sel >= 0 && sel < n) {
        auto* model = (ListBoxModelCP*)wnd->listBox->model;
        ItemDataCP* data = model->Data(sel);
        selectedCmdId = data ? data->cmdId : 0;
    }
    int qStart = 0, qEnd = 0, qLen = 0;
    EditGetSelection(wnd->editQuery, qStart, qEnd);
    qLen = EditGetTextLen(wnd->editQuery);
    int thumbPage = wnd->thumbnailCtrl ? wnd->thumbnailCtrl->selectedPage : 0;
    int rendered = wnd->thumbnailCtrl ? wnd->thumbnailCtrl->RenderedCount() : 0;
    out.Append(fmt("OK sel=%d items=%d querySel=%d,%d queryLen=%d cmd=%d rtl=%d thumb=%d page=%d rendered=%d\n", sel, n,
                   qStart, qEnd, qLen, selectedCmdId, (int)CommandPaletteUiRtl(), (int)wnd->thumbnailMode, thumbPage,
                   rendered));
    return finish(0);
}
