/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by ChangeColorDialogCommon.cpp and each app's ChangeColorDialog.cpp ---

static const int kMaxCustomColors = 13;
void HsvToRgb(float h, float s, float v, u8& r, u8& g, u8& b);
Color WithAlpha(Color c, u8 a);
u8 OpacityOf(Color c);
void SaveCustomColors(const Vec<Color>& colors);
// which tab the color picked in the generic dialog applies to
struct TabColorTarget {
    MainWindow* win = nullptr;
    Str filePath;
};

// implemented by each app
void TabColorPicked(TabColorTarget* target, ChangeColorsArgs* args);
