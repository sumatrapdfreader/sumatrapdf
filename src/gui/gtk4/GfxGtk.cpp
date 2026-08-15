/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "gui/PlatformFont.h"
#include "gui/Gfx.h"

#include <pango/pangocairo.h>

struct GfxGtk : Gfx {
    cairo_t* cr = nullptr;
    bool mirrored = false;
    cairo_matrix_t baseMatrix{};

    explicit GfxGtk(cairo_t* cr) : cr(cr) { cairo_get_matrix(cr, &baseMatrix); }

    void FillRect(const Rect&, Color) override;
    void FillRects(const Rect*, int count, Color, u8 alpha, int outlineWidth) override;
    void DrawRect(const Rect&, Color, int thickness) override;
    void DrawDashedRect(const Rect&, Color) override;
    void FillRoundedRect(const Rect&, int radius, Color fill, Color border) override;
    void FillEllipse(const Rect&, Color, u8 alpha) override;
    void DrawLine(const Rect&, Color, int thickness) override;
    void DrawLineAA(Point, Point, Color, float thickness, u8 alpha) override;
    void DrawFocusRect(const Rect&) override;
    void DrawText(Str, const Rect&, u32 flags, PlatformFont*, Color) override;
    void DrawTextAt(Str, Point, u32 flags, PlatformFont*, Color) override;
    Size MeasureText(Str, PlatformFont*) override;
    void DrawPixmap(Pixmap*, const Rect&) override;
    void PushClip(const Rect&) override;
    void PopClip() override;
    bool SetMirrored(bool) override;
};

static void SetSource(cairo_t* cr, Color color, u8 alpha = 255) {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    UnpackColor(color, r, g, b);
    cairo_set_source_rgba(cr, r / 255.0, g / 255.0, b / 255.0, alpha / 255.0);
}

void GfxGtk::FillRect(const Rect& r, Color color) {
    if (r.IsEmpty() || ColorSkipsPaint(color)) {
        return;
    }
    SetSource(cr, color);
    cairo_rectangle(cr, r.x, r.y, r.dx, r.dy);
    cairo_fill(cr);
}

void GfxGtk::FillRects(const Rect* rects, int count, Color color, u8 alpha, int outlineWidth) {
    if (!rects || count <= 0 || ColorSkipsPaint(color)) {
        return;
    }
    cairo_new_path(cr);
    for (int i = 0; i < count; i++) {
        const Rect& r = rects[i];
        if (!r.IsEmpty()) {
            cairo_rectangle(cr, r.x, r.y, r.dx, r.dy);
        }
    }
    SetSource(cr, color, alpha);
    cairo_fill_preserve(cr);
    if (outlineWidth > 0) {
        SetSource(cr, kColBlack, alpha);
        cairo_set_line_width(cr, outlineWidth);
        cairo_stroke(cr);
    } else {
        cairo_new_path(cr);
    }
}

void GfxGtk::DrawRect(const Rect& r, Color color, int thickness) {
    if (r.IsEmpty() || ColorSkipsPaint(color) || thickness < 1) {
        return;
    }
    double inset = thickness / 2.0;
    SetSource(cr, color);
    cairo_set_line_width(cr, thickness);
    cairo_rectangle(cr, r.x + inset, r.y + inset, std::max(r.dx - thickness, 0), std::max(r.dy - thickness, 0));
    cairo_stroke(cr);
}

void GfxGtk::DrawDashedRect(const Rect& r, Color color) {
    if (r.IsEmpty() || ColorSkipsPaint(color)) {
        return;
    }
    const double dashes[] = {2.0, 2.0};
    cairo_save(cr);
    cairo_set_dash(cr, dashes, dimofi(dashes), 0);
    DrawRect(r, color, 1);
    cairo_restore(cr);
}

static void RoundedRectPath(cairo_t* cr, const Rect& r, int radius) {
    double rr = std::min((double)radius, std::min(r.dx, r.dy) / 2.0);
    constexpr double quarter = M_PI / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.Right() - rr, r.y + rr, rr, -quarter, 0);
    cairo_arc(cr, r.Right() - rr, r.Bottom() - rr, rr, 0, quarter);
    cairo_arc(cr, r.x + rr, r.Bottom() - rr, rr, quarter, M_PI);
    cairo_arc(cr, r.x + rr, r.y + rr, rr, M_PI, M_PI + quarter);
    cairo_close_path(cr);
}

