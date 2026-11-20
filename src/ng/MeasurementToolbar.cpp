/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The Measure window (CmdMeasure): orig's tool window of virtual controls,
// here a gpui window of its own (DlgWindowOpen) with the same rows. The model,
// the label text, hit testing and the expression evaluator are shared with
// orig (MeasurementToolbarCommon.cpp); this has the window, the clicks on the
// canvas and the drawing.
//
// ng: on macOS Command + click deletes a measurement, as Command stands in for
// Ctrl in the canvas's other clicks (CanvasCtrlPressed).

#include "gui/GpuiBridge.h"
#include <float.h>

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "SumatraPDF.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "Theme.h"
#include "AnnotPlacement.h"
#include "AnnotEditToolbar.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "shared/PdfToolsCommon.h"
#include "MeasurementToolbarCommon.h"
#include "MeasurementToolbar.h"

#include "SumatraLog.h"

// orig sizes its window to the layout at 440 px wide
constexpr float kMeasureDlgDx = 440;
constexpr float kMeasureEditDx = 140;
// the labels on the page: orig's app font
constexpr float kLabelFontPx = 12;

struct MeasurementToolbar : MeasurementState {
    MainWindow* win = nullptr;
    ToolWindow* tw = nullptr;
    DialogSelect mode;
    gp::InputState* knownLength = nullptr;
    gp::InputState* unitEdit = nullptr;
    bool snap = true;
    // what orig's VirtTexts and button states show
    Str result;
    Str status;
    bool applyEnabled = false;
    bool clearEnabled = false;
    bool resetEnabled = false;

    ~MeasurementToolbar() {
        str::Free(unit);
        str::Free(result);
    }
    void SetMode();
    void Update();
    void Calibrate();
    void Clear();
    void ResetScale();
    double DefaultScale();
    void SaveScale();
};

