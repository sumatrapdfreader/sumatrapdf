#include "stdafx.h"

#include "render-speed-test.h"
#include "util.h"

using namespace Gdiplus;

/*
Results: GDI seems to be easily 10x faster. Sample timings:

gdi  measure,0.3284
gdi  measure,0.0611
gdi  measure,0.0602
gdi+ measure,15.1113
gdi+ measure,28.5502
gdi+ measure,12.9735
gdi  draw,1.0829
gdi  draw,0.8567
gdi  draw,0.9709
gdi+ draw,14.1167
gdi+ draw,13.6285
gdi+ draw,14.3014

*/

// a program to test various ways of measuring and drawing text
// TODO:
// - add DirectDraw version
// - re-layout when the window size changes
// - solve alpha-transparency issue with GDI
//   http://www.gamedev.net/topic/333453-32-bit-alpha-bitmaps-and-gdi-fun-alert/
//   http://stackoverflow.com/questions/5647322/gdi-font-rendering-especially-in-layered-windows
//   http://www.codeproject.com/Questions/182071/GDI-font-rendering-and-layered-windows

HINSTANCE hInst;

// TODO: static_assert(sizeof(i64)==8)
#define APP_TITLE       L"RenderSpeedTest"
#define APP_WIN_CLASS   L"RENDERSPEEDTEST_WIN_CLS"

#define FONT_NAME       L"Tahoma"
#define FONT_SIZE       10

// we want to run it couple of times and then we pick the best number
#define REPEAT_COUNT 3

// all times are in milli-seconds
double gGdiMeasureTimes[REPEAT_COUNT];
double gGdiDrawTimes[REPEAT_COUNT];
double gGdiplusMeasureTimes[REPEAT_COUNT];
double gGdiplusDrawTimes[REPEAT_COUNT];

void DumpTiming(const char *name, double timeMs) {
    char *s = str::Format("%s,%.4f\n", name, (float) timeMs);
    OutputDebugStringA(s);
    free(s);
}

void DumpTimings() {
    for (int i = 0; i < REPEAT_COUNT; i++) {
        DumpTiming("gdi  measure", gGdiMeasureTimes[i]);
    }
    for (int i = 0; i < REPEAT_COUNT; i++) {
        DumpTiming("gdi+ measure", gGdiplusMeasureTimes[i]);
    }
    for (int i = 0; i < REPEAT_COUNT; i++) {
        DumpTiming("gdi  draw", gGdiDrawTimes[i]);
    }
    for (int i = 0; i < REPEAT_COUNT; i++) {
        DumpTiming("gdi+ draw", gGdiplusDrawTimes[i]);
    }
}

// set consistent mode for our graphics objects so that we get
// the same results when measuring text
void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g->SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);
}

class ITextMeasure {
public:
    virtual RectF Measure(const char *s, size_t sLen) = 0;
};

class ITextDraw {
public:
    virtual void Draw(const char *s, size_t sLen, RectF& bb) = 0;
};

// note: gdi+ seems to under-report the width, the longer the text, the
// bigger the difference. I'm trying to correct for that with those magic values
#define PER_CHAR_DX_ADJUST .2f
#define PER_STR_DX_ADJUST  1.f

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
RectF MeasureTextAccurate(Graphics *g, Font *f, const WCHAR *s, size_t len)
{
    if (0 == len)
        return RectF(0, 0, 0, 0); // TODO: should set height to font's height
    // note: frankly, I don't see a difference between those StringFormat variations
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    //StringFormat sf(StringFormat::GenericDefault());
    //StringFormat sf;
    RectF layoutRect;
    CharacterRange cr(0, (INT)len);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region r;
    Status status = g->MeasureCharacterRanges(s, (INT)len, f, layoutRect, &sf, 1, &r);
    if (status != Status::Ok)
        return RectF(0, 0, 0, 0); // TODO: should set height to font's height
    RectF bbox;
    r.GetBounds(&bbox, g);
    if (bbox.Width != 0)
        bbox.Width += PER_STR_DX_ADJUST + (PER_CHAR_DX_ADJUST * (float) len);
    return bbox;
}

