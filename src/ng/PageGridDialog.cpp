/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// CmdConfigurePageGrid: spacing / origin / color / line style, like
// PDF-XChange's Measurement > Grid and Guides. Appearance is saved in
// FixedPageUI.PageGrid; the Show Grid checkbox is the session toggle.
//
// ng: orig's dialog is a WS_POPUPWINDOW with two DropDowns, six Edits, a
// Checkbox and a swatch that opens the win32 color picker. Here it is a gpui
// Dialog with the same rows; the swatch opens our own Change Colors dialog.
// The grid overlay itself (orig paints it in Canvas.cpp) is here too, drawn
// through the canvas helpers, because nothing else needs it.

#include "gui/GpuiBridge.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "PageGridDialogCommon.h"

#include "SumatraLog.h"

// orig keeps this in Canvas.cpp: session-only, not saved
static bool gShowPageGrid = false;

bool ShowPageGrid() {
    return gShowPageGrid;
}

void TogglePageGrid() {
    gShowPageGrid = !gShowPageGrid;
    logf("TogglePageGrid: %d\n", (int)gShowPageGrid);
}

void SetShowPageGrid(bool on) {
    gShowPageGrid = on;
}

// --- the overlay (orig's Canvas.cpp) ----------------------------------------

static void PageGridDash(gp::PaintCtx* ctx, Point a, Point b, Color col) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    float len = sqrtf((float)(dx * dx + dy * dy));
    if (len < 1.f) {
        return;
    }
    for (float t = 0; t < len; t += 4.f) {
        float t2 = t + 2.f;
        if (t2 > len) {
            t2 = len;
        }
        Point p0{a.x + (int)(dx * (t / len)), a.y + (int)(dy * (t / len))};
        Point p1{a.x + (int)(dx * (t2 / len)), a.y + (int)(dy * (t2 / len))};
        CanvasDrawLine(ctx, p0, p1, col, 1);
    }
}

static void PaintPageGridSlanted(DisplayModel* dm, gp::PaintCtx* ctx, const PageGridDraw& g) {
    EngineBase* engine = dm->GetEngine();
    if (!engine || engine->isImageCollection) {
        return;
    }
    Rect viewPort(Point(), dm->GetViewPort().Size());
    Vec<Rect> dots;
    for (int pageNo = 1; pageNo <= dm->PageCount(); pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }
        Rect bounds = pi->pageOnScreen.Intersect(viewPort);
        if (bounds.IsEmpty()) {
            continue;
        }
        RectF box = engine->PageMediabox(pageNo);
        if (box.IsEmpty()) {
            continue;
        }
        RectF vis = dm->CvtFromScreen(bounds, pageNo).Intersect(box);
        if (vis.IsEmpty()) {
            continue;
        }
        bool drawMinor = false;
        if (!PageGridSlantVisible(dm, pageNo, g, box, drawMinor)) {
            continue;
        }
        Vec<PageGridSeg> segs;
        PageGridSlantedSegs(g, box, vis, drawMinor, segs);
        bool dashed = g.style == PageGridStyleKind::Dotted;
        for (PageGridSeg& s : segs) {
            if (s.dot) {
                Point p = dm->CvtToScreen(pageNo, PointF(s.x0, s.y0));
                int d = s.major ? 3 : 1;
                Rect r{p.x - (d / 2), p.y - (d / 2), d, d};
                if (!r.Intersect(bounds).IsEmpty()) {
                    VecAppend(dots, r);
                }
                continue;
            }
            Point a = dm->CvtToScreen(pageNo, PointF(s.x0, s.y0));
            Point b = dm->CvtToScreen(pageNo, PointF(s.x1, s.y1));
            if (dashed && !s.major) {
                PageGridDash(ctx, a, b, g.color);
                continue;
            }
            float w = (s.major && !dashed) ? 2.f : 1.f;
            CanvasDrawLine(ctx, a, b, g.color, w);
        }
    }
    if (len(dots) > 0) {
        CanvasFillRects(ctx, dots.els, len(dots), g.color, 255, 0);
    }
}