struct MeasureView {
    static void OnSnap(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnApply(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnClear(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnReset(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnClose(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<MeasureView> gMeasureView;
// the one open Measure window; orig allows one per main window, but the
// dialog window layer calls back without one
static MeasurementToolbar* gMeasure = nullptr;

// orig's HwndSetFocus(win->hwndFrame)
static void FocusFrame(MainWindow* win) {
    AppShellFocusFrame(win);
    gp::WindowActivateInApp(win->gpuiWin);
}

//--- the window

// the size of a document unit in points: PDF units are points, images are pixels
double MeasurementToolbar::DefaultScale() {
    float dpi = win->AsFixed()->GetEngine()->fileDPI;
    return dpi > 0 ? kPointsPerInch / dpi : 1;
}

void MeasurementToolbar::SaveScale() {
    FileState* fs = FileHistoryFindByPath(win->CurrentTab()->filePath);
    if (!fs) {
        return;
    }
    bool isDefault = str::Eq(unit, StrL("pt")) && scale == DefaultScale();
    fs->measurementScale = isDefault ? 0 : (float)scale;
    str::ReplaceWithCopy(&fs->measurementUnit, isDefault ? Str{} : unit);
    ScheduleSaveSettings();
}

void MeasurementToolbar::Update() {
    applyEnabled = haveCalib && Dist(calib.pts[0], calib.pts[1]) > 0;
    PointF pts[3];
    TempStr text;
    if (nPts > 0) {
        int n = MeasurementPoints(this, cur, true, pts);
        text = MeasurementTextTemp(this, cur, n, pts);
    } else if (tool == MeasureTool::Calibrate && haveCalib) {
        text = MeasurementTextTemp(this, calib, 2, calib.pts);
    } else if (len(done) > 0) {
        const Measurement& m = done[len(done) - 1];
        int n = MeasurementPoints(this, m, false, pts);
        text = MeasurementTextTemp(this, m, n, pts);
    }
    str::ReplaceWithCopy(&result, len(text) > 0 ? Str(text) : Str(fmt(Tr("Units: %s").s, unit)));

    if (nPts == 0) {
        status = tool == MeasureTool::Calibrate && haveCalib
                     ? Tr("Enter the known length and units, then apply.")
                     : Tr("Click the start point. Ctrl+click deletes a measurement.");
    } else if (nPts == 1 && tool == MeasureTool::Radius) {
        status = Tr("Click the other end of the arc.");
    } else if (nPts == 2) {
        status = Tr("Click a point on the arc.");
    } else {
        status = Tr("Click the end point on the same page.");
    }
    clearEnabled = len(done) > 0 || haveCalib;
    resetEnabled = scale != DefaultScale() || !str::Eq(unit, StrL("pt"));
    if (tw) {
        ToolWindowInvalidate(tw);
    }
    if (win) {
        AppShellInvalidate(win);
    }
}

void MeasurementToolbar::SetMode() {
    tool = (MeasureTool)mode.sel;
    nPts = 0;
    haveHover = false;
    Update();
    if (win) {
        FocusFrame(win);
    }
}

void MeasurementToolbar::Calibrate() {
    if (!win || !haveCalib) {
        return;
    }
    double dist = Dist(calib.pts[0], calib.pts[1]);
    Str text = InputTextTemp(knownLength);
    str::TrimWsBoth(text);
    double value = 0;
    bool parsed = len(text) > 0 && EvalLength(text, &value);
    Str units = InputTextTemp(unitEdit);
    str::TrimWsBoth(units);
    double ratio = dist > 0 ? value / dist : 0;
    if (!parsed || !isfinite(value) || value <= 0 || !isfinite(ratio) || ratio <= 0 || ratio > FLT_MAX ||
        (float)ratio == 0 || len(units) == 0) {
        status = Tr("Enter a positive length and a unit name.");
        if (tw) {
            ToolWindowInvalidate(tw);
        }
        return;
    }
    scale = (float)ratio;
    str::ReplaceWithCopy(&unit, units);
    SaveScale();
    Update();
    FocusFrame(win);
}

void MeasurementToolbar::Clear() {
    VecReset(done);
    haveCalib = false;
    nPts = 0;
    Update();
}

void MeasurementToolbar::ResetScale() {
    scale = DefaultScale();
    str::ReplaceWithCopy(&unit, StrL("pt"));
    gp::InputSetValue(unitEdit, ToGpui(unit));
    SaveScale();
    Update();
}

void MeasureView::OnSnap(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gMeasure) {
        gMeasure->snap = !gMeasure->snap;
    }
    gp::Notify(cx);
}

void MeasureView::OnApply(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gMeasure) {
        gMeasure->Calibrate();
    }
    gp::Notify(cx);
}

void MeasureView::OnClear(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gMeasure) {
        gMeasure->Clear();
    }
    gp::Notify(cx);
}

void MeasureView::OnReset(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gMeasure) {
        gMeasure->ResetScale();
    }
    gp::Notify(cx);
}

static void CloseMeasureWindow() {
    if (gMeasure) {
        CloseMeasurement(gMeasure->win);
    }
}

void MeasureView::OnClose(MeasureView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseMeasureWindow();
    gp::Notify(cx);
}

static Str MeasureTitle() {
    return Tr("Measure");
}

static gp::El* MeasureButton(gp::Ctx* cx, gp::Str id, Str label, gp::Listener onClick, bool enabled) {
    return gpc::Button::New(cx, id)
        ->Label(ToGpui(label))
        ->Outline()
        ->WithSize(gp::UiSize::Small)
        ->Disabled(!enabled)
        ->OnClick(onClick)
        ->IntoEl();
}

// orig's layout: a table of Tool / Known length / Units, the snap checkbox,
// the three buttons, then the result and the status line
static gp::El* MeasureBuild(MainWindow* win, gp::Ctx* cx) {
    MeasurementToolbar* bar = gMeasure;
    if (!bar || bar->win != win || !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gMeasureView.IsValid()) {
        gMeasureView = gp::EntityNewState<MeasureView>(cx->app);
    }
    if (bar->mode.PollChanged(cx->app)) {
        bar->SetMode();
    }
    bool calibrating = bar->tool == MeasureTool::Calibrate;
    Color colEdge = ThemeEdgeColor();

    Str toolLabel = Tr("Tool:"), lengthLabel = Tr("Known length:"), unitsLabel = Tr("Units:");
    float labelDx = 0;
    for (Str l : {toolLabel, lengthLabel, unitsLabel}) {
        labelDx = std::max(labelDx, DlgFieldLabelDx(cx, l));
    }
    auto row = [&](Str label, gp::El* ctrl) {
        return DlgFieldRow(cx)->Child(DlgFieldLabel(cx, label, labelDx))->Child(ctrl);
    };
    auto edit = [&](gp::Str id, gp::InputState* st) {
        return gp::Div(cx->a)
            ->W(kMeasureEditDx)
            ->Shrink0()
            ->Child(DlgUnderlineInput(cx, id, st, kToolDlgTextFontPx, colEdge, false, !calibrating));
    };

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    gp::El* fields = DlgSection(cx);
    fields->Child(
        row(toolLabel, gp::Div(cx->a)->Flex1()->MinW(0)->Child(bar->mode.Build(cx, StrL("measure-tool"), gp::kFill))));
    fields->Child(row(lengthLabel, edit(GStrL("measure-length"), bar->knownLength)));
    fields->Child(row(unitsLabel, edit(GStrL("measure-unit"), bar->unitEdit)));
    fields->Child(DlgFieldRow(cx)->Child(DlgCheck(cx, StrL("measure-snap"), Tr("&Snap to lines in the drawing"),
                                                  bar->snap, gp::ListenTo(gMeasureView, &MeasureView::OnSnap))));

    gp::El* buttons = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(kToolDlgGapDx)->PadY(kToolDlgFilesPadDy);
    buttons->Child(MeasureButton(cx, GStrL("measure-apply"), Tr("Apply calibration"),
                                 gp::ListenTo(gMeasureView, &MeasureView::OnApply), bar->applyEnabled));
    buttons->Child(MeasureButton(cx, GStrL("measure-clear"), Tr("Clear"),
                                 gp::ListenTo(gMeasureView, &MeasureView::OnClear), bar->clearEnabled));
    buttons->Child(MeasureButton(cx, GStrL("measure-reset"), Tr("Reset scale"),
                                 gp::ListenTo(gMeasureView, &MeasureView::OnReset), bar->resetEnabled));
    fields->Child(buttons);

    Color colText = ThemeWindowTextColor();
    fields->Child(gp::TextEl(cx->a, GpuiDup(cx->a, bar->result))->Font(kToolDlgQueryFontPx)->Fg(ToGpui(colText)));

    Str status = bar->status;
#if OS_DARWIN
    // Command stands in for Ctrl on the canvas
    status = str::ReplaceTemp(status, StrL("Ctrl+click"), StrL("Cmd+click"));
#endif
    fields->Child(
        gp::Div(cx->a)
            ->PadT(kToolDlgFilesPadDy)
            ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, status))->Font(kToolDlgTextFontPx)->Fg(ToGpui(colText))));
    col->Child(fields);
    return DlgToolContent(cx, col, MeasureTitle(), kMeasureDlgDx, gp::ListenTo(gMeasureView, &MeasureView::OnClose));
}

