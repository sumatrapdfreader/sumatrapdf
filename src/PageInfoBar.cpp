/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Win.h"
#include "base/UITask.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"
#include "gui/VirtHost.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "Annotation.h"
#include "TextSelection.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Theme.h"
#include "SumatraPDF.h"
#include "SumatraPDFCommon.h"
#include "PageInfoBar.h"

constexpr int kPageInfoBarDy = 22;
constexpr int kPageInfoBarPadDx = 8;

// how the bar was last laid out
enum class PageInfoBarPlace {
    None,
    Row,
    Overlay,
};

// a host per window; its two texts are rebuilt when they change
struct PageInfoBar {
    VirtHost* host = nullptr;
    PageInfoBarPlace place = PageInfoBarPlace::None;
    Str builtLeft;
    Str builtRight;
    int builtDpi = 0;
    Color builtBg = 0;
    Color builtText = 0;

    ~PageInfoBar() {
        str::Free(builtLeft);
        str::Free(builtRight);
        delete host;
    }
};

bool PageInfoBarTakesRow(MainWindow* win) {
    return PageInfoBarShown(win) && GetPageInfoBarMode() == PageInfoBarMode::Bottom;
}

bool PageInfoBarOverlays(MainWindow* win) {
    return PageInfoBarShown(win) && GetPageInfoBarMode() == PageInfoBarMode::BottomOverlay;
}

int PageInfoBarDy(MainWindow* win) {
    return DpiScaleByDpi(DpiGetForHwnd(win->hwndFrame), kPageInfoBarDy);
}

HWND PageInfoBarEnsure(MainWindow* win) {
    if (win->pageInfoBar) {
        return win->pageInfoBar->host->native;
    }
    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStrL(L"SumatraPageInfoBar");
    args.initialSize = {100, PageInfoBarDy(win)};
    args.bgColor = ThemeControlBackgroundColor();
    args.isRtl = IsUIRtl();
    args.visible = false;
    args.noActivate = true;
    args.clipSiblings = true;
    args.userData = win;
    VirtHost* host = VirtHost::Create(args);
    if (!host) {
        return nullptr;
    }
    win->pageInfoBar = new PageInfoBar();
    win->pageInfoBar->host = host;
    return host->native;
}

void PageInfoBarSetVisible(MainWindow* win, bool visible) {
    PageInfoBar* bar = win->pageInfoBar;
    if (!bar) {
        return;
    }
    if (!visible) {
        bar->place = PageInfoBarPlace::None;
    } else {
        bar->place = PageInfoBarTakesRow(win) ? PageInfoBarPlace::Row : PageInfoBarPlace::Overlay;
    }
    bar->host->Show(visible);
}

// over the bottom of the canvas's client area, so not over its scrollbars
void PageInfoBarPlaceOverlay(MainWindow* win) {
    PageInfoBar* bar = win->pageInfoBar;
    if (!bar || bar->place != PageInfoBarPlace::Overlay) {
        return;
    }
    Rect cr = HwndClientRect(win->hwndCanvas);
    POINT pt{0, 0};
    MapWindowPoints(win->hwndCanvas, win->hwndFrame, &pt, 1);
    int dy = std::min(PageInfoBarDy(win), cr.dy);
    Rect r{pt.x, pt.y + cr.dy - dy, cr.dx, dy};
    bar->host->SetPos(r, true);
}

void DeletePageInfoBar(MainWindow* win) {
    delete win->pageInfoBar;
    win->pageInfoBar = nullptr;
}

// the file on the left, the current page on the right; the left one gives
// way when the window is narrow
void PageInfoBarUpdate(MainWindow* win) {
    PageInfoBar* bar = win->pageInfoBar;
    if (!bar || bar->place == PageInfoBarPlace::None) {
        return;
    }
    TempStr left;
    TempStr right;
    PageInfoBarTextTemp(win, &left, &right);
    int dpi = DpiGetForHwnd(win->hwndFrame);
    Color bg = ThemeControlBackgroundColor();
    Color txt = ThemeWindowTextColor();
    bool same = str::Eq(bar->builtLeft, left) && str::Eq(bar->builtRight, right) && bar->builtDpi == dpi &&
                bar->builtBg == bg && bar->builtText == txt;
    if (same) {
        return;
    }
    str::ReplaceWithCopy(&bar->builtLeft, left);
    str::ReplaceWithCopy(&bar->builtRight, right);
    bar->builtDpi = dpi;
    bar->builtBg = bg;
    bar->builtText = txt;

    VirtHost* host = bar->host;
    host->bgColor = bg;
    PlatformFont* font = GetAppFontForDpi(dpi);
    int pad = DpiScaleByDpi(dpi, kPageInfoBarPadDx);
    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->gap = pad * 2;
    row->AddChild(NewVirtText({.s = left, .font = font, .textColor = txt, .isRtl = IsUIRtl(), .ellipsis = true}), 1);
    // right-aligned: what the measured width misses spills into the gap, not off the bar
    row->AddChild(
        NewVirtText({.s = right, .font = font, .textColor = txt, .align = VirtTextAlign::Right, .isRtl = IsUIRtl()}));
    host->SetLayout(new Padding(row, Insets{0, pad, 0, pad}));
    host->Invalidate();
}

TempStr PageInfoBarGeometryTemp(MainWindow* win) {
    Rect c = ChildPosWithinParent(win->hwndCanvas);
    Rect cr = HwndClientRect(win->hwndCanvas);
    TempStr canvas = fmt("canvas=%d,%d,%d,%d", c.x, c.y, cr.dx, cr.dy);
    PageInfoBar* bar = win->pageInfoBar;
    if (!bar || bar->place == PageInfoBarPlace::None || !HwndIsVisible(bar->host->native)) {
        return fmt("%s bar=none", canvas);
    }
    Rect b = ChildPosWithinParent(bar->host->native);
    return fmt("%s bar=%d,%d,%d,%d", canvas, b.x, b.y, b.dx, b.dy);
}

// shown, hidden or moved: the frame lays it out again; otherwise only its text
void PageInfoBarChanged(MainWindow* win) {
    if (!IsMainWindowValid(win)) {
        return;
    }
    PageInfoBarPlace want = PageInfoBarPlace::None;
    if (PageInfoBarTakesRow(win)) {
        want = PageInfoBarPlace::Row;
    } else if (PageInfoBarOverlays(win)) {
        want = PageInfoBarPlace::Overlay;
    }
    PageInfoBarPlace have = win->pageInfoBar ? win->pageInfoBar->place : PageInfoBarPlace::None;
    if (want != have) {
        ScheduleUiUpdate(win, kUiForceRelayout | kUiRelayout);
        return;
    }
    PageInfoBarUpdate(win);
}
