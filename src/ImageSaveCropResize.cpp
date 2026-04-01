/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/GuessFileType.h"
#include "FzImgReader.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "SumatraConfig.h"
#include "Theme.h"
#include "DarkModeSubclass.h"
#include "Translations.h"
#include "ImageSaveCropResize.h"

#include "utils/Log.h"

#include <wincodec.h>

using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::Graphics;
using Gdiplus::Ok;
using Gdiplus::Status;

constexpr const WCHAR* kImageEditWinClassName = L"SUMATRA_PDF_IMAGE_EDIT";

constexpr int kMinWindowWidth = 640;
constexpr int kImagePadding = 16;
constexpr int kResizeEdgeThreshold = 2;
constexpr int kDragHandleSize = 6;
constexpr int kControlAreaDy = 100;
constexpr int kPathLabelRowDy = 16 + 6; // label height + kRowPadding
constexpr int kRowPadding = 6;
constexpr int kButtonPadding = 8;

struct ImageFormat {
    const char* label;
    const GUID* containerFormat; // WIC container format GUID
    const char* ext;
    bool needsProbe; // if true, check if encoder is available before offering
    bool available;  // set after probing
};

// clang-format off
static ImageFormat gImageFormats[] = {
    {"PNG",  &GUID_ContainerFormatPng,  ".png",  false, true},
    {"JPEG", &GUID_ContainerFormatJpeg, ".jpg",  false, true},
    {"BMP",  &GUID_ContainerFormatBmp,  ".bmp",  false, true},
    {"GIF",  &GUID_ContainerFormatGif,  ".gif",  false, true},
    {"TIFF", &GUID_ContainerFormatTiff, ".tif",  false, true},
    {"WebP", &GUID_ContainerFormatWebp, ".webp", true,  false},
};
// clang-format on

constexpr int kDefaultFormatIdx = 0; // PNG

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
    Move // only used in crop mode
};

struct ImageEditWindow {
    ImageEditMode mode = ImageEditMode::Crop;
    bool fromRenderedBitmap = false;

    HWND hwnd = nullptr;
    HWND hwndParent = nullptr;

    // child controls
    HWND hwndPathLabel = nullptr;
    HWND hwndDestEdit = nullptr;
    HWND hwndBrowseBtn = nullptr;
    HWND hwndInfoLabel = nullptr;
    Button* btnCancel = nullptr;
    Button* btnSave = nullptr;
    Button* btnCrop = nullptr;   // "Crop" or "Apply Crop"
    Button* btnResize = nullptr; // "Resize" or "Apply Resize"
    DropDown* dropFormat = nullptr;
    Vec<int> formatIndices; // maps dropdown index to gImageFormats index

    // source image
    char* filePath = nullptr;
    Bitmap* srcBitmap = nullptr;
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
    // resize mode drag state
    int dragNewW = 0;
    int dragNewH = 0;

    HFONT hFont = nullptr;

    ImageEditWindow() = default;
    ~ImageEditWindow() {
        delete srcBitmap;
        free(filePath);
        delete btnCancel;
        delete btnSave;
        delete btnCrop;
        delete btnResize;
        delete dropFormat;
    }
};

static Vec<ImageEditWindow*> gImageEditWindows;

static ImageEditWindow* FindImageEditWindowByHwnd(HWND hwnd) {
    for (auto* ew : gImageEditWindows) {
        if (ew->hwnd == hwnd) {
            return ew;
        }
    }
    return nullptr;
}

// Convert display coordinates to image coordinates (crop mode)
static int DisplayToImageX(ImageEditWindow* ew, int dx) {
    if (ew->imgDisplayW <= 0) {
        return 0;
    }
    int v = (int)((float)(dx - ew->imgDisplayX) * ew->imgW / ew->imgDisplayW);
    return std::clamp(v, 0, ew->imgW);
}

static int DisplayToImageY(ImageEditWindow* ew, int dy) {
    if (ew->imgDisplayH <= 0) {
        return 0;
    }
    int v = (int)((float)(dy - ew->imgDisplayY) * ew->imgH / ew->imgDisplayH);
    return std::clamp(v, 0, ew->imgH);
}

// Convert image coordinates to display coordinates (crop mode)
static int ImageToDisplayX(ImageEditWindow* ew, int ix) {
    if (ew->imgW <= 0) {
        return ew->imgDisplayX;
    }
    return ew->imgDisplayX + (int)((float)ix * ew->imgDisplayW / ew->imgW);
}

static int ImageToDisplayY(ImageEditWindow* ew, int iy) {
    if (ew->imgH <= 0) {
        return ew->imgDisplayY;
    }
    return ew->imgDisplayY + (int)((float)iy * ew->imgDisplayH / ew->imgH);
}

// Convert display-scale sizes to image-scale sizes (resize mode)
static int DisplayToImageW(ImageEditWindow* ew, int dispW) {
    if (ew->imgDisplayW <= 0) {
        return 0;
    }
    return (int)((float)dispW * ew->imgW / ew->imgDisplayW);
}

static int DisplayToImageH(ImageEditWindow* ew, int dispH) {
    if (ew->imgDisplayH <= 0) {
        return 0;
    }
    return (int)((float)dispH * ew->imgH / ew->imgDisplayH);
}

// Convert image size to display size (resize mode)
static int ImageToDisplayW(ImageEditWindow* ew, int iw) {
    if (ew->imgW <= 0) {
        return 0;
    }
    return (int)((float)iw * ew->imgDisplayW / ew->imgW);
}

static int ImageToDisplayH(ImageEditWindow* ew, int ih) {
    if (ew->imgH <= 0) {
        return 0;
    }
    return (int)((float)ih * ew->imgDisplayH / ew->imgH);
}

static void LayoutControls(ImageEditWindow* ew);