float DpiScaled(float n) {
    static float scale = 0.0f;
    if (scale == 0.0f) {
        win::GetHwndDpi(HWND_DESKTOP, &scale);
    }
    return n * scale;
}

int PixelToPoint(int n) {
    return n * 96 / 72;
}

HFONT gFont = nullptr;

HFONT GetGdiFont() {
    if (gFont == nullptr) {
        HDC hdc = GetDC(NULL);
        gFont = CreateSimpleFont(hdc, FONT_NAME, (int) DpiScaled((float) PixelToPoint(FONT_SIZE)));
        ReleaseDC(NULL, hdc);
    }
    return gFont;
}

void DeleteGdiFont() {
    DeleteObject(gFont);
}

class TextMeasureGdi : public ITextMeasure {
private:
    HDC         hdc;
    WCHAR       txtConvBuf[512];

    TextMeasureGdi() { }

public:

    static TextMeasureGdi* Create(HDC hdc);

    virtual RectF Measure(const char *s, size_t sLen);
    virtual ~TextMeasureGdi() {}
};

RectF TextMeasureGdi::Measure(const char *s, size_t sLen) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    SIZE txtSize;
    GetTextExtentPoint32W(hdc, txtConvBuf, (int)strLen, &txtSize);
    RectF res(0.0f, 0.0f, (float) txtSize.cx, (float) txtSize.cy);
    return res;
}

TextMeasureGdi *TextMeasureGdi::Create(HDC hdc) {
    auto res = new TextMeasureGdi();
    res->hdc = hdc;
    return res;
}

class TextMeasureGdiplus : public ITextMeasure {
private:
    enum {
        bmpDx = 32,
        bmpDy = 4,
        stride = bmpDx * 4,
    };

    Graphics *  gfx;
    Font *      fnt;
    Bitmap *    bmp;
    BYTE        data[bmpDx * bmpDy * 4];
    WCHAR       txtConvBuf[512];
    TextMeasureGdiplus() : gfx(nullptr), bmp(nullptr), fnt(nullptr) {}

public:
    float GetFontHeight() const {
        return fnt->GetHeight(gfx);
    }

    static TextMeasureGdiplus* Create(HDC hdc);

    virtual RectF Measure(const char *s, size_t sLen);
    virtual ~TextMeasureGdiplus();
};

TextMeasureGdiplus::~TextMeasureGdiplus() {
    ::delete bmp;
    ::delete gfx;
    ::delete fnt;
};

RectF TextMeasureGdiplus::Measure(const char *s, size_t sLen) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    return MeasureTextAccurate(gfx, fnt, txtConvBuf, strLen);
}

TextMeasureGdiplus *TextMeasureGdiplus::Create(HDC hdc) {
    auto res = new TextMeasureGdiplus();
    res->bmp = ::new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, res->data);
    if (!res->bmp)
        return nullptr;
    res->gfx = ::new Graphics((Image*) res->bmp);
    InitGraphicsMode(res->gfx);
    res->fnt = ::new Font(hdc, GetGdiFont());
    //res->fnt = ::new Font(FONT_NAME, FONT_SIZE);
    return res;
}

class TextDrawGdi : public ITextDraw {
private:
    HDC hdc;
    WCHAR       txtConvBuf[512];

    TextDrawGdi() { }

public:
    static TextDrawGdi *Create(HDC hdc);

    virtual void Draw(const char *s, size_t sLen, RectF& bb);
    virtual ~TextDrawGdi() {}
};

TextDrawGdi* TextDrawGdi::Create(HDC hdc) {
    auto res = new TextDrawGdi();
    res->hdc = hdc;
    return res;
}

void TextDrawGdi::Draw(const char *s, size_t sLen, RectF& bb) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    PointF loc;
    bb.GetLocation(&loc);
    int x = (int) bb.X;
    int y = (int) bb.Y;
    ExtTextOutW(hdc, x, y, 0, NULL, txtConvBuf, strLen, NULL);
}

class TextDrawGdiplus : public ITextDraw {
private:
    Font *      fnt;
    Brush *     col;
    WCHAR       txtConvBuf[512];

