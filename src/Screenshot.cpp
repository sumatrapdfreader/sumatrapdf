/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/Log.h"

#include "Notifications.h"
#include "AppTools.h"
#include "Screenshot.h"
#include "ImageSaveCropResize.h"
#include "Commands.h"
#include "Accelerators.h"

struct MainWindow;
extern Vec<MainWindow*> gWindows;

static const WCHAR* kScreenshotOverlayClassName = L"SumatraScreenshotOverlay";
static bool gScreenshotClassRegistered = false;

// horizontal padding in each grid cell (on each side of thumbnail)
constexpr int kGridPaddingX = 16;
// vertical padding in each grid cell (on each side of thumbnail)
constexpr int kGridPaddingY = 16;
// outer padding around the entire overlay
constexpr int kOuterPadding = 32;
// border thickness for selected thumbnail
constexpr int kBorderThickness = 3;
// max thumbnail size
constexpr int kMaxThumbSize = 240;
// height reserved for label text below thumbnail
constexpr int kLabelHeight = 20;
// extra gap below label
constexpr int kLabelGap = 4;
// height for the info bar at the bottom
constexpr int kInfoBarHeight = 30;

struct CapturedScreenshot {
    HBITMAP bmp = nullptr;   // full-size capture
    HBITMAP thumb = nullptr; // scaled thumbnail
    HWND srcHwnd = nullptr;  // source window (nullptr for desktop)
    int origW = 0;
    int origH = 0;
    int thumbW = 0;
    int thumbH = 0;
    char* processName = nullptr; // for file naming
};

struct ScreenshotOverlayData {
    Vec<CapturedScreenshot> captures;
    int selected = 0; // index of currently selected (hovered/arrow-keyed)
    int cols = 0;
    int rows = 0;
    Vec<int> colWidths;  // width of each column
    Vec<int> rowHeights; // height of each row
    Vec<int> colX;       // x start of each column (cumulative)
    Vec<int> rowY;       // y start of each row (cumulative)
    int winW = 0;
    int winH = 0;
};

static bool ShouldCaptureWindow(HWND hwnd, HWND overlayHwnd) {
    if (hwnd == overlayHwnd) {
        return false;
    }
    if (!IsWindowVisible(hwnd)) {
        return false;
    }
    if (hwnd == GetDesktopWindow()) {
        return false;
    }
    WCHAR className[256];
    if (GetClassNameW(hwnd, className, 256) > 0) {
        if (str::Eq(className, L"Progman") || str::Eq(className, L"WorkerW")) {
            return false;
        }
    }
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }
    HWND hwndOwner = GetWindow(hwnd, GW_OWNER);
    if (hwndOwner != nullptr && !(exStyle & WS_EX_APPWINDOW)) {
        // owned window without WS_EX_APPWINDOW: accept if it has a caption
        // (modal dialogs like config windows), reject otherwise
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        if ((style & WS_CAPTION) != WS_CAPTION) {
            return false;
        }
    }
    BOOL isCloaked = FALSE;
    if (SUCCEEDED(dwm::GetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked)))) {
        if (isCloaked) {
            return false;
        }
    }
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        return false;
    }
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0) {
        return false;
    }
    if (w * h < 100) {
        return false;
    }
    WCHAR title[256];
    int titleLen = GetWindowTextW(hwnd, title, 256);
    if (titleLen == 0) {
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        if (!(style & WS_POPUP)) {
            return false;
        }
    }
    return true;
}

static HBITMAP CaptureDesktop() {
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    SelectObject(hdcMem, hbm);
    BitBlt(hdcMem, 0, 0, w, h, hdcScreen, x, y, SRCCOPY);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return hbm;
}