static int GetSelectedFormatIdx(ImageEditWindow* ew) {
    if (!ew->dropFormat) {
        return kDefaultFormatIdx;
    }
    int ddIdx = ew->dropFormat->GetCurrentSelection();
    if (ddIdx < 0 || ddIdx >= ew->formatIndices.Size()) {
        return kDefaultFormatIdx;
    }
    return ew->formatIndices.at(ddIdx);
}

static void OnFormatChanged(ImageEditWindow* ew) {
    int idx = GetSelectedFormatIdx(ew);
    const char* newExt = gImageFormats[idx].ext;
    // update the extension in the dest path
    WCHAR destW[MAX_PATH + 1]{};
    GetWindowTextW(ew->hwndDestEdit, destW, MAX_PATH);
    TempStr dest = ToUtf8Temp(destW);
    if (!str::IsEmpty(dest)) {
        TempStr oldExt = path::GetExtTemp(dest);
        int baseLen = str::Leni(dest) - str::Leni(oldExt);
        TempStr base = str::DupTemp(dest, baseLen);
        TempStr newDest = str::FormatTemp("%s%s", base, newExt);
        SetWindowTextW(ew->hwndDestEdit, ToWStrTemp(newDest));
    }
    SetFocus(ew->hwnd);
}

static void UpdateSaveButtonText(ImageEditWindow* ew) {
    WCHAR destW[MAX_PATH + 1]{};
    GetWindowTextW(ew->hwndDestEdit, destW, MAX_PATH);
    TempStr dest = ToUtf8Temp(destW);
    const char* text = file::Exists(dest) ? _TRN("Overwrite") : _TRN("Save");
    ew->btnSave->SetText(text);
    // re-layout since button width may have changed
    LayoutControls(ew);
}

static TempStr FormatCropInfoTemp(int srcW, int srcH, int cropW, int cropH, int cropX, int cropY) {
    return str::FormatTemp("%d x %d => %d x %d @ %d , %d", srcW, srcH, cropW, cropH, cropX, cropY);
}

static TempStr FormatResizeInfoTemp(int srcW, int srcH, int newW, int newH) {
    float pctW = (srcW > 0) ? (float)newW * 100.0f / srcW : 0.0f;
    float pctH = (srcH > 0) ? (float)newH * 100.0f / srcH : 0.0f;
    return str::FormatTemp("%d x %d => %d x %d (%.2f%% x %.2f%%)", srcW, srcH, newW, newH, pctW, pctH);
}

static void UpdateInfoLabel(ImageEditWindow* ew) {
    TempStr s;
    if (ew->mode == ImageEditMode::Save) {
        s = str::FormatTemp("%d x %d", ew->imgW, ew->imgH);
    } else if (ew->mode == ImageEditMode::Crop) {
        s = FormatCropInfoTemp(ew->imgW, ew->imgH, ew->cropW, ew->cropH, ew->cropX, ew->cropY);
    } else {
        s = FormatResizeInfoTemp(ew->imgW, ew->imgH, ew->newW, ew->newH);
    }
    SetWindowTextA(ew->hwndInfoLabel, s);
}

// invalidate only the image area, not the control area below
static void InvalidateImageArea(ImageEditWindow* ew) {
    RECT rc = {0, 0, 0, 0};
    GetClientRect(ew->hwnd, &rc);
    rc.bottom = ew->imgAreaH;
    InvalidateRect(ew->hwnd, &rc, FALSE);
}

static int GetControlAreaDy(ImageEditWindow* ew) {
    return ew->fromRenderedBitmap ? (kControlAreaDy - kPathLabelRowDy) : kControlAreaDy;
}

static void CalcImageLayout(ImageEditWindow* ew) {
    Rect cRc = ClientRect(ew->hwnd);
    ew->imgAreaH = cRc.dy - GetControlAreaDy(ew);
    if (ew->imgAreaH < 10) {
        ew->imgAreaH = 10;
    }

    // fit image within image area with padding
    int availW = cRc.dx - 2 * kImagePadding;
    int availH = ew->imgAreaH - 2 * kImagePadding;
    if (availW <= 0 || availH <= 0 || ew->imgW <= 0 || ew->imgH <= 0) {
        ew->imgDisplayX = kImagePadding;
        ew->imgDisplayY = kImagePadding;
        ew->imgDisplayW = 0;
        ew->imgDisplayH = 0;
        return;
    }

    float scaleX = (float)availW / ew->imgW;
    float scaleY = (float)availH / ew->imgH;
    float scale = std::min(scaleX, scaleY);
    // don't upscale beyond 100%
    if (scale > 1.0f) {
        scale = 1.0f;
    }

    ew->imgDisplayW = (int)(ew->imgW * scale);
    ew->imgDisplayH = (int)(ew->imgH * scale);
    // center in available area
    ew->imgDisplayX = kImagePadding + (availW - ew->imgDisplayW) / 2;
    ew->imgDisplayY = kImagePadding + (availH - ew->imgDisplayH) / 2;
}

