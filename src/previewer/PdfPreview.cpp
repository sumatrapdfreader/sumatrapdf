/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Archive.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/GdiPlusUtil.h"
#include "base/Win.h"
#include "gui/PlatformFont.h"
#include "gui/PlatformText.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "RegistryPreview.h"

#include "PdfPreview.h"

constexpr Color kColWindowBg = MkRgb(0x99, 0x99, 0x99);
constexpr int kPreviewMargin = 2;
constexpr UINT kUwmPaintAgain = (WM_USER + 101);
// Engine-scale zoom (1 = 100%). Below fit-page we snap back to fit.
constexpr float kPreviewZoomMin = 0.1f;
constexpr float kPreviewZoomMax = 16.f;
constexpr float kPreviewZoomStep = 1.2f;
// Render the whole page when it stays under this many pixels (~32 MB at 32bpp).
// Past that, only the visible region plus a pan slop is rendered.
constexpr i64 kPreviewMaxFullPagePixels = 8 * 1024 * 1024;
constexpr int kPreviewPanPad = 256;

static bool SameZoom(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static PdfPreview* PreviewFromHwnd(HWND hwnd) {
    return (PdfPreview*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

EBookUI* GetEBookUI() {
    return nullptr;
}

FileEBookUI* GetFileEBookUI(Str) {
    return nullptr;
}

// Copy a rendered page into the 32bpp DIB the shell gets, rather than going
// through GetDIBits(bmp->hbmp): only the mupdf engines render into a DIB
// section, so for DjVu and the image engines hbmp is null and GetDIBits failed,
// which is why those never had a thumbnail (issue #1530). Pixmap::data is
// always there.
// Rows are written bottom-up (the DIB has a positive biHeight) and anything
// translucent is composited over white, the same paper the preview window
// paints behind a page. Alpha ends up opaque, matching WTSAT_RGB:
// cf. http://msdn.microsoft.com/en-us/library/bb774612(v=VS.85).aspx
static void CopyPixmapToThumbnail(const Pixmap* bmp, u8* dst, int dx, int dy) {
    int srcBpp = PixmapBytesPerPixel(bmp->format);
    bool isRgb = bmp->format == PixmapFormat::RGBA8;
    bool hasAlpha = srcBpp == 4;
    bool premul = bmp->premultiplied;
    for (int y = 0; y < dy; y++) {
        const u8* src = bmp->data + ((size_t)y * (size_t)bmp->stride);
        u8* row = dst + ((size_t)(dy - 1 - y) * (size_t)dx * 4);
        for (int x = 0; x < dx; x++) {
            int c0 = src[0], c1 = src[1], c2 = src[2];
            int b = isRgb ? c2 : c0;
            int r = isRgb ? c0 : c2;
            int g = c1;
            int a = hasAlpha ? src[3] : 255;
            if (a != 255) {
                int inv = 255 - a;
                if (premul) {
                    b += inv;
                    g += inv;
                    r += inv;
                } else {
                    b = ((b * a) + (255 * inv)) / 255;
                    g = ((g * a) + (255 * inv)) / 255;
                    r = ((r * a) + (255 * inv)) / 255;
                }
            }
            row[0] = (u8)std::min(b, 255);
            row[1] = (u8)std::min(g, 255);
            row[2] = (u8)std::min(r, 255);
            row[3] = 0xFF;
            src += srcBpp;
            row += 4;
        }
    }
}

IFACEMETHODIMP PdfPreview::GetThumbnail(uint cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    EngineBase* engine = GetEngine();
    if (!engine) {
        logf("PdfPreview::GetThumbnail: failed to get the engine\n");
        return E_FAIL;
    }

    logf("PdfPreview::GetThumbnail(cx=%d, engine: %s\n", (int)cx, Str(engine->kind));

    RectF page = engine->Transform(engine->PageMediabox(1), 1, 1.0, 0);
    float zoom = std::min((float)cx / page.dx, (float)cx / page.dy) - 0.001f;
    Rect thumb = RectF(0, 0, page.dx * zoom, page.dy * zoom).Round();

    page = engine->Transform(ToRectF(thumb), 1, zoom, 0, true);
    RenderPageArgs args(1, zoom, 0, &page);
    Pixmap* bmp = engine->RenderPage(args);
    if (!bmp || !bmp->data) {
        log(StrL("PdfPreview::GetThumbnail: RenderPage() failed\n"));
        FreePixmap(bmp);
        return E_FAIL;
    }
    defer {
        FreePixmap(bmp);
    };

    // Size the bitmap from what was actually rendered: rounding can put it a
    // pixel off `thumb`, and copying with the wrong dimensions shears the image.
    int dx = bmp->width;
    int dy = bmp->height;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biHeight = dy;
    bmi.bmiHeader.biWidth = dx;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    u8* bmpData = nullptr;
    HBITMAP hthumb = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, (void**)&bmpData, nullptr, 0);
    if (!hthumb) {
        log(StrL("PdfPreview::GetThumbnail: CreateDIBSection() failed\n"));
        return E_OUTOFMEMORY;
    }

    CopyPixmapToThumbnail(bmp, bmpData, dx, dy);

    *phbmp = hthumb;
    if (pdwAlpha) {
        *pdwAlpha = WTSAT_RGB;
    }
    log(StrL("PdfPreview::GetThumbnail: provided thumbnail\n"));
    return S_OK;
}

// Page at userZoom (or fit), pan when it is larger than the pane.
// userZoom == 0 means "fit page". Zooming back to fit-or-smaller snaps to fit.
struct PreviewLayout {
    Rect content;
    RectF page;
    float fitZoom = 0;
    float zoom = 0;
    Rect onScreen;
    Rect visible;
    bool canPanX = false;
    bool canPanY = false;
};

static void ResetPreviewView(PdfPreview* preview) {
    if (!preview) {
        return;
    }
    preview->userZoom = 0;
    preview->panX = 0;
    preview->panY = 0;
    preview->panning = false;
}

class PageRenderer {
    EngineBase* engine = nullptr;
    HWND hwnd = nullptr;

    int currPage = 0;
    float currZoom = 0.f;
    Rect currClip;
    Pixmap* currBmp = nullptr;
    int reqPage = 0;
    float reqZoom = 0.f;
    Rect reqClip;
    RectF reqPageRect;
    bool reqUseClip = false;
    bool reqAbort = false;
    AbortCookie* abortCookie = nullptr;

    Mutex currAccess;
    ThreadHandle thread = nullptr;

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
    }
    ~PageRenderer() {
        if (thread) {
            WaitForSingleObject(thread, INFINITE);
        }
        FreePixmap(currBmp);
    }

    RectF GetPageRect(int pageNo) {
        if (preventRecursion) {
            return {};
        }

        preventRecursion = true;
        // assume that any engine methods could lead to a seek
        RectF bbox = engine->PageMediabox(pageNo);
        bbox = engine->Transform(bbox, pageNo, 1.0, 0);
        preventRecursion = false;
        return bbox;
    }

    void Render(HDC hdc, const PreviewLayout& lo, int pageNo);

  protected:
    RectF ScreenToPage(int pageNo, float zoom, RectF screen) {
        if (preventRecursion) {
            return {};
        }
        preventRecursion = true;
        RectF r = engine->Transform(screen, pageNo, zoom, 0, true);
        preventRecursion = false;
        return r;
    }

    static bool ClipCovers(int page, float zoom, Rect clip, int wantPage, float wantZoom, Rect visInPage) {
        return page == wantPage && SameZoom(zoom, wantZoom) && clip.Intersect(visInPage) == visInPage;
    }

    static void BlitCached(HDC hdc, Pixmap* bmp, Rect clip, Rect visInPage, Rect onScreen) {
        if (!bmp) {
            return;
        }
        Rect overlap = clip.Intersect(visInPage);
        if (overlap.IsEmpty()) {
            return;
        }
        Rect src(overlap.x - clip.x, overlap.y - clip.y, overlap.dx, overlap.dy);
        src = src.Intersect(Rect(0, 0, bmp->width, bmp->height));
        if (src.IsEmpty()) {
            return;
        }
        Rect dst(onScreen.x + clip.x + src.x, onScreen.y + clip.y + src.y, src.dx, src.dy);
        BlitPixmapRegion(bmp, hdc, dst, src);
    }

    static DWORD WINAPI RenderThread(LPVOID data) {
        log(StrL("PageRenderer::RenderThread started\n"));
        ScopedCom comScope; // because the engine reads data from a COM IStream

        PageRenderer* pr = (PageRenderer*)data;
        RenderPageArgs args(pr->reqPage, pr->reqZoom, 0, pr->reqUseClip ? &pr->reqPageRect : nullptr,
                            RenderTarget::View, &pr->abortCookie);
        Pixmap* bmp = pr->engine->RenderPage(args);

        ScopedMutex scope(&pr->currAccess);

        if (bmp && !pr->reqAbort) {
            FreePixmap(pr->currBmp);
            pr->currBmp = bmp;
            pr->currPage = pr->reqPage;
            pr->currZoom = pr->reqZoom;
            pr->currClip = pr->reqClip;
        } else {
            FreePixmap(bmp);
        }
        delete pr->abortCookie;
        pr->abortCookie = nullptr;

        ThreadHandle th = pr->thread;
        pr->thread = nullptr;
        PostMessageW(pr->hwnd, kUwmPaintAgain, 0, 0);

        SafeCloseThreadHandle(&th);
        DestroyTempArena();
        return 0;
    }
};

void PageRenderer::Render(HDC hdc, const PreviewLayout& lo, int pageNo) {
    if (lo.visible.IsEmpty() || lo.zoom <= 0) {
        return;
    }

    Rect visInPage(lo.visible.x - lo.onScreen.x, lo.visible.y - lo.onScreen.y, lo.visible.dx, lo.visible.dy);
    Rect pageRectPx(0, 0, lo.onScreen.dx, lo.onScreen.dy);
    visInPage = visInPage.Intersect(pageRectPx);
    if (visInPage.IsEmpty()) {
        return;
    }

    i64 fullPixels = (i64)lo.onScreen.dx * (i64)lo.onScreen.dy;
    Rect wantClip;
    if (fullPixels > 0 && fullPixels <= kPreviewMaxFullPagePixels) {
        wantClip = pageRectPx;
    } else {
        wantClip = visInPage;
        wantClip.Inflate(kPreviewPanPad, kPreviewPanPad);
        wantClip = wantClip.Intersect(pageRectPx);
    }

    ScopedMutex scope(&currAccess);

    if (currBmp && ClipCovers(currPage, currZoom, currClip, pageNo, lo.zoom, visInPage)) {
        BlitCached(hdc, currBmp, currClip, visInPage, lo.onScreen);
        return;
    }

    if (currBmp && currPage == pageNo && SameZoom(currZoom, lo.zoom)) {
        BlitCached(hdc, currBmp, currClip, visInPage, lo.onScreen);
    }

    if (!thread) {
        bool useClip = wantClip != pageRectPx;
        RectF pageClip;
        if (useClip) {
            RectF screenClip((float)wantClip.x, (float)wantClip.y, (float)wantClip.dx, (float)wantClip.dy);
            pageClip = ScreenToPage(pageNo, lo.zoom, screenClip);
            if (pageClip.IsEmpty()) {
                return;
            }
        }
        reqPage = pageNo;
        reqZoom = lo.zoom;
        reqClip = wantClip;
        reqPageRect = pageClip;
        reqUseClip = useClip;
        reqAbort = false;
        thread = CreateThread(nullptr, 0, RenderThread, this, 0, nullptr);
    } else if (!ClipCovers(reqPage, reqZoom, reqClip, pageNo, lo.zoom, visInPage)) {
        if (abortCookie) {
            abortCookie->Abort();
        }
        reqAbort = true;
    }
}

static PreviewLayout ComputePreviewLayout(HWND hwnd, PdfPreview* preview, int pageNo) {
    PreviewLayout lo;
    lo.content = HwndClientRect(hwnd);
    lo.content.Inflate(-kPreviewMargin, -kPreviewMargin);
    if (!preview || !preview->renderer || lo.content.IsEmpty()) {
        return lo;
    }
    lo.page = preview->renderer->GetPageRect(pageNo);
    if (lo.page.IsEmpty() || lo.page.dx <= 0 || lo.page.dy <= 0) {
        return lo;
    }
    lo.fitZoom = std::min((float)lo.content.dx / lo.page.dx, (float)lo.content.dy / lo.page.dy) - 0.001f;
    if (lo.fitZoom < kPreviewZoomMin) {
        lo.fitZoom = kPreviewZoomMin;
    }
    if (preview->userZoom > 0 && preview->userZoom <= lo.fitZoom) {
        preview->userZoom = 0;
        preview->panX = 0;
        preview->panY = 0;
    }
    lo.zoom = preview->userZoom > 0 ? preview->userZoom : lo.fitZoom;
    Rect pagePx = RectF(0, 0, lo.page.dx * lo.zoom, lo.page.dy * lo.zoom).Round();
    lo.canPanX = pagePx.dx > lo.content.dx;
    lo.canPanY = pagePx.dy > lo.content.dy;
    if (lo.canPanX) {
        preview->panX = limitValue(preview->panX, lo.content.dx - pagePx.dx, 0);
        lo.onScreen.x = lo.content.x + preview->panX;
    } else {
        preview->panX = 0;
        lo.onScreen.x = lo.content.x + (lo.content.dx - pagePx.dx) / 2;
    }
    if (lo.canPanY) {
        preview->panY = limitValue(preview->panY, lo.content.dy - pagePx.dy, 0);
        lo.onScreen.y = lo.content.y + preview->panY;
    } else {
        preview->panY = 0;
        lo.onScreen.y = lo.content.y + (lo.content.dy - pagePx.dy) / 2;
    }
    lo.onScreen.dx = pagePx.dx;
    lo.onScreen.dy = pagePx.dy;
    lo.visible = lo.onScreen.Intersect(lo.content);
    return lo;
}

static LRESULT OnPaint(HWND hwnd) {
    Rect rect = HwndClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rect);
    HDC hdc = buffer.GetDC();
    HBRUSH brushBg = CreateSolidBrush(kColWindowBg);
    HBRUSH brushWhite = GetStockBrush(WHITE_BRUSH);
    RECT rcClient = ToRECT(rect);
    HdcFillRect(hdc, ToRect(rcClient), brushBg);

    PdfPreview* preview = PreviewFromHwnd(hwnd);
    if (preview && preview->renderer) {
        int pageNo = GetScrollPos(hwnd, SB_VERT);
        PreviewLayout lo = ComputePreviewLayout(hwnd, preview, pageNo);
        if (!lo.visible.IsEmpty()) {
            HdcFillRect(hdc, lo.visible, brushWhite);
            preview->renderer->Render(hdc, lo, pageNo);
        }
    }

    DeleteObject(brushBg);
    DeleteObject(brushWhite);

    PAINTSTRUCT ps;
    buffer.Flush(BeginPaint(hwnd, &ps));
    EndPaint(hwnd, &ps);
    return 0;
}

