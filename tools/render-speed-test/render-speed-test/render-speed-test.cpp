#include "stdafx.h"

#include "render-speed-test.h"
#include "util.h"

using namespace Gdiplus;

// a program to test various ways of measuring and drawing text
// TODO:
// - calc & save timings
// - add GDI version
// - add DirectDraw version
// - re-layout when the window size changes

HINSTANCE hInst;

// TODO: static_assert(sizeof(i64)==8)
#define APP_TITLE       L"RenderSpeedTest"
#define APP_WIN_CLASS   L"RENDERSPEEDTEST_WIN_CLS"

// set consistent mode for our graphics objects so that we get
// the same results when measuring text
void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
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

    static TextMeasureGdiplus* Create();

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

TextMeasureGdiplus *TextMeasureGdiplus::Create() {
    auto res = new TextMeasureGdiplus();
    res->bmp = ::new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, res->data);
    if (!res->bmp)
        return nullptr;
    res->gfx = ::new Graphics((Image*) res->bmp);
    InitGraphicsMode(res->gfx);
    res->fnt = ::new Font(L"Tahoma", 10.0);
    return res;
}

class TextDrawGdiplus : public ITextDraw {
private:
    TextDrawGdiplus() : gfx(nullptr) { }
    Font *      fnt;
    Brush *     col;
    WCHAR       txtConvBuf[512];

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
    res->fnt = ::new Font(L"Tahoma", 10.0);
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

struct MeasuredString *g_strings = nullptr;
size_t g_nStrings = 0;

void FreeMeasuredStrings() {
    free((void*) g_strings);
    g_strings = nullptr;
    g_nStrings = 0;
}

MeasuredString *GetMeasuredString(size_t n) {
    if (g_nStrings >= MAX_STRINGS) {
        return nullptr;
    }
    return &g_strings[n];
}

MeasuredString *AllocMeasuredString(const char *s, size_t sLen, float dx, float dy) {
    if (nullptr == g_strings) {
        g_strings = AllocStruct<MeasuredString>(MAX_STRINGS);
    }
    g_nStrings++;
    auto ms = GetMeasuredString(g_nStrings-1);
    ms->s = s;
    ms->sLen = sLen;
    ms->bb.X = 0;
    ms->bb.Y = 0;
    ms->bb.Width = dx;
    ms->bb.Height = dy;
    return ms;
}

void MeasureStrings(ITextMeasure *m, char *s, size_t sLen) {
    
    IterWords(s, sLen, [&m](char *s, size_t sLen) {
        RectF bb = m->Measure(s, sLen);
        AllocMeasuredString(s, sLen, bb.Width, bb.Height);
    });
}

void LayoutStrings(float areaDx, float spaceDx, float lineDy) {
    float x = 0;
    float y = 0;
    float maxTextDy = lineDy; // for current line
    for (size_t i = 0; i < g_nStrings; i++) {
        auto ms = GetMeasuredString(i);
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

void DoLayout(char *s, float areaDx) {
    auto sLen = strlen(s);
    auto m = TextMeasureGdiplus::Create();
    MeasureStrings(m, s, sLen);
    float fontDy = m->GetFontHeight();
    float spaceDx = m->Measure(" ", 1).Width;
    delete m;
    LayoutStrings(areaDx, spaceDx, fontDy);
}

struct SampleWindow : Window<SampleWindow>
{
    SampleWindow()
    {
        WNDCLASS wc = {};

        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hInstance = hInst;
        wc.lpszClassName = L"Render_Speed_Test_Wnd_Cls";
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;

        RegisterClass(&wc);

        //ASSERT(!m_window);

        CreateWindowW(wc.lpszClassName,
            L"Render Speed Test",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 720, 480,
            nullptr,
            nullptr,
            wc.hInstance,
            this);

        //ASSERT(m_window);
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

    void PaintHandler()
    {
        static auto didLayout = false;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_window, &ps);

        ClientRect rcClient(m_window);
        RECT rTmp = rcClient.ToRECT();

        if (!didLayout) {
            DoLayout(SAMPLE_TEXT, (float)rcClient.Size().dx);
            didLayout = true;
        }
        ScopedGdiObj<HBRUSH> brushAboutBg(CreateSolidBrush(RGB(0xff, 0xff, 0xff)));
        FillRect(hdc, &rTmp, brushAboutBg);
        Pen                  debugPen(Color(255, 0, 0), 1);

        auto td = TextDrawGdiplus::Create(hdc);
        for (size_t i = 0; i < g_nStrings; i++) {
            auto ms = GetMeasuredString(i);
            if (ms->IsNewline()) {
                continue;
            }
            auto& bb = ms->bb;
            td->Draw(ms->s, ms->sLen, bb);
            td->gfx->DrawRectangle(&debugPen, bb);

        }
        delete td;
        EndPaint(m_window, &ps);
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

    SampleWindow window;

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RENDERSPEEDTEST));

    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    FreeMeasuredStrings();
    return (int) msg.wParam;
}
