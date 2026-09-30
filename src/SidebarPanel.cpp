/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Translations.h"
#include "Theme.h"
#include "SvgIcons.h"
#include "Favorites.h"
#include "PageThumbnails.h"
#include "TableOfContents.h"
#include "SidebarPanel.h"

constexpr int kViewIconDx = 16;
constexpr int kViewIconPad = 3;

static const char* ViewIcon(SidebarView v) {
    switch (v) {
        case SidebarView::Bookmarks:
            return gIconSidebarBookmarks;
        case SidebarView::Thumbnails:
            return gIconHomeThumbnails;
        default:
            return gIconSidebarFavorites;
    }
}

static Str ViewName(SidebarView v) {
    switch (v) {
        case SidebarView::Bookmarks:
            return Tr("Bookmarks");
        case SidebarView::Thumbnails:
            return Tr("Thumbnails");
        default:
            return Tr("Favorites");
    }
}

// the window's controls for a view, and the layout that stacks them
static ILayout* ViewLayout(MainWindow* win, SidebarView v) {
    switch (v) {
        case SidebarView::Bookmarks:
            return win->tocViewLayout;
        case SidebarView::Thumbnails:
            return win->pageThumbs;
        default:
            return win->favViewLayout;
    }
}

static int ViewControls(MainWindow* win, SidebarView v, ControlBase* out[2]) {
    switch (v) {
        case SidebarView::Bookmarks:
            out[0] = win->tocFilterEdit;
            out[1] = win->tocTreeView;
            return 2;
        case SidebarView::Favorites:
            out[0] = win->favFilterEdit;
            out[1] = win->favTreeView;
            return 2;
        default:
            return 0;
    }
}

bool IsSidebarViewAvailable(MainWindow* win, SidebarView v) {
    switch (v) {
        case SidebarView::Bookmarks:
            return win->IsDocLoaded() && win->ctrl && win->ctrl->HasToc();
        case SidebarView::Thumbnails:
            return CanShowThumbnails(win->CurrentTab());
        default:
            return !gPluginMode && CanAccessDisk();
    }
}

// the visible sidebar panel that shows v, if any
SidebarPanel* SidebarPanelShowing(MainWindow* win, SidebarView v) {
    SidebarPanel* top = win->sidebarTop;
    SidebarPanel* bottom = win->sidebarBottom;
    if (top && win->uiState.sidebarTopVisible && top->view == v) {
        return top;
    }
    if (bottom && win->uiState.sidebarBottomVisible && bottom->view == v) {
        return bottom;
    }
    return nullptr;
}

bool IsSidebarViewShown(MainWindow* win, SidebarView v) {
    return SidebarPanelShowing(win, v) != nullptr;
}

static SidebarPanel* OtherPanel(SidebarPanel* p) {
    MainWindow* win = p->win;
    return p == win->sidebarTop ? win->sidebarBottom : win->sidebarTop;
}

static void SaveSidebarViews(MainWindow* win) {
    if (WindowTab* tab = win->CurrentTab()) {
        tab->sidebarView = win->sidebarTop->view;
    }
    str::ReplaceWithCopy(&gSettings->sidebarBottomView, SidebarViewToStr(win->sidebarBottom->view));
}

// p shows v from now on; if the other panel did, it takes p's view (swap)
static void SetPanelView(SidebarPanel* p, SidebarView v) {
    SidebarPanel* other = OtherPanel(p);
    if (other->view == v) {
        other->view = p->view;
    }
    p->view = v;
    SaveSidebarViews(p->win);
}

// The top panel's view is the document's, the bottom's is app-wide, so they can
// collide on a tab switch: the bottom one gives way
void ResolveSidebarViews(MainWindow* win) {
    SidebarPanel* top = win->sidebarTop;
    SidebarPanel* bottom = win->sidebarBottom;
    if (!top || !bottom) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    top->view = tab ? tab->sidebarView : SidebarView::Bookmarks;
    if (bottom->view != top->view) {
        return;
    }
    const SidebarView order[] = {SidebarView::Favorites, SidebarView::Bookmarks, SidebarView::Thumbnails};
    for (SidebarView v : order) {
        if (v != top->view) {
            bottom->view = v;
            break;
        }
    }
    SaveSidebarViews(win);
}

