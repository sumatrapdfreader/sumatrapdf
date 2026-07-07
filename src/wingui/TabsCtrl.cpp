/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

// Forward declaration - defined in MainWindow.cpp
struct MainWindow;
MainWindow* FindMainWindowByHwnd(HWND hwnd);

//--- Tabs

Kind kindTabs = "tabs";

// non-selected tabs narrower than this hide their close button so that
// clicks drag/select instead of accidentally closing the tab
constexpr int kMinTabWidthForClose = 64;

using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Ok;
using Gdiplus::PathData;
using Gdiplus::Pen;
using Gdiplus::Region;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringFormat;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;

static void HwndTabsSetItemSize(HWND hwnd, Size sz) {
    TabCtrl_SetItemSize(hwnd, sz.dx, sz.dy);
}

// hwnd is kept LTR (like the canvas); UI direction comes from the parent frame
static bool IsTabsRtl(HWND hwnd) {
    HWND parent = GetParent(hwnd);
    return parent && HwndIsRtl(parent);
}

TabInfo::~TabInfo() {
    str::Free(text);
    str::Free(tooltip);
}

void TabsCtrl::ScheduleRepaint() {
    HwndScheduleRepaint(hwnd);
}

// Calculates tab's elements, based on its width and height.
// Generates a GraphicsPath, which is used for painting the tab, etc.
void TabsCtrl::LayoutTabs() {
    Rect rect = ClientRect(hwnd);
    int dy = rect.dy;
    int nTabs = TabCount();
    if (nTabs == 0) {
        // logfa("TabsCtrl::Layout size: (%d, %d), no tabs\n", rect.dx, rect.dy);
        HwndScheduleRepaint(hwnd);
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

    int closeDy = DpiScale(hwnd, 16);
    int closeDx = closeDy;
    int closeY = (dy - closeDy) / 2;
    // logfa("  closeDx: %d, closeDy: %d\n", closeDx, closeDy);

    bool isRtl = IsTabsRtl(hwnd);
    int closePad = 8; // padding between close circle and tab edge

    HFONT hfont = GetFont();
    int x = isRtl ? rect.dx : 0;
    int xEnd;
    TooltipInfo* tools = AllocArrayTemp<TooltipInfo>(nTabs);
    for (int i = 0; i < nTabs; i++) {
        // bounded loop with a valid index: index tabs directly instead of going
        // through GetTab (which re-issues TCM_GETITEMCOUNT each call)
        TabInfo* ti = tabs[i];
        if (isRtl) {
            xEnd = x - dx;
            ti->r = {xEnd, 0, dx, dy};
            ti->rClose = {xEnd + closePad, closeY, closeDx, closeDy};
            ti->rCloseHit = {xEnd, 0, closeDx + 2 * closePad, dy};
        } else {
            xEnd = x + dx;
            ti->r = {x, 0, dx, dy};
            ti->rClose = {xEnd - closeDx - closePad, closeY, closeDx, closeDy};
            ti->rCloseHit = {xEnd - closeDx - 2 * closePad, 0, closeDx + 2 * closePad, dy};
        }
        ti->titleSize = HwndMeasureText(hwnd, ti->text, hfont);
        if (IsRunningOnWine() && i == 0) {
            logf("TabsCtrl::LayoutTabs: titleSize=(%d,%d) fontDyPx=%d\n", ti->titleSize.dx, ti->titleSize.dy,
                 FontDyPx(hwnd, hfont));
        }
        int y = (dy - ti->titleSize.dy) / 2;
        // logfa("  ti->titleSize.dy: %d\n", ti->titleSize.dy);
        if (y < 0) {
            y = 0;
        }
        if (isRtl) {
            ti->titlePos = {xEnd + dx - 2 - ti->titleSize.dx, y};
        } else {
            ti->titlePos = {x + 2, y};
        }
        if (withToolTips) {
            tools[i].s = ti->tooltip;
            tools[i].id = i;
            tools[i].r = ti->r;
        }
        x = xEnd;
    }
    if (withToolTips) {
        HWND ttHwnd = GetToolTipsHwnd();
        TooltipRemoveAll(ttHwnd);
        TooltipAddTools(ttHwnd, hwnd, tools, nTabs);
    }

    HwndTabsSetItemSize(hwnd, tabSize);
}

// Finds the index of the tab, which contains the given point.
TabsCtrl::MouseState TabsCtrl::TabStateFromMousePosition(const Point& p) {
    TabsCtrl::MouseState res;
    Point pt = p;
    if (pt.x < 0 || pt.y < 0) {
        return res;
    }
    int nTabs = TabCount();
    for (int i = 0; i < nTabs; i++) {
        TabInfo* ti = tabs[i];
        Rect r = ti->r;
        // logfa("testing i=%d rect: %d %d %d %d pt: %d %d\n", i, ti->r.x, ti->r.y, ti->r.dx, ti->r.dy, pt.x, pt.y);
        if (!r.Contains(pt)) {
            continue;
        }
        res.tabIdx = i;
        bool isSelected = (i == GetSelected());
        bool closeActive = isSelected || r.dx >= kMinTabWidthForClose;
        res.overClose = closeActive && ti->rCloseHit.Contains(pt);
        res.tabInfo = ti;
        Rect rightHalf = r;
        int halfDx = r.dx / 2;
        rightHalf.x = r.x + halfDx;
        rightHalf.dx = halfDx;
        res.inRightHalf = rightHalf.Contains(pt);
        return res;
    }

    return res;
}

Gdiplus::Color GdipCol(COLORREF c) {
    return GdiRgbFromCOLORREF(c);
}

static COLORREF TabTextColorForBackground(COLORREF tabBg) {
    COLORREF text = ThemeWindowTextColor();
    if (abs((int)GetLightness(text) - (int)GetLightness(tabBg)) >= 80) {
        return text;
    }
    return IsLightColor(tabBg) ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

bool TabsCtrl::IsValidIdx(int idx) {
    return idx >= 0 && idx < TabCount();
}

void TabsCtrl::Paint(HDC hdc, const RECT& rc) {
    // verify the cursor is actually inside the tab control; if not, ignore stale lastMousePos
    Point cursorPos = HwndGetCursorPos(hwnd);
    Rect clientRc = ClientRect(hwnd);
    bool mouseInside = clientRc.Contains(cursorPos);
    TabsCtrl::MouseState tabState;
    if (mouseInside) {
        tabState = TabStateFromMousePosition(cursorPos);
    }
    int tabUnderMouse = tabState.tabIdx;
    bool overClose = tabState.overClose && tabState.tabInfo && tabState.tabInfo->canClose;
    int selectedIdx = GetSelected();
    if (IsValidIdx(tabForceShowSelected)) {
        selectedIdx = tabForceShowSelected;
    }

    // logfa("TabsCtrl::Paint, underMouse: %d, overClose: %d, selected: %d, rc: pos: (%d, %d), size: (%d, %d)\n",
    //  tabUnderMouse, (int)overClose, selectedIdx, rc.left, rc.top, RectDx(rc), RectDy(rc));

    Graphics gfx(hdc);
    gfx.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    gfx.SetCompositingQuality(CompositingQualityHighQuality);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    gfx.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    gfx.SetPageUnit(UnitPixel);

    SolidBrush br(GdipCol(ThemeControlBackgroundColor()));

    Font f(hdc, GetFont());

    Gdiplus::Rect gr = ToGdipRect(rc);
    gfx.FillRectangle(&br, gr);

    StringFormat sf(StringFormat::GenericDefault());
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    if (IsTabsRtl(hwnd)) {
        sf.SetAlignment(Gdiplus::StringAlignmentFar);
    }

    TabInfo* ti;
    int n = TabCount();
    Rect r;
    Gdiplus::RectF rTxt;
    COLORREF tabBgSelected = ThemeControlBackgroundColor();
    COLORREF tabBgHighlight;
    COLORREF tabBgBackground;
    tabBgBackground = AccentColor(tabBgSelected, 25);
    tabBgHighlight = AccentColor(tabBgSelected, 35);

    COLORREF tabBgCol;
    for (int i = 0; i < n; i++) {
        // Get the correct colors based on the state and the current theme
        tabBgCol = tabBgBackground;
        bool isSelected = selectedIdx == i;
        bool isUnderMouse = tabUnderMouse == i;
        if (isSelected) {
            tabBgCol = tabBgSelected;
        } else if (isUnderMouse) {
            tabBgCol = tabBgHighlight;
        }

        // bounded loop with a valid index: index tabs directly (avoids the
        // per-iteration TCM_GETITEMCOUNT round-trip GetTab does)
        ti = tabs[i];

        // use per-tab color if explicitly set
        if (!IsSpecialColor(ti->tabColor)) {
            tabBgCol = ti->tabColor;
            if (!isSelected) {
                tabBgCol = AccentColor(ti->tabColor, isUnderMouse ? 35 : 25);
            }
        }

        COLORREF textColor = TabTextColorForBackground(tabBgCol);

        gfx.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);

        // draw background
        br.SetColor(GdipCol(tabBgCol));
        gr = ToGdipRect(ti->r);
        gfx.FillRectangle(&br, gr);

        // debug: paint close hit area in light green
        if (false && ti->canClose && (i == tabUnderMouse)) {
            Gdiplus::SolidBrush dbgBr(Gdiplus::Color(80, 0, 255, 0));
            gfx.FillRectangle(&dbgBr, ToGdipRect(ti->rCloseHit));
        }

        // draw text
        gfx.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
        r = ti->rClose;
        rTxt = ToGdipRectF(ti->r);
        if (IsTabsRtl(hwnd)) {
            // RTL: [8px | close | text | 8px]
            rTxt.X += (8 + r.dx);
        } else {
            // LTR: [8px | text | close | 8px]
            rTxt.X += 8;
        }
        rTxt.Width -= (8 + r.dx + 8);
        br.SetColor(GdipCol(textColor));
        WCHAR* ws = CWStrTemp(ti->text);
        gfx.DrawString(ws, -1, &f, rTxt, &sf, &br);

        // draw red dot after tab text for dirty (unsaved) tabs
        if (ti->isDirty) {
            gfx.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            // measure actual rendered text width (may be truncated with ellipsis)
            Gdiplus::RectF bounds;
            gfx.MeasureString(ws, -1, &f, rTxt, &sf, &bounds);
            int dotRadius = DpiScale(hwnd, 3);
            int dotX = (int)(bounds.X + bounds.Width) + dotRadius;
            // clamp to not exceed the text area
            int maxX = (int)(rTxt.X + rTxt.Width) - dotRadius * 2;
            if (dotX > maxX) {
                dotX = maxX;
            }
            int dotY = ti->r.y + (ti->r.dy - dotRadius * 2) / 2;
            SolidBrush redBr(Color(255, 0xEE, 0x22, 0x22));
            gfx.FillEllipse(&redBr, dotX, dotY, dotRadius * 2, dotRadius * 2);
            gfx.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        }
        bool closeVisible = ti->canClose && (isSelected || (isUnderMouse && ti->r.dx >= kMinTabWidthForClose));
        if (closeVisible) {
            DrawCloseButtonArgs closeArgs;
            closeArgs.hdc = hdc;
            closeArgs.r = ti->rClose;
            closeArgs.isHover = overClose && isUnderMouse;
            closeArgs.colBg = tabBgCol;
            DrawCloseButton(closeArgs);
        }
    }
}

HBITMAP TabsCtrl::RenderForDragging(int idx) {
    TabInfo* ti = GetTab(idx);
    if (!ti) {
        return nullptr;
    }
    Bitmap bitmap(ti->r.dx, ti->r.dy);
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
    Gdiplus::Rect gr(0, 0, ti->r.dx, ti->r.dy);
    gfx->FillRectangle(&br, gr);

    HDC hdc = GetDC(hwnd);
    Font f(hdc, GetFont());
    ReleaseDC(hwnd, hdc);

    Gdiplus::RectF rTxt(0, 0, (float)ti->r.dx, (float)ti->r.dy);
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

TabsCtrl::TabsCtrl() {
    kind = kindTabs;
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
    tabs->tabHighlighted = tabUnderMouse;
    tabs->tabHighlightedClose = overClose ? tabUnderMouse : -1;
    if (tabs->draggingTab) {
        tabs->draggingTab = false;
        ImageList_EndDrag();
    }
}

TabsCtrl::~TabsCtrl() {}

static void TriggerSelectionChanged(TabsCtrl* tabs) {
    if (!tabs->onSelectionChanged.IsValid()) {
        return;
    }
    TabsCtrl::SelectionChangedEvent ev;
    ev.tabs = tabs;
    tabs->onSelectionChanged.Call(&ev);
}

static bool TriggerSelectionChanging(TabsCtrl* tabs) {
    if (!tabs->onSelectionChanging.IsValid()) {
        // allow changing
        return false;
    }

    TabsCtrl::SelectionChangingEvent ev;
    tabs->onSelectionChanging.Call(&ev);
    return (LRESULT)ev.preventChanging;
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
    tabsCtrl->SetSelected(tabIdxTo);
    tabsCtrl->LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(tabsCtrl);
}

LRESULT TabsCtrl::OnNotifyReflect(WPARAM wp, LPARAM lp) {
    NMHDR* hdr = (NMHDR*)lp;
    switch (hdr->code) {
        case TCN_SELCHANGING:
            return (LRESULT)TriggerSelectionChanging(this);

        case TCN_SELCHANGE:
            TriggerSelectionChanged(this);
            HwndScheduleRepaint(hwnd);
            break;

        case TTN_GETDISPINFOA:
        case TTN_GETDISPINFOW:
            break;
    }
    return 0;
}

static bool CanDragTab(TabInfo* tab) {
    if (tab->isPinned) return false;
    return true;
}

LRESULT TabsCtrl::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
        // TempStr msgName = WinMsgNameTemp(msg);
        //  logfa("msg; %s, tabUnderMouse: %d, overClose: %d\n", msgName, tabUnderMouse, (int)overClose);
    }

    if (draggingTab && msg == WM_MOUSEMOVE) {
        POINT p;
        p.x = mousePos.x;
        p.y = mousePos.y;
        MapWindowPoints(hwnd, NULL, &p, 1);
        // logfa("%s moving to: %d %d\n", WinMsgNameTemp(msg), p.x, p.y);
        ImageList_DragMove(p.x, p.y);
        return 0;
    }

    // Check if mouse has moved beyond system drag threshold
    bool beyondDragThreshold = false;
    if (msg == WM_MOUSEMOVE && GetCapture() == hwnd && !draggingTab) {
        if (tabHighlighted >= 0 && tabHighlighted < TabCount()) {
            int cxDrag = GetSystemMetrics(SM_CXDRAG);
            int cyDrag = GetSystemMetrics(SM_CYDRAG);
            beyondDragThreshold = (abs(mousePos.x - grabLocation.x - GetTab(tabHighlighted)->r.x) > cxDrag) ||
                                  (abs(mousePos.y - grabLocation.y - GetTab(tabHighlighted)->r.y) > cyDrag);
        }
    }

    switch (msg) {
        case WM_NCHITTEST: {
            if (false) {
                return HTCLIENT;
            }
            // parts that are HTTRANSPARENT are used to move the window
            if (!inTitleBar || hwnd == GetCapture()) {
                return HTCLIENT;
            }
            HwndScreenToClient(hwnd, mousePos);
            tabState = TabStateFromMousePosition(mousePos);
            if (tabState.tabIdx >= 0) {
                return HTCLIENT;
            }
            return HTTRANSPARENT;
        }

        case WM_SIZE:
            LayoutTabs();
            break;

        case WM_MOUSELEAVE:
            if (tabWidthFrozen) {
                tabWidthFrozen = false;
                LayoutTabs();
            }
            if (tabHighlighted != tabUnderMouse || tabHighlightedClose != -1) {
                tabHighlighted = tabUnderMouse;
                tabHighlightedClose = -1;
                HwndScheduleRepaint(hwnd);
            }
            break;

        case WM_MOUSEMOVE: {
            TrackMouseLeave(hwnd);
            bool isDragging = (GetCapture() == hwnd);
            int hl = tabHighlighted;
            if (isDragging && beyondDragThreshold) {
                if (hl < 0) {
                    return 0;
                }
                // move the tab out: draw it as a image and drag around the screen
                draggingTab = true;
                TabInfo* thl = GetTab(hl);
                HBITMAP hbmp = RenderForDragging(hl);
                if (!hbmp) {
                    logfa("TabsCtrl::WndProc: RenderForDragging failed for tab %d\n", hl);
                    return 0;
                }
                HIMAGELIST himl = ImageList_Create(thl->r.dx, thl->r.dy, 0, 1, 0);
                ImageList_Add(himl, hbmp, NULL);
                ImageList_BeginDrag(himl, 0, grabLocation.x, grabLocation.y);
                DeleteObject(hbmp);
                DeleteObject(himl);
                POINT p(mousePos.x, mousePos.y);
                MapWindowPoints(hwnd, NULL, &p, 1);
                ImageList_DragEnter(NULL, p.x, p.y);
                return 0;
            }

            if (hl != tabUnderMouse) {
                tabHighlighted = tabUnderMouse;
                // logf("tab: WM_MOUSEMOVE: tabHighlighted = tabUnderMouse: %d\n", tabHighlighted);
                // note: hl == -1 possible repro: we start drag, a file gets loaded via DDE etc.
                // which re-layouts tabs and mouse is no longer over a tab
                if (isDragging && hl != -1 && tabUnderMouse != -1) {
                    // send notification if the highlighted tab is dragged over another
                    if (!CanDragTab(GetTab(tabUnderMouse))) {
                        TriggerTabDragged(this, hl, tabUnderMouse);
                        UpdateAfterDrag(this, hl, tabUnderMouse);
                    }
                } else {
                    // highlight a different tab
                    HwndScheduleRepaint(hwnd);
                }
                return 0;
            }
            int xHl = -1;
            if (overClose && !isDragging) {
                xHl = hl;
            }
            // logfa("inX=%d, hl=%d, xHl=%d, xHighlighted=%d\n", (int)inX, hl, xHl, tab->xHighlighted);
            if (tabHighlightedClose != xHl) {
                // logfa("before invalidate, xHl=%d, xHighlited=%d\n", xHl, tab->xHighlighted);
                tabHighlightedClose = xHl;
                HwndScheduleRepaint(hwnd);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            tabHighlighted = tabUnderMouse;
            if (overClose) {
                HwndScheduleRepaint(hwnd);
                tabBeingClosed = tabUnderMouse;
                return 0;
            }
            if (tabUnderMouse < 0) {
                return 0;
            }

            int selectedTab = GetSelected();
            if (tabUnderMouse != selectedTab) {
                bool stopChange = TriggerSelectionChanging(this);
                if (stopChange) {
                    return 0;
                }
                SetSelected(tabUnderMouse);
                TriggerSelectionChanged(this);
                // LoadModelIntoTab() can pump messages; ensure tabs are fully painted.
                HwndRepaintNow(hwnd);
            }
            // SetSelected/TriggerSelectionChanged above can pump messages
            // (LoadModelIntoTab), which may remove tabs and leave tabUnderMouse
            // stale (>= TabCount()); GetTab then returns nullptr.
            TabInfo* ti = GetTab(tabUnderMouse);
            if (!ti || ti->isPinned) {
                return 0;
            }

            grabLocation.x = mousePos.x - ti->r.x;
            grabLocation.y = mousePos.y - ti->r.y;
            SetCapture(hwnd);
            return 0;
        }

        case WM_LBUTTONUP: {
            bool isDragging = (GetCapture() == hwnd);
            if (isDragging) {
                ReleaseCapture();
            }
            if (tabBeingClosed != -1 && tabUnderMouse == tabBeingClosed && overClose) {
                // freeze tab widths so next close button stays under cursor
                // unfreezes when mouse leaves the tab control
                frozenTabDx = tabSize.dx;
                tabWidthFrozen = true;
                // send notification that the tab is closed
                TriggerTabClosed(this, tabBeingClosed);
                // TriggerTabClosed() might have destroyed the window and this TabsCtrl
                if (!FindMainWindowByHwnd(hwnd)) {
                    return 0;
                }
                HwndScheduleRepaint(hwnd);
                tabBeingClosed = -1;
                return 0;
            }
            // we don't always get WM_MOUSEMOVE before WM_LBUTTONUP so
            // update tabHighlighted
            tabHighlighted = tabUnderMouse;

            if (!draggingTab) {
                return 0;
            }
            draggingTab = false;
            ImageList_EndDrag();
            int selectedTab = GetSelected();
            if (tabUnderMouse < 0) {
                // migrate to new/different window
                POINT p(mousePos.x, mousePos.y);
                ClientToScreen(hwnd, &p);
                Point scPoint(p.x, p.y);
                TriggerTabMigration(this, selectedTab, scPoint);
                return 0;
            }
            int dstIdx = tabUnderMouse;
            if (tabState.inRightHalf) {
                dstIdx++;
            }
            if (dstIdx == selectedTab) {
                return 0;
            }
            if ((dstIdx < TabCount()) && GetTab(dstIdx)->isPinned) {
                return 0;
            }
            TriggerTabDragged(this, selectedTab, dstIdx);
            UpdateAfterDrag(this, selectedTab, dstIdx);
            HwndScheduleRepaint(hwnd);
            return 0;
        }

        case WM_MBUTTONDOWN: {
            // middle-clicking unconditionally closes the tab

            tabBeingClosed = tabUnderMouse;
            if (tabBeingClosed < 0 || !canClose) {
                return 0;
            }
            TriggerTabClosed(this, tabBeingClosed);
            // TriggerTabClosed() might have destroyed the window and this TabsCtrl
            if (!FindMainWindowByHwnd(hwnd)) {
                return 0;
            }
            HwndScheduleRepaint(hwnd);
            return 0;
        }

        case WM_ERASEBKGND:
            // We paint the full client in WM_PAINT. Don't erase here: TabCtrl_SetCurSel
            // invalidates native (LTR) item rects while we lay out tabs manually in RTL.
            return TRUE;

        case WM_NCPAINT:
            return 0; // prevent native tab control from drawing its edge

        case WM_NCCALCSIZE:
            return 0; // remove non-client area so no edge is reserved

        case WM_PAINT: {
            // TabCtrl_SetCurSel invalidates native (LTR) item rects; we lay out tabs
            // manually (RTL tabs start from the right). Avoid BeginPaint's clip region.
            RECT clientRc = ClientRECT(hwnd);
            HDC hdc = GetDC(hwnd);
            DoubleBuffer buffer(hwnd, ToRect(clientRc));
            Paint(buffer.GetDC(), clientRc);
            buffer.Flush(hdc);
            ReleaseDC(hwnd, hdc);
            ValidateRect(hwnd, nullptr);
            return 0;
        }
    }

    return WndProcDefault(hwnd, msg, wp, lp);
}

HWND TabsCtrl::Create(TabsCtrl::CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.isRtl = args.isRtl;
    cargs.font = args.font;
    cargs.className = WC_TABCONTROLW;
    withToolTips = args.withToolTips;
    tabDefaultDx = args.tabDefaultDx;

    cargs.style = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT;
    if (withToolTips) {
        cargs.style |= TCS_TOOLTIPS;
    }

    HWND hwnd = CreateControl(cargs);
    if (!hwnd) {
        return nullptr;
    }

    if (withToolTips) {
        HWND ttHwnd = GetToolTipsHwnd();
        SetWindowStyle(ttHwnd, TTS_NOPREFIX, true);
    }
    return hwnd;
}

Size TabsCtrl::GetIdealSize() {
    Size sz{32, 128};
    return sz;
}

int TabsCtrl::TabCount() {
    int n = TabCtrl_GetItemCount(hwnd);
    return n;
}

// takes ownership of tab
int TabsCtrl::InsertTab(int idx, TabInfo* tab) {
    ReportIf(idx < 0);
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = CWStrTemp(tab->text);
    int res = TabCtrl_InsertItem(hwnd, idx, &item);
    if (res < 0) {
        return res;
    }
    tabs.InsertAt(idx, tab);
    // LayoutTabs() must be before SetSelected() because SetSelected()
    // triggers sync repaint which paints tab texts in wrong positions
    // because we didn't position them yet in layout.
    LayoutTabs();
    SetSelected(idx);
    TabsCtrlUpdateAfterChangingTabsCount(this);
    return idx;
}

void TabsCtrl::SetTextAndTooltip(int idx, Str text, Str tooltip) {
    TabInfo* tab = GetTab(idx);
    str::ReplaceWithCopy(&tab->text, text);
    str::ReplaceWithCopy(&tab->tooltip, tooltip);
    LayoutTabs();
    HwndScheduleRepaint(hwnd);
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
    BOOL ok = TabCtrl_DeleteItem(hwnd, idx);
    ReportIf(!ok);
    TabInfo* tab = tabs[idx];
    UINT_PTR userData = tab->userData;
    tabs.RemoveAt(idx);
    delete tab;
    int selectedTab = GetSelected();
    if (idx < selectedTab) {
        SetSelected(selectedTab - 1);
    } else if (idx == selectedTab) {
        SetSelected(0);
    }
    LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(this);
    return userData;
}

void TabsCtrl::SwapTabs(int idx1, int idx2) {
    TabInfo* tmp = tabs[idx1];
    tabs[idx1] = tabs[idx2];
    tabs[idx2] = tmp;
}

// Note: the caller should take care of deleting userData
void TabsCtrl::RemoveAllTabs() {
    TabCtrl_DeleteAllItems(hwnd);
    DeleteVecMembers(tabs);
    tabs.Reset();
    LayoutTabs();
    TabsCtrlUpdateAfterChangingTabsCount(this);
}

TabInfo* TabsCtrl::GetTab(int idx) {
    // This is the fail-safe accessor for tab indices that legitimately go out
    // of range: -1 ("no tab" - TabCtrl_GetCurSel with nothing selected,
    // tabUnderMouse/tabHighlighted when not over a tab) and a stale index that
    // can briefly occur during teardown / DDE-triggered reload. Both are
    // expected, so just bail to nullptr (callers must null-check).
    // Bound against the tabs Vec we actually index, not the native control
    // count: InsertTab adds to the native control before the Vec, so the two
    // can transiently disagree.
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
    int idx = TabCtrl_GetCurSel(hwnd);
    return idx;
}

int TabsCtrl::SetSelected(int idx) {
    int nTabs = TabCount();
    if (idx < 0 || idx >= nTabs) {
        logf("TabsCtrl::SetSelected(): idx: %d, TabsCount(): %d\n", idx, nTabs);
    }
    ReportIf(idx < 0 || idx >= nTabs);
    // Suppress native tab invalidation (LTR item rects); we repaint the full client.
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    int prevSelectedIdx = TabCtrl_SetCurSel(hwnd, idx);
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    HwndRepaintNow(hwnd);
    return prevSelectedIdx;
}

void TabsCtrl::SetHighlighted(int idx) {
    int oldSelectedIdx = GetSelected();
    if (IsValidIdx(tabForceShowSelected)) {
        oldSelectedIdx = tabForceShowSelected;
    }
    int newSelectedIdx = GetSelected();
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
    HwndRepaintNow(hwnd);
}

HWND TabsCtrl::GetToolTipsHwnd() {
    HWND res = TabCtrl_GetToolTips(hwnd);
    return res;
}
