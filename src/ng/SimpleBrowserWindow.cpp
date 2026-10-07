/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: orig's SimpleBrowserWindow.cpp. The Back / Forward / url row and the
// "a new-window request or an external link goes to the user's browser" rule
// are orig's; the window itself is a tool window or a gpui overlay over the
// main window (see the header), so orig's CreateCustom / WM_SIZE / DPI
// plumbing is gone.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Translations.h"
#include "Theme.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "gui/BrowserView.h"
#include "SimpleBrowserWindow.h"

#include "SumatraLog.h"

struct SimpleBrowser;

struct SimpleBrowserCb : BrowserViewCallback {
    SimpleBrowser* browser = nullptr;

    SimpleBrowserResourceProvider provider;

    bool OnBeforeNavigate(Str url, bool newWindow) override;
    void OnDocumentComplete(Str url) override;
    Str GetDataForUrl(Str path) override { return provider.getData ? provider.getData(provider.ctx, path) : Str(); }
    void OnLButtonDown() override {}
    void DownloadData(Str, Str) override {}
    void OnJsNotify(Str method, Str paramsJson) override {
        if (jsNotify) {
            jsNotify(nullptr, method, paramsJson);
        }
    }

    void (*jsNotify)(void* ctx, Str method, Str paramsJson) = nullptr;
};

struct SimpleBrowser {
    MainWindow* win = nullptr;
    bool visible = false;
    Str title;      // owned
    Str currentUrl; // owned
    SimpleBrowserCb cb;
    BrowserView* view = nullptr;
    // orig's window, where the platform can have one; null: an overlay
    ToolWindow* tw = nullptr;
    void (*onPosChanged)(Rect outer) = nullptr;
};

static SimpleBrowser gBrowser;

struct SimpleBrowserView {
    static void OnBack(SimpleBrowserView* self, gpui::Ctx* cx, const gpui::ClickEvent*);
    static void OnForward(SimpleBrowserView* self, gpui::Ctx* cx, const gpui::ClickEvent*);
    static void OnClose(SimpleBrowserView* self, gpui::Ctx* cx, const gpui::ClickEvent*);
};

static gp::Entity<SimpleBrowserView> gBrowserView;

// an absolute http(s)/mailto URL is "non-internal": it points outside the
// content we serve from our virtual host
static bool IsExternalUrl(Str url) {
    return str::StartsWithI(url, StrL("http://")) || str::StartsWithI(url, StrL("https://")) ||
           str::StartsWithI(url, StrL("mailto:"));
}

bool SimpleBrowserCb::OnBeforeNavigate(Str url, bool newWindow) {
    // a target="_blank" link arrives as a new-window request; open those in the
    // user's default browser instead of in this view
    if (newWindow) {
        SumatraLaunchBrowser(url);
        return false;
    }
    if (IsExternalUrl(url)) {
        str::ReplaceWithCopy(&gBrowser.currentUrl, url);
    }
    return true;
}

void SimpleBrowserCb::OnDocumentComplete(Str url) {
    if (len(url) > 0) {
        str::ReplaceWithCopy(&gBrowser.currentUrl, url);
    }
    logf("SimpleBrowserWindow: loaded '%s'\n", gBrowser.currentUrl);
    AppShellInvalidate(gBrowser.win);
}

bool IsSimpleBrowserWindowVisible() {
    return gBrowser.visible && !gBrowser.tw;
}

bool IsSimpleBrowserWindowOpen() {
    return gBrowser.visible;
}

// orig's manual window belongs to no main window: it stays when the one it
// was opened from closes, as long as another is left
static void SimpleBrowserToolOnOwnerClosed(MainWindow* newOwner) {
    gBrowser.win = newOwner;
    BrowserViewSetWindow(gBrowser.view, newOwner);
}

void SimpleBrowserWindowCloseFor(MainWindow* win) {
    if (gBrowser.win != win) {
        return;
    }
    if (gBrowser.visible && gBrowser.tw) {
        for (MainWindow* w : gWindows) {
            if (w != win && IsMainWindowValidAndNotClosing(w)) {
                ToolWindowSetOwner(gBrowser.tw, w);
                SimpleBrowserToolOnOwnerClosed(w);
                return;
            }
        }
    }
    SimpleBrowserWindowClose();
}

void SimpleBrowserWindowClose() {
    if (!gBrowser.visible) {
        return;
    }
    gBrowser.visible = false;
    if (gBrowser.view) {
        BrowserViewDelete(gBrowser.view);
        gBrowser.view = nullptr;
    }
    if (gBrowser.tw) {
        ToolWindowClose(gBrowser.tw);
        gBrowser.tw = nullptr;
    }
    str::FreePtr(&gBrowser.title);
    str::FreePtr(&gBrowser.currentUrl);
    AppShellInvalidate(gBrowser.win);
}

