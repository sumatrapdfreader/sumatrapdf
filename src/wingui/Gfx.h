/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Gfx is the drawing surface the VirtWnd controls paint on. It exists so the
// controls don't name an OS imaging API: on Windows it wraps an HDC, elsewhere
// it will wrap the platform's equivalent. Like Pixmap, the backing is described
// per-platform with #if rather than an opaque handle.
//
// Rects are in the coordinates of whatever the surface was created for (for
// VirtWnd painting: HWND client coords).

struct PlatformFont;
struct Pixmap;

struct Gfx {
#if OS_WIN
    HDC hdc = nullptr;
#endif
};

// how text is placed in the rect it is drawn into
enum GfxTextFlags : u32 {
    gfxTextLeft = 0,
    gfxTextCenter = 1 << 0,
    gfxTextRight = 1 << 1,
    // single line, vertically centered, "…" when it doesn't fit
    gfxTextEllipsis = 1 << 2,
    gfxTextRtl = 1 << 3,
};

// kColorUnset means "keep whatever color the surface is set to"
void GfxFillRect(Gfx*, const Rect&, COLORREF);
void GfxDrawLine(Gfx*, const Rect&, COLORREF, int thickness = 1);
void GfxDrawText(Gfx*, Str, const Rect&, u32 flags, PlatformFont*, COLORREF = kColorUnset);
// blends the pixmap's alpha over what is already there
void GfxDrawPixmap(Gfx*, Pixmap*, const Rect&);

#if OS_WIN
Gfx GfxFromHdc(HDC);
// for app code that still paints with win32 directly (custom controls,
// gdiplus, ...)
inline HDC GfxHdc(Gfx* gfx) {
    return gfx ? gfx->hdc : nullptr;
}
#endif
