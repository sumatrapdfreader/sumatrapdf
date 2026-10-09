/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's image editor is an overlapped window of its own with virtual
// controls, GDI+ drawing and WIC encoders. Where the platform can have such a
// window (gui/ToolWindow.h) it is that: sized to the image, growing while a
// resize handle is dragged past its edge. Elsewhere it is a gpui dialog inside
// the main window, of one size with the image scaled to fit. Both have the
// same three modes (Save / Crop / Resize), the same destination row with a
// format drop-down, the same crop rectangle with eight edge handles and the
// same resize handles, the same "write the original bytes back when nothing
// changed and the extension still matches" rule, and the same Copy to
// clipboard.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "base/File.h"
#include "base/ByteReaderWriter.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "ImageReader.h"
#include "base/UITask.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Commands.h"
#include "gui/Dpi.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "ImageSaveCropResize.h"

#include "SumatraLog.h"

// The host's translation of s, or s itself when it has none.
static Str HostTr(Str s) {
    if (gImageEditHost.Translate) {
        return gImageEditHost.Translate(s);
    }
    return s;
}

struct ImageFormat {
    Str label;
    Str ext;
    bool needsProbe; // if true, check if an encoder is available before offering
    bool available;  // set after probing
    bool isPdf;
};

// clang-format off
static ImageFormat gImageFormats[] = {
    {StrL("PNG"),  StrL(".png"),  false, true,  false},
    {StrL("JPEG"), StrL(".jpg"),  false, true,  false},
    {StrL("BMP"),  StrL(".bmp"),  false, true,  false},
    {StrL("GIF"),  StrL(".gif"),  true,  false, false},
    {StrL("TIFF"), StrL(".tif"),  true,  false, false},
    {StrL("WebP"), StrL(".webp"), true,  false, false},
    {StrL("PDF"),  StrL(".pdf"),  false, true,  true},
};
// clang-format on

// index of the PDF entry in gImageFormats
constexpr int kPdfFormatIdx = 6;
constexpr int kDefaultFormatIdx = 0; // PNG

Str ImageSaveExtFromData(Str data) {
    if (len(data) == 0) {
        return {};
    }
    switch (GuessFileTypeFromData(data)) {
        case FileType::Png:
            return StrL(".png");
        case FileType::Jpeg:
            return StrL(".jpg");
        case FileType::Gif:
            return StrL(".gif");
        case FileType::Tiff:
            return StrL(".tif");
        case FileType::Bmp:
            return StrL(".bmp");
        case FileType::Ico:
            return StrL(".ico");
        case FileType::Webp:
            return StrL(".webp");
        case FileType::Jxl:
            return StrL(".jxl");
        case FileType::Jp2:
            return StrL(".jp2");
        default:
            return {};
    }
}

static bool ExtMatchesOriginal(Str ext, Str originalExt) {
    if (len(ext) == 0 || len(originalExt) == 0) {
        return false;
    }
    if (str::EqI(ext, originalExt)) {
        return true;
    }
    if (str::EqI(originalExt, StrL(".jpg")) && str::EqI(ext, StrL(".jpeg"))) {
        return true;
    }
    if (str::EqI(originalExt, StrL(".tif")) && str::EqI(ext, StrL(".tiff"))) {
        return true;
    }
    return false;
}

static bool gFormatsProbed = false;

static void ProbeImageFormats() {
    if (gFormatsProbed) {
        return;
    }
    gFormatsProbed = true;
    for (auto& f : gImageFormats) {
        if (!f.needsProbe) {
            continue;
        }
        f.available = gImageEditHost.ImageFormatAvailable && gImageEditHost.ImageFormatAvailable(f.ext);
    }
}

static int FormatIdxFromExt(Str ext) {
    if (str::EqI(ext, StrL(".jpeg"))) {
        ext = StrL(".jpg");
    } else if (str::EqI(ext, StrL(".tiff"))) {
        ext = StrL(".tif");
    }
    for (int i = 0; i < dimofi(gImageFormats); i++) {
        if (gImageFormats[i].available && str::EqI(gImageFormats[i].ext, ext)) {
            return i;
        }
    }
    return kDefaultFormatIdx;
}

static TempStr PathWithExtTemp(Str path, Str ext) {
    TempStr noExt = path::GetPathNoExtTemp(path);
    return fmt("%s%s", noExt, ext);
}

enum class DragEdge {
    None,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Move,   // only used in crop mode
    NewCrop // only used in crop mode
};

struct ImageEditWnd {
    MainWindow* win = nullptr;
    bool visible = false;
    ImageEditMode mode = ImageEditMode::Crop;
    bool fromRenderedBitmap = false;
    bool closeWithEsc = false;

    // source image
    Str filePath;     // owned
    Str originalData; // owned
    Str originalExt;  // owned
    bool srcWasModified = false;
    Pixmap* srcPixmap = nullptr;
    int imgW = 0;
    int imgH = 0;

    // where the image is drawn, in window dips, and the image pixels per dip
    gpui::Bounds imgArea{};
    float scale = 1;

    // crop rectangle in image coordinates (crop mode)
    int cropX = 0;
    int cropY = 0;
    int cropW = 0;
    int cropH = 0;

    // new size in image coordinates (resize mode)
    int newW = 0;
    int newH = 0;

    // drag state
    bool isDragging = false;
    DragEdge dragEdge = DragEdge::None;
    DragEdge hoverEdge = DragEdge::None;
    Point dragStart;
    int dragCropX = 0;
    int dragCropY = 0;
    int dragCropW = 0;
    int dragCropH = 0;
    bool dragMoved = false;
    int dragNewW = 0;
    int dragNewH = 0;

    gpui::InputState* destEdit = nullptr;
    DialogSelect ddFormat;
    Vec<int> formatIndices; // maps drop-down index to gImageFormats index
    // the transient "Copied to clipboard" line, as orig's info label shows it
    Str statusMsg;
    int statusMs = 0;
    gpui::RenderImage* img = nullptr;
    // orig's window, where the platform can have one; null: a dialog in the
    // frame
    ToolWindow* tw = nullptr;
    // the height of the image area of that window (dips)
    float toolImgAreaDy = 0;
#if OS_WIN
    // orig's class name. A gpui tool window stays GpuiSystemMonitor.
    HWND classHwnd = nullptr;
#endif
};

static ImageEditWnd gImgEdit;

// orig's sizes at 96 dpi
constexpr int kImgMinWindowDx = 640;
constexpr int kImgDownsizeMinDx = 320;
constexpr int kImgDownsizeMinDy = 200;
constexpr float kImgPadding = 16;
constexpr float kImgControlAreaDy = 100;
constexpr float kImgRowPadding = 6;
constexpr float kImgButtonPadding = 8;
constexpr float kImgLabelDy = 15;

static void ImageEditOpenToolWindow(MainWindow* win);
static void ImageEditToolSizeToImage(int prevW, int prevH);
static void ImageEditToolGrow(DragEdge edge);

