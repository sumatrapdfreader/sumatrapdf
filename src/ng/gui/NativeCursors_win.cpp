/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's cursors that are bitmaps: the hand of a drag-pan (dragcursor.cur),
// the laser pointer dot (Canvas.cpp) and the note / ink placement cursors made
// from the annotation's SVG icon (AnnotPlacement.cpp).
// ng: gpui's CursorKind is a fixed list with no cursor from an image, so the
// canvas asks gpui for a stand-in kind and names the cursor it means in
// MainWindow::nativeCursor; the frame's subclass (gui/NativeWindow.cpp) sets
// the real one. Windows only; elsewhere the canvas keeps a crosshair / a
// painted dot.

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include <mupdf/fitz.h>

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "ImageReader.h"
#include "EmbeddedResources.h"
#include "Theme.h"
#include "AppSettings.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "SvgIcons.h"
#include "Toolbar.h"
#include "resource.h"
#include "gui/NativeCursors.h"
#include "LaserPointerCursor.h"

#include "SumatraLog.h"

// --- the laser pointer (orig's Canvas.cpp) ---------------------------------

// --- annotation placement (orig's AnnotPlacement.cpp) ----------------------

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
        logf("CreateSvgPlacementCursor: rendering svg icon failed with: '%s'\n", Str(fz_caught_message(ctx)));
        return nullptr;
    }
    return pixmap;
}

static HCURSOR gCursorTextAnnotationPlacement = nullptr;
static int gCursorTextAnnotationPlacementDx = 0;
static int gCursorTextAnnotationPlacementDy = 0;
static Color gCursorTextAnnotationPlacementColor = 0;

static HCURSOR gCursorInkAnnotationPlacement = nullptr;
static int gCursorInkAnnotationPlacementDx = 0;
static int gCursorInkAnnotationPlacementDy = 0;
static Color gCursorInkAnnotationPlacementColor = 0;

// ng: orig takes the icon's pixels from its SVG pixmap cache
// (GetCachedPixmapForSvg); the port has no such cache because gpui draws the
// icons itself, so the icon is rendered here: BGRA, alpha-premultiplied,
// transparent where the SVG left the background
static HBITMAP RenderSvgToBitmap(const char* icon, int dx, int dy, Color color) {
    // an icon with a <text> needs a base14 font, which mupdf only gets from the
    // embedded archive
    static fz_context* ctx = nullptr;
    if (!ctx) {
        InstallEmbeddedFontLoader();
        ctx = fz_new_context_windows();
    }
    fz_pixmap* pixmap = RenderSvgToFzPixmap(ctx, Str(icon), dx, dy, color);
    if (!pixmap) {
        return nullptr;
    }
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dx;
    bmi.bmiHeader.biHeight = -dy; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (hbmp && bits) {
        int srcN = pixmap->n;
        for (int y = 0; y < dy; y++) {
            u8* s = pixmap->samples + (pixmap->stride * y);
            u8* d = (u8*)bits + ((size_t)y * dx * 4);
            for (int x = 0; x < dx; x++) {
                u8 a = pixmap->alpha ? s[srcN - 1] : 0xff;
                d[0] = a ? s[2] : 0;
                d[1] = a ? s[1] : 0;
                d[2] = a ? s[0] : 0;
                d[3] = a;
                d += 4;
                s += srcN;
            }
        }
    }
    fz_drop_pixmap(ctx, pixmap);
    return hbmp;
}

static HCURSOR CreateSvgPlacementCursor(const char* icon, int dx, int dy, Color color, DWORD hotspotX, DWORD hotspotY) {
    HBITMAP hbmpColor = RenderSvgToBitmap(icon, dx, dy, color);
    if (!hbmpColor) {
        return nullptr;
    }

    int maskBytesPerRow = ((dx + 15) / 16) * 2;
    u8* maskBits = AllocArray<u8>(maskBytesPerRow * dy);
    HBITMAP hbmpMask = CreateBitmap(dx, dy, 1, 1, maskBits);
    free(maskBits);
    if (!hbmpMask) {
        DeleteObject(hbmpColor);
        return nullptr;
    }

    ICONINFO ii{};
    ii.fIcon = FALSE;
    ii.xHotspot = hotspotX;
    ii.yHotspot = hotspotY;
    ii.hbmMask = hbmpMask;
    ii.hbmColor = hbmpColor;
    HCURSOR cursor = (HCURSOR)CreateIconIndirect(&ii);
    DeleteObject(hbmpMask);
    DeleteObject(hbmpColor);
    return cursor;
}

