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
    StringPos() : s(NULL), len(0) {
    }
    StringPos(const char *s, size_t len, RectF bb) : s(s), len(len), bb(bb) {
    }
    const char *s;
    size_t len;
    RectF bb;
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
        wi.len = 1;
        wi.s = NewLineStr;
        return &wi;
    }
    wi.s = curr;
    SkipNonWordBreak(curr, left);
    wi.len = curr - wi.s;
    assert(wi.len > 0);
    return &wi;
}

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
// TODO: this seems to sometimes reports size that is slightly too small
static RectF MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    assert(len > 0);
    // note: frankly, I don't see a difference between those StringFormat variations
    StringFormat sf(StringFormat::GenericTypographic());
    //StringFormat sf(StringFormat::GenericDefault());
    //StringFormat sf;
    RectF layoutRect;
    CharacterRange cr(0, len);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region r;
    g->MeasureCharacterRanges(s, len, f, layoutRect, &sf, 1, &r);
    RectF bb;
    r.GetBounds(&bb, g);
    bb.Width += 4.5f; // TODO: total magic, but seems to produce better results
    return bb;
}

// this usually reports size that is too large
static RectF MeasureTextStandard(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    RectF bb;
    PointF pz(0,0);
    g->MeasureString(s, len, f, pz, &bb);
    return bb;
}

static inline RectF MeasureText(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    RectF bb1 = MeasureTextStandard(g, f, s, len);
    RectF bb2 = MeasureTextAccurate(g, f, s, len);
    return bb2;
}

// TODO: not quite sure why spaceDx1 != spaceDx2, using spaceDx2 because
// is smaller and looks as better spacing to me
// note: we explicitly use MeasureTextStandard() because
// MeasureTextAccurate() ignores the trailing whitespace
static REAL GetSpaceDx(Graphics *g, Font *f)
{
    RectF bb;
#if 1
    bb = MeasureTextStandard(g, f, L" ", 1);
    REAL spaceDx1 = bb.Width;
    return spaceDx1;
#else
    bb = MeasureTextStandard(g, f, L"wa", 2);
    REAL l1 = bb.Width;
    bb = MeasureTextStandard(g, f, L"w a", 3);
    REAL l2 = bb.Width;
    REAL spaceDx2 = l2 - l1;
    return spaceDx2;
#endif
}

class PageLayout
{
    enum TextJustification {
        Left, Right, Center, Both
    };

    struct StrDx {
        StrDx() : s(NULL), len(0), dx(0), dy(0) {
        }
        StrDx(const char *s, size_t len, REAL dx, REAL dy) : s(s), len(len), dx(dx), dy(dy) {
        }
        const char *s;
        size_t len;
        REAL dx, dy;
    };

public:
    PageLayout(int dx, int dy) {
        pageDx = (REAL)dx; pageDy = (REAL)dy;
        lineSpacing = 0; spaceDx = 0;
        pages = NULL; p = NULL;
        x = y = 0;
    }

    Vec<Page *> *Layout(Graphics *g, Font *f, const char *s);

private:
    REAL GetTotalLineDx();
    void LayoutLeftStartingAt(REAL offX);
    void JustifyLineLeft();
    void JustifyLineRight();
    void JustifyLineCenter();
    void JustifyLineBoth();
    void JustifyLine();

    void StartLayout();
    void StartNewPage();
    void StartNewLine();
    void RemoveLastPageIfEmpty();
    void AddWord(WordInfo *wi);

    // constant during layout process
    REAL pageDx, pageDy;
    REAL lineSpacing;
    REAL spaceDx;
    Graphics *g;
    Font *f;

    // temporary state during layout process
    TextJustification j;
    Vec<Page *> *pages;
    Page *p; // current page
    REAL x, y; // current position in a page
    WCHAR buf[512];
    int newLinesCount; // consecutive newlines
    Vec<StrDx> lineStringsDx;
};

void PageLayout::StartLayout()
{
    j = Both;
    pages = new Vec<Page*>();
    lineSpacing = f->GetHeight(g);
    spaceDx = GetSpaceDx(g, f);
    StartNewPage();
}

void PageLayout::StartNewPage()
{
    x = y = 0;
    newLinesCount = 0;
    p = new Page((int)pageDx, (int)pageDy);
    pages->Append(p);
}

REAL PageLayout::GetTotalLineDx()
{
    REAL dx = -spaceDx;
    for (size_t i = 0; i < lineStringsDx.Count(); i++) {
        StrDx sdx = lineStringsDx.At(i);
        dx += sdx.dx;
        dx += spaceDx;
    }
    return dx;
}

void PageLayout::LayoutLeftStartingAt(REAL offX)
{
    x = offX;
    for (size_t i = 0; i < lineStringsDx.Count(); i++) {
        StrDx sdx = lineStringsDx.At(i);
        RectF bb(x, y, sdx.dx, sdx.dy);
        StringPos sp(sdx.s, sdx.len, bb);
        p->strings->Append(sp);
        x += (sdx.dx + spaceDx);
    }
}

void PageLayout::JustifyLineLeft()
{
    LayoutLeftStartingAt(0);
}

void PageLayout::JustifyLineRight()
{
    x = pageDx;
    for (size_t i = 0; i < lineStringsDx.Count(); i++) {
        StrDx sdx = lineStringsDx.At(lineStringsDx.Count() - i - 1);
        x -= sdx.dx;
        RectF bb(x, y, sdx.dx, sdx.dy);
        StringPos sp(sdx.s, sdx.len, bb);
        p->strings->Append(sp);
        x -= spaceDx;
    }
}

