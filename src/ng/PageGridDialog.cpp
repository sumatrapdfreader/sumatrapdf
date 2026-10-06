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

#include "SumatraLog.h"

static SeqStrings kPageGridUnitTok = "pt\0in\0mm\0cm\0";
static SeqStrings kPageGridStyleTok = "dots\0dotted\0solid\0";

constexpr float kPageGridPtPerIn = 72.f;
constexpr float kPageGridMmPerIn = 25.4f;
constexpr float kPageGridDefaultSizePt = 72.f;
constexpr int kPageGridDefaultSubdivisions = 4;
constexpr Color kPageGridDefaultColor = MkRgb(128, 128, 255);
constexpr int kPageGridDefaultStyleIdx = 0;
constexpr int kPageGridDefaultUnitIdx = 1;

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

void RedrawPageGridWindows() {
    for (MainWindow* w : gWindows) {
        if (w) {
            w->RedrawAll(true);
        }
    }
}

static PageGrid* PageGridPrefs() {
    return gSettings ? &gSettings->fixedPageUI.pageGrid : nullptr;
}

// --- the overlay (orig's Canvas.cpp) ----------------------------------------

enum class PageGridStyleKind {
    Dots,
    Dotted,
    Solid
};

struct PageGridDraw {
    float widthPt = kPageGridDefaultSizePt;
    float heightPt = kPageGridDefaultSizePt;
    int subdiv = kPageGridDefaultSubdivisions;
    float offsetXPt = 0;
    float offsetYPt = 0;
    Color color = kPageGridDefaultColor;
    PageGridStyleKind style = PageGridStyleKind::Dots;
};

static PageGridDraw GetPageGridDraw() {
    PageGridDraw d;
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return d;
    }
    if (pg->width > 0) {
        d.widthPt = pg->width;
    }
    if (pg->height > 0) {
        d.heightPt = pg->height;
    }
    if (pg->subdivisions > 0) {
        d.subdiv = pg->subdivisions;
    }
    d.offsetXPt = pg->offsetX;
    d.offsetYPt = pg->offsetY;
    ParseColor(pg->color);
    if (pg->color.parsedOk && !IsSpecialColor(pg->color.col)) {
        d.color = pg->color.col;
    }
    if (str::EqI(pg->style, StrL("dotted"))) {
        d.style = PageGridStyleKind::Dotted;
    } else if (str::EqI(pg->style, StrL("solid"))) {
        d.style = PageGridStyleKind::Solid;
    }
    d.widthPt = limitValue(d.widthPt, 1.f, 720.f);
    d.heightPt = limitValue(d.heightPt, 1.f, 720.f);
    d.subdiv = limitValue(d.subdiv, 1, 32);
    d.offsetXPt = limitValue(d.offsetXPt, -720.f, 720.f);
    d.offsetYPt = limitValue(d.offsetYPt, -720.f, 720.f);
    return d;
}

static float PageGridAlignDown(float v, float origin, float step) {
    if (step <= 0) {
        return origin;
    }
    return origin + (floorf((v - origin) / step) * step);
}

static bool PageGridIsMajor(float v, float origin, float minorPt, int subdiv) {
    if (minorPt <= 0.f || subdiv < 1) {
        return true;
    }
    int i = (int)floorf(((v - origin) / minorPt) + 0.5f);
    if (i < 0) {
        i = -i;
    }
    return (i % subdiv) == 0;
}

// ng: orig strokes with a GDI pen; here every line is a filled rect in document
// coordinates, which is what CanvasFillRects takes. Style "dots" puts a 1px /
// 3px square at each intersection; "dotted" and "solid" draw the lines (major
// heavier), dotted as a dashed run of short pieces. Skips comics, as orig does.
void PaintPageGrid(DisplayModel* dm, gpui::PaintCtx* ctx) {
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
    Vec<Rect> rects;

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
    if (len(rects) > 0) {
        CanvasFillRects(ctx, rects.els, len(rects), g.color, 255, 0);
    }
}

