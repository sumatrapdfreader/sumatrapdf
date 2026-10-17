/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "PageGridDialogCommon.h"

SeqStrings kPageGridUnitTok = "pt\0in\0mm\0cm\0";

SeqStrings kPageGridStyleTok = "dots\0dotted\0solid\0";

constexpr float kPageGridPtPerIn = 72.f;

constexpr float kPageGridMmPerIn = 25.4f;

float PageGridToPt(float v, int unit) {
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

float PageGridFromPt(float pt, int unit) {
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

TempStr PageGridNumTemp(float v) {
    return fmt("%.4g", v);
}

Str PageGridUnitName(int i) {
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

Str PageGridStyleName(int i) {
    if (i == 1) {
        return Tr("Dotted lines");
    }
    if (i == 2) {
        return Tr("Solid lines");
    }
    return Tr("Dots");
}

PageGrid* PageGridPrefs() {
    return gSettings ? &gSettings->fixedPageUI.pageGrid : nullptr;
}

void CopyPageGridSnap(PageGridSnap& dst, const PageGrid& src, bool showGrid) {
    dst.width = src.width > 0 ? src.width : kPageGridDefaultSizePt;
    dst.height = src.height > 0 ? src.height : kPageGridDefaultSizePt;
    dst.subdivisions = src.subdivisions > 0 ? src.subdivisions : kPageGridDefaultSubdivisions;
    dst.offsetX = src.offsetX;
    dst.offsetY = src.offsetY;
    dst.slant = src.slant;
    str::ReplaceWithCopy(&dst.color, src.color.s);
    str::ReplaceWithCopy(&dst.style, src.style);
    str::ReplaceWithCopy(&dst.units, src.units);
    dst.showGrid = showGrid;
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
    pg->slant = kPageGridDefaultSlant;
    SetColorText(pg->color, SerializeColorTemp(kPageGridDefaultColor));
    str::ReplaceWithCopy(&pg->style, SeqStrByIndex(kPageGridStyleTok, kPageGridDefaultStyleIdx));
    str::ReplaceWithCopy(&pg->units, SeqStrByIndex(kPageGridUnitTok, kPageGridDefaultUnitIdx));
}

TempStr PageGridStateTemp() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return str::DupTemp(StrL("ERROR no-settings"));
    }
    return fmt("show=%d width=%g height=%g subdiv=%d ox=%g oy=%g slant=%g color=%s style=%s units=%s checker=%d\n",
               ShowPageGrid() ? 1 : 0, pg->width, pg->height, pg->subdivisions, pg->offsetX, pg->offsetY, pg->slant,
               pg->color.s ? pg->color.s : StrL(""), pg->style.s ? pg->style : StrL(""),
               pg->units.s ? pg->units : StrL(""), ShowTransparencyGrid() ? 1 : 0);
}

void RedrawPageGridWindows() {
    for (MainWindow* w : gWindows) {
        if (w) {
            w->RedrawAll(true);
        }
    }
}

float PageGridAlignDown(float v, float origin, float step) {
    if (step <= 0) {
        return origin;
    }
    return origin + (floorf((v - origin) / step) * step);
}

bool PageGridIsMajor(float v, float origin, float minorPt, int subdiv) {
    if (minorPt <= 0.f || subdiv < 1) {
        return true;
    }
    int i = (int)floorf(((v - origin) / minorPt) + 0.5f);
    if (i < 0) {
        i = -i;
    }
    return (i % subdiv) == 0;
}

constexpr float kPageGridDegToRad = 0.0174532925f;
constexpr int kPageGridMinMinorPx = 6;
constexpr int kPageGridMinMajorPx = 4;
constexpr int kPageGridMaxDots = 8000;
constexpr int kPageGridMaxLines = 800;
constexpr float kPageGridDragSlopPx = 5.f;
constexpr float kPageGridDragMinPx = 16.f;

float PageGridClampSlant(float deg) {
    return limitValue(deg, kPageGridMinSlant, kPageGridMaxSlant);
}

bool PageGridIsUpright(float deg) {
    float d = deg - kPageGridDefaultSlant;
    if (d < 0) {
        d = -d;
    }
    return d < 0.5f;
}

static float PageGridCot(float deg) {
    if (PageGridIsUpright(deg)) {
        return 0;
    }
    float s = sinf(deg * kPageGridDegToRad);
    if (s > -0.02f && s < 0.02f) {
        return 0;
    }
    return -cosf(deg * kPageGridDegToRad) / s;
}

PageGridDraw GetPageGridDraw() {
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
    float slant = pg->slant;
    if (slant == 0) {
        slant = kPageGridDefaultSlant;
    }
    d.slantDeg = PageGridClampSlant(slant);
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

// xAt of slanted lines (x = xAt + (y - oy) * cot) that cross vis.
static void PageGridSlantSpan(const RectF& vis, float oy, float cot, float& lo, float& hi) {
    float y0 = vis.y;
    float y1 = vis.y + vis.dy;
    float x0 = vis.x;
    float x1 = vis.x + vis.dx;
    float s0 = (y0 - oy) * cot;
    float s1 = (y1 - oy) * cot;
    lo = std::min(x0 - s0, x0 - s1);
    hi = std::max(x1 - s0, x1 - s1);
}

static bool PageGridClipSlanted(const RectF& vis, float xAt, float oy, float cot, float& x0, float& y0, float& x1,
                                float& y1) {
    float top = vis.y;
    float bot = vis.y + vis.dy;
    float left = vis.x;
    float right = vis.x + vis.dx;
    if (bot < top) {
        float t = top;
        top = bot;
        bot = t;
    }
    if (right < left) {
        float t = left;
        left = right;
        right = t;
    }
    if (cot == 0) {
        if (xAt < left || xAt > right) {
            return false;
        }
        x0 = xAt;
        y0 = top;
        x1 = xAt;
        y1 = bot;
        return bot > top;
    }
    float ya = oy + (left - xAt) / cot;
    float yb = oy + (right - xAt) / cot;
    float in0 = std::min(ya, yb);
    float in1 = std::max(ya, yb);
    float cy0 = std::max(top, in0);
    float cy1 = std::min(bot, in1);
    if (cy1 < cy0) {
        return false;
    }
    x0 = xAt + (cy0 - oy) * cot;
    y0 = cy0;
    x1 = xAt + (cy1 - oy) * cot;
    y1 = cy1;
    return true;
}

static void PageGridEmitSlanted(const PageGridDraw& g, const RectF& box, const RectF& vis, bool minor, bool dots,
                                Vec<PageGridSeg>& out) {
    float minorX = g.widthPt / (float)g.subdiv;
    float minorY = g.heightPt / (float)g.subdiv;
    float stepX = minor ? minorX : g.widthPt;
    float stepY = minor ? minorY : g.heightPt;
    if (stepX <= 0.f || stepY <= 0.f) {
        return;
    }
    float ox = box.x + g.offsetXPt;
    float oy = box.y + g.offsetYPt;
    float cot = PageGridCot(g.slantDeg);
    float xLo = 0;
    float xHi = 0;
    PageGridSlantSpan(vis, oy, cot, xLo, xHi);
    float xStart = PageGridAlignDown(xLo, ox, stepX);
    float yStart = PageGridAlignDown(vis.y, oy, stepY);
    float xEnd = vis.x + vis.dx;
    float yEnd = vis.y + vis.dy;
    int nx = (int)((xHi - xStart) / stepX) + 2;
    int ny = (int)((yEnd - yStart) / stepY) + 2;
    if (nx > 4000 || ny > 4000) {
        return;
    }
    // Minor marks that would flood the page are dropped; the caller retries majors.
    if (minor) {
        bool tooMany = dots ? (nx > 0 && ny > 0 && nx * ny > kPageGridMaxDots) : (nx + ny > kPageGridMaxLines);
        if (tooMany) {
            return;
        }
    }

    auto pushLine = [&](float ax, float ay, float bx, float by, bool major) {
        PageGridSeg s;
        s.x0 = ax;
        s.y0 = ay;
        s.x1 = bx;
        s.y1 = by;
        s.major = major;
        VecAppend(out, s);
    };
    auto pushDot = [&](float x, float y, bool major) {
        PageGridSeg s;
        s.x0 = x;
        s.y0 = y;
        s.major = major;
        s.dot = true;
        VecAppend(out, s);
    };

    if (dots) {
        for (float y = yStart; y <= yEnd + 0.01f; y += stepY) {
            bool yMajor = !minor || PageGridIsMajor(y, oy, minorY, g.subdiv);
            for (float xAt = xStart; xAt <= xHi + 0.01f; xAt += stepX) {
                float x = xAt + (y - oy) * cot;
                if (x < vis.x - 0.01f || x > xEnd + 0.01f) {
                    continue;
                }
                bool major = !minor || (yMajor && PageGridIsMajor(xAt, ox, minorX, g.subdiv));
                pushDot(x, y, major);
            }
        }
        return;
    }

    for (float xAt = xStart; xAt <= xHi + 0.01f; xAt += stepX) {
        float ax, ay, bx, by;
        if (!PageGridClipSlanted(vis, xAt, oy, cot, ax, ay, bx, by)) {
            continue;
        }
        bool major = !minor || PageGridIsMajor(xAt, ox, minorX, g.subdiv);
        pushLine(ax, ay, bx, by, major);
    }
    for (float y = yStart; y <= yEnd + 0.01f; y += stepY) {
        bool major = !minor || PageGridIsMajor(y, oy, minorY, g.subdiv);
        pushLine(vis.x, y, xEnd, y, major);
    }
}

void PageGridSlantedSegs(const PageGridDraw& g, const RectF& box, const RectF& vis, bool wantMinor,
                         Vec<PageGridSeg>& out) {
    int before = len(out);
    PageGridEmitSlanted(g, box, vis, wantMinor, g.style == PageGridStyleKind::Dots, out);
    if (wantMinor && len(out) == before) {
        PageGridEmitSlanted(g, box, vis, false, g.style == PageGridStyleKind::Dots, out);
    }
}

static int PageGridScreenDist(DisplayModel* dm, int pageNo, PointF a, PointF b) {
    Point sa = dm->CvtToScreen(pageNo, a);
    Point sb = dm->CvtToScreen(pageNo, b);
    int dx = sa.x - sb.x;
    int dy = sa.y - sb.y;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    return dx > dy ? dx : dy;
}

bool PageGridSlantVisible(DisplayModel* dm, int pageNo, const PageGridDraw& g, const RectF& box, bool& drawMinor) {
    drawMinor = false;
    if (!dm || g.subdiv < 1) {
        return false;
    }
    float minorX = g.widthPt / (float)g.subdiv;
    float minorY = g.heightPt / (float)g.subdiv;
    if (minorX <= 0.f || minorY <= 0.f) {
        return false;
    }
    float ox = box.x + g.offsetXPt;
    float oy = box.y + g.offsetYPt;
    PointF o(ox, oy);
    int hx = PageGridScreenDist(dm, pageNo, o, PointF(ox + minorX, oy));
    int hy = PageGridScreenDist(dm, pageNo, o, PointF(ox, oy + minorY));
    float rad = g.slantDeg * kPageGridDegToRad;
    float s = sinf(rad);
    float pitch = minorX * fabsf(s);
    int hs = PageGridScreenDist(dm, pageNo, o, PointF(ox - s * pitch, oy - cosf(rad) * pitch));
    int cell = std::min(hx, std::min(hy, hs));
    if (cell * g.subdiv < kPageGridMinMajorPx) {
        return false;
    }
    drawMinor = cell >= kPageGridMinMinorPx;
    return true;
}

struct PageGridHit {
    PageGridAxis axis = PageGridAxis::None;
    int index = 0;
    bool minor = false;
    int pageNo = 0;
    float ox = 0;
    float oy = 0;
    float boxX = 0;
    float boxY = 0;
    float cot = 0;
    int subdiv = 4;
};

static float PageGridPxToPt(DisplayModel* dm, int pageNo, int x, int y, float px) {
    PointF a = dm->CvtFromScreen(Point(x, y), pageNo);
    PointF b = dm->CvtFromScreen(Point(x + (int)px, y), pageNo);
    PointF c = dm->CvtFromScreen(Point(x, y + (int)px), pageNo);
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float ex = c.x - a.x;
    float ey = c.y - a.y;
    float d1 = sqrtf(dx * dx + dy * dy);
    float d2 = sqrtf(ex * ex + ey * ey);
    return d1 > d2 ? d1 : d2;
}

static bool PageGridHitTest(DisplayModel* dm, int x, int y, PageGridHit& hit) {
    hit = PageGridHit{};
    if (!dm || !ShowPageGrid()) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || engine->isImageCollection) {
        return false;
    }
    int pageNo = dm->GetPageNoByPoint(Point(x, y));
    if (!dm->ValidPageNo(pageNo)) {
        return false;
    }
    RectF box = engine->PageMediabox(pageNo);
    if (box.IsEmpty()) {
        return false;
    }
    PointF p = dm->CvtFromScreen(Point(x, y), pageNo);
    if (p.x < box.x || p.y < box.y || p.x > box.x + box.dx || p.y > box.y + box.dy) {
        return false;
    }
    PageGridDraw g = GetPageGridDraw();
    float minorX = g.widthPt / (float)g.subdiv;
    float minorY = g.heightPt / (float)g.subdiv;
    if (minorX <= 0.f || minorY <= 0.f) {
        return false;
    }
    float ox = box.x + g.offsetXPt;
    float oy = box.y + g.offsetYPt;
    float cot = PageGridCot(g.slantDeg);
    float rad = g.slantDeg * kPageGridDegToRad;
    float sAbs = fabsf(sinf(rad));
    if (sAbs < 0.02f) {
        sAbs = 0.02f;
    }
    PointF origin(ox, oy);
    int minorAcross = PageGridScreenDist(dm, pageNo, origin, PointF(ox + minorX, oy));
    float pitch = minorX * sAbs;
    int minorSlant = PageGridScreenDist(dm, pageNo, origin, PointF(ox - sinf(rad) * pitch, oy - cosf(rad) * pitch));
    int minorW = std::min(minorAcross, minorSlant);
    int minorH = PageGridScreenDist(dm, pageNo, origin, PointF(ox, oy + minorY));

    bool minorWOk = minorW >= (int)kPageGridDragMinPx;
    bool majorWOk = minorW * g.subdiv >= (int)kPageGridDragMinPx;
    bool minorHOk = minorH >= (int)kPageGridDragMinPx;
    bool majorHOk = minorH * g.subdiv >= (int)kPageGridDragMinPx;
    if (!majorWOk && !majorHOk) {
        return false;
    }
    float stepX = minorWOk ? minorX : g.widthPt;
    float stepY = minorHOk ? minorY : g.heightPt;
    float slop = PageGridPxToPt(dm, pageNo, x, y, kPageGridDragSlopPx);

    float best = slop;
    if (majorWOk) {
        float u = (p.x - ox) - (p.y - oy) * cot;
        int index = (int)floorf(u / stepX + 0.5f);
        float dist = fabsf(u - (float)index * stepX) * sAbs;
        if (dist <= best) {
            best = dist;
            hit.axis = PageGridAxis::Width;
            hit.index = index;
            hit.minor = minorWOk;
        }
    }
    if (majorHOk) {
        float v = p.y - oy;
        int index = (int)floorf(v / stepY + 0.5f);
        float dist = fabsf(v - (float)index * stepY);
        if (dist <= best) {
            best = dist;
            hit.axis = PageGridAxis::Height;
            hit.index = index;
            hit.minor = minorHOk;
        }
    }
    if (hit.axis == PageGridAxis::None) {
        return false;
    }
    hit.pageNo = pageNo;
    hit.ox = ox;
    hit.oy = oy;
    hit.boxX = box.x;
    hit.boxY = box.y;
    hit.cot = cot;
    hit.subdiv = g.subdiv;
    return true;
}

struct PageGridDrag {
    bool active = false;
    PageGridHit hit;
    float width = 0;
    float height = 0;
    float offX = 0;
    float offY = 0;
};

static PageGridDrag gPageGridDrag;

PageGridAxis PageGridCursorAxis(DisplayModel* dm, int x, int y, bool allow) {
    if (gPageGridDrag.active) {
        return gPageGridDrag.hit.axis;
    }
    if (!allow) {
        return PageGridAxis::None;
    }
    PageGridHit hit;
    if (!PageGridHitTest(dm, x, y, hit)) {
        return PageGridAxis::None;
    }
    return hit.axis;
}

bool PageGridDragStart(DisplayModel* dm, int x, int y) {
    PageGridHit hit;
    if (!PageGridHitTest(dm, x, y, hit)) {
        return false;
    }
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return false;
    }
    gPageGridDrag.active = true;
    gPageGridDrag.hit = hit;
    gPageGridDrag.width = pg->width;
    gPageGridDrag.height = pg->height;
    gPageGridDrag.offX = pg->offsetX;
    gPageGridDrag.offY = pg->offsetY;
    return true;
}

