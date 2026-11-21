/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's OLE drag and drop of the canvas (Canvas.cpp): the selected text or an
// image dragged out of the window, and a link (a browser's image) dropped in.
// ng: gpui runs the OS drag (WindowStartDragOut) and reports dropped text and
// links as an ExternalText drag; what to drag and what a drop does stay here.

#include "gui/GpuiBridge.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Selection.h"
#include "PdfTools.h"
#include "PngOptimizer.h"
#include "SearchAndDDECommon.h"
#include "DropUrlCommon.h"
#include "gui/DragDrop.h"

#include "SumatraLog.h"

// Longest edge of the proportional drag-out thumbnail (dips).
constexpr float kDragImageThumbnailSize = 220;

bool CanDragOut(MainWindow* win) {
    return IsMainWindowValid(win) && win->gpuiWin && gp::AppCanDragOut(win->gpuiWin->app);
}

void StartTextDragDrop(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->gpuiWin) {
        return;
    }
    bool isTextOnly = false;
#if OS_WIN
    Str lineSep = StrL("\r\n");
#else
    Str lineSep = StrL("\n");
#endif
    TempStr text = GetSelectedTextTemp(win->CurrentTab(), lineSep, isTextOnly);
    if (len(text) == 0) {
        return;
    }
    gp::DragOutData data;
    data.text = ToGpui(text);
    bool ok = gp::WindowStartDragOut(win->gpuiWin, data);
    logf("StartTextDragDrop: %d bytes, started %d\n", len(text), (int)ok);
}

// the encoded image of win->imageDragElement; empty when it is gone
static Str ImageDragPng(MainWindow* win) {
#if OS_WIN
    DisplayModel* dm = win->AsFixed();
    IPageElement* el = win->imageDragElement;
    // page elements are freed when the page content changes (redaction, image replace)
    if (!dm || !el || win->imageDragElementGen != AtomicIntGet(&dm->GetEngine()->elementsGeneration)) {
        return {};
    }
    RenderedBitmap* rb = dm->GetEngine()->GetImageForPageElement(el);
    if (!rb) {
        return {};
    }
    Pixmap* px = PixmapFromRenderedBitmap(rb); // takes ownership of rb
    Str png = px ? EncodePngFromPixmap(px) : Str();
    FreePixmap(px);
    return png;
#else
    // ng: only the win32 engines make a RenderedBitmap (see CopyRenderedBitmapToClipboard)
    (void)win;
    return {};
#endif
}

// where the press was on the image, as fractions of its size: the drag
// preview stays under the pointer at the same spot
static void ImageDragHotSpot(MainWindow* win, float* hx, float* hy) {
    *hx = 0.5f;
    *hy = 0.5f;
    DisplayModel* dm = win->AsFixed();
    if (!dm || win->imageDragPageNo <= 0) {
        return;
    }
    Rect rc = dm->CvtToScreen(win->imageDragPageNo, win->imageDragElement->GetRect());
    if (rc.dx <= 0 || rc.dy <= 0) {
        return;
    }
    int relX = limitValue(win->dragStart.x - rc.x, 0, rc.dx);
    int relY = limitValue(win->dragStart.y - rc.y, 0, rc.dy);
    *hx = (float)relX / (float)rc.dx;
    *hy = (float)relY / (float)rc.dy;
}

// the element is the window's until the drag starts: gpui copies the image
void StartImageDragDrop(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->gpuiWin) {
        return;
    }
    Str png = ImageDragPng(win);
    if (len(png) > 0) {
        gp::DragOutData data;
        data.imageBytes = (const uint8_t*)png.s;
        data.imageBytesLen = len(png);
        data.previewMaxEdge = kDragImageThumbnailSize;
        ImageDragHotSpot(win, &data.previewHotX, &data.previewHotY);
        bool ok = gp::WindowStartDragOut(win->gpuiWin, data);
        logf("StartImageDragDrop: %d bytes of png, started %d\n", len(png), (int)ok);
        str::Free(png);
    }
    win->imageDragElement = nullptr;
    win->imageDragPageNo = -1;
}

void OpenDownloadedPath(Str* path) {
    MainWindow* win = LastActiveWindow();
    if (!IsMainWindowValidAndNotClosing(win) && len(gWindows) > 0) {
        win = gWindows[0];
    }
    if (IsMainWindowValidAndNotClosing(win)) {
        StartLoadDocument(win, *path);
    }
    str::Free(*path);
    delete path;
}

static bool IsHttpUrl(Str s) {
    return str::StartsWithI(s, StrL("http://")) || str::StartsWithI(s, StrL("https://"));
}

// orig's CanvasDropTarget::Drop for what is not files: the link, or text that
// is one, is downloaded and opened. Merge PDF takes only files.
void CanvasOnTextDrop(MainWindow* win, Str text, Str url) {
    if (!IsMainWindowValidAndNotClosing(win) || !CanAccessDisk() || gPluginMode || PdfToolDialogIsMerge(win)) {
        return;
    }
    if (!IsHttpUrl(url)) {
        url = IsHttpUrl(text) ? text : Str();
    }
    if (len(url) == 0) {
        return;
    }
    logf("CanvasOnTextDrop: '%s'\n", url);
    auto* data = new DownloadAndOpenUrlData();
    data->url = str::Dup(url);
    auto fn = MkFunc0<DownloadAndOpenUrlData>(DownloadAndOpenUrl, data);
    RunAsync(fn, StrL("DownloadAndOpenUrl"));
}

int SplitDroppedPaths(Str all, Str* out, int maxPaths) {
    int n = 0;
    int i = 0;
    while (i < len(all) && n < maxPaths) {
        int start = i;
        while (i < len(all) && all.s[i] != '\n' && all.s[i] != '\r') {
            i++;
        }
        int lineLen = i - start;
        while (lineLen > 0 && (all.s[start] == ' ' || all.s[start] == '\t')) {
            start++;
            lineLen--;
        }
        if (lineLen > 0) {
            out[n++] = Str(all.s + start, lineLen);
        }
        if (i < len(all) && all.s[i] == '\r') {
            i++;
        }
        if (i < len(all) && all.s[i] == '\n') {
            i++;
        }
    }
    return n;
}
