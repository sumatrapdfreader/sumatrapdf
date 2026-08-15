/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Gfx is the drawing surface the VirtCtrl controls paint on. It exists so the
// controls don't name an OS imaging API: it's an abstract base class and each
// platform provides an implementation. On Windows the virtual-control paint
// paths get one from GfxCreate(): Direct2D when available, gdiplus otherwise,
// both anti-aliased. GfxHdc (plain gdi, immediate) survives for surfaces that
// mix raw gdi drawing with Gfx drawing; see its comment.
//
// Rects are in the coordinates of whatever the surface was created for (for
// VirtCtrl painting: HWND client coords).
//
// The API is stateless apart from the clip and the mirroring: every call takes
// the font and the color it draws with, so painting code never has to save and
// restore surface state.

// windows.h #defines DrawText to DrawTextW / DrawTextA and we want it as a
// method name. No file that includes us calls the win32 DrawText directly;
// use HdcDrawText() instead.
#ifdef DrawText
#undef DrawText
#endif

struct PlatformFont;
struct Pixmap;
struct HwndBase;

#if OS_WIN
namespace Gdiplus {
class Font;
class Graphics;
} // namespace Gdiplus

struct ID2D1DCRenderTarget;
struct ID2D1SolidColorBrush;
struct ID2D1StrokeStyle;
#elif OS_LINUX
struct _cairo;
using cairo_t = struct _cairo;
#endif

// how text is placed in the rect it is drawn into
enum GfxTextFlags : u32 {
    gfxTextLeft = 0,
    gfxTextCenter = 1 << 0,
    gfxTextRight = 1 << 1,
    // single line, "…" at the end when it doesn't fit
    gfxTextEllipsis = 1 << 2,
    gfxTextRtl = 1 << 3,
    // like gfxTextEllipsis, but the "…" goes in the middle, keeping the last
    // path component visible
    gfxTextPathEllipsis = 1 << 4,
    // newlines don't start a new line
    gfxTextSingleLine = 1 << 5,
    // vertically centered in the rect; implies single line
    gfxTextVCenter = 1 << 6,
    // the rect only positions the text; drawing isn't clipped to it
    gfxTextNoClip = 1 << 7,
    // wraps at word boundaries to the width of the rect. With gfxTextEllipsis
    // the "…" replaces the tail of a word too long to wrap
    gfxTextWrap = 1 << 8,
};

struct Gfx {
    Gfx() = default;
#if OS_WIN
    virtual ~Gfx();
    HDC doubleBufferTarget = nullptr;
    HDC doubleBufferSource = nullptr;
    Size doubleBufferSize;
#else
    virtual ~Gfx() = default;
#endif

    // kColorTransparent / kColorUnset skip the fill
    virtual void FillRect(const Rect&, Color) = 0;
    // fills the union of the rectangles; the optional outline follows its outside edge
    virtual void FillRects(const Rect*, int count, Color, u8 alpha = 255, int outlineWidth = 0) = 0;
    // 1px-per-thickness outline drawn inside the rect
    virtual void DrawRect(const Rect&, Color, int thickness = 1) = 0;
    virtual void DrawDashedRect(const Rect&, Color) = 0;
    // anti-aliased; transparent / unset skip fill / border
    virtual void FillRoundedRect(const Rect&, int radius, Color fill, Color border = kColorTransparent) = 0;
    virtual void FillEllipse(const Rect&, Color, u8 alpha = 255) = 0;
    virtual void DrawLine(const Rect&, Color, int thickness = 1) = 0;
    // anti-aliased line between two points, for diagonals
    virtual void DrawLineAA(Point, Point, Color, float thickness = 1.0f, u8 alpha = 255) = 0;
    virtual void DrawFocusRect(const Rect&) = 0;

    virtual void DrawText(Str, const Rect&, u32 flags, PlatformFont*, Color = kColorUnset) = 0;
    // the point is the top-left of the text (alignment flags don't apply)
    virtual void DrawTextAt(Str, Point, u32 flags, PlatformFont*, Color = kColorUnset) = 0;
    virtual Size MeasureText(Str, PlatformFont*) = 0;

    // blends the pixmap's alpha over what is already there
    virtual void DrawPixmap(Pixmap*, const Rect&) = 0;

    // clips to the intersection of the rect and the current clip; nests
    virtual void PushClip(const Rect&) = 0;
    virtual void PopClip() = 0;

    // returns the previous setting. Painting a row we lay out right-to-left
    // ourselves turns mirroring off so the glyphs don't come out reversed
    virtual bool SetMirrored(bool) = 0;
};

#if OS_WIN
// Draws with plain gdi, immediately and without anti-aliasing. Kept for the
// surfaces that interleave raw gdi drawing with Gfx drawing on the same HDC
// (the caption frame and installer): GfxDirect2D writes to the
// HDC only when destroyed and would overwrite the gdi-drawn content, and only
// gdi honors the DC's world transform (an offset DoubleBuffer). Everything
// that draws purely through Gfx should use GfxCreate() instead.
struct GfxHdc : Gfx {
    HDC hdc = nullptr;
    Vec<int> savedDCs; // one per PushClip()

    GfxHdc() = default;
    explicit GfxHdc(HDC);
    ~GfxHdc() override = default;