static bool RequestedTopVisible(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return false;
    }
    return win->presentation == PM_ENABLED ? tab->showTocPresentation : tab->showToc;
}

// shows the panels again as they are to be for the current tab
void ApplySidebarPanels(MainWindow* win) {
    SetSidebarVisibility(win, RequestedTopVisible(win), gSettings->showFavorites);
}

// First free panel: the top one if it's hidden, else the bottom one if it is,
// else the top one switches to v. Without a tab the top panel has nowhere to
// keep its view (e.g. Favorites on the home page), so the bottom one shows v
void ShowSidebarView(MainWindow* win, SidebarView v) {
    if (!IsSidebarViewAvailable(win, v)) {
        return;
    }
    if (SidebarPanel* shown = SidebarPanelShowing(win, v)) {
        FocusSidebarPanel(shown);
        return;
    }
    SidebarPanel* p = win->sidebarTop;
    bool topTaken = win->uiState.sidebarTopVisible && !win->uiState.sidebarBottomVisible;
    if (topTaken || !win->CurrentTab()) {
        p = win->sidebarBottom;
    }
    SetPanelView(p, v);
    bool top = p == win->sidebarTop || RequestedTopVisible(win);
    bool bottom = p == win->sidebarBottom || gSettings->showFavorites;
    SetSidebarVisibility(win, top, bottom, SidebarResizeFrame::Adjust);
    FocusSidebarPanel(p);
}

void HideSidebarView(MainWindow* win, SidebarView v) {
    SidebarPanel* p = SidebarPanelShowing(win, v);
    if (!p) {
        return;
    }
    bool top = p != win->sidebarTop && RequestedTopVisible(win);
    bool bottom = p != win->sidebarBottom && gSettings->showFavorites;
    SetSidebarVisibility(win, top, bottom, SidebarResizeFrame::Adjust);
}

static void OnViewClick(SidebarPanel* p, SidebarView v) {
    MainWindow* win = p->win;
    if (p->view == v || !IsSidebarViewAvailable(win, v)) {
        return;
    }
    SetPanelView(p, v);
    SetSidebarVisibility(win, RequestedTopVisible(win), gSettings->showFavorites);
    FocusSidebarPanel(p);
}

static void OnBookmarksClick(SidebarPanel* p, VirtMouseEvent*) {
    OnViewClick(p, SidebarView::Bookmarks);
}

static void OnThumbnailsClick(SidebarPanel* p, VirtMouseEvent*) {
    OnViewClick(p, SidebarView::Thumbnails);
}

static void OnFavoritesClick(SidebarPanel* p, VirtMouseEvent*) {
    OnViewClick(p, SidebarView::Favorites);
}

// ✕: hides the panel; on the Favorites tab, closes the tab
static void OnCloseClick(SidebarPanel* p, VirtMouseEvent*) {
    MainWindow* win = p->win;
    if (p->kind == SidebarPanelKind::FavoritesTab) {
        if (WindowTab* favTab = FindFavoritesTab(win)) {
            CloseTab(favTab, false);
        }
        return;
    }
    bool top = p != win->sidebarTop && RequestedTopVisible(win);
    bool bottom = p != win->sidebarBottom && gSettings->showFavorites;
    SetSidebarVisibility(win, top, bottom, SidebarResizeFrame::Adjust);
}

// where the keyboard goes in the panel: its tree, or the panel for thumbnails
HWND SidebarPanelFocusHwnd(SidebarPanel* p) {
    MainWindow* win = p->win;
    switch (p->view) {
        case SidebarView::Bookmarks:
            return win->tocTreeView->hwnd;
        case SidebarView::Favorites:
            return win->favTreeView->hwnd;
        default:
            return p->hwnd;
    }
}

bool SidebarPanelHasFocus(SidebarPanel* p) {
    HWND focus = GetFocus();
    return p && focus && (focus == p->hwnd || IsChild(p->hwnd, focus));
}

