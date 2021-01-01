/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}
#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"
#include "utils/LogDbg.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/CheckboxCtrl.h"
#include "wingui/EditCtrl.h"
#include "wingui/DropDownCtrl.h"
#include "wingui/StaticCtrl.h"
#include "wingui/ProgressCtrl.h"

#include "test-app.h"

#include "projectcontext.h"
#include "heapbuf.h"
#include "lice/lice.h"

static LICE_IBitmap* framebuffer;

static HINSTANCE hInst;
static const WCHAR* WIN_CLASS = L"LiceWndCls";
static HWND g_hwnd = nullptr;
static void* gLvg = nullptr;
static LICE_IBitmap* gSvgPrinter = nullptr;

fz_locks_context fz_locks_ctx{};
CRITICAL_SECTION mutexes[FZ_LOCK_MAX];
fz_context* ctx = nullptr;
fz_image* svgPrinterImage = nullptr;
fz_pixmap* svgPrinterPixmap = nullptr;


#define COL_GRAY RGB(0xdd, 0xdd, 0xdd)
#define COL_WHITE RGB(0xff, 0xff, 0xff)
#define COL_BLACK RGB(0, 0, 0)

const char* lvg01 = R"(<LVG 1 100 100
  aa
  line 0 0 100 100
  line 1 0 101 100
  line 2 0 102 100
  line 3 0 103 100
  color f00
  circle 20 20 10 fill=true
  color 550
  circle 80 80 10 fill=true
>)";

// https://github.com/robertjanes/svg-path-prettify
const char* svgPrinter = R"(<svg xmlns="http://www.w3.org/2000/svg" class="icon icon-tabler icon-tabler-printer" width="24" height="24" viewBox="0 0 24 24" stroke-width="2" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M17 17h2a2 2 0 0 0 2 -2v-4a2 2 0 0 0 -2 -2h-14a2 2 0 0 0 -2 2v4a2 2 0 0 0 2 2h2" />
  <path d="M17 9v-4a2 2 0 0 0 -2 -2h-6a2 2 0 0 0 -2 2v4" />
  <rect x="7" y="13" width="10" height="8" rx="2" />
</svg>
)";

#if 0
extern int LICE_RGBA_from_SVG2(const char* s,int len);

int LICE_RGBA_from_SVG2(const char* s,int len) {
    return LICE_RGBA_from_SVG(s, len);
}

extern "C" int LICE_RGBA_from_SVG(const char* s,int len) {
    return LICE_RGBA_from_SVG2(s, len);
}
#endif

WDL_HeapBuf* HeapBufFromString(std::string_view sv) {
    WDL_HeapBuf* buf = new WDL_HeapBuf();
    size_t n = sv.size();
    buf->Resize((int)n, false);
    memcpy(buf->Get(), sv.data(), n);
    return buf;
}

void* loadLvgFromString(std::string_view sv) {
    auto buf = HeapBufFromString(sv);
    auto pctx = ProjectCreateMemCtx(buf);
    void* p = LICE_LoadLVGFromContext(pctx, NULL, 0, 0);
    delete pctx;
    delete buf;
    return p;
}

static BITMAPINFO* dibinf = NULL;

static void CreateDib() {
    if (dibinf) {
        return;
    }
    dibinf = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + 12);
    dibinf->bmiHeader.biSize = sizeof(dibinf->bmiHeader);
    dibinf->bmiHeader.biPlanes = 1;
    dibinf->bmiHeader.biBitCount = 32;
    dibinf->bmiHeader.biCompression = BI_RGB;
    dibinf->bmiHeader.biXPelsPerMeter = 2834;
    dibinf->bmiHeader.biYPelsPerMeter = 2834;
    dibinf->bmiHeader.biClrUsed = 0;
    dibinf->bmiHeader.biClrImportant = 0;
    dibinf->bmiHeader.biClrUsed = 0;
}

