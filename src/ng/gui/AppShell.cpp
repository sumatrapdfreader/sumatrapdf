/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the gpui half of the application shell. It draws what orig draws with
// win32: the menu bar (from Menu.cpp's MenuModel), the tab strip (orig's
// TabsCtrl look: title, close button, plus button), the canvas area and the
// notifications. It owns no application state - every frame reads MainWindow.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#include "base/UITask.h"
#include "base/File.h"
#if OS_WASM
#include <emscripten/emscripten.h>
#endif

#include "gui/UIModels.h"
#include "base/Pixmap.h"
#include "gui/Dpi.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "DisplayModel.h"
#include "Commands.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Translations.h"
#include "Theme.h"
#include "CommandAvailability.h"
#include "Menu.h"
#include "TipMarkup.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "gui/NativeFileDlg.h"
#include "gui/AppShell.h"
#include "gui/WasmBridge.h"
#include "gui/GpuiTheme.h"
#include "gui/DocCanvas.h"
#include "gui/TabsUI.h"
#include "gui/TabSwitcher.h"
#include "gui/BrowserView.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "CommandPalette.h"
#include "Selection.h"
#include "SelectionToolbar.h"
#include "LinkFollow.h"
#include "SearchAndDDE.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Toolbar.h"
#include "OverlayScrollbar.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "ReadAloud.h"
#include "SelectTextKeyboard.h"
#include "SumatraDialogs.h"
#include "PdfTools.h"
#include "gui/Sidebar.h"
#include "gui/NavFilesUI.h"
#include "gui/ToolWindow.h"
#include "HomePage.h"
#include "DocumentProperties.h"
#include "TabGroupsManage.h"
#include "Annotation.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "AnnotPlacement.h"
#include "AnnotTextPopup.h"
#include "RefHover.h"
#include "FormFields.h"
#include "ImageSaveCropResize.h"
#include "Screenshot.h"
#include "ExplorerQuickLook.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "gui/DialogWidgets.h"
#include "StressTesting.h"

// every command goes through one gpui action, with the command id as its
// argument. The bindings carry a key context so a menu row does not pick up
// another command's stroke through Kbd::ForAction
static const char* kShellKeyContext = "SumatraWindow";

static uint32_t ActSumatraCmd() {
    static uint32_t id = gp::ActionOf(GStrL("sumatra::Cmd"));
    return id;
}

static gp::App* gApp = nullptr;

struct ShellView {
    MainWindow* win = nullptr;
    // the caption's system menu button, as laid out on the last frame
    gp::Bounds sysMenuBtn{};
    gp::Entity<gpc::AppMenuBarState> menuBarState;
    gp::Entity<gp::PopupMenuState> menuPopupState;
    int menuPopupIdx = -1;
    // the menus underline their access keys: Alt was pressed and is still
    // down, or a menu it opened is still up
    bool menuCues = false;
    bool altDown = false;
    // orig's SC_KEYMENU: Alt or F10 pressed and released alone puts the
    // keyboard in the menu bar. menuKey is the one that is down with nothing
    // else pressed since, menuArmedIdx the title the keyboard is on (no popup
    // yet), menuBarTemp a hidden bar shown for as long as that lasts
    int menuKey = 0;
    int menuArmedIdx = -1;
    bool menuBarTemp = false;
    // the press that armed the bar (the menu button) is still being dispatched
    bool menuArmedByClick = false;
    // a dialog, the palette or the annotation list was built last frame: orig
    // gives each a window of its own, which has the keyboard while it is up
    bool overlayUp = false;
    bool dialogUp = false;
    // the sidebar splitter is being dragged past where it may go
    bool splitterRefused = false;
    // when the last tick ran (gp::TimeNow) and the sub-millisecond carry
    double lastTickTime = 0;
    double tickMsRest = 0;