struct ImageEditView {
    static void OnSave(ImageEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(ImageEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCrop(ImageEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnResize(ImageEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCopy(ImageEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnBrowse(ImageEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnDown(ImageEditView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnUp(ImageEditView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev);
    static void OnMove(ImageEditView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
    static void OnDragMove(ImageEditView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
};

static gp::Entity<ImageEditView> gImgEditView;

// --- coordinate conversion --------------------------------------------------

static int DisplayToImageX(float dx) {
    int v = (int)((dx - gImgEdit.imgArea.x) / gImgEdit.scale);
    return limitValue(v, 0, gImgEdit.imgW);
}

static int DisplayToImageY(float dy) {
    int v = (int)((dy - gImgEdit.imgArea.y) / gImgEdit.scale);
    return limitValue(v, 0, gImgEdit.imgH);
}

static float ImageToDisplayX(int ix) {
    return gImgEdit.imgArea.x + ((float)ix * gImgEdit.scale);
}

static float ImageToDisplayY(int iy) {
    return gImgEdit.imgArea.y + ((float)iy * gImgEdit.scale);
}

static bool IsCropChanged() {
    return gImgEdit.cropX != 0 || gImgEdit.cropY != 0 || gImgEdit.cropW != gImgEdit.imgW ||
           gImgEdit.cropH != gImgEdit.imgH;
}

static bool IsResizeChanged() {
    return gImgEdit.newW != gImgEdit.imgW || gImgEdit.newH != gImgEdit.imgH;
}

// --- the destination path and the format drop-down --------------------------

static int SelectedFormatIdx() {
    int i = gImgEdit.ddFormat.sel;
    if (i < 0 || i >= len(gImgEdit.formatIndices)) {
        return kDefaultFormatIdx;
    }
    return gImgEdit.formatIndices[i];
}

static TempStr DestPathTemp() {
    if (!gImgEdit.destEdit) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(gImgEdit.destEdit)));
}

static void SetDestPath(Str path) {
    if (gImgEdit.destEdit) {
        gp::InputSetValue(gImgEdit.destEdit, ToGpui(path));
    }
}

// the drop-down picked another format: swap the destination's extension
static void OnFormatChanged() {
    int idx = SelectedFormatIdx();
    TempStr dest = DestPathTemp();
    if (len(dest) == 0) {
        return;
    }
    SetDestPath(PathWithExtTemp(dest, gImageFormats[idx].ext));
}

static void FillFormats(int selectIdx) {
    ProbeImageFormats();
    VecReset(gImgEdit.formatIndices);
    StrVec items;
    int sel = 0;
    for (int i = 0; i < dimofi(gImageFormats); i++) {
        if (!gImageFormats[i].available) {
            continue;
        }
        if (gImageFormats[i].isPdf && !gImageEditHost.SavePixmapAsPdf) {
            continue;
        }
        if (i == selectIdx) {
            sel = len(gImgEdit.formatIndices);
        }
        VecAppend(gImgEdit.formatIndices, i);
        items.Append(gImageFormats[i].label);
    }
    gImgEdit.ddFormat.SetItems(items, sel);
}

// --- painting ---------------------------------------------------------------

constexpr int kCheckerSize = 8;
constexpr float kHandleSize = 7;

static void PaintCheckerboard(gp::PaintCtx* ctx, gp::Bounds b) {
    gp::Rgba light = ToGpui(kColWhite);
    gp::Rgba dark = ToGpui(MkRgb(0xcc, 0xcc, 0xcc));
    gp::CanvasFillRect(ctx, b.x, b.y, b.w, b.h, light);
    int nx = (int)(b.w / kCheckerSize) + 1;
    int ny = (int)(b.h / kCheckerSize) + 1;
    for (int y = 0; y < ny; y++) {
        for (int x = 0; x < nx; x++) {
            if (((x + y) & 1) == 0) {
                continue;
            }
            float px = b.x + (float)(x * kCheckerSize);
            float py = b.y + (float)(y * kCheckerSize);
            float pw = std::min((float)kCheckerSize, b.x + b.w - px);
            float ph = std::min((float)kCheckerSize, b.y + b.h - py);
            gp::CanvasFillRect(ctx, px, py, pw, ph, dark);
        }
    }
}

static void PaintDragHandle(gp::PaintCtx* ctx, float x, float y) {
    float h = kHandleSize;
    gp::CanvasFillRect(ctx, x - (h / 2), y - (h / 2), h, h, ToGpui(MkRgb(0x33, 0x99, 0xff)));
    gp::CanvasStrokeRound(ctx, x - (h / 2), y - (h / 2), h, h, 0, 1, ToGpui(kColWhite));
}

static void PaintDimmed(gp::PaintCtx* ctx, gp::Bounds b) {
    gp::Rgba dim = ToGpui(kColBlack);
    dim.a = 120;
    gp::CanvasFillRect(ctx, b.x, b.y, b.w, b.h, dim);
}

// orig's "new size" rectangle in its own window: centered on the image
static gp::Bounds ResizeRectDisplay() {
    ImageEditWnd& w = gImgEdit;
    float rw = (float)w.newW * w.scale;
    float rh = (float)w.newH * w.scale;
    float cx = w.imgArea.x + (w.imgArea.w / 2);
    float cy = w.imgArea.y + (w.imgArea.h / 2);
    return {cx - (rw / 2), cy - (rh / 2), rw, rh};
}

static void PaintImageArea(gp::PaintCtx* ctx, gp::El* e, void*) {
    gp::Bounds b = e->Bounds();
    ImageEditWnd& w = gImgEdit;
    if (!w.srcPixmap || w.imgW <= 0 || w.imgH <= 0) {
        return;
    }
    // fit the image into the area, never magnifying past 1:1; orig's
    // CalcImageLayout leaves kImagePadding around it
    float pad = w.tw ? kImgPadding : 0;
    float availW = std::max(b.w - 2 * pad, 1.f);
    float availH = std::max(b.h - 2 * pad, 1.f);
    float sx = availW / (float)w.imgW;
    float sy = availH / (float)w.imgH;
    float k = std::min(std::min(sx, sy), 1.0f);
    float dw = (float)(int)((float)w.imgW * k);
    float dh = (float)(int)((float)w.imgH * k);
    gp::Bounds area;
    area.x = b.x + pad + (float)(int)((availW - dw) / 2);
    area.y = b.y + pad + (float)(int)((availH - dh) / 2);
    area.w = dw;
    area.h = dh;
    w.imgArea = area;
    w.scale = k;

    // orig's window is a checkerboard behind and around the image
    PaintCheckerboard(ctx, w.tw ? b : area);
    if (!w.img) {
        w.img = RenderImageFromPixmap(ctx->pa, w.srcPixmap);
    }
    if (w.img) {
        gp::RenderImageDraw(ctx, w.img, area, area, 0, 0, false);
    }
    gp::CanvasStrokeRound(ctx, area.x, area.y, area.w, area.h, 0, 1, ToGpui(MkRgb(0x80, 0x80, 0x80)));

    if (w.mode == ImageEditMode::Crop) {
        float cx = ImageToDisplayX(w.cropX);
        float cy = ImageToDisplayY(w.cropY);
        float cw = (float)w.cropW * k;
        float ch = (float)w.cropH * k;
        // everything outside the crop rectangle is dimmed
        PaintDimmed(ctx, {area.x, area.y, area.w, cy - area.y});
        PaintDimmed(ctx, {area.x, cy + ch, area.w, area.y + area.h - (cy + ch)});
        PaintDimmed(ctx, {area.x, cy, cx - area.x, ch});
        PaintDimmed(ctx, {cx + cw, cy, area.x + area.w - (cx + cw), ch});
        gp::CanvasStrokeRound(ctx, cx, cy, cw, ch, 0, 1, ToGpui(MkRgb(0x33, 0x99, 0xff)));
        PaintDragHandle(ctx, cx, cy);
        PaintDragHandle(ctx, cx + (cw / 2), cy);
        PaintDragHandle(ctx, cx + cw, cy);
        PaintDragHandle(ctx, cx, cy + (ch / 2));
        PaintDragHandle(ctx, cx + cw, cy + (ch / 2));
        PaintDragHandle(ctx, cx, cy + ch);
        PaintDragHandle(ctx, cx + (cw / 2), cy + ch);
        PaintDragHandle(ctx, cx + cw, cy + ch);
        return;
    }
    if (w.mode == ImageEditMode::Resize && w.tw) {
        // orig: the new size is a rectangle centered on the image, with
        // eight handles
        gp::Bounds r = ResizeRectDisplay();
        gp::CanvasStrokeRound(ctx, r.x, r.y, r.w, r.h, 0, 1, ToGpui(MkRgb(0x33, 0x99, 0xff)));
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (i != 1 || j != 1) {
                    PaintDragHandle(ctx, r.x + (r.w * (float)i / 2), r.y + (r.h * (float)j / 2));
                }
            }
        }
        return;
    }
    if (w.mode == ImageEditMode::Resize) {
        float rw = (float)w.newW * k;
        float rh = (float)w.newH * k;
        gp::CanvasStrokeRound(ctx, area.x, area.y, rw, rh, 0, 1, ToGpui(MkRgb(0x33, 0x99, 0xff)));
        PaintDragHandle(ctx, area.x + rw, area.y);
        PaintDragHandle(ctx, area.x + rw, area.y + (rh / 2));
        PaintDragHandle(ctx, area.x + rw, area.y + rh);
        PaintDragHandle(ctx, area.x + (rw / 2), area.y + rh);
        PaintDragHandle(ctx, area.x, area.y + rh);
    }
}

// --- hit testing ------------------------------------------------------------

static bool NearF(float a, float b) {
    return a - b < kHandleSize && b - a < kHandleSize;
}

static DragEdge HitTestCropEdge(float mx, float my) {
    ImageEditWnd& w = gImgEdit;
    float x0 = ImageToDisplayX(w.cropX);
    float y0 = ImageToDisplayY(w.cropY);
    float x1 = x0 + ((float)w.cropW * w.scale);
    float y1 = y0 + ((float)w.cropH * w.scale);
    bool nearL = NearF(mx, x0);
    bool nearR = NearF(mx, x1);
    bool nearT = NearF(my, y0);
    bool nearB = NearF(my, y1);
    bool inX = mx >= x0 - kHandleSize && mx <= x1 + kHandleSize;
    bool inY = my >= y0 - kHandleSize && my <= y1 + kHandleSize;
    if (nearL && nearT) {
        return DragEdge::TopLeft;
    }
    if (nearR && nearT) {
        return DragEdge::TopRight;
    }
    if (nearL && nearB) {
        return DragEdge::BottomLeft;
    }
    if (nearR && nearB) {
        return DragEdge::BottomRight;
    }
    if (nearL && inY) {
        return DragEdge::Left;
    }
    if (nearR && inY) {
        return DragEdge::Right;
    }
    if (nearT && inX) {
        return DragEdge::Top;
    }
    if (nearB && inX) {
        return DragEdge::Bottom;
    }
    if (mx > x0 && mx < x1 && my > y0 && my < y1) {
        return DragEdge::Move;
    }
    return DragEdge::None;
}

// like HitTestCropEdge(), but a hit on a not-yet-changed crop rect, or anywhere
// in the image outside it, starts a new crop instead
static DragEdge HitTestCropEdgeOrNewCrop(float mx, float my) {
    DragEdge e = HitTestCropEdge(mx, my);
    if (e != DragEdge::None && IsCropChanged()) {
        return e;
    }
    return DragEdge::NewCrop;
}

// orig's HitTestResizeEdge, for the centered rectangle
static DragEdge HitTestResizeRect(float mx, float my) {
    gp::Bounds r = ResizeRectDisplay();
    float right = r.x + r.w;
    float bottom = r.y + r.h;
    bool onLeft = NearF(mx, r.x);
    bool onRight = NearF(mx, right);
    bool onTop = NearF(my, r.y);
    bool onBottom = NearF(my, bottom);
    bool inVert = my > r.y - kHandleSize && my < bottom + kHandleSize;
    bool inHorz = mx > r.x - kHandleSize && mx < right + kHandleSize;
    if (onLeft && onTop) {
        return DragEdge::TopLeft;
    }
    if (onRight && onTop) {
        return DragEdge::TopRight;
    }
    if (onLeft && onBottom) {
        return DragEdge::BottomLeft;
    }
    if (onRight && onBottom) {
        return DragEdge::BottomRight;
    }
    if (onLeft && inVert) {
        return DragEdge::Left;
    }
    if (onRight && inVert) {
        return DragEdge::Right;
    }
    if (onTop && inHorz) {
        return DragEdge::Top;
    }
    if (onBottom && inHorz) {
        return DragEdge::Bottom;
    }
    return DragEdge::None;
}

static DragEdge HitTestResizeEdge(float mx, float my) {
    ImageEditWnd& w = gImgEdit;
    if (w.tw) {
        return HitTestResizeRect(mx, my);
    }
    float x1 = w.imgArea.x + ((float)w.newW * w.scale);
    float y1 = w.imgArea.y + ((float)w.newH * w.scale);
    bool nearR = NearF(mx, x1);
    bool nearB = NearF(my, y1);
    bool inX = mx >= w.imgArea.x && mx <= x1 + kHandleSize;
    bool inY = my >= w.imgArea.y && my <= y1 + kHandleSize;
    if (nearR && nearB) {
        return DragEdge::BottomRight;
    }
    if (nearR && inY) {
        return DragEdge::Right;
    }
    if (nearB && inX) {
        return DragEdge::Bottom;
    }
    return DragEdge::None;
}

static void SetCropFromDisplaySelection(Point start, float mx, float my) {
    int x0 = DisplayToImageX((float)start.x);
    int y0 = DisplayToImageY((float)start.y);
    int x1 = DisplayToImageX(mx);
    int y1 = DisplayToImageY(my);
    ImageEditWnd& w = gImgEdit;
    w.cropX = std::min(x0, x1);
    w.cropY = std::min(y0, y1);
    w.cropW = std::max(1, std::max(x0, x1) - w.cropX);
    w.cropH = std::max(1, std::max(y0, y1) - w.cropY);
}

// --- mode switching ---------------------------------------------------------

static void ResetToImageSize() {
    ImageEditWnd& w = gImgEdit;
    w.cropX = 0;
    w.cropY = 0;
    w.cropW = w.imgW;
    w.cropH = w.imgH;
    w.newW = w.imgW;
    w.newH = w.imgH;
    w.isDragging = false;
    w.dragEdge = DragEdge::None;
    w.hoverEdge = DragEdge::None;
}

static bool ReplaceSrcPixmap(Pixmap* px) {
    if (!px) {
        return false;
    }
    FreePixmap(gImgEdit.srcPixmap);
    if (gImgEdit.img) {
        gp::RenderImageRelease(gImgEdit.img);
        gImgEdit.img = nullptr;
    }
    gImgEdit.srcPixmap = px;
    gImgEdit.imgW = px->width;
    gImgEdit.imgH = px->height;
    gImgEdit.srcWasModified = true;
    ResetToImageSize();
    return true;
}

// a rectangular copy of the source
static Pixmap* CropPixmap(Pixmap* src, int x, int y, int w, int h) {
    if (!src || w <= 0 || h <= 0) {
        return nullptr;
    }
    Pixmap* dst = AllocPixmap(w, h, src->format, src->premultiplied);
    if (!dst) {
        return nullptr;
    }
    dst->hasAlpha = src->hasAlpha;
    int bpp = PixmapBytesPerPixel(src->format);
    for (int row = 0; row < h; row++) {
        const u8* s = src->data + ((size_t)(y + row) * src->stride) + ((size_t)x * bpp);
        u8* d = dst->data + ((size_t)row * dst->stride);
        memcpy(d, s, (size_t)w * bpp);
    }
    return dst;
}

// ng: orig resizes with GDI+ (InterpolationModeHighQualityBicubic and no pixel
// offset). There is no portable resampler here, so this is a box filter when
// shrinking and bilinear when growing - good enough for what the editor does.
static Pixmap* ResizePixmap(Pixmap* src, int w, int h) {
    if (!src || w <= 0 || h <= 0) {
        return nullptr;
    }
    Pixmap* dst = AllocPixmap(w, h, src->format, src->premultiplied);
    if (!dst) {
        return nullptr;
    }
    dst->hasAlpha = src->hasAlpha;
    int bpp = PixmapBytesPerPixel(src->format);
    float fx = (float)src->width / (float)w;
    float fy = (float)src->height / (float)h;
    for (int y = 0; y < h; y++) {
        u8* d = dst->data + ((size_t)y * dst->stride);
        int sy0 = (int)(y * fy);
        int sy1 = std::max(sy0 + 1, (int)((y + 1) * fy));
        sy1 = std::min(sy1, src->height);
        for (int x = 0; x < w; x++) {
            int sx0 = (int)(x * fx);
            int sx1 = std::max(sx0 + 1, (int)((x + 1) * fx));
            sx1 = std::min(sx1, src->width);
            int acc[4]{};
            int n = 0;
            for (int sy = sy0; sy < sy1; sy++) {
                const u8* s = src->data + ((size_t)sy * src->stride) + ((size_t)sx0 * bpp);
                for (int sx = sx0; sx < sx1; sx++) {
                    for (int c = 0; c < bpp; c++) {
                        acc[c] += s[c];
                    }
                    s += bpp;
                    n++;
                }
            }
            if (n == 0) {
                n = 1;
            }
            for (int c = 0; c < bpp; c++) {
                d[c] = (u8)(acc[c] / n);
            }
            d += bpp;
        }
    }
    return dst;
}

static void ApplyCrop() {
    ImageEditWnd& w = gImgEdit;
    if (!IsCropChanged() || w.cropW <= 0 || w.cropH <= 0) {
        return;
    }
    Pixmap* px = CropPixmap(w.srcPixmap, w.cropX, w.cropY, w.cropW, w.cropH);
    if (!px) {
        return;
    }
    int prevW = w.imgW;
    int prevH = w.imgH;
    ReplaceSrcPixmap(px);
    ImageEditToolSizeToImage(prevW, prevH);
    logf("ImageEdit: cropped to %d x %d\n", w.imgW, w.imgH);
}

static void ApplyResize() {
    ImageEditWnd& w = gImgEdit;
    if (!IsResizeChanged() || w.newW <= 0 || w.newH <= 0) {
        return;
    }
    Pixmap* px = ResizePixmap(w.srcPixmap, w.newW, w.newH);
    if (!px) {
        return;
    }
    int prevW = w.imgW;
    int prevH = w.imgH;
    ReplaceSrcPixmap(px);
    ImageEditToolSizeToImage(prevW, prevH);
    logf("ImageEdit: resized to %d x %d\n", w.imgW, w.imgH);
}

// Same ResizePixmap path as Apply Resize. Reports dest size and the RGB of
// the left and right edge so a test can catch a shifted sample (issue #3434).
TempStr ImageResizeEdgesResultTemp(Str imagePath, int newW, int newH, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (len(imagePath) == 0 || !file::Exists(imagePath) || newW < 2 || newH < 1) {
        return fail(StrL("ERROR bad-args"));
    }
    if (!gImageEditHost.LoadImageFile) {
        InitImageEditHost();
    }
    if (!gImageEditHost.LoadImageFile) {
        return fail(StrL("ERROR no-loader"));
    }
    Pixmap* src = gImageEditHost.LoadImageFile(imagePath);
    if (!src) {
        return fail(StrL("ERROR load-failed"));
    }
    Pixmap* dst = ResizePixmap(src, newW, newH);
    FreePixmap(src);
    if (!dst || !dst->data || dst->width < 2 || dst->height < 1) {
        FreePixmap(dst);
        return fail(StrL("ERROR resize-failed"));
    }
    int bpp = PixmapBytesPerPixel(dst->format);
    if (bpp < 3) {
        FreePixmap(dst);
        return fail(fmt("ERROR pixmap-fmt=%d", (int)dst->format));
    }

    auto pixel = [&](int x, int y, int* r, int* g, int* b) {
        const u8* px = dst->data + ((size_t)y * (size_t)dst->stride) + ((size_t)x * bpp);
        if (dst->format == PixmapFormat::RGBA8) {
            *r = px[0];
            *g = px[1];
            *b = px[2];
        } else {
            *b = px[0];
            *g = px[1];
            *r = px[2];
        }
    };
    int lr, lg, lb, rr, rg, rb;
    pixel(0, dst->height / 2, &lr, &lg, &lb);
    pixel(dst->width - 1, dst->height / 2, &rr, &rg, &rb);
    out.Append(fmt("size=%dx%d left=%d,%d,%d right=%d,%d,%d\n", dst->width, dst->height, lr, lg, lb, rr, rg, rb));
    FreePixmap(dst);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

static void SwitchToMode(ImageEditMode mode) {
    gImgEdit.mode = mode;
    ResetToImageSize();
}

// --- saving -----------------------------------------------------------------

// Uncompressed chunky CMYK TIFF. samples are C,M,Y,K, PDF polarity (0 = no ink).
static bool WriteCmykTiff(Str destPath, int w, int h, int srcStride, const u8* samples) {
    if (len(destPath) == 0 || w <= 0 || h <= 0 || srcStride < w * 4 || !samples) {
        return false;
    }
    const int rowBytes = w * 4;
    const u32 dataLen = (u32)rowBytes * (u32)h;
    const int nTags = 11;
    const u32 ifdOff = 8;
    const u32 ifdSize = 2 + ((u32)nTags * 12) + 4;
    const u32 bitsOff = ifdOff + ifdSize;
    const u32 dataOff = bitsOff + 8;

    ByteWriterLE wr(8 + (int)ifdSize + 8 + (int)dataLen);
    wr.Write8x2('I', 'I');
    wr.Write16(42);
    wr.Write32(ifdOff);
    wr.Write16((u16)nTags);

    auto tagLong = [&](u16 tag, u32 val) {
        wr.Write16(tag);
        wr.Write16(4); // LONG
        wr.Write32(1);
        wr.Write32(val);
    };
    auto tagShort = [&](u16 tag, u16 val) {
        wr.Write16(tag);
        wr.Write16(3); // SHORT
        wr.Write32(1);
        wr.Write16(val);
        wr.Write16(0);
    };
    auto tagShortArray = [&](u16 tag, u32 count, u32 offset) {
        wr.Write16(tag);
        wr.Write16(3);
        wr.Write32(count);
        wr.Write32(offset);
    };

    tagLong(256, (u32)w);           // ImageWidth
    tagLong(257, (u32)h);           // ImageLength
    tagShortArray(258, 4, bitsOff); // BitsPerSample
    tagShort(259, 1);               // Compression = none
    tagShort(262, 5);               // PhotometricInterpretation = Separated
    tagLong(273, dataOff);          // StripOffsets
    tagShort(277, 4);               // SamplesPerPixel
    tagLong(278, (u32)h);           // RowsPerStrip
    tagLong(279, dataLen);          // StripByteCounts
    tagShort(284, 1);               // PlanarConfiguration = chunky
    tagShort(332, 1);               // InkSet = CMYK
    wr.Write32(0);                  // next IFD

    wr.Write16(8);
    wr.Write16(8);
    wr.Write16(8);
    wr.Write16(8);

    for (int y = 0; y < h; y++) {
        const u8* row = samples + ((size_t)y * (size_t)srcStride);
        for (int x = 0; x < rowBytes; x++) {
            wr.Write8(row[x]);
        }
    }
    return file::WriteFile(destPath, wr.AsByteSlice());
}

bool TrySaveOriginalAsCmykTiff(Str originalData, Str destPath) {
    int w = 0, h = 0, stride = 0;
    Vec<u8> samples;
    if (!DecodeJpegToCmyk(originalData, w, h, stride, samples)) {
        return false;
    }
    return WriteCmykTiff(destPath, w, h, stride, samples.els);
}

static void FinishSave(Str dest) {
    MainWindow* win = gImgEdit.win;
    TempStr saved = str::DupTemp(dest);
    logf("ImageEdit: saved '%s'\n", saved);
    CloseImageEditWindow();
    if (gImageEditHost.OpenSavedFile) {
        gImageEditHost.OpenSavedFile(win, saved);
    }
}

static void DoSave() {
    ImageEditWnd& w = gImgEdit;
    if (!w.srcPixmap) {
        return;
    }
    if (w.mode == ImageEditMode::Crop && (w.cropW <= 0 || w.cropH <= 0)) {
        return;
    }
    if (w.mode == ImageEditMode::Resize && (w.newW <= 0 || w.newH <= 0)) {
        return;
    }
    TempStr rawDest = DestPathTemp();
    if (len(rawDest) == 0) {
        return;
    }

    bool unmodified = !w.srcWasModified;
    if (w.mode == ImageEditMode::Crop && IsCropChanged()) {
        unmodified = false;
    }
    if (w.mode == ImageEditMode::Resize && IsResizeChanged()) {
        unmodified = false;
    }

    int fmtIdx = SelectedFormatIdx();
    Str fmtExt = gImageFormats[fmtIdx].ext;
    TempStr destExt = path::GetExtTemp(rawDest);
    bool writeOriginal = unmodified && len(w.originalData) > 0 && ExtMatchesOriginal(destExt, w.originalExt) &&
                         !gImageFormats[fmtIdx].isPdf;

    TempStr dest;
    if (writeOriginal || str::EqI(destExt, fmtExt)) {
        dest = str::DupTemp(rawDest);
    } else {
        dest = PathWithExtTemp(rawDest, fmtExt);
    }

    if (writeOriginal) {
        if (!file::WriteFile(dest, w.originalData)) {
            MessageBoxWarning(w.win, HostTr(StrL("Failed to save image")), StrL("Save Image"));
            return;
        }
        FinishSave(dest);
        return;
    }

    // Unmodified CMYK JPEG saved as TIFF stays CMYK. The pixmap path is RGB.
    if (unmodified && len(w.originalData) > 0 && str::EqI(fmtExt, StrL(".tif"))) {
        if (TrySaveOriginalAsCmykTiff(w.originalData, dest)) {
            FinishSave(dest);
            return;
        }
    }

    Pixmap* result = w.srcPixmap;
    Pixmap* owned = nullptr;
    if (w.mode == ImageEditMode::Crop && IsCropChanged()) {
        owned = CropPixmap(w.srcPixmap, w.cropX, w.cropY, w.cropW, w.cropH);
        result = owned;
    } else if (w.mode == ImageEditMode::Resize && IsResizeChanged()) {
        owned = ResizePixmap(w.srcPixmap, w.newW, w.newH);
        result = owned;
    }
    if (!result) {
        MessageBoxWarning(w.win, HostTr(StrL("Failed to save image")), StrL("Save Image"));
        return;
    }

    bool saved;
    if (gImageFormats[fmtIdx].isPdf) {
        saved = gImageEditHost.SavePixmapAsPdf && gImageEditHost.SavePixmapAsPdf(result, dest);
    } else {
        saved = gImageEditHost.SavePixmapAsImage && gImageEditHost.SavePixmapAsImage(result, dest, fmtExt);
    }
    FreePixmap(owned);
    if (!saved) {
        MessageBoxWarning(w.win, HostTr(StrL("Failed to save image")), StrL("Save Image"));
        return;
    }
    FinishSave(dest);
}

// --- the dialog -------------------------------------------------------------

#if OS_WIN
// Tests look the editor up as this class and post Esc to it.
constexpr const WCHAR* kImageEditWinClass = L"SUMATRA_PDF_IMAGE_EDIT";

static LRESULT CALLBACK ImageEditClassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        ImageEditOnEscape();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void EnsureImageEditClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ImageEditClassProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kImageEditWinClass;
    registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static void ShowImageEditClassWindow() {
    EnsureImageEditClass();
    if (gImgEdit.classHwnd && IsWindow(gImgEdit.classHwnd)) {
        return;
    }
    gImgEdit.classHwnd = CreateWindowExW(WS_EX_NOACTIVATE, kImageEditWinClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr,
                                         nullptr, GetModuleHandleW(nullptr), nullptr);
}

static void DestroyImageEditClassWindow() {
    HWND hwnd = gImgEdit.classHwnd;
    gImgEdit.classHwnd = nullptr;
    if (hwnd && IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }
}
#endif

// the dialog in the frame; a window of its own is not the frame's business
bool IsImageEditWindowVisible() {
    return gImgEdit.visible && !gImgEdit.tw;
}

void CloseImageEditWindow() {
    if (!gImgEdit.visible) {
        return;
    }
    gImgEdit.visible = false;
#if OS_WIN
    DestroyImageEditClassWindow();
#endif
    MainWindow* win = gImgEdit.win;
    if (gImgEdit.tw) {
        ToolWindowClose(gImgEdit.tw);
        gImgEdit.tw = nullptr;
    } else if (win && win->gpuiWin && gImgEdit.destEdit) {
        gp::InputBlur(gImgEdit.destEdit, win->gpuiWin->app, win->gpuiWin);
    }
    delete gImgEdit.destEdit;
    gImgEdit.destEdit = nullptr;
    gImgEdit.ddFormat.Free();
    VecReset(gImgEdit.formatIndices);
    if (gImgEdit.img) {
        gp::RenderImageRelease(gImgEdit.img);
        gImgEdit.img = nullptr;
    }
    FreePixmap(gImgEdit.srcPixmap);
    gImgEdit.srcPixmap = nullptr;
    str::ReplaceWithCopy(&gImgEdit.filePath, {});
    str::ReplaceWithCopy(&gImgEdit.originalData, {});
    str::ReplaceWithCopy(&gImgEdit.originalExt, {});
    str::ReplaceWithCopy(&gImgEdit.statusMsg, {});
    AppShellInvalidate(win);
}

TempStr ImageEditStateTemp() {
    if (!gImgEdit.visible) {
        return StrL("no image editor");
    }
    const char* modeName = "save";
    if (gImgEdit.mode == ImageEditMode::Crop) {
        modeName = "crop";
    } else if (gImgEdit.mode == ImageEditMode::Resize) {
        modeName = "resize";
    }
    return fmt("ImageEdit: %s, image %d x %d at %d,%d k=%d%%, crop %d %d %d %d, new %d x %d, dest '%s'", Str(modeName),
               gImgEdit.imgW, gImgEdit.imgH, (int)gImgEdit.imgArea.x, (int)gImgEdit.imgArea.y,
               (int)(gImgEdit.scale * 100), gImgEdit.cropX, gImgEdit.cropY, gImgEdit.cropW, gImgEdit.cropH,
               gImgEdit.newW, gImgEdit.newH, DestPathTemp());
}

#if OS_WIN
// The picker keeps its bitmap and deletes it. The editor takes ownership of
// the one it is given, so hand it a copy.
void ShowImageEditWindow(HWND parent, ImageEditMode mode, Str filePath, RenderedBitmap* rbmp, bool selectPdf,
                         Str originalData, bool closeOnEsc) {
    MainWindow* win = nullptr;
    for (MainWindow* w : gWindows) {
        if (parent && AppShellNativeHwnd(w) == parent) {
            win = w;
            break;
        }
    }
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    RenderedBitmap* owned = rbmp ? rbmp->Clone() : nullptr;
    ShowImageEditWindow(win, mode, filePath, owned, selectPdf, originalData, closeOnEsc);
}
#endif

void ShowImageEditWindow(MainWindow* win, ImageEditMode mode, Str filePath, RenderedBitmap* rbmp, bool selectPdf,
                         Str originalData, bool closeOnEsc) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    InitImageEditHost();
    CloseImageEditWindow();

    Pixmap* px = nullptr;
    bool fromRendered = false;
    if (rbmp) {
        // ng: orig keeps the bitmap and the caller deletes it; here the
        // Pixmap takes its pixels over, so the editor owns it from now on
#if OS_WIN
        px = PixmapFromRenderedBitmap(rbmp);
        if (px && px->format == PixmapFormat::Native) {
            Pixmap* copy = PixmapCopyAs32bppDIB(px);
            FreePixmap(px);
            px = copy;
        }
#else
        // ng: a RenderedBitmap is win32-only, so rbmp is always null here
#endif
        fromRendered = true;
    } else if (len(filePath) > 0 && gImageEditHost.LoadImageFile) {
        px = gImageEditHost.LoadImageFile(filePath);
    }
    if (!px) {
        logf("ShowImageEditWindow: no image ('%s')\n", filePath);
        return;
    }

    gImgEdit.win = win;
    gImgEdit.mode = mode;
    gImgEdit.fromRenderedBitmap = fromRendered;
    gImgEdit.closeWithEsc = closeOnEsc || (gImageEditHost.escToExit && mode == ImageEditMode::Save);
    gImgEdit.srcPixmap = px;
    gImgEdit.imgW = px->width;
    gImgEdit.imgH = px->height;
    gImgEdit.srcWasModified = false;
    gImgEdit.filePath = str::Dup(filePath);
    gImgEdit.originalData = str::Dup(originalData);
    gImgEdit.originalExt = str::Dup(ImageSaveExtFromData(originalData));
    ResetToImageSize();

    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gImgEdit.destEdit = s;
    gImgEdit.ddFormat.Init(win->gpuiWin ? win->gpuiWin->app : nullptr);

    int fmtIdx = kDefaultFormatIdx;
    ProbeImageFormats();
    if (selectPdf) {
        fmtIdx = kPdfFormatIdx;
    } else if (len(filePath) > 0) {
        fmtIdx = FormatIdxFromExt(path::GetExtTemp(filePath));
    }
    FillFormats(fmtIdx);
    TempStr dest = len(filePath) > 0 ? str::DupTemp(filePath) : StrL("image.png");
    dest = PathWithExtTemp(dest, gImageFormats[fmtIdx].ext);
    if (len(filePath) > 0) {
        // orig: never the source file itself
        dest = MakeUniqueFilePathTemp(dest);
    }
    SetDestPath(dest);

    gImgEdit.visible = true;
    ImageEditOpenToolWindow(win);
#if OS_WIN
    ShowImageEditClassWindow();
#endif
    logf("%s\n", ImageEditStateTemp());
    AppShellInvalidate(win);
}

// --- events -----------------------------------------------------------------

void ImageEditView::OnCancel(ImageEditView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseImageEditWindow();
    gp::Notify(cx);
}

void ImageEditView::OnSave(ImageEditView*, gp::Ctx* cx, const gp::ClickEvent*) {
    DoSave();
    gp::Notify(cx);
}

void ImageEditView::OnCrop(ImageEditView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gImgEdit.mode == ImageEditMode::Crop && IsCropChanged()) {
        ApplyCrop();
    } else {
        SwitchToMode(ImageEditMode::Crop);
    }
    gp::Notify(cx);
    AppShellInvalidate(gImgEdit.win);
}

void ImageEditView::OnResize(ImageEditView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gImgEdit.mode == ImageEditMode::Resize && IsResizeChanged()) {
        ApplyResize();
    } else {
        SwitchToMode(ImageEditMode::Resize);
    }
    gp::Notify(cx);
    AppShellInvalidate(gImgEdit.win);
}

void ImageEditView::OnCopy(ImageEditView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ImageEditWnd& w = gImgEdit;
    Pixmap* px = w.srcPixmap;
    Pixmap* owned = nullptr;
    if (w.mode == ImageEditMode::Crop && IsCropChanged()) {
        owned = CropPixmap(px, w.cropX, w.cropY, w.cropW, w.cropH);
        px = owned;
    } else if (w.mode == ImageEditMode::Resize && IsResizeChanged()) {
        owned = ResizePixmap(px, w.newW, w.newH);
        px = owned;
    }
    bool ok = ImageEditCopyToClipboard(px);
    FreePixmap(owned);
    str::ReplaceWithCopy(&w.statusMsg, ok ? StrL("Copied to clipboard") : StrL("Copy failed"));
    w.statusMs = 0;
    gp::Notify(cx);
    AppShellInvalidate(w.win);
}

static void OnDestPathPicked(SavePathArgs* args) {
    if (len(args->path) == 0) {
        return;
    }
    SetDestPath(args->path);
    int idx = FormatIdxFromExt(path::GetExtTemp(args->path));
    for (int i = 0; i < len(gImgEdit.formatIndices); i++) {
        if (gImgEdit.formatIndices[i] == idx) {
            gImgEdit.ddFormat.SetSel(gImgEdit.win && gImgEdit.win->gpuiWin ? gImgEdit.win->gpuiWin->app : nullptr, i);
            break;
        }
    }
    AppShellInvalidate(gImgEdit.win);
}

void ImageEditView::OnBrowse(ImageEditView*, gp::Ctx* cx, const gp::ClickEvent*) {
    int fmtIdx = SelectedFormatIdx();
    auto* args = new SavePathArgs();
    args->win = gImgEdit.win;
    args->title = str::Dup(Tr("Save As"));
    args->initialPath = str::Dup(DestPathTemp());
    args->defExt = str::Dup(gImageFormats[fmtIdx].ext);
    args->filter =
        str::Dup(StrL("All Image Files\1*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tiff;*.tif;*.webp\1"
                      "PNG Files\1*.png\1"
                      "JPEG Files\1*.jpg;*.jpeg\1"
                      "BMP Files\1*.bmp\1"
                      "All Files\1*.*\1"));
    args->noDefExt = true;
    args->onDone = MkFunc1Void<SavePathArgs*>(OnDestPathPicked);
    ShowSavePathDialog(args);
    gp::Notify(cx);
}

void ImageEditView::OnDown(ImageEditView*, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    ImageEditWnd& w = gImgEdit;
    if (ev->button != gp::MouseButton::Left) {
        return;
    }
    w.dragStart = Point{(int)ev->x, (int)ev->y};
    w.dragMoved = false;
    if (w.mode == ImageEditMode::Crop) {
        w.dragEdge = HitTestCropEdgeOrNewCrop(ev->x, ev->y);
        w.dragCropX = w.cropX;
        w.dragCropY = w.cropY;
        w.dragCropW = w.cropW;
        w.dragCropH = w.cropH;
    } else if (w.mode == ImageEditMode::Resize) {
        w.dragEdge = HitTestResizeEdge(ev->x, ev->y);
        w.dragNewW = w.newW;
        w.dragNewH = w.newH;
    } else {
        w.dragEdge = DragEdge::None;
    }
    w.isDragging = w.dragEdge != DragEdge::None;
    gp::Notify(cx);
}

static void DragCropTo(float mx, float my) {
    ImageEditWnd& w = gImgEdit;
    int ix = DisplayToImageX(mx);
    int iy = DisplayToImageY(my);
    int x0 = w.dragCropX;
    int y0 = w.dragCropY;
    int x1 = w.dragCropX + w.dragCropW;
    int y1 = w.dragCropY + w.dragCropH;
    switch (w.dragEdge) {
        case DragEdge::Left:
        case DragEdge::TopLeft:
        case DragEdge::BottomLeft:
            x0 = std::min(ix, x1 - 1);
            break;
        case DragEdge::Right:
        case DragEdge::TopRight:
        case DragEdge::BottomRight:
            x1 = std::max(ix, x0 + 1);
            break;
        default:
            break;
    }
    switch (w.dragEdge) {
        case DragEdge::Top:
        case DragEdge::TopLeft:
        case DragEdge::TopRight:
            y0 = std::min(iy, y1 - 1);
            break;
        case DragEdge::Bottom:
        case DragEdge::BottomLeft:
        case DragEdge::BottomRight:
            y1 = std::max(iy, y0 + 1);
            break;
        default:
            break;
    }
    if (w.dragEdge == DragEdge::Move) {
        int dx = (int)((mx - (float)w.dragStart.x) / w.scale);
        int dy = (int)((my - (float)w.dragStart.y) / w.scale);
        x0 = limitValue(w.dragCropX + dx, 0, w.imgW - w.dragCropW);
        y0 = limitValue(w.dragCropY + dy, 0, w.imgH - w.dragCropH);
        x1 = x0 + w.dragCropW;
        y1 = y0 + w.dragCropH;
    }
    w.cropX = limitValue(x0, 0, w.imgW - 1);
    w.cropY = limitValue(y0, 0, w.imgH - 1);
    w.cropW = limitValue(x1 - w.cropX, 1, w.imgW - w.cropX);
    w.cropH = limitValue(y1 - w.cropY, 1, w.imgH - w.cropY);
}

// orig's resize drag: the rectangle is centered, so an edge moves twice the
// pointer's way; the size is free of the image's (the window grows)
static void DragResizeRectTo(float mx, float my) {
    ImageEditWnd& w = gImgEdit;
    int imgDx = (int)((mx - (float)w.dragStart.x) / w.scale);
    int imgDy = (int)((my - (float)w.dragStart.y) / w.scale);
    DragEdge edge = w.dragEdge;
    int nw = w.dragNewW;
    int nh = w.dragNewH;
    if (edge == DragEdge::Left || edge == DragEdge::TopLeft || edge == DragEdge::BottomLeft) {
        nw = w.dragNewW - (imgDx * 2);
    }
    if (edge == DragEdge::Right || edge == DragEdge::TopRight || edge == DragEdge::BottomRight) {
        nw = w.dragNewW + (imgDx * 2);
    }
    if (edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight) {
        nh = w.dragNewH - (imgDy * 2);
    }
    if (edge == DragEdge::Bottom || edge == DragEdge::BottomLeft || edge == DragEdge::BottomRight) {
        nh = w.dragNewH + (imgDy * 2);
    }
    w.newW = std::max(nw, 1);
    w.newH = std::max(nh, 1);
}

static void DragResizeTo(float mx, float my) {
    ImageEditWnd& w = gImgEdit;
    if (w.tw) {
        DragResizeRectTo(mx, my);
        return;
    }
    int nw = w.newW;
    int nh = w.newH;
    if (w.dragEdge == DragEdge::Right || w.dragEdge == DragEdge::BottomRight) {
        nw = std::max(1, DisplayToImageX(mx));
    }
    if (w.dragEdge == DragEdge::Bottom || w.dragEdge == DragEdge::BottomRight) {
        nh = std::max(1, DisplayToImageY(my));
    }
    // keep the aspect ratio, as orig's corner drag does
    if (w.dragEdge == DragEdge::Right) {
        nh = std::max(1, (nw * w.imgH) / w.imgW);
    } else if (w.dragEdge == DragEdge::Bottom) {
        nw = std::max(1, (nh * w.imgW) / w.imgH);
    }
    w.newW = nw;
    w.newH = nh;
}

static void ImageEditDragTo(gp::Ctx* cx, float mx, float my) {
    ImageEditWnd& w = gImgEdit;
    w.dragMoved = true;
    if (w.dragEdge == DragEdge::NewCrop) {
        SetCropFromDisplaySelection(w.dragStart, mx, my);
    } else if (w.mode == ImageEditMode::Crop) {
        DragCropTo(mx, my);
    } else if (w.mode == ImageEditMode::Resize) {
        DragResizeTo(mx, my);
        ImageEditToolGrow(w.dragEdge);
    }
    gp::Notify(cx);
    AppShellInvalidate(w.win);
}

void ImageEditView::OnMove(ImageEditView*, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    ImageEditWnd& w = gImgEdit;
    if (!w.isDragging) {
        if (w.mode == ImageEditMode::Crop) {
            w.hoverEdge = HitTestCropEdge(ev->x, ev->y);
        } else if (w.mode == ImageEditMode::Resize) {
            w.hoverEdge = HitTestResizeEdge(ev->x, ev->y);
        }
        return;
    }
    // in its own window the drag follows the pointer past the image area
    // (OnDragMove)
    if (w.tw) {
        return;
    }
    ImageEditDragTo(cx, ev->x, ev->y);
}

// ng: a mouse-move listener hears the pointer only over its element; the
// drag listener of the pressed element hears it wherever it goes, which a
// handle pulled past the window's edge needs
void ImageEditView::OnDragMove(ImageEditView*, gp::Ctx* cx, const gp::DragMoveEvent* ev) {
    if (!gImgEdit.isDragging || !gImgEdit.tw) {
        return;
    }
    ImageEditDragTo(cx, ev->event.x, ev->event.y);
}

void ImageEditView::OnUp(ImageEditView*, gp::Ctx* cx, const gp::MouseUpEvent*) {
    ImageEditWnd& w = gImgEdit;
    if (!w.isDragging) {
        return;
    }
    w.isDragging = false;
    // a click that moved nothing in a fresh crop leaves the whole image
    if (w.dragEdge == DragEdge::NewCrop && !w.dragMoved) {
        w.cropX = 0;
        w.cropY = 0;
        w.cropW = w.imgW;
        w.cropH = w.imgH;
    }
    w.dragEdge = DragEdge::None;
    logf("%s\n", ImageEditStateTemp());
    gp::Notify(cx);
    AppShellInvalidate(w.win);
}

// orig nudges the edge under the cursor by one pixel (ten with Shift)
// orig's WM_KEYDOWN: Ctrl + C is the Copy button
bool ImageEditOnKeyDown(MainWindow* win, int vk, bool ctrl) {
    ImageEditWnd& w = gImgEdit;
    if (!w.visible || w.win != win || !ctrl || vk != 'C' || !gImgEditView.IsValid() || !win->gpuiWin) {
        return false;
    }
    gp::Window* gw = w.tw ? ToolWindowGpui(w.tw) : win->gpuiWin;
    if (!gw) {
        return false;
    }
    gp::ClickEvent ev;
    ev.keyboard = true;
    gp::Listener l = gp::ListenTo(gImgEditView, &ImageEditView::OnCopy);
    gp::ListenerCall(gw->app, gw, l, &ev);
    return true;
}

// orig: Esc closes only with closeWithEsc, otherwise it returns from crop /
// resize to save mode
void ImageEditOnEscape() {
    ImageEditWnd& w = gImgEdit;
    if (!w.visible) {
        return;
    }
    if (w.closeWithEsc) {
        CloseImageEditWindow();
    } else if (w.mode != ImageEditMode::Save) {
        SwitchToMode(ImageEditMode::Save);
        AppShellInvalidate(w.win);
    }
}

// orig's HandleImageEditArrowKey
bool ImageEditOnArrowKey(MainWindow* win, int vk, bool) {
    ImageEditWnd& w = gImgEdit;
    if (!w.visible || w.win != win) {
        return false;
    }
    if (w.mode == ImageEditMode::Resize) {
        if (vk == VK_LEFT) {
            w.newW -= 1;
        } else if (vk == VK_RIGHT) {
            w.newW += 1;
        } else if (vk == VK_UP) {
            w.newH += 1;
        } else if (vk == VK_DOWN) {
            w.newH -= 1;
        } else {
            return false;
        }
        w.newW = std::max(w.newW, 1);
        w.newH = std::max(w.newH, 1);
        logf("%s\n", ImageEditStateTemp());
        AppShellInvalidate(win);
        return true;
    }
    if (w.mode != ImageEditMode::Crop) {
        return false;
    }
    auto edge = w.hoverEdge;
    if (edge == DragEdge::None) {
        return false;
    }
    int dx = 0, dy = 0;
    if (vk == VK_LEFT) {
        dx = -1;
    } else if (vk == VK_RIGHT) {
        dx = 1;
    } else if (vk == VK_UP) {
        dy = -1;
    } else if (vk == VK_DOWN) {
        dy = 1;
    } else {
        return false;
    }
    if (edge == DragEdge::Move) {
        w.cropX += dx;
        w.cropY += dy;
        w.cropX = std::max(w.cropX, 0);
        w.cropY = std::max(w.cropY, 0);
        if (w.cropX + w.cropW > w.imgW) {
            w.cropX = w.imgW - w.cropW;
        }
        if (w.cropY + w.cropH > w.imgH) {
            w.cropY = w.imgH - w.cropH;
        }
    } else {
        if (dx != 0 && (edge == DragEdge::Left || edge == DragEdge::TopLeft || edge == DragEdge::BottomLeft)) {
            w.cropX += dx;
            w.cropW -= dx;
        }
        if (dx != 0 && (edge == DragEdge::Right || edge == DragEdge::TopRight || edge == DragEdge::BottomRight)) {
            w.cropW += dx;
        }
        if (dy != 0 && (edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight)) {
            w.cropY += dy;
            w.cropH -= dy;
        }
        if (dy != 0 && (edge == DragEdge::Bottom || edge == DragEdge::BottomLeft || edge == DragEdge::BottomRight)) {
            w.cropH += dy;
        }
        if (w.cropX < 0) {
            w.cropW += w.cropX;
            w.cropX = 0;
        }
        if (w.cropY < 0) {
            w.cropH += w.cropY;
            w.cropY = 0;
        }
        w.cropW = std::max(w.cropW, 1);
        w.cropH = std::max(w.cropH, 1);
        if (w.cropX + w.cropW > w.imgW) {
            w.cropW = w.imgW - w.cropX;
        }
        if (w.cropY + w.cropH > w.imgH) {
            w.cropH = w.imgH - w.cropY;
        }
    }
    logf("%s\n", ImageEditStateTemp());
    AppShellInvalidate(win);
    return true;
}

// Right arrow grows the width. The destination edit is focused first: that is
// where Tab lands, and the arrow must still resize (issue #5734).
TempStr ImageResizeArrowKeyResultTemp(Str imagePath, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (len(imagePath) == 0 || !file::Exists(imagePath)) {
        return fail(StrL("ERROR missing-image"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    ShowImageEditWindow(win, ImageEditMode::Resize, imagePath);
    if (!gImgEdit.visible || gImgEdit.mode != ImageEditMode::Resize) {
        return fail(StrL("ERROR dialog-not-opened"));
    }
    if (win->gpuiWin && gImgEdit.destEdit) {
        gp::InputFocus(gImgEdit.destEdit, win->gpuiWin->app, win->gpuiWin);
    }
    int wBefore = gImgEdit.newW;
    bool handled = ImageEditOnArrowKey(win, VK_RIGHT, false);
    int wAfter = gImgEdit.newW;
    CloseImageEditWindow();
    if (!handled || wAfter != wBefore + 1) {
        out.Append(fmt("FAIL before=%d after=%d\n", wBefore, wAfter));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }
    out.Append(fmt("OK newW=%d\n", wAfter));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// --- building ---------------------------------------------------------------

static TempStr InfoTextTemp() {
    ImageEditWnd& w = gImgEdit;
    if (len(w.statusMsg) > 0) {
        return str::DupTemp(w.statusMsg);
    }
    // orig's UpdateInfoLabel
    if (w.mode == ImageEditMode::Crop && IsCropChanged()) {
        return fmt("%d x %d => %d x %d @ %d, %d", w.imgW, w.imgH, w.cropW, w.cropH, w.cropX, w.cropY);
    }
    if (w.mode == ImageEditMode::Resize && IsResizeChanged()) {
        float pctW = (float)w.newW * 100.0f / (float)w.imgW;
        float pctH = (float)w.newH * 100.0f / (float)w.imgH;
        return fmt("%d x %d => %d x %d (%.2f%% x %.2f%%)", w.imgW, w.imgH, w.newW, w.newH, pctW, pctH);
    }
    return fmt("%d x %d", w.imgW, w.imgH);
}

static Str DialogTitle() {
    switch (gImgEdit.mode) {
        case ImageEditMode::Crop:
            return HostTr(StrL("Crop Image"));
        case ImageEditMode::Resize:
            return HostTr(StrL("Resize Image"));
        default:
            return HostTr(StrL("Save Image"));
    }
}

// --- a window of its own (Windows) ------------------------------------------

// orig's GetControlAreaDy: the strip under the image, less the source path's
// row when the image did not come from a file
static float ImageEditControlAreaDy() {
    float dy = kImgControlAreaDy;
    if (gImgEdit.fromRenderedBitmap) {
        dy -= kImgLabelDy + kImgRowPadding;
    }
    return dy;
}

// orig's CalcImageEditWindowSizeEx: the window for the image at 100% with its
// padding and the control strip, at least kMinWindowWidth wide and no larger
// than the work area. Screen pixels
static Size ImageEditToolWindowSize(MainWindow* win, const ToolWindowDesc& desc, int imgW, int imgH) {
    int dpi = std::max(AppShellWindowDpi(win), 96);
    int clientDx = imgW + 2 * (int)kImgPadding;
    int clientDy = imgH + 2 * (int)kImgPadding + (int)ImageEditControlAreaDy();
    Size sz = ToolWindowOuterSize(desc, win, Size(clientDx, clientDy));
    sz.dx = std::max(sz.dx, MulDiv(kImgMinWindowDx, dpi, 96));
    Rect work = AppShellWorkArea(win);
    sz.dx = std::min(sz.dx, work.dx);
    sz.dy = std::min(sz.dy, work.dy);
    return sz;
}

static Str ImageEditToolTitle();
static ToolWindowDesc ImageEditToolDesc();

// orig's ResizeImageEditWindowToImage, after a crop or a resize was applied:
// the window is sized to the new image (not below 320 x 200 of image when it
// got smaller) and centered on the main window again
static void ImageEditToolSizeToImage(int prevW, int prevH) {
    ImageEditWnd& w = gImgEdit;
    if (!w.tw || !IsMainWindowValid(w.win)) {
        return;
    }
    int layoutW = w.imgW;
    int layoutH = w.imgH;
    if (w.imgW < prevW || w.imgH < prevH) {
        layoutW = std::max(layoutW, kImgDownsizeMinDx);
        layoutH = std::max(layoutH, kImgDownsizeMinDy);
    }
    Size sz = ImageEditToolWindowSize(w.win, ImageEditToolDesc(), layoutW, layoutH);
    ToolWindowMove(w.tw, ToolWindowCenteredOuter(w.win, sz));
}

static DragEdge gImgGrowEdge = DragEdge::None;
static bool gImgGrowPosted = false;

static void ImageEditToolGrowNow();

// ng: sizing a window makes gpui render it at once; inside the mouse-move
// listener that drops the frame the drag is running in, so the window grows
// from the ui task queue
static void ImageEditToolGrow(DragEdge edge) {
    if (!gImgEdit.tw || gImgEdit.mode != ImageEditMode::Resize) {
        return;
    }
    gImgGrowEdge = edge;
    if (gImgGrowPosted) {
        return;
    }
    gImgGrowPosted = true;
    uitask::Post(MkFunc0Void(ImageEditToolGrowNow), "ImageEditToolGrow");
}

// orig's GrowWindowIfNeeded: while the new size is dragged past what the
// window shows, the window grows in the direction of the drag, up to the
// work area
static void ImageEditToolGrowNow() {
    gImgGrowPosted = false;
    DragEdge edge = gImgGrowEdge;
    ImageEditWnd& w = gImgEdit;
    if (!w.visible || !w.tw || w.mode != ImageEditMode::Resize || !IsMainWindowValid(w.win)) {
        return;
    }
    gp::Window* gw = ToolWindowGpui(w.tw);
    if (!gw) {
        return;
    }
    gp::WinSize ws = gp::WindowSize(gw);
    int dpi = std::max(AppShellWindowDpi(w.win), 96);
    float neededW = (float)w.newW * w.scale + 2 * kImgPadding;
    float neededH = (float)w.newH * w.scale + 2 * kImgPadding;
    int extraW = std::max(MulDiv((int)(neededW - ws.dipW), dpi, 96), 0);
    int extraH = std::max(MulDiv((int)(neededH - w.toolImgAreaDy), dpi, 96), 0);
    if (extraW <= 0 && extraH <= 0) {
        return;
    }
    Rect winRc = ToolWindowRect(w.tw);
    Rect work = AppShellWorkArea(w.win);
    int newW = std::min(winRc.dx + extraW, work.dx);
    int newH = std::min(winRc.dy + extraH, work.dy);
    int deltaW = newW - winRc.dx;
    int deltaH = newH - winRc.dy;
    if (deltaW <= 0 && deltaH <= 0) {
        return;
    }
    int newX = winRc.x;
    int newY = winRc.y;
    bool growLeft = edge == DragEdge::Left || edge == DragEdge::TopLeft || edge == DragEdge::BottomLeft;
    bool growUp = edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight;
    if (growLeft && deltaW > 0) {
        newX = std::max(winRc.x - deltaW, work.x);
    } else if (deltaW > 0 && newX + newW > work.Right()) {
        newX = std::max(work.Right() - newW, work.x);
    }
    if (growUp && deltaH > 0) {
        newY = std::max(winRc.y - deltaH, work.y);
    } else if (deltaH > 0 && newY + newH > work.Bottom()) {
        newY = std::max(work.Bottom() - newH, work.y);
    }
    ToolWindowMove(w.tw, Rect(newX, newY, newW, newH));
}

static TempStr InfoTextTemp();
static Str DialogTitle();

static Str ImageEditToolTitle() {
    return DialogTitle();
}

constexpr float kImgToolFontPx = 12;
constexpr float kImgCtrlDy = 23;
constexpr float kImgBtnPadDx = 9;
constexpr float kImgCtrlGap = 7;
constexpr float kImgFormatDx = 69;

// orig's control strip: the source path, then the destination edit with
// "...", Save and Copy, then the info line with the format and Crop / Resize.
// It is inset 8 on the left and 24 on the right (orig lays it out in the
// client width less 16 and pads that by 8 again)
static gp::El* ImageEditToolBuild(MainWindow*, gp::Ctx* cx) {
    ImageEditWnd& w = gImgEdit;
    if (!w.visible || !w.tw) {
        return nullptr;
    }
    if (!gImgEditView.IsValid()) {
        gImgEditView = gp::EntityNewState<ImageEditView>(cx->app);
    }
    if (w.ddFormat.PollChanged(cx->app)) {
        OnFormatChanged();
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float font = kImgToolFontPx * ToolWindowSetUiFontPx(cx, kImgToolFontPx);
    gp::WinSize ws = gp::WindowSize(cx->win);
    float controlDy = ImageEditControlAreaDy();
    w.toolImgAreaDy = std::max(ws.dipH - controlDy, 10.f);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0);
    gp::El* imgArea = gp::Div(cx->a)
                          ->PathClick(GStrL("imgedit-area"))
                          ->OnDrag(GStrL("sumatra-imgedit"))
                          ->W(gp::kFill)
                          ->H(w.toolImgAreaDy)
                          ->Shrink0()
                          ->OnMouseDown(gp::ListenTo(gImgEditView, &ImageEditView::OnDown))
                          ->OnMouseUp(gp::ListenTo(gImgEditView, &ImageEditView::OnUp))
                          ->OnMouseUpOut(gp::ListenTo(gImgEditView, &ImageEditView::OnUp))
                          ->OnMouseMove(gp::ListenTo(gImgEditView, &ImageEditView::OnMove))
                          ->OnDragMove(gp::ListenTo(gImgEditView, &ImageEditView::OnDragMove));
    imgArea->customPaint = &PaintImageArea;
    imgArea->customUser = nullptr;
    col->Child(imgArea);

    gp::El* strip = gp::Div(cx->a)
                        ->FlexCol()
                        ->W(gp::kFill)
                        ->H(controlDy)
                        ->Shrink0()
                        ->PadT(kImgRowPadding)
                        ->PadL(kImgButtonPadding)
                        ->PadR(3 * kImgButtonPadding);
    if (!w.fromRenderedBitmap) {
        strip->Child(gp::Div(cx->a)
                         ->FlexRow()
                         ->ItemsCenter()
                         ->W(gp::kFill)
                         ->H(kImgLabelDy + kImgRowPadding)
                         ->PadB(kImgRowPadding)
                         ->PadL(4)
                         ->Shrink0()
                         ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, w.filePath))
                                     ->Font(font)
                                     ->Fg(th.foreground)
                                     ->Flex1()
                                     ->MinW(0)
                                     ->Truncate()));
    }
    auto button = [&](gp::Str id, Str label, gp::Listener onClick, bool disabled) {
        gpc::Button* b = gpc::Button::New(cx, id)->Disabled(disabled);
        return DlgAccelEl(cx, b, label, onClick, disabled)->H(kImgCtrlDy)->PadX(kImgBtnPadDx)->Shrink0();
    };
    gp::Listener onSave = gp::ListenTo(gImgEditView, &ImageEditView::OnSave);
    // orig's UpdateSaveButtonText
    Str saveLabel = file::Exists(DestPathTemp()) ? HostTr(StrL("&Overwrite")) : HostTr(StrL("&Save"));
    gp::El* row2 = gp::Div(cx->a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->W(gp::kFill)
                       ->H(kImgCtrlDy + kImgRowPadding)
                       ->PadB(kImgRowPadding)
                       ->Gap(kImgCtrlGap)
                       ->Shrink0();
    row2->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gpc::Input::New(cx, GStrL("imgedit-dest"), w.destEdit)->WithSize(gp::UiSize::Small)->W(gp::kFill)->IntoEl()));
    row2->Child(
        button(GStrL("imgedit-browse"), StrL("..."), gp::ListenTo(gImgEditView, &ImageEditView::OnBrowse), false));
    row2->Child(button(GStrL("imgedit-save"), saveLabel, onSave, false));
    row2->Child(button(GStrL("imgedit-copy"), HostTr(StrL("C&opy")), gp::ListenTo(gImgEditView, &ImageEditView::OnCopy),
                       false));
    strip->Child(row2);
    DlgSetDefault(cx, onSave, true);

    // orig's UpdateModeButtons
    bool cropMode = w.mode == ImageEditMode::Crop;
    bool resizeMode = w.mode == ImageEditMode::Resize;
    Str cropLabel = cropMode ? HostTr(StrL("&Apply Crop")) : HostTr(StrL("&Crop"));
    Str resizeLabel = resizeMode ? HostTr(StrL("&Apply Resize")) : HostTr(StrL("&Resize"));
    gp::El* row3 = gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(kImgCtrlDy)->Gap(kImgCtrlGap)->Shrink0();
    row3->Child(
        gp::TextEl(cx->a, GpuiDup(cx->a, InfoTextTemp()))->Font(font)->Fg(th.foreground)->Flex1()->MinW(0)->Truncate());
    row3->Child(w.ddFormat.Build(cx, StrL("imgedit-format"), kImgFormatDx));
    row3->Child(button(GStrL("imgedit-crop"), cropLabel, gp::ListenTo(gImgEditView, &ImageEditView::OnCrop),
                       cropMode && !IsCropChanged()));
    row3->Child(button(GStrL("imgedit-resize"), resizeLabel, gp::ListenTo(gImgEditView, &ImageEditView::OnResize),
                       resizeMode && !IsResizeChanged()));
    strip->Child(row3);
    col->Child(strip);
    return col;
}

// the arrow keys nudge the size or the crop edge even from the destination
// edit (orig's issue #5734); Esc as ImageEditOnEscape
static bool ImageEditToolOnCaptureKey(MainWindow* win, gp::Ctx* cx, const gp::KeyEvent* ev) {
    ImageEditWnd& w = gImgEdit;
    if (!w.visible || IsTrackedPopupOpen(cx)) {
        return false;
    }
    bool isArrow = ev->vk == VK_LEFT || ev->vk == VK_RIGHT || ev->vk == VK_UP || ev->vk == VK_DOWN;
    if (isArrow && !ev->ctrl && !ev->alt) {
        return ImageEditOnArrowKey(win, ev->vk, ev->shift);
    }
    return false;
}

static bool ImageEditToolOnKey(MainWindow* win, gp::Ctx* cx, const gp::KeyEvent* ev) {
    ImageEditWnd& w = gImgEdit;
    if (!w.visible) {
        return false;
    }
    if (ev->vk == VK_ESCAPE) {
        ImageEditOnEscape();
        return true;
    }
    if (ev->vk == 'C' && ev->ctrl) {
        bool editFocused = cx->win->input && cx->win->input->focused;
        return !editFocused && ImageEditOnKeyDown(win, ev->vk, true);
    }
    if (ev->vk == VK_RETURN && !ev->ctrl && !ev->alt) {
        bool editFocused = cx->win->input && cx->win->input->focused;
        return DlgDefaultOnEnter(cx, editFocused);
    }
    return false;
}

// orig's window has no owner: it stays when the main window it was opened
// from closes
static void ImageEditToolOnOwnerClosed(MainWindow* newOwner) {
    gImgEdit.win = newOwner;
}

static void ImageEditToolOnClosed(MainWindow*) {
    gImgEdit.tw = nullptr;
    CloseImageEditWindow();
}

// orig: WS_OVERLAPPEDWINDOW, no owner
static ToolWindowDesc ImageEditToolDesc() {
    ToolWindowDesc desc;
    desc.name = "imageedit";
    desc.title = ImageEditToolTitle;
    desc.frame = ToolWinFrame::Overlapped;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::TopLevel;
    desc.build = ImageEditToolBuild;
    desc.onKey = ImageEditToolOnKey;
    desc.onCaptureKey = ImageEditToolOnCaptureKey;
    desc.onClosed = ImageEditToolOnClosed;
    desc.onOwnerClosed = ImageEditToolOnOwnerClosed;
    return desc;
}

static void ImageEditOpenToolWindow(MainWindow* win) {
    if (gImgEdit.tw || !ToolWindowsAvailable()) {
        return;
    }
    ToolWindowDesc desc = ImageEditToolDesc();
    Size sz = ImageEditToolWindowSize(win, desc, gImgEdit.imgW, gImgEdit.imgH);
    gImgEdit.tw = ToolWindowOpen(desc, win, ToolWindowCenteredOuter(win, sz));
}

gp::El* ImageEditWindowBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gImgEdit.visible || gImgEdit.tw || gImgEdit.win != win) {
        return nullptr;
    }
    if (!gImgEditView.IsValid()) {
        gImgEditView = gp::EntityNewState<ImageEditView>(cx->app);
    }
    if (gImgEdit.ddFormat.PollChanged(cx->app)) {
        OnFormatChanged();
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8);

    gp::El* imgArea = gp::Div(cx->a)
                          ->W(gp::kFill)
                          ->H(360)
                          ->Bg(th.tokens.muted)
                          ->OnMouseDown(gp::ListenTo(gImgEditView, &ImageEditView::OnDown))
                          ->OnMouseUp(gp::ListenTo(gImgEditView, &ImageEditView::OnUp))
                          ->OnMouseMove(gp::ListenTo(gImgEditView, &ImageEditView::OnMove));
    imgArea->customPaint = &PaintImageArea;
    imgArea->customUser = nullptr;
    body->Child(imgArea);

    body->Child(gp::TextEl(cx->a, GpuiDup(cx->a, InfoTextTemp()))->Font(12)->Fg(th.mutedFg));

    gp::El* destRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8);
    destRow->Child(gp::Div(cx->a)->Flex1()->Child(gpc::Input::New(cx, GStrL("imgedit-dest"), gImgEdit.destEdit)
                                                      ->WithSize(gp::UiSize::Small)
                                                      ->W(gp::kFill)
                                                      ->IntoEl()));
    destRow->Child(gpc::Button::New(cx, GStrL("imgedit-browse"))
                       ->Label(ToGpui(HostTr(StrL("Browse..."))))
                       ->WithSize(gp::UiSize::Small)
                       ->OnClick(gp::ListenTo(gImgEditView, &ImageEditView::OnBrowse))
                       ->IntoEl());
    destRow->Child(gImgEdit.ddFormat.Build(cx, StrL("imgedit-format"), 110));
    body->Child(destRow);