// ng: orig strokes with a GDI pen; here every line is a filled rect in document
// coordinates, which is what CanvasFillRects takes. Style "dots" puts a 1px /
// 3px square at each intersection; "dotted" and "solid" draw the lines (major
// heavier), dotted as a dashed run of short pieces. Skips comics, as orig does.
static void CollectPageGridMarks(DisplayModel* dm, Vec<Rect>& rects) {
    EngineBase* engine = dm->GetEngine();
    if (!engine || engine->isImageCollection) {
        return;
    }
    PageGridDraw g = GetPageGridDraw();
    float minorX = g.widthPt / (float)g.subdiv;
    float minorY = g.heightPt / (float)g.subdiv;
    if (minorX <= 0.f || minorY <= 0.f) {
        return;
    }
    Rect viewPort(Point(), dm->GetViewPort().Size());

    for (int pageNo = 1; pageNo <= dm->PageCount(); pageNo++) {
        PageInfo* pi = dm->GetPageInfo(pageNo);
        if (!pi || !pi->isShown || 0.0 == pi->visibleRatio) {
            continue;
        }
        Rect bounds = pi->pageOnScreen.Intersect(viewPort);
        if (bounds.IsEmpty()) {
            continue;
        }
        RectF box = engine->PageMediabox(pageNo);
        if (box.IsEmpty()) {
            continue;
        }
        RectF vis = dm->CvtFromScreen(bounds, pageNo).Intersect(box);
        if (vis.IsEmpty()) {
            continue;
        }
        float ox = box.x + g.offsetXPt;
        float oy = box.y + g.offsetYPt;

        if (g.style == PageGridStyleKind::Dots) {
            for (float y = PageGridAlignDown((float)vis.y, oy, minorY); y <= (float)vis.Bottom(); y += minorY) {
                bool majorY = PageGridIsMajor(y, oy, minorY, g.subdiv);
                for (float x = PageGridAlignDown((float)vis.x, ox, minorX); x <= (float)vis.Right(); x += minorX) {
                    bool major = majorY && PageGridIsMajor(x, ox, minorX, g.subdiv);
                    Point p = dm->CvtToScreen(pageNo, PointF(x, y));
                    int d = major ? 3 : 1;
                    Rect r{p.x - (d / 2), p.y - (d / 2), d, d};
                    if (!r.Intersect(bounds).IsEmpty()) {
                        VecAppend(rects, r);
                    }
                }
            }
            continue;
        }

        bool dashed = g.style == PageGridStyleKind::Dotted;
        for (float x = PageGridAlignDown((float)vis.x, ox, minorX); x <= (float)vis.Right(); x += minorX) {
            bool major = PageGridIsMajor(x, ox, minorX, g.subdiv);
            Point a = dm->CvtToScreen(pageNo, PointF(x, (float)vis.y));
            Point b = dm->CvtToScreen(pageNo, PointF(x, (float)vis.Bottom()));
            int w = (major && !dashed) ? 2 : 1;
            int y0 = std::min(a.y, b.y);
            int y1 = std::max(a.y, b.y);
            if (!dashed || major) {
                VecAppend(rects, Rect{a.x, y0, w, y1 - y0}.Intersect(bounds));
                continue;
            }
            for (int y = y0; y < y1; y += 4) {
                VecAppend(rects, Rect{a.x, y, 1, 2}.Intersect(bounds));
            }
        }
        for (float y = PageGridAlignDown((float)vis.y, oy, minorY); y <= (float)vis.Bottom(); y += minorY) {
            bool major = PageGridIsMajor(y, oy, minorY, g.subdiv);
            Point a = dm->CvtToScreen(pageNo, PointF((float)vis.x, y));
            Point b = dm->CvtToScreen(pageNo, PointF((float)vis.Right(), y));
            int h = (major && !dashed) ? 2 : 1;
            int x0 = std::min(a.x, b.x);
            int x1 = std::max(a.x, b.x);
            if (!dashed || major) {
                VecAppend(rects, Rect{x0, a.y, x1 - x0, h}.Intersect(bounds));
                continue;
            }
            for (int x = x0; x < x1; x += 4) {
                VecAppend(rects, Rect{x, a.y, 2, 1}.Intersect(bounds));
            }
        }
    }
}

// Overlay on, for a paginated document that is not a blank presentation screen.
bool PageGridCovers(MainWindow* win) {
    if (!win || !ShowPageGrid() || !win->AsFixed()) {
        return false;
    }
    return win->presentation != PM_BLACK_SCREEN && win->presentation != PM_WHITE_SCREEN;
}