static HCURSOR GetTextAnnotationPlacementCursor() {
    int dx = std::max(ToolbarIconSize(), DpiGetSystemMetrics(SM_CXCURSOR));
    int dy = std::max(ToolbarIconSize(), DpiGetSystemMetrics(SM_CYCURSOR));
    Color color = ThemeWindowTextColor();
    if (gCursorTextAnnotationPlacement && dx == gCursorTextAnnotationPlacementDx &&
        dy == gCursorTextAnnotationPlacementDy && color == gCursorTextAnnotationPlacementColor) {
        return gCursorTextAnnotationPlacement;
    }
    HCURSOR cursor = CreateSvgPlacementCursor(gIconAnnotText, dx, dy, color, 0, 0);
    if (!cursor) {
        return gCursorTextAnnotationPlacement;
    }
    if (gCursorTextAnnotationPlacement) {
        DestroyCursor(gCursorTextAnnotationPlacement);
    }
    gCursorTextAnnotationPlacement = cursor;
    gCursorTextAnnotationPlacementDx = dx;
    gCursorTextAnnotationPlacementDy = dy;
    gCursorTextAnnotationPlacementColor = color;
    return gCursorTextAnnotationPlacement;
}

static HCURSOR GetInkAnnotationPlacementCursor() {
    int dx = std::max(ToolbarIconSize(), DpiGetSystemMetrics(SM_CXCURSOR));
    int dy = std::max(ToolbarIconSize(), DpiGetSystemMetrics(SM_CYCURSOR));
    Color color = ThemeWindowTextColor();
    if (gCursorInkAnnotationPlacement && dx == gCursorInkAnnotationPlacementDx &&
        dy == gCursorInkAnnotationPlacementDy && color == gCursorInkAnnotationPlacementColor) {
        return gCursorInkAnnotationPlacement;
    }
    DWORD hotspotX = (DWORD)((4 * dx) / 24);
    DWORD hotspotY = (DWORD)((20 * dy) / 24);
    HCURSOR cursor = CreateSvgPlacementCursor(gIconEditAnnotations, dx, dy, color, hotspotX, hotspotY);
    if (!cursor) {
        return gCursorInkAnnotationPlacement;
    }
    if (gCursorInkAnnotationPlacement) {
        DestroyCursor(gCursorInkAnnotationPlacement);
    }
    gCursorInkAnnotationPlacement = cursor;
    gCursorInkAnnotationPlacementDx = dx;
    gCursorInkAnnotationPlacementDy = dy;
    gCursorInkAnnotationPlacementColor = color;
    return cursor;
}

void DeleteAnnotationPlacementCursors() {
    if (gCursorTextAnnotationPlacement) {
        DestroyCursor(gCursorTextAnnotationPlacement);
    }
    gCursorTextAnnotationPlacement = nullptr;
    gCursorTextAnnotationPlacementDx = 0;
    gCursorTextAnnotationPlacementDy = 0;
    gCursorTextAnnotationPlacementColor = 0;

    if (gCursorInkAnnotationPlacement) {
        DestroyCursor(gCursorInkAnnotationPlacement);
    }
    gCursorInkAnnotationPlacement = nullptr;
    gCursorInkAnnotationPlacementDx = 0;
    gCursorInkAnnotationPlacementDy = 0;
    gCursorInkAnnotationPlacementColor = 0;
}

// orig's gCursorDrag: the hand of dragcursor.cur, a resource of the exe
static HCURSOR GetDragCursor() {
    static HCURSOR cur = LoadCursorW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDC_CURSORDRAG));
    return cur;
}

HCURSOR NativeCursorGet(NativeCursor cursor) {
    switch (cursor) {
        case NativeCursor::Drag:
            return GetDragCursor();
        case NativeCursor::LaserPointer:
            return GetLaserPointerCursor();
        case NativeCursor::TextAnnotationPlacement:
            return GetTextAnnotationPlacementCursor();
        case NativeCursor::InkAnnotationPlacement:
            return GetInkAnnotationPlacementCursor();
        default:
            return nullptr;
    }
}

void NativeCursorsDelete() {
    DeleteLaserPointerCursor();
    DeleteAnnotationPlacementCursors();
}
