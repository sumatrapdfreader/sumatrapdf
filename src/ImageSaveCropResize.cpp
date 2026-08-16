/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/GdiPlusUtil.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "PngOptimizer.h"
#include "ImageSaveCropResize.h"

ImageEditHost gImageEditHost;

// The host's translation of s, or s itself when it has none.
// A warning box. The host may have a styled one, but a plain one is fine here.
static void WarnBox(HWND hwnd, Str msg, Str title) {
    MessageBoxWarningSimple(hwnd, ToWStrTemp(msg), ToWStrTemp(title));
}

static PlatformFont* ImageEditFont(HWND hwnd) {
    if (hwnd) {
        DpiSetFromHwnd(hwnd);
    }
    if (gImageEditHost.GetFont) {
        return gImageEditHost.GetFont();
    }
    return GetDefaultGuiFont();
}

static Str Tr(Str s) {
    if (gImageEditHost.Translate) {
        Str t = gImageEditHost.Translate(s);
        if (t) {
            return t;
        }
    }
    return s;
}

#include <commctrl.h>
#include <wincodec.h>

using Gdiplus::Bitmap;
using Gdiplus::Graphics;
using Gdiplus::Ok;
using Gdiplus::Status;

constexpr const WCHAR* kImageEditWinClassName = L"SUMATRA_PDF_IMAGE_EDIT";

constexpr int kMinWindowWidth = 640;
constexpr int kDownsizeMinImgDx = 320;
constexpr int kDownsizeMinImgDy = 200;
constexpr int kImagePadding = 16;
constexpr int kResizeEdgeThreshold = 2;
constexpr int kDragHandleSize = 6;
constexpr int kControlAreaDy = 100;
constexpr int kRowPadding = 6;
constexpr int kButtonPadding = 8;

struct ImageFormat {
    Str label;
    const GUID* containerFormat; // WIC container format GUID (null for PDF)
    Str ext;
    bool needsProbe; // if true, check if encoder is available before offering
    bool available;  // set after probing
    bool isPdf;      // PDF is created via PdfCreator, not WIC
};