static void DeleteDib() {
    free(dibinf);
    dibinf = nullptr;
}

static void BlitPixmap(HDC hdc, fz_pixmap* image, int panx, int pany) {
    CreateDib();

    int image_w = fz_pixmap_width(ctx, image);
    int image_h = fz_pixmap_height(ctx, image);
    int image_n = fz_pixmap_components(ctx, image);
    u8* samples = fz_pixmap_samples(ctx, image);

    dibinf->bmiHeader.biWidth = image_w;
    dibinf->bmiHeader.biHeight = -image_h;
    dibinf->bmiHeader.biSizeImage = image_h * 4;
    if (image_n == 4) {
        SetDIBitsToDevice(hdc, panx, pany, image_w, image_h, 0, 0, 0, image_h, samples, dibinf,
                          DIB_RGB_COLORS);
    } else if (image_n == 2) {
        size_t i = image_w * (size_t)image_h;
        u8* color = (u8*)malloc(i * 4);
        u8* s = samples;
        u8* d = color;
        for (; i > 0; i--) {
            d[2] = d[1] = d[0] = *s++;
            d[3] = *s++;
            d += 4;
        }
        SetDIBitsToDevice(hdc, panx, pany, image_w, image_h, 0, 0, 0, image_h, color, dibinf, DIB_RGB_COLORS);
        free(color);
    } else if (image_n == 3) {
        size_t i = image_w * (size_t)image_h;
        u8* color = (u8*)malloc(i * 4);
        u8* s = samples;
        u8* d = color;
        for (; i > 0; i--) {
            d[0] = *s++;
            d[1] = *s++;
            d[2] = *s++;
            d[3] = 0xff;
            d += 4;
        }
        SetDIBitsToDevice(hdc, panx, pany, image_w, image_h, 0, 0, 0, image_h, color, dibinf, DIB_RGB_COLORS);
        free(color);
    } else {
        CrashMe();
    }
}

static void Draw(HWND hwnd, HDC hdc) {
    RECT rc = GetClientRect(hwnd);

    int dx = RectDx(rc);
    int dy = RectDy(rc);
    if (!framebuffer->resize(dx, dy)) {
        dbglog("framebuffer->resize failed\n");
        AutoDeleteBrush brush(CreateSolidBrush(COL_GRAY));
        FillRect(hdc, &rc, brush);
        return;
    }
    auto bgCol = LICE_RGBA(0xc3, 0xc3, 0xc3, 255);
    LICE_Clear(framebuffer, bgCol);

#if 1
    LICE_IBitmap* bmpDest = new LICE_SubBitmap(framebuffer, 0, 0, 100, 100);
    // TODO: to avoid black background, need to modify lvgImageCtx::render()
    // and remove LICE_Clear
    LICE_RenderLVG(gLvg, 100, 100, bmpDest);
    delete bmpDest;
#else
    auto bm = LICE_RenderLVG(gLvg, 100, 100, nullptr);
    // alternative that renders to a temp bitmap
    RECT srcRect = {0, 0, 100, 100};
    int mode = LICE_BLIT_MODE_COPY | LICE_BLIT_USE_ALPHA;
    LICE_Blit(framebuffer, bmp, 10, 10, &srcRect, 1.f, mode);
    {
        srcRect = {0, 0, 24, 24};
        // mode = LICE_BLIT_MODE_COPY;
        LICE_Blit(framebuffer, gSvgPrinter, 120, 120, &srcRect, 1.f, mode);
    }
#endif

    int x = rc.left;
    int y = rc.top;
    BitBlt(hdc, x, y, dx, dy, framebuffer->getDC(), 0, 0, SRCCOPY);

    BlitPixmap(hdc, svgPrinterPixmap, 200, 200);
}