static void DeleteMeasurementToolbar(MeasurementToolbar* bar) {
    gp::Window* gw = bar->win ? bar->win->gpuiWin : nullptr;
    gp::InputState* edits[] = {bar->knownLength, bar->unitEdit};
    for (gp::InputState* e : edits) {
        if (e) {
            InputBlurWhereFocused(e, gw);
        }
        delete e;
    }
    bar->mode.Free();
    delete bar;
}

void ToggleMeasurement(MainWindow* win) {
    if (win->measurementToolbar) {
        CloseMeasurement(win);
        return;
    }
    if (!win->AsFixed() || !ToolWindowsAvailable()) {
        return;
    }
    // one Measure window at a time: the dialog window layer has no owner pointer to hand back
    if (gMeasure) {
        CloseMeasurement(gMeasure->win);
    }
    CancelAnnotationPlacement(win);
    CloseSignDocumentDialog(win);
    DeleteLinkTooltip(win);
    HideAnnotationHoverOverlay(win);

    auto* bar = new MeasurementToolbar();
    bar->win = win;
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    bar->knownLength = new gp::InputState();
    bar->knownLength->focus = gp::FocusHandleNew(app);
    bar->unitEdit = new gp::InputState();
    bar->unitEdit->focus = gp::FocusHandleNew(app);
    gp::InputSetValue(bar->knownLength, ToGpui(StrL("1")));
    bar->mode.Init(app);
    // in MeasureTool order
    StrVec items;
    items.Append(Tr("Length"));
    items.Append(Tr("Rectangular area"));
    items.Append(Tr("Radius"));
    items.Append(Tr("Guide line"));
    items.Append(Tr("Calibrate"));
    bar->mode.SetItems(items, (int)bar->tool);

    bar->scale = bar->DefaultScale();
    str::ReplaceWithCopy(&bar->unit, StrL("pt"));
    FileState* fs = FileHistoryFindByPath(win->CurrentTab()->filePath);
    if (fs && isfinite(fs->measurementScale) && fs->measurementScale > 0 && len(fs->measurementUnit) > 0) {
        bar->scale = fs->measurementScale;
        str::ReplaceWithCopy(&bar->unit, fs->measurementUnit);
    }
    gp::InputSetValue(bar->unitEdit, ToGpui(bar->unit));
    win->measurementToolbar = bar;
    gMeasure = bar;
    bar->SetMode();

    DlgWindowSpec spec;
    spec.name = "measure";
    spec.title = MeasureTitle;
    spec.modal = false;
    // orig's is owned by the main window, which stays enabled
    spec.owned = true;
    spec.build = MeasureBuild;
    spec.close = CloseMeasureWindow;
    spec.clientDx = kMeasureDlgDx;
    spec.resizable = true;
    spec.minClientDx = kMeasureDlgDx;
    bar->tw = DlgWindowOpen(spec, win);
    if (!bar->tw) {
        win->measurementToolbar = nullptr;
        gMeasure = nullptr;
        DeleteMeasurementToolbar(bar);
    }
}

