/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WinUtil.h"
#include "mui/Mui.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "RegistryPreview.h"

// TODO: move code to PdfPreviewBase.cpp
#include "PdfPreviewBase.h"

#include "utils/Log.h"

constexpr COLORREF kColWindowBg = RGB(0x99, 0x99, 0x99);
constexpr int kPreviewMargin = 2;
constexpr UINT kUwmPaintAgain = (WM_USER + 101);

void _uploadDebugReportIfFunc(bool, const char*) {
    // no-op implementation to satisfy SubmitBugReport()
}

IFACEMETHODIMP PreviewBase::GetThumbnail(uint cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    EngineBase* engine = GetEngine();
    if (!engine) {
        logf("PreviewBase::GetThumbnail: failed to get the engine\n");
        return E_FAIL;
    }

    logf("PreviewBase::GetThumbnail(cx=%d, engine: %s\n", (int)cx, engine->kind);

    RectF page = engine->Transform(engine->PageMediabox(1), 1, 1.0, 0);
    float zoom = std::min(cx / (float)page.dx, cx / (float)page.dy) - 0.001f;
    Rect thumb = RectF(0, 0, page.dx * zoom, page.dy * zoom).Round();

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biHeight = thumb.dy;
    bmi.bmiHeader.biWidth = thumb.dx;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    u8* bmpData = nullptr;
    HBITMAP hthumb = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, (void**)&bmpData, nullptr, 0);
    if (!hthumb) {
        log("PreviewBase::GetThumbnail: CreateDIBSection() failed\n");
        return E_OUTOFMEMORY;
    }

    page = engine->Transform(ToRectF(thumb), 1, zoom, 0, true);
    RenderPageArgs args(1, zoom, 0, &page);
    RenderedBitmap* bmp = engine->RenderPage(args);

    HDC hdc = GetDC(nullptr);
    if (bmp && GetDIBits(hdc, bmp->GetBitmap(), 0, thumb.dy, bmpData, &bmi, DIB_RGB_COLORS)) {
        // cf. http://msdn.microsoft.com/en-us/library/bb774612(v=VS.85).aspx
        for (int i = 0; i < thumb.dx * thumb.dy; i++) {
            bmpData[4 * i + 3] = 0xFF;
        }

        *phbmp = hthumb;
        if (pdwAlpha) {
            *pdwAlpha = WTSAT_RGB;
        }
        log("PreviewBase::GetThumbnail: provided thumbnail\n");
    } else {
        DeleteObject(hthumb);
        hthumb = nullptr;
        log("PreviewBase::GetThumbnail: GetDIBits() failed\n");
    }

    ReleaseDC(nullptr, hdc);
    delete bmp;

    return hthumb ? S_OK : E_NOTIMPL;
}

class PageRenderer {
    EngineBase* engine = nullptr;
    HWND hwnd = nullptr;

    int currPage = 0;
    RenderedBitmap* currBmp = nullptr;
    // due to rounding differences, currBmp->Size() and currSize can differ slightly
    Size currSize;
    int reqPage = 0;
    float reqZoom = 0.f;
    Size reqSize = {};
    bool reqAbort = false;
    AbortCookie* abortCookie = nullptr;

    CRITICAL_SECTION currAccess;
    HANDLE thread = nullptr;

    // seeking inside an IStream spins an inner event loop
    // which can cause reentrance in OnPaint and leave an
    // engine semi-initialized when it's called recursively
    // (this only applies for the UI thread where the critical
    // sections can't prevent recursion without the risk of deadlock)
    bool preventRecursion = false;

  public:
    PageRenderer(EngineBase* engine, HWND hwnd) {
        this->engine = engine;
        this->hwnd = hwnd;
        InitializeCriticalSection(&currAccess);
    }
    ~PageRenderer() {
        if (thread) {
            WaitForSingleObject(thread, INFINITE);
        }
        delete currBmp;
        DeleteCriticalSection(&currAccess);
    }

    RectF GetPageRect(int pageNo) {
        if (preventRecursion) {
            return RectF();
        }

        preventRecursion = true;
        // assume that any engine methods could lead to a seek
        RectF bbox = engine->PageMediabox(pageNo);
        bbox = engine->Transform(bbox, pageNo, 1.0, 0);
        preventRecursion = false;
        return bbox;
    }

