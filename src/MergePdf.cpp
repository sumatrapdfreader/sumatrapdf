/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "base/Pixmap.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/UITask.h"
#include "gui/Dpi.h"

#include <commdlg.h>

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
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkMode.h"
#include "PageThumbnails.h"
#include "MergePdf.h"

// Merge PDF (issue #6070): the pages of the document and of PDFs added to it
// as a grid of thumbnails. Pages are dragged to a new place and marked as
// removed (not dropped, so they can be restored); the result is saved over the
// document or as a new PDF.
//
//   +----------------------------------------------------------+
//   |  [1]   [2]   [x3]   [1]   [2] ...     thumbnails (MergeGrid)
//   +----------------------------------------------------------+
//   [Add PDF...] [Remove] [Restore] 7 pages, 1 removed  ■ a.pdf  ■ b.pdf  [Save] [Save As...] [Cancel]

constexpr int kThumbDx = 140;
constexpr int kThumbDy = 198;
constexpr int kGap = 24;
constexpr int kDialogPadding = 10;
constexpr int kDialogDx = 1100;
constexpr int kScrollbarDx = 10;
// a page's source color: a strip along its top
constexpr int kStripDy = 5;
// the remove / restore button in a page's top-right corner
constexpr int kCornerBtnDx = 24;
constexpr int kCornerBtnInset = 6;
constexpr int kDropBarDx = 3;
constexpr int kRenderScreens = 1;
constexpr int kKeepScreens = 2;
constexpr UINT_PTR kAutoScrollTimerId = 1;
constexpr UINT kAutoScrollMs = 40;
constexpr Color kDropBarColor = MkRgb(0, 120, 215);
constexpr Color kRemoveBtnColor = MkRgb(90, 90, 90);
constexpr Color kRestoreBtnColor = MkRgb(0, 120, 215);

static Pixmap* const kRenderFailed = (Pixmap*)(intptr_t)-1;

// the files' colors on their pages, in the order they were added
static const Color kSourceColors[] = {
    MkRgb(0x1f, 0x77, 0xb4), MkRgb(0xff, 0x7f, 0x0e), MkRgb(0x2c, 0xa0, 0x2c),
    MkRgb(0xd6, 0x27, 0x28), MkRgb(0x94, 0x67, 0xbd), MkRgb(0x8c, 0x56, 0x4b),
    MkRgb(0xe3, 0x77, 0xc2), MkRgb(0xbc, 0xbd, 0x22), MkRgb(0x17, 0xbe, 0xcf),
};

struct MergeGrid;
struct MergePdfWnd;

// a PDF whose pages are in the grid
struct MergeSource {
    Str path;     // owned
    Str password; // owned
    // a reference; renders the thumbnails. For the document: its tab's engine
    EngineBase* engine = nullptr;
    int pageCount = 0;
    Color color = 0;
    // by pageNo - 1; UI thread only
    Vec<Pixmap*> thumbs;
};

// The sources, shared by the dialog and the thread rendering their thumbnails.
// Deleted by whichever of the two is done last
struct MergeSources {
    Vec<MergeSource*> all;
    // null once the dialog is gone
    MergeGrid* grid = nullptr;
    AtomicInt cancel = 0;
    bool workerRunning = false;
};

struct MergeItem {
    MergeSource* src = nullptr;
    int pageNo = 0;
    bool removed = false;
    bool selected = false;
};

enum class MergePress {
    None,
    Page,
    CornerBtn,
    Band,
};

struct MergeRowsModel : ListBoxModel {
    int rows = 0;

    int ItemsCount() override { return rows; }
    Str Item(int) override { return {}; }
};

// The pages as a grid of thumbnails. Click / Ctrl / Shift click and a rubber
// band select pages, dragging moves the selected ones
struct MergeGrid : VirtListBox {
    MergePdfWnd* wnd = nullptr;
    MergeSources* sources = nullptr;
    MergeRowsModel* rowsModel = nullptr;
    Vec<MergeItem> items;
    int cols = 1;
    int thumbDx = 0;
    int thumbDy = 0;
    int gap = 0;
    int hoverIdx = -1;
    // the keyboard's page
    int focusIdx = 0;
    int anchorIdx = 0;

    MergePress press = MergePress::None;
    int pressIdx = -1;
    Point pressPt; // window coords
    bool dragging = false;
    // while dragging pages or files: they'd go in front of this item
    // (len(items): at the end); -1 when not over the grid
    int dropBefore = -1;
    // the rubber band, in content coords (y + scrollY)
    Point bandFrom;
    Point bandTo;
    // the selection the band started from (Ctrl adds to it)
    Vec<u8> bandBase;
    // where the mouse was last, local coords; auto scroll goes on from it
    Point lastPt;
    bool autoScrolling = false;

    MergeGrid(MergePdfWnd*, MergeSources*, PlatformFont*, int dpi);
    ~MergeGrid() override;

    void SetBounds(Rect) override;
    void Paint(VirtPaintCtx&) override;
    void DrawRow(DrawItemEvent*);
    void DrawCell(Gfx*, int idx);
    void OnGridMouseDown(VirtMouseEvent*);
    void OnGridMouseMove(VirtMouseEvent*);
    void OnGridMouseUp(VirtMouseEvent*);
    void OnGridMouseWheel(VirtMouseEvent*);
    void OnGridKeyDown(VirtKeyEvent*);
    void OnGridCaptureLost();
    void OnGridMouseLeave();
    void OnGridTooltip(VirtTooltipEvent*);
    void AutoScrollStep();
    void EndPress();

    void ItemsChanged();
    void Changed();
    void StartRendering();
    Rect CellRect(int idx);
    Rect CornerBtnRect(int idx);
    int ItemAt(Point ptLocal);
    int DropPositionAt(Point ptLocal);
    void SetDropBefore(int);
    void MoveSelected(int before);
    void SetRemoved(bool removed);
    void InsertPages(MergeSource*, int at);
    int SelectedCount();
    int SelectedRemovedCount();
    int InsertPosition();
    void SelectOnly(int idx);
    void SelectAll();
    void ShowContextMenu();

    int FirstVisibleIdx();
    int LastVisibleIdx();

  protected:
    int GridLeft();
    void SetHover(int idx);
    void UpdateBand();
    void UpdateAutoScroll();
    void MoveFocus(int idx, bool shift);
};

struct MergePdfWnd : WindowBase {
    MainWindow* win = nullptr;
    // the document the dialog was opened for; what Save writes
    Str docPath; // owned
    MergeSources* sources = nullptr;
    MergeGrid* grid = nullptr;
    VirtButton* btnAdd = nullptr;
    VirtButton* btnRemove = nullptr;
    VirtButton* btnRestore = nullptr;
    VirtButton* btnSave = nullptr;
    VirtButton* btnSaveAs = nullptr;
    VirtButton* btnCancel = nullptr;
    // the page count, then the files' colors
    VirtCustom* info = nullptr;
    bool tornDown = false;

    ~MergePdfWnd() override;

    bool Create(MainWindow*, WindowTab*);
    MergeSource* AddSource(Str path, EngineBase* engine);
    bool AddPdf(Str path, int at);
    void UpdateUI();
    bool SaveTo(Str destPath);
    void AfterSave(Str path);