// replace black corner pixels from PrintWindow with bgColor using a rounded corner mask
static void FixRoundedCorners(HBITMAP hbm, int w, int h, COLORREF bgColor) {
    // get DWM corner radius; typical Windows 11 value is 8
    int radius = 8;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    DWORD* pixels = (DWORD*)malloc(w * h * 4);
    if (!pixels) {
        ReleaseDC(nullptr, hdc);
        return;
    }
    GetDIBits(hdc, hbm, 0, h, pixels, &bmi, DIB_RGB_COLORS);

    DWORD bg = (GetRValue(bgColor) << 16) | (GetGValue(bgColor) << 8) | GetBValue(bgColor);

    // fix the four corners
    for (int cy = 0; cy < radius; cy++) {
        for (int cx = 0; cx < radius; cx++) {
            // distance from corner center to pixel
            int dx = radius - 1 - cx;
            int dy = radius - 1 - cy;
            if (dx * dx + dy * dy > radius * radius) {
                // outside the rounded corner — replace with background
                // top-left
                pixels[cy * w + cx] = bg;
                // top-right
                pixels[cy * w + (w - 1 - cx)] = bg;
                // bottom-left
                pixels[(h - 1 - cy) * w + cx] = bg;
                // bottom-right
                pixels[(h - 1 - cy) * w + (w - 1 - cx)] = bg;
            }
        }
    }

    SetDIBits(hdc, hbm, 0, h, pixels, &bmi, DIB_RGB_COLORS);
    free(pixels);
    ReleaseDC(nullptr, hdc);
}

// outW/outH receive the actual captured size (may differ from GetWindowRect due to cropping)
static HBITMAP CaptureWindowBmp(HWND hwnd, int* outW, int* outH) {
    RECT fullRect;
    if (!GetWindowRect(hwnd, &fullRect)) {
        return nullptr;
    }
    int fullW = fullRect.right - fullRect.left;
    int fullH = fullRect.bottom - fullRect.top;
    if (fullW <= 0 || fullH <= 0) {
        return nullptr;
    }

    HDC hdcWin = GetWindowDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcWin);
    HBITMAP hbmFull = CreateCompatibleBitmap(hdcWin, fullW, fullH);
    SelectObject(hdcMem, hbmFull);

    // Try PrintWindow with PW_RENDERFULLCONTENT (0x2) first (Windows 8.1+)
    BOOL ok = PrintWindow(hwnd, hdcMem, 0x2);
    if (!ok) {
        ok = BitBlt(hdcMem, 0, 0, fullW, fullH, hdcWin, 0, 0, SRCCOPY);
    }

    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcWin);

    if (!ok) {
        DeleteObject(hbmFull);
        return nullptr;
    }

    // crop to visible frame bounds (excludes invisible resize/shadow border)
    RECT visibleRect;
    HRESULT hr = dwm::GetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &visibleRect, sizeof(visibleRect));
    if (SUCCEEDED(hr)) {
        int cropX = visibleRect.left - fullRect.left;
        int cropY = visibleRect.top - fullRect.top;
        int cropW = visibleRect.right - visibleRect.left;
        int cropH = visibleRect.bottom - visibleRect.top;
        if (cropX > 0 || cropY > 0 || cropW < fullW || cropH < fullH) {
            HDC hdcScreen = GetDC(nullptr);
            HDC hdcSrc = CreateCompatibleDC(hdcScreen);
            HDC hdcDst = CreateCompatibleDC(hdcScreen);
            HBITMAP hbmCropped = CreateCompatibleBitmap(hdcScreen, cropW, cropH);
            SelectObject(hdcSrc, hbmFull);
            SelectObject(hdcDst, hbmCropped);
            BitBlt(hdcDst, 0, 0, cropW, cropH, hdcSrc, cropX, cropY, SRCCOPY);
            DeleteDC(hdcSrc);
            DeleteDC(hdcDst);
            ReleaseDC(nullptr, hdcScreen);
            DeleteObject(hbmFull);
            hbmFull = hbmCropped;
            fullW = cropW;
            fullH = cropH;
        }
    }

    // fix black corners from DWM rounded windows
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_DEFAULT;
    dwm::GetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
    if (cornerPref == DWMWCP_DEFAULT || cornerPref == DWMWCP_ROUND || cornerPref == DWMWCP_ROUNDSMALL) {
        COLORREF bgColor = GetSysColor(COLOR_WINDOW);
        FixRoundedCorners(hbmFull, fullW, fullH, bgColor);
    }

    *outW = fullW;
    *outH = fullH;
    return hbmFull;
}

