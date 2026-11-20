/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "LaserPointerCursor.h"

// A laser dot: a white-hot center inside a saturated red core, surrounded by a
// glow that fades to transparent so the dot is visible on light and dark pages
// alike. Writes size*size premultiplied 0xAARRGGBB pixels, top row first.
void LaserPointerDotPixels(u32* pixels, int size) {
    float center = (float)size / 2.f;
    float glowR = center;
    float coreR = (float)size * 0.16f;
    float hotR = (float)size * 0.07f;
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
            pixels[(y * size) + x] = ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | b;
        }
    }
}
