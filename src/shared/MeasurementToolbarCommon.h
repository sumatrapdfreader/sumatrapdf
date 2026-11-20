/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- the Measure tool's model, shared by each app's MeasurementToolbar.cpp ---

struct DisplayModel;

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

// what the Measure window knows apart from its controls
struct MeasurementState {
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
};

int PointsNeeded(MeasureTool tool);
double Dist(PointF a, PointF b);
bool Circle(PointF a, PointF b, PointF c, PointF* center, double* r);
int MeasurementPoints(MeasurementState* bar, const Measurement& m, bool inProgress, PointF* pts);
bool EvalLength(Str text, double* out);
TempStr MeasurementTextTemp(MeasurementState* bar, const Measurement& m, int n, const PointF* pts);
double ScreenToPageDist(DisplayModel* dm, int pageNo, Point pt, int px);
bool EraseAt(MeasurementState* bar, DisplayModel* dm, int pageNo, Point pt);
TempStr MeasurementTestStateTemp(MeasurementState* bar);
