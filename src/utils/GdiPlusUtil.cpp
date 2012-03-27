/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "Allocator.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "GeomUtil.h"
#include "Vec.h"

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

// note: gdi+ seems to under-report the width, the longer the text, the
// bigger the difference. I'm trying to correct for that with those magic values
#define PER_CHAR_DX_ADJUST .2f
#define PER_STR_DX_ADJUST  1.f

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
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
        bbox.Width += PER_STR_DX_ADJUST + (PER_CHAR_DX_ADJUST * (float)len);
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

// returns number of characters of string s that fits in a given width dx
// note: could be speed up a bit because in our use case we already know
// the width of the whole string so we could supply it to the function, but
// this shouldn't happen often, so that's fine. It's also possible that
// a smarter approach is possible, but this usually only does 3 MeasureText
// calls, so it's not that bad
int StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx)
{
    RectF r = MeasureText(g, f, s, len);
    if (r.Width <= dx)
        return len;
    // make the best guess of the length that fits
    size_t n = (int)((dx / r.Width) * (float)len);
    CrashIf((0 == n) || (n > len));
    r = MeasureText(g, f, s, n);
    // find the length len of s that fits within dx iff width of len+1 exceeds dx
    int dir = 1; // increasing length
    if (r.Width > dx)
        dir = -1; // decreasing length
    for (;;) {
        n += dir;
        r = MeasureText(g, f, s, n);
        if (1 == dir) {
            // if advancing length, we know that previous string did fit, so if
            // the new one doesn't fit, the previous length was the right one
            if (r.Width > dx)
                return n - 1;
        } else {
            // if decreasing length, we know that previous string didn't fit, so if
            // the one one fits, it's of the correct length
            if (r.Width < dx)
                return n;
        }
    }
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

#define COL_CLOSE_X RGB(0xa0, 0xa0, 0xa0)
#define COL_CLOSE_X_HOVER RGB(0xf9, 0xeb, 0xeb)  // white-ish
#define COL_CLOSE_HOVER_BG RGB(0xC1, 0x35, 0x35) // red-ish

// Draws the 'x' close button in regular state or onhover state
// Tries to mimic visual style of Chrome tab close button
void DrawCloseButton(DRAWITEMSTRUCT *dis)
{
    RectI r(RectI::FromRECT(dis->rcItem));
    ScopedMem<TCHAR> s(win::GetText(dis->hwndItem));
    bool onHover = str::Eq(s, BUTTON_HOVER_TEXT);

    Graphics g(dis->hDC);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    Color c;
    c.SetFromCOLORREF(GetSysColor(COLOR_BTNFACE)); // hoping this is always the right color
    SolidBrush bgBrush(c);
    g.FillRectangle(&bgBrush, r.x, r.y, r.dx, r.dy);

    // in onhover state, background is a red-ish circle
    if (onHover) {
        c.SetFromCOLORREF(COL_CLOSE_HOVER_BG);
        SolidBrush b(c);
        g.FillEllipse(&b, r.x, r.y, r.dx-2, r.dy-2);
    }

    // draw 'x'
    c.SetFromCOLORREF(onHover ? COL_CLOSE_X_HOVER : COL_CLOSE_X);
    Pen p(c, 2);
    if (onHover) {
        g.DrawLine(&p, Point(4,      4), Point(r.dx-6, r.dy-6));
        g.DrawLine(&p, Point(r.dx-6, 4), Point(4,      r.dy-6));
    } else {
        g.DrawLine(&p, Point(4,      5), Point(r.dx-6, r.dy-5));
        g.DrawLine(&p, Point(r.dx-6, 5), Point(4,      r.dy-5));
    }
}

void GetBaseTransform(Matrix& m, RectF pageRect, float zoom, int rotation)
{
    rotation = rotation % 360;
    if (rotation < 0) rotation = rotation + 360;
    if (90 == rotation)
        m.Translate(0, -pageRect.Height, MatrixOrderAppend);
    else if (180 == rotation)
        m.Translate(-pageRect.Width, -pageRect.Height, MatrixOrderAppend);
    else if (270 == rotation)
        m.Translate(-pageRect.Width, 0, MatrixOrderAppend);
    else if (0 == rotation)
        m.Translate(0, 0, MatrixOrderAppend);
    else
        CrashIf(true);

    m.Scale(zoom, zoom, MatrixOrderAppend);
    m.Rotate((REAL)rotation, MatrixOrderAppend);
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

// TGA images are fairly simple to create and compare and
// yet offer an average compression ratio of 5:1
// cf. http://en.wikipedia.org/wiki/Truevision_TGA

#pragma pack(push)
#pragma pack(1)
struct TgaHeader {
    uint8_t idLength;
    uint8_t cmapType;
    uint8_t imageType;
    uint16_t cmapFirstEntry;
    uint16_t cmapLength;
    uint8_t cmapBitDepth;
    uint16_t offsetX, offsetY;
    uint16_t width, height;
    uint8_t bitDepth;
    uint8_t flags;
};
#pragma pack(pop)

inline bool memeq3(char *pix1, char *pix2)
{
    return (*(DWORD *)pix1 & 0xFFFFFF) == (*(DWORD *)pix2 & 0xFFFFFF);
}

unsigned char *SerializeRunLengthEncoded(HBITMAP hbmp, size_t *bmpBytesOut)
{
    SizeI size = GetBitmapSize(hbmp);
    int stride = ((size.dx * 3 + 3) / 4) * 4;
    size_t bmpBytes;
    ScopedMem<char> bmpData((char *)SerializeBitmap(hbmp, &bmpBytes));
    if (!bmpData)
        return NULL;

    str::Str<char> tgaData;

    TgaHeader header = { 0 };
    header.imageType = 10; // true color, run-length encoded
    header.width = size.dx;
    header.height = size.dy;
    header.bitDepth = 24;
    tgaData.Append((char *)&header, sizeof(header));

    for (int k = 0; k < size.dy; k++) {
        char *line = bmpData + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO) + k * stride;
        for (int i = 0, j = 1; i < size.dx; i += j, j = 1) {
            // determine the length of a run of identical pixels
            while (i + j < size.dx && j < 128 && memeq3(line + i * 3, line + (i + j) * 3)) {
                j++;
            }
            if (j > 1) {
                tgaData.Append(j - 1 + 128);
                tgaData.Append(line + i * 3, 3);
            } else {
                // determine the length of a run of different pixels
                while (i + j < size.dx && j < 128 && !memeq3(line + (i + j - 1) * 3, line + (i + j) * 3)) {
                    j++;
                }
                tgaData.Append(j - 1);
                tgaData.Append(line + i * 3, j * 3);
            }
        }
    }
    tgaData.Append("\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0", 26);

    if (bmpBytesOut)
        *bmpBytesOut = tgaData.Size();
    return (unsigned char *)tgaData.StealData();
}
