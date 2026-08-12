/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Gfx is the drawing surface the VirtCtrl controls paint on. It exists so the
// controls don't name an OS imaging API: it's an abstract base class and each
// platform provides an implementation (GfxHdc wraps an HDC on Windows).
//
// Rects are in the coordinates of whatever the surface was created for (for
// VirtCtrl painting: HWND client coords).

// windows.h #defines DrawText to DrawTextW / DrawTextA and we want it as a
// method name. No file that includes us calls the win32 DrawText directly;
// use HdcDrawText() instead.
#ifdef DrawText
#undef DrawText
#endif

struct PlatformFont;
struct Pixmap;

// how text is placed in the rect it is drawn into
enum GfxTextFlags : u32 {
    gfxTextLeft = 0,
    gfxTextCenter = 1 << 0,
    gfxTextRight = 1 << 1,
    // single line, vertically centered, "…" when it doesn't fit
    gfxTextEllipsis = 1 << 2,
    gfxTextRtl = 1 << 3,
    // like gfxTextEllipsis, but the "…" goes in the middle, keeping the last
    // path component visible
    gfxTextPathEllipsis = 1 << 4,
};

struct Gfx {
    Gfx() = default;
    virtual ~Gfx() = default;

    // kColorUnset means "keep whatever color the surface is set to"
    virtual void FillRect(const Rect&, COLORREF) = 0;
    virtual void DrawLine(const Rect&, COLORREF, int thickness = 1) = 0;
    virtual void DrawText(Str, const Rect&, u32 flags, PlatformFont*, COLORREF = kColorUnset) = 0;
    // blends the pixmap's alpha over what is already there
    virtual void DrawPixmap(Pixmap*, const Rect&) = 0;
};

#if OS_WIN
struct GfxHdc : Gfx {
    HDC hdc = nullptr;

    GfxHdc() = default;
    explicit GfxHdc(HDC);
    ~GfxHdc() override = default;

    void FillRect(const Rect&, COLORREF) override;
    void DrawLine(const Rect&, COLORREF, int thickness = 1) override;
    void DrawText(Str, const Rect&, u32 flags, PlatformFont*, COLORREF = kColorUnset) override;
    void DrawPixmap(Pixmap*, const Rect&) override;
};

// for app code that still paints with win32 directly (custom controls,
// gdiplus, ...)
inline HDC GfxGetHdc(Gfx* gfx) {
    return gfx ? static_cast<GfxHdc*>(gfx)->hdc : nullptr;
}
#endif
