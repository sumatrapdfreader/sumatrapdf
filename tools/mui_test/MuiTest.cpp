/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: BSD */

#include "BaseUtil.h"

#include "FileUtil.h"
#include "Resource.h"
#include "WinUtil.h"

#include "DebugLog.h"

using namespace Gdiplus;

// point is a pixel * dpi factor, where 1 pixel == 1 point at 72 dpi
// we use a typedef to make it clear which values are in points
typedef float SizeInPoint;

#define WIN_CLASS_NAME    L"MUI_TEST_FRAME"

#define TEN_SECONDS_IN_MS 10*1000

// it's ARGB, the same format as Gdiplus::Color::ARGB
typedef uint32_t GfxCol;

GfxCol MakeGfxCol(int r, int g, int b)
{
    return Color::MakeARGB(255, r, g, b);
}

GfxCol MakeGfxCol(int a, int r, int g, int b)
{
    return Color::MakeARGB(a, r, g, b);
}

GfxCol COL_WHITE = MakeGfxCol(0xff, 0xff, 0xff);
GfxCol COL_BLACK = MakeGfxCol(0, 0, 0);
GfxCol COL_GRAY = MakeGfxCol(0xcc, 0xcc, 0xcc);

inline Gdiplus::ARGB GfxColToARGB(GfxCol c)
{
    // they are the same format
    return c;
}

struct GfxPoint
{
    int x, y;
};

struct GfxSize
{
    int dx, dy;

    GfxSize() { dx = 0; dy = 0; }
    GfxSize(int dx, int dy) : dx(dx), dy(dy) { }

    void Set(int dx2, int dy2) {
        dx = dx2;
        dy = dy2;
    }

    void operator=(const GfxSize& other) {
        dx = (float)other.dx;
        dy = (float)other.dy;
    }
};

struct GfxSizeF
{
    float dx, dy;

    GfxSizeF() { dx = 0; dy = 0; }

    void operator=(const GfxSizeF& other) {
        dx = other.dx;
        dy = other.dy;
    }

    void operator=(const GfxSize& other) {
        dx = (float)other.dx;
        dy = (float)other.dy;
    }
};

struct GfxRect
{
    int x, y, dx, dy;

    GfxRect() { x = 0; y = 0; dx = 0; dy = 0; }

    GfxRect(const GfxSize& s) {
        x = 0; y = 0;
        dx = s.dx;
        dy = s.dy;
    }

    GfxRect(const GfxRect& other) {
        x = other.x;
        y = other.y;
        dx = other.dx;
        dy = other.dy;
    }

    void Inflate(int n)
    {
        x -= n;
        y -= n;
        dx += (n*2);
        dy += (n*2);
    }

    void Inflate(int left, int top, int right, int bottom)
    {
        x += left;
        y += top;
        dx -= left;
        dx += right;
        dy -= top;
        dy += bottom;
    }
};

class Gfx
{
public:
    GfxSize     size;
    GfxSizeF    sizeF;

    Gfx() {}
    virtual ~Gfx() {}
    virtual void DrawRect(const GfxRect& r, GfxCol col, float width) = 0;
    virtual void DrawFilledRect(const GfxRect& r, GfxCol col) = 0;
    void SetSize(int dx, int dy);
};

void Gfx::SetSize(int dx, int dy)
{
    size.Set(dx, dy);
    sizeF = size;
}

// set consistent mode for our graphics objects so that we get
// the same results when measuring text
void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);
    //g->SetPixelOffsetMode(PixelOffsetModeNone);
    g->SetPixelOffsetMode(PixelOffsetModeHalf);
}

class GdiplusGfx : public Gfx
{
    Gdiplus::Graphics *g;
    HDC dc;

public:
    GdiplusGfx(HDC dc) {
        g = Gdiplus::Graphics::FromHDC(dc);
        InitGraphicsMode(g);
        PixelOffsetMode pm = g->GetPixelOffsetMode();
        plogf("pm = %d", (int)pm);
    }
    virtual ~GdiplusGfx() {
        delete g;
    }
    virtual void DrawRect(const GfxRect& r, GfxCol col, float width);
    virtual void DrawFilledRect(const GfxRect& r, GfxCol col);
};

void GdiplusGfx::DrawFilledRect(const GfxRect& r, GfxCol col)
{
    SolidBrush br(GfxColToARGB(col));
#if 0
    float x = (float)r.x - 0.5f;
    float y = (float)r.y - 0.5f;
#else
    float x = (float)r.x;
    float y = (float)r.y;
#endif
    Gdiplus::RectF r2(x, y, r.dx, r.dy);
    g->FillRectangle(&br, r2);
}

void GdiplusGfx::DrawRect(const GfxRect& r, GfxCol col, float width)
{
    Pen p(Color(GfxColToARGB(col)), width);
    PenAlignment a = p.GetAlignment();
    plogf("a = %d", (int)a);
    p.SetAlignment(PenAlignmentInset);
#if 1
    float x = (float)r.x - 0.5f;
    float y = (float)r.y - 0.5f;
#else
    float x = (float)r.x;
    float y = (float)r.y;
#endif
    float dx = (float)r.dx;
    float dy = (float)r.dy;
    Gdiplus::RectF r2(x, y, dx, dy);
    g->DrawRectangle(&p, r2);
}