// clang-format off
static ImageFormat gImageFormats[] = {
    {StrL("PNG"),  &GUID_ContainerFormatPng,  StrL(".png"),  false, true,  false},
    {StrL("JPEG"), &GUID_ContainerFormatJpeg, StrL(".jpg"),  false, true,  false},
    {StrL("BMP"),  &GUID_ContainerFormatBmp,  StrL(".bmp"),  false, true,  false},
    {StrL("GIF"),  &GUID_ContainerFormatGif,  StrL(".gif"),  false, true,  false},
    {StrL("TIFF"), &GUID_ContainerFormatTiff, StrL(".tif"),  false, true,  false},
    {StrL("WebP"), &GUID_ContainerFormatWebp, StrL(".webp"), true,  false, false},
    {StrL("PDF"),  nullptr,                   StrL(".pdf"),  false, true,  true},
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
    if (!ext || !originalExt) {
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

static bool gFormatsProbed = false;

static void ProbeImageFormats() {
    if (gFormatsProbed) {
        return;
    }
    gFormatsProbed = true;

    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory),
                                  (void**)&pFactory);
    if (FAILED(hr) || !pFactory) {
        return;
    }

    for (auto& fmt : gImageFormats) {
        if (!fmt.needsProbe) {
            continue;
        }
        IWICBitmapEncoder* pEncoder = nullptr;
        hr = pFactory->CreateEncoder(*fmt.containerFormat, nullptr, &pEncoder);
        if (SUCCEEDED(hr) && pEncoder) {
            fmt.available = true;
            pEncoder->Release();
        }
    }

    pFactory->Release();
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

// a button labelled "&Save": virtual controls draw their text as-is, so the
// '&' is stripped for the label and kept here as the Alt- shortcut
struct ImageEditButton : VirtButton {
    using VirtButton::VirtButton;
    WCHAR mnemonic = 0;

    void SetLabel(Str s);
};

struct ImageEditWindow : WindowBase {
    ImageEditMode mode = ImageEditMode::Crop;
    bool fromRenderedBitmap = false;

    HWND hwndParent = nullptr;

    // the same tree as WindowBase::layout, which owns it. The window places it
    // itself (below the image area), so it keeps its own pointer to it
    ILayout* controlLayout = nullptr;
    VirtText* staticPathLabel = nullptr;
    VirtText* staticInfoLabel = nullptr;
    Edit* destEdit = nullptr;
    ImageEditButton* btnBrowse = nullptr;
    ImageEditButton* btnSave = nullptr;
    ImageEditButton* btnCrop = nullptr;   // "Crop" or "Apply Crop"
    ImageEditButton* btnResize = nullptr; // "Resize" or "Apply Resize"
    DropDown* dropFormat = nullptr;
    Vec<int> formatIndices; // maps dropdown index to gImageFormats index

    // source image
    Str filePath;
    // encoded original (JPEG/PNG/…); written as-is when dest ext matches
    // and the bitmap has not been cropped or resized
    Str originalData;
    Str originalExt;
    bool srcWasModified = false;
    Bitmap* srcBitmap = nullptr;
    Pixmap* srcPixmap = nullptr;
    int imgW = 0;
    int imgH = 0;

    // display
    int imgDisplayX = 0;
    int imgDisplayY = 0;
    int imgDisplayW = 0;
    int imgDisplayH = 0;
    int imgAreaH = 0; // height of the image area (window height - control area)

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
    DragEdge hoverEdge = DragEdge::None; // edge under mouse, for arrow key nudging
    POINT dragStart{};
    // crop mode drag state
    int dragCropX = 0;
    int dragCropY = 0;
    int dragCropW = 0;
    int dragCropH = 0;
    bool dragMoved = false;
    // resize mode drag state
    int dragNewW = 0;
    int dragNewH = 0;

    ImageEditWindow() = default;
    ~ImageEditWindow() override {
        delete srcBitmap;
        FreePixmap(srcPixmap);
        str::Free(filePath);
        str::Free(originalData);
        // ~WindowBase deletes `layout`, which is controlLayout
    }

    void WndProc(WindowBase::WndProcEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    void OnSize(WindowBase::SizeEvent* ev);
    void OnDpiChanged(WindowBase::DpiChangedEvent* ev);
    void OnActivate(WindowBase::ActivateEvent* ev);
    void OnDestroy(WindowBase::DestroyEvent* ev);
    void OnSetCursor(WindowBase::SetCursorEvent* ev);
};

// "&Save" -> label "Save", mnemonic 'S'
void ImageEditButton::SetLabel(Str str) {
    int ampIdx = str::IndexOfChar(str, '&');
    if (ampIdx < 0 || ampIdx + 1 >= len(str)) {
        mnemonic = 0;
        SetText(str);
        return;
    }
    mnemonic = (WCHAR)str.s[ampIdx + 1];
    TempStr stripped = str::JoinTemp(Str(str.s, ampIdx), Str(str.s + ampIdx + 1, len(str) - ampIdx - 1));
    SetText(stripped);
}

static Vec<ImageEditWindow*> gImageEditWindows;
static UINT_PTR gDestEditSubclassId = 0;

static void UpdateInfoLabel(ImageEditWindow* ew);
static void UpdateModeButtons(ImageEditWindow* ew);
static void InvalidateImageArea(ImageEditWindow* ew);
static bool HandleImageEditArrowKey(ImageEditWindow* ew, WPARAM wp);

static LRESULT CALLBACK WndProcDestEditSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*idSubclass*/,
                                                DWORD_PTR data) {
    ImageEditWindow* ew = (ImageEditWindow*)data;
    if (msg == WM_KEYDOWN && ew && HandleImageEditArrowKey(ew, wp)) {
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void DeleteImageEditWindow(ImageEditWindow* ew) {
    delete ew;
}

static WCHAR UpperW(WCHAR c) {
    if (c >= L'a' && c <= L'z') {
        return (WCHAR)(c - L'a' + L'A');
    }
    return c;
}

static void HideKeyboardCues(HWND hwnd) {
    SendMessageW(hwnd, WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEACCEL), 0);
}

static bool HandleImageEditArrowKey(ImageEditWindow* ew, WPARAM wp) {
    if (!ew) {
        return false;
    }
    if (ew->mode == ImageEditMode::Resize) {
        if (wp == VK_LEFT) {
            ew->newW -= 1;
        } else if (wp == VK_RIGHT) {
            ew->newW += 1;
        } else if (wp == VK_UP) {
            ew->newH += 1;
        } else if (wp == VK_DOWN) {
            ew->newH -= 1;
        } else {
            return false;
        }
        ew->newW = std::max(ew->newW, 1);
        ew->newH = std::max(ew->newH, 1);
        UpdateInfoLabel(ew);
        UpdateModeButtons(ew);
        InvalidateImageArea(ew);
        return true;
    }
    if (ew->mode != ImageEditMode::Crop) {
        return false;
    }
    auto edge = ew->hoverEdge;
    if (edge == DragEdge::None) {
        return false;
    }
    int dx = 0, dy = 0;
    if (wp == VK_LEFT) {
        dx = -1;
    } else if (wp == VK_RIGHT) {
        dx = 1;
    } else if (wp == VK_UP) {
        dy = -1;
    } else if (wp == VK_DOWN) {
        dy = 1;
    } else {
        return false;
    }
    if (edge == DragEdge::Move) {
        ew->cropX += dx;
        ew->cropY += dy;
        ew->cropX = std::max(ew->cropX, 0);
        ew->cropY = std::max(ew->cropY, 0);
        if (ew->cropX + ew->cropW > ew->imgW) {
            ew->cropX = ew->imgW - ew->cropW;
        }
        if (ew->cropY + ew->cropH > ew->imgH) {
            ew->cropY = ew->imgH - ew->cropH;
        }
    } else {
        if (dx != 0 && (edge == DragEdge::Left || edge == DragEdge::TopLeft || edge == DragEdge::BottomLeft)) {
            ew->cropX += dx;
            ew->cropW -= dx;
        }
        if (dx != 0 && (edge == DragEdge::Right || edge == DragEdge::TopRight || edge == DragEdge::BottomRight)) {
            ew->cropW += dx;
        }
        if (dy != 0 && (edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight)) {
            ew->cropY += dy;
            ew->cropH -= dy;
        }
        if (dy != 0 && (edge == DragEdge::Bottom || edge == DragEdge::BottomLeft || edge == DragEdge::BottomRight)) {
            ew->cropH += dy;
        }
        if (ew->cropX < 0) {
            ew->cropW += ew->cropX;
            ew->cropX = 0;
        }
        if (ew->cropY < 0) {
            ew->cropH += ew->cropY;
            ew->cropY = 0;
        }
        ew->cropW = std::max(ew->cropW, 1);
        ew->cropH = std::max(ew->cropH, 1);
        if (ew->cropX + ew->cropW > ew->imgW) {
            ew->cropW = ew->imgW - ew->cropX;
        }
        if (ew->cropY + ew->cropH > ew->imgH) {
            ew->cropH = ew->imgH - ew->cropY;
        }
    }
    UpdateInfoLabel(ew);
    UpdateModeButtons(ew);
    InvalidateImageArea(ew);
    return true;
}

static void RestoreImageEditFocus(ImageEditWindow* ew) {
    if (!ew || !ew->hwnd) {
        return;
    }
    if (ew->mode == ImageEditMode::Save && ew->destEdit) {
        SetFocus(ew->destEdit->hwnd);
    } else {
        SetFocus(ew->hwnd);
    }
}

static bool HasFocusInImageEdit(ImageEditWindow* ew) {
    if (!ew || !ew->hwnd) {
        return false;
    }
    HWND focus = GetFocus();
    return focus && (focus == ew->hwnd || IsChild(ew->hwnd, focus));
}

static bool TriggerImageEditMnemonic(ImageEditWindow* ew, WCHAR key) {
    if (!ew) {
        return false;
    }
    key = UpperW(key);
    ImageEditButton* btns[] = {ew->btnSave, ew->btnCrop, ew->btnResize};
    for (ImageEditButton* btn : btns) {
        if (!btn || !btn->IsEnabled() || btn->mnemonic == 0) {
            continue;
        }
        if (UpperW(btn->mnemonic) == key) {
            if (btn->onClick.IsValid()) {
                VirtMouseEvent ev;
                ev.target = btn;
                ev.hit = btn;
                btn->onClick.Call(&ev);
            }
            return true;
        }
    }
    return false;
}

// Convert display coordinates to image coordinates (crop mode)
static int DisplayToImageX(ImageEditWindow* ew, int dx) {
    if (ew->imgDisplayW <= 0) {
        return 0;
    }
    int v = (int)((float)(dx - ew->imgDisplayX) * (float)ew->imgW / (float)ew->imgDisplayW);
    return setMinMax(v, 0, ew->imgW);
}

static int DisplayToImageY(ImageEditWindow* ew, int dy) {
    if (ew->imgDisplayH <= 0) {
        return 0;
    }
    int v = (int)((float)(dy - ew->imgDisplayY) * (float)ew->imgH / (float)ew->imgDisplayH);
    return setMinMax(v, 0, ew->imgH);
}

// Convert image coordinates to display coordinates (crop mode)
static int ImageToDisplayX(ImageEditWindow* ew, int ix) {
    if (ew->imgW <= 0) {
        return ew->imgDisplayX;
    }
    return ew->imgDisplayX + (int)((float)ix * (float)ew->imgDisplayW / (float)ew->imgW);
}

static int ImageToDisplayY(ImageEditWindow* ew, int iy) {
    if (ew->imgH <= 0) {
        return ew->imgDisplayY;
    }
    return ew->imgDisplayY + (int)((float)iy * (float)ew->imgDisplayH / (float)ew->imgH);
}

// Convert display-scale sizes to image-scale sizes (resize mode)
static int DisplayToImageW(ImageEditWindow* ew, int dispW) {
    if (ew->imgDisplayW <= 0) {
        return 0;
    }
    return (int)((float)dispW * (float)ew->imgW / (float)ew->imgDisplayW);
}

static int DisplayToImageH(ImageEditWindow* ew, int dispH) {
    if (ew->imgDisplayH <= 0) {
        return 0;
    }
    return (int)((float)dispH * (float)ew->imgH / (float)ew->imgDisplayH);
}

// Convert image size to display size (resize mode)
static int ImageToDisplayW(ImageEditWindow* ew, int iw) {
    if (ew->imgW <= 0) {
        return 0;
    }
    return (int)((float)iw * (float)ew->imgDisplayW / (float)ew->imgW);
}

static int ImageToDisplayH(ImageEditWindow* ew, int ih) {
    if (ew->imgH <= 0) {
        return 0;
    }
    return (int)((float)ih * (float)ew->imgDisplayH / (float)ew->imgH);
}

static void LayoutControls(ImageEditWindow* ew);
static void CalcImageLayout(ImageEditWindow* ew);

static int ImageEditImagePadding() {
    return DpiScale(kImagePadding);
}

static int ImageEditRowPadding() {
    return DpiScale(kRowPadding);
}

static int ImageEditButtonPadding() {
    return DpiScale(kButtonPadding);
}

static int ImageEditLabelDy(ImageEditWindow* ew) {
    return PlatformFontMeasureText(ew->font, "Ag").dy;
}

static int ImageEditPathLabelRowDy(ImageEditWindow* ew) {
    return ImageEditLabelDy(ew) + ImageEditRowPadding();
}

static void ImageEditApplyFont(ImageEditWindow* ew) {
    PlatformFont* font = ew->font;
    auto setWndFont = [&](ControlBase* w) {
        if (w && w->hwnd) {
            w->SetFont(font);
        }
    };
    auto setVirtFont = [&](VirtText* w) {
        if (w) {
            w->font = font;
        }
    };
    setVirtFont(ew->staticPathLabel);
    setVirtFont(ew->staticInfoLabel);
    setVirtFont(ew->btnBrowse);
    setVirtFont(ew->btnSave);
    setVirtFont(ew->btnCrop);
    setVirtFont(ew->btnResize);
    setWndFont(ew->destEdit);
    setWndFont(ew->dropFormat);
}

static int GetSelectedFormatIdx(ImageEditWindow* ew) {
    if (!ew->dropFormat) {
        return kDefaultFormatIdx;
    }
    int ddIdx = ew->dropFormat->GetCurrentSelection();
    if (ddIdx < 0 || ddIdx >= len(ew->formatIndices)) {
        return kDefaultFormatIdx;
    }
    return ew->formatIndices[ddIdx];
}

static void OnFormatChanged(ImageEditWindow* ew) {
    int idx = GetSelectedFormatIdx(ew);
    Str newExt = gImageFormats[idx].ext;
    // update the extension in the dest path
    TempStr dest = ew->destEdit ? ew->destEdit->GetTextTemp() : TempStr{};
    if (len(dest) > 0) {
        TempStr oldExt = path::GetExtTemp(dest);
        int baseLen = len(dest) - len(oldExt);
        TempStr base = str::DupTemp(Str(dest.s, baseLen));
        TempStr newDest = fmt("%s%s", base, newExt);
        ew->destEdit->SetText(newDest);
    }
    SetFocus(ew->hwnd);
}

static void UpdateSaveButtonText(ImageEditWindow* ew) {
    if (!ew->btnSave || !ew->destEdit) {
        return;
    }
    TempStr dest = ew->destEdit->GetTextTemp();
    Str text = file::Exists(dest) ? Str(Tr("&Overwrite")) : Str(Tr("&Save"));
    ew->btnSave->SetLabel(text);
    // re-layout since button width may have changed
    LayoutControls(ew);
}

static bool IsCropChanged(ImageEditWindow* ew);
static bool IsResizeChanged(ImageEditWindow* ew);

static TempStr FormatCropInfoTemp(int srcW, int srcH, int cropW, int cropH, int cropX, int cropY) {
    return fmt("%d x %d => %d x %d @ %d, %d", srcW, srcH, cropW, cropH, cropX, cropY);
}

static TempStr FormatResizeInfoTemp(int srcW, int srcH, int newW, int newH) {
    float pctW = (srcW > 0) ? (float)newW * 100.0f / (float)srcW : 0.0f;
    float pctH = (srcH > 0) ? (float)newH * 100.0f / (float)srcH : 0.0f;
    return fmt("%d x %d => %d x %d (%.2f%% x %.2f%%)", srcW, srcH, newW, newH, pctW, pctH);
}

static void UpdateInfoLabel(ImageEditWindow* ew) {
    TempStr s;
    if (ew->mode == ImageEditMode::Crop && IsCropChanged(ew)) {
        s = FormatCropInfoTemp(ew->imgW, ew->imgH, ew->cropW, ew->cropH, ew->cropX, ew->cropY);
    } else if (ew->mode == ImageEditMode::Resize && IsResizeChanged(ew)) {
        s = FormatResizeInfoTemp(ew->imgW, ew->imgH, ew->newW, ew->newH);
    } else {
        s = fmt("%d x %d", ew->imgW, ew->imgH);
    }
    if (ew->staticInfoLabel) {
        ew->staticInfoLabel->SetText(s);
    }
    LayoutControls(ew);
}

// invalidate only the image area, not the control area below
static void InvalidateImageArea(ImageEditWindow* ew) {
    Rect imageRect = HwndClientRect(ew->hwnd);
    imageRect.dy = ew->imgAreaH;
    HwndInvalidateRect(ew->hwnd, imageRect, false);
}

static int GetControlAreaDy(ImageEditWindow* ew) {
    int dy = DpiScale(kControlAreaDy);
    if (ew->fromRenderedBitmap) {
        dy -= ImageEditPathLabelRowDy(ew);
    }
    return dy;
}

// Client image dimensions used to compute outer window size (image at 100% + padding + controls).
static Size CalcImageEditWindowSizeEx(HWND dpiHwnd, HWND hwndParent, bool fromRenderedBitmap, int imgW, int imgH,
                                      ImageEditWindow* ew) {
    int imagePadding;
    int controlDy;
    if (ew && ew->hwnd) {
        imagePadding = ImageEditImagePadding();
        controlDy = GetControlAreaDy(ew);
        dpiHwnd = ew->hwnd;
    } else {
        imagePadding = DpiScale(kImagePadding);
        controlDy = DpiScale(kControlAreaDy);
        if (fromRenderedBitmap) {
            controlDy -= DpiScale(16) + DpiScale(kRowPadding);
        }
    }
    int wantW = imgW + (2 * imagePadding);
    int wantH = imgH + (2 * imagePadding) + controlDy;
    RECT rc = {0, 0, wantW, wantH};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int winW = rc.right - rc.left;
    int minWinW = DpiScale(kMinWindowWidth);
    winW = std::max(winW, minWinW);
    int winH = rc.bottom - rc.top;
    HMONITOR hMon = MonitorFromWindow(hwndParent ? hwndParent : dpiHwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hMon, &mi);
    int screenW = mi.rcWork.right - mi.rcWork.left;
    int screenH = mi.rcWork.bottom - mi.rcWork.top;
    winW = std::min(winW, screenW);
    winH = std::min(winH, screenH);
    return {winW, winH};
}

static void ImageEditLayoutDimensions(int imgW, int imgH, bool downsizing, int* layoutW, int* layoutH) {
    *layoutW = imgW;
    *layoutH = imgH;
    if (downsizing) {
        int minDx = DpiScale(kDownsizeMinImgDx);
        int minDy = DpiScale(kDownsizeMinImgDy);
        *layoutW = std::max(*layoutW, minDx);
        *layoutH = std::max(*layoutH, minDy);
    }
}

static void ResizeImageEditWindowToImage(ImageEditWindow* ew, int prevW, int prevH) {
    bool downsizing = ew->imgW < prevW || ew->imgH < prevH;
    int layoutW, layoutH;
    ImageEditLayoutDimensions(ew->imgW, ew->imgH, downsizing, &layoutW, &layoutH);
    Size winSize = CalcImageEditWindowSizeEx(ew->hwnd, ew->hwndParent, ew->fromRenderedBitmap, layoutW, layoutH, ew);
    SetWindowPos(ew->hwnd, nullptr, 0, 0, winSize.dx, winSize.dy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    HwndCenterDialog(ew->hwnd, ew->hwndParent);
    CalcImageLayout(ew);
    LayoutControls(ew);
    HwndInvalidate(ew->hwnd, true);
}

static void CalcImageLayout(ImageEditWindow* ew) {
    Rect cRc = HwndClientRect(ew->hwnd);
    ew->imgAreaH = cRc.dy - GetControlAreaDy(ew);
    ew->imgAreaH = std::max(ew->imgAreaH, 10);

    int imgPad = ImageEditImagePadding();
    // fit image within image area with padding
    int availW = cRc.dx - (2 * imgPad);
    int availH = ew->imgAreaH - (2 * imgPad);
    if (availW <= 0 || availH <= 0 || ew->imgW <= 0 || ew->imgH <= 0) {
        ew->imgDisplayX = imgPad;
        ew->imgDisplayY = imgPad;
        ew->imgDisplayW = 0;
        ew->imgDisplayH = 0;
        return;
    }

    float scaleX = (float)availW / (float)ew->imgW;
    float scaleY = (float)availH / (float)ew->imgH;
    float scale = std::min(scaleX, scaleY);
    // don't upscale beyond 100%
    scale = std::min(scale, 1.0f);

    ew->imgDisplayW = (int)((float)ew->imgW * scale);
    ew->imgDisplayH = (int)((float)ew->imgH * scale);
    // center in available area
    ew->imgDisplayX = imgPad + ((availW - ew->imgDisplayW) / 2);
    ew->imgDisplayY = imgPad + ((availH - ew->imgDisplayH) / 2);
}

// Grow the window if the new-size rectangle exceeds the image display area (resize mode only).
// Tries to grow in the direction of the drag, moving the window if needed,
// but stops at screen edges.
static void GrowWindowIfNeeded(ImageEditWindow* ew, DragEdge edge) {
    int imgPad = ImageEditImagePadding();
    // calculate how much display space the new size needs
    int neededDispW = ImageToDisplayW(ew, ew->newW) + (2 * imgPad);
    int neededDispH = ImageToDisplayH(ew, ew->newH) + (2 * imgPad);

    Rect cRc = HwndClientRect(ew->hwnd);
    int availW = cRc.dx;
    int availH = ew->imgAreaH;

    int extraW = neededDispW - availW;
    int extraH = neededDispH - availH;
    if (extraW <= 0 && extraH <= 0) {
        return;
    }
    extraW = std::max(extraW, 0);
    extraH = std::max(extraH, 0);

    // get screen work area
    HMONITOR hMon = MonitorFromWindow(ew->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hMon, &mi);
    int screenL = mi.rcWork.left;
    int screenT = mi.rcWork.top;
    int screenR = mi.rcWork.right;
    int screenB = mi.rcWork.bottom;

    Rect winRc = HwndWindowRect(ew->hwnd);
    int winX = winRc.x;
    int winY = winRc.y;
    int winW = winRc.dx;
    int winH = winRc.dy;

    int newWinW = winW + extraW;
    int newWinH = winH + extraH;

    // don't exceed screen size
    int maxW = screenR - screenL;
    int maxH = screenB - screenT;
    newWinW = std::min(newWinW, maxW);
    newWinH = std::min(newWinH, maxH);

    int deltaW = newWinW - winW;
    int deltaH = newWinH - winH;
    if (deltaW <= 0 && deltaH <= 0) {
        return;
    }

    int newX = winX;
    int newY = winY;

    // determine grow direction based on which edge is being dragged
    bool growLeft = (edge == DragEdge::Left || edge == DragEdge::TopLeft || edge == DragEdge::BottomLeft);
    bool growUp = (edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight);

    if (growLeft && deltaW > 0) {
        newX = winX - deltaW;
        newX = std::max(newX, screenL);
    } else if (deltaW > 0) {
        // grow right - check we don't go past screen edge
        if (newX + newWinW > screenR) {
            newX = screenR - newWinW;
            newX = std::max(newX, screenL);
        }
    }

    if (growUp && deltaH > 0) {
        newY = winY - deltaH;
        newY = std::max(newY, screenT);
    } else if (deltaH > 0) {
        // grow down
        if (newY + newWinH > screenB) {
            newY = screenB - newWinH;
            newY = std::max(newY, screenT);
        }
    }

    SetWindowPos(ew->hwnd, nullptr, newX, newY, newWinW, newWinH, SWP_NOZORDER);
    // recalc layout after window resize
    CalcImageLayout(ew);
}

static DragEdge HitTestCropEdge(ImageEditWindow* ew, int mx, int my) {
    int left = ImageToDisplayX(ew, ew->cropX);
    int right = ImageToDisplayX(ew, ew->cropX + ew->cropW);
    int top = ImageToDisplayY(ew, ew->cropY);
    int bottom = ImageToDisplayY(ew, ew->cropY + ew->cropH);

    int t = kResizeEdgeThreshold;

    bool onLeft = (mx >= left - t && mx <= left + t);
    bool onRight = (mx >= right - t && mx <= right + t);
    bool onTop = (my >= top - t && my <= top + t);
    bool onBottom = (my >= bottom - t && my <= bottom + t);

    bool inVertRange = (my >= top - t && my <= bottom + t);
    bool inHorzRange = (mx >= left - t && mx <= right + t);

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
    if (onLeft && inVertRange) {
        return DragEdge::Left;
    }
    if (onRight && inVertRange) {
        return DragEdge::Right;
    }
    if (onTop && inHorzRange) {
        return DragEdge::Top;
    }
    if (onBottom && inHorzRange) {
        return DragEdge::Bottom;
    }
    // inside the crop rect = move
    if (mx > left + t && mx < right - t && my > top + t && my < bottom - t) {
        return DragEdge::Move;
    }
    return DragEdge::None;
}

static DragEdge HitTestResizeEdge(ImageEditWindow* ew, int mx, int my) {
    // the "new size" rectangle, centered in the display area
    int dispNewW = ImageToDisplayW(ew, ew->newW);
    int dispNewH = ImageToDisplayH(ew, ew->newH);
    int cx = ew->imgDisplayX + (ew->imgDisplayW / 2);
    int cy = ew->imgDisplayY + (ew->imgDisplayH / 2);
    int left = cx - (dispNewW / 2);
    int right = left + dispNewW;
    int top = cy - (dispNewH / 2);
    int bottom = top + dispNewH;

    int t = kResizeEdgeThreshold;

    bool onLeft = (mx >= left - t && mx <= left + t);
    bool onRight = (mx >= right - t && mx <= right + t);
    bool onTop = (my >= top - t && my <= top + t);
    bool onBottom = (my >= bottom - t && my <= bottom + t);

    bool inVertRange = (my >= top - t && my <= bottom + t);
    bool inHorzRange = (mx >= left - t && mx <= right + t);

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
    if (onLeft && inVertRange) {
        return DragEdge::Left;
    }
    if (onRight && inVertRange) {
        return DragEdge::Right;
    }
    if (onTop && inHorzRange) {
        return DragEdge::Top;
    }
    if (onBottom && inHorzRange) {
        return DragEdge::Bottom;
    }
    return DragEdge::None;
}

static bool IsPointInDisplayedImage(ImageEditWindow* ew, int mx, int my) {
    return mx >= ew->imgDisplayX && mx <= ew->imgDisplayX + ew->imgDisplayW && my >= ew->imgDisplayY &&
           my <= ew->imgDisplayY + ew->imgDisplayH;
}

// like HitTestCropEdge(), but a hit on a not-yet-changed crop rect, or anywhere
// in the image outside it, starts a new crop instead
static DragEdge HitTestCropEdgeOrNewCrop(ImageEditWindow* ew, int mx, int my) {
    DragEdge edge = HitTestCropEdge(ew, mx, my);
    bool startsNewCrop = (edge == DragEdge::Move && !IsCropChanged(ew)) ||
                         (edge == DragEdge::None && IsPointInDisplayedImage(ew, mx, my));
    return startsNewCrop ? DragEdge::NewCrop : edge;
}

static bool IsImageEditDragDistance(POINT start, int mx, int my) {
    int dragDx = GetSystemMetrics(SM_CXDRAG);
    int dragDy = GetSystemMetrics(SM_CYDRAG);
    return abs(mx - start.x) > dragDx || abs(my - start.y) > dragDy;
}

static void SetCropFromDisplaySelection(ImageEditWindow* ew, POINT start, int mx, int my) {
    int x0 = DisplayToImageX(ew, start.x);
    int y0 = DisplayToImageY(ew, start.y);
    int x1 = DisplayToImageX(ew, mx);
    int y1 = DisplayToImageY(ew, my);

    int x = x0 < x1 ? x0 : x1;
    int y = y0 < y1 ? y0 : y1;
    int w = abs(x1 - x0);
    int h = abs(y1 - y0);
    if (w < 1) {
        w = 1;
        if (x >= ew->imgW) {
            x = ew->imgW - 1;
        }
    }
    if (h < 1) {
        h = 1;
        if (y >= ew->imgH) {
            y = ew->imgH - 1;
        }
    }
    if (x + w > ew->imgW) {
        w = ew->imgW - x;
    }
    if (y + h > ew->imgH) {
        h = ew->imgH - y;
    }

    ew->cropX = x;
    ew->cropY = y;
    ew->cropW = w;
    ew->cropH = h;
}

static HCURSOR GetCursorForEdge(DragEdge edge) {
    switch (edge) {
        case DragEdge::Left:
        case DragEdge::Right:
            return GetCachedCursor(IDC_SIZEWE);
        case DragEdge::Top:
        case DragEdge::Bottom:
            return GetCachedCursor(IDC_SIZENS);
        case DragEdge::TopLeft:
        case DragEdge::BottomRight:
            return GetCachedCursor(IDC_SIZENWSE);
        case DragEdge::TopRight:
        case DragEdge::BottomLeft:
            return GetCachedCursor(IDC_SIZENESW);
        case DragEdge::Move:
            return GetCachedCursor(IDC_SIZEALL);
        case DragEdge::NewCrop:
            return GetCachedCursor(IDC_CROSS);
        default:
            return GetCachedCursor(IDC_ARROW);
    }
}

static void PaintCheckerboard(Gfx* gfx, Rect rc) {
    constexpr int kCheckerSize = 8;
    constexpr Color kCheckerDark = MkRgb(204, 204, 204);
    for (int y = 0; y < rc.dy; y += kCheckerSize) {
        for (int x = 0; x < rc.dx; x += kCheckerSize) {
            int dx = std::min(kCheckerSize, rc.dx - x);
            int dy = std::min(kCheckerSize, rc.dy - y);
            bool dark = ((x / kCheckerSize) + (y / kCheckerSize)) % 2 != 0;
            gfx->FillRect({rc.x + x, rc.y + y, dx, dy}, dark ? kCheckerDark : kColWhite);
        }
    }
}

static void PaintSaveImage(ImageEditWindow* ew, Gfx* gfx, Rect imageArea) {
    PaintCheckerboard(gfx, imageArea);

    if (!ew->srcPixmap || ew->imgDisplayW <= 0 || ew->imgDisplayH <= 0) {
        return;
    }
    gfx->DrawPixmap(ew->srcPixmap, {ew->imgDisplayX, ew->imgDisplayY, ew->imgDisplayW, ew->imgDisplayH});
}

static void PaintDragHandle(Gfx* gfx, int x, int y) {
    int hs = kDragHandleSize;
    int hh = hs / 2;
    Rect r{x - hh, y - hh, hs, hs};
    gfx->FillRect(r, kColWhite);
    gfx->DrawRect(r, kColBlack);
}

static void PaintCropImage(ImageEditWindow* ew, Gfx* gfx, Rect imageArea) {
    PaintCheckerboard(gfx, imageArea);

    if (!ew->srcPixmap || ew->imgDisplayW <= 0 || ew->imgDisplayH <= 0) {
        return;
    }

    // draw the image
    gfx->DrawPixmap(ew->srcPixmap, {ew->imgDisplayX, ew->imgDisplayY, ew->imgDisplayW, ew->imgDisplayH});

    // draw semi-transparent overlay over non-cropped areas
    int cropDispX = ImageToDisplayX(ew, ew->cropX);
    int cropDispY = ImageToDisplayY(ew, ew->cropY);
    int cropDispR = ImageToDisplayX(ew, ew->cropX + ew->cropW);
    int cropDispB = ImageToDisplayY(ew, ew->cropY + ew->cropH);

    int ix = ew->imgDisplayX;
    int iy = ew->imgDisplayY;
    int iw = ew->imgDisplayW;
    int ih = ew->imgDisplayH;
    int ir = ix + iw;
    int ib = iy + ih;

    Vec<Rect> overlay;
    if (cropDispY > iy) {
        overlay.Append({ix, iy, iw, cropDispY - iy});
    }
    if (cropDispB < ib) {
        overlay.Append({ix, cropDispB, iw, ib - cropDispB});
    }
    if (cropDispX > ix) {
        overlay.Append({ix, cropDispY, cropDispX - ix, cropDispB - cropDispY});
    }
    if (cropDispR < ir) {
        overlay.Append({cropDispR, cropDispY, ir - cropDispR, cropDispB - cropDispY});
    }
    gfx->FillRects(overlay.els, len(overlay), kColBlack, 128);

    // draw crop border
    gfx->DrawDashedRect({cropDispX, cropDispY, cropDispR - cropDispX, cropDispB - cropDispY}, kColWhite);

    // draw drag handles at corners and edge midpoints
    int midX = (cropDispX + cropDispR) / 2;
    int midY = (cropDispY + cropDispB) / 2;

    // corners
    PaintDragHandle(gfx, cropDispX, cropDispY);
    PaintDragHandle(gfx, cropDispR, cropDispY);
    PaintDragHandle(gfx, cropDispX, cropDispB);
    PaintDragHandle(gfx, cropDispR, cropDispB);
    // edge midpoints
    PaintDragHandle(gfx, midX, cropDispY);
    PaintDragHandle(gfx, midX, cropDispB);
    PaintDragHandle(gfx, cropDispX, midY);
    PaintDragHandle(gfx, cropDispR, midY);
}

static void PaintResizeImage(ImageEditWindow* ew, Gfx* gfx, Rect imageArea) {
    PaintCheckerboard(gfx, imageArea);

    if (!ew->srcPixmap || ew->imgDisplayW <= 0 || ew->imgDisplayH <= 0) {
        return;
    }

    // draw the full image
    Rect imageRect{ew->imgDisplayX, ew->imgDisplayY, ew->imgDisplayW, ew->imgDisplayH};
    gfx->DrawPixmap(ew->srcPixmap, imageRect);

    // draw semi-transparent overlay over the entire image
    gfx->FillRects(&imageRect, 1, kColBlack, 128);

    // draw the "new size" rectangle showing the resized portion, centered
    int dispNewW = ImageToDisplayW(ew, ew->newW);
    int dispNewH = ImageToDisplayH(ew, ew->newH);
    int cx = ew->imgDisplayX + (ew->imgDisplayW / 2);
    int cy = ew->imgDisplayY + (ew->imgDisplayH / 2);
    int newLeft = cx - (dispNewW / 2);
    int newTop = cy - (dispNewH / 2);

    // redraw the image portion at the new size area (clear overlay there)
    // clip source to full image, draw scaled into the new rect
    Rect newRect{newLeft, newTop, dispNewW, dispNewH};
    gfx->DrawPixmap(ew->srcPixmap, newRect);

    // draw border around the new size rectangle
    gfx->DrawDashedRect(newRect, kColWhite);

    // draw drag handles
    int midX = newLeft + (dispNewW / 2);
    int midY = newTop + (dispNewH / 2);
    int right = newLeft + dispNewW;
    int bottom = newTop + dispNewH;

    PaintDragHandle(gfx, newLeft, newTop);
    PaintDragHandle(gfx, right, newTop);
    PaintDragHandle(gfx, newLeft, bottom);
    PaintDragHandle(gfx, right, bottom);
    PaintDragHandle(gfx, midX, newTop);
    PaintDragHandle(gfx, midX, bottom);
    PaintDragHandle(gfx, newLeft, midY);
    PaintDragHandle(gfx, right, midY);
}

static void LayoutControls(ImageEditWindow* ew) {
    if (!ew->controlLayout) {
        return;
    }
    Rect cRc = HwndClientRect(ew->hwnd);
    int btnPad = ImageEditButtonPadding();
    int w = cRc.dx - (2 * btnPad);
    Constraints bc = Loose({w, Inf});
    Size layoutSize = ew->controlLayout->Layout(bc);
    Rect bounds{0, ew->imgAreaH, cRc.dx, layoutSize.dy};
    ew->controlLayout->SetBounds(bounds);
    RefreshVirtTops(ew->hwnd, ew->controlLayout, bounds, &ew->vroot);
    // the labels and buttons are painted by us, so a changed label (or a
    // button that grew) needs the control area repainted
    HwndInvalidateRect(ew->hwnd, {0, ew->imgAreaH, cRc.dx, cRc.dy - ew->imgAreaH}, false);
}

static void OnBrowse(ImageEditWindow* ew) {
    WCHAR dstFileName[MAX_PATH + 1]{};
    if (ew->destEdit) {
        WCHAR* dest = CWStrTemp(ew->destEdit->GetTextTemp());
        wstr::BufSet(WStr(dstFileName, dimof(dstFileName)), WStr(dest));
    }

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ew->hwnd;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter =
        L"All Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tiff;*.tif;*.webp\0"
        L"PNG Files\0*.png\0"
        L"JPEG Files\0*.jpg;*.jpeg\0"
        L"BMP Files\0*.bmp\0"
        L"All Files\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetSaveFileNameW(&ofn) && ew->destEdit) {
        ew->destEdit->SetText(ToUtf8Temp(dstFileName));
    }
}

// Save a GDI+ Bitmap using WIC. Supports all formats that have a WIC encoder installed,
// including WebP on Windows 10+.
static bool SaveBitmapWithWIC(Bitmap* bmp, WStr destPath, const GUID* containerFormat) {
    if (!bmp || !destPath || !containerFormat) {
        return false;
    }

    // get HBITMAP from GDI+ bitmap
    HBITMAP hbmp = nullptr;
    Gdiplus::Color bgColor(255, 255, 255, 255);
    if (bmp->GetHBITMAP(bgColor, &hbmp) != Ok || !hbmp) {
        return false;
    }

    bool ok = false;
    IWICImagingFactory* pFactory = nullptr;
    IWICBitmapEncoder* pEncoder = nullptr;
    IWICBitmapFrameEncode* pFrame = nullptr;
    IWICBitmap* pWicBitmap = nullptr;
    IStream* pStream = nullptr;
    IPropertyBag2* pProps = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory),
                                  (void**)&pFactory);
    if (FAILED(hr)) {
        goto Done;
    }

    hr = pFactory->CreateBitmapFromHBITMAP(hbmp, nullptr, WICBitmapUsePremultipliedAlpha, &pWicBitmap);
    if (FAILED(hr)) {
        goto Done;
    }

    hr = SHCreateStreamOnFileEx(destPath.s, STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, FILE_ATTRIBUTE_NORMAL,
                                TRUE, nullptr, &pStream);
    if (FAILED(hr)) {
        goto Done;
    }

    hr = pFactory->CreateEncoder(*containerFormat, nullptr, &pEncoder);
    if (FAILED(hr)) {
        goto Done;
    }

    hr = pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        goto Done;
    }

    hr = pEncoder->CreateNewFrame(&pFrame, &pProps);
    if (FAILED(hr)) {
        goto Done;
    }

    hr = pFrame->Initialize(pProps);
    if (FAILED(hr)) {
        goto Done;
    }

    {
        UINT w = 0, h = 0;
        pWicBitmap->GetSize(&w, &h);
        hr = pFrame->SetSize(w, h);
        if (FAILED(hr)) {
            goto Done;
        }

        WICPixelFormatGUID pixFmt = GUID_WICPixelFormat32bppBGRA;
        hr = pFrame->SetPixelFormat(&pixFmt);
        if (FAILED(hr)) {
            goto Done;
        }

        hr = pFrame->WriteSource(pWicBitmap, nullptr);
        if (FAILED(hr)) {
            goto Done;
        }
    }

    hr = pFrame->Commit();
    if (FAILED(hr)) {
        goto Done;
    }

    hr = pEncoder->Commit();
    ok = SUCCEEDED(hr);