    void Render(HDC hdc, Rect target, int pageNo, float zoom) {
        log("PageRenderer::Render()\n");

        ScopedCritSec scope(&currAccess);
        if (currBmp && currPage == pageNo && currSize == target.Size()) {
            currBmp->Blit(hdc, target);
        } else if (!thread) {
            reqPage = pageNo;
            reqZoom = zoom;
            reqSize = target.Size();
            reqAbort = false;
            thread = CreateThread(nullptr, 0, RenderThread, this, 0, nullptr);
        } else if (reqPage != pageNo || reqSize != target.Size()) {
            if (abortCookie) {
                abortCookie->Abort();
            }
            reqAbort = true;
        }
    }

  protected:
    static DWORD WINAPI RenderThread(LPVOID data) {
        log("PageRenderer::RenderThread started\n");
        ScopedCom comScope; // because the engine reads data from a COM IStream

        PageRenderer* pr = (PageRenderer*)data;
        RenderPageArgs args(pr->reqPage, pr->reqZoom, 0, nullptr, RenderTarget::View, &pr->abortCookie);
        RenderedBitmap* bmp = pr->engine->RenderPage(args);
        if (!bmp) {
            return 0;
        }

        ScopedCritSec scope(&pr->currAccess);

        if (!pr->reqAbort) {
            delete pr->currBmp;
            pr->currBmp = bmp;
            pr->currPage = pr->reqPage;
            pr->currSize = pr->reqSize;
        } else {
            delete bmp;
        }
        delete pr->abortCookie;
        pr->abortCookie = nullptr;

        HANDLE th = pr->thread;
        pr->thread = nullptr;
        PostMessageW(pr->hwnd, kUwmPaintAgain, 0, 0);

        CloseHandle(th);
        DestroyTempAllocator();
        return 0;
    }
};

static LRESULT OnPaint(HWND hwnd) {
    Rect rect = ClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rect);
    HDC hdc = buffer.GetDC();
    HBRUSH brushBg = CreateSolidBrush(kColWindowBg);
    HBRUSH brushWhite = GetStockBrush(WHITE_BRUSH);
    RECT rcClient = ToRECT(rect);
    FillRect(hdc, &rcClient, brushBg);

    PreviewBase* preview = (PreviewBase*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview && preview->renderer) {
        int pageNo = GetScrollPos(hwnd, SB_VERT);
        RectF page = preview->renderer->GetPageRect(pageNo);
        if (!page.IsEmpty()) {
            rect.Inflate(-kPreviewMargin, -kPreviewMargin);
            float zoom = (float)std::min(rect.dx / page.dx, rect.dy / page.dy) - 0.001f;
            Rect onScreen = RectF((float)rect.x, (float)rect.y, (float)page.dx * zoom, (float)page.dy * zoom).Round();
            onScreen.Offset((rect.dx - onScreen.dx) / 2, (rect.dy - onScreen.dy) / 2);

            RECT rcPage = ToRECT(onScreen);
            FillRect(hdc, &rcPage, brushWhite);
            preview->renderer->Render(hdc, onScreen, pageNo, zoom);
        }
    }

    DeleteObject(brushBg);
    DeleteObject(brushWhite);

    PAINTSTRUCT ps;
    buffer.Flush(BeginPaint(hwnd, &ps));
    EndPaint(hwnd, &ps);
    return 0;
}

static LRESULT OnVScroll(HWND hwnd, WPARAM wp) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    switch (LOWORD(wp)) {
        case SB_TOP:
            si.nPos = si.nMin;
            break;
        case SB_BOTTOM:
            si.nPos = si.nMax;
            break;
        case SB_LINEUP:
            si.nPos--;
            break;
        case SB_LINEDOWN:
            si.nPos++;
            break;
        case SB_PAGEUP:
            si.nPos--;
            break;
        case SB_PAGEDOWN:
            si.nPos++;
            break;
        case SB_THUMBTRACK:
            si.nPos = si.nTrackPos;
            break;
    }
    si.fMask = SIF_POS;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
    return 0;
}

static LRESULT OnKeydown(HWND hwnd, WPARAM key) {
    switch (key) {
        case VK_DOWN:
        case VK_RIGHT:
        case VK_NEXT:
            return OnVScroll(hwnd, SB_PAGEDOWN);
        case VK_UP:
        case VK_LEFT:
        case VK_PRIOR:
            return OnVScroll(hwnd, SB_PAGEUP);
        case VK_HOME:
            return OnVScroll(hwnd, SB_TOP);
        case VK_END:
            return OnVScroll(hwnd, SB_BOTTOM);
        default:
            return 0;
    }
}