static void PageGridApplyDrag(DisplayModel* dm, int x, int y) {
    PageGrid* pg = PageGridPrefs();
    if (!pg || !gPageGridDrag.active || !dm) {
        return;
    }
    const PageGridHit& h = gPageGridDrag.hit;
    PointF p = dm->CvtFromScreen(Point(x, y), h.pageNo);
    if (h.axis == PageGridAxis::Width) {
        if (h.index == 0) {
            pg->offsetX = limitValue(p.x - h.boxX - (p.y - h.oy) * h.cot, -720.f, 720.f);
        } else {
            float u = (p.x - h.ox) - (p.y - h.oy) * h.cot;
            float step = u / (float)h.index;
            // Past the origin the grabbed line cannot follow; keep the minimum.
            float major = h.minor ? step * (float)h.subdiv : step;
            pg->width = limitValue(major, 1.f, 720.f);
            pg->height = pg->width;
        }
        return;
    }
    if (h.index == 0) {
        pg->offsetY = limitValue(p.y - h.boxY, -720.f, 720.f);
        return;
    }
    float step = (p.y - h.oy) / (float)h.index;
    float major = h.minor ? step * (float)h.subdiv : step;
    pg->height = limitValue(major, 1.f, 720.f);
    pg->width = pg->height;
}

