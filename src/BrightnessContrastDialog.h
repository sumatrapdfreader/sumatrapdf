/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Preview only re-renders; Remember also stores the values for the file
enum class PageAdjustSave {
    Preview,
    Remember,
};

void SetTabPageAdjust(WindowTab* tab, int brightness, int contrast, PageAdjustSave save);
void ShowBrightnessContrastDialog(MainWindow* win);