    void OnAdd(VirtMouseEvent* ev = nullptr);
    void OnRemove(VirtMouseEvent* ev = nullptr);
    void OnRestore(VirtMouseEvent* ev = nullptr);
    void OnSave(VirtMouseEvent* ev = nullptr);
    void OnSaveAs(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnKey(KeyEvent* ev);
    void OnTimer(WindowBase::TimerEvent* ev);
    void PaintInfo(VirtPaintCtx* ctx);
};

static MergePdfWnd* gMergeWnd = nullptr;

//--- thumbnails, rendered in the background

struct MergeThumbTask {
    MergeSources* sources = nullptr;
    MergeSource* src = nullptr;
    int pageNo = 0;
    Pixmap* bitmap = nullptr;
};

struct MergeRenderBatch {
    MergeSources* sources = nullptr;
    Vec<MergeSource*> srcs;
    Vec<int> pageNos;
    int thumbDx = 0;
    int thumbDy = 0;
};

static void FreeThumb(Pixmap* thumb) {
    if (thumb != kRenderFailed) {
        FreePixmap(thumb);
    }
}

static void DeleteMergeSources(MergeSources* sources) {
    ReportIf(sources->workerRunning);
    for (MergeSource* src : sources->all) {
        for (Pixmap* thumb : src->thumbs) {
            FreeThumb(thumb);
        }
        if (src->engine) {
            src->engine->Release();
        }
        str::Free(src->path);
        str::Free(src->password);
        delete src;
    }
    delete sources;
}

static void FinishThumb(MergeThumbTask* task) {
    MergeSources* sources = task->sources;
    MergeSource* src = task->src;
    int idx = task->pageNo - 1;
    if (sources->grid && idx >= 0 && idx < len(src->thumbs) && !src->thumbs[idx]) {
        src->thumbs[idx] = task->bitmap ? task->bitmap : kRenderFailed;
        task->bitmap = nullptr;
        sources->grid->Invalidate();
    }
    FreePixmap(task->bitmap);
    delete task;
}

static void FinishRenderBatch(MergeSources* sources) {
    sources->workerRunning = false;
    if (!sources->grid) {
        DeleteMergeSources(sources);
        return;
    }
    // the grid may have scrolled meanwhile
    sources->grid->StartRendering();
}

static void RenderBatchAsync(MergeRenderBatch* batch) {
    MergeSources* sources = batch->sources;
    for (int i = 0; i < len(batch->pageNos); i++) {
        if (AtomicIntGet(&sources->cancel) != 0) {
            break;
        }
        auto* task = new MergeThumbTask;
        task->sources = sources;
        task->src = batch->srcs[i];
        task->pageNo = batch->pageNos[i];
        task->bitmap =
            RenderPageThumbnail(task->src->engine, task->pageNo, kInvalidLocation, 0, batch->thumbDx, batch->thumbDy);
        uitask::Post(MkFunc0<MergeThumbTask>(FinishThumb, task));
    }
    uitask::Post(MkFunc0<MergeSources>(FinishRenderBatch, sources));
    delete batch;
}

//--- MergeGrid

MergeGrid::MergeGrid(MergePdfWnd* wnd, MergeSources* sources, PlatformFont* font, int dpi) {
    this->wnd = wnd;
    this->sources = sources;
    this->font = font;
    this->dpi = dpi;
    thumbDx = DpiScaleByDpi(dpi, kThumbDx);
    thumbDy = DpiScaleByDpi(dpi, kThumbDy);
    gap = DpiScaleByDpi(dpi, kGap);
    itemDy = thumbDy + gap;
    int pad = gap / 2;
    padding = {pad, pad, pad, pad};
    SetColor(kColListText, ThemeWindowTextColor());
    SetColor(kColListBg, ThemeWindowControlBackgroundColor());

    rowsModel = new MergeRowsModel();
    SetModel(rowsModel);

    onDrawItem = MkMethod1<MergeGrid, DrawItemEvent*, &MergeGrid::DrawRow>(this);
    VirtCtrl::onMouseDown = MkMethod1<MergeGrid, VirtMouseEvent*, &MergeGrid::OnGridMouseDown>(this);
    VirtCtrl::onMouseMove = MkMethod1<MergeGrid, VirtMouseEvent*, &MergeGrid::OnGridMouseMove>(this);
    VirtCtrl::onMouseUp = MkMethod1<MergeGrid, VirtMouseEvent*, &MergeGrid::OnGridMouseUp>(this);
    VirtCtrl::onMouseWheel = MkMethod1<MergeGrid, VirtMouseEvent*, &MergeGrid::OnGridMouseWheel>(this);
    // a second quick click is just another click
    VirtCtrl::onDoubleClick = {};
    VirtCtrl::onKeyDown = MkMethod1<MergeGrid, VirtKeyEvent*, &MergeGrid::OnGridKeyDown>(this);
    VirtCtrl::onCaptureLost = MkMethod0<MergeGrid, &MergeGrid::OnGridCaptureLost>(this);
    VirtCtrl::onMouseLeave = MkMethod0<MergeGrid, &MergeGrid::OnGridMouseLeave>(this);
    onGetTooltip = MkMethod1<MergeGrid, VirtTooltipEvent*, &MergeGrid::OnGridTooltip>(this);
}

MergeGrid::~MergeGrid() = default;

// the number of rows follows the items; keeps the scroll position
void MergeGrid::ItemsChanged() {
    int oldScrollY = scrollY;
    rowsModel->rows = (len(items) + cols - 1) / cols;
    SetModel(rowsModel);
    ScrollTo(oldScrollY);
    focusIdx = ClampI(focusIdx, 0, std::max(0, len(items) - 1));
    anchorIdx = ClampI(anchorIdx, 0, std::max(0, len(items) - 1));
    hoverIdx = -1;
    Changed();
    StartRendering();
}

// the selection or the pages changed: the dialog's buttons and status follow
void MergeGrid::Changed() {
    Invalidate();
    if (wnd) {
        wnd->UpdateUI();
    }
}

void MergeGrid::SetBounds(Rect r) {
    int availableDx = r.dx - padding.left - padding.right - DpiScaleByDpi(dpi, kScrollbarDx);
    int newCols = std::max(1, (availableDx + gap) / (thumbDx + gap));
    VirtListBox::SetBounds(r);
    if (newCols != cols) {
        cols = newCols;
        ItemsChanged();
    }
    StartRendering();
}

// x of the first column, local coords: the grid is centered
int MergeGrid::GridLeft() {
    int sbDx = MaxScrollY() > 0 ? DpiScaleByDpi(dpi, kScrollbarDx) : 0;
    int itemsDx = bounds.dx - padding.left - padding.right - sbDx;
    int gridDx = (cols * thumbDx) + ((cols - 1) * gap);
    return padding.left + std::max(0, (itemsDx - gridDx) / 2);
}

// the thumbnail of item idx, local coords
Rect MergeGrid::CellRect(int idx) {
    int row = idx / cols;
    int col = idx % cols;
    return {GridLeft() + (col * (thumbDx + gap)), padding.top + (row * itemDy) - scrollY, thumbDx, thumbDy};
}

Rect MergeGrid::CornerBtnRect(int idx) {
    Rect r = CellRect(idx);
    int dx = DpiScaleByDpi(dpi, kCornerBtnDx);
    int inset = DpiScaleByDpi(dpi, kCornerBtnInset);
    return {r.x + r.dx - dx - inset, r.y + inset, dx, dx};
}

// -1 when not on a thumbnail
int MergeGrid::ItemAt(Point pt) {
    if (pt.y < padding.top || pt.y >= padding.top + ViewportDy()) {
        return -1;
    }
    int row = (pt.y - padding.top + scrollY) / itemDy;
    int col = (pt.x - GridLeft()) / (thumbDx + gap);
    if (pt.x < GridLeft() || col >= cols) {
        return -1;
    }
    int idx = (row * cols) + col;
    if (idx >= len(items) || !CellRect(idx).Contains(pt)) {
        return -1;
    }
    return idx;
}

// Where pages dropped at pt go: in front of the returned item, at the gap
// nearest to the point (len(items): after the last)
int MergeGrid::DropPositionAt(Point pt) {
    int n = len(items);
    if (pt.x < 0 || pt.y < 0 || pt.x >= bounds.dx || pt.y >= bounds.dy) {
        return -1;
    }
    int y = pt.y - padding.top + scrollY;
    int row = std::max(0, y / itemDy);
    if (y < 0) {
        row = 0;
    }
    if (row >= rowsModel->rows) {
        return n;
    }
    // left of a thumbnail's middle: in front of it
    int step = thumbDx + gap;
    int x = pt.x - GridLeft() - (thumbDx / 2);
    int slot = x < 0 ? 0 : ClampI((x / step) + 1, 0, cols);
    return std::min((row * cols) + slot, n);
}

void MergeGrid::SetDropBefore(int before) {
    if (before == dropBefore) {
        return;
    }
    dropBefore = before;
    Invalidate();
}

int MergeGrid::FirstVisibleIdx() {
    return (scrollY / itemDy) * cols;
}

int MergeGrid::LastVisibleIdx() {
    int rows = (ViewportDy() / itemDy) + 2;
    return std::min(len(items), FirstVisibleIdx() + (rows * cols)) - 1;
}

void MergeGrid::DrawRow(DrawItemEvent* ev) {
    int first = ev->itemIndex * cols;
    int last = std::min(len(items), first + cols) - 1;
    for (int idx = first; idx <= last; idx++) {
        DrawCell(ev->gfx, idx);
    }
}

// a circular arrow: the restore button's glyph
static void DrawRestoreGlyph(Gfx* gfx, Rect r, Color col, float thickness) {
    constexpr int kSegments = 10;
    constexpr float kPi = 3.14159265f;
    float cx = r.x + (r.dx / 2.f);
    float cy = r.y + (r.dy / 2.f);
    float radius = r.dx * 0.28f;
    // from the top, counterclockwise around to the right
    float from = -kPi / 2.f;
    float sweep = -1.5f * kPi;
    Point prev{(int)(cx + (radius * cosf(from))), (int)(cy + (radius * sinf(from)))};
    for (int i = 1; i <= kSegments; i++) {
        float a = from + ((sweep * i) / kSegments);
        Point pt{(int)(cx + (radius * cosf(a))), (int)(cy + (radius * sinf(a)))};
        gfx->DrawLineAA(prev, pt, col, thickness);
        prev = pt;
    }
    // the arrowhead at the start, pointing right
    Point tip{(int)cx + 1, (int)(cy - radius)};
    int head = (int)(radius * 0.7f);
    gfx->DrawLineAA(tip, {tip.x - head, tip.y - head}, col, thickness);
    gfx->DrawLineAA(tip, {tip.x - head, tip.y + head}, col, thickness);
}

void MergeGrid::DrawCell(Gfx* gfx, int idx) {
    MergeItem& item = items[idx];
    Point origin = OriginInWindow();
    Rect r = CellRect(idx);
    r.Offset(origin.x, origin.y);
    Color colBg = GetColor(kColListBg);

    // the selection, or the page under the mouse: a frame in the gap around it
    Color colFrame = kColorUnset;
    if (item.selected) {
        colFrame = AccentColor(colBg, HasFlag(vwfFocused) ? 60 : 40);
    } else if (idx == hoverIdx && press == MergePress::None) {
        colFrame = AccentColor(colBg, 15);
    }
    if (colFrame != kColorUnset) {
        Rect fr = r;
        int d = gap / 3;
        fr.Inflate(d, d);
        gfx->FillRoundedRect(fr, d, colFrame);
    }
    gfx->FillRect(r, kColWhite);
    // white pages on a white grid need an edge
    Rect edge = r;
    edge.Inflate(1, 1);
    gfx->DrawRect(edge, AccentColor(colBg, 30));

    Pixmap* thumb = item.src->thumbs[item.pageNo - 1];
    if (thumb && thumb != kRenderFailed) {
        int drawDx = std::min(thumb->width, thumbDx);
        int drawDy = std::min(thumb->height, thumbDy);
        Rect target{r.x + ((thumbDx - drawDx) / 2), r.y + ((thumbDy - drawDy) / 2), drawDx, drawDy};
        gfx->DrawPixmap(thumb, target);
    }
    if (len(sources->all) > 1) {
        gfx->FillRect({r.x, r.y, r.dx, DpiScaleByDpi(dpi, kStripDy)}, item.src->color);
    }
    if (item.removed) {
        gfx->FillRects(&r, 1, colBg, 190);
    }
    if (idx == focusIdx && HasFlag(vwfFocused)) {
        Rect fr = r;
        fr.Inflate(2, 2);
        gfx->DrawFocusRect(fr);
    }

    // the page number, in a pill at the bottom
    TempStr label = fmt("%d", item.pageNo);
    Size ts = gfx->MeasureText(label, font);
    int padX = DpiScaleByDpi(dpi, 6);
    int padY = DpiScaleByDpi(dpi, 2);
    int inset = DpiScaleByDpi(dpi, 4);
    int boxDx = std::min(ts.dx + (padX * 2), r.dx - (inset * 2));
    int boxDy = ts.dy + (padY * 2);
    Rect box{r.x + ((r.dx - boxDx) / 2), r.y + r.dy - boxDy - inset, boxDx, boxDy};
    gfx->FillRoundedRect(box, boxDy / 2, ThemeWindowBackgroundColor());
    gfx->DrawText(label, box, gfxTextCenter | gfxTextVCenter, font, ThemeWindowTextColor());

    // remove on the page under the mouse, restore on every removed page
    if (!item.removed && (idx != hoverIdx || dragging)) {
        return;
    }
    Rect btn = CornerBtnRect(idx);
    btn.Offset(origin.x, origin.y);
    gfx->FillEllipse(btn, item.removed ? kRestoreBtnColor : kRemoveBtnColor, 230);
    float thickness = (float)DpiScaleByDpi(dpi, 2);
    if (item.removed) {
        DrawRestoreGlyph(gfx, btn, kColWhite, thickness);
        return;
    }
    int d = btn.dx * 3 / 10;
    Point c{btn.x + (btn.dx / 2), btn.y + (btn.dy / 2)};
    gfx->DrawLineAA({c.x - d, c.y - d}, {c.x + d, c.y + d}, kColWhite, thickness);
    gfx->DrawLineAA({c.x - d, c.y + d}, {c.x + d, c.y - d}, kColWhite, thickness);
}

// the thumbnails, then where dragged pages or files would go and the rubber band
void MergeGrid::Paint(VirtPaintCtx& ctx) {
    VirtListBox::Paint(ctx);
    Point origin = OriginInWindow();
    ctx.gfx->PushClip(ctx.clip.Intersect(ctx.bounds));
    int n = len(items);
    if (dropBefore >= 0 && n > 0) {
        bool atEnd = dropBefore >= n;
        Rect r = CellRect(atEnd ? n - 1 : dropBefore);
        r.Offset(origin.x, origin.y);
        int barDx = DpiScaleByDpi(dpi, kDropBarDx);
        int x = atEnd ? r.x + thumbDx + (gap / 2) : r.x - (gap / 2);
        ctx.gfx->FillRect({x - (barDx / 2), r.y, barDx, thumbDy}, kDropBarColor);
    }
    if (press == MergePress::Band) {
        int x0 = std::min(bandFrom.x, bandTo.x);
        int y0 = std::min(bandFrom.y, bandTo.y) - scrollY;
        Rect band{origin.x + x0, origin.y + y0, std::abs(bandTo.x - bandFrom.x), std::abs(bandTo.y - bandFrom.y)};
        ctx.gfx->FillRects(&band, 1, kDropBarColor, 40);
        ctx.gfx->DrawRect(band, kDropBarColor);
    }
    ctx.gfx->PopClip();
}

int MergeGrid::SelectedCount() {
    int n = 0;
    for (MergeItem& it : items) {
        n += it.selected ? 1 : 0;
    }
    return n;
}

int MergeGrid::SelectedRemovedCount() {
    int n = 0;
    for (MergeItem& it : items) {
        n += (it.selected && it.removed) ? 1 : 0;
    }
    return n;
}

// where added pages go: after the last selected page, else at the end
int MergeGrid::InsertPosition() {
    for (int i = len(items) - 1; i >= 0; i--) {
        if (items[i].selected) {
            return i + 1;
        }
    }
    return len(items);
}

void MergeGrid::SelectOnly(int idx) {
    for (int i = 0; i < len(items); i++) {
        items[i].selected = i == idx;
    }
    anchorIdx = idx;
    focusIdx = idx;
    Changed();
}

void MergeGrid::SelectAll() {
    for (MergeItem& it : items) {
        it.selected = true;
    }
    Changed();
}

// the selected pages go together in front of item `before` and stay selected
void MergeGrid::MoveSelected(int before) {
    Vec<MergeItem> moved;
    Vec<MergeItem> rest;
    int at = before;
    for (int i = 0; i < len(items); i++) {
        if (!items[i].selected) {
            VecAppend(rest, items[i]);
            continue;
        }
        VecAppend(moved, items[i]);
        if (i < before) {
            at--;
        }
    }
    if (len(moved) == 0) {
        return;
    }
    VecReset(items);
    for (int i = 0; i < at; i++) {
        VecAppend(items, rest[i]);
    }
    VecAppendVec(items, moved);
    for (int i = at; i < len(rest); i++) {
        VecAppend(items, rest[i]);
    }
    focusIdx = at;
    anchorIdx = at;
    ItemsChanged();
}

void MergeGrid::SetRemoved(bool removed) {
    for (MergeItem& it : items) {
        if (it.selected) {
            it.removed = removed;
        }
    }
    Changed();
}

// every page of src, selected, in front of item `at`
void MergeGrid::InsertPages(MergeSource* src, int at) {
    for (MergeItem& it : items) {
        it.selected = false;
    }
    at = ClampI(at, 0, len(items));
    for (int i = 0; i < src->pageCount; i++) {
        MergeItem it;
        it.src = src;
        it.pageNo = i + 1;
        it.selected = true;
        VecInsertAt(items, at + i, it);
    }
    focusIdx = at;
    anchorIdx = at;
    ItemsChanged();
    EnsureVisible(at / cols);
}

void MergeGrid::SetHover(int idx) {
    if (idx == hoverIdx) {
        return;
    }
    hoverIdx = idx;
    Invalidate();
}

// selects what the band touches, on top of what was selected when it started
void MergeGrid::UpdateBand() {
    Rect band{std::min(bandFrom.x, bandTo.x), std::min(bandFrom.y, bandTo.y), std::abs(bandTo.x - bandFrom.x),
              std::abs(bandTo.y - bandFrom.y)};
    band.Offset(0, -scrollY);
    for (int i = 0; i < len(items); i++) {
        bool base = i < len(bandBase) && bandBase[i];
        items[i].selected = base || !band.Intersect(CellRect(i)).IsEmpty();
    }
    Changed();
}

// near the top or bottom edge a drag scrolls, for as long as the mouse stays there
void MergeGrid::UpdateAutoScroll() {
    int edge = thumbDy / 4;
    bool needed = (dragging || press == MergePress::Band) && (lastPt.y < edge || lastPt.y > bounds.dy - edge);
    if (needed == autoScrolling) {
        return;
    }
    autoScrolling = needed;
    HWND hwnd = GetHwnd();
    if (needed) {
        SetTimer(hwnd, kAutoScrollTimerId, kAutoScrollMs, nullptr);
    } else {
        KillTimer(hwnd, kAutoScrollTimerId);
    }
}

void MergeGrid::AutoScrollStep() {
    int edge = thumbDy / 4;
    int dy = 0;
    if (lastPt.y < edge) {
        dy = -(edge - lastPt.y);
    } else if (lastPt.y > bounds.dy - edge) {
        dy = lastPt.y - (bounds.dy - edge);
    }
    if (dy == 0 || !ScrollBy(dy)) {
        return;
    }
    if (dragging) {
        SetDropBefore(DropPositionAt(lastPt));
    } else if (press == MergePress::Band) {
        bandTo = {lastPt.x, lastPt.y + scrollY};
        UpdateBand();
    }
    StartRendering();
}

void MergeGrid::EndPress() {
    press = MergePress::None;
    pressIdx = -1;
    dragging = false;
    VecReset(bandBase);
    SetDropBefore(-1);
    UpdateAutoScroll();
    if (root && root->captured == this) {
        root->ReleaseCapture();
    }
    Invalidate();
}

void MergeGrid::OnGridMouseDown(VirtMouseEvent* ev) {
    ev->didHandle = true;
    Point pt = ev->pt;
    lastPt = pt;
    // the scrollbar strip is the list's own
    bool onScrollbar = MaxScrollY() > 0 && pt.x >= bounds.dx - padding.right - DpiScaleByDpi(dpi, kScrollbarDx);
    if (onScrollbar) {
        int oldScrollY = scrollY;
        VirtListBox::OnMouseDown(ev);
        if (scrollY != oldScrollY) {
            StartRendering();
        }
        return;
    }
    int idx = ItemAt(pt);
    if (ev->button == 1) {
        // right button: the menu acts on the selection, which includes the page
        if (idx >= 0 && !items[idx].selected) {
            SelectOnly(idx);
        }
        return;
    }
    if (ev->button != 0) {
        return;
    }
    if (root) {
        root->SetCapture(this);
    }
    pressPt = ev->ptWindow;
    if (idx >= 0 && CornerBtnRect(idx).Contains(pt)) {
        press = MergePress::CornerBtn;
        pressIdx = idx;
        return;
    }
    if (idx < 0) {
        // a rubber band; Ctrl adds what it touches to the selection
        press = MergePress::Band;
        bandFrom = {pt.x, pt.y + scrollY};
        bandTo = bandFrom;
        VecReset(bandBase);
        for (MergeItem& it : items) {
            VecAppend(bandBase, (u8)(ev->isCtrl && it.selected ? 1 : 0));
        }
        UpdateBand();
        return;
    }
    press = MergePress::Page;
    pressIdx = idx;
    focusIdx = idx;
    if (ev->isShift) {
        for (int i = 0; i < len(items); i++) {
            bool inRange = i >= std::min(anchorIdx, idx) && i <= std::max(anchorIdx, idx);
            items[i].selected = inRange || (ev->isCtrl && items[i].selected);
        }
    } else if (ev->isCtrl) {
        items[idx].selected = !items[idx].selected;
        anchorIdx = idx;
    } else if (!items[idx].selected) {
        // a plain click on a selected page may start dragging all of them, so
        // only on release does it select just that page
        SelectOnly(idx);
    }
    Changed();
}

void MergeGrid::OnGridMouseMove(VirtMouseEvent* ev) {
    int oldScrollY = scrollY;
    VirtListBox::OnMouseMove(ev);
    if (scrollY != oldScrollY) {
        StartRendering();
    }
    ev->didHandle = true;
    if (press == MergePress::None) {
        int idx = ItemAt(ev->pt);
        SetHover(idx);
        bool onBtn = idx >= 0 && CornerBtnRect(idx).Contains(ev->pt);
        cursor = onBtn ? CursorId::Hand : CursorId::None;
        return;
    }
    // captured: ev->pt is in window coords
    Point origin = OriginInWindow();
    Point pt{ev->ptWindow.x - origin.x, ev->ptWindow.y - origin.y};
    lastPt = pt;
    if (press == MergePress::Band) {
        bandTo = {pt.x, pt.y + scrollY};
        UpdateBand();
        UpdateAutoScroll();
        return;
    }
    if (press != MergePress::Page) {
        return;
    }
    if (!dragging) {
        int dx = std::abs(ev->ptWindow.x - pressPt.x);
        int dy = std::abs(ev->ptWindow.y - pressPt.y);
        if (dx <= GetSystemMetrics(SM_CXDRAG) && dy <= GetSystemMetrics(SM_CYDRAG)) {
            return;
        }
        dragging = true;
        if (!items[pressIdx].selected) {
            SelectOnly(pressIdx);
        }
    }
    SetDropBefore(DropPositionAt(pt));
    UpdateAutoScroll();
}

void MergeGrid::OnGridMouseUp(VirtMouseEvent* ev) {
    VirtListBox::OnMouseUp(ev);
    if (ev->button == 1) {
        ShowContextMenu();
        return;
    }
    MergePress what = press;
    int idx = pressIdx;
    bool wasDragging = dragging;
    int before = dropBefore;
    Point origin = OriginInWindow();
    Point pt{ev->ptWindow.x - origin.x, ev->ptWindow.y - origin.y};
    bool modifiers = ev->isCtrl || ev->isShift;
    EndPress();
    if (what == MergePress::CornerBtn) {
        // only if released over the button it went down on
        if (ItemAt(pt) == idx && CornerBtnRect(idx).Contains(pt)) {
            items[idx].removed = !items[idx].removed;
            Changed();
        }
        return;
    }
    if (what != MergePress::Page) {
        return;
    }
    if (wasDragging) {
        if (before >= 0) {
            MoveSelected(before);
        }
        return;
    }
    if (!modifiers) {
        SelectOnly(idx);
    }
}

void MergeGrid::OnGridCaptureLost() {
    VirtListBox::OnCaptureLost();
    press = MergePress::None;
    dragging = false;
    SetDropBefore(-1);
    UpdateAutoScroll();
}

void MergeGrid::OnGridMouseLeave() {
    if (press == MergePress::None) {
        SetHover(-1);
    }
}

void MergeGrid::OnGridMouseWheel(VirtMouseEvent* ev) {
    ev->didHandle = true;
    int dy = -(ev->wheelDelta * GetItemHeight()) / WHEEL_DELTA;
    if (ScrollBy(dy)) {
        SetHover(-1);
        StartRendering();
    }
}

void MergeGrid::OnGridTooltip(VirtTooltipEvent* ev) {
    int idx = ItemAt(ev->ptLocal);
    if (idx < 0) {
        return;
    }
    MergeItem& item = items[idx];
    if (CornerBtnRect(idx).Contains(ev->ptLocal)) {
        ev->tip = str::DupTemp(item.removed ? Tr("Restore page") : Tr("Remove page"));
        return;
    }
    ev->tip = fmt(Tr("%s, page %d").s, path::GetBaseNameTemp(item.src->path), item.pageNo);
}

// the keyboard's page moves; Shift selects from the anchor to it
void MergeGrid::MoveFocus(int idx, bool shift) {
    idx = ClampI(idx, 0, len(items) - 1);
    if (!shift) {
        SelectOnly(idx);
    } else {
        focusIdx = idx;
        for (int i = 0; i < len(items); i++) {
            items[i].selected = i >= std::min(anchorIdx, idx) && i <= std::max(anchorIdx, idx);
        }
        Changed();
    }
    EnsureVisible(idx / cols);
    StartRendering();
}

// arrows move through the pages, Delete removes (or restores) the selected ones
void MergeGrid::OnGridKeyDown(VirtKeyEvent* ev) {
    int n = len(items);
    if (n == 0) {
        return;
    }
    ev->didHandle = true;
    int pageStep = std::max(1, ViewportDy() / itemDy) * cols;
    switch (ev->vkey) {
        case VK_LEFT:
            MoveFocus(focusIdx - 1, ev->isShift);
            return;
        case VK_RIGHT:
            MoveFocus(focusIdx + 1, ev->isShift);
            return;
        case VK_UP:
            MoveFocus(focusIdx - cols, ev->isShift);
            return;
        case VK_DOWN:
            MoveFocus(focusIdx + cols, ev->isShift);
            return;
        case VK_PRIOR:
            MoveFocus(focusIdx - pageStep, ev->isShift);
            return;
        case VK_NEXT:
            MoveFocus(focusIdx + pageStep, ev->isShift);
            return;
        case VK_HOME:
            MoveFocus(0, ev->isShift);
            return;
        case VK_END:
            MoveFocus(n - 1, ev->isShift);
            return;
        case VK_DELETE:
            // all removed already: restore them
            SetRemoved(SelectedRemovedCount() < SelectedCount());
            return;
        case VK_APPS:
            ShowContextMenu();
            return;
    }
    if (ev->isCtrl && ev->vkey == 'A') {
        SelectAll();
        return;
    }
    ev->didHandle = false;
}

enum {
    kMenuRemove = 1,
    kMenuRestore,
    kMenuSelectAll,
};

void MergeGrid::ShowContextMenu() {
    int nSel = SelectedCount();
    int nRemoved = SelectedRemovedCount();
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (nSel > nRemoved ? 0 : MF_GRAYED), kMenuRemove, CWStrTemp(Tr("&Remove")));
    AppendMenuW(menu, MF_STRING | (nRemoved > 0 ? 0 : MF_GRAYED), kMenuRestore, CWStrTemp(Tr("R&estore")));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuSelectAll, CWStrTemp(Tr("Select &All\tCtrl+A")));
    POINT pt{};
    GetCursorPos(&pt);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, GetHwnd(), nullptr);
    DestroyMenu(menu);
    if (cmd == kMenuRemove || cmd == kMenuRestore) {
        SetRemoved(cmd == kMenuRemove);
    } else if (cmd == kMenuSelectAll) {
        SelectAll();
    }
}

