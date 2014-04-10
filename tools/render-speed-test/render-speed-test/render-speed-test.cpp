#include "stdafx.h"

#include "render-speed-test.h"
#include "util.h"

using namespace Gdiplus;

// a program to test various ways of measuring and drawing text
// TODO:
// - calc & save timings
// - add DirectDraw version
// - re-layout when the window size changes

HINSTANCE hInst;

// TODO: static_assert(sizeof(i64)==8)
#define APP_TITLE       L"RenderSpeedTest"
#define APP_WIN_CLASS   L"RENDERSPEEDTEST_WIN_CLS"

#define FONT_NAME       L"Tahoma"
#define FONT_SIZE       10

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
    res->fnt = ::new Font(FONT_NAME, FONT_SIZE);
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
    /*
    res->gfx = ::new Graphics(dc);
    InitGraphicsMode(res->gfx);
    res->fnt = ::new Font(L"Tahoma", 10.0);
    res->col = ::new SolidBrush(Color(0, 0, 0));
    */
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
    res->fnt = ::new Font(FONT_NAME, FONT_SIZE);
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

MeasuredStrings *MeasureStrings(ITextMeasure *m, char *s, size_t sLen) {
    auto res = new MeasuredStrings();
    IterWords(s, sLen, [&m,&res](char *s, size_t sLen) {
        RectF bb = m->Measure(s, sLen);
        res->AllocMeasuredString(s, sLen, bb.Width, bb.Height);
    });
    return res;
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
    auto strings = MeasureStrings(m, s, sLen);
    float fontDy = m->GetFontHeight();
    float spaceDx = m->Measure(" ", 1).Width;
    LayoutStrings(strings, areaDx, spaceDx, fontDy);
    return strings;
}

MeasuredStrings * DoLayoutGdi(TextMeasureGdi *m, char *s, float areaDx) {
    auto sLen = strlen(s);
    auto strings = MeasureStrings(m, s, sLen);
    //float fontDy = m->GetFontHeight();
    float fontDy = 12.0f; // TODO: use real font height
    float spaceDx = m->Measure(" ", 1).Width;
    LayoutStrings(strings, areaDx, spaceDx, fontDy);
    return strings;
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

    void PaintHandlerGdiplus()
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_window, &ps);

        ClientRect rcClient(m_window);
        RECT rTmp = rcClient.ToRECT();

        if (!strings) {
            auto m = TextMeasureGdiplus::Create();
            strings = DoLayoutGdiplus(m, SAMPLE_TEXT, (float) rcClient.Size().dx);
            delete m;
        }
        ScopedGdiObj<HBRUSH> brushAboutBg(CreateSolidBrush(RGB(0xff, 0xff, 0xff)));
        FillRect(hdc, &rTmp, brushAboutBg);
        Pen                  debugPen(Color(255, 0, 0), 1);

        auto td = TextDrawGdiplus::Create(hdc);
        for (size_t i = 0; i < strings->nStrings; i++) {
            auto ms = strings->GetMeasuredString(i);
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

    void PaintHandlerGdi()
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_window, &ps);

        if (font == nullptr) {
            font = CreateSimpleFont(hdc, FONT_NAME, (int) DpiScaled((float)PixelToPoint(FONT_SIZE)));
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
        for (size_t i = 0; i < strings->nStrings; i++) {
            auto ms = strings->GetMeasuredString(i);
            if (ms->IsNewline()) {
                continue;
            }
            auto& bb = ms->bb;
            td->Draw(ms->s, ms->sLen, bb);
            r.left = (int) bb.X;
            r.right = r.left + (int) bb.Width;
            r.top = (int) bb.Y;
            r.bottom = r.top + (int) bb.Height;
            FrameRect(hdc, &r, brushDebugPen);
        }
        delete td;
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

    return (int) msg.wParam;
}
