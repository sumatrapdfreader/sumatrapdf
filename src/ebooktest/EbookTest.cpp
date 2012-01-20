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
static VirtWndPainter * gVirtWndPainterFrame = NULL;

static bool gShowTextBoundingBoxes = false;

Color gCol1(196, 64, 50); Color gCol1Shadow(134, 48, 39);
Color gCol2(227, 107, 35); Color gCol2Shadow(155, 77, 31);
Color gCol3(93,  160, 40); Color gCol3Shadow(51, 87, 39);
Color gCol4(69, 132, 190); Color gCol4Shadow(47, 89, 127);
Color gCol5(112, 115, 207); Color gCol5Shadow(66, 71, 118);

//Color gColBg(0xff, 0xf2, 0); // this is yellow
Color gColBg(0xe9, 0xe9, 0xe9); // this is darkish gray
Color gColBgTop(0xfa, 0xfa, 0xfa); // this is lightish gray

struct EbookWindowInfo 
{
    MobiParse *         mb;
    const char *        html;
    PageLayout      *   pageLayout;

    EbookWindowInfo() : mb(NULL), html(NULL), pageLayout(NULL) { }

    ~EbookWindowInfo() {
        delete mb;
        delete pageLayout;
    }
};

class EbookLayout : public Layout
{
public:
    EbookLayout() {
    }

    virtual ~EbookLayout() {
    }

    virtual void Measure(Size availableSize, VirtWnd *wnd);
    virtual void Arrange(Rect finalSize, VirtWnd *wnd);
};

void EbookLayout::Measure(Size availableSize, VirtWnd *wnd)
{
    if (2 != wnd->GetChildCount())
        return;
    wnd->desiredSize = availableSize;
    VirtWnd *next = wnd->children.At(0);
    VirtWnd *prev = wnd->children.At(1);
    next->Measure(availableSize);
    prev->Measure(availableSize);
}

void EbookLayout::Arrange(Rect finalSize, VirtWnd *wnd)
{
    if (2 != wnd->GetChildCount())
        return;
    VirtWnd *next = wnd->children.At(0);
    int rectDy = finalSize.Height;
    int rectDx = finalSize.Width;
    int nextPosY = (rectDy - next->desiredSize.Height) / 2;
    if (nextPosY < 0)
        nextPosY = 0;

    int dx = next->desiredSize.Width;
    Rect nextPos(0, nextPosY, next->desiredSize.Width, next->desiredSize.Height);
    next->Arrange(nextPos);

    VirtWnd *prev = wnd->children.At(1);
    int prevPosY = (rectDy - prev->desiredSize.Height) / 2;
    if (prevPosY < 0)
        prevPosY = 0;

    Rect prevPos(rectDx - dx, prevPosY, prev->desiredSize.Width, prev->desiredSize.Height);
    prev->Arrange(prevPos);

    wnd->pos = finalSize;
}

class VirtWndEbook : public VirtWnd
{
public:
    VirtWndEbook();

    virtual ~VirtWndEbook() {
    }

    virtual void Measure(Size availableSize) {
        desiredSize = availableSize;
    }

    PageLayout *    pageLayout;

    virtual void Paint(Graphics *gfx, int offX, int offY);
};

VirtWndEbook::VirtWndEbook()
{
    layout = new EbookLayout();
    children.Append(new VirtWndButton(_T("Next")));
    children.Append(new VirtWndButton(_T("Prev")));
}

static EbookWindowInfo *gCurrentEbook = NULL;

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

static inline void EnableAndShow(HWND hwnd, bool enable)
{
    ShowWindow(hwnd, enable ? SW_SHOW : SW_HIDE);
    EnableWindow(hwnd, enable);
}

// A sample text to display if we don't show an actual mobi file
static const char *gSampleHtml = "<html><p align=justify>ClearType is <b>dependent</b> on the <i>orientation and ordering</i> of the LCD stripes.</p> <p align='right'><em>Currently</em>, ClearType is implemented <hr> only for vertical stripes that are ordered RGB.</p> <p align=center>This might be a concern if you are using a tablet PC.</p> <p>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The <strike>following example</strike> draws text with two <u>different quality</u> settings.</p> On to the <b>next<mbp:pagebreak>page</b></html>";

