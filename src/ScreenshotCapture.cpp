/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Screenshot capture and the picker overlay. Shared between SumatraPDF and
// other apps: everything host-specific is reached through the ScreenshotHost
// hooks in ScreenshotCapture.h, which each app fills in at startup.

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/File.h"
#include "base/WinDynCalls.h" // DWM corner prefs shim for mingw-w64 < 12
#include <dwmapi.h>
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/Timer.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"

#include "ImageSaveCropResize.h"
#include "ScreenshotCapture.h"

ScreenshotHost gScreenshotHost;

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
// WM_TIMER id used to clear a transient info-bar status message
constexpr UINT_PTR kStatusTimerId = 1;
constexpr UINT kStatusMsgTimeoutMs = 1500;

struct CapturedScreenshot {
    HBITMAP bmp = nullptr;   // full-size capture
    HBITMAP thumb = nullptr; // scaled thumbnail
    HWND srcHwnd = nullptr;  // source window (nullptr for desktop)
    int origW = 0;
    int origH = 0;
    int thumbW = 0;
    int thumbH = 0;
    Str processName; // for file naming
};

struct ScreenshotOverlayData {
    Vec<CapturedScreenshot> captures;
    HWND hwndRestore = nullptr;
    int selected = 0; // index of currently selected (hovered/arrow-keyed)
    int cols = 0;
    int rows = 0;
    Vec<int> colWidths;  // width of each column
    Vec<int> rowHeights; // height of each row
    Vec<int> colX;       // x start of each column (cumulative)
    Vec<int> rowY;       // y start of each row (cumulative)
    int winW = 0;
    int winH = 0;
    // transient message shown in the info bar (e.g. "Copied to clipboard"),
    // cleared by a one-shot timer
    Str statusMsg;
    UINT_PTR statusTimer = 0;
};

// true if hwnd is a floating window of ours owned by one of our frames (e.g. the
// floating find bar, a WS_POPUP | WS_EX_TOOLWINDOW window owned by hwndFrame).
// Such windows are part of our UI and worth capturing even though the generic
// filters below would normally drop tool / owned-caption-less windows.
static bool IsOwnedByAppFrame(HWND hwnd) {
    HWND owner = GetWindow(hwnd, GW_OWNER);
    if (!owner) {
        return false;
    }
    return gScreenshotHost.IsAppFrame && gScreenshotHost.IsAppFrame(owner);
}