void PageGridDragMove(DisplayModel* dm, int x, int y) {
    if (!gPageGridDrag.active) {
        return;
    }
    PageGridApplyDrag(dm, x, y);
    PageGridDialogSyncFromPrefs();
    RedrawPageGridWindows();
}

bool PageGridDragActive() {
    return gPageGridDrag.active;
}

void PageGridDragEnd() {
    if (!gPageGridDrag.active) {
        return;
    }
    gPageGridDrag.active = false;
    PageGridDialogSyncFromPrefs();
    RedrawPageGridWindows();
    if (IsPageGridDialogOpen() || !HasPermission(Perm::SavePreferences)) {
        return;
    }
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    bool changed = pg->width != gPageGridDrag.width || pg->height != gPageGridDrag.height ||
                   pg->offsetX != gPageGridDrag.offX || pg->offsetY != gPageGridDrag.offY;
    if (changed) {
        ScheduleSaveSettings();
    }
}

void PageGridDragCancel() {
    if (!gPageGridDrag.active) {
        return;
    }
    gPageGridDrag.active = false;
    PageGrid* pg = PageGridPrefs();
    if (pg) {
        pg->width = gPageGridDrag.width;
        pg->height = gPageGridDrag.height;
        pg->offsetX = gPageGridDrag.offX;
        pg->offsetY = gPageGridDrag.offY;
    }
    PageGridDialogSyncFromPrefs();
    RedrawPageGridWindows();
}