Done:
    if (pProps) {
        pProps->Release();
    }
    if (pFrame) {
        pFrame->Release();
    }
    if (pEncoder) {
        pEncoder->Release();
    }
    if (pStream) {
        pStream->Release();
    }
    if (pWicBitmap) {
        pWicBitmap->Release();
    }
    if (pFactory) {
        pFactory->Release();
    }
    DeleteObject(hbmp);
    return ok;
}

static void OnSave(ImageEditWindow* ew) {
    if (!ew->srcBitmap) {
        return;
    }
    if (ew->mode == ImageEditMode::Crop && (ew->cropW <= 0 || ew->cropH <= 0)) {
        return;
    }
    if (ew->mode == ImageEditMode::Resize && (ew->newW <= 0 || ew->newH <= 0)) {
        return;
    }

    TempStr rawDest = ew->destEdit ? ew->destEdit->GetTextTemp() : TempStr{};
    if (len(rawDest) == 0) {
        return;
    }

    bool unmodified = !ew->srcWasModified;
    if (ew->mode == ImageEditMode::Crop && IsCropChanged(ew)) {
        unmodified = false;
    }
    if (ew->mode == ImageEditMode::Resize && IsResizeChanged(ew)) {
        unmodified = false;
    }

    int fmtIdx = GetSelectedFormatIdx(ew);
    Str fmtExt = gImageFormats[fmtIdx].ext;
    TempStr destExt = path::GetExtTemp(rawDest);
    bool writeOriginal = unmodified && len(ew->originalData) > 0 && ExtMatchesOriginal(destExt, ew->originalExt) &&
                         !gImageFormats[fmtIdx].isPdf;

    TempStr dest;
    if (writeOriginal || str::EqI(destExt, fmtExt)) {
        dest = str::DupTemp(rawDest);
    } else {
        dest = PathWithExtTemp(rawDest, fmtExt);
    }

    if (writeOriginal) {
        bool saved = file::WriteFile(dest, ew->originalData);
        if (!saved) {
            WarnBox(ew->hwnd, "Failed to save image", "Save Image");
            return;
        }
        HWND hwndParent = ew->hwndParent;
        Str savedPath = str::Dup(dest);
        DestroyWindow(ew->hwnd);
        if (gImageEditHost.OpenSavedFile) {
            gImageEditHost.OpenSavedFile(hwndParent, savedPath);
        }
        str::Free(savedPath);
        return;
    }

    Bitmap* result = nullptr;
    if (ew->mode == ImageEditMode::Save) {
        // save as-is
        result = ew->srcBitmap->Clone(0, 0, ew->imgW, ew->imgH, ew->srcBitmap->GetPixelFormat());
        if (!result) {
            WarnBox(ew->hwnd, "Failed to save image", "Save Image");
            return;
        }
    } else if (ew->mode == ImageEditMode::Crop) {
        // create cropped bitmap
        Gdiplus::Rect srcRect(ew->cropX, ew->cropY, ew->cropW, ew->cropH);
        result = ew->srcBitmap->Clone(srcRect, ew->srcBitmap->GetPixelFormat());
        if (!result) {
            WarnBox(ew->hwnd, "Failed to create cropped image", Tr("Crop Image"));
            return;
        }
    } else {
        // create resized bitmap
        result = new Bitmap(ew->newW, ew->newH, ew->srcBitmap->GetPixelFormat());
        if (!result) {
            WarnBox(ew->hwnd, "Failed to create resized image", Tr("Resize Image"));
            return;
        }
        Graphics g(result);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(ew->srcBitmap, 0, 0, ew->newW, ew->newH);
    }

    bool saved;
    if (gImageFormats[fmtIdx].isPdf) {
        saved = gImageEditHost.SaveBitmapAsPdf && gImageEditHost.SaveBitmapAsPdf(result, dest);
    } else {
        TempWStr destW = ToWStrTemp(dest);
        saved = SaveBitmapWithWIC(result, destW, gImageFormats[fmtIdx].containerFormat);
    }
    delete result;

    if (!saved) {
        WarnBox(ew->hwnd, "Failed to save image", "Save Image");
        return;
    }
    OptimizePngFileAsync(dest);

    // load the saved image
    HWND hwndParent = ew->hwndParent;
    Str savedPath = str::Dup(dest);
    DestroyWindow(ew->hwnd);

    if (gImageEditHost.OpenSavedFile) {
        gImageEditHost.OpenSavedFile(hwndParent, savedPath);
    }
    str::Free(savedPath);
}

