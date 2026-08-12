/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtWnd.h"

#include "Theme.h"

// Forward declaration - defined in MainWindow.cpp
struct MainWindow;
MainWindow* FindMainWindowByHwnd(HWND hwnd);

//--- Tabs
//
// Each tab is a TabWnd in an HBox that lays them out along the bar, with
// the tab's ✕ as a child of the tab. The control keeps the HWND (it owns the
// drag loop, which needs capture and screen coordinates) and the tab list;
// everything on screen belongs to the tree.

static Kind kindTabs = "tabs";
static Kind kindTabWnd = "tabWnd";

// non-selected tabs narrower than this hide their close button so that
// clicks drag/select instead of accidentally closing the tab
constexpr int kMinTabWidthForClose = 64;

using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::Graphics;
using Gdiplus::SolidBrush;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringFormat;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;

// hwnd is kept LTR (like the canvas); UI direction comes from the parent frame
static bool IsTabsRtl(HWND hwnd) {
    HWND parent = GetParent(hwnd);
    return parent && HwndIsRtl(parent);
}

TabInfo::~TabInfo() {
    str::Free(text);
    str::Free(tooltip);
}

static Gdiplus::Color GdipCol(COLORREF c) {
    return GdiRgbFromCOLORREF(c);
}

static COLORREF TabTextColorForBackground(COLORREF tabBg) {
    COLORREF text = ThemeWindowTextColor();
    if (abs((int)GetLightness(text) - (int)GetLightness(tabBg)) >= 80) {
        return text;
    }
    return IsLightColor(tabBg) ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

//--- TabWnd: one tab

// paints the tab (background, title, dirty dot) and hosts its ✕. It doesn't own
// its TabInfo: the control's `tabs` does
struct TabWnd : VirtWnd {
    TabsCtrl* tabsCtrl = nullptr;
    TabInfo* ti = nullptr;
    VirtCloseButton* closeBtn = nullptr;
    Size idealSize;
    // the ✕ glyph itself, inside the close button's larger hit area
    Rect rClose;

    TabWnd();
    ~TabWnd() override = default;

    int Idx();
    bool IsSelected();
    bool IsUnderMouse();
    bool CloseVisible();
    COLORREF BgColor();

    Size GetIdealSize() override;
    void SetBounds(Rect) override;
    void Paint(VirtPaintCtx&) override;
    bool OnMouseDown(VirtMouseEvent&) override;
    bool OnMouseUp(VirtMouseEvent&) override;
    TempStr GetTooltipTemp(Point) override;
};

static void TabCloseClicked(TabWnd*, VirtMouseEvent*);

TabWnd::TabWnd() {
    kind = kindTabWnd;
    closeBtn = new VirtCloseButton();
    closeBtn->onClick = MkFunc1(TabCloseClicked, this);
    closeBtn->visibility = Visibility::Collapse;
    AddChild(closeBtn);
}

int TabWnd::Idx() {
    return tabsCtrl ? tabsCtrl->tabs.Find(ti) : -1;
}

bool TabWnd::IsSelected() {
    int idx = Idx();
    if (idx < 0) {
        return false;
    }
    TabsCtrl* tc = tabsCtrl;
    if (tc->IsValidIdx(tc->tabForceShowSelected)) {
        return idx == tc->tabForceShowSelected;
    }
    return idx == tc->selectedIdx;
}

bool TabWnd::IsUnderMouse() {
    return tabsCtrl && tabsCtrl->tabHighlighted == Idx();
}

COLORREF TabWnd::BgColor() {
    COLORREF selected = ThemeControlBackgroundColor();
    bool isSelected = IsSelected();
    bool isUnderMouse = IsUnderMouse();
    // a tab with a color of its own keeps it, shaded when it isn't selected
    if (!IsSpecialColor(ti->tabColor)) {
        if (isSelected) {
            return ti->tabColor;
        }
        return AccentColor(ti->tabColor, isUnderMouse ? 35 : 25);
    }
    if (isSelected) {
        return selected;
    }
    return AccentColor(selected, isUnderMouse ? 35 : 25);
}

Size TabWnd::GetIdealSize() {
    return idealSize;
}

// the ✕ is inset from the tab's edge, but its hit area is the whole gutter
// (~40 DIP, full height) so it stays easy to hit
void TabWnd::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    HWND hwnd = GetHwnd();
    int dx = r.dx;
    int dy = r.dy;

    // Close glyph grows with tab height (taller UI fonts / tab bar) so it
    // stays usable on touch; floor 16 DIP, cap 28 DIP (issue #5220).
    int closeMin = DpiScale(hwnd, 16);
    int closeMax = DpiScale(hwnd, 28);
    int closeDy = dy - DpiScale(hwnd, 6);
    closeDy = limitValue(closeDy, closeMin, closeMax);
    if (closeDy > dy) {
        closeDy = dy;
    }
    int closeDx = closeDy;

    // Padding between circle and tab edge; grow with the button.
    int closePad = std::max(DpiScale(hwnd, 6), closeDx / 2);
    // Keep the glyph inside the tab when tabs are very narrow.
    if (closeDx + closePad > dx && dx > 0) {
        closeDx = std::min(closeDx, std::max(DpiScale(hwnd, 12), dx - 2));
        closeDy = closeDx;
        closePad = std::max(1, (dx - closeDx) / 2);
    }
    int closeY = (dy - closeDy) / 2;

    // Hit target: at least ~40 DIP wide (touch-friendly), full tab height.
    // Cap at half the tab so title still has a drag/select zone.
    int minHitDx = DpiScale(hwnd, 40);
    int hitDx = std::max(closeDx + (2 * closePad), minHitDx);
    hitDx = std::min(hitDx, std::max(closeDx + closePad, dx / 2));
    hitDx = std::min(hitDx, dx);

    bool isRtl = IsTabsRtl(hwnd);
    Rect hit;
    if (isRtl) {
        hit = {r.x, r.y, hitDx, dy};
        rClose = {r.x + closePad, r.y + closeY, closeDx, closeDy};
    } else {
        hit = {r.x + dx - hitDx, r.y, hitDx, dy};
        rClose = {r.x + dx - closeDx - closePad, r.y + closeY, closeDx, closeDy};
    }
    // the glyph is painted in the button's content rect, so the padding is what
    // makes the hit area bigger than the ✕ itself
    closeBtn->padding.left = rClose.x - hit.x;
    closeBtn->padding.top = rClose.y - hit.y;
    closeBtn->padding.right = hit.Right() - rClose.Right();
    closeBtn->padding.bottom = hit.Bottom() - rClose.Bottom();
    closeBtn->SetBounds(hit);
}