void PageLayout::JustifyLineCenter()
{
    REAL margin = (pageDx - GetTotalLineDx());
    LayoutLeftStartingAt(margin / 2.f);
}

// TODO: a line at the end of paragraph (i.e. followed by an empty line or the last line)
// should be justified left. Need to look ahead for that
void PageLayout::JustifyLineBoth()
{
    REAL extraDxSpace = (pageDx - GetTotalLineDx()) / (REAL)(lineStringsDx.Count() - 1);
    size_t middleString = lineStringsDx.Count() / 2;

    // first half of strings are laid out starting from left
    x = 0;
    for (size_t i = 0; i <= middleString; i++) {
        StrDx sdx = lineStringsDx.At(i);
        RectF bb(x, y, sdx.dx, sdx.dy);
        StringPos sp(sdx.s, sdx.len, bb);
        p->strings->Append(sp);
        x += (sdx.dx + spaceDx);
    }

    // second half of strings are laid out from right
    x = pageDx;
    for (size_t i = lineStringsDx.Count() - 1; i > middleString; i--) {
        StrDx sdx = lineStringsDx.At(i);
        x -= sdx.dx;
        RectF bb(x, y, sdx.dx, sdx.dy);
        StringPos sp(sdx.s, sdx.len, bb);
        p->strings->Append(sp);
        x -= (spaceDx + extraDxSpace);
    }
}

void PageLayout::JustifyLine()
{
    if (0 == lineStringsDx.Count())
        return; // nothing to do
    switch (j) {
        case Left:
            JustifyLineLeft();
            break;
        case Right:
            JustifyLineRight();
            break;
        case Center:
            JustifyLineCenter();
            break;
        case Both:
            JustifyLineBoth();
            break;
        default:
            assert(0);
            break;
    }
    lineStringsDx.Reset();
}

void PageLayout::StartNewLine()
{
    JustifyLine();
    x = 0;
    y += lineSpacing;
    lineStringsDx.Reset();
    if (y > pageDy)
        StartNewPage();
}

void PageLayout::AddWord(WordInfo *wi)
{
    RectF bb;
    if (wi->IsNewline()) {
        // a single newline is considered "soft" and ignored
        // two or more consequitive newlines are considered a
        // single paragraph break
        newLinesCount++;
        if (2 == newLinesCount) {
            bool needsTwo = (x != 0);
            StartNewLine();
            if (needsTwo)
                StartNewLine();
        }
        return;
    }
    newLinesCount = 0;
    Utf8ToWchar(wi->s, wi->len, buf, dimof(buf));
    bb = MeasureText(g, f, buf, wi->len);
    // TODO: handle a case where a single word is bigger than the whole
    // line, in which case it must be split into multiple lines
    REAL dx = bb.Width;
    if (x + dx > pageDx) {
        // start new line if the new text would exceed the line length
        StartNewLine();
    }
    StrDx sdx(wi->s, wi->len, dx, bb.Height);
    lineStringsDx.Append(sdx);
    x += (dx + spaceDx);
}

void PageLayout::RemoveLastPageIfEmpty()
{
    // TODO: write me
}

// How layout works: 
// * measure the strings
// * remember a line's worth of widths
// * when we fill a line we calculate the position of strings in
//   a line for a given justification setting (left, right, center, both)
Vec<Page*> *PageLayout::Layout(Graphics *g, Font *f, const char *s)
{
    this->g = g;
    this->f = f;
    StartLayout();
    WordsIter iter(s);
    for (;;) {
        WordInfo *wi = iter.Next();
        if (NULL == wi)
            break;
        AddWord(wi);
    }
    if (j == Both)
        j = Left;
    JustifyLine();
    RemoveLastPageIfEmpty();
    Vec<Page*> *ret = pages;
    pages = NULL;
    return ret;
}

Vec<Page*> *LayoutText(Graphics *g, Font *f, int pageDx, int pageDy, const char *s)
{
    PageLayout *l = new PageLayout(pageDx, pageDy);
    Vec<Page*> *ret = l->Layout(g, f, s);
    delete l;
    return ret;
}

static bool gShowTextBoundingBoxes = false;

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
        RectF bb = sp.bb;
        bb.X += offX;
        bb.Y += offY;
        Utf8ToWchar(sp.s, sp.len, buf, dimof(buf));
        bb.GetLocation(&pos);
        if (gShowTextBoundingBoxes) {
            //g->FillRectangle(&br, bb);
            g->DrawRectangle(&pen, bb);
        }
        g->DrawString(buf, sp.len, f, pos, NULL, &br);
    }
}

static void DrawFrame2(Graphics &g, RectI r)
{
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetPageUnit(UnitPixel);

    const int pageBorderX = 10;
    const int pageBorderY = 10;
    if (!gPages) {
        int pageDx = r.dx - (pageBorderX * 2);
        int pageDy = r.dy - (pageBorderY * 2);
        gPages = LayoutText(&g, gFont, pageDx, pageDy , gTxt);
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

    DeleteVecMembers<Page*>(*gPages);
    delete gPages;
    ::delete gFont;
    ::delete frameBmp;

Exit:
    return ret;
}