static void SimpleBrowserOpenToolWindow(Rect pos);

void SimpleBrowserWindowShow(const SimpleBrowserCreateArgs& args) {
    if (!args.win || len(args.url) == 0) {
        return;
    }
    if (!BrowserViewAvailable()) {
        logf("SimpleBrowserWindowShow: no embedded browser, opening '%s' externally\n", args.url);
        gp::OpenUrl(ToGpui(args.url));
        return;
    }
    // orig: an open window navigates and comes to the front
    bool sameHost = gBrowser.tw || gBrowser.win == args.win;
    if (gBrowser.visible && sameHost && gBrowser.view) {
        str::ReplaceWithCopy(&gBrowser.currentUrl, args.url);
        BrowserViewNavigate(gBrowser.view, args.url);
        ToolWindowActivate(gBrowser.tw);
        AppShellInvalidate(gBrowser.win);
        return;
    }
    SimpleBrowserWindowClose();
    gBrowser.win = args.win;
    gBrowser.title = str::Dup(len(args.title) > 0 ? args.title : StrL("Browser Window"));
    gBrowser.currentUrl = str::Dup(args.url);
    gBrowser.cb.browser = &gBrowser;
    gBrowser.cb.provider = args.resourceProvider;
    gBrowser.cb.jsNotify = args.jsNotify;
    // without a provider nothing is served from the virtual host, which is
    // what a plain browser window wants
    Str host = len(args.resourceUriPrefix) > 0 ? args.resourceUriPrefix : StrL("https://sumatrapdf.browser/");
    gBrowser.view = BrowserViewCreate(args.win, nullptr, &gBrowser.cb, host);
    if (!gBrowser.view) {
        gp::OpenUrl(ToGpui(args.url));
        return;
    }
    BrowserViewSetVisible(gBrowser.view, true);
    BrowserViewNavigate(gBrowser.view, args.url);
    gBrowser.visible = true;
    gBrowser.onPosChanged = args.onPosChanged;
    SimpleBrowserOpenToolWindow(args.pos);
    logf("SimpleBrowserWindowShow: '%s'\n", args.url);
    AppShellInvalidate(args.win);
}

void SimpleBrowserView::OnBack(SimpleBrowserView*, gp::Ctx* cx, const gp::ClickEvent*) {
    BrowserViewGoBack(gBrowser.view);
    gp::Notify(cx);
}

void SimpleBrowserView::OnForward(SimpleBrowserView*, gp::Ctx* cx, const gp::ClickEvent*) {
    BrowserViewGoForward(gBrowser.view);
    gp::Notify(cx);
}

void SimpleBrowserView::OnClose(SimpleBrowserView*, gp::Ctx* cx, const gp::ClickEvent*) {
    SimpleBrowserWindowClose();
    gp::Notify(cx);
}

// --- a window of its own (Windows) ------------------------------------------

// orig's nav row: Back | Forward | url, inset by kNavRowPadding
constexpr float kNavRowPadding = 6;
constexpr float kNavBtnGap = 4;
// orig's themed buttons: the text in the 12 px font plus 2 x 12 by 2 x 5
constexpr float kNavFontPx = 12;
constexpr float kNavBtnDy = 25;
constexpr float kNavBtnMinDx = 70;
constexpr float kNavBtnPadDx = 12;

static Str SimpleBrowserToolTitle() {
    return gBrowser.title;
}

static gp::El* SimpleBrowserToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gBrowser.visible || !gBrowser.tw) {
        return nullptr;
    }
    if (!gBrowserView.IsValid()) {
        gBrowserView = gp::EntityNewState<SimpleBrowserView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float fontScale = ToolWindowSetUiFontPx(cx, kNavFontPx);
    auto button = [&](gp::Str id, Str label, gp::Listener onClick, bool enabled) {
        return gpc::Button::New(cx, id)
            ->Label(ToGpui(label))
            ->Disabled(!enabled)
            ->OnClick(onClick)
            ->IntoEl()
            ->H(kNavBtnDy)
            ->MinW(kNavBtnMinDx)
            ->PadX(kNavBtnPadDx)
            ->Shrink0();
    };
    gp::El* row =
        gp::Div(cx->a)->FlexRow()->W(gp::kFill)->Shrink0()->ItemsCenter()->Gap(kNavBtnGap)->Pad(kNavRowPadding);
    row->Child(button(GStrL("browser-back"), Tr("Back"), gp::ListenTo(gBrowserView, &SimpleBrowserView::OnBack),
                      BrowserViewCanGoBack(gBrowser.view)));
    row->Child(button(GStrL("browser-forward"), Tr("Forward"),
                      gp::ListenTo(gBrowserView, &SimpleBrowserView::OnForward),
                      BrowserViewCanGoForward(gBrowser.view)));
    row->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(gp::TextEl(cx->a, GpuiDup(cx->a, gBrowser.currentUrl))
                                                           ->Font(kNavFontPx * fontScale)
                                                           ->Fg(th.foreground)
                                                           ->Truncate()));
    return gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->Child(row)->Child(
        gp::Div(cx->a)->Flex1()->W(gp::kFill)->MinH(0)->Child(BrowserViewBuild(gBrowser.view, cx)));
}

