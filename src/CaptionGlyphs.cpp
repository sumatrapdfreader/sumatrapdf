/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "CaptionGlyphs.h"

#include "base/GdiPlus.h"

// Segoe Fluent Icons (U+E921, U+E922, U+E923, U+E8BB) outline data extracted once
// from SegoeIcons.ttf at 2048 em units. Rendered with GDI+ so caption buttons
// match Win11 rounded style without relying on the system font.

constexpr float kCaptionGlyphEm = 2048.0f;

struct PathBuilder {
    Gdiplus::GraphicsPath* path = nullptr;
    float cx = 0;
    float cy = 0;

    void MoveTo(float x, float y) {
        ReportIf(!path);
        path->StartFigure();
        cx = x;
        cy = y;
    }

    void LineTo(float x, float y) {
        ReportIf(!path);
        path->AddLine(cx, cy, x, y);
        cx = x;
        cy = y;
    }

    void CurveTo(float x1, float y1, float x2, float y2, float x3, float y3) {
        ReportIf(!path);
        path->AddBezier(cx, cy, x1, y1, x2, y2, x3, y3);
        cx = x3;
        cy = y3;
    }

    void ClosePath() {
        ReportIf(!path);
        path->CloseFigure();
    }
};

static void BuildMinimizePath(Gdiplus::GraphicsPath* path) {
    PathBuilder b{path};
    b.MoveTo(102, 1024);
    b.CurveTo(88, 1024, 74.8333f, 1026.67f, 62.5f, 1032);
    b.CurveTo(50.1667f, 1037.33f, 39.3333f, 1044.67f, 30, 1054);
    b.CurveTo(20.6667f, 1063.33f, 13.3333f, 1074.17f, 8, 1086.5f);
    b.CurveTo(2.66667f, 1098.83f, 0, 1112, 0, 1126);
    b.CurveTo(0, 1140, 2.66667f, 1153.17f, 8, 1165.5f);
    b.CurveTo(13.3333f, 1177.83f, 20.6667f, 1188.83f, 30, 1198.5f);
    b.CurveTo(39.3333f, 1208.17f, 50.1667f, 1215.67f, 62.5f, 1221);
    b.CurveTo(74.8333f, 1226.33f, 88, 1229, 102, 1229);
    b.LineTo(1946, 1229);
    b.CurveTo(1960, 1229, 1973.17f, 1226.33f, 1985.5f, 1221);
    b.CurveTo(1997.83f, 1215.67f, 2008.67f, 1208.17f, 2018, 1198.5f);
    b.CurveTo(2027.33f, 1188.83f, 2034.67f, 1177.83f, 2040, 1165.5f);
    b.CurveTo(2045.33f, 1153.17f, 2048, 1140, 2048, 1126);
    b.CurveTo(2048, 1112, 2045.33f, 1098.83f, 2040, 1086.5f);
    b.CurveTo(2034.67f, 1074.17f, 2027.33f, 1063.33f, 2018, 1054);
    b.CurveTo(2008.67f, 1044.67f, 1997.83f, 1037.33f, 1985.5f, 1032);
    b.CurveTo(1973.17f, 1026.67f, 1960, 1024, 1946, 1024);
    b.ClosePath();
}

