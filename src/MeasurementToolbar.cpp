/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include <float.h>
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
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
#include "MeasurementToolbar.h"

// The Measure window: pick a tool, then click points on the page. Points are
// kept in page coordinates, so zooming, scrolling or rotating between clicks
// doesn't move them, and finished measurements stay on the page, labeled,
// until cleared or the window closes.
//
//   Length      2 clicks   length, dx, dy
//   Area        2 clicks   opposite corners of a rectangle
//   Radius      3 clicks   two ends of an arc and a point on it
//   Guide line  2 clicks   a line across the whole page, for lining things up
//   Calibrate   2 clicks   a known length, then Apply with its real size
//
// Clicks snap to the ends and corners of the page's vector lines.
// Ctrl + click deletes a measurement.

enum class MeasureTool {
    Length,
    Area,
    Radius,
    Guide,
    Calibrate,
};

constexpr double kPointsPerInch = 72;
// how near (in screen pixels) a click must be to snap to a point or hit a measurement
constexpr int kSnapPx = 8;
constexpr int kHitPx = 6;
// segments a radius arc is drawn with
constexpr int kArcSegments = 64;

static const Color kColMeasure = MkRgb(220, 50, 30);
static const Color kColGuide = MkRgb(120, 120, 120);

struct Measurement {
    MeasureTool tool = MeasureTool::Length;
    int pageNo = 0;
    PointF pts[3];
};

struct MeasurementToolbar : WindowBase {
    MainWindow* win = nullptr;
    DropDown* mode = nullptr;
    Edit* knownLength = nullptr;
    Edit* unitEdit = nullptr;
    Checkbox* snap = nullptr;
    VirtButton* apply = nullptr;
    VirtButton* clear = nullptr;
    VirtButton* resetScale = nullptr;
    VirtText* result = nullptr;
    VirtText* status = nullptr;
    PlatformFont* labelFont = nullptr;
    MeasureTool tool = MeasureTool::Length;

    Vec<Measurement> done;
    // the one being clicked: its first nPts points are set
    Measurement cur;
    int nPts = 0;
    PointF hover;
    bool haveHover = false;
    // the last two-point line drawn with Calibrate
    Measurement calib;
    bool haveCalib = false;

    // snap points of one page, computed when that page is first hovered
    int snapPageNo = 0;
    Vec<PointF> snapPts;
    PointF lastPt;

    double scale = 1;
    Str unit;

    ~MeasurementToolbar() override { str::Free(unit); }
    bool Create(MainWindow*);
    void SetMode();
    void Update();
    void Calibrate(VirtMouseEvent* = nullptr);
    void Clear(VirtMouseEvent* = nullptr);
    void ResetScale(VirtMouseEvent* = nullptr);
    void OnClose(WindowBase::CloseEvent*);
    void OnDestroy(WindowBase::DestroyEvent*);
    double DefaultScale();
    void SaveScale();
};

static int PointsNeeded(MeasureTool tool) {
    return tool == MeasureTool::Radius ? 3 : 2;
}

static double Dist(PointF a, PointF b) {
    double dx = (double)b.x - a.x;
    double dy = (double)b.y - a.y;
    return sqrt((dx * dx) + (dy * dy));
}

// center and radius of the circle through a, b, c; false if they're on a line
static bool Circle(PointF a, PointF b, PointF c, PointF* center, double* r) {
    double ax = a.x, ay = a.y, bx = b.x, by = b.y, cx = c.x, cy = c.y;
    double d = 2 * ((ax * (by - cy)) + (bx * (cy - ay)) + (cx * (ay - by)));
    if (fabs(d) < 1e-9) {
        return false;
    }
    double a2 = (ax * ax) + (ay * ay);
    double b2 = (bx * bx) + (by * by);
    double c2 = (cx * cx) + (cy * cy);
    double ux = ((a2 * (by - cy)) + (b2 * (cy - ay)) + (c2 * (ay - by))) / d;
    double uy = ((a2 * (cx - bx)) + (b2 * (ax - cx)) + (c2 * (bx - ax))) / d;
    *center = PointF((float)ux, (float)uy);
    *r = Dist(*center, a);
    return true;
}

