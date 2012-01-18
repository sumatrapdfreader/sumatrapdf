/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WinUtil.h"

#include "Scopes.h"
#include "StrUtil.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

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
    RectF bbox;
    REAL maxDy = 0;
    REAL totalDx = 0;
    for (size_t i = 0; i < len; i++) {
        r[i].GetBounds(&bbox, g);
        if (bbox.Height > maxDy)
            maxDy = bbox.Height;
        totalDx += bbox.Width;
    }
    bbox.Width = totalDx;
    bbox.Height = maxDy;
    return bbox;
}

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
// TODO: this seems to sometimes report size that is slightly too small
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
    RectF bbox;
    r.GetBounds(&bbox, g);
    bbox.Width += 4.5f; // TODO: total magic, but seems to produce better results
    return bbox;
}

// this usually reports size that is too large
RectF MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    RectF bbox;
    PointF pz(0,0);
    g->MeasureString(s, len, f, pz, &bbox);
    return bbox;
}

RectF MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    //RectF bbox = MeasureTextStandard(g, f, s, len);
    RectF bbox = MeasureTextAccurate(g, f, s, len);
    //RectF bbox = MeasureTextAccurate2(g, f, s, len);
    return bbox;
}

// TODO: not quite sure why spaceDx1 != spaceDx2, using spaceDx2 because
// is smaller and looks as better spacing to me
REAL GetSpaceDx(Graphics *g, Font *f)
{
    RectF bbox;
#if 1
    bbox = MeasureText(g, f, L" ", 1);
    REAL spaceDx1 = bbox.Width;
    return spaceDx1;
#else
    bbox = MeasureText(g, f, L"wa", 2);
    REAL l1 = bbox.Width;
    bbox = MeasureText(g, f, L"w a", 3);
    REAL l2 = bbox.Width;
    REAL spaceDx2 = l2 - l1;
    return spaceDx2;
#endif
}

const TCHAR *GfxFileExtFromData(char *data, size_t len)
{
    char header[9] = { 0 };
    memcpy(header, data, min(len, sizeof(header)));

    if (str::StartsWith(header, "BM"))
        return _T(".bmp");
    if (str::StartsWith(header, "\x89PNG\x0D\x0A\x1A\x0A"))
        return _T(".png");
    if (str::StartsWith(header, "\xFF\xD8"))
        return _T(".jpg");
    if (str::StartsWith(header, "GIF87a") || str::StartsWith(header, "GIF89a"))
        return _T(".gif");
    if (memeq(header, "MM\x00\x2A", 4) || memeq(header, "II\x2A\x00", 4))
        return _T(".tif");
    return NULL;
}

// cf. http://stackoverflow.com/questions/4598872/creating-hbitmap-from-memory-buffer/4616394#4616394
Bitmap *BitmapFromData(void *data, size_t len)
{
    ScopedComPtr<IStream> stream(CreateStreamFromData(data, len));
    if (!stream)
        return NULL;

    Bitmap *bmp = Bitmap::FromStream(stream);
    if (bmp && bmp->GetLastStatus() != Ok) {
        delete bmp;
        bmp = NULL;
    }

    return bmp;
}


