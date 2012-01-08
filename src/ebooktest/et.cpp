/* Copyright 2010-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "Resource.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "Version.h"
#include "Vec.h"
#include "CmdLineParser.h"
#include "Transactions.h"
#include "Scopes.h"
#include "PageLayout.h"
#include "MobiParse.h"
#include "MobiHtmlParse.h"

using namespace Gdiplus;

#define ET_FRAME_CLASS_NAME    _T("ET_FRAME")

#define WIN_DX    640
#define WIN_DY    480

static HINSTANCE        ghinst;
static HWND             gHwndFrame = NULL;

static HFONT            gFontDefault;

Color gCol1(196, 64, 50); Color gCol1Shadow(134, 48, 39);
Color gCol2(227, 107, 35); Color gCol2Shadow(155, 77, 31);
Color gCol3(93,  160, 40); Color gCol3Shadow(51, 87, 39);
Color gCol4(69, 132, 190); Color gCol4Shadow(47, 89, 127);
Color gCol5(112, 115, 207); Color gCol5Shadow(66, 71, 118);

//Color gColBg(0xff, 0xf2, 0); // this is yellow
Color gColBg(0xe9, 0xe9, 0xe9); // this is darkish gray
Color gColBgTop(0xfa, 0xfa, 0xfa); // this is lightish gray

#define TEN_SECONDS_IN_MS 10*1000

static HFONT CreateDefaultGuiFont()
{
    HDC hdc = GetDC(NULL);
    HFONT font = GetSimpleFont(hdc, _T("MS Shell Dlg"), 14);
    ReleaseDC(NULL, hdc);
    return font;
}

static float gUiDPIFactor = 1.0f;

inline int dpiAdjust(int value)
{
    return (int)(value * gUiDPIFactor);
}

static void InvalidateFrame()
{
    ClientRect rc(gHwndFrame);
    InvalidateRect(gHwndFrame, &rc.ToRECT(), FALSE);
}

static void OnExit()
{
    SendMessage(gHwndFrame, WM_CLOSE, 0, 0);
}

inline void EnableAndShow(HWND hwnd, bool enable)
{
    ShowWindow(hwnd, enable ? SW_SHOW : SW_HIDE);
    EnableWindow(hwnd, enable);
}

class FrameTimeoutCalculator {

    LARGE_INTEGER   timeStart;
    LARGE_INTEGER   timeLast;
    LONGLONG        ticksPerFrame;
    LONGLONG        ticsPerMs;
    LARGE_INTEGER   timeFreq;

public:
    FrameTimeoutCalculator(int framesPerSecond) {
        QueryPerformanceFrequency(&timeFreq); // number of ticks per second
        ticsPerMs = timeFreq.QuadPart / 1000;
        ticksPerFrame = timeFreq.QuadPart / framesPerSecond;
        QueryPerformanceCounter(&timeStart);
        timeLast = timeStart;
    }

    // in seconds, as a double
    double ElapsedTotal() {
        LARGE_INTEGER timeCurr;
        QueryPerformanceCounter(&timeCurr);
        LONGLONG elapsedTicks =  timeCurr.QuadPart - timeStart.QuadPart;
        double res = (double)elapsedTicks / (double)timeFreq.QuadPart;
        return res;
    }

    DWORD GetTimeoutInMilliseconds() {
        LARGE_INTEGER timeCurr;
        LONGLONG elapsedTicks;
        QueryPerformanceCounter(&timeCurr);
        elapsedTicks = timeCurr.QuadPart - timeLast.QuadPart;
        if (elapsedTicks > ticksPerFrame) {
            return 0;
        } else {
            LONGLONG timeoutMs = (ticksPerFrame - elapsedTicks) / ticsPerMs;
            return (DWORD)timeoutMs;
        }
    }

    void Step() {
        timeLast.QuadPart += ticksPerFrame;
    }
};

static Font *gFont = NULL;

// A sample text we're laying out
const char *gTxt = "ClearType is dependent on the orientation and ordering of the LCD stripes. Currently,\nClearType is implemented only for vertical stripes that are ordered RGB. This might be a concern if you are\n using a tablet PC, where the display can be oriented in any direction, or if you are using a screen that\ncan be turned from landscape to portrait.\n\nThe following example draws text with two different quality settings:";

Vec<Page*> *gPages;

Vec<Page*> *LayoutText(Graphics *gfx, Font *defaultFont, int pageDx, int pageDy, const char *s)
{
    PageLayout layouter(pageDx, pageDy);
    return layouter.LayoutText(gfx, defaultFont, s);
}

Vec<Page*> *LayoutMobiFile(const TCHAR *file, Graphics *gfx, Font *defaultFont, int pageDx, int pageDy)
{
    MobiParse *mb = MobiParse::ParseFile(file);
    if (!mb)
        return NULL;

    size_t sLen;
    char *s = mb->GetBookHtmlData(sLen);
    size_t forLayoutLen;
    uint8_t *forLayout = MobiHtmlToDisplay((uint8_t*)s, sLen, forLayoutLen);
    PageLayout layouter(pageDx, pageDy);
    Vec<Page*> *pages = layouter.LayoutInternal(gfx, defaultFont, forLayout, forLayoutLen);
    //TODO: leaking forLayout data, we need to preserve it for display since
    //pages data point to it
    //free(forLayout);
    delete mb;
    return pages;
}

static bool gShowTextBoundingBoxes = true;

static void DrawPage(Graphics *g, Font *f, int pageNo, REAL offX, REAL offY)
{
    StringFormat sf(StringFormat::GenericTypographic());
    SolidBrush br(Color(0,0,0));
    SolidBrush br2(Color(255, 255, 255, 255));
    Pen pen(Color(255, 0, 0), 1);
    Page *p = gPages->At(pageNo);
    size_t n = p->strings->Count();
    WCHAR buf[512];
    PointF pos;
    for (size_t i = 0; i < n; i++) {
        StringPos sp = p->strings->At(i);
        RectF bbox = sp.bbox;
        bbox.X += offX;
        bbox.Y += offY;
        str::Utf8ToWcharBuf(sp.s, sp.len, buf, dimof(buf));
        bbox.GetLocation(&pos);
        if (gShowTextBoundingBoxes) {
            //g->FillRectangle(&br, bbox);
            g->DrawRectangle(&pen, bbox);
        }
        g->DrawString(buf, sp.len, f, pos, NULL, &br);
    }
}

const int pageBorderX = 10;
const int pageBorderY = 10;

static void LayoutTestText(Graphics* gfx, RectI r)
{
    int pageDx = r.dx - (pageBorderX * 2);
    int pageDy = r.dy - (pageBorderY * 2);

    TCHAR *testFile = _T("docs\\KafkaTrial.mobi");
    if (file::Exists(testFile))
        gPages = LayoutMobiFile(testFile, gfx, gFont, pageDx, pageDy);

    if (!gPages) // in case LayoutMobiFile() failed
        gPages = LayoutText(gfx, gFont, pageDx, pageDy , gTxt);
}

static void DrawFrame2(Graphics &g, RectI r)
{
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetPageUnit(UnitPixel);

    if (!gPages) {
        LayoutTestText(&g, r);
    }

    //SolidBrush bgBrush(gColBg);
    Gdiplus::Rect r2(r.x-1, r.y-1, r.dx+2, r.dy+2);
    LinearGradientBrush bgBrush(RectF(0, 0, (REAL)r.dx, (REAL)r.dy), Color(0xd0,0xd0,0xd0), Color(0xff,0xff,0xff), LinearGradientModeVertical);
    g.FillRectangle(&bgBrush, r2);

    if (gShowTextBoundingBoxes) {
        Pen p(Color(0,0,255), 1);
        g.DrawRectangle(&p, pageBorderX, pageBorderY, r.dx - (pageBorderX * 2), r.dy - (pageBorderY * 2));
    }
    DrawPage(&g, gFont, 0, (REAL)pageBorderX, (REAL)pageBorderY);
}

static bool BitmapSizeEquals(Bitmap *bmp, int dx, int dy)
{
    if (NULL == bmp)
        return false;
    return ((dx == bmp->GetWidth()) && (dy == bmp->GetHeight()));
}

static Bitmap *frameBmp = NULL;

static void DrawFrame(HWND hwnd, HDC dc, PAINTSTRUCT *ps)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    Graphics g(dc);
    ClientRect rc2(hwnd);
    if (!BitmapSizeEquals(frameBmp, rc2.dx, rc2.dy)) {
        delete frameBmp;
        frameBmp = ::new Bitmap(rc2.dx, rc2.dy, &g);
    }
    Graphics g2((Image*)frameBmp);
    DrawFrame2(g2, rc2);
    g.DrawImage(frameBmp, 0, 0);
}

static void OnPaintFrame(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    DrawFrame(hwnd, dc, &ps);
    EndPaint(hwnd, &ps);
}

static void OnLButtonDown()
{
}

static void OnCreateWindow(HWND hwnd)
{
    //gFont = ::new Font(L"Times New Roman", 16, FontStyleRegular);
    //HDC dc = GetDC(hwnd);
    //gFont = ::new Font(dc, gFontDefault); 
    gFont = ::new Font(L"Georgia", 16, FontStyleRegular);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
            OnCreateWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT:
            OnPaintFrame(hwnd);
            break;

        case WM_LBUTTONDOWN:
            OnLButtonDown();
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDCANCEL:
                    OnExit();
                    break;

                default:
                    return DefWindowProc(hwnd, message, wParam, lParam);
            }
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static BOOL RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEX  wcex;

    FillWndClassEx(wcex, hInstance);
    wcex.lpszClassName  = ET_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.lpfnWndProc    = WndProcFrame;

    ATOM atom = RegisterClassEx(&wcex);
    return atom != NULL;
}

static BOOL InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;
    gFontDefault = CreateDefaultGuiFont();
    win::GetHwndDpi(NULL, &gUiDPIFactor);

    gHwndFrame = CreateWindow(
            ET_FRAME_CLASS_NAME, _T("Ebook Test ") CURR_VERSION_STR,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            dpiAdjust(WIN_DX), dpiAdjust(WIN_DY),
            NULL, NULL,
            ghinst, NULL);

    if (!gHwndFrame)
        return FALSE;

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
static int RunApp()
{
    MSG msg;
    FrameTimeoutCalculator ftc(60);
    MillisecondTimer t;
    t.Start();
    for (;;) {
        const DWORD timeout = ftc.GetTimeoutInMilliseconds();
        DWORD res = WAIT_TIMEOUT;
        if (timeout > 0) {
            res = MsgWaitForMultipleObjects(0, 0, TRUE, timeout, QS_ALLEVENTS);
        }
        if (res == WAIT_TIMEOUT) {
            //AnimStep();
            ftc.Step();
        }

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return (int)msg.wParam;
            }
            if (!IsDialogMessage(gHwndFrame, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int ret = 1;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    //ParseCommandLine(GetCommandLine());
    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    ret = RunApp();

    if (gPages)
        DeleteVecMembers<Page*>(*gPages);
    delete gPages;
    ::delete gFont;
    ::delete frameBmp;

Exit:
    return ret;
}