// Grow the window if the new-size rectangle exceeds the image display area (resize mode only).
// Tries to grow in the direction of the drag, moving the window if needed,
// but stops at screen edges.
static void GrowWindowIfNeeded(ImageEditWindow* ew, DragEdge edge) {
    // calculate how much display space the new size needs
    int neededDispW = ImageToDisplayW(ew, ew->newW) + 2 * kImagePadding;
    int neededDispH = ImageToDisplayH(ew, ew->newH) + 2 * kImagePadding;

    Rect cRc = ClientRect(ew->hwnd);
    int availW = cRc.dx;
    int availH = ew->imgAreaH;

    int extraW = neededDispW - availW;
    int extraH = neededDispH - availH;
    if (extraW <= 0 && extraH <= 0) {
        return;
    }
    if (extraW < 0) {
        extraW = 0;
    }
    if (extraH < 0) {
        extraH = 0;
    }

    // get screen work area
    HMONITOR hMon = MonitorFromWindow(ew->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hMon, &mi);
    int screenL = mi.rcWork.left;
    int screenT = mi.rcWork.top;
    int screenR = mi.rcWork.right;
    int screenB = mi.rcWork.bottom;

    RECT winRc;
    GetWindowRect(ew->hwnd, &winRc);
    int winX = winRc.left;
    int winY = winRc.top;
    int winW = winRc.right - winRc.left;
    int winH = winRc.bottom - winRc.top;

    int newWinW = winW + extraW;
    int newWinH = winH + extraH;

    // don't exceed screen size
    int maxW = screenR - screenL;
    int maxH = screenB - screenT;
    if (newWinW > maxW) {
        newWinW = maxW;
    }
    if (newWinH > maxH) {
        newWinH = maxH;
    }

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
        if (newX < screenL) {
            newX = screenL;
        }
    } else if (deltaW > 0) {
        // grow right - check we don't go past screen edge
        if (newX + newWinW > screenR) {
            newX = screenR - newWinW;
            if (newX < screenL) {
                newX = screenL;
            }
        }
    }

    if (growUp && deltaH > 0) {
        newY = winY - deltaH;
        if (newY < screenT) {
            newY = screenT;
        }
    } else if (deltaH > 0) {
        // grow down
        if (newY + newWinH > screenB) {
            newY = screenB - newWinH;
            if (newY < screenT) {
                newY = screenT;
            }
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
    int cx = ew->imgDisplayX + ew->imgDisplayW / 2;
    int cy = ew->imgDisplayY + ew->imgDisplayH / 2;
    int left = cx - dispNewW / 2;
    int right = left + dispNewW;
    int top = cy - dispNewH / 2;
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

static HCURSOR GetCursorForEdge(DragEdge edge) {
    switch (edge) {
        case DragEdge::Left:
        case DragEdge::Right:
            return LoadCursor(nullptr, IDC_SIZEWE);
        case DragEdge::Top:
        case DragEdge::Bottom:
            return LoadCursor(nullptr, IDC_SIZENS);
        case DragEdge::TopLeft:
        case DragEdge::BottomRight:
            return LoadCursor(nullptr, IDC_SIZENWSE);
        case DragEdge::TopRight:
        case DragEdge::BottomLeft:
            return LoadCursor(nullptr, IDC_SIZENESW);
        case DragEdge::Move:
            return LoadCursor(nullptr, IDC_SIZEALL);
        default:
            return LoadCursor(nullptr, IDC_ARROW);
    }
}

static void PaintSaveImage(ImageEditWindow* ew, HDC hdc) {
    Rect cRc = ClientRect(ew->hwnd);

    HBRUSH bgBrush = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
    RECT rcImg = {0, 0, cRc.dx, ew->imgAreaH};
    FillRect(hdc, &rcImg, bgBrush);

    if (!ew->srcBitmap || ew->imgDisplayW <= 0 || ew->imgDisplayH <= 0) {
        return;
    }

    Graphics g(hdc);
    g.DrawImage(ew->srcBitmap, ew->imgDisplayX, ew->imgDisplayY, ew->imgDisplayW, ew->imgDisplayH);
}

static void PaintCropImage(ImageEditWindow* ew, HDC hdc) {
    Rect cRc = ClientRect(ew->hwnd);

    // fill image area background
    HBRUSH bgBrush = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
    RECT rcImg = {0, 0, cRc.dx, ew->imgAreaH};
    FillRect(hdc, &rcImg, bgBrush);

    if (!ew->srcBitmap || ew->imgDisplayW <= 0 || ew->imgDisplayH <= 0) {
        return;
    }

    Graphics g(hdc);

    // draw the image
    g.DrawImage(ew->srcBitmap, ew->imgDisplayX, ew->imgDisplayY, ew->imgDisplayW, ew->imgDisplayH);

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

    // draw semi-transparent overlay using GDI+ with explicit Gdiplus::Brush pointer
    Gdiplus::SolidBrush overlayBrush(Gdiplus::Color(128, 0, 0, 0));
    Gdiplus::Brush* pBrush = &overlayBrush;

    // top strip
    if (cropDispY > iy) {
        g.FillRectangle(pBrush, ix, iy, iw, cropDispY - iy);
    }
    // bottom strip
    if (cropDispB < ib) {
        g.FillRectangle(pBrush, ix, cropDispB, iw, ib - cropDispB);
    }
    // left strip (between top and bottom crop)
    if (cropDispX > ix) {
        g.FillRectangle(pBrush, ix, cropDispY, cropDispX - ix, cropDispB - cropDispY);
    }
    // right strip
    if (cropDispR < ir) {
        g.FillRectangle(pBrush, cropDispR, cropDispY, ir - cropDispR, cropDispB - cropDispY);
    }

    // draw crop border
    Gdiplus::Pen pen(Color(255, 255, 255), 1.0f);
    pen.SetDashStyle(Gdiplus::DashStyleDash);
    g.DrawRectangle(&pen, cropDispX, cropDispY, cropDispR - cropDispX, cropDispB - cropDispY);

    // draw drag handles at corners and edge midpoints
    int hs = kDragHandleSize;
    int hh = hs / 2;
    int midX = (cropDispX + cropDispR) / 2;
    int midY = (cropDispY + cropDispB) / 2;

    Gdiplus::SolidBrush handleBrush(Color(255, 255, 255, 255));
    Gdiplus::Pen handlePen(Color(255, 0, 0, 0), 1);

    auto drawHandle = [&](int cx, int cy) {
        g.FillRectangle(&handleBrush, cx - hh, cy - hh, hs, hs);
        g.DrawRectangle(&handlePen, cx - hh, cy - hh, hs, hs);
    };

    // corners
    drawHandle(cropDispX, cropDispY);
    drawHandle(cropDispR, cropDispY);
    drawHandle(cropDispX, cropDispB);
    drawHandle(cropDispR, cropDispB);
    // edge midpoints
    drawHandle(midX, cropDispY);
    drawHandle(midX, cropDispB);
    drawHandle(cropDispX, midY);
    drawHandle(cropDispR, midY);
}

static void PaintResizeImage(ImageEditWindow* ew, HDC hdc) {
    Rect cRc = ClientRect(ew->hwnd);

    // fill image area background
    HBRUSH bgBrush = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
    RECT rcImg = {0, 0, cRc.dx, ew->imgAreaH};
    FillRect(hdc, &rcImg, bgBrush);

    if (!ew->srcBitmap || ew->imgDisplayW <= 0 || ew->imgDisplayH <= 0) {
        return;
    }

    Graphics g(hdc);

    // draw the full image
    g.DrawImage(ew->srcBitmap, ew->imgDisplayX, ew->imgDisplayY, ew->imgDisplayW, ew->imgDisplayH);

    // draw semi-transparent overlay over the entire image
    Gdiplus::SolidBrush overlayBrush(Gdiplus::Color(128, 0, 0, 0));
    Gdiplus::Brush* pBrush = &overlayBrush;
    g.FillRectangle(pBrush, ew->imgDisplayX, ew->imgDisplayY, ew->imgDisplayW, ew->imgDisplayH);

    // draw the "new size" rectangle showing the resized portion, centered
    int dispNewW = ImageToDisplayW(ew, ew->newW);
    int dispNewH = ImageToDisplayH(ew, ew->newH);
    int cx = ew->imgDisplayX + ew->imgDisplayW / 2;
    int cy = ew->imgDisplayY + ew->imgDisplayH / 2;
    int newLeft = cx - dispNewW / 2;
    int newTop = cy - dispNewH / 2;

    // redraw the image portion at the new size area (clear overlay there)
    // clip source to full image, draw scaled into the new rect
    g.DrawImage(ew->srcBitmap, newLeft, newTop, dispNewW, dispNewH);

    // draw border around the new size rectangle
    Gdiplus::Pen pen(Color(255, 255, 255), 1.0f);
    pen.SetDashStyle(Gdiplus::DashStyleDash);
    g.DrawRectangle(&pen, newLeft, newTop, dispNewW, dispNewH);

    // draw drag handles
    int hs = kDragHandleSize;
    int hh = hs / 2;
    int midX = newLeft + dispNewW / 2;
    int midY = newTop + dispNewH / 2;
    int right = newLeft + dispNewW;
    int bottom = newTop + dispNewH;

    Gdiplus::SolidBrush handleBrush(Color(255, 255, 255, 255));
    Gdiplus::Pen handlePen(Color(255, 0, 0, 0), 1);

    auto drawHandle = [&](int hx, int hy) {
        g.FillRectangle(&handleBrush, hx - hh, hy - hh, hs, hs);
        g.DrawRectangle(&handlePen, hx - hh, hy - hh, hs, hs);
    };

    drawHandle(newLeft, newTop);
    drawHandle(right, newTop);
    drawHandle(newLeft, bottom);
    drawHandle(right, bottom);
    drawHandle(midX, newTop);
    drawHandle(midX, bottom);
    drawHandle(newLeft, midY);
    drawHandle(right, midY);
}

static void LayoutControls(ImageEditWindow* ew) {
    Rect cRc = ClientRect(ew->hwnd);
    int y = ew->imgAreaH + kRowPadding;
    int x = kButtonPadding;
    int w = cRc.dx - 2 * kButtonPadding;

    // row 1: file path label — skip if from RenderedBitmap
    if (!ew->fromRenderedBitmap) {
        int editBorder = GetSystemMetrics(SM_CXEDGE);
        LRESULT margins = SendMessageW(ew->hwndDestEdit, EM_GETMARGINS, 0, 0);
        int editLeftMargin = LOWORD(margins);
        int labelShift = editBorder + editLeftMargin;
        MoveWindow(ew->hwndPathLabel, x + labelShift, y, w - labelShift, 16, TRUE);
        y += 16 + kRowPadding;
    }

    // row 2: dest edit + browse button
    int browseW = 30;
    MoveWindow(ew->hwndDestEdit, x, y, w - browseW - 4, 22, TRUE);
    MoveWindow(ew->hwndBrowseBtn, x + w - browseW, y, browseW, 22, TRUE);
    y += 22 + kRowPadding;

    // row 3: info label, cancel, save
    // layout buttons first to know where info label must stop
    int bx = cRc.dx - kButtonPadding;
    if (ew->btnSave && ew->btnCancel) {
        // right-to-left: Cancel, Resize, Crop, [Format], Save
        Size szCancel = ew->btnCancel->GetIdealSize();
        bx -= szCancel.dx;
        ew->btnCancel->SetBounds({bx, y, szCancel.dx, szCancel.dy});
        if (ew->btnResize) {
            Size szResize = ew->btnResize->GetIdealSize();
            bx -= szResize.dx + 4;
            ew->btnResize->SetBounds({bx, y, szResize.dx, szResize.dy});
        }
        if (ew->btnCrop) {
            Size szCrop = ew->btnCrop->GetIdealSize();
            bx -= szCrop.dx + 4;
            ew->btnCrop->SetBounds({bx, y, szCrop.dx, szCrop.dy});
        }
        if (ew->dropFormat) {
            Size szDrop = ew->dropFormat->GetIdealSize();
            bx -= szDrop.dx + 4;
            // vertically center dropdown with buttons
            int dropY = y + (szCancel.dy - szDrop.dy) / 2;
            ew->dropFormat->SetBounds({bx, dropY, szDrop.dx, szDrop.dy});
        }
        Size szSave = ew->btnSave->GetIdealSize();
        bx -= szSave.dx + 4;
        ew->btnSave->SetBounds({bx, y, szSave.dx, szSave.dy});
    }

    // size info label to its text, but don't overlap buttons
    HDC hdc = GetDC(ew->hwndInfoLabel);
    HFONT oldFont = (HFONT)SelectObject(hdc, ew->hFont);
    char buf[256];
    int textLen = GetWindowTextA(ew->hwndInfoLabel, buf, dimof(buf));
    SIZE textSize{};
    GetTextExtentPoint32A(hdc, buf, textLen, &textSize);
    SelectObject(hdc, oldFont);
    ReleaseDC(ew->hwndInfoLabel, hdc);
    int maxLabelW = bx - x - 8;
    int labelW = std::min((int)textSize.cx + 8, maxLabelW);
    if (labelW < 0) {
        labelW = 0;
    }
    MoveWindow(ew->hwndInfoLabel, x, y + 4, labelW, 16, TRUE);
}

static void OnBrowse(ImageEditWindow* ew) {
    WCHAR dstFileName[MAX_PATH + 1]{};
    // pre-populate with current dest path
    int len = GetWindowTextW(ew->hwndDestEdit, dstFileName, MAX_PATH);
    (void)len;

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

    if (GetSaveFileNameW(&ofn)) {
        SetWindowTextW(ew->hwndDestEdit, dstFileName);
    }
}

// Save a GDI+ Bitmap using WIC. Supports all formats that have a WIC encoder installed,
// including WebP on Windows 10+.
static bool SaveBitmapWithWIC(Bitmap* bmp, const WCHAR* destPath, const GUID* containerFormat) {
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

    hr = SHCreateStreamOnFileEx(destPath, STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, FILE_ATTRIBUTE_NORMAL, TRUE,
                                nullptr, &pStream);
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

    WCHAR rawDestW[MAX_PATH + 1]{};
    GetWindowTextW(ew->hwndDestEdit, rawDestW, MAX_PATH);
    TempStr rawDest = ToUtf8Temp(rawDestW);
    if (str::IsEmpty(rawDest)) {
        return;
    }

    // ensure extension matches selected format
    int fmtIdx = GetSelectedFormatIdx(ew);
    const char* fmtExt = gImageFormats[fmtIdx].ext;
    TempStr destExt = path::GetExtTemp(rawDest);
    TempStr dest;
    if (str::EqI(destExt, fmtExt)) {
        dest = str::DupTemp(rawDest);
    } else {
        int baseLen = str::Leni(rawDest) - str::Leni(destExt);
        TempStr base = str::DupTemp(rawDest, baseLen);
        dest = str::FormatTemp("%s%s", base, fmtExt);
    }

    Bitmap* result = nullptr;
    if (ew->mode == ImageEditMode::Save) {
        // save as-is
        result = ew->srcBitmap->Clone(0, 0, ew->imgW, ew->imgH, ew->srcBitmap->GetPixelFormat());
        if (!result) {
            MessageBoxWarning(ew->hwnd, "Failed to save image", "Save Image");
            return;
        }
    } else if (ew->mode == ImageEditMode::Crop) {
        // create cropped bitmap
        Gdiplus::Rect srcRect(ew->cropX, ew->cropY, ew->cropW, ew->cropH);
        result = ew->srcBitmap->Clone(srcRect, ew->srcBitmap->GetPixelFormat());
        if (!result) {
            MessageBoxWarning(ew->hwnd, "Failed to create cropped image", _TRA("Crop Image"));
            return;
        }
    } else {
        // create resized bitmap
        result = new Bitmap(ew->newW, ew->newH, ew->srcBitmap->GetPixelFormat());
        if (!result) {
            MessageBoxWarning(ew->hwnd, "Failed to create resized image", _TRA("Resize Image"));
            return;
        }
        Graphics g(result);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(ew->srcBitmap, 0, 0, ew->newW, ew->newH);
    }

    TempWStr destW = ToWStrTemp(dest);
    bool saved = SaveBitmapWithWIC(result, destW, gImageFormats[fmtIdx].containerFormat);
    delete result;

    if (!saved) {
        MessageBoxWarning(ew->hwnd, "Failed to save image", "Save Image");
        return;
    }

    // load the saved image
    HWND hwndParent = ew->hwndParent;
    char* savedPath = str::Dup(dest);
    DestroyWindow(ew->hwnd);

    MainWindow* win = FindMainWindowByHwnd(hwndParent);
    if (!win && !gWindows.IsEmpty()) {
        win = gWindows.at(0);
    }
    if (win) {
        LoadArgs args(savedPath, win);
        StartLoadDocument(&args);
    }
    free(savedPath);
}

static void OnCancel(ImageEditWindow* ew) {
    DestroyWindow(ew->hwnd);
}

static void UpdateInfoLabel(ImageEditWindow* ew);
static void UpdateModeButtons(ImageEditWindow* ew);

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
    SetWindowTextW(ew->hwnd, L"Save Image");
    UpdateModeButtons(ew);
    UpdateSaveButtonText(ew);
    UpdateInfoLabel(ew);
    LayoutControls(ew);
    InvalidateImageArea(ew);
    SetFocus(ew->hwnd);
}

static void ReplaceSrcBitmap(ImageEditWindow* ew, Bitmap* newBmp) {
    delete ew->srcBitmap;
    ew->srcBitmap = newBmp;
    ew->imgW = (int)newBmp->GetWidth();
    ew->imgH = (int)newBmp->GetHeight();
    CalcImageLayout(ew);
}

static bool IsCropChanged(ImageEditWindow* ew) {
    return ew->cropX != 0 || ew->cropY != 0 || ew->cropW != ew->imgW || ew->cropH != ew->imgH;
}

static bool IsResizeChanged(ImageEditWindow* ew) {
    return ew->newW != ew->imgW || ew->newH != ew->imgH;
}

static void UpdateModeButtons(ImageEditWindow* ew) {
    if (ew->mode == ImageEditMode::Crop) {
        ew->btnCrop->SetText(_TRN("Apply Crop"));
        ew->btnCrop->SetIsEnabled(IsCropChanged(ew));
        ew->btnResize->SetText(_TRN("Resize"));
        ew->btnResize->SetIsEnabled(true);
    } else if (ew->mode == ImageEditMode::Resize) {
        ew->btnCrop->SetText(_TRN("Crop"));
        ew->btnCrop->SetIsEnabled(true);
        ew->btnResize->SetText(_TRN("Apply Resize"));
        ew->btnResize->SetIsEnabled(IsResizeChanged(ew));
    } else {
        // Save mode
        ew->btnCrop->SetText(_TRN("Crop"));
        ew->btnCrop->SetIsEnabled(true);
        ew->btnResize->SetText(_TRN("Resize"));
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
        ReplaceSrcBitmap(ew, cropped);
        ew->cropX = 0;
        ew->cropY = 0;
        ew->cropW = ew->imgW;
        ew->cropH = ew->imgH;
        UpdateModeButtons(ew);
        UpdateInfoLabel(ew);
        LayoutControls(ew);
        InvalidateImageArea(ew);
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
        Graphics g(resized);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(ew->srcBitmap, 0, 0, ew->newW, ew->newH);
        ReplaceSrcBitmap(ew, resized);
        ew->newW = ew->imgW;
        ew->newH = ew->imgH;
        UpdateModeButtons(ew);
        UpdateInfoLabel(ew);
        LayoutControls(ew);
        InvalidateImageArea(ew);
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
    SetWindowTextW(ew->hwnd, L"Crop Image");
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
    SetWindowTextW(ew->hwnd, L"Resize Image");
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

LRESULT CALLBACK WndProcImageEdit(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

LRESULT CALLBACK WndProcImageEdit(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ImageEditWindow* ew;

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res != 0) {
        return res;
    }

    switch (msg) {
        case WM_CREATE:
            break;

        case WM_SIZE: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (ew) {
                CalcImageLayout(ew);
                LayoutControls(ew);
                if (ew->mode == ImageEditMode::Crop) {
                    InvalidateImageArea(ew);
                } else {
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            return 0;
        }

        case WM_PAINT: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (ew) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                // double-buffer only the image area to avoid flicker
                Rect cRc = ClientRect(hwnd);
                int paintH = ew->imgAreaH;
                if (paintH > cRc.dy) {
                    paintH = cRc.dy;
                }
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBmp = CreateCompatibleBitmap(hdc, cRc.dx, paintH);
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
                if (ew->mode == ImageEditMode::Save) {
                    PaintSaveImage(ew, memDC);
                } else if (ew->mode == ImageEditMode::Crop) {
                    PaintCropImage(ew, memDC);
                } else {
                    PaintResizeImage(ew, memDC);
                }
                BitBlt(hdc, 0, 0, cRc.dx, paintH, memDC, 0, 0, SRCCOPY);
                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
                EndPaint(hwnd, &ps);
            }
            return 0;
        }

        case WM_ERASEBKGND: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (ew) {
                // paint control area background, skip image area (double-buffered)
                HDC hdc = (HDC)wp;
                RECT crc;
                GetClientRect(hwnd, &crc);
                RECT ctrlRc = {0, ew->imgAreaH, crc.right, crc.bottom};
                FillRect(hdc, &ctrlRc, GetSysColorBrush(COLOR_BTNFACE));
            }
            return 1;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wp;
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }

        case WM_MOUSEMOVE: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (!ew || ew->mode == ImageEditMode::Save) {
                break;
            }
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            if (ew->isDragging) {
                if (ew->mode == ImageEditMode::Crop) {
                    int imgDx = DisplayToImageX(ew, mx) - DisplayToImageX(ew, ew->dragStart.x);
                    int imgDy = DisplayToImageY(ew, my) - DisplayToImageY(ew, ew->dragStart.y);

                    auto edge = ew->dragEdge;
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
                        if (nx < 0) {
                            nx = 0;
                        }
                        if (ny < 0) {
                            ny = 0;
                        }
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
                        nw = ew->dragNewW - imgDx * 2; // symmetric resize
                    }
                    if (edge == DragEdge::Right || edge == DragEdge::TopRight || edge == DragEdge::BottomRight) {
                        nw = ew->dragNewW + imgDx * 2;
                    }
                    // top/bottom edges change height
                    if (edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight) {
                        nh = ew->dragNewH - imgDy * 2;
                    }
                    if (edge == DragEdge::Bottom || edge == DragEdge::BottomLeft || edge == DragEdge::BottomRight) {
                        nh = ew->dragNewH + imgDy * 2;
                    }

                    if (nw < 1) {
                        nw = 1;
                    }
                    if (nh < 1) {
                        nh = 1;
                    }

                    ew->newW = nw;
                    ew->newH = nh;
                    GrowWindowIfNeeded(ew, edge);
                    UpdateInfoLabel(ew);
                    InvalidateImageArea(ew);
                }
            } else {
                DragEdge edge;
                if (ew->mode == ImageEditMode::Crop) {
                    edge = HitTestCropEdge(ew, mx, my);
                } else {
                    edge = HitTestResizeEdge(ew, mx, my);
                }
                ew->hoverEdge = edge;
                SetCursor(GetCursorForEdge(edge));
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (!ew) {
                break;
            }
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            DragEdge edge;
            if (ew->mode == ImageEditMode::Crop) {
                edge = HitTestCropEdge(ew, mx, my);
            } else {
                edge = HitTestResizeEdge(ew, mx, my);
            }
            if (edge != DragEdge::None) {
                ew->isDragging = true;
                ew->dragEdge = edge;
                ew->dragStart = {mx, my};
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
            return 0;
        }

        case WM_LBUTTONUP: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (ew && ew->isDragging) {
                ew->isDragging = false;
                ReleaseCapture();
                UpdateModeButtons(ew);
            }
            return 0;
        }

        case WM_SETCURSOR: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (ew && ew->mode != ImageEditMode::Save && LOWORD(lp) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                DragEdge edge;
                if (ew->mode == ImageEditMode::Crop) {
                    edge = HitTestCropEdge(ew, pt.x, pt.y);
                } else {
                    edge = HitTestResizeEdge(ew, pt.x, pt.y);
                }
                if (edge != DragEdge::None) {
                    SetCursor(GetCursorForEdge(edge));
                    return TRUE;
                }
            }
            return DefWindowProc(hwnd, msg, wp, lp);
        }

        case WM_CHAR:
            if (VK_ESCAPE == wp) {
                return 0;
            }
            break;

        case WM_KEYDOWN: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (!ew) {
                break;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wp == 'C') {
                CopyEditedImageToClipboard(ew);
                return 0;
            }
            if (wp == VK_ESCAPE) {
                if (ew->mode != ImageEditMode::Save) {
                    SwitchToSaveMode(ew);
                } else if (gGlobalPrefs->escToExit) {
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            if (ew->mode == ImageEditMode::Resize) {
                // left/right change width, up/down change height
                if (wp == VK_LEFT) {
                    ew->newW -= 1;
                } else if (wp == VK_RIGHT) {
                    ew->newW += 1;
                } else if (wp == VK_UP) {
                    ew->newH += 1;
                } else if (wp == VK_DOWN) {
                    ew->newH -= 1;
                } else {
                    break;
                }
                if (ew->newW < 1) {
                    ew->newW = 1;
                }
                if (ew->newH < 1) {
                    ew->newH = 1;
                }
                UpdateInfoLabel(ew);
                UpdateModeButtons(ew);
                InvalidateImageArea(ew);
                return 0;
            }
            // crop mode: need hoverEdge and directional deltas
            {
                auto edge = ew->hoverEdge;
                if (edge == DragEdge::None) {
                    break;
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
                }
                if (edge == DragEdge::Move) {
                    ew->cropX += dx;
                    ew->cropY += dy;
                    if (ew->cropX < 0) {
                        ew->cropX = 0;
                    }
                    if (ew->cropY < 0) {
                        ew->cropY = 0;
                    }
                    if (ew->cropX + ew->cropW > ew->imgW) {
                        ew->cropX = ew->imgW - ew->cropW;
                    }
                    if (ew->cropY + ew->cropH > ew->imgH) {
                        ew->cropY = ew->imgH - ew->cropH;
                    }
                } else {
                    // nudge individual edge
                    if (dx != 0 &&
                        (edge == DragEdge::Left || edge == DragEdge::TopLeft || edge == DragEdge::BottomLeft)) {
                        ew->cropX += dx;
                        ew->cropW -= dx;
                    }
                    if (dx != 0 &&
                        (edge == DragEdge::Right || edge == DragEdge::TopRight || edge == DragEdge::BottomRight)) {
                        ew->cropW += dx;
                    }
                    if (dy != 0 && (edge == DragEdge::Top || edge == DragEdge::TopLeft || edge == DragEdge::TopRight)) {
                        ew->cropY += dy;
                        ew->cropH -= dy;
                    }
                    if (dy != 0 &&
                        (edge == DragEdge::Bottom || edge == DragEdge::BottomLeft || edge == DragEdge::BottomRight)) {
                        ew->cropH += dy;
                    }
                    // clamp
                    if (ew->cropX < 0) {
                        ew->cropW += ew->cropX;
                        ew->cropX = 0;
                    }
                    if (ew->cropY < 0) {
                        ew->cropH += ew->cropY;
                        ew->cropY = 0;
                    }
                    if (ew->cropW < 1) {
                        ew->cropW = 1;
                    }
                    if (ew->cropH < 1) {
                        ew->cropH = 1;
                    }
                    if (ew->cropX + ew->cropW > ew->imgW) {
                        ew->cropW = ew->imgW - ew->cropX;
                    }
                    if (ew->cropY + ew->cropH > ew->imgH) {
                        ew->cropH = ew->imgH - ew->cropY;
                    }
                }
            } // end crop mode block
            UpdateInfoLabel(ew);
            UpdateModeButtons(ew);
            InvalidateImageArea(ew);
            return 0;
        }

        case WM_COMMAND: {
            ew = FindImageEditWindowByHwnd(hwnd);
            if (!ew) {
                break;
            }
            int code = HIWORD(wp);
            // browse button
            if ((HWND)lp == ew->hwndBrowseBtn && code == BN_CLICKED) {
                OnBrowse(ew);
                UpdateSaveButtonText(ew);
                return 0;
            }
            // dest edit changed
            if ((HWND)lp == ew->hwndDestEdit && code == EN_CHANGE) {
                UpdateSaveButtonText(ew);
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            ew = FindImageEditWindowByHwnd(hwnd);
            if (ew) {
                gImageEditWindows.Remove(ew);
                delete ew;
            }
            break;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

void ShowImageEditWindow(MainWindow* win, ImageEditMode mode, const char* filePath, RenderedBitmap* rbmp) {
    if (!win) {
        return;
    }

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
        // load from current tab's file
        if (!filePath) {
            WindowTab* tab = win->CurrentTab();
            if (!tab || !tab->filePath) {
                return;
            }
            Kind engineType = tab->GetEngineType();
            if (engineType != kindEngineImage) {
                return;
            }
            filePath = tab->filePath;
        }
        ByteSlice data = file::ReadFile(filePath);
        if (data.empty()) {
            return;
        }
        bmp = BitmapFromData(data);
        data.Free();
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

    auto* ew = new ImageEditWindow();
    ew->mode = mode;
    ew->fromRenderedBitmap = fromRenderedBitmap;
    ew->filePath = filePath ? str::Dup(filePath) : nullptr;
    ew->srcBitmap = bmp;
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
    WNDCLASSEX wcex = {};
    FillWndClassEx(wcex, kImageEditWinClassName, WndProcImageEdit);
    wcex.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
    wcex.hIcon = LoadIconW(h, iconName);
    RegisterClassEx(&wcex);

    // calculate window size: image at 100% + padding + control area, clamped to screen
    int wantW = imgW + 2 * kImagePadding;
    int controlDy = fromRenderedBitmap ? (kControlAreaDy - kPathLabelRowDy) : kControlAreaDy;
    int wantH = imgH + 2 * kImagePadding + controlDy;
    // add window chrome
    RECT rc = {0, 0, wantW, wantH};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int winW = rc.right - rc.left;
    if (winW < kMinWindowWidth) {
        winW = kMinWindowWidth;
    }
    int winH = rc.bottom - rc.top;
    // clamp to screen
    HMONITOR hMon = MonitorFromWindow(win->hwndFrame, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(hMon, &mi);
    int screenW = mi.rcWork.right - mi.rcWork.left;
    int screenH = mi.rcWork.bottom - mi.rcWork.top;
    if (winW > screenW) {
        winW = screenW;
    }
    if (winH > screenH) {
        winH = screenH;
    }

    const WCHAR* title = L"Save Image";
    if (mode == ImageEditMode::Crop) {
        title = L"Crop Image";
    } else if (mode == ImageEditMode::Resize) {
        title = L"Resize Image";
    }
    HWND hwnd = CreateWindowExW(0, kImageEditWinClassName, title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,
                                CW_USEDEFAULT, winW, winH, nullptr, nullptr, h, nullptr);
    if (!hwnd) {
        gImageEditWindows.Remove(ew);
        delete ew;
        return;
    }

    ew->hwnd = hwnd;
    ew->hwndParent = win->hwndFrame;

    // create font
    ew->hFont = GetDefaultGuiFont();

    // create child controls
    // row 1: file path label (read-only) — hidden when from RenderedBitmap
    DWORD pathLabelStyle = WS_CHILD | SS_LEFT | SS_PATHELLIPSIS;
    if (!fromRenderedBitmap) {
        pathLabelStyle |= WS_VISIBLE;
    }
    ew->hwndPathLabel = CreateWindowExW(0, L"STATIC", filePath ? ToWStrTemp(filePath) : L"", pathLabelStyle, 0, 0, 0, 0,
                                        hwnd, nullptr, h, nullptr);
    SendMessageW(ew->hwndPathLabel, WM_SETFONT, (WPARAM)ew->hFont, TRUE);

    // row 2: dest edit + browse
    TempStr destPath = filePath ? MakeUniqueFilePathTemp(filePath) : str::DupTemp("");
    ew->hwndDestEdit = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, ToWStrTemp(destPath),
                                       WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, nullptr, h, nullptr);
    SendMessageW(ew->hwndDestEdit, WM_SETFONT, (WPARAM)ew->hFont, TRUE);

    ew->hwndBrowseBtn = CreateWindowExW(0, L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                                        nullptr, h, nullptr);
    SendMessageW(ew->hwndBrowseBtn, WM_SETFONT, (WPARAM)ew->hFont, TRUE);

    // row 3: info label
    TempStr infoStr;
    if (mode == ImageEditMode::Save) {
        infoStr = str::FormatTemp("%d x %d", imgW, imgH);
    } else if (mode == ImageEditMode::Crop) {
        infoStr = FormatCropInfoTemp(imgW, imgH, imgW, imgH, 0, 0);
    } else {
        infoStr = FormatResizeInfoTemp(imgW, imgH, imgW, imgH);
    }
    ew->hwndInfoLabel = CreateWindowExW(0, L"STATIC", ToWStrTemp(infoStr), WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0,
                                        hwnd, nullptr, h, nullptr);
    SendMessageW(ew->hwndInfoLabel, WM_SETFONT, (WPARAM)ew->hFont, TRUE);

    // buttons
    {
        auto* btn = new Button();
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRN("Cancel");
        btn->Create(args);
        btn->onClick = MkFunc0<ImageEditWindow>(OnCancel, ew);
        ew->btnCancel = btn;
    }
    {
        auto* btn = new Button();
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRN("Save");
        btn->Create(args);
        btn->onClick = MkFunc0<ImageEditWindow>(OnSave, ew);
        ew->btnSave = btn;
    }
    {
        auto* btn = new Button();
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRN("Crop");
        btn->Create(args);
        btn->onClick = MkFunc0<ImageEditWindow>(OnCropButton, ew);
        ew->btnCrop = btn;
    }
    {
        auto* btn = new Button();
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRN("Resize");
        btn->Create(args);
        btn->onClick = MkFunc0<ImageEditWindow>(OnResizeButton, ew);
        ew->btnResize = btn;
    }

    // format dropdown
    {
        auto* dd = new DropDown();
        DropDown::CreateArgs args;
        args.parent = hwnd;
        dd->Create(args);
        StrVec items;
        int defaultDdIdx = 0;
        for (int i = 0; i < (int)dimof(gImageFormats); i++) {
            if (!gImageFormats[i].available) {
                continue;
            }
            if (i == kDefaultFormatIdx) {
                defaultDdIdx = ew->formatIndices.Size();
            }
            ew->formatIndices.Append(i);
            items.Append(gImageFormats[i].label);
        }
        dd->SetItems(items);
        dd->SetCurrentSelection(defaultDdIdx);
        dd->onSelectionChanged = MkFunc0<ImageEditWindow>(OnFormatChanged, ew);
        ew->dropFormat = dd;
    }

    CalcImageLayout(ew);
    UpdateModeButtons(ew);
    LayoutControls(ew);
    UpdateSaveButtonText(ew);

    CenterDialog(hwnd, win->hwndFrame);
    HwndEnsureVisible(hwnd);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    ShowWindow(hwnd, SW_SHOW);
}