void GfxGtk::FillRoundedRect(const Rect& r, int radius, Color fill, Color border) {
    if (r.IsEmpty()) {
        return;
    }
    if (radius <= 0) {
        FillRect(r, fill);
        DrawRect(r, border, 1);
        return;
    }
    RoundedRectPath(cr, r, radius);
    if (!ColorSkipsPaint(fill)) {
        SetSource(cr, fill);
        cairo_fill_preserve(cr);
    }
    if (!ColorSkipsPaint(border)) {
        SetSource(cr, border);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
    } else {
        cairo_new_path(cr);
    }
}

void GfxGtk::FillEllipse(const Rect& r, Color color, u8 alpha) {
    if (r.IsEmpty() || ColorSkipsPaint(color)) {
        return;
    }
    cairo_save(cr);
    cairo_translate(cr, r.x + r.dx / 2.0, r.y + r.dy / 2.0);
    cairo_scale(cr, r.dx / 2.0, r.dy / 2.0);
    cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
    SetSource(cr, color, alpha);
    cairo_fill(cr);
    cairo_restore(cr);
}

void GfxGtk::DrawLine(const Rect& r, Color color, int thickness) {
    Rect line = r;
    if (line.dy == 0) {
        line.dy = thickness;
    } else if (line.dx == 0) {
        line.dx = thickness;
    }
    FillRect(line, color);
}

void GfxGtk::DrawLineAA(Point p1, Point p2, Color color, float thickness, u8 alpha) {
    if (ColorSkipsPaint(color) || thickness <= 0) {
        return;
    }
    SetSource(cr, color, alpha);
    cairo_set_line_width(cr, thickness);
    cairo_move_to(cr, p1.x, p1.y);
    cairo_line_to(cr, p2.x, p2.y);
    cairo_stroke(cr);
}

void GfxGtk::DrawFocusRect(const Rect& r) {
    DrawDashedRect(r, kColBlack);
}

static PangoLayout* MakeLayout(cairo_t* cr, Str s, PlatformFont* font, const Rect* bounds, u32 flags) {
    PangoLayout* layout = pango_cairo_create_layout(cr);
    pango_layout_set_text(layout, s.s, len(s));
    if (font && font->nativeFont) {
        pango_layout_set_font_description(layout, (PangoFontDescription*)font->nativeFont);
    }
    if (bounds) {
        pango_layout_set_width(layout, std::max(bounds->dx, 0) * PANGO_SCALE);
    }
    if (flags & gfxTextWrap) {
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    } else {
        pango_layout_set_single_paragraph_mode(layout, TRUE);
    }
    if (flags & gfxTextPathEllipsis) {
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_MIDDLE);
    } else if (flags & gfxTextEllipsis) {
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    }
    if (flags & gfxTextCenter) {
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    } else if (flags & gfxTextRight) {
        pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
    }
    PangoDirection direction = flags & gfxTextRtl ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
    pango_context_set_base_dir(pango_layout_get_context(layout), direction);
    pango_layout_set_auto_dir(layout, FALSE);
    return layout;
}

void GfxGtk::DrawText(Str s, const Rect& r, u32 flags, PlatformFont* font, Color color) {
    if (len(s) == 0 || r.IsEmpty()) {
        return;
    }
    PangoLayout* layout = MakeLayout(cr, s, font, &r, flags);
    int textDx = 0;
    int textDy = 0;
    pango_layout_get_pixel_size(layout, &textDx, &textDy);
    double y = r.y;
    if (flags & gfxTextVCenter) {
        y += std::max((r.dy - textDy) / 2, 0);
    }
    cairo_save(cr);
    if (!(flags & gfxTextNoClip)) {
        cairo_rectangle(cr, r.x, r.y, r.dx, r.dy);
        cairo_clip(cr);
    }
    SetSource(cr, color == kColorUnset ? kColBlack : color);
    cairo_move_to(cr, r.x, y);
    pango_cairo_show_layout(cr, layout);
    cairo_restore(cr);
    g_object_unref(layout);
}