static bool ShouldCaptureWindow(HWND hwnd, HWND overlayHwnd) {
    if (hwnd == overlayHwnd) {
        return false;
    }
    if (!HwndIsVisible(hwnd)) {
        return false;
    }
    if (hwnd == GetDesktopWindow()) {
        return false;
    }
    WCHAR className[256];
    bool isMenu = false;
    if (GetClassNameW(hwnd, className, 256) > 0) {
        if (wstr::Eq(className, WStrL(L"Progman")) || wstr::Eq(className, WStrL(L"WorkerW"))) {
            return false;
        }
        // #32768 is the Win32 menu window class (context menus, popups etc.)
        isMenu = wstr::Eq(className, WStrL(L"#32768"));
    }
    // our own floating UI (e.g. the find bar) is a tool window owned by a frame;
    // capture it even though the tool/owned-window filters below would drop it
    bool ownedByApp = IsOwnedByAppFrame(hwnd);
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!isMenu && !ownedByApp && (exStyle & WS_EX_TOOLWINDOW)) {
        return false;
    }
    HWND hwndOwner = GetWindow(hwnd, GW_OWNER);
    if (!isMenu && !ownedByApp && hwndOwner != nullptr && !(exStyle & WS_EX_APPWINDOW)) {
        // owned window without WS_EX_APPWINDOW: accept if it has a caption
        // (modal dialogs like config windows), reject otherwise
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        if ((style & WS_CAPTION) != WS_CAPTION) {
            return false;
        }
    }
    BOOL isCloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked)))) {
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
    // skip thin slivers of our own floating UI (e.g. the overlay scrollbar) -
    // require at least 20x20px for owned-by-app windows
    if (ownedByApp && (w < 20 || h < 20)) {
        return false;
    }
    if (HwndGetTextLen(hwnd) == 0) {
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

static void InitBitmapInfo32(BITMAPINFO& bmi, int w, int h) {
    bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
}

// check if a specific edge row/column is predominantly near-black
// uses stride to handle sub-regions within a larger pixel buffer
// edge: 0=left, 1=right, 2=top, 3=bottom
static bool IsEdgeMostlyBlackEx(DWORD* pixels, int stride, int x0, int y0, int w, int h, int edge) {
    int blackCount = 0;
    int total = 0;
    switch (edge) {
        case 0: // left column
            for (int y = y0; y < y0 + h; y++) {
                if (IsNearBlack(RgbToColor(pixels[(y * stride) + x0] & 0x00FFFFFF))) {
                    blackCount++;
                }
                total++;
            }
            break;
        case 1: // right column
            for (int y = y0; y < y0 + h; y++) {
                if (IsNearBlack(RgbToColor(pixels[(y * stride) + x0 + w - 1] & 0x00FFFFFF))) {
                    blackCount++;
                }
                total++;
            }
            break;
        case 2: // top row
            for (int x = x0; x < x0 + w; x++) {
                if (IsNearBlack(RgbToColor(pixels[(y0 * stride) + x] & 0x00FFFFFF))) {
                    blackCount++;
                }
                total++;
            }
            break;
        case 3: // bottom row
            for (int x = x0; x < x0 + w; x++) {
                if (IsNearBlack(RgbToColor(pixels[((y0 + h - 1) * stride) + x] & 0x00FFFFFF))) {
                    blackCount++;
                }
                total++;
            }
            break;
    }
    return total > 0 && blackCount > (total * 7 / 10);
}

// trim black border pixels left by PrintWindow on DWM-composited windows
// returns a new cropped bitmap (caller owns it), or the original if no trimming needed
static HBITMAP TrimBlackBorders(HBITMAP hbm, int* pW, int* pH) {
    int origW = *pW, origH = *pH;

    BITMAPINFO bmi;
    InitBitmapInfo32(bmi, origW, origH);

    HDC hdc = GetDC(nullptr);
    DWORD* pixels = (DWORD*)malloc((size_t)origW * origH * 4);
    if (!pixels) {
        ReleaseDC(nullptr, hdc);
        return hbm;
    }
    GetDIBits(hdc, hbm, 0, origH, pixels, &bmi, DIB_RGB_COLORS);

    // detect trim amounts by scanning edges; trim up to 2px per edge
    int trimLeft = 0, trimRight = 0, trimTop = 0, trimBottom = 0;
    int x0 = 0, y0 = 0, w = origW, h = origH;

    for (int i = 0; i < 2 && w > 0; i++) {
        if (IsEdgeMostlyBlackEx(pixels, origW, x0, y0, w, h, 0)) {
            trimLeft++;
            x0++;
            w--;
        } else {
            break;
        }
    }
    for (int i = 0; i < 2 && w > 0; i++) {
        if (IsEdgeMostlyBlackEx(pixels, origW, x0, y0, w, h, 1)) {
            trimRight++;
            w--;
        } else {
            break;
        }
    }
    for (int i = 0; i < 2 && h > 0; i++) {
        if (IsEdgeMostlyBlackEx(pixels, origW, x0, y0, w, h, 2)) {
            trimTop++;
            y0++;
            h--;
        } else {
            break;
        }
    }
    for (int i = 0; i < 2 && h > 0; i++) {
        if (IsEdgeMostlyBlackEx(pixels, origW, x0, y0, w, h, 3)) {
            trimBottom++;
            h--;
        } else {
            break;
        }
    }

    free(pixels);

    if (trimLeft == 0 && trimRight == 0 && trimTop == 0 && trimBottom == 0) {
        ReleaseDC(nullptr, hdc);
        return hbm;
    }

    int newW = origW - trimLeft - trimRight;
    int newH = origH - trimTop - trimBottom;
    if (newW <= 0 || newH <= 0) {
        ReleaseDC(nullptr, hdc);
        return hbm;
    }

    HDC hdcSrc = CreateCompatibleDC(hdc);
    HDC hdcDst = CreateCompatibleDC(hdc);
    HBITMAP hbmNew = CreateCompatibleBitmap(hdc, newW, newH);
    SelectObject(hdcSrc, hbm);
    SelectObject(hdcDst, hbmNew);
    BitBlt(hdcDst, 0, 0, newW, newH, hdcSrc, trimLeft, trimTop, SRCCOPY);
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    ReleaseDC(nullptr, hdc);
    DeleteObject(hbm);

    *pW = newW;
    *pH = newH;
    return hbmNew;
}

// replace black corner pixels from PrintWindow with bgColor using a rounded corner mask
static void FixRoundedCorners(HBITMAP hbm, int w, int h, Color bgColor, int radius) {
    BITMAPINFO bmi;
    InitBitmapInfo32(bmi, w, h);

    HDC hdc = GetDC(nullptr);
    // cast before multiply so the product cannot overflow int→size_t
    DWORD* pixels = (DWORD*)malloc((size_t)w * (size_t)h * 4u);
    if (!pixels) {
        ReleaseDC(nullptr, hdc);
        return;
    }
    GetDIBits(hdc, hbm, 0, h, pixels, &bmi, DIB_RGB_COLORS);

    DWORD bg = (GetRValue(bgColor) << 16) | (GetGValue(bgColor) << 8) | GetBValue(bgColor);

    // only replace pixels that are actually near-black (avoids clobbering legitimate content)
    auto replaceIfBlack = [&](int idx) {
        if (IsNearBlack(RgbToColor(pixels[idx] & 0x00FFFFFF))) {
            pixels[idx] = bg;
        }
    };

    // fix the four corners
    for (int cy = 0; cy < radius; cy++) {
        for (int cx = 0; cx < radius; cx++) {
            // distance from corner center to pixel
            int dx = radius - 1 - cx;
            int dy = radius - 1 - cy;
            if ((dx * dx) + (dy * dy) > radius * radius) {
                // outside the rounded corner — replace with background if black
                replaceIfBlack((cy * w) + cx);                     // top-left
                replaceIfBlack((cy * w) + (w - 1 - cx));           // top-right
                replaceIfBlack(((h - 1 - cy) * w) + cx);           // bottom-left
                replaceIfBlack(((h - 1 - cy) * w) + (w - 1 - cx)); // bottom-right
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
    HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &visibleRect, sizeof(visibleRect));
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

    // trim black border pixels left by PrintWindow on DWM-composited windows
    hbmFull = TrimBlackBorders(hbmFull, &fullW, &fullH);

    // fix black corners from DWM rounded windows
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_DEFAULT;
    DwmGetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
    if (cornerPref == DWMWCP_DEFAULT || cornerPref == DWMWCP_ROUND || cornerPref == DWMWCP_ROUNDSMALL) {
        Color bgColor = GetSysColor(COLOR_WINDOW);
        // DWM corner radius is 8 logical pixels (4 for ROUNDSMALL), scale by DPI
        int baseRadius = (cornerPref == DWMWCP_ROUNDSMALL) ? 4 : 8;
        int dpi = DpiGetForHwnd(hwnd);
        int radius = MulDiv(baseRadius, dpi, 96);
        radius = std::max(radius, baseRadius);
        FixRoundedCorners(hbmFull, fullW, fullH, bgColor, radius);
    }

    *outW = fullW;
    *outH = fullH;
    return hbmFull;
}

static TempStr GetWindowProcessNameTemp(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return {};
    }
    AutoCloseHandle hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc.IsValid()) {
        return {};
    }
    WCHAR path[MAX_PATH]{};
    DWORD pathLen = MAX_PATH;
    if (!QueryFullProcessImageNameW(hProc, 0, path, &pathLen)) {
        return {};
    }
    TempStr fullPath = ToUtf8Temp(path);
    TempStr baseName = path::GetBaseNameTemp(fullPath);
    TempStr noExt = path::GetPathNoExtTemp(baseName);
    return noExt;
}

static TempStr MakeUniquePathTemp(Str dir, Str base) {
    TempStr name = fmt("%s.png", base);
    TempStr path = path::JoinTemp(dir, name);
    if (!file::Exists(Str(path))) {
        return path;
    }
    for (int i = 1; i < 10000; i++) {
        name = fmt("%s.%d.png", base, i);
        path = path::JoinTemp(dir, name);
        if (!file::Exists(Str(path))) {
            return path;
        }
    }
    return {};
}

// Create a scaled thumbnail of a bitmap (only downscale, never upscale)
static HBITMAP CreateThumbnail(HBITMAP src, int srcW, int srcH, int* outW, int* outH) {
    int tw, th;
    if (srcW <= kMaxThumbSize && srcH <= kMaxThumbSize) {
        // source fits within thumbnail size, use original size
        tw = srcW;
        th = srcH;
    } else if (srcW >= srcH) {
        tw = kMaxThumbSize;
        th = MulDiv(srcH, kMaxThumbSize, srcW);
    } else {
        th = kMaxThumbSize;
        tw = MulDiv(srcW, kMaxThumbSize, srcH);
    }
    tw = std::max(tw, 1);
    th = std::max(th, 1);

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
    VecReset(data->captures);
}

// one capture target; the result lands in cs (cs.bmp stays null on failure)
struct CaptureItem {
    HWND hwnd = nullptr; // nullptr: capture the desktop
    bool done = false;
    CapturedScreenshot cs;
};

struct CaptureCtx {
    Vec<CaptureItem>* items = nullptr;
    AtomicInt nextIdx;
    AtomicInt captureMs; // aggregate PrintWindow/BitBlt time across items
    AtomicInt thumbMs;   // aggregate thumbnail scaling time across items
};

static void CaptureOneItem(CaptureCtx* ctx, CaptureItem* item) {
    auto t = TimeGet();
    CapturedScreenshot& cs = item->cs;
    if (!item->hwnd) {
        HBITMAP hbm = CaptureDesktop();
        if (!hbm) {
            return;
        }
        cs.bmp = hbm;
        cs.origW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        cs.origH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        cs.processName = str::Dup(StrL("desktop"));
    } else {
        int w = 0, h = 0;
        HBITMAP hbm = CaptureWindowBmp(item->hwnd, &w, &h);
        if (!hbm) {
            return;
        }
        cs.bmp = hbm;
        cs.srcHwnd = item->hwnd;
        cs.origW = w;
        cs.origH = h;
        TempStr procName = GetWindowProcessNameTemp(item->hwnd);
        cs.processName = procName ? str::Dup(procName) : str::Dup(StrL("unknown"));
    }
    AtomicIntAdd(&ctx->captureMs, (int)TimeSinceInMs(t));
    t = TimeGet();
    cs.thumb = CreateThumbnail(cs.bmp, cs.origW, cs.origH, &cs.thumbW, &cs.thumbH);
    AtomicIntAdd(&ctx->thumbMs, (int)TimeSinceInMs(t));
}

static void CaptureWorker(CaptureCtx* ctx) {
    int n = len(*ctx->items);
    for (;;) {
        int i = AtomicIntInc(&ctx->nextIdx) - 1;
        if (i >= n) {
            return;
        }
        CaptureItem* item = &(*ctx->items)[i];
        if (!item->done) {
            CaptureOneItem(ctx, item);
        }
    }
}

struct EnumCaptureCtx {
    Vec<CaptureItem>* items;
    HWND overlayHwnd;
};

static BOOL CALLBACK EnumCaptureWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumCaptureCtx* ctx = (EnumCaptureCtx*)lParam;
    if (!ShouldCaptureWindow(hwnd, ctx->overlayHwnd)) {
        return TRUE;
    }
    CaptureItem item;
    item.hwnd = hwnd;
    VecAppend(*ctx->items, item);
    return TRUE;
}

// capturing + thumbnailing each window is independent and dominated by
// PrintWindow / StretchBlt, so run the items on a small thread pool
static void CaptureAllScreenshots(ScreenshotOverlayData* data, HWND overlayHwnd) {
    auto timeStart = TimeGet();

    // collect capture targets; item 0 is the desktop
    Vec<CaptureItem> items;
    {
        CaptureItem item;
        VecAppend(items, item);
    }
    EnumCaptureCtx ectx;
    ectx.items = &items;
    ectx.overlayHwnd = overlayHwnd;
    EnumWindows(EnumCaptureWindowsProc, (LPARAM)&ectx);
    int nItems = len(items);

    CaptureCtx ctx;
    ctx.items = &items;
    AtomicIntSet(&ctx.nextIdx, 0);
    AtomicIntSet(&ctx.captureMs, 0);
    AtomicIntSet(&ctx.thumbMs, 0);

    // capture our own windows on this thread first: PrintWindow can message
    // the window's owning thread (us), which would deadlock while we're
    // blocked waiting for the pool below
    DWORD myProcId = GetCurrentProcessId();
    for (int i = 0; i < nItems; i++) {
        CaptureItem& item = items[i];
        if (!item.hwnd) {
            continue;
        }
        DWORD procId = 0;
        GetWindowThreadProcessId(item.hwnd, &procId);
        if (procId == myProcId) {
            CaptureOneItem(&ctx, &item);
            item.done = true;
        }
    }

    // measured (8-window desktop): 1 thread 186 ms, 2: 140, 3: 125, 4: 130,
    // 6: 134, 8: 136. PrintWindow serializes inside DWM, so per-window
    // captures slow each other down and gains saturate at ~3 threads
    int numThreads = std::min(CpuCoreCount() - 2, 4);
    numThreads = std::min(numThreads, nItems);
    if (numThreads <= 1) {
        CaptureWorker(&ctx);
    } else {
        Vec<ThreadHandle> threads;
        for (int t = 0; t < numThreads; t++) {
            auto fn = MkFunc0(CaptureWorker, &ctx);
            VecAppend(threads, StartThread(fn, StrL("ScreenshotCapture")));
        }
        for (ThreadHandle h : threads) {
            WaitForSingleObject(h, INFINITE);
            SafeCloseThreadHandle(&h);
        }
    }

    // keep successful captures, in enumeration order (desktop first)
    for (int i = 0; i < nItems; i++) {
        if (items[i].cs.bmp) {
            VecAppend(data->captures, items[i].cs);
        }
    }

    logf("CaptureAllScreenshots: %d windows in %.1f ms (%d threads; capture %d ms, thumbs %d ms aggregate)\n",
         len(data->captures), TimeSinceInMs(timeStart), numThreads, AtomicIntGet(&ctx.captureMs),
         AtomicIntGet(&ctx.thumbMs));
}

// Compute grid layout and overlay window size
static void ComputeLayout(ScreenshotOverlayData* data) {
    int n = len(data->captures);
    if (n == 0) {
        return;
    }

    data->cols = (int)ceil(sqrt((double)n));
    data->rows = (n + data->cols - 1) / data->cols;

    // compute per-column widths (max thumb width in that column + 2*kGridPaddingX)
    VecResize(data->colWidths, data->cols);
    for (int c = 0; c < data->cols; c++) {
        data->colWidths[c] = 0;
    }
    for (int i = 0; i < n; i++) {
        int col = i % data->cols;
        int tw = data->captures[i].thumbW;
        data->colWidths[col] = std::max(tw, data->colWidths[col]);
    }
    for (int c = 0; c < data->cols; c++) {
        data->colWidths[c] += 2 * kGridPaddingX;
    }

    // compute per-row heights (max thumb height in that row + label + 2*kGridPaddingY)
    VecResize(data->rowHeights, data->rows);
    for (int r = 0; r < data->rows; r++) {
        data->rowHeights[r] = 0;
    }
    for (int i = 0; i < n; i++) {
        int row = i / data->cols;
        int th = data->captures[i].thumbH;
        data->rowHeights[row] = std::max(th, data->rowHeights[row]);
    }
    for (int r = 0; r < data->rows; r++) {
        data->rowHeights[r] += kLabelGap + kLabelHeight + (2 * kGridPaddingY);
    }

    // compute cumulative x/y offsets for each column/row
    VecResize(data->colX, data->cols);
    int x = kOuterPadding;
    for (int c = 0; c < data->cols; c++) {
        data->colX[c] = x;
        x += data->colWidths[c];
    }

    VecResize(data->rowY, data->rows);
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
static Rect GetThumbRect(ScreenshotOverlayData* data, int idx) {
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
    int tx = cellX + ((cellW - cs.thumbW) / 2);
    int ty = cellY + kGridPaddingY + ((thumbAreaH - (2 * kGridPaddingY) - cs.thumbH) / 2);

    return {tx, ty, cs.thumbW, cs.thumbH};
}

// Hit test: returns index of thumbnail under point, or -1
static int HitTestThumb(ScreenshotOverlayData* data, int mx, int my) {
    int n = len(data->captures);
    for (int i = 0; i < n; i++) {
        Rect rc = GetThumbRect(data, i);
        // expand hit area to include the label
        rc.dy += kLabelGap + kLabelHeight;
        if (rc.Contains({mx, my})) {
            return i;
        }
    }
    return -1;
}

static void SaveSelectedScreenshot(ScreenshotOverlayData* data) {
    if (data->selected < 0 || data->selected >= len(data->captures)) {
        return;
    }
    TempStr screenshotDir = gScreenshotHost.GetSaveDirTemp ? gScreenshotHost.GetSaveDirTemp() : TempStr();
    if (len(screenshotDir) == 0) {
        return;
    }
    dir::CreateAll(screenshotDir);

    auto& cs = data->captures[data->selected];
    TempStr filePath = MakeUniquePathTemp(screenshotDir, cs.processName);
    if (len(filePath) == 0) {
        return;
    }

    HWND owner = gScreenshotHost.GetOwnerHwnd ? gScreenshotHost.GetOwnerHwnd() : nullptr;
    HBITMAP hbmpCopy = (HBITMAP)CopyImage(cs.bmp, IMAGE_BITMAP, cs.origW, cs.origH, 0);
    RenderedBitmap* rbmp = new RenderedBitmap(hbmpCopy, Size(cs.origW, cs.origH));
    ShowImageEditWindow(owner, ImageEditMode::Crop, filePath, rbmp, false, {}, true);
    delete rbmp;
}

static void PaintOverlayLayered(HWND hwnd, ScreenshotOverlayData* data) {
    int w = data->winW;
    int h = data->winH;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) {
        return;
    }
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

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
    if (!hbmDib || !pixels) {
        if (hbmDib) {
            DeleteObject(hbmDib);
        }
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmDib);

    // Fill background: light blue at 93% opaque (7% transparent), premultiplied
    DWORD bgPixel = PremultiplyPixel(MkRgb(220, 230, 245), 237);
    for (int i = 0; i < w * h; i++) {
        pixels[i] = bgPixel;
    }

    // Draw thumbnails and labels using GDI onto the DIB
    // First draw into a temp compatible DC, then copy pixels with full alpha
    HDC hdcTemp = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmTemp = CreateCompatibleBitmap(hdcScreen, w, h);
    if (!hdcTemp || !hbmTemp) {
        if (hbmTemp) {
            DeleteObject(hbmTemp);
        }
        if (hdcTemp) {
            DeleteDC(hdcTemp);
        }
        SelectObject(hdcMem, oldBmp);
        DeleteObject(hbmDib);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }
    HGDIOBJ oldTemp = SelectObject(hdcTemp, hbmTemp);

    // select GUI font for text drawing
    PlatformFont* guiFont = GetDefaultGuiFont();
    HGDIOBJ oldFont = SelectObject(hdcTemp, guiFont->GetHFont());

    // white background for the temp surface
    Rect fullRect = {0, 0, w, h};
    HBRUSH brWhite = CreateSolidBrush(kColWhite);
    HdcFillRect(hdcTemp, fullRect, brWhite);
    DeleteObject(brWhite);

    int n = len(data->captures);
    for (int i = 0; i < n; i++) {
        auto& cs = data->captures[i];
        Rect rc = GetThumbRect(data, i);

        // draw thumbnail onto temp DC
        HDC hdcSrc = CreateCompatibleDC(hdcTemp);
        HGDIOBJ prev = SelectObject(hdcSrc, cs.thumb);
        BitBlt(hdcTemp, rc.x, rc.y, cs.thumbW, cs.thumbH, hdcSrc, 0, 0, SRCCOPY);
        SelectObject(hdcSrc, prev);
        DeleteDC(hdcSrc);

        // draw label below thumbnail: process name on left, dimensions on right
        Rect labelRect = {rc.x + 4, rc.y + rc.dy + kLabelGap, rc.dx - 8, kLabelHeight};
        SetTextColor(hdcTemp, kColBlack);
        SetBkMode(hdcTemp, TRANSPARENT);
        HdcDrawText(hdcTemp, cs.processName, labelRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        TempStr dimStr = fmt("%dx%d", cs.origW, cs.origH);
        HdcDrawText(hdcTemp, dimStr, labelRect, DT_RIGHT | DT_SINGLELINE);

        // draw selection border around thumbnail and label
        if (i == data->selected) {
            HPEN pen = CreatePen(PS_SOLID, kBorderThickness, MkRgb(0, 120, 215));
            HGDIOBJ oldPen = SelectObject(hdcTemp, pen);
            HGDIOBJ oldBrush = SelectObject(hdcTemp, GetStockObject(NULL_BRUSH));
            int b = (kBorderThickness / 2) + 1;
            int selLeft = std::min(rc.x, labelRect.x) - b;
            int selTop = rc.y - b;
            int selRight = std::max(rc.x + rc.dx, labelRect.x + labelRect.dx) + b;
            int selBottom = labelRect.y + labelRect.dy + b;
            Rectangle(hdcTemp, selLeft, selTop, selRight, selBottom);
            SelectObject(hdcTemp, oldBrush);
            SelectObject(hdcTemp, oldPen);
            DeleteObject(pen);
        }
    }

    // Draw 1px blue border around the window
    HPEN borderPen = CreatePen(PS_SOLID, 1, MkRgb(0, 90, 180));
    HGDIOBJ oldPen2 = SelectObject(hdcTemp, borderPen);
    HGDIOBJ oldBrush2 = SelectObject(hdcTemp, GetStockObject(NULL_BRUSH));
    Rectangle(hdcTemp, 0, 0, w, h);
    SelectObject(hdcTemp, oldBrush2);
    SelectObject(hdcTemp, oldPen2);
    DeleteObject(borderPen);

    // Draw info bar at the top with solid blue background
    Rect infoRect = {0, 0, w, kInfoBarHeight};
    HBRUSH brBlue = CreateSolidBrush(MkRgb(0, 90, 180));
    HdcFillRect(hdcTemp, infoRect, brBlue);
    DeleteObject(brBlue);

    // Use bigger bold font for info text
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    int infoFontSize = std::abs((int)(ncm.lfMessageFont.lfHeight * 1.3));
    PlatformFont* infoFont = GetUserGuiFontEx({}, infoFontSize, true, false);
    HGDIOBJ prevInfoFont = SelectObject(hdcTemp, infoFont->GetHFont());

    SetTextColor(hdcTemp, kColWhite);
    SetBkMode(hdcTemp, TRANSPARENT);
    // a transient status message (e.g. "Copied to clipboard") replaces the hint
    WStr infoText = WStrL(L"Select screenshot to save. ↑ ↓ to navigate. Enter to save. Ctrl+C to copy. Esc to cancel");
    if (data->statusMsg) {
        infoText = ToWStrTemp(data->statusMsg);
    }
    HdcDrawText(hdcTemp, infoText, infoRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcTemp, prevInfoFont);

    // Read back temp bitmap pixels
    BITMAPINFO bmiTemp{};
    bmiTemp.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiTemp.bmiHeader.biWidth = w;
    bmiTemp.bmiHeader.biHeight = -h;
    bmiTemp.bmiHeader.biPlanes = 1;
    bmiTemp.bmiHeader.biBitCount = 32;
    bmiTemp.bmiHeader.biCompression = BI_RGB;
    DWORD* tempPixels = (DWORD*)malloc((size_t)w * h * 4);
    if (!tempPixels) {
        SelectObject(hdcTemp, oldFont);
        SelectObject(hdcTemp, oldTemp);
        DeleteObject(hbmTemp);
        DeleteDC(hdcTemp);
        SelectObject(hdcMem, oldBmp);
        DeleteObject(hbmDib);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }
    GetDIBits(hdcTemp, hbmTemp, 0, h, tempPixels, &bmiTemp, DIB_RGB_COLORS);

    // Copy thumbnail and label regions with full opacity into the DIB
    for (int i = 0; i < n; i++) {
        auto& cs = data->captures[i];
        Rect rc = GetThumbRect(data, i);

        // expand region to include border and label
        int b = (kBorderThickness / 2) + 2;
        int x0 = std::max(0, rc.x - b);
        int y0 = std::max(0, rc.y - b);
        int x1 = std::min(w, rc.x + rc.dx + b);
        int y1 = std::min(h, rc.y + rc.dy + 4 + kLabelHeight + b);

        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                Color c = RgbToColor(tempPixels[(y * w) + x] & 0x00FFFFFF);
                pixels[(y * w) + x] = PremultiplyPixel(c, 255);
            }
        }
    }

    // Copy info bar region at top with full opacity
    {
        for (int y = 0; y < kInfoBarHeight; y++) {
            for (int x = 0; x < w; x++) {
                Color c = RgbToColor(tempPixels[(y * w) + x] & 0x00FFFFFF);
                pixels[(y * w) + x] = PremultiplyPixel(c, 255);
            }
        }
    }

    // Copy 1px border around the window with full opacity
    for (int x = 0; x < w; x++) {
        // top edge
        DWORD px = tempPixels[x];
        pixels[x] = (255u << 24) | (px & 0xFFFFFF);
        // bottom edge
        px = tempPixels[((h - 1) * w) + x];
        pixels[((h - 1) * w) + x] = (255u << 24) | (px & 0xFFFFFF);
    }
    for (int y = 0; y < h; y++) {
        // left edge
        DWORD px = tempPixels[(size_t)y * w];
        pixels[(size_t)y * w] = (255u << 24) | (px & 0xFFFFFF);
        // right edge
        px = tempPixels[(y * w) + w - 1];
        pixels[(y * w) + w - 1] = (255u << 24) | (px & 0xFFFFFF);
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

    Rect winRect = HwndWindowRect(hwnd);
    POINT ptDst = {winRect.x, winRect.y};
    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &szWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, oldBmp);
    DeleteObject(hbmDib);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

// Show a transient message in the info bar; a one-shot timer clears it again.
static void ShowOverlayStatus(HWND hwnd, ScreenshotOverlayData* data, Str msg) {
    str::ReplaceWithCopy(&data->statusMsg, msg);
    if (data->statusTimer) {
        KillTimer(hwnd, data->statusTimer);
    }
    data->statusTimer = SetTimer(hwnd, kStatusTimerId, kStatusMsgTimeoutMs, nullptr);
    PaintOverlayLayered(hwnd, data);
}

// Copy the currently selected screenshot to the clipboard (Ctrl+C in the picker).
static void CopySelectedScreenshotToClipboard(HWND hwnd, ScreenshotOverlayData* data) {
    if (data->selected < 0 || data->selected >= len(data->captures)) {
        return;
    }
    auto& cs = data->captures[data->selected];
    bool ok = cs.bmp && CopyImageToClipboard(cs.bmp, false);
    ShowOverlayStatus(hwnd, data, ok ? StrL("Copied to clipboard") : StrL("Copy failed"));
}

// Destroying the picker directly leaves its UI thread without keyboard focus.
// Return cancellation to the window that was active before the picker opened.
static void CancelScreenshotOverlay(HWND hwnd, ScreenshotOverlayData* data) {
    HWND hwndRestore = data->hwndRestore;
    DestroyWindow(hwnd);
    if (!hwndRestore || !IsWindow(hwndRestore)) {
        return;
    }
    SetForegroundWindow(hwndRestore);
    if (GetWindowThreadProcessId(hwndRestore, nullptr) == GetCurrentThreadId()) {
        SetFocus(hwndRestore);
    }
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
                    CancelScreenshotOverlay(hwnd, data);
                    return 0;
                case VK_LEFT:
                    if (data->selected > 0) {
                        data->selected--;
                        PaintOverlayLayered(hwnd, data);
                    }
                    return 0;
                case VK_RIGHT:
                    if (data->selected < len(data->captures) - 1) {
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
                    if (data->selected + data->cols < len(data->captures)) {
                        data->selected += data->cols;
                        PaintOverlayLayered(hwnd, data);
                    }
                    return 0;
                case 'C':
                    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                        CopySelectedScreenshotToClipboard(hwnd, data);
                        return 0;
                    }
                    break;
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

        case WM_TIMER:
            if (data && wp == kStatusTimerId) {
                KillTimer(hwnd, data->statusTimer);
                data->statusTimer = 0;
                str::Free(data->statusMsg);
                data->statusMsg = {};
                PaintOverlayLayered(hwnd, data);
                return 0;
            }
            break;

        case WM_DESTROY:
            if (data) {
                if (data->statusTimer) {
                    KillTimer(hwnd, data->statusTimer);
                }
                str::Free(data->statusMsg);
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
    wcex.hCursor = GetCachedCursor(IDC_ARROW);
    RegisterClassEx(&wcex);
    gScreenshotClassRegistered = true;
}

// Captures every eligible window plus the desktop, then puts up the picker
// overlay. Choosing one opens it in the image editor.
void TakeScreenshots(HWND hwndRestore) {
    RegisterScreenshotOverlayClass();

    // Menu commands supply their frame; a global hotkey restores the window
    // that was in the foreground when it fired.
    if (!hwndRestore) {
        hwndRestore = GetForegroundWindow();
    }

    auto* data = new ScreenshotOverlayData();
    data->hwndRestore = hwndRestore;
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

    if (len(data->captures) == 0) {
        logf("Screenshot: no windows captured\n");
        DestroyWindow(hwnd);
        return;
    }

    // Select the previously active window's thumbnail, or first if not found
    data->selected = 0;
    if (hwndRestore) {
        int n = len(data->captures);
        for (int i = 0; i < n; i++) {
            if (data->captures[i].srcHwnd == hwndRestore) {
                data->selected = i;
                break;
            }
        }
    }

    ComputeLayout(data);

    // Clamp to screen size
    data->winW = std::min(data->winW, screenW);
    data->winH = std::min(data->winH, screenH);

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
