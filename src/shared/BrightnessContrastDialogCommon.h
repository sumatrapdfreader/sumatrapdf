/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by BrightnessContrastDialogCommon.cpp and each app's BrightnessContrastDialog.cpp ---

constexpr int kPageAdjustMin = -100;
constexpr int kPageAdjustMax = 100;

// Preview only re-renders; Remember also stores the values for the file
enum class PageAdjustSave {
    Preview,
    Remember,
};

void SetTabPageAdjust(WindowTab* tab, int brightness, int contrast, PageAdjustSave save);
TempStr PageAdjustTestTemp(WindowTab* tab, bool set, int brightness, int contrast);