enum class PreviewPagePan {
    Top,
    Bottom
};

static LRESULT OnVScroll(HWND hwnd, WPARAM wp, PreviewPagePan pagePan = PreviewPagePan::Top) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);
    int oldPos = si.nPos;

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

    if (si.nPos != oldPos) {
        if (PdfPreview* preview = PreviewFromHwnd(hwnd)) {
            preview->panX = 0;
            preview->panY = pagePan == PreviewPagePan::Bottom ? INT_MIN : 0;
            preview->panning = false;
        }
    }

    HwndInvalidate(hwnd, false);
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

static bool PreviewTryPan(HWND hwnd, PdfPreview* preview, int dx, int dy) {
    if (!preview) {
        return false;
    }
    int pageNo = GetScrollPos(hwnd, SB_VERT);
    PreviewLayout lo = ComputePreviewLayout(hwnd, preview, pageNo);
    if (!lo.canPanX && !lo.canPanY) {
        return false;
    }
    int oldX = preview->panX;
    int oldY = preview->panY;
    if (lo.canPanX) {
        preview->panX += dx;
    }
    if (lo.canPanY) {
        preview->panY += dy;
    }
    ComputePreviewLayout(hwnd, preview, pageNo);
    if (preview->panX == oldX && preview->panY == oldY) {
        return false;
    }
    HwndInvalidate(hwnd, false);
    return true;
}

