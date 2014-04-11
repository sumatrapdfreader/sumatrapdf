#include "stdafx.h"
#include "util.h"

using namespace Gdiplus;

namespace str {

size_t Utf8ToWcharBuf(const char *s, size_t sLen, WCHAR *bufOut, size_t cchBufOutSize)
{
    //CrashIf(0 == cchBufOutSize);
    int cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int) sLen, NULL, 0);
    if ((size_t) cchConverted >= cchBufOutSize)
        cchConverted = (int) cchBufOutSize - 1;
    MultiByteToWideChar(CP_UTF8, 0, s, (int) sLen, bufOut, cchConverted);
    bufOut[cchConverted] = '\0';
    return cchConverted;
}

size_t BufSet(WCHAR *dst, size_t dstCchSize, const WCHAR *src)
{
    size_t srcCchSize = str::Len(src);
    size_t toCopy = min(dstCchSize - 1, srcCchSize);
    wcsncpy_s(dst, dstCchSize, src, toCopy);
    return toCopy;
}

char *DupN(char *s, size_t sLen) {
    auto res = (char*) malloc(sLen + 1);
    if (!res)
        return NULL;
    memcpy(res, s, sLen);
    res[sLen] = 0;
    return res;
}

char *Dup(const char *s)
{
    return s ? _strdup(s) : NULL;
}

char *FmtV(const char *fmt, va_list args)
{
    char    message[256];
    size_t  bufCchSize = dimof(message);
    char  * buf = message;
    for (;;)
    {
        int count = _vsnprintf_s(buf, bufCchSize, _TRUNCATE, fmt, args);
        if ((count >= 0) && ((size_t) count < bufCchSize))
            break;
        /* we have to make the buffer bigger. The algorithm used to calculate
        the new size is arbitrary (aka. educated guess) */
        if (buf != message)
            free(buf);
        if (bufCchSize < 4 * 1024)
            bufCchSize += bufCchSize;
        else
            bufCchSize += 1024;
        buf = (char*)calloc(1, bufCchSize);
        if (!buf)
            break;
    }

    if (buf == message)
        buf = str::Dup(message);

    return buf;
}

char *Format(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *res = FmtV(fmt, args);
    va_end(args);
    return res;
}
}

namespace win {
int GetHwndDpi(HWND hwnd, float *uiDPIFactor)
{
    HDC dc = GetDC(hwnd);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    // round untypical resolutions up to the nearest quarter
    if (uiDPIFactor)
        *uiDPIFactor = ceil(dpi * 4.0f / USER_DEFAULT_SCREEN_DPI) / 4.0f;
    ReleaseDC(hwnd, dc);
    return dpi;
}
}

HFONT CreateSimpleFont(HDC hdc, const WCHAR *fontName, int fontSize)
{
    LOGFONT lf = { 0 };

    lf.lfWidth = 0;
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
    lf.lfItalic = FALSE;
    lf.lfUnderline = FALSE;
    lf.lfStrikeOut = FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH;
    str::BufSet(lf.lfFaceName, dimof(lf.lfFaceName), fontName);
    lf.lfWeight = FW_DONTCARE;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;

    return CreateFontIndirect(&lf);
}

void InitAllCommonControls()
{
    INITCOMMONCONTROLSEX cex = { 0 };
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&cex);
}

void InitGraphicsMode(Gdiplus::Graphics *g);

// set consistent mode for our graphics objects so that we get
// the same results when measuring text
void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g->SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);
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
    CharacterRange cr(0, (INT) len);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region r;
    Status status = g->MeasureCharacterRanges(s, (INT) len, f, layoutRect, &sf, 1, &r);
    if (status != Status::Ok)
        return RectF(0, 0, 0, 0); // TODO: should set height to font's height
    RectF bbox;
    r.GetBounds(&bbox, g);
    if (bbox.Width != 0)
        bbox.Width += PER_STR_DX_ADJUST + (PER_CHAR_DX_ADJUST * (float) len);
    return bbox;
}

