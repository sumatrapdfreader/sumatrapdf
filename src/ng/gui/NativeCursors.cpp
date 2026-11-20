/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's cursors that are bitmaps: the hand of a drag-pan (dragcursor.cur),
// the laser pointer dot (Canvas.cpp) and the note / ink placement cursors made
// from the annotation's SVG icon (AnnotPlacement.cpp). ng renders them to
// pixels and gives them to gpui as cursor images, on every platform.

#include "gui/GpuiBridge.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EmbeddedResources.h"
#include "Theme.h"
#include "AppSettings.h"
#include "TextSelection.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "SvgIcons.h"
#include "mupdf/fitz.h"
#include "ImageReader.h"
#include "LaserPointerCursor.h"
#include "gui/NativeCursors.h"

#include "SumatraLog.h"

// SM_CXCURSOR at 96 DPI
constexpr int kSysCursorSize = 32;

// one cursor image per NativeCursor, re-made when its size or color changes
struct NativeCursorCache {
    gp::CursorImage* img = nullptr;
    int size = 0;
    Color color = 0;
};

static NativeCursorCache gCursors[(int)NativeCursor::InkAnnotationPlacement + 1];

// premultiplied RGBA, size*size, top row first
struct CursorPixels {
    u8* rgba = nullptr;
    int size = 0;
    int hotX = 0;
    int hotY = 0;
};

static void CursorPixelsAlloc(CursorPixels& px, int size) {
    px.size = size;
    px.rgba = AllocArray<u8>((size_t)size * size * 4);
}

// --- the drag hand (orig's dragcursor.cur) ----------------------------------

// dragcursor.cur is a 32x32 monochrome cursor with the hot spot at (6, 4):
// '#' black, 'o' white, anything else transparent; rows below are empty
static const char* kDragHand[] = {
    "  ## ## ##   ", //
    " #oo#oo#oo## ", //
    " #oooooooo#o#", //
    "  #ooooooooo#", //
    " ##ooooooooo#", //
    "#ooooooooooo#", //
    "#oooooooooo# ", //
    " #ooooooooo# ", //
    "  #ooooooo#  ", //
    "   #oooooo#  ", //
    "   #oooooo#  ", //
};
constexpr int kDragCurSize = 32;
constexpr int kDragHotX = 6;
constexpr int kDragHotY = 4;

// Windows scales a monochrome cursor to the system cursor size; nearest
// neighbor keeps its hard edges
static bool DragHandPixels(CursorPixels& px, int size) {
    CursorPixelsAlloc(px, size);
    px.hotX = (kDragHotX * size) / kDragCurSize;
    px.hotY = (kDragHotY * size) / kDragCurSize;
    int nRows = (int)dimof(kDragHand);
    for (int y = 0; y < size; y++) {
        int sy = (y * kDragCurSize) / size;
        if (sy >= nRows) {
            continue;
        }
        const char* row = kDragHand[sy];
        int rowLen = (int)strlen(row);
        for (int x = 0; x < size; x++) {
            int sx = (x * kDragCurSize) / size;
            char c = sx < rowLen ? row[sx] : ' ';
            if (c != '#' && c != 'o') {
                continue;
            }
            u8 v = c == 'o' ? 0xff : 0;
            u8* d = px.rgba + (((size_t)y * size) + x) * 4;
            d[0] = v;
            d[1] = v;
            d[2] = v;
            d[3] = 0xff;
        }
    }
    return true;
}

// --- the laser pointer (orig's Canvas.cpp) ----------------------------------

// the hotspot is the center of the dot, unlike an arrow's tip
static bool LaserPointerPixels(CursorPixels& px, int size) {
    CursorPixelsAlloc(px, size);
    px.hotX = size / 2;
    px.hotY = size / 2;
    u32* argb = AllocArray<u32>((size_t)size * size);
    LaserPointerDotPixels(argb, size);
    for (int i = 0; i < size * size; i++) {
        u32 c = argb[i];
        u8* d = px.rgba + (size_t)i * 4;
        d[0] = (u8)(c >> 16);
        d[1] = (u8)(c >> 8);
        d[2] = (u8)c;
        d[3] = (u8)(c >> 24);
    }
    free(argb);
    return true;
}

// --- annotation placement (orig's AnnotPlacement.cpp) -----------------------

