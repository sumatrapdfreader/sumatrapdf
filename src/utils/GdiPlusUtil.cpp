/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Allocator.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "WinUtil.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

// Get width of each character and add them up.
// Doesn't seem to be any different than MeasureTextAccurate() i.e. it still
// underreports the width
RectF MeasureTextAccurate2(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    CrashIf(0 == len);
    FixedArray<Region, 1024> regionBuf(len);
    Region *r = regionBuf.Get();
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
    Status status = g->MeasureCharacterRanges(s, len, f, layoutRect, &sf, len, r);
    CrashIf(status != Ok);
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

void InitFontMetricsCache(FontMetricsCache *metrics, Graphics *gfx, Font *font)
{
    WCHAR s[2] = { 0 };
    metrics->font = font;
    RectF bbox;
    for (size_t i = 0; i < 256-32; i++) {
        s[0] = i + 32;
        bbox = MeasureTextAccurate2(gfx, font, s, 1);
        metrics->dx[i] = bbox.Width;
        metrics->dy[i] = bbox.Height;
    }
}

static bool MeasureTextUsingMetricsCache(FontMetricsCache *fontMetrics, const WCHAR *s, size_t len, RectF& bboxOut)
{
    float totalDx = 0;
    float maxDy = 0;
    for (size_t i = 0; i < len; i++) {
        int n = s[i];
        if ((n < 32) || (n > 256))
            return false;
        totalDx += fontMetrics->dx[n];
        float dy = fontMetrics->dy[n];
        if (dy > maxDy)
            maxDy = dy;
    }
    bboxOut.X = 0; bboxOut.Y = 0;
    bboxOut.Width = totalDx; bboxOut.Height = maxDy;
    return true;
}

 // TODO: total magic, but seems to produce better results
#define MAGIC_DX_ADJUST 1.5f

// Measure text using optional FontMetricsCache. The caller must ensure that the
// Font matches fontMetrics as they are updated lazily.
RectF MeasureText(Graphics *g, Font *f, FontMetricsCache *fontMetrics, const WCHAR *s, size_t len)
{
    RectF bbox;
    CrashIf(f != fontMetrics->font);
    if (MeasureTextUsingMetricsCache(fontMetrics, s, len, bbox)) {
        if (bbox.Width != 0)
            bbox.Width += MAGIC_DX_ADJUST;
        return bbox;
    }
    return MeasureTextAccurate2(g, f, s, len);
}

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
// TODO: this seems to sometimes report size that is slightly too small
// Adding a magic MAGIC_DX_ADJUST to the width seems to make it more or less right
RectF MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    if (0 == len)
        return RectF(0, 0, 0, 0); // TODO: should set height to font's height
    // note: frankly, I don't see a difference between those StringFormat variations
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    //StringFormat sf(StringFormat::GenericDefault());
    //StringFormat sf;
    RectF layoutRect;
    CharacterRange cr(0, len);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region r;
    Status status = g->MeasureCharacterRanges(s, len, f, layoutRect, &sf, 1, &r);
    CrashIf(status != Ok);
    RectF bbox;
    r.GetBounds(&bbox, g);
    if (bbox.Width != 0)
        bbox.Width += MAGIC_DX_ADJUST;
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
    if (-1 == len)
        len = str::Len(s);
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
#if 0
    bbox = MeasureText(g, f, L" ", 1);
    REAL spaceDx1 = bbox.Width;
    return spaceDx1;
#else
    // this method seems to return (much) smaller size that measuring
    // the space itself
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

// adapted from http://cpansearch.perl.org/src/RJRAY/Image-Size-3.230/lib/Image/Size.pm
Rect BitmapSizeFromData(char *data, size_t len)
{
    Rect result;
    // too short to contain magic number and image dimensions
    if (len < 8) {
    }
    // Bitmap
    else if (str::StartsWith(data, "BM")) {
        if (len >= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
            BITMAPINFOHEADER *bmi = (BITMAPINFOHEADER *)(data + sizeof(BITMAPFILEHEADER));
            DWORD width = LEtoHl(bmi->biWidth);
            DWORD height = LEtoHl(bmi->biHeight);
            result = Rect(0, 0, width, height);
        }
    }
    // PNG
    else if (str::StartsWith(data, "\x89PNG\x0D\x0A\x1A\x0A")) {
        if (len >= 24 && str::StartsWith(data + 12, "IHDR")) {
            DWORD width = BEtoHl(*(DWORD *)(data + 16));
            DWORD height = BEtoHl(*(DWORD *)(data + 20));
            result = Rect(0, 0, width, height);
        }
    }
    // JPEG
    else if (str::StartsWith(data, "\xFF\xD8")) {
        // find the last start of frame marker for non-differential Huffman coding
        for (size_t ix = 2; ix + 9 < len && data[ix] == '\xFF'; ) {
            if ('\xC0' <= data[ix + 1] && data[ix + 1] <= '\xC3') {
                WORD width = BEtoHs(*(WORD *)(data + ix + 7));
                WORD height = BEtoHs(*(WORD *)(data + ix + 5));
                result = Rect(0, 0, width, height);
            }
            ix += BEtoHs(*(WORD *)(data + ix + 2)) + 2;
        }
    }
    // GIF
    else if (str::StartsWith(data, "GIF87a") || str::StartsWith(data, "GIF89a")) {
        if (len >= 13) {
            // find the first image's actual size instead of using the
            // "logical screen" size which is sometimes too large
            size_t ix = 13;
            // skip the global color table
            if ((data[10] & 0x80))
                ix += 3 * (1 << ((data[10] & 0x07) + 1));
            while (ix + 8 < len) {
                if (data[ix] == '\x2c') {
                    WORD width = LEtoHs(*(WORD *)(data + ix + 5));
                    WORD height = LEtoHs(*(WORD *)(data + ix + 7));
                    result = Rect(0, 0, width, height);
                    break;
                }
                else if (data[ix] == '\x21' && data[ix + 1] == '\xF9')
                    ix += 8;
                else if (data[ix] == '\x21' && data[ix + 1] == '\xFE') {
                    char *commentEnd = (char *)memchr(data + ix + 2, '\0', len - ix - 2);
                    ix = commentEnd ? commentEnd - data + 1 : len;
                }
                else if (data[ix] == '\x21' && data[ix + 1] == '\x01' && ix + 15 < len) {
                    char *textDataEnd = (char *)memchr(data + ix + 15, '\0', len - ix - 15);
                    ix = textDataEnd ? textDataEnd - data + 1 : len;
                }
                else if (data[ix] == '\x21' && data[ix + 1] == '\xFF' && ix + 14 < len) {
                    char *applicationDataEnd = (char *)memchr(data + ix + 14, '\0', len - ix - 14);
                    ix = applicationDataEnd ? applicationDataEnd - data + 1 : len;
                }
                else
                    break;
            }
        }
    }
    // TIFF
    else if (memeq(data, "MM\x00\x2A", 4) || memeq(data, "II\x2A\x00", 4)) {
        // TODO: speed this up (if necessary)
    }

    if (result.IsEmptyArea()) {
        // let GDI+ extract the image size if we've failed
        // (currently happens for animated GIFs and for all TIFFs)
        Bitmap *bmp = BitmapFromData(data, len);
        if (bmp)
            result = Rect(0, 0, bmp->GetWidth(), bmp->GetHeight());
        delete bmp;
    }

    return result;
}
