/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PdfPreview.h"

IFACEMETHODIMP PreviewBase::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
    RectD page = m_engine->PageMediabox(1);
    float zoom = cx / (float)page.dx;
    RectI thumb = RectD(0, 0, cx, page.dy * zoom).Round();
    if ((UINT)thumb.dy > cx)
        thumb.dy = cx;

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biHeight = thumb.dx;
    bmi.bmiHeader.biWidth = thumb.dy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    unsigned char *bmpData = NULL;
    HBITMAP hthumb = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&bmpData, NULL, 0);
    if (!hthumb)
        return E_OUTOFMEMORY;

    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);
    HBITMAP hbmp = CreateCompatibleBitmap(hDC, thumb.dx, thumb.dy);
    DeleteObject(SelectObject(hDCMem, hbmp));
    bool ok = m_engine->RenderPage(hDCMem, thumb, 1, zoom, 0);

    if (ok && GetDIBits(hDC, hbmp, 0, thumb.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        // cf. http://msdn.microsoft.com/en-us/library/bb774612(v=VS.85).aspx
        for (int i = 0; i < thumb.dx * thumb.dy; i++)
            bmpData[4 * i + 3] = 255;

        *phbmp = hthumb;
        *pdwAlpha = WTSAT_RGB;
    }
    else {
        DeleteObject(hthumb);
        hthumb = NULL;
    }

    DeleteObject(hbmp);
    DeleteDC(hDCMem);
    ReleaseDC(NULL, hDC);

    return hthumb ? S_OK : E_FAIL;
}

IFACEMETHODIMP CPdfPreview::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] = {
        QITABENT(CPdfPreview, IInitializeWithStream),
        QITABENT(CPdfPreview, IThumbnailProvider),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}