    TextDrawGdiplus() : gfx(nullptr) { }

public:
    Graphics *  gfx;

    static TextDrawGdiplus *Create(HDC dc);
    virtual void Draw(const char *s, size_t sLen, RectF& bb);
    virtual ~TextDrawGdiplus();
};

TextDrawGdiplus* TextDrawGdiplus::Create(HDC dc) {
    auto res = new TextDrawGdiplus();
    res->gfx = ::new Graphics(dc);
    InitGraphicsMode(res->gfx);
    res->fnt = ::new Font(dc, GetGdiFont());
    //res->fnt = ::new Font(FONT_NAME, FONT_SIZE);
    res->col = ::new SolidBrush(Color(0, 0, 0));
    return res;
}

TextDrawGdiplus::~TextDrawGdiplus() {
    ::delete gfx;
    ::delete fnt;
}

void TextDrawGdiplus::Draw(const char *s, size_t sLen, RectF& bb) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    PointF loc;
    bb.GetLocation(&loc);
    gfx->DrawString(txtConvBuf, (INT) strLen, fnt, loc, col);
}

// normalizes newline characters in-place (i.e. replaces '\r' and "\r\n" with '\n')
// return the new end of the buffer (guaranteed to be <= end)
char *normalize_nl(char *, char *end) {
    // TODO: write me
    return end;
}

struct MeasuredString {
    const char *s;
    size_t sLen;
    RectF bb;

    bool IsNewline() const {
        return (sLen == 1) && (*s == '\n');
    }

    bool IsSpace() const {
        return (sLen == 1) && (*s == ' ');
    }
};

#define MAX_STRINGS 4096

struct MeasuredStrings {
    MeasuredString *strings = nullptr;
    size_t nStrings = 0;

    MeasuredStrings() : strings(nullptr), nStrings(0) {
    }

    ~MeasuredStrings() {
        Free();
    }

    void Free();

    MeasuredString *GetMeasuredString(size_t n);
    MeasuredString *AllocMeasuredString(const char *s, size_t sLen, float dx, float dy);

};

void MeasuredStrings::Free() {
    free((void*) strings);
    strings = nullptr;
    nStrings = 0;
}

MeasuredString *MeasuredStrings::GetMeasuredString(size_t n) {
    if (nStrings >= MAX_STRINGS) {
        return nullptr;
    }
    return &strings[n];
}

MeasuredString *MeasuredStrings::AllocMeasuredString(const char *s, size_t sLen, float dx, float dy) {
    if (nullptr == strings) {
        strings = AllocStruct<MeasuredString>(MAX_STRINGS);
    }
    nStrings++;
    auto ms = GetMeasuredString(nStrings-1);
    ms->s = s;
    ms->sLen = sLen;
    ms->bb.X = 0;
    ms->bb.Y = 0;
    ms->bb.Width = dx;
    ms->bb.Height = dy;
    return ms;
}

MeasuredStrings *BreakTextIntoStrings(char *s, size_t sLen) {
    auto res = new MeasuredStrings();
    IterWords(s, sLen, [&res](char *s, size_t sLen) {
        res->AllocMeasuredString(s, sLen, 0.f, 0.f);
    });
    return res;
}

void MeasureStrings(MeasuredStrings *strings, ITextMeasure *m) {
    for (size_t i = 0; i < strings->nStrings; i++) {
        auto ms = strings->GetMeasuredString(i);
        RectF bb = m->Measure(ms->s, ms->sLen);
        ms->bb.Width = bb.Width;
        ms->bb.Height = bb.Height;
    }
}