    static gp::El* Render(ShellView* self, gp::Ctx* cx);
    static void OnCmd(ShellView* self, gp::Ctx* cx, const gp::ActionEvent* ev);
    static void OnSidebarResize(ShellView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
    static void OnSidebarResized(ShellView* self, gp::Ctx* cx, const gp::MouseUpEvent*);
    static void OnSystemMenu(ShellView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnTick(ShellView* self, gp::Ctx* cx, const gp::TickEvent* ev);
    static void OnMouseDown(ShellView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnKeyDown(ShellView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnKeyUp(ShellView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnCaptureKey(ShellView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnNotifClose(ShellView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t key);
    static void OnNotifLink(ShellView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t packed);
};

// ng: one per MainWindow (step 10b). Every listener is bound to this window's
// entity, so a second window dispatches into its own state.
struct ShellUI {
    gp::Entity<ShellView> view;
    gp::FocusHandle frameFocus;
};

static gp::Entity<ShellView> ShellViewOf(MainWindow* win) {
    return win->shell ? win->shell->view : gp::Entity<ShellView>{};
}

// --- the plumbing AppShell.h promises --------------------------------------

void AppShellInvalidate(MainWindow* win) {
    if (win && win->gpuiWin) {
        gp::AppInvalidate(win->gpuiWin);
    }
    // what a tool window shows is this window's state
    ToolWindowsInvalidateFor(win);
}

// gpui keeps the last kFrameTraceCap frames per window; the cursor makes each
// call report only what was drawn since the previous one
// --- the caption (orig's tabsInTitlebar) -------------------------------------

static gp::El* BuildMenuBar(ShellView* self, gp::Ctx* cx);

// gap in pixels between top of caption and tabs; this area allows dragging the window
constexpr int kCaptionTopPadding = 8;
// orig's kFrameBorderSize: with the caption the frame keeps a border around
// everything it lays out (not when maximized or fullscreen)
constexpr int kFrameBorderSize = 1;
// orig's capTabsRow1: the tab strip is 2 px taller in the one-row caption
constexpr int kCaptionTabBarDy = kTabBarDy + 2;
constexpr int kTabsButtonGapX = 32;
// size (DIP) of the min/max/restore/close caption glyphs
constexpr int kCaptionGlyphDip = 10;

enum CaptionButtons {
    CB_MINIMIZE = 0,
    CB_MAXIMIZE = 1,
    CB_RESTORE = 2,
    CB_CLOSE = 3,
};

// ng: gpui opens a window with or without its client-side title bar; where it
// has one that the frame's resize borders, snapping and the maximize button's
// snap layouts work with (Windows), the port draws orig's caption in it
static bool CanHaveTabsInTitlebar() {
#if OS_WIN
    return !gPluginMode;
#else
    return false;
#endif
}

void AppShellSetClientTitleBar(MainWindow* win, bool on) {
    if (win && win->gpuiWin) {
        win->gpuiWin->opts.clientTitleBar = on;
    }
}

void SetTabsInTitlebar(MainWindow* win, bool inTitleBar) {
    if (inTitleBar == win->tabsInTitlebar) {
        return;
    }
    win->tabsInTitlebar = inTitleBar;
    if (!win->isFullScreen && !win->presentation) {
        AppShellSetClientTitleBar(win, inTitleBar);
        AppShellFrameChanged(win);
    }
    AppShellInvalidate(win);
}

static bool ShowCaption(MainWindow* win) {
    return win->tabsInTitlebar && !win->presentation && !win->isFullScreen;
}

// orig's SyncCaptionLayout sizes: the window buttons span the padding above
// the tabs too, the app icon matches the tab band
static int CaptionWinBtnDy(MainWindow* win, bool twoRow) {
    if (twoRow) {
        return kMenuBarDy;
    }
    int pad = win->isMaximized ? 0 : kCaptionTopPadding;
    return pad + kTabBarDy + 2;
}

int AppShellFrameBorder(MainWindow* win) {
    return ShowCaption(win) && !win->isMaximized ? kFrameBorderSize : 0;
}

static int CaptionDy(MainWindow* win, bool twoRow) {
    int dy = CaptionWinBtnDy(win, twoRow);
    if (twoRow && TabsAreVisible(win)) {
        dy += kTabBarDy;
    }
    return dy;
}

bool AppShellCaptionRects(MainWindow* win, Rect* menuOut, Rect* tabsOut, int* dyOut) {
    if (!ShowCaption(win)) {
        return false;
    }
    bool twoRow = win->isMenuBarVisible && !gp::AppHasMenuBar();
    int winBtn = CaptionWinBtnDy(win, twoRow);
    bool hasTabs = TabsAreVisible(win);
    int border = AppShellFrameBorder(win);
    int dx = win->frameRc.dx - 2 * border;
    *dyOut = border + CaptionDy(win, twoRow);
    if (twoRow) {
        *menuOut = Rect{border + kMenuBarDy, border, dx - kMenuBarDy - 3 * winBtn, kMenuBarDy};
        *tabsOut = Rect{border, border + winBtn, dx, hasTabs ? kTabBarDy : 0};
        return true;
    }
    *menuOut = Rect{};
    *tabsOut =
        Rect{border + kTabBarDy, border + winBtn - kCaptionTabBarDy, win->tabsAvailDx, hasTabs ? kCaptionTabBarDy : 0};
    return true;
}

// orig's DrawCaptionButton for the min / max / restore / close buttons: the
// hot / pushed ground and a Windows 11 style glyph. `user` is the button.
static void PaintCaptionButton(gp::PaintCtx* ctx, gp::El* e, void* user) {
    int button = (int)(intptr_t)user;
    gp::Bounds b = e->Bounds();
    bool isClose = (button == CB_CLOSE);
    int id = e->clickId;
    bool isHot = ctx->hoverId == id;
    bool isPushed = isHot && ctx->activeId == id;
    bool isInactive = ctx->window && !ctx->window->active;
    Color bgc = ThemeControlBackgroundColor();

    if (isHot || isPushed) {
        gp::Rgba bgCol;
        if (isClose) {
            bgCol = gp::Rgba{196, 43, 28, (uint8_t)(isPushed ? 200 : 255)};
        } else {
            bgCol = ToGpui(isPushed ? AccentColor(bgc, 40) : AccentColor(bgc, 20));
        }
        gp::CanvasFillRect(ctx, b.x, b.y, b.w, b.h, bgCol);
    }

    Color iconCol;
    if (isInactive && !isHot) {
        iconCol = MkRgb(153, 153, 153);
    } else if (isClose && (isHot || isPushed)) {
        iconCol = kColWhite;
    } else {
        iconCol = ThemeWindowTextColor();
    }
    gp::Rgba col = ToGpui(iconCol);

    // ng: orig fills Segoe Fluent Icons outlines; these are the same shapes
    // from lines
    float sz = (float)kCaptionGlyphDip;
    float x = (float)(int)(b.x + (b.w - sz) / 2);
    float y = (float)(int)(b.y + (b.h - sz) / 2);
    switch (button) {
        case CB_MINIMIZE:
            gp::CanvasFillRect(ctx, x, y + (float)(int)(sz / 2), sz, 1, col);
            break;
        case CB_MAXIMIZE:
            gp::CanvasStrokeRound(ctx, x + 0.5f, y + 0.5f, sz - 1, sz - 1, 1, 1, col);
            break;
        case CB_RESTORE: {
            // the window in front, and the top / right of the one behind it
            float d = 2;
            gp::CanvasStrokeRound(ctx, x + 0.5f, y + d + 0.5f, sz - d - 1, sz - d - 1, 1, 1, col);
            gp::CanvasFillRect(ctx, x + d, y, sz - d - 1, 1, col);
            gp::CanvasFillRect(ctx, x + sz - 1, y + 1, 1, sz - d - 1, col);
            break;
        }
        case CB_CLOSE:
            gp::CanvasLine(ctx, x, y, x + sz, y + sz, 1, col);
            gp::CanvasLine(ctx, x + sz, y, x, y + sz, 1, col);
            break;
    }
}

static gp::El* CaptionButton(gp::Ctx* cx, int button, int clickId, int size) {
    gp::El* b = gp::Div(cx->a)->W((float)size)->H((float)size)->Shrink0()->Click(clickId);
    b->customPaint = &PaintCaptionButton;
    b->customUser = (void*)(intptr_t)button;
    return b;
}

#if OS_WIN
// the icon's pixels are premultiplied BGRA, which is what gpui takes
static gp::ImageLoadState AppIconLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imgOut) {
    static gp::RenderImage* img = nullptr;
    static bool tried = false;
    auto* px = (Pixmap*)user;
    if (!tried) {
        tried = true;
        if (px->stride == px->width * 4) {
            img = gp::RenderImageFromBgra(pa, px->data, px->width, px->height);
        }
    }
    *imgOut = img;
    return img ? gp::ImageLoadState::Ready : gp::ImageLoadState::Failed;
}
#endif

// orig's CB_SYSTEM_MENU: the app icon; a click opens the window menu under it
static gp::El* CaptionSystemMenuButton(ShellView* self, gp::Ctx* cx, int size) {
    MainWindow* win = self->win;
    gp::El* b = gp::Div(cx->a)
                    ->FlexRow()
                    ->W((float)size)
                    ->H((float)size)
                    ->Shrink0()
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->BoundsOut(&self->sysMenuBtn)
                    ->PathClick(GStrL("sumatra-sys-menu"))
                    ->OnClick(gp::Listen(cx, &ShellView::OnSystemMenu));
#if OS_WIN
    if (Pixmap* px = AppShellAppIconPixmap(win)) {
        float k = CanvasScale(win);
        gp::ImageSource src = gp::ImageSource::FromCustom(AppIconLoad, px);
        b->Child(gp::ImageEl(cx->a, src, GStrL(""))->W((float)px->width * k)->H((float)px->height * k)->Shrink0());
    }
#else
    (void)win;
#endif
    return b;
}

// orig's CreateCaptionLayout / SyncCaptionLayout:
// single row: sys | menu | tabs | gap | min | max/restore | close
// two row:     sys | menu bar | drag | min | max/restore | close
//              tabs
// ng: the menu button is the tab strip's own first cell (gui/TabsUI.cpp)
static void BuildCaption(ShellView* self, gp::Ctx* cx, gp::El* root, bool twoRow) {
    MainWindow* win = self->win;
    const gp::Theme& th = gp::ThemeNow(cx->app);
    bool maximized = win->isMaximized;
    int winBtn = CaptionWinBtnDy(win, twoRow);
    // hamburger / app icon match the tab band; min/max/close span the pad too
    int tabBtn = twoRow ? kMenuBarDy : kTabBarDy;

    gp::El* row1 = gp::Div(cx->a)
                       ->FlexRow()
                       ->W(gp::kFill)
                       ->H((float)winBtn)
                       ->Shrink0()
                       ->ItemsEnd()
                       ->Bg(ToGpui(ThemeControlBackgroundColor()))
                       ->Click(gp::ClickWinCaption);
    row1->Child(CaptionSystemMenuButton(self, cx, tabBtn));
    gp::El* tabs = nullptr;
    if (twoRow) {
        row1->Child(
            gp::Div(cx->a)->H((float)kMenuBarDy)->Shrink0()->Bg(th.tokens.titleBar)->Child(BuildMenuBar(self, cx)));
        row1->Child(gp::Div(cx->a)->Flex1()->H((float)winBtn)->Click(gp::ClickWinCaption));
        win->tabsAvailDx = 0;
        tabs = TabsUIBuild(win, cx, kTabBarDy);
    } else {
        int frameDx = win->frameRc.dx - 2 * AppShellFrameBorder(win);
        win->tabsAvailDx = std::max(0, frameDx - tabBtn - kTabsButtonGapX - 3 * winBtn);
        tabs = TabsUIBuild(win, cx, kCaptionTabBarDy);
        if (tabs) {
            row1->Child(gp::Div(cx->a)->Flex1()->MinW(0)->H((float)kCaptionTabBarDy)->Child(tabs));
        } else {
            // a window without tabs still has orig's menu button
            if (gp::El* menuBtn = TabsUIMenuButton(win, cx)) {
                row1->Child(
                    gp::Div(cx->a)->FlexRow()->ItemsCenter()->H((float)kCaptionTabBarDy)->Shrink0()->Child(menuBtn));
            }
            row1->Child(gp::Div(cx->a)->Flex1()->H((float)winBtn)->Click(gp::ClickWinCaption));
        }
        row1->Child(gp::Div(cx->a)->W((float)kTabsButtonGapX)->H((float)winBtn)->Shrink0()->Click(gp::ClickWinCaption));
    }
    row1->Child(CaptionButton(cx, CB_MINIMIZE, gp::ClickWinMin, winBtn));
    row1->Child(CaptionButton(cx, maximized ? CB_RESTORE : CB_MAXIMIZE, gp::ClickWinMax, winBtn));
    row1->Child(CaptionButton(cx, CB_CLOSE, gp::ClickWinClose, winBtn));
    root->Child(row1);
    if (twoRow && tabs) {
        root->Child(tabs);
    }
}

TempStr AppShellFrameStatsTemp(MainWindow* win, bool reset) {
    static u64 cursor = 0;
    if (reset) {
        cursor = 0;
    }
    if (!win || !win->gpuiWin) {
        return StrL("frames=0");
    }
    gp::FrameTiming frames[gp::kFrameTraceCap];
    int n = gp::WindowCollectFrames(win->gpuiWin, &cursor, frames, gp::kFrameTraceCap);
    if (n <= 0) {
        return StrL("frames=0");
    }
    float ms[gp::kFrameTraceCap];
    double total = 0;
    double maxMs = 0;
    int over16 = 0;
    for (int i = 0; i < n; i++) {
        float v = frames[i].drawSecs * 1000.0f;
        ms[i] = v;
        total += v;
        if (v > maxMs) {
            maxMs = v;
        }
        if (v > 16.0f) {
            over16++;
        }
    }
    // insertion sort: at most kFrameTraceCap values
    for (int i = 1; i < n; i++) {
        float v = ms[i];
        int j = i - 1;
        for (; j >= 0 && ms[j] > v; j--) {
            ms[j + 1] = ms[j];
        }
        ms[j + 1] = v;
    }
    int idx = (n * 95) / 100;
    if (idx >= n) {
        idx = n - 1;
    }
    return fmt("frames=%d meanMs=%.2f p95Ms=%.2f maxMs=%.2f over16ms=%d", n, total / n, (double)ms[idx], maxMs, over16);
}

static gp::Modifiers TestMods(int mods) {
    gp::Modifiers m;
    m.control = (mods & 1) != 0;
    m.shift = (mods & 2) != 0;
    m.alt = (mods & 4) != 0;
    return m;
}

static gp::MouseButton TestButton(int b) {
    switch (b) {
        case 1:
            return gp::MouseButton::Right;
        case 2:
            return gp::MouseButton::Middle;
    }
    return gp::MouseButton::Left;
}

// ng: kinds are key / keyup (a = vk, b = mods: 1 Ctrl, 2 Shift, 4 Alt,
// 8 auto-repeat), char (a = code point), down / up / click (a, b = x, y in
// dips; c = button: 0 left, 1 right, 2 middle; d = mods), move (c = pressed
// button + 1, or 0) and wheel (c = delta in 120ths of a notch, 1000000 + delta
// for a horizontal one)
TempStr AppShellTestInput(MainWindow* win, Str kind, int a, int b, int c, int d) {
    // a modal dialog in a window of its own has the input, as on screen: the
    // main window is disabled, so what a test sends "to the window" is the
    // dialog's (mouse coordinates are the dialog's client area then)
    if (ToolWindow* modal = ToolWindowModalOf(win)) {
        TempStr res = AppShellTestInputGpui(ToolWindowGpui(modal), kind, a, b, c, d);
        ToolWindowInvalidate(modal);
        return res;
    }
    // and so has one of orig's modeless dialogs while it is the active window
    if (gp::Window* dlgWin = DlgWindowInputTarget(win)) {
        TempStr res = AppShellTestInputGpui(dlgWin, kind, a, b, c, d);
        gp::AppInvalidate(dlgWin);
        return res;
    }
    // the command palette's popup window takes the keyboard while it is up
    if (gp::Window* paletteWin = CommandPaletteInputWindow(win)) {
        TempStr res = AppShellTestInputGpui(paletteWin, kind, a, b, c, d);
        gp::AppInvalidate(paletteWin);
        return res;
    }
    return AppShellTestInputGpui(win ? win->gpuiWin : nullptr, kind, a, b, c, d);
}

TempStr AppShellTestInputGpui(gp::Window* gw, Str kind, int a, int b, int c, int d) {
    if (!gw) {
        return StrL("ERR no-window");
    }
    float x = (float)a;
    float y = (float)b;
    if (str::Eq(kind, StrL("key"))) {
        gp::KeyDownFlags flags;
        flags.held = (b & 8) != 0;
        bool eaten = gp::WindowKeyDown(gw, a, (b & 2) != 0, (b & 1) != 0, (b & 4) != 0, false, false, flags);
        // a handled key makes gpui drop the WM_CHAR that follows it. None
        // follows here, so the next "char" - another keystroke - was dropped
        gw->eatChar = false;
        return fmt("OK eaten=%d", eaten ? 1 : 0);
    }
    if (str::Eq(kind, StrL("keyup"))) {
        gp::WindowKeyUp(gw, a, (b & 2) != 0, (b & 1) != 0, (b & 4) != 0);
        return StrL("OK");
    }
    if (str::Eq(kind, StrL("char"))) {
        gp::WindowChar(gw, (u32)a, false, false);
        return StrL("OK");
    }
    if (str::Eq(kind, StrL("move"))) {
        gp::PlatformInput in = gp::InputMouseMove(x, y, c != 0, TestButton(c - 1), TestMods(d));
        gp::WindowDispatchInput(gw, &in);
        return StrL("OK");
    }
    if (str::Eq(kind, StrL("wheel"))) {
        bool horizontal = c >= 500000;
        if (horizontal) {
            c -= 1000000;
        }
        float delta = (float)c / 120.f * gp::WheelNotchPixels(gw->app);
        gp::PlatformInput in = gp::InputScrollWheel(x, y, horizontal ? -delta : 0.f, horizontal ? 0.f : delta, false,
                                                    TestMods(d), gp::TouchPhase::Moved);
        gp::WindowDispatchInput(gw, &in);
        return StrL("OK");
    }
    bool isDown = str::Eq(kind, StrL("down"));
    bool isUp = str::Eq(kind, StrL("up"));
    bool isClick = str::Eq(kind, StrL("click"));
    if (!isDown && !isUp && !isClick) {
        return StrL("ERR kind");
    }
    gp::MouseButton btn = TestButton(c);
    if (isDown || isClick) {
        gp::PlatformInput mv = gp::InputMouseMove(x, y, false, btn, TestMods(d));
        gp::WindowDispatchInput(gw, &mv);
        int clicks = gp::WindowClickCount(gw, x, y, btn);
        gp::PlatformInput in = gp::InputMouseDown(btn, x, y, TestMods(d), clicks, false);
        gp::WindowDispatchInput(gw, &in);
    }
    if (isUp || isClick) {
        gp::PlatformInput in = gp::InputMouseUp(btn, x, y, TestMods(d), gp::WindowCurrentClickCount(gw));
        gp::WindowDispatchInput(gw, &in);
    }
    return StrL("OK");
}

bool AppShellNativeCursorActive(MainWindow* win) {
    gp::Window* gw = IsMainWindowValid(win) ? win->gpuiWin : nullptr;
    if (!gw || win->nativeCursor == NativeCursor::None) {
        return false;
    }
    return (int)gw->cursor == win->canvasCursor && gw->cursor == gp::CursorKind::OpenHand;
}

void AppShellAfterNativeDrag(MainWindow* win) {
    gp::Window* gw = IsMainWindowValid(win) ? win->gpuiWin : nullptr;
    if (!gw || !gw->mouseDown) {
        return;
    }
    gp::PlatformInput in =
        gp::InputMouseUp(gp::MouseButton::Left, gw->mouseX, gw->mouseY, {}, gp::WindowCurrentClickCount(gw));
    gp::WindowDispatchInput(gw, &in);
    AppShellInvalidate(win);
}

TempStr AppShellUiStateTemp(MainWindow* win) {
    gp::Window* gw = win ? win->gpuiWin : nullptr;
    if (!gw || !win->shell) {
        return StrL("ERR no-window");
    }
    auto* view = (ShellView*)gp::EntityGet(gw->app, win->shell->view.id);
    auto* mb = view ? (gpc::AppMenuBarState*)gp::EntityGet(gw->app, view->menuBarState.id) : nullptr;
    // the focus is a modal dialog's while one is up in a window of its own
    ToolWindow* modal = ToolWindowModalOf(win);
    gp::Window* dlgWin = modal ? ToolWindowGpui(modal) : DlgWindowInputTarget(win);
    gp::Window* paletteWin = dlgWin ? nullptr : CommandPaletteInputWindow(win);
    gp::Window* focusWin = dlgWin ? dlgWin : paletteWin ? paletteWin : gw;
    bool edit = focusWin->input && focusWin->input->focused;
    WindowTab* tab = win->CurrentTab();
    str::Builder out;
    out.Append(fmt("focus frame=%d edit=%d id=%d", !dlgWin && !paletteWin && AppShellIsFrameFocused(win) ? 1 : 0,
                   edit ? 1 : 0, gp::WindowFocusedId(focusWin)));
    if (edit) {
        gp::Str v = gp::InputValue(focusWin->input);
        out.Append(fmt(" editText='%s'", Str{(char*)v.s, (int)v.len}));
    }
    out.Append(fmt(" menu=%d menuArmed=%d menuBarVisible=%d menuBarTemp=%d", mb ? mb->selected : -1,
                   view ? view->menuArmedIdx : -1, win->isMenuBarVisible ? 1 : 0, view && view->menuBarTemp ? 1 : 0));
    out.Append(fmt(" page=%d findUI=%d selection=%d annotSel=%d presentation=%d fullscreen=%d mouseAction=%d",
                   win->ctrl ? win->ctrl->CurrentPageNo() : 0, IsFindUIVisible(win) ? 1 : 0, win->showSelection ? 1 : 0,
                   tab && tab->selectedAnnotation ? 1 : 0, (int)win->presentation, win->isFullScreen ? 1 : 0,
                   (int)win->mouseAction));
    out.Append(fmt(" toc=%d fav=%d tabs=%d windows=%d", win->uiState.tocVisible ? 1 : 0,
                   win->uiState.favVisible ? 1 : 0, win->TabCount(), len(gWindows)));
    out.Append(fmt(" pageBox=%d chapterBox=%d ", IsToolbarLocationBoxFocused(win, false) ? 1 : 0,
                   IsToolbarLocationBoxFocused(win, true) ? 1 : 0));
    out.Append(SidebarStateTemp(win));
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        Rect vp = dm->GetViewPort();
        out.Append(fmt(" view=%d,%d zoom=%.1f", vp.x, vp.y, (double)dm->GetZoomReal(dm->CurrentPageNo())));
    }
    out.Append(fmt(" annotHover=%d", IsAnnotationHoverOverlayVisible(win) ? 1 : 0));
    // the tooltip gpui shows or is about to show (it is a layer of its own,
    // which a window capture does not have)
    auto* tipOverlay = (gp::TooltipOverlay*)gp::EntityGet(gw->app, gw->tooltip);
    if (!tipOverlay || !(tipOverlay->hasContent || tipOverlay->hasPending)) {
        tipOverlay = (gp::TooltipOverlay*)gp::EntityGet(gw->app, gw->rootTooltip);
    }
    if (tipOverlay && (tipOverlay->hasContent || tipOverlay->hasPending)) {
        gp::Str tip = tipOverlay->hasContent ? tipOverlay->content.text : tipOverlay->pending.text;
        out.Append(fmt(" tip='%s'", Str{(char*)tip.s, (int)tip.len}));
    }
    Rect crc = win->canvasRc;
    out.Append(fmt(" canvas=%d,%d,%d,%d", crc.x, crc.y, crc.dx, crc.dy));
    if (win->overlayScrollV) {
        OverlayScrollbar* v = win->overlayScrollV;
        OverlayScrollbar* h = win->overlayScrollH;
        out.Append(fmt(" sbV=%d/%d/%d sbH=%d/%d/%d", (int)v->state, OverlayScrollbarWidth(v), v->nPos, (int)h->state,
                       OverlayScrollbarWidth(h), h->nPos));
    }
    bool dialogUp = dlgWin || DialogsAccelTable(win) != DialogAccels::All;
    out.Append(fmt(" popup=%d dialog=%d overlay=%d", IsTrackedPopupOpenInApp(gw->app) ? 1 : 0, dialogUp ? 1 : 0,
                   paletteWin || (view && view->overlayUp) ? 1 : 0));
    return ToStrTemp(out);
}

void AppShellShowMenuBarTemp(MainWindow* win) {
    if (!win || !win->gpuiWin || !win->shell || gp::AppHasMenuBar()) {
        return;
    }
    auto* view = (ShellView*)gp::EntityGet(win->gpuiWin->app, win->shell->view.id);
    if (!view || !win->menu) {
        return;
    }
    if (!win->isMenuBarVisible) {
        view->menuBarTemp = true;
    }
    view->menuArmedIdx = 0;
    view->menuCues = true;
    AppShellInvalidate(win);
}

void AppShellSuppressAltMenu(MainWindow* win) {
    if (!win || !win->gpuiWin || !win->shell) {
        return;
    }
    auto* view = (ShellView*)gp::EntityGet(win->gpuiWin->app, win->shell->view.id);
    if (view) {
        view->menuKey = 0;
    }
}

// orig's MainWindow::Focus(): bring the window to the foreground. Used by the
// DDE / reuse-instance handlers, which open a file into a running instance.
void AppShellActivateWindow(MainWindow* win) {
    if (win && win->gpuiWin) {
        gp::AppActivate(win->gpuiWin);
    }
}

// F6 treats the unfocused root as the document frame, like orig's frame HWND.
bool AppShellIsFrameFocused(MainWindow* win) {
    if (!win || !win->gpuiWin || !win->shell) {
        return false;
    }
    gp::FocusHandle focused = gp::WindowFocused(win->gpuiWin);
    return !focused.IsValid() || gp::FocusHandleIsFocused(win->gpuiWin, win->shell->frameFocus);
}

void AppShellFocusFrame(MainWindow* win) {
    if (!win || !win->gpuiWin || !win->shell) {
        return;
    }
    gp::Window* gw = win->gpuiWin;
    if (gw->input) {
        gp::InputBlur(gw->input, gw->app, gw);
    }
    gp::FocusHandleFocus(gw, win->shell->frameFocus);
    AppShellInvalidate(win);
}

void AppShellQuit() {
    if (gApp) {
        gp::AppQuitAll(gApp);
    }
}

#if !OS_WIN
// ng: the Windows versions drive the native frame from NativeWindow.cpp.
void AppShellSetFullScreen(MainWindow* win, bool fullScreen, bool) {
    gp::Window* gw = win ? win->gpuiWin : nullptr;
    if (gw) {
        gp::WindowSetFullScreen(gw, fullScreen);
    }
}

Rect AppShellWindowScreenRect(MainWindow* win) {
    return win ? win->frameRc : Rect{};
}

Rect AppShellCanvasScreenRect(MainWindow*) {
    return {};
}

// ng: gpui has no window position API (see "gpui gaps")
bool AppShellPlaceWindow(MainWindow*, Rect, bool) {
    return false;
}

bool AppShellNormalWindowRect(MainWindow*, Rect*) {
    return false;
}

int AppShellWindowDpi(MainWindow*) {
    return 96;
}

void AppShellShowCursor(MainWindow* win, bool show) {
    if (win && win->gpuiWin) {
        gp::WindowSetCursorVisible(win->gpuiWin, show);
    }
}

void AppShellPreventSleep(bool) {}
#endif

gp::App* AppShellGetApp() {
    return gApp;
}

void AppShellSetApp(gp::App* app) {
    gApp = app;
}

void AppShellCloseWindow(MainWindow* win) {
    ToolWindowsCloseFor(win);
    if (win && win->gpuiWin) {
        gp::WindowOnShouldClose(win->gpuiWin, nullptr, nullptr);
        gp::AppQuit(win->gpuiWin);
        win->gpuiWin = nullptr;
    }
}

void AppShellDeleteWindow(MainWindow* win) {
    AppShellForgetNativeHwnd(win);
    if (!win->shell) {
        return;
    }
    delete win->shell;
    win->shell = nullptr;
}

// ng: gpui never tells the application that its window went away (see "gpui
// gaps"), so the tick of a surviving window notices the closed one. With the
// last window gone the message loop stops and GpuiMain's shutdown does it.
void AppShellReapClosedWindows() {
    Vec<MainWindow*> wins = gWindows;
    for (MainWindow* win : wins) {
        if (!IsMainWindowValid(win) || win->isBeingClosed) {
            continue;
        }
        if (win->gpuiWin && win->gpuiWin->running) {
            continue;
        }
        logf("AppShellReapClosedWindows: window 0x%p was closed\n", win);
        win->gpuiWin = nullptr;
        CloseWindow(win, false, false);
    }
}

void AppShellSetTitle(MainWindow* win, Str title) {
    if (win && win->gpuiWin) {
        gp::AppSetTitle(win->gpuiWin, ToGpui(title));
    }
}

#if OS_LINUX
static bool HasProgramInPath(Str name) {
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) {
        return false;
    }
    StrVec dirs;
    Split(&dirs, Str(pathEnv), StrL(":"), true);
    for (Str dir : dirs) {
        if (file::Exists(path::JoinTemp(dir, name))) {
            return true;
        }
    }
    return false;
}
#endif

// ng: without zenity or kdialog gpui's prompt returns nothing; the port's own
// pickers are there for that
bool AppShellHasOsFilePicker() {
#if OS_LINUX
    static int has = -1;
    if (has < 0) {
        has = (HasProgramInPath(StrL("zenity")) || HasProgramInPath(StrL("kdialog"))) ? 1 : 0;
        logf("AppShellHasOsFilePicker: %d\n", has);
    }
    return has == 1;
#else
    return true;
#endif
}

struct PickFileCtx {
    Func1<Str> onPicked;
};

static void OnOwnPickerDone(PickFileCtx* ctx, SavePathArgs* args) {
    Func1<Str> onPicked = ctx->onPicked;
    delete ctx;
    if (len(args->path) > 0) {
        TempStr path = str::DupTemp(args->path);
        onPicked.Call(path);
    }
}

bool AppShellPickFileAsync(MainWindow* win, Str title, Str filter, const Func1<Str>& onPicked) {
    if (!win || !win->gpuiWin) {
        return false;
    }
#if OS_WASM
    (void)title;
    (void)filter;
    WasmPickFile(onPicked);
    return true;
#else
    if (AppShellHasOsFilePicker()) {
        return false;
    }
    // the port's dialog, in the mode that picks a file that exists
    auto* ctx = new PickFileCtx{onPicked};
    auto* args = new SavePathArgs();
    args->win = win;
    args->title = str::Dup(len(title) > 0 ? title : Tr("Open"));
    WindowTab* tab = win->CurrentTab();
    TempStr dir = tab && len(tab->filePath) > 0 ? path::GetDirTemp(tab->filePath) : TempStr{};
    // only the directory matters; the name field starts empty
    args->initialPath = str::Dup(len(dir) > 0 ? path::JoinTemp(dir, StrL("")) : Str{});
    args->filter = str::Dup(filter);
    args->openExisting = true;
    args->onDone = MkFunc1(OnOwnPickerDone, ctx);
    ShowSavePathDialog(args);
    return true;
#endif
}

// ng: the same open prompt with a caller-chosen title (a certificate, an
// image); gpui's PathPrompt has no file-type filter
TempStr AppShellPromptForPathTemp(MainWindow* win, Str title, Str filter, Str initialPath) {
    if (!win || !win->gpuiWin) {
        return {};
    }
#if OS_WIN
    if (NativeFileDlgEnabled()) {
        // orig's GetOpenFileNameW, which has no title of its own
        StrVec paths;
        if (!NativeOpenFileDlg(win, filter, initialPath, false, &paths)) {
            return {};
        }
        return str::DupTemp(paths[0]);
    }
#endif
    (void)filter;
    (void)initialPath;
    gp::PathPrompt prompt;
    prompt.files = true;
    prompt.title = ToGpui(len(title) > 0 ? title : Tr("Open"));
    gp::TempStr path = gp::PromptForPathTemp(win->gpuiWin, prompt);
    return str::DupTemp(FromGpui(path));
}

#if OS_WASM
// WasmShell.js calls this. gpui leaves Ctrl+K / Cmd+K with the browser, so
// the accelerator never runs.
extern "C" EMSCRIPTEN_KEEPALIVE void sumatra_wasm_command_palette() {
    if (len(gWindows) == 0) {
        return;
    }
    ExecuteCmd(gWindows[0], CmdCommandPalette);
}

// the browser's file input answers on a later event-loop turn, so the document
// is loaded from the callback and the prompt itself has nothing to return
static void OnWasmFilePicked(MainWindow* win, Str path) {
    LoadDocument(win, path);
}
#endif

TempStr AppShellPromptForFileTemp(MainWindow* win) {
    if (!win || !win->gpuiWin) {
        return {};
    }
#if OS_WASM
    WasmPickFile(MkFunc1(OnWasmFilePicked, win));
    return {};
#else
    gp::PathPrompt prompt;
    prompt.files = true;
    prompt.title = ToGpui(Tr("Open"));
    gp::TempStr path = gp::PromptForPathTemp(win->gpuiWin, prompt);
    // gpui's temp string lives in its own arena, ours has to outlive this call
    return str::DupTemp(FromGpui(path));
#endif
}

// ng: gpui's prompt answers one path (see "gpui gaps"); Windows' own dialog
// has orig's OFN_ALLOWMULTISELECT
bool AppShellPromptForFiles(MainWindow* win, Str filter, StrVec* pathsOut) {
    if (!win || !win->gpuiWin) {
        return false;
    }
#if OS_WIN
    if (NativeFileDlgEnabled()) {
        return NativeOpenFileDlg(win, filter, {}, true, pathsOut);
    }
#endif
    (void)filter;
    TempStr path = AppShellPromptForFileTemp(win);
    if (len(path) == 0) {
        return false;
    }
    pathsOut->Append(path);
    return true;
}

// --- key bindings -----------------------------------------------------------

static void BindKeys() {
    int n = 0;
    const AccelStroke* strokes = GetAcceleratorStrokes(n);
    auto* bindings = AllocArrayTemp<gp::KeyBinding>(n);
    int nBind = 0;
    for (int i = 0; i < n; i++) {
        if (len(strokes[i].stroke) == 0) {
            continue;
        }
        gp::KeyBinding& b = bindings[nBind++];
        b.stroke = strokes[i].stroke.s;
        b.action = ActSumatraCmd();
        b.context = kShellKeyContext;
        b.arg = (intptr_t)strokes[i].cmd;
    }
    gp::KeymapBind(bindings, nBind);
    logf("BindKeys: %d shortcuts bound\n", nBind);
}

// --- menu bar ---------------------------------------------------------------

// The macOS menu bar belongs to the process, so only the front window
// installs it. A rebuild of a background window waits until that window
// is the front one.
static bool gMenuDirty = true;
static MainWindow* gMenuOwner = nullptr;
static bool gMenuVis = true;

static bool WindowOwnsAppMenu(MainWindow* win) {
    if (!win || !win->gpuiWin) {
        return false;
    }
    if (win->gpuiWin->active) {
        return true;
    }
    for (MainWindow* w : gWindows) {
        if (w && w->gpuiWin && w->gpuiWin->active) {
            return false;
        }
    }
    MainWindow* last = nullptr;
    for (MainWindow* w : gWindows) {
        last = w;
    }
    return win == last;
}

static bool MenuContainsCmd(const MenuModel* menu, int cmdId) {
    if (!menu) {
        return false;
    }
    for (const MenuItemModel& it : menu->items) {
        if (it.cmdId == cmdId) {
            return true;
        }
        if (MenuContainsCmd(it.submenu, cmdId)) {
            return true;
        }
    }
    return false;
}

// One MenuModel level as gpui rows. Labels are temp strings; AppSetMenus
// copies them before it returns.
static int FillMenuRows(gp::MenuRow* dst, const MenuModel* menu) {
    int n = 0;
    for (const MenuItemModel& it : menu->items) {
        gp::MenuRow& row = dst[n++];
        if (it.separator) {
            row.separator = true;
            continue;
        }
        row.label = ToGpui(ParseMenuAccelTextTemp(it.title).display);
        row.disabled = it.disabled;
        if (it.submenu) {
            int subN = it.submenu->items.len;
            gp::MenuRow* sub = subN > 0 ? AllocArrayTemp<gp::MenuRow>(subN) : nullptr;
            row.submenu = sub;
            row.submenuN = sub ? FillMenuRows(sub, it.submenu) : 0;
            continue;
        }
        row.action = ActSumatraCmd();
        row.arg = it.cmdId;
        row.checked = it.checked;
    }
    return n;
}

// AppKit shows the first menu under the process name, whatever it is
// called here, so the application's own menu comes before File. The
// Window menu is the one AppKit keeps Minimize and the window list in.
static void InstallNativeMenu(MainWindow* win) {
    int nTop = 0;
    for (const MenuItemModel& it : win->menu->items) {
        if (it.submenu) {
            nTop++;
        }
    }
    gp::MenuDef* defs = AllocArrayTemp<gp::MenuDef>(nTop + 2);
    int n = 0;

    gp::MenuRow* appRows = AllocArrayTemp<gp::MenuRow>(4);
    appRows[0].label = ToGpui(ParseMenuAccelTextTemp(Tr("&About")).display);
    appRows[0].action = ActSumatraCmd();
    appRows[0].arg = CmdHelpAbout;
    appRows[1].label = ToGpui(ParseMenuAccelTextTemp(Tr("&Settings...")).display);
    appRows[1].action = ActSumatraCmd();
    appRows[1].arg = CmdOptions;
    appRows[2].separator = true;
    appRows[3].label = ToGpui(Tr("Quit"));
    appRows[3].action = ActSumatraCmd();
    appRows[3].arg = CmdExit;
    appRows[3].stroke = "cmd-q";
    defs[n].name = GStrL("SumatraPDF");
    defs[n].items = appRows;
    defs[n].n = 4;
    n++;

    gp::MenuDef windowDef = {};
    windowDef.name = GStrL("Window");
    bool windowPlaced = false;
    for (const MenuItemModel& it : win->menu->items) {
        if (!it.submenu) {
            continue;
        }
        if (!windowPlaced && MenuContainsCmd(it.submenu, CmdHelpAbout)) {
            defs[n++] = windowDef;
            windowPlaced = true;
        }
        int subN = it.submenu->items.len;
        gp::MenuRow* sub = subN > 0 ? AllocArrayTemp<gp::MenuRow>(subN) : nullptr;
        defs[n].name = ToGpui(ParseMenuAccelTextTemp(it.title).display);
        defs[n].items = sub;
        defs[n].n = sub ? FillMenuRows(sub, it.submenu) : 0;
        defs[n].disabled = it.disabled;
        n++;
    }
    if (!windowPlaced) {
        defs[n++] = windowDef;
    }
    gp::AppSetMenus(gApp, defs, n);
}

bool AppShellNativeMenu() {
    return gp::AppHasMenuBar();
}

void AppShellMenuRebuilt(MainWindow* win) {
    gMenuDirty = true;
    AppShellSyncMenu(win);
}

void AppShellSyncMenu(MainWindow* win) {
    if (!gp::AppHasMenuBar() || !gApp || !WindowOwnsAppMenu(win)) {
        return;
    }
    // ShowMenubar hides a row of a Windows window. Here the menu is the
    // process menu bar; hiding it (the default once tabs are on) leaves none.
    // Fullscreen hides the bar on its own.
    if (!gMenuDirty && gMenuOwner == win && gMenuVis) {
        return;
    }
    gp::AppSetMenuBarVisible(true);
    if (win->menu) {
        InstallNativeMenu(win);
    }
    gMenuDirty = false;
    gMenuOwner = win;
    gMenuVis = true;
}

// Windows underlines the access keys always or only once Alt was pressed
// ("Underline access keys when available", off by default)
static bool MenuCuesAlways() {
#if OS_WIN
    BOOL on = FALSE;
    SystemParametersInfoW(SPI_GETKEYBOARDCUES, 0, &on, 0);
    return on != FALSE;
#else
    return false;
#endif
}

// the label with orig's '&' letter underlined
static gp::El* MenuCueLabel(gp::Ctx* cx, gp::Str label, const MenuAccelText& at, gp::Rgba fg) {
    auto* span = gp::ArenaNew<gp::TextSpan>(cx->a);
    span->lo = at.underlineOff;
    span->hi = at.underlineOff + at.underlineLen;
    span->color = fg;
    return gp::TextEl(cx->a, label)->Font(14)->Fg(fg)->Underlines(span, 1);
}

// ng: a gpui menu row takes a plain label, but draws `element` in its place
// when the row has one
static void SetMenuRowCue(gp::Ctx* cx, gpc::PopupMenu* menu, gp::Str label, const MenuAccelText& at, bool disabled) {
    if (at.underlineOff < 0 || menu->items.len == 0) {
        return;
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::Rgba fg = disabled ? th.mutedFg : th.foreground;
    menu->items[menu->items.len - 1].element = MenuCueLabel(cx, label, at, fg);
}

static gpc::PopupMenu* BuildPopup(gp::Ctx* cx, MenuModel* model, Str id, bool cues) {
    gpc::PopupMenu* menu = gpc::PopupMenu::New(cx, GpuiDup(cx->a, id))->MinW(220);
    int i = -1;
    for (const MenuItemModel& it : model->items) {
        i++;
        if (it.separator) {
            menu->Separator();
            continue;
        }
        MenuAccelText at = ParseMenuAccelTextTemp(it.title);
        gp::Str label = GpuiDup(cx->a, at.display);
        if (it.submenu) {
            TempStr subId = fmt("%s-%d", id, i);
            menu->Submenu(label, BuildPopup(cx, it.submenu, subId, cues));
            menu->Disabled(it.disabled);
        } else {
            menu->MenuWithAction(label, ActSumatraCmd(), (intptr_t)it.cmdId);
            // ng: always set, even when empty: a row without a hint asks the
            // keymap, which answers with some other command's shortcut (every
            // command is the one action, told apart by its argument)
            menu->Kbd(len(it.accel) > 0 ? GpuiDup(cx->a, it.accel) : GStrL(""));
            menu->Disabled(it.disabled);
            menu->Checked(it.checked);
        }
        if (cues) {
            SetMenuRowCue(cx, menu, label, at, it.disabled);
        }
    }
    return menu;
}

// ng: AppMenuBar takes a plain title and builds its own text element (bar >
// one column per menu > the title box > the text), so the underline is put
// on that element after the fact; a title that is not where it is expected
// stays plain
static void UnderlineMenuBarTitle(gp::Ctx* cx, gp::El* bar, int barIdx, gp::Str label, const MenuAccelText& at) {
    if (at.underlineOff < 0 || !bar) {
        return;
    }
    gp::El* wrap = bar->first;
    for (int i = 0; wrap && i < barIdx; i++) {
        wrap = wrap->next;
    }
    gp::El* box = wrap ? wrap->first : nullptr;
    gp::El* text = box ? box->first : nullptr;
    if (!text || text->kind != gp::ElKind::Text || text->text.s != label.s) {
        return;
    }
    auto* span = gp::ArenaNew<gp::TextSpan>(cx->a);
    span->lo = at.underlineOff;
    span->hi = at.underlineOff + at.underlineLen;
    span->color = gp::ThemeNow(cx->app).foreground;
    text->Underlines(span, 1);
}

// ng: gpui's AppMenuBar has no Alt access keys, so the '&' in a label is
// matched here (orig lets win32 open the menu and underline the letter)
static bool MenuBarOnAltKey(ShellView* self, gp::Ctx* cx, u16 vk) {
    MainWindow* win = self->win;
    // the macOS menu bar is the system's; it has no Alt access keys
    if (gp::AppHasMenuBar() || !win->menu) {
        return false;
    }
    if (vk < 'A' || vk > 'Z') {
        return false;
    }
    char key = (char)(vk - 'A' + 'a');
    int idx = -1;
    for (const MenuItemModel& it : win->menu->items) {
        if (!it.submenu) {
            continue;
        }
        idx++;
        if (MenuAccessKey(it.title) != key) {
            continue;
        }
        auto* st = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
        if (!st) {
            return false;
        }
        // temporarily show the menu bar if it has been hidden
        if (!win->isMenuBarVisible) {
            self->menuBarTemp = true;
        }
        gpc::AppMenuBarSelect(st, cx, idx);
        self->menuArmedIdx = -1;
        self->menuCues = true;
        return true;
    }
    return false;
}

static int MenuBarTitleCount(MainWindow* win) {
    int n = 0;
    for (const MenuItemModel& it : win->menu->items) {
        if (it.submenu) {
            n++;
        }
    }
    return n;
}

// orig's SC_KEYMENU without a character: the keyboard is in the menu bar, on
// its first title, with no menu open
static void MenuBarArm(ShellView* self, gp::Ctx* cx) {
    MainWindow* win = self->win;
    if (gp::AppHasMenuBar() || !win->menu || MenuBarTitleCount(win) == 0) {
        return;
    }
    auto* st = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
    if (!st || st->selected >= 0) {
        return;
    }
    // temporarily show the menu bar if it has been hidden
    if (!win->isMenuBarVisible) {
        self->menuBarTemp = true;
    }
    self->menuArmedIdx = 0;
    self->menuCues = true;
}

// ng: what win32's menu loop does for a key while the bar has the keyboard
// and no popup is open; every key is the bar's until it lets go
static void MenuBarArmedOnKey(ShellView* self, gp::Ctx* cx, int vk) {
    MainWindow* win = self->win;
    auto* st = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
    int n = win->menu ? MenuBarTitleCount(win) : 0;
    if (!st || n == 0) {
        self->menuArmedIdx = -1;
        return;
    }
    switch (vk) {
        case VK_LEFT:
            self->menuArmedIdx = (self->menuArmedIdx + n - 1) % n;
            return;
        case VK_RIGHT:
            self->menuArmedIdx = (self->menuArmedIdx + 1) % n;
            return;
        case VK_DOWN:
        case VK_UP:
        case VK_RETURN: {
            int idx = self->menuArmedIdx;
            self->menuArmedIdx = -1;
            gpc::AppMenuBarSelect(st, cx, idx);
            return;
        }
        case VK_ESCAPE:
        case VK_MENU:
        case VK_F10:
            self->menuArmedIdx = -1;
            return;
    }
    if (vk >= 'A' && vk <= 'Z') {
        // opens the menu with that access key; win32 beeps when there is none
        MenuBarOnAltKey(self, cx, (u16)vk);
    }
}

// the title the keyboard is on looks like the open one
static void HighlightMenuBarTitle(gp::Ctx* cx, gp::El* bar, int barIdx) {
    gp::El* wrap = bar ? bar->first : nullptr;
    for (int i = 0; wrap && i < barIdx; i++) {
        wrap = wrap->next;
    }
    gp::El* box = wrap ? wrap->first : nullptr;
    if (box) {
        box->Bg(gp::ThemeNow(cx->app).tokens.secondary);
    }
}

struct MenuKeyCmd {
    MainWindow* win = nullptr;
    int cmdId = 0;
};

static void RunMenuKeyCmd(MenuKeyCmd* c) {
    if (IsMainWindowValidAndNotClosing(c->win)) {
        ExecuteCmd(c->win, c->cmdId);
        AppShellInvalidate(c->win);
    }
    delete c;
}

// ng: win32 runs the row whose access key is typed while a menu is open; a
// gpui popup has no access keys, so the open menu's rows are matched here.
// Only the menu hanging off the bar: a row inside an open submenu is not.
static bool MenuOnAccessKey(ShellView* self, gp::Ctx* cx, u16 vk) {
    MainWindow* win = self->win;
    if (gp::AppHasMenuBar()) {
        return false;
    }
    auto* st = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
    if (!win->menu || !st || st->selected < 0 || vk < 'A' || vk > 'Z') {
        return false;
    }
    gp::PopupMenuState* popupState = self->menuPopupIdx == st->selected ? self->menuPopupState.Get(cx) : nullptr;
    if (!popupState || popupState->openSubmenu >= 0) {
        return false;
    }
    char key = (char)(vk - 'A' + 'a');
    MenuModel* model = nullptr;
    int barIdx = -1;
    for (const MenuItemModel& it : win->menu->items) {
        if (it.submenu && ++barIdx == st->selected) {
            model = it.submenu;
            break;
        }
    }
    if (!model) {
        return false;
    }
    int row = -1;
    for (const MenuItemModel& it : model->items) {
        row++;
        if (it.separator || it.disabled || MenuAccessKey(it.title) != key) {
            continue;
        }
        if (it.submenu) {
            popupState->selected = row;
            popupState->openSubmenu = row;
            return true;
        }
        // the popup holds the keyboard until it is gone, so the command runs
        // after this frame
        gpc::AppMenuBarSelect(st, cx, -1);
        auto* c = new MenuKeyCmd{win, it.cmdId};
        uitask::Post(MkFunc0<MenuKeyCmd>(RunMenuKeyCmd, c), "MenuKeyCmd");
        return true;
    }
    return false;
}

static gp::El* BuildMenuBar(ShellView* self, gp::Ctx* cx) {
    MainWindow* win = self->win;
    auto* state = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
    if (self->menuPopupIdx >= 0) {
        gp::PopupMenuState* popupState = self->menuPopupState.Get(cx);
        bool wasDismissed = state && state->selected == self->menuPopupIdx && popupState && !popupState->open;
        if (!state || state->selected != self->menuPopupIdx || wasDismissed) {
            if (popupState) {
                popupState->open = false;
            }
            self->menuPopupState = {};
            self->menuPopupIdx = -1;
        }
        if (wasDismissed) {
            gpc::AppMenuBarSelect(state, cx, -1);
        }
    }

    // orig's menu is win32's: the access keys show from an Alt press until
    // the menu it opened is gone, or always if the system says so
    bool menuOpen = state && state->selected >= 0;
    if (!self->altDown && !menuOpen && self->menuArmedIdx < 0) {
        self->menuCues = false;
    }
    bool cues = self->menuCues || MenuCuesAlways();

    struct BarTitle {
        gp::Str label;
        MenuAccelText at;
    };
    int nTitles = 0;
    auto* titles = AllocArrayTemp<BarTitle>(len(win->menu->items));

    gpc::AppMenuBar* bar = gpc::AppMenuBar::New(cx, GStrL("sumatra-menubar"), self->menuBarState);
    int i = -1;
    int barIdx = -1;
    for (const MenuItemModel& it : win->menu->items) {
        i++;
        if (!it.submenu) {
            continue;
        }
        barIdx++;
        TempStr id = fmt("sumatra-menu-%d", i);
        MenuAccelText at = ParseMenuAccelTextTemp(it.title);
        gp::Str label = GpuiDup(cx->a, at.display);
        titles[nTitles++] = {label, at};
        gpc::PopupMenu* popup = BuildPopup(cx, it.submenu, id, cues);
        if (state && state->selected == barIdx && self->menuPopupIdx < 0) {
            gp::PopupMenuState* popupState = popup->state.Get(cx);
            if (popupState) {
                // AppMenuBar controls visibility itself. Keep PopupMenu's open
                // state in sync so its normal confirm/outside dismissal works.
                popupState->open = true;
                popupState->selected = -1;
                popupState->openSubmenu = -1;
                self->menuPopupState = popup->state;
                self->menuPopupIdx = barIdx;
            }
        }
        bar->Menu(label, popup);
    }
    gp::El* el = bar->IntoEl();
    for (int t = 0; cues && t < nTitles; t++) {
        UnderlineMenuBarTitle(cx, el, t, titles[t].label, titles[t].at);
    }
    if (self->menuArmedIdx >= 0 && !menuOpen) {
        HighlightMenuBarTitle(cx, el, self->menuArmedIdx);
    }
    return el;
}

// --- canvas -----------------------------------------------------------------

// ng: orig hands the browser control the canvas HWND when a CHM / markdown
// tab becomes current and hides it when it stops being current (Tabs.cpp).
// The frame that draws the canvas does it here, so a tab switch, a tab close
// and a window close are all covered. Returns the current tab's browser view.
static BrowserView* UpdateBrowserViews(MainWindow* win, bool overlayUp) {
    WindowTab* currTab = win->CurrentTab();
    BrowserView* curr = nullptr;
    for (WindowTab* tab : win->Tabs()) {
        ChmModel* cm = tab->AsChm();
        MarkdownModel* mm = tab->AsMarkdown();
        if (!cm && !mm) {
            continue;
        }
        // ng: the webview is an OS child window and covers whatever gpui draws
        // under it, so a palette or a dialog (which orig puts in a window of
        // its own, above the browser) hides it while it is up
        if (tab != currTab || overlayUp) {
            if (cm) {
                cm->RemoveParentWindow();
            } else {
                mm->RemoveParentWindow();
            }
            continue;
        }
        if (cm) {
            cm->SetParentWindow(win, nullptr);
            curr = cm->docView;
        } else {
            mm->SetParentWindow(win, nullptr);
            curr = mm->docView;
        }
    }
    return curr;
}

// a fixed-page document draws through DocCanvas, the Home tab through
// HomePage, a CHM or markdown document through the browser view
static gp::El* BuildCanvas(ShellView* self, gp::Ctx* cx, bool overlayUp) {
    MainWindow* win = self->win;
    const gp::Theme& th = gp::ThemeNow(cx->app);
    BrowserView* browser = UpdateBrowserViews(win, overlayUp);
    if (win->AsFixed()) {
        return DocCanvasBuild(win, cx);
    }
    if (win->IsCurrentTabAbout()) {
        return HomePageBuild(win, cx);
    }
    WindowTab* currTab = win->CurrentTab();
    if (currTab && currTab->IsFavoritesTab()) {
        return SidebarBuildFavTab(win, cx);
    }
    if (browser && !overlayUp) {
        gp::El* box = gp::Div(cx->a)->Flex1()->W(gp::kFill)->Bg(ThemeGpuiCanvasBg());
        box->Child(BrowserViewBuild(browser, cx));
        return box;
    }
    gp::El* canvas = gp::Div(cx->a)->FlexCol()->Flex1()->W(gp::kFill)->ItemsCenter()->JustifyCenter()->Gap(6)->Bg(
        ThemeGpuiCanvasBg());
    WindowTab* tab = win->CurrentTab();
    DocController* ctrl = tab ? tab->ctrl : nullptr;
    if (!ctrl) {
        canvas->Child(gp::TextEl(cx->a, GStrL("SumatraPDF is a PDF reader"))->Font(16)->Fg(th.mutedFg));
        return canvas;
    }
    TempStr s = fmt("%s", tab->GetTabTitle());
    canvas->Child(gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(18)->Fg(th.foreground));
    s = fmt("Page %d of %d", ctrl->CurrentPageNo(), ctrl->PageCount());
    canvas->Child(gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(14)->Fg(th.mutedFg));
    return canvas;
}

static void BuildNotifications(MainWindow* win, gp::Ctx* cx, gp::El* parent);
static bool HasNotificationsFor(MainWindow* win);

// the sidebar, the splitter and the canvas; orig's RelayoutFrame puts the same
// three side by side with a SplitterCtrl between them
static gp::El* BuildBody(ShellView* self, gp::Ctx* cx, bool overlayUp) {
    MainWindow* win = self->win;
    gp::El* canvas =
        BuildCanvas(self, cx, overlayUp)->TrackFocus(win->shell->frameFocus)->FocusOnPress()->TabStop(false);
    // orig makes the notifications children of hwndCanvas, so they sit over the
    // canvas and not over the sidebar or the toolbar
    if (HasNotificationsFor(win)) {
        gp::El* box = gp::Div(cx->a)->FlexCol()->Flex1()->W(gp::kFill)->MinH(0)->Child(canvas);
        BuildNotifications(win, cx, box);
        canvas = box;
    }
    gp::El* sidebar = SidebarBuild(win, cx);
    gp::El* aiChat = overlayUp ? nullptr : AIChatPanelBuild(win, cx);
    // orig docks the folder picker's window beside the frame; here it is the
    // frame's last column
    gp::El* navFiles = NavFilesUIBuild(win, cx);
    if (!sidebar && !aiChat && !navFiles) {
        return canvas;
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* row = gp::Div(cx->a)->FlexRow()->Flex1()->W(gp::kFill)->MinH(0);
    gp::El* sidebarBox = nullptr;
    gp::El* splitter = nullptr;
    if (sidebar) {
        sidebarBox = gp::Div(cx->a)->FlexCol()->W((float)win->sidebarDx)->H(gp::kFill)->Shrink0()->Child(sidebar);
        splitter = gp::Div(cx->a)
                       ->W((float)kSplitterDx)
                       ->H(gp::kFill)
                       ->Shrink0()
                       ->Bg(th.border)
                       ->Cursor(self->splitterRefused ? gp::CursorKind::NotAllowed : gp::CursorKind::ColResize)
                       ->PathClick(GStrL("sidebar-splitter"))
                       ->OnDrag(GStrL("sumatra-sidebar-splitter"))
                       ->OnDragMove(gp::ListenTo(ShellViewOf(win), &ShellView::OnSidebarResize))
                       ->OnMouseUp(gp::ListenTo(ShellViewOf(win), &ShellView::OnSidebarResized))
                       ->OnMouseUpOut(gp::ListenTo(ShellViewOf(win), &ShellView::OnSidebarResized));
        if (!gSettings->sidebarOnRight) {
            row->Child(sidebarBox);
            row->Child(splitter);
        }
    }
    row->Child(gp::Div(cx->a)->FlexCol()->Flex1()->MinW(0)->H(gp::kFill)->Child(canvas));
    if (sidebarBox && gSettings->sidebarOnRight) {
        row->Child(splitter);
        row->Child(sidebarBox);
    }
    if (aiChat) {
        row->Child(aiChat);
    }
    if (navFiles) {
        row->Child(navFiles);
    }
    return row;
}

// --- notifications ----------------------------------------------------------

// ng: step 6 handed each notification to gpui's toast stack, which anchors to
// the window, draws its own card and is not in what PrintWindow() returns.
// Step 14a draws them the way orig does instead: a card in the corner of the
// canvas it belongs to, with orig's five NotifCorner anchors, its close box
// and the tip markup ([text](CmdFoo), **bold**, (Kbd/..)) as real spans.

constexpr int kNotifPadX = 10;
constexpr int kNotifPadY = 6;
// orig's kTopLeftMargin
constexpr int kNotifTopLeftMargin = 8;
constexpr int kNotifCloseGap = 8;
constexpr int kNotifCloseDx = 16;

static bool NotifShownNow(MainWindow* win, NotificationWnd* n);

static bool HasNotificationsFor(MainWindow* win) {
    for (NotificationWnd* n : GetNotifications()) {
        if (NotifShownNow(win, n)) {
            return true;
        }
    }
    return false;
}

static bool NotifShownNow(MainWindow* win, NotificationWnd* n) {
    if (n->win != win) {
        return false;
    }
    return !n->tab || n->tab == win->CurrentTab();
}

// orig's NotificationWnd::Layout limits the text to the parent's width, so the
// close box stays reachable even for a very long message (issue #2916)
static float NotifMaxTextDx(MainWindow* win) {
    int dx = win->canvasRc.dx - 2 * DpiScale(kNotifTopLeftMargin);
    // the card's border, its padding and the close box after the text
    dx -= 2 + 2 * kNotifPadX + kNotifCloseGap + kNotifCloseDx;
    return dx > 0 ? (float)dx : (float)(1 << 20);
}

static gp::El* BuildNotifSpans(gp::Ctx* cx, NotificationWnd* n, Color colText, Color colLink) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* row = gp::Div(cx->a)->FlexRow()->FlexWrap()->ItemsCenter()->MaxW(NotifMaxTextDx(n->win));
    if (!n->spans) {
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, n->msg))->Font(14)->Fg(ToGpui(colText)));
        return row;
    }
    const Vec<TipSpan>& spans = *n->spans;
    for (int i = 0; i < len(spans); i++) {
        const TipSpan& sp = spans[i];
        gp::El* el = gp::TextEl(cx->a, GpuiDup(cx->a, sp.text))->Font(14)->Fg(ToGpui(colText));
        switch (sp.kind) {
            case TipSpanKind::Text:
                break;
            case TipSpanKind::Bold:
                el->Bold();
                break;
            case TipSpanKind::Link:
                el->Fg(ToGpui(colLink))
                    ->Underline()
                    ->Cursor(gp::CursorKind::Pointer)
                    // orig's VirtRichText::OnGetTooltip: the link's target
                    ->Tip(GpuiDup(cx->a, sp.target))
                    ->PathClick(GpuiDup(cx->a, fmt("notif-%d-link-%d", (int)n->key, i)))
                    ->OnClick(gp::ListenTo(ShellViewOf(n->win), &ShellView::OnNotifLink,
                                           (intptr_t)(((u64)n->key << 16) | (u32)i)));
                break;
            case TipSpanKind::Kbd:
            case TipSpanKind::Code:
                el->Mono()->Font(13)->Bg(th.tokens.muted)->PadX(4)->Radius(3);
                break;
        }
        row->Child(el);
    }
    return row;
}

static gp::El* BuildNotifCard(gp::Ctx* cx, NotificationWnd* n) {
    // orig's NotificationWnd::Colors()
    Color colBg = n->warning ? ThemeNotificationsHighlightColor() : ThemeNotificationsBackgroundColor();
    Color colText = n->warning ? ThemeNotificationsHighlightTextColor() : ThemeNotificationsTextColor();
    Color colLink = n->warning ? ThemeNotificationsHighlightLinkColor() : ThemeWindowLinkColor();
    bool isBar = n->corner == NotifCorner::BottomBar;
    gp::El* card = gp::Div(cx->a)
                       ->FlexCol()
                       ->PadX((float)kNotifPadX)
                       ->PadY((float)kNotifPadY)
                       ->Bg(ToGpui(colBg))
                       ->Border(1, ToGpui(ThemeWindowTextColor()));
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap((float)kNotifCloseGap);
    if (isBar) {
        card->W(gp::kFill);
        row->W(gp::kFill)->JustifyCenter();
    }
    row->Child(BuildNotifSpans(cx, n, colText, colLink));
    row->Child(gp::TextEl(cx->a, GStrL("\xc3\x97"))
                   ->Font(14)
                   ->Fg(ToGpui(colText))
                   ->Cursor(gp::CursorKind::Pointer)
                   ->PathClick(GpuiDup(cx->a, fmt("notif-%d-close", (int)n->key)))
                   ->OnClick(gp::ListenTo(ShellViewOf(n->win), &ShellView::OnNotifClose, (intptr_t)n->key)));
    card->Child(row);
    if (n->progressPerc >= 0) {
        gp::Rgba col = ToGpui(ThemeNotificationsProgressColor());
        gp::El* progress = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->H(5)->Border(1, col);
        progress->Child(gp::Div(cx->a)->WFrac((float)n->progressPerc / 100.f)->H(gp::kFill)->Bg(col));
        card->Gap((float)kNotifPadY)->Child(progress);
    }
    return card;
}

// one absolutely positioned stack per corner, as orig's RelayoutNotifications
// stacks them toward the opposite edge
// ng: the stacks are added to the canvas wrapper directly rather than into one
// full-size overlay, which would win every hit test over the canvas under it
static void BuildNotifications(MainWindow* win, gp::Ctx* cx, gp::El* parent) {
    gp::El* stacks[(int)NotifCorner::Count] = {};
    for (NotificationWnd* n : GetNotifications()) {
        if (!NotifShownNow(win, n)) {
            continue;
        }
        int ci = (int)n->corner;
        if (!stacks[ci]) {
            bool atRight = n->corner == NotifCorner::TopRight || n->corner == NotifCorner::BottomRight;
            bool atBottom = n->corner != NotifCorner::TopLeft && n->corner != NotifCorner::TopRight;
            gp::El* st = gp::Div(cx->a)->FlexCol()->Gap(4)->Absolute();
            if (n->corner == NotifCorner::BottomBar) {
                st->Left(0)->Right(0)->Bottom(0);
            } else {
                float mx = (float)DpiScale(n->xMargin);
                float my = (float)DpiScale(n->yMargin);
                if (atRight) {
                    st->Right(mx)->ItemsEnd();
                } else {
                    st->Left(mx);
                }
                if (atBottom) {
                    st->Bottom(my);
                } else {
                    st->Top(my);
                }
            }
            stacks[ci] = st;
        }
        stacks[ci]->Child(BuildNotifCard(cx, n));
    }
    for (gp::El* st : stacks) {
        if (st) {
            parent->Child(st);
        }
    }
}

void ShellView::OnNotifClose(ShellView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t keyIn) {
    NotificationWnd* n = GetNotificationByKey((u32)keyIn);
    if (n) {
        logf("NotificationClose: '%s'\n", n->msg);
        CloseNotification(n, NotifCloseReason::User);
    }
    gp::Notify(cx);
}

void ShellView::OnNotifLink(ShellView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t packed) {
    u32 key = (u32)((u64)packed >> 16);
    int spanIdx = (int)((u64)packed & 0xffff);
    NotificationWnd* n = GetNotificationByKey(key);
    if (!n || !n->spans || spanIdx < 0 || spanIdx >= len(*n->spans)) {
        return;
    }
    Str target = (*n->spans)[spanIdx].target;
    logf("NotificationLink: '%s'\n", target);
    if (str::StartsWith(target, StrL("http"))) {
        SumatraLaunchBrowser(target);
        return;
    }
    // orig's ExecuteTipLink: Cmd targets may include arguments (e.g.
    // "CmdFixDefaultApp .pdf"); those go through CreateCommandFromDefinition
    int cmdId = GetCommandIdByName(target);
    if (cmdId <= 0 && str::StartsWith(target, StrL("Cmd"))) {
        CustomCommand* cmd = CreateCommandFromDefinition(target);
        cmdId = cmd ? cmd->id : 0;
    }
    if (cmdId > 0) {
        ExecuteCmd(self->win, cmdId);
    }
    gp::Notify(cx);
}

// --- the view ---------------------------------------------------------------

// ng: orig gives a focused edit control its own accelerator table, so only the
// "safe" shortcuts reach the application while the user types. gpui resolves
// the key to our one action instead, and the key handler below (which runs
// first) decides whether the command may run. Leaving the action event
// propagating means gpui does not swallow the character either.
static bool gCmdSuppressed = false;

// After a Ctrl/Alt+key accelerator, ignore auto-repeat of that key until it is
// released. A slow CmdFindFirst (ASan) lets Windows queue a repeat KEYDOWN F
// after Ctrl is up; F is CmdToggleFullscreen and steals find/search focus.
static int gIgnoreRepeatKey = 0;
// the key of the Ctrl / Alt chord being dispatched; OnCmd makes it the above
static int gChordKey = 0;

// ng: a gpui Dialog takes the focus itself when it opens (its focus trap,
// "dialog-<layer>"), and runs only its own OnOk on Enter. That is no control of
// the dialog's: orig's default button answers Enter then
static bool IsDialogTrapFocused(gp::Window* win) {
    constexpr int kMaxDialogLayers = 4;
    for (int i = 0; i < kMaxDialogLayers; i++) {
        if (win->focusId == gp::HashClickId(ToGpui(fmt("dialog-%d", i)))) {
            return true;
        }
    }
    return false;
}

static bool IsTextFieldFocused(gp::Ctx* cx) {
    return cx->win && cx->win->input && cx->win->input->focused;
}

// ng: true while the keyboard focus is inside a gpui key context `name`
// (gpui's own walk in WindowResolveKeyAction, which is not exported per name)
static bool IsFocusInKeyContext(gp::Window* win, gp::Str name) {
    if (!win || !win->focusId) {
        return false;
    }
    int ix = -1;
    for (int i = 0; i < win->focusEls.len; i++) {
        if (win->focusEls[i].id == win->focusId) {
            ix = win->focusEls[i].dispatchIx;
            break;
        }
    }
    uint32_t context = gp::KeyContextOf(name);
    for (int i = ix - 1; i >= 0 && i < win->dispatch.len; i--) {
        if (win->dispatch[i].subtreeEnd > ix && win->dispatch[i].context == context) {
            return true;
        }
    }
    return false;
}

void ShellView::OnCmd(ShellView* self, gp::Ctx* cx, const gp::ActionEvent* ev) {
    if (gChordKey && !gCmdSuppressed) {
        gIgnoreRepeatKey = gChordKey;
    }
    gChordKey = 0;
    if (gCmdSuppressed) {
        gCmdSuppressed = false;
        const_cast<gp::ActionEvent*>(ev)->propagate = true;
        return;
    }
    ExecuteCmd(self->win, (int)ev->arg);
    gp::Notify(cx);
}

// orig's OnSidebarSplitterMove: the sidebar ends where the cursor is, never
// narrower than kSidebarMinDx and never leaving less than kMinDocCanvasDx for
// the document
void ShellView::OnSidebarResize(ShellView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // orig sets this while something else owns the pointer, so proximity does
    // not pop the overlay scrollbar out mid-drag
    gOverlayScrollbarSuppressThick = true;
    int border = AppShellFrameBorder(win);
    int sidebarDx = (gSettings->sidebarOnRight ? win->frameRc.dx - (int)ev->event.x : (int)ev->event.x) - border;
    // note: without the min/max(..., curDx), the sidebar will be
    //       stuck at its width if it accidentally got too wide or too narrow
    int curDx = win->sidebarDx;
    int minDx = std::min(kSidebarMinDx, curDx);
    // allow wider than half window (long Favorites names)
    int maxDx = std::max(win->frameRc.dx - kMinDocCanvasDx, curDx);
    // orig's splitter refuses the move and shows IDC_NO
    bool refused = sidebarDx < minDx || sidebarDx > maxDx;
    if (refused != self->splitterRefused) {
        self->splitterRefused = refused;
        gp::Notify(cx);
    }
    if (refused || sidebarDx == win->sidebarDx) {
        return;
    }
    win->sidebarDx = sidebarDx;
    gp::Notify(cx);
}

// orig's HandleCaptionClick(CB_SYSTEM_MENU)
void ShellView::OnSystemMenu(ShellView* self, gp::Ctx*, const gp::ClickEvent*) {
    if (IsMainWindowValidAndNotClosing(self->win)) {
        AppShellOpenSystemMenu(self->win, FromGpui(self->sysMenuBtn));
    }
}

void ShellView::OnSidebarResized(ShellView* self, gp::Ctx* cx, const gp::MouseUpEvent*) {
    gOverlayScrollbarSuppressThick = false;
    self->splitterRefused = false;
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win) || win->sidebarDx == gSettings->sidebarDx) {
        return;
    }
    logf("OnSidebarResized: sidebarDx %d\n", win->sidebarDx);
    gSettings->sidebarDx = win->sidebarDx;
    ScheduleSaveSettings();
    gp::Notify(cx);
}