static void BuildMaximizePath(Gdiplus::GraphicsPath* path) {
    PathBuilder b{path};
    b.MoveTo(302, 0);
    b.CurveTo(262, 0, 223.667f, 8.16667f, 187, 24.5f);
    b.CurveTo(150.333f, 40.8333f, 118.167f, 62.8333f, 90.5f, 90.5f);
    b.CurveTo(62.8333f, 118.167f, 40.8333f, 150.333f, 24.5f, 187);
    b.CurveTo(8.16667f, 223.667f, 0, 262, 0, 302);
    b.LineTo(0, 1746);
    b.CurveTo(0, 1786, 8.16667f, 1824.33f, 24.5f, 1861);
    b.CurveTo(40.8333f, 1897.67f, 62.8333f, 1929.83f, 90.5f, 1957.5f);
    b.CurveTo(118.167f, 1985.17f, 150.333f, 2007.17f, 187, 2023.5f);
    b.CurveTo(223.667f, 2039.83f, 262, 2048, 302, 2048);
    b.LineTo(1746, 2048);
    b.CurveTo(1786, 2048, 1824.33f, 2039.83f, 1861, 2023.5f);
    b.CurveTo(1897.67f, 2007.17f, 1929.83f, 1985.17f, 1957.5f, 1957.5f);
    b.CurveTo(1985.17f, 1929.83f, 2007.17f, 1897.67f, 2023.5f, 1861);
    b.CurveTo(2039.83f, 1824.33f, 2048, 1786, 2048, 1746);
    b.LineTo(2048, 302);
    b.CurveTo(2048, 262, 2039.83f, 223.667f, 2023.5f, 187);
    b.CurveTo(2007.17f, 150.333f, 1985.17f, 118.167f, 1957.5f, 90.5f);
    b.CurveTo(1929.83f, 62.8333f, 1897.67f, 40.8333f, 1861, 24.5f);
    b.CurveTo(1824.33f, 8.16667f, 1786, 0, 1746, 0);
    b.ClosePath();
    b.MoveTo(1741, 205);
    b.CurveTo(1755, 205, 1768.17f, 207.667f, 1780.5f, 213);
    b.CurveTo(1792.83f, 218.333f, 1803.67f, 225.667f, 1813, 235);
    b.CurveTo(1822.33f, 244.333f, 1829.67f, 255.167f, 1835, 267.5f);
    b.CurveTo(1840.33f, 279.833f, 1843, 293, 1843, 307);
    b.LineTo(1843, 1741);
    b.CurveTo(1843, 1755, 1840.33f, 1768.17f, 1835, 1780.5f);
    b.CurveTo(1829.67f, 1792.83f, 1822.33f, 1803.67f, 1813, 1813);
    b.CurveTo(1803.67f, 1822.33f, 1792.83f, 1829.67f, 1780.5f, 1835);
    b.CurveTo(1768.17f, 1840.33f, 1755, 1843, 1741, 1843);
    b.LineTo(307, 1843);
    b.CurveTo(293, 1843, 279.833f, 1840.33f, 267.5f, 1835);
    b.CurveTo(255.167f, 1829.67f, 244.333f, 1822.33f, 235, 1813);
    b.CurveTo(225.667f, 1803.67f, 218.333f, 1792.83f, 213, 1780.5f);
    b.CurveTo(207.667f, 1768.17f, 205, 1755, 205, 1741);
    b.LineTo(205, 307);
    b.CurveTo(205, 293, 207.667f, 279.833f, 213, 267.5f);
    b.CurveTo(218.333f, 255.167f, 225.667f, 244.333f, 235, 235);
    b.CurveTo(244.333f, 225.667f, 255.167f, 218.333f, 267.5f, 213);
    b.CurveTo(279.833f, 207.667f, 293, 205, 307, 205);
    b.ClosePath();
}