static TempStr GetWindowProcessNameTemp(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return nullptr;
    }
    AutoCloseHandle hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc.IsValid()) {
        return nullptr;
    }
    WCHAR path[MAX_PATH]{};
    DWORD pathLen = MAX_PATH;
    if (!QueryFullProcessImageNameW(hProc, 0, path, &pathLen)) {
        return nullptr;
    }
    TempStr fullPath = ToUtf8Temp(path);
    TempStr baseName = path::GetBaseNameTemp(fullPath);
    TempStr noExt = path::GetPathNoExtTemp(baseName);
    return noExt;
}

static TempStr MakeUniquePathTemp(const char* dir, const char* base) {
    TempStr name = str::FormatTemp("%s.png", base);
    TempStr path = path::JoinTemp(dir, name);
    if (!file::Exists(path)) {
        return path;
    }
    for (int i = 1; i < 10000; i++) {
        name = str::FormatTemp("%s.%d.png", base, i);
        path = path::JoinTemp(dir, name);
        if (!file::Exists(path)) {
            return path;
        }
    }
    return nullptr;
}

// Create a scaled thumbnail of a bitmap
static HBITMAP CreateThumbnail(HBITMAP src, int srcW, int srcH, int* outW, int* outH) {
    int tw, th;
    if (srcW >= srcH) {
        tw = kMaxThumbSize;
        th = MulDiv(srcH, kMaxThumbSize, srcW);
    } else {
        th = kMaxThumbSize;
        tw = MulDiv(srcW, kMaxThumbSize, srcH);
    }
    if (tw < 1) {
        tw = 1;
    }
    if (th < 1) {
        th = 1;
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HDC hdcDst = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmThumb = CreateCompatibleBitmap(hdcScreen, tw, th);
    SelectObject(hdcSrc, src);
    SelectObject(hdcDst, hbmThumb);
    SetStretchBltMode(hdcDst, HALFTONE);
    SetBrushOrgEx(hdcDst, 0, 0, nullptr);
    StretchBlt(hdcDst, 0, 0, tw, th, hdcSrc, 0, 0, srcW, srcH, SRCCOPY);
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    ReleaseDC(nullptr, hdcScreen);

    *outW = tw;
    *outH = th;
    return hbmThumb;
}

static void FreeCapturedScreenshots(ScreenshotOverlayData* data) {
    for (auto& cs : data->captures) {
        if (cs.bmp) {
            DeleteObject(cs.bmp);
        }
        if (cs.thumb) {
            DeleteObject(cs.thumb);
        }
        str::Free(cs.processName);
    }
    data->captures.Reset();
}

struct EnumCaptureCtx {
    Vec<CapturedScreenshot>* captures;
    HWND overlayHwnd;
};

static BOOL CALLBACK EnumCaptureWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumCaptureCtx* ctx = (EnumCaptureCtx*)lParam;
    if (!ShouldCaptureWindow(hwnd, ctx->overlayHwnd)) {
        return TRUE;
    }
    int w = 0, h = 0;
    HBITMAP hbm = CaptureWindowBmp(hwnd, &w, &h);
    if (!hbm) {
        return TRUE;
    }

    CapturedScreenshot cs;
    cs.bmp = hbm;
    cs.srcHwnd = hwnd;
    cs.origW = w;
    cs.origH = h;
    cs.thumb = CreateThumbnail(hbm, w, h, &cs.thumbW, &cs.thumbH);
    TempStr procName = GetWindowProcessNameTemp(hwnd);
    cs.processName = str::Dup(procName ? procName : "unknown");
    ctx->captures->Append(cs);
    return TRUE;
}