// Renders the thumbnails around the visible ones that aren't yet, and frees
// those far from them
void MergeGrid::StartRendering() {
    int n = len(items);
    if (sources->workerRunning || n == 0 || ViewportDy() <= 0) {
        return;
    }
    int firstVisible = FirstVisibleIdx();
    int lastVisible = LastVisibleIdx();
    int perScreen = std::max(cols, lastVisible - firstVisible + 1);
    int firstIdx = std::max(0, firstVisible - (kRenderScreens * perScreen));
    int lastIdx = std::min(n - 1, lastVisible + (kRenderScreens * perScreen));
    int keepFirst = std::max(0, firstIdx - (kKeepScreens * perScreen));
    int keepLast = std::min(n - 1, lastIdx + (kKeepScreens * perScreen));

    // keep[src][pageNo - 1]
    Vec<Vec<u8>*> keep;
    for (MergeSource* src : sources->all) {
        auto* k = new Vec<u8>();
        VecAppendBlanks(*k, src->pageCount);
        VecAppend(keep, k);
    }
    for (int i = keepFirst; i <= keepLast; i++) {
        int s = VecFind(sources->all, items[i].src);
        (*keep[s])[items[i].pageNo - 1] = 1;
    }
    for (int s = 0; s < len(sources->all); s++) {
        MergeSource* src = sources->all[s];
        for (int i = 0; i < len(src->thumbs); i++) {
            if (src->thumbs[i] && !(*keep[s])[i]) {
                FreeThumb(src->thumbs[i]);
                src->thumbs[i] = nullptr;
            }
        }
        delete keep[s];
    }

    auto* batch = new MergeRenderBatch;
    auto queue = [&](int idx) {
        MergeItem& it = items[idx];
        if (it.src->thumbs[it.pageNo - 1]) {
            return;
        }
        VecAppend(batch->srcs, it.src);
        VecAppend(batch->pageNos, it.pageNo);
    };
    for (int i = firstVisible; i <= lastVisible; i++) {
        queue(i);
    }
    for (int i = firstIdx; i < firstVisible; i++) {
        queue(i);
    }
    for (int i = lastVisible + 1; i <= lastIdx; i++) {
        queue(i);
    }
    if (len(batch->pageNos) == 0) {
        delete batch;
        return;
    }
    batch->sources = sources;
    batch->thumbDx = thumbDx;
    batch->thumbDy = thumbDy;
    AtomicIntSet(&sources->cancel, 0);
    sources->workerRunning = true;
    RunAsync(MkFunc0<MergeRenderBatch>(RenderBatchAsync, batch), StrL("MergePdfThumbnails"));
}