static void BuildRestorePath(Gdiplus::GraphicsPath* path) {
    PathBuilder b{path};
    b.MoveTo(1843, 1441);
    b.CurveTo(1843, 1496.33f, 1832, 1548.5f, 1810, 1597.5f);
    b.CurveTo(1788, 1646.5f, 1758.17f, 1689.17f, 1720.5f, 1725.5f);
    b.CurveTo(1682.83f, 1761.83f, 1639.17f, 1790.5f, 1589.5f, 1811.5f);
    b.CurveTo(1539.83f, 1832.5f, 1487.67f, 1843, 1433, 1843);
    b.LineTo(427, 1843);
    b.CurveTo(437.667f, 1873.67f, 452.667f, 1901.67f, 472, 1927);
    b.CurveTo(491.333f, 1952.33f, 513.667f, 1974, 539, 1992);
    b.CurveTo(564.333f, 2010, 592.167f, 2023.83f, 622.5f, 2033.5f);
    b.CurveTo(652.833f, 2043.17f, 684.333f, 2048, 717, 2048);
    b.LineTo(1433, 2048);
    b.CurveTo(1517.67f, 2048, 1597.33f, 2031.83f, 1672, 1999.5f);
    b.CurveTo(1746.67f, 1967.17f, 1811.83f, 1923.33f, 1867.5f, 1868);
    b.CurveTo(1923.17f, 1812.67f, 1967.17f, 1747.67f, 1999.5f, 1673);
    b.CurveTo(2031.83f, 1598.33f, 2048, 1518.67f, 2048, 1434);
    b.LineTo(2048, 717);
    b.CurveTo(2048, 684.333f, 2043.17f, 652.833f, 2033.5f, 622.5f);
    b.CurveTo(2023.83f, 592.167f, 2010, 564.333f, 1992, 539);
    b.CurveTo(1974, 513.667f, 1952.33f, 491.333f, 1927, 472);
    b.CurveTo(1901.67f, 452.667f, 1873.67f, 437.667f, 1843, 427);
    b.ClosePath();
    b.MoveTo(302, 0);
    b.CurveTo(262, 0, 223.667f, 8.16667f, 187, 24.5f);
    b.CurveTo(150.333f, 40.8333f, 118.167f, 62.8333f, 90.5f, 90.5f);
    b.CurveTo(62.8333f, 118.167f, 40.8333f, 150.333f, 24.5f, 187);
    b.CurveTo(8.16667f, 223.667f, 0, 262, 0, 302);
    b.LineTo(0, 1336);
    b.CurveTo(0, 1376.67f, 8.16667f, 1415.17f, 24.5f, 1451.5f);
    b.CurveTo(40.8333f, 1487.83f, 62.8333f, 1519.83f, 90.5f, 1547.5f);
    b.CurveTo(118.167f, 1575.17f, 150.167f, 1597.17f, 186.5f, 1613.5f);
    b.CurveTo(222.833f, 1629.83f, 261.333f, 1638, 302, 1638);
    b.LineTo(1336, 1638);
    b.CurveTo(1376.67f, 1638, 1415.33f, 1629.83f, 1452, 1613.5f);
    b.CurveTo(1488.67f, 1597.17f, 1520.67f, 1575.33f, 1548, 1548);
    b.CurveTo(1575.33f, 1520.67f, 1597.17f, 1488.67f, 1613.5f, 1452);
    b.CurveTo(1629.83f, 1415.33f, 1638, 1376.67f, 1638, 1336);
    b.LineTo(1638, 302);
    b.CurveTo(1638, 261.333f, 1629.83f, 222.833f, 1613.5f, 186.5f);
    b.CurveTo(1597.17f, 150.167f, 1575.17f, 118.167f, 1547.5f, 90.5f);
    b.CurveTo(1519.83f, 62.8333f, 1487.83f, 40.8333f, 1451.5f, 24.5f);
    b.CurveTo(1415.17f, 8.16667f, 1376.67f, 0, 1336, 0);
    b.ClosePath();
    b.MoveTo(1331, 205);
    b.CurveTo(1345, 205, 1358.17f, 207.667f, 1370.5f, 213);
    b.CurveTo(1382.83f, 218.333f, 1393.83f, 225.667f, 1403.5f, 235);
    b.CurveTo(1413.17f, 244.333f, 1420.67f, 255.167f, 1426, 267.5f);
    b.CurveTo(1431.33f, 279.833f, 1434, 293, 1434, 307);
    b.LineTo(1434, 1331);
    b.CurveTo(1434, 1345, 1431.33f, 1358.33f, 1426, 1371);
    b.CurveTo(1420.67f, 1383.67f, 1413.33f, 1394.67f, 1404, 1404);
    b.CurveTo(1394.67f, 1413.33f, 1383.67f, 1420.67f, 1371, 1426);
    b.CurveTo(1358.33f, 1431.33f, 1345, 1434, 1331, 1434);
    b.LineTo(307, 1434);
    b.CurveTo(293, 1434, 279.833f, 1431.33f, 267.5f, 1426);
    b.CurveTo(255.167f, 1420.67f, 244.333f, 1413.17f, 235, 1403.5f);
    b.CurveTo(225.667f, 1393.83f, 218.333f, 1382.83f, 213, 1370.5f);
    b.CurveTo(207.667f, 1358.17f, 205, 1345, 205, 1331);
    b.LineTo(205, 307);
    b.CurveTo(205, 293, 207.667f, 279.833f, 213, 267.5f);
    b.CurveTo(218.333f, 255.167f, 225.667f, 244.333f, 235, 235);
    b.CurveTo(244.333f, 225.667f, 255.167f, 218.333f, 267.5f, 213);
    b.CurveTo(279.833f, 207.667f, 293, 205, 307, 205);
    b.ClosePath();
}

