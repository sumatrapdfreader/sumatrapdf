/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "GdiPlusUtil.h"

using namespace Gdiplus;

// A helper for allocating an array of elements of type T
// either on stack (if they fit within StackBufInBytes)
// or in memory. Allocating on stack is a perf optimization
template <typename T, int StackBufInBytes>
class FixedArray {
    T stackBuf[StackBufInBytes / sizeof(T)];
    T *memBuf;
public:
    FixedArray(size_t elCount) {
        memBuf = NULL;
        size_t stackEls = StackBufInBytes / sizeof(T);
        if (elCount > stackEls)
            memBuf = (T*)malloc(elCount * sizeof(T));
    }
    ~FixedArray() {
        free(memBuf);
    }
    T *Get() {
        if (memBuf)
            return memBuf;
        return &(stackBuf[0]);
    }
};

// Get width of each character and add them up.
// Doesn't seem to be any different than MeasureTextAccurate() i.e. it still
// underreports the width
RectF MeasureTextAccurate2(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    assert(len > 0);
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    RectF layoutRect;
    FixedArray<CharacterRange, 1024> charRangesBuf(len);
    CharacterRange *charRanges = charRangesBuf.Get();
    for (size_t i = 0; i < len; i++) {
        charRanges[i].First = i;
        charRanges[i].Length = 1;
    }
    sf.SetMeasurableCharacterRanges(len, charRanges);
    FixedArray<Region, 1024> regionBuf(len);
    Region *r = regionBuf.Get();
    g->MeasureCharacterRanges(s, len, f, layoutRect, &sf, len, r);
    RectF bb;
    REAL maxDy = 0;
    REAL totalDx = 0;
    for (size_t i = 0; i < len; i++) {
        r[i].GetBounds(&bb, g);
        if (bb.Height > maxDy)
            maxDy = bb.Height;
        totalDx += bb.Width;
    }
    bb.Width = totalDx;
    bb.Height = maxDy;
    return bb;
}

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
// TODO: this seems to sometimes reports size that is slightly too small
// Adding a magic 4.5f to the width seems to make it more or less right
RectF MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    assert(len > 0);
    // note: frankly, I don't see a difference between those StringFormat variations
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
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
    //RectF bb = MeasureTextStandard(g, f, s, len);
    RectF bb = MeasureTextAccurate(g, f, s, len);
    //RectF bb = MeasureTextAccurate2(g, f, s, len);
    return bb;
}

// TODO: not quite sure why spaceDx1 != spaceDx2, using spaceDx2 because
// is smaller and looks as better spacing to me
// note: we explicitly use MeasureTextStandard() because
// MeasureTextAccurate() ignores the trailing whitespace
REAL GetSpaceDx(Graphics *g, Font *f)
{
    RectF bb;
#if 1
    bb = MeasureText(g, f, L" ", 1);
    REAL spaceDx1 = bb.Width;
    return spaceDx1;
#else
    bb = MeasureText(g, f, L"wa", 2);
    REAL l1 = bb.Width;
    bb = MeasureText(g, f, L"w a", 3);
    REAL l2 = bb.Width;
    REAL spaceDx2 = l2 - l1;
    return spaceDx2;
#endif
}