void LayoutStrings(MeasuredStrings *strings, float areaDx, float spaceDx, float lineDy) {
    float x = 0;
    float y = 0;
    float maxTextDy = lineDy; // for current line
    for (size_t i = 0; i < strings->nStrings; i++) {
        auto ms = strings->GetMeasuredString(i);
        if (ms->IsNewline()) {
            x = 0;
            y += maxTextDy + 2;
            continue;
        }

        auto& bb = ms->bb;
        float textDx = bb.Width;
        float textDy = bb.Height;
        if (textDy > maxTextDy) {
            maxTextDy = textDy;
        }

        if (x + textDx < areaDx) {
            bb.X = x;
            bb.Y = y;
            x += textDx + spaceDx;
            continue;
        }

        // would exceed dx - advance to new line
        if (x == 0) {
            // first word in the line, so put it on this line
            bb.X = x;
            bb.Y = y;
            // advance to next line
            y += maxTextDy + 2;
            continue;
        }

        bb.X = 0;
        y += maxTextDy + 2;
        maxTextDy = textDy;
        bb.Y = y;
        x = textDx + spaceDx;
    }
}

MeasuredStrings *DoLayoutGdiplus(TextMeasureGdiplus *m, char *s, float areaDx) {
    auto sLen = strlen(s);
    auto strings = BreakTextIntoStrings(s, sLen);
    for (int i = 0; i < REPEAT_COUNT; i++) {
        Timer timer(true);
        MeasureStrings(strings, m);
        gGdiplusMeasureTimes[i] = timer.Stop();
    }
    float fontDy = m->GetFontHeight();
    float spaceDx = m->Measure(" ", 1).Width;
    LayoutStrings(strings, areaDx, spaceDx, fontDy);
    return strings;
}

MeasuredStrings * DoLayoutGdi(TextMeasureGdi *m, char *s, float areaDx) {
    auto sLen = strlen(s);
    auto strings = BreakTextIntoStrings(s, sLen);
    for (int i = 0; i < REPEAT_COUNT; i++) {
        Timer timer(true);
        MeasureStrings(strings, m);
        gGdiMeasureTimes[i] = timer.Stop();
    }
    //float fontDy = m->GetFontHeight();
    float fontDy = 12.0f; // TODO: use real font height
    float spaceDx = m->Measure(" ", 1).Width;
    LayoutStrings(strings, areaDx, spaceDx, fontDy);
    return strings;
}

enum RenderType {
    RENDER_GDI,
    RENDER_GDI_PLUS,
    RENDER_DIRECT_DRAW
};

struct SampleWindow : Window<SampleWindow>
{
    HFONT font;
    RenderType renderType;
    MeasuredStrings *strings;

    SampleWindow(RenderType rt, const WCHAR *title)
    {
        font = nullptr;
        renderType = rt;
        strings = nullptr;

        WNDCLASS wc = {};

        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hInstance = hInst;
        wc.lpszClassName = L"Render_Speed_Test_Wnd_Cls";
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;

        RegisterClass(&wc);

        CrashIf(m_window);

        CreateWindowW(wc.lpszClassName,
            title,
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 720, 480,
            nullptr,
            nullptr,
            wc.hInstance,
            this);

        CrashIf(!m_window);
    }

    ~SampleWindow() {
        DeleteObject(font);
        delete strings;
    }

    LRESULT MessageHandler(UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (WM_PAINT == message)
        {
            PaintHandler();
            return 0;
        }

        if (WM_ERASEBKGND == message) {
            // do nothing, helps to avoid flicker
            return TRUE;
        }

        return Window::MessageHandler(message, wparam, lparam);
    }

    // Note: not sure if that measures the right thing - clipping optimization
    // might distort timings
    void DrawStrings(ITextDraw *td)
    {
        for (size_t i = 0; i < strings->nStrings; i++) {
            auto ms = strings->GetMeasuredString(i);
            if (ms->IsNewline()) {
                continue;
            }
            auto& bb = ms->bb;
            td->Draw(ms->s, ms->sLen, bb);
        }
    }

    void PaintHandlerGdiplus()
    {
        static bool firstTime = true;

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_window, &ps);

        ClientRect rcClient(m_window);
        RECT rTmp = rcClient.ToRECT();

        if (!strings) {
            auto m = TextMeasureGdiplus::Create(hdc);
            strings = DoLayoutGdiplus(m, SAMPLE_TEXT, (float) rcClient.Size().dx);
            delete m;
        }
        ScopedGdiObj<HBRUSH> brushAboutBg(CreateSolidBrush(RGB(0xff, 0xff, 0xff)));
        FillRect(hdc, &rTmp, brushAboutBg);
        Pen                  debugPen(Color(255, 0, 0), 1);

