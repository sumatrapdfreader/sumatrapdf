/* Copyright 2010-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "Resource.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "Version.h"
#include "Vec.h"
#include "mui.h"
#include "CmdLineParser.h"
#include "FrameTimeoutCalculator.h"
#include "Transactions.h"
#include "Scopes.h"
#include "PageLayout.h"
#include "MobiParse.h"
#include "EbookTestMenu.h"

using namespace Gdiplus;
using namespace mui;

class VirtWndEbook;

#define ET_FRAME_CLASS_NAME    _T("ET_FRAME")

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12

#define WIN_DX    640
#define WIN_DY    480

static HINSTANCE        ghinst;
static HWND             gHwndFrame = NULL;
static VirtWndEbook *   gVirtWndFrame = NULL;

static bool gShowTextBoundingBoxes = false;

Color gCol1(196, 64, 50); Color gCol1Shadow(134, 48, 39);
Color gCol2(227, 107, 35); Color gCol2Shadow(155, 77, 31);
Color gCol3(93,  160, 40); Color gCol3Shadow(51, 87, 39);
Color gCol4(69, 132, 190); Color gCol4Shadow(47, 89, 127);
Color gCol5(112, 115, 207); Color gCol5Shadow(66, 71, 118);

//Color gColBg(0xff, 0xf2, 0); // this is yellow
Color gColBg(0xe9, 0xe9, 0xe9); // this is darkish gray
Color gColBgTop(0xfa, 0xfa, 0xfa); // this is lightish gray

// A sample text to display if we don't show an actual mobi file
static const char *gSampleHtml = "<html><p align=justify>ClearType is <b>dependent</b> on the <i>orientation and ordering</i> of the LCD stripes.</p> <p align='right'><em>Currently</em>, ClearType is implemented <hr> only for vertical stripes that are ordered RGB.</p> <p align=center>This might be a concern if you are using a tablet PC.</p> <p>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The <strike>following example</strike> draws text with two <u>different quality</u> settings.</p> On to the <b>next<mbp:pagebreak>page</b></html>";

class EbookLayout : public Layout
{
public:
    EbookLayout(VirtWndButton *next, VirtWndButton *prev) :
      next(next), prev(prev)
    {
    }

    virtual ~EbookLayout() {
    }

    VirtWndButton *next;
    VirtWndButton *prev;

    virtual void Measure(Size availableSize, VirtWnd *wnd);
    virtual void Arrange(Rect finalRect, VirtWnd *wnd);
};

void EbookLayout::Measure(Size availableSize, VirtWnd *wnd)
{
    wnd->desiredSize = availableSize;
    next->Measure(availableSize);
    prev->Measure(availableSize);
}

void EbookLayout::Arrange(Rect finalRect, VirtWnd *wnd)
{
    int btnDx, btnDy, btnY, btnX;

    int rectDy = finalRect.Height;
    int rectDx = finalRect.Width;

    // prev is on the left size
    btnDy = prev->desiredSize.Height;
    btnY = (rectDy - btnDy) / 2;
    if (btnY < 0)
        btnY = 0;

    Rect prevPos(0, btnY, prev->desiredSize.Width, btnDy);
    prev->Arrange(prevPos);

    // next is on the right size
    btnDy = next->desiredSize.Height;
    btnY = (rectDy - btnDy) / 2;
    if (btnY < 0)
        btnY = 0;

    btnDx = next->desiredSize.Width;
    btnX = rectDx - btnDx;
    Rect nextPos(btnX, btnY, btnDx, btnDy);
    next->Arrange(nextPos);

    wnd->pos = finalRect;
}

class VirtWndEbook : public VirtWndHwnd, public IClickHandler
{
    void DoPageLayout(int dx, int dy);
    void AdvancePage(int dist);
public:
    MobiParse *     mb;
    const char *    html;
    PageLayout *    pageLayout;
    int             currPageNo;

    VirtWndButton * next;
    VirtWndButton * prev;

    VirtWndEbook(HWND hwnd);

    virtual ~VirtWndEbook() {
        delete mb;
        delete pageLayout;
    }

    virtual void Measure(Size availableSize) {
        layout->Measure(availableSize, this);
        desiredSize = availableSize;
    }

    virtual void Arrange(Rect finalRect) {
        layout->Arrange(finalRect, this);
        pos = finalRect;
        DoPageLayout(pos.Width, pos.Height);
    }

    virtual void Paint(Graphics *gfx, int offX, int offY);

    // IClickHandler
    virtual void Clicked(VirtWnd *w);

    void SetHtml(const char *html);
    void LoadMobi(const TCHAR *fileName);
};

void VirtWndEbook::AdvancePage(int dist)
{
    if (!pageLayout)
        return;
    int newPageNo = currPageNo + dist;
    if (newPageNo < 1)
        return;
    if (newPageNo > (int)pageLayout->PageCount())
        return;
    currPageNo = newPageNo;
    RequestRepaint(this);
}

PageLayout *LayoutHtmlOrMobi(const char *html, MobiParse *mb, int dx, int dy)
{
    PageLayout *layout = new PageLayout(dx, dy);
    size_t len;
    if (html)
        len = strlen(html);
    else
        html = mb->GetBookHtmlData(len);

    bool ok = layout->LayoutHtml(FONT_NAME, FONT_SIZE, html, len);
    if (ok)
        return layout;

    delete layout;
    return NULL;
}

void VirtWndEbook::DoPageLayout(int dx, int dy)
{
    if (pageLayout && (pageLayout->pageDx == dx) && (pageLayout->pageDy == dy))
        return;

    PageLayout *newLayout = LayoutHtmlOrMobi(html, mb, dx, dy);
    if (!newLayout)
        return;
    delete pageLayout;
    pageLayout = newLayout;
    currPageNo = 1;
    RequestRepaint(this);
}

void VirtWndEbook::SetHtml(const char *html)
{
    this->html = html;
}

void VirtWndEbook::LoadMobi(const TCHAR *fileName)
{
    mb = MobiParse::ParseFile(fileName);
    if (!mb)
        return;
    html = NULL;
    delete pageLayout;
    pageLayout = NULL;
}

VirtWndEbook::VirtWndEbook(HWND hwnd)
{
    mb = NULL;
    html = NULL;
    pageLayout = NULL;
    currPageNo = 0;
    SetHwnd(hwnd);
    next = new VirtWndButton(_T("Next"));
    prev = new VirtWndButton(_T("Prev"));
    AddChild(next);
    AddChild(prev);
    layout = new EbookLayout(next, prev);
    RegisterForClickEvent(next, this);
    RegisterForClickEvent(prev, this);
}

void VirtWndEbook::Clicked(VirtWnd *w)
{
    if (w == next) {
        AdvancePage(1);
    } else if (w == prev) {
        AdvancePage(-1);
    } else {
        CrashAlwaysIf(true);
    }
}

#define TEN_SECONDS_IN_MS 10*1000

static float gUiDPIFactor = 1.0f;
inline int dpiAdjust(int value)
{
    return (int)(value * gUiDPIFactor);
}

static void OnExit()
{
    SendMessage(gHwndFrame, WM_CLOSE, 0, 0);
}

static inline void EnableAndShow(HWND hwnd, bool enable)
{
    ShowWindow(hwnd, enable ? SW_SHOW : SW_HIDE);
    EnableWindow(hwnd, enable);
}

static void DrawPageLayout(Graphics *g, PageLayout *pg, int pageNo, REAL offX, REAL offY)
{
    StringFormat sf(StringFormat::GenericTypographic());
    SolidBrush br(Color(0,0,0));
    SolidBrush br2(Color(255, 255, 255, 255));
    Pen pen(Color(255, 0, 0), 1);
    Pen blackPen(Color(0, 0, 0), 1);

    Font *font = pg->GetFontByIdx(0);

    WCHAR buf[512];
    PointF pos;
    DrawInstr *end;
    DrawInstr *currInstr = pg->GetInstructionsForPage(pageNo, end);
    while (currInstr < end) {
        RectF bbox = currInstr->bbox;
        bbox.X += offX;
        bbox.Y += offY;
        if (InstrTypeLine == currInstr->type) {
            // hr is a line drawn in the middle of bounding box
            REAL y = bbox.Y + bbox.Height / 2.f;
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            if (gShowTextBoundingBoxes) {
                //g->FillRectangle(&br, bbox);
                g->DrawRectangle(&pen, bbox);
            }
            g->DrawLine(&blackPen, p1, p2);
        } else if (InstrTypeString == currInstr->type) {
            size_t strLen = str::Utf8ToWcharBuf((const char*)currInstr->str.s, currInstr->str.len, buf, dimof(buf));
            bbox.GetLocation(&pos);
            if (gShowTextBoundingBoxes) {
                //g->FillRectangle(&br, bbox);
                g->DrawRectangle(&pen, bbox);
            }
            g->DrawString(buf, strLen, font, pos, NULL, &br);
        } else if (InstrTypeSetFont == currInstr->type) {
            font = pg->GetFontByIdx(currInstr->setFont.fontIdx);
        }
        ++currInstr;
    }
}

void VirtWndEbook::Paint(Graphics *gfx, int offX, int offY)
{
    if (!pageLayout)
        return;
    DrawPageLayout(gfx, pageLayout, currPageNo - 1, (REAL)offX, (REAL)offY);
}

static void UpdatePageCount()
{
    size_t pagesCount = gVirtWndFrame->pageLayout->PageCount();
    ScopedMem<TCHAR> s(str::Format(_T("%d pages"), (int)pagesCount));
    win::SetText(gHwndFrame, s.Get());
}

const int pageBorderX = 10;
const int pageBorderY = 10;

#if 0
static void DrawFrame2(Graphics &g, RectI r)
{
    DrawPage(&g, 0, (REAL)pageBorderX, (REAL)pageBorderY);
    if (gShowTextBoundingBoxes) {
        Pen p(Color(0,0,255), 1);
        g.DrawRectangle(&p, pageBorderX, pageBorderY, r.dx - (pageBorderX * 2), r.dy - (pageBorderY * 2));
    }
}
#endif

static void OnPaintFrame(HWND hwnd)
{
    gVirtWndFrame->OnPaint(hwnd);
}

static void OnLButtonDown()
{
}

Rect EbookPosFromWindowSize(HWND hwnd, int dx = -1, int dy = -1)
{
    if ((-1 == dx) || (-1 == dy))
    {
        ClientRect hwndRect(hwnd);
        dx = hwndRect.dx;
        dy = hwndRect.dy;
    }
    Rect r = Rect(0, 0, dx, dy);
    r.Inflate(-pageBorderX, -pageBorderY);
    return r;
}

static void OnCreateWindow(HWND hwnd)
{
    gVirtWndFrame = new VirtWndEbook(hwnd);
    gVirtWndFrame->SetHtml(gSampleHtml);

    Rect r = EbookPosFromWindowSize(hwnd);
    gVirtWndFrame->pos = r;

    HMENU menu = BuildMenu();
    // triggers OnSize(), so must be called after we
    // have things set up to handle OnSize()
    SetMenu(hwnd, menu);
}

static void OnOpen(HWND hwnd)
{
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;

    str::Str<TCHAR> fileFilter;
    fileFilter.Append(_T("All supported documents"));

    ofn.lpstrFilter = _T("All supported documents\0;*.mobi;*.awz;\0\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY |
                OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    ofn.nMaxFile = MAX_PATH * 100;
    ScopedMem<TCHAR> file(SAZA(TCHAR, ofn.nMaxFile));
    ofn.lpstrFile = file;

    if (!GetOpenFileName(&ofn))
        return;

    TCHAR *fileName = ofn.lpstrFile + ofn.nFileOffset;
    if (*(fileName - 1)) {
        // special case: single filename without NULL separator
        gVirtWndFrame->LoadMobi(ofn.lpstrFile);
        return;
    }
    // this is just a test app, no need to support multiple files
    CrashIf(true);
}

static void OnToggleBbox(HWND hwnd)
{
    gShowTextBoundingBoxes = !gShowTextBoundingBoxes;
    InvalidateRect(hwnd, NULL, TRUE);
}

static LRESULT OnCommand(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);

    if ((IDM_EXIT == wmId) || (IDCANCEL == wmId)) {
        OnExit();
        return 0;
    }

    if (IDM_OPEN == wmId) {
        OnOpen(hwnd);
        return 0;
    }

    if (IDM_TOGGLE_BBOX == wmId) {
        OnToggleBbox(hwnd);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void MainLayout(VirtWnd *wnd, Size windowSize)
{
    if (windowSize.Height > pageBorderX * 2)
        windowSize.Width -= (pageBorderX * 2);
    if (windowSize.Width > pageBorderY * 2)
        windowSize.Height -= (pageBorderY * 2);

    wnd->Measure(windowSize);
    Size s = wnd->desiredSize;
    Rect r(pageBorderX, pageBorderY, s.Width, s.Height);
    wnd->Arrange(r);
}

static void OnSize(HWND hwnd, int dx, int dy)
{
   Size s(dx, dy);
   MainLayout(gVirtWndFrame, Size(dx, dy));
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (gVirtWndFrame) {
        bool handled;
        LRESULT res = gVirtWndFrame->evtMgr->OnMessage(msg, wParam, lParam, handled);
        if (handled)
            return res;
    }

    switch (msg)
    {
        case WM_CREATE:
            OnCreateWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_SIZE:
            {
                int dx = LOWORD(lParam);
                int dy = HIWORD(lParam);
                OnSize(hwnd, dx, dy);
                break;
            }

        case WM_ERASEBKGND:
            return TRUE;

        case WM_PAINT:
            OnPaintFrame(hwnd);
            break;

        case WM_LBUTTONDOWN:
            OnLButtonDown();
            break;

        case WM_COMMAND:
            OnCommand(hwnd, msg, wParam, lParam);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
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
    win::GetHwndDpi(NULL, &gUiDPIFactor);

    gHwndFrame = CreateWindow(
            ET_FRAME_CLASS_NAME, _T("Ebook Test ") CURR_VERSION_STR,
            WS_OVERLAPPEDWINDOW,
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

    mui::Initialize();

    //ParseCommandLine(GetCommandLine());
    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    ret = RunApp();

    delete gVirtWndFrame;

Exit:
    mui::Destroy();
    return ret;
}