// the panel's HWND shows only on the next relayout (see SetSidebarVisibility),
// so whether it's to show comes from uiState
void FocusSidebarPanel(SidebarPanel* p) {
    if (!p) {
        return;
    }
    MainWindow* win = p->win;
    bool shown = p->kind == SidebarPanelKind::FavoritesTab;
    shown |= p == win->sidebarTop && win->uiState.sidebarTopVisible;
    shown |= p == win->sidebarBottom && win->uiState.sidebarBottomVisible;
    if (!shown) {
        return;
    }
    HwndSetFocus(SidebarPanelFocusHwnd(p));
    if (p->view == SidebarView::Thumbnails && p->root) {
        p->root->SetFocus(p->win->pageThumbs);
    }
}

void LayoutSidebarPanel(SidebarPanel* p) {
    if (!p || !p->layout) {
        return;
    }
    Rect rc = HwndClientRect(p->hwnd);
    if (rc.IsEmpty()) {
        return;
    }
    bool firstLayout = p->layout->lastBounds.IsEmpty();
    if (p->layout->lastBounds.dx != rc.dx || p->layout->lastBounds.dy != rc.dy) {
        LayoutTreeToSize(p->hwnd, p->layout, {rc.dx, rc.dy}, &p->root);
        // a new layout drops the root's focus: the focused thumbnails keep it
        bool thumbsFocused = p->hosted && p->hosted == p->win->pageThumbs && GetFocus() == p->hwnd;
        if (thumbsFocused && p->root) {
            p->root->SetFocus(p->win->pageThumbs);
        }
    }
    if (firstLayout && p->view == SidebarView::Bookmarks) {
        RefreshTocTreeIfNeeded(p->win);
    }
}

// after the view changed: lay out again, and let a hidden panel's virtual
// controls go (a view that moved on must not be painted or hit-tested here)
void RelayoutSidebarPanel(SidebarPanel* p) {
    if (!p || !p->layout) {
        return;
    }
    p->layout->lastBounds = {};
    Rect rc = HwndClientRect(p->hwnd);
    if (rc.IsEmpty()) {
        RefreshVirtTops(p->hwnd, p->layout, {}, &p->root);
        return;
    }
    LayoutSidebarPanel(p);
}

static bool PanelHostsThumbnails(SidebarPanel* p) {
    return p->hosted && p->hosted == p->win->pageThumbs;
}

static bool IsSidebarPanelHwnd(MainWindow* win, HWND hwnd) {
    SidebarPanel* panels[] = {win->sidebarTop, win->sidebarBottom, win->favoritesTabPanel};
    for (SidebarPanel* p : panels) {
        if (p && p->hwnd == hwnd) {
            return true;
        }
    }
    return false;
}

