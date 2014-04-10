#include "stdafx.h"
#include "util.h"

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
    CrashAlwaysIf(0 == dstCchSize);

    size_t srcCchSize = str::Len(src);
    size_t toCopy = min(dstCchSize - 1, srcCchSize);

    errno_t err = wcsncpy_s(dst, dstCchSize, src, toCopy);
    CrashIf(err || dst[toCopy] != '\0');

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