void ShellView::OnMouseDown(ShellView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    // an Alt released while another window had the keyboard never told us
    self->altDown = ev->modifiers.alt;
    // a click ends the menu bar's keyboard mode, and the Alt that is down is
    // no longer alone
    self->menuKey = 0;
    // (a click on the bar itself opens a menu; one on the menu button armed it)
    bool onMenuBar =
        (self->win->isMenuBarVisible || self->menuBarTemp) && !gp::AppHasMenuBar() && ev->y < (float)kMenuBarDy;
    if (self->menuArmedIdx >= 0 && !onMenuBar && !self->menuArmedByClick) {
        self->menuArmedIdx = -1;
        gp::Notify(cx);
    }
    self->menuArmedByClick = false;
    // orig's WM_APPCOMMAND: the mouse's back / forward buttons, anywhere in
    // the frame
    if (ev->button == gp::MouseButton::NavigateBack || ev->button == gp::MouseButton::NavigateForward) {
        if (IsMainWindowValidAndNotClosing(self->win)) {
            ExecuteCmd(self->win, ev->button == gp::MouseButton::NavigateBack ? CmdNavigateBack : CmdNavigateForward);
            gp::Notify(cx);
        }
        return;
    }
    NavFilesOnMouseDown(self->win, ev->x, ev->y);
    AnnotFilterOnMouseDown(self->win, ev->x, ev->y);
    if (!CommandPaletteOnMouseDown(self->win, ev->x, ev->y)) {
        return;
    }
    gp::WindowStopPropagation(cx);
    gp::Notify(cx);
}