//--- dropping PDF files on the grid

static void PdfPathsFromDataObject(IDataObject* dataObj, StrVec& out) {
    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    if (FAILED(dataObj->GetData(&fmt, &medium)) || !medium.hGlobal) {
        return;
    }
    HDROP hDrop = (HDROP)medium.hGlobal;
    int nFiles = DragQueryFileW(hDrop, DRAGQUERY_NUMFILES, nullptr, 0);
    WCHAR pathW[MAX_PATH]{};
    for (int i = 0; i < nFiles; i++) {
        DragQueryFileW(hDrop, i, pathW, dimof(pathW));
        TempStr path = ToUtf8Temp(pathW);
        if (str::EndsWithI(path, StrL(".pdf"))) {
            out.Append(path);
        }
    }
    ReleaseStgMedium(&medium);
}

struct AddDroppedPdfs {
    MergePdfWnd* wnd = nullptr;
    StrVec paths;
    int before = 0;
};

// after Drop() returned, so the drag source isn't kept waiting
static void AddDroppedPdfsNow(AddDroppedPdfs* d) {
    if (gMergeWnd == d->wnd && !d->wnd->tornDown) {
        int before = d->before;
        for (Str path : d->paths) {
            int n = len(d->wnd->grid->items);
            d->wnd->AddPdf(path, before);
            before += len(d->wnd->grid->items) - n;
        }
    }
    delete d;
}