bool TabWnd::CloseVisible() {
    if (!ti->canClose) {
        return false;
    }
    return IsSelected() || (IsUnderMouse() && bounds.dx >= kMinTabWidthForClose);
}

void TabWnd::Paint(VirtPaintCtx& ctx) {
    HDC hdc = GfxHdc(ctx.gfx);
    HWND hwnd = GetHwnd();
    Rect r = ctx.bounds;
    COLORREF tabBgCol = BgColor();
    COLORREF textColor = TabTextColorForBackground(tabBgCol);

    Graphics gfx(hdc);
    gfx.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    gfx.SetCompositingQuality(CompositingQualityHighQuality);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    gfx.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    gfx.SetPageUnit(UnitPixel);

    SolidBrush br(GdipCol(tabBgCol));
    gfx.FillRectangle(&br, ToGdipRect(r));

    bool isRtl = IsTabsRtl(hwnd);
    StringFormat sf(StringFormat::GenericDefault());
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    if (isRtl) {
        sf.SetAlignment(Gdiplus::StringAlignmentFar);
    }

    // draw text — inset from the close glyph (size varies with tab height)
    gfx.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    Gdiplus::RectF rTxt = ToGdipRectF(r);
    int textPad = DpiScale(hwnd, 8);
    int textGap = DpiScale(hwnd, 4);
    if (isRtl) {
        // RTL: close on the left — text after the close circle
        int textLeft = rClose.x + rClose.dx + textGap;
        rTxt.X = (Gdiplus::REAL)textLeft;
        rTxt.Width = (Gdiplus::REAL)std::max(0, (r.x + r.dx - textPad) - textLeft);
    } else {
        // LTR: close on the right — text before the close circle
        rTxt.X = (Gdiplus::REAL)(r.x + textPad);
        rTxt.Width = (Gdiplus::REAL)std::max(0, rClose.x - textGap - (int)rTxt.X);
    }
    Font f(hdc, tabsCtrl->GetFont());
    br.SetColor(GdipCol(textColor));
    WCHAR* ws = CWStrTemp(ti->text);
    gfx.DrawString(ws, -1, &f, rTxt, &sf, &br);

    // draw red dot after tab text for dirty (unsaved) tabs
    if (ti->isDirty) {
        gfx.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        // measure actual rendered text width (may be truncated with ellipsis)
        Gdiplus::RectF textBounds;
        gfx.MeasureString(ws, -1, &f, rTxt, &sf, &textBounds);
        int dotRadius = DpiScale(hwnd, 3);
        int dotX = (int)(textBounds.X + textBounds.Width) + dotRadius;
        // clamp to not exceed the text area
        int maxX = (int)(rTxt.X + rTxt.Width) - (dotRadius * 2);
        dotX = std::min(dotX, maxX);
        int dotY = r.y + ((r.dy - (dotRadius * 2)) / 2);
        SolidBrush redBr(Color(255, 0xEE, 0x22, 0x22));
        gfx.FillEllipse(&redBr, dotX, dotY, dotRadius * 2, dotRadius * 2);
        gfx.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    }

    // the ✕ blends into the tab, so it takes the tab's background
    closeBtn->circleColor = tabBgCol;
}