static void PreviewZoomAt(HWND hwnd, PdfPreview* preview, Point focus, int wheelDelta) {
    if (!preview) {
        return;
    }
    int pageNo = GetScrollPos(hwnd, SB_VERT);
    PreviewLayout lo = ComputePreviewLayout(hwnd, preview, pageNo);
    if (lo.zoom <= 0 || lo.page.IsEmpty()) {
        return;
    }
    float notches = (float)wheelDelta / (float)WHEEL_DELTA;
    float newZoom = lo.zoom * powf(kPreviewZoomStep, notches);
    float zoomMax = std::max(kPreviewZoomMax, lo.fitZoom * 4.f);
    newZoom = limitValue(newZoom, kPreviewZoomMin, zoomMax);
    if (newZoom <= lo.fitZoom) {
        if (preview->userZoom == 0) {
            return;
        }
        ResetPreviewView(preview);
        HwndInvalidate(hwnd, false);
        return;
    }
    if (SameZoom(newZoom, lo.zoom) && preview->userZoom > 0) {
        return;
    }
    if (!lo.onScreen.Contains(focus)) {
        focus = Point(lo.onScreen.x + lo.onScreen.dx / 2, lo.onScreen.y + lo.onScreen.dy / 2);
    }
    float pageX = (float)(focus.x - lo.onScreen.x) / lo.zoom;
    float pageY = (float)(focus.y - lo.onScreen.y) / lo.zoom;
    preview->userZoom = newZoom;
    Rect newPx = RectF(0, 0, lo.page.dx * newZoom, lo.page.dy * newZoom).Round();
    if (newPx.dx > lo.content.dx) {
        preview->panX = focus.x - lo.content.x - (int)floorf(pageX * newZoom + 0.5f);
    } else {
        preview->panX = 0;
    }
    if (newPx.dy > lo.content.dy) {
        preview->panY = focus.y - lo.content.y - (int)floorf(pageY * newZoom + 0.5f);
    } else {
        preview->panY = 0;
    }
    ComputePreviewLayout(hwnd, preview, pageNo);
    HwndInvalidate(hwnd, false);
}