static void CaptureAllScreenshots(ScreenshotOverlayData* data, HWND overlayHwnd) {
    // Desktop first
    HBITMAP hbmDesktop = CaptureDesktop();
    if (hbmDesktop) {
        int dw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int dh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        CapturedScreenshot cs;
        cs.bmp = hbmDesktop;
        cs.origW = dw;
        cs.origH = dh;
        cs.thumb = CreateThumbnail(hbmDesktop, dw, dh, &cs.thumbW, &cs.thumbH);
        cs.processName = str::Dup("desktop");
        data->captures.Append(cs);
    }

    // Individual windows
    EnumCaptureCtx ctx;
    ctx.captures = &data->captures;
    ctx.overlayHwnd = overlayHwnd;
    EnumWindows(EnumCaptureWindowsProc, (LPARAM)&ctx);
}

// Compute grid layout and overlay window size
static void ComputeLayout(ScreenshotOverlayData* data) {
    int n = data->captures.Size();
    if (n == 0) {
        return;
    }

    data->cols = (int)ceil(sqrt((double)n));
    data->rows = (n + data->cols - 1) / data->cols;

    // compute per-column widths (max thumb width in that column + 2*kGridPaddingX)
    data->colWidths.SetSize(data->cols);
    for (int c = 0; c < data->cols; c++) {
        data->colWidths[c] = 0;
    }
    for (int i = 0; i < n; i++) {
        int col = i % data->cols;
        int tw = data->captures[i].thumbW;
        if (tw > data->colWidths[col]) {
            data->colWidths[col] = tw;
        }
    }
    for (int c = 0; c < data->cols; c++) {
        data->colWidths[c] += 2 * kGridPaddingX;
    }

    // compute per-row heights (max thumb height in that row + label + 2*kGridPaddingY)
    data->rowHeights.SetSize(data->rows);
    for (int r = 0; r < data->rows; r++) {
        data->rowHeights[r] = 0;
    }
    for (int i = 0; i < n; i++) {
        int row = i / data->cols;
        int th = data->captures[i].thumbH;
        if (th > data->rowHeights[row]) {
            data->rowHeights[row] = th;
        }
    }
    for (int r = 0; r < data->rows; r++) {
        data->rowHeights[r] += kLabelGap + kLabelHeight + (2 * kGridPaddingY);
    }

    // compute cumulative x/y offsets for each column/row
    data->colX.SetSize(data->cols);
    int x = kOuterPadding;
    for (int c = 0; c < data->cols; c++) {
        data->colX[c] = x;
        x += data->colWidths[c];
    }

    data->rowY.SetSize(data->rows);
    int y = kInfoBarHeight + kOuterPadding; // start below info bar
    for (int r = 0; r < data->rows; r++) {
        data->rowY[r] = y;
        y += data->rowHeights[r];
    }

    // window sized to fit content + outer padding + info bar at top
    data->winW = x + kOuterPadding;
    data->winH = y + kOuterPadding;
}

// Get the bounding rect for thumbnail at index i (in client coords)
static RECT GetThumbRect(ScreenshotOverlayData* data, int idx) {
    int col = idx % data->cols;
    int row = idx / data->cols;
    auto& cs = data->captures[idx];

    int cellX = data->colX[col];
    int cellY = data->rowY[row];
    int cellW = data->colWidths[col];
    // thumb area height = rowHeight - kLabelGap - kLabelHeight - 2*kGridPaddingY + 2*kGridPaddingY
    // simplifies to: rowHeight - kLabelGap - kLabelHeight
    int thumbAreaH = data->rowHeights[row] - kLabelGap - kLabelHeight;

    // center thumbnail in cell
    int tx = cellX + (cellW - cs.thumbW) / 2;
    int ty = cellY + kGridPaddingY + (thumbAreaH - (2 * kGridPaddingY) - cs.thumbH) / 2;

    RECT rc;
    rc.left = tx;
    rc.top = ty;
    rc.right = tx + cs.thumbW;
    rc.bottom = ty + cs.thumbH;
    return rc;
}

// Hit test: returns index of thumbnail under point, or -1
static int HitTestThumb(ScreenshotOverlayData* data, int mx, int my) {
    int n = data->captures.Size();
    for (int i = 0; i < n; i++) {
        RECT rc = GetThumbRect(data, i);
        // expand hit area to include the label
        rc.bottom += kLabelGap + kLabelHeight;
        POINT pt = {mx, my};
        if (PtInRect(&rc, pt)) {
            return i;
        }
    }
    return -1;
}

