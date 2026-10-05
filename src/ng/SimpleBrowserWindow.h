/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

// orig's WebViewResourceProvider: what answers a request to the window's
// virtual host. `getData` returns a borrowed buffer (the provider owns it).
struct SimpleBrowserResourceProvider {
    void* ctx = nullptr;
    Str (*getData)(void* ctx, Str path) = nullptr;
};

struct SimpleBrowserCreateArgs {
    MainWindow* win = nullptr;
    Str title;
    Str url;
    // requests to this host go to `resourceProvider` instead of the network
    Str resourceUriPrefix;
    SimpleBrowserResourceProvider resourceProvider;
    // orig's webView->events.jsNotify: window.__sumatra__.notify() in the page
    void (*jsNotify)(void* ctx, Str method, Str paramsJson) = nullptr;
    // where the platform can give it a window of its own: the window
    // rectangle in screen pixels (orig's args.pos) and orig's onPosChanged
    Rect pos;
    void (*onPosChanged)(Rect outer) = nullptr;
};

// ng: orig's SimpleBrowserWindow is an overlapped win32 window hosting a
// WebView2 with a Back / Forward / url row above it. Where the platform can
// have a second window (gui/ToolWindow.h) it is one here too; elsewhere it is
// a full-size overlay inside the main window, with the same row and the same
// behaviour (external links go to the user's browser). It is what F1 opens.
void SimpleBrowserWindowShow(const SimpleBrowserCreateArgs& args);
void SimpleBrowserWindowClose();
// `win` is closing: the overlay or the window goes with it
void SimpleBrowserWindowCloseFor(MainWindow* win);
// the overlay in the frame
bool IsSimpleBrowserWindowVisible();
// the overlay, or the window of its own
bool IsSimpleBrowserWindowOpen();
gpui::El* SimpleBrowserWindowBuild(MainWindow* win, gpui::Ctx* cx);