void CloseMeasurement(MainWindow* win) {
    if (!win || !win->measurementToolbar) {
        return;
    }
    auto* bar = win->measurementToolbar;
    win->measurementToolbar = nullptr;
    if (gMeasure == bar) {
        gMeasure = nullptr;
    }
    DlgWindowClose(&bar->tw);
    DeleteMeasurementToolbar(bar);
    if (win->canvasCursor == (int)gp::CursorKind::Crosshair) {
        CanvasSetCursor(win, (int)gp::CursorKind::Arrow);
    }
    AppShellInvalidate(win);
}

// Drops the measurement being clicked; false if there was none.
bool MeasurementCancel(MainWindow* win) {
    auto* bar = win ? win->measurementToolbar : nullptr;
    if (!bar || bar->nPts == 0) {
        return false;
    }
    bar->nPts = 0;
    bar->haveHover = false;
    bar->Update();
    return true;
}

//--- clicks on the page

// pt (on the screen) as a page point, moved to the nearest line end in reach
static PointF SnappedPoint(MeasurementToolbar* bar, DisplayModel* dm, int pageNo, Point pt) {
    PointF p = dm->CvtFromScreen(pt, pageNo);
    if (!bar->snap) {
        return p;
    }
    if (bar->snapPageNo != pageNo) {
        bar->snapPageNo = pageNo;
        EngineMupdfPagePathPoints(dm->GetEngine(), pageNo, bar->snapPts);
    }
    double reach = ScreenToPageDist(dm, pageNo, pt, DpiScale(kSnapPx));
    double best = reach * reach;
    PointF res = p;
    for (PointF q : bar->snapPts) {
        double dx = (double)q.x - p.x;
        double dy = (double)q.y - p.y;
        double d = (dx * dx) + (dy * dy);
        if (d <= best) {
            best = d;
            res = q;
        }
    }
    return res;
}

