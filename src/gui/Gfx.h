/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Gfx is the drawing surface the VirtCtrl controls paint on. It exists so the
// controls don't name an OS imaging API: it's an abstract base class and each
// platform provides an implementation (on Windows GfxHdc draws with gdi into an
// HDC and GfxGdiplus draws with gdiplus).
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

#if OS_WIN
namespace Gdiplus {
class Font;
class Graphics;
} // namespace Gdiplus
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
    virtual ~Gfx() = default;

    // kColorUnset means "keep whatever color the surface is set to"
    virtual void FillRect(const Rect&, COLORREF) = 0;
    // 1px-per-thickness outline drawn inside the rect
    virtual void DrawRect(const Rect&, COLORREF, int thickness = 1) = 0;
    // anti-aliased; either color can be kColorUnset to skip fill / border
    virtual void FillRoundedRect(const Rect&, int radius, COLORREF fill, COLORREF border = kColorUnset) = 0;
    virtual void FillEllipse(const Rect&, COLORREF, u8 alpha = 255) = 0;
    virtual void DrawLine(const Rect&, COLORREF, int thickness = 1) = 0;
    // anti-aliased line between two points, for diagonals
    virtual void DrawLineAA(Point, Point, COLORREF, float thickness = 1.0f, u8 alpha = 255) = 0;
    virtual void DrawFocusRect(const Rect&) = 0;

    virtual void DrawText(Str, const Rect&, u32 flags, PlatformFont*, COLORREF = kColorUnset) = 0;
    // the point is the top-left of the text (alignment flags don't apply)
    virtual void DrawTextAt(Str, Point, u32 flags, PlatformFont*, COLORREF = kColorUnset) = 0;
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
struct GfxHdc : Gfx {
    HDC hdc = nullptr;
    Vec<int> savedDCs; // one per PushClip()

    GfxHdc() = default;
    explicit GfxHdc(HDC);
    ~GfxHdc() override = default;

    void FillRect(const Rect&, COLORREF) override;
    void DrawRect(const Rect&, COLORREF, int thickness = 1) override;
    void FillRoundedRect(const Rect&, int radius, COLORREF fill, COLORREF border = kColorUnset) override;
    void FillEllipse(const Rect&, COLORREF, u8 alpha = 255) override;
    void DrawLine(const Rect&, COLORREF, int thickness = 1) override;
    void DrawLineAA(Point, Point, COLORREF, float thickness = 1.0f, u8 alpha = 255) override;
    void DrawFocusRect(const Rect&) override;
    void DrawText(Str, const Rect&, u32 flags, PlatformFont*, COLORREF = kColorUnset) override;
    void DrawTextAt(Str, Point, u32 flags, PlatformFont*, COLORREF = kColorUnset) override;
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
    COLORREF textColor = 0;
    Vec<u32> savedStates; // one per PushClip()

    GfxGdiplus() = default;
    explicit GfxGdiplus(HDC);
    explicit GfxGdiplus(Gdiplus::Graphics*);
    ~GfxGdiplus() override;

    void FillRect(const Rect&, COLORREF) override;
    void DrawRect(const Rect&, COLORREF, int thickness = 1) override;
    void FillRoundedRect(const Rect&, int radius, COLORREF fill, COLORREF border = kColorUnset) override;
    void FillEllipse(const Rect&, COLORREF, u8 alpha = 255) override;
    void DrawLine(const Rect&, COLORREF, int thickness = 1) override;
    void DrawLineAA(Point, Point, COLORREF, float thickness = 1.0f, u8 alpha = 255) override;
    void DrawFocusRect(const Rect&) override;
    void DrawText(Str, const Rect&, u32 flags, PlatformFont*, COLORREF = kColorUnset) override;
    void DrawTextAt(Str, Point, u32 flags, PlatformFont*, COLORREF = kColorUnset) override;
    Size MeasureText(Str, PlatformFont*) override;
    void DrawPixmap(Pixmap*, const Rect&) override;
    void PushClip(const Rect&) override;
    void PopClip() override;
    bool SetMirrored(bool) override;

    Gdiplus::Font* GetGdiplusFont(PlatformFont*, bool* owned);
};
#endif