void GfxGtk::DrawTextAt(Str s, Point pos, u32 flags, PlatformFont* font, Color color) {
    Size size = PlatformFontMeasureText(font, s);
    flags &= ~(gfxTextCenter | gfxTextRight | gfxTextVCenter);
    DrawText(s, {pos.x, pos.y, std::max(size.dx, 1), std::max(size.dy, 1)}, flags | gfxTextNoClip, font, color);
}

Size GfxGtk::MeasureText(Str s, PlatformFont* font) {
    return PlatformFontMeasureText(font, s);
}

static cairo_surface_t* SurfaceFromPixmap(Pixmap* pixmap, u8** ownedData) {
    *ownedData = nullptr;
    if (!pixmap || !pixmap->data || pixmap->format == PixmapFormat::Native) {
        return nullptr;
    }
    u8* data = pixmap->data;
    int stride = pixmap->stride;
    if (pixmap->format != PixmapFormat::BGRA8 || !pixmap->premultiplied) {
        stride = pixmap->width * 4;
        data = AllocArray<u8>((size_t)stride * pixmap->height);
        *ownedData = data;
        for (int y = 0; y < pixmap->height; y++) {
            const u8* src = pixmap->data + y * pixmap->stride;
            u8* dst = data + y * stride;
            for (int x = 0; x < pixmap->width; x++) {
                u8 r = 0;
                u8 g = 0;
                u8 b = 0;
                u8 a = 255;
                if (pixmap->format == PixmapFormat::RGBA8) {
                    r = src[0];
                    g = src[1];
                    b = src[2];
                    a = src[3];
                    src += 4;
                } else {
                    b = src[0];
                    g = src[1];
                    r = src[2];
                    if (pixmap->format == PixmapFormat::BGRA8) {
                        a = src[3];
                        src += 4;
                    } else {
                        src += 3;
                    }
                }
                dst[0] = (u8)((b * a + 127) / 255);
                dst[1] = (u8)((g * a + 127) / 255);
                dst[2] = (u8)((r * a + 127) / 255);
                dst[3] = a;
                dst += 4;
            }
        }
    }
    return cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, pixmap->width, pixmap->height, stride);
}

void GfxGtk::DrawPixmap(Pixmap* pixmap, const Rect& r) {
    if (!pixmap || r.IsEmpty()) {
        return;
    }
    u8* ownedData = nullptr;
    cairo_surface_t* surface = SurfaceFromPixmap(pixmap, &ownedData);
    if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        if (surface) {
            cairo_surface_destroy(surface);
        }
        free(ownedData);
        return;
    }
    cairo_surface_mark_dirty(surface);
    cairo_save(cr);
    cairo_rectangle(cr, r.x, r.y, r.dx, r.dy);
    cairo_clip(cr);
    cairo_translate(cr, r.x, r.y);
    cairo_scale(cr, (double)r.dx / pixmap->width, (double)r.dy / pixmap->height);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_surface_destroy(surface);
    free(ownedData);
}

void GfxGtk::PushClip(const Rect& r) {
    cairo_save(cr);
    cairo_rectangle(cr, r.x, r.y, r.dx, r.dy);
    cairo_clip(cr);
}

void GfxGtk::PopClip() {
    cairo_restore(cr);
}

bool GfxGtk::SetMirrored(bool mirror) {
    bool previous = mirrored;
    if (mirror == mirrored) {
        return previous;
    }
    mirrored = mirror;
    cairo_set_matrix(cr, &baseMatrix);
    if (mirror) {
        double x1 = 0;
        double y1 = 0;
        double x2 = 0;
        double y2 = 0;
        cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
        cairo_translate(cr, x2, 0);
        cairo_scale(cr, -1, 1);
    }
    return previous;
}

Gfx* GfxCreate(cairo_t* cr) {
    return cr ? new GfxGtk(cr) : nullptr;
}