static void SaveSelectedScreenshot(ScreenshotOverlayData* data) {
    if (data->selected < 0 || data->selected >= data->captures.Size()) {
        return;
    }
    TempStr dataDir = GetAppDataDirTemp();
    TempStr screenshotDir = path::JoinTemp(dataDir, "Screenshots");
    dir::CreateAll(screenshotDir);

    auto& cs = data->captures[data->selected];
    TempStr filePath = MakeUniquePathTemp(screenshotDir, cs.processName);
    if (!filePath) {
        return;
    }

    MainWindow* win = gWindows.Size() > 0 ? gWindows[0] : nullptr;
    if (!win) {
        return;
    }
    HBITMAP hbmpCopy = (HBITMAP)CopyImage(cs.bmp, IMAGE_BITMAP, cs.origW, cs.origH, 0);
    RenderedBitmap* rbmp = new RenderedBitmap(hbmpCopy, Size(cs.origW, cs.origH));
    ShowImageEditWindow(win, ImageEditMode::Save, filePath, rbmp);
    delete rbmp;
}

// Premultiply alpha for a pixel: component = component * alpha / 255
static inline DWORD PremultiplyPixel(BYTE r, BYTE g, BYTE b, BYTE a) {
    r = (BYTE)((r * a) / 255);
    g = (BYTE)((g * a) / 255);
    b = (BYTE)((b * a) / 255);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static void PaintOverlayLayered(HWND hwnd, ScreenshotOverlayData* data) {
    int w = data->winW;
    int h = data->winH;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    // Create 32-bit ARGB DIB section for per-pixel alpha
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD* pixels = nullptr;
    HBITMAP hbmDib = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pixels, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmDib);

    // Fill background: light blue at 93% opaque (7% transparent), premultiplied
    DWORD bgPixel = PremultiplyPixel(220, 230, 245, 237);
    for (int i = 0; i < w * h; i++) {
        pixels[i] = bgPixel;
    }

    // Draw thumbnails and labels using GDI onto the DIB
    // First draw into a temp compatible DC, then copy pixels with full alpha
    HDC hdcTemp = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmTemp = CreateCompatibleBitmap(hdcScreen, w, h);
    HGDIOBJ oldTemp = SelectObject(hdcTemp, hbmTemp);

    // select GUI font for text drawing
    HFONT guiFont = GetDefaultGuiFont();
    HGDIOBJ oldFont = SelectObject(hdcTemp, guiFont);

    // white background for the temp surface
    RECT fullRect = {0, 0, w, h};
    HBRUSH brWhite = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdcTemp, &fullRect, brWhite);
    DeleteObject(brWhite);

    int n = data->captures.Size();
    for (int i = 0; i < n; i++) {
        auto& cs = data->captures[i];
        RECT rc = GetThumbRect(data, i);

        // draw thumbnail onto temp DC
        HDC hdcSrc = CreateCompatibleDC(hdcTemp);
        HGDIOBJ prev = SelectObject(hdcSrc, cs.thumb);
        BitBlt(hdcTemp, rc.left, rc.top, cs.thumbW, cs.thumbH, hdcSrc, 0, 0, SRCCOPY);
        SelectObject(hdcSrc, prev);
        DeleteDC(hdcSrc);

        // draw label below thumbnail: process name on left, dimensions on right
        RECT labelRect;
        labelRect.left = rc.left + 4;
        labelRect.right = rc.right - 4;
        labelRect.top = rc.bottom + kLabelGap;
        labelRect.bottom = rc.bottom + kLabelGap + kLabelHeight;
        SetTextColor(hdcTemp, RGB(0, 0, 0));
        SetBkMode(hdcTemp, TRANSPARENT);
        TempWStr nameW = ToWStrTemp(cs.processName);
        DrawTextW(hdcTemp, nameW, -1, &labelRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        TempStr dimStr = str::FormatTemp("%dx%d", cs.origW, cs.origH);
        TempWStr dimW = ToWStrTemp(dimStr);
        DrawTextW(hdcTemp, dimW, -1, &labelRect, DT_RIGHT | DT_SINGLELINE);

        // draw selection border around thumbnail and label
        if (i == data->selected) {
            HPEN pen = CreatePen(PS_SOLID, kBorderThickness, RGB(0, 120, 215));
            HGDIOBJ oldPen = SelectObject(hdcTemp, pen);
            HGDIOBJ oldBrush = SelectObject(hdcTemp, GetStockObject(NULL_BRUSH));
            int b = (kBorderThickness / 2) + 1;
            int selLeft = std::min((int)rc.left, (int)labelRect.left) - b;
            int selTop = rc.top - b;
            int selRight = std::max((int)rc.right, (int)labelRect.right) + b;
            int selBottom = labelRect.bottom + b;
            Rectangle(hdcTemp, selLeft, selTop, selRight, selBottom);
            SelectObject(hdcTemp, oldBrush);
            SelectObject(hdcTemp, oldPen);
            DeleteObject(pen);
        }
    }

    // Draw 1px blue border around the window
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(0, 90, 180));
    HGDIOBJ oldPen2 = SelectObject(hdcTemp, borderPen);
    HGDIOBJ oldBrush2 = SelectObject(hdcTemp, GetStockObject(NULL_BRUSH));
    Rectangle(hdcTemp, 0, 0, w, h);
    SelectObject(hdcTemp, oldBrush2);
    SelectObject(hdcTemp, oldPen2);
    DeleteObject(borderPen);

    // Draw info bar at the top with solid blue background
    RECT infoRect;
    infoRect.left = 0;
    infoRect.right = w;
    infoRect.top = 0;
    infoRect.bottom = kInfoBarHeight;
    HBRUSH brBlue = CreateSolidBrush(RGB(0, 90, 180));
    FillRect(hdcTemp, &infoRect, brBlue);
    DeleteObject(brBlue);

    // Use bigger bold font for info text
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = (LONG)(ncm.lfMessageFont.lfHeight * 1.3);
    ncm.lfMessageFont.lfWeight = FW_BOLD;
    HFONT infoFont = CreateFontIndirectW(&ncm.lfMessageFont);
    HGDIOBJ prevInfoFont = SelectObject(hdcTemp, infoFont);

    SetTextColor(hdcTemp, RGB(255, 255, 255));
    SetBkMode(hdcTemp, TRANSPARENT);
    DrawTextW(hdcTemp, L"Select screenshot to save. ↑ ↓ to navigate. Enter to select. Esc to cancel", -1, &infoRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcTemp, prevInfoFont);
    DeleteObject(infoFont);

    // Read back temp bitmap pixels
    BITMAPINFO bmiTemp{};
    bmiTemp.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiTemp.bmiHeader.biWidth = w;
    bmiTemp.bmiHeader.biHeight = -h;
    bmiTemp.bmiHeader.biPlanes = 1;
    bmiTemp.bmiHeader.biBitCount = 32;
    bmiTemp.bmiHeader.biCompression = BI_RGB;
    DWORD* tempPixels = (DWORD*)malloc(w * h * 4);
    GetDIBits(hdcTemp, hbmTemp, 0, h, tempPixels, &bmiTemp, DIB_RGB_COLORS);

    // Copy thumbnail and label regions with full opacity into the DIB
    for (int i = 0; i < n; i++) {
        auto& cs = data->captures[i];
        RECT rc = GetThumbRect(data, i);

        // expand region to include border and label
        int b = (kBorderThickness / 2) + 2;
        int x0 = std::max(0, (int)rc.left - b);
        int y0 = std::max(0, (int)rc.top - b);
        int x1 = std::min(w, (int)rc.right + b);
        int y1 = std::min(h, (int)rc.bottom + 4 + kLabelHeight + b);

        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                DWORD px = tempPixels[y * w + x];
                BYTE r = (px >> 16) & 0xFF;
                BYTE g = (px >> 8) & 0xFF;
                BYTE bb = px & 0xFF;
                pixels[y * w + x] = (255u << 24) | (r << 16) | (g << 8) | bb;
            }
        }
    }

    // Copy info bar region at top with full opacity
    {
        for (int y = 0; y < kInfoBarHeight; y++) {
            for (int x = 0; x < w; x++) {
                DWORD px = tempPixels[y * w + x];
                BYTE r = (px >> 16) & 0xFF;
                BYTE g = (px >> 8) & 0xFF;
                BYTE bb = px & 0xFF;
                pixels[y * w + x] = (255u << 24) | (r << 16) | (g << 8) | bb;
            }
        }
    }

    // Copy 1px border around the window with full opacity
    for (int x = 0; x < w; x++) {
        // top edge
        DWORD px = tempPixels[x];
        pixels[x] = (255u << 24) | (px & 0xFFFFFF);
        // bottom edge
        px = tempPixels[(h - 1) * w + x];
        pixels[(h - 1) * w + x] = (255u << 24) | (px & 0xFFFFFF);
    }
    for (int y = 0; y < h; y++) {
        // left edge
        DWORD px = tempPixels[y * w];
        pixels[y * w] = (255u << 24) | (px & 0xFFFFFF);
        // right edge
        px = tempPixels[y * w + w - 1];
        pixels[y * w + w - 1] = (255u << 24) | (px & 0xFFFFFF);
    }

    free(tempPixels);
    SelectObject(hdcTemp, oldFont);
    SelectObject(hdcTemp, oldTemp);
    DeleteObject(hbmTemp);
    DeleteDC(hdcTemp);

    // Update layered window
    POINT ptSrc = {0, 0};
    SIZE szWnd = {w, h};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    RECT winRect;
    GetWindowRect(hwnd, &winRect);
    POINT ptDst = {winRect.left, winRect.top};
    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &szWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, oldBmp);
    DeleteObject(hbmDib);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

