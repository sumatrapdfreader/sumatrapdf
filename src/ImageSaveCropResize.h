/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct RenderedBitmap;

enum class ImageEditMode {
    Save,
    Crop,
    Resize
};

// parent is the window the editor is owned by and centred on. Either filePath
// names an image to load or rbmp holds one already rendered; with neither
// there is nothing to edit and the call does nothing.
void ShowImageEditWindow(HWND parent, ImageEditMode mode, Str filePath = {}, RenderedBitmap* rbmp = nullptr,
                         bool selectPdf = false);

// Headless test for issue #5734: arrow keys must resize even when focus is on the dest path edit.
TempStr ImageResizeArrowKeyResultTemp(Str imagePath, int* exitCodeOut = nullptr);