class MergeDropTarget : public IDropTarget {
    AtomicInt refCount = 1;
    MergePdfWnd* wnd = nullptr;
    bool hasPdf = false;

    int PositionAt(POINTL ptScreen) {
        if (!hasPdf) {
            return -1;
        }
        POINT p{ptScreen.x, ptScreen.y};
        ScreenToClient(wnd->hwnd, &p);
        Point origin = wnd->grid->OriginInWindow();
        return wnd->grid->DropPositionAt({p.x - origin.x, p.y - origin.y});
    }

  public:
    explicit MergeDropTarget(MergePdfWnd* w) : wnd(w) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AtomicIntInc(&refCount); }
    ULONG STDMETHODCALLTYPE Release() override {
        LONG r = InterlockedDecrement(&refCount);
        if (r == 0) {
            delete this;
        }
        return r;
    }

    STDMETHODIMP DragEnter(IDataObject* dataObj, DWORD keys, POINTL pt, DWORD* pdwEffect) override {
        StrVec paths;
        PdfPathsFromDataObject(dataObj, paths);
        hasPdf = len(paths) > 0;
        return DragOver(keys, pt, pdwEffect);
    }

    STDMETHODIMP DragOver(DWORD /*keys*/, POINTL pt, DWORD* pdwEffect) override {
        int before = PositionAt(pt);
        wnd->grid->SetDropBefore(before);
        *pdwEffect = before >= 0 ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }

    STDMETHODIMP DragLeave() override {
        wnd->grid->SetDropBefore(-1);
        return S_OK;
    }

    STDMETHODIMP Drop(IDataObject* dataObj, DWORD /*keys*/, POINTL pt, DWORD* pdwEffect) override {
        int before = PositionAt(pt);
        wnd->grid->SetDropBefore(-1);
        *pdwEffect = DROPEFFECT_NONE;
        if (before < 0) {
            return S_OK;
        }
        auto* d = new AddDroppedPdfs();
        PdfPathsFromDataObject(dataObj, d->paths);
        d->wnd = wnd;
        d->before = before;
        uitask::Post(MkFunc0<AddDroppedPdfs>(AddDroppedPdfsNow, d));
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
};

//--- MergePdfWnd

MergePdfWnd::~MergePdfWnd() {
    str::FreePtr(&docPath);
    // ~WindowBase deletes `layout`, which owns the grid and the buttons
}

// the dialog is going away: the sources go with it, or with the render thread
static void TeardownMergeWnd(MergePdfWnd* w) {
    if (w->tornDown) {
        return;
    }
    w->tornDown = true;
    RevokeDragDrop(w->hwnd);
    KillTimer(w->hwnd, kAutoScrollTimerId);
    MergeSources* sources = w->sources;
    sources->grid = nullptr;
    w->grid->sources = nullptr;
    w->grid->wnd = nullptr;
    AtomicIntSet(&sources->cancel, 1);
    if (!sources->workerRunning) {
        DeleteMergeSources(sources);
    }
    w->sources = nullptr;
    if (gMergeWnd == w) {
        gMergeWnd = nullptr;
    }
    if (w->win && IsMainWindowValidAndNotClosing(w->win)) {
        SetActiveWindow(w->win->hwndFrame);
    }
    w->ScheduleDelete();
}