static LRESULT CALLBACK WndProcScreenshotOverlay(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ScreenshotOverlayData* data = (ScreenshotOverlayData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_ERASEBKGND:
            return TRUE;

        case WM_MOUSEACTIVATE:
            SetFocus(hwnd);
            return MA_ACTIVATE;

        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (!data) {
                break;
            }
            switch (wp) {
                case VK_ESCAPE:
                    DestroyWindow(hwnd);
                    return 0;
                case VK_LEFT:
                    if (data->selected > 0) {
                        data->selected--;
                        PaintOverlayLayered(hwnd, data);
                    }
                    return 0;
                case VK_RIGHT:
                    if (data->selected < data->captures.Size() - 1) {
                        data->selected++;
                        PaintOverlayLayered(hwnd, data);
                    }
                    return 0;
                case VK_UP:
                    if (data->selected >= data->cols) {
                        data->selected -= data->cols;
                        PaintOverlayLayered(hwnd, data);
                    }
                    return 0;
                case VK_DOWN:
                    if (data->selected + data->cols < data->captures.Size()) {
                        data->selected += data->cols;
                        PaintOverlayLayered(hwnd, data);
                    }
                    return 0;
                case VK_RETURN:
                    SaveSelectedScreenshot(data);
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;

        case WM_MOUSEMOVE:
            if (data) {
                int mx = GET_X_LPARAM(lp);
                int my = GET_Y_LPARAM(lp);
                int hit = HitTestThumb(data, mx, my);
                if (hit >= 0 && hit != data->selected) {
                    data->selected = hit;
                    PaintOverlayLayered(hwnd, data);
                }
            }
            return 0;

        case WM_LBUTTONDOWN:
            if (data) {
                int mx = GET_X_LPARAM(lp);
                int my = GET_Y_LPARAM(lp);
                int hit = HitTestThumb(data, mx, my);
                if (hit >= 0) {
                    data->selected = hit;
                    SaveSelectedScreenshot(data);
                    DestroyWindow(hwnd);
                }
            }
            return 0;

        case WM_DESTROY:
            if (data) {
                FreeCapturedScreenshots(data);
                delete data;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterScreenshotOverlayClass() {
    if (gScreenshotClassRegistered) {
        return;
    }
    WNDCLASSEX wcex{};
    FillWndClassEx(wcex, kScreenshotOverlayClassName, WndProcScreenshotOverlay);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassEx(&wcex);
    gScreenshotClassRegistered = true;
}

void TakeScreenshots() {
    RegisterScreenshotOverlayClass();

    // Remember the foreground window and capture screenshots before creating
    // our overlay window to avoid disturbing what's on screen
    HWND hwndForeground = GetForegroundWindow();

    auto* data = new ScreenshotOverlayData();
    CaptureAllScreenshots(data, nullptr);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    DWORD style = WS_POPUP;
    HWND hwnd = CreateWindowExW(exStyle, kScreenshotOverlayClassName, nullptr, style, 0, 0, 1, 1, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        logf("Screenshot: failed to create overlay window\n");
        FreeCapturedScreenshots(data);
        delete data;
        return;
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)data);

    if (data->captures.IsEmpty()) {
        logf("Screenshot: no windows captured\n");
        DestroyWindow(hwnd);
        return;
    }

    // Select the previously active window's thumbnail, or first if not found
    data->selected = 0;
    if (hwndForeground) {
        int n = data->captures.Size();
        for (int i = 0; i < n; i++) {
            if (data->captures[i].srcHwnd == hwndForeground) {
                data->selected = i;
                break;
            }
        }
    }

    ComputeLayout(data);

    // Clamp to screen size
    if (data->winW > screenW) {
        data->winW = screenW;
    }
    if (data->winH > screenH) {
        data->winH = screenH;
    }

    // Center on screen
    int x = (screenW - data->winW) / 2;
    int y = (screenH - data->winH) / 2;
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, data->winW, data->winH, 0);

    // Paint the layered content
    PaintOverlayLayered(hwnd, data);

    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}

static bool IsOtherSumatraProcessRunning() {
    DWORD myPid = GetCurrentProcessId();
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, L"SUMATRA_PDF_FRAME", nullptr)) != nullptr) {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != myPid) {
            return true;
        }
    }
    return false;
}