static void BuildClosePath(Gdiplus::GraphicsPath* path) {
    PathBuilder b{path};
    b.MoveTo(1024, 879);
    b.LineTo(175, 30);
    b.CurveTo(155, 10, 131, 0, 103, 0);
    b.CurveTo(73.6667f, 0, 49.1667f, 9.83333f, 29.5f, 29.5f);
    b.CurveTo(9.83333f, 49.1667f, 0, 73.6667f, 0, 103);
    b.CurveTo(0, 131, 10, 155, 30, 175);
    b.LineTo(879, 1024);
    b.LineTo(30, 1873);
    b.CurveTo(10, 1893, 0, 1917.33f, 0, 1946);
    b.CurveTo(0, 1960, 2.66667f, 1973.33f, 8, 1986);
    b.CurveTo(13.3333f, 1998.67f, 20.6667f, 2009.5f, 30, 2018.5f);
    b.CurveTo(39.3333f, 2027.5f, 50.3333f, 2034.67f, 63, 2040);
    b.CurveTo(75.6667f, 2045.33f, 89, 2048, 103, 2048);
    b.CurveTo(131, 2048, 155, 2038, 175, 2018);
    b.LineTo(1024, 1169);
    b.LineTo(1873, 2018);
    b.CurveTo(1893, 2038, 1917.33f, 2048, 1946, 2048);
    b.CurveTo(1960, 2048, 1973.17f, 2045.33f, 1985.5f, 2040);
    b.CurveTo(1997.83f, 2034.67f, 2008.67f, 2027.33f, 2018, 2018);
    b.CurveTo(2027.33f, 2008.67f, 2034.67f, 1997.83f, 2040, 1985.5f);
    b.CurveTo(2045.33f, 1973.17f, 2048, 1960, 2048, 1946);
    b.CurveTo(2048, 1917.33f, 2038, 1893, 2018, 1873);
    b.LineTo(1169, 1024);
    b.LineTo(2018, 175);
    b.CurveTo(2038, 155, 2048, 131, 2048, 103);
    b.CurveTo(2048, 89, 2045.33f, 75.6667f, 2040, 63);
    b.CurveTo(2034.67f, 50.3333f, 2027.5f, 39.3333f, 2018.5f, 30);
    b.CurveTo(2009.5f, 20.6667f, 1998.67f, 13.3333f, 1986, 8);
    b.CurveTo(1973.33f, 2.66667f, 1960, 0, 1946, 0);
    b.CurveTo(1917.33f, 0, 1893, 10, 1873, 30);
    b.ClosePath();
}

static void BuildCaptionSysButtonPath(CaptionSysButtonKind kind, Gdiplus::GraphicsPath* path) {
    switch (kind) {
        case CaptionSysButtonKind::Minimize:
            BuildMinimizePath(path);
            break;
        case CaptionSysButtonKind::Maximize:
            BuildMaximizePath(path);
            break;
        case CaptionSysButtonKind::Restore:
            BuildRestorePath(path);
            break;
        case CaptionSysButtonKind::Close:
            BuildClosePath(path);
            break;
    }
}

void DrawCaptionSysButtonGlyph(HDC hdc, CaptionSysButtonKind kind, Rect rc, COLORREF iconCol, int iconPx) {
    if (iconPx < 1) {
        return;
    }
    Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
    BuildCaptionSysButtonPath(kind, &path);

    float scale = (float)iconPx / kCaptionGlyphEm;
    float ox = (float)rc.x + ((float)rc.dx - (float)iconPx) / 2.0f;
    float oy = (float)rc.y + ((float)rc.dy + (float)iconPx) / 2.0f;

    Gdiplus::Matrix m;
    m.Translate(ox, oy);
    m.Scale(scale, -scale);
    path.Transform(&m);

    Gdiplus::Graphics gfx(hdc);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush br(Gdiplus::Color(GetRValue(iconCol), GetGValue(iconCol), GetBValue(iconCol)));
    gfx.FillPath(&br, &path);
}