// the points of m that are set, with the cursor as the next one while it's
// being clicked
static int MeasurementPoints(MeasurementToolbar* bar, const Measurement& m, bool inProgress, PointF* pts) {
    int n = PointsNeeded(m.tool);
    if (!inProgress) {
        for (int i = 0; i < n; i++) {
            pts[i] = m.pts[i];
        }
        return n;
    }
    for (int i = 0; i < bar->nPts; i++) {
        pts[i] = m.pts[i];
    }
    int k = bar->nPts;
    if (bar->haveHover && k < n) {
        pts[k++] = bar->hover;
    }
    return k;
}

//--- arithmetic in the known length field: "100-20", "2*50", "25/2", "(3+1)*2"

struct ExprParser {
    const char* s;
    bool ok = true;

    void SkipWs() {
        while (*s == ' ' || *s == '\t') {
            s++;
        }
    }

    double Factor() {
        SkipWs();
        if (*s == '-') {
            s++;
            return -Factor();
        }
        if (*s == '(') {
            s++;
            double v = Expr();
            SkipWs();
            if (*s != ')') {
                ok = false;
                return 0;
            }
            s++;
            return v;
        }
        char* end = nullptr;
        double v = strtod(s, &end);
        if (end == s) {
            ok = false;
            return 0;
        }
        s = end;
        return v;
    }

    double Term() {
        double v = Factor();
        for (;;) {
            SkipWs();
            if (*s == '*') {
                s++;
                v *= Factor();
            } else if (*s == '/') {
                s++;
                v /= Factor();
            } else {
                return v;
            }
        }
    }

    double Expr() {
        double v = Term();
        for (;;) {
            SkipWs();
            if (*s == '+') {
                s++;
                v += Term();
            } else if (*s == '-') {
                s++;
                v -= Term();
            } else {
                return v;
            }
        }
    }
};