static LRESULT CALLBACK WndProcSidebarPanel(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*subclassId*/,
                                            DWORD_PTR data) {
    SidebarPanel* p = (SidebarPanel*)data;
    MainWindow* win = p->win;

    // a tree view keeps notifying the panel it was created in after it moved
    // to another one, which TryReflectMessages() ignores (not its parent)
    if (msg == WM_NOTIFY) {
        HWND from = ((NMHDR*)lp)->hwndFrom;
        ControlBase* ctrl = ControlFromHwnd(from);
        bool moved = ctrl && GetParent(from) != hwnd;
        if (moved && IsSidebarPanelHwnd(win, GetParent(from))) {
            return ctrl->DispatchNotifyReflect(wp, lp);
        }
    }

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    // the focus ring shows while the thumbnails have the keyboard
    bool thumbs = PanelHostsThumbnails(p);
    if ((msg == WM_SETFOCUS || msg == WM_KILLFOCUS) && p->root && thumbs) {
        p->root->SetFocus(msg == WM_SETFOCUS ? win->pageThumbs : nullptr);
    }

    // with the thumbnails focused, every key they don't take goes to the canvas,
    // as if it had the focus (Tab included: it moves the focus on)
    bool isKey = msg == WM_KEYDOWN || msg == WM_CHAR;
    bool thumbsKey = msg == WM_KEYDOWN && ThumbnailsTakeKey(win, hwnd, wp);
    if (isKey && thumbs && !thumbsKey) {
        SendMessageW(win->hwndFrame, msg, wp, lp);
        return 0;
    }

    // the header and the thumbnails are virtual controls, so this window paints
    // them and hands them their input
    if (VirtHostOnMessage(hwnd, p->root, msg, wp, lp, res, ThemeControlBackgroundColor())) {
        return res;
    }

    if (msg == WM_SIZE) {
        LayoutSidebarPanel(p);
    }
    // DefWindowProc() passes an unhandled wheel on to the frame, which scrolls
    // the document
    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void UpdateViewIcons(SidebarPanel* p, int dpi) {
    int sz = RoundUp(DpiScaleByDpi(dpi, kViewIconDx), 4);
    int pad = DpiScaleByDpi(dpi, kViewIconPad);
    for (int i = 0; i < kSidebarViewCount; i++) {
        VirtIconButton* b = p->viewBtns[i];
        SidebarView v = (SidebarView)i;
        Str svg = Str(ViewIcon(v));
        b->pixmap = GetCachedPixmapForSvg(svg, sz, sz);
        b->pixmapDisabled = GetCachedPixmapForSvg(svg, sz, sz, ThemeWindowTextDisabledColor());
        b->padding = Insets{pad, pad, pad, pad};
        b->isSelected = p->view == v && p->hosted;
        b->SetIsEnabled(IsSidebarViewAvailable(p->win, v));
        b->Invalidate();
    }
}

SidebarPanel* CreateSidebarPanel(MainWindow* win, SidebarPanelKind kind) {
    auto* p = new SidebarPanel();
    p->win = win;
    p->kind = kind;
    if (kind == SidebarPanelKind::Bottom) {
        p->view = SidebarViewFromStr(gSettings->sidebarBottomView, SidebarView::Favorites);
    } else if (kind == SidebarPanelKind::FavoritesTab) {
        p->view = SidebarView::Favorites;
    }

    HMODULE hmod = GetModuleHandle(nullptr);
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    int dx = gSettings->sidebarDx;
    p->hwnd = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, win->hwndFrame, nullptr, hmod, nullptr);

    // [B][T][F] ... [x]; the Favorites tab has only the ✕
    auto* header = new HBox();
    header->alignMain = MainAxisAlign::MainStart;
    header->alignCross = CrossAxisAlign::CrossCenter;
    using ClickFn = void (*)(SidebarPanel*, VirtMouseEvent*);
    const ClickFn onClick[kSidebarViewCount] = {OnBookmarksClick, OnThumbnailsClick, OnFavoritesClick};
    for (int i = 0; i < kSidebarViewCount; i++) {
        auto* b = new VirtIconButton();
        b->onClick = MkFunc1(onClick[i], p);
        b->SetIsVisible(kind != SidebarPanelKind::FavoritesTab);
        p->viewBtns[i] = b;
        header->AddChild(b);
    }
    header->AddChild(new Spacer(0, 0), 1);
    p->closeBtn = new VirtCloseButton();
    p->closeBtn->onClick = MkFunc1(OnCloseClick, p);
    header->AddChild(p->closeBtn);

    p->noView = new Spacer(0, 0);
    p->layout = new VBox();
    p->layout->alignMain = MainAxisAlign::MainStart;
    p->layout->alignCross = CrossAxisAlign::Stretch;
    p->layout->AddChild(header);
    p->layout->AddChild(p->noView, 1);

    p->subclassId = NextSubclassId();
    if (!SetWindowSubclass(p->hwnd, WndProcSidebarPanel, p->subclassId, (DWORD_PTR)p)) {
        // can fail under low memory / desktop heap exhaustion, so don't assert
        logf("CreateSidebarPanel: SetWindowSubclass() failed, err: %d\n", (int)GetLastError());
        p->subclassId = 0;
    }
    ApplyCloseButtonDpi(p->closeBtn, DpiGetForHwnd(p->hwnd));
    UpdateViewIcons(p, DpiGetForHwnd(p->hwnd));
    return p;
}

// the view's layout goes back to the window: the panel doesn't own it
void DeleteSidebarPanel(SidebarPanel* p) {
    if (!p) {
        return;
    }
    if (p->subclassId != 0) {
        RemoveWindowSubclass(p->hwnd, WndProcSidebarPanel, p->subclassId);
    }
    p->layout->children[1].layout = p->noView;
    delete p->layout;
    delete p->root;
    delete p;
}