        auto td = TextDrawGdiplus::Create(hdc);

        if (firstTime) {
            for (int i = 0; i < REPEAT_COUNT; i++) {
                // GDI+ does anti-aliasing so it would accumulate text over-time
                // if we didn't erase the background again
                FillRect(hdc, &rTmp, brushAboutBg);
                Timer timer(true);
                DrawStrings(td);
                gGdiplusDrawTimes[i] = timer.Stop();
            }
            firstTime = false;
        }
        else {
            DrawStrings(td);
        }

        // draw bounding-boxes to visualize correctness of text measurements
        for (size_t i = 0; i < strings->nStrings; i++) {
            auto ms = strings->GetMeasuredString(i);
            if (ms->IsNewline()) {
                continue;
            }
            auto& bb = ms->bb;
            td->gfx->DrawRectangle(&debugPen, bb);
        }
        delete td;
        EndPaint(m_window, &ps);
    }

    void PaintHandlerGdi()
    {
        static bool firstTime = true;

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_window, &ps);

        if (font == nullptr) {
            font = GetGdiFont();
            //font = CreateSimpleFont(hdc, FONT_NAME, (int) DpiScaled((float)PixelToPoint(FONT_SIZE)));
            CrashIf(!font);
        }

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkColor(hdc, RGB(0xff, 0xff, 0xff));

        HFONT oldfnt = SelectFont(hdc, font);

        ClientRect rcClient(m_window);
        RECT rTmp = rcClient.ToRECT();
        RECT r;
        if (!strings) {
            auto m = TextMeasureGdi::Create(hdc);
            strings = DoLayoutGdi(m, SAMPLE_TEXT, (float) rcClient.Size().dx);
            delete m;
        }
        ScopedGdiObj<HBRUSH> brushAboutBg(CreateSolidBrush(RGB(0xff, 0xff, 0xff)));
        FillRect(hdc, &rTmp, brushAboutBg);
        ScopedGdiObj<HBRUSH> brushDebugPen(CreateSolidBrush(RGB(255, 0, 0)));

        auto td = TextDrawGdi::Create(hdc);
        if (firstTime) {
            for (int i = 0; i < REPEAT_COUNT; i++) {
                // just to be sure, reset the background before each draw
                FillRect(hdc, &rTmp, brushAboutBg);
                Timer timer(true);
                DrawStrings(td);
                gGdiDrawTimes[i] = timer.Stop();
            }
            firstTime = false;
        }
        else {
            DrawStrings(td);
        }
        delete td;

        // draw bounding-boxes to visualize correctness of text measurements
        for (size_t i = 0; i < strings->nStrings; i++) {
            auto ms = strings->GetMeasuredString(i);
            if (ms->IsNewline()) {
                continue;
            }
            auto& bb = ms->bb;
            r.left = (int) bb.X;
            r.right = r.left + (int) bb.Width;
            r.top = (int) bb.Y;
            r.bottom = r.top + (int) bb.Height;
            FrameRect(hdc, &r, brushDebugPen);
        }

        SelectFont(hdc, oldfnt);
        EndPaint(m_window, &ps);
    }

    void PaintHandler()
    {
        if (renderType == RENDER_GDI) {
            PaintHandlerGdi();
        }
        else if (renderType == RENDER_GDI_PLUS) {
            PaintHandlerGdiplus();
        }
        else {
            CrashIf(true);
        }
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE,
                     _In_ LPWSTR,
                     _In_ int )
{
    hInst = hInstance;

    ScopedCom initCom;
    InitAllCommonControls();
    ScopedGdiPlus initGdiplus;

    MSG msg;
    HACCEL hAccelTable;

    SampleWindow window(RENDER_GDI, L"Gdi rendering");
    SampleWindow window2(RENDER_GDI_PLUS, L"Gdi+ rendering");

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RENDERSPEEDTEST));

    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DumpTimings();
    return (int) msg.wParam;
}
