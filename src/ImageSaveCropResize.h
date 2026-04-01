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
                         RenderedBitmap* rbmp = nullptr);