static void HostView(SidebarPanel* p, ILayout* view) {
    p->hosted = view;
    p->layout->children[1].layout = view ? view : p->noView;
}

// shows a view's native controls in host, or hides them
static void PlaceViewControls(MainWindow* win, SidebarView v, SidebarPanel* host) {
    ControlBase* ctrls[2];
    int n = ViewControls(win, v, ctrls);
    for (int i = 0; i < n; i++) {
        HWND hwnd = ctrls[i]->hwnd;
        if (host && GetParent(hwnd) != host->hwnd) {
            SetParent(hwnd, host->hwnd);
        }
        // SetIsVisible() only flips WS_VISIBLE, which Windows doesn't act on
        // until the window moves: ShowWindow() first, it's a no-op after
        ShowWindow(hwnd, host ? SW_SHOW : SW_HIDE);
        ctrls[i]->SetIsVisible(host != nullptr);
    }
}

// Puts each view in the panel that shows it: the top or bottom panel, or the
// Favorites tab's while that's the current tab. The others are hidden
void AttachSidebarViews(MainWindow* win) {
    SidebarPanel* top = win->sidebarTop;
    SidebarPanel* bottom = win->sidebarBottom;
    SidebarPanel* favTab = win->favoritesTabPanel;
    if (!top || !bottom || !favTab) {
        return;
    }
    bool favTabActive = win->CurrentTab() && win->CurrentTab()->IsFavoritesTab();
    SidebarPanel* hostOf[kSidebarViewCount]{};
    for (int i = 0; i < kSidebarViewCount; i++) {
        SidebarView v = (SidebarView)i;
        if (v == SidebarView::Favorites && favTabActive) {
            hostOf[i] = favTab;
        } else if (top->view == v) {
            hostOf[i] = top;
        } else if (bottom->view == v) {
            hostOf[i] = bottom;
        }
    }
    SidebarPanel* panels[] = {top, bottom, favTab};
    for (SidebarPanel* p : panels) {
        ILayout* view = nullptr;
        if (hostOf[(int)p->view] == p) {
            view = ViewLayout(win, p->view);
        }
        HostView(p, view);
    }
    for (int i = 0; i < kSidebarViewCount; i++) {
        PlaceViewControls(win, (SidebarView)i, hostOf[i]);
    }
    PageThumbnailsCtrl* thumbs = win->pageThumbs;
    thumbs->SetIsVisible(hostOf[(int)SidebarView::Thumbnails] != nullptr);
    if (!SidebarPanelHasFocus(hostOf[(int)SidebarView::Thumbnails])) {
        // a panel's root forgets its focus when its controls change, the
        // control's flag stays: that would leave the focus ring on
        thumbs->SetFlag(vwfFocused, false);
    }
    UpdateSidebarThumbnails(win);
    for (SidebarPanel* p : panels) {
        UpdateViewIcons(p, DpiGetForHwnd(p->hwnd));
        RelayoutSidebarPanel(p);
    }
}

void UpdateSidebarPanelsText(MainWindow* win) {
    SidebarPanel* panels[] = {win->sidebarTop, win->sidebarBottom, win->favoritesTabPanel};
    for (SidebarPanel* p : panels) {
        if (!p) {
            continue;
        }
        for (int i = 0; i < kSidebarViewCount; i++) {
            p->viewBtns[i]->SetTooltip(ViewName((SidebarView)i));
        }
    }
}

void UpdateSidebarPanelsIcons(MainWindow* win) {
    SidebarPanel* panels[] = {win->sidebarTop, win->sidebarBottom, win->favoritesTabPanel};
    for (SidebarPanel* p : panels) {
        if (p) {
            UpdateViewIcons(p, DpiGetForHwnd(p->hwnd));
        }
    }
}

void UpdateSidebarPanelsDpi(MainWindow* win, int dpi) {
    SidebarPanel* panels[] = {win->sidebarTop, win->sidebarBottom, win->favoritesTabPanel};
    for (SidebarPanel* p : panels) {
        if (!p) {
            continue;
        }
        ApplyCloseButtonDpi(p->closeBtn, dpi);
        UpdateViewIcons(p, dpi);
        // force a layout even if the panel's size in pixels is unchanged
        p->layout->lastBounds = {};
    }
}