void PaintPageGrid(DisplayModel* dm, gpui::PaintCtx* ctx) {
    PageGridDraw g = GetPageGridDraw();
    if (!PageGridIsUpright(g.slantDeg)) {
        PaintPageGridSlanted(dm, ctx, g);
        return;
    }
    Vec<Rect> rects;
    CollectPageGridMarks(dm, rects);
    if (len(rects) > 0) {
        CanvasFillRects(ctx, rects.els, len(rects), g.color, 255, 0);
    }
}

int PageGridMarkPixels(MainWindow* win) {
    if (!PageGridCovers(win)) {
        return 0;
    }
    Vec<Rect> rects;
    CollectPageGridMarks(win->AsFixed(), rects);
    int n = 0;
    for (Rect r : rects) {
        if (r.dx > 0 && r.dy > 0) {
            n += r.dx * r.dy;
        }
    }
    return n;
}

// --- the dialog -------------------------------------------------------------

struct PageGridDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    PageGridSnap snap;
    Color currentColor = kPageGridDefaultColor;
    int unitIdx = kPageGridDefaultUnitIdx;
    bool showGrid = false;
    bool updating = false;
    DialogSelect ddUnits;
    DialogSelect ddStyle;
    gpui::InputState* editWidth = nullptr;
    gpui::InputState* editHeight = nullptr;
    gpui::InputState* editSub = nullptr;
    gpui::InputState* editSlant = nullptr;
    gpui::InputState* editOffX = nullptr;
    gpui::InputState* editOffY = nullptr;
    gpui::InputState* editColor = nullptr;
    // orig's window opens with the first edit focused and selected
    bool wantFocus = false;
};

static PageGridDlg gPageGrid;

// orig's layout: the units, the spacing and the offset under muted headings,
// the look, hairlines between those, then the footer; the labels in one column
constexpr float kGridDlgDx = 360;
constexpr float kGridEditDx = 66;
constexpr float kGridSwatch = 22;
constexpr float kGridSwatchRadius = 4;