// find custom shortcut key string for CmdScreenshot, or nullptr if none
static const char* FindScreenshotShortcut() {
    auto curr = gFirstCustomCommand;
    while (curr) {
        if (curr->origId == CmdScreenshot && !str::IsEmptyOrWhiteSpace(curr->key)) {
            return curr->key;
        }
        curr = curr->next;
    }
    return nullptr;
}

static UINT AccelFVirtToHotkeyMod(BYTE fVirt) {
    UINT mod = 0;
    if (fVirt & FALT) {
        mod |= MOD_ALT;
    }
    if (fVirt & FCONTROL) {
        mod |= MOD_CONTROL;
    }
    if (fVirt & FSHIFT) {
        mod |= MOD_SHIFT;
    }
    return mod;
}

void RegisterScreenshotHotkey(HWND hwnd) {
    const char* shortcut = FindScreenshotShortcut();
    UINT mod = 0;
    UINT vk = VK_SNAPSHOT;
    if (shortcut) {
        ACCEL accel{};
        if (ParseShortcutString(shortcut, accel)) {
            mod = AccelFVirtToHotkeyMod(accel.fVirt);
            vk = accel.key;
        }
    }
    BOOL ok = RegisterHotKey(hwnd, kScreenshotHotkeyId, mod, vk);
    if (!ok && !IsOtherSumatraProcessRunning()) {
        if (shortcut) {
            MaybeDelayedWarningNotification("Couldn't register '%s' global hotkey for taking screenshots", shortcut);
        } else {
            MaybeDelayedWarningNotification("Couldn't register PrtScr global hotkey for taking screenshots");
        }
    }
}

void UnregisterScreenshotHotkey(HWND hwnd) {
    UnregisterHotKey(hwnd, kScreenshotHotkeyId);
}