// orig's closeOnCtrlW and the manual's closeOnF1; no closeOnEsc (the page
// uses Esc for its own UI)
static bool SimpleBrowserToolOnKey(MainWindow*, gp::Ctx*, const gp::KeyEvent* ev) {
    bool isCtrlW = ev->vk == 'W' && ev->ctrl && !ev->alt;
    bool isF1 = ev->vk == VK_F1 && !ev->ctrl && !ev->shift && !ev->alt;
    if (!isCtrlW && !isF1) {
        return false;
    }
    SimpleBrowserWindowClose();
    return true;
}

static void SimpleBrowserToolOnClosed(MainWindow*) {
    gBrowser.tw = nullptr;
    SimpleBrowserWindowClose();
}

static void SimpleBrowserToolOnMoved(MainWindow*, Rect outer) {
    if (gBrowser.visible && gBrowser.tw && gBrowser.onPosChanged) {
        gBrowser.onPosChanged(outer);
    }
}

// ng: a WebView2 is made with a nested message loop, so not inside a frame
static void SimpleBrowserToolOnTick(MainWindow*, gp::Ctx* cx, int) {
    if (gBrowser.visible && gBrowser.tw) {
        BrowserViewCreatePendingIn(gBrowser.view, cx);
    }
}

static void SimpleBrowserOpenToolWindow(Rect pos) {
    if (gBrowser.tw || pos.IsEmpty() || !ToolWindowsAvailable()) {
        return;
    }
    // orig: WS_OVERLAPPEDWINDOW without an owner
    ToolWindowDesc desc;
    desc.name = "manual";
    desc.title = SimpleBrowserToolTitle;
    desc.frame = ToolWinFrame::Overlapped;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::TopLevel;
    desc.minClient = Size(320, 240);
    desc.build = SimpleBrowserToolBuild;
    desc.onKey = SimpleBrowserToolOnKey;
    desc.onClosed = SimpleBrowserToolOnClosed;
    desc.onOwnerClosed = SimpleBrowserToolOnOwnerClosed;
    desc.onMoved = SimpleBrowserToolOnMoved;
    desc.onTick = SimpleBrowserToolOnTick;
    gBrowser.tw = ToolWindowOpen(desc, gBrowser.win, pos);
    if (gBrowser.tw) {
        BrowserViewSetOwnHost(gBrowser.view);
    }
}

gp::El* SimpleBrowserWindowBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gBrowser.visible || gBrowser.tw || gBrowser.win != win) {
        return nullptr;
    }
    if (!gBrowserView.IsValid()) {
        gBrowserView = gp::EntityNewState<SimpleBrowserView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::WinSize ws = gp::WindowSize(cx->win);
    float pad = (float)DpiScale(6);
    float w = std::max(ws.dipW - 2 * pad, 100.f);
    float h = std::max(ws.dipH - 2 * pad, 100.f);

    gp::El* col = gp::Div(cx->a)
                      ->FlexCol()
                      ->Absolute()
                      ->Left(pad)
                      ->Top(pad)
                      ->W(w)
                      ->H(h)
                      ->Gap(4)
                      ->Pad(6)
                      ->Radius(6)
                      ->Bg(th.tokens.background)
                      ->Border(1, th.border);

    gp::El* row = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(4);
    row->Child(gpc::Button::New(cx, GStrL("browser-back"))
                   ->Label(ToGpui(Tr("Back")))
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gBrowserView, &SimpleBrowserView::OnBack))
                   ->IntoEl());
    row->Child(gpc::Button::New(cx, GStrL("browser-forward"))
                   ->Label(ToGpui(Tr("Forward")))
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gBrowserView, &SimpleBrowserView::OnForward))
                   ->IntoEl());
    row->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gp::TextEl(cx->a, GpuiDup(cx->a, gBrowser.currentUrl))->Font(12)->Fg(th.mutedFg)));
    row->Child(gpc::Button::New(cx, GStrL("browser-close"))
                   ->Label(GStrL("\xc3\x97"))
                   ->Ghost()
                   ->Compact()
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gBrowserView, &SimpleBrowserView::OnClose))
                   ->IntoEl());
    col->Child(row);
    col->Child(gp::Div(cx->a)->Flex1()->W(gp::kFill)->MinH(0)->Child(BrowserViewBuild(gBrowser.view, cx)));
    return col;
}