    void FillRect(const Rect&, Color) override;
    void FillRects(const Rect*, int count, Color, u8 alpha = 255, int outlineWidth = 0) override;
    void DrawRect(const Rect&, Color, int thickness = 1) override;
    void DrawDashedRect(const Rect&, Color) override;
    void FillRoundedRect(const Rect&, int radius, Color fill, Color border = kColorTransparent) override;
    void FillEllipse(const Rect&, Color, u8 alpha = 255) override;
    void DrawLine(const Rect&, Color, int thickness = 1) override;
    void DrawLineAA(Point, Point, Color, float thickness = 1.0f, u8 alpha = 255) override;
    void DrawFocusRect(const Rect&) override;
    void DrawText(Str, const Rect&, u32 flags, PlatformFont*, Color = kColorUnset) override;
    void DrawTextAt(Str, Point, u32 flags, PlatformFont*, Color = kColorUnset) override;
    Size MeasureText(Str, PlatformFont*) override;
    void DrawPixmap(Pixmap*, const Rect&) override;
    void PushClip(const Rect&) override;
    void PopClip() override;
    bool SetMirrored(bool) override;
};

// Draws with gdiplus, so shapes and text are anti-aliased. Still needs an HDC
// to draw into (that's how gdiplus reaches a window), but nothing outside this
// class uses gdi to draw.
struct GfxGdiplus : Gfx {
    HDC hdc = nullptr;
    Gdiplus::Graphics* gfx = nullptr;
    bool ownsGfx = false;
    // gdiplus has no "current text color", so kColorUnset resolves to this
    Color textColor = 0;
    Vec<u32> savedStates; // one per PushClip()

    GfxGdiplus() = default;
    explicit GfxGdiplus(HDC);
    explicit GfxGdiplus(Gdiplus::Graphics*);
    ~GfxGdiplus() override;

    void FillRect(const Rect&, Color) override;
    void FillRects(const Rect*, int count, Color, u8 alpha = 255, int outlineWidth = 0) override;
    void DrawRect(const Rect&, Color, int thickness = 1) override;
    void DrawDashedRect(const Rect&, Color) override;
    void FillRoundedRect(const Rect&, int radius, Color fill, Color border = kColorTransparent) override;
    void FillEllipse(const Rect&, Color, u8 alpha = 255) override;
    void DrawLine(const Rect&, Color, int thickness = 1) override;
    void DrawLineAA(Point, Point, Color, float thickness = 1.0f, u8 alpha = 255) override;
    void DrawFocusRect(const Rect&) override;
    void DrawText(Str, const Rect&, u32 flags, PlatformFont*, Color = kColorUnset) override;
    void DrawTextAt(Str, Point, u32 flags, PlatformFont*, Color = kColorUnset) override;
    Size MeasureText(Str, PlatformFont*) override;
    void DrawPixmap(Pixmap*, const Rect&) override;
    void PushClip(const Rect&) override;
    void PopClip() override;
    bool SetMirrored(bool) override;

    Gdiplus::Font* GetGdiplusFont(PlatformFont*, bool* owned);
};

struct ID2D1DCRenderTarget;
struct ID2D1SolidColorBrush;
struct ID2D1StrokeStyle;

// Draws with Direct2D and lays text out with DirectWrite. Like GfxGdiplus it
// draws into an HDC, so it fits the same places; the render target is bound at
// 96 dpi, so a DIP is a pixel and the caller's coordinates go through unscaled.
// Everything is a no-op when Direct2DAvailable() is false.
struct GfxDirect2D : Gfx {
    HDC hdc = nullptr;
    ID2D1DCRenderTarget* target = nullptr; // owned; null when d2d isn't there
    ID2D1SolidColorBrush* brush = nullptr; // owned, re-colored per draw
    ID2D1StrokeStyle* dottedStroke = nullptr;
    bool drawing = false; // between BeginDraw() and EndDraw()
    // d2d has no "current text color", so kColorUnset resolves to this
    Color textColor = 0;
    Vec<u8> clipDepth; // one per PushClip()

    GfxDirect2D() = default;
    explicit GfxDirect2D(HDC);
    ~GfxDirect2D() override;

    void FillRect(const Rect&, Color) override;
    void FillRects(const Rect*, int count, Color, u8 alpha = 255, int outlineWidth = 0) override;
    void DrawRect(const Rect&, Color, int thickness = 1) override;
    void DrawDashedRect(const Rect&, Color) override;
    void FillRoundedRect(const Rect&, int radius, Color fill, Color border = kColorTransparent) override;
    void FillEllipse(const Rect&, Color, u8 alpha = 255) override;
    void DrawLine(const Rect&, Color, int thickness = 1) override;
    void DrawLineAA(Point, Point, Color, float thickness = 1.0f, u8 alpha = 255) override;
    void DrawFocusRect(const Rect&) override;
    void DrawText(Str, const Rect&, u32 flags, PlatformFont*, Color = kColorUnset) override;
    void DrawTextAt(Str, Point, u32 flags, PlatformFont*, Color = kColorUnset) override;
    Size MeasureText(Str, PlatformFont*) override;
    void DrawPixmap(Pixmap*, const Rect&) override;
    void PushClip(const Rect&) override;
    void PopClip() override;
    bool SetMirrored(bool) override;

    ID2D1SolidColorBrush* GetBrush(Color, u8 alpha = 255);
};

bool Direct2DAvailable();

// flip to draw with Direct2D instead of gdiplus, for comparing the two
extern bool gUseDirect2D;

Gfx* GfxCreate(HDC);
Gfx* GfxCreateWithDoubleBuffer(HwndBase*, HDC);
void GfxDestroyDoubleBuffer(HwndBase*);
#endif

#if OS_LINUX
Gfx* GfxCreate(cairo_t*);
#elif OS_DARWIN
Gfx* GfxCreate(void*);
#endif