bool MeasurementOnClick(MainWindow* win, Point pt, bool ctrl) {
    auto* bar = win->measurementToolbar;
    if (!bar || !win->AsFixed()) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    int pageNo = dm->GetPageNoByPoint(pt);
    if (pageNo < 1) {
        return true;
    }
    FocusFrame(win);
    if (ctrl) {
        EraseAt(bar, dm, pageNo, pt);
        bar->Update();
        return true;
    }
    if (bar->nPts > 0 && pageNo != bar->cur.pageNo) {
        // a measurement stays on the page it started on
        return true;
    }
    PointF p = SnappedPoint(bar, dm, pageNo, pt);
    bar->lastPt = p;
    if (bar->nPts == 0) {
        bar->cur = {};
        bar->cur.tool = bar->tool;
        bar->cur.pageNo = pageNo;
    }
    bar->cur.pts[bar->nPts++] = p;
    bar->hover = p;
    bar->haveHover = true;
    if (bar->nPts == PointsNeeded(bar->tool)) {
        if (bar->tool == MeasureTool::Calibrate) {
            bar->calib = bar->cur;
            bar->haveCalib = true;
        } else {
            VecAppend(bar->done, bar->cur);
        }
        bar->nPts = 0;
    }
    bar->Update();
    return true;
}

bool MeasurementOnMove(MainWindow* win, Point pt) {
    auto* bar = win->measurementToolbar;
    DisplayModel* dm = win->AsFixed();
    if (!bar || !dm || win->mouseAction != MouseAction::None) {
        return false;
    }
    CanvasSetCursor(win, (int)gp::CursorKind::Crosshair);
    if (bar->nPts > 0 && dm->GetPageNoByPoint(pt) == bar->cur.pageNo) {
        bar->hover = SnappedPoint(bar, dm, bar->cur.pageNo, pt);
        bar->haveHover = true;
        bar->Update();
    }
    return true;
}

// orig's OnSetCursor: the cross while measuring, and no hover tips
bool MeasurementOnSetCursor(MainWindow* win) {
    if (!win->measurementToolbar || win->mouseAction != MouseAction::None) {
        return false;
    }
    CanvasSetCursor(win, (int)gp::CursorKind::Crosshair);
    return true;
}

//--- drawing

// where a label goes relative to its point
enum class LabelAt {
    Center,
    // its bottom a little above the point, clear of a line through it
    Above,
};

static void DrawLabel(gp::PaintCtx* ctx, Str text, Point at, LabelAt where = LabelAt::Center) {
    if (len(text) == 0) {
        return;
    }
    Size sz = CanvasMeasureText(ctx, text, kLabelFontPx);
    int pad = DpiScale(3);
    int dy = sz.dy + (2 * pad);
    // ng: orig puts the center 14 px above the line, which gpui's taller line box would overlap
    int y = where == LabelAt::Above ? at.y - dy - DpiScale(4) : at.y - (dy / 2);
    Rect r{at.x - (sz.dx / 2) - pad, y, sz.dx + (2 * pad), dy};
    CanvasFillRoundRect(ctx, r, DpiScale(3), MkRgb(255, 255, 255), kColMeasure);
    CanvasDrawText(ctx, text, Point{r.x + pad, r.y + pad}, kColMeasure, kLabelFontPx);
}