static bool EvalLength(Str text, double* out) {
    ExprParser p{CStrTemp(text)};
    double v = p.Expr();
    p.SkipWs();
    if (!p.ok || *p.s != 0) {
        return false;
    }
    *out = v;
    return true;
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

static TempStr MeasurementTextTemp(MeasurementToolbar* bar, const Measurement& m, int n, const PointF* pts) {
    double s = bar->scale;
    Str u = bar->unit;
    // nothing to say yet about a measurement whose points are the same
    if (n < 2 || Dist(pts[0], pts[1]) <= 0) {
        return {};
    }
    switch (m.tool) {
        case MeasureTool::Length:
        case MeasureTool::Calibrate: {
            double dx = fabs((double)pts[1].x - pts[0].x) * s;
            double dy = fabs((double)pts[1].y - pts[0].y) * s;
            return fmt("%.4g %s (x %.4g, y %.4g)", Dist(pts[0], pts[1]) * s, u, dx, dy);
        }
        case MeasureTool::Area: {
            double dx = fabs((double)pts[1].x - pts[0].x) * s;
            double dy = fabs((double)pts[1].y - pts[0].y) * s;
            return fmt("%.4g \xC3\x97 %.4g = %.4g %s\xC2\xB2", dx, dy, dx * dy, u);
        }
        case MeasureTool::Radius: {
            PointF c;
            double r = 0;
            if (n < 3 || !Circle(pts[0], pts[1], pts[2], &c, &r)) {
                return {};
            }
            return fmt("R %.4g %s", r * s, u);
        }
        case MeasureTool::Guide:
            return {};
    }
    return {};
}

void MeasurementToolbar::Update() {
    apply->SetIsEnabled(haveCalib && Dist(calib.pts[0], calib.pts[1]) > 0);
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
    result->SetText(len(text) > 0 ? Str(text) : Str(fmt(Tr("Units: %s").s, unit)));

    Str st;
    if (nPts == 0) {
        st = tool == MeasureTool::Calibrate && haveCalib
                 ? Tr("Enter the known length and units, then apply.")
                 : Tr("Click the start point. Ctrl+click deletes a measurement.");
    } else if (nPts == 1 && tool == MeasureTool::Radius) {
        st = Tr("Click the other end of the arc.");
    } else if (nPts == 2) {
        st = Tr("Click a point on the arc.");
    } else {
        st = Tr("Click the end point on the same page.");
    }
    status->SetText(st);
    clear->SetIsEnabled(len(done) > 0 || haveCalib);
    resetScale->SetIsEnabled(scale != DefaultScale() || !str::Eq(unit, StrL("pt")));
    HwndInvalidate(hwnd);
    if (win) {
        win->RedrawCanvas();
    }
}

void MeasurementToolbar::SetMode() {
    tool = (MeasureTool)CbGetCurrentSelection(mode);
    nPts = 0;
    haveHover = false;
    knownLength->SetIsEnabled(tool == MeasureTool::Calibrate);
    unitEdit->SetIsEnabled(tool == MeasureTool::Calibrate);
    Update();
    if (win) {
        HwndSetFocus(win->hwndFrame);
    }
}

void MeasurementToolbar::Calibrate(VirtMouseEvent*) {
    if (!win || !haveCalib) {
        return;
    }
    double dist = Dist(calib.pts[0], calib.pts[1]);
    Str text = knownLength->GetTextTemp();
    str::TrimWsBoth(text);
    double value = 0;
    bool parsed = len(text) > 0 && EvalLength(text, &value);
    Str units = unitEdit->GetTextTemp();
    str::TrimWsBoth(units);
    double ratio = dist > 0 ? value / dist : 0;
    if (!parsed || !isfinite(value) || value <= 0 || !isfinite(ratio) || ratio <= 0 || ratio > FLT_MAX ||
        (float)ratio == 0 || len(units) == 0) {
        status->SetText(Tr("Enter a positive length and a unit name."));
        HwndInvalidate(hwnd);
        return;
    }
    scale = (float)ratio;
    str::ReplaceWithCopy(&unit, units);
    SaveScale();
    Update();
    HwndSetFocus(win->hwndFrame);
}

void MeasurementToolbar::Clear(VirtMouseEvent*) {
    VecReset(done);
    haveCalib = false;
    nPts = 0;
    Update();
}

void MeasurementToolbar::ResetScale(VirtMouseEvent*) {
    scale = DefaultScale();
    str::ReplaceWithCopy(&unit, StrL("pt"));
    unitEdit->SetText(unit);
    SaveScale();
    Update();
}

void MeasurementToolbar::OnClose(WindowBase::CloseEvent*) {
    CloseMeasurement(win);
}

void MeasurementToolbar::OnDestroy(WindowBase::DestroyEvent*) {
    CloseMeasurement(win);
}

bool MeasurementToolbar::Create(MainWindow* mainWin) {
    win = mainWin;
    SetFont(GetAppFont());
    CreateCustomArgs args;
    args.owner = win->hwndFrame;
    args.title = Tr("Measure");
    args.visible = false;
    args.style = WS_POPUPWINDOW | WS_CAPTION;
    args.font = GetFont();
    CreateCustom(args);
    if (!hwnd) {
        return false;
    }
    closeOnEsc = true;
    onClose = MkMethod1<MeasurementToolbar, WindowBase::CloseEvent*, &MeasurementToolbar::OnClose>(this);
    onDestroy = MkMethod1<MeasurementToolbar, WindowBase::DestroyEvent*, &MeasurementToolbar::OnDestroy>(this);
    labelFont = GetAppFont();

    auto* box = new VBox();
    box->alignCross = CrossAxisAlign::Stretch;
    box->gap = DpiScale(8);
    auto* table = new Table();
    table->SetSize(3, 2);
    table->colGap = DpiScale(8);
    table->rowGap = DpiScale(6);
    const Str labels[] = {Tr("Tool:"), Tr("Known length:"), Tr("Units:")};
    for (int i = 0; i < dimofi(labels); i++) {
        table->SetCell(i, 0, NewVirtText({.s = labels[i], .font = font, .isRtl = IsUIRtl()}));
    }
    DropDown::CreateArgs ddArgs;
    ddArgs.parent = hwnd;
    ddArgs.font = font;
    ddArgs.isRtl = IsUIRtl();
    mode = new DropDown();
    mode->Create(ddArgs);
    // in MeasureTool order
    StrVec items;
    items.Append(Tr("Length"));
    items.Append(Tr("Rectangular area"));
    items.Append(Tr("Radius"));
    items.Append(Tr("Guide line"));
    items.Append(Tr("Calibrate"));
    mode->SetItems(items);
    CbSetCurrentSelection(mode, (int)tool);
    mode->onSelectionChanged = MkMethod0<MeasurementToolbar, &MeasurementToolbar::SetMode>(this);
    table->SetCell(0, 1, mode).alignH = CrossAxisAlign::Stretch;

    Edit::CreateArgs editArgs;
    editArgs.parent = hwnd;
    editArgs.font = font;
    editArgs.withBorder = true;
    editArgs.isRtl = IsUIRtl();
    editArgs.idealWidthChars = 12;
    knownLength = new Edit();
    knownLength->Create(editArgs);
    knownLength->SetText(StrL("1"));
    unitEdit = new Edit();
    unitEdit->Create(editArgs);
    table->SetCell(1, 1, knownLength).alignH = CrossAxisAlign::Stretch;
    table->SetCell(2, 1, unitEdit).alignH = CrossAxisAlign::Stretch;
    box->AddChild(table);

    snap = NewDlgCheckbox(hwnd, Tr("&Snap to lines in the drawing"), font, true);
    box->AddChild(snap);

    auto* buttons = new HBox();
    buttons->gap = DpiScale(8);
    apply = NewThemedButton(hwnd, Tr("Apply calibration"), font, false);
    apply->onClick = MkMethod1<MeasurementToolbar, VirtMouseEvent*, &MeasurementToolbar::Calibrate>(this);
    clear = NewThemedButton(hwnd, Tr("Clear"), font, false);
    clear->onClick = MkMethod1<MeasurementToolbar, VirtMouseEvent*, &MeasurementToolbar::Clear>(this);
    resetScale = NewThemedButton(hwnd, Tr("Reset scale"), font, false);
    resetScale->onClick = MkMethod1<MeasurementToolbar, VirtMouseEvent*, &MeasurementToolbar::ResetScale>(this);
    buttons->AddChild(apply);
    buttons->AddChild(clear);
    buttons->AddChild(resetScale);
    box->AddChild(buttons);

    result = NewVirtText({.font = font, .isRtl = IsUIRtl()});
    status = NewVirtText({.font = font, .isRtl = IsUIRtl()});
    box->AddChild(result);
    box->AddChild(status);
    layout = new Padding(box, DpiScaledInsets(8));

    scale = DefaultScale();
    str::ReplaceWithCopy(&unit, StrL("pt"));
    FileState* fs = FileHistoryFindByPath(win->CurrentTab()->filePath);
    if (fs && isfinite(fs->measurementScale) && fs->measurementScale > 0 && len(fs->measurementUnit) > 0) {
        scale = fs->measurementScale;
        str::ReplaceWithCopy(&unit, fs->measurementUnit);
    }
    unitEdit->SetText(unit);
    SetMode();
    SizeToLayout(this, layout, DpiScale(440), 0, win->hwndFrame);
    UpdateTheme();
    SetIsVisible(true);
    return true;
}

void ToggleMeasurement(MainWindow* win) {
    if (win->measurementToolbar) {
        CloseMeasurement(win);
        return;
    }
    if (!win->AsFixed()) {
        return;
    }
    CancelAnnotationPlacement(win);
    CloseSignDocumentDialog(win);
    win->DeleteToolTip();
    auto* bar = new MeasurementToolbar();
    win->measurementToolbar = bar;
    if (!bar->Create(win)) {
        win->measurementToolbar = nullptr;
        delete bar;
    }
}

void CloseMeasurement(MainWindow* win) {
    if (!win || !win->measurementToolbar) {
        return;
    }
    auto* bar = win->measurementToolbar;
    win->measurementToolbar = nullptr;
    bar->win = nullptr;
    bar->SetIsVisible(false);
    bar->ScheduleDelete();
    win->RedrawCanvas();
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

// pixels at the screen point pt, in page units of pageNo
static double ScreenToPageDist(DisplayModel* dm, int pageNo, Point pt, int px) {
    PointF a = dm->CvtFromScreen(pt, pageNo);
    PointF b = dm->CvtFromScreen(Point{pt.x + px, pt.y}, pageNo);
    return Dist(a, b);
}

// pt (on the screen) as a page point, moved to the nearest line end in reach
static PointF SnappedPoint(MeasurementToolbar* bar, DisplayModel* dm, int pageNo, Point pt) {
    PointF p = dm->CvtFromScreen(pt, pageNo);
    if (!bar->snap->IsChecked()) {
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

static double DistToSegment(PointF p, PointF a, PointF b) {
    double dx = (double)b.x - a.x;
    double dy = (double)b.y - a.y;
    double l2 = (dx * dx) + (dy * dy);
    if (l2 <= 0) {
        return Dist(p, a);
    }
    double t = (((double)p.x - a.x) * dx + ((double)p.y - a.y) * dy) / l2;
    t = limitValue(t, 0.0, 1.0);
    return Dist(p, PointF((float)(a.x + t * dx), (float)(a.y + t * dy)));
}

// how far p is from what m draws
static double DistToMeasurement(PointF p, const Measurement& m) {
    const PointF* q = m.pts;
    switch (m.tool) {
        case MeasureTool::Area: {
            PointF c1(q[1].x, q[0].y);
            PointF c2(q[0].x, q[1].y);
            double d = DistToSegment(p, q[0], c1);
            d = std::min(d, DistToSegment(p, c1, q[1]));
            d = std::min(d, DistToSegment(p, q[1], c2));
            return std::min(d, DistToSegment(p, c2, q[0]));
        }
        case MeasureTool::Radius: {
            PointF c;
            double r = 0;
            if (!Circle(q[0], q[1], q[2], &c, &r)) {
                return DBL_MAX;
            }
            return fabs(Dist(p, c) - r);
        }
        case MeasureTool::Guide: {
            // the line through the two points, not just between them
            double dx = (double)q[1].x - q[0].x;
            double dy = (double)q[1].y - q[0].y;
            double l = sqrt((dx * dx) + (dy * dy));
            if (l <= 0) {
                return Dist(p, q[0]);
            }
            return fabs((dx * ((double)q[0].y - p.y)) - (dy * ((double)q[0].x - p.x))) / l;
        }
        default:
            return DistToSegment(p, q[0], q[1]);
    }
}

// Ctrl + click: deletes the measurement nearest to pt, if one is in reach
static bool EraseAt(MeasurementToolbar* bar, DisplayModel* dm, int pageNo, Point pt) {
    PointF p = dm->CvtFromScreen(pt, pageNo);
    double best = ScreenToPageDist(dm, pageNo, pt, DpiScale(kHitPx));
    int found = -1;
    for (int i = 0; i < len(bar->done); i++) {
        if (bar->done[i].pageNo != pageNo) {
            continue;
        }
        double d = DistToMeasurement(p, bar->done[i]);
        if (d <= best) {
            best = d;
            found = i;
        }
    }
    if (found < 0) {
        return false;
    }
    VecRemoveAt(bar->done, found);
    return true;
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
    HwndSetFocus(win->hwndFrame);
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
    SetCursor(LoadCursorW(nullptr, IDC_CROSS));
    if (bar->nPts > 0 && dm->GetPageNoByPoint(pt) == bar->cur.pageNo) {
        bar->hover = SnappedPoint(bar, dm, bar->cur.pageNo, pt);
        bar->haveHover = true;
        bar->Update();
    }
    return true;
}

//--- drawing

static void DrawLabel(MeasurementToolbar* bar, Gfx* gfx, Str text, Point at) {
    if (len(text) == 0) {
        return;
    }
    Size sz = gfx->MeasureText(text, bar->labelFont);
    int pad = DpiScale(3);
    Rect r{at.x - (sz.dx / 2) - pad, at.y - (sz.dy / 2) - pad, sz.dx + (2 * pad), sz.dy + (2 * pad)};
    gfx->FillRoundedRect(r, DpiScale(3), MkRgb(255, 255, 255), kColMeasure);
    gfx->DrawText(text, r, gfxTextCenter | gfxTextVCenter, bar->labelFont, kColMeasure);
}

static void DrawCross(DisplayModel* dm, Gfx* gfx, int pageNo, PointF p) {
    Point s = dm->CvtToScreen(pageNo, p);
    int c = DpiScale(5);
    gfx->DrawLineAA({s.x - c, s.y}, {s.x + c, s.y}, kColMeasure, 1.5f);
    gfx->DrawLineAA({s.x, s.y - c}, {s.x, s.y + c}, kColMeasure, 1.5f);
}

static void PaintOne(MeasurementToolbar* bar, DisplayModel* dm, Gfx* gfx, const Measurement& m, bool inProgress) {
    PointF pts[3];
    int n = MeasurementPoints(bar, m, inProgress, pts);
    int pageNo = m.pageNo;
    float w = (float)DpiScale(2);
    auto toScreen = [&](PointF p) { return dm->CvtToScreen(pageNo, p); };
    for (int i = 0; i < n; i++) {
        DrawCross(dm, gfx, pageNo, pts[i]);
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
            gfx->DrawLineAA(a, b, kColMeasure, w);
            DrawLabel(bar, gfx, text, Point{(a.x + b.x) / 2, (a.y + b.y) / 2 - DpiScale(14)});
            break;
        case MeasureTool::Area: {
            Point c1 = toScreen(PointF(pts[1].x, pts[0].y));
            Point c2 = toScreen(PointF(pts[0].x, pts[1].y));
            gfx->DrawLineAA(a, c1, kColMeasure, w);
            gfx->DrawLineAA(c1, b, kColMeasure, w);
            gfx->DrawLineAA(b, c2, kColMeasure, w);
            gfx->DrawLineAA(c2, a, kColMeasure, w);
            DrawLabel(bar, gfx, text, Point{(a.x + b.x) / 2, (a.y + b.y) / 2});
            break;
        }
        case MeasureTool::Radius: {
            PointF c;
            double r = 0;
            if (n < 3 || !Circle(pts[0], pts[1], pts[2], &c, &r)) {
                gfx->DrawLineAA(a, b, kColMeasure, 1.0f, 128);
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
                gfx->DrawLineAA(prev, p, kColMeasure, w);
                prev = p;
            }
            Point cs = toScreen(c);
            Point mid = toScreen(pts[2]);
            gfx->DrawLineAA(cs, mid, kColMeasure, 1.0f, 128);
            DrawCross(dm, gfx, pageNo, c);
            DrawLabel(bar, gfx, text, Point{(cs.x + mid.x) / 2, (cs.y + mid.y) / 2});
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
            gfx->DrawLineAA(g0, g1, kColGuide, 1.0f, 200);
            break;
        }
    }
}

void PaintMeasurement(MainWindow* win, Gfx* gfx) {
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
        gfx->PushClip(page);
        PaintOne(bar, dm, gfx, m, inProgress);
        gfx->PopClip();
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
            LPARAM lp = MAKELPARAM(p.x, p.y);
            SendMessageW(win->hwndCanvas, WM_LBUTTONDOWN, MK_LBUTTON, lp);
            SendMessageW(win->hwndCanvas, WM_LBUTTONUP, 0, lp);
        }
    } else if (str::Eq(action, StrL("apply"))) {
        bar->knownLength->SetText(length);
        bar->unitEdit->SetText(unit);
        bar->Calibrate();
    } else if (str::Eq(action, StrL("clear"))) {
        bar->Clear();
    } else if (str::Eq(action, StrL("reset"))) {
        bar->ResetScale();
    } else if (str::Eq(action, StrL("snap"))) {
        bar->snap->SetIsChecked(pageNo != 0);
    } else if (!str::Eq(action, StrL("state"))) {
        MeasureTool tool = MeasureTool::Length;
        const char* names[] = {"length", "area", "radius", "guide", "calibrate"};
        for (int i = 0; i < dimofi(names); i++) {
            if (str::Eq(action, Str(names[i]))) {
                tool = (MeasureTool)i;
            }
        }
        CbSetCurrentSelection(bar->mode, (int)tool);
        bar->SetMode();
    }

    const Measurement* m = nullptr;
    bool inProgress = bar->nPts > 0;
    if (inProgress) {
        m = &bar->cur;
    } else if (bar->tool == MeasureTool::Calibrate && bar->haveCalib) {
        m = &bar->calib;
    } else if (len(bar->done) > 0) {
        m = &bar->done[len(bar->done) - 1];
    }
    double lengthV = 0, area = 0, radius = 0;
    int page = 0;
    if (m) {
        PointF pts[3];
        int n = MeasurementPoints(bar, *m, inProgress, pts);
        page = m->pageNo;
        if (n >= 2) {
            lengthV = Dist(pts[0], pts[1]) * bar->scale;
            area = fabs(((double)pts[1].x - pts[0].x) * ((double)pts[1].y - pts[0].y)) * bar->scale * bar->scale;
        }
        PointF c;
        if (n >= 3 && Circle(pts[0], pts[1], pts[2], &c, &radius)) {
            radius *= bar->scale;
        }
    }
    return fmt("show=1 page=%d complete=%d count=%d scale=%.9g length=%.9g area=%.9g radius=%.9g unit=%s x=%.6g y=%.6g",
               page, inProgress ? 0 : 1, len(bar->done), bar->scale, lengthV, area, radius, bar->unit, bar->lastPt.x,
               bar->lastPt.y);
}