static void SwitchToSaveMode(ImageEditWindow* ew) {
    ew->mode = ImageEditMode::Save;
    ew->isDragging = false;
    ew->dragEdge = DragEdge::None;
    ew->hoverEdge = DragEdge::None;
    ew->cropX = 0;
    ew->cropY = 0;
    ew->cropW = ew->imgW;
    ew->cropH = ew->imgH;
    ew->newW = ew->imgW;
    ew->newH = ew->imgH;
    HwndSetText(ew->hwnd, "Save Image");
    UpdateModeButtons(ew);
    UpdateSaveButtonText(ew);
    UpdateInfoLabel(ew);
    LayoutControls(ew);
    InvalidateImageArea(ew);
    SetFocus(ew->hwnd);
}

static bool ReplaceSrcBitmap(ImageEditWindow* ew, Bitmap* newBmp) {
    Pixmap* newPixmap = PixmapFromGdiplus(newBmp);
    if (!newPixmap) {
        delete newBmp;
        return false;
    }
    delete ew->srcBitmap;
    FreePixmap(ew->srcPixmap);
    ew->srcBitmap = newBmp;
    ew->srcPixmap = newPixmap;
    ew->imgW = (int)newBmp->GetWidth();
    ew->imgH = (int)newBmp->GetHeight();
    return true;
}