static void OnMergeWndClose(WindowBase::CloseEvent* ev) {
    TeardownMergeWnd((MergePdfWnd*)ev->e->self);
}

static void OnMergeWndDestroy(WindowBase::DestroyEvent* ev) {
    TeardownMergeWnd((MergePdfWnd*)ev->e->self);
}

MergeSource* MergePdfWnd::AddSource(Str path, EngineBase* engine) {
    auto* src = new MergeSource();
    src->path = str::Dup(path);
    src->password = str::Dup(EngineMupdfGetPassword(engine));
    src->engine = engine;
    src->pageCount = engine->PageCount();
    src->color = kSourceColors[len(sources->all) % dimof(kSourceColors)];
    VecAppendBlanks(src->thumbs, src->pageCount);
    VecAppend(sources->all, src);
    return src;
}

// its pages go in front of item `at`
bool MergePdfWnd::AddPdf(Str path, int at) {
    EngineBase* engine = CreatePdfEngineForDialog(path, hwnd);
    if (!engine || engine->PageCount() <= 0) {
        if (engine) {
            engine->Release();
        }
        MessageBoxWarning(hwnd, fmt(Tr("Couldn't open '%s'").s, path::GetBaseNameTemp(path)), Tr("Merge PDF"));
        return false;
    }
    MergeSource* src = AddSource(path, engine);
    grid->InsertPages(src, at);
    return true;
}

// The page count, then which file a page comes from: its color and its name.
// One control that takes the free space: a longer text needs no layout, which
// would end a drag
void MergePdfWnd::PaintInfo(VirtPaintCtx* ctx) {
    int nPages = len(grid->items);
    int nRemoved = 0;
    for (MergeItem& it : grid->items) {
        nRemoved += it.removed ? 1 : 0;
    }
    TempStr s = fmt(Tr("%d pages").s, nPages - nRemoved);
    if (nRemoved > 0) {
        s = fmt(Tr("%d pages, %d removed").s, nPages - nRemoved, nRemoved);
    }
    Rect r = ctx->content;
    Color col = GetColor(kColWinText);
    int gap = font->averageCharWidth;
    ctx->gfx->PushClip(ctx->clip.Intersect(r));
    Size ts = ctx->gfx->MeasureText(s, font);
    ctx->gfx->DrawText(s, {r.x, r.y, ts.dx, r.dy}, gfxTextVCenter, font, col);
    int x = r.x + ts.dx + (gap * 4);
    int swatch = ts.dy * 2 / 3;
    for (int i = 0; len(sources->all) > 1 && i < len(sources->all); i++) {
        MergeSource* src = sources->all[i];
        TempStr name = path::GetBaseNameTemp(src->path);
        ts = ctx->gfx->MeasureText(name, font);
        ctx->gfx->FillRect({x, r.y + ((r.dy - swatch) / 2), swatch, swatch}, src->color);
        x += swatch + gap;
        ctx->gfx->DrawText(name, {x, r.y, ts.dx, r.dy}, gfxTextVCenter, font, col);
        x += ts.dx + (gap * 3);
    }
    ctx->gfx->PopClip();
}

static void PaintMergeInfo(MergePdfWnd* w, VirtPaintCtx* ctx) {
    w->PaintInfo(ctx);
}

// buttons and the page count after the pages or the selection changed
void MergePdfWnd::UpdateUI() {
    int nPages = len(grid->items);
    int nRemoved = 0;
    for (MergeItem& it : grid->items) {
        nRemoved += it.removed ? 1 : 0;
    }
    int nSel = grid->SelectedCount();
    int nSelRemoved = grid->SelectedRemovedCount();
    btnRemove->SetIsEnabled(nSel > nSelRemoved);
    btnRestore->SetIsEnabled(nSelRemoved > 0);
    bool canSave = nPages > nRemoved;
    btnSave->SetIsEnabled(canSave);
    btnSaveAs->SetIsEnabled(canSave);
    info->Invalidate();
}

//--- where Add PDF... puts the pages

//   Insert 'b.pdf':
//   ( ) At the end
//   ( ) At the beginning
//   (o) After page [ 3 ] (of 12)
//                     [OK] [Cancel]
struct InsertPosWnd : WindowBase {
    // where the answer goes: the caller's, which outlives the modal loop
    int* atOut = nullptr;
    int nItems = 0;
    Checkbox* rbEnd = nullptr;
    Checkbox* rbStart = nullptr;
    Checkbox* rbAfter = nullptr;
    Edit* editPage = nullptr;

    bool Create(HWND owner, PlatformFont*, Str what, int nItems, int afterPage);
    Checkbox* NewRadio(Str text, bool checked, bool groupStart);
    void OnPageEdited();
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
};

static void OnInsertPosClose(WindowBase::CloseEvent* ev) {
    ((InsertPosWnd*)ev->e->self)->ScheduleDelete();
}

Checkbox* InsertPosWnd::NewRadio(Str text, bool checked, bool groupStart) {
    Checkbox::CreateArgs args;
    args.parent = hwnd;
    args.text = text;
    args.font = font;
    args.isRtl = IsUIRtl();
    args.isRadio = true;
    args.isGroupStart = groupStart;
    args.initialState = checked ? Checkbox::State::Checked : Checkbox::State::Unchecked;
    auto* rb = new Checkbox();
    rb->Create(args);
    rb->SetColors(ThemeWindowTextColor(), DarkModeDialogBgColor());
    return rb;
}

// typing a page number picks "After page"
void InsertPosWnd::OnPageEdited() {
    if (rbAfter->IsChecked()) {
        return;
    }
    rbEnd->SetIsChecked(false);
    rbStart->SetIsChecked(false);
    rbAfter->SetIsChecked(true);
}

void InsertPosWnd::OnOk(VirtMouseEvent*) {
    int at = nItems;
    if (rbStart->IsChecked()) {
        at = 0;
    } else if (rbAfter->IsChecked()) {
        at = ClampI(ParseInt(editPage->GetTextTemp()), 0, nItems);
    }
    *atOut = at;
    ScheduleDelete();
}

void InsertPosWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

// afterPage: 1-based, 0 when nothing is selected (then "At the end" is picked)
bool InsertPosWnd::Create(HWND owner, PlatformFont* f, Str what, int n, int afterPage) {
    nItems = n;
    closeOnEsc = true;
    onClose = MkFunc1Void(OnInsertPosClose);
    SetFont(f);

    CreateCustomArgs cargs;
    cargs.owner = owner;
    cargs.title = Tr("Add PDF");
    cargs.font = font;
    cargs.style = WS_POPUPWINDOW | WS_CAPTION;
    cargs.visible = false;
    cargs.isRtl = IsUIRtl();
    cargs.bgColor = DarkModeDialogBgColor();
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }

    int gap = DpiScaleByDpi(GetDpi(), 6);
    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(NewVirtText({.s = fmt(Tr("Insert '%s':").s, what), .font = font, .isRtl = IsUIRtl()}));
    vbox->AddChild(new Spacer(0, gap));
    bool hasPage = afterPage > 0;
    rbEnd = NewRadio(Tr("At the &end"), !hasPage, true);
    vbox->AddChild(rbEnd);
    vbox->AddChild(new Spacer(0, gap));
    rbStart = NewRadio(Tr("At the &beginning"), false, false);
    vbox->AddChild(rbStart);
    vbox->AddChild(new Spacer(0, gap));

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->gap = font->averageCharWidth;
    rbAfter = NewRadio(Tr("&After page"), hasPage, false);
    row->AddChild(rbAfter);
    Edit::CreateArgs eargs;
    eargs.parent = hwnd;
    eargs.font = font;
    eargs.withBorder = true;
    eargs.numbersOnly = true;
    eargs.selectAllOnFocus = true;
    eargs.isRtl = IsUIRtl();
    eargs.text = fmt("%d", hasPage ? afterPage : nItems);
    eargs.idealWidthChars = 6;
    editPage = new Edit();
    editPage->Create(eargs);
    // set after Create(), which reports the initial text as a change
    editPage->onTextChanged = MkMethod0<InsertPosWnd, &InsertPosWnd::OnPageEdited>(this);
    row->AddChild(editPage);
    row->AddChild(NewVirtText({.s = fmt(Tr("(of %d)").s, nItems), .font = font, .isRtl = IsUIRtl()}));
    vbox->AddChild(row);
    vbox->AddChild(new Spacer(0, gap * 2));

    auto* buttons = new HBox();
    buttons->alignMain = MainAxisAlign::MainEnd;
    buttons->gap = font->averageCharWidth;
    auto* btnOk = NewThemedButton(hwnd, Tr("OK"), font, true);
    btnOk->onClick = MkMethod1<InsertPosWnd, VirtMouseEvent*, &InsertPosWnd::OnOk>(this);
    buttons->AddChild(btnOk);
    auto* btnCancel = NewThemedButton(hwnd, Tr("Cancel"), font, false);
    btnCancel->onClick = MkMethod1<InsertPosWnd, VirtMouseEvent*, &InsertPosWnd::OnCancel>(this);
    buttons->AddChild(btnCancel);
    vbox->AddChild(buttons);
    layout = new Padding(vbox, DpiScaledInsets(10, 10));

    LayoutAndSizeToContent(layout, DpiScale(260), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, owner);
    UpdateTheme();
    SetIsVisible(true);
    if (hasPage) {
        EditSelectAll(editPage);
        EditSetFocus(editPage);
    } else {
        HwndSetFocus(rbEnd->hwnd);
    }
    return true;
}

