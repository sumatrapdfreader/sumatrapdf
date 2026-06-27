/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct RenderedBitmap;

enum class ImageEditMode {
    Save,
    Crop,
    Resize
};

void ShowImageEditWindow(MainWindow* win, ImageEditMode mode, Str filePath = {},
                         RenderedBitmap* rbmp = nullptr, bool selectPdf = false);

// Headless test for issue #5734: arrow keys must resize even when focus is on the dest path edit.
Str TestImageResizeArrowKeyResult(Str imagePath, int* exitCodeOut = nullptr);