static EbookWindowInfo *LoadSampleHtml()
{
    EbookWindowInfo *wi = new EbookWindowInfo();
    wi->html = gSampleHtml;
    return wi;
}

static EbookWindowInfo *LoadEbook(const TCHAR *fileName)
{
    EbookWindowInfo *wi = new EbookWindowInfo();
    wi->mb = MobiParse::ParseFile(fileName);
    if (!wi->mb) {
        delete wi;
        return NULL;
    }
    return wi;
}

static PageLayout *LayoutMobiFile(EbookWindowInfo *wi, Graphics *gfx, int pageDx, int pageDy)
{
    PageLayout *layout = new PageLayout(pageDx, pageDy);
    const char *html = wi->html;
    size_t len;
    if (html) {
        len = strlen(wi->html);
    } else {
        html = wi->mb->GetBookHtmlData(len);
    }
    bool ok = layout->LayoutHtml(gfx, FONT_NAME, FONT_SIZE, html, len);
    if (!ok) {
        delete layout;
        return NULL;
    }
    return layout;
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
    DrawPageLayout(gfx, pageLayout, 0, (REAL)offX, (REAL)offY);
}

static void UpdatePageCount()
{
    size_t pagesCount = gCurrentEbook->pageLayout->PageCount();
    ScopedMem<TCHAR> s(str::Format(_T("%d pages"), (int)pagesCount));
    win::SetText(gHwndFrame, s.Get());
}

const int pageBorderX = 10;
const int pageBorderY = 10;

static void ReLayout(Graphics* gfx, int pageDx, int pageDy)
{
    if (gCurrentEbook->pageLayout)
    {
        int currPageDx = (int)gCurrentEbook->pageLayout->pageDx;
        int currPageDy = (int)gCurrentEbook->pageLayout->pageDy;
        if ((pageDx == currPageDx) && (pageDy == currPageDy))
            return;
    }
    gCurrentEbook->pageLayout = LayoutMobiFile(gCurrentEbook, gfx, pageDx, pageDy);
    gVirtWndFrame->pageLayout = gCurrentEbook->pageLayout;
    UpdatePageCount();
}

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
    gVirtWndPainterFrame->OnPaint(hwnd, gVirtWndFrame);
}

static void OnLButtonDown()
{
}

static void LoadEbook(TCHAR *fileName, HWND hwnd)
{
    EbookWindowInfo *wi = LoadEbook(fileName);
    if (!wi)
        return;
    delete gCurrentEbook;
    gCurrentEbook = wi;
}

static void LoadSampleAsCurrentDoc()
{
    EbookWindowInfo *wi = LoadSampleHtml();
    if (!wi)
        return;
    delete gCurrentEbook;
    gCurrentEbook = wi;
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

static void RelayoutByHwnd(HWND hwnd, int dx = -1, int dy = -1)
{
    HDC dc = GetDC(hwnd);
    Graphics gfx(dc);
    Rect r = EbookPosFromWindowSize(hwnd, dx, dy);
    ReLayout(&gfx, r.Width, r.Height);
}

static void OnCreateWindow(HWND hwnd)
{
    LoadSampleAsCurrentDoc();
    gVirtWndFrame = new VirtWndEbook();
    gVirtWndFrame->hwndParent = hwnd;
    gVirtWndPainterFrame = new VirtWndPainter();
    Rect r = EbookPosFromWindowSize(hwnd);
    gVirtWndFrame->pos = r;
    RelayoutByHwnd(hwnd);

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
        LoadEbook(ofn.lpstrFile, hwnd);
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
        windowSize.Height -= (pageBorderX * 2);
    if (windowSize.Width > pageBorderY * 2)
        windowSize.Height -= (pageBorderY * 2);

    wnd->Measure(windowSize);
    Size s = wnd->desiredSize;
    Rect r(pageBorderX, pageBorderY, s.Width, s.Height);
    wnd->Arrange(r);
}

static void OnSize(HWND hwnd, int dx, int dy)
{
    RelayoutByHwnd(hwnd, dx, dy);
    Size s(dx, dy);
    MainLayout(gVirtWndFrame, Size(dx, dy));
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
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

    //ParseCommandLine(GetCommandLine());
    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    ret = RunApp();

    delete gCurrentEbook;
    delete gVirtWndFrame;
    delete gVirtWndPainterFrame;

Exit:
    return ret;
}
