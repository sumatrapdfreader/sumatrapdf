/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

inline Color TabTextColorForBackground(Color text, Color tabBg) {
    constexpr int kMinLightnessDifference = 80;
    if (abs((int)GetLightness(text) - (int)GetLightness(tabBg)) >= kMinLightnessDifference) {
        return text;
    }
    return IsLightColor(tabBg) ? kColBlack : kColWhite;
}