TempStr PageGridProbeTemp(DisplayModel* dm) {
    if (!dm) {
        return str::DupTemp(StrL("ERROR no-window"));
    }
    EngineBase* engine = dm->GetEngine();
    int pageNo = dm->CurrentPageNo();
    if (!engine || !dm->ValidPageNo(pageNo)) {
        return str::DupTemp(StrL("ERROR no-page"));
    }
    RectF box = engine->PageMediabox(pageNo);
    PageGridDraw g = GetPageGridDraw();
    float ox = box.x + g.offsetXPt;
    float oy = box.y + g.offsetYPt;
    float cot = PageGridCot(g.slantDeg);
    float yA = box.y + box.dy * 0.3f;
    float yB = box.y + box.dy * 0.7f;
    float yM = (yA + yB) * 0.5f;
    Point sA = dm->CvtToScreen(pageNo, PointF(ox + (yA - oy) * cot, yA));
    Point sB = dm->CvtToScreen(pageNo, PointF(ox + (yB - oy) * cot, yB));
    Point sW = dm->CvtToScreen(pageNo, PointF(ox + g.widthPt + (yM - oy) * cot, yM));
    Point sH = dm->CvtToScreen(pageNo, PointF(ox + g.widthPt * 0.5f, oy + g.heightPt));
    return fmt("slant=%g x0=%d x1=%d sx=%d sy=%d hx=%d hy=%d", g.slantDeg, sA.x, sB.x, sW.x, sW.y, sH.x, sH.y);
}
