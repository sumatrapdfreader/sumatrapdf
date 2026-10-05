/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct RenderedBitmap;
struct Pixmap;
struct MainWindow;

// Hooks the app provides to the shared image editor. Every member is optional;
// each one describes what is lost by leaving it out.
// ng: orig hands the editor Gdiplus::Bitmap and an HWND; here it is a Pixmap
// and the MainWindow the dialog lives in, so the editor is portable.
struct ImageEditHost {
    // Decodes an image file. Without it the editor can only work on a bitmap it
    // was handed (a screenshot), not one loaded from disk.
    Pixmap* (*LoadImageFile)(Str path) = nullptr;
    // Writes a single-page PDF. Without it PDF isn't offered as a destination
    // format, since nothing could produce one.
    bool (*SavePixmapAsPdf)(Pixmap* px, Str destPath) = nullptr;
    // Writes an encoded image file; ext picks the format (".png", ".jpg", ...)
    bool (*SavePixmapAsImage)(Pixmap* px, Str destPath, Str ext) = nullptr;
    // true when this machine has an encoder for ext
    bool (*ImageFormatAvailable)(Str ext) = nullptr;
    // Called with what was just written, for a host that can display it.
    void (*OpenSavedFile)(MainWindow* parent, Str path) = nullptr;
    // Translates a UI string; without it the English source string is used.
    Str (*Translate)(Str) = nullptr;
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

// win is the window the editor is shown in and centred on. Either filePath
// names an image to load or rbmp holds one already rendered; with neither
// there is nothing to edit and the call does nothing.
// originalData, when set, is the encoded source (JPEG stream, CBZ page, …);
// Save writes those bytes if the image is not cropped/resized and the dest
// extension still matches the original format. Caller keeps ownership.
void ShowImageEditWindow(MainWindow* win, ImageEditMode mode, Str filePath = {}, RenderedBitmap* rbmp = nullptr,
                         bool selectPdf = false, Str originalData = {}, bool closeOnEsc = false);
// ng: the editor is a gpui dialog inside the window, not a window of its own
void CloseImageEditWindow();
bool IsImageEditWindowVisible();
gpui::El* ImageEditWindowBuild(MainWindow* win, gpui::Ctx* cx);
bool ImageEditOnArrowKey(MainWindow* win, int vk, bool shift);
bool ImageEditOnKeyDown(MainWindow* win, int vk, bool ctrl);
void ImageEditOnEscape();

// the crop / resize state as one line, for the log and the tests
TempStr ImageEditStateTemp();

// ng: gpui's clipboard is text only (see "gpui gaps"), so an image goes
// through the platform's clipboard; false everywhere but Windows
bool ImageEditCopyToClipboard(Pixmap* px);
Pixmap* ImageEditGetClipboard(MainWindow* win);
bool ImageEditHasClipboard(MainWindow* win);
