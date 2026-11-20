/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <float.h>
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MeasurementToolbarCommon.h"

int PointsNeeded(MeasureTool tool) {
    return tool == MeasureTool::Radius ? 3 : 2;
}

double Dist(PointF a, PointF b) {
    double dx = (double)b.x - a.x;
    double dy = (double)b.y - a.y;
    return sqrt((dx * dx) + (dy * dy));
}

// center and radius of the circle through a, b, c; false if they're on a line
bool Circle(PointF a, PointF b, PointF c, PointF* center, double* r) {
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
int MeasurementPoints(MeasurementState* bar, const Measurement& m, bool inProgress, PointF* pts) {
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

bool EvalLength(Str text, double* out) {
    ExprParser p{CStrTemp(text)};
    double v = p.Expr();
    p.SkipWs();
    if (!p.ok || *p.s != 0) {
        return false;
    }
    *out = v;
    return true;
}

TempStr MeasurementTextTemp(MeasurementState* bar, const Measurement& m, int n, const PointF* pts) {
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

//--- clicks on the page

// pixels at the screen point pt, in page units of pageNo
double ScreenToPageDist(DisplayModel* dm, int pageNo, Point pt, int px) {
    PointF a = dm->CvtFromScreen(pt, pageNo);
    PointF b = dm->CvtFromScreen(Point{pt.x + px, pt.y}, pageNo);
    return Dist(a, b);
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
bool EraseAt(MeasurementState* bar, DisplayModel* dm, int pageNo, Point pt) {
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

//--- for tests

// state: "show=1 page=1 complete=1 count=2 scale=.. length=.. area=.. radius=.. unit=m x=.. y=.."
// length / area / radius are of the measurement being made, else the last one
TempStr MeasurementTestStateTemp(MeasurementState* bar) {
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