static void DrawCross(DisplayModel* dm, gp::PaintCtx* ctx, int pageNo, PointF p) {
    Point s = dm->CvtToScreen(pageNo, p);
    int c = DpiScale(5);
    CanvasDrawLine(ctx, {s.x - c, s.y}, {s.x + c, s.y}, kColMeasure, 1.5f);
    CanvasDrawLine(ctx, {s.x, s.y - c}, {s.x, s.y + c}, kColMeasure, 1.5f);
}

// orig's DrawLineAA with an alpha
static Color WithAlpha(Color col, u8 alpha) {
    return MkRgba(GetRed(col), GetGreen(col), GetBlue(col), alpha);
}

static void PaintOne(MeasurementToolbar* bar, DisplayModel* dm, gp::PaintCtx* ctx, const Measurement& m,
                     bool inProgress) {
    PointF pts[3];
    int n = MeasurementPoints(bar, m, inProgress, pts);
    int pageNo = m.pageNo;
    float w = (float)DpiScale(2);
    auto toScreen = [&](PointF p) { return dm->CvtToScreen(pageNo, p); };
    for (int i = 0; i < n; i++) {
        DrawCross(dm, ctx, pageNo, pts[i]);
    }
    if (n < 2) {
        return;
    }
    Point a = toScreen(pts[0]);
    Point b = toScreen(pts[1]);
    TempStr text = MeasurementTextTemp(bar, m, n, pts);
    switch (m.tool) {
        case MeasureTool::Length:
        case MeasureTool::Calibrate:
            CanvasDrawLine(ctx, a, b, kColMeasure, w);
            DrawLabel(ctx, text, Point{(a.x + b.x) / 2, (a.y + b.y) / 2}, LabelAt::Above);
            break;
        case MeasureTool::Area: {
            Point c1 = toScreen(PointF(pts[1].x, pts[0].y));
            Point c2 = toScreen(PointF(pts[0].x, pts[1].y));
            CanvasDrawLine(ctx, a, c1, kColMeasure, w);
            CanvasDrawLine(ctx, c1, b, kColMeasure, w);
            CanvasDrawLine(ctx, b, c2, kColMeasure, w);
            CanvasDrawLine(ctx, c2, a, kColMeasure, w);
            DrawLabel(ctx, text, Point{(a.x + b.x) / 2, (a.y + b.y) / 2});
            break;
        }
        case MeasureTool::Radius: {
            PointF c;
            double r = 0;
            if (n < 3 || !Circle(pts[0], pts[1], pts[2], &c, &r)) {
                CanvasDrawLine(ctx, a, b, WithAlpha(kColMeasure, 128), 1.0f);
                break;
            }
            // the arc from the first point to the second through the third
            double a0 = atan2((double)pts[0].y - c.y, (double)pts[0].x - c.x);
            double a1 = atan2((double)pts[1].y - c.y, (double)pts[1].x - c.x);
            double a2 = atan2((double)pts[2].y - c.y, (double)pts[2].x - c.x);
            double twoPi = 2 * M_PI;
            double d1 = fmod(a1 - a0 + (2 * twoPi), twoPi);
            double d2 = fmod(a2 - a0 + (2 * twoPi), twoPi);
            double sweep = d2 < d1 ? d1 : d1 - twoPi;
            Point prev = a;
            for (int i = 1; i <= kArcSegments; i++) {
                double t = a0 + (sweep * i / kArcSegments);
                Point p = toScreen(PointF((float)(c.x + (r * cos(t))), (float)(c.y + (r * sin(t)))));
                CanvasDrawLine(ctx, prev, p, kColMeasure, w);
                prev = p;
            }
            Point cs = toScreen(c);
            Point mid = toScreen(pts[2]);
            CanvasDrawLine(ctx, cs, mid, WithAlpha(kColMeasure, 128), 1.0f);
            DrawCross(dm, ctx, pageNo, c);
            DrawLabel(ctx, text, Point{(cs.x + mid.x) / 2, (cs.y + mid.y) / 2});
            break;
        }
        case MeasureTool::Guide: {
            // as long as the page is wide, both ways; the page clip cuts it
            RectF page = dm->GetEngine()->PageMediabox(pageNo);
            double l = Dist(pts[0], pts[1]);
            if (l <= 0) {
                break;
            }
            double ext = (page.dx + page.dy) / l;
            float ex = (float)((pts[1].x - pts[0].x) * ext);
            float ey = (float)((pts[1].y - pts[0].y) * ext);
            Point g0 = toScreen(PointF(pts[0].x - ex, pts[0].y - ey));
            Point g1 = toScreen(PointF(pts[0].x + ex, pts[0].y + ey));
            CanvasDrawLine(ctx, g0, g1, WithAlpha(kColGuide, 200), 1.0f);
            break;
        }
    }
}