static bool IsCropChanged(ImageEditWindow* ew) {
    return ew->cropX != 0 || ew->cropY != 0 || ew->cropW != ew->imgW || ew->cropH != ew->imgH;
}

static bool IsResizeChanged(ImageEditWindow* ew) {
    return ew->newW != ew->imgW || ew->newH != ew->imgH;
}

static void UpdateModeButtons(ImageEditWindow* ew) {
    if (ew->mode == ImageEditMode::Crop) {
        ew->btnCrop->SetLabel(Tr("&Apply Crop"));
        ew->btnCrop->SetIsEnabled(IsCropChanged(ew));
        ew->btnResize->SetLabel(Tr("&Resize"));
        ew->btnResize->SetIsEnabled(true);
    } else if (ew->mode == ImageEditMode::Resize) {
        ew->btnCrop->SetLabel(Tr("&Crop"));
        ew->btnCrop->SetIsEnabled(true);
        ew->btnResize->SetLabel(Tr("&Apply Resize"));
        ew->btnResize->SetIsEnabled(IsResizeChanged(ew));
    } else {
        // Save mode
        ew->btnCrop->SetLabel(Tr("&Crop"));
        ew->btnCrop->SetIsEnabled(true);
        ew->btnResize->SetLabel(Tr("&Resize"));
        ew->btnResize->SetIsEnabled(true);
    }
}

static void ApplyCrop(ImageEditWindow* ew) {
    if (!IsCropChanged(ew)) {
        return;
    }
    if (ew->cropW <= 0 || ew->cropH <= 0) {
        return;
    }
    Gdiplus::Rect srcRect(ew->cropX, ew->cropY, ew->cropW, ew->cropH);
    Bitmap* cropped = ew->srcBitmap->Clone(srcRect, ew->srcBitmap->GetPixelFormat());
    if (cropped) {
        int prevW = ew->imgW;
        int prevH = ew->imgH;
        if (!ReplaceSrcBitmap(ew, cropped)) {
            return;
        }
        ew->srcWasModified = true;
        ew->cropX = 0;
        ew->cropY = 0;
        ew->cropW = ew->imgW;
        ew->cropH = ew->imgH;
        UpdateModeButtons(ew);
        UpdateInfoLabel(ew);
        ResizeImageEditWindowToImage(ew, prevW, prevH);
    }
}