    gp::El* modes = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(8);
    // orig's UpdateModeButtons: the labels carry the Alt- mnemonics
    Str cropLabel =
        (gImgEdit.mode == ImageEditMode::Crop && IsCropChanged()) ? HostTr(StrL("&Apply Crop")) : HostTr(StrL("&Crop"));
    Str resizeLabel = (gImgEdit.mode == ImageEditMode::Resize && IsResizeChanged()) ? HostTr(StrL("&Apply Resize"))
                                                                                    : HostTr(StrL("&Resize"));
    modes->Child(DlgAccelEl(cx,
                            gpc::Button::New(cx, GStrL("imgedit-crop"))
                                ->WithSize(gp::UiSize::Small)
                                ->Selected(gImgEdit.mode == ImageEditMode::Crop),
                            cropLabel, gp::ListenTo(gImgEditView, &ImageEditView::OnCrop)));
    modes->Child(DlgAccelEl(cx,
                            gpc::Button::New(cx, GStrL("imgedit-resize"))
                                ->WithSize(gp::UiSize::Small)
                                ->Selected(gImgEdit.mode == ImageEditMode::Resize),
                            resizeLabel, gp::ListenTo(gImgEditView, &ImageEditView::OnResize)));
    modes->Child(DlgAccelEl(cx, gpc::Button::New(cx, GStrL("imgedit-copy"))->WithSize(gp::UiSize::Small),
                            HostTr(StrL("C&opy")), gp::ListenTo(gImgEditView, &ImageEditView::OnCopy)));

    gp::El* footer = DialogFooter(cx, modes, gImgEditView, HostTr(StrL("&Save")), Tr("Cancel"), &ImageEditView::OnSave,
                                  &ImageEditView::OnCancel);

    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(DialogTitle()))
        ->Body(body)
        ->Footer(footer)
        ->W(720)
        ->OnClose(gp::ListenTo(gImgEditView, &ImageEditView::OnCancel))
        ->IntoEl(gp::WindowSize(cx->win));
}
