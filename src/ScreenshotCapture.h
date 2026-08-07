/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Screenshot capture and the picker overlay, shared between apps. The host
// fills gScreenshotHost in at startup; every member is optional and the
// capture works without any of them, minus the behavior each one describes.
struct ScreenshotHost {
    // true if hwnd is one of the host's own top-level windows. Floating UI the
    // host owns (a find bar, say) looks like a tool window and would otherwise
    // be dropped, so windows owned by one of these are captured too.
    bool (*IsAppFrame)(HWND) = nullptr;
    // directory screenshots are saved into, created if it isn't there yet.
    // Nothing is saved when this is missing.
    TempStr (*GetSaveDirTemp)() = nullptr;
    // window the image editor is centred on and owned by
    HWND (*GetOwnerHwnd)() = nullptr;
};

extern ScreenshotHost gScreenshotHost;

void TakeScreenshots();
