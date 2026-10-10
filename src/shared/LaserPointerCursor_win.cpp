/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "Settings.h"
#include "Theme.h"
#include "AppSettings.h"
#include "LaserPointerCursor.h"

#include "SumatraLog.h"

// The laser pointer: a cursor drawn at run time, the same in orig and ng.

// logical size of the cursor bitmap. The dot itself is a small part of it,
// the rest is the glow fading out to fully transparent
constexpr int kLaserPointerCursorSize = 32;

static HCURSOR gCursorLaserPointer = nullptr;

static int gCursorLaserPointerSize = 0;

// A laser dot: a white-hot center inside a saturated red core, surrounded by a
// glow that fades to transparent so the dot is visible on light and dark pages
// alike. The hotspot is the center of the dot, unlike an arrow's tip.
static HCURSOR CreateLaserPointerCursor(int size) {
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmpColor = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmpColor || !bits) {
        DeleteObject(hbmpColor);
        return nullptr;
    }

    float center = (float)size / 2.f;
    float glowR = center;
    float coreR = (float)size * 0.16f;
    float hotR = (float)size * 0.07f;
    DWORD* pixels = (DWORD*)bits;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = ((float)x + 0.5f) - center;
            float dy = ((float)y + 0.5f) - center;
            float d = sqrtf((dx * dx) + (dy * dy));

            // the glow, densest where it meets the core and quadratically
            // fading out; it also fills the core, so that anti-aliasing the
            // core's edge blends it into the glow rather than into a gap
            float t = limitValue((d - coreR) / (glowR - coreR), 0.f, 1.f);
            float glowA = (d < glowR) ? 0.45f * (1.f - t) * (1.f - t) : 0.f;
            // the dot itself, white-hot in the middle, saturated red at the edge
            float coreA = limitValue(coreR + 0.5f - d, 0.f, 1.f);
            float hot = limitValue(d / hotR, 0.f, 1.f);
            float coreG = 255.f - (200.f * hot);

            // core over glow, written out premultiplied (as 32bpp cursors want)
            float glowW = glowA * (1.f - coreA);
            float alpha = coreA + glowW;
            if (alpha <= 0.f) {
                pixels[(y * size) + x] = 0;
                continue;
            }
            u8 a = (u8)(alpha * 255.f);
            u8 r = (u8)((255.f * coreA) + (255.f * glowW));
            u8 g = (u8)((coreG * coreA) + (16.f * glowW));
            u8 b = g;
            pixels[(y * size) + x] = ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | b;
        }
    }

    // 32bpp cursors carry their own alpha, but CreateIconIndirect still wants a
    // mask bitmap; an all-zero AND mask leaves the color bitmap in charge
    int maskBytesPerRow = ((size + 15) / 16) * 2;
    u8* maskBits = AllocArray<u8>(maskBytesPerRow * size);
    HBITMAP hbmpMask = CreateBitmap(size, size, 1, 1, maskBits);
    HCURSOR res = nullptr;
    if (hbmpMask) {
        ICONINFO ii{};
        ii.fIcon = FALSE;
        ii.xHotspot = (DWORD)(size / 2);
        ii.yHotspot = (DWORD)(size / 2);
        ii.hbmMask = hbmpMask;
        ii.hbmColor = hbmpColor;
        res = (HCURSOR)CreateIconIndirect(&ii);
        DeleteObject(hbmpMask);
    }
    DeleteObject(hbmpColor);
    free(maskBits);
    return res;
}

// the cursor is sized for the DPI of the window it's shown in, so it's
// re-created when the canvas moves to a monitor with a different scaling
HCURSOR GetLaserPointerCursor() {
    int size = DpiScale(kLaserPointerCursorSize);
    if (gCursorLaserPointer && (gCursorLaserPointerSize == size)) {
        return gCursorLaserPointer;
    }
    HCURSOR cur = CreateLaserPointerCursor(size);
    if (!cur) {
        // a cursor of the wrong size beats no cursor at all
        return gCursorLaserPointer;
    }
    if (gCursorLaserPointer) {
        DestroyCursor(gCursorLaserPointer);
    }
    gCursorLaserPointer = cur;
    gCursorLaserPointerSize = size;
    return cur;
}

void DeleteLaserPointerCursor() {
    if (gCursorLaserPointer) {
        DestroyCursor(gCursorLaserPointer);
        gCursorLaserPointer = nullptr;
        gCursorLaserPointerSize = 0;
    }
}