// A custom ToolbarSvgIcon comes from the settings file, so it can be malformed:
// a typo, or the file caught half-written by the settings watcher while the user
// is editing it. mupdf signals that by throwing, and an uncaught mupdf exception
// aborts the whole process, so everything here has to be inside fz_try.
static fz_pixmap* RenderSvgToFzPixmap(fz_context* ctx, Str svgData, int dx, int dy, Color fgCol) {
    TempStr strokeCol = SerializeColorTemp(fgCol);
    TempStr svg = str::ReplaceTemp(svgData, StrL("currentColor"), strokeCol);

    fz_buffer* buf = nullptr;
    fz_display_list* list = nullptr;
    fz_device* dev = nullptr;
    fz_pixmap* pixmap = nullptr;
    fz_var(buf);
    fz_var(list);
    fz_var(dev);
    fz_var(pixmap);
    fz_try(ctx) {
        buf = fz_new_buffer_from_copied_data(ctx, (u8*)svg.s, svg.len);
        float svgWidth = 0;
        float svgHeight = 0;
        list = fz_new_display_list_from_svg(ctx, buf, nullptr, nullptr, &svgWidth, &svgHeight);
        pixmap = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_make_irect(0, 0, dx, dy), nullptr, 1);
        fz_clear_pixmap(ctx, pixmap);
        dev = fz_new_draw_device(ctx, fz_scale((float)dx / svgWidth, (float)dy / svgHeight), pixmap);
        fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, nullptr);
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
        dev = nullptr;
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_display_list(ctx, list);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, pixmap);
        fz_report_error(ctx);
        logf("RenderSvgToFzPixmap: rendering svg icon failed with: '%s'\n", Str(fz_caught_message(ctx)));
        return nullptr;
    }
    return pixmap;
}

// ng: orig takes the icon's pixels from its SVG pixmap cache
// (GetCachedPixmapForSvg); the port has no such cache because gpui draws the
// icons itself, so the icon is rendered here, transparent where the SVG left
// the background
static bool SvgIconPixels(CursorPixels& px, const char* icon, int size, Color color) {
    // an icon with a <text> needs a base14 font, which mupdf only gets from the
    // embedded archive
    static fz_context* ctx = nullptr;
    if (!ctx) {
        InstallEmbeddedFontLoader();
        ctx = fz_new_context_windows();
    }
    fz_pixmap* pixmap = RenderSvgToFzPixmap(ctx, Str(icon), size, size, color);
    if (!pixmap) {
        return false;
    }
    CursorPixelsAlloc(px, size);
    int srcN = pixmap->n;
    for (int y = 0; y < size; y++) {
        u8* s = pixmap->samples + (pixmap->stride * y);
        u8* d = px.rgba + ((size_t)y * size * 4);
        for (int x = 0; x < size; x++) {
            u8 a = pixmap->alpha ? s[srcN - 1] : 0xff;
            d[0] = a ? s[0] : 0;
            d[1] = a ? s[1] : 0;
            d[2] = a ? s[2] : 0;
            d[3] = a;
            d += 4;
            s += srcN;
        }
    }
    fz_drop_pixmap(ctx, pixmap);
    return true;
}

// orig: max(ToolbarIconSize(), SM_CXCURSOR), both for the window's DPI
static int PlacementCursorDipSize() {
    return std::max(RoundUp(gSettings->toolbarSize, 4), kSysCursorSize);
}

// --- the cache ----------------------------------------------------------------

static bool RenderNativeCursor(CursorPixels& px, NativeCursor cursor, int size, Color color) {
    switch (cursor) {
        case NativeCursor::Drag:
            return DragHandPixels(px, size);
        case NativeCursor::LaserPointer:
            return LaserPointerPixels(px, size);
        case NativeCursor::TextAnnotationPlacement:
            return SvgIconPixels(px, gIconAnnotText, size, color);
        case NativeCursor::InkAnnotationPlacement:
            if (!SvgIconPixels(px, gIconEditAnnotations, size, color)) {
                return false;
            }
            px.hotX = (4 * size) / 24;
            px.hotY = (20 * size) / 24;
            return true;
        default:
            return false;
    }
}

// The image is in device pixels for the window's scale and is re-made when the
// window moves to a monitor with a different scale or the theme changes color.
gp::CursorImage* NativeCursorGet(MainWindow* win, NativeCursor cursor) {
    gp::Window* gw = win ? win->gpuiWin : nullptr;
    int idx = (int)cursor;
    if (!gw || idx <= 0 || idx >= (int)dimof(gCursors)) {
        return nullptr;
    }
    float scale = gp::WindowScaleFactor(gw);
    bool placement = cursor == NativeCursor::TextAnnotationPlacement || cursor == NativeCursor::InkAnnotationPlacement;
    int dip =
        placement ? PlacementCursorDipSize() : (cursor == NativeCursor::Drag ? kDragCurSize : kLaserPointerCursorSize);
    int size = (int)(((float)dip * scale) + 0.5f);
    Color color = placement ? ThemeWindowTextColor() : 0;

    NativeCursorCache& c = gCursors[idx];
    if (c.img && c.size == size && c.color == color) {
        return c.img;
    }
    CursorPixels px;
    bool ok = RenderNativeCursor(px, cursor, size, color);
    gp::CursorImage* img = ok ? gp::CursorImageNew(gw->app, px.rgba, px.size, px.size, px.hotX, px.hotY) : nullptr;
    free(px.rgba);
    if (!img) {
        // a cursor of the wrong size beats no cursor at all
        return c.img;
    }
    gp::CursorImageFree(c.img);
    c.img = img;
    c.size = size;
    c.color = color;
    return img;
}

// before AppFree: a cursor image belongs to the app
void NativeCursorsDelete() {
    for (NativeCursorCache& c : gCursors) {
        gp::CursorImageFree(c.img);
        c = NativeCursorCache{};
    }
}
