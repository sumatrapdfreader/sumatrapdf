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
#include "FrameTimeoutCalculator.h"
#include "Transactions.h"
#include "Scopes.h"
#include "PageLayout.h"
#include "MobiParse.h"
#include "MobiHtmlParse.h"
#include "EbookTestMenu.h"

using namespace Gdiplus;

#define ET_FRAME_CLASS_NAME    _T("ET_FRAME")

#define WIN_DX    640
#define WIN_DY    480

static HINSTANCE        ghinst;
static HWND             gHwndFrame = NULL;

static HFONT            gFontDefault;

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
    Vec<uint8_t> *      forLayout;
    PageLayout      *   pageLayout;

    EbookWindowInfo() : mb(NULL), forLayout(NULL), pageLayout(NULL) { }
    ~EbookWindowInfo() {
        delete mb;
        delete forLayout;
        delete pageLayout;
    }
};

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

static Font *gFont = NULL;

// A sample text to display if we don't show an actual mobi file
static const char *gSampleHtml = "<html><p align=left>ClearType is <b>dependent</b> on the <i>orientation and ordering</i> of the LCD stripes.</p> <p align='justify'>Currently, ClearType is implemented <hr> only for vertical stripes that are ordered RGB.</p> <p align=right>This might be a concern if you are using a tablet PC.</p> <p align=center>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The following example draws text with two different quality settings.</p></html>";

static EbookWindowInfo *LoadSampleHtml()
{
    Vec<uint8_t> *forLayout = MobiHtmlToDisplay((uint8_t*)gSampleHtml, sizeof(gSampleHtml) - 1, NULL);
    if (!forLayout)
        return NULL;
    EbookWindowInfo *wi = new EbookWindowInfo();
    wi->forLayout = forLayout;
    return wi;
}

static EbookWindowInfo *LoadEbook(const TCHAR *fileName)
{
    EbookWindowInfo *wi = new EbookWindowInfo();
    wi->mb = MobiParse::ParseFile(fileName);
    if (!wi->mb)
        goto Error;

    size_t sLen;
    char *s = wi->mb->GetBookHtmlData(sLen);
    wi->forLayout = MobiHtmlToDisplay((uint8_t*)s, sLen, false);
    if (!wi->forLayout)
        goto Error;

    return wi;

Error:
    delete wi;
    return NULL;
}

static PageLayout *LayoutMobiFile(EbookWindowInfo *wi, Graphics *gfx, Font *defaultFont, int pageDx, int pageDy)
{
    PageLayout *layout = new PageLayout(pageDx, pageDy);
    bool ok = layout->LayoutInternal(gfx, defaultFont, wi->forLayout->LendData(), wi->forLayout->Count());
    if (!ok) {
        delete layout;
        return NULL;
    }
    return layout;
}

static void DrawPage(Graphics *g, Font *f, int pageNo, REAL offX, REAL offY)
{
    StringFormat sf(StringFormat::GenericTypographic());
    SolidBrush br(Color(0,0,0));
    SolidBrush br2(Color(255, 255, 255, 255));
    Pen pen(Color(255, 0, 0), 1);
    Pen blackPen(Color(0, 0, 0), 1);

    WCHAR buf[512];
    PointF pos;
    size_t instrCount;
    DrawInstr *currInstr = gCurrentEbook->pageLayout->GetInstructionsForPage(pageNo, &instrCount);
    for (size_t i = 0; i < instrCount; i++) {
        RectF bbox = currInstr->bbox;
        bbox.X += offX;
        bbox.Y += offY;
        if (InstrTypeLine == currInstr->type) {
            // hr is a line drawn in the middle of bounding box
            REAL y = bbox.Y + bbox.Height / 2.f;
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            g->DrawLine(&blackPen, p1, p2);
            continue;
        }
        CrashAlwaysIf(InstrTypeString != currInstr->type);
        size_t strLen = str::Utf8ToWcharBuf((const char*)currInstr->str.s, currInstr->str.len, buf, dimof(buf));
        bbox.GetLocation(&pos);
        if (gShowTextBoundingBoxes) {
            //g->FillRectangle(&br, bbox);
            g->DrawRectangle(&pen, bbox);
        }
        g->DrawString(buf, strLen, f, pos, NULL, &br);
        ++currInstr;
    }
}

const int pageBorderX = 10;
const int pageBorderY = 10;

static void ReLayout(Graphics* gfx, RectI r)
{
    CrashAlwaysIf(gCurrentEbook->pageLayout);

    int pageDx = r.dx - (pageBorderX * 2);
    int pageDy = r.dy - (pageBorderY * 2);

    if (gCurrentEbook)
        gCurrentEbook->pageLayout = LayoutMobiFile(gCurrentEbook, gfx, gFont, pageDx, pageDy);
}

static void DrawFrame2(Graphics &g, RectI r)
{
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetPageUnit(UnitPixel);

    if (!gCurrentEbook->pageLayout)
        ReLayout(&g, r);

    if (!gCurrentEbook->pageLayout)
        return;

    //SolidBrush bgBrush(gColBg);
    Gdiplus::Rect r2(r.x-1, r.y-1, r.dx+2, r.dy+2);
    LinearGradientBrush bgBrush(RectF(0, 0, (REAL)r.dx, (REAL)r.dy), Color(0xd0,0xd0,0xd0), Color(0xff,0xff,0xff), LinearGradientModeVertical);
    g.FillRectangle(&bgBrush, r2);

    DrawPage(&g, gFont, 0, (REAL)pageBorderX, (REAL)pageBorderY);
    if (gShowTextBoundingBoxes) {
        Pen p(Color(0,0,255), 1);
        g.DrawRectangle(&p, pageBorderX, pageBorderY, r.dx - (pageBorderX * 2), r.dy - (pageBorderY * 2));
    }
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

    if (!gCurrentEbook->pageLayout)
        return;
    size_t pagesCount = gCurrentEbook->pageLayout->PageCount();
    ScopedMem<TCHAR> s(str::Format(_T("%d pages"), (int)pagesCount));
    win::SetText(hwnd, s.Get());

}

static void OnLButtonDown()
{
}

static void LoadEbookAndTriggerLayout(TCHAR *fileName, HWND hwnd)
{
    EbookWindowInfo *wi = LoadEbook(fileName);
    if (!wi)
        return;
    delete gCurrentEbook;
    gCurrentEbook = wi;
    InvalidateRect(hwnd, NULL, true);
}

static void LoadSampleAsCurrentDoc()
{
    EbookWindowInfo *wi = LoadSampleHtml();
    if (!wi)
        return;
    delete gCurrentEbook;
    gCurrentEbook = wi;
}

static void OnCreateWindow(HWND hwnd)
{
    //gFont = ::new Font(L"Times New Roman", 16, FontStyleRegular);
    //HDC dc = GetDC(hwnd);
    //gFont = ::new Font(dc, gFontDefault); 
    gFont = ::new Font(L"Georgia", 10, FontStyleRegular);
    HMENU menu = BuildMenu();
    SetMenu(hwnd, menu);
    LoadSampleAsCurrentDoc();
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
        LoadEbookAndTriggerLayout(ofn.lpstrFile, hwnd);
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

    delete gCurrentEbook;
    ::delete gFont;
    ::delete frameBmp;

Exit:
    return ret;
}