bool TabWnd::OnMouseDown(VirtMouseEvent& ev) {
    tabsCtrl->OnTabMouseDown(this, ev);
    return true;
}

bool TabWnd::OnMouseUp(VirtMouseEvent&) {
    // the drag / migration handling needs capture and screen coordinates, so it
    // stays in the control's WndProc
    return true;
}

TempStr TabWnd::GetTooltipTemp(Point) {
    return str::DupTemp(ti->tooltip);
}

static void TabCloseClicked(TabWnd* tab, VirtMouseEvent*) {
    tab->tabsCtrl->CloseTab(tab->Idx());
}

//--- TabsCtrl

TabsCtrl::TabsCtrl() {
    kind = kindTabs;
}

TabsCtrl::~TabsCtrl() {
    // ~ControlBase deletes vroot and layout (which owns the TabWnds)
    delete tooltip;
}

void TabsCtrl::ScheduleRepaint() {
    HwndScheduleRepaint(hwnd);
}

bool TabsCtrl::IsValidIdx(int idx) {
    return idx >= 0 && idx < TabCount();
}

int TabsCtrl::TabCount() {
    return len(tabs);
}

TabWnd* TabsCtrl::TabWndAt(int idx) {
    if (!IsValidIdx(idx)) {
        return nullptr;
    }
    return tabWnds[idx];
}

// the tab wnds are rebuilt whenever the tab list changes: there are few of them
// and it keeps the tree and the list impossible to get out of step
void TabsCtrl::RebuildTabWnds() {
    if (!bar) {
        return;
    }
    // the box owns the tabs, so free them before dropping both lists
    for (auto& c : bar->children) {
        delete c.layout;
    }
    bar->children.Reset();
    tabWnds.Reset();
    int n = TabCount();
    for (int i = 0; i < n; i++) {
        auto* w = new TabWnd();
        w->tabsCtrl = this;
        w->ti = tabs[i];
        tabWnds.Append(w);
    }
    // RTL tabs run right to left, which for the box means reversed children
    bool isRtl = IsTabsRtl(hwnd);
    bar->alignMain = isRtl ? MainAxisAlign::MainEnd : MainAxisAlign::MainStart;
    for (int i = 0; i < n; i++) {
        bar->AddChild(tabWnds[isRtl ? (n - 1 - i) : i]);
    }
}

// Calculates the size of a tab and lays the bar out.
void TabsCtrl::LayoutTabs() {
    Rect rect = HwndClientRect(hwnd);
    int dy = rect.dy;
    int nTabs = TabCount();
    if (nTabs == 0) {
        // Do not ScheduleRepaint here: an empty bar with a forced repaint can
        // re-enter layout/paint forever if something keeps calling LayoutTabs
        // (issue #5861). The parent hides the control when there are no tabs.
        return;
    }
    int dx;
    if (tabWidthFrozen && frozenTabDx > 0) {
        dx = frozenTabDx;
    } else {
        auto maxDx = (rect.dx - 5) / nTabs;
        dx = std::min(tabDefaultDx, maxDx);
    }
    tabSize = {dx, dy};
    if (IsRunningOnWine()) {
        logf("TabsCtrl::LayoutTabs: hwnd=%p client=(%d,%d) tabSize=(%d,%d) nTabs=%d\n", hwnd, rect.dx, rect.dy,
             tabSize.dx, tabSize.dy, nTabs);
    }

    for (int i = 0; i < nTabs; i++) {
        tabWnds[i]->idealSize = tabSize;
    }
    bar->Layout(Tight({rect.dx, rect.dy}));
    bar->SetBounds(rect);
    // the tabs are virtual controls: this is what paints them and sends them
    // their input
    DoLayout(rect.Size());

    if (withToolTips && tooltip) {
        TooltipInfo* tools = AllocArrayTemp<TooltipInfo>(nTabs);
        for (int i = 0; i < nTabs; i++) {
            tools[i].s = tabs[i]->tooltip;
            tools[i].id = i;
            tools[i].r = tabWnds[i]->bounds;
        }
        TooltipRemoveAll(tooltip->hwnd);
        TooltipAddTools(tooltip->hwnd, hwnd, tools, nTabs);
    }
}