Gfx *CreateGdiplusGfx(HDC dc)
{
    return new GdiplusGfx(dc);
}

class Painter
{
    Gfx *gfx;
public:
    void OnWmPaint(HWND hwnd);
};

void Painter::OnWmPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);

    gfx = CreateGdiplusGfx(dc);
    ClientRect cr(hwnd);
    gfx->SetSize(cr.dx, cr.dy); // TODO: get it from HDC and set during creation

    GfxRect r = gfx->size;
    gfx->DrawFilledRect(r, COL_WHITE);

    r.Inflate(1, 3, 0, -2);
    gfx->DrawFilledRect(r, COL_GRAY);
    r.Inflate(-1);
    gfx->DrawRect(r, COL_BLACK, 1.f);

    delete gfx;

    EndPaint(hwnd, &ps);
}

class EventHandler;
void RegisterEventHandler(EventHandler* h);
void UnregisterEventHandler(EventHandler* h);

class EventHandler
{
public:
    HWND hwnd;

    Painter *painter;

    EventHandler(HWND hwnd) : hwnd(hwnd) {
        painter = new Painter;
        RegisterEventHandler(this);
    }

    ~EventHandler() {
        hwnd = 0;
        UnregisterEventHandler(this);
    }

    LRESULT HandleMsg(UINT msg, WPARAM wparam, LPARAM lparam, bool& handled);
};

Vec<EventHandler*> allEventHandlers;

void RegisterEventHandler(EventHandler* h)
{
    allEventHandlers.Append(h);
}

void UnregisterEventHandler(EventHandler* h)
{
    bool removed = allEventHandlers.Remove(h);
    CrashIf(!removed);
    if (0 == allEventHandlers.Count())
        PostQuitMessage(0);
}

EventHandler *FindEventHandler(HWND hwnd)
{
    for (size_t i = 0; i < allEventHandlers.Count(); i++) {
        EventHandler *h = allEventHandlers.At(i);
        if (hwnd == h->hwnd)
            return h;
    }
    return NULL;
}

LRESULT EventHandler::HandleMsg(UINT msg, WPARAM wparam, LPARAM lparam, bool& handled)
{
    handled = false;

    if (WM_DESTROY == msg) {
        // TODO: maybe just schedule ourselves for deletion during out
        // loop processing
        handled = true;
        delete this;
    } else if (WM_CLOSE == msg) {
        handled = true;
        DestroyWindow(hwnd);
    } else if (painter && (WM_PAINT == msg)) {
        handled = true;
        painter->OnWmPaint(hwnd);
    }

    return 0;
}

// TODO: this is just for curiosity. Remove.
bool MsgCanHappenBeforeRegister(UINT msg)
{
    static UINT messages[] = { WM_CREATE, WM_NCCREATE, WM_GETMINMAXINFO, WM_NCCALCSIZE, WM_NCDESTROY };
    for (int i = 0; i < sizeof(messages); i++) {
        if (msg == messages[i])
            return true;
    }
    return false;
}

static LRESULT CALLBACK WndProcEventHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    EventHandler *h = FindEventHandler(hwnd);
    if (!h) {
        CrashIf(!MsgCanHappenBeforeRegister(msg));
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    CrashIf(hwnd != h->hwnd);
    bool handled;
    LRESULT res = h->HandleMsg(msg, wParam, lParam, handled);
    if (handled)
        return res;

    return DefWindowProc(hwnd, msg, wParam, lParam);

#if 0
    switch (msg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_KEYDOWN:
            return OnKeyDown(hwnd, msg, wParam, lParam);
        case WM_COMMAND:
            OnCommand(hwnd, msg, wParam, lParam);
            break;
        default:
    }
    return 0;
#endif
}

static void RegisterWinClass()
{
    static bool registered = false;
    WNDCLASSEX  wcex = { 0 };

    if (registered)
        return;

    registered = true;
    FillWndClassEx(wcex, NULL, WIN_CLASS_NAME, WndProcEventHandler);
    // clear out CS_HREDRAW | CS_VREDRAW style so that resizing doesn't
    // cause the whole window redraw
    wcex.style = 0;
    ATOM atom = RegisterClassEx(&wcex);
    CrashIf(!atom);
}

EventHandler *EvCreateWindow(SizeInPoint dxp, SizeInPoint dyp, const char *title)
{
    ScopedMem<WCHAR> t(str::conv::FromUtf8(title));
    int dx = win::GlobalDpiAdjust(dxp);
    int dy = win::GlobalDpiAdjust(dyp);
    RegisterWinClass();

    HWND hwnd = CreateWindow(
            WIN_CLASS_NAME, L"Mui Test",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            dx, dy,
            NULL, NULL,
            NULL, NULL);
    CrashIf(!hwnd);
    EventHandler *h = new EventHandler(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    return h;
}

static int RunMessageLoop()
{
    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

int RunApp()
{
    EvCreateWindow(640, 480, "hello");
    int res = RunMessageLoop();
    return res;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef DEBUG
    // report memory leaks on DbgOut
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
    // ensure that C functions behave consistently under all OS locales
    // (use Win32 functions where localized input or output is desired)
    setlocale(LC_ALL, "C");

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    int res = RunApp();
    return res;
}