#if 0
//alpha parameter = const alpha (combined with source alpha if spcified)
void LICE_Blit(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, const RECT *srcrect, float alpha, int mode);
void LICE_Blit(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int srcx, int srcy, int srcw, int srch, float alpha, int mode);
#endif

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // dbglogf("msg: 0x%x, wp: %d, lp: %d\n", msg, (int)wp, (int)lp);

    switch (msg) {
        case WM_CREATE:
            // CreateMainLayout(hwnd);
            break;

        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            int currWinDx = RectDx(rect);
            int currWinDy = RectDy(rect);
            dbglogf("WM_SIZE: wp: %d, (%d,%d)\n", (int)wp, currWinDx, currWinDy);
            // doMainLayout();
            return 0;
            // return DefWindowProc(hwnd, msg, wp, lp);
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wp);
            switch (wmId) {
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;
                default:
                    return DefWindowProc(hwnd, msg, wp, lp);
            }
        } break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            Draw(hwnd, hdc);
            EndPaint(hwnd, &ps);
            // ValidateRect(hwnd, NULL);
        } break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}
static ATOM RegisterWinClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex{};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TESTWIN));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_TESTWIN);
    wcex.lpszClassName = WIN_CLASS;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

static BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;
    const WCHAR* cls = WIN_CLASS;

    DWORD dwExStyle = 0;
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    int dx = 640;
    int dy = 480;
    HWND hwnd = CreateWindowExW(dwExStyle, cls, L"Test lice", dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, dx, dy, nullptr,
                                nullptr, hInstance, nullptr);

    if (!hwnd) {
        return FALSE;
    }

    g_hwnd = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return TRUE;
}

static void fz_lock_context_cs(void* user, int lock) {
    EnterCriticalSection(&mutexes[lock]);
}

static void fz_unlock_context_cs(void* user, int lock) {
    LeaveCriticalSection(&mutexes[lock]);
}

static void createSvgPixmaps() {
    static bool didInit = false;
    if (didInit) {
        return;
    }
    didInit = true;
    for (int i = 0; i < FZ_LOCK_MAX; i++) {
        InitializeCriticalSection(&mutexes[i]);
    }

    fz_locks_ctx.user = nullptr;
    fz_locks_ctx.lock = fz_lock_context_cs;
    fz_locks_ctx.unlock = fz_unlock_context_cs;
    ctx = fz_new_context(nullptr, &fz_locks_ctx, FZ_STORE_DEFAULT);

    fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (u8*)svgPrinter, str::Len(svgPrinter));
    svgPrinterImage = fz_new_image_from_svg(ctx, buf, nullptr, nullptr);
    svgPrinterPixmap = fz_get_pixmap_from_image(ctx, svgPrinterImage, nullptr, nullptr, nullptr, nullptr);
    fz_drop_buffer(ctx, buf);
}

static void destroySvgPixmaps() {
    fz_drop_pixmap(ctx, svgPrinterPixmap);
    svgPrinterPixmap = nullptr;
    fz_drop_image(ctx, svgPrinterImage);
    svgPrinterImage = nullptr;
    fz_drop_context(ctx);
    ctx = nullptr;
}

extern LICE_IBitmap* LICE_LoadSVGFromBuffer(const char* buffer, int buflen, LICE_IBitmap* bmp);

int TestLice(HINSTANCE hInstance, int nCmdShow) {
    createSvgPixmaps();
    RegisterWinClass(hInstance);

    gLvg = loadLvgFromString(lvg01);

    gSvgPrinter = LICE_LoadSVGFromBuffer(svgPrinter, (int)str::Len(svgPrinter), nullptr);

    framebuffer = new LICE_SysBitmap(0, 0);
    if (!CreateMainWindow(hInstance, nCmdShow)) {
        CrashAlwaysIf(true);
        return FALSE;
    }
    HACCEL accelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TESTWIN));
    auto res = RunMessageLoop(accelTable, g_hwnd);
    delete framebuffer;
    LICE_DestroyLVG(gLvg);
    destroySvgPixmaps();
    DeleteDib();
    return res;
}