// Finds the index of the tab which contains the given point.
TabsCtrl::MouseState TabsCtrl::TabStateFromMousePosition(const Point& p) {
    TabsCtrl::MouseState res;
    if (p.x < 0 || p.y < 0 || !vroot) {
        return res;
    }
    Point ptLocal{0, 0};
    VirtWnd* hit = vroot->WndFromPoint(p, &ptLocal);
    // the only child of a tab is its ✕, so anything below a tab is the ✕
    bool overClose = hit && hit->parent && IsVirtWndOfKind(hit->parent, kindTabWnd);
    TabWnd* tab = nullptr;
    for (VirtWnd* w = hit; w; w = w->parent) {
        if (IsVirtWndOfKind(w, kindTabWnd)) {
            tab = (TabWnd*)w;
            break;
        }
    }
    if (!tab) {
        return res;
    }
    res.tabIdx = tab->Idx();
    res.tabInfo = tab->ti;
    res.overClose = overClose && tab->CloseVisible();
    Rect r = tab->bounds;
    Rect rightHalf = r;
    int halfDx = r.dx / 2;
    rightHalf.x = r.x + halfDx;
    rightHalf.dx = halfDx;
    res.inRightHalf = rightHalf.Contains(p);
    return res;
}

// which tab the mouse is on decides the highlight and which ✕ is shown
void TabsCtrl::UpdateHover(int tabUnderMouse) {
    bool changed = (tabHighlighted != tabUnderMouse);
    tabHighlighted = tabUnderMouse;
    int n = TabCount();
    for (int i = 0; i < n; i++) {
        TabWnd* w = tabWnds[i];
        auto vis = w->CloseVisible() ? Visibility::Visible : Visibility::Collapse;
        if (w->closeBtn->visibility != vis) {
            w->closeBtn->visibility = vis;
            changed = true;
        }
    }
    if (changed) {
        HwndScheduleRepaint(hwnd);
    }
}

HBITMAP TabsCtrl::RenderForDragging(int idx) {
    TabInfo* ti = GetTab(idx);
    TabWnd* tw = TabWndAt(idx);
    if (!ti || !tw) {
        return nullptr;
    }
    Rect r = tw->bounds;
    Bitmap bitmap(r.dx, r.dy);
    Graphics* gfx = Graphics::FromImage(&bitmap);
    // DrawString() on a bitmap does not work with CompositingModeSourceCopy - obscure bug.
    gfx->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    gfx->SetCompositingQuality(CompositingQualityHighQuality);
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    gfx->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    gfx->SetPageUnit(UnitPixel);

    StringFormat sf(StringFormat::GenericDefault());
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

    COLORREF bgCol = tabSelectedBg;
    COLORREF textCol = tabSelectedText;

    SolidBrush br(GdipCol(bgCol));
    Gdiplus::Rect gr(0, 0, r.dx, r.dy);
    gfx->FillRectangle(&br, gr);

    HDC hdc = GetDC(hwnd);
    Font f(hdc, GetFont());
    ReleaseDC(hwnd, hdc);

    Gdiplus::RectF rTxt(0, 0, (float)r.dx, (float)r.dy);
    rTxt.X += 8;
    rTxt.Width -= (8 + 8);
    br.SetColor(GdipCol(textCol));
    WCHAR* ws = CWStrTemp(ti->text);
    gfx->DrawString(ws, -1, &f, rTxt, &sf, &br);

    HBITMAP ret;
    bitmap.GetHBITMAP(Color(255, 255, 255), &ret);
    delete gfx;
    return ret;
}

// must be called after LayoutTabs()
static void TabsCtrlUpdateAfterChangingTabsCount(TabsCtrl* tabs) {
    HWND hwnd = tabs->hwnd;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
    tabs->tabBeingClosed = -1;
    Point mousePos = HwndGetCursorPos(hwnd);
    auto tabState = tabs->TabStateFromMousePosition(mousePos);
    bool canClose = tabState.tabInfo && tabState.tabInfo->canClose;
    bool overClose = tabState.overClose && canClose;
    int tabUnderMouse = tabState.tabIdx;
    tabs->UpdateHover(tabUnderMouse);
    tabs->tabHighlightedClose = overClose ? tabUnderMouse : -1;
    if (tabs->draggingTab) {
        tabs->draggingTab = false;
        ImageList_EndDrag();
    }
}

static void TriggerSelectionChanged(TabsCtrl* tabs) {
    if (!tabs->onSelectionChanged.IsValid()) {
        return;
    }
    TabsCtrl::SelectionChangedEvent ev;
    ev.tabs = tabs;
    ev.tabIdx = tabs->selectedIdx;
    tabs->onSelectionChanged.Call(&ev);
}

static bool TriggerSelectionChanging(TabsCtrl* tabs) {
    if (!tabs->onSelectionChanging.IsValid()) {
        // allow changing
        return false;
    }

    TabsCtrl::SelectionChangingEvent ev;
    tabs->onSelectionChanging.Call(&ev);
    return ev.preventChanging;
}