// --- the dialog -------------------------------------------------------------

struct PageGridSnap {
    float width = kPageGridDefaultSizePt;
    float height = kPageGridDefaultSizePt;
    int subdivisions = kPageGridDefaultSubdivisions;
    float offsetX = 0;
    float offsetY = 0;
    Str color;
    Str style;
    Str units;
    bool showGrid = false;
};

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
    gpui::InputState* editOffX = nullptr;
    gpui::InputState* editOffY = nullptr;
    gpui::InputState* editColor = nullptr;
    // orig's window opens with the first edit focused and selected
    bool wantFocus = false;
};

static PageGridDlg gPageGrid;

// orig's window at 96 dpi: a 360 wide client area, 4 / 8 around. Four tables
// of label + control rows: each table's labels share a column, the controls
// start 16 after it, the rows are 23 high and 6 apart, the edits 66 wide. A
// heading has 8 above and 4 under it; a table after the first 8 above it; the
// buttons 8 above and 4 below
constexpr float kGridWinDx = 360;
constexpr float kGridWinPadX = 8;
constexpr float kGridWinPadY = 4;
constexpr float kGridWinColGap = 16;
constexpr float kGridWinRowGap = 6;
constexpr float kGridWinGap = 8;
constexpr float kGridWinEditDx = 66;
constexpr float kGridWinSwatch = 22;

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

static float PageGridToPt(float v, int unit) {
    switch (unit) {
        case 1:
            return v * kPageGridPtPerIn;
        case 2:
            return v * kPageGridPtPerIn / kPageGridMmPerIn;
        case 3:
            return v * kPageGridPtPerIn / 2.54f;
        default:
            return v;
    }
}

static float PageGridFromPt(float pt, int unit) {
    switch (unit) {
        case 1:
            return pt / kPageGridPtPerIn;
        case 2:
            return pt * kPageGridMmPerIn / kPageGridPtPerIn;
        case 3:
            return pt * 2.54f / kPageGridPtPerIn;
        default:
            return pt;
    }
}

static TempStr PageGridNumTemp(float v) {
    return fmt("%.4g", v);
}

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

static Str PageGridUnitName(int i) {
    switch (i) {
        case 1:
            return Tr("inches");
        case 2:
            return Tr("millimeters");
        case 3:
            return Tr("centimeters");
        default:
            return Tr("points");
    }
}

static Str PageGridStyleName(int i) {
    if (i == 1) {
        return Tr("Dotted lines");
    }
    if (i == 2) {
        return Tr("Solid lines");
    }
    return Tr("Dots");
}

static void CopyPageGridSnap(PageGridSnap& dst, const PageGrid& src, bool showGrid) {
    dst.width = src.width > 0 ? src.width : kPageGridDefaultSizePt;
    dst.height = src.height > 0 ? src.height : kPageGridDefaultSizePt;
    dst.subdivisions = src.subdivisions > 0 ? src.subdivisions : kPageGridDefaultSubdivisions;
    dst.offsetX = src.offsetX;
    dst.offsetY = src.offsetY;
    str::ReplaceWithCopy(&dst.color, src.color.s);
    str::ReplaceWithCopy(&dst.style, src.style);
    str::ReplaceWithCopy(&dst.units, src.units);
    dst.showGrid = showGrid;
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

// Restore the shipped appearance settings as a live preview. Show Grid is a
// session toggle rather than a saved setting, so leave it unchanged.
void ResetPageGridToDefaults() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    pg->width = kPageGridDefaultSizePt;
    pg->height = kPageGridDefaultSizePt;
    pg->subdivisions = kPageGridDefaultSubdivisions;
    pg->offsetX = 0;
    pg->offsetY = 0;
    SetColorText(pg->color, SerializeColorTemp(kPageGridDefaultColor));
    str::ReplaceWithCopy(&pg->style, SeqStrByIndex(kPageGridStyleTok, kPageGridDefaultStyleIdx));
    str::ReplaceWithCopy(&pg->units, SeqStrByIndex(kPageGridUnitTok, kPageGridDefaultUnitIdx));
}

