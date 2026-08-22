/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct RenderedBitmap;
struct PlatformFont;
namespace Gdiplus {
class Bitmap;
}

// Hooks the app provides to the shared image editor. Every member is optional;
// each one describes what is lost by leaving it out.
struct ImageEditHost {
    // Decodes an image file. Without it the editor can only work on a bitmap it
    // was handed (a screenshot), not one loaded from disk.
    Gdiplus::Bitmap* (*LoadImageFile)(Str path) = nullptr;
    // Writes a single-page PDF. Without it PDF isn't offered as a destination
    // format, since nothing could produce one.
    bool (*SaveBitmapAsPdf)(Gdiplus::Bitmap* bmp, Str destPath) = nullptr;
    // Called with what was just written, for a host that can display it.
    void (*OpenSavedFile)(HWND parent, Str path) = nullptr;
    // Translates a UI string; without it the English source string is used.
    Str (*Translate)(Str) = nullptr;
    // Applies the host's dark mode to a window the editor created.
    void (*ApplyDarkMode)(HWND) = nullptr;
    // Font for the editor's controls; without it the default GUI font is used.
    PlatformFont* (*GetFont)() = nullptr;
    // Window the editor falls back to as a parent (used by the headless test).
    HWND (*GetOwnerHwnd)() = nullptr;
    // Icon resource id for the editor window; 0 leaves it without one.
    int appIconId = 0;
    // whether Esc closes the editor from its save mode
    bool escToExit = false;
};

extern ImageEditHost gImageEditHost;

void InitImageEditHost();

enum class ImageEditMode {
    Save,
    Crop,
    Resize
};

// Canonical save extension for encoded image bytes (.jpg/.png/…); empty if unknown.
Str ImageSaveExtFromData(Str data);

// parent is the window the editor is owned by and centred on. Either filePath
// names an image to load or rbmp holds one already rendered; with neither
// there is nothing to edit and the call does nothing.
// originalData, when set, is the encoded source (JPEG stream, CBZ page, …);
// Save writes those bytes if the image is not cropped/resized and the dest
// extension still matches the original format. Caller keeps ownership.
void ShowImageEditWindow(HWND parent, ImageEditMode mode, Str filePath = {}, RenderedBitmap* rbmp = nullptr,
                         bool selectPdf = false, Str originalData = {}, bool closeOnEsc = false);

TempStr ImageResizeArrowKeyResultTemp(Str imagePath, int* exitCodeOut = nullptr);
TempStr ImageResizeEdgesResultTemp(Str imagePath, int newW, int newH, int* exitCodeOut = nullptr);

bool TrySaveOriginalAsCmykTiff(Str originalData, Str destPath);