static void TriggerTabMigration(TabsCtrl* tabs, int tabIdx, Point p) {
    if (!tabs->onTabMigration.IsValid()) {
        return;
    }
    TabsCtrl::MigrationEvent ev;
    ev.tabs = tabs;
    ev.tabIdx = tabIdx;
    ev.releasePoint = p;
    tabs->onTabMigration.Call(&ev);
}

static void TriggerTabClosed(TabsCtrl* tabs, int tabIdx) {
    if ((tabIdx < 0) || !tabs->onTabClosed.IsValid()) {
        return;
    }
    TabsCtrl::ClosedEvent ev;
    ev.tabs = tabs;
    ev.tabIdx = tabIdx;
    tabs->onTabClosed.Call(&ev);
}

static void TriggerTabDragged(TabsCtrl* tabs, int tab1, int tab2) {
    if (!tabs->onTabDragged.IsValid()) {
        return;
    }
    TabsCtrl::DraggedEvent ev;
    ev.tabs = tabs;
    ev.tab1 = tab1;
    ev.tab2 = tab2;
    tabs->onTabDragged.Call(&ev);
}

static void UpdateAfterDrag(TabsCtrl* tabsCtrl, int tabIdxFrom, int tabIdxTo) {
    int nTabs = tabsCtrl->TabCount();
    bool badState =
        (tabIdxFrom == tabIdxTo) || (tabIdxFrom < 0) || (tabIdxTo < 0) || (tabIdxFrom >= nTabs) || (tabIdxTo > nTabs);
    if (badState) {
        logfa("tabIdxFrom: %d, tabIdxTo: %d, nTabs: %d\n", tabIdxFrom, tabIdxTo, nTabs);
        ReportDebugIf(true);
        return;
    }

    auto&& tabs = tabsCtrl->tabs;
    TabInfo* moved = tabs[tabIdxFrom];
    tabs.RemoveAt(tabIdxFrom);
    if (tabIdxFrom < tabIdxTo) {
        // we moved from left to right e.g. from 1 to 3
        // after removing 1 we insert not at 3 but 2
        tabIdxTo -= 1;
    }
    tabs.InsertAt(tabIdxTo, moved);
    tabsCtrl->RebuildTabWnds();
    tabsCtrl->SetSelected(tabIdxTo);
    tabsCtrl->LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(tabsCtrl);
}

// clicking a tab selects it (and arms a possible drag)
void TabsCtrl::OnTabMouseDown(TabWnd* tab, VirtMouseEvent& ev) {
    int idx = tab->Idx();
    UpdateHover(idx);
    if (idx != selectedIdx) {
        if (TriggerSelectionChanging(this)) {
            return;
        }
        SetSelected(idx);
        TriggerSelectionChanged(this);
        // LoadModelIntoTab() can pump messages; ensure tabs are fully painted.
        HwndRepaintNow(hwnd);
    }
    // SetSelected/TriggerSelectionChanged above can pump messages
    // (LoadModelIntoTab), which may remove tabs and leave idx stale
    TabInfo* ti = GetTab(idx);
    if (!ti || ti->isPinned) {
        return;
    }
    Rect r = tab->bounds;
    grabLocation.x = ev.ptWindow.x - r.x;
    grabLocation.y = ev.ptWindow.y - r.y;
    SetCapture(hwnd);
}

void TabsCtrl::CloseTab(int idx) {
    if (!IsValidIdx(idx)) {
        return;
    }
    // freeze tab widths so next close button stays under cursor;
    // unfreezes when mouse leaves the tab control
    frozenTabDx = tabSize.dx;
    tabWidthFrozen = true;
    TriggerTabClosed(this, idx);
    // TriggerTabClosed() might have destroyed the window and this TabsCtrl
    if (!FindMainWindowByHwnd(hwnd)) {
        return;
    }
    HwndScheduleRepaint(hwnd);
    tabBeingClosed = -1;
}

static bool CanDragTab(TabInfo* tab) {
    if (tab->isPinned) return false;
    return true;
}

