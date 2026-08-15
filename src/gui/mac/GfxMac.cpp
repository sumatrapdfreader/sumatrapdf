/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "gui/mac/GuiMacBridge.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"

struct GfxMac : Gfx {
    void* ctx = nullptr;

    explicit GfxMac(void* context) : ctx(context) {}

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

static MacGuiRect MacRect(const Rect& r) {
    return {r.x, r.y, r.dx, r.dy};
}

void GfxMac::FillRect(const Rect& r, Color color) {
    if (!r.IsEmpty() && !ColorSkipsPaint(color)) {
        MacGuiFillRect(ctx, MacRect(r), color, 255);
    }
}

void GfxMac::FillRects(const Rect* rects, int count, Color color, u8 alpha, int outlineWidth) {
    if (!rects || count <= 0 || ColorSkipsPaint(color)) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (!rects[i].IsEmpty()) {
            MacGuiFillRect(ctx, MacRect(rects[i]), color, alpha);
            if (outlineWidth > 0) {
                MacGuiDrawRect(ctx, MacRect(rects[i]), kColBlack, outlineWidth, false);
            }
        }
    }
}

void GfxMac::DrawRect(const Rect& r, Color color, int thickness) {
    if (!r.IsEmpty() && !ColorSkipsPaint(color) && thickness > 0) {
        MacGuiDrawRect(ctx, MacRect(r), color, thickness, false);
    }
}

void GfxMac::DrawDashedRect(const Rect& r, Color color) {
    if (!r.IsEmpty() && !ColorSkipsPaint(color)) {
        MacGuiDrawRect(ctx, MacRect(r), color, 1, true);
    }
}

void GfxMac::FillRoundedRect(const Rect& r, int radius, Color fill, Color border) {
    if (!r.IsEmpty()) {
        MacGuiFillRoundedRect(ctx, MacRect(r), radius, fill, !ColorSkipsPaint(fill), border, !ColorSkipsPaint(border));
    }
}

void GfxMac::FillEllipse(const Rect& r, Color color, u8 alpha) {
    if (!r.IsEmpty() && !ColorSkipsPaint(color)) {
        MacGuiFillEllipse(ctx, MacRect(r), color, alpha);
    }
}

void GfxMac::DrawLine(const Rect& r, Color color, int thickness) {
    if (ColorSkipsPaint(color) || thickness <= 0) {
        return;
    }
    if (r.dy == 0) {
        MacGuiDrawLine(ctx, r.x, r.y, r.Right(), r.y, color, (float)thickness, 255);
    } else if (r.dx == 0) {
        MacGuiDrawLine(ctx, r.x, r.y, r.x, r.Bottom(), color, (float)thickness, 255);
    } else {
        MacGuiFillRect(ctx, MacRect(r), color, 255);
    }
}

void GfxMac::DrawLineAA(Point p1, Point p2, Color color, float thickness, u8 alpha) {
    if (!ColorSkipsPaint(color) && thickness > 0) {
        MacGuiDrawLine(ctx, p1.x, p1.y, p2.x, p2.y, color, thickness, alpha);
    }
}

void GfxMac::DrawFocusRect(const Rect& r) {
    DrawDashedRect(r, kColBlack);
}

static uint32_t MacTextFlags(u32 flags) {
    uint32_t result = 0;
    if (flags & gfxTextCenter) result |= MacGuiTextCenter;
    if (flags & gfxTextRight) result |= MacGuiTextRight;
    if (flags & gfxTextEllipsis) result |= MacGuiTextEllipsis;
    if (flags & gfxTextPathEllipsis) result |= MacGuiTextPathEllipsis;
    if (flags & gfxTextSingleLine) result |= MacGuiTextSingleLine;
    if (flags & gfxTextVCenter) result |= MacGuiTextVCenter;
    if (flags & gfxTextNoClip) result |= MacGuiTextNoClip;
    if (flags & gfxTextWrap) result |= MacGuiTextWrap;
    return result;
}

void GfxMac::DrawText(Str s, const Rect& r, u32 flags, PlatformFont* font, Color color) {
    if (len(s) == 0 || r.IsEmpty() || !font) {
        return;
    }
    MacGuiDrawText(ctx, s.s, len(s), MacRect(r), MacTextFlags(flags), font->nativeFont,
                   color == kColorUnset ? kColBlack : color);
}

void GfxMac::DrawTextAt(Str s, Point pos, u32 flags, PlatformFont* font, Color color) {
    Size size = PlatformFontMeasureText(font, s);
    flags &= ~(gfxTextCenter | gfxTextRight | gfxTextVCenter);
    DrawText(s, {pos.x, pos.y, std::max(size.dx, 1), std::max(size.dy, 1)}, flags | gfxTextNoClip, font, color);
}

Size GfxMac::MeasureText(Str s, PlatformFont* font) {
    return PlatformFontMeasureText(font, s);
}

static u8* PixmapAsPremultipliedBgra(Pixmap* pixmap, int* strideOut) {
    *strideOut = pixmap->width * 4;
    size_t nBytes = (size_t)*strideOut * (size_t)pixmap->height;
    u8* result = AllocArray<u8>(nBytes);
    if (!result) {
        return nullptr;
    }
    for (int y = 0; y < pixmap->height; y++) {
        const u8* src = pixmap->data + y * pixmap->stride;
        u8* dst = result + y * *strideOut;
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
            if (!pixmap->premultiplied) {
                r = (u8)((r * a + 127) / 255);
                g = (u8)((g * a + 127) / 255);
                b = (u8)((b * a + 127) / 255);
            }
            dst[0] = b;
            dst[1] = g;
            dst[2] = r;
            dst[3] = a;
            dst += 4;
        }
    }
    return result;
}

void GfxMac::DrawPixmap(Pixmap* pixmap, const Rect& r) {
    if (!pixmap || !pixmap->data || pixmap->format == PixmapFormat::Native || r.IsEmpty()) {
        return;
    }
    const u8* data = pixmap->data;
    int stride = pixmap->stride;
    u8* ownedData = nullptr;
    if (pixmap->format != PixmapFormat::BGRA8 || !pixmap->premultiplied) {
        ownedData = PixmapAsPremultipliedBgra(pixmap, &stride);
        if (!ownedData) {
            return;
        }
        data = ownedData;
    }
    MacGuiDrawPixmap(ctx, data, pixmap->width, pixmap->height, stride, MacRect(r));
    free(ownedData);
}

void GfxMac::PushClip(const Rect& r) {
    MacGuiPushClip(ctx, MacRect(r));
}

void GfxMac::PopClip() {
    MacGuiPopClip(ctx);
}

bool GfxMac::SetMirrored(bool mirrored) {
    return MacGuiSetMirrored(ctx, mirrored);
}

Gfx* GfxCreate(void* context) {
    return context ? new GfxMac(context) : nullptr;
}
