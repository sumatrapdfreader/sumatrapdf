/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PdfDarkMode.h"

#include <math.h>

#include "base/UtAssert.h"

static float SrgbToLinear(float c) {
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return powf((c + 0.055f) / 1.055f, 2.4f);
}

static float RelLuminance(float r, float g, float b) {
    float lr = SrgbToLinear(r);
    float lg = SrgbToLinear(g);
    float lb = SrgbToLinear(b);
    return 0.2126f * lr + 0.7152f * lg + 0.0722f * lb;
}

static float ContrastRatio(float r1, float g1, float b1, float r2, float g2, float b2) {
    float l1 = RelLuminance(r1, g1, b1);
    float l2 = RelLuminance(r2, g2, b2);
    if (l1 < l2) {
        float t = l1;
        l1 = l2;
        l2 = t;
    }
    return (l1 + 0.05f) / (l2 + 0.05f);
}

static DarkModePalette TestPalette() {
    DarkModePalette p;
    p.textR = 0.90f;
    p.textG = 0.90f;
    p.textB = 0.88f;
    p.bgR = 0.07f;
    p.bgG = 0.09f;
    p.bgB = 0.14f;
    p.diffR = p.bgR - p.textR;
    p.diffG = p.bgG - p.textG;
    p.diffB = p.bgB - p.textB;
    return p;
}

void PdfDarkModeOklab_UnitTests() {
    DarkModePalette palette = TestPalette();
    float out[3] = {};

    MapRgbToDarkThemeOklab(0.f, 0.f, 0.f, palette, out);
    utassert(out[0] > 0.5f && out[1] > 0.5f && out[2] > 0.5f);

    MapRgbToDarkThemeOklab(1.f, 1.f, 1.f, palette, out);
    utassert(out[0] < 0.35f && out[1] < 0.35f && out[2] < 0.40f);

    float midOut[3] = {};
    MapRgbToDarkThemeOklab(0.5f, 0.5f, 0.5f, palette, midOut);
    utassert(midOut[0] > out[0] && midOut[0] < out[0] + 0.6f);

    // Monotone gray ramp: lighter inputs map to darker outputs.
    float prevOutL = 2.f;
    for (int i = 10; i <= 90; i += 20) {
        float g = i / 100.f;
        MapRgbToDarkThemeOklab(g, g, g, palette, out);
        float outL = RelLuminance(out[0], out[1], out[2]);
        utassert(outL <= prevOutL + 0.001f);
        prevOutL = outL;
    }

    MapRgbToDarkThemeOklab(0.85f, 0.15f, 0.12f, palette, out);
    utassert(out[0] > out[1] && out[0] > out[2]);

    MapRgbToDarkThemeOklab(0.12f, 0.55f, 0.48f, palette, out);
    utassert(out[1] > out[0] && out[1] > out[2]);

    MapRgbToDarkThemeOklab(0.15f, 0.20f, 0.85f, palette, out);
    utassert(out[2] > out[0] && out[2] > out[1]);

    MapRgbToDarkThemeOklab(0.72f, 0.74f, 0.78f, palette, out);
    utassert(fabsf(out[0] - out[1]) < 0.15f && fabsf(out[1] - out[2]) < 0.15f);

    MapRgbToDarkThemeOklab(0.f, 0.f, 0.f, palette, out);
    float cr = ContrastRatio(out[0], out[1], out[2], palette.bgR, palette.bgG, palette.bgB);
    utassert(cr >= 4.0f);

    utassert(PdfDarkModeOklabDistance(1.f, 1.f, 1.f, 1.f, 1.f, 1.f) < 0.001f);
    utassert(PdfDarkModeOklabDistance(1.f, 1.f, 1.f, 0.f, 0.f, 0.f) > 0.15f);
    utassert(PdfDarkModeOklabDistance(0.95f, 0.93f, 0.88f, 0.97f, 0.95f, 0.90f) < 0.06f);
}