// Asks where to insert the pages of paths: at the end, the beginning or after a
// page, which starts as the last selected one. -1 when cancelled
static int AskInsertPosition(MergePdfWnd* w, const StrVec& paths) {
    int n = len(w->grid->items);
    int selected = w->grid->InsertPosition();
    int afterPage = w->grid->SelectedCount() > 0 ? selected : 0;
    Str what = path::GetBaseNameTemp(paths[0]);
    if (len(paths) > 1) {
        what = fmt(Tr("%d files").s, len(paths));
    }
    int at = -1;
    auto* dlg = new InsertPosWnd();
    dlg->atOut = &at;
    if (!dlg->Create(w->hwnd, w->font, what, n, afterPage)) {
        delete dlg;
        return -1;
    }
    RunModalWindow(dlg->hwnd, w->hwnd);
    return at;
}

void MergePdfWnd::OnAdd(VirtMouseEvent*) {
    // a multi-select buffer: the folder, then the file names, then an empty string
    constexpr int kBufSize = 64 * 1024;
    Vec<WCHAR> buf;
    VecAppendBlanks(buf, kBufSize);
    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = buf.els;
    ofn.nMaxFile = kBufSize;
    ofn.lpstrFilter = L"PDF documents\0*.pdf\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn) || tornDown) {
        return;
    }
    StrVec paths;
    WCHAR* first = buf.els;
    WCHAR* name = first + wcslen(first) + 1;
    if (*name == 0) {
        // one file: the buffer is its full path
        paths.Append(ToUtf8Temp(first));
    }
    for (; *name; name += wcslen(name) + 1) {
        paths.Append(path::JoinTemp(ToUtf8Temp(first), ToUtf8Temp(name)));
    }
    int at = AskInsertPosition(this, paths);
    if (at < 0 || tornDown) {
        return;
    }
    for (Str path : paths) {
        int n = len(grid->items);
        AddPdf(path, at);
        at += len(grid->items) - n;
    }
    SetFocusTo(grid);
}

void MergePdfWnd::OnRemove(VirtMouseEvent*) {
    grid->SetRemoved(true);
}

void MergePdfWnd::OnRestore(VirtMouseEvent*) {
    grid->SetRemoved(false);
}

// Writes the pages not removed to destPath, which may be one of the sources:
// the result goes to a temp file first
bool MergePdfWnd::SaveTo(Str destPath) {
    Vec<PdfMergePage> pages;
    for (MergeItem& it : grid->items) {
        if (!it.removed) {
            VecAppend(pages, PdfMergePage{VecFind(sources->all, it.src), it.pageNo});
        }
    }
    if (len(pages) == 0) {
        return false;
    }
    Vec<PdfMergeSource> srcs;
    for (MergeSource* src : sources->all) {
        VecAppend(srcs, PdfMergeSource{src->path, src->password});
    }
    // unsaved changes of the document (annotations) are merged from a copy
    TempStr srcCopy;
    EngineBase* docEngine = sources->all[0]->engine;
    if (EngineHasUnsavedAnnotations(docEngine)) {
        srcCopy = GetTempFilePathTemp(StrL("merge"));
        if (len(srcCopy) == 0 || !EngineMupdfSaveCopy(docEngine, srcCopy)) {
            return false;
        }
        srcs[0].path = srcCopy;
    }
    TempStr tmpPath = GetTempFilePathTemp(StrL("merge"));
    bool ok = len(tmpPath) > 0 && EngineMupdfMergePdfs(srcs, pages, tmpPath);
    if (ok) {
        WindowTab* tab = FindTabByFile(destPath);
        if (tab) {
            // it's reloaded below; the file watcher needn't
            tab->ignoreNextAutoReload = true;
        }
        Str data = file::ReadFile(tmpPath);
        ok = len(data) > 0 && file::WriteFile(destPath, data);
        str::Free(data);
    }
    if (len(tmpPath) > 0) {
        file::Delete(tmpPath);
    }
    if (len(srcCopy) > 0) {
        file::Delete(srcCopy);
    }
    logf("MergePdfWnd::SaveTo: %d pages from %d files to '%s', ok: %d\n", len(pages), len(srcs), destPath, (int)ok);
    return ok;
}

// closes the dialog and shows the saved file: reloaded where it's open, else in a new tab
void MergePdfWnd::AfterSave(Str path) {
    MainWindow* w = IsMainWindowValidAndNotClosing(win) ? win : nullptr;
    TempStr savedPath = str::DupTemp(path);
    Close();
    WindowTab* tab = FindTabByFile(savedPath);
    if (tab) {
        SelectTabInWindow(tab);
        ReloadDocument(tab->win, false);
        // the write notifies the file watcher, which would reload it again
        tab->ignoreNextAutoReload = true;
        return;
    }
    if (!w) {
        return;
    }
    LoadArgs args(savedPath, w);
    StartLoadDocument(&args);
}

void MergePdfWnd::OnSave(VirtMouseEvent*) {
    if (!SaveTo(docPath)) {
        MessageBoxWarning(hwnd, fmt(Tr("Couldn't save '%s'").s, docPath), Tr("Merge PDF"));
        return;
    }
    AfterSave(docPath);
}

void MergePdfWnd::OnSaveAs(VirtMouseEvent*) {
    WCHAR dstFileName[MAX_PATH + 1]{};
    wstr::BufSet(WStr(dstFileName, MAX_PATH), ToWStrTemp(MakeUniqueFilePathTemp(docPath)));
    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = L"PDF documents\0*.pdf\0";
    ofn.lpstrDefExt = L"pdf";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn) || tornDown) {
        return;
    }
    TempStr path = ToUtf8Temp(dstFileName);
    if (!SaveTo(path)) {
        MessageBoxWarning(hwnd, fmt(Tr("Couldn't save '%s'").s, path), Tr("Merge PDF"));
        return;
    }
    AfterSave(path);
}

void MergePdfWnd::OnCancel(VirtMouseEvent*) {
    Close();
}

// Esc ends a drag rather than closing the dialog
void MergePdfWnd::OnKey(KeyEvent* ev) {
    if (ev->vkey == VK_ESCAPE && grid->press != MergePress::None) {
        grid->EndPress();
        ev->didHandle = true;
    }
}

void MergePdfWnd::OnTimer(WindowBase::TimerEvent* ev) {
    if (ev->timerId == kAutoScrollTimerId) {
        grid->AutoScrollStep();
    }
}

