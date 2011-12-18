/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "GdiPlusUtil.h"

using namespace Gdiplus;

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
// TODO: this seems to sometimes reports size that is slightly too small
RectF MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    assert(len > 0);
    // note: frankly, I don't see a difference between those StringFormat variations
    StringFormat sf(StringFormat::GenericTypographic());
    //StringFormat sf(StringFormat::GenericDefault());
    //StringFormat sf;
    RectF layoutRect;
    CharacterRange cr(0, len);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region r;
    g->MeasureCharacterRanges(s, len, f, layoutRect, &sf, 1, &r);
    RectF bb;
    r.GetBounds(&bb, g);
    bb.Width += 4.5f; // TODO: total magic, but seems to produce better results
    return bb;
}

// this usually reports size that is too large
RectF MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    RectF bb;
    PointF pz(0,0);
    g->MeasureString(s, len, f, pz, &bb);
    return bb;
}

RectF MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    //RectF bb1 = MeasureTextStandard(g, f, s, len);
    RectF bb2 = MeasureTextAccurate(g, f, s, len);
    return bb2;
}

// TODO: not quite sure why spaceDx1 != spaceDx2, using spaceDx2 because
// is smaller and looks as better spacing to me
// note: we explicitly use MeasureTextStandard() because
// MeasureTextAccurate() ignores the trailing whitespace
REAL GetSpaceDx(Graphics *g, Font *f)
{
    RectF bb;
#if 1
    bb = MeasureTextStandard(g, f, L" ", 1);
    REAL spaceDx1 = bb.Width;
    return spaceDx1;
#else
    bb = MeasureTextStandard(g, f, L"wa", 2);
    REAL l1 = bb.Width;
    bb = MeasureTextStandard(g, f, L"w a", 3);
    REAL l2 = bb.Width;
    REAL spaceDx2 = l2 - l1;
    return spaceDx2;
#endif
}
