/* Copyright 2010-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include <GdiPlus.h>

#include <shlobj.h>
#include <Tlhelp32.h>
#include <Shlwapi.h>
#include <objidl.h>
#include <io.h>

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

using namespace Gdiplus;

// define to 1 to enable shadow effect, to 0 to disable
#define DRAW_TEXT_SHADOW 1
#define DRAW_MSG_TEXT_SHADOW 0

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

static REAL DrawMessage(Graphics &g, TCHAR *msg, REAL y, REAL dx, Color color)
{
#if 0
    ScopedMem<WCHAR> s(str::conv::ToWStr(msg));

    Gdiplus::RectF maxbox(0, y, dx, 0);
    Gdiplus::RectF bbox;
    g.MeasureString(s, -1, &f, maxbox, &bbox);

    bbox.X += (dx - bbox.Width) / 2.f;
    StringFormat sft;
    sft.SetAlignment(StringAlignmentCenter);
    SolidBrush b(color);
    g.DrawString(s, -1, &f, bbox, &sft, &b);

    return bbox.Height;
#else
    return 0;
#endif
}

// A sample text we're laying out
const char *gTxt = "ClearType is dependent on the orientation and ordering of the LCD stripes. Currently,\nClearType is implemented only for vertical stripes that are ordered RGB. This might be a concern if you are\n using a tablet PC, where the display can be oriented in any direction, or if you are using a screen that\ncan be turned from landscape to portrait.\n\nThe following example draws text with two different quality settings:";

struct StringPos {
    StringPos() : s(NULL), len(0), x(0), y(0) {
    }
    StringPos(const char *s, size_t len, REAL x, REAL y) : s(s), len(len), x(x), y(y) {
    }
    const char *s;
    size_t len;
    REAL x,y;
};

class Page {
public:
    Page() : dx(0), dy(0), strings(NULL) {
    }
    Page(int dx, int dy) : dx(dx), dy(dy) {
        strings = new Vec<StringPos>();
    }
    ~Page() {
        delete strings;
    }
    int dx, dy; // used during layout
    Vec<StringPos> *strings;
};

Vec<Page*> *gPages;

static void Utf8ToWchar(const char *s, size_t sLen, WCHAR *bufOut, size_t bufOutMax)
{
    // TODO: clearly, write proper utf8 => wchar, check we don't exceed bufOutMax
    for (size_t i = 0; i < sLen; i++) {
        bufOut[i] = *s++;
    }
    bufOut[sLen] = 0;
}

struct WordInfo {
    const char *s;
    size_t len;
    bool IsNewline() {
        return ((len == 1) && (s[0] == '\n'));
    }
};

class WordsIter {
public:
    WordsIter(const char *s) : s(s) {
        Reset();
    }

    void Reset() {
        curr = s;
        len = strlen(s);
        left = len;
    }

    WordInfo *Next();

private:
    WordInfo wi;

    static const char *NewLineStr;
    const char *s;
    size_t len;

    const char *curr;
    size_t left;
};

const char *WordsIter::NewLineStr = "\n";

static void SkipCharInStr(const char *& s, size_t& left, char c)
{
    while ((left > 0) && (*s == c)) {
        ++s; --left;
    }
}

static bool IsWordBreak(char c)
{
    return (c == ' ') || (c == '\n') || (c == '\r');
}

static void SkipNonWordBreak(const char *& s, size_t& left)
{
    while ((left > 0) && !IsWordBreak(*s)) {
        ++s; --left;
    }
}

// return true if s points to "\n", "\n\r", "\r" or "\r\n"
// and advance s/left to skip it
// We don't want to collapse multiple consequitive newlines into
// one as we want to be able to detect paragraph breaks (i.e. empty
// newlines i.e. a newline following another newline)
static bool IsNewlineSkip(const char *& s, size_t& left)
{
    if (0 == left)
        return false;
    if ('\r' == *s) {
        --left; ++s;
        if ((left > 0) && ('\n' == *s)) {
            --left; ++s;
        }
        return true;
    } else if ('\n' == *s) {
        --left; ++s;
        if ((left > 0) && ('\r' == *s)) {
            --left; ++s;
        }
        return true;
    }
    return false;
}

// iterates words in a string e.g. "foo bar\n" returns "foo", "bar" and "\n"
// also unifies line endings i.e. "\r" an "\n\r" are turned into a single "\n"
// returning NULL means end of iterations
WordInfo *WordsIter::Next()
{
    SkipCharInStr(curr, left, ' ');
    if (0 == left)
        return NULL;
    assert(*curr != 0);
    if (IsNewlineSkip(curr, left)) {
        wi.len = 0;
        wi.s = NewLineStr;
        return &wi;
    }
    wi.s = curr;
    SkipNonWordBreak(curr, left);
    wi.len = curr - wi.s;
    assert(wi.len > 0);
    return &wi;
}

static Vec<Page*> *LayoutText(Graphics *g, Font *f, int pageDx, int pageDy, const char *s)
{
    Vec<Page*> *pages = new Vec<Page*>();
    WCHAR buf[512];
    PointF pointZero(0, 0);
    RectF bb;
    g->MeasureString(L" ", 1, f, pointZero, &bb);
    REAL spaceDx = bb.Width;
    //FontFamily ff;
    //f->GetFamily(&ff);
    //REAL lineSpacing = (REAL)ff.GetLineSpacing(f->GetStyle()); // TODO: why does it return ridiculusly big numbe?
    REAL lineSpacing = f->GetHeight(g);
    WordsIter iter(s);
    WordInfo *wi;
    for (;;) {
        // layout a single page
        Page *page = new Page(pageDx, pageDy);
        REAL dyLeft = (REAL)pageDy;
        REAL y = 0;
        while (dyLeft > lineSpacing) {
            // layout a single line
            REAL x = 0;
            for (;;) {
                wi = iter.Next();
                if (NULL == wi)
                    break;
                Utf8ToWchar(wi->s, wi->len, buf, dimof(buf));
                g->MeasureString(buf, wi->len, f, pointZero, &bb);
                StringPos sp(wi->s, wi->len, x, y);
                x += bb.Width;
                page->strings->Append(sp);
                x += spaceDx;
                if (x > pageDx)
                    break;
            }
            if (NULL == wi)
                break;
            dyLeft -= lineSpacing;
            y += lineSpacing;
        }
        pages->Append(page);
        if (NULL == wi)
            break;
    }
    return pages;
}

static void DrawPage(Graphics *g, Font *f, int pageNo, REAL offX, REAL offY)
{
    SolidBrush br(Color(0,0,0)); // black
    Page *p = gPages->At(pageNo);
    size_t n = p->strings->Count();
    WCHAR buf[512];
    for (size_t i = 0; i < n; i++) {
        StringPos sp = p->strings->At(i);
        PointF o(sp.x + offX, sp.y + offY);
        Utf8ToWchar(sp.s, sp.len, buf, dimof(buf));
        g->DrawString(buf, sp.len, f, o, NULL, &br);
    }
}

static void DrawFrame2(Graphics &g, RectI r)
{
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    const int pageBorderX = 10;
    const int pageBorderY = 20;
    if (!gPages) {
        int pageDx = r.dx - (pageBorderX * 2);
        int pageDy = r.dy - (pageBorderY * 2);
        gPages = LayoutText(&g, gFont, pageDx, pageDy , gTxt);
    }

    //SolidBrush bgBrush(gColBg);
    Gdiplus::Rect r2(r.y - 1, r.x - 1, r.dx + 1, r.dy + 1);
    LinearGradientBrush bgBrush(RectF(0, 0, (REAL)r.dx, (REAL)r.dy), Color(0xd0,0xd0,0xd0), Color(0xff,0xff,0xff), LinearGradientModeVertical);
    g.FillRectangle(&bgBrush, r2);

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
    gFont = ::new Font(L"Times New Roman", 16, FontStyleRegular);
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

    DeleteVecMembers<Page*>(*gPages);
    delete gPages;
    ::delete gFont;
    ::delete frameBmp;

Exit:
    return ret;
}