static void ApplyResize(ImageEditWindow* ew) {
    if (!IsResizeChanged(ew)) {
        return;
    }
    if (ew->newW <= 0 || ew->newH <= 0) {
        return;
    }
    Bitmap* resized = new Bitmap(ew->newW, ew->newH, ew->srcBitmap->GetPixelFormat());
    if (resized) {
        int prevW = ew->imgW;
        int prevH = ew->imgH;
        Graphics g(resized);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(ew->srcBitmap, 0, 0, ew->newW, ew->newH);
        if (!ReplaceSrcBitmap(ew, resized)) {
            return;
        }
        ew->srcWasModified = true;
        ew->newW = ew->imgW;
        ew->newH = ew->imgH;
        UpdateModeButtons(ew);
        UpdateInfoLabel(ew);
        ResizeImageEditWindowToImage(ew, prevW, prevH);
    }
}

static void SwitchToCropMode(ImageEditWindow* ew) {
    // apply pending resize before switching
    if (ew->mode == ImageEditMode::Resize && IsResizeChanged(ew)) {
        ApplyResize(ew);
    }
    ew->mode = ImageEditMode::Crop;
    ew->cropX = 0;
    ew->cropY = 0;
    ew->cropW = ew->imgW;
    ew->cropH = ew->imgH;
    HwndSetText(ew->hwnd, "Crop Image");
    UpdateModeButtons(ew);
    UpdateSaveButtonText(ew);
    UpdateInfoLabel(ew);
    LayoutControls(ew);
    InvalidateImageArea(ew);
    SetFocus(ew->hwnd);
}

static void SwitchToResizeMode(ImageEditWindow* ew) {
    // apply pending crop before switching
    if (ew->mode == ImageEditMode::Crop && IsCropChanged(ew)) {
        ApplyCrop(ew);
    }
    ew->mode = ImageEditMode::Resize;
    ew->newW = ew->imgW;
    ew->newH = ew->imgH;
    HwndSetText(ew->hwnd, "Resize Image");
    UpdateModeButtons(ew);
    UpdateSaveButtonText(ew);
    UpdateInfoLabel(ew);
    LayoutControls(ew);
    InvalidateImageArea(ew);
    SetFocus(ew->hwnd);
}

static void OnCropButton(ImageEditWindow* ew) {
    if (ew->mode == ImageEditMode::Crop) {
        ApplyCrop(ew);
    } else {
        SwitchToCropMode(ew);
    }
}

static void OnResizeButton(ImageEditWindow* ew) {
    if (ew->mode == ImageEditMode::Resize) {
        ApplyResize(ew);
    } else {
        SwitchToResizeMode(ew);
    }
}

static Bitmap* CreateBitmapForClipboard(ImageEditWindow* ew) {
    if (!ew || !ew->srcBitmap) {
        return nullptr;
    }
    if (ew->mode == ImageEditMode::Crop && ew->cropW > 0 && ew->cropH > 0) {
        Gdiplus::Rect srcRect(ew->cropX, ew->cropY, ew->cropW, ew->cropH);
        return ew->srcBitmap->Clone(srcRect, ew->srcBitmap->GetPixelFormat());
    }
    if (ew->mode == ImageEditMode::Resize && ew->newW > 0 && ew->newH > 0 &&
        (ew->newW != ew->imgW || ew->newH != ew->imgH)) {
        Bitmap* resized = new Bitmap(ew->newW, ew->newH, ew->srcBitmap->GetPixelFormat());
        if (!resized) {
            return nullptr;
        }
        Graphics g(resized);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(ew->srcBitmap, 0, 0, ew->newW, ew->newH);
        return resized;
    }
    return ew->srcBitmap->Clone(0, 0, ew->imgW, ew->imgH, ew->srcBitmap->GetPixelFormat());
}

static bool CopyEditedImageToClipboard(ImageEditWindow* ew) {
    Bitmap* bmp = CreateBitmapForClipboard(ew);
    if (!bmp) {
        return false;
    }
    HBITMAP tmp = nullptr;
    Status status = bmp->GetHBITMAP((Gdiplus::ARGB)0xffffffff, &tmp);
    delete bmp;
    if (status != Ok || !tmp) {
        return false;
    }
    ScopedGdiObj<HBITMAP> hbmp(tmp);
    return CopyImageToClipboard(tmp, false);
}

// Tab moves between the dest edit, the format drop-down and the buttons; the
// ring is the layout order and covers HWND and virtual controls alike
void ImageEditWindow::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey != VK_TAB) {
        return;
    }
    if (ev->hwnd != hwnd && !::IsChild(hwnd, ev->hwnd)) {
        return;
    }
    TabNavigate(ev->isShift);
    ev->didHandle = true;
}

void ImageEditWindow::OnSize(WindowBase::SizeEvent* ev) {
    if (ev->msg != WM_SIZE) {
        return;
    }
    CalcImageLayout(this);
    LayoutControls(this);
    if (mode == ImageEditMode::Crop) {
        InvalidateImageArea(this);
    } else {
        HwndInvalidate(hwnd, true);
    }
}

