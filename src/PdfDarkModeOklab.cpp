/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PdfDarkMode.h"

#include <math.h>

struct OklabColor {
    float L = 0.f;
    float a = 0.f;
    float b = 0.f;
};

static float SrgbToLinear(float c) {
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return powf((c + 0.055f) / 1.055f, 2.4f);
}

static float LinearToSrgb(float c) {
    if (c <= 0.0031308f) {
        return 12.92f * c;
    }
    return 1.055f * powf(c, 1.f / 2.4f) - 0.055f;
}

static float Clamp01(float v) {
    if (v < 0.f) {
        return 0.f;
    }
    if (v > 1.f) {
        return 1.f;
    }
    return v;
}

static OklabColor SrgbToOklab(float r, float g, float b) {
    float lr = SrgbToLinear(r);
    float lg = SrgbToLinear(g);
    float lb = SrgbToLinear(b);

    float l = 0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb;
    float m = 0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb;
    float s = 0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb;

    l = cbrtf(l);
    m = cbrtf(m);
    s = cbrtf(s);

    OklabColor lab;
    lab.L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    lab.a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    lab.b = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
    return lab;
}

static void OklabToSrgb(const OklabColor& lab, float* outR, float* outG, float* outB) {
    float l_ = lab.L + 0.3963377774f * lab.a + 0.2158037573f * lab.b;
    float m_ = lab.L - 0.1055613458f * lab.a - 0.0638541728f * lab.b;
    float s_ = lab.L - 0.0894841775f * lab.a - 1.2914855480f * lab.b;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    float lr = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float lg = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float lb = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

    *outR = Clamp01(LinearToSrgb(lr));
    *outG = Clamp01(LinearToSrgb(lg));
    *outB = Clamp01(LinearToSrgb(lb));
}

static float OklabChroma(const OklabColor& lab) {
    return sqrtf(lab.a * lab.a + lab.b * lab.b);
}

void MapRgbToDarkThemeOklab(float r, float g, float b, const DarkModePalette& palette, float* outRgb) {
    OklabColor src = SrgbToOklab(r, g, b);
    OklabColor text = SrgbToOklab(palette.textR, palette.textG, palette.textB);
    OklabColor bg = SrgbToOklab(palette.bgR, palette.bgG, palette.bgB);

    // Monotone lightness remap in OKLab; preserve hue via a/b direction.
    float outL = text.L + src.L * (bg.L - text.L);

    const float minL = 0.08f;
    const float maxL = 0.92f;
    if (outL < minL) {
        outL = minL;
    }
    if (outL > maxL) {
        outL = maxL;
    }

    float chroma = OklabChroma(src);
    const float maxChroma = 0.38f;
    if (chroma > maxChroma) {
        chroma = maxChroma;
    }

    float outA = 0.f;
    float outB = 0.f;
    float srcChroma = OklabChroma(src);
    if (srcChroma > 1e-5f) {
        float scale = chroma / srcChroma;
        outA = src.a * scale;
        outB = src.b * scale;
    }

    // Pull extreme highlights down slightly on dark backgrounds.
    if (src.L > 0.82f && chroma < 0.06f) {
        float paperMix = (src.L - 0.82f) / 0.18f;
        if (paperMix > 1.f) {
            paperMix = 1.f;
        }
        outL = outL * (1.f - 0.35f * paperMix) + bg.L * (0.35f * paperMix);
    }

    OklabColor out{outL, outA, outB};
    OklabToSrgb(out, &outRgb[0], &outRgb[1], &outRgb[2]);
}

float PdfDarkModeOklabDistance(float r1, float g1, float b1, float r2, float g2, float b2) {
    OklabColor a = SrgbToOklab(r1, g1, b1);
    OklabColor c = SrgbToOklab(r2, g2, b2);
    float dL = a.L - c.L;
    float da = a.a - c.a;
    float db = a.b - c.b;
    return sqrtf(dL * dL + da * da + db * db);
}