// ng: the keys that are not commands. orig handles them in its canvas /
// frame WndProc: Escape cancels a drag and drops the selection, and the
// keyboard link-hint mode eats plain letters while it is on.
void ShellView::OnKeyDown(ShellView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // gpui runs this listener before it looks the key up in the keymap, and
    // clearing propagate is what stops the command from also firing
    auto* mut = const_cast<gp::KeyEvent*>(ev);
    gChordKey = 0;
    if (gIgnoreRepeatKey && ev->vk == gIgnoreRepeatKey) {
        if (ev->held && !ev->ctrl && !ev->alt) {
            mut->propagate = false;
            return;
        }
        if (!ev->held) {
            gIgnoreRepeatKey = 0;
        }
    }
    bool isChordKey =
        (ev->ctrl || ev->alt) && ev->vk != 0 && ev->vk != VK_CONTROL && ev->vk != VK_MENU && ev->vk != VK_SHIFT;
    if (isChordKey) {
        gChordKey = ev->vk;
    }
    if (self->altDown != ev->alt) {
        // the menus show their access keys from here on
        self->altDown = ev->alt;
        self->menuCues = self->menuCues || ev->alt;
        if (ev->alt) {
            DlgAccelOnAlt();
        }
        gp::Notify(cx);
    }
    // the keyboard is in the menu bar: every key is the bar's
    if (self->menuArmedIdx >= 0) {
        self->menuKey = 0;
        if (ev->vk != 0) {
            MenuBarArmedOnKey(self, cx, ev->vk);
        }
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    bool isMenuKey = !ev->ctrl && !ev->shift && (ev->vk == VK_MENU || (ev->vk == VK_F10 && !ev->alt));
    if (isMenuKey) {
        if (!ev->held) {
            self->menuKey = ev->vk;
        }
    } else if (ev->vk != 0) {
        self->menuKey = 0;
    }
    bool editFocused = IsTextFieldFocused(cx);
    gCmdSuppressed = editFocused && SafeAcceleratorCmd((u16)ev->vk, ev->ctrl, ev->shift, ev->alt) == 0;
    // orig's dialogs are windows of their own with no accelerator table, so
    // no shortcut reaches the document while one is up (Properties keeps the
    // edit table)
    DialogAccels dlgAccels = self->dialogUp ? DialogsAccelTable(win) : DialogAccels::All;
    if (dlgAccels == DialogAccels::None) {
        gCmdSuppressed = true;
    } else if (dlgAccels == DialogAccels::Edit) {
        gCmdSuppressed = SafeAcceleratorCmd((u16)ev->vk, ev->ctrl, ev->shift, ev->alt) == 0;
    }
    // a win32 menu runs a modal loop that takes every key while it is open;
    // an open gpui popup has the focus, so its keys (Esc, arrows, Enter) are
    // left to it instead of also closing a dialog or moving a list under it
    if (IsFocusInKeyContext(cx->win, GStrL("PopupMenu"))) {
        // win32: Alt or F10 leaves an open menu
        if (isMenuKey) {
            self->menuKey = 0;
            auto* st = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
            if (st && st->selected >= 0) {
                gpc::AppMenuBarSelect(st, cx, -1);
                mut->propagate = false;
                gp::Notify(cx);
            }
            return;
        }
        if (!ev->alt && !ev->ctrl && MenuOnAccessKey(self, cx, (u16)ev->vk)) {
            mut->propagate = false;
            gp::Notify(cx);
        }
        return;
    }
    // a context menu does not take the focus; a dialog's open drop-down
    // would have its dialog closed under it
    if (IsTrackedPopupOpen(cx)) {
        if (ev->vk == VK_ESCAPE) {
            DismissTrackedPopup(cx);
            mut->propagate = false;
            gp::Notify(cx);
        }
        return;
    }
    // the palette is a window of its own in orig and owns the keyboard while
    // it is up; here the shell hands it the keys it answers first
    if (CommandPaletteOnKeyDown(win, ev->vk, ev->ctrl, ev->shift)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // the hotkey dialog turns every key into the combination it captures
    if (SetScreenshotHotkeyOnKey(win, (int)ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (self->dialogUp && DialogsOnKeyDown(win, (int)ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's dialogs are modal: a letter that is a dialog's access key goes to
    // the dialog, with Alt or (outside a text field) without
    if (!ev->shift && DlgAccelOnKey(cx, (int)ev->vk, ev->alt, ev->ctrl, editFocused)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // the Merge PDF dialog's page grid: arrows, Delete, Ctrl + A
    if (!editFocused && PdfToolDialogOnKeyDown(win, (int)ev->vk, ev->ctrl, ev->shift)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // the folder picker eats the keys it drives while it has the keyboard
    // (orig gives it its own top-level window, which takes the focus)
    NavKeyResult navRes = NavFilesOnKeyDown(win, ev->vk, ev->ctrl, ev->alt);
    if (navRes == NavKeyResult::Handled) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (navRes == NavKeyResult::Typed) {
        // its filter box has the focus now: the character goes there and the
        // key must not also run a command
        gCmdSuppressed = SafeAcceleratorCmd((u16)ev->vk, ev->ctrl, ev->shift, ev->alt) == 0;
        gp::Notify(cx);
        return;
    }
    // so does the annotation list, another window of its own in orig
    if (AnnotFilterOnKeyDown(win, ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's WM_APPCOMMAND for the browser keys. ng: win32 makes that message
    // in DefWindowProc from the key-down, which gpui does not pass on, so the
    // keys are answered here (the message itself, as a mouse driver or a
    // remote control sends it, is answered in NativeWindow.cpp)
    if (!self->overlayUp && !ev->ctrl && !ev->alt) {
        int appCmd = 0;
        switch (ev->vk) {
            case 0xA6: // VK_BROWSER_BACK
                appCmd = CmdNavigateBack;
                break;
            case 0xA7: // VK_BROWSER_FORWARD
                appCmd = CmdNavigateForward;
                break;
            case 0xA8: // VK_BROWSER_REFRESH
                appCmd = CmdReloadDocument;
                break;
            case 0xAA: // VK_BROWSER_SEARCH
                appCmd = CmdFindFirst;
                break;
            case 0xAB: // VK_BROWSER_FAVORITES
                appCmd = CmdToggleBookmarks;
                break;
        }
        if (appCmd) {
            ExecuteCmd(win, appCmd);
            mut->propagate = false;
            gp::Notify(cx);
            return;
        }
    }
    // orig's WM_CONTEXTMENU from the keyboard (the Apps key, Shift + F10): the
    // frame forwards it to the canvas, which opens its menu at the cursor
    bool isCtxMenuKey = ev->vk == VK_APPS || (ev->vk == VK_F10 && ev->shift && !ev->ctrl && !ev->alt);
    if (ev->vk == VK_APPS && !editFocused && PdfToolDialogContextMenuFromKey(win, cx)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (isCtxMenuKey && !editFocused && !self->overlayUp) {
        if (SidebarContextMenuFromKey(win, cx)) {
            // the focused tree showed its own
        } else if (win->AsFixed()) {
            DocCanvasContextMenuFromKey(win, cx);
        } else if (win->IsCurrentTabAbout()) {
            HomePageContextMenuFromKey(win, cx);
        }
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's AdvanceFocus, from FrameOnChar, TocTreeKeyDown2 and
    // OnLocationEditChar: Tab in the frame, a sidebar tree or the page / chapter
    // box cycles between them. Elsewhere (a dialog, the find bar) Tab is gpui's
    if (ev->vk == VK_TAB && !ev->ctrl && !ev->alt && !self->overlayUp) {
        bool inTabOrder = AppShellIsFrameFocused(win) || IsToolbarPageBoxFocused(win) ||
                          SidebarPanelHasFocus(win, true) || SidebarPanelHasFocus(win, false);
        if (inTabOrder) {
            AdvanceFocus(win, ev->shift);
            mut->propagate = false;
            gp::Notify(cx);
            return;
        }
    }
    if (SidebarOnKeyDown(win, ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (ev->alt && !ev->ctrl && !ev->shift && MenuBarOnAltKey(self, cx, (u16)ev->vk)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (!ev->alt && !ev->ctrl && MenuOnAccessKey(self, cx, (u16)ev->vk)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // the image editor nudges the crop / resize edge with the arrow keys
    if ((ev->vk == VK_LEFT || ev->vk == VK_RIGHT || ev->vk == VK_UP || ev->vk == VK_DOWN) &&
        ImageEditOnArrowKey(win, (int)ev->vk, ev->shift)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // Arrows, Home/End and PageUp/PageDown normally accelerate to scroll /
    // go-to-page commands. While the keyboard selection caret is up they move
    // the caret instead (issues #4684, #4116); orig returns from its
    // PreTranslate before auto-scroll, the reading bar and the nudge see them
    bool isCaretKey = ev->vk == VK_LEFT || ev->vk == VK_RIGHT || ev->vk == VK_UP || ev->vk == VK_DOWN ||
                      ev->vk == VK_HOME || ev->vk == VK_END || ev->vk == VK_PRIOR || ev->vk == VK_NEXT;
    if (!editFocused && !ev->alt && isCaretKey && SelectTextWithKeyboardActive(win) &&
        SelectTextWithKeyboardOnKeyDown(win, ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (!editFocused && !ev->ctrl && !ev->alt && NudgeSelectedAnnotation(win, (int)ev->vk, ev->shift)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // a placement mode and the contents editor own the keys they answer
    if (!ev->alt && AnnotationPlacementOnKeyDown(win, (int)ev->vk)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (!ev->ctrl && !ev->alt && !ev->shift && IsFormFieldEditActive() && FormFieldEditOnKeyDown((int)ev->vk)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (ev->vk == VK_TAB && IsFormFieldEditActive() && FormFieldEditOnTab(ev->shift)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (ExplorerQuickLookOnKeyDown(win, (int)ev->vk, ev->ctrl || ev->shift || ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's FrameOnKeydown / FrameOnChar: a black or white presentation screen
    // swallows a key that is not an accelerator and goes back to the slide
    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        if (SafeAcceleratorCmd((u16)ev->vk, ev->ctrl, ev->shift, ev->alt) == 0) {
            if (ev->vk == VK_ESCAPE || (ev->vk == 0 && ev->ch != 0)) {
                win->ChangePresentationMode(PM_ENABLED);
            }
            mut->propagate = false;
            gp::Notify(cx);
            return;
        }
    }
    if (ev->vk == VK_ESCAPE) {
        if (IsFormFieldEditActive()) {
            CommitFormFieldEdit(false);
        } else if (IsEditingFreeTextInPlace(win)) {
            EndFreeTextInPlaceEdit(false);
        } else if (IsEditingAnnotContents(win)) {
            EndAnnotContentsEdit(false);
        } else if (IsAnnotationTextPopupShown(win)) {
            HideAnnotationTextPopup(win);
        } else if (AnnotFilterOnEscape(win)) {
            // the annotation list is closed now
        } else if (ToolbarOnEscape(win)) {
            // the page box gave the focus back
        } else if (DialogsOnEscape(win)) {
            // the topmost dialog is gone now
        } else if (ReadingAutoScrollIsOn(win)) {
            // orig's PreTranslate: these two take Esc before the frame does
            ReadingAutoScrollStop(win);
        } else if (ReadingBarIsOn(win)) {
            ReadingBarHide(win);
        } else if (IsFindEditFocused(win)) {
            // orig's FindBarWnd::OnKeyDown: Esc goes to the focused window
            HideFindBar(win);
        } else {
            // orig's FrameOnKeydown: the drag is cancelled and a selected
            // annotation leaves its edit mode (issue #5933), and the WM_CHAR
            // that follows still runs OnFrameKeyEsc
            CanvasCancelDrag(win);
            WindowTab* tabEsc = win->CurrentTab();
            if (tabEsc && tabEsc->selectedAnnotation) {
                SetSelectedAnnotation(tabEsc, nullptr);
            }
            if (StopKeyboardLinkFollowing(win)) {
                // the mode is off now
            } else if (StopSelectTextWithKeyboard(win)) {
                // so is the caret
            } else if (CancelAnnotationPlacement(win)) {
                // the placement mode is off now
            } else if (CancelPlacingSignature(win)) {
                // so is the signature rectangle
            } else if (IsFindUIVisible(win)) {
                HideFindBar(win);
            } else if (AbortFinding(win, true)) {
                // the search is stopped now
            } else if (DismissNotificationsOnEsc(win)) {
                // one group of tips is gone now
            } else if (win->showSelection) {
                // clear the user's text/rect selection (ClearSearchResult only clears
                // find-match highlights since issue #5737, so it can't do this anymore)
                DeleteOldSelectionInfo(win, true);
                ClearSearchResult(win); // repaints; also drops any find-match highlights
                ToolbarUpdateStateForWindow(win, false);
            } else if (win->InPresentation() || win->isFullScreen) {
                // orig's OnFrameKeyEsc: Escape leaves the mode it put us in
                ToggleFullScreen(win, win->InPresentation());
            } else if (!win->pdfAnnotationsToolbarEnabled && gSettings->escToExit) {
                // Esc is the cancel key while the Edit PDF toolbar is up, so it
                // must not also quit (issue #6118)
                mut->propagate = false;
                CloseWindowIfCan(win, true);
                return;
            }
        }
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's dialogs let Up / Down drive their list even while the search box
    // has the focus, and Enter runs the default button
    if (ev->vk == VK_UP || ev->vk == VK_DOWN) {
        if (DialogsOnArrowKey(win, ev->vk == VK_UP ? -1 : 1, editFocused)) {
            mut->propagate = false;
            gp::Notify(cx);
            return;
        }
    }
    if (editFocused) {
        return;
    }
    if (ev->vk == VK_RETURN && DialogsOnEnter(win)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's ActivateOnEnter: Enter clicks the focused button (gpui does that
    // for a focused control of its own), or the dialog's default button
    bool noCtrlFocused = cx->win->focusId <= 0 || IsDialogTrapFocused(cx->win);
    if (ev->vk == VK_RETURN && !ev->ctrl && !ev->alt && self->dialogUp && noCtrlFocused &&
        DlgDefaultOnEnter(cx, false)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig runs these two in its PreTranslate, before the accelerators
    if (ReadingAutoScrollOnKey(win, ev->vk, ev->ctrl, ev->shift, ev->alt) ||
        ReadingBarOnKey(win, ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig routes the arrow keys, Enter and Del on the start page to the file
    // list instead of scrolling (issue #1136)
    // only without modifiers: Shift + Up, Alt + Left &c stay accelerators.
    // Ctrl + Enter is no accelerator and reaches orig's canvas as a key
    bool homeKey = !ev->shift && !ev->alt && (!ev->ctrl || ev->vk == VK_RETURN);
    if (homeKey && win->IsCurrentTabAbout() && HomePageOnKeyDown(win, ev->vk, ev->ctrl)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (SelectTextWithKeyboardOnKeyDown(win, ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's FrameOnKeydown: Shift + arrows extend a text selection, numpad
    // * and / rotate, Delete removes the selected annotation
    if (FrameOnKeydown(win, (int)ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's FrameOnSysChar: Alt + 1..9 select a tab
    if (ev->alt && !ev->ctrl && FrameOnSysChar(win, (int)ev->vk)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (ev->ctrl || ev->alt || ev->platform) {
        return;
    }
    if (SelectTextWithKeyboardActive(win) && SelectTextWithKeyboardOnChar(win, ev->vk)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // only unmodified letters type a hint: Shift + F still toggles the mode off
    if (!ev->shift && KeyboardLinkFollowingCapturesKey(win, ev->vk) && KeyboardLinkFollowingOnChar(win, ev->vk)) {
        mut->propagate = false;
        gp::Notify(cx);
        return;
    }
    // orig's FrameOnChar: + = - / b. gpui hands the typed character on in a
    // key event of its own, without a virtual key
    if (ev->vk == 0 && ev->ch != 0) {
        if (SidebarOnChar(win, ev->ch) || FrameOnChar(win, ev->ch, ev->shift)) {
            mut->propagate = false;
            gp::Notify(cx);
        }
    }
}

// ng: a gpui text field takes Enter before the shell's key listener runs.
// orig's ActivateOnEnter clicks the default button from an edit too, so the
// dialogs whose fields have no Enter handler of their own get it here
void ShellView::OnCaptureKey(ShellView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // orig's FindWindowWnd::OnKeyDown: the floating find window walks its
    // results with F3 and with the keys it borrows from the search edit
    if (!self->dialogUp && FindWindowOnKeyDown(win, ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        const_cast<gp::KeyEvent*>(ev)->propagate = false;
        gp::Notify(cx);
        return;
    }
    if (!self->dialogUp) {
        return;
    }
    if (ev->vk != VK_RETURN || ev->ctrl || ev->alt || ev->shift || !IsTextFieldFocused(cx)) {
        return;
    }
    if (gp::InputIsMultiLine(cx->win->input) || IsTrackedPopupOpen(cx)) {
        return;
    }
    if (DlgDefaultOnEnter(cx, true)) {
        const_cast<gp::KeyEvent*>(ev)->propagate = false;
        gp::Notify(cx);
    }
}

// ng: orig's smart tab switcher is a popup window that watches for the Ctrl
// key going up in its PreTranslate; here the shell's root element does
void ShellView::OnKeyUp(ShellView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (gIgnoreRepeatKey && ev->vk == gIgnoreRepeatKey) {
        gIgnoreRepeatKey = 0;
    }
    bool altDown = ev->alt && ev->vk != VK_MENU;
    if (self->altDown != altDown) {
        self->altDown = altDown;
        gp::Notify(cx);
    }
    if (self->menuKey && ev->vk == self->menuKey) {
        self->menuKey = 0;
        // a dialog, the palette or a popup has the keyboard: no menu
        if (!self->overlayUp && !IsTrackedPopupOpen(cx)) {
            MenuBarArm(self, cx);
            gp::Notify(cx);
        }
    }
    if (CommandPaletteOnKeyUp(win, ev->vk)) {
        gp::Notify(cx);
    }
    // ng: Windows sends no key-down for PrtSc, only the key-up; orig's hotkey
    // dialog sees it through a low-level keyboard hook
    if (ev->vk == VK_SNAPSHOT && SetScreenshotHotkeyOnKey(win, VK_SNAPSHOT, ev->ctrl, ev->shift, ev->alt)) {
        gp::Notify(cx);
    }
}

// drains the ui task queue, runs middle-button auto-scroll and repaints while
// a page is still rendering
void ShellView::OnTick(ShellView* self, gp::Ctx* cx, const gp::TickEvent* ev) {
    uitask::DrainQueue();
    AppShellReapClosedWindows();
    NavFilesReapClosedWindow();
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    // ng: gpui reports the interval asked for, not the time that passed; a
    // WM_TIMER comes about every 27 ms for a 16 ms interval and slower still
    // in a background window, so timeouts are counted in real time
    double now = gp::TimeNow();
    double elapsedMs = self->lastTickTime > 0 ? (now - self->lastTickTime) * 1000.0 + self->tickMsRest : ev->ms;
    self->lastTickTime = now;
    int ms = (int)elapsedMs;
    self->tickMsRest = elapsedMs - ms;

    BrowserViewCreatePending(win, cx);
    AIChatTick(win, ms);
    ChangeThemePreviewTick(ms);
    PdfToolDialogTick(win, ms);
    // what moved inside the frame (a sidebar, a toolbar row) moves the canvas
    ToolWindowsFollow(win);
    ToolbarTick(win, ms);
    AutoReloadTick(win, ms);
    DocCanvasAutoScrollTick(win, ms);
    CanvasTickPresentation(win, ms);
    DocCanvasSmoothScrollTick(win, ms);
    ReadingAutoScrollTick(win, ms);
    ReadAloudTick(win, ms);
    OverlayScrollbarsTick(win, ms);
    FindDebounceTick(win, ms);
    SelectTextWithKeyboardBlinkTick(win, ms);
    AnnotFilterTick(win, ms);
    AnnotationNudgeTick(win, ms);
    RefHoverTick(win, ms);
    ForwardSearchMarkTick(win, ms);
    ExpireNotifications(win, ms);
    // ng: orig's stress test re-arms a WM_TIMER after every step
    OnStressTestTimer(win);
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (DocCanvasWantsRepaint(win)) {
        gp::Notify(cx);
    }
}

gp::El* ShellView::Render(ShellView* self, gp::Ctx* cx) {
    // everything drawn this frame is allocated after this point
    ResetTempArena();
    DlgAccelBeginFrame(cx->win);

    MainWindow* win = self->win;
    const gp::Theme& th = gp::ThemeNow(cx->app);
    // CmdExit deletes the window before the loop stops; a frame in flight must
    // not read it
    if (!IsMainWindowValid(win)) {
        return gp::Div(cx->a)->SizeFull()->Bg(th.tokens.background);
    }
    if (!win->menu) {
        RebuildMenuBar(win);
    }
    AppShellSyncMenu(win);
    gp::WinSize ws = gp::WindowSize(cx->win);
    win->frameRc = Rect{0, 0, (int)ws.dipW, (int)ws.dipH};
    win->isMaximized = cx->win && cx->win->maximized;
    // what is left for the document once the menu bar and the tab strip have
    // taken their rows; step 7's canvas measures itself instead. On macOS
    // the menu bar is the system's and takes no row of the window.
    bool nativeMenu = gp::AppHasMenuBar();
    // orig's WM_EXITMENULOOP: hide the menu bar again if it was shown only
    // temporarily
    if (self->menuArmedIdx >= 0) {
        // a click on a title opened its menu: the bar is no longer just armed
        auto* mbOpen = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
        if (mbOpen && mbOpen->selected >= 0) {
            self->menuArmedIdx = -1;
        }
    }
    if (self->menuBarTemp && self->menuArmedIdx < 0) {
        auto* mbState = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
        if (win->isMenuBarVisible || !mbState || mbState->selected < 0) {
            self->menuBarTemp = false;
        }
    }
    bool menuBarShown = (win->isMenuBarVisible || self->menuBarTemp) && !nativeMenu;
    // orig: tabsInTitlebar follows UseTabs
    SetTabsInTitlebar(win, CanHaveTabsInTitlebar() && SettingsUseTabs() && !win->isQuickLook);
    bool showCaption = ShowCaption(win);
    int chromeDy = (menuBarShown ? kMenuBarDy : 0) + (TabsAreVisible(win) ? kTabBarDy : 0);
    if (showCaption) {
        chromeDy = CaptionDy(win, menuBarShown);
    } else {
        win->tabsAvailDx = 0;
    }
    // with ToolbarPosition = bottom it takes a row under the canvas instead
    int toolbarDy = ToolbarDy(win);
    int border = AppShellFrameBorder(win);
    bool tbAtBottom = ToolbarAtBottom();
    if (!tbAtBottom) {
        chromeDy += toolbarDy;
    }
    // the splitter owns the sidebar's width while it is dragged; read it back
    // so the document relayouts with the canvas
    int sidebarDx = 0;
    if (win->uiState.tocVisible || win->uiState.favVisible) {
        sidebarDx = win->sidebarDx + kSplitterDx;
    }
    int bodyDy = win->frameRc.dy - 2 * border - chromeDy - (tbAtBottom ? toolbarDy : 0);
    int aiChatDx = AIChatPanelDx(win);
    int navFilesDx = NavFilesPanelDx(win);
    int bodyDx = win->frameRc.dx - 2 * border;
    win->canvasRc = Rect{border + sidebarDx, border + chromeDy, bodyDx - sidebarDx - aiChatDx - navFilesDx, bodyDy};
#if OS_DARWIN
    if (DisplayModel* dm = win->AsFixed()) {
        dm->renderScale = AppShellRenderScale(win->gpuiWin);
    }
#endif
    // only when it really changed: SetViewPortSize relayouts, which repaints,
    // which would come straight back here
    Size vps = win->GetViewPortSize();
    if (win->ctrl && !(vps == win->lastViewPortSize)) {
        win->lastViewPortSize = vps;
        win->ctrl->SetViewPortSize(vps);
    }

    gp::El* root = gp::Div(cx->a)
                       ->FlexCol()
                       ->SizeFull()
                       ->Bg(th.tokens.background)
                       ->KeyContext(GStrL("SumatraWindow"))
                       ->OnMouseDown(gp::Listen(cx, &ShellView::OnMouseDown), gp::DispatchPhase::Capture)
                       ->CaptureKeyDown(gp::Listen(cx, &ShellView::OnCaptureKey))
                       ->OnKeyDown(gp::Listen(cx, &ShellView::OnKeyDown))
                       ->OnKeyUp(gp::Listen(cx, &ShellView::OnKeyUp))
                       ->OnAction(ActSumatraCmd(), gp::Listen(cx, &ShellView::OnCmd));
    if (border > 0) {
        root->Pad((float)border)->Bg(ToGpui(ThemeControlBackgroundColor()));
    }
    if (showCaption) {
        BuildCaption(self, cx, root, menuBarShown);
    } else {
        if (menuBarShown) {
            root->Child(gp::Div(cx->a)
                            ->W(gp::kFill)
                            ->Shrink0()
                            ->PadX(4)
                            ->Bg(th.tokens.titleBar)
                            ->Child(BuildMenuBar(self, cx)));
        }
        if (gp::El* tabs = TabsUIBuild(win, cx, kTabBarDy)) {
            root->Child(tabs);
        }
    }
    gp::El* toolbar = ToolbarBuild(win, cx);
    if (toolbar && !win->isToolbarOverlay && !tbAtBottom) {
        root->Child(toolbar);
    }
    // what sits over the canvas is built first: a browser-hosted document is a
    // native child window, so the canvas has to know it must step aside
    gp::El* dlg = DialogsBuild(win, cx);
    gp::El* palette = CommandPaletteBuild(win, cx);
    gp::El* annotList = AnnotFilterListBuild(win, cx);
    self->overlayUp = dlg || palette || annotList;
    bool dialogWasUp = self->dialogUp;
    self->dialogUp = dlg != nullptr;
    // a dialog that had the focus itself leaves it on nothing: back to the
    // frame, where a dialog window's goes
    if (dialogWasUp && !self->overlayUp && win->shell && IsDialogTrapFocused(cx->win)) {
        gp::FocusHandleFocus(cx->win, win->shell->frameFocus);
    }
    root->Child(BuildBody(self, cx, self->overlayUp));
    // ng: orig's frame always has a focused window. A gpui window starts with
    // none, and a menu closed from the keyboard goes back to "the focus before
    // it", which was nothing: the keyboard stayed on the vanished popup
    if (!self->overlayUp && self->menuArmedIdx < 0 && !gp::WindowFocused(cx->win).IsValid()) {
        auto* mbState = (gpc::AppMenuBarState*)gp::EntityGet(cx->app, self->menuBarState.id);
        if (!mbState || mbState->selected < 0) {
            gp::FocusHandleFocus(cx->win, win->shell->frameFocus);
        }
    }
    if (toolbar && !win->isToolbarOverlay && tbAtBottom) {
        root->Child(toolbar);
    }

    // in overlay mode the toolbar floats over the canvas, and its drop-down
    // floats over both
    if (toolbar && win->isToolbarOverlay) {
        root->Child(toolbar);
    }
    if (gp::El* strip = ToolbarOverlayBuild(win, cx)) {
        root->Child(strip);
    }
    // orig floats the find bar over the frame, pinned to its right edge
    if (gp::El* findBar = FindBarBuild(win, cx)) {
        root->Child(findBar);
    }
    // the floating variant (SearchUIFloating): orig's owned tool window
    if (gp::El* findWin = FindWindowBuild(win, cx)) {
        root->Child(findWin);
    }
    if (dlg) {
        root->Child(dlg);
    }
    // the dropped list of an editable combo box (a dialog's, the find bar's)
    if (gp::El* comboList = DialogComboListBuild(cx)) {
        root->Child(comboList);
    }
    if (palette) {
        root->Child(palette);
    }
    if (annotList) {
        root->Child(annotList);
    }

    return root;
}

// --- creation ---------------------------------------------------------------

static void RunWindowCloseRequest(MainWindow* win) {
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return;
    }
    win->isClosePending = false;
    RequestCloseWindow(win, true);
}

static bool OnWindowShouldClose(void* data, gp::Window*) {
    auto* win = (MainWindow*)data;
    if (!IsMainWindowValid(win) || win->isBeingClosed) {
        return true;
    }
    if (!win->isClosePending) {
        win->isClosePending = true;
        uitask::Post(MkFunc0(RunWindowCloseRequest, win), "WindowCloseRequest");
    }
    return false;
}

MainWindow* AppShellCreateWindow(gp::App* app, int dipW, int dipH) {
    bool isFirst = (len(gWindows) == 0);
    gApp = app;
    auto* ui = new ShellUI();
    ui->view = gp::EntityNew<ShellView>(app);
    ui->frameFocus = gp::FocusHandleNew(app);
    auto* view = (ShellView*)gp::EntityGet(app, ui->view.id);

    // orig sets tabsInTitlebar from UseTabs when it creates the frame
    gp::WinOpts opts;
    opts.clientTitleBar = CanHaveTabsInTitlebar() && SettingsUseTabs();
    gp::Window* gw = gp::WindowOpenView(app, GStrL("SumatraPDF"), dipW, dipH, ui->view.id, opts);
    auto* win = new MainWindow(gw);
    win->shell = ui;
    win->tabsInTitlebar = opts.clientTitleBar;
    // orig's IsMenubarVisible()
    win->isMenuBarVisible = SettingsUseTabs() ? gSettings->showMenubarWithTabs : gSettings->showMenubar;
    VecAppend(gWindows, win);
    gp::WindowOnShouldClose(gw, OnWindowShouldClose, win);
    view->win = win;
    view->menuBarState = gp::EntityNewState<gpc::AppMenuBarState>(app);

    if (isFirst) {
        BindKeys();
        // uitask::Post from a worker wakes the main loop; the interval is the
        // fallback that also expires notifications
        uitask::SetWakeupFn([] { gp::ExecPost(gp::MkFunc0Void(uitask::DrainQueue)); });
    }
    RebuildMenuBar(win);
    CreateToolbar(win);
    UpdateWindowTitle(win);

    DocCanvasHookWindow(win, gw);
    OverlayScrollbarHookWindow(gw);
    // ng: orig runs auto-scroll on a 10 ms timer and repaints a rendering page
    // on a 500 ms one; one interval drives both here
    gp::WindowSetInterval(gw, 16, gp::ListenTo(ui->view, &ShellView::OnTick));
    AppShellEnableFileDrop(win);
    logf("AppShellCreateWindow: window 0x%p, %d windows open\n", win, len(gWindows));
    return win;
}

int AppShellRun(gp::App* app) {
    return gp::AppRun(app);
}