void PaintMeasurement(MainWindow* win, gp::PaintCtx* ctx) {
    auto* bar = win->measurementToolbar;
    DisplayModel* dm = win->AsFixed();
    if (!bar || !dm) {
        return;
    }
    auto paint = [&](const Measurement& m, bool inProgress) {
        if (!dm->PageVisible(m.pageNo)) {
            return;
        }
        Rect page = dm->CvtToScreen(m.pageNo, dm->GetEngine()->PageMediabox(m.pageNo));
        CanvasClipPush(ctx, page);
        PaintOne(bar, dm, ctx, m, inProgress);
        CanvasClipPop(ctx);
    };
    for (const Measurement& m : bar->done) {
        paint(m, false);
    }
    if (bar->haveCalib && bar->tool == MeasureTool::Calibrate && bar->nPts == 0) {
        paint(bar->calib, false);
    }
    if (bar->nPts > 0) {
        paint(bar->cur, true);
    }
}

//--- for tests

// state: "show=1 page=1 complete=1 count=2 scale=.. length=.. area=.. radius=.. unit=m x=.. y=.."
// length / area / radius are of the measurement being made, else the last one
TempStr TestMeasurement(MainWindow* win, Str action, int pageNo, PointF pt, Str length, Str unit) {
    auto* bar = win ? win->measurementToolbar : nullptr;
    if (!bar) {
        return StrL("show=0");
    }
    bool isClick = str::Eq(action, StrL("click"));
    bool isErase = str::Eq(action, StrL("erase"));
    DisplayModel* dm = win->AsFixed();
    if ((isClick || isErase) && dm && dm->ValidPageNo(pageNo)) {
        Point p = dm->CvtToScreen(pageNo, pt);
        if (isErase) {
            MeasurementOnClick(win, p, true);
        } else {
            // orig posts WM_LBUTTONDOWN / UP without MK_CONTROL
            CanvasSetKeyModifiers(false, false, false);
            DocCanvasClick(win, p.x, p.y);
        }
    } else if (str::Eq(action, StrL("apply"))) {
        gp::InputSetValue(bar->knownLength, ToGpui(length));
        gp::InputSetValue(bar->unitEdit, ToGpui(unit));
        bar->Calibrate();
    } else if (str::Eq(action, StrL("clear"))) {
        bar->Clear();
    } else if (str::Eq(action, StrL("reset"))) {
        bar->ResetScale();
    } else if (str::Eq(action, StrL("snap"))) {
        bar->snap = pageNo != 0;
        if (bar->tw) {
            ToolWindowInvalidate(bar->tw);
        }
    } else if (!str::Eq(action, StrL("state"))) {
        MeasureTool tool = MeasureTool::Length;
        const char* names[] = {"length", "area", "radius", "guide", "calibrate"};
        for (int i = 0; i < dimofi(names); i++) {
            if (str::Eq(action, Str(names[i]))) {
                tool = (MeasureTool)i;
            }
        }
        gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
        bar->mode.SetSel(app, (int)tool);
        bar->SetMode();
    }
    return MeasurementTestStateTemp(bar);
}