void ImageEditWindow::OnDpiChanged(WindowBase::DpiChangedEvent* ev) {
    RECT* r = ev->suggested;
    if (r) {
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    font = ImageEditFont(hwnd);
    ImageEditApplyFont(this);
    CalcImageLayout(this);
    LayoutControls(this);
    HwndInvalidate(hwnd, true);
    ev->didHandle = true;
}

// modeless dialog: route Alt+<mnemonic> / Tab via IsDialogMessage in main loop
void ImageEditWindow::OnActivate(WindowBase::ActivateEvent* ev) {
    if (ev->state == WA_INACTIVE) {
        if (GetCurrentModelessDialog() == hwnd) {
            SetCurrentModelessDialog(nullptr);
        }
        return;
    }
    SetCurrentModelessDialog(hwnd);
    // menu commands often leave keyboard focus on the main window
    if (!HasFocusInImageEdit(this)) {
        RestoreImageEditFocus(this);
    }
}

void ImageEditWindow::OnDestroy(WindowBase::DestroyEvent*) {
    if (GetCurrentModelessDialog() == hwnd) {
        SetCurrentModelessDialog(nullptr);
    }
    HWND parent = hwndParent;
    if (destEdit && destEdit->hwnd && gDestEditSubclassId) {
        RemoveWindowSubclass(destEdit->hwnd, WndProcDestEditSubclass, gDestEditSubclassId);
    }
    gImageEditWindows.Remove(this);
    // deleting a window while handling its own message is unsafe;
    // uitask runs after this dispatch finishes
    auto fn = MkFunc0<ImageEditWindow>(DeleteImageEditWindow, this);
    uitask::Post(fn, "DeleteImageEditWindow");
    if (parent) {
        HwndToForeground(parent);
    }
}

void ImageEditWindow::OnSetCursor(WindowBase::SetCursorEvent* ev) {
    if (mode == ImageEditMode::Save || ev->hitTest != HTCLIENT) {
        return;
    }
    Point pt = HwndGetCursorPos(hwnd);
    DragEdge edge;
    if (mode == ImageEditMode::Crop) {
        edge = HitTestCropEdgeOrNewCrop(this, pt.x, pt.y);
    } else {
        edge = HitTestResizeEdge(this, pt.x, pt.y);
    }
    if (edge != DragEdge::None) {
        SetCursor(GetCursorForEdge(edge));
        ev->result = TRUE;
        ev->didHandle = true;
    }
}

void ImageEditWindow::WndProc(WindowBase::WndProcEvent* ev) {
    HWND hwnd = ev->hwnd;
    UINT msg = ev->msg;
    WPARAM wp = ev->wparam;
    LPARAM lp = ev->lparam;
    ImageEditWindow* ew = this;

    // the labels and buttons are virtual controls: hand them the mouse and,
    // when one of them has the focus, the keyboard
    if (vroot) {
        LRESULT vres = 0;
        if (VirtTreeOnMessage(hwnd, vroot, msg, wp, lp, vres)) {
            {
                ev->result = vres;
                ev->didHandle = true;
                return;
            }
        }
    }

    switch (msg) {
        case WM_PAINT: {
            if (!ew) {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            Rect cRc = HwndClientRect(hwnd);
            Rect imageArea{0, 0, cRc.dx, std::min(ew->imgAreaH, cRc.dy)};
            Gfx* gfx = GfxCreateWithDoubleBuffer(ew, hdc);
            if (ew->mode == ImageEditMode::Save) {
                PaintSaveImage(ew, gfx, imageArea);
            } else if (ew->mode == ImageEditMode::Crop) {
                PaintCropImage(ew, gfx, imageArea);
            } else {
                PaintResizeImage(ew, gfx, imageArea);
            }
            Rect controlArea{0, imageArea.dy, cRc.dx, cRc.dy - imageArea.dy};
            gfx->FillRect(controlArea, GetSysColor(COLOR_BTNFACE));
            if (ew->vroot) {
                ew->vroot->Paint(gfx, controlArea);
            }
            delete gfx;
            EndPaint(hwnd, &ps);
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }

        case WM_ERASEBKGND: {
            if (!ew) {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
            {
                ev->result = 1;
                ev->didHandle = true;
                return;
            }
        }

        case WM_MOUSEMOVE: {
            if (!ew || ew->mode == ImageEditMode::Save) {
                break;
            }
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            if (ew->isDragging) {
                if (ew->mode == ImageEditMode::Crop) {
                    auto edge = ew->dragEdge;
                    if (edge == DragEdge::NewCrop) {
                        if (!ew->dragMoved && !IsImageEditDragDistance(ew->dragStart, mx, my)) {
                            {
                                ev->result = 0;
                                ev->didHandle = true;
                                return;
                            }
                        }
                        ew->dragMoved = true;
                        SetCropFromDisplaySelection(ew, ew->dragStart, mx, my);
                    } else {
                        ew->dragMoved = true;
                        int imgDx = DisplayToImageX(ew, mx) - DisplayToImageX(ew, ew->dragStart.x);
                        int imgDy = DisplayToImageY(ew, my) - DisplayToImageY(ew, ew->dragStart.y);

                        int nx = ew->dragCropX;
                        int ny = ew->dragCropY;
                        int nw = ew->dragCropW;
                        int nh = ew->dragCropH;

                        if (edge == DragEdge::Left || edge == DragEdge::TopLeft || edge == DragEdge::BottomLeft) {
                            nx = ew->dragCropX + imgDx;
                            nw = ew->dragCropW - imgDx;
                        }
                        if (edge == DragEdge::Right || edge == DragEdge::TopRight || edge == DragEdge::BottomRight) {
                            nw = ew->dragCropW + imgDx;
                        }
                        if (edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight) {
                            ny = ew->dragCropY + imgDy;
                            nh = ew->dragCropH - imgDy;
                        }
                        if (edge == DragEdge::Bottom || edge == DragEdge::BottomLeft || edge == DragEdge::BottomRight) {
                            nh = ew->dragCropH + imgDy;
                        }
                        if (edge == DragEdge::Move) {
                            nx = ew->dragCropX + imgDx;
                            ny = ew->dragCropY + imgDy;
                            // clamp to image bounds
                            nx = std::max(nx, 0);
                            ny = std::max(ny, 0);
                            if (nx + nw > ew->imgW) {
                                nx = ew->imgW - nw;
                            }
                            if (ny + nh > ew->imgH) {
                                ny = ew->imgH - nh;
                            }
                        }

                        // enforce minimum size and bounds
                        if (nw < 1) {
                            nw = 1;
                            nx = ew->cropX;
                        }
                        if (nh < 1) {
                            nh = 1;
                            ny = ew->cropY;
                        }
                        if (nx < 0) {
                            nw += nx;
                            nx = 0;
                        }
                        if (ny < 0) {
                            nh += ny;
                            ny = 0;
                        }
                        if (nx + nw > ew->imgW) {
                            nw = ew->imgW - nx;
                        }
                        if (ny + nh > ew->imgH) {
                            nh = ew->imgH - ny;
                        }

                        ew->cropX = nx;
                        ew->cropY = ny;
                        ew->cropW = nw;
                        ew->cropH = nh;
                    }
                    UpdateInfoLabel(ew);
                    InvalidateImageArea(ew);
                } else {
                    // resize mode
                    int dx = mx - ew->dragStart.x;
                    int dy = my - ew->dragStart.y;
                    // convert pixel deltas to image-space deltas
                    int imgDx = DisplayToImageW(ew, dx);
                    int imgDy = DisplayToImageH(ew, dy);

                    auto edge = ew->dragEdge;
                    int nw = ew->dragNewW;
                    int nh = ew->dragNewH;

                    // left/right edges change width
                    if (edge == DragEdge::Left || edge == DragEdge::TopLeft || edge == DragEdge::BottomLeft) {
                        nw = ew->dragNewW - (imgDx * 2); // symmetric resize
                    }
                    if (edge == DragEdge::Right || edge == DragEdge::TopRight || edge == DragEdge::BottomRight) {
                        nw = ew->dragNewW + (imgDx * 2);
                    }
                    // top/bottom edges change height
                    if (edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight) {
                        nh = ew->dragNewH - (imgDy * 2);
                    }
                    if (edge == DragEdge::Bottom || edge == DragEdge::BottomLeft || edge == DragEdge::BottomRight) {
                        nh = ew->dragNewH + (imgDy * 2);
                    }

                    nw = std::max(nw, 1);
                    nh = std::max(nh, 1);

                    ew->newW = nw;
                    ew->newH = nh;
                    GrowWindowIfNeeded(ew, edge);
                    UpdateInfoLabel(ew);
                    InvalidateImageArea(ew);
                }
            } else {
                DragEdge edge;
                if (ew->mode == ImageEditMode::Crop) {
                    edge = HitTestCropEdgeOrNewCrop(ew, mx, my);
                } else {
                    edge = HitTestResizeEdge(ew, mx, my);
                }
                ew->hoverEdge = edge;
                SetCursor(GetCursorForEdge(edge));
            }
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }

        case WM_LBUTTONDOWN: {
            if (!ew) {
                break;
            }
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            DragEdge edge = DragEdge::None;
            if (ew->mode == ImageEditMode::Crop) {
                edge = HitTestCropEdgeOrNewCrop(ew, mx, my);
            } else if (ew->mode == ImageEditMode::Save) {
                if (IsPointInDisplayedImage(ew, mx, my)) {
                    SwitchToCropMode(ew);
                    edge = DragEdge::NewCrop;
                }
            } else {
                edge = HitTestResizeEdge(ew, mx, my);
                if (edge == DragEdge::None && IsPointInDisplayedImage(ew, mx, my)) {
                    SwitchToCropMode(ew);
                    edge = DragEdge::NewCrop;
                }
            }
            if (edge != DragEdge::None) {
                ew->isDragging = true;
                ew->dragEdge = edge;
                ew->dragStart = {mx, my};
                ew->dragMoved = false;
                if (ew->mode == ImageEditMode::Crop) {
                    ew->dragCropX = ew->cropX;
                    ew->dragCropY = ew->cropY;
                    ew->dragCropW = ew->cropW;
                    ew->dragCropH = ew->cropH;
                } else {
                    ew->dragNewW = ew->newW;
                    ew->dragNewH = ew->newH;
                }
                SetCapture(hwnd);
            }
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }

        case WM_LBUTTONUP: {
            if (ew && ew->isDragging) {
                if (ew->mode == ImageEditMode::Crop && ew->dragEdge == DragEdge::NewCrop && !ew->dragMoved) {
                    ew->cropX = ew->dragCropX;
                    ew->cropY = ew->dragCropY;
                    ew->cropW = ew->dragCropW;
                    ew->cropH = ew->dragCropH;
                    UpdateInfoLabel(ew);
                    InvalidateImageArea(ew);
                }
                ew->isDragging = false;
                ReleaseCapture();
                UpdateModeButtons(ew);
            }
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }

        case WM_MOUSEACTIVATE:
            if (ew) {
                SetFocus(hwnd);
            }
            {
                ev->result = MA_ACTIVATE;
                ev->didHandle = true;
                return;
            }

        case WM_SYSKEYDOWN: {
            if (ew && GetFocus() == hwnd && wp != VK_MENU && TriggerImageEditMnemonic(ew, (WCHAR)wp)) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            break;
        }

        case WM_SYSCHAR: {
            if (ew && TriggerImageEditMnemonic(ew, (WCHAR)wp)) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            break;
        }

        case WM_CHAR:
            if (VK_ESCAPE == wp) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            break;

        case WM_KEYDOWN: {
            if (!ew) {
                break;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wp == 'C') {
                CopyEditedImageToClipboard(ew);
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            if (wp == VK_ESCAPE) {
                if (ew->mode != ImageEditMode::Save) {
                    SwitchToSaveMode(ew);
                } else if (gImageEditHost.escToExit) {
                    DestroyWindow(hwnd);
                }
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            if (HandleImageEditArrowKey(ew, wp)) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            break;
        }

        default:
            return; // fall through to WndProcDefault
    }
    {
        ev->result = 0;
        ev->didHandle = true;
        return;
    }
}

// this window is drawn in the system colors, not the app's theme (its own
// background is COLOR_BTNFACE), so the buttons override the themed defaults
static ImageEditButton* NewImageEditButton(ImageEditWindow* ew, Str text, const VirtMouseHandler& onClick) {
    auto* b = new ImageEditButton(Str{}, ew->font);
    Color bg = GetSysColor(COLOR_BTNFACE);
    b->SetColor(kColBtnText, GetSysColor(COLOR_BTNTEXT));
    b->SetColor(kColBtnTextDisabled, GetSysColor(COLOR_GRAYTEXT));
    b->SetColor(kColBtnBg, AccentColor(bg, 14));
    b->SetColor(kColBtnBgHover, AccentColor(bg, 28));
    b->SetColor(kColBtnBorder, AccentColor(bg, 40));
    b->textPadding = DpiScaledInsets(4, 10);
    b->SetLabel(text);
    b->onClick = onClick;
    return b;
}

void ShowImageEditWindow(HWND parent, ImageEditMode mode, Str filePath, RenderedBitmap* rbmp, bool selectPdf,
                         Str originalData) {
    ProbeImageFormats();

    Bitmap* bmp = nullptr;
    bool fromRenderedBitmap = (rbmp != nullptr);

    if (fromRenderedBitmap) {
        // create GDI+ bitmap from RenderedBitmap
        HBITMAP hbmp = rbmp->GetBitmap();
        if (!hbmp) {
            return;
        }
        bmp = new Bitmap(hbmp, nullptr);
        if (!bmp || bmp->GetWidth() == 0) {
            delete bmp;
            return;
        }
    } else {
        // the caller names the image; there is nothing to edit without one
        if (!filePath) {
            return;
        }
        Str data = file::ReadFile(filePath);
        if (len(data) == 0) {
            return;
        }
        str::Free(data);
        if (!gImageEditHost.LoadImageFile) {
            return;
        }
        bmp = gImageEditHost.LoadImageFile(filePath);
        if (!bmp) {
            return;
        }
    }

    int imgW = (int)bmp->GetWidth();
    int imgH = (int)bmp->GetHeight();
    if (imgW <= 0 || imgH <= 0) {
        delete bmp;
        return;
    }

    Str origOwned;
    if (len(originalData) > 0) {
        origOwned = str::Dup(originalData);
    } else if (!fromRenderedBitmap && filePath && file::Exists(filePath)) {
        origOwned = file::ReadFile(filePath);
    }
    Str origExt = ImageSaveExtFromData(origOwned);

    auto* ew = new ImageEditWindow();
    ew->mode = mode;
    ew->fromRenderedBitmap = fromRenderedBitmap;
    ew->filePath = filePath ? str::Dup(filePath) : Str();
    ew->originalData = origOwned;
    ew->originalExt = origExt;
    ew->srcBitmap = bmp;
    ew->srcPixmap = PixmapFromGdiplus(bmp);
    if (!ew->srcPixmap) {
        delete ew;
        return;
    }
    ew->imgW = imgW;
    ew->imgH = imgH;

    if (mode == ImageEditMode::Crop) {
        // initial crop = full image
        ew->cropX = 0;
        ew->cropY = 0;
        ew->cropW = imgW;
        ew->cropH = imgH;
    } else {
        ew->newW = imgW;
        ew->newH = imgH;
    }

    gImageEditWindows.Append(ew);

    HMODULE h = GetModuleHandleW(nullptr);
    Size winSize = CalcImageEditWindowSizeEx(parent, parent, fromRenderedBitmap, imgW, imgH, nullptr);

    Str title = "Save Image";
    if (mode == ImageEditMode::Crop) {
        title = "Crop Image";
    } else if (mode == ImageEditMode::Resize) {
        title = "Resize Image";
    }
    {
        CreateCustomArgs cargs;
        cargs.className = kImageEditWinClassName;
        cargs.title = title;
        cargs.style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
        cargs.exStyle = WS_EX_CONTROLPARENT;
        cargs.pos = {CW_USEDEFAULT, CW_USEDEFAULT, winSize.dx, winSize.dy};
        cargs.visible = false;
        cargs.bgColor = GetSysColor(COLOR_BTNFACE);
        if (gImageEditHost.appIconId) {
            cargs.icon = LoadIconW(h, MAKEINTRESOURCEW(gImageEditHost.appIconId));
        }
        // LayoutControls positions the image plus the control strip
        ew->autoLayout = false;
        ew->onWndProc = MkMethod1<ImageEditWindow, WindowBase::WndProcEvent*, &ImageEditWindow::WndProc>(ew);
        ew->onKeyDown = MkMethod1<ImageEditWindow, KeyEvent*, &ImageEditWindow::OnKeyDown>(ew);
        ew->onSize = MkMethod1<ImageEditWindow, WindowBase::SizeEvent*, &ImageEditWindow::OnSize>(ew);
        ew->onDpiChanged = MkMethod1<ImageEditWindow, WindowBase::DpiChangedEvent*, &ImageEditWindow::OnDpiChanged>(ew);
        ew->onActivate = MkMethod1<ImageEditWindow, WindowBase::ActivateEvent*, &ImageEditWindow::OnActivate>(ew);
        ew->onDestroy = MkMethod1<ImageEditWindow, WindowBase::DestroyEvent*, &ImageEditWindow::OnDestroy>(ew);
        ew->onSetCursor = MkMethod1<ImageEditWindow, WindowBase::SetCursorEvent*, &ImageEditWindow::OnSetCursor>(ew);
        ew->CreateCustom(cargs);
    }
    HWND hwnd = ew->hwnd;
    if (!hwnd) {
        gImageEditWindows.Remove(ew);
        delete ew;
        return;
    }

    ew->hwndParent = parent;

    ew->font = ImageEditFont(hwnd);

    // create child controls
    TempStr destPath = StrL("");
    if (filePath) {
        destPath = filePath;
        if (origExt && !ExtMatchesOriginal(path::GetExtTemp(destPath), origExt)) {
            destPath = PathWithExtTemp(destPath, origExt);
        }
        destPath = MakeUniqueFilePathTemp(destPath);
    }
    {
        auto* edit = new Edit();
        Edit::CreateArgs editArgs;
        editArgs.parent = hwnd;
        editArgs.font = ew->font;
        editArgs.text = destPath;
        editArgs.withBorder = true;
        edit->Create(editArgs);
        edit->onTextChanged = MkFunc0<ImageEditWindow>(UpdateSaveButtonText, ew);
        if (!gDestEditSubclassId) {
            gDestEditSubclassId = 1;
        }
        SetWindowSubclass(edit->hwnd, WndProcDestEditSubclass, gDestEditSubclassId, (DWORD_PTR)ew);
        ew->destEdit = edit;
    }
    ew->btnBrowse = NewImageEditButton(ew, StrL("..."), MkFunc0(OnBrowse, ew));
    if (!fromRenderedBitmap) {
        ew->staticPathLabel = NewVirtText({
            .s = filePath ? filePath : Str{},
            .font = ew->font,
            .textColor = GetSysColor(COLOR_BTNTEXT),
            .pathEllipsis = true,
        });
    }

    // row 3: info label (buttons and format dropdown added below, then wired into controlLayout)
    TempStr infoStr;
    if (mode == ImageEditMode::Save) {
        infoStr = fmt("%d x %d", imgW, imgH);
    } else if (mode == ImageEditMode::Crop) {
        infoStr = FormatCropInfoTemp(imgW, imgH, imgW, imgH, 0, 0);
    } else {
        infoStr = FormatResizeInfoTemp(imgW, imgH, imgW, imgH);
    }
    ew->staticInfoLabel = NewVirtText({
        .s = infoStr,
        .font = ew->font,
        .textColor = GetSysColor(COLOR_BTNTEXT),
        .ellipsis = true,
    });

    // buttons
    ew->btnSave = NewImageEditButton(ew, Tr("&Save"), MkFunc0(OnSave, ew));
    ew->btnCrop = NewImageEditButton(ew, Tr("&Crop"), MkFunc0(OnCropButton, ew));
    ew->btnResize = NewImageEditButton(ew, Tr("&Resize"), MkFunc0(OnResizeButton, ew));

    // format dropdown
    {
        auto* dd = new DropDown();
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = ew->font;
        dd->Create(args);
        StrVec items;
        int defaultDdIdx = 0;
        int wantFmtIdx = kDefaultFormatIdx;
        if (selectPdf) {
            wantFmtIdx = kPdfFormatIdx;
        } else if (origExt) {
            wantFmtIdx = FormatIdxFromExt(origExt);
        }
        for (int i = 0; i < dimofi(gImageFormats); i++) {
            if (!gImageFormats[i].available) {
                continue;
            }
            if (i == wantFmtIdx) {
                defaultDdIdx = len(ew->formatIndices);
            }
            ew->formatIndices.Append(i);
            items.Append(gImageFormats[i].label);
        }
        dd->SetItems(items);
        dd->SetCurrentSelection(defaultDdIdx);
        dd->onSelectionChanged = MkFunc0<ImageEditWindow>(OnFormatChanged, ew);
        ew->dropFormat = dd;
        if (selectPdf) {
            // sync the dest edit's extension to the pre-selected PDF format
            OnFormatChanged(ew);
        }
    }

    {
        auto* vbox = new VBox();
        vbox->alignMain = MainAxisAlign::MainStart;
        vbox->alignCross = CrossAxisAlign::Stretch;

        if (ew->staticPathLabel && ew->destEdit) {
            int labelShift = ew->destEdit->GetLeftTextMargin();
            auto* pathPad = new Padding(ew->staticPathLabel, {0, 0, ImageEditRowPadding(), labelShift});
            vbox->AddChild(pathPad);
        }

        {
            auto* row2 = new HBox();
            row2->alignMain = MainAxisAlign::MainStart;
            row2->alignCross = CrossAxisAlign::CrossCenter;
            row2->gap = ew->font->averageCharWidth;
            row2->AddChild(ew->destEdit, 1);
            row2->AddChild(ew->btnBrowse);
            row2->AddChild(ew->btnSave);
            auto* row2Pad = new Padding(row2, {0, 0, ImageEditRowPadding(), 0});
            vbox->AddChild(row2Pad);
        }

        {
            auto* row3 = new HBox();
            row3->alignMain = MainAxisAlign::MainStart;
            row3->alignCross = CrossAxisAlign::CrossCenter;
            row3->gap = ew->font->averageCharWidth;
            row3->AddChild(ew->staticInfoLabel, 1);
            if (ew->dropFormat) {
                row3->AddChild(ew->dropFormat);
            }
            if (ew->btnCrop) {
                row3->AddChild(ew->btnCrop);
            }
            if (ew->btnResize) {
                row3->AddChild(ew->btnResize);
            }
            vbox->AddChild(row3);
        }

        ew->controlLayout = new Padding(vbox, DpiScaledInsets(kRowPadding, kButtonPadding));
        // WindowBase owns and tab-navigates `layout`; it is the same tree
        ew->layout = ew->controlLayout;
    }

    CalcImageLayout(ew);
    UpdateModeButtons(ew);
    LayoutControls(ew);
    UpdateSaveButtonText(ew);

    HwndCenterDialog(hwnd, parent);
    HwndEnsureOnScreen(hwnd);
    if (gImageEditHost.ApplyDarkMode) {
        gImageEditHost.ApplyDarkMode(hwnd);
    }
    HideKeyboardCues(hwnd);
    ew->SetIsVisible(true);
    SetCurrentModelessDialog(hwnd);
    HwndToForeground(hwnd);
    RestoreImageEditFocus(ew);
}

// Headless test for issue #5734: arrow keys must resize even when focus is on the dest path edit.
TempStr ImageResizeArrowKeyResultTemp(Str imagePath, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
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
    HWND parent = gImageEditHost.GetOwnerHwnd ? gImageEditHost.GetOwnerHwnd() : nullptr;
    if (!parent) {
        return fail(StrL("NOTREADY no-window"));
    }

    int beforeCount = len(gImageEditWindows);
    ShowImageEditWindow(parent, ImageEditMode::Resize, imagePath);
    if (len(gImageEditWindows) != beforeCount + 1) {
        return fail(StrL("ERROR dialog-not-opened"));
    }
    ImageEditWindow* ew = gImageEditWindows[len(gImageEditWindows) - 1];
    int wBefore = ew->newW;
    SetFocus(ew->destEdit->hwnd);
    SendMessageW(ew->destEdit->hwnd, WM_KEYDOWN, VK_RIGHT, 0);
    int wAfter = ew->newW;
    if (wAfter != wBefore + 1) {
        out.Append(fmt("FAIL before=%d after=%d\n", wBefore, wAfter));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        DestroyWindow(ew->hwnd);
        return ToStrTemp(out);
    }
    out.Append(fmt("OK newW=%d\n", wAfter));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    DestroyWindow(ew->hwnd);
    return ToStrTemp(out);
}