TempStr PageGridStateTemp() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return str::DupTemp(StrL("ERROR no-settings"));
    }
    return fmt("show=%d width=%g height=%g subdiv=%d ox=%g oy=%g color=%s style=%s units=%s\n", ShowPageGrid() ? 1 : 0,
               pg->width, pg->height, pg->subdivisions, pg->offsetX, pg->offsetY, pg->color.s ? pg->color.s : StrL(""),
               pg->style.s ? pg->style : StrL(""), pg->units.s ? pg->units : StrL(""));
}

// orig's window (modeless, as orig's), where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gPageGridTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsPageGridDialogVisible() {
    return gPageGrid.visible && !gPageGridTw;
}

static void FreePageGridInputs() {
    MainWindow* win = gPageGrid.win;
    gp::InputState* edits[] = {gPageGrid.editWidth, gPageGrid.editHeight, gPageGrid.editSub,
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
    gp::InputState** edits[] = {&gPageGrid.editWidth, &gPageGrid.editHeight, &gPageGrid.editSub,
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
    spec.clientDx = kGridWinDx;
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

static gp::El* GridRow(gp::Ctx* cx, Str label, gp::El* ctrl) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* row = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8);
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(13)->Fg(th.foreground)->W(120)->Shrink0());
    row->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(ctrl));
    return row;
}

