#include "stdafx.h"

#include "render-speed-test.h"
#include "util.h"

using namespace Gdiplus;

// a program to test various ways of measuring and drawing text
// TODO:
// - load a file with text to test
// - do layout and display
// - test GDI+
// - test GDI
// - test DirectDraw

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
    Bitmap *    bmp;
    Font *      fnt;
    BYTE        data[bmpDx * bmpDy * 4];
    WCHAR       txtConvBuf[512];
    TextMeasureGdiplus() : gfx(nullptr), bmp(nullptr), fnt(nullptr) {}

public:
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
    // TODO: allocate font
    return res;
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
};

#define MAX_STRINGS 4096

struct MeasuredString *g_strings = nullptr;
int g_nStrings = 0;

void FreeMeasuredStrings() {
    free((void*) g_strings);
    g_strings = nullptr;
    g_nStrings = 0;
}

MeasuredString *AllocMeasuredString(const char *s, size_t sLen, float dx, float dy) {
    if (nullptr == g_strings) {
        g_strings = AllocStruct<MeasuredString>(MAX_STRINGS);
    }
    if (g_nStrings >= MAX_STRINGS) {
        return nullptr;
    }
    auto ms = &g_strings[g_nStrings++];
    ms->s = s;
    ms->sLen = sLen;
    ms->bb.X = 0;
    ms->bb.Y = 0;
    ms->bb.Width = dx;
    ms->bb.Height = dy;
    return ms;
}

void MeasureStrings(char *s, size_t sLen) {
    auto m = TextMeasureGdiplus::Create();
    IterWords(s, sLen, [&](char *s, size_t sLen) {
        RectF bb = m->Measure(s, sLen);
        AllocMeasuredString(s, sLen, bb.Width, bb.Height);
    });
    delete m;
}


#if 0
void LayoutStrings(float areaDx) {
    float x = 0;
    float y = 0;
    float maxTextDy = 0; // for current line
    for (i = 0; i < g_nStrings; i++) {
        float textDx = size.Width;
        float textDy = size.Height;
        if (textDy > maxTextDy) {
            maxTextDy = textDy;
        }
        if (x + textDx > areaDx) {
            bool firstWordInLine = (x == 0);
            if (firstWordInLine) {

            }
            if (!firstWordInLine) {
                y += maxTextDy + 2;
                maxTextDy = textDy;
                x = 0;
            }

            y += maxTextDy + 2;
            maxTextDy = 0;
        }
    }
}
#endif

void DoLayout(char *s, float /*areaDx*/) {
    auto sLen = strlen(s);
    MeasureStrings(s, sLen);
    //LayoutStrings(areaDx);
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
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
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

        return Window::MessageHandler(message, wparam, lparam);
    }

    void PaintHandler()
    {
        static auto didLayout = false;
        if (!didLayout) {
            // TODO: get the real dx of the window
            DoLayout(SAMPLE_TEXT, 640.f);
            didLayout = true;
        }
#if 0
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_window, &ps);
        // TODO: draw
        EndPaint(m_window, &ps);
#endif
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE,
                     _In_ LPWSTR,
                     _In_ int )
{
    hInst = hInstance;

    //MSG msg;
    //HACCEL hAccelTable;

    SampleWindow window;
    MSG message;

    while (GetMessage(&message, nullptr, 0, 0))
    {
        DispatchMessage(&message);
    }

    /*
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
    */
    FreeMeasuredStrings();
    return 0;
}
