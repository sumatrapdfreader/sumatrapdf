/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "PdfPreview.h"
#include "WinUtil.h"
#include "Scopes.h"

IFACEMETHODIMP PreviewBase::GetThumbnail(UINT cx, HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
    BaseEngine *engine = GetEngine();
    if (!engine)
        return E_FAIL;

    RectD page = engine->Transform(engine->PageMediabox(1), 1, 1.0, 0);
    float zoom = min(cx / (float)page.dx, cx / (float)page.dy);
    RectI thumb = RectD(0, 0, page.dx * zoom, page.dy * zoom).Round();

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biHeight = thumb.dy;
    bmi.bmiHeader.biWidth = thumb.dx;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    unsigned char *bmpData = NULL;
    HBITMAP hthumb = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&bmpData, NULL, 0);
    if (!hthumb)
        return E_OUTOFMEMORY;

    page = engine->Transform(thumb.Convert<double>(), 1, zoom, 0, true);
    RenderedBitmap *bmp = engine->RenderBitmap(1, zoom, 0, &page);

    HDC hdc = GetDC(NULL);
    if (bmp && GetDIBits(hdc, bmp->GetBitmap(), 0, thumb.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        // cf. http://msdn.microsoft.com/en-us/library/bb774612(v=VS.85).aspx
        for (int i = 0; i < thumb.dx * thumb.dy; i++)
            bmpData[4 * i + 3] = 0xFF;

        *phbmp = hthumb;
        *pdwAlpha = WTSAT_RGB;
    }
    else {
        DeleteObject(hthumb);
        hthumb = NULL;
    }

    ReleaseDC(NULL, hdc);
    delete bmp;

    return hthumb ? S_OK : E_FAIL;
}

#define COL_WINDOW_BG   RGB(0x99, 0x99, 0x99)
#define PREVIEW_MARGIN  2
#define UWM_PAINT_AGAIN (WM_USER + 1)

class PageRenderer {
    BaseEngine *engine;
    HWND hwnd;

    int currPage;
    RenderedBitmap *currBmp;
    int reqPage;
    float reqZoom;

    CRITICAL_SECTION currAccess;
    HANDLE thread;

public:
    PageRenderer(BaseEngine *engine, HWND hwnd) : engine(engine), hwnd(hwnd),
        currPage(0), currBmp(NULL), reqPage(0), reqZoom(0), thread(NULL) {
        InitializeCriticalSection(&currAccess);
    }
    ~PageRenderer() {
        if (thread)
            WaitForSingleObject(thread, INFINITE);
        delete currBmp;
        DeleteCriticalSection(&currAccess);
    }

    BaseEngine *GetEngine() const { return engine; }

    void Render(HDC hdc, RectI target, int pageNo, float zoom) {
        ScopedCritSec scope(&currAccess);
        if (currPage == pageNo && currBmp && currBmp->Size() == target.Size())
            currBmp->StretchDIBits(hdc, target);
        else if (!thread) {
            reqPage = pageNo;
            reqZoom = zoom;
            thread = CreateThread(NULL, 0, RenderThread, this, 0, 0);
        }
    }

protected:
    static DWORD WINAPI RenderThread(LPVOID data) {
        ScopedCom comScope; // because the engine reads data from a COM IStream

        PageRenderer *pr = (PageRenderer *)data;
        RenderedBitmap *bmp = pr->engine->RenderBitmap(pr->reqPage, pr->reqZoom, 0);
        pr->engine->RunGC();

        ScopedCritSec scope(&pr->currAccess);

        delete pr->currBmp;
        pr->currBmp = bmp;
        pr->currPage = pr->reqPage;;

        HANDLE thread = pr->thread;
        pr->thread = NULL;
        PostMessage(pr->hwnd, UWM_PAINT_AGAIN, 0, 0);

        CloseHandle(thread);
        return 0;
    }
};

static LRESULT OnPaint(HWND hwnd)
{
    ClientRect rect(hwnd);
    DoubleBuffer buffer(hwnd, rect);
    HDC hdc = buffer.GetDC();
    HBRUSH brushBg = CreateSolidBrush(COL_WINDOW_BG);
    HBRUSH brushWhite = GetStockBrush(WHITE_BRUSH);
    FillRect(hdc, &rect.ToRECT(), brushBg);

    PreviewBase *preview = (PreviewBase *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview && preview->renderer) {
        BaseEngine *engine = preview->renderer->GetEngine();
        int pageNo = GetScrollPos(hwnd, SB_VERT);
        rect.Inflate(-PREVIEW_MARGIN, -PREVIEW_MARGIN);
        RectD page = engine->Transform(engine->PageMediabox(pageNo), pageNo, 1.0, 0);
        float zoom = (float)min(rect.dx / page.dx, rect.dy / page.dy);
        RectI onScreen = RectD(rect.x, rect.y, page.dx * zoom, page.dy * zoom).Round();
        onScreen.Offset((rect.dx - onScreen.dx) / 2, (rect.dy - onScreen.dy) / 2);

        FillRect(hdc, &onScreen.ToRECT(), brushWhite);
        preview->renderer->Render(hdc, onScreen, pageNo, zoom);
    }

    DeleteObject(brushBg);
    DeleteObject(brushWhite);

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

static LRESULT OnKeydown(HWND hwnd, WPARAM key)
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

static LRESULT OnDestroy(HWND hwnd)
{
    PreviewBase *preview = (PreviewBase *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview) {
        delete preview->renderer;
        preview->renderer = NULL;
    }
    return 0;
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
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        return 0;
    case WM_MOUSEWHEEL:
        return OnVScroll(hwnd, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? SB_LINEUP : SB_LINEDOWN);
    case WM_DESTROY:
        return OnDestroy(hwnd);
    case UWM_PAINT_AGAIN:
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
        return 0;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

IFACEMETHODIMP PreviewBase::DoPreview()
{
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = PreviewWndProc;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = _T("SumatraPDF_PreviewPane");
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wcex);

    m_hwnd = CreateWindow(wcex.lpszClassName, NULL, WS_CHILD | WS_VSCROLL | WS_VISIBLE,
                          m_rcParent.x, m_rcParent.x, m_rcParent.dx, m_rcParent.dy,
                          m_hwndParent, NULL, NULL, NULL);
    if (!m_hwnd)
        return HRESULT_FROM_WIN32(GetLastError());

    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
    BaseEngine *engine = GetEngine();
    this->renderer = engine ? new PageRenderer(engine, m_hwnd) : NULL;

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