static LRESULT OnDestroy(HWND hwnd) {
    PreviewBase* preview = (PreviewBase*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (preview) {
        delete preview->renderer;
        preview->renderer = nullptr;
    }
    return 0;
}

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            return OnPaint(hwnd);
        case WM_VSCROLL:
            return OnVScroll(hwnd, wp);
        case WM_KEYDOWN:
            return OnKeydown(hwnd, wp);
        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            return 0;
        case WM_MOUSEWHEEL:
            return OnVScroll(hwnd, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? SB_LINEUP : SB_LINEDOWN);
        case WM_DESTROY:
            return OnDestroy(hwnd);
        case kUwmPaintAgain:
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

IFACEMETHODIMP PreviewBase::DoPreview() {
    log("PreviewBase::DoPreview()\n");

    WNDCLASSEX wcex{};
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = PreviewWndProc;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"SumatraPDF_PreviewPane";
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wcex);

    m_hwnd = CreateWindow(wcex.lpszClassName, nullptr, WS_CHILD | WS_VSCROLL | WS_VISIBLE, m_rcParent.x, m_rcParent.x,
                          m_rcParent.dx, m_rcParent.dy, m_hwndParent, nullptr, nullptr, nullptr);
    if (!m_hwnd) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    this->renderer = nullptr;
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    EngineBase* engine = GetEngine();
    int pageCount = 1;
    if (engine) {
        pageCount = engine->PageCount();
        this->renderer = new PageRenderer(engine, m_hwnd);
        // don't use the engine afterwards directly (cf. PageRenderer::preventRecursion)
        engine = nullptr;
    }

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nPos = 1;
    si.nMin = 1;
    si.nMax = pageCount;
    si.nPage = si.nMax > 1 ? 1 : 2;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

    ShowWindow(m_hwnd, SW_SHOW);
    return S_OK;
}

EngineBase* PdfPreview::LoadEngine(IStream* stream) {
    log("PdfPreview::LoadEngine()\n");
    return CreateEngineMupdfFromStream(stream, "foo.pdf");
}

#if 0
EngineBase* XpsPreview::LoadEngine(IStream* stream) {
    return CreateEngineXpFromStream(stream);
}
#endif

EngineBase* DjVuPreview::LoadEngine(IStream* stream) {
    log("DjVuPreview::LoadEngine()\n");
    return CreateEngineDjVuFromStream(stream);
}

EpubPreview::EpubPreview(long* plRefCount) : PreviewBase(plRefCount, kEpubPreviewClsid) {
    log("EpubPreview::EpubPreview()\n");
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

EpubPreview::~EpubPreview() {
    mui::Destroy();
}

EngineBase* EpubPreview::LoadEngine(IStream* stream) {
    log("EpubPreview::LoadEngine()\n");
    return CreateEngineEpubFromStream(stream);
}

Fb2Preview::Fb2Preview(long* plRefCount) : PreviewBase(plRefCount, kFb2PreviewClsid) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

Fb2Preview::~Fb2Preview() {
    mui::Destroy();
}

EngineBase* Fb2Preview::LoadEngine(IStream* stream) {
    log("Fb2Preview::LoadEngine()\n");
    return CreateEngineFb2FromStream(stream);
}

MobiPreview::MobiPreview(long* plRefCount) : PreviewBase(plRefCount, kMobiPreviewClsid) {
    m_gdiScope = new ScopedGdiPlus();
    mui::Initialize();
}

MobiPreview::~MobiPreview() {
    mui::Destroy();
}

EngineBase* MobiPreview::LoadEngine(IStream* stream) {
    log("MobiPreview::LoadEngine()\n");
    return CreateEngineMobiFromStream(stream);
}

EngineBase* CbxPreview::LoadEngine(IStream* stream) {
    log("CbxPreview::LoadEngine()\n");
    return CreateEngineCbxFromStream(stream);
}

EngineBase* TgaPreview::LoadEngine(IStream* stream) {
    log("TgaPreview::LoadEngine()\n");
    return CreateEngineImageFromStream(stream);
}