void TabsCtrl::WndProc(ControlBase::WndProcEvent* ev) {
    HWND hwnd = ev->hwnd;
    UINT msg = ev->msg;
    WPARAM wp = ev->wparam;
    LPARAM lp = ev->lparam;
    Point mousePos = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (WM_MOUSELEAVE == msg) {
        mousePos = HwndGetCursorPos(hwnd);
    }

    TabsCtrl::MouseState tabState;

    bool overClose = false;
    bool canClose = true;
    int tabUnderMouse = -1;

    if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || (msg == WM_MOUSELEAVE)) {
        tabState = TabStateFromMousePosition(mousePos);
        tabUnderMouse = tabState.tabIdx;
        canClose = tabState.tabInfo && tabState.tabInfo->canClose;
        overClose = tabState.overClose && canClose;
        lastMousePos = mousePos;
    }

    if (draggingTab && msg == WM_MOUSEMOVE) {
        Point p = HwndMapWindowPoint(hwnd, nullptr, mousePos);
        ImageList_DragMove(p.x, p.y);
        {
            ev->result = 0;
            ev->didHandle = true;
            return;
        }
    }

    // Check if mouse has moved beyond system drag threshold
    bool beyondDragThreshold = false;
    if (msg == WM_MOUSEMOVE && GetCapture() == hwnd && !draggingTab) {
        TabWnd* hlWnd = TabWndAt(tabHighlighted);
        if (hlWnd) {
            int cxDrag = GetSystemMetrics(SM_CXDRAG);
            int cyDrag = GetSystemMetrics(SM_CYDRAG);
            Rect r = hlWnd->bounds;
            beyondDragThreshold =
                (abs(mousePos.x - grabLocation.x - r.x) > cxDrag) || (abs(mousePos.y - grabLocation.y - r.y) > cyDrag);
        }
    }

    switch (msg) {
        case WM_NCHITTEST: {
            // parts that are HTTRANSPARENT are used to move the window
            if (!inTitleBar || hwnd == GetCapture()) {
                {
                    ev->result = HTCLIENT;
                    ev->didHandle = true;
                    return;
                }
            }
            mousePos = HwndScreenToClient(hwnd, mousePos);
            tabState = TabStateFromMousePosition(mousePos);
            if (tabState.tabIdx >= 0) {
                {
                    ev->result = HTCLIENT;
                    ev->didHandle = true;
                    return;
                }
            }
            {
                ev->result = HTTRANSPARENT;
                ev->didHandle = true;
                return;
            }
        }

        case WM_SIZE:
            LayoutTabs();
            break;

        case WM_MOUSELEAVE: {
            if (tabWidthFrozen) {
                tabWidthFrozen = false;
                LayoutTabs();
            }
            LRESULT res = 0;
            if (vroot) {
                vroot->OnMessage(msg, wp, lp, res);
            }
            tabHighlightedClose = -1;
            UpdateHover(-1);
            break;
        }

        case WM_MOUSEMOVE: {
            TrackMouseLeave(hwnd);
            bool isDragging = (GetCapture() == hwnd);
            int hl = tabHighlighted;
            if (isDragging && beyondDragThreshold) {
                if (hl < 0) {
                    {
                        ev->result = 0;
                        ev->didHandle = true;
                        return;
                    }
                }
                // move the tab out: draw it as a image and drag around the screen
                draggingTab = true;
                TabWnd* hlWnd = TabWndAt(hl);
                HBITMAP hbmp = RenderForDragging(hl);
                if (!hbmp || !hlWnd) {
                    logfa("TabsCtrl::WndProc: RenderForDragging failed for tab %d\n", hl);
                    {
                        ev->result = 0;
                        ev->didHandle = true;
                        return;
                    }
                }
                Rect r = hlWnd->bounds;
                HIMAGELIST himl = ImageList_Create(r.dx, r.dy, 0, 1, 0);
                ImageList_Add(himl, hbmp, nullptr);
                ImageList_BeginDrag(himl, 0, grabLocation.x, grabLocation.y);
                DeleteObject(hbmp);
                DeleteObject(himl);
                Point p = HwndMapWindowPoint(hwnd, nullptr, mousePos);
                ImageList_DragEnter(nullptr, p.x, p.y);
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }

            LRESULT res = 0;
            if (vroot) {
                vroot->OnMessage(msg, wp, lp, res);
            }

            if (hl != tabUnderMouse) {
                // note: hl == -1 possible repro: we start drag, a file gets loaded via DDE etc.
                // which re-layouts tabs and mouse is no longer over a tab
                if (isDragging && hl != -1 && tabUnderMouse != -1) {
                    // send notification if the highlighted tab is dragged over another
                    if (!CanDragTab(GetTab(tabUnderMouse))) {
                        TriggerTabDragged(this, hl, tabUnderMouse);
                        UpdateAfterDrag(this, hl, tabUnderMouse);
                        {
                            ev->result = 0;
                            ev->didHandle = true;
                            return;
                        }
                    }
                }
                UpdateHover(tabUnderMouse);
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            int xHl = -1;
            if (overClose && !isDragging) {
                xHl = hl;
            }
            if (tabHighlightedClose != xHl) {
                tabHighlightedClose = xHl;
                HwndScheduleRepaint(hwnd);
            }
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }

        case WM_LBUTTONDOWN: {
            if (overClose) {
                tabBeingClosed = tabUnderMouse;
            }
            LRESULT res = 0;
            if (vroot) {
                vroot->OnMessage(msg, wp, lp, res);
            }
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }

        case WM_LBUTTONUP: {
            bool isDragging = (GetCapture() == hwnd);
            if (isDragging) {
                ReleaseCapture();
            }
            if (tabBeingClosed != -1 && tabUnderMouse == tabBeingClosed && overClose) {
                // the ✕ is a control of its own: it runs CloseTab() from here
                LRESULT res = 0;
                if (vroot) {
                    vroot->OnMessage(msg, wp, lp, res);
                }
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            // we don't always get WM_MOUSEMOVE before WM_LBUTTONUP so
            // update the hover state
            UpdateHover(tabUnderMouse);

            if (!draggingTab) {
                LRESULT res = 0;
                if (vroot) {
                    vroot->OnMessage(msg, wp, lp, res);
                }
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            draggingTab = false;
            ImageList_EndDrag();
            int selectedTab = selectedIdx;
            if (tabUnderMouse < 0) {
                // migrate to new/different window
                Point scPoint = HwndClientToScreen(hwnd, mousePos);
                TriggerTabMigration(this, selectedTab, scPoint);
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            int dstIdx = tabUnderMouse;
            if (tabState.inRightHalf) {
                dstIdx++;
            }
            if (dstIdx == selectedTab) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            if ((dstIdx < TabCount()) && GetTab(dstIdx)->isPinned) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            TriggerTabDragged(this, selectedTab, dstIdx);
            UpdateAfterDrag(this, selectedTab, dstIdx);
            HwndScheduleRepaint(hwnd);
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }

        case WM_MBUTTONDOWN: {
            // middle-clicking unconditionally closes the tab
            tabBeingClosed = tabUnderMouse;
            if (tabBeingClosed < 0 || !canClose) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            CloseTab(tabBeingClosed);
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }

        case WM_SETCURSOR: {
            LRESULT res = 0;
            if (VirtTreeOnMessage(hwnd, vroot, msg, wp, lp, res)) {
                ev->result = res;
                ev->didHandle = true;
                return;
            }
            break;
        }

        case WM_ERASEBKGND:
            // we paint the full client in WM_PAINT
            {
                ev->result = TRUE;
                ev->didHandle = true;
                return;
            }

        case WM_PAINT: {
            // BeginPaint / EndPaint only to consume the update region: ValidateRect
            // doesn't clear it on a hidden control, so WM_PAINT was regenerated
            // forever (fullscreen hides the tab bar), starving the low-priority
            // WM_TIMER that drives smooth wheel scrolling (issue #5865).
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            defer {
                EndPaint(hwnd, &ps);
            };
            if (!IsWindowVisible(hwnd)) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            Rect clientRc = HwndClientRect(hwnd);
            if (clientRc.IsEmpty()) {
                {
                    ev->result = 0;
                    ev->didHandle = true;
                    return;
                }
            }
            HDC hdc = GetDC(hwnd);
            COLORREF bgCol = ThemeControlBackgroundColor();
            if (vroot) {
                PaintVirtTree(vroot, hdc, clientRc, bgCol);
            } else {
                // no tabs: nothing but the background
                HdcFillRect(hdc, clientRc, bgCol);
            }
            ReleaseDC(hwnd, hdc);
            {
                ev->result = 0;
                ev->didHandle = true;
                return;
            }
        }
    }

    return; // WndProcDefault
}

HWND TabsCtrl::Create(TabsCtrl::CreateArgs& args) {
    onWndProc = MkMethod1<TabsCtrl, ControlBase::WndProcEvent*, &TabsCtrl::WndProc>(this);
    CreateCustomArgs cargs;
    cargs.parent = args.parent;
    cargs.isRtl = args.isRtl;
    cargs.font = args.font;
    cargs.style = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE;
    cargs.visible = true;
    withToolTips = args.withToolTips;
    tabDefaultDx = args.tabDefaultDx;

    HWND hwnd = CreateCustom(cargs);
    if (!hwnd) {
        return nullptr;
    }

    bar = new HBox();
    bar->alignCross = CrossAxisAlign::Stretch;
    layout = bar;

    if (withToolTips) {
        Tooltip::CreateArgs targs;
        targs.parent = hwnd;
        targs.font = args.font;
        tooltip = new Tooltip();
        tooltip->Create(targs);
        HwndSetWindowStyle(tooltip->hwnd, TTS_NOPREFIX, true);
    }
    return hwnd;
}

Size TabsCtrl::GetIdealSize() {
    Size sz{32, 128};
    return sz;
}

// takes ownership of tab
int TabsCtrl::InsertTab(int idx, TabInfo* tab, bool update) {
    ReportIf(idx < 0);
    tabs.InsertAt(idx, tab);
    RebuildTabWnds();
    if (update) {
        // LayoutTabs() must be before SetSelected() because SetSelected()
        // triggers sync repaint which paints tab texts in wrong positions
        // because we didn't position them yet in layout.
        LayoutTabs();
        SetSelected(idx);
        TabsCtrlUpdateAfterChangingTabsCount(this);
    }
    return idx;
}

void TabsCtrl::SetTextAndTooltip(int idx, Str text, Str tooltip2) {
    TabInfo* tab = GetTab(idx);
    if (!tab) {
        return;
    }
    str::ReplaceWithCopy(&tab->text, text);
    str::ReplaceWithCopy(&tab->tooltip, tooltip2);
    LayoutTabs();
    // Immediate paint so F2 rename / path changes show without waiting for
    // hover or the next idle paint (tabs-in-titlebar caption especially).
    HwndRepaintNow(hwnd);
}

void TabsCtrl::SetTabDirty(int idx, bool dirty) {
    TabInfo* tab = GetTab(idx);
    if (tab && tab->isDirty != dirty) {
        tab->isDirty = dirty;
        LayoutTabs(); // rebuilds tooltips from current ti->tooltip values
        // LayoutTabs only schedules a repaint; force it so the dirty (red dot)
        // indicator updates immediately (e.g. right after editing a form field)
        HwndRepaintNow(hwnd);
    }
}

// returns userData because it's not owned by TabsCtrl
UINT_PTR TabsCtrl::RemoveTab(int idx) {
    ReportIf(idx < 0);
    ReportIf(idx >= TabCount());
    int selectedTab = selectedIdx;
    TabInfo* tab = tabs[idx];
    UINT_PTR userData = tab->userData;
    tabs.RemoveAt(idx);
    delete tab;
    RebuildTabWnds();
    if (TabCount() > 0 && selectedTab >= 0) {
        if (idx < selectedTab) {
            selectedTab--;
        } else if (idx == selectedTab) {
            selectedTab = 0;
        }
        SetSelected(selectedTab);
    } else {
        selectedIdx = -1;
    }
    LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(this);
    return userData;
}

void TabsCtrl::SwapTabs(int idx1, int idx2) {
    TabInfo* tmp = tabs[idx1];
    tabs[idx1] = tabs[idx2];
    tabs[idx2] = tmp;
    RebuildTabWnds();
}

// Note: the caller should take care of deleting userData
void TabsCtrl::RemoveAllTabs() {
    DeleteVecMembers(tabs);
    tabs.Reset();
    selectedIdx = -1;
    RebuildTabWnds();
    LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(this);
}

TabInfo* TabsCtrl::GetTab(int idx) {
    // This is the fail-safe accessor for tab indices that legitimately go out
    // of range: -1 ("no tab") and a stale index that can briefly occur during
    // teardown / DDE-triggered reload. Both are expected, so just bail to
    // nullptr (callers must null-check).
    if (idx < 0 || idx >= len(tabs)) {
        if (idx < -1) {
            // no sentinel ever produces an index below -1: treat as corruption
            ReportIf(true);
        } else if (idx != -1) {
            // idx >= tabs.Size() (idx >= 0): rare - a stale index during teardown
            // or a caller off-by-one. Log a breadcrumb so genuine caller bugs stay
            // diagnosable, without uploading a debug report. -1 ("no tab" sentinel)
            // is ubiquitous, so stay silent for it.
            logf("TabsCtrl::GetTab: out-of-range idx=%d (tabs=%d)\n", idx, len(tabs));
        }
        return nullptr;
    }
    return tabs[idx];
}

int TabsCtrl::GetSelected() {
    return selectedIdx;
}

int TabsCtrl::SetSelected(int idx) {
    int nTabs = TabCount();
    if (idx < 0 || idx >= nTabs) {
        logf("TabsCtrl::SetSelected(): idx: %d, TabsCount(): %d\n", idx, nTabs);
    }
    ReportIf(idx < 0 || idx >= nTabs);
    int prevSelectedIdx = selectedIdx;
    selectedIdx = idx;
    UpdateHover(tabHighlighted);
    HwndRepaintNow(hwnd);
    return prevSelectedIdx;
}

void TabsCtrl::SetHighlighted(int idx) {
    int oldSelectedIdx = selectedIdx;
    if (IsValidIdx(tabForceShowSelected)) {
        oldSelectedIdx = tabForceShowSelected;
    }
    int newSelectedIdx = selectedIdx;
    if (IsValidIdx(idx)) {
        newSelectedIdx = idx;
    }
    if (tabForceShowSelected == idx) {
        return;
    }
    tabForceShowSelected = idx;
    if (oldSelectedIdx == newSelectedIdx) {
        return;
    }
    UpdateHover(tabHighlighted);
    HwndRepaintNow(hwnd);
}

HWND TabsCtrl::GetToolTipsHwnd() {
    return tooltip ? tooltip->hwnd : nullptr;
}