struct PageGridView {
    static void OnOk(PageGridView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(PageGridView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnReset(PageGridView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnShowGrid(PageGridView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnPickColor(PageGridView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnChanged(PageGridView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnColorChanged(PageGridView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<PageGridView> gPageGridView;

static TempStr EditTextTemp(gp::InputState* e) {
    if (!e) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(e)));
}

static bool ParseEditFloat(gp::InputState* e, float* out) {
    TempStr s = EditTextTemp(e);
    if (len(s) == 0) {
        return false;
    }
    const char* cs = CStrTemp(s);
    char* end = nullptr;
    float v = strtof(cs, &end);
    if (end == cs) {
        return false;
    }
    *out = v;
    return true;
}

static bool ParseEditInt(gp::InputState* e, int* out) {
    float v = 0;
    if (!ParseEditFloat(e, &v)) {
        return false;
    }
    *out = (int)(v + (v >= 0 ? 0.5f : -0.5f));
    return true;
}

static void WriteSnapToPrefs() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    const PageGridSnap& snap = gPageGrid.snap;
    pg->width = snap.width;
    pg->height = snap.height;
    pg->subdivisions = snap.subdivisions;
    pg->offsetX = snap.offsetX;
    pg->offsetY = snap.offsetY;
    pg->slant = snap.slant;
    SetColorText(pg->color, snap.color);
    str::ReplaceWithCopy(&pg->style, snap.style);
    str::ReplaceWithCopy(&pg->units, snap.units);
    SetShowPageGrid(snap.showGrid);
}

static void FillEditsFromPt() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    int u = gPageGrid.unitIdx;
    gPageGrid.updating = true;
    gp::InputSetValue(gPageGrid.editWidth,
                      ToGpui(PageGridNumTemp(PageGridFromPt(pg->width > 0 ? pg->width : kPageGridDefaultSizePt, u))));
    gp::InputSetValue(gPageGrid.editHeight,
                      ToGpui(PageGridNumTemp(PageGridFromPt(pg->height > 0 ? pg->height : kPageGridDefaultSizePt, u))));
    int n = pg->subdivisions > 0 ? pg->subdivisions : kPageGridDefaultSubdivisions;
    gp::InputSetValue(gPageGrid.editSub, ToGpui(fmt("%d", n)));
    float slant = pg->slant == 0 ? kPageGridDefaultSlant : pg->slant;
    gp::InputSetValue(gPageGrid.editSlant, ToGpui(PageGridNumTemp(slant)));
    gp::InputSetValue(gPageGrid.editOffX, ToGpui(PageGridNumTemp(PageGridFromPt(pg->offsetX, u))));
    gp::InputSetValue(gPageGrid.editOffY, ToGpui(PageGridNumTemp(PageGridFromPt(pg->offsetY, u))));
    gPageGrid.updating = false;
}

static bool ReadControlsToPt(float& widthPt, float& heightPt, int& subdiv, float& offX, float& offY) {
    float w = 0, h = 0, ox = 0, oy = 0;
    int sub = 0;
    if (!ParseEditFloat(gPageGrid.editWidth, &w) || !ParseEditFloat(gPageGrid.editHeight, &h) ||
        !ParseEditInt(gPageGrid.editSub, &sub)) {
        return false;
    }
    ParseEditFloat(gPageGrid.editOffX, &ox);
    ParseEditFloat(gPageGrid.editOffY, &oy);
    int u = gPageGrid.unitIdx;
    w = PageGridToPt(w, u);
    h = PageGridToPt(h, u);
    ox = PageGridToPt(ox, u);
    oy = PageGridToPt(oy, u);
    if (w < 1.f || h < 1.f || sub < 1) {
        return false;
    }
    widthPt = limitValue(w, 1.f, 720.f);
    heightPt = limitValue(h, 1.f, 720.f);
    subdiv = limitValue(sub, 1, 32);
    offX = limitValue(ox, -720.f, 720.f);
    offY = limitValue(oy, -720.f, 720.f);
    return true;
}

// orig's ApplyLive: every edit previews on the page right away
static void ApplyLive() {
    if (gPageGrid.updating) {
        return;
    }
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    float w, h, ox, oy;
    int sub;
    if (ReadControlsToPt(w, h, sub, ox, oy)) {
        pg->width = w;
        pg->height = h;
        pg->subdivisions = sub;
        pg->offsetX = ox;
        pg->offsetY = oy;
    }
    float slant = pg->slant;
    if (ParseEditFloat(gPageGrid.editSlant, &slant)) {
        pg->slant = PageGridClampSlant(slant);
    }
    int styleIdx = gPageGrid.ddStyle.sel;
    if (styleIdx < 0) {
        styleIdx = kPageGridDefaultStyleIdx;
    }
    str::ReplaceWithCopy(&pg->style, SeqStrByIndex(kPageGridStyleTok, styleIdx));
    str::ReplaceWithCopy(&pg->units, SeqStrByIndex(kPageGridUnitTok, gPageGrid.unitIdx));
    SetColorText(pg->color, SerializeColorTemp(gPageGrid.currentColor));
    SetShowPageGrid(gPageGrid.showGrid);
    RedrawPageGridWindows();
}

// orig's OnUnitsChanged: the values keep their length, the numbers change
static void OnUnitsChanged(int idx) {
    PageGrid* pg = PageGridPrefs();
    float w, h, ox, oy;
    int sub;
    if (ReadControlsToPt(w, h, sub, ox, oy) && pg) {
        pg->width = w;
        pg->height = h;
        pg->subdivisions = sub;
        pg->offsetX = ox;
        pg->offsetY = oy;
    }
    gPageGrid.unitIdx = idx < 0 ? kPageGridDefaultUnitIdx : idx;
    FillEditsFromPt();
    ApplyLive();
}

static void LoadFromPrefs() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    CopyPageGridSnap(gPageGrid.snap, *pg, ShowPageGrid());
    ParseColor(pg->color);
    gPageGrid.currentColor =
        (pg->color.parsedOk && !IsSpecialColor(pg->color.col)) ? pg->color.col : kPageGridDefaultColor;
    int unitIdx = SeqStrIndexIS(kPageGridUnitTok, pg->units);
    gPageGrid.unitIdx = unitIdx < 0 ? kPageGridDefaultUnitIdx : unitIdx;
    int styleIdx = SeqStrIndexIS(kPageGridStyleTok, pg->style);
    if (styleIdx < 0) {
        styleIdx = kPageGridDefaultStyleIdx;
    }
    StrVec units;
    for (int i = 0; i < 4; i++) {
        units.Append(PageGridUnitName(i));
    }
    gPageGrid.ddUnits.SetItems(units, gPageGrid.unitIdx);
    StrVec styles;
    for (int i = 0; i < 3; i++) {
        styles.Append(PageGridStyleName(i));
    }
    gPageGrid.ddStyle.SetItems(styles, styleIdx);
    gPageGrid.updating = true;
    gp::InputSetValue(gPageGrid.editColor, ToGpui(SerializeColorTemp(gPageGrid.currentColor)));
    gPageGrid.updating = false;
    gPageGrid.showGrid = ShowPageGrid();
    FillEditsFromPt();
}

// orig's window (modeless, as orig's), where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gPageGridTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsPageGridDialogVisible() {
    return gPageGrid.visible && !gPageGridTw;
}

bool IsPageGridDialogOpen() {
    return gPageGrid.visible;
}

void PageGridDialogSyncFromPrefs() {
    if (!gPageGrid.visible || !gPageGrid.editSlant) {
        return;
    }
    FillEditsFromPt();
}

static void FreePageGridInputs() {
    MainWindow* win = gPageGrid.win;
    gp::InputState* edits[] = {gPageGrid.editWidth, gPageGrid.editHeight, gPageGrid.editSub,  gPageGrid.editSlant,
                               gPageGrid.editOffX,  gPageGrid.editOffY,   gPageGrid.editColor};
    for (gp::InputState* e : edits) {
        if (e && win && win->gpuiWin) {
            gp::InputBlur(e, win->gpuiWin->app, win->gpuiWin);
        }
        delete e;
    }
    gPageGrid.editWidth = nullptr;
    gPageGrid.editHeight = nullptr;
    gPageGrid.editSub = nullptr;
    gPageGrid.editSlant = nullptr;
    gPageGrid.editOffX = nullptr;
    gPageGrid.editOffY = nullptr;
    gPageGrid.editColor = nullptr;
    gPageGrid.ddUnits.Free();
    gPageGrid.ddStyle.Free();
}

// orig's OnCancel: the values the dialog opened with go back
void ClosePageGridDialog() {
    if (!gPageGrid.visible) {
        return;
    }
    gPageGrid.visible = false;
    DlgWindowClose(&gPageGridTw);
    // its window can outlive the main window by a moment (the layer tells
    // it later): nothing of a closed main window is touched
    if (!IsMainWindowValid(gPageGrid.win)) {
        gPageGrid.win = nullptr;
    }
    WriteSnapToPrefs();
    RedrawPageGridWindows();
    FreePageGridInputs();
    AppShellInvalidate(gPageGrid.win);
}

static Str PageGridDlgTitle() {
    return Tr("Page Grid");
}

void ShowPageGridDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (gPageGrid.visible) {
        return;
    }
    gPageGrid.win = win;
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gp::InputState** edits[] = {&gPageGrid.editWidth, &gPageGrid.editHeight, &gPageGrid.editSub,  &gPageGrid.editSlant,
                                &gPageGrid.editOffX,  &gPageGrid.editOffY,   &gPageGrid.editColor};
    for (gp::InputState** e : edits) {
        *e = new gp::InputState();
        (*e)->focus = gp::FocusHandleNew(app);
    }
    gPageGrid.ddUnits.Init(app);
    gPageGrid.ddStyle.Init(app);
    LoadFromPrefs();
    gPageGrid.visible = true;
    DlgWindowSpec spec;
    spec.name = "pagegrid";
    spec.title = PageGridDlgTitle;
    spec.modal = false;
    spec.build = PageGridDialogBuild;
    spec.close = ClosePageGridDialog;
    // orig's is owned by the main window, which stays enabled
    spec.owned = true;
    spec.clientDx = kGridDlgDx;
    gPageGrid.wantFocus = true;
    gPageGridTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void PageGridView::OnCancel(PageGridView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ClosePageGridDialog();
    gp::Notify(cx);
}

void PageGridView::OnOk(PageGridView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ApplyLive();
    logf("PageGridDialog: %s", PageGridStateTemp());
    if (HasPermission(Perm::SavePreferences)) {
        ScheduleSaveSettings();
    }
    gPageGrid.visible = false;
    DlgWindowClose(&gPageGridTw);
    FreePageGridInputs();
    AppShellInvalidate(gPageGrid.win);
    gp::Notify(cx);
}

void PageGridView::OnReset(PageGridView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ResetPageGridToDefaults();
    gPageGrid.currentColor = kPageGridDefaultColor;
    gPageGrid.unitIdx = kPageGridDefaultUnitIdx;
    gPageGrid.ddUnits.SetSel(cx->app, kPageGridDefaultUnitIdx);
    gPageGrid.ddStyle.SetSel(cx->app, kPageGridDefaultStyleIdx);
    gPageGrid.updating = true;
    gp::InputSetValue(gPageGrid.editColor, ToGpui(SerializeColorTemp(gPageGrid.currentColor)));
    gPageGrid.updating = false;
    FillEditsFromPt();
    ApplyLive();
    gp::Notify(cx);
}

void PageGridView::OnShowGrid(PageGridView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gPageGrid.showGrid = !gPageGrid.showGrid;
    ApplyLive();
    gp::Notify(cx);
    AppShellInvalidate(gPageGrid.win);
}

void PageGridView::OnChanged(PageGridView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    ApplyLive();
    gp::Notify(cx);
}

// orig's OnColorEditChanged
void PageGridView::OnColorChanged(PageGridView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::Change || gPageGrid.updating) {
        return;
    }
    ParsedColor parsed;
    ParseColor(parsed, EditTextTemp(gPageGrid.editColor));
    if (!parsed.parsedOk || IsSpecialColor(parsed.col)) {
        return;
    }
    gPageGrid.currentColor = parsed.col;
    ApplyLive();
    gp::Notify(cx);
}

// ng: orig's swatch opens the win32 CHOOSECOLOR dialog; here it opens the
// port's own color picker
static void PageGridColorPicked(void*, ChangeColorsArgs* args) {
    if (!args->didSelect || args->color == kColorUnset) {
        return;
    }
    gPageGrid.currentColor = args->color;
    gPageGrid.updating = true;
    gp::InputSetValue(gPageGrid.editColor, ToGpui(SerializeColorTemp(gPageGrid.currentColor)));
    gPageGrid.updating = false;
    ApplyLive();
    AppShellInvalidate(gPageGrid.win);
}

void PageGridView::OnPickColor(PageGridView*, gp::Ctx* cx, const gp::ClickEvent*) {
    auto* args = new ChangeColorsArgs();
    args->win = gPageGrid.win;
    args->title = Tr("Color:");
    args->color = gPageGrid.currentColor;
    ParseColorList(gSettings->customColors, args->colors, 13);
    args->onClose = MkFunc1(PageGridColorPicked, (void*)nullptr);
    ShowChangeColorsDialog(args);
    gp::Notify(cx);
}

gp::El* PageGridDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gPageGrid.visible || gPageGrid.win != win) {
        return nullptr;
    }
    if (gPageGridTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gPageGridView.IsValid()) {
        gPageGridView = gp::EntityNewState<PageGridView>(cx->app);
    }
    if (gPageGrid.ddUnits.PollChanged(cx->app)) {
        OnUnitsChanged(gPageGrid.ddUnits.sel);
    }
    if (gPageGrid.ddStyle.PollChanged(cx->app)) {
        ApplyLive();
    }
    gp::Listener onChanged = gp::ListenTo(gPageGridView, &PageGridView::OnChanged);
    gPageGrid.editWidth->onChange = onChanged;
    gPageGrid.editHeight->onChange = onChanged;
    gPageGrid.editSub->onChange = onChanged;
    gPageGrid.editSlant->onChange = onChanged;
    gPageGrid.editOffX->onChange = onChanged;
    gPageGrid.editOffY->onChange = onChanged;
    gPageGrid.editColor->onChange = gp::ListenTo(gPageGridView, &PageGridView::OnColorChanged);
    gp::Listener onOk = gp::ListenTo(gPageGridView, &PageGridView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gPageGridView, &PageGridView::OnCancel);
    DlgSetDefault(cx, onOk, true);
    Color colEdge = ThemeEdgeColor();

    Str units = Tr("Units:"), horz = Tr("Horizontal:"), vert = Tr("Vertical:"), sub = Tr("Subdivisions:");
    Str slant = Tr("Slant:"), fromLeft = Tr("From left:"), fromBottom = Tr("From bottom:");
    Str style = Tr("Grid style:"), color = Tr("Color:");
    float labelDx = 0;
    for (Str l : {units, horz, vert, sub, slant, fromLeft, fromBottom, style, color}) {
        labelDx = std::max(labelDx, DlgFieldLabelDx(cx, l));
    }
    auto row = [&](Str label, gp::El* ctrl) {
        return DlgFieldRow(cx)->Child(DlgFieldLabel(cx, label, labelDx))->Child(ctrl);
    };
    auto edit = [&](gp::Str id, gp::InputState* st) {
        return gp::Div(cx->a)
            ->W(kGridEditDx)
            ->Shrink0()
            ->Child(DlgUnderlineInput(cx, id, st, kToolDlgTextFontPx, colEdge));
    };
    auto select = [&](DialogSelect& dd, Str id) {
        return gp::Div(cx->a)->Flex1()->MinW(0)->Child(dd.Build(cx, id, gp::kFill));
    };

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    gp::El* unitsBox = DlgSection(cx);
    unitsBox->Child(row(units, select(gPageGrid.ddUnits, StrL("grid-units"))));
    col->Child(unitsBox);
    col->Child(DlgHairline(cx));

    gp::El* spacing = DlgSection(cx);
    spacing->Child(DlgSectionHeader(cx, Tr("Distance between grid lines")));
    spacing->Child(row(horz, edit(GStrL("grid-w"), gPageGrid.editWidth)));
    spacing->Child(row(vert, edit(GStrL("grid-h"), gPageGrid.editHeight)));
    spacing->Child(row(sub, edit(GStrL("grid-sub"), gPageGrid.editSub)));
    spacing->Child(row(slant, edit(GStrL("grid-slant"), gPageGrid.editSlant)));
    spacing->Child(DlgSectionHeader(cx, Tr("Drag a line on the page to resize it")));
    spacing->Child(DlgSectionHeader(cx, Tr("Alt+drag when this dialog is closed")));
    spacing->Child(DlgSectionHeader(cx, Tr("Grid line origin offset")));
    spacing->Child(row(fromLeft, edit(GStrL("grid-ox"), gPageGrid.editOffX)));
    spacing->Child(row(fromBottom, edit(GStrL("grid-oy"), gPageGrid.editOffY)));
    col->Child(spacing);
    col->Child(DlgHairline(cx));

    gp::El* look = DlgSection(cx);
    look->Child(row(style, select(gPageGrid.ddStyle, StrL("grid-style"))));
    gp::El* swatch = gp::Div(cx->a)
                         ->W(kGridSwatch)
                         ->H(kGridSwatch)
                         ->Shrink0()
                         ->Radius(kGridSwatchRadius)
                         ->Border(1, ToGpui(colEdge))
                         ->Bg(ToGpui(gPageGrid.currentColor))
                         ->Cursor(gp::CursorKind::Pointer)
                         ->PathClick(GStrL("grid-swatch"))
                         ->OnClick(gp::ListenTo(gPageGridView, &PageGridView::OnPickColor));
    gp::El* colorBox = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(kToolDlgGapDx)->Shrink0();
    colorBox->Child(swatch)->Child(edit(GStrL("grid-color"), gPageGrid.editColor));
    look->Child(row(color, colorBox));
    look->Child(row({}, DlgCheck(cx, StrL("grid-show"), Tr("&Show Grid"), gPageGrid.showGrid,
                                 gp::ListenTo(gPageGridView, &PageGridView::OnShowGrid))));
    col->Child(look);

    gp::El* reset = DlgGhostButton(cx, GStrL("grid-reset"), Tr("Reset to defaults"),
                                   gp::ListenTo(gPageGridView, &PageGridView::OnReset));
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel, reset));
    gp::El* content = DlgToolContent(cx, col, Tr("Page Grid"), kGridDlgDx, onCancel);

    if (gPageGrid.wantFocus) {
        gp::InputFocus(gPageGrid.editWidth, cx->app, cx->win);
        gp::InputSelectAll(gPageGrid.editWidth, cx->app, cx->win);
        gPageGrid.wantFocus = cx->win->input != gPageGrid.editWidth;
    }
    return content;
}
