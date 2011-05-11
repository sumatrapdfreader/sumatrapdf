/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PdfPreview.h"
#include "WinUtil.h"

IFACEMETHODIMP PreviewBase::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
    BaseEngine *engine = GetEngine();
    if (!engine)
        return E_FAIL;

    RectD page = engine->PageMediabox(1);
    if (engine->PageRotation(1) % 180 != 0)
        swap(page.dx, page.dy);
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
    bool ok = engine->RenderPage(hDCMem, thumb, 1, zoom, 0);

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

#define COL_WINDOW_BG RGB(0xcc, 0xcc, 0xcc)
#define PREVIEW_MARGIN  2

static LRESULT OnPaint(HWND hwnd)
{
    ClientRect rect(hwnd);
    DoubleBuffer buffer(hwnd, rect);
    HDC hdc = buffer.GetDC();
    HBRUSH brushBg = CreateSolidBrush(COL_WINDOW_BG);
    FillRect(hdc, &rect.ToRECT(), brushBg);

    PreviewBase *preview = (PreviewBase *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview) {
        BaseEngine *engine = preview->GetEngine();
        if (engine) {
            int pageNo = GetScrollPos(hwnd, SB_VERT);
            rect.Inflate(-PREVIEW_MARGIN, -PREVIEW_MARGIN);
            RectD page = engine->PageMediabox(pageNo);
            if (engine->PageRotation(pageNo) % 180 != 0)
                swap(page.dx, page.dy);
            float zoom = (float)min(rect.dx / page.dx, rect.dy / page.dy);
            RectI onScreen = RectD(rect.x, rect.y, page.dx * zoom, page.dy * zoom).Round();
            onScreen.Offset((rect.dx - onScreen.dx) / 2, (rect.dy - onScreen.dy) / 2);

            // TODO: rendering can be quite slow - move to a separate thread?
            engine->RenderPage(hdc, onScreen, pageNo, zoom, 0);
        }
    }

    DeleteObject(brushBg);

    PAINTSTRUCT ps;
    buffer.Flush(BeginPaint(hwnd, &ps));
    EndPaint(hwnd, &ps);
    return 0;
}

static LRESULT OnVScroll(HWND hwnd, WPARAM wParam)
{
    SCROLLINFO si = { 0 };
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    switch (LOWORD(wParam)) {
    case SB_TOP:        si.nPos = si.nMin; break;
    case SB_BOTTOM:     si.nPos = si.nMax; break;
    case SB_LINEUP:     si.nPos--; break;
    case SB_LINEDOWN:   si.nPos++; break;
    case SB_PAGEUP:     si.nPos--; break;
    case SB_PAGEDOWN:   si.nPos++; break;
    case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
    }
    si.fMask = SIF_POS;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
    return 0;
}

static LRESULT OnKeydown(HWND hwnd, int key)
{
    switch (key) {
    case VK_DOWN: case VK_RIGHT: case VK_NEXT:
        return OnVScroll(hwnd, SB_PAGEDOWN);
    case VK_UP: case VK_LEFT: case VK_PRIOR:
        return OnVScroll(hwnd, SB_PAGEUP);
    case VK_HOME:
        return OnVScroll(hwnd, SB_TOP);
    case VK_END:
        return OnVScroll(hwnd, SB_BOTTOM);
    default:
        return 0;
    }
}

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT:
        return OnPaint(hwnd);
    case WM_VSCROLL:
        return OnVScroll(hwnd, wParam);
    case WM_KEYDOWN:
        return OnKeydown(hwnd, wParam);
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

IFACEMETHODIMP PreviewBase::DoPreview()
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = PreviewWndProc;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = L"SumatraPDF_PreviewPane";
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wcex);

    m_hwnd = CreateWindowW(wcex.lpszClassName, NULL, WS_CHILD | WS_VSCROLL | WS_VISIBLE,
                           m_rcParent.x, m_rcParent.x, m_rcParent.dx, m_rcParent.dy,
                           m_hwndParent, NULL, NULL, NULL);
    if (!m_hwnd)
        return HRESULT_FROM_WIN32(GetLastError());

    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
    BaseEngine *engine = GetEngine();

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nPos = 1;
    si.nMin = 1;
    si.nMax = engine ? engine->PageCount() : 1;
    si.nPage = si.nMax > 1 ? 1 : 2;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

    ShowWindow(m_hwnd, SW_SHOW);
    return S_OK;
}

IFACEMETHODIMP PreviewBase::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] = {
        QITABENT(PreviewBase, IInitializeWithStream),
        QITABENT(PreviewBase, IThumbnailProvider),
        QITABENT(PreviewBase, IObjectWithSite),
        QITABENT(PreviewBase, IPreviewHandler),
        QITABENT(PreviewBase, IOleWindow),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}