static void PreviewStartPan(HWND hwnd, PdfPreview* preview, Point pt) {
    if (!preview) {
        return;
    }
    int pageNo = GetScrollPos(hwnd, SB_VERT);
    PreviewLayout lo = ComputePreviewLayout(hwnd, preview, pageNo);
    if (!lo.canPanX && !lo.canPanY) {
        return;
    }
    preview->panning = true;
    preview->panLast = pt;
    SetCapture(hwnd);
    SetCursorCached(IDC_SIZEALL);
}

static void PreviewEndPan(HWND hwnd, PdfPreview* preview) {
    if (!preview || !preview->panning) {
        return;
    }
    preview->panning = false;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
}

static LRESULT OnMouseWheel(HWND hwnd, WPARAM wp, LPARAM lp, bool horizontal) {
    PdfPreview* preview = PreviewFromHwnd(hwnd);
    short delta = GET_WHEEL_DELTA_WPARAM(wp);
    bool isCtrl = (LOWORD(wp) & MK_CONTROL) || IsCtrlPressed();
    bool isShift = (LOWORD(wp) & MK_SHIFT) || IsShiftPressed();
    if (!horizontal && isCtrl) {
        Point pt = HwndScreenToClient(hwnd, Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
        PreviewZoomAt(hwnd, preview, pt, delta);
        return 0;
    }
    int pageNo = GetScrollPos(hwnd, SB_VERT);
    PreviewLayout lo = ComputePreviewLayout(hwnd, preview, pageNo);
    int step = std::max(40, lo.content.dy / 8);
    int dist = MulDiv((int)delta, step, WHEEL_DELTA);
    if (horizontal || isShift) {
        PreviewTryPan(hwnd, preview, horizontal ? -dist : dist, 0);
        return 0;
    }
    if (lo.canPanY) {
        if (PreviewTryPan(hwnd, preview, 0, dist)) {
            return 0;
        }
        if (dist == 0) {
            return 0;
        }
        return OnVScroll(hwnd, dist > 0 ? SB_LINEUP : SB_LINEDOWN,
                         dist > 0 ? PreviewPagePan::Bottom : PreviewPagePan::Top);
    }
    return OnVScroll(hwnd, delta > 0 ? SB_LINEUP : SB_LINEDOWN);
}

static LRESULT OnDestroy(HWND hwnd) {
    PdfPreview* preview = PreviewFromHwnd(hwnd);
    if (preview) {
        delete preview->renderer;
        preview->renderer = nullptr;
        preview->panning = false;
    }
    return 0;
}

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PdfPreview* preview = PreviewFromHwnd(hwnd);
    switch (msg) {
        case WM_PAINT:
            return OnPaint(hwnd);
        case WM_ERASEBKGND:
            return 1;
        case WM_VSCROLL:
            return OnVScroll(hwnd, wp);
        case WM_KEYDOWN:
            return OnKeydown(hwnd, wp);
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
            HwndSetFocus(hwnd);
            PreviewStartPan(hwnd, preview, Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
            return 0;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
            PreviewEndPan(hwnd, preview);
            return 0;
        case WM_LBUTTONDBLCLK:
            PreviewEndPan(hwnd, preview);
            if (preview && preview->userZoom > 0) {
                ResetPreviewView(preview);
                HwndInvalidate(hwnd, false);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (preview && preview->panning) {
                Point pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                int dx = pt.x - preview->panLast.x;
                int dy = pt.y - preview->panLast.y;
                preview->panLast = pt;
                PreviewTryPan(hwnd, preview, dx, dy);
            }
            return 0;
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
            if (preview) {
                preview->panning = false;
            }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT && preview && (preview->panning || preview->userZoom > 0)) {
                SetCursorCached(IDC_SIZEALL);
                return TRUE;
            }
            break;
        case WM_MOUSEWHEEL:
            return OnMouseWheel(hwnd, wp, lp, false);
        case WM_MOUSEHWHEEL:
            return OnMouseWheel(hwnd, wp, lp, true);
        case WM_DESTROY:
            return OnDestroy(hwnd);
        case kUwmPaintAgain:
            HwndInvalidate(hwnd, false);
            UpdateWindow(hwnd);
            return 0;
        default:
            break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

IFACEMETHODIMP PdfPreview::DoPreview() {
    log(StrL("PdfPreview::DoPreview()\n"));

    WNDCLASSEX wcex{};
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = PreviewWndProc;
    wcex.hCursor = GetCachedCursor(IDC_ARROW);
    wcex.lpszClassName = L"SumatraPDF_PreviewPane";
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    RegisterClassEx(&wcex);

    m_hwnd = CreateWindow(wcex.lpszClassName, nullptr, WS_CHILD | WS_VSCROLL | WS_VISIBLE, m_rcParent.x, m_rcParent.x,
                          m_rcParent.dx, m_rcParent.dy, m_hwndParent, nullptr, nullptr, nullptr);
    if (!m_hwnd) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    this->renderer = nullptr;
    ResetPreviewView(this);
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

static bool NeedsGdiPlus(PreviewType type) {
    return type == PreviewType::DjVu || type == PreviewType::Epub || type == PreviewType::Fb2 ||
           type == PreviewType::Mobi || type == PreviewType::Cbx || type == PreviewType::Tga;
}

PdfPreview::PdfPreview(AtomicInt* plRefCount, PreviewType type) {
    m_type = type;
    m_plModuleRef = plRefCount;
    AtomicIntInc(m_plModuleRef);
    if (NeedsGdiPlus(type)) {
        m_gdiScope = new ScopedGdiPlus();
    }
}

PdfPreview::~PdfPreview() {
    Unload();
    if (m_gdiScope) {
        // the cached gdiplus objects text measuring keeps around must go before
        // gdiplus itself does
        PlatformFontDestroy();
        delete m_gdiScope;
    }
    InterlockedDecrement(m_plModuleRef);
}

// If data is a zip (fb2z/fbz/fb2.zip), return owned bytes of the .fb2 member.
// Otherwise return empty (caller uses original data).
static Str ExtractFb2FromZipData(Str data) {
    if (len(data) < 4 || data.s[0] != 'P' || data.s[1] != 'K') {
        return {};
    }
    Archive* archive = OpenArchiveFromData(data);
    if (!archive) {
        return {};
    }
    AutoDelete delArchive(archive);
    const auto& files = archive->GetFileInfos();
    int fb2Id = -1;
    for (auto* fi : files) {
        if (str::EndsWithI(fi->name, StrL(".fb2"))) {
            if (fb2Id >= 0) {
                return {}; // more than one .fb2
            }
            fb2Id = fi->fileId;
        } else if (!str::EndsWithI(fi->name, StrL(".url"))) {
            // same restrictiveness as Fb2Doc archive load
            return {};
        }
    }
    if (fb2Id < 0 && len(files) == 1) {
        fb2Id = 0;
    }
    if (fb2Id < 0) {
        return {};
    }
    auto* fi = archive->GetFileDataById(fb2Id);
    if (!fi || !fi->data) {
        return {};
    }
    // take ownership of the decompressed bytes
    Str res = Str(fi->data, fi->fileSizeUncompressed);
    fi->data = nullptr;
    return res;
}

// Prefer mupdf for FB2 (same engine as the main viewer). The legacy
// CreateEngineFb2FromData path fails on some plain FB2 files that mupdf
// opens fine (issue #1677 sample set). Zip containers are unwrapped first.
static EngineBase* CreateFb2PreviewEngine(Str data) {
    Str extracted = ExtractFb2FromZipData(data);
    Str plain = extracted ? extracted : data;
    EngineBase* engine = CreateEngineMupdfFromData(plain, StrL("document.fb2"), nullptr);
    str::Free(extracted);
    if (engine) {
        return engine;
    }
    // fall back to the old formatter engine
    return CreateEngineFb2FromData(data);
}

// data stays owned by the caller: every engine below copies what it needs
EngineBase* PdfPreview::LoadEngine(const Str& data) {
    if (str::IsNull(data)) {
        return nullptr;
    }
    switch (m_type) {
        case PreviewType::Pdf:
            return CreateEngineMupdfFromData(data, StrL("foo.pdf"), nullptr);
        case PreviewType::Xps:
            return CreateEngineMupdfFromData(data, StrL("foo.xps"), nullptr);
        case PreviewType::DjVu:
            return CreateEngineDjvuDecFromData(data);
        case PreviewType::Epub:
            return CreateEngineEpubFromData(data);
        case PreviewType::Fb2:
            return CreateFb2PreviewEngine(data);
        case PreviewType::Mobi: {
            Str pdf = ExtractPdfFromPrintReplicaData(data);
            if (len(pdf) > 0) {
                EngineBase* engine = CreateEngineMupdfFromData(pdf, StrL("file.pdf"), nullptr);
                str::Free(pdf);
                return engine;
            }
            return CreateEngineMobiFromData(data);
        }
        case PreviewType::Cbx:
            return CreateEngineCbxFromData(data);
        case PreviewType::Tga:
            return CreateEngineImageFromData(data);
    }
    return nullptr;
}
