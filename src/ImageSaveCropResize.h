/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct RenderedBitmap;

enum class ImageEditMode {
    Save,
    Crop,
    Resize
};

void ShowImageEditWindow(MainWindow* win, ImageEditMode mode, const char* filePath = nullptr,
                         RenderedBitmap* rbmp = nullptr, bool selectPdf = false);

// Headless test for issue #5734: arrow keys must resize even when focus is on the dest path edit.
char* TestImageResizeArrowKeyResult(const char* imagePath, int* exitCodeOut = nullptr);