static gp::El* PageGridWinBuild(gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kGridWinPadX)->PadY(kGridWinPadY);

    // one row of a table whose label column is `labelDx` wide
    auto row = [&](Str label, float labelDx, gp::El* ctrl, float padT, float dy = kDlgWinEditDy) {
        gp::El* r = gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(dy + padT)->PadT(padT)->Shrink0();
        r->Child(gp::Div(cx->a)
                     ->W(labelDx + kGridWinColGap)
                     ->Shrink0()
                     ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(font)->Fg(th.foreground)));
        r->Child(ctrl);
        return r;
    };
    auto edit = [&](gp::Str id, gp::InputState* st) {
        return gpc::Input::New(cx, id, st)
            ->WithSize(gp::UiSize::Small)
            ->W(kGridWinEditDx)
            ->IntoEl()
            ->H(kDlgWinEditDy)
            ->Shrink0();
    };
    auto select = [&](DialogSelect& dd, Str id) {
        gp::El* sel = dd.Build(cx, id, gp::kFill)->H(kDlgWinEditDy);
        if (sel->first) {
            sel->first->H(kDlgWinEditDy);
        }
        return gp::Div(cx->a)->Flex1()->MinW(0)->Child(sel);
    };
    auto heading = [&](Str s) {
        return gp::Div(cx->a)->PadT(kGridWinGap)->Shrink0()->Child(DlgWinLabel(cx, ToGpui(s), font, kGridWinPadY));
    };
    auto widest = [&](std::initializer_list<Str> labels) {
        float dx = 0;
        for (Str s : labels) {
            dx = std::max(dx, DlgWinTextDx(cx, s));
        }
        return dx;
    };

    Str units = Tr("Units:");
    col->Child(row(units, DlgWinTextDx(cx, units), select(gPageGrid.ddUnits, StrL("grid-units")), 0));

    col->Child(heading(Tr("Distance between grid lines")));
    Str horz = Tr("Horizontal:"), vert = Tr("Vertical:"), sub = Tr("Subdivisions:");
    float dx = widest({horz, vert, sub});
    col->Child(row(horz, dx, edit(GStrL("grid-w"), gPageGrid.editWidth), 0));
    col->Child(row(vert, dx, edit(GStrL("grid-h"), gPageGrid.editHeight), kGridWinRowGap));
    col->Child(row(sub, dx, edit(GStrL("grid-sub"), gPageGrid.editSub), kGridWinRowGap));

    col->Child(heading(Tr("Grid line origin offset")));
    Str fromLeft = Tr("From left:"), fromBottom = Tr("From bottom:");
    dx = widest({fromLeft, fromBottom});
    col->Child(row(fromLeft, dx, edit(GStrL("grid-ox"), gPageGrid.editOffX), kGridWinGap));
    col->Child(row(fromBottom, dx, edit(GStrL("grid-oy"), gPageGrid.editOffY), kGridWinRowGap));

    Str style = Tr("Grid style:"), color = Tr("Color:");
    dx = widest({style, color});
    col->Child(row(style, dx, select(gPageGrid.ddStyle, StrL("grid-style")), kGridWinGap));
    gp::El* colorRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(kGridWinGap);
    colorRow->Child(gp::Div(cx->a)
                        ->W(kGridWinSwatch)
                        ->H(kGridWinSwatch)
                        ->Shrink0()
                        ->Border(1, th.foreground)
                        ->Bg(ToGpui(gPageGrid.currentColor))
                        ->Cursor(gp::CursorKind::Pointer)
                        ->PathClick(GStrL("grid-swatch"))
                        ->OnClick(gp::ListenTo(gPageGridView, &PageGridView::OnPickColor)));
    colorRow->Child(edit(GStrL("grid-color"), gPageGrid.editColor));
    col->Child(row(color, dx, colorRow, kGridWinRowGap));
    constexpr float kCheckLabelGap = 2;
    col->Child(row({}, dx,
                   DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("grid-show"))->Checked(gPageGrid.showGrid),
                              Tr("&Show Grid"), gp::ListenTo(gPageGridView, &PageGridView::OnShowGrid))
                       ->Gap(kCheckLabelGap),
                   kGridWinRowGap, kDlgWinCheckDy));

    gp::Listener onOk = gp::ListenTo(gPageGridView, &PageGridView::OnOk);
    DlgSetDefault(cx, onOk, true);
    gp::El* buttons = gp::Div(cx->a)
                          ->FlexRow()
                          ->ItemsCenter()
                          ->W(gp::kFill)
                          ->H(kDlgWinBtnDy + kGridWinGap + kGridWinPadY)
                          ->PadT(kGridWinGap)
                          ->PadB(kGridWinPadY)
                          ->Gap(kDlgWinBtnGap)
                          ->Shrink0();
    buttons->Child(DlgWinButton(cx, GStrL("grid-reset"), Tr("Reset to defaults"),
                                gp::ListenTo(gPageGridView, &PageGridView::OnReset), false));
    buttons->Child(gp::Div(cx->a)->Flex1());
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gPageGridView, &PageGridView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("OK"), onOk, true));
    col->Child(buttons);
    return DlgWinContent(cx, col);
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
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::Listener onChanged = gp::ListenTo(gPageGridView, &PageGridView::OnChanged);
    gPageGrid.editWidth->onChange = onChanged;
    gPageGrid.editHeight->onChange = onChanged;
    gPageGrid.editSub->onChange = onChanged;
    gPageGrid.editOffX->onChange = onChanged;
    gPageGrid.editOffY->onChange = onChanged;
    gPageGrid.editColor->onChange = gp::ListenTo(gPageGridView, &PageGridView::OnColorChanged);

    if (DlgWindowIsHost(cx)) {
        gp::El* content = PageGridWinBuild(cx);
        if (gPageGrid.wantFocus) {
            gp::InputFocus(gPageGrid.editWidth, cx->app, cx->win);
            gp::InputSelectAll(gPageGrid.editWidth, cx->app, cx->win);
            gPageGrid.wantFocus = cx->win->input != gPageGrid.editWidth;
        }
        return content;
    }

    auto numInput = [&](Str id, gp::InputState* st) {
        return gpc::Input::New(cx, GpuiDup(cx->a, id), st)->WithSize(gp::UiSize::Small)->W(100)->IntoEl();
    };

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(6);
    body->Child(GridRow(cx, Tr("Units:"), gPageGrid.ddUnits.Build(cx, StrL("grid-units"), gp::kFill)));
    body->Child(gp::TextEl(cx->a, ToGpui(Tr("Distance between grid lines")))->Font(13)->Bold()->Fg(th.foreground));
    body->Child(GridRow(cx, Tr("Horizontal:"), numInput(StrL("grid-w"), gPageGrid.editWidth)));
    body->Child(GridRow(cx, Tr("Vertical:"), numInput(StrL("grid-h"), gPageGrid.editHeight)));
    body->Child(GridRow(cx, Tr("Subdivisions:"), numInput(StrL("grid-sub"), gPageGrid.editSub)));
    body->Child(gp::TextEl(cx->a, ToGpui(Tr("Grid line origin offset")))->Font(13)->Bold()->Fg(th.foreground));
    body->Child(GridRow(cx, Tr("From left:"), numInput(StrL("grid-ox"), gPageGrid.editOffX)));
    body->Child(GridRow(cx, Tr("From bottom:"), numInput(StrL("grid-oy"), gPageGrid.editOffY)));
    body->Child(GridRow(cx, Tr("Grid style:"), gPageGrid.ddStyle.Build(cx, StrL("grid-style"), gp::kFill)));

    gp::El* colorRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8);
    colorRow->Child(gp::Div(cx->a)
                        ->W(22)
                        ->H(22)
                        ->Shrink0()
                        ->Border(1, th.border)
                        ->Bg(ToGpui(gPageGrid.currentColor))
                        ->Cursor(gp::CursorKind::Pointer)
                        ->PathClick(GStrL("grid-swatch"))
                        ->OnClick(gp::ListenTo(gPageGridView, &PageGridView::OnPickColor)));
    colorRow->Child(
        gp::Div(cx->a)->Flex1()->MinW(0)->Child(gpc::Input::New(cx, GStrL("grid-color"), gPageGrid.editColor)
                                                    ->WithSize(gp::UiSize::Small)
                                                    ->W(gp::kFill)
                                                    ->IntoEl()));
    body->Child(GridRow(cx, Tr("Color:"), colorRow));
    body->Child(DlgAccelEl(cx, gpc::Checkbox::New(cx, GStrL("grid-show"))->Checked(gPageGrid.showGrid),
                           Tr("&Show Grid"), gp::ListenTo(gPageGridView, &PageGridView::OnShowGrid)));

    gp::El* reset = gpc::Button::New(cx, GStrL("grid-reset"))
                        ->Label(ToGpui(Tr("Reset to defaults")))
                        ->WithSize(gp::UiSize::Small)
                        ->OnClick(gp::ListenTo(gPageGridView, &PageGridView::OnReset))
                        ->IntoEl();
    gp::El* footer =
        DialogFooter(cx, reset, gPageGridView, Tr("OK"), Tr("Cancel"), &PageGridView::OnOk, &PageGridView::OnCancel);

    gp::El* dlg = DlgIntoEl(cx, gpc::Dialog::New(cx)
                                    ->Open(true)
                                    ->Title(ToGpui(Tr("Page Grid")))
                                    ->Body(body)
                                    ->Footer(footer)
                                    ->W(420)
                                    ->OnClose(gp::ListenTo(gPageGridView, &PageGridView::OnCancel)));
    if (gPageGrid.wantFocus) {
        gPageGrid.wantFocus = false;
        gp::InputFocus(gPageGrid.editWidth, cx->app, cx->win);
        gp::InputSelectAll(gPageGrid.editWidth, cx->app, cx->win);
    }
    return dlg;
}