bool MergePdfWnd::Create(MainWindow* w, WindowTab* tab) {
    win = w;
    EngineBase* engine = tab->GetEngine();
    docPath = str::Dup(tab->filePath);
    sources = new MergeSources();
    engine->AddRef();
    AddSource(docPath, engine);

    closeOnEsc = true;
    onClose = MkFunc1Void(OnMergeWndClose);
    onDestroy = MkFunc1Void(OnMergeWndDestroy);
    onKeyDown = MkMethod1<MergePdfWnd, KeyEvent*, &MergePdfWnd::OnKey>(this);
    onTimer = MkMethod1<MergePdfWnd, WindowBase::TimerEvent*, &MergePdfWnd::OnTimer>(this);

    CreateCustomArgs cargs;
    cargs.title = fmt("%s - %s", Tr("Merge PDF"), path::GetBaseNameTemp(docPath));
    cargs.font = GetDefaultGuiFont();
    cargs.style = WS_OVERLAPPEDWINDOW;
    cargs.visible = false;
    cargs.isRtl = IsUIRtl();
    cargs.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
    cargs.bgColor = DarkModeDialogBgColor();
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    int btnGap = font->averageCharWidth;

    grid = new MergeGrid(this, sources, GetAppFont(), GetDpi());
    sources->grid = grid;

    auto* bottom = new HBox();
    bottom->alignCross = CrossAxisAlign::CrossCenter;
    bottom->gap = btnGap;
    btnAdd = NewThemedButton(hwnd, Tr("Add PDF..."), font, false);
    btnAdd->onClick = MkMethod1<MergePdfWnd, VirtMouseEvent*, &MergePdfWnd::OnAdd>(this);
    bottom->AddChild(btnAdd);
    btnRemove = NewThemedButton(hwnd, Tr("Remove"), font, false);
    btnRemove->onClick = MkMethod1<MergePdfWnd, VirtMouseEvent*, &MergePdfWnd::OnRemove>(this);
    bottom->AddChild(btnRemove);
    btnRestore = NewThemedButton(hwnd, Tr("Restore"), font, false);
    btnRestore->onClick = MkMethod1<MergePdfWnd, VirtMouseEvent*, &MergePdfWnd::OnRestore>(this);
    bottom->AddChild(btnRestore);
    info = new VirtCustom();
    info->idealSize = {0, PlatformFontMeasureText(font, StrL("M")).dy};
    info->onPaint = MkFunc1(PaintMergeInfo, this);
    bottom->AddChild(info, 1);
    btnSave = NewThemedButton(hwnd, Tr("Save"), font, false);
    btnSave->onClick = MkMethod1<MergePdfWnd, VirtMouseEvent*, &MergePdfWnd::OnSave>(this);
    bottom->AddChild(btnSave);
    btnSaveAs = NewThemedButton(hwnd, Tr("Save As..."), font, true);
    btnSaveAs->onClick = MkMethod1<MergePdfWnd, VirtMouseEvent*, &MergePdfWnd::OnSaveAs>(this);
    bottom->AddChild(btnSaveAs);
    btnCancel = NewThemedButton(hwnd, Tr("Cancel"), font, false);
    btnCancel->onClick = MkMethod1<MergePdfWnd, VirtMouseEvent*, &MergePdfWnd::OnCancel>(this);
    bottom->AddChild(btnCancel);

    int gap = DpiScaleByDpi(GetDpi(), kDialogPadding);
    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(grid, 1);
    vbox->AddChild(new Spacer(0, gap));
    vbox->AddChild(bottom);
    layout = new Padding(vbox, Insets{gap, gap, gap, gap});

    grid->InsertPages(sources->all[0], 0);
    grid->SelectOnly(ClampI(tab->ctrl->CurrentPageNo() - 1, 0, std::max(len(grid->items) - 1, 0)));

    // most of the work area of the document's monitor
    Rect work = GetWorkAreaRect(HwndWindowRect(w->hwndFrame), w->hwndFrame);
    int dx = std::min(work.dx, DpiScaleByDpi(GetDpi(), kDialogDx));
    int dy = work.dy * 85 / 100;
    SetWindowPos(hwnd, nullptr, 0, 0, dx, dy, SWP_NOMOVE | SWP_NOZORDER);
    DoLayout();
    HwndCenterDialog(hwnd, w->hwndFrame);
    HwndEnsureOnScreen(hwnd);
    UpdateTheme();
    UpdateUI();

    auto* dt = new MergeDropTarget(this);
    RegisterDragDrop(hwnd, dt);
    dt->Release(); // RegisterDragDrop AddRef'd it

    SetIsVisible(true);
    SetFocusTo(grid);
    grid->EnsureVisible(grid->focusIdx / grid->cols);
    grid->StartRendering();
    return true;
}

void ShowMergePdfDialog(MainWindow* win) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!tab || !tab->IsDocLoaded() || len(tab->filePath) == 0 || !EngineMupdfIsPdf(tab->GetEngine())) {
        return;
    }
    if (gMergeWnd) {
        BringWindowToTop(gMergeWnd->hwnd);
        return;
    }
    logf("ShowMergePdfDialog: '%s'\n", tab->filePath);
    auto* wnd = new MergePdfWnd();
    wnd->SetFont(GetDefaultGuiFont());
    if (!wnd->Create(win, tab)) {
        delete wnd;
        return;
    }
    gMergeWnd = wnd;
}

struct AskAndAddPdf {
    MergePdfWnd* wnd = nullptr;
    Str path; // owned
};

static void AskAndAddPdfNow(AskAndAddPdf* d) {
    if (gMergeWnd == d->wnd && !d->wnd->tornDown) {
        StrVec paths;
        paths.Append(d->path);
        int at = AskInsertPosition(d->wnd, paths);
        if (at >= 0 && gMergeWnd == d->wnd && !d->wnd->tornDown) {
            d->wnd->AddPdf(d->path, at);
        }
    }
    str::Free(d->path);
    delete d;
}

// Drives the Merge PDF dialog and reports it. action: "open", "add" (arg: a
// PDF path, n: in front of that item, -1: where Add PDF... puts it), "askpos"
// (arg: a PDF path; asks where to add it, like Add PDF...), "move"
// (arg: 0-based items like "0,2", n: in front of that item), "remove" /
// "restore" (arg: items), "save" / "saveas" (arg: the path), "close" or ""
// (report only). Reports the items as src:page (r: removed, s: selected) and,
// for clicking, the dialog's hwnd and each visible thumbnail and its corner
// button in client coords. Used by tests/issue-6070.ts.
TempStr MergePdfResultTemp(Str action, Str arg, int n, int* exitCodeOut) {
    auto finish = [exitCodeOut](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    if (str::Eq(action, StrL("open"))) {
        MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
        ShowMergePdfDialog(win);
    }
    MergePdfWnd* w = gMergeWnd;
    if (!w) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-dialog")));
    }
    MergeGrid* grid = w->grid;
    auto selectItems = [&]() {
        StrVec parts;
        Split(&parts, arg, StrL(","), true);
        for (MergeItem& it : grid->items) {
            it.selected = false;
        }
        for (Str p : parts) {
            int idx = ParseInt(p);
            if (idx >= 0 && idx < len(grid->items)) {
                grid->items[idx].selected = true;
            }
        }
    };
    str::Builder out;
    if (str::Eq(action, StrL("add"))) {
        out.Append(fmt("added=%d ", (int)w->AddPdf(arg, n < 0 ? grid->InsertPosition() : n)));
    } else if (str::Eq(action, StrL("askpos"))) {
        // the question is modal: asked after this request has been answered
        auto* d = new AskAndAddPdf();
        d->wnd = w;
        d->path = str::Dup(arg);
        uitask::Post(MkFunc0<AskAndAddPdf>(AskAndAddPdfNow, d));
    } else if (str::Eq(action, StrL("move"))) {
        selectItems();
        grid->MoveSelected(n);
    } else if (str::Eq(action, StrL("remove")) || str::Eq(action, StrL("restore"))) {
        selectItems();
        grid->SetRemoved(str::Eq(action, StrL("remove")));
    } else if (str::Eq(action, StrL("save")) || str::Eq(action, StrL("saveas"))) {
        Str path = str::Eq(action, StrL("save")) ? Str(w->docPath) : arg;
        bool ok = w->SaveTo(path);
        TempStr res = fmt("OK saved=%d", (int)ok);
        if (ok) {
            w->AfterSave(path);
        }
        return finish(0, res);
    } else if (str::Eq(action, StrL("close"))) {
        w->Close();
        return finish(0, str::DupTemp(StrL("OK closed")));
    }
    out.Append(fmt("hwnd=%d focused=%d items=", (int)(intptr_t)w->hwnd, (int)grid->HasFlag(vwfFocused)));
    for (int i = 0; i < len(grid->items); i++) {
        MergeItem& it = grid->items[i];
        out.Append(fmt(i == 0 ? "%d:%d" : ",%d:%d", VecFind(w->sources->all, it.src), it.pageNo));
        if (it.removed) {
            out.AppendChar('r');
        }
        if (it.selected) {
            out.AppendChar('s');
        }
    }
    Point origin = grid->OriginInWindow();
    out.Append(fmt(" canSave=%d rects=", (int)w->btnSave->IsEnabled()));
    for (int i = grid->FirstVisibleIdx(); i <= grid->LastVisibleIdx(); i++) {
        Rect r = grid->CellRect(i);
        Rect b = grid->CornerBtnRect(i);
        if (r.y < 0 || r.y + r.dy > grid->bounds.dy) {
            continue;
        }
        out.Append(fmt("%d:%d,%d,%d,%d:%d,%d,%d,%d;", i, r.x + origin.x, r.y + origin.y, r.dx, r.dy, b.x + origin.x,
                       b.y + origin.y, b.dx, b.dy));
    }
    return finish(0, fmt("OK %s", ToStrTemp(out)));
}
